// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>

#if IS_ENABLED(CONFIG_QCOM_FSA4480_I2C)
#include <linux/soc/qcom/fsa4480-i2c.h>
#endif
#if IS_ENABLED(CONFIG_QCOM_WCD939X_I2C)
#include <linux/soc/qcom/wcd939x-i2c.h>
#endif

#include "dp_aux.h"
#include "dp_hpd.h"
#include "dp_debug.h"

#define DP_AUX_ENUM_STR(x)		#x
#define DP_AUX_IPC_NUM_PAGES 10

#define DP_AUX_DEBUG(dp_aux, fmt, ...) \
	do { \
		if (dp_aux) \
			ipc_log_string(dp_aux->ipc_log_context, "[d][%-4d]"fmt,\
					current->pid, ##__VA_ARGS__); \
		DP_DEBUG_V(fmt, ##__VA_ARGS__); \
	} while (0)

#define DP_AUX_WARN(dp_aux, fmt, ...) \
	do { \
		if (dp_aux) \
			ipc_log_string(dp_aux->ipc_log_context, "[w][%-4d]"fmt,\
					current->pid, ##__VA_ARGS__); \
		DP_WARN_V(fmt, ##__VA_ARGS__); \
	} while (0)

#define DP_AUX_WARN_RATELIMITED(dp_aux, fmt, ...) \
	do { \
		if (dp_aux) \
			ipc_log_string(dp_aux->ipc_log_context, "[w][%-4d]"fmt,\
					current->pid, ##__VA_ARGS__); \
		DP_WARN_RATELIMITED_V(fmt, ##__VA_ARGS__); \
	} while (0)

#define DP_AUX_ERR(dp_aux, fmt, ...) \
	do { \
		if (dp_aux) \
			ipc_log_string(dp_aux->ipc_log_context, "[e][%-4d]"fmt,\
					current->pid, ##__VA_ARGS__); \
		DP_ERR_V(fmt, ##__VA_ARGS__); \
	} while (0)

#define DP_AUX_ERR_RATELIMITED(dp_aux, fmt, ...) \
	do { \
		if (dp_aux) \
			ipc_log_string(dp_aux->ipc_log_context, "[e][%-4d]"fmt,\
					current->pid, ##__VA_ARGS__); \
		DP_ERR_RATELIMITED_V(fmt, ##__VA_ARGS__); \
	} while (0)

enum {
	DP_AUX_DATA_INDEX_WRITE = BIT(31),
};

struct dp_aux_private {
	struct device *dev;
	struct dp_aux dp_aux;
	struct dp_catalog_aux *catalog;
	struct dp_aux_cfg *cfg;
	struct device_node *aux_switch_node;
	struct mutex mutex;
	struct completion comp;
	struct drm_dp_aux drm_aux;

	struct dp_aux_bridge *aux_bridge;
	struct dp_aux_bridge *sim_bridge;
	bool bridge_in_transfer;
	bool sim_in_transfer;

	bool cmd_busy;
	bool native;
	bool read;
	bool no_send_addr;
	bool no_send_stop;
	bool enabled;

	u32 offset;
	u32 segment;
	u32 aux_error_num;
	u32 retry_cnt;

	bool switch_enable;
	int switch_orientation;

	atomic_t aborted;
};

static void dp_aux_hex_dump(struct drm_dp_aux *drm_aux,
		struct drm_dp_aux_msg *msg)
{
	char prefix[64];
	int i, linelen, remaining = msg->size;
	const int rowsize = 16;
	u8 linebuf[64];
	struct dp_aux_private *aux = container_of(drm_aux,
		struct dp_aux_private, drm_aux);
	struct dp_aux *dp_aux = &aux->dp_aux;

	snprintf(prefix, sizeof(prefix), "%s %s %4xh(%2zu): ",
		(msg->request & DP_AUX_I2C_MOT) ? "I2C" : "NAT",
		(msg->request & DP_AUX_I2C_READ) ? "RD" : "WR",
		msg->address, msg->size);

	for (i = 0; i < msg->size; i += rowsize) {
		linelen = min(remaining, rowsize);
		remaining -= rowsize;

		hex_dump_to_buffer(msg->buffer + i, linelen, rowsize, 1,
			linebuf, sizeof(linebuf), false);

		if (msg->size == 1 && msg->address == 0)
			DP_DEBUG_V("%s%s\n", prefix, linebuf);
		else
			DP_AUX_DEBUG(dp_aux, "%s%s\n", prefix, linebuf);
	}
}

static char *dp_aux_get_error(u32 aux_error)
{
	switch (aux_error) {
	case DP_AUX_ERR_NONE:
		return DP_AUX_ENUM_STR(DP_AUX_ERR_NONE);
	case DP_AUX_ERR_ADDR:
		return DP_AUX_ENUM_STR(DP_AUX_ERR_ADDR);
	case DP_AUX_ERR_TOUT:
		return DP_AUX_ENUM_STR(DP_AUX_ERR_TOUT);
	case DP_AUX_ERR_NACK:
		return DP_AUX_ENUM_STR(DP_AUX_ERR_NACK);
	case DP_AUX_ERR_DEFER:
		return DP_AUX_ENUM_STR(DP_AUX_ERR_DEFER);
	case DP_AUX_ERR_NACK_DEFER:
		return DP_AUX_ENUM_STR(DP_AUX_ERR_NACK_DEFER);
	default:
		return "unknown";
	}
}

static u32 dp_aux_write(struct dp_aux_private *aux,
		struct drm_dp_aux_msg *msg)
{
	u32 data[4], reg, len;
	u8 *msgdata = msg->buffer;
	int const aux_cmd_fifo_len = 128;
	int i = 0;
	struct dp_aux *dp_aux = &aux->dp_aux;

	if (aux->read)
		len = 4;
	else
		len = msg->size + 4;

	/*
	 * cmd fifo only has depth of 144 bytes
	 * limit buf length to 128 bytes here
	 */
	if (len > aux_cmd_fifo_len) {
		DP_AUX_ERR(dp_aux, "buf len error\n");
		return 0;
	}

	/* Pack cmd and write to HW */
	data[0] = (msg->address >> 16) & 0xf; /* addr[19:16] */
	if (aux->read)
		data[0] |=  BIT(4); /* R/W */

	data[1] = (msg->address >> 8) & 0xff;	/* addr[15:8] */
	data[2] = msg->address & 0xff;		/* addr[7:0] */
	data[3] = (msg->size - 1) & 0xff;	/* len[7:0] */

	for (i = 0; i < len; i++) {
		reg = (i < 4) ? data[i] : msgdata[i - 4];
		reg = ((reg) << 8) & 0x0000ff00; /* index = 0, write */
		if (i == 0)
			reg |= DP_AUX_DATA_INDEX_WRITE;
		aux->catalog->data = reg;
		aux->catalog->write_data(aux->catalog);
	}

	aux->catalog->clear_trans(aux->catalog, false);
	aux->catalog->clear_hw_interrupts(aux->catalog);

	reg = 0; /* Transaction number == 1 */
	if (!aux->native) { /* i2c */
		reg |= BIT(8);

		if (aux->no_send_addr)
			reg |= BIT(10);

		if (aux->no_send_stop)
			reg |= BIT(11);
	}

	reg |= BIT(9);
	aux->catalog->data = reg;
	aux->catalog->write_trans(aux->catalog);

	return len;
}

static int dp_aux_cmd_fifo_tx(struct dp_aux_private *aux,
		struct drm_dp_aux_msg *msg)
{
	u32 ret = 0, len = 0, timeout;
	int const aux_timeout_ms = HZ/4;
	struct dp_aux *dp_aux = &aux->dp_aux;
	char prefix[64];

	snprintf(prefix, sizeof(prefix), "%s %s %4xh(%2zu): ",
			(msg->request & DP_AUX_I2C_MOT) ? "I2C" : "NAT",
			(msg->request & DP_AUX_I2C_READ) ? "RD" : "WR",
			msg->address, msg->size);

	reinit_completion(&aux->comp);

	len = dp_aux_write(aux, msg);
	if (len == 0) {
		DP_AUX_ERR(dp_aux, "DP AUX write failed: %s\n", prefix);
		return -EINVAL;
	}

	timeout = wait_for_completion_timeout(&aux->comp, aux_timeout_ms);
	if (!timeout) {
		DP_AUX_WARN_RATELIMITED(dp_aux, "aux timeout during [%s]\n", prefix);
		return -ETIMEDOUT;
	}

	if (aux->aux_error_num == DP_AUX_ERR_NONE) {
		ret = len;
	} else {
		DP_AUX_WARN_RATELIMITED(dp_aux, "aux err [%s] during [%s]\n",
				dp_aux_get_error(aux->aux_error_num), prefix);
		ret = -EINVAL;
	}

	return ret;
}

static void dp_aux_cmd_fifo_rx(struct dp_aux_private *aux,
		struct drm_dp_aux_msg *msg)
{
	u32 data;
	u8 *dp;
	u32 i, actual_i;
	u32 len = msg->size;
	struct dp_aux *dp_aux = &aux->dp_aux;

	aux->catalog->clear_trans(aux->catalog, true);

	data = 0;
	data |= DP_AUX_DATA_INDEX_WRITE; /* INDEX_WRITE */
	data |= BIT(0);  /* read */

	aux->catalog->data = data;
	aux->catalog->write_data(aux->catalog);

	dp = msg->buffer;

	/* discard first byte */
	data = aux->catalog->read_data(aux->catalog);

	for (i = 0; i < len; i++) {
		data = aux->catalog->read_data(aux->catalog);
		*dp++ = (u8)((data >> 8) & 0xff);

		actual_i = (data >> 16) & 0xFF;
		if (i != actual_i)
			DP_AUX_WARN(dp_aux, "Index mismatch: expected %d, found %d\n",
				i, actual_i);
	}
}

static void dp_aux_native_handler(struct dp_aux_private *aux)
{
	u32 isr = aux->catalog->isr;

	if (isr & DP_INTR_AUX_I2C_DONE)
		aux->aux_error_num = DP_AUX_ERR_NONE;
	else if (isr & DP_INTR_WRONG_ADDR)
		aux->aux_error_num = DP_AUX_ERR_ADDR;
	else if (isr & DP_INTR_TIMEOUT)
		aux->aux_error_num = DP_AUX_ERR_TOUT;
	if (isr & DP_INTR_NACK_DEFER)
		aux->aux_error_num = DP_AUX_ERR_NACK;
	if (isr & DP_INTR_AUX_ERROR) {
		aux->aux_error_num = DP_AUX_ERR_PHY;
		aux->catalog->clear_hw_interrupts(aux->catalog);
	}

	complete(&aux->comp);
}

static void dp_aux_i2c_handler(struct dp_aux_private *aux)
{
	u32 isr = aux->catalog->isr;

	if (isr & DP_INTR_AUX_I2C_DONE) {
		if (isr & (DP_INTR_I2C_NACK | DP_INTR_I2C_DEFER))
			aux->aux_error_num = DP_AUX_ERR_NACK;
		else
			aux->aux_error_num = DP_AUX_ERR_NONE;
	} else {
		if (isr & DP_INTR_WRONG_ADDR)
			aux->aux_error_num = DP_AUX_ERR_ADDR;
		else if (isr & DP_INTR_TIMEOUT)
			aux->aux_error_num = DP_AUX_ERR_TOUT;
		if (isr & DP_INTR_NACK_DEFER)
			aux->aux_error_num = DP_AUX_ERR_NACK_DEFER;
		if (isr & DP_INTR_I2C_NACK)
			aux->aux_error_num = DP_AUX_ERR_NACK;
		if (isr & DP_INTR_I2C_DEFER)
			aux->aux_error_num = DP_AUX_ERR_DEFER;
		if (isr & DP_INTR_AUX_ERROR) {
			aux->aux_error_num = DP_AUX_ERR_PHY;
			aux->catalog->clear_hw_interrupts(aux->catalog);
		}
	}

	complete(&aux->comp);
}

static void dp_aux_isr(struct dp_aux *dp_aux)
{
	struct dp_aux_private *aux;

	if (!dp_aux) {
		DP_AUX_ERR(dp_aux, "invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	aux->catalog->get_irq(aux->catalog, aux->cmd_busy);

	if (!aux->cmd_busy)
		return;

	if (aux->native)
		dp_aux_native_handler(aux);
	else
		dp_aux_i2c_handler(aux);
}

static void dp_aux_reconfig(struct dp_aux *dp_aux)
{
	struct dp_aux_private *aux;

	if (!dp_aux) {
		DP_AUX_ERR(dp_aux, "invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	aux->catalog->update_aux_cfg(aux->catalog,
			aux->cfg, PHY_AUX_CFG1);
	aux->catalog->reset(aux->catalog);
}

static void dp_aux_abort_transaction(struct dp_aux *dp_aux, bool abort)
{
	struct dp_aux_private *aux;

	if (!dp_aux) {
		DP_AUX_ERR(dp_aux, "invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	atomic_set(&aux->aborted, abort);
}

static void dp_aux_update_offset_and_segment(struct dp_aux_private *aux,
		struct drm_dp_aux_msg *input_msg)
{
	u32 const edid_address = 0x50;
	u32 const segment_address = 0x30;
	bool i2c_read = input_msg->request &
		(DP_AUX_I2C_READ & DP_AUX_NATIVE_READ);
	u8 *data = NULL;

	if (aux->native || i2c_read || ((input_msg->address != edid_address) &&
		(input_msg->address != segment_address)))
		return;


	data = input_msg->buffer;
	if (input_msg->address == segment_address)
		aux->segment = *data;
	else
		aux->offset = *data;
}

/**
 * dp_aux_transfer_helper() - helper function for EDID read transactions
 *
 * @aux: DP AUX private structure
 * @input_msg: input message from DRM upstream APIs
 * @send_seg: send the seg to sink
 *
 * return: void
 *
 * This helper function is used to fix EDID reads for non-compliant
 * sinks that do not handle the i2c middle-of-transaction flag correctly.
 */
static void dp_aux_transfer_helper(struct dp_aux_private *aux,
		struct drm_dp_aux_msg *input_msg, bool send_seg)
{
	struct drm_dp_aux_msg helper_msg;
	u32 const message_size = 0x10;
	u32 const segment_address = 0x30;
	u32 const edid_block_length = 0x80;
	bool i2c_mot = input_msg->request & DP_AUX_I2C_MOT;
	bool i2c_read = input_msg->request &
		(DP_AUX_I2C_READ & DP_AUX_NATIVE_READ);

	if (!i2c_mot || !i2c_read || (input_msg->size == 0))
		return;

	/*
	 * Sending the segment value and EDID offset will be performed
	 * from the DRM upstream EDID driver for each block. Avoid
	 * duplicate AUX transactions related to this while reading the
	 * first 16 bytes of each block.
	 */
	if (!(aux->offset % edid_block_length) || !send_seg)
		goto end;

	aux->read = false;
	aux->cmd_busy = true;
	aux->no_send_addr = true;
	aux->no_send_stop = true;

	/*
	 * Send the segment address for i2c reads for segment > 0 and for which
	 * the middle-of-transaction flag is set. This is required to support
	 * EDID reads of more than 2 blocks as the segment address is reset to 0
	 * since we are overriding the middle-of-transaction flag for read
	 * transactions.
	 */
	if (aux->segment) {
		memset(&helper_msg, 0, sizeof(helper_msg));
		helper_msg.address = segment_address;
		helper_msg.buffer = &aux->segment;
		helper_msg.size = 1;
		dp_aux_cmd_fifo_tx(aux, &helper_msg);
	}

	/*
	 * Send the offset address for every i2c read in which the
	 * middle-of-transaction flag is set. This will ensure that the sink
	 * will update its read pointer and return the correct portion of the
	 * EDID buffer in the subsequent i2c read trasntion triggered in the
	 * native AUX transfer function.
	 */
	memset(&helper_msg, 0, sizeof(helper_msg));
	helper_msg.address = input_msg->address;
	helper_msg.buffer = &aux->offset;
	helper_msg.size = 1;
	dp_aux_cmd_fifo_tx(aux, &helper_msg);
end:
	aux->offset += message_size;
	if (aux->offset == 0x80 || aux->offset == 0x100)
		aux->segment = 0x0; /* reset segment at end of block */
}

static int dp_aux_transfer_ready(struct dp_aux_private *aux,
		struct drm_dp_aux_msg *msg, bool send_seg)
{
	int ret = 0;
	int const aux_cmd_native_max = 16;
	int const aux_cmd_i2c_max = 128;
	struct dp_aux *dp_aux = &aux->dp_aux;

	if (atomic_read(&aux->aborted)) {
		ret = -ETIMEDOUT;
		goto error;
	}

	aux->native = msg->request & (DP_AUX_NATIVE_WRITE & DP_AUX_NATIVE_READ);

	/* Ignore address only message */
	if ((msg->size == 0) || (msg->buffer == NULL)) {
		msg->reply = aux->native ?
			DP_AUX_NATIVE_REPLY_ACK : DP_AUX_I2C_REPLY_ACK;
		goto error;
	}

	/* msg sanity check */
	if ((aux->native && (msg->size > aux_cmd_native_max)) ||
		(msg->size > aux_cmd_i2c_max)) {
		DP_AUX_ERR(dp_aux, "%s: invalid msg: size(%zu), request(%x)\n",
			__func__, msg->size, msg->request);
		ret = -EINVAL;
		goto error;
	}

	dp_aux_update_offset_and_segment(aux, msg);

	dp_aux_transfer_helper(aux, msg, send_seg);

	aux->read = msg->request & (DP_AUX_I2C_READ & DP_AUX_NATIVE_READ);

	if (aux->read) {
		aux->no_send_addr = true;
		aux->no_send_stop = false;
	} else {
		aux->no_send_addr = true;
		aux->no_send_stop = true;
	}

	aux->cmd_busy = true;
error:
	return ret;
}

static inline bool dp_aux_is_sideband_msg(u32 address, size_t size)
{
	return (address >= 0x1000 && address + size < 0x1800) ||
			(address >= 0x2000 && address + size < 0x2200);
}

/*
 * This function does the real job to process an AUX transaction.
 * It will call aux_reset() function to reset the AUX channel,
 * if the waiting is timeout.
 */
static ssize_t dp_aux_transfer(struct drm_dp_aux *drm_aux,
		struct drm_dp_aux_msg *msg)
{
	ssize_t ret;
	int const retry_count = 5;
	struct dp_aux_private *aux = container_of(drm_aux,
		struct dp_aux_private, drm_aux);

	mutex_lock(&aux->mutex);

	ret = dp_aux_transfer_ready(aux, msg, true);
	if (ret)
		goto unlock_exit;

	if (!aux->cmd_busy) {
		ret = msg->size;
		goto unlock_exit;
	}

	ret = dp_aux_cmd_fifo_tx(aux, msg);
	if ((ret < 0) && !atomic_read(&aux->aborted)) {
		aux->retry_cnt++;
		if (!(aux->retry_cnt % retry_count))
			aux->catalog->update_aux_cfg(aux->catalog,
				aux->cfg, PHY_AUX_CFG1);
		aux->catalog->reset(aux->catalog);
		goto unlock_exit;
	} else if (ret < 0) {
		goto unlock_exit;
	}

	if (aux->aux_error_num == DP_AUX_ERR_NONE) {
		if (aux->read)
			dp_aux_cmd_fifo_rx(aux, msg);

		dp_aux_hex_dump(drm_aux, msg);

		msg->reply = aux->native ?
			DP_AUX_NATIVE_REPLY_ACK : DP_AUX_I2C_REPLY_ACK;
	} else {
		/* Reply defer to retry */
		msg->reply = aux->native ?
			DP_AUX_NATIVE_REPLY_DEFER : DP_AUX_I2C_REPLY_DEFER;
	}

	/* Return requested size for success or retry */
	ret = msg->size;
	aux->retry_cnt = 0;

unlock_exit:
	aux->cmd_busy = false;
	mutex_unlock(&aux->mutex);
	return ret;
}

static ssize_t dp_aux_bridge_transfer(struct drm_dp_aux *drm_aux,
		struct drm_dp_aux_msg *msg)
{
	struct dp_aux_private *aux = container_of(drm_aux,
			struct dp_aux_private, drm_aux);
	ssize_t size;

	if (aux->bridge_in_transfer) {
		size = dp_aux_transfer(drm_aux, msg);
	} else {
		aux->bridge_in_transfer = true;
		size = aux->aux_bridge->transfer(aux->aux_bridge,
				drm_aux, msg);
		aux->bridge_in_transfer = false;
		dp_aux_hex_dump(drm_aux, msg);
	}

	return size;
}

static ssize_t dp_aux_transfer_debug(struct drm_dp_aux *drm_aux,
		struct drm_dp_aux_msg *msg)
{
	struct dp_aux_private *aux = container_of(drm_aux,
			struct dp_aux_private, drm_aux);
	ssize_t size;
	int aborted;

	mutex_lock(&aux->mutex);
	aborted = atomic_read(&aux->aborted);
	mutex_unlock(&aux->mutex);
	if (aborted) {
		size = -ETIMEDOUT;
		goto end;
	}

	if (aux->sim_in_transfer) {
		if (aux->aux_bridge && aux->aux_bridge->transfer)
			size = dp_aux_bridge_transfer(drm_aux, msg);
		else
			size = dp_aux_transfer(drm_aux, msg);
	} else {
		aux->sim_in_transfer = true;
		size = aux->sim_bridge->transfer(aux->sim_bridge,
				drm_aux, msg);
		aux->sim_in_transfer = false;
		dp_aux_hex_dump(drm_aux, msg);
	}
end:
	return size;
}

static void dp_aux_reset_phy_config_indices(struct dp_aux_cfg *aux_cfg)
{
	int i = 0;

	for (i = 0; i < PHY_AUX_CFG_MAX; i++)
		aux_cfg[i].current_index = 0;
}

static void dp_aux_init(struct dp_aux *dp_aux, struct dp_aux_cfg *aux_cfg)
{
	struct dp_aux_private *aux;

	if (!dp_aux || !aux_cfg) {
		DP_AUX_ERR(dp_aux, "invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	if (aux->enabled)
		return;

	dp_aux_reset_phy_config_indices(aux_cfg);
	aux->catalog->setup(aux->catalog, aux_cfg);
	aux->catalog->reset(aux->catalog);
	aux->catalog->enable(aux->catalog, true);
	atomic_set(&aux->aborted, 0);
	aux->retry_cnt = 0;
	aux->enabled = true;
}

static void dp_aux_deinit(struct dp_aux *dp_aux)
{
	struct dp_aux_private *aux;

	if (!dp_aux) {
		DP_AUX_ERR(dp_aux, "invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	if (!aux->enabled)
		return;

	atomic_set(&aux->aborted, 1);
	aux->catalog->enable(aux->catalog, false);
	aux->enabled = false;
}

static int dp_aux_register(struct dp_aux *dp_aux, struct drm_device *drm_dev)
{
	struct dp_aux_private *aux;
	int ret = 0;

	if (!dp_aux) {
		DP_AUX_ERR(dp_aux, "invalid input\n");
		ret = -EINVAL;
		goto exit;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	aux->drm_aux.name = "sde_dp_aux";
	aux->drm_aux.dev = aux->dev;
	aux->drm_aux.transfer = dp_aux_transfer;
#if (KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE)
	aux->drm_aux.drm_dev = drm_dev;
#endif
	atomic_set(&aux->aborted, 1);
	ret = drm_dp_aux_register(&aux->drm_aux);
	if (ret) {
		DP_AUX_ERR(dp_aux, "%s: failed to register drm aux: %d\n", __func__, ret);
		goto exit;
	}
	dp_aux->drm_aux = &aux->drm_aux;

	/* if bridge is defined, override transfer function */
	if (aux->aux_bridge && aux->aux_bridge->transfer)
		aux->drm_aux.transfer = dp_aux_bridge_transfer;
exit:
	return ret;
}

static void dp_aux_deregister(struct dp_aux *dp_aux)
{
	struct dp_aux_private *aux;

	if (!dp_aux) {
		DP_AUX_ERR(dp_aux, "invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);
	drm_dp_aux_unregister(&aux->drm_aux);
}

static void dp_aux_set_sim_mode(struct dp_aux *dp_aux,
		struct dp_aux_bridge *sim_bridge)
{
	struct dp_aux_private *aux;

	if (!dp_aux) {
		DP_AUX_ERR(dp_aux, "invalid input\n");
		return;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	mutex_lock(&aux->mutex);

	aux->sim_bridge = sim_bridge;

	if (sim_bridge) {
		atomic_set(&aux->aborted, 0);
		aux->drm_aux.transfer = dp_aux_transfer_debug;
	} else if (aux->aux_bridge && aux->aux_bridge->transfer) {
		aux->drm_aux.transfer = dp_aux_bridge_transfer;
	} else {
		aux->drm_aux.transfer = dp_aux_transfer;
	}

	mutex_unlock(&aux->mutex);
}

#if IS_ENABLED(CONFIG_QCOM_FSA4480_I2C)
static int dp_aux_configure_fsa_switch(struct dp_aux *dp_aux,
		bool enable, int orientation)
{
	struct dp_aux_private *aux;
	int rc = 0;
	enum fsa_function event = FSA_USBC_DISPLAYPORT_DISCONNECTED;

	if (!dp_aux) {
		DP_AUX_ERR(dp_aux, "invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	if (!aux->aux_switch_node) {
		DP_AUX_DEBUG(dp_aux, "undefined fsa4480 handle\n");
		rc = -EINVAL;
		goto end;
	}

	if (enable) {
		switch (orientation) {
		case ORIENTATION_CC1:
			event = FSA_USBC_ORIENTATION_CC1;
			break;
		case ORIENTATION_CC2:
			event = FSA_USBC_ORIENTATION_CC2;
			break;
		default:
			DP_AUX_ERR(dp_aux, "invalid orientation\n");
			rc = -EINVAL;
			goto end;
		}
	}

	DP_AUX_DEBUG(dp_aux, "enable=%d, orientation=%d, event=%d\n",
			enable, orientation, event);

	rc = fsa4480_switch_event(aux->aux_switch_node, event);

	if (rc)
		DP_AUX_ERR(dp_aux, "failed to configure fsa4480 i2c device (%d)\n", rc);
end:
	return rc;
}
#endif

#if IS_ENABLED(CONFIG_QCOM_WCD939X_I2C)
static int dp_aux_configure_wcd_switch(struct dp_aux *dp_aux,
		bool enable, int orientation)
{
	struct dp_aux_private *aux;
	int rc = 0;
	enum wcd_usbss_cable_status status = WCD_USBSS_CABLE_DISCONNECT;
	enum wcd_usbss_cable_types event = WCD_USBSS_DP_AUX_CC1;

	if (!dp_aux) {
		DP_AUX_ERR(dp_aux, "invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	if (!aux->aux_switch_node) {
		DP_AUX_DEBUG(dp_aux, "undefined wcd939x switch handle\n");
		rc = -EINVAL;
		goto end;
	}

	if ((aux->switch_enable == enable) && (aux->switch_orientation == orientation))
		goto end;

	if (enable) {
		status = WCD_USBSS_CABLE_CONNECT;

		switch (orientation) {
		case ORIENTATION_CC1:
			event = WCD_USBSS_DP_AUX_CC1;
			break;
		case ORIENTATION_CC2:
			event = WCD_USBSS_DP_AUX_CC2;
			break;
		default:
			DP_AUX_ERR(dp_aux, "invalid orientation\n");
			rc = -EINVAL;
			goto end;
		}
	}

	DP_AUX_DEBUG(dp_aux, "enable=%d, orientation=%d, event=%d\n",
			enable, orientation, event);

	rc = wcd_usbss_switch_update(event, status);
	if (rc) {
		DP_AUX_ERR(dp_aux, "failed to configure wcd939x i2c device (%d)\n", rc);
	} else {
		aux->switch_enable = enable;
		aux->switch_orientation = orientation;
	}
end:
	return rc;
}
#endif

struct dp_aux *dp_aux_get(struct device *dev, struct dp_catalog_aux *catalog,
		struct dp_parser *parser, struct device_node *aux_switch,
		struct dp_aux_bridge *aux_bridge, void *ipc_log_context,
		enum dp_aux_switch_type switch_type)
{
	int rc = 0;
	struct dp_aux_private *aux;
	struct dp_aux *dp_aux = NULL;

	if (!catalog || !parser) {
		DP_AUX_ERR(dp_aux, "invalid input\n");
		rc = -ENODEV;
		goto error;
	}

	aux = devm_kzalloc(dev, sizeof(*aux), GFP_KERNEL);
	if (!aux) {
		rc = -ENOMEM;
		goto error;
	}

	init_completion(&aux->comp);
	aux->cmd_busy = false;
	mutex_init(&aux->mutex);

	aux->dev = dev;
	aux->catalog = catalog;
	aux->cfg = parser->aux_cfg;
	aux->aux_switch_node = aux_switch;
	aux->aux_bridge = aux_bridge;
	dp_aux = &aux->dp_aux;
	aux->retry_cnt = 0;
	aux->switch_orientation = -1;

	dp_aux->isr     = dp_aux_isr;
	dp_aux->init    = dp_aux_init;
	dp_aux->deinit  = dp_aux_deinit;
	dp_aux->drm_aux_register = dp_aux_register;
	dp_aux->drm_aux_deregister = dp_aux_deregister;
	dp_aux->reconfig = dp_aux_reconfig;
	dp_aux->abort = dp_aux_abort_transaction;
	dp_aux->set_sim_mode = dp_aux_set_sim_mode;
	dp_aux->ipc_log_context = ipc_log_context;

	/*Condition to avoid allocating function pointers for aux bypass mode*/
	if (switch_type != DP_AUX_SWITCH_BYPASS) {
#if IS_ENABLED(CONFIG_QCOM_FSA4480_I2C)
		if (switch_type == DP_AUX_SWITCH_FSA4480) {
			dp_aux->switch_configure = dp_aux_configure_fsa_switch;
			dp_aux->switch_register_notifier = fsa4480_reg_notifier;
			dp_aux->switch_unregister_notifier = fsa4480_unreg_notifier;
		}
#endif
#if IS_ENABLED(CONFIG_QCOM_WCD939X_I2C)
		if (switch_type == DP_AUX_SWITCH_WCD939x) {
			dp_aux->switch_configure = dp_aux_configure_wcd_switch;
			dp_aux->switch_register_notifier = wcd_usbss_reg_notifier;
			dp_aux->switch_unregister_notifier = wcd_usbss_unreg_notifier;
		}
#endif
	}

	return dp_aux;
error:
	return ERR_PTR(rc);
}

void dp_aux_put(struct dp_aux *dp_aux)
{
	struct dp_aux_private *aux;

	if (!dp_aux)
		return;

	aux = container_of(dp_aux, struct dp_aux_private, dp_aux);

	mutex_destroy(&aux->mutex);

	devm_kfree(aux->dev, aux);
}
