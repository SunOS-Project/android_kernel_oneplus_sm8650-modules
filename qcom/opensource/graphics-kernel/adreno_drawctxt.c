// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2002,2007-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>

#include "adreno.h"
#include "adreno_trace.h"

static void wait_callback(struct kgsl_device *device,
		struct kgsl_event_group *group, void *priv, int result)
{
	struct adreno_context *drawctxt = priv;

	wake_up_all(&drawctxt->waiting);
}

static int _check_context_timestamp(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int timestamp)
{
	/* Bail if the drawctxt has been invalidated or destroyed */
	if (kgsl_context_is_bad(context))
		return 1;

	return kgsl_check_timestamp(device, context, timestamp);
}

/**
 * adreno_drawctxt_dump() - dump information about a draw context
 * @device: KGSL device that owns the context
 * @context: KGSL context to dump information about
 *
 * Dump specific information about the context to the kernel log.  Used for
 * fence timeout callbacks
 */
void adreno_drawctxt_dump(struct kgsl_device *device,
		struct kgsl_context *context)
{
	unsigned int queue, start, retire;
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	int index, pos;
	char buf[120];

	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_QUEUED, &queue);
	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_CONSUMED, &start);
	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_RETIRED, &retire);

	/*
	 * We may have kgsl sync obj timer running, which also uses same
	 * lock, take a lock with software interrupt disabled (bh)
	 * to avoid spin lock recursion.
	 *
	 * Use Spin trylock because dispatcher can acquire drawctxt->lock
	 * if context is pending and the fence it is waiting on just got
	 * signalled. Dispatcher acquires drawctxt->lock and tries to
	 * delete the sync obj timer using del_timer_sync().
	 * del_timer_sync() waits till timer and its pending handlers
	 * are deleted. But if the timer expires at the same time,
	 * timer handler could be waiting on drawctxt->lock leading to a
	 * deadlock. To prevent this use spin_trylock_bh.
	 */
	if (!spin_trylock_bh(&drawctxt->lock)) {
		dev_err(device->dev, "  context[%u]: could not get lock\n",
			context->id);
		return;
	}

	dev_err(device->dev,
		"  context[%u]: queue=%u, submit=%u, start=%u, retire=%u\n",
		context->id, queue, drawctxt->submitted_timestamp,
		start, retire);

	if (drawctxt->drawqueue_head != drawctxt->drawqueue_tail) {
		struct kgsl_drawobj *drawobj =
			drawctxt->drawqueue[drawctxt->drawqueue_head];

		if (test_bit(ADRENO_CONTEXT_FENCE_LOG, &context->priv)) {
			dev_err(device->dev,
				"  possible deadlock. Context %u might be blocked for itself\n",
				context->id);
			goto stats;
		}

		if (!kref_get_unless_zero(&drawobj->refcount))
			goto stats;

		if (drawobj->type == SYNCOBJ_TYPE) {
			struct kgsl_drawobj_sync *syncobj = SYNCOBJ(drawobj);

			if (kgsl_drawobj_events_pending(syncobj)) {
				dev_err(device->dev,
					"  context[%u] (ts=%u) Active sync points:\n",
					context->id, drawobj->timestamp);

				kgsl_dump_syncpoints(device, syncobj);
			}
		}

		kgsl_drawobj_put(drawobj);
	}

stats:
	memset(buf, 0, sizeof(buf));

	pos = 0;

	for (index = 0; index < SUBMIT_RETIRE_TICKS_SIZE; index++) {
		uint64_t msecs;
		unsigned int usecs;

		if (!drawctxt->submit_retire_ticks[index])
			continue;
		msecs = drawctxt->submit_retire_ticks[index] * 10;
		usecs = do_div(msecs, 192);
		usecs = do_div(msecs, 1000);
		pos += scnprintf(buf + pos, sizeof(buf) - pos, "%u.%0u ",
			(unsigned int)msecs, usecs);
	}
	dev_err(device->dev, "  context[%u]: submit times: %s\n",
		context->id, buf);

	spin_unlock_bh(&drawctxt->lock);
}

/**
 * adreno_drawctxt_wait() - sleep until a timestamp expires
 * @adreno_dev: pointer to the adreno_device struct
 * @drawctxt: Pointer to the draw context to sleep for
 * @timetamp: Timestamp to wait on
 * @timeout: Number of jiffies to wait (0 for infinite)
 *
 * Register an event to wait for a timestamp on a context and sleep until it
 * has past.  Returns < 0 on error, -ETIMEDOUT if the timeout expires or 0
 * on success
 */
int adreno_drawctxt_wait(struct adreno_device *adreno_dev,
		struct kgsl_context *context,
		uint32_t timestamp, unsigned int timeout)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	int ret;
	long ret_temp;

	if (kgsl_context_detached(context))
		return -ENOENT;

	if (kgsl_context_invalid(context))
		return -EDEADLK;

	trace_adreno_drawctxt_wait_start(-1, context->id, timestamp);

	ret = kgsl_add_event(device, &context->events, timestamp,
		wait_callback, (void *) drawctxt);
	if (ret)
		goto done;

	/*
	 * If timeout is 0, wait forever. msecs_to_jiffies will force
	 * values larger than INT_MAX to an infinite timeout.
	 */
	if (timeout == 0)
		timeout = UINT_MAX;

	ret_temp = wait_event_interruptible_timeout(drawctxt->waiting,
			_check_context_timestamp(device, context, timestamp),
			msecs_to_jiffies(timeout));

	if (ret_temp <= 0) {
		kgsl_cancel_event(device, &context->events, timestamp,
			wait_callback, (void *)drawctxt);

		ret = ret_temp ? (int)ret_temp : -ETIMEDOUT;
		goto done;
	}
	ret = 0;

	/* -EDEADLK if the context was invalidated while we were waiting */
	if (kgsl_context_invalid(context))
		ret = -EDEADLK;


	/* Return -EINVAL if the context was detached while we were waiting */
	if (kgsl_context_detached(context))
		ret = -ENOENT;

done:
	trace_adreno_drawctxt_wait_done(-1, context->id, timestamp, ret);
	return ret;
}

/**
 * adreno_drawctxt_wait_rb() - Wait for the last RB timestamp at which this
 * context submitted a command to the corresponding RB
 * @adreno_dev: The device on which the timestamp is active
 * @context: The context which subbmitted command to RB
 * @timestamp: The RB timestamp of last command submitted to RB by context
 * @timeout: Timeout value for the wait
 * Caller must hold the device mutex
 */
static int adreno_drawctxt_wait_rb(struct adreno_device *adreno_dev,
		struct kgsl_context *context,
		uint32_t timestamp, unsigned int timeout)
{
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	int ret = 0;

	/*
	 * If the context is invalid (OR) not submitted commands to GPU
	 * then return immediately - we may end up waiting for a timestamp
	 * that will never come
	 */
	if (kgsl_context_invalid(context) ||
			!test_bit(KGSL_CONTEXT_PRIV_SUBMITTED, &context->priv))
		goto done;

	trace_adreno_drawctxt_wait_start(drawctxt->rb->id, context->id,
					timestamp);

	ret = adreno_ringbuffer_waittimestamp(drawctxt->rb, timestamp, timeout);
done:
	trace_adreno_drawctxt_wait_done(drawctxt->rb->id, context->id,
					timestamp, ret);
	return ret;
}

static int drawctxt_detach_drawobjs(struct adreno_context *drawctxt,
		struct kgsl_drawobj **list)
{
	int count = 0;

	while (drawctxt->drawqueue_head != drawctxt->drawqueue_tail) {
		struct kgsl_drawobj *drawobj =
			drawctxt->drawqueue[drawctxt->drawqueue_head];

		drawctxt->drawqueue_head = (drawctxt->drawqueue_head + 1) %
			ADRENO_CONTEXT_DRAWQUEUE_SIZE;

		list[count++] = drawobj;
	}

	return count;
}

/**
 * adreno_drawctxt_invalidate() - Invalidate an adreno draw context
 * @device: Pointer to the KGSL device structure for the GPU
 * @context: Pointer to the KGSL context structure
 *
 * Invalidate the context and remove all queued commands and cancel any pending
 * waiters
 */
void adreno_drawctxt_invalidate(struct kgsl_device *device,
		struct kgsl_context *context)
{
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	struct kgsl_drawobj *list[ADRENO_CONTEXT_DRAWQUEUE_SIZE];
	int i, count;

	trace_adreno_drawctxt_invalidate(drawctxt);

	spin_lock(&drawctxt->lock);
	set_bit(KGSL_CONTEXT_PRIV_INVALID, &context->priv);

	/*
	 * set the timestamp to the last value since the context is invalidated
	 * and we want the pending events for this context to go away
	 */
	kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, soptimestamp),
			drawctxt->timestamp);

	kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp),
			drawctxt->timestamp);

	/* Get rid of commands still waiting in the queue */
	count = drawctxt_detach_drawobjs(drawctxt, list);
	spin_unlock(&drawctxt->lock);

	for (i = 0; i < count; i++) {
		kgsl_cancel_events_timestamp(device, &context->events,
			list[i]->timestamp);
		kgsl_drawobj_destroy(list[i]);
	}

	/* Make sure all pending events are processed or cancelled */
	kgsl_flush_event_group(device, &context->events);

	/* Give the bad news to everybody waiting around */
	wake_up_all(&drawctxt->waiting);
	wake_up_all(&drawctxt->wq);
	wake_up_all(&drawctxt->timeout);
}

void adreno_drawctxt_set_guilty(struct kgsl_device *device,
		struct kgsl_context *context)
{
	if (!context)
		return;

	context->reset_status = KGSL_CTX_STAT_GUILTY_CONTEXT_RESET_EXT;

	adreno_drawctxt_invalidate(device, context);
}

#define KGSL_CONTEXT_PRIORITY_MED	0x8

/**
 * adreno_drawctxt_create - create a new adreno draw context
 * @dev_priv: the owner of the context
 * @flags: flags for the context (passed from user space)
 *
 * Create and return a new draw context for the 3D core.
 */
struct kgsl_context *
adreno_drawctxt_create(struct kgsl_device_private *dev_priv,
			uint32_t *flags)
{
	struct adreno_context *drawctxt;
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int ret;
	unsigned int local;

	local = *flags & (KGSL_CONTEXT_PREAMBLE |
		KGSL_CONTEXT_NO_GMEM_ALLOC |
		KGSL_CONTEXT_PER_CONTEXT_TS |
		KGSL_CONTEXT_USER_GENERATED_TS |
		KGSL_CONTEXT_NO_FAULT_TOLERANCE |
		KGSL_CONTEXT_INVALIDATE_ON_FAULT |
		KGSL_CONTEXT_CTX_SWITCH |
		KGSL_CONTEXT_PRIORITY_MASK |
		KGSL_CONTEXT_TYPE_MASK |
		KGSL_CONTEXT_PWR_CONSTRAINT |
		KGSL_CONTEXT_IFH_NOP |
		KGSL_CONTEXT_SECURE |
		KGSL_CONTEXT_PREEMPT_STYLE_MASK |
		KGSL_CONTEXT_LPAC |
		KGSL_CONTEXT_NO_SNAPSHOT |
		KGSL_CONTEXT_FAULT_INFO);

	/* Check for errors before trying to initialize */

	/* If preemption is not supported, ignore preemption request */
	if (!adreno_preemption_feature_set(adreno_dev))
		local &= ~KGSL_CONTEXT_PREEMPT_STYLE_MASK;

	/* We no longer support legacy context switching */
	if ((local & KGSL_CONTEXT_PREAMBLE) == 0 ||
		(local & KGSL_CONTEXT_NO_GMEM_ALLOC) == 0) {
		dev_err_once(device->dev,
			"legacy context switch not supported\n");
		return ERR_PTR(-EINVAL);
	}

	/* Make sure that our target can support secure contexts if requested */
	if (!kgsl_mmu_is_secured(&dev_priv->device->mmu) &&
			(local & KGSL_CONTEXT_SECURE)) {
		dev_err_once(device->dev, "Secure context not supported\n");
		return ERR_PTR(-EOPNOTSUPP);
	}

	if ((local & KGSL_CONTEXT_LPAC) &&
			(!(adreno_dev->lpac_enabled))) {
		dev_err_once(device->dev, "LPAC context not supported\n");
		return ERR_PTR(-EOPNOTSUPP);
	}

	if ((local & KGSL_CONTEXT_LPAC) && (local & KGSL_CONTEXT_SECURE)) {
		dev_err_once(device->dev, "LPAC secure context not supported\n");
		return ERR_PTR(-EOPNOTSUPP);
	}

	drawctxt = kzalloc(sizeof(struct adreno_context), GFP_KERNEL);

	if (drawctxt == NULL)
		return ERR_PTR(-ENOMEM);

	drawctxt->timestamp = 0;

	drawctxt->base.flags = local;

	/* Always enable per-context timestamps */
	drawctxt->base.flags |= KGSL_CONTEXT_PER_CONTEXT_TS;
	drawctxt->type = (drawctxt->base.flags & KGSL_CONTEXT_TYPE_MASK)
		>> KGSL_CONTEXT_TYPE_SHIFT;
	spin_lock_init(&drawctxt->lock);
	init_waitqueue_head(&drawctxt->wq);
	init_waitqueue_head(&drawctxt->waiting);
	init_waitqueue_head(&drawctxt->timeout);

	/* If the priority is not set by user, set it for them */
	if ((drawctxt->base.flags & KGSL_CONTEXT_PRIORITY_MASK) ==
			KGSL_CONTEXT_PRIORITY_UNDEF)
		drawctxt->base.flags |= (KGSL_CONTEXT_PRIORITY_MED <<
				KGSL_CONTEXT_PRIORITY_SHIFT);

	/* Store the context priority */
	drawctxt->base.priority =
		(drawctxt->base.flags & KGSL_CONTEXT_PRIORITY_MASK) >>
		KGSL_CONTEXT_PRIORITY_SHIFT;

	/*
	 * Now initialize the common part of the context. This allocates the
	 * context id, and then possibly another thread could look it up.
	 * So we want all of our initializtion that doesn't require the context
	 * id to be done before this call.
	 */
	ret = kgsl_context_init(dev_priv, &drawctxt->base);
	if (ret != 0) {
		kfree(drawctxt);
		return ERR_PTR(ret);
	}

	kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(drawctxt->base.id, soptimestamp),
			0);
	kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(drawctxt->base.id, eoptimestamp),
			0);

	adreno_context_debugfs_init(ADRENO_DEVICE(device), drawctxt);

	INIT_LIST_HEAD(&drawctxt->active_node);
	INIT_LIST_HEAD(&drawctxt->hw_fence_list);
	INIT_LIST_HEAD(&drawctxt->hw_fence_inflight_list);

	if (adreno_dev->dispatch_ops && adreno_dev->dispatch_ops->setup_context)
		adreno_dev->dispatch_ops->setup_context(adreno_dev, drawctxt);

	if (gpudev->preemption_context_init) {
		ret = gpudev->preemption_context_init(&drawctxt->base);
		if (ret != 0) {
			kgsl_context_detach(&drawctxt->base);
			return ERR_PTR(ret);
		}
	}

    if (strstr(drawctxt->base.proc_priv->cmdline, "com.tencent.tmgp.dfm")) {
		drawctxt->base.flags |= KGSL_CONTEXT_NO_FAULT_TOLERANCE;
    }

	/* copy back whatever flags we dediced were valid */
	*flags = drawctxt->base.flags;

	return &drawctxt->base;
}

static void wait_for_timestamp_rb(struct kgsl_device *device,
	struct adreno_context *drawctxt)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_context *context = &drawctxt->base;
	int ret;

	/*
	 * internal_timestamp is set in adreno_ringbuffer_addcmds,
	 * which holds the device mutex.
	 */
	mutex_lock(&device->mutex);

	/*
	 * Wait for the last global timestamp to pass before continuing.
	 * The maxumum wait time is 30s, some large IB's can take longer
	 * than 10s and if hang happens then the time for the context's
	 * commands to retire will be greater than 10s. 30s should be sufficient
	 * time to wait for the commands even if a hang happens.
	 */
	ret = adreno_drawctxt_wait_rb(adreno_dev, &drawctxt->base,
		drawctxt->internal_timestamp, 30 * 1000);

	/*
	 * If the wait for global fails due to timeout then mark it as
	 * context detach timeout fault and schedule dispatcher to kick
	 * in GPU recovery. For a ADRENO_CTX_DETATCH_TIMEOUT_FAULT we clear
	 * the policy and invalidate the context. If EAGAIN error is returned
	 * then recovery will kick in and there will be no more commands in the
	 * RB pipe from this context which is what we are waiting for, so ignore
	 * -EAGAIN error.
	 */
	if (ret && ret != -EAGAIN) {
		dev_err(device->dev,
				"Wait for global ctx=%u ts=%u type=%d error=%d\n",
				drawctxt->base.id, drawctxt->internal_timestamp,
				drawctxt->type, ret);

		adreno_set_gpu_fault(adreno_dev,
				ADRENO_CTX_DETATCH_TIMEOUT_FAULT);
		mutex_unlock(&device->mutex);

		/* Schedule dispatcher to kick in recovery */
		adreno_dispatcher_schedule(device);

		/* Wait for context to be invalidated and release context */
		wait_event_interruptible_timeout(drawctxt->timeout,
					kgsl_context_invalid(&drawctxt->base),
					msecs_to_jiffies(5000));
		return;
	}

	kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, soptimestamp),
			drawctxt->timestamp);

	kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp),
			drawctxt->timestamp);

	adreno_profile_process_results(adreno_dev);

	mutex_unlock(&device->mutex);
}

void adreno_drawctxt_detach(struct kgsl_context *context)
{
	struct kgsl_device *device;
	struct adreno_device *adreno_dev;
	const struct adreno_gpudev *gpudev;
	struct adreno_context *drawctxt;
	int count, i;
	struct kgsl_drawobj *list[ADRENO_CONTEXT_DRAWQUEUE_SIZE];

	if (context == NULL)
		return;

	device = context->device;
	adreno_dev = ADRENO_DEVICE(device);
	gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	drawctxt = ADRENO_CONTEXT(context);

	spin_lock(&drawctxt->lock);

	spin_lock(&adreno_dev->active_list_lock);
	list_del_init(&drawctxt->active_node);
	spin_unlock(&adreno_dev->active_list_lock);

	count = drawctxt_detach_drawobjs(drawctxt, list);
	spin_unlock(&drawctxt->lock);

	for (i = 0; i < count; i++) {
		/*
		 * If the context is detached while we are waiting for
		 * the next command in GFT SKIP CMD, print the context
		 * detached status here.
		 */
		adreno_fault_skipcmd_detached(adreno_dev, drawctxt, list[i]);
		kgsl_drawobj_destroy(list[i]);
	}

	debugfs_remove_recursive(drawctxt->debug_root);
	/* The debugfs file has a reference, release it */
	if (drawctxt->debug_root)
		kgsl_context_put(context);

	if (gpudev->context_detach)
		gpudev->context_detach(drawctxt);
	else
		wait_for_timestamp_rb(device, drawctxt);

	if (context->user_ctxt_record) {
		gpumem_free_entry(context->user_ctxt_record);

		/* Put the extra ref from gpumem_alloc_entry() */
		kgsl_mem_entry_put(context->user_ctxt_record);
	}

	/* wake threads waiting to submit commands from this context */
	wake_up_all(&drawctxt->waiting);
	wake_up_all(&drawctxt->wq);
}

void adreno_drawctxt_destroy(struct kgsl_context *context)
{
	struct adreno_context *drawctxt;
	struct adreno_device *adreno_dev;
	const struct adreno_gpudev *gpudev;

	if (context == NULL)
		return;

	drawctxt = ADRENO_CONTEXT(context);

	adreno_dev = ADRENO_DEVICE(context->device);
	gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (gpudev->context_destroy)
		gpudev->context_destroy(adreno_dev, drawctxt);
	kfree(drawctxt);
}

static void _drawctxt_switch_wait_callback(struct kgsl_device *device,
		struct kgsl_event_group *group,
		void *priv, int result)
{
	struct adreno_context *drawctxt = (struct adreno_context *) priv;

	kgsl_context_put(&drawctxt->base);
}

void adreno_put_drawctxt_on_timestamp(struct kgsl_device *device,
		struct adreno_context *drawctxt,
		struct adreno_ringbuffer *rb, u32 timestamp)
{
	if (!drawctxt)
		return;

	if (kgsl_add_event(device, &rb->events, timestamp,
		_drawctxt_switch_wait_callback, drawctxt))
		kgsl_context_put(&drawctxt->base);
}

static void _add_context(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt)
{
	/* Remove it from the list */
	list_del_init(&drawctxt->active_node);

	/* And push it to the front */
	drawctxt->active_time = jiffies;
	list_add(&drawctxt->active_node, &adreno_dev->active_list);
}

static int __count_context(struct adreno_context *drawctxt, void *data)
{
	unsigned long expires = drawctxt->active_time + msecs_to_jiffies(100);

	return time_after(jiffies, expires) ? 0 : 1;
}

static int __count_drawqueue_context(struct adreno_context *drawctxt,
				void *data)
{
	unsigned long expires = drawctxt->active_time + msecs_to_jiffies(100);

	if (time_after(jiffies, expires))
		return 0;

	return (&drawctxt->rb->dispatch_q ==
			(struct adreno_dispatcher_drawqueue *) data) ? 1 : 0;
}

static int _adreno_count_active_contexts(struct adreno_device *adreno_dev,
		int (*func)(struct adreno_context *, void *), void *data)
{
	struct adreno_context *ctxt;
	int count = 0;

	list_for_each_entry(ctxt, &adreno_dev->active_list, active_node) {
		if (func(ctxt, data) == 0)
			return count;

		count++;
	}

	return count;
}

void adreno_track_context(struct adreno_device *adreno_dev,
		struct adreno_dispatcher_drawqueue *drawqueue,
		struct adreno_context *drawctxt)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	spin_lock(&adreno_dev->active_list_lock);

	_add_context(adreno_dev, drawctxt);

	device->active_context_count =
			_adreno_count_active_contexts(adreno_dev,
					__count_context, NULL);

	if (drawqueue)
		drawqueue->active_context_count =
				_adreno_count_active_contexts(adreno_dev,
					__count_drawqueue_context, drawqueue);

	spin_unlock(&adreno_dev->active_list_lock);
}
