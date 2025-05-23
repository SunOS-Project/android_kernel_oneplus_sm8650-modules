/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef DP_TX_DESC_H
#define DP_TX_DESC_H

#include "dp_types.h"
#include "dp_tx.h"
#include "dp_internal.h"

/*
 * 21 bits cookie
 * 1 bit special pool indicator
 * 3 bits unused
 * 2 bits pool id 0 ~ 3,
 * 10 bits page id 0 ~ 1023
 * 5 bits offset id 0 ~ 31 (Desc size = 128, Num descs per page = 4096/128 = 32)
 */
/* ???Ring ID needed??? */

#define DP_TX_DESC_ID_SPCL_MASK    0x100000
#define DP_TX_DESC_ID_SPCL_OS      20
#define DP_TX_DESC_ID_POOL_MASK    0x018000
#define DP_TX_DESC_ID_POOL_OS      15
#define DP_TX_DESC_ID_PAGE_MASK    0x007FE0
#define DP_TX_DESC_ID_PAGE_OS      5
#define DP_TX_DESC_ID_OFFSET_MASK  0x00001F
#define DP_TX_DESC_ID_OFFSET_OS    0

/*
 * Compilation assert on tx desc size
 *
 * if assert is hit please update POOL_MASK,
 * PAGE_MASK according to updated size
 *
 * for current PAGE mask allowed size range of tx_desc
 * is between 128 and 256
 */
QDF_COMPILE_TIME_ASSERT(dp_tx_desc_size,
			((sizeof(struct dp_tx_desc_s)) <=
			 (DP_BLOCKMEM_SIZE >> DP_TX_DESC_ID_PAGE_OS)) &&
			((sizeof(struct dp_tx_desc_s)) >
			 (DP_BLOCKMEM_SIZE >> (DP_TX_DESC_ID_PAGE_OS + 1)))
		       );

#ifdef QCA_LL_TX_FLOW_CONTROL_V2
#define TX_DESC_LOCK_CREATE(lock)
#define TX_DESC_LOCK_DESTROY(lock)
#define TX_DESC_LOCK_LOCK(lock)
#define TX_DESC_LOCK_UNLOCK(lock)
#define IS_TX_DESC_POOL_STATUS_INACTIVE(pool) \
	((pool)->status == FLOW_POOL_INACTIVE)
#ifdef QCA_AC_BASED_FLOW_CONTROL
#define TX_DESC_POOL_MEMBER_CLEAN(_tx_desc_pool)       \
	dp_tx_flow_pool_member_clean(_tx_desc_pool)

#else /* !QCA_AC_BASED_FLOW_CONTROL */
#define TX_DESC_POOL_MEMBER_CLEAN(_tx_desc_pool)       \
do {                                                   \
	(_tx_desc_pool)->elem_size = 0;                \
	(_tx_desc_pool)->freelist = NULL;              \
	(_tx_desc_pool)->pool_size = 0;                \
	(_tx_desc_pool)->avail_desc = 0;               \
	(_tx_desc_pool)->start_th = 0;                 \
	(_tx_desc_pool)->stop_th = 0;                  \
	(_tx_desc_pool)->status = FLOW_POOL_INACTIVE;  \
} while (0)
#endif /* QCA_AC_BASED_FLOW_CONTROL */
#else /* !QCA_LL_TX_FLOW_CONTROL_V2 */
#define TX_DESC_LOCK_CREATE(lock)  qdf_spinlock_create(lock)
#define TX_DESC_LOCK_DESTROY(lock) qdf_spinlock_destroy(lock)
#define TX_DESC_LOCK_LOCK(lock)    qdf_spin_lock_bh(lock)
#define TX_DESC_LOCK_UNLOCK(lock)  qdf_spin_unlock_bh(lock)
#define IS_TX_DESC_POOL_STATUS_INACTIVE(pool) (false)
#define TX_DESC_POOL_MEMBER_CLEAN(_tx_desc_pool)       \
do {                                                   \
	(_tx_desc_pool)->elem_size = 0;                \
	(_tx_desc_pool)->num_allocated = 0;            \
	(_tx_desc_pool)->freelist = NULL;              \
	(_tx_desc_pool)->elem_count = 0;               \
	(_tx_desc_pool)->num_free = 0;                 \
} while (0)
#endif /* !QCA_LL_TX_FLOW_CONTROL_V2 */
#define MAX_POOL_BUFF_COUNT 10000

#ifdef DP_TX_TRACKING
static inline void dp_tx_desc_set_magic(struct dp_tx_desc_s *tx_desc,
					uint32_t magic_pattern)
{
	tx_desc->magic = magic_pattern;
}
#else
static inline void dp_tx_desc_set_magic(struct dp_tx_desc_s *tx_desc,
					uint32_t magic_pattern)
{
}
#endif

/**
 * dp_tx_desc_pool_alloc() - Allocate Tx Descriptor pool(s)
 * @soc: Handle to DP SoC structure
 * @pool_id: pool to allocate
 * @num_elem: Number of descriptor elements per pool
 * @spcl_tx_desc: if special desc
 *
 * This function allocates memory for SW tx descriptors
 * (used within host for tx data path).
 * The number of tx descriptors required will be large
 * since based on number of clients (1024 clients x 3 radios),
 * outstanding MSDUs stored in TQM queues and LMAC queues will be significantly
 * large.
 *
 * To avoid allocating a large contiguous memory, it uses multi_page_alloc qdf
 * function to allocate memory
 * in multiple pages. It then iterates through the memory allocated across pages
 * and links each descriptor
 * to next descriptor, taking care of page boundaries.
 *
 * Since WiFi 3.0 HW supports multiple Tx rings, multiple pools are allocated,
 * one for each ring;
 * This minimizes lock contention when hard_start_xmit is called
 * from multiple CPUs.
 * Alternately, multiple pools can be used for multiple VDEVs for VDEV level
 * flow control.
 *
 * Return: Status code. 0 for success.
 */
QDF_STATUS dp_tx_desc_pool_alloc(struct dp_soc *soc, uint8_t pool_id,
				 uint32_t num_elem, bool spcl_tx_desc);

/**
 * dp_tx_desc_pool_init() - Initialize Tx Descriptor pool(s)
 * @soc: Handle to DP SoC structure
 * @pool_id: pool to allocate
 * @num_elem: Number of descriptor elements per pool
 * @spcl_tx_desc: if special desc
 *
 * Return: QDF_STATUS_SUCCESS
 *	   QDF_STATUS_E_FAULT
 */
QDF_STATUS dp_tx_desc_pool_init(struct dp_soc *soc, uint8_t pool_id,
				uint32_t num_elem, bool spcl_tx_desc);

/**
 * dp_tx_desc_pool_free() -  Free the tx dexcriptor pools
 * @soc: Handle to DP SoC structure
 * @pool_id: pool to free
 * @spcl_tx_desc: if special desc
 *
 */
void dp_tx_desc_pool_free(struct dp_soc *soc, uint8_t pool_id,
			  bool spcl_tx_desc);

/**
 * dp_tx_desc_pool_deinit() - de-initialize Tx Descriptor pool(s)
 * @soc: Handle to DP SoC structure
 * @pool_id: pool to de-initialize
 * @spcl_tx_desc: if special desc
 *
 */
void dp_tx_desc_pool_deinit(struct dp_soc *soc, uint8_t pool_id,
			    bool spcl_tx_desc);

/**
 * dp_tx_ext_desc_pool_alloc_by_id() - allocate TX extension Descriptor pool
 *                                     based on pool ID
 * @soc: Handle to DP SoC structure
 * @num_elem: Number of descriptor elements per pool
 * @pool_id: Pool ID
 *
 * Return - QDF_STATUS_SUCCESS
 *	    QDF_STATUS_E_NOMEM
 */
QDF_STATUS dp_tx_ext_desc_pool_alloc_by_id(struct dp_soc *soc,
					   uint32_t num_elem,
					   uint8_t pool_id);
/**
 * dp_tx_ext_desc_pool_alloc() - allocate Tx extension Descriptor pool(s)
 * @soc: Handle to DP SoC structure
 * @num_pool: Number of pools to allocate
 * @num_elem: Number of descriptor elements per pool
 *
 * Return: QDF_STATUS_SUCCESS
 *	   QDF_STATUS_E_NOMEM
 */
QDF_STATUS dp_tx_ext_desc_pool_alloc(struct dp_soc *soc, uint8_t num_pool,
				     uint32_t num_elem);

/**
 * dp_tx_ext_desc_pool_init_by_id() - initialize Tx extension Descriptor pool
 *                                    based on pool ID
 * @soc: Handle to DP SoC structure
 * @num_elem: Number of descriptor elements per pool
 * @pool_id: Pool ID
 *
 * Return - QDF_STATUS_SUCCESS
 *	    QDF_STATUS_E_FAULT
 */
QDF_STATUS dp_tx_ext_desc_pool_init_by_id(struct dp_soc *soc, uint32_t num_elem,
					  uint8_t pool_id);

/**
 * dp_tx_ext_desc_pool_init() - initialize Tx extension Descriptor pool(s)
 * @soc: Handle to DP SoC structure
 * @num_pool: Number of pools to initialize
 * @num_elem: Number of descriptor elements per pool
 *
 * Return: QDF_STATUS_SUCCESS
 *	   QDF_STATUS_E_NOMEM
 */
QDF_STATUS dp_tx_ext_desc_pool_init(struct dp_soc *soc, uint8_t num_pool,
				    uint32_t num_elem);

/**
 * dp_tx_ext_desc_pool_free_by_id() - free TX extension Descriptor pool
 *                                    based on pool ID
 * @soc: Handle to DP SoC structure
 * @pool_id: Pool ID
 *
 */
void dp_tx_ext_desc_pool_free_by_id(struct dp_soc *soc, uint8_t pool_id);

/**
 * dp_tx_ext_desc_pool_free() -  free Tx extension Descriptor pool(s)
 * @soc: Handle to DP SoC structure
 * @num_pool: Number of pools to free
 *
 */
void dp_tx_ext_desc_pool_free(struct dp_soc *soc, uint8_t num_pool);

/**
 * dp_tx_ext_desc_pool_deinit_by_id() - deinit Tx extension Descriptor pool
 *                                      based on pool ID
 * @soc: Handle to DP SoC structure
 * @pool_id: Pool ID
 *
 */
void dp_tx_ext_desc_pool_deinit_by_id(struct dp_soc *soc, uint8_t pool_id);

/**
 * dp_tx_ext_desc_pool_deinit() -  deinit Tx extension Descriptor pool(s)
 * @soc: Handle to DP SoC structure
 * @num_pool: Number of pools to de-initialize
 *
 */
void dp_tx_ext_desc_pool_deinit(struct dp_soc *soc, uint8_t num_pool);

/**
 * dp_tx_tso_desc_pool_alloc_by_id() - allocate TSO Descriptor pool based
 *                                     on pool ID
 * @soc: Handle to DP SoC structure
 * @num_elem: Number of descriptor elements per pool
 * @pool_id: Pool ID
 *
 * Return - QDF_STATUS_SUCCESS
 *	    QDF_STATUS_E_NOMEM
 */
QDF_STATUS dp_tx_tso_desc_pool_alloc_by_id(struct dp_soc *soc, uint32_t num_elem,
					   uint8_t pool_id);

/**
 * dp_tx_tso_desc_pool_alloc() - allocate TSO Descriptor pool(s)
 * @soc: Handle to DP SoC structure
 * @num_pool: Number of pools to allocate
 * @num_elem: Number of descriptor elements per pool
 *
 * Return: QDF_STATUS_SUCCESS
 *	   QDF_STATUS_E_NOMEM
 */
QDF_STATUS dp_tx_tso_desc_pool_alloc(struct dp_soc *soc, uint8_t num_pool,
				     uint32_t num_elem);

/**
 * dp_tx_tso_desc_pool_init_by_id() - initialize TSO Descriptor pool
 *                                    based on pool ID
 * @soc: Handle to DP SoC structure
 * @num_elem: Number of descriptor elements per pool
 * @pool_id: Pool ID
 *
 * Return - QDF_STATUS_SUCCESS
 *	    QDF_STATUS_E_NOMEM
 */
QDF_STATUS dp_tx_tso_desc_pool_init_by_id(struct dp_soc *soc, uint32_t num_elem,
					  uint8_t pool_id);

/**
 * dp_tx_tso_desc_pool_init() - initialize TSO Descriptor pool(s)
 * @soc: Handle to DP SoC structure
 * @num_pool: Number of pools to initialize
 * @num_elem: Number of descriptor elements per pool
 *
 * Return: QDF_STATUS_SUCCESS
 *	   QDF_STATUS_E_NOMEM
 */
QDF_STATUS dp_tx_tso_desc_pool_init(struct dp_soc *soc, uint8_t num_pool,
				    uint32_t num_elem);

/**
 * dp_tx_tso_desc_pool_free_by_id() - free TSO Descriptor pool based on pool ID
 * @soc: Handle to DP SoC structure
 * @pool_id: Pool ID
 */
void dp_tx_tso_desc_pool_free_by_id(struct dp_soc *soc, uint8_t pool_id);

/**
 * dp_tx_tso_desc_pool_free() - free TSO Descriptor pool(s)
 * @soc: Handle to DP SoC structure
 * @num_pool: Number of pools to free
 *
 */
void dp_tx_tso_desc_pool_free(struct dp_soc *soc, uint8_t num_pool);

/**
 * dp_tx_tso_desc_pool_deinit_by_id() - deinitialize TSO Descriptor pool
 *                                      based on pool ID
 * @soc: Handle to DP SoC structure
 * @pool_id: Pool ID
 */
void dp_tx_tso_desc_pool_deinit_by_id(struct dp_soc *soc, uint8_t pool_id);

/**
 * dp_tx_tso_desc_pool_deinit() - deinitialize TSO Descriptor pool(s)
 * @soc: Handle to DP SoC structure
 * @num_pool: Number of pools to free
 *
 */
void dp_tx_tso_desc_pool_deinit(struct dp_soc *soc, uint8_t num_pool);

/**
 * dp_tx_tso_num_seg_pool_alloc_by_id() - Allocate descriptors that tracks the
 *                             fragments in each tso segment based on pool ID
 * @soc: handle to dp soc structure
 * @num_elem: total number of descriptors to be allocated
 * @pool_id: Pool ID
 *
 * Return - QDF_STATUS_SUCCESS
 *	    QDF_STATUS_E_NOMEM
 */
QDF_STATUS dp_tx_tso_num_seg_pool_alloc_by_id(struct dp_soc *soc,
					      uint32_t num_elem,
					      uint8_t pool_id);

/**
 * dp_tx_tso_num_seg_pool_alloc() - Allocate descriptors that tracks the
 *                              fragments in each tso segment
 *
 * @soc: handle to dp soc structure
 * @num_pool: number of pools to allocate
 * @num_elem: total number of descriptors to be allocated
 *
 * Return: QDF_STATUS_SUCCESS
 *	   QDF_STATUS_E_NOMEM
 */
QDF_STATUS dp_tx_tso_num_seg_pool_alloc(struct dp_soc *soc, uint8_t num_pool,
					uint32_t num_elem);

/**
 * dp_tx_tso_num_seg_pool_init_by_id() - Initialize descriptors that tracks the
 *                              fragments in each tso segment based on pool ID
 *
 * @soc: handle to dp soc structure
 * @num_elem: total number of descriptors to be initialized
 * @pool_id: Pool ID
 *
 * Return - QDF_STATUS_SUCCESS
 *	    QDF_STATUS_E_FAULT
 */
QDF_STATUS dp_tx_tso_num_seg_pool_init_by_id(struct dp_soc *soc,
					     uint32_t num_elem,
					     uint8_t pool_id);

/**
 * dp_tx_tso_num_seg_pool_init() - Initialize descriptors that tracks the
 *                              fragments in each tso segment
 *
 * @soc: handle to dp soc structure
 * @num_pool: number of pools to initialize
 * @num_elem: total number of descriptors to be initialized
 *
 * Return: QDF_STATUS_SUCCESS
 *	   QDF_STATUS_E_FAULT
 */
QDF_STATUS dp_tx_tso_num_seg_pool_init(struct dp_soc *soc, uint8_t num_pool,
				       uint32_t num_elem);

/**
 * dp_tx_tso_num_seg_pool_free_by_id() - free descriptors that tracks the
 *                              fragments in each tso segment based on pool ID
 *
 * @soc: handle to dp soc structure
 * @pool_id: Pool ID
 */
void dp_tx_tso_num_seg_pool_free_by_id(struct dp_soc *soc, uint8_t pool_id);

/**
 * dp_tx_tso_num_seg_pool_free() - free descriptors that tracks the
 *                              fragments in each tso segment
 *
 * @soc: handle to dp soc structure
 * @num_pool: number of pools to free
 */
void dp_tx_tso_num_seg_pool_free(struct dp_soc *soc, uint8_t num_pool);

/**
 * dp_tx_tso_num_seg_pool_deinit_by_id() - de-initialize descriptors that tracks
 *                           the fragments in each tso segment based on pool ID
 * @soc: handle to dp soc structure
 * @pool_id: Pool ID
 */
void dp_tx_tso_num_seg_pool_deinit_by_id(struct dp_soc *soc, uint8_t pool_id);

/**
 * dp_tx_tso_num_seg_pool_deinit() - de-initialize descriptors that tracks the
 *                              fragments in each tso segment
 *
 * @soc: handle to dp soc structure
 * @num_pool: number of pools to de-initialize
 *
 * Return: QDF_STATUS_SUCCESS
 *	   QDF_STATUS_E_FAULT
 */
void dp_tx_tso_num_seg_pool_deinit(struct dp_soc *soc, uint8_t num_pool);

#ifdef DP_UMAC_HW_RESET_SUPPORT
/**
 * dp_tx_desc_pool_cleanup() -  Clean up the tx dexcriptor pools
 * @soc: Handle to DP SoC structure
 * @nbuf_list: nbuf list for delayed free
 * @cleanup: cleanup the pool
 *
 */
void dp_tx_desc_pool_cleanup(struct dp_soc *soc, qdf_nbuf_t *nbuf_list,
			     bool cleanup);
#endif

/**
 * dp_tx_desc_clear() - Clear contents of tx desc
 * @tx_desc: descriptor to free
 *
 * Return: none
 */
static inline void
dp_tx_desc_clear(struct dp_tx_desc_s *tx_desc)
{
	tx_desc->vdev_id = DP_INVALID_VDEV_ID;
	tx_desc->nbuf = NULL;
	tx_desc->flags = 0;
	tx_desc->next = NULL;
}

#ifdef QCA_LL_TX_FLOW_CONTROL_V2
void dp_tx_flow_control_init(struct dp_soc *);
void dp_tx_flow_control_deinit(struct dp_soc *);

QDF_STATUS dp_txrx_register_pause_cb(struct cdp_soc_t *soc,
	tx_pause_callback pause_cb);
QDF_STATUS dp_tx_flow_pool_map(struct cdp_soc_t *soc, uint8_t pdev_id,
			       uint8_t vdev_id);
void dp_tx_flow_pool_unmap(struct cdp_soc_t *handle, uint8_t pdev_id,
			   uint8_t vdev_id);
void dp_tx_clear_flow_pool_stats(struct dp_soc *soc);
struct dp_tx_desc_pool_s *dp_tx_create_flow_pool(struct dp_soc *soc,
	uint8_t flow_pool_id, uint32_t flow_pool_size);

QDF_STATUS dp_tx_flow_pool_map_handler(struct dp_pdev *pdev, uint8_t flow_id,
	uint8_t flow_type, uint8_t flow_pool_id, uint32_t flow_pool_size);
void dp_tx_flow_pool_unmap_handler(struct dp_pdev *pdev, uint8_t flow_id,
	uint8_t flow_type, uint8_t flow_pool_id);

/**
 * dp_tx_get_desc_flow_pool() - get descriptor from flow pool
 * @pool: flow pool
 *
 * Caller needs to take lock and do sanity checks.
 *
 * Return: tx descriptor
 */
static inline
struct dp_tx_desc_s *dp_tx_get_desc_flow_pool(struct dp_tx_desc_pool_s *pool)
{
	struct dp_tx_desc_s *tx_desc = pool->freelist;

	pool->freelist = pool->freelist->next;
	pool->avail_desc--;
	return tx_desc;
}

/**
 * dp_tx_put_desc_flow_pool() - put descriptor to flow pool freelist
 * @pool: flow pool
 * @tx_desc: tx descriptor
 *
 * Caller needs to take lock and do sanity checks.
 *
 * Return: none
 */
static inline
void dp_tx_put_desc_flow_pool(struct dp_tx_desc_pool_s *pool,
			struct dp_tx_desc_s *tx_desc)
{
	tx_desc->next = pool->freelist;
	pool->freelist = tx_desc;
	pool->avail_desc++;
}

static inline void
dp_tx_desc_free_list(struct dp_tx_desc_pool_s *pool,
		     struct dp_tx_desc_s *head_desc,
		     struct dp_tx_desc_s *tail_desc,
		     uint32_t fast_desc_count)
{
}

#ifdef QCA_AC_BASED_FLOW_CONTROL

/**
 * dp_tx_flow_pool_member_clean() - Clean the members of TX flow pool
 * @pool: flow pool
 *
 * Return: None
 */
static inline void
dp_tx_flow_pool_member_clean(struct dp_tx_desc_pool_s *pool)
{
	pool->elem_size = 0;
	pool->freelist = NULL;
	pool->pool_size = 0;
	pool->avail_desc = 0;
	qdf_mem_zero(pool->start_th, FL_TH_MAX);
	qdf_mem_zero(pool->stop_th, FL_TH_MAX);
	pool->status = FLOW_POOL_INACTIVE;
}

/**
 * dp_tx_is_threshold_reached() - Check if current avail desc meet threshold
 * @pool: flow pool
 * @avail_desc: available descriptor number
 *
 * Return: true if threshold is met, false if not
 */
static inline bool
dp_tx_is_threshold_reached(struct dp_tx_desc_pool_s *pool, uint16_t avail_desc)
{
	if (qdf_unlikely(avail_desc == pool->stop_th[DP_TH_BE_BK]))
		return true;
	else if (qdf_unlikely(avail_desc == pool->stop_th[DP_TH_VI]))
		return true;
	else if (qdf_unlikely(avail_desc == pool->stop_th[DP_TH_VO]))
		return true;
	else if (qdf_unlikely(avail_desc == pool->stop_th[DP_TH_HI]))
		return true;
	else
		return false;
}

/**
 * dp_tx_adjust_flow_pool_state() - Adjust flow pool state
 * @soc: dp soc
 * @pool: flow pool
 */
static inline void
dp_tx_adjust_flow_pool_state(struct dp_soc *soc,
			     struct dp_tx_desc_pool_s *pool)
{
	if (pool->avail_desc > pool->stop_th[DP_TH_BE_BK]) {
		pool->status = FLOW_POOL_ACTIVE_UNPAUSED;
		return;
	} else if (pool->avail_desc <= pool->stop_th[DP_TH_BE_BK] &&
		   pool->avail_desc > pool->stop_th[DP_TH_VI]) {
		pool->status = FLOW_POOL_BE_BK_PAUSED;
	} else if (pool->avail_desc <= pool->stop_th[DP_TH_VI] &&
		   pool->avail_desc > pool->stop_th[DP_TH_VO]) {
		pool->status = FLOW_POOL_VI_PAUSED;
	} else if (pool->avail_desc <= pool->stop_th[DP_TH_VO] &&
		   pool->avail_desc > pool->stop_th[DP_TH_HI]) {
		pool->status = FLOW_POOL_VO_PAUSED;
	} else if (pool->avail_desc <= pool->stop_th[DP_TH_HI]) {
		pool->status = FLOW_POOL_ACTIVE_PAUSED;
	}

	switch (pool->status) {
	case FLOW_POOL_ACTIVE_PAUSED:
		soc->pause_cb(pool->flow_pool_id,
			      WLAN_NETIF_PRIORITY_QUEUE_OFF,
			      WLAN_DATA_FLOW_CTRL_PRI);
		fallthrough;

	case FLOW_POOL_VO_PAUSED:
		soc->pause_cb(pool->flow_pool_id,
			      WLAN_NETIF_VO_QUEUE_OFF,
			      WLAN_DATA_FLOW_CTRL_VO);
		fallthrough;

	case FLOW_POOL_VI_PAUSED:
		soc->pause_cb(pool->flow_pool_id,
			      WLAN_NETIF_VI_QUEUE_OFF,
			      WLAN_DATA_FLOW_CTRL_VI);
		fallthrough;

	case FLOW_POOL_BE_BK_PAUSED:
		soc->pause_cb(pool->flow_pool_id,
			      WLAN_NETIF_BE_BK_QUEUE_OFF,
			      WLAN_DATA_FLOW_CTRL_BE_BK);
		break;
	default:
		dp_err("Invalid pool status:%u to adjust", pool->status);
	}
}

/**
 * dp_tx_desc_alloc() - Allocate a Software Tx descriptor from given pool
 * @soc: Handle to DP SoC structure
 * @desc_pool_id: ID of the flow control fool
 *
 * Return: TX descriptor allocated or NULL
 */
static inline struct dp_tx_desc_s *
dp_tx_desc_alloc(struct dp_soc *soc, uint8_t desc_pool_id)
{
	struct dp_tx_desc_s *tx_desc = NULL;
	struct dp_tx_desc_pool_s *pool = &soc->tx_desc[desc_pool_id];
	bool is_pause = false;
	enum netif_action_type act = WLAN_NETIF_ACTION_TYPE_NONE;
	enum dp_fl_ctrl_threshold level = DP_TH_BE_BK;
	enum netif_reason_type reason;

	if (qdf_likely(pool)) {
		qdf_spin_lock_bh(&pool->flow_pool_lock);
		if (qdf_likely(pool->avail_desc &&
		    pool->status != FLOW_POOL_INVALID &&
		    pool->status != FLOW_POOL_INACTIVE)) {
			tx_desc = dp_tx_get_desc_flow_pool(pool);
			tx_desc->pool_id = desc_pool_id;
			tx_desc->flags = DP_TX_DESC_FLAG_ALLOCATED;
			dp_tx_desc_set_magic(tx_desc,
					     DP_TX_MAGIC_PATTERN_INUSE);
			is_pause = dp_tx_is_threshold_reached(pool,
							      pool->avail_desc);

			if (qdf_unlikely(pool->status ==
					 FLOW_POOL_ACTIVE_UNPAUSED_REATTACH)) {
				dp_tx_adjust_flow_pool_state(soc, pool);
				is_pause = false;
			}

			if (qdf_unlikely(is_pause)) {
				switch (pool->status) {
				case FLOW_POOL_ACTIVE_UNPAUSED:
					/* pause network BE\BK queue */
					act = WLAN_NETIF_BE_BK_QUEUE_OFF;
					reason = WLAN_DATA_FLOW_CTRL_BE_BK;
					level = DP_TH_BE_BK;
					pool->status = FLOW_POOL_BE_BK_PAUSED;
					break;
				case FLOW_POOL_BE_BK_PAUSED:
					/* pause network VI queue */
					act = WLAN_NETIF_VI_QUEUE_OFF;
					reason = WLAN_DATA_FLOW_CTRL_VI;
					level = DP_TH_VI;
					pool->status = FLOW_POOL_VI_PAUSED;
					break;
				case FLOW_POOL_VI_PAUSED:
					/* pause network VO queue */
					act = WLAN_NETIF_VO_QUEUE_OFF;
					reason = WLAN_DATA_FLOW_CTRL_VO;
					level = DP_TH_VO;
					pool->status = FLOW_POOL_VO_PAUSED;
					break;
				case FLOW_POOL_VO_PAUSED:
					/* pause network HI PRI queue */
					act = WLAN_NETIF_PRIORITY_QUEUE_OFF;
					reason = WLAN_DATA_FLOW_CTRL_PRI;
					level = DP_TH_HI;
					pool->status = FLOW_POOL_ACTIVE_PAUSED;
					break;
				case FLOW_POOL_ACTIVE_PAUSED:
					act = WLAN_NETIF_ACTION_TYPE_NONE;
					break;
				default:
					dp_err_rl("pool status is %d!",
						  pool->status);
					break;
				}

				if (act != WLAN_NETIF_ACTION_TYPE_NONE) {
					pool->latest_pause_time[level] =
						qdf_get_system_timestamp();
					soc->pause_cb(desc_pool_id,
						      act,
						      reason);
				}
			}
		} else {
			pool->pkt_drop_no_desc++;
		}
		qdf_spin_unlock_bh(&pool->flow_pool_lock);
	} else {
		dp_err_rl("NULL desc pool pool_id %d", desc_pool_id);
		soc->pool_stats.pkt_drop_no_pool++;
	}

	return tx_desc;
}

/**
 * dp_tx_desc_free() - Free a tx descriptor and attach it to free list
 * @soc: Handle to DP SoC structure
 * @tx_desc: the tx descriptor to be freed
 * @desc_pool_id: ID of the flow control pool
 *
 * Return: None
 */
static inline void
dp_tx_desc_free(struct dp_soc *soc, struct dp_tx_desc_s *tx_desc,
		uint8_t desc_pool_id)
{
	struct dp_tx_desc_pool_s *pool = &soc->tx_desc[desc_pool_id];
	qdf_time_t unpause_time = qdf_get_system_timestamp(), pause_dur;
	enum netif_action_type act = WLAN_WAKE_ALL_NETIF_QUEUE;
	enum netif_reason_type reason;

	qdf_spin_lock_bh(&pool->flow_pool_lock);
	tx_desc->vdev_id = DP_INVALID_VDEV_ID;
	tx_desc->nbuf = NULL;
	tx_desc->flags = 0;
	dp_tx_desc_set_magic(tx_desc, DP_TX_MAGIC_PATTERN_FREE);
	dp_tx_put_desc_flow_pool(pool, tx_desc);
	switch (pool->status) {
	case FLOW_POOL_ACTIVE_PAUSED:
		if (pool->avail_desc > pool->start_th[DP_TH_HI]) {
			act = WLAN_NETIF_PRIORITY_QUEUE_ON;
			reason = WLAN_DATA_FLOW_CTRL_PRI;
			pool->status = FLOW_POOL_VO_PAUSED;

			/* Update maximum pause duration for HI queue */
			pause_dur = unpause_time -
					pool->latest_pause_time[DP_TH_HI];
			if (pool->max_pause_time[DP_TH_HI] < pause_dur)
				pool->max_pause_time[DP_TH_HI] = pause_dur;
		}
		break;
	case FLOW_POOL_VO_PAUSED:
		if (pool->avail_desc > pool->start_th[DP_TH_VO]) {
			act = WLAN_NETIF_VO_QUEUE_ON;
			reason = WLAN_DATA_FLOW_CTRL_VO;
			pool->status = FLOW_POOL_VI_PAUSED;

			/* Update maximum pause duration for VO queue */
			pause_dur = unpause_time -
					pool->latest_pause_time[DP_TH_VO];
			if (pool->max_pause_time[DP_TH_VO] < pause_dur)
				pool->max_pause_time[DP_TH_VO] = pause_dur;
		}
		break;
	case FLOW_POOL_VI_PAUSED:
		if (pool->avail_desc > pool->start_th[DP_TH_VI]) {
			act = WLAN_NETIF_VI_QUEUE_ON;
			reason = WLAN_DATA_FLOW_CTRL_VI;
			pool->status = FLOW_POOL_BE_BK_PAUSED;

			/* Update maximum pause duration for VI queue */
			pause_dur = unpause_time -
					pool->latest_pause_time[DP_TH_VI];
			if (pool->max_pause_time[DP_TH_VI] < pause_dur)
				pool->max_pause_time[DP_TH_VI] = pause_dur;
		}
		break;
	case FLOW_POOL_BE_BK_PAUSED:
		if (pool->avail_desc > pool->start_th[DP_TH_BE_BK]) {
			act = WLAN_NETIF_BE_BK_QUEUE_ON;
			reason = WLAN_DATA_FLOW_CTRL_BE_BK;
			pool->status = FLOW_POOL_ACTIVE_UNPAUSED;

			/* Update maximum pause duration for BE_BK queue */
			pause_dur = unpause_time -
					pool->latest_pause_time[DP_TH_BE_BK];
			if (pool->max_pause_time[DP_TH_BE_BK] < pause_dur)
				pool->max_pause_time[DP_TH_BE_BK] = pause_dur;
		}
		break;
	case FLOW_POOL_INVALID:
		if (pool->avail_desc == pool->pool_size) {
			dp_tx_desc_pool_deinit(soc, desc_pool_id, false);
			dp_tx_desc_pool_free(soc, desc_pool_id, false);
			qdf_spin_unlock_bh(&pool->flow_pool_lock);
			dp_err_rl("pool %d is freed!!", desc_pool_id);
			return;
		}
		break;

	case FLOW_POOL_ACTIVE_UNPAUSED:
		break;

	case FLOW_POOL_ACTIVE_UNPAUSED_REATTACH:
		fallthrough;
	default:
		dp_err_rl("pool %d status: %d",
			  desc_pool_id, pool->status);
		break;
	};

	if (act != WLAN_WAKE_ALL_NETIF_QUEUE)
		soc->pause_cb(pool->flow_pool_id,
			      act, reason);
	qdf_spin_unlock_bh(&pool->flow_pool_lock);
}

static inline void
dp_tx_spcl_desc_free(struct dp_soc *soc, struct dp_tx_desc_s *tx_desc,
		     uint8_t desc_pool_id)
{
}

static inline struct dp_tx_desc_s *dp_tx_spcl_desc_alloc(struct dp_soc *soc,
							 uint8_t desc_pool_id)
{
	return NULL;
}
#else /* QCA_AC_BASED_FLOW_CONTROL */

static inline bool
dp_tx_is_threshold_reached(struct dp_tx_desc_pool_s *pool, uint16_t avail_desc)
{
	if (qdf_unlikely(avail_desc < pool->stop_th))
		return true;
	else
		return false;
}

/**
 * dp_tx_desc_alloc() - Allocate a Software Tx Descriptor from given pool
 * @soc: Handle to DP SoC structure
 * @desc_pool_id:
 *
 * Return: Tx descriptor or NULL
 */
static inline struct dp_tx_desc_s *
dp_tx_desc_alloc(struct dp_soc *soc, uint8_t desc_pool_id)
{
	struct dp_tx_desc_s *tx_desc = NULL;
	struct dp_tx_desc_pool_s *pool = &soc->tx_desc[desc_pool_id];

	if (pool) {
		qdf_spin_lock_bh(&pool->flow_pool_lock);
		if (pool->status <= FLOW_POOL_ACTIVE_PAUSED &&
		    pool->avail_desc) {
			tx_desc = dp_tx_get_desc_flow_pool(pool);
			tx_desc->pool_id = desc_pool_id;
			tx_desc->flags = DP_TX_DESC_FLAG_ALLOCATED;
			dp_tx_desc_set_magic(tx_desc,
					     DP_TX_MAGIC_PATTERN_INUSE);
			if (qdf_unlikely(pool->avail_desc < pool->stop_th)) {
				pool->status = FLOW_POOL_ACTIVE_PAUSED;
				qdf_spin_unlock_bh(&pool->flow_pool_lock);
				/* pause network queues */
				soc->pause_cb(desc_pool_id,
					       WLAN_STOP_ALL_NETIF_QUEUE,
					       WLAN_DATA_FLOW_CONTROL);
			} else {
				qdf_spin_unlock_bh(&pool->flow_pool_lock);
			}
		} else {
			pool->pkt_drop_no_desc++;
			qdf_spin_unlock_bh(&pool->flow_pool_lock);
		}
	} else {
		soc->pool_stats.pkt_drop_no_pool++;
	}

	return tx_desc;
}

static inline struct dp_tx_desc_s *dp_tx_spcl_desc_alloc(struct dp_soc *soc,
							 uint8_t desc_pool_id)
{
	return NULL;
}
/**
 * dp_tx_desc_free() - Free a tx descriptor and attach it to free list
 * @soc: Handle to DP SoC structure
 * @tx_desc: Descriptor to free
 * @desc_pool_id: Descriptor pool Id
 *
 * Return: None
 */
static inline void
dp_tx_desc_free(struct dp_soc *soc, struct dp_tx_desc_s *tx_desc,
		uint8_t desc_pool_id)
{
	struct dp_tx_desc_pool_s *pool = &soc->tx_desc[desc_pool_id];

	qdf_spin_lock_bh(&pool->flow_pool_lock);
	tx_desc->vdev_id = DP_INVALID_VDEV_ID;
	tx_desc->nbuf = NULL;
	tx_desc->flags = 0;
	dp_tx_desc_set_magic(tx_desc, DP_TX_MAGIC_PATTERN_FREE);
	dp_tx_put_desc_flow_pool(pool, tx_desc);
	switch (pool->status) {
	case FLOW_POOL_ACTIVE_PAUSED:
		if (pool->avail_desc > pool->start_th) {
			soc->pause_cb(pool->flow_pool_id,
				       WLAN_WAKE_ALL_NETIF_QUEUE,
				       WLAN_DATA_FLOW_CONTROL);
			pool->status = FLOW_POOL_ACTIVE_UNPAUSED;
		}
		break;
	case FLOW_POOL_INVALID:
		if (pool->avail_desc == pool->pool_size) {
			dp_tx_desc_pool_deinit(soc, desc_pool_id, false);
			dp_tx_desc_pool_free(soc, desc_pool_id, false);
			qdf_spin_unlock_bh(&pool->flow_pool_lock);
			qdf_print("%s %d pool is freed!!",
				  __func__, __LINE__);
			return;
		}
		break;

	case FLOW_POOL_ACTIVE_UNPAUSED:
		break;
	default:
		qdf_print("%s %d pool is INACTIVE State!!",
			  __func__, __LINE__);
		break;
	};

	qdf_spin_unlock_bh(&pool->flow_pool_lock);
}

static inline void
dp_tx_spcl_desc_free(struct dp_soc *soc, struct dp_tx_desc_s *tx_desc,
		     uint8_t desc_pool_id)
{
}
#endif /* QCA_AC_BASED_FLOW_CONTROL */

static inline bool
dp_tx_desc_thresh_reached(struct cdp_soc_t *soc_hdl, uint8_t vdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);
	struct dp_tx_desc_pool_s *pool;
	bool status;

	if (!vdev)
		return false;

	pool = vdev->pool;
	status = dp_tx_is_threshold_reached(pool, pool->avail_desc);
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);

	return status;
}
#else /* QCA_LL_TX_FLOW_CONTROL_V2 */

static inline void dp_tx_flow_control_init(struct dp_soc *handle)
{
}

static inline void dp_tx_flow_control_deinit(struct dp_soc *handle)
{
}

static inline QDF_STATUS dp_tx_flow_pool_map_handler(struct dp_pdev *pdev,
	uint8_t flow_id, uint8_t flow_type, uint8_t flow_pool_id,
	uint32_t flow_pool_size)
{
	return QDF_STATUS_SUCCESS;
}

static inline void dp_tx_flow_pool_unmap_handler(struct dp_pdev *pdev,
	uint8_t flow_id, uint8_t flow_type, uint8_t flow_pool_id)
{
}

#ifdef QCA_DP_TX_HW_SW_NBUF_DESC_PREFETCH
static inline
void dp_tx_prefetch_desc(struct dp_tx_desc_s *tx_desc)
{
	if (tx_desc)
		prefetch(tx_desc);
}
#else
static inline
void dp_tx_prefetch_desc(struct dp_tx_desc_s *tx_desc)
{
}
#endif

/**
 * dp_tx_desc_alloc() - Allocate a Software Tx Descriptor from given pool
 * @soc: Handle to DP SoC structure
 * @desc_pool_id: pool id
 *
 * Return: Tx Descriptor or NULL
 */
static inline struct dp_tx_desc_s *dp_tx_desc_alloc(struct dp_soc *soc,
						uint8_t desc_pool_id)
{
	struct dp_tx_desc_s *tx_desc = NULL;
	struct dp_tx_desc_pool_s *pool = NULL;

	pool = dp_get_tx_desc_pool(soc, desc_pool_id);

	TX_DESC_LOCK_LOCK(&pool->lock);

	tx_desc = pool->freelist;

	/* Pool is exhausted */
	if (!tx_desc) {
		TX_DESC_LOCK_UNLOCK(&pool->lock);
		return NULL;
	}

	pool->freelist = pool->freelist->next;
	pool->num_allocated++;
	pool->num_free--;
	dp_tx_prefetch_desc(pool->freelist);

	tx_desc->flags = DP_TX_DESC_FLAG_ALLOCATED;

	TX_DESC_LOCK_UNLOCK(&pool->lock);

	return tx_desc;
}

static inline struct dp_tx_desc_s *dp_tx_spcl_desc_alloc(struct dp_soc *soc,
							 uint8_t desc_pool_id)
{
	struct dp_tx_desc_s *tx_desc = NULL;
	struct dp_tx_desc_pool_s *pool = NULL;

	pool = dp_get_spcl_tx_desc_pool(soc, desc_pool_id);

	TX_DESC_LOCK_LOCK(&pool->lock);

	tx_desc = pool->freelist;

	/* Pool is exhausted */
	if (!tx_desc) {
		TX_DESC_LOCK_UNLOCK(&pool->lock);
		return NULL;
	}

	pool->freelist = pool->freelist->next;
	pool->num_allocated++;
	pool->num_free--;
	dp_tx_prefetch_desc(pool->freelist);

	tx_desc->flags = DP_TX_DESC_FLAG_ALLOCATED;
	tx_desc->flags |= DP_TX_DESC_FLAG_SPECIAL;

	TX_DESC_LOCK_UNLOCK(&pool->lock);

	return tx_desc;
}

/**
 * dp_tx_desc_alloc_multiple() - Allocate batch of software Tx Descriptors
 *                            from given pool
 * @soc: Handle to DP SoC structure
 * @desc_pool_id: pool id should pick up
 * @num_requested: number of required descriptor
 *
 * allocate multiple tx descriptor and make a link
 *
 * Return: first descriptor pointer or NULL
 */
static inline struct dp_tx_desc_s *dp_tx_desc_alloc_multiple(
		struct dp_soc *soc, uint8_t desc_pool_id, uint8_t num_requested)
{
	struct dp_tx_desc_s *c_desc = NULL, *h_desc = NULL;
	uint8_t count;
	struct dp_tx_desc_pool_s *pool = NULL;

	pool = dp_get_tx_desc_pool(soc, desc_pool_id);

	TX_DESC_LOCK_LOCK(&pool->lock);

	if ((num_requested == 0) ||
			(pool->num_free < num_requested)) {
		TX_DESC_LOCK_UNLOCK(&pool->lock);
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			"%s, No Free Desc: Available(%d) num_requested(%d)",
			__func__, pool->num_free,
			num_requested);
		return NULL;
	}

	h_desc = pool->freelist;

	/* h_desc should never be NULL since num_free > requested */
	qdf_assert_always(h_desc);

	c_desc = h_desc;
	for (count = 0; count < (num_requested - 1); count++) {
		c_desc->flags = DP_TX_DESC_FLAG_ALLOCATED;
		c_desc = c_desc->next;
	}
	pool->num_free -= count;
	pool->num_allocated += count;
	pool->freelist = c_desc->next;
	c_desc->next = NULL;

	TX_DESC_LOCK_UNLOCK(&pool->lock);
	return h_desc;
}

/**
 * dp_tx_desc_free() - Free a tx descriptor and attach it to free list
 * @soc: Handle to DP SoC structure
 * @tx_desc: descriptor to free
 * @desc_pool_id: ID of the free pool
 */
static inline void
dp_tx_desc_free(struct dp_soc *soc, struct dp_tx_desc_s *tx_desc,
		uint8_t desc_pool_id)
{
	struct dp_tx_desc_pool_s *pool = NULL;

	dp_tx_desc_clear(tx_desc);
	pool = dp_get_tx_desc_pool(soc, desc_pool_id);
	TX_DESC_LOCK_LOCK(&pool->lock);
	tx_desc->next = pool->freelist;
	pool->freelist = tx_desc;
	pool->num_allocated--;
	pool->num_free++;
	TX_DESC_LOCK_UNLOCK(&pool->lock);
}

static inline void
dp_tx_spcl_desc_free(struct dp_soc *soc, struct dp_tx_desc_s *tx_desc,
		     uint8_t desc_pool_id)
{
	struct dp_tx_desc_pool_s *pool = NULL;

	dp_tx_desc_clear(tx_desc);

	pool = dp_get_spcl_tx_desc_pool(soc, desc_pool_id);
	TX_DESC_LOCK_LOCK(&pool->lock);
	tx_desc->next = pool->freelist;
	pool->freelist = tx_desc;
	pool->num_allocated--;
	pool->num_free++;
	TX_DESC_LOCK_UNLOCK(&pool->lock);
}

static inline void
dp_tx_desc_free_list(struct dp_tx_desc_pool_s *pool,
		     struct dp_tx_desc_s *head_desc,
		     struct dp_tx_desc_s *tail_desc,
		     uint32_t fast_desc_count)
{
	TX_DESC_LOCK_LOCK(&pool->lock);
	pool->num_allocated -= fast_desc_count;
	pool->num_free += fast_desc_count;
	tail_desc->next = pool->freelist;
	pool->freelist = head_desc;
	TX_DESC_LOCK_UNLOCK(&pool->lock);
}

#endif /* QCA_LL_TX_FLOW_CONTROL_V2 */

#ifdef QCA_DP_TX_DESC_ID_CHECK
/**
 * dp_tx_is_desc_id_valid() - check is the tx desc id valid
 * @soc: Handle to DP SoC structure
 * @tx_desc_id:
 *
 * Return: true or false
 */
static inline bool
dp_tx_is_desc_id_valid(struct dp_soc *soc, uint32_t tx_desc_id)
{
	uint8_t pool_id;
	uint16_t page_id, offset;
	struct dp_tx_desc_pool_s *pool;

	pool_id = (tx_desc_id & DP_TX_DESC_ID_POOL_MASK) >>
			DP_TX_DESC_ID_POOL_OS;
	/* Pool ID is out of limit */
	if (pool_id > wlan_cfg_get_num_tx_desc_pool(
				soc->wlan_cfg_ctx)) {
		QDF_TRACE(QDF_MODULE_ID_DP,
			  QDF_TRACE_LEVEL_FATAL,
			  "%s:Tx Comp pool id %d not valid",
			  __func__,
			  pool_id);
		goto warn_exit;
	}

	pool = &soc->tx_desc[pool_id];
	/* the pool is freed */
	if (IS_TX_DESC_POOL_STATUS_INACTIVE(pool)) {
		QDF_TRACE(QDF_MODULE_ID_DP,
			  QDF_TRACE_LEVEL_FATAL,
			  "%s:the pool %d has been freed",
			  __func__,
			  pool_id);
		goto warn_exit;
	}

	page_id = (tx_desc_id & DP_TX_DESC_ID_PAGE_MASK) >>
				DP_TX_DESC_ID_PAGE_OS;
	/* the page id is out of limit */
	if (page_id >= pool->desc_pages.num_pages) {
		QDF_TRACE(QDF_MODULE_ID_DP,
			  QDF_TRACE_LEVEL_FATAL,
			  "%s:the page id %d invalid, pool id %d, num_page %d",
			  __func__,
			  page_id,
			  pool_id,
			  pool->desc_pages.num_pages);
		goto warn_exit;
	}

	offset = (tx_desc_id & DP_TX_DESC_ID_OFFSET_MASK) >>
				DP_TX_DESC_ID_OFFSET_OS;
	/* the offset is out of limit */
	if (offset >= pool->desc_pages.num_element_per_page) {
		QDF_TRACE(QDF_MODULE_ID_DP,
			  QDF_TRACE_LEVEL_FATAL,
			  "%s:offset %d invalid, pool%d,num_elem_per_page %d",
			  __func__,
			  offset,
			  pool_id,
			  pool->desc_pages.num_element_per_page);
		goto warn_exit;
	}

	return true;

warn_exit:
	QDF_TRACE(QDF_MODULE_ID_DP,
		  QDF_TRACE_LEVEL_FATAL,
		  "%s:Tx desc id 0x%x not valid",
		  __func__,
		  tx_desc_id);
	qdf_assert_always(0);
	return false;
}

#else
static inline bool
dp_tx_is_desc_id_valid(struct dp_soc *soc, uint32_t tx_desc_id)
{
	return true;
}
#endif /* QCA_DP_TX_DESC_ID_CHECK */

#ifdef QCA_DP_TX_DESC_FAST_COMP_ENABLE
static inline void dp_tx_desc_update_fast_comp_flag(struct dp_soc *soc,
						    struct dp_tx_desc_s *desc,
						    uint8_t allow_fast_comp)
{
	if (qdf_likely(!(desc->flags & DP_TX_DESC_FLAG_TO_FW)) &&
	    qdf_likely(allow_fast_comp))
		desc->flags |= DP_TX_DESC_FLAG_SIMPLE;

	if (qdf_likely(desc->nbuf->is_from_recycler) &&
	    qdf_likely(desc->nbuf->fast_xmit))
		desc->flags |= DP_TX_DESC_FLAG_FAST;
}

#else
static inline void dp_tx_desc_update_fast_comp_flag(struct dp_soc *soc,
						    struct dp_tx_desc_s *desc,
						    uint8_t allow_fast_comp)
{
}
#endif /* QCA_DP_TX_DESC_FAST_COMP_ENABLE */

/**
 * dp_tx_desc_find() - find dp tx descriptor from pool/page/offset
 * @soc: handle for the device sending the data
 * @pool_id: pool id
 * @page_id: page id
 * @offset: offset from base address
 * @spcl_pool: bit to indicate if this is a special pool
 *
 * Use page and offset to find the corresponding descriptor object in
 * the given descriptor pool.
 *
 * Return: the descriptor object that has the specified ID
 */
static inline
struct dp_tx_desc_s *dp_tx_desc_find(struct dp_soc *soc,
				     uint8_t pool_id, uint16_t page_id,
				     uint16_t offset, bool spcl_pool)
{
	struct dp_tx_desc_pool_s *tx_desc_pool = NULL;

	tx_desc_pool = spcl_pool ? dp_get_spcl_tx_desc_pool(soc, pool_id) :
				dp_get_tx_desc_pool(soc, pool_id);

	return tx_desc_pool->desc_pages.cacheable_pages[page_id] +
		tx_desc_pool->elem_size * offset;
}

/**
 * dp_tx_ext_desc_alloc() - Get tx extension descriptor from pool
 * @soc: handle for the device sending the data
 * @desc_pool_id: target pool id
 *
 * Return: None
 */
static inline
struct dp_tx_ext_desc_elem_s *dp_tx_ext_desc_alloc(struct dp_soc *soc,
		uint8_t desc_pool_id)
{
	struct dp_tx_ext_desc_elem_s *c_elem;

	desc_pool_id = dp_tx_ext_desc_pool_override(desc_pool_id);
	qdf_spin_lock_bh(&soc->tx_ext_desc[desc_pool_id].lock);
	if (soc->tx_ext_desc[desc_pool_id].num_free <= 0) {
		qdf_spin_unlock_bh(&soc->tx_ext_desc[desc_pool_id].lock);
		return NULL;
	}
	c_elem = soc->tx_ext_desc[desc_pool_id].freelist;
	soc->tx_ext_desc[desc_pool_id].freelist =
		soc->tx_ext_desc[desc_pool_id].freelist->next;
	soc->tx_ext_desc[desc_pool_id].num_free--;
	qdf_spin_unlock_bh(&soc->tx_ext_desc[desc_pool_id].lock);
	return c_elem;
}

/**
 * dp_tx_ext_desc_free() - Release tx extension descriptor to the pool
 * @soc: handle for the device sending the data
 * @elem: ext descriptor pointer should release
 * @desc_pool_id: target pool id
 *
 * Return: None
 */
static inline void dp_tx_ext_desc_free(struct dp_soc *soc,
	struct dp_tx_ext_desc_elem_s *elem, uint8_t desc_pool_id)
{
	desc_pool_id = dp_tx_ext_desc_pool_override(desc_pool_id);
	qdf_spin_lock_bh(&soc->tx_ext_desc[desc_pool_id].lock);
	elem->next = soc->tx_ext_desc[desc_pool_id].freelist;
	soc->tx_ext_desc[desc_pool_id].freelist = elem;
	soc->tx_ext_desc[desc_pool_id].num_free++;
	qdf_spin_unlock_bh(&soc->tx_ext_desc[desc_pool_id].lock);
	return;
}

/**
 * dp_tx_ext_desc_free_multiple() - Free multiple tx extension descriptor and
 *                           attach it to free list
 * @soc: Handle to DP SoC structure
 * @desc_pool_id: pool id should pick up
 * @elem: tx descriptor should be freed
 * @num_free: number of descriptors should be freed
 *
 * Return: none
 */
static inline void dp_tx_ext_desc_free_multiple(struct dp_soc *soc,
		struct dp_tx_ext_desc_elem_s *elem, uint8_t desc_pool_id,
		uint8_t num_free)
{
	struct dp_tx_ext_desc_elem_s *head, *tail, *c_elem;
	uint8_t freed = num_free;

	/* caller should always guarantee atleast list of num_free nodes */
	qdf_assert_always(elem);

	head = elem;
	c_elem = head;
	tail = head;
	while (c_elem && freed) {
		tail = c_elem;
		c_elem = c_elem->next;
		freed--;
	}

	/* caller should always guarantee atleast list of num_free nodes */
	qdf_assert_always(tail);

	desc_pool_id = dp_tx_ext_desc_pool_override(desc_pool_id);
	qdf_spin_lock_bh(&soc->tx_ext_desc[desc_pool_id].lock);
	tail->next = soc->tx_ext_desc[desc_pool_id].freelist;
	soc->tx_ext_desc[desc_pool_id].freelist = head;
	soc->tx_ext_desc[desc_pool_id].num_free += num_free;
	qdf_spin_unlock_bh(&soc->tx_ext_desc[desc_pool_id].lock);

	return;
}

#if defined(FEATURE_TSO)
/**
 * dp_tx_tso_desc_alloc() - function to allocate a TSO segment
 * @soc: device soc instance
 * @pool_id: pool id should pick up tso descriptor
 *
 * Allocates a TSO segment element from the free list held in
 * the soc
 *
 * Return: tso_seg, tso segment memory pointer
 */
static inline struct qdf_tso_seg_elem_t *dp_tx_tso_desc_alloc(
		struct dp_soc *soc, uint8_t pool_id)
{
	struct qdf_tso_seg_elem_t *tso_seg = NULL;

	qdf_spin_lock_bh(&soc->tx_tso_desc[pool_id].lock);
	if (soc->tx_tso_desc[pool_id].freelist) {
		soc->tx_tso_desc[pool_id].num_free--;
		tso_seg = soc->tx_tso_desc[pool_id].freelist;
		soc->tx_tso_desc[pool_id].freelist =
			soc->tx_tso_desc[pool_id].freelist->next;
	}
	qdf_spin_unlock_bh(&soc->tx_tso_desc[pool_id].lock);

	return tso_seg;
}

/**
 * dp_tx_tso_desc_free() - function to free a TSO segment
 * @soc: device soc instance
 * @pool_id: pool id should pick up tso descriptor
 * @tso_seg: tso segment memory pointer
 *
 * Returns a TSO segment element to the free list held in the
 * HTT pdev
 *
 * Return: none
 */
static inline void dp_tx_tso_desc_free(struct dp_soc *soc,
		uint8_t pool_id, struct qdf_tso_seg_elem_t *tso_seg)
{
	qdf_spin_lock_bh(&soc->tx_tso_desc[pool_id].lock);
	tso_seg->next = soc->tx_tso_desc[pool_id].freelist;
	soc->tx_tso_desc[pool_id].freelist = tso_seg;
	soc->tx_tso_desc[pool_id].num_free++;
	qdf_spin_unlock_bh(&soc->tx_tso_desc[pool_id].lock);
}

static inline
struct qdf_tso_num_seg_elem_t  *dp_tso_num_seg_alloc(struct dp_soc *soc,
		uint8_t pool_id)
{
	struct qdf_tso_num_seg_elem_t *tso_num_seg = NULL;

	qdf_spin_lock_bh(&soc->tx_tso_num_seg[pool_id].lock);
	if (soc->tx_tso_num_seg[pool_id].freelist) {
		soc->tx_tso_num_seg[pool_id].num_free--;
		tso_num_seg = soc->tx_tso_num_seg[pool_id].freelist;
		soc->tx_tso_num_seg[pool_id].freelist =
			soc->tx_tso_num_seg[pool_id].freelist->next;
	}
	qdf_spin_unlock_bh(&soc->tx_tso_num_seg[pool_id].lock);

	return tso_num_seg;
}

static inline
void dp_tso_num_seg_free(struct dp_soc *soc,
		uint8_t pool_id, struct qdf_tso_num_seg_elem_t *tso_num_seg)
{
	qdf_spin_lock_bh(&soc->tx_tso_num_seg[pool_id].lock);
	tso_num_seg->next = soc->tx_tso_num_seg[pool_id].freelist;
	soc->tx_tso_num_seg[pool_id].freelist = tso_num_seg;
	soc->tx_tso_num_seg[pool_id].num_free++;
	qdf_spin_unlock_bh(&soc->tx_tso_num_seg[pool_id].lock);
}
#endif

/**
 * dp_tx_me_alloc_buf() - Alloc descriptor from me pool
 * @pdev: DP_PDEV handle for datapath
 *
 * Return: tx descriptor on success, NULL on error
 */
static inline struct dp_tx_me_buf_t*
dp_tx_me_alloc_buf(struct dp_pdev *pdev)
{
	struct dp_tx_me_buf_t *buf = NULL;
	qdf_spin_lock_bh(&pdev->tx_mutex);
	if (pdev->me_buf.freelist) {
		buf = pdev->me_buf.freelist;
		pdev->me_buf.freelist = pdev->me_buf.freelist->next;
		pdev->me_buf.buf_in_use++;
	} else {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
				"Error allocating memory in pool");
		qdf_spin_unlock_bh(&pdev->tx_mutex);
		return NULL;
	}
	qdf_spin_unlock_bh(&pdev->tx_mutex);
	return buf;
}

/**
 * dp_tx_me_free_buf() - Unmap the buffer holding the dest
 * address, free me descriptor and add it to the free-pool
 * @pdev: DP_PDEV handle for datapath
 * @buf : Allocated ME BUF
 *
 * Return:void
 */
static inline void
dp_tx_me_free_buf(struct dp_pdev *pdev, struct dp_tx_me_buf_t *buf)
{
	/*
	 * If the buf containing mac address was mapped,
	 * it must be unmapped before freeing the me_buf.
	 * The "paddr_macbuf" member in the me_buf structure
	 * holds the mapped physical address and it must be
	 * set to 0 after unmapping.
	 */
	if (buf->paddr_macbuf) {
		qdf_mem_unmap_nbytes_single(pdev->soc->osdev,
					    buf->paddr_macbuf,
					    QDF_DMA_TO_DEVICE,
					    QDF_MAC_ADDR_SIZE);
		buf->paddr_macbuf = 0;
	}
	qdf_spin_lock_bh(&pdev->tx_mutex);
	buf->next = pdev->me_buf.freelist;
	pdev->me_buf.freelist = buf;
	pdev->me_buf.buf_in_use--;
	qdf_spin_unlock_bh(&pdev->tx_mutex);
}
#endif /* DP_TX_DESC_H */
