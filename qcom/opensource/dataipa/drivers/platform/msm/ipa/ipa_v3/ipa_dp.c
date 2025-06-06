
// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/inet.h>
#include <linux/if_ether.h>
#include <net/ip6_checksum.h>

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/msm_gsi.h>
#include <net/sock.h>
#include <net/ipv6.h>
#include <asm/page.h>
#include <linux/mutex.h>
#include "gsi.h"
#include "ipa_i.h"
#include "ipa_trace.h"
#include "ipahal.h"
#include "ipahal_fltrt.h"
#include "ipa_stats.h"
#include <rmnet_mem.h>

#define IPA_GSI_EVENT_RP_SIZE 8
#define IPA_WAN_NAPI_MAX_FRAMES (NAPI_WEIGHT / IPA_WAN_AGGR_PKT_CNT)
#define IPA_WAN_PAGE_ORDER 3
#define IPA_LAN_AGGR_PKT_CNT 1
#define IPA_LAN_NAPI_MAX_FRAMES (NAPI_WEIGHT / IPA_LAN_AGGR_PKT_CNT)
#define IPA_LAST_DESC_CNT 0xFFFF
#define POLLING_INACTIVITY_RX 40
#define POLLING_MIN_SLEEP_RX 1010
#define POLLING_MAX_SLEEP_RX 1050
#define POLLING_INACTIVITY_TX 40
#define POLLING_MIN_SLEEP_TX 400
#define POLLING_MAX_SLEEP_TX 500
#define SUSPEND_MIN_SLEEP_RX 1000
#define SUSPEND_MAX_SLEEP_RX 1005
/* 8K less 1 nominal MTU (1500 bytes) rounded to units of KB */
#define IPA_MTU 1500
#define IPA_GENERIC_AGGR_BYTE_LIMIT 6
#define IPA_GENERIC_AGGR_TIME_LIMIT 500 /* 0.5msec */
#define IPA_GENERIC_AGGR_PKT_LIMIT 0

#define IPA_GSB_AGGR_BYTE_LIMIT 14
#define IPA_GSB_RX_BUFF_BASE_SZ 16384
#define IPA_QMAP_RX_BUFF_BASE_SZ 576
#define IPA_GENERIC_RX_BUFF_BASE_SZ 8192
#define IPA_REAL_GENERIC_RX_BUFF_SZ(X) (SKB_DATA_ALIGN(\
		(X) + NET_SKB_PAD) +\
		SKB_DATA_ALIGN(sizeof(struct skb_shared_info)))
#define IPA_GENERIC_RX_BUFF_SZ(X) ((X) -\
		(IPA_REAL_GENERIC_RX_BUFF_SZ(X) - (X)))
#define IPA_GENERIC_RX_BUFF_LIMIT (\
		IPA_REAL_GENERIC_RX_BUFF_SZ(\
		IPA_GENERIC_RX_BUFF_BASE_SZ) -\
		IPA_GENERIC_RX_BUFF_BASE_SZ)

/* less 1 nominal MTU (1500 bytes) rounded to units of KB */
#define IPA_ADJUST_AGGR_BYTE_LIMIT(X) (((X) - IPA_MTU)/1000)

#define IPA_RX_BUFF_CLIENT_HEADROOM 256

#define IPA_WLAN_RX_POOL_SZ 100
#define IPA_WLAN_RX_POOL_SZ_LOW_WM 5
#define IPA_WLAN_RX_BUFF_SZ 2048
#define IPA_WLAN_COMM_RX_POOL_LOW 100
#define IPA_WLAN_COMM_RX_POOL_HIGH 900

#define IPA_ODU_RX_BUFF_SZ 2048
#define IPA_ODU_RX_POOL_SZ 64

#define IPA_ODL_RX_BUFF_SZ (16 * 1024)

#define IPA_GSI_MAX_CH_LOW_WEIGHT 15
#define IPA_GSI_EVT_RING_INT_MODT (16) /* 0.5ms under 32KHz clock */
#define IPA_GSI_EVT_RING_INT_MODC (20)

#define IPA_GSI_CH_20_WA_NUM_CH_TO_ALLOC 10
/* The below virtual channel cannot be used by any entity */
#define IPA_GSI_CH_20_WA_VIRT_CHAN 29

#define IPA_DEFAULT_SYS_YELLOW_WM 32
/* High threshold is set for 50% of the buffer */
#define IPA_BUFF_THRESHOLD_HIGH 112
#define IPA_REPL_XFER_THRESH 20
#define IPA_REPL_XFER_MAX 36

#define IPA_TX_SEND_COMPL_NOP_DELAY_NS (2 * 1000 * 1000)

#define IPA_APPS_BW_FOR_PM 700

#define IPA_SEND_MAX_DESC (20)

#define IPA_EOT_THRESH 32

#define IPA_QMAP_ID_BYTE 0

#define IPA_MEM_ALLOC_RETRY 5

static int ipa3_tx_switch_to_intr_mode(struct ipa3_sys_context *sys);
static int ipa3_rx_switch_to_intr_mode(struct ipa3_sys_context *sys);
static struct sk_buff *ipa3_get_skb_ipa_rx(unsigned int len, gfp_t flags);
static void ipa3_replenish_wlan_rx_cache(struct ipa3_sys_context *sys);
static void ipa3_replenish_rx_cache(struct ipa3_sys_context *sys);
static void ipa3_first_replenish_rx_cache(struct ipa3_sys_context *sys);
static void ipa3_replenish_rx_work_func(struct work_struct *work);
static void ipa3_fast_replenish_rx_cache(struct ipa3_sys_context *sys);
static void ipa3_replenish_rx_page_cache(struct ipa3_sys_context *sys);
static void ipa3_wq_page_repl(struct work_struct *work);
static void ipa3_replenish_rx_page_recycle(struct ipa3_sys_context *sys);
static struct ipa3_rx_pkt_wrapper *ipa3_alloc_rx_pkt_page(gfp_t flag,
	bool is_tmp_alloc, struct ipa3_sys_context *sys);
static void ipa3_wq_handle_rx(struct work_struct *work);
static void ipa3_wq_rx_common(struct ipa3_sys_context *sys,
	struct gsi_chan_xfer_notify *notify);
static void ipa3_rx_napi_chain(struct ipa3_sys_context *sys,
		struct gsi_chan_xfer_notify *notify, uint32_t num);
static void ipa3_wlan_wq_rx_common(struct ipa3_sys_context *sys,
				struct gsi_chan_xfer_notify *notify);
static int ipa3_assign_policy(struct ipa_sys_connect_params *in,
		struct ipa3_sys_context *sys);
static void ipa3_cleanup_rx(struct ipa3_sys_context *sys);
static void ipa3_wq_rx_avail(struct work_struct *work);
static void ipa3_alloc_wlan_rx_common_cache(u32 size);
static void ipa3_cleanup_wlan_rx_common_cache(void);
static void ipa3_wq_repl_rx(struct work_struct *work);
static void ipa3_dma_memcpy_notify(struct ipa3_sys_context *sys);
static int ipa_gsi_setup_coal_def_channel(struct ipa_sys_connect_params *in,
	struct ipa3_ep_context *ep, struct ipa3_ep_context *coal_ep);
static int ipa_gsi_setup_channel(struct ipa_sys_connect_params *in,
	struct ipa3_ep_context *ep);
static int ipa_gsi_setup_event_ring(struct ipa3_ep_context *ep,
	u32 ring_size, gfp_t mem_flag);
static int ipa_gsi_setup_transfer_ring(struct ipa3_ep_context *ep,
	u32 ring_size, struct ipa3_sys_context *user_data, gfp_t mem_flag);
static int ipa3_teardown_pipe(u32 clnt_hdl);
static int ipa_populate_tag_field(struct ipa3_desc *desc,
		struct ipa3_tx_pkt_wrapper *tx_pkt,
		struct ipahal_imm_cmd_pyld **tag_pyld_ret);
static int ipa_poll_gsi_pkt(struct ipa3_sys_context *sys,
	struct gsi_chan_xfer_notify *notify);
static int ipa_poll_gsi_n_pkt(struct ipa3_sys_context *sys,
	struct gsi_chan_xfer_notify *notify, int expected_num,
	int *actual_num);
static unsigned long tag_to_pointer_wa(uint64_t tag);
static uint64_t pointer_to_tag_wa(struct ipa3_tx_pkt_wrapper *tx_pkt);
static void ipa3_tasklet_rx_notify(unsigned long data);
static void ipa3_tasklet_find_freepage(unsigned long data);
static u32 ipa_adjust_ra_buff_base_sz(u32 aggr_byte_limit);
static int ipa3_rmnet_ll_rx_poll(struct napi_struct *napi_rx, int budget);

struct gsi_chan_xfer_notify g_lan_rx_notify[IPA_LAN_NAPI_MAX_FRAMES];

static void ipa3_collect_default_coal_recycle_stats_wq(struct work_struct *work);
static DECLARE_DELAYED_WORK(ipa3_collect_default_coal_recycle_stats_wq_work,
	ipa3_collect_default_coal_recycle_stats_wq);

static void ipa3_collect_low_lat_data_recycle_stats_wq(struct work_struct *work);
static DECLARE_DELAYED_WORK(ipa3_collect_low_lat_data_recycle_stats_wq_work,
	ipa3_collect_low_lat_data_recycle_stats_wq);

static void ipa3_collect_default_coal_recycle_stats_wq(struct work_struct *work)
{
	struct ipa3_sys_context *sys;
	int stat_interval_index;
	int ep_idx = -1;

	/* For targets which don't require coalescing pipe */
	ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_WAN_COAL_CONS);
	if (ep_idx == -1)
		ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);

	if (ep_idx == -1)
		sys = NULL;
	else
		sys = ipa3_ctx->ep[ep_idx].sys;

	mutex_lock(&ipa3_ctx->recycle_stats_collection_lock);
	stat_interval_index = ipa3_ctx->recycle_stats.default_coal_stats_index;
	ipa3_ctx->recycle_stats.interval_time_in_ms = IPA_LNX_PIPE_PAGE_RECYCLING_INTERVAL_TIME;

	/* Coalescing pipe page recycling stats */
	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_COALESCING][stat_interval_index].total_cumulative
			= ipa3_ctx->stats.page_recycle_stats[0].total_replenished;
	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_COALESCING][stat_interval_index].recycle_cumulative
			= ipa3_ctx->stats.page_recycle_stats[0].page_recycled;
	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_COALESCING][stat_interval_index].temp_cumulative
			= ipa3_ctx->stats.page_recycle_stats[0].tmp_alloc;

	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_COALESCING][stat_interval_index].total_diff
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_COALESCING][stat_interval_index].total_cumulative
			- ipa3_ctx->prev_coal_recycle_stats.total_replenished;
	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_COALESCING][stat_interval_index].recycle_diff
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_COALESCING][stat_interval_index].recycle_cumulative
			- ipa3_ctx->prev_coal_recycle_stats.page_recycled;
	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_COALESCING][stat_interval_index].temp_diff
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_COALESCING][stat_interval_index].temp_cumulative
			- ipa3_ctx->prev_coal_recycle_stats.tmp_alloc;

	ipa3_ctx->prev_coal_recycle_stats.total_replenished
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_COALESCING][stat_interval_index].total_cumulative;
	ipa3_ctx->prev_coal_recycle_stats.page_recycled
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_COALESCING][stat_interval_index].recycle_cumulative;
	ipa3_ctx->prev_coal_recycle_stats.tmp_alloc
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_COALESCING][stat_interval_index].temp_cumulative;

	/* Default pipe page recycling stats */
	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_DEFAULT][stat_interval_index].total_cumulative
			= ipa3_ctx->stats.page_recycle_stats[1].total_replenished;
	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_DEFAULT][stat_interval_index].recycle_cumulative
			= ipa3_ctx->stats.page_recycle_stats[1].page_recycled;
	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_DEFAULT][stat_interval_index].temp_cumulative
			= ipa3_ctx->stats.page_recycle_stats[1].tmp_alloc;

	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_DEFAULT][stat_interval_index].total_diff
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_DEFAULT][stat_interval_index].total_cumulative
			- ipa3_ctx->prev_default_recycle_stats.total_replenished;
	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_DEFAULT][stat_interval_index].recycle_diff
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_DEFAULT][stat_interval_index].recycle_cumulative
			- ipa3_ctx->prev_default_recycle_stats.page_recycled;
	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_DEFAULT][stat_interval_index].temp_diff
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_DEFAULT][stat_interval_index].temp_cumulative
			- ipa3_ctx->prev_default_recycle_stats.tmp_alloc;

	ipa3_ctx->prev_default_recycle_stats.total_replenished
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_DEFAULT][stat_interval_index].total_cumulative;
	ipa3_ctx->prev_default_recycle_stats.page_recycled
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_DEFAULT][stat_interval_index].recycle_cumulative;
	ipa3_ctx->prev_default_recycle_stats.tmp_alloc
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_DEFAULT][stat_interval_index].temp_cumulative;

	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_COALESCING][stat_interval_index].valid = 1;
	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_DEFAULT][stat_interval_index].valid = 1;

	/* Single Indexing for coalescing and default pipe */
	ipa3_ctx->recycle_stats.default_coal_stats_index =
			(ipa3_ctx->recycle_stats.default_coal_stats_index + 1) % IPA_LNX_PIPE_PAGE_RECYCLING_INTERVAL_COUNT;

	if (sys && atomic_read(&sys->curr_polling_state))
		queue_delayed_work(ipa3_ctx->collect_recycle_stats_wq,
				&ipa3_collect_default_coal_recycle_stats_wq_work, msecs_to_jiffies(10));

	mutex_unlock(&ipa3_ctx->recycle_stats_collection_lock);

	return;

}

static void ipa3_collect_low_lat_data_recycle_stats_wq(struct work_struct *work)
{
	struct ipa3_sys_context *sys;
	int stat_interval_index;
	int ep_idx;

	ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS);
	if (ep_idx == -1)
		sys = NULL;
	else
		sys = ipa3_ctx->ep[ep_idx].sys;

	mutex_lock(&ipa3_ctx->recycle_stats_collection_lock);
	stat_interval_index = ipa3_ctx->recycle_stats.low_lat_stats_index;

	/* Low latency data pipe page recycling stats */
	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_LOW_LAT_DATA][stat_interval_index].total_cumulative
			= ipa3_ctx->stats.page_recycle_stats[2].total_replenished;
	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_LOW_LAT_DATA][stat_interval_index].recycle_cumulative
			= ipa3_ctx->stats.page_recycle_stats[2].page_recycled;
	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_LOW_LAT_DATA][stat_interval_index].temp_cumulative
			= ipa3_ctx->stats.page_recycle_stats[2].tmp_alloc;

	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_LOW_LAT_DATA][stat_interval_index].total_diff
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_LOW_LAT_DATA][stat_interval_index].total_cumulative
			- ipa3_ctx->prev_low_lat_data_recycle_stats.total_replenished;
	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_LOW_LAT_DATA][stat_interval_index].recycle_diff
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_LOW_LAT_DATA][stat_interval_index].recycle_cumulative
			- ipa3_ctx->prev_low_lat_data_recycle_stats.page_recycled;
	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_LOW_LAT_DATA][stat_interval_index].temp_diff
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_LOW_LAT_DATA][stat_interval_index].temp_cumulative
			- ipa3_ctx->prev_low_lat_data_recycle_stats.tmp_alloc;

	ipa3_ctx->prev_low_lat_data_recycle_stats.total_replenished
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_LOW_LAT_DATA][stat_interval_index].total_cumulative;
	ipa3_ctx->prev_low_lat_data_recycle_stats.page_recycled
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_LOW_LAT_DATA][stat_interval_index].recycle_cumulative;
	ipa3_ctx->prev_low_lat_data_recycle_stats.tmp_alloc
			= ipa3_ctx->recycle_stats.rx_channel[RX_WAN_LOW_LAT_DATA][stat_interval_index].temp_cumulative;

	ipa3_ctx->recycle_stats.rx_channel[RX_WAN_LOW_LAT_DATA][stat_interval_index].valid = 1;

	/* Indexing for low lat data stats pipe */
	ipa3_ctx->recycle_stats.low_lat_stats_index =
			(ipa3_ctx->recycle_stats.low_lat_stats_index + 1) % IPA_LNX_PIPE_PAGE_RECYCLING_INTERVAL_COUNT;

	if (sys && atomic_read(&sys->curr_polling_state))
		queue_delayed_work(ipa3_ctx->collect_recycle_stats_wq,
				&ipa3_collect_low_lat_data_recycle_stats_wq_work, msecs_to_jiffies(10));

	mutex_unlock(&ipa3_ctx->recycle_stats_collection_lock);

	return;
}

/**
 * ipa3_write_done_common() - this function is responsible on freeing
 * all tx_pkt_wrappers related to a skb
 * @tx_pkt: the first tx_pkt_warpper related to a certain skb
 * @sys:points to the ipa3_sys_context the EOT was received on
 * returns the number of tx_pkt_wrappers that were freed
 */
static int ipa3_write_done_common(struct ipa3_sys_context *sys,
				struct ipa3_tx_pkt_wrapper *tx_pkt)
{
	struct ipa3_tx_pkt_wrapper *next_pkt;
	int i, cnt;
	void *user1;
	int user2;
	void (*callback)(void *user1, int user2);

	if (unlikely(tx_pkt == NULL)) {
		IPAERR("tx_pkt is NULL\n");
		return 0;
	}

	cnt = tx_pkt->cnt;
	for (i = 0; i < cnt; i++) {
		spin_lock_bh(&sys->spinlock);
		if (unlikely(list_empty(&sys->head_desc_list))) {
			spin_unlock_bh(&sys->spinlock);
			IPAERR_RL("list is empty missing descriptors");
			return i;
		}
		next_pkt = list_next_entry(tx_pkt, link);
		list_del(&tx_pkt->link);
		sys->len--;
		if (!tx_pkt->no_unmap_dma) {
			if (tx_pkt->type != IPA_DATA_DESC_SKB_PAGED) {
				dma_unmap_single(ipa3_ctx->pdev,
					tx_pkt->mem.phys_base,
					tx_pkt->mem.size,
					DMA_TO_DEVICE);
			} else {
				dma_unmap_page(ipa3_ctx->pdev,
					tx_pkt->mem.phys_base,
					tx_pkt->mem.size,
					DMA_TO_DEVICE);
			}
		}
		callback = tx_pkt->callback;
		user1 = tx_pkt->user1;
		user2 = tx_pkt->user2;
		if (sys->avail_tx_wrapper >=
			ipa3_ctx->tx_wrapper_cache_max_size ||
			sys->ep->client == IPA_CLIENT_APPS_CMD_PROD) {
			kmem_cache_free(ipa3_ctx->tx_pkt_wrapper_cache,
				tx_pkt);
		} else {
			list_add_tail(&tx_pkt->link,
				&sys->avail_tx_wrapper_list);
			sys->avail_tx_wrapper++;
		}
		spin_unlock_bh(&sys->spinlock);
		if (callback)
			(*callback)(user1, user2);
		tx_pkt = next_pkt;
	}
	return i;
}

static void ipa3_wq_write_done_status(int src_pipe,
			struct ipa3_tx_pkt_wrapper *tx_pkt)
{
	struct ipa3_sys_context *sys;

	WARN_ON(src_pipe >= ipa3_ctx->ipa_num_pipes);

	if (!ipa3_ctx->ep[src_pipe].status.status_en)
		return;

	sys = ipa3_ctx->ep[src_pipe].sys;
	if (!sys)
		return;

	ipa3_write_done_common(sys, tx_pkt);
}

/**
 * ipa_write_done() - this function will be (eventually) called when a Tx
 * operation is complete
 * @data: user pointer point to the ipa3_sys_context
 *
 * Will be called in deferred context.
 * - invoke the callback supplied by the client who sent this command
 * - iterate over all packets and validate that
 *   the order for sent packet is the same as expected
 * - delete all the tx packet descriptors from the system
 *   pipe context (not needed anymore)
 */
static void ipa3_tasklet_write_done(unsigned long data)
{
	struct ipa3_sys_context *sys;
	struct ipa3_tx_pkt_wrapper *this_pkt;
	bool xmit_done = false;

	sys = (struct ipa3_sys_context *)data;
	spin_lock_bh(&sys->spinlock);
	while (atomic_add_unless(&sys->xmit_eot_cnt, -1, 0)) {
		while (!list_empty(&sys->head_desc_list)) {
			this_pkt = list_first_entry(&sys->head_desc_list,
				struct ipa3_tx_pkt_wrapper, link);
			xmit_done = this_pkt->xmit_done;
			spin_unlock_bh(&sys->spinlock);
			ipa3_write_done_common(sys, this_pkt);
			spin_lock_bh(&sys->spinlock);
			if (xmit_done)
				break;
		}
	}
	spin_unlock_bh(&sys->spinlock);
}

static int ipa3_napi_poll_tx_complete(struct ipa3_sys_context *sys, int budget)
{
	struct ipa3_tx_pkt_wrapper *this_pkt = NULL;
	int entry_budget = budget;
	int poll_status = 0;
	int num_of_desc = 0;
	int i = 0;
	struct gsi_chan_xfer_notify notify[NAPI_TX_WEIGHT];

	do {
		poll_status =
			ipa_poll_gsi_n_pkt(sys, notify, budget, &num_of_desc);
		for(i = 0; i < num_of_desc; i++) {
			this_pkt = notify[i].xfer_user_data;
			/* For shared event ring sys context might change */
			sys = this_pkt->sys;
			ipa3_write_done_common(sys, this_pkt);
			budget--;
		}
		IPADBG_LOW("Number of desc polled %d", num_of_desc);
	} while(budget > 0 && !poll_status);
	return entry_budget - budget;
}

static int ipa3_aux_napi_poll_tx_complete(struct napi_struct *napi_tx,
						int budget)
{
	struct ipa3_sys_context *sys = container_of(napi_tx,
		struct ipa3_sys_context, napi_tx);
	bool napi_rescheduled = false;
	int tx_done = 0;
	int ret = 0;

	tx_done += ipa3_napi_poll_tx_complete(sys, budget - tx_done);

	/* Doorbell needed here for continuous polling */
	gsi_ring_evt_doorbell_polling_mode(sys->ep->gsi_chan_hdl);

	if (tx_done < budget) {
		napi_complete(napi_tx);
		ret = ipa3_tx_switch_to_intr_mode(sys);

		/* if we got an EOT while we marked NAPI as complete */
		if (ret == -GSI_STATUS_PENDING_IRQ && napi_reschedule(napi_tx)) {
			/* rescheduale will perform poll again, don't dec vote twice*/
			napi_rescheduled = true;
		}

		if(!napi_rescheduled)
			IPA_ACTIVE_CLIENTS_DEC_EP_NO_BLOCK(sys->ep->client);
	}
	IPADBG_LOW("the number of tx completions is: %d", tx_done);
	return min(tx_done, budget);
}

static int ipa3_napi_tx_complete(struct ipa3_sys_context *sys, int budget)
{
	struct ipa3_tx_pkt_wrapper *this_pkt = NULL;
	bool xmit_done = false;
	int entry_budget = budget;

	spin_lock_bh(&sys->spinlock);
	while (budget > 0 && atomic_read(&sys->xmit_eot_cnt) > 0) {
		if (unlikely(list_empty(&sys->head_desc_list))) {
			IPADBG_LOW("list is empty");
			break;
		}
		this_pkt = list_first_entry(&sys->head_desc_list,
			struct ipa3_tx_pkt_wrapper, link);
		xmit_done = this_pkt->xmit_done;
		spin_unlock_bh(&sys->spinlock);
		budget -= ipa3_write_done_common(sys, this_pkt);
		spin_lock_bh(&sys->spinlock);
		if (xmit_done)
			atomic_add_unless(&sys->xmit_eot_cnt, -1, 0);
	}
	spin_unlock_bh(&sys->spinlock);
	return entry_budget - budget;
}

static int ipa3_aux_napi_tx_complete(struct napi_struct *napi_tx, int budget)
{
	struct ipa3_sys_context *sys = container_of(napi_tx,
		struct ipa3_sys_context, napi_tx);
	int tx_done = 0;

poll_tx:
	tx_done += ipa3_napi_tx_complete(sys, budget - tx_done);
	if (tx_done < budget) {
		napi_complete(napi_tx);
		atomic_set(&sys->in_napi_context, 0);

		/*if we got an EOT while we marked NAPI as complete*/
		if (atomic_read(&sys->xmit_eot_cnt) > 0 &&
		    !atomic_cmpxchg(&sys->in_napi_context, 0, 1)
		    && napi_reschedule(napi_tx)) {
			goto poll_tx;
		}
	}
	IPADBG_LOW("the number of tx completions is: %d", tx_done);
	return min(tx_done, budget);
}

static void ipa3_send_nop_desc(struct work_struct *work)
{
	struct ipa3_sys_context *sys = container_of(work,
		struct ipa3_sys_context, work);
	struct gsi_xfer_elem nop_xfer;
	struct ipa3_tx_pkt_wrapper *tx_pkt;

	IPADBG_LOW("gsi send NOP for ch: %lu\n", sys->ep->gsi_chan_hdl);
	if (atomic_read(&sys->workqueue_flushed))
		return;

	spin_lock_bh(&sys->spinlock);
	if (!list_empty(&sys->avail_tx_wrapper_list)) {
		tx_pkt = list_first_entry(&sys->avail_tx_wrapper_list,
				struct ipa3_tx_pkt_wrapper, link);
		list_del(&tx_pkt->link);
		sys->avail_tx_wrapper--;
		memset(tx_pkt, 0, sizeof(struct ipa3_tx_pkt_wrapper));
	} else {
		spin_unlock_bh(&sys->spinlock);
		tx_pkt = kmem_cache_zalloc(ipa3_ctx->tx_pkt_wrapper_cache,
			GFP_KERNEL);
		spin_lock_bh(&sys->spinlock);
	}
	if (!tx_pkt) {
		spin_unlock_bh(&sys->spinlock);
		queue_work(sys->wq, &sys->work);
		return;
	}

	INIT_LIST_HEAD(&tx_pkt->link);
	tx_pkt->cnt = 1;
	tx_pkt->no_unmap_dma = true;
	tx_pkt->sys = sys;
	if (unlikely(!sys->nop_pending)) {
		spin_unlock_bh(&sys->spinlock);
		kmem_cache_free(ipa3_ctx->tx_pkt_wrapper_cache, tx_pkt);
		return;
	}
	list_add_tail(&tx_pkt->link, &sys->head_desc_list);

	memset(&nop_xfer, 0, sizeof(nop_xfer));
	nop_xfer.type = GSI_XFER_ELEM_NOP;
	nop_xfer.flags = GSI_XFER_FLAG_EOT;
	nop_xfer.xfer_user_data = tx_pkt;
	if (gsi_queue_xfer(sys->ep->gsi_chan_hdl, 1, &nop_xfer, true)) {
		list_del(&tx_pkt->link);
		kmem_cache_free(ipa3_ctx->tx_pkt_wrapper_cache, tx_pkt);
		spin_unlock_bh(&sys->spinlock);
		IPAERR("gsi_queue_xfer for ch:%lu failed\n",
			sys->ep->gsi_chan_hdl);
		queue_work(sys->wq, &sys->work);
		return;
	}
	sys->len++;
	sys->nop_pending = false;
	spin_unlock_bh(&sys->spinlock);

	/* make sure TAG process is sent before clocks are gated */
	ipa3_ctx->tag_process_before_gating = true;
}


/**
 * ipa3_send() - Send multiple descriptors in one HW transaction
 * @sys: system pipe context
 * @num_desc: number of packets
 * @desc: packets to send (may be immediate command or data)
 * @in_atomic:  whether caller is in atomic context
 *
 * This function is used for GPI connection.
 * - ipa3_tx_pkt_wrapper will be used for each ipa
 *   descriptor (allocated from wrappers cache)
 * - The wrapper struct will be configured for each ipa-desc payload and will
 *   contain information which will be later used by the user callbacks
 * - Each packet (command or data) that will be sent will also be saved in
 *   ipa3_sys_context for later check that all data was sent
 *
 * Return codes: 0: success, -EFAULT: failure
 */
int ipa3_send(struct ipa3_sys_context *sys,
		u32 num_desc,
		struct ipa3_desc *desc,
		bool in_atomic)
{
	struct ipa3_tx_pkt_wrapper *tx_pkt, *tx_pkt_first = NULL;
	struct ipahal_imm_cmd_pyld *tag_pyld_ret = NULL;
	struct ipa3_tx_pkt_wrapper *next_pkt;
	struct gsi_xfer_elem gsi_xfer[IPA_SEND_MAX_DESC];
	int i = 0;
	int j;
	int result;
	u32 mem_flag = GFP_ATOMIC;
	const struct ipa_gsi_ep_config *gsi_ep_cfg;
	bool send_nop = false;
	unsigned int max_desc;

	if (unlikely(!in_atomic))
		mem_flag = GFP_KERNEL;

	gsi_ep_cfg = ipa_get_gsi_ep_info(sys->ep->client);
	if (unlikely(!gsi_ep_cfg)) {
		IPAERR("failed to get gsi EP config for client=%d\n",
			sys->ep->client);
		return -EFAULT;
	}
	if (unlikely(num_desc > IPA_SEND_MAX_DESC)) {
		IPAERR("max descriptors reached need=%d max=%d\n",
			num_desc, IPA_SEND_MAX_DESC);
		WARN_ON(1);
		return -EPERM;
	}

	max_desc = gsi_ep_cfg->ipa_if_tlv;
	if (gsi_ep_cfg->prefetch_mode == GSI_SMART_PRE_FETCH ||
		gsi_ep_cfg->prefetch_mode == GSI_FREE_PRE_FETCH)
		max_desc -= gsi_ep_cfg->prefetch_threshold;

	if (unlikely(num_desc > max_desc)) {
		IPAERR("Too many chained descriptors need=%d max=%d\n",
			num_desc, max_desc);
		WARN_ON(1);
		return -EPERM;
	}

	/* initialize only the xfers we use */
	memset(gsi_xfer, 0, sizeof(gsi_xfer[0]) * num_desc);

	spin_lock_bh(&sys->spinlock);

	if (unlikely(atomic_read(&sys->ep->disconnect_in_progress))) {
		IPAERR("Pipe disconnect in progress dropping the packet\n");
		spin_unlock_bh(&sys->spinlock);
		return -EFAULT;
	}

	for (i = 0; i < num_desc; i++) {
		if (!list_empty(&sys->avail_tx_wrapper_list)) {
			tx_pkt = list_first_entry(&sys->avail_tx_wrapper_list,
				struct ipa3_tx_pkt_wrapper, link);
			list_del(&tx_pkt->link);
			sys->avail_tx_wrapper--;

			memset(tx_pkt, 0, sizeof(struct ipa3_tx_pkt_wrapper));
		} else {
			tx_pkt = kmem_cache_zalloc(
				ipa3_ctx->tx_pkt_wrapper_cache,
				GFP_ATOMIC);
		}
		if (!tx_pkt) {
			IPAERR("failed to alloc tx wrapper\n");
			result = -ENOMEM;
			goto failure;
		}
		INIT_LIST_HEAD(&tx_pkt->link);

		if (i == 0) {
			tx_pkt_first = tx_pkt;
			tx_pkt->cnt = num_desc;
		}

		/* populate tag field */
		if (desc[i].is_tag_status) {
			if (ipa_populate_tag_field(&desc[i], tx_pkt,
				&tag_pyld_ret)) {
				IPAERR("Failed to populate tag field\n");
				result = -EFAULT;
				goto failure_dma_map;
			}
		}

		tx_pkt->type = desc[i].type;

		if (desc[i].type != IPA_DATA_DESC_SKB_PAGED) {
			tx_pkt->mem.base = desc[i].pyld;
			tx_pkt->mem.size = desc[i].len;

			if (!desc[i].dma_address_valid) {
				tx_pkt->mem.phys_base =
					dma_map_single(ipa3_ctx->pdev,
					tx_pkt->mem.base,
					tx_pkt->mem.size,
					DMA_TO_DEVICE);
			} else {
				tx_pkt->mem.phys_base =
					desc[i].dma_address;
				tx_pkt->no_unmap_dma = true;
			}
		} else {
			tx_pkt->mem.base = desc[i].frag;
			tx_pkt->mem.size = desc[i].len;

			if (!desc[i].dma_address_valid) {
				tx_pkt->mem.phys_base =
					skb_frag_dma_map(ipa3_ctx->pdev,
					desc[i].frag,
					0, tx_pkt->mem.size,
					DMA_TO_DEVICE);
			} else {
				tx_pkt->mem.phys_base =
					desc[i].dma_address;
				tx_pkt->no_unmap_dma = true;
			}
		}
		if (dma_mapping_error(ipa3_ctx->pdev, tx_pkt->mem.phys_base)) {
			IPAERR("failed to do dma map.\n");
			result = -EFAULT;
			goto failure_dma_map;
		}

		tx_pkt->sys = sys;
		tx_pkt->callback = desc[i].callback;
		tx_pkt->user1 = desc[i].user1;
		tx_pkt->user2 = desc[i].user2;
		tx_pkt->xmit_done = false;

		list_add_tail(&tx_pkt->link, &sys->head_desc_list);
		sys->len++;
		gsi_xfer[i].addr = tx_pkt->mem.phys_base;

		/*
		 * Special treatment for immediate commands, where
		 * the structure of the descriptor is different
		 */
		if (desc[i].type == IPA_IMM_CMD_DESC) {
			gsi_xfer[i].len = desc[i].opcode;
			gsi_xfer[i].type =
				GSI_XFER_ELEM_IMME_CMD;
		} else {
			gsi_xfer[i].len = desc[i].len;
			gsi_xfer[i].type =
				GSI_XFER_ELEM_DATA;
		}

		if (i == (num_desc - 1)) {
			if (ipa3_ctx->tx_poll ||
				!sys->use_comm_evt_ring ||
				(sys->pkt_sent % IPA_EOT_THRESH == 0)) {
				gsi_xfer[i].flags |=
					GSI_XFER_FLAG_EOT;
				gsi_xfer[i].flags |=
					GSI_XFER_FLAG_BEI;
				hrtimer_try_to_cancel(&sys->db_timer);
				sys->nop_pending = false;
			} else {
				send_nop = true;
			}
			gsi_xfer[i].xfer_user_data =
				tx_pkt_first;
		} else {
			gsi_xfer[i].flags |=
				GSI_XFER_FLAG_CHAIN;
		}
	}

	IPADBG_LOW("ch:%lu queue xfer\n", sys->ep->gsi_chan_hdl);
	result = gsi_queue_xfer(sys->ep->gsi_chan_hdl, num_desc,
			gsi_xfer, true);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR_RL("GSI xfer failed.\n");
		result = -EFAULT;
		goto failure;
	}

	if (send_nop && !sys->nop_pending)
		sys->nop_pending = true;
	else
		send_nop = false;

	sys->pkt_sent++;
	spin_unlock_bh(&sys->spinlock);

	/* set the timer for sending the NOP descriptor */
	if (send_nop) {
		ktime_t time = ktime_set(0, IPA_TX_SEND_COMPL_NOP_DELAY_NS);

		IPADBG_LOW("scheduling timer for ch %lu\n",
			sys->ep->gsi_chan_hdl);
		hrtimer_start(&sys->db_timer, time, HRTIMER_MODE_REL);
	}

	/* make sure TAG process is sent before clocks are gated */
	ipa3_ctx->tag_process_before_gating = true;

	return 0;

failure_dma_map:
	kmem_cache_free(ipa3_ctx->tx_pkt_wrapper_cache, tx_pkt);

failure:
	ipahal_destroy_imm_cmd(tag_pyld_ret);
	tx_pkt = tx_pkt_first;
	for (j = 0; j < i; j++) {
		next_pkt = list_next_entry(tx_pkt, link);
		list_del(&tx_pkt->link);
		sys->len--;

		if (!tx_pkt->no_unmap_dma) {
			if (desc[j].type != IPA_DATA_DESC_SKB_PAGED) {
				dma_unmap_single(ipa3_ctx->pdev,
					tx_pkt->mem.phys_base,
					tx_pkt->mem.size, DMA_TO_DEVICE);
			} else {
				dma_unmap_page(ipa3_ctx->pdev,
					tx_pkt->mem.phys_base,
					tx_pkt->mem.size,
					DMA_TO_DEVICE);
			}
		}
		kmem_cache_free(ipa3_ctx->tx_pkt_wrapper_cache, tx_pkt);
		tx_pkt = next_pkt;
	}

	spin_unlock_bh(&sys->spinlock);
	return result;
}

/**
 * ipa3_send_one() - Send a single descriptor
 * @sys:	system pipe context
 * @desc:	descriptor to send
 * @in_atomic:  whether caller is in atomic context
 *
 * - Allocate tx_packet wrapper
 * - transfer data to the IPA
 * - after the transfer was done the SPS will
 *   notify the sending user via ipa_sps_irq_comp_tx()
 *
 * Return codes: 0: success, -EFAULT: failure
 */
int ipa3_send_one(struct ipa3_sys_context *sys, struct ipa3_desc *desc,
	bool in_atomic)
{
	return ipa3_send(sys, 1, desc, in_atomic);
}

/**
 * ipa3_transport_irq_cmd_ack - callback function which will be called by
 * the transport driver after an immediate command is complete.
 * @user1:	pointer to the descriptor of the transfer
 * @user2:
 *
 * Complete the immediate commands completion object, this will release the
 * thread which waits on this completion object (ipa3_send_cmd())
 */
static void ipa3_transport_irq_cmd_ack(void *user1, int user2)
{
	struct ipa3_desc *desc = (struct ipa3_desc *)user1;

	if (WARN(!desc, "desc is NULL"))
		return;

	IPADBG_LOW("got ack for cmd=%d\n", desc->opcode);
	complete(&desc->xfer_done);
}

/**
 * ipa3_transport_irq_cmd_ack_free - callback function which will be
 * called by the transport driver after an immediate command is complete.
 * This function will also free the completion object once it is done.
 * @tag_comp: pointer to the completion object
 * @ignored: parameter not used
 *
 * Complete the immediate commands completion object, this will release the
 * thread which waits on this completion object (ipa3_send_cmd())
 */
static void ipa3_transport_irq_cmd_ack_free(void *tag_comp, int ignored)
{
	struct ipa3_tag_completion *comp = tag_comp;

	if (!comp) {
		IPAERR("comp is NULL\n");
		return;
	}

	complete(&comp->comp);
	if (atomic_dec_return(&comp->cnt) == 0)
		kfree(comp);
}

/**
 * ipa3_send_cmd - send immediate commands
 * @num_desc:	number of descriptors within the desc struct
 * @descr:	descriptor structure
 *
 * Function will block till command gets ACK from IPA HW, caller needs
 * to free any resources it allocated after function returns
 * The callback in ipa3_desc should not be set by the caller
 * for this function.
 */
int ipa3_send_cmd(u16 num_desc, struct ipa3_desc *descr)
{
	struct ipa3_desc *desc;
	int i, result = 0;
	struct ipa3_sys_context *sys;
	int ep_idx;

	for (i = 0; i < num_desc; i++)
		IPADBG("sending imm cmd %d\n", descr[i].opcode);

	ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_CMD_PROD);
	if (-1 == ep_idx) {
		IPAERR("Client %u is not mapped\n",
			IPA_CLIENT_APPS_CMD_PROD);
		return -EFAULT;
	}

	sys = ipa3_ctx->ep[ep_idx].sys;
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	if (num_desc == 1) {
		init_completion(&descr->xfer_done);

		if (descr->callback || descr->user1)
			WARN_ON(1);

		descr->callback = ipa3_transport_irq_cmd_ack;
		descr->user1 = descr;
		if (ipa3_send_one(sys, descr, true)) {
			IPAERR("fail to send immediate command\n");
			result = -EFAULT;
			goto bail;
		}
		wait_for_completion(&descr->xfer_done);
	} else {
		desc = &descr[num_desc - 1];
		init_completion(&desc->xfer_done);

		if (desc->callback || desc->user1)
			WARN_ON(1);

		desc->callback = ipa3_transport_irq_cmd_ack;
		desc->user1 = desc;
		if (ipa3_send(sys, num_desc, descr, true)) {
			IPAERR("fail to send multiple immediate command set\n");
			result = -EFAULT;
			goto bail;
		}
		wait_for_completion(&desc->xfer_done);
	}

bail:
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return result;
}

/**
 * ipa3_send_cmd_timeout - send immediate commands with limited time
 *	waiting for ACK from IPA HW
 * @num_desc:	number of descriptors within the desc struct
 * @descr:	descriptor structure
 * @timeout:	millisecond to wait till get ACK from IPA HW
 *
 * Function will block till command gets ACK from IPA HW or timeout.
 * Caller needs to free any resources it allocated after function returns
 * The callback in ipa3_desc should not be set by the caller
 * for this function.
 */
int ipa3_send_cmd_timeout(u16 num_desc, struct ipa3_desc *descr, u32 timeout)
{
	struct ipa3_desc *desc;
	int i, result = 0;
	struct ipa3_sys_context *sys;
	int ep_idx;
	int completed;
	struct ipa3_tag_completion *comp;

	for (i = 0; i < num_desc; i++)
		IPADBG("sending imm cmd %d\n", descr[i].opcode);

	ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_CMD_PROD);
	if (-1 == ep_idx) {
		IPAERR("Client %u is not mapped\n",
			IPA_CLIENT_APPS_CMD_PROD);
		return -EFAULT;
	}

	comp = kzalloc(sizeof(*comp), GFP_ATOMIC);
	if (!comp)
		return -ENOMEM;

	init_completion(&comp->comp);

	/* completion needs to be released from both here and in ack callback */
	atomic_set(&comp->cnt, 2);

	sys = ipa3_ctx->ep[ep_idx].sys;

	if (num_desc == 1) {
		if (descr->callback || descr->user1)
			WARN_ON(1);

		descr->callback = ipa3_transport_irq_cmd_ack_free;
		descr->user1 = comp;
		if (ipa3_send_one(sys, descr, true)) {
			IPAERR("fail to send immediate command\n");
			kfree(comp);
			result = -EFAULT;
			goto bail;
		}
	} else {
		desc = &descr[num_desc - 1];

		if (desc->callback || desc->user1)
			WARN_ON(1);

		desc->callback = ipa3_transport_irq_cmd_ack_free;
		desc->user1 = comp;
		if (ipa3_send(sys, num_desc, descr, true)) {
			IPAERR("fail to send multiple immediate command set\n");
			kfree(comp);
			result = -EFAULT;
			goto bail;
		}
	}

	completed = wait_for_completion_timeout(
		&comp->comp, msecs_to_jiffies(timeout));
	if (!completed) {
		IPADBG("timeout waiting for imm-cmd ACK\n");
		result = -EBUSY;
	}

	if (atomic_dec_return(&comp->cnt) == 0)
		kfree(comp);

bail:
	return result;
}

/**
 * ipa3_handle_rx_core() - The core functionality of packet reception. This
 * function is read from multiple code paths.
 *
 * All the packets on the Rx data path are received on the IPA_A5_LAN_WAN_IN
 * endpoint. The function runs as long as there are packets in the pipe.
 * For each packet:
 *  - Disconnect the packet from the system pipe linked list
 *  - Unmap the packets skb, make it non DMAable
 *  - Free the packet from the cache
 *  - Prepare a proper skb
 *  - Call the endpoints notify function, passing the skb in the parameters
 *  - Replenish the rx cache
 */
static int ipa3_handle_rx_core(struct ipa3_sys_context *sys, bool process_all,
		bool in_poll_state)
{
	int ret;
	int cnt = 0;
	struct gsi_chan_xfer_notify notify = { 0 };

	while ((in_poll_state ? atomic_read(&sys->curr_polling_state) :
		!atomic_read(&sys->curr_polling_state))) {
		if (cnt && !process_all)
			break;

		ret = ipa_poll_gsi_pkt(sys, &notify);
		if (ret)
			break;

		if (IPA_CLIENT_IS_MEMCPY_DMA_CONS(sys->ep->client))
			ipa3_dma_memcpy_notify(sys);
		else if (IPA_CLIENT_IS_WLAN_CONS(sys->ep->client))
			ipa3_wlan_wq_rx_common(sys, &notify);
		else
			ipa3_wq_rx_common(sys, &notify);

		++cnt;
	}
	return cnt;
}

/**
 * __ipa3_update_curr_poll_state -> update current polling for default wan and
 *                                  coalescing pipe.
 * In RSC/RSB enabled cases using common event ring, so both the pipe
 * polling state should be in sync.
 */
void __ipa3_update_curr_poll_state(enum ipa_client_type client, int state)
{
	int ep_idx = IPA_EP_NOT_ALLOCATED;

	switch (client) {
		case IPA_CLIENT_APPS_WAN_COAL_CONS:
			ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);
			break;
		case IPA_CLIENT_APPS_WAN_CONS:
			ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_WAN_COAL_CONS);
			break;
		case IPA_CLIENT_APPS_LAN_COAL_CONS:
			ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS);
			break;
		case IPA_CLIENT_APPS_LAN_CONS:
			ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_LAN_COAL_CONS);
			break;
		default:
			break;
	}

	if (ep_idx != IPA_EP_NOT_ALLOCATED && ipa3_ctx->ep[ep_idx].sys)
		atomic_set(&ipa3_ctx->ep[ep_idx].sys->curr_polling_state,
									state);
}

static int ipa3_tx_switch_to_intr_mode(struct ipa3_sys_context *sys) {
	int ret;

	atomic_set(&sys->curr_polling_state, 0);
	__ipa3_update_curr_poll_state(sys->ep->client, 0);
	ret = gsi_config_channel_mode(sys->ep->gsi_chan_hdl,
				      GSI_CHAN_MODE_CALLBACK);
	if ((ret != GSI_STATUS_SUCCESS) &&
	    !atomic_read(&sys->curr_polling_state)) {
		if (ret == -GSI_STATUS_PENDING_IRQ) {
			atomic_set(&sys->curr_polling_state, 1);
			__ipa3_update_curr_poll_state(sys->ep->client, 1);
		} else {
			IPAERR("Failed to switch to intr mode %d ch_id %d\n",
				sys->curr_polling_state, sys->ep->gsi_chan_hdl);
		}
	}
	return ret;
}

/**
 * ipa3_rx_switch_to_intr_mode() - Operate the Rx data path in interrupt mode
 */
static int ipa3_rx_switch_to_intr_mode(struct ipa3_sys_context *sys)
{
	int ret;

	atomic_set(&sys->curr_polling_state, 0);
	__ipa3_update_curr_poll_state(sys->ep->client, 0);
	ipa_pm_deferred_deactivate(sys->pm_hdl);
	ipa3_dec_release_wakelock();
	ret = gsi_config_channel_mode(sys->ep->gsi_chan_hdl,
		GSI_CHAN_MODE_CALLBACK);
	if ((ret != GSI_STATUS_SUCCESS) &&
		!atomic_read(&sys->curr_polling_state)) {
		if (ret == -GSI_STATUS_PENDING_IRQ) {
			ipa3_inc_acquire_wakelock();
			atomic_set(&sys->curr_polling_state, 1);
			__ipa3_update_curr_poll_state(sys->ep->client, 1);
		} else {
			IPAERR("Failed to switch to intr mode %d ch_id %d\n",
			 sys->curr_polling_state, sys->ep->gsi_chan_hdl);
		}
	}

	return ret;
}

/**
 * ipa3_handle_rx() - handle packet reception. This function is executed in the
 * context of a work queue.
 * @work: work struct needed by the work queue
 *
 * ipa3_handle_rx_core() is run in polling mode. After all packets has been
 * received, the driver switches back to interrupt mode.
 */
static void ipa3_handle_rx(struct ipa3_sys_context *sys)
{
	enum ipa_client_type client_type;
	int inactive_cycles;
	int cnt;
	int ret;

start_poll:
	ipa_pm_activate_sync(sys->pm_hdl);
	inactive_cycles = 0;
	do {
		cnt = ipa3_handle_rx_core(sys, true, true);
		if (cnt == 0)
			inactive_cycles++;
		else
			inactive_cycles = 0;

		trace_idle_sleep_enter3(sys->ep->client);
		usleep_range(POLLING_MIN_SLEEP_RX, POLLING_MAX_SLEEP_RX);
		trace_idle_sleep_exit3(sys->ep->client);

		/*
		 * if pipe is out of buffers there is no point polling for
		 * completed descs; release the worker so delayed work can
		 * run in a timely manner
		 */
		if (sys->len == 0)
			break;

	} while (inactive_cycles <= POLLING_INACTIVITY_RX);

	trace_poll_to_intr3(sys->ep->client);
	ret = ipa3_rx_switch_to_intr_mode(sys);
	if (ret == -GSI_STATUS_PENDING_IRQ)
		goto start_poll;

	if (IPA_CLIENT_IS_WAN_CONS(sys->ep->client))
		client_type = IPA_CLIENT_APPS_WAN_COAL_CONS;
	else if (IPA_CLIENT_IS_LAN_CONS(sys->ep->client))
		client_type = IPA_CLIENT_APPS_LAN_COAL_CONS;
	else
		client_type = sys->ep->client;

	IPA_ACTIVE_CLIENTS_DEC_EP(client_type);
}

static void ipa3_switch_to_intr_rx_work_func(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct ipa3_sys_context *sys;

	dwork = container_of(work, struct delayed_work, work);
	sys = container_of(dwork, struct ipa3_sys_context, switch_to_intr_work);

	if (sys->napi_obj || IPA_CLIENT_IS_LOW_LAT_CONS(sys->ep->client)) {
		/* interrupt mode is done in ipa3_rx_poll context */
		ipa_assert();
	} else
		ipa3_handle_rx(sys);
}

enum hrtimer_restart ipa3_ring_doorbell_timer_fn(struct hrtimer *param)
{
	struct ipa3_sys_context *sys = container_of(param,
		struct ipa3_sys_context, db_timer);

	queue_work(sys->wq, &sys->work);
	return HRTIMER_NORESTART;
}

static void ipa_pm_sys_pipe_cb(void *p, enum ipa_pm_cb_event event)
{
	struct ipa3_sys_context *sys = (struct ipa3_sys_context *)p;

	switch (event) {
	case IPA_PM_CLIENT_ACTIVATED:
		/*
		 * this event is ignored as the sync version of activation
		 * will be used.
		 */
		break;
	case IPA_PM_REQUEST_WAKEUP:
		/*
		 * pipe will be unsuspended as part of
		 * enabling IPA clocks
		 */
		IPADBG("calling wakeup for client %d\n", sys->ep->client);
		if (sys->ep->client == IPA_CLIENT_APPS_WAN_CONS) {
			IPA_ACTIVE_CLIENTS_INC_SPECIAL("PIPE_SUSPEND_WAN");
			usleep_range(SUSPEND_MIN_SLEEP_RX,
				SUSPEND_MAX_SLEEP_RX);
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL("PIPE_SUSPEND_WAN");
		} else if (sys->ep->client == IPA_CLIENT_APPS_LAN_CONS) {
			IPA_ACTIVE_CLIENTS_INC_SPECIAL("PIPE_SUSPEND_LAN");
			usleep_range(SUSPEND_MIN_SLEEP_RX,
				SUSPEND_MAX_SLEEP_RX);
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL("PIPE_SUSPEND_LAN");
		} else if (sys->ep->client == IPA_CLIENT_ODL_DPL_CONS) {
			IPA_ACTIVE_CLIENTS_INC_SPECIAL("PIPE_SUSPEND_ODL");
			usleep_range(SUSPEND_MIN_SLEEP_RX,
				SUSPEND_MAX_SLEEP_RX);
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL("PIPE_SUSPEND_ODL");
		} else if (sys->ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS) {
			IPA_ACTIVE_CLIENTS_INC_SPECIAL("PIPE_SUSPEND_COAL");
			usleep_range(SUSPEND_MIN_SLEEP_RX,
				SUSPEND_MAX_SLEEP_RX);
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL("PIPE_SUSPEND_COAL");
		} else if (sys->ep->client == IPA_CLIENT_APPS_LAN_COAL_CONS) {
			IPA_ACTIVE_CLIENTS_INC_SPECIAL("PIPE_SUSPEND_LAN_COAL");
			usleep_range(SUSPEND_MIN_SLEEP_RX,
				SUSPEND_MAX_SLEEP_RX);
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL("PIPE_SUSPEND_LAN_COAL");
		} else if (sys->ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_CONS) {
			IPA_ACTIVE_CLIENTS_INC_SPECIAL("PIPE_SUSPEND_LOW_LAT");
			usleep_range(SUSPEND_MIN_SLEEP_RX,
				SUSPEND_MAX_SLEEP_RX);
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL("PIPE_SUSPEND_LOW_LAT");
		} else if (sys->ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS) {
			IPA_ACTIVE_CLIENTS_INC_SPECIAL("PIPE_SUSPEND_LOW_LAT_DATA");
			usleep_range(SUSPEND_MIN_SLEEP_RX,
				SUSPEND_MAX_SLEEP_RX);
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL("PIPE_SUSPEND_LOW_LAT_DATA");
		} else
			IPAERR("Unexpected event %d\n for client %d\n",
				event, sys->ep->client);
		break;
	default:
		IPAERR("Unexpected event %d\n for client %d\n",
			event, sys->ep->client);
		WARN_ON(1);
		return;
	}
}

int ipa3_setup_tput_pipe(void)
{
	struct ipa3_ep_context *ep;
	int ipa_ep_idx, result;
	struct ipa_sys_connect_params sys_in;

	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_TPUT_CONS;
	sys_in.desc_fifo_sz = IPA_SYS_TPUT_EP_DESC_FIFO_SZ;

	ipa_ep_idx = ipa_get_ep_mapping(sys_in.client);
	if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED) {
		IPAERR("Invalid client.\n");
		return -EFAULT;
	}
	ep = &ipa3_ctx->ep[ipa_ep_idx];
	if (ep->valid == 1) {
		IPAERR("EP %d already allocated.\n", ipa_ep_idx);
		return -EFAULT;
	}
	IPA_ACTIVE_CLIENTS_INC_EP(sys_in.client);
	memset(ep, 0, offsetof(struct ipa3_ep_context, sys));
	ep->valid = 1;
	ep->client = sys_in.client;

	result = ipa_gsi_setup_channel(&sys_in, ep);
	if (result) {
		IPAERR("Failed to setup GSI channel\n");
		goto fail_setup;
	}

	result = ipa3_enable_data_path(ipa_ep_idx);
	if (result) {
		IPAERR("enable data path failed res=%d ep=%d.\n", result,
			 ipa_ep_idx);
		goto fail_setup;
	}
	IPA_ACTIVE_CLIENTS_DEC_EP(sys_in.client);
	return 0;

fail_setup:
	memset(&ipa3_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa3_ep_context));
	IPA_ACTIVE_CLIENTS_DEC_EP(sys_in.client);
	return result;
}

static void ipa3_schd_freepage_work(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct ipa3_sys_context *sys;

	dwork = container_of(work, struct delayed_work, work);
	sys = container_of(dwork, struct ipa3_sys_context, freepage_work);

	IPADBG_LOW("WQ scheduled, reschedule sort tasklet\n");

	tasklet_schedule(&sys->tasklet_find_freepage);
}

static void ipa3_tasklet_find_freepage(unsigned long data)
{
	struct ipa3_sys_context *sys;
	struct ipa3_rx_pkt_wrapper *rx_pkt = NULL;
	struct ipa3_rx_pkt_wrapper *tmp = NULL;
	struct page *cur_page;
	int found_free_page = 0;
	struct list_head temp_head;

	sys = (struct ipa3_sys_context *)data;

	if(sys->page_recycle_repl == NULL)
		return;
	INIT_LIST_HEAD(&temp_head);
	spin_lock_bh(&sys->common_sys->spinlock);
	list_for_each_entry_safe(rx_pkt, tmp,
		&sys->page_recycle_repl->page_repl_head, link) {
		cur_page = rx_pkt->page_data.page;
		if (page_ref_count(cur_page) == 1) {
			/* Found a free page. */
			list_del_init(&rx_pkt->link);
			list_add(&rx_pkt->link, &temp_head);
			found_free_page++;
		}
	}
	if (!found_free_page) {
		/*Not found free page rescheduling tasklet after 2msec*/
		IPADBG_LOW("Scheduling WQ not found free pages\n");
		++ipa3_ctx->stats.num_of_times_wq_reschd;
		queue_delayed_work(sys->freepage_wq,
				&sys->freepage_work,
				msecs_to_jiffies(ipa3_ctx->page_wq_reschd_time));
	} else {
		/*Allow to use pre-allocated buffers*/
		list_splice(&temp_head, &sys->page_recycle_repl->page_repl_head);
		ipa3_ctx->stats.page_recycle_cnt_in_tasklet += found_free_page;
		IPADBG_LOW("found free pages count = %d\n", found_free_page);
		ipa3_ctx->free_page_task_scheduled = false;
		atomic_set(&sys->common_sys->page_avilable, 1);
	}
	spin_unlock_bh(&sys->common_sys->spinlock);

}

/**
 * ipa_setup_sys_pipe() - Setup an IPA GPI pipe and perform
 * IPA EP configuration
 * @sys_in:	[in] input needed to setup the pipe and configure EP
 * @clnt_hdl:	[out] client handle
 *
 *  - configure the end-point registers with the supplied
 *    parameters from the user.
 *  - Creates a GPI connection with IPA.
 *  - allocate descriptor FIFO
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_setup_sys_pipe(struct ipa_sys_connect_params *sys_in, u32 *clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int i, ipa_ep_idx;
	int wan_handle, lan_handle;
	int wan_coal_ep_id, lan_coal_ep_id;
	int result = -EINVAL;
	struct ipahal_reg_coal_qmap_cfg qmap_cfg;
	char buff[IPA_RESOURCE_NAME_MAX];
	struct ipa_ep_cfg ep_cfg_copy;
	int (*tx_completion_func)(struct napi_struct *, int);

	if (sys_in == NULL || clnt_hdl == NULL) {
		IPAERR(
			"NULL args: sys_in(%p) and/or clnt_hdl(%u)\n",
			sys_in, clnt_hdl);
		goto fail_gen;
	}

	if (sys_in->client >= IPA_CLIENT_MAX || sys_in->desc_fifo_sz == 0) {
		IPAERR("bad parm client:%d fifo_sz:%d\n",
			sys_in->client, sys_in->desc_fifo_sz);
		goto fail_gen;
	}

	if ( ! IPA_CLIENT_IS_MAPPED(sys_in->client, ipa_ep_idx) ) {
		IPAERR("Invalid client.\n");
		goto fail_gen;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];
	if (ep->valid == 1) {
		IPAERR("EP %d already allocated.\n", ipa_ep_idx);
		goto fail_gen;
	}

	*clnt_hdl = 0;
	wan_coal_ep_id = ipa_get_ep_mapping(IPA_CLIENT_APPS_WAN_COAL_CONS);
	lan_coal_ep_id = ipa_get_ep_mapping(IPA_CLIENT_APPS_LAN_COAL_CONS);

	/* save the input config parameters */
	if (IPA_CLIENT_IS_APPS_COAL_CONS(sys_in->client))
		ep_cfg_copy = sys_in->ipa_ep_cfg;

	IPA_ACTIVE_CLIENTS_INC_EP(sys_in->client);
	memset(ep, 0, offsetof(struct ipa3_ep_context, sys));

	if (!ep->sys) {
		struct ipa_pm_register_params pm_reg;

		memset(&pm_reg, 0, sizeof(pm_reg));
		ep->sys = kzalloc(sizeof(struct ipa3_sys_context), GFP_KERNEL);
		if (!ep->sys) {
			IPAERR("failed to sys ctx for client %d\n",
					sys_in->client);
			result = -ENOMEM;
			goto fail_and_disable_clocks;
		}

		ep->sys->ep = ep;
		snprintf(buff, IPA_RESOURCE_NAME_MAX, "ipawq%d",
				sys_in->client);
		ep->sys->wq = alloc_workqueue(buff,
				WQ_MEM_RECLAIM | WQ_UNBOUND | WQ_SYSFS, 1);

		if (!ep->sys->wq) {
			IPAERR("failed to create wq for client %d\n",
					sys_in->client);
			result = -EFAULT;
			goto fail_wq;
		}

		snprintf(buff, IPA_RESOURCE_NAME_MAX, "iparepwq%d",
				sys_in->client);
		ep->sys->repl_wq = alloc_workqueue(buff,
				WQ_MEM_RECLAIM | WQ_UNBOUND | WQ_SYSFS | WQ_HIGHPRI,
				1);
		if (!ep->sys->repl_wq) {
			IPAERR("failed to create rep wq for client %d\n",
					sys_in->client);
			result = -EFAULT;
			goto fail_wq2;
		}

		snprintf(buff, IPA_RESOURCE_NAME_MAX, "ipafreepagewq%d",
				sys_in->client);

		INIT_LIST_HEAD(&ep->sys->head_desc_list);
		INIT_LIST_HEAD(&ep->sys->rcycl_list);
		INIT_LIST_HEAD(&ep->sys->avail_tx_wrapper_list);
		ep->sys->avail_tx_wrapper = 0;
		spin_lock_init(&ep->sys->spinlock);
		hrtimer_init(&ep->sys->db_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
		ep->sys->db_timer.function = ipa3_ring_doorbell_timer_fn;

		/* create IPA PM resources for handling polling mode */
		if (sys_in->client == IPA_CLIENT_APPS_WAN_CONS &&
			wan_coal_ep_id != IPA_EP_NOT_ALLOCATED &&
			ipa3_ctx->ep[wan_coal_ep_id].valid == 1) {
			/* Use coalescing pipe PM handle for default pipe also*/
			ep->sys->pm_hdl = ipa3_ctx->ep[wan_coal_ep_id].sys->pm_hdl;
		} else if (sys_in->client == IPA_CLIENT_APPS_LAN_CONS &&
			lan_coal_ep_id != IPA_EP_NOT_ALLOCATED &&
			ipa3_ctx->ep[lan_coal_ep_id].valid == 1) {
			/* Use coalescing pipe PM handle for default pipe also*/
			ep->sys->pm_hdl = ipa3_ctx->ep[lan_coal_ep_id].sys->pm_hdl;
		} else if (IPA_CLIENT_IS_CONS(sys_in->client)) {
			ep->sys->freepage_wq = alloc_workqueue(buff,
					WQ_MEM_RECLAIM | WQ_UNBOUND | WQ_SYSFS |
					WQ_HIGHPRI, 1);
			if (!ep->sys->freepage_wq) {
				IPAERR("failed to create freepage wq for client %d\n",
						sys_in->client);
				result = -EFAULT;
				goto fail_wq3;
			}

			pm_reg.name = ipa_clients_strings[sys_in->client];
			pm_reg.callback = ipa_pm_sys_pipe_cb;
			pm_reg.user_data = ep->sys;
			pm_reg.group = IPA_PM_GROUP_APPS;
			result = ipa_pm_register(&pm_reg, &ep->sys->pm_hdl);
			if (result) {
				IPAERR("failed to create IPA PM client %d\n",
					result);
				goto fail_pm;
			}

			if (IPA_CLIENT_IS_APPS_CONS(sys_in->client)) {
				result = ipa_pm_associate_ipa_cons_to_client(
					ep->sys->pm_hdl, sys_in->client);
				if (result) {
					IPAERR("failed to associate\n");
					goto fail_gen2;
				}
			}

			result = ipa_pm_set_throughput(ep->sys->pm_hdl,
				IPA_APPS_BW_FOR_PM);
			if (result) {
				IPAERR("failed to set profile IPA PM client\n");
				goto fail_gen2;
			}
		}
	} else {
		memset(ep->sys, 0, offsetof(struct ipa3_sys_context, ep));
	}

	if(ipa3_ctx->tx_poll)
		tx_completion_func = &ipa3_aux_napi_poll_tx_complete;
	else
		tx_completion_func = &ipa3_aux_napi_tx_complete;

	atomic_set(&ep->sys->xmit_eot_cnt, 0);
	if (IPA_CLIENT_IS_PROD(sys_in->client))
		tasklet_init(&ep->sys->tasklet, ipa3_tasklet_write_done,
				(unsigned long) ep->sys);
	if (sys_in->client == IPA_CLIENT_APPS_WAN_LOW_LAT_CONS)
		tasklet_init(&ep->sys->tasklet, ipa3_tasklet_rx_notify,
				(unsigned long) ep->sys);

	if (IPA_CLIENT_IS_PROD(sys_in->client) &&
		ipa3_ctx->tx_napi_enable) {
		if (sys_in->client == IPA_CLIENT_APPS_LAN_PROD) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(6, 0, 14))
			netif_napi_add_tx_weight(&ipa3_ctx->generic_ndev,
			&ep->sys->napi_tx, tx_completion_func,
			NAPI_TX_WEIGHT);
#else
			netif_tx_napi_add(&ipa3_ctx->generic_ndev,
			&ep->sys->napi_tx, tx_completion_func,
			NAPI_TX_WEIGHT);

#endif
			ep->sys->napi_tx_enable = ipa3_ctx->tx_napi_enable;
			ep->sys->tx_poll = ipa3_ctx->tx_poll;
		} else if(sys_in->client == IPA_CLIENT_APPS_WAN_PROD ||
			sys_in->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_PROD) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(6, 0, 14))
			netif_napi_add_tx_weight((struct net_device *)sys_in->priv,
			&ep->sys->napi_tx, tx_completion_func,
			NAPI_TX_WEIGHT);
#else
			netif_tx_napi_add((struct net_device *)sys_in->priv,
			&ep->sys->napi_tx, tx_completion_func,
			NAPI_TX_WEIGHT);
#endif
			ep->sys->napi_tx_enable = ipa3_ctx->tx_napi_enable;
			ep->sys->tx_poll = ipa3_ctx->tx_poll;
		} else {
			/*CMD pipe*/
			ep->sys->tx_poll = false;
			ep->sys->napi_tx_enable = false;
		}
		if(ep->sys->napi_tx_enable) {
			napi_enable(&ep->sys->napi_tx);
			IPADBG("napi_enable on producer client %d completed",
				sys_in->client);
		}
	}

	if (sys_in->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(6, 0, 14))
		netif_napi_add((struct net_device *)sys_in->priv,
			&ep->sys->napi_rx, ipa3_rmnet_ll_rx_poll);
#else
		netif_napi_add((struct net_device *)sys_in->priv,
			&ep->sys->napi_rx, ipa3_rmnet_ll_rx_poll, NAPI_WEIGHT);
#endif
		napi_enable(&ep->sys->napi_rx);
	}

	ep->client = sys_in->client;
	ep->sys->ext_ioctl_v2 = sys_in->ext_ioctl_v2;
	ep->sys->int_modt = sys_in->int_modt;
	ep->sys->int_modc = sys_in->int_modc;
	ep->sys->buff_size = sys_in->buff_size;
	ep->sys->page_order = (sys_in->ext_ioctl_v2) ?
			get_order(sys_in->buff_size) : IPA_WAN_PAGE_ORDER;
	ep->skip_ep_cfg = sys_in->skip_ep_cfg;
	if (ipa3_assign_policy(sys_in, ep->sys)) {
		IPAERR("failed to sys ctx for client %d\n", sys_in->client);
		result = -ENOMEM;
		goto fail_napi;
	}

	ep->valid = 1;
	ep->client_notify = sys_in->notify;
	ep->sys->napi_obj = sys_in->napi_obj;
	ep->priv = sys_in->priv;
	ep->keep_ipa_awake = sys_in->keep_ipa_awake;
	atomic_set(&ep->avail_fifo_desc,
		((sys_in->desc_fifo_sz / IPA_FIFO_ELEMENT_SIZE) - 1));

	if (ep->status.status_en && IPA_CLIENT_IS_CONS(ep->client) &&
	    ep->sys->status_stat == NULL) {
		ep->sys->status_stat =
			kzalloc(sizeof(struct ipa3_status_stats), GFP_KERNEL);
		if (!ep->sys->status_stat)
			goto fail_napi;
	}

	if (!ep->skip_ep_cfg) {
		if (ipa3_cfg_ep(ipa_ep_idx, &sys_in->ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto fail_napi;
		}
		if (ipa3_cfg_ep_status(ipa_ep_idx, &ep->status)) {
			IPAERR("fail to configure status of EP.\n");
			goto fail_napi;
		}
		IPADBG("ep %d configuration successful\n", ipa_ep_idx);
	} else {
		IPADBG("skipping ep %d configuration\n", ipa_ep_idx);
	}

	result = ipa_gsi_setup_channel(sys_in, ep);
	if (result) {
		IPAERR("Failed to setup GSI channel\n");
		goto fail_napi;
	}

	*clnt_hdl = ipa_ep_idx;
	ep->sys->common_sys = ipa3_ctx->ep[ipa_ep_idx].sys;

	if (ep->sys->repl_hdlr == ipa3_fast_replenish_rx_cache) {
		ep->sys->repl = kzalloc(sizeof(*ep->sys->repl), GFP_KERNEL);
		if (!ep->sys->repl) {
			IPAERR("failed to alloc repl for client %d\n",
					sys_in->client);
			result = -ENOMEM;
			goto fail_napi;
		}
		atomic_set(&ep->sys->repl->pending, 0);
		ep->sys->repl->capacity = ep->sys->rx_pool_sz + 1;

		ep->sys->repl->cache = kcalloc(ep->sys->repl->capacity,
				sizeof(void *), GFP_KERNEL);
		if (!ep->sys->repl->cache) {
			IPAERR("ep=%d fail to alloc repl cache\n", ipa_ep_idx);
			ep->sys->repl_hdlr = ipa3_replenish_rx_cache;
			ep->sys->repl->capacity = 0;
		} else {
			atomic_set(&ep->sys->repl->head_idx, 0);
			atomic_set(&ep->sys->repl->tail_idx, 0);
			ipa3_wq_repl_rx(&ep->sys->repl_work);
		}
	}

	/* Use common page pool for Coal and defalt pipe if applicable. */
	if (ep->sys->repl_hdlr == ipa3_replenish_rx_page_recycle) {
		if (!(ipa3_ctx->wan_common_page_pool &&
			sys_in->client == IPA_CLIENT_APPS_WAN_CONS &&
			wan_coal_ep_id != IPA_EP_NOT_ALLOCATED &&
			ipa3_ctx->ep[wan_coal_ep_id].valid == 1)) {
			/* Allocate page recycling pool only once. */
			if (!ep->sys->page_recycle_repl) {
				ep->sys->page_recycle_repl = kzalloc(
					sizeof(*ep->sys->page_recycle_repl), GFP_KERNEL);
				if (!ep->sys->page_recycle_repl) {
					IPAERR("failed to alloc repl for client %d\n",
							sys_in->client);
					result = -ENOMEM;
					goto fail_napi;
				}
				atomic_set(&ep->sys->page_recycle_repl->pending, 0);
				/* For common page pool double the pool size. */
				if (ipa3_ctx->wan_common_page_pool &&
					sys_in->client == IPA_CLIENT_APPS_WAN_COAL_CONS)
					ep->sys->page_recycle_repl->capacity =
							(ep->sys->rx_pool_sz + 1) *
							ipa3_ctx->ipa_gen_rx_cmn_page_pool_sz_factor;
				else if (sys_in->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS)
					ep->sys->page_recycle_repl->capacity =
						(ep->sys->rx_pool_sz + 1) *
						ipa3_ctx->ipa_gen_rx_ll_pool_sz_factor;
				else
					ep->sys->page_recycle_repl->capacity =
							(ep->sys->rx_pool_sz + 1) *
							IPA_GENERIC_RX_PAGE_POOL_SZ_FACTOR;
				IPADBG("Page repl capacity for client:%d, value:%d\n",
						   sys_in->client, ep->sys->page_recycle_repl->capacity);
				INIT_LIST_HEAD(&ep->sys->page_recycle_repl->page_repl_head);
				INIT_DELAYED_WORK(&ep->sys->freepage_work, ipa3_schd_freepage_work);
				tasklet_init(&ep->sys->tasklet_find_freepage,
					ipa3_tasklet_find_freepage, (unsigned long) ep->sys);
				ipa3_replenish_rx_page_cache(ep->sys);
			} else {
 				ep->sys->napi_sort_page_thrshld_cnt = 0;
				/* Sort the pages once. */
				ipa3_tasklet_find_freepage((unsigned long) ep->sys);
			}

			ep->sys->repl = kzalloc(sizeof(*ep->sys->repl), GFP_KERNEL);
			if (!ep->sys->repl) {
				IPAERR("failed to alloc repl for client %d\n",
					   sys_in->client);
				result = -ENOMEM;
				goto fail_page_recycle_repl;
			}
			/* For common page pool triple the pool size. */
			if (ipa3_ctx->wan_common_page_pool &&
				sys_in->client == IPA_CLIENT_APPS_WAN_COAL_CONS)
				ep->sys->repl->capacity = (ep->sys->rx_pool_sz + 1) *
				ipa3_ctx->ipa_gen_rx_cmn_temp_pool_sz_factor;
			else
				ep->sys->repl->capacity = (ep->sys->rx_pool_sz + 1);
			IPADBG("Repl capacity for client:%d, value:%d\n",
					   sys_in->client, ep->sys->repl->capacity);
			atomic_set(&ep->sys->repl->pending, 0);
			ep->sys->repl->cache = kcalloc(ep->sys->repl->capacity,
					sizeof(void *), GFP_KERNEL);
			atomic_set(&ep->sys->repl->head_idx, 0);
			atomic_set(&ep->sys->repl->tail_idx, 0);

			ipa3_wq_page_repl(&ep->sys->repl_work);
		} else {
			/* Use pool same as coal pipe when common page pool is used. */
			ep->sys->common_buff_pool = true;
			ep->sys->common_sys = ipa3_ctx->ep[wan_coal_ep_id].sys;
			ep->sys->repl = ipa3_ctx->ep[wan_coal_ep_id].sys->repl;
			ep->sys->page_recycle_repl =
				ipa3_ctx->ep[wan_coal_ep_id].sys->page_recycle_repl;
		}
	}

	if (IPA_CLIENT_IS_CONS(sys_in->client)) {
		if ((IPA_CLIENT_IS_WAN_CONS(sys_in->client) ||
			sys_in->client ==
			IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS) &&
			ipa3_ctx->ipa_wan_skb_page) {
			ipa3_replenish_rx_page_recycle(ep->sys);
		} else
			ipa3_first_replenish_rx_cache(ep->sys);
		for (i = 0; i < GSI_VEID_MAX; i++)
			INIT_LIST_HEAD(&ep->sys->pending_pkts[i]);
	}

	if (IPA_CLIENT_IS_WLAN_CONS(sys_in->client)) {
		ipa3_alloc_wlan_rx_common_cache(IPA_WLAN_COMM_RX_POOL_LOW);
		atomic_inc(&ipa3_ctx->wc_memb.active_clnt_cnt);
	}

	ipa3_ctx->skip_ep_cfg_shadow[ipa_ep_idx] = ep->skip_ep_cfg;
	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(sys_in->client)) {
		if (ipa3_ctx->modem_cfg_emb_pipe_flt &&
			(sys_in->client == IPA_CLIENT_APPS_WAN_PROD ||
				sys_in->client ==
				IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_PROD))
			IPADBG("modem cfg emb pipe flt\n");
		else
			ipa3_install_dflt_flt_rules(ipa_ep_idx);
	}

	result = ipa3_enable_data_path(ipa_ep_idx);
	if (result) {
		IPAERR("enable data path failed res=%d ep=%d.\n", result,
			ipa_ep_idx);
		goto fail_repl;
	}

	result = gsi_start_channel(ep->gsi_chan_hdl);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("gsi_start_channel failed res=%d ep=%d.\n", result,
			ipa_ep_idx);
		goto fail_gen3;
	}

	IPADBG("client %d (ep: %d) connected sys=%pK\n", sys_in->client,
			ipa_ep_idx, ep->sys);

	/*
	 * Configure the registers and setup the default pipe
	 */
	if (IPA_CLIENT_IS_APPS_COAL_CONS(sys_in->client)) {

		const char* str = "";

		if (sys_in->client == IPA_CLIENT_APPS_WAN_COAL_CONS) {

			str = "wan";

			qmap_cfg.mux_id_byte_sel = IPA_QMAP_ID_BYTE;

			ipahal_write_reg_fields(IPA_COAL_QMAP_CFG, &qmap_cfg);

			if (!sys_in->ext_ioctl_v2) {
				sys_in->client = IPA_CLIENT_APPS_WAN_CONS;
				sys_in->ipa_ep_cfg = ep_cfg_copy;
				result = ipa_setup_sys_pipe(sys_in, &wan_handle);
			}

		} else { /* (sys_in->client == IPA_CLIENT_APPS_LAN_COAL_CONS) */

			str = "lan";

			if (!sys_in->ext_ioctl_v2) {
				sys_in->client = IPA_CLIENT_APPS_LAN_CONS;
				sys_in->ipa_ep_cfg = ep_cfg_copy;
				sys_in->notify = ipa3_lan_rx_cb;
				result = ipa_setup_sys_pipe(sys_in, &lan_handle);
			}
		}

		if (result) {
			IPAERR(
				"Failed to setup default %s coalescing pipe\n",
				str);
			goto fail_repl;
		}

		ipa3_default_evict_register();
	}

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ep->client);

	return 0;

fail_gen3:
	ipa3_disable_data_path(ipa_ep_idx);
fail_repl:
	if (IPA_CLIENT_IS_CONS(ep->client) && !ep->sys->common_buff_pool)
		ipa3_cleanup_rx(ep->sys);

	ep->sys->repl_hdlr = ipa3_replenish_rx_cache;
	if (ep->sys->repl && !ep->sys->common_buff_pool) {
		kfree(ep->sys->repl);
		ep->sys->repl = NULL;
	}
fail_page_recycle_repl:
	if (ep->sys->page_recycle_repl && !ep->sys->common_buff_pool) {
		kfree(ep->sys->page_recycle_repl);
		ep->sys->page_recycle_repl = NULL;
	}
fail_napi:
	if (sys_in->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS) {
		napi_disable(&ep->sys->napi_rx);
		netif_napi_del(&ep->sys->napi_rx);
	}
	/* Delete NAPI TX object. */
	if (ipa3_ctx->tx_napi_enable &&
		(IPA_CLIENT_IS_PROD(sys_in->client)))
		netif_napi_del(&ep->sys->napi_tx);
fail_gen2:
	ipa_pm_deregister(ep->sys->pm_hdl);
fail_pm:
	if (ep->sys->freepage_wq)
		destroy_workqueue(ep->sys->freepage_wq);
fail_wq3:
	destroy_workqueue(ep->sys->repl_wq);
fail_wq2:
	destroy_workqueue(ep->sys->wq);
fail_wq:
	kfree(ep->sys);
	memset(&ipa3_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa3_ep_context));
fail_and_disable_clocks:
	IPA_ACTIVE_CLIENTS_DEC_EP(sys_in->client);
	*clnt_hdl = -1;
fail_gen:
	IPA_STATS_INC_CNT(ipa3_ctx->stats.pipe_setup_fail_cnt);
	return result;
}
EXPORT_SYMBOL(ipa_setup_sys_pipe);

static void delete_avail_tx_wrapper_list(struct ipa3_ep_context *ep)
{
	struct ipa3_tx_pkt_wrapper *tx_pkt_iterator = NULL;
	struct ipa3_tx_pkt_wrapper *tx_pkt_temp = NULL;

	spin_lock_bh(&ep->sys->spinlock);
	list_for_each_entry_safe(tx_pkt_iterator, tx_pkt_temp,
	    &ep->sys->avail_tx_wrapper_list, link) {
		list_del(&tx_pkt_iterator->link);
		kmem_cache_free(ipa3_ctx->tx_pkt_wrapper_cache, tx_pkt_iterator);
		ep->sys->avail_tx_wrapper--;
	}
	ep->sys->avail_tx_wrapper = 0;
	spin_unlock_bh(&ep->sys->spinlock);
}

/**
 * ipa_teardown_sys_pipe() - Teardown the GPI pipe and cleanup IPA EP
 * @clnt_hdl:	[in] the handle obtained from ipa_setup_sys_pipe
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_teardown_sys_pipe(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int empty;
	int result;
	int i;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_disable_data_path(clnt_hdl);

	if (IPA_CLIENT_IS_PROD(ep->client)) {
		do {
			spin_lock_bh(&ep->sys->spinlock);
			atomic_set(&ep->disconnect_in_progress, 1);
			empty = list_empty(&ep->sys->head_desc_list);
			spin_unlock_bh(&ep->sys->spinlock);
			if (!empty)
				usleep_range(95, 105);
			else
				break;
		} while (1);

		delete_avail_tx_wrapper_list(ep);
		/* Delete NAPI TX object. For WAN_PROD, it is deleted
		 * in rmnet_ipa driver.
		 */
		if (ipa3_ctx->tx_napi_enable &&
			(ep->client != IPA_CLIENT_APPS_WAN_PROD))
			netif_napi_del(&ep->sys->napi_tx);
	}

	if(ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS) {
		napi_disable(&ep->sys->napi_rx);
		netif_napi_del(&ep->sys->napi_rx);
	}

	if ( ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS ) {
		stop_coalescing();
		ipa3_force_close_coal(false, true);
	}

	/* channel stop might fail on timeout if IPA is busy */
	for (i = 0; i < IPA_GSI_CHANNEL_STOP_MAX_RETRY; i++) {
		result = ipa_stop_gsi_channel(clnt_hdl);
		if (result == GSI_STATUS_SUCCESS)
			break;

		if (result != -GSI_STATUS_AGAIN &&
			result != -GSI_STATUS_TIMED_OUT)
			break;
	}

	if ( ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS ) {
		start_coalescing();
	}

	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("GSI stop chan err: %d.\n", result);
		ipa_assert();
		return result;
	}

	/* Wait untill end point moving to interrupt mode before teardown */
	do {
		usleep_range(95, 105);
	} while (atomic_read(&ep->sys->curr_polling_state));

	if (IPA_CLIENT_IS_CONS(ep->client))
		cancel_delayed_work_sync(&ep->sys->replenish_rx_work);
	flush_workqueue(ep->sys->wq);
	if (IPA_CLIENT_IS_PROD(ep->client))
		atomic_set(&ep->sys->workqueue_flushed, 1);

	/*
	 * Tear down the default pipe before we reset the channel
	 */
	if (ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS) {

		if ( ! IPA_CLIENT_IS_MAPPED(IPA_CLIENT_APPS_WAN_CONS, i) ) {
			IPAERR("Failed to get idx for IPA_CLIENT_APPS_WAN_CONS");
			if (!ep->keep_ipa_awake)
				IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
			return i;
		}

		/* If the default channel is already torn down,
		 * resetting only coalescing channel.
		 */
		if (ipa3_ctx->ep[i].valid) {
			result = ipa3_teardown_pipe(i);
			if (result) {
				IPAERR("failed to teardown default coal pipe\n");
				if (!ep->keep_ipa_awake) {
					IPA_ACTIVE_CLIENTS_DEC_EP(
						ipa3_get_client_mapping(clnt_hdl));
				}
				return result;
			}
		} else {
			napi_disable(ep->sys->napi_obj);
			netif_napi_del(ep->sys->napi_obj);
		}
	}

	/*
	 * Tear down the default pipe before we reset the channel
	 */
	if (ep->client == IPA_CLIENT_APPS_LAN_COAL_CONS) {

		if ( ! IPA_CLIENT_IS_MAPPED(IPA_CLIENT_APPS_LAN_CONS, i) ) {
			IPAERR("Failed to get idx for IPA_CLIENT_APPS_LAN_CONS,");
			if (!ep->keep_ipa_awake)
				IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
			return i;
		}

		/* If the default channel is already torn down,
		 * resetting only coalescing channel.
		 */
		if (ipa3_ctx->ep[i].valid) {
			result = ipa3_teardown_pipe(i);
			if (result) {
				IPAERR("failed to teardown default coal pipe\n");
				if (!ep->keep_ipa_awake) {
					IPA_ACTIVE_CLIENTS_DEC_EP(
						ipa3_get_client_mapping(clnt_hdl));
				}
				return result;
			}
		}
	}

	result = ipa3_reset_gsi_channel(clnt_hdl);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("Failed to reset chan: %d.\n", result);
		ipa_assert();
		return result;
	}
	dma_free_coherent(ipa3_ctx->pdev,
		ep->gsi_mem_info.chan_ring_len,
		ep->gsi_mem_info.chan_ring_base_vaddr,
		ep->gsi_mem_info.chan_ring_base_addr);
	result = gsi_dealloc_channel(ep->gsi_chan_hdl);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("Failed to dealloc chan: %d.\n", result);
		ipa_assert();
		return result;
	}

	/* free event ring only when it is present */
	if (ep->sys->use_comm_evt_ring) {
		ipa3_ctx->gsi_evt_comm_ring_rem +=
			ep->gsi_mem_info.chan_ring_len;
	} else if (ep->gsi_evt_ring_hdl != ~0) {
		result = gsi_reset_evt_ring(ep->gsi_evt_ring_hdl);
		if (WARN(result != GSI_STATUS_SUCCESS, "reset evt %d", result)) {
			ipa_assert();
			return result;
		}

		dma_free_coherent(ipa3_ctx->pdev,
			ep->gsi_mem_info.evt_ring_len,
			ep->gsi_mem_info.evt_ring_base_vaddr,
			ep->gsi_mem_info.evt_ring_base_addr);

		if (ep->gsi_mem_info.evt_ring_rp_vaddr) {
			dma_free_coherent(ipa3_ctx->pdev,
				IPA_GSI_EVENT_RP_SIZE,
				ep->gsi_mem_info.evt_ring_rp_vaddr,
				ep->gsi_mem_info.evt_ring_rp_addr);
				ep->gsi_mem_info.evt_ring_rp_addr = 0;
				ep->gsi_mem_info.evt_ring_rp_vaddr = 0;
		}

		result = gsi_dealloc_evt_ring(ep->gsi_evt_ring_hdl);
		if (WARN(result != GSI_STATUS_SUCCESS, "deall evt %d", result)) {
			ipa_assert();
			return result;
		}
	}
	if (ep->sys->repl_wq)
		flush_workqueue(ep->sys->repl_wq);

	if (ep->sys->repl_hdlr == ipa3_replenish_rx_page_recycle) {
		cancel_delayed_work_sync(&ep->sys->common_sys->freepage_work);

		if (ep->sys->freepage_wq)
			flush_workqueue(ep->sys->freepage_wq);

		tasklet_kill(&ep->sys->common_sys->tasklet_find_freepage);
	}

	if (IPA_CLIENT_IS_CONS(ep->client) && !ep->sys->common_buff_pool)
		ipa3_cleanup_rx(ep->sys);

	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(ep->client)) {
		if (ipa3_ctx->modem_cfg_emb_pipe_flt &&
			(ep->client == IPA_CLIENT_APPS_WAN_PROD ||
				ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_PROD))
			IPADBG("modem cfg emb pipe flt\n");
		else
			ipa3_delete_dflt_flt_rules(clnt_hdl);
	}

	if (IPA_CLIENT_IS_WLAN_CONS(ep->client))
		atomic_dec(&ipa3_ctx->wc_memb.active_clnt_cnt);

	memset(&ep->wstats, 0, sizeof(struct ipa3_wlan_stats));

	if (!atomic_read(&ipa3_ctx->wc_memb.active_clnt_cnt))
		ipa3_cleanup_wlan_rx_common_cache();

	ep->valid = 0;

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPADBG("client (ep: %d) disconnected\n", clnt_hdl);

	return 0;
}
EXPORT_SYMBOL(ipa_teardown_sys_pipe);

/**
 * ipa3_teardown_pipe()
 *
 *   Teardown and cleanup of the physical connection (i.e. data
 *   structures, buffers, GSI channel, work queues, etc) associated
 *   with the passed client handle and the endpoint context that the
 *   handle represents.
 *
 * @clnt_hdl:  [in] A handle obtained from ipa_setup_sys_pipe
 *
 * Returns:	0 on success, negative on failure
 */
static int ipa3_teardown_pipe(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int result;
	int i;

	ep = &ipa3_ctx->ep[clnt_hdl];

	ipa3_disable_data_path(clnt_hdl);

	/* channel stop might fail on timeout if IPA is busy */
	for (i = 0; i < IPA_GSI_CHANNEL_STOP_MAX_RETRY; i++) {
		result = ipa_stop_gsi_channel(clnt_hdl);
		if (result == GSI_STATUS_SUCCESS)
			break;

		if (result != -GSI_STATUS_AGAIN &&
		    result != -GSI_STATUS_TIMED_OUT)
			break;
	}

	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("GSI stop chan err: %d.\n", result);
		ipa_assert();
		return result;
	}

	if (IPA_CLIENT_IS_WAN_CONS(ep->client)) {
		/* Wait for any pending irqs */
		usleep_range(POLLING_MIN_SLEEP_RX, POLLING_MAX_SLEEP_RX);
		/* Wait until end point moving to interrupt mode before teardown */
		do {
			usleep_range(95, 105);
		} while (atomic_read(&ep->sys->curr_polling_state));

		napi_disable(ep->sys->napi_obj);
		netif_napi_del(ep->sys->napi_obj);
	}

	result = ipa3_reset_gsi_channel(clnt_hdl);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("Failed to reset chan: %d.\n", result);
		ipa_assert();
		return result;
	}
	dma_free_coherent(ipa3_ctx->pdev,
		ep->gsi_mem_info.chan_ring_len,
		ep->gsi_mem_info.chan_ring_base_vaddr,
		ep->gsi_mem_info.chan_ring_base_addr);
	result = gsi_dealloc_channel(ep->gsi_chan_hdl);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("Failed to dealloc chan: %d.\n", result);
		ipa_assert();
		return result;
	}

	if (IPA_CLIENT_IS_CONS(ep->client))
		cancel_delayed_work_sync(&ep->sys->replenish_rx_work);

	flush_workqueue(ep->sys->wq);

	if (ep->sys->repl_wq)
		flush_workqueue(ep->sys->repl_wq);
	if (IPA_CLIENT_IS_CONS(ep->client) && !ep->sys->common_buff_pool)
		ipa3_cleanup_rx(ep->sys);

	ep->valid = 0;

	IPADBG("client (ep: %d) disconnected\n", clnt_hdl);

	return 0;
}

/**
 * ipa3_tx_comp_usr_notify_release() - Callback function which will call the
 * user supplied callback function to release the skb, or release it on
 * its own if no callback function was supplied.
 * @user1
 * @user2
 *
 * This notified callback is for the destination client.
 */
static void ipa3_tx_comp_usr_notify_release(void *user1, int user2)
{
	struct sk_buff *skb = (struct sk_buff *)user1;
	int ep_idx = user2;

	IPADBG_LOW("skb=%pK ep=%d\n", skb, ep_idx);

	IPA_STATS_INC_CNT(ipa3_ctx->stats.tx_pkts_compl);

	if (ipa3_ctx->ep[ep_idx].client_notify)
		ipa3_ctx->ep[ep_idx].client_notify(ipa3_ctx->ep[ep_idx].priv,
				IPA_WRITE_DONE, (unsigned long)skb);
	else
		dev_kfree_skb_any(skb);
}

void ipa3_tx_cmd_comp(void *user1, int user2)
{
	ipahal_destroy_imm_cmd(user1);
}

/**
 * ipa_tx_dp() - Data-path tx handler
 * @dst:	[in] which IPA destination to route tx packets to
 * @skb:	[in] the packet to send
 * @metadata:	[in] TX packet meta-data
 *
 * Data-path tx handler, this is used for both SW data-path which by-passes most
 * IPA HW blocks AND the regular HW data-path for WLAN AMPDU traffic only. If
 * dst is a "valid" CONS type, then SW data-path is used. If dst is the
 * WLAN_AMPDU PROD type, then HW data-path for WLAN AMPDU is used. Anything else
 * is an error. For errors, client needs to free the skb as needed. For success,
 * IPA driver will later invoke client callback if one was supplied. That
 * callback should free the skb. If no callback supplied, IPA driver will free
 * the skb internally
 *
 * The function will use two descriptors for this send command
 * (for A5_WLAN_AMPDU_PROD only one desciprtor will be sent),
 * the first descriptor will be used to inform the IPA hardware that
 * apps need to push data into the IPA (IP_PACKET_INIT immediate command).
 * Once this send was done from transport point-of-view the IPA driver will
 * get notified by the supplied callback.
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_tx_dp(enum ipa_client_type dst, struct sk_buff *skb,
		struct ipa_tx_meta *meta)
{
	struct ipa3_desc *desc;
	struct ipa3_desc _desc[3];
	int dst_ep_idx;
	struct ipa3_sys_context *sys;
	int src_ep_idx;
	int num_frags, f;
	const struct ipa_gsi_ep_config *gsi_ep;
	int data_idx;
	unsigned int max_desc;
	enum ipa_client_type type;

	if (unlikely(!ipa3_ctx)) {
		IPAERR("IPA3 driver was not initialized\n");
		return -EINVAL;
	}

	if (skb->len == 0) {
		IPAERR("packet size is 0\n");
		return -EINVAL;
	}

	/*
	 * USB_CONS: PKT_INIT ep_idx = dst pipe
	 * Q6_CONS: PKT_INIT ep_idx = sender pipe
	 * A5_LAN_WAN_PROD: HW path ep_idx = sender pipe
	 *
	 * LAN TX: all PKT_INIT
	 * WAN TX: PKT_INIT (cmd) + HW (data)
	 *
	 */
	if (IPA_CLIENT_IS_CONS(dst)) {
		src_ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_LAN_PROD);
		if (-1 == src_ep_idx) {
			IPAERR("Client %u is not mapped\n",
				IPA_CLIENT_APPS_LAN_PROD);
			goto fail_gen;
		}
		dst_ep_idx = ipa_get_ep_mapping(dst);
	} else {
		src_ep_idx = ipa_get_ep_mapping(dst);
		if (-1 == src_ep_idx) {
			IPAERR("Client %u is not mapped\n", dst);
			goto fail_gen;
		}
		if (meta && meta->pkt_init_dst_ep_valid)
			dst_ep_idx = meta->pkt_init_dst_ep;
		else
			dst_ep_idx = -1;
	}

	if (atomic_read(&ipa3_ctx->is_suspend_mode_enabled)) {
		atomic_set(&ipa3_ctx->is_suspend_mode_enabled, 0);
		type = ipa3_get_client_by_pipe(src_ep_idx);
		IPAERR("Client %s woke up the system\n", ipa_clients_strings[type]);
	}

	sys = ipa3_ctx->ep[src_ep_idx].sys;

	if (!sys || !sys->ep->valid) {
		IPAERR_RL("pipe %d not valid\n", src_ep_idx);
		goto fail_pipe_not_valid;
	}

	trace_ipa_tx_dp(skb,sys->ep->client);
	num_frags = skb_shinfo(skb)->nr_frags;
	/*
	 * make sure TLV FIFO supports the needed frags.
	 * 2 descriptors are needed for IP_PACKET_INIT and TAG_STATUS.
	 * 1 descriptor needed for the linear portion of skb.
	 */
	gsi_ep = ipa_get_gsi_ep_info(ipa3_ctx->ep[src_ep_idx].client);
	if (unlikely(gsi_ep == NULL)) {
		IPAERR("failed to get EP %d GSI info\n", src_ep_idx);
		goto fail_gen;
	}
	max_desc =  gsi_ep->ipa_if_tlv;
	if (gsi_ep->prefetch_mode == GSI_SMART_PRE_FETCH ||
		gsi_ep->prefetch_mode == GSI_FREE_PRE_FETCH)
		max_desc -= gsi_ep->prefetch_threshold;
	if (num_frags + 3 > max_desc) {
		if (skb_linearize(skb)) {
			IPAERR("Failed to linear skb with %d frags\n",
				num_frags);
			goto fail_gen;
		}
		num_frags = 0;
	}
	if (num_frags) {
		/* 1 desc for tag to resolve status out-of-order issue;
		 * 1 desc is needed for the linear portion of skb;
		 * 1 desc may be needed for the PACKET_INIT;
		 * 1 desc for each frag
		 */
		desc = kzalloc(sizeof(*desc) * (num_frags + 3), GFP_ATOMIC);
		if (!desc) {
			IPAERR("failed to alloc desc array\n");
			goto fail_gen;
		}
	} else {
		memset(_desc, 0, 3 * sizeof(struct ipa3_desc));
		desc = &_desc[0];
	}

	if (dst_ep_idx != -1) {
		/* SW data path */
		int skb_idx;
		struct iphdr *network_header;

		network_header = (struct iphdr *)(skb_network_header(skb));

		data_idx = 0;

		if (sys->policy == IPA_POLICY_NOINTR_MODE) {
			/*
			 * For non-interrupt mode channel (where there is no
			 * event ring) TAG STATUS are used for completion
			 * notification. IPA will generate a status packet with
			 * tag info as a result of the TAG STATUS command.
			 */
			desc[data_idx].is_tag_status = true;
			data_idx++;
		}

		if ((ipa3_ctx->ipa_hw_type >= IPA_HW_v5_0) &&
		    ((network_header->version == 4 &&
		     network_header->protocol == IPPROTO_ICMP) ||
		    (((struct ipv6hdr *)network_header)->version == 6 &&
		     ((struct ipv6hdr *)network_header)->nexthdr == NEXTHDR_ICMP))) {
			ipa_imm_cmd_modify_ip_packet_init_ex_dest_pipe(
				ipa3_ctx->pkt_init_ex_imm[ipa3_ctx->ipa_num_pipes].base,
				dst_ep_idx);
			desc[data_idx].opcode = ipa3_ctx->pkt_init_ex_imm_opcode;
			desc[data_idx].dma_address =
				ipa3_ctx->pkt_init_ex_imm[ipa3_ctx->ipa_num_pipes].phys_base;
		} else if (ipa3_ctx->ep[dst_ep_idx].cfg.ulso.is_ulso_pipe &&
			skb_is_gso(skb)) {
			desc[data_idx].opcode = ipa3_ctx->pkt_init_ex_imm_opcode;
			desc[data_idx].dma_address =
				ipa3_ctx->pkt_init_ex_imm[dst_ep_idx].phys_base;
		} else {
			desc[data_idx].opcode = ipa3_ctx->pkt_init_imm_opcode;
			desc[data_idx].dma_address = ipa3_ctx->pkt_init_imm[dst_ep_idx];
		}
		desc[data_idx].dma_address_valid = true;
		desc[data_idx].type = IPA_IMM_CMD_DESC;
		desc[data_idx].callback = NULL;
		data_idx++;
		desc[data_idx].pyld = skb->data;
		desc[data_idx].len = skb_headlen(skb);
		desc[data_idx].type = IPA_DATA_DESC_SKB;
		desc[data_idx].callback = ipa3_tx_comp_usr_notify_release;
		desc[data_idx].user1 = skb;
		desc[data_idx].user2 = (meta && meta->pkt_init_dst_ep_valid &&
				meta->pkt_init_dst_ep_remote) ?
				src_ep_idx :
				dst_ep_idx;
		if (meta && meta->dma_address_valid) {
			desc[data_idx].dma_address_valid = true;
			desc[data_idx].dma_address = meta->dma_address;
		}

		skb_idx = data_idx;
		data_idx++;

		for (f = 0; f < num_frags; f++) {
			desc[data_idx + f].frag = &skb_shinfo(skb)->frags[f];
			desc[data_idx + f].type = IPA_DATA_DESC_SKB_PAGED;
			desc[data_idx + f].len =
				skb_frag_size(desc[data_idx + f].frag);
		}
		/* don't free skb till frag mappings are released */
		if (num_frags) {
			desc[data_idx + f - 1].callback =
				desc[skb_idx].callback;
			desc[data_idx + f - 1].user1 = desc[skb_idx].user1;
			desc[data_idx + f - 1].user2 = desc[skb_idx].user2;
			desc[skb_idx].callback = NULL;
		}

		if (ipa3_send(sys, num_frags + data_idx, desc, true)) {
			IPAERR_RL("fail to send skb %pK num_frags %u SWP\n",
				skb, num_frags);
			goto fail_send;
		}
		IPA_STATS_INC_CNT(ipa3_ctx->stats.tx_sw_pkts);
	} else {
		/* HW data path */
		data_idx = 0;
		if (sys->policy == IPA_POLICY_NOINTR_MODE) {
			/*
			 * For non-interrupt mode channel (where there is no
			 * event ring) TAG STATUS are used for completion
			 * notification. IPA will generate a status packet with
			 * tag info as a result of the TAG STATUS command.
			 */
			desc[data_idx].is_tag_status = true;
			data_idx++;
		}
		desc[data_idx].pyld = skb->data;
		desc[data_idx].len = skb_headlen(skb);
		desc[data_idx].type = IPA_DATA_DESC_SKB;
		desc[data_idx].callback = ipa3_tx_comp_usr_notify_release;
		desc[data_idx].user1 = skb;
		desc[data_idx].user2 = src_ep_idx;

		if (meta && meta->dma_address_valid) {
			desc[data_idx].dma_address_valid = true;
			desc[data_idx].dma_address = meta->dma_address;
		}
		if (num_frags == 0) {
			if (ipa3_send(sys, data_idx + 1, desc, true)) {
				IPAERR_RL("fail to send skb %pK HWP\n", skb);
				goto fail_mem;
			}
		} else {
			for (f = 0; f < num_frags; f++) {
				desc[data_idx+f+1].frag =
					&skb_shinfo(skb)->frags[f];
				desc[data_idx+f+1].type =
					IPA_DATA_DESC_SKB_PAGED;
				desc[data_idx+f+1].len =
					skb_frag_size(desc[data_idx+f+1].frag);
			}
			/* don't free skb till frag mappings are released */
			desc[data_idx+f].callback = desc[data_idx].callback;
			desc[data_idx+f].user1 = desc[data_idx].user1;
			desc[data_idx+f].user2 = desc[data_idx].user2;
			desc[data_idx].callback = NULL;

			if (ipa3_send(sys, num_frags + data_idx + 1,
				desc, true)) {
				IPAERR_RL("fail to send skb %pK num_frags %u\n",
					skb, num_frags);
				goto fail_mem;
			}
		}
		IPA_STATS_INC_CNT(ipa3_ctx->stats.tx_hw_pkts);
	}

	trace_ipa3_tx_done(sys->ep->client);
	if (num_frags) {
		kfree(desc);
		IPA_STATS_INC_CNT(ipa3_ctx->stats.tx_non_linear);
	}
	return 0;

fail_send:
fail_mem:
	if (num_frags)
		kfree(desc);
fail_gen:
	return -EFAULT;
fail_pipe_not_valid:
	return -EPIPE;
}
EXPORT_SYMBOL(ipa_tx_dp);

static void ipa3_wq_handle_rx(struct work_struct *work)
{
	struct ipa3_sys_context *sys;
	enum ipa_client_type client_type;

	sys = container_of(work, struct ipa3_sys_context, work);
	/*
	 * Mark client as WAN_COAL_CONS only as
	 * NAPI only use sys of WAN_COAL_CONS.
	 */
	if (IPA_CLIENT_IS_WAN_CONS(sys->ep->client))
		client_type = IPA_CLIENT_APPS_WAN_COAL_CONS;
	else if (IPA_CLIENT_IS_LAN_CONS(sys->ep->client))
		client_type = IPA_CLIENT_APPS_LAN_COAL_CONS;
	else
		client_type = sys->ep->client;

	IPA_ACTIVE_CLIENTS_INC_EP(client_type);
	if (ipa_net_initialized && sys->napi_obj) {
		napi_schedule(sys->napi_obj);
	} else if (ipa_net_initialized &&
		sys->ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS) {
		napi_schedule(&sys->napi_rx);
	} else if (IPA_CLIENT_IS_LOW_LAT_CONS(sys->ep->client)) {
		tasklet_schedule(&sys->tasklet);
	} else
		ipa3_handle_rx(sys);
}

static void ipa3_wq_repl_rx(struct work_struct *work)
{
	struct ipa3_sys_context *sys;
	void *ptr;
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	gfp_t flag = GFP_KERNEL;
	u32 next;
	u32 curr;

	sys = container_of(work, struct ipa3_sys_context, repl_work);
	atomic_set(&sys->repl->pending, 0);
	curr = atomic_read(&sys->repl->tail_idx);

begin:
	while (1) {
		next = (curr + 1) % sys->repl->capacity;
		if (next == atomic_read(&sys->repl->head_idx))
			goto fail_kmem_cache_alloc;

		rx_pkt = kmem_cache_zalloc(ipa3_ctx->rx_pkt_wrapper_cache,
					   flag);
		if (!rx_pkt)
			goto fail_kmem_cache_alloc;

		INIT_WORK(&rx_pkt->work, ipa3_wq_rx_avail);
		rx_pkt->sys = sys;

		rx_pkt->data.skb = sys->get_skb(sys->rx_buff_sz, flag);
		if (rx_pkt->data.skb == NULL)
			goto fail_skb_alloc;

		ptr = skb_put(rx_pkt->data.skb, sys->rx_buff_sz);
		rx_pkt->data.dma_addr = dma_map_single(ipa3_ctx->pdev, ptr,
						     sys->rx_buff_sz,
						     DMA_FROM_DEVICE);
		if (dma_mapping_error(ipa3_ctx->pdev, rx_pkt->data.dma_addr)) {
			pr_err_ratelimited("%s dma map fail %pK for %pK sys=%pK\n",
			       __func__, (void *)rx_pkt->data.dma_addr,
			       ptr, sys);
			goto fail_dma_mapping;
		}

		sys->repl->cache[curr] = rx_pkt;
		curr = next;
		/* ensure write is done before setting tail index */
		mb();
		atomic_set(&sys->repl->tail_idx, next);
	}

	return;

fail_dma_mapping:
	sys->free_skb(rx_pkt->data.skb);
fail_skb_alloc:
	kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
fail_kmem_cache_alloc:
	if (atomic_read(&sys->repl->tail_idx) ==
			atomic_read(&sys->repl->head_idx)) {
		if (IPA_CLIENT_IS_WAN_CONS(sys->ep->client))
			IPA_STATS_INC_CNT(ipa3_ctx->stats.wan_repl_rx_empty);
		else if (IPA_CLIENT_IS_LAN_CONS(sys->ep->client))
			IPA_STATS_INC_CNT(ipa3_ctx->stats.lan_repl_rx_empty);
		else if (sys->ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_CONS)
			IPA_STATS_INC_CNT(ipa3_ctx->stats.low_lat_repl_rx_empty);
		else if (sys->ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS)
			IPA_STATS_INC_CNT(ipa3_ctx->stats.rmnet_ll_repl_rx_empty);
		pr_err_ratelimited("%s sys=%pK repl ring empty\n",
				__func__, sys);
		goto begin;
	}
}

static struct page *ipa3_alloc_page(
	gfp_t flag, u32 *page_order, bool try_lower)
{
	struct page *page = NULL;
	u32 p_order = *page_order;

	page = __dev_alloc_pages(flag, p_order);
	/* We will only try 1 page order lower. */
	if (unlikely(!page)) {
		if (try_lower && p_order > 0) {
			p_order = p_order - 1;
			page = __dev_alloc_pages(flag, p_order);
			if (likely(page))
				ipa3_ctx->stats.lower_order++;
		}
	}
	*page_order = p_order;
	return page;
}

static struct page *ipa3_rmnet_alloc_page(
	gfp_t flag, u32 *page_order, bool try_lower)
{
	struct page *page = NULL;
	u32 p_order = *page_order;
	int rc, porder;

	while (true) {
		page = rmnet_mem_get_pages_entry(
			flag, p_order, &rc, &porder, IPA_ID);

		if (unlikely(!page)) {
			if (p_order > 0) {
				p_order = p_order - 1;
				continue;
			}
			break;
		}

		ipa3_ctx->stats.lower_order++;
		break;
	}

	if (unlikely(!page))
		IPAERR("rmnet page alloc fails\n");

	*page_order = p_order;
	return page;
}

static struct ipa3_rx_pkt_wrapper *ipa3_alloc_rx_pkt_page(
	gfp_t flag, bool is_tmp_alloc, struct ipa3_sys_context *sys)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;

	flag |= __GFP_NOMEMALLOC;
	rx_pkt = kmem_cache_zalloc(ipa3_ctx->rx_pkt_wrapper_cache,
		flag);
	if (unlikely(!rx_pkt))
		return NULL;

	rx_pkt->page_data.page_order = sys->page_order;
	/* For temporary allocations, avoid triggering OOM Killer. */
	if (is_tmp_alloc) {
		flag |= __GFP_RETRY_MAYFAIL | __GFP_NOWARN;
		rx_pkt->page_data.page = ipa3_rmnet_alloc_page(
			flag, &rx_pkt->page_data.page_order, true);
	} else {
		/* Try a lower order page for order 3 pages in case allocation fails. */
		rx_pkt->page_data.page = ipa3_alloc_page(flag,
					&rx_pkt->page_data.page_order,
					(is_tmp_alloc && rx_pkt->page_data.page_order == 3));
	}

	if (unlikely(!rx_pkt->page_data.page))
		goto fail_page_alloc;

	rx_pkt->len = PAGE_SIZE << rx_pkt->page_data.page_order;

	rx_pkt->page_data.dma_addr = dma_map_page(ipa3_ctx->pdev,
			rx_pkt->page_data.page, 0,
			rx_pkt->len, DMA_FROM_DEVICE);
	if (dma_mapping_error(ipa3_ctx->pdev,
		rx_pkt->page_data.dma_addr)) {
		pr_err_ratelimited("%s dma map fail %pK for %pK\n",
			__func__, (void *)rx_pkt->page_data.dma_addr,
			rx_pkt->page_data.page);
		goto fail_dma_mapping;
	}
	if (is_tmp_alloc)
		rx_pkt->page_data.is_tmp_alloc = true;
	else
		rx_pkt->page_data.is_tmp_alloc = false;
	return rx_pkt;

fail_dma_mapping:
	__free_pages(rx_pkt->page_data.page, rx_pkt->page_data.page_order);
fail_page_alloc:
	kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
	return NULL;
}

static void ipa3_replenish_rx_page_cache(struct ipa3_sys_context *sys)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	u32 curr;

	for (curr = 0; curr < sys->page_recycle_repl->capacity; curr++) {
		rx_pkt = ipa3_alloc_rx_pkt_page(GFP_KERNEL, false, sys);
		if (!rx_pkt) {
			IPAERR("ipa3_alloc_rx_pkt_page fails\n");
			ipa_assert();
			break;
		}
		INIT_LIST_HEAD(&rx_pkt->link);
		rx_pkt->sys = sys;
		list_add_tail(&rx_pkt->link,
			&sys->page_recycle_repl->page_repl_head);
	}
	atomic_set(&sys->common_sys->page_avilable, 1);

	return;

}

static void ipa3_wq_page_repl(struct work_struct *work)
{
	struct ipa3_sys_context *sys;
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	u32 next;
	u32 curr;

	sys = container_of(work, struct ipa3_sys_context, repl_work);
	atomic_set(&sys->repl->pending, 0);
	curr = atomic_read(&sys->repl->tail_idx);

begin:
	while (1) {
		next = (curr + 1) % sys->repl->capacity;
		if (unlikely(next == atomic_read(&sys->repl->head_idx)))
			goto fail_kmem_cache_alloc;
		rx_pkt = ipa3_alloc_rx_pkt_page(GFP_KERNEL, true, sys);
		if (unlikely(!rx_pkt)) {
			IPAERR("ipa3_alloc_rx_pkt_page fails\n");
			break;
		}
		rx_pkt->sys = sys;
		sys->repl->cache[curr] = rx_pkt;
		curr = next;
		/* ensure write is done before setting tail index */
		mb();
		atomic_set(&sys->repl->tail_idx, next);
	}

	return;

fail_kmem_cache_alloc:
	if (atomic_read(&sys->repl->tail_idx) ==
			atomic_read(&sys->repl->head_idx)) {
		if (sys->ep->client == IPA_CLIENT_APPS_WAN_CONS ||
			sys->ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS)
			IPA_STATS_INC_CNT(ipa3_ctx->stats.wan_repl_rx_empty);
		if (sys->ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS)
			IPA_STATS_INC_CNT(ipa3_ctx->stats.rmnet_ll_repl_rx_empty);
		pr_err_ratelimited("%s sys=%pK wq_repl ring empty\n",
				__func__, sys);
		goto begin;
	}

}

static inline void __trigger_repl_work(struct ipa3_sys_context *sys)
{
	int tail, head, avail;

	if (atomic_read(&sys->repl->pending))
		return;

	tail = atomic_read(&sys->repl->tail_idx);
	head = atomic_read(&sys->repl->head_idx);
	avail = (tail - head) % sys->repl->capacity;

	if (avail < sys->repl->capacity / 2) {
		atomic_set(&sys->repl->pending, 1);
		queue_work(sys->repl_wq, &sys->repl_work);
	}
}

static struct ipa3_rx_pkt_wrapper * ipa3_get_free_page
(
	struct ipa3_sys_context *sys,
	u32 stats_i
)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt = NULL;
	struct ipa3_rx_pkt_wrapper *tmp = NULL;
	struct page *cur_page;
	int i = 0;
	u8 LOOP_THRESHOLD = ipa3_ctx->page_poll_threshold;

	spin_lock_bh(&sys->common_sys->spinlock);
	list_for_each_entry_safe(rx_pkt, tmp,
		&sys->page_recycle_repl->page_repl_head, link) {
		if (i == LOOP_THRESHOLD)
			break;
		cur_page = rx_pkt->page_data.page;
		if (page_ref_count(cur_page) == 1) {
			/* Found a free page. */
			page_ref_inc(cur_page);
			list_del_init(&rx_pkt->link);
			++ipa3_ctx->stats.page_recycle_cnt[stats_i][i];
			sys->common_sys->napi_sort_page_thrshld_cnt = 0;
			spin_unlock_bh(&sys->common_sys->spinlock);
			return rx_pkt;
		}
		i++;
	}
	spin_unlock_bh(&sys->common_sys->spinlock);
	IPADBG_LOW("napi_sort_page_thrshld_cnt = %d ipa_max_napi_sort_page_thrshld = %d\n",
			sys->common_sys->napi_sort_page_thrshld_cnt,
			ipa3_ctx->ipa_max_napi_sort_page_thrshld);
	/*Scheduling tasklet to find the free page*/
	if (sys->common_sys->napi_sort_page_thrshld_cnt >=
			ipa3_ctx->ipa_max_napi_sort_page_thrshld) {
		atomic_set(&sys->common_sys->page_avilable, 0);
		tasklet_schedule(&sys->common_sys->tasklet_find_freepage);
		++ipa3_ctx->stats.num_sort_tasklet_sched[stats_i];
		spin_lock(&ipa3_ctx->notifier_lock);
		if(ipa3_ctx->ipa_rmnet_notifier_enabled &&
		   !ipa3_ctx->free_page_task_scheduled) {
				atomic_inc(&ipa3_ctx->stats.num_free_page_task_scheduled);
				if (stats_i ==2) {
					raw_notifier_call_chain(ipa3_ctx->ipa_rmnet_notifier_list_internal,
					FREE_PAGE_TASK_SCHEDULED_LL, &sys->common_sys->napi_sort_page_thrshld_cnt);
				}
				else {
					raw_notifier_call_chain(ipa3_ctx->ipa_rmnet_notifier_list_internal,
						FREE_PAGE_TASK_SCHEDULED, &sys->common_sys->napi_sort_page_thrshld_cnt);
				}
				ipa3_ctx->free_page_task_scheduled = true;
			}
			spin_unlock(&ipa3_ctx->notifier_lock);
	}
	return NULL;
}

int ipa_register_notifier(void *fn_ptr)
{
	struct ipa_notifier_block_data *ipa_notifier_block;
	if (fn_ptr == NULL)
		return -EFAULT;
	spin_lock(&ipa3_ctx->notifier_lock);
	ipa_notifier_block = (struct ipa_notifier_block_data *)kzalloc(sizeof(struct ipa_notifier_block_data), GFP_KERNEL);
	if (ipa_notifier_block == NULL) {
		IPAWANERR("Buffer threshold notifier failure\n");
		spin_unlock(&ipa3_ctx->notifier_lock);
		return -EFAULT;
	}
	ipa_notifier_block->ipa_rmnet_notifier.notifier_call = fn_ptr;
	list_add(&ipa_notifier_block->entry, &ipa3_ctx->notifier_block_list_head);
	raw_notifier_chain_register(ipa3_ctx->ipa_rmnet_notifier_list_internal,
		&ipa_notifier_block->ipa_rmnet_notifier);
	IPAWANERR("Registered noifier for buffer threshold\n");

	ipa3_ctx->ipa_rmnet_notifier_enabled = true;
	spin_unlock(&ipa3_ctx->notifier_lock);
	return 0;
}
EXPORT_SYMBOL(ipa_register_notifier);

int ipa_unregister_notifier(void *fn_ptr)
{
	struct ipa_notifier_block_data *ipa_notifier_block, *temp;
	if (fn_ptr == NULL)
		return -EFAULT;
	spin_lock(&ipa3_ctx->notifier_lock);
	/* Find the client pointer, unregister and remove from the list */
	list_for_each_entry_safe(ipa_notifier_block, temp, &ipa3_ctx->notifier_block_list_head, entry) {
		if (ipa_notifier_block->ipa_rmnet_notifier.notifier_call == fn_ptr) {
			raw_notifier_chain_unregister(ipa3_ctx->ipa_rmnet_notifier_list_internal,
					&ipa_notifier_block->ipa_rmnet_notifier);
			list_del(&ipa_notifier_block->entry);
			kfree(ipa_notifier_block);
			IPAWANERR("Client removed from list and unregistered succesfully\n");
			spin_unlock(&ipa3_ctx->notifier_lock);
			return 0;
		}
	}
	spin_unlock(&ipa3_ctx->notifier_lock);
	IPAWANERR("Unable to find the client in the list\n");
	return 0;
}
EXPORT_SYMBOL(ipa_unregister_notifier);

 static void ipa3_replenish_rx_page_recycle(struct ipa3_sys_context *sys)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	int ret;
	int rx_len_cached = 0;
	struct gsi_xfer_elem gsi_xfer_elem_array[IPA_REPL_XFER_MAX];
	u32 curr_wq;
	int idx = 0;
	u32 stats_i = 0;

	/* start replenish only when buffers go lower than the threshold */
	if (sys->rx_pool_sz - sys->len < IPA_REPL_XFER_THRESH)
		return;
	switch (sys->ep->client) {
		case IPA_CLIENT_APPS_WAN_COAL_CONS:
			stats_i = 0;
			break;
		case IPA_CLIENT_APPS_WAN_CONS:
			stats_i = 1;
			break;
		case IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS:
			stats_i = 2;
			break;
		default:
			IPAERR_RL("Unexpected client%d\n", sys->ep->client);
	}

	rx_len_cached = sys->len;
	curr_wq = atomic_read(&sys->repl->head_idx);

	while (rx_len_cached < sys->rx_pool_sz) {
		/* check for an idle page that can be used */
		if (atomic_read(&sys->common_sys->page_avilable) &&
			((rx_pkt = ipa3_get_free_page(sys,stats_i)) != NULL)) {
			ipa3_ctx->stats.page_recycle_stats[stats_i].page_recycled++;

		} else {
			/*
			 * Could not find idle page at curr index.
			 * Allocate a new one.
			 */
			if (curr_wq == atomic_read(&sys->repl->tail_idx))
				break;
			ipa3_ctx->stats.page_recycle_stats[stats_i].tmp_alloc++;
			rx_pkt = sys->repl->cache[curr_wq];
			curr_wq = (++curr_wq == sys->repl->capacity) ?
								 0 : curr_wq;
		}
		rx_pkt->sys = sys;

		trace_ipa3_replenish_rx_page_recycle(
			stats_i,
			rx_pkt->page_data.page,
			rx_pkt->page_data.is_tmp_alloc);

		dma_sync_single_for_device(ipa3_ctx->pdev,
			rx_pkt->page_data.dma_addr,
			rx_pkt->len, DMA_FROM_DEVICE);
		gsi_xfer_elem_array[idx].addr = rx_pkt->page_data.dma_addr;
		gsi_xfer_elem_array[idx].len = rx_pkt->len;
		gsi_xfer_elem_array[idx].flags = GSI_XFER_FLAG_EOT;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_EOB;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_BEI;
		gsi_xfer_elem_array[idx].type = GSI_XFER_ELEM_DATA;
		gsi_xfer_elem_array[idx].xfer_user_data = rx_pkt;
		rx_len_cached++;
		idx++;
		ipa3_ctx->stats.page_recycle_stats[stats_i].total_replenished++;
		/*
		 * gsi_xfer_elem_buffer has a size of IPA_REPL_XFER_THRESH.
		 * If this size is reached we need to queue the xfers.
		 */
		if (idx == IPA_REPL_XFER_MAX) {
			ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
				gsi_xfer_elem_array, false);
			if (ret != GSI_STATUS_SUCCESS) {
				/* we don't expect this will happen */
				IPAERR("failed to provide buffer: %d\n", ret);
				ipa_assert();
				break;
			}
			idx = 0;
		}
	}
	/* only ring doorbell once here */
	ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
			gsi_xfer_elem_array, true);
	if (ret == GSI_STATUS_SUCCESS) {
		/* ensure write is done before setting head index */
		mb();
		atomic_set(&sys->repl->head_idx, curr_wq);
		sys->len = rx_len_cached;
	} else {
		/* we don't expect this will happen */
		IPAERR("failed to provide buffer: %d\n", ret);
		ipa_assert();
	}

	 if (sys->common_buff_pool)
	 	__trigger_repl_work(sys->common_sys);
	 else
		__trigger_repl_work(sys);

	if (rx_len_cached <= IPA_DEFAULT_SYS_YELLOW_WM) {
		if (sys->ep->client == IPA_CLIENT_APPS_WAN_CONS) {
			IPA_STATS_INC_CNT(ipa3_ctx->stats.wan_rx_empty);
			spin_lock(&ipa3_ctx->notifier_lock);
			if (ipa3_ctx->ipa_rmnet_notifier_enabled
				&& !ipa3_ctx->buff_below_thresh_for_def_pipe_notified) {
				atomic_inc(&ipa3_ctx->stats.num_buff_below_thresh_for_def_pipe_notified);
				raw_notifier_call_chain(ipa3_ctx->ipa_rmnet_notifier_list_internal,
					BUFF_BELOW_LOW_THRESHOLD_FOR_DEFAULT_PIPE, &rx_len_cached);
				ipa3_ctx->buff_above_thresh_for_def_pipe_notified = false;
				ipa3_ctx->buff_below_thresh_for_def_pipe_notified = true;
			}
			spin_unlock(&ipa3_ctx->notifier_lock);
		}
		else if (sys->ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS) {
			IPA_STATS_INC_CNT(ipa3_ctx->stats.wan_rx_empty_coal);
			spin_lock(&ipa3_ctx->notifier_lock);
			if (ipa3_ctx->ipa_rmnet_notifier_enabled
				&& !ipa3_ctx->buff_below_thresh_for_coal_pipe_notified) {
				atomic_inc(&ipa3_ctx->stats.num_buff_below_thresh_for_coal_pipe_notified);
				raw_notifier_call_chain(ipa3_ctx->ipa_rmnet_notifier_list_internal,
					BUFF_BELOW_LOW_THRESHOLD_FOR_COAL_PIPE, &rx_len_cached);
				ipa3_ctx->buff_above_thresh_for_coal_pipe_notified = false;
				ipa3_ctx->buff_below_thresh_for_coal_pipe_notified = true;
			}
			spin_unlock(&ipa3_ctx->notifier_lock);
		}
		else if (sys->ep->client == IPA_CLIENT_APPS_LAN_CONS)
			IPA_STATS_INC_CNT(ipa3_ctx->stats.lan_rx_empty);
		else if (sys->ep->client == IPA_CLIENT_APPS_LAN_COAL_CONS)
			IPA_STATS_INC_CNT(ipa3_ctx->stats.lan_rx_empty_coal);
		else if (sys->ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS) {
			IPA_STATS_INC_CNT(ipa3_ctx->stats.rmnet_ll_rx_empty);
			spin_lock(&ipa3_ctx->notifier_lock);
			if (ipa3_ctx->ipa_rmnet_notifier_enabled
				&& !ipa3_ctx->buff_below_thresh_for_ll_pipe_notified) {
				atomic_inc(&ipa3_ctx->stats.num_buff_below_thresh_for_ll_pipe_notified);
				raw_notifier_call_chain(ipa3_ctx->ipa_rmnet_notifier_list_internal,
					BUFF_BELOW_LOW_THRESHOLD_FOR_LL_PIPE, &rx_len_cached);
				ipa3_ctx->buff_above_thresh_for_ll_pipe_notified = false;
				ipa3_ctx->buff_below_thresh_for_ll_pipe_notified = true;
			}
			spin_unlock(&ipa3_ctx->notifier_lock);
		}
		else
			WARN_ON(1);
	}

	if (rx_len_cached >= IPA_BUFF_THRESHOLD_HIGH) {
		if (sys->ep->client == IPA_CLIENT_APPS_WAN_CONS) {
			spin_lock(&ipa3_ctx->notifier_lock);
			if(ipa3_ctx->ipa_rmnet_notifier_enabled &&
				!ipa3_ctx->buff_above_thresh_for_def_pipe_notified) {
				atomic_inc(&ipa3_ctx->stats.num_buff_above_thresh_for_def_pipe_notified);
				raw_notifier_call_chain(ipa3_ctx->ipa_rmnet_notifier_list_internal,
					BUFF_ABOVE_HIGH_THRESHOLD_FOR_DEFAULT_PIPE, &rx_len_cached);
				ipa3_ctx->buff_above_thresh_for_def_pipe_notified = true;
				ipa3_ctx->buff_below_thresh_for_def_pipe_notified = false;
			}
			spin_unlock(&ipa3_ctx->notifier_lock);
		} else if (sys->ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS) {
			spin_lock(&ipa3_ctx->notifier_lock);
			if(ipa3_ctx->ipa_rmnet_notifier_enabled &&
				!ipa3_ctx->buff_above_thresh_for_coal_pipe_notified) {
				atomic_inc(&ipa3_ctx->stats.num_buff_above_thresh_for_coal_pipe_notified);
				raw_notifier_call_chain(ipa3_ctx->ipa_rmnet_notifier_list_internal,
					BUFF_ABOVE_HIGH_THRESHOLD_FOR_COAL_PIPE, &rx_len_cached);
				ipa3_ctx->buff_above_thresh_for_coal_pipe_notified = true;
				ipa3_ctx->buff_below_thresh_for_coal_pipe_notified = false;
			}
			spin_unlock(&ipa3_ctx->notifier_lock);
		} else if (sys->ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS) {
			spin_lock(&ipa3_ctx->notifier_lock);
			if(ipa3_ctx->ipa_rmnet_notifier_enabled &&
				!ipa3_ctx->buff_above_thresh_for_ll_pipe_notified) {
				atomic_inc(&ipa3_ctx->stats.num_buff_above_thresh_for_ll_pipe_notified);
				raw_notifier_call_chain(ipa3_ctx->ipa_rmnet_notifier_list_internal,
					BUFF_ABOVE_HIGH_THRESHOLD_FOR_LL_PIPE, &rx_len_cached);
				ipa3_ctx->buff_above_thresh_for_ll_pipe_notified = true;
				ipa3_ctx->buff_below_thresh_for_ll_pipe_notified = false;
			}
			spin_unlock(&ipa3_ctx->notifier_lock);
		}
	}

	return;
}

static void ipa3_replenish_wlan_rx_cache(struct ipa3_sys_context *sys)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt = NULL;
	struct ipa3_rx_pkt_wrapper *tmp;
	int ret;
	struct gsi_xfer_elem gsi_xfer_elem_one;
	u32 rx_len_cached = 0;

	IPADBG_LOW("\n");

	spin_lock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);
	rx_len_cached = sys->len;

	if (rx_len_cached < sys->rx_pool_sz) {
		list_for_each_entry_safe(rx_pkt, tmp,
			&ipa3_ctx->wc_memb.wlan_comm_desc_list, link) {
			list_del(&rx_pkt->link);

			if (ipa3_ctx->wc_memb.wlan_comm_free_cnt > 0)
				ipa3_ctx->wc_memb.wlan_comm_free_cnt--;

			rx_pkt->len = 0;
			rx_pkt->sys = sys;

			memset(&gsi_xfer_elem_one, 0,
				sizeof(gsi_xfer_elem_one));
			gsi_xfer_elem_one.addr = rx_pkt->data.dma_addr;
			gsi_xfer_elem_one.len = IPA_WLAN_RX_BUFF_SZ;
			gsi_xfer_elem_one.flags |= GSI_XFER_FLAG_EOT;
			gsi_xfer_elem_one.flags |= GSI_XFER_FLAG_EOB;
			gsi_xfer_elem_one.type = GSI_XFER_ELEM_DATA;
			gsi_xfer_elem_one.xfer_user_data = rx_pkt;

			ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, 1,
				&gsi_xfer_elem_one, true);

			if (ret) {
				IPAERR("failed to provide buffer: %d\n", ret);
				goto fail_provide_rx_buffer;
			}

			rx_len_cached = ++sys->len;

			if (rx_len_cached >= sys->rx_pool_sz) {
				spin_unlock_bh(
					&ipa3_ctx->wc_memb.wlan_spinlock);
				return;
			}
		}
	}
	spin_unlock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);

	if (rx_len_cached < sys->rx_pool_sz &&
			ipa3_ctx->wc_memb.wlan_comm_total_cnt <
			 IPA_WLAN_COMM_RX_POOL_HIGH) {
		ipa3_replenish_rx_cache(sys);
		ipa3_ctx->wc_memb.wlan_comm_total_cnt +=
			(sys->len - rx_len_cached);
	}

	return;

fail_provide_rx_buffer:
	list_del(&rx_pkt->link);
	spin_unlock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);
}

static void ipa3_cleanup_wlan_rx_common_cache(void)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	struct ipa3_rx_pkt_wrapper *tmp;

	spin_lock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);

	list_for_each_entry_safe(rx_pkt, tmp,
		&ipa3_ctx->wc_memb.wlan_comm_desc_list, link) {
		list_del(&rx_pkt->link);
		dma_unmap_single(ipa3_ctx->pdev, rx_pkt->data.dma_addr,
				IPA_WLAN_RX_BUFF_SZ, DMA_FROM_DEVICE);
		dev_kfree_skb_any(rx_pkt->data.skb);
		kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
		ipa3_ctx->wc_memb.wlan_comm_free_cnt--;
		ipa3_ctx->wc_memb.wlan_comm_total_cnt--;
	}
	ipa3_ctx->wc_memb.total_tx_pkts_freed = 0;

	if (ipa3_ctx->wc_memb.wlan_comm_free_cnt != 0)
		IPAERR("wlan comm buff free cnt: %d\n",
			ipa3_ctx->wc_memb.wlan_comm_free_cnt);

	if (ipa3_ctx->wc_memb.wlan_comm_total_cnt != 0)
		IPAERR("wlan comm buff total cnt: %d\n",
			ipa3_ctx->wc_memb.wlan_comm_total_cnt);

	spin_unlock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);

}

static void ipa3_alloc_wlan_rx_common_cache(u32 size)
{
	void *ptr;
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	int rx_len_cached = 0;
	gfp_t flag = GFP_NOWAIT | __GFP_NOWARN;

	rx_len_cached = ipa3_ctx->wc_memb.wlan_comm_total_cnt;
	while (rx_len_cached < size) {
		rx_pkt = kmem_cache_zalloc(ipa3_ctx->rx_pkt_wrapper_cache,
					   flag);
		if (!rx_pkt)
			goto fail_kmem_cache_alloc;

		INIT_LIST_HEAD(&rx_pkt->link);
		INIT_WORK(&rx_pkt->work, ipa3_wq_rx_avail);

		rx_pkt->data.skb =
			ipa3_get_skb_ipa_rx(IPA_WLAN_RX_BUFF_SZ,
						flag);
		if (rx_pkt->data.skb == NULL) {
			IPAERR("failed to alloc skb\n");
			goto fail_skb_alloc;
		}
		ptr = skb_put(rx_pkt->data.skb, IPA_WLAN_RX_BUFF_SZ);
		rx_pkt->data.dma_addr = dma_map_single(ipa3_ctx->pdev, ptr,
				IPA_WLAN_RX_BUFF_SZ, DMA_FROM_DEVICE);
		if (dma_mapping_error(ipa3_ctx->pdev, rx_pkt->data.dma_addr)) {
			IPAERR("dma_map_single failure %pK for %pK\n",
			       (void *)rx_pkt->data.dma_addr, ptr);
			goto fail_dma_mapping;
		}

		spin_lock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);
		list_add_tail(&rx_pkt->link,
			&ipa3_ctx->wc_memb.wlan_comm_desc_list);
		rx_len_cached = ++ipa3_ctx->wc_memb.wlan_comm_total_cnt;

		ipa3_ctx->wc_memb.wlan_comm_free_cnt++;
		spin_unlock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);

	}

	return;

fail_dma_mapping:
	dev_kfree_skb_any(rx_pkt->data.skb);
fail_skb_alloc:
	kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
fail_kmem_cache_alloc:
	return;
}

/**
 * ipa3_first_replenish_rx_cache() - Replenish the Rx packets cache for the first time.
 *
 * The function allocates buffers in the rx_pkt_wrapper_cache cache until there
 * are IPA_RX_POOL_CEIL buffers in the cache.
 *   - Allocate a buffer in the cache
 *   - Initialized the packets link
 *   - Initialize the packets work struct
 *   - Allocate the packets socket buffer (skb)
 *   - Fill the packets skb with data
 *   - Make the packet DMAable
 *   - Add the packet to the system pipe linked list
 */
static void ipa3_first_replenish_rx_cache(struct ipa3_sys_context *sys)
{
	void *ptr;
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	int ret;
	int idx = 0;
	int rx_len_cached = 0;
	struct gsi_xfer_elem gsi_xfer_elem_array[IPA_REPL_XFER_MAX];
	gfp_t flag = GFP_NOWAIT | __GFP_NOWARN;

	rx_len_cached = sys->len;

	/* start replenish only when buffers go lower than the threshold */
	if (sys->rx_pool_sz - sys->len < IPA_REPL_XFER_THRESH)
		return;

	while (rx_len_cached < sys->rx_pool_sz) {
		rx_pkt = kmem_cache_zalloc(ipa3_ctx->rx_pkt_wrapper_cache,
					   flag);
		if (!rx_pkt) {
			IPAERR("failed to alloc cache\n");
			goto fail_kmem_cache_alloc;
		}

		INIT_WORK(&rx_pkt->work, ipa3_wq_rx_avail);
		rx_pkt->sys = sys;

		rx_pkt->data.skb = sys->get_skb(sys->rx_buff_sz, flag);
		if (rx_pkt->data.skb == NULL) {
			IPAERR("failed to alloc skb\n");
			goto fail_skb_alloc;
		}
		ptr = skb_put(rx_pkt->data.skb, sys->rx_buff_sz);
		rx_pkt->data.dma_addr = dma_map_single(ipa3_ctx->pdev, ptr,
						     sys->rx_buff_sz,
						     DMA_FROM_DEVICE);
		if (dma_mapping_error(ipa3_ctx->pdev, rx_pkt->data.dma_addr)) {
			IPAERR("dma_map_single failure %pK for %pK\n",
			       (void *)rx_pkt->data.dma_addr, ptr);
			goto fail_dma_mapping;
		}

		gsi_xfer_elem_array[idx].addr = rx_pkt->data.dma_addr;
		gsi_xfer_elem_array[idx].len = sys->rx_buff_sz;
		gsi_xfer_elem_array[idx].flags = GSI_XFER_FLAG_EOT;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_EOB;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_BEI;
		gsi_xfer_elem_array[idx].type = GSI_XFER_ELEM_DATA;
		gsi_xfer_elem_array[idx].xfer_user_data = rx_pkt;
		idx++;
		rx_len_cached++;
		/*
		 * gsi_xfer_elem_buffer has a size of IPA_REPL_XFER_MAX.
		 * If this size is reached we need to queue the xfers.
		 */
		if (idx == IPA_REPL_XFER_MAX) {
			ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
				gsi_xfer_elem_array, false);
			if (ret != GSI_STATUS_SUCCESS) {
				/* we don't expect this will happen */
				IPAERR("failed to provide buffer: %d\n", ret);
				WARN_ON(1);
				break;
			}
			idx = 0;
		}
	}
	goto done;

fail_dma_mapping:
	sys->free_skb(rx_pkt->data.skb);
fail_skb_alloc:
	kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
fail_kmem_cache_alloc:
	/* Ensuring minimum buffers are submitted to HW */
	if (rx_len_cached < IPA_REPL_XFER_THRESH) {
		queue_delayed_work(sys->wq, &sys->replenish_rx_work,
				msecs_to_jiffies(1));
		return;
	}
done:
	/* only ring doorbell once here */
	ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
		gsi_xfer_elem_array, true);
	if (ret == GSI_STATUS_SUCCESS) {
		sys->len = rx_len_cached;
	} else {
		/* we don't expect this will happen */
		IPAERR("failed to provide buffer: %d\n", ret);
		WARN_ON(1);
	}
}

/**
 * ipa3_replenish_rx_cache() - Replenish the Rx packets cache.
 *
 * The function allocates buffers in the rx_pkt_wrapper_cache cache until there
 * are IPA_RX_POOL_CEIL buffers in the cache.
 *   - Allocate a buffer in the cache
 *   - Initialized the packets link
 *   - Initialize the packets work struct
 *   - Allocate the packets socket buffer (skb)
 *   - Fill the packets skb with data
 *   - Make the packet DMAable
 *   - Add the packet to the system pipe linked list
 */
static void ipa3_replenish_rx_cache(struct ipa3_sys_context *sys)
{
	void *ptr;
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	int ret;
	int idx = 0;
	int rx_len_cached = 0;
	struct gsi_xfer_elem gsi_xfer_elem_array[IPA_REPL_XFER_MAX];
	gfp_t flag = GFP_NOWAIT | __GFP_NOWARN;

	rx_len_cached = sys->len;

	/* start replenish only when buffers go lower than the threshold */
	if (sys->rx_pool_sz - sys->len < IPA_REPL_XFER_THRESH)
		return;


	while (rx_len_cached < sys->rx_pool_sz) {
		rx_pkt = kmem_cache_zalloc(ipa3_ctx->rx_pkt_wrapper_cache,
					   flag);
		if (!rx_pkt)
			goto fail_kmem_cache_alloc;

		INIT_WORK(&rx_pkt->work, ipa3_wq_rx_avail);
		rx_pkt->sys = sys;

		rx_pkt->data.skb = sys->get_skb(sys->rx_buff_sz, flag);
		if (rx_pkt->data.skb == NULL) {
			IPAERR("failed to alloc skb\n");
			goto fail_skb_alloc;
		}
		ptr = skb_put(rx_pkt->data.skb, sys->rx_buff_sz);
		rx_pkt->data.dma_addr = dma_map_single(ipa3_ctx->pdev, ptr,
						     sys->rx_buff_sz,
						     DMA_FROM_DEVICE);
		if (dma_mapping_error(ipa3_ctx->pdev, rx_pkt->data.dma_addr)) {
			IPAERR("dma_map_single failure %pK for %pK\n",
			       (void *)rx_pkt->data.dma_addr, ptr);
			goto fail_dma_mapping;
		}

		gsi_xfer_elem_array[idx].addr = rx_pkt->data.dma_addr;
		gsi_xfer_elem_array[idx].len = sys->rx_buff_sz;
		gsi_xfer_elem_array[idx].flags = GSI_XFER_FLAG_EOT;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_EOB;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_BEI;
		gsi_xfer_elem_array[idx].type = GSI_XFER_ELEM_DATA;
		gsi_xfer_elem_array[idx].xfer_user_data = rx_pkt;
		idx++;
		rx_len_cached++;
		/*
		 * gsi_xfer_elem_buffer has a size of IPA_REPL_XFER_MAX.
		 * If this size is reached we need to queue the xfers.
		 */
		if (idx == IPA_REPL_XFER_MAX) {
			ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
				gsi_xfer_elem_array, false);
			if (ret != GSI_STATUS_SUCCESS) {
				/* we don't expect this will happen */
				IPAERR("failed to provide buffer: %d\n", ret);
				WARN_ON(1);
				break;
			}
			idx = 0;
		}
	}
	goto done;

fail_dma_mapping:
	sys->free_skb(rx_pkt->data.skb);
fail_skb_alloc:
	kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
fail_kmem_cache_alloc:
	if (rx_len_cached == 0) {
		queue_delayed_work(sys->wq, &sys->replenish_rx_work,
				msecs_to_jiffies(1));
		return;
	}
done:
	/* only ring doorbell once here */
	ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
		gsi_xfer_elem_array, true);
	if (ret == GSI_STATUS_SUCCESS) {
		sys->len = rx_len_cached;
	} else {
		/* we don't expect this will happen */
		IPAERR("failed to provide buffer: %d\n", ret);
		WARN_ON(1);
	}
}

static void ipa3_replenish_rx_cache_recycle(struct ipa3_sys_context *sys)
{
	void *ptr;
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	int ret;
	int idx = 0;
	int rx_len_cached = 0;
	struct gsi_xfer_elem gsi_xfer_elem_array[IPA_REPL_XFER_MAX];
	gfp_t flag = GFP_NOWAIT | __GFP_NOWARN;
	u32 stats_i =
		(sys->ep->client == IPA_CLIENT_APPS_LAN_COAL_CONS) ? 0 :
		(sys->ep->client == IPA_CLIENT_APPS_LAN_CONS)      ? 1 : 2;

	/* start replenish only when buffers go lower than the threshold */
	if (sys->rx_pool_sz - sys->len < IPA_REPL_XFER_THRESH)
		return;

	rx_len_cached = sys->len;

	while (rx_len_cached < sys->rx_pool_sz) {
		if (list_empty(&sys->rcycl_list)) {
			rx_pkt = kmem_cache_zalloc(
				ipa3_ctx->rx_pkt_wrapper_cache, flag);
			if (!rx_pkt)
				goto fail_kmem_cache_alloc;

			INIT_WORK(&rx_pkt->work, ipa3_wq_rx_avail);
			rx_pkt->sys = sys;

			rx_pkt->data.skb = sys->get_skb(sys->rx_buff_sz, flag);
			if (rx_pkt->data.skb == NULL) {
				IPAERR("failed to alloc skb\n");
				kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache,
					rx_pkt);
				goto fail_kmem_cache_alloc;
			}
			ipa3_ctx->stats.cache_recycle_stats[stats_i].pkt_allocd++;
		} else {
			spin_lock_bh(&sys->spinlock);
			rx_pkt = list_first_entry(
				&sys->rcycl_list,
				struct ipa3_rx_pkt_wrapper, link);
			list_del_init(&rx_pkt->link);
			spin_unlock_bh(&sys->spinlock);
			ipa3_ctx->stats.cache_recycle_stats[stats_i].pkt_found++;
		}

		ptr = skb_put(rx_pkt->data.skb, sys->rx_buff_sz);

		rx_pkt->data.dma_addr = dma_map_single(
			ipa3_ctx->pdev, ptr, sys->rx_buff_sz, DMA_FROM_DEVICE);

		if (dma_mapping_error( ipa3_ctx->pdev, rx_pkt->data.dma_addr)) {
			IPAERR("dma_map_single failure %pK for %pK\n",
				   (void *)rx_pkt->data.dma_addr, ptr);
			goto fail_dma_mapping;
		}

		gsi_xfer_elem_array[idx].addr = rx_pkt->data.dma_addr;
		gsi_xfer_elem_array[idx].len = sys->rx_buff_sz;
		gsi_xfer_elem_array[idx].flags = GSI_XFER_FLAG_EOT;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_EOB;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_BEI;
		gsi_xfer_elem_array[idx].type = GSI_XFER_ELEM_DATA;
		gsi_xfer_elem_array[idx].xfer_user_data = rx_pkt;
		idx++;
		rx_len_cached++;
		ipa3_ctx->stats.cache_recycle_stats[stats_i].tot_pkt_replenished++;
		/*
		 * gsi_xfer_elem_buffer has a size of IPA_REPL_XFER_MAX.
		 * If this size is reached we need to queue the xfers.
		 */
		if (idx == IPA_REPL_XFER_MAX) {
			ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
				gsi_xfer_elem_array, false);
			if (ret != GSI_STATUS_SUCCESS) {
				/* we don't expect this will happen */
				IPAERR("failed to provide buffer: %d\n", ret);
				WARN_ON(1);
				break;
			}
			idx = 0;
		}
	}
	goto done;
fail_dma_mapping:
	spin_lock_bh(&sys->spinlock);
	ipa3_skb_recycle(rx_pkt->data.skb);
	list_add_tail(&rx_pkt->link, &sys->rcycl_list);
	spin_unlock_bh(&sys->spinlock);
fail_kmem_cache_alloc:
	if (rx_len_cached == 0) {
		queue_delayed_work(sys->wq, &sys->replenish_rx_work,
		msecs_to_jiffies(1));
		return;
	}

done:
	/* only ring doorbell once here */
	ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
		gsi_xfer_elem_array, true);
	if (ret == GSI_STATUS_SUCCESS) {
		sys->len = rx_len_cached;
	} else {
		/* we don't expect this will happen */
		IPAERR("failed to provide buffer: %d\n", ret);
		WARN_ON(1);
	}
}

static void ipa3_fast_replenish_rx_cache(struct ipa3_sys_context *sys)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	int ret;
	int rx_len_cached = 0;
	struct gsi_xfer_elem gsi_xfer_elem_array[IPA_REPL_XFER_MAX];
	u32 curr;
	int idx = 0;

	/* start replenish only when buffers go lower than the threshold */
	if (sys->rx_pool_sz - sys->len < IPA_REPL_XFER_THRESH)
		return;

	spin_lock_bh(&sys->spinlock);
	rx_len_cached = sys->len;
	curr = atomic_read(&sys->repl->head_idx);

	while (rx_len_cached < sys->rx_pool_sz) {
		if (curr == atomic_read(&sys->repl->tail_idx))
			break;
		rx_pkt = sys->repl->cache[curr];
		gsi_xfer_elem_array[idx].addr = rx_pkt->data.dma_addr;
		gsi_xfer_elem_array[idx].len = sys->rx_buff_sz;
		gsi_xfer_elem_array[idx].flags = GSI_XFER_FLAG_EOT;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_EOB;
		gsi_xfer_elem_array[idx].flags |= GSI_XFER_FLAG_BEI;
		gsi_xfer_elem_array[idx].type = GSI_XFER_ELEM_DATA;
		gsi_xfer_elem_array[idx].xfer_user_data = rx_pkt;
		rx_len_cached++;
		curr = (++curr == sys->repl->capacity) ? 0 : curr;
		idx++;
		/*
		 * gsi_xfer_elem_buffer has a size of IPA_REPL_XFER_THRESH.
		 * If this size is reached we need to queue the xfers.
		 */
		if (idx == IPA_REPL_XFER_MAX) {
			ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
				gsi_xfer_elem_array, false);
			if (ret != GSI_STATUS_SUCCESS) {
				/* we don't expect this will happen */
				IPAERR("failed to provide buffer: %d\n", ret);
				WARN_ON(1);
				break;
			}
			idx = 0;
		}
	}
	/* only ring doorbell once here */
	ret = gsi_queue_xfer(sys->ep->gsi_chan_hdl, idx,
			gsi_xfer_elem_array, true);
	if (ret == GSI_STATUS_SUCCESS) {
		/* ensure write is done before setting head index */
		mb();
		atomic_set(&sys->repl->head_idx, curr);
		sys->len = rx_len_cached;
	} else {
		/* we don't expect this will happen */
		IPAERR("failed to provide buffer: %d\n", ret);
		WARN_ON(1);
	}

	spin_unlock_bh(&sys->spinlock);

	__trigger_repl_work(sys);

	if (rx_len_cached <= IPA_DEFAULT_SYS_YELLOW_WM) {
		if (IPA_CLIENT_IS_WAN_CONS(sys->ep->client))
			IPA_STATS_INC_CNT(ipa3_ctx->stats.wan_rx_empty);
		else if (IPA_CLIENT_IS_LAN_CONS(sys->ep->client))
			IPA_STATS_INC_CNT(ipa3_ctx->stats.lan_rx_empty);
		else if (sys->ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_CONS)
			IPA_STATS_INC_CNT(ipa3_ctx->stats.low_lat_rx_empty);
		else if (sys->ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS)
			IPA_STATS_INC_CNT(ipa3_ctx->stats.rmnet_ll_rx_empty);
		else
			WARN_ON_RATELIMIT_IPA(1);
		queue_delayed_work(sys->wq, &sys->replenish_rx_work,
				msecs_to_jiffies(1));
	}
}

static void ipa3_replenish_rx_work_func(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct ipa3_sys_context *sys;

	dwork = container_of(work, struct delayed_work, work);
	sys = container_of(dwork, struct ipa3_sys_context, replenish_rx_work);
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	sys->repl_hdlr(sys);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
}

/**
 * free_rx_pkt() - function to free the skb and rx_pkt_wrapper
 *
 * @chan_user_data: ipa_sys_context used for skb size and skb_free func
 * @xfer_uder_data: rx_pkt wrapper to be freed
 *
 */
static void free_rx_pkt(void *chan_user_data, void *xfer_user_data)
{

	struct ipa3_rx_pkt_wrapper *rx_pkt = (struct ipa3_rx_pkt_wrapper *)
		xfer_user_data;
	struct ipa3_sys_context *sys = (struct ipa3_sys_context *)
		chan_user_data;

	dma_unmap_single(ipa3_ctx->pdev, rx_pkt->data.dma_addr,
		sys->rx_buff_sz, DMA_FROM_DEVICE);
	sys->free_skb(rx_pkt->data.skb);
	kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
}

/**
 * free_rx_page() - function to free the page and rx_pkt_wrapper
 *
 * @chan_user_data: ipa_sys_context used for skb size and skb_free func
 * @xfer_uder_data: rx_pkt wrapper to be freed
 *
 */
static void free_rx_page(void *chan_user_data, void *xfer_user_data)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt = (struct ipa3_rx_pkt_wrapper *)
		xfer_user_data;

	if (!rx_pkt->page_data.is_tmp_alloc) {
		list_del_init(&rx_pkt->link);
		page_ref_dec(rx_pkt->page_data.page);
		spin_lock_bh(&rx_pkt->sys->common_sys->spinlock);
		/* Add the element to head. */
		list_add(&rx_pkt->link,
			&rx_pkt->sys->page_recycle_repl->page_repl_head);
		spin_unlock_bh(&rx_pkt->sys->common_sys->spinlock);
	} else {
		dma_unmap_page(ipa3_ctx->pdev, rx_pkt->page_data.dma_addr,
			rx_pkt->len, DMA_FROM_DEVICE);
		__free_pages(rx_pkt->page_data.page, rx_pkt->page_data.page_order);
		kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
	}
}

/**
 * ipa3_cleanup_rx() - release RX queue resources
 *
 */
static void ipa3_cleanup_rx(struct ipa3_sys_context *sys)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	struct ipa3_rx_pkt_wrapper *r;
	u32 head;
	u32 tail;

	/*
	 * buffers not consumed by gsi are cleaned up using cleanup callback
	 * provided to gsi
	 */

	spin_lock_bh(&sys->spinlock);
	list_for_each_entry_safe(rx_pkt, r,
				 &sys->rcycl_list, link) {
		list_del(&rx_pkt->link);
		if (rx_pkt->data.dma_addr)
			dma_unmap_single(ipa3_ctx->pdev, rx_pkt->data.dma_addr,
				sys->rx_buff_sz, DMA_FROM_DEVICE);
		else
			IPADBG("DMA address already freed\n");
		sys->free_skb(rx_pkt->data.skb);
		kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
	}
	spin_unlock_bh(&sys->spinlock);

	if (sys->repl) {
		head = atomic_read(&sys->repl->head_idx);
		tail = atomic_read(&sys->repl->tail_idx);
		while (head != tail) {
			rx_pkt = sys->repl->cache[head];
			if (sys->repl_hdlr != ipa3_replenish_rx_page_recycle) {
				dma_unmap_single(ipa3_ctx->pdev,
					rx_pkt->data.dma_addr,
					sys->rx_buff_sz,
					DMA_FROM_DEVICE);
				sys->free_skb(rx_pkt->data.skb);
			} else {
				dma_unmap_page(ipa3_ctx->pdev,
					rx_pkt->page_data.dma_addr,
					rx_pkt->len,
					DMA_FROM_DEVICE);
				__free_pages(rx_pkt->page_data.page, rx_pkt->page_data.page_order);
			}
			kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache,
				rx_pkt);
			head = (head + 1) % sys->repl->capacity;
		}

		kfree(sys->repl->cache);
		kfree(sys->repl);
		sys->repl = NULL;
	}
}

static struct sk_buff *ipa3_skb_copy_for_client(struct sk_buff *skb, int len)
{
	struct sk_buff *skb2 = NULL;

	if (!ipa3_ctx->lan_rx_napi_enable)
		skb2 = __dev_alloc_skb(len + IPA_RX_BUFF_CLIENT_HEADROOM,
					GFP_KERNEL);
	else
		skb2 = __dev_alloc_skb(len + IPA_RX_BUFF_CLIENT_HEADROOM,
					GFP_ATOMIC);

	if (likely(skb2)) {
		/* Set the data pointer */
		skb_reserve(skb2, IPA_RX_BUFF_CLIENT_HEADROOM);
		memcpy(skb2->data, skb->data, len);
		skb2->len = len;
		skb_set_tail_pointer(skb2, len);
	}

	return skb2;
}

static int ipa3_lan_rx_pyld_hdlr(struct sk_buff *skb,
		struct ipa3_sys_context *sys)
{
	struct ipahal_pkt_status status;
	u32 pkt_status_sz;
	struct sk_buff *skb2;
	int pad_len_byte = 0;
	int len;
	unsigned char *buf;
	int src_pipe;
	unsigned int used = *(unsigned int *)skb->cb;
	unsigned int used_align = ALIGN(used, 32);
	unsigned long unused = IPA_GENERIC_RX_BUFF_BASE_SZ - used;
	struct ipa3_tx_pkt_wrapper *tx_pkt = NULL;
	unsigned long ptr;
	enum ipa_client_type type;

	IPA_DUMP_BUFF(skb->data, 0, skb->len);

	if (skb->len == 0) {
		IPAERR("ZLT packet arrived to AP\n");
		goto out;
	}

	if (sys->len_partial) {
		IPADBG_LOW("len_partial %d\n", sys->len_partial);
		buf = skb_push(skb, sys->len_partial);
		memcpy(buf, sys->prev_skb->data, sys->len_partial);
		sys->len_partial = 0;
		sys->free_skb(sys->prev_skb);
		sys->prev_skb = NULL;
		goto begin;
	}

	/* this pipe has TX comp (status only) + mux-ed LAN RX data
	 * (status+data)
	 */
	if (sys->len_rem) {
		IPADBG_LOW("rem %d skb %d pad %d\n", sys->len_rem, skb->len,
				sys->len_pad);
		if (sys->len_rem <= skb->len) {
			if (sys->prev_skb) {
				if (!ipa3_ctx->lan_rx_napi_enable)
					skb2 = skb_copy_expand(sys->prev_skb,
						0, sys->len_rem, GFP_KERNEL);
				else
					skb2 = skb_copy_expand(sys->prev_skb,
						0, sys->len_rem, GFP_ATOMIC);
				if (likely(skb2)) {
					memcpy(skb_put(skb2, sys->len_rem),
						skb->data, sys->len_rem);
					skb_trim(skb2,
						skb2->len - sys->len_pad);
					skb2->truesize = skb2->len +
						sizeof(struct sk_buff);
					if (sys->drop_packet)
						dev_kfree_skb_any(skb2);
					else
						sys->ep->client_notify(
							sys->ep->priv,
							IPA_RECEIVE,
							(unsigned long)(skb2));
				} else {
					IPAERR("copy expand failed\n");
				}
				dev_kfree_skb_any(sys->prev_skb);
			}
			skb_pull(skb, sys->len_rem);
			sys->prev_skb = NULL;
			sys->len_rem = 0;
			sys->len_pad = 0;
		} else {
			if (sys->prev_skb) {
				if (!ipa3_ctx->lan_rx_napi_enable)
					skb2 = skb_copy_expand(sys->prev_skb, 0,
						skb->len, GFP_KERNEL);
				else
					skb2 = skb_copy_expand(sys->prev_skb, 0,
						skb->len, GFP_ATOMIC);
				if (likely(skb2)) {
					memcpy(skb_put(skb2, skb->len),
						skb->data, skb->len);
				} else {
					IPAERR("copy expand failed\n");
				}
				dev_kfree_skb_any(sys->prev_skb);
				sys->prev_skb = skb2;
			}
			sys->len_rem -= skb->len;
			goto out;
		}
	}

begin:
	pkt_status_sz = ipahal_pkt_status_get_size();
	while (skb->len) {
		sys->drop_packet = false;
		IPADBG_LOW("LEN_REM %d\n", skb->len);

		if (skb->len < pkt_status_sz) {
			WARN_ON(sys->prev_skb != NULL);
			IPADBG_LOW("status straddles buffer\n");
			if (!ipa3_ctx->lan_rx_napi_enable)
				sys->prev_skb = skb_copy(skb, GFP_KERNEL);
			else
				sys->prev_skb = skb_copy(skb, GFP_ATOMIC);
			sys->len_partial = skb->len;
			goto out;
		}

		ipahal_pkt_status_parse(skb->data, &status);
		IPADBG_LOW("STATUS opcode=%d src=%d dst=%d len=%d\n",
				status.status_opcode, status.endp_src_idx,
				status.endp_dest_idx, status.pkt_len);
		if (atomic_read(&ipa3_ctx->is_suspend_mode_enabled)) {
			atomic_set(&ipa3_ctx->is_suspend_mode_enabled, 0);
			type = ipa3_get_client_by_pipe(status.endp_src_idx);
			IPAERR("Client %s woke up the system\n", ipa_clients_strings[type]);
			trace_ipa_tx_dp(skb, sys->ep->client);
		}
		if (sys->status_stat) {
			sys->status_stat->status[sys->status_stat->curr] =
				status;
			sys->status_stat->curr++;
			if (sys->status_stat->curr == IPA_MAX_STATUS_STAT_NUM)
				sys->status_stat->curr = 0;
		}

		switch (status.status_opcode) {
		case IPAHAL_PKT_STATUS_OPCODE_DROPPED_PACKET:
		case IPAHAL_PKT_STATUS_OPCODE_PACKET:
		case IPAHAL_PKT_STATUS_OPCODE_SUSPENDED_PACKET:
		case IPAHAL_PKT_STATUS_OPCODE_PACKET_2ND_PASS:
		case IPAHAL_PKT_STATUS_OPCODE_DCMP:
			break;
		case IPAHAL_PKT_STATUS_OPCODE_NEW_FRAG_RULE:
			IPAERR_RL("Frag packets received on lan consumer\n");
			IPAERR_RL("STATUS opcode=%d src=%d dst=%d src ip=%x\n",
				status.status_opcode, status.endp_src_idx,
				status.endp_dest_idx, status.src_ip_addr);
			skb_pull(skb, pkt_status_sz);
			continue;
		default:
			IPAERR_RL("unsupported opcode(%d)\n",
				status.status_opcode);
			skb_pull(skb, pkt_status_sz);
			continue;
		}

		IPA_STATS_EXCP_CNT(status.exception,
				ipa3_ctx->stats.rx_excp_pkts);
		if (status.endp_dest_idx >= ipa3_ctx->ipa_num_pipes ||
			status.endp_src_idx >= ipa3_ctx->ipa_num_pipes) {
			IPAERR_RL("status fields invalid\n");
			IPAERR_RL("STATUS opcode=%d src=%d dst=%d len=%d\n",
				status.status_opcode, status.endp_src_idx,
				status.endp_dest_idx, status.pkt_len);
			WARN_ON(1);
			/* HW gave an unexpected status */
			ipa_assert();
		}
		if (IPAHAL_PKT_STATUS_MASK_FLAG_VAL(
			IPAHAL_PKT_STATUS_MASK_TAG_VALID_SHFT, &status)) {
			struct ipa3_tag_completion *comp;

			IPADBG_LOW("TAG packet arrived\n");
			if (status.tag_info == IPA_COOKIE) {
				skb_pull(skb, pkt_status_sz);
				if (skb->len < sizeof(comp)) {
					IPAERR("TAG arrived without packet\n");
					goto out;
				}
				memcpy(&comp, skb->data, sizeof(comp));
				skb_pull(skb, sizeof(comp));
				complete(&comp->comp);
				if (atomic_dec_return(&comp->cnt) == 0)
					kfree(comp);
				continue;
			} else {
				ptr = tag_to_pointer_wa(status.tag_info);
				tx_pkt = (struct ipa3_tx_pkt_wrapper *)ptr;
				IPADBG_LOW("tx_pkt recv = %pK\n", tx_pkt);
			}
		}
		if (status.pkt_len == 0) {
			IPADBG_LOW("Skip aggr close status\n");
			skb_pull(skb, pkt_status_sz);
			IPA_STATS_INC_CNT(ipa3_ctx->stats.aggr_close);
			IPA_STATS_DEC_CNT(ipa3_ctx->stats.rx_excp_pkts
				[IPAHAL_PKT_STATUS_EXCEPTION_NONE]);
			continue;
		}

		if (status.endp_dest_idx == (sys->ep - ipa3_ctx->ep)) {
			/* RX data */
			src_pipe = status.endp_src_idx;

			/*
			 * A packet which is received back to the AP after
			 * there was no route match.
			 */
			if (status.exception ==
				IPAHAL_PKT_STATUS_EXCEPTION_NONE &&
				ipahal_is_rule_miss_id(status.rt_rule_id))
				sys->drop_packet = true;

			if (skb->len == pkt_status_sz &&
				status.exception ==
				IPAHAL_PKT_STATUS_EXCEPTION_NONE) {
				WARN_ON(sys->prev_skb != NULL);
				IPADBG_LOW("Ins header in next buffer\n");
				if (!ipa3_ctx->lan_rx_napi_enable)
					sys->prev_skb = skb_copy(skb,
						GFP_KERNEL);
				else
					sys->prev_skb = skb_copy(skb,
						GFP_ATOMIC);
				sys->len_partial = skb->len;
				goto out;
			}

			/*
			 * Padding not needed for LAN coalescing pipe, hence we
			 * only pad when not LAN coalescing pipe.
			 */
			if (sys->ep->client != IPA_CLIENT_APPS_LAN_COAL_CONS)
				pad_len_byte = ((status.pkt_len + 3) & ~3) -
					status.pkt_len;
			len = status.pkt_len + pad_len_byte;
			IPADBG_LOW("pad %d pkt_len %d len %d\n", pad_len_byte,
					status.pkt_len, len);

			if (status.exception ==
					IPAHAL_PKT_STATUS_EXCEPTION_DEAGGR) {
				IPADBG_LOW(
					"Dropping packet on DeAggr Exception\n");
				sys->drop_packet = true;
			}

			skb2 = ipa3_skb_copy_for_client(skb,
				min(status.pkt_len + pkt_status_sz, skb->len));
			if (likely(skb2)) {
				if (skb->len < len + pkt_status_sz) {
					IPADBG_LOW("SPL skb len %d len %d\n",
							skb->len, len);
					sys->prev_skb = skb2;
					sys->len_rem = len - skb->len +
						pkt_status_sz;
					sys->len_pad = pad_len_byte;
					skb_pull(skb, skb->len);
				} else {
					skb_trim(skb2, status.pkt_len +
							pkt_status_sz);
					IPADBG_LOW("rx avail for %d\n",
							status.endp_dest_idx);
					if (sys->drop_packet) {
						dev_kfree_skb_any(skb2);
					} else if (status.pkt_len >
						   IPA_GENERIC_AGGR_BYTE_LIMIT *
						   1024) {
						IPAERR("packet size invalid\n");
						IPAERR("STATUS opcode=%d\n",
							status.status_opcode);
						IPAERR("src=%d dst=%d len=%d\n",
							status.endp_src_idx,
							status.endp_dest_idx,
							status.pkt_len);
						sys->drop_packet = true;
						dev_kfree_skb_any(skb2);
					} else {
						skb2->truesize = skb2->len +
						sizeof(struct sk_buff) +
						(ALIGN(len +
						pkt_status_sz, 32) *
						unused / used_align);
						sys->ep->client_notify(
							sys->ep->priv,
							IPA_RECEIVE,
							(unsigned long)(skb2));
					}
					skb_pull(skb, len + pkt_status_sz);
				}
			} else {
				IPAERR("fail to alloc skb\n");
				if (skb->len < len) {
					sys->prev_skb = NULL;
					sys->len_rem = len - skb->len +
						pkt_status_sz;
					sys->len_pad = pad_len_byte;
					skb_pull(skb, skb->len);
				} else {
					skb_pull(skb, len + pkt_status_sz);
				}
			}
			/* TX comp */
			ipa3_wq_write_done_status(src_pipe, tx_pkt);
			IPADBG_LOW("tx comp imp for %d\n", src_pipe);
		} else {
			/* TX comp */
			ipa3_wq_write_done_status(status.endp_src_idx, tx_pkt);
			IPADBG_LOW("tx comp exp for %d\n",
				status.endp_src_idx);
			skb_pull(skb, pkt_status_sz);
			IPA_STATS_INC_CNT(ipa3_ctx->stats.stat_compl);
			IPA_STATS_DEC_CNT(ipa3_ctx->stats.rx_excp_pkts
				[IPAHAL_PKT_STATUS_EXCEPTION_NONE]);
		}
		tx_pkt = NULL;
	}

out:
	ipa3_skb_recycle(skb);
	return 0;
}

static struct sk_buff *ipa3_join_prev_skb(struct sk_buff *prev_skb,
		struct sk_buff *skb, unsigned int len)
{
	struct sk_buff *skb2;

	skb2 = skb_copy_expand(prev_skb, 0,
			len, GFP_KERNEL);
	if (likely(skb2)) {
		memcpy(skb_put(skb2, len),
			skb->data, len);
	} else {
		IPAERR("copy expand failed\n");
		skb2 = NULL;
	}
	dev_kfree_skb_any(prev_skb);

	return skb2;
}

static void ipa3_wan_rx_handle_splt_pyld(struct sk_buff *skb,
		struct ipa3_sys_context *sys)
{
	struct sk_buff *skb2;

	IPADBG_LOW("rem %d skb %d\n", sys->len_rem, skb->len);
	if (sys->len_rem <= skb->len) {
		if (sys->prev_skb) {
			skb2 = ipa3_join_prev_skb(sys->prev_skb, skb,
					sys->len_rem);
			if (likely(skb2)) {
				IPADBG_LOW(
					"removing Status element from skb and sending to WAN client");
				skb_pull(skb2, ipahal_pkt_status_get_size());
				skb2->truesize = skb2->len +
					sizeof(struct sk_buff);
				sys->ep->client_notify(sys->ep->priv,
					IPA_RECEIVE,
					(unsigned long)(skb2));
			}
		}
		skb_pull(skb, sys->len_rem);
		sys->prev_skb = NULL;
		sys->len_rem = 0;
	} else {
		if (sys->prev_skb) {
			skb2 = ipa3_join_prev_skb(sys->prev_skb, skb,
					skb->len);
			sys->prev_skb = skb2;
		}
		sys->len_rem -= skb->len;
		skb_pull(skb, skb->len);
	}
}

static int ipa3_low_lat_rx_pyld_hdlr(struct sk_buff *skb,
		struct ipa3_sys_context *sys)
{
	if (skb->len == 0) {
		IPAERR("ZLT\n");
		goto bail;
	}

	IPA_DUMP_BUFF(skb->data, 0, skb->len);
	if (!sys->ep->client_notify) {
		IPAERR("client_notify is NULL");
		goto bail;
	}
	sys->ep->client_notify(sys->ep->priv,
		IPA_RECEIVE, (unsigned long)(skb));
	return 0;

bail:
	sys->free_skb(skb);
	return 0;
}

static int ipa3_wan_rx_pyld_hdlr(struct sk_buff *skb,
		struct ipa3_sys_context *sys)
{
	struct ipahal_pkt_status status;
	unsigned char *skb_data;
	u32 pkt_status_sz;
	struct sk_buff *skb2;
	u16 pkt_len_with_pad;
	u32 qmap_hdr;
	int checksum_trailer_exists;
	int frame_len;
	int ep_idx;
	unsigned int used = *(unsigned int *)skb->cb;
	unsigned int used_align = ALIGN(used, 32);
	unsigned long unused = IPA_GENERIC_RX_BUFF_BASE_SZ - used;

	IPA_DUMP_BUFF(skb->data, 0, skb->len);
	if (skb->len == 0) {
		IPAERR("ZLT\n");
		goto bail;
	}

	if (ipa3_ctx->ipa_client_apps_wan_cons_agg_gro) {
		sys->ep->client_notify(sys->ep->priv,
			IPA_RECEIVE, (unsigned long)(skb));
		return 0;
	}
	if (sys->repl_hdlr == ipa3_replenish_rx_cache_recycle) {
		IPAERR("Recycle should enable only with GRO Aggr\n");
		ipa_assert();
	}

	/*
	 * payload splits across 2 buff or more,
	 * take the start of the payload from prev_skb
	 */
	if (sys->len_rem)
		ipa3_wan_rx_handle_splt_pyld(skb, sys);

	pkt_status_sz = ipahal_pkt_status_get_size();
	while (skb->len) {
		IPADBG_LOW("LEN_REM %d\n", skb->len);
		if (skb->len < pkt_status_sz) {
			IPAERR("status straddles buffer\n");
			WARN_ON(1);
			goto bail;
		}
		ipahal_pkt_status_parse(skb->data, &status);
		skb_data = skb->data;
		IPADBG_LOW("STATUS opcode=%d src=%d dst=%d len=%d ttl_dec=%d\n",
			status.status_opcode, status.endp_src_idx, status.endp_dest_idx,
			status.pkt_len, status.ttl_dec);

		if (sys->status_stat) {
			sys->status_stat->status[sys->status_stat->curr] =
				status;
			sys->status_stat->curr++;
			if (sys->status_stat->curr == IPA_MAX_STATUS_STAT_NUM)
				sys->status_stat->curr = 0;
		}

		if ((status.status_opcode !=
			IPAHAL_PKT_STATUS_OPCODE_DROPPED_PACKET) &&
			(status.status_opcode !=
			IPAHAL_PKT_STATUS_OPCODE_PACKET) &&
			(status.status_opcode !=
			IPAHAL_PKT_STATUS_OPCODE_PACKET_2ND_PASS)) {
			IPAERR("unsupported opcode(%d)\n",
				status.status_opcode);
			skb_pull(skb, pkt_status_sz);
			continue;
		}

		IPA_STATS_INC_CNT(ipa3_ctx->stats.rx_pkts);
		if (status.ttl_dec)
			IPA_STATS_INC_CNT(ipa3_ctx->stats.ttl_cnt);
		if (status.endp_dest_idx >= ipa3_ctx->ipa_num_pipes ||
			status.endp_src_idx >= ipa3_ctx->ipa_num_pipes) {
			IPAERR("status fields invalid\n");
			WARN_ON(1);
			goto bail;
		}
		if (status.pkt_len == 0) {
			IPADBG_LOW("Skip aggr close status\n");
			skb_pull(skb, pkt_status_sz);
			IPA_STATS_DEC_CNT(ipa3_ctx->stats.rx_pkts);
			IPA_STATS_INC_CNT(ipa3_ctx->stats.wan_aggr_close);
			continue;
		}
		ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_WAN_CONS);
		if (status.endp_dest_idx != ep_idx) {
			IPAERR("expected endp_dest_idx %d received %d\n",
					ep_idx, status.endp_dest_idx);
			WARN_ON(1);
			goto bail;
		}
		/* RX data */
		if (skb->len == pkt_status_sz) {
			IPAERR("Ins header in next buffer\n");
			WARN_ON(1);
			goto bail;
		}
		qmap_hdr = *(u32 *)(skb_data + pkt_status_sz);
		/*
		 * Take the pkt_len_with_pad from the last 2 bytes of the QMAP
		 * header
		 */

		/*QMAP is BE: convert the pkt_len field from BE to LE*/
		pkt_len_with_pad = ntohs((qmap_hdr>>16) & 0xffff);
		IPADBG_LOW("pkt_len with pad %d\n", pkt_len_with_pad);
		/*get the CHECKSUM_PROCESS bit*/
		checksum_trailer_exists = IPAHAL_PKT_STATUS_MASK_FLAG_VAL(
			IPAHAL_PKT_STATUS_MASK_CKSUM_PROCESS_SHFT, &status);
		IPADBG_LOW("checksum_trailer_exists %d\n",
				checksum_trailer_exists);

		frame_len = pkt_status_sz + IPA_QMAP_HEADER_LENGTH +
			    pkt_len_with_pad;
		if (checksum_trailer_exists)
			frame_len += IPA_DL_CHECKSUM_LENGTH;
		IPADBG_LOW("frame_len %d\n", frame_len);

		skb2 = skb_clone(skb, GFP_KERNEL);
		if (likely(skb2)) {
			/*
			 * the len of actual data is smaller than expected
			 * payload split across 2 buff
			 */
			if (skb->len < frame_len) {
				IPADBG_LOW("SPL skb len %d len %d\n",
						skb->len, frame_len);
				sys->prev_skb = skb2;
				sys->len_rem = frame_len - skb->len;
				skb_pull(skb, skb->len);
			} else {
				skb_trim(skb2, frame_len);
				IPADBG_LOW("rx avail for %d\n",
						status.endp_dest_idx);
				IPADBG_LOW(
					"removing Status element from skb and sending to WAN client");
				skb_pull(skb2, pkt_status_sz);
				skb2->truesize = skb2->len +
					sizeof(struct sk_buff) +
					(ALIGN(frame_len, 32) *
					 unused / used_align);
				sys->ep->client_notify(sys->ep->priv,
					IPA_RECEIVE, (unsigned long)(skb2));
				skb_pull(skb, frame_len);
			}
		} else {
			IPAERR("fail to clone\n");
			if (skb->len < frame_len) {
				sys->prev_skb = NULL;
				sys->len_rem = frame_len - skb->len;
				skb_pull(skb, skb->len);
			} else {
				skb_pull(skb, frame_len);
			}
		}
	}
bail:
	sys->free_skb(skb);
	return 0;
}

static struct sk_buff *ipa3_get_skb_ipa_rx(unsigned int len, gfp_t flags)
{
	return __dev_alloc_skb(len, flags);
}

static void ipa_free_skb_rx(struct sk_buff *skb)
{
	dev_kfree_skb_any(skb);
}

void ipa3_lan_rx_cb(void *priv, enum ipa_dp_evt_type evt, unsigned long data)
{
	struct sk_buff *rx_skb = (struct sk_buff *)data;
	struct ipahal_pkt_status_thin status;
	struct ipa3_ep_context *ep;
	unsigned int src_pipe;
	u32 metadata;
	u8 ucp;
	void (*client_notify)(void *client_priv, enum ipa_dp_evt_type evt,
		       unsigned long data);
	void *client_priv;

	ipahal_pkt_status_parse_thin(rx_skb->data, &status);
	src_pipe = status.endp_src_idx;
	metadata = status.metadata;
	ucp = status.ucp;
	ep = &ipa3_ctx->ep[src_pipe];
	if (unlikely(src_pipe >= ipa3_ctx->ipa_num_pipes) ||
		unlikely(atomic_read(&ep->disconnect_in_progress))) {
		IPAERR("drop pipe=%d\n", src_pipe);
		dev_kfree_skb_any(rx_skb);
		return;
	}
	if (status.exception == IPAHAL_PKT_STATUS_EXCEPTION_NONE) {
		u32 extra = ( lan_coal_enabled() ) ? 0 : IPA_LAN_RX_HEADER_LENGTH;
		skb_pull(rx_skb, ipahal_pkt_status_get_size() + extra);
	}
	else
		skb_pull(rx_skb, ipahal_pkt_status_get_size());

	/* Metadata Info
	 *  ------------------------------------------
	 *  |   3     |   2     |    1        |  0   |
	 *  | fw_desc | vdev_id | qmap mux id | Resv |
	 *  ------------------------------------------
	 */
	*(u16 *)rx_skb->cb = ((metadata >> 16) & 0xFFFF);
	*(u8 *)(rx_skb->cb + 4) = ucp;
	IPADBG_LOW("meta_data: 0x%x cb: 0x%x\n",
			metadata, *(u32 *)rx_skb->cb);
	IPADBG_LOW("ucp: %d\n", *(u8 *)(rx_skb->cb + 4));

	spin_lock(&ipa3_ctx->disconnect_lock);
	if (likely((!atomic_read(&ep->disconnect_in_progress)) &&
				ep->valid && ep->client_notify)) {
		client_notify = ep->client_notify;
		client_priv = ep->priv;
		spin_unlock(&ipa3_ctx->disconnect_lock);
		client_notify(client_priv, IPA_RECEIVE,
				(unsigned long)(rx_skb));
	} else {
		spin_unlock(&ipa3_ctx->disconnect_lock);
		dev_kfree_skb_any(rx_skb);
	}

}

/*
 * The following will help us deduce the real size of an ipv6 header
 * that may or may not have extensions...
 */
static int _skip_ipv6_exthdr(
	u8     *hdr_ptr,
	int     start,
	u8     *nexthdrp,
	__be16 *fragp )
{
	u8 nexthdr = *nexthdrp;

	*fragp = 0;

	while ( ipv6_ext_hdr(nexthdr) ) {

		struct ipv6_opt_hdr *hp;

		int hdrlen;

		if (nexthdr == NEXTHDR_NONE)
			return -EINVAL;

		hp = (struct ipv6_opt_hdr*) (hdr_ptr + (u32) start);

		if (nexthdr == NEXTHDR_FRAGMENT) {

			u32 off = offsetof(struct frag_hdr, frag_off);

			__be16 *fp = (__be16*) (hdr_ptr + (u32)start + off);

			*fragp = *fp;

			if (ntohs(*fragp) & ~0x7)
				break;

			hdrlen = 8;

		} else if (nexthdr == NEXTHDR_AUTH) {

			hdrlen = ipv6_authlen(hp);

		} else {

			hdrlen = ipv6_optlen(hp);
		}

		nexthdr = hp->nexthdr;

		start += hdrlen;
	}

	*nexthdrp = nexthdr;

	return start;
}

/*
 * The following defines and structure used for calculating Ethernet
 * frame type and size...
 */
#define IPA_ETH_VLAN_2TAG 0x88A8
#define IPA_ETH_VLAN_TAG  0x8100
#define IPA_ETH_TAG_SZ    sizeof(u32)

/*
 * The following structure used for containing packet payload
 * information.
 */
typedef struct ipa_pkt_data_s {
	void* pkt;
	u32   pkt_len;
} ipa_pkt_data_t;

/*
 * The following structure used for consolidating all header
 * information.
 */
typedef struct ipa_header_data_s {
	struct ethhdr* eth_hdr;
	u32            eth_hdr_size;
	u8             ip_vers;
	void*          ip_hdr;
	u32            ip_hdr_size;
	u8             ip_proto;
	void*          proto_hdr;
	u32            proto_hdr_size;
	u32            aggr_hdr_len;
	u32            curr_seq;
} ipa_header_data_t;

static int
_calc_partial_csum(
	struct sk_buff*    skb,
	ipa_header_data_t* hdr_data,
	u32                aggr_payload_size )
{
	u32 ip_hdr_size;
	u32 proto_hdr_size;
	u8  ip_vers;
	u8  ip_proto;
	u8* new_ip_hdr;
	u8* new_proto_hdr;
	u32 len_for_calc;
	__sum16 pseudo;

	if ( !skb || !hdr_data ) {

		IPAERR(
			"NULL args: skb(%p) and/or hdr_data(%p)\n",
			skb, hdr_data);

		return -1;

	} else {

		ip_hdr_size    = hdr_data->ip_hdr_size;
		proto_hdr_size = hdr_data->proto_hdr_size;
		ip_vers        = hdr_data->ip_vers;
		ip_proto       = hdr_data->ip_proto;

		new_ip_hdr    = (u8*) skb->data + hdr_data->eth_hdr_size;

		new_proto_hdr = new_ip_hdr + ip_hdr_size;

		len_for_calc  = proto_hdr_size + aggr_payload_size;

		skb->ip_summed = CHECKSUM_PARTIAL;

		if ( ip_vers == 4 ) {

			struct iphdr* iph = (struct iphdr*) new_ip_hdr;

			iph->check = 0;
			iph->check = ip_fast_csum(iph, iph->ihl);

			pseudo = ~csum_tcpudp_magic(
				iph->saddr,
				iph->daddr,
				len_for_calc,
				ip_proto,
				0);

		} else { /* ( ip_vers == 6 ) */

			struct ipv6hdr* iph = (struct ipv6hdr*) new_ip_hdr;

			pseudo = ~csum_ipv6_magic(
				&iph->saddr,
				&iph->daddr,
				len_for_calc,
				ip_proto,
				0);
		}

		if ( ip_proto == IPPROTO_TCP ) {

			struct tcphdr* hdr = (struct tcphdr*) new_proto_hdr;

			hdr->check = pseudo;

			skb->csum_offset = offsetof(struct tcphdr, check);

		} else {

			struct udphdr* hdr = (struct udphdr*) new_proto_hdr;

			hdr->check = pseudo;

			skb->csum_offset = offsetof(struct udphdr, check);
		}
	}

	return 0;
}

/*
 * The following function takes the constituent parts of an Ethernet
 * and IP packet and creates an skb from them...
 */
static int
_prep_and_send_skb(
	struct sk_buff*         rx_skb,
	struct ipa3_ep_context* ep,
	u32                     metadata,
	u8                      ucp,
	ipa_header_data_t*      hdr_data,
	ipa_pkt_data_t*         pkts,
	u32                     num_pkts,
	u32                     aggr_payload_size,
	u8                      pkt_id,
	bool                    recalc_cksum )
{
	struct ethhdr* eth_hdr;
	u32            eth_hdr_size;
	u8             ip_vers;
	void*          ip_hdr;
	u32            ip_hdr_size;
	u8             ip_proto;
	void*          proto_hdr;
	u32            proto_hdr_size;
	u32            aggr_hdr_len;
	u32            i;

	void          *new_proto_hdr, *new_ip_hdr, *new_eth_hdr;

	struct skb_shared_info *shinfo;

	struct sk_buff *head_skb;

	void *client_priv;
	void (*client_notify)(
		void *client_priv,
		enum ipa_dp_evt_type evt,
		unsigned long data);

	client_notify = 0;

	spin_lock(&ipa3_ctx->disconnect_lock);
	if (ep->valid && ep->client_notify &&
		likely((!atomic_read(&ep->disconnect_in_progress)))) {

		client_notify = ep->client_notify;
		client_priv   = ep->priv;
	}
	spin_unlock(&ipa3_ctx->disconnect_lock);

	if ( client_notify ) {

		eth_hdr        = hdr_data->eth_hdr;
		eth_hdr_size   = hdr_data->eth_hdr_size;
		ip_vers        = hdr_data->ip_vers;
		ip_hdr         = hdr_data->ip_hdr;
		ip_hdr_size    = hdr_data->ip_hdr_size;
		ip_proto       = hdr_data->ip_proto;
		proto_hdr      = hdr_data->proto_hdr;
		proto_hdr_size = hdr_data->proto_hdr_size;
		aggr_hdr_len   = hdr_data->aggr_hdr_len;

		if ( rx_skb ) {

			head_skb = rx_skb;

			ipa3_ctx->stats.coal.coal_left_as_is++;

		} else {

			head_skb = alloc_skb(aggr_hdr_len + aggr_payload_size, GFP_ATOMIC);

			if ( unlikely(!head_skb) ) {
				IPAERR("skb alloc failure\n");
				return -1;
			}

			ipa3_ctx->stats.coal.coal_reconstructed++;

			head_skb->protocol = ip_proto;

			/*
			 * Copy MAC header into the skb...
			 */
			new_eth_hdr = skb_put_data(head_skb, eth_hdr, eth_hdr_size);

			skb_reset_mac_header(head_skb);

			/*
			 * Copy, and update, IP[4|6] header into the skb...
			 */
			new_ip_hdr = skb_put_data(head_skb, ip_hdr, ip_hdr_size);

			if ( ip_vers == 4 ) {

				struct iphdr* ip4h = new_ip_hdr;

				ip4h->id = htons(ntohs(ip4h->id) + pkt_id);

				ip4h->tot_len =
					htons(ip_hdr_size + proto_hdr_size + aggr_payload_size);

			} else {

				struct ipv6hdr* ip6h = new_ip_hdr;

				ip6h->payload_len =
					htons(proto_hdr_size + aggr_payload_size);
			}

			skb_reset_network_header(head_skb);

			/*
			 * Copy, and update, [TCP|UDP] header into the skb...
			 */
			new_proto_hdr = skb_put_data(head_skb, proto_hdr, proto_hdr_size);

			if ( ip_proto == IPPROTO_TCP ) {

				struct tcphdr* hdr = new_proto_hdr;

				hdr_data->curr_seq += (aggr_payload_size) ? aggr_payload_size : 1;

				hdr->seq = htonl(hdr_data->curr_seq);

			} else {

				struct udphdr* hdr = new_proto_hdr;

				u16 len = sizeof(struct udphdr) + aggr_payload_size;

				hdr->len = htons(len);
			}

			skb_reset_transport_header(head_skb);

			/*
			 * Now aggregate all the individual physical payloads into
			 * th eskb.
			 */
			for ( i = 0; i < num_pkts; i++ ) {
				skb_put_data(head_skb, pkts[i].pkt, pkts[i].pkt_len);
			}
		}

		/*
		 * Is a recalc of the various checksums in order?
		 */
		if ( recalc_cksum ) {
			_calc_partial_csum(head_skb, hdr_data, aggr_payload_size);
		}

		/*
		 * Let's add some resegmentation info into the head skb. The
		 * data will allow the stack to resegment the data...should it
		 * need to relative to MTU...
		 */
		shinfo = skb_shinfo(head_skb);

		shinfo->gso_segs = num_pkts;
		shinfo->gso_size = pkts[0].pkt_len;

		if (ip_proto == IPPROTO_TCP) {
			shinfo->gso_type = (ip_vers == 4) ? SKB_GSO_TCPV4 : SKB_GSO_TCPV6;
			ipa3_ctx->stats.coal.coal_tcp++;
			ipa3_ctx->stats.coal.coal_tcp_bytes += aggr_payload_size;
		} else {
			shinfo->gso_type = SKB_GSO_UDP_L4;
			ipa3_ctx->stats.coal.coal_udp++;
			ipa3_ctx->stats.coal.coal_udp_bytes += aggr_payload_size;
		}

		/*
		 * Send this new skb to the client...
		 */
		*(u16 *)head_skb->cb = ((metadata >> 16) & 0xFFFF);
		*(u8 *)(head_skb->cb + 4) = ucp;

		IPADBG_LOW("meta_data: 0x%x cb: 0x%x\n",
				   metadata, *(u32 *)head_skb->cb);
		IPADBG_LOW("ucp: %d\n", *(u8 *)(head_skb->cb + 4));

		client_notify(client_priv, IPA_RECEIVE, (unsigned long)(head_skb));
	}

	return 0;
}

/*
 * The following will process a coalesced LAN packet from the IPA...
 */
void ipa3_lan_coal_rx_cb(
	void                *priv,
	enum ipa_dp_evt_type evt,
	unsigned long        data)
{
	struct sk_buff *rx_skb = (struct sk_buff *) data;

	unsigned int                    src_pipe;
	u8                              ucp;
	u32                             metadata;

	struct ipahal_pkt_status_thin   status;
	struct ipa3_ep_context         *ep;

	u8*                             qmap_hdr_data_ptr;
	struct qmap_hdr_data            qmap_hdr;

	struct coal_packet_status_info *cpsi, *cpsi_orig;
	u8*                             stat_info_ptr;

	u32               pkt_status_sz = ipahal_pkt_status_get_size();

	u32               eth_hdr_size;
	u32               ip_hdr_size;
	u8                ip_vers, ip_proto;
	u32               proto_hdr_size;
	u32               cpsi_hdrs_size;
	u32               aggr_payload_size;

	u32               pkt_len;

	struct ethhdr*    eth_hdr;
	void*             ip_hdr;
	struct iphdr*     ip4h;
	struct ipv6hdr*   ip6h;
	void*             proto_hdr;
	u8*               pkt_data;
	bool              gro = true;
	bool              cksum_is_zero;
	ipa_header_data_t hdr_data;

	ipa_pkt_data_t    in_pkts[MAX_COAL_PACKETS];
	u32               in_pkts_sub;

	u8                tot_pkts;

	u32               i, j;

	u64               cksum_mask = 0;

	int               ret;

	IPA_DUMP_BUFF(skb->data, 0, skb->len);

	ipa3_ctx->stats.coal.coal_rx++;

	ipahal_pkt_status_parse_thin(rx_skb->data, &status);
	src_pipe = status.endp_src_idx;
	metadata = status.metadata;
	ucp = status.ucp;
	ep = &ipa3_ctx->ep[src_pipe];
	if (unlikely(src_pipe >= ipa3_ctx->ipa_num_pipes) ||
		unlikely(atomic_read(&ep->disconnect_in_progress))) {
		IPAERR("drop pipe=%d\n", src_pipe);
		goto process_done;
	}

	memset(&hdr_data, 0, sizeof(hdr_data));
	memset(&qmap_hdr, 0, sizeof(qmap_hdr));

	/*
	 * Let's get to, then parse, the qmap header...
	 */
	qmap_hdr_data_ptr = rx_skb->data + pkt_status_sz;

	ret = ipahal_qmap_parse(qmap_hdr_data_ptr, &qmap_hdr);

	if ( unlikely(ret) ) {
		IPAERR("ipahal_qmap_parse fail\n");
		ipa3_ctx->stats.coal.coal_hdr_qmap_err++;
		goto process_done;
	}

	if ( ! VALID_NLS(qmap_hdr.num_nlos) ) {
		IPAERR("Bad num_nlos(%u) value\n", qmap_hdr.num_nlos);
		ipa3_ctx->stats.coal.coal_hdr_nlo_err++;
		goto process_done;
	}

	stat_info_ptr = qmap_hdr_data_ptr + sizeof(union qmap_hdr_u);

	cpsi = cpsi_orig = (struct coal_packet_status_info*) stat_info_ptr;

	/*
	 * Reconstruct the 48 bits of checksum info. And count total
	 * packets as well...
	 */
	for (i = tot_pkts = 0;
		 i < MAX_COAL_PACKET_STATUS_INFO;
		 ++i, ++cpsi) {

		cpsi->pkt_len = ntohs(cpsi->pkt_len);

		cksum_mask |= ((u64) cpsi->pkt_cksum_errs) << (8 * i);

		if ( i < qmap_hdr.num_nlos ) {
			tot_pkts += cpsi->num_pkts;
		}
	}

	/*
	 * A bounds check.
	 *
	 * Technically, the hardware shouldn't give us a bad count, but
	 * just to be safe...
	 */
	if ( tot_pkts > MAX_COAL_PACKETS ) {
		IPAERR("tot_pkts(%u) > MAX_COAL_PACKETS(%u)\n",
			   tot_pkts, MAX_COAL_PACKETS);
		ipa3_ctx->stats.coal.coal_hdr_pkt_err++;
		goto process_done;
	}

	ipa3_ctx->stats.coal.coal_pkts += tot_pkts;

	/*
	 * Move along past the coal headers...
	 */
	cpsi_hdrs_size = MAX_COAL_PACKET_STATUS_INFO * sizeof(u32);

	pkt_data = stat_info_ptr + cpsi_hdrs_size;

	/*
	 * Let's processes the Ethernet header...
	 */
	eth_hdr = (struct ethhdr*) pkt_data;

	switch ( ntohs(eth_hdr->h_proto) )
	{
	case IPA_ETH_VLAN_2TAG:
		eth_hdr_size = sizeof(struct ethhdr) + (IPA_ETH_TAG_SZ * 2);
		break;
	case IPA_ETH_VLAN_TAG:
		eth_hdr_size = sizeof(struct ethhdr) + IPA_ETH_TAG_SZ;
		break;
	default:
		eth_hdr_size = sizeof(struct ethhdr);
		break;
	}

	/*
	 * Get to and process the ip header...
	 */
	ip_hdr = (u8*) eth_hdr + eth_hdr_size;

	/*
	 * Is it a IPv[4|6] header?
	 */
	if (((struct iphdr*) ip_hdr)->version == 4) {
		/*
		 * Eth frame is carrying ip v4 payload.
		 */
		ip_vers     = 4;
		ip4h        = (struct iphdr*) ip_hdr;
		ip_hdr_size = ip4h->ihl * sizeof(u32);
		ip_proto    = ip4h->protocol;

		/*
		 * Don't allow grouping of any packets with IP options
		 * (i.e. don't allow when ihl != 5)...
		 */
		gro = (ip4h->ihl == 5);

	} else if (((struct ipv6hdr*) ip_hdr)->version == 6) {
		/*
		 * Eth frame is carrying ip v6 payload.
		 */
		int hdr_size;
		__be16 frag_off;

		ip_vers     = 6;
		ip6h        = (struct ipv6hdr*) ip_hdr;
		ip_proto    = ip6h->nexthdr;

		/*
		 * If extension headers exist, we need to analyze/skip them,
		 * hence...
		 */
		hdr_size = _skip_ipv6_exthdr(
			(u8*) ip_hdr,
			sizeof(*ip6h),
			&ip_proto,
			&frag_off);

		/*
		 * If we run into a problem, or this has a fragmented header
		 * (which technically should not be possible if the HW works
		 * as intended), bail.
		 */
		if (hdr_size < 0 || frag_off) {
			IPAERR(
				"_skip_ipv6_exthdr() failed. Errored with hdr_size(%d) "
				"and/or frag_off(%d)\n",
				hdr_size,
				ntohs(frag_off));
			ipa3_ctx->stats.coal.coal_ip_invalid++;
			goto process_done;
		}

		ip_hdr_size = hdr_size;

		/*
		 * Don't allow grouping of any packets with IPv6 extension
		 * headers (i.e. don't allow when ip_hdr_size != basic v6
		 * header size).
		 */
		gro = (ip_hdr_size == sizeof(*ip6h));

	} else {

		IPAERR("Not a v4 or v6 header...can't process\n");
		ipa3_ctx->stats.coal.coal_ip_invalid++;
		goto process_done;
	}

	/*
	 * Get to and process the protocol header...
	 */
	proto_hdr = (u8*) ip_hdr + ip_hdr_size;

	if (ip_proto == IPPROTO_TCP) {

		struct tcphdr* hdr = (struct tcphdr*) proto_hdr;

		hdr_data.curr_seq = ntohl(hdr->seq);

		proto_hdr_size = hdr->doff * sizeof(u32);

		cksum_is_zero = false;

	} else if (ip_proto == IPPROTO_UDP) {

		proto_hdr_size = sizeof(struct udphdr);

		cksum_is_zero = (ip_vers == 4 && ((struct udphdr*) proto_hdr)->check == 0);

	} else {

		IPAERR("Not a TCP or UDP heqder...can't process\n");
		ipa3_ctx->stats.coal.coal_trans_invalid++;
		goto process_done;

	}

	/*
	 * The following will adjust the skb internals (ie. skb->data and
	 * skb->len), such that they're positioned, and reflect, the data
	 * starting at the ETH header...
	 */
	skb_pull(
		rx_skb,
		pkt_status_sz +
		sizeof(union qmap_hdr_u) +
		cpsi_hdrs_size);

	/*
	 * Consolidate all header, header type, and header size info...
	 */
	hdr_data.eth_hdr        = eth_hdr;
	hdr_data.eth_hdr_size   = eth_hdr_size;
	hdr_data.ip_vers        = ip_vers;
	hdr_data.ip_hdr         = ip_hdr;
	hdr_data.ip_hdr_size    = ip_hdr_size;
	hdr_data.ip_proto       = ip_proto;
	hdr_data.proto_hdr      = proto_hdr;
	hdr_data.proto_hdr_size = proto_hdr_size;
	hdr_data.aggr_hdr_len   = eth_hdr_size + ip_hdr_size + proto_hdr_size;

	if ( qmap_hdr.vcid < GSI_VEID_MAX ) {
		ipa3_ctx->stats.coal.coal_veid[qmap_hdr.vcid] += 1;
	}

	/*
	 * Quick check to see if we really need to go any further...
	 */
	if ( gro && qmap_hdr.num_nlos == 1 && qmap_hdr.chksum_valid ) {

		cpsi = cpsi_orig;

		in_pkts[0].pkt     = rx_skb->data  + hdr_data.aggr_hdr_len;
		in_pkts[0].pkt_len = cpsi->pkt_len - (ip_hdr_size + proto_hdr_size);

		in_pkts_sub = 1;

		aggr_payload_size = rx_skb->len - hdr_data.aggr_hdr_len;

		_prep_and_send_skb(
			rx_skb,
			ep, metadata, ucp,
			&hdr_data,
			in_pkts,
			in_pkts_sub,
			aggr_payload_size,
			tot_pkts,
			false);

		return;
	}

	/*
	 * Time to process packet payloads...
	 */
	pkt_data = (u8*) proto_hdr + proto_hdr_size;

	for ( i = tot_pkts = 0, cpsi = cpsi_orig;
		  i < qmap_hdr.num_nlos;
		  ++i, ++cpsi ) {

		aggr_payload_size = in_pkts_sub = 0;

		for ( j = 0;
			  j < cpsi->num_pkts;
			  j++, tot_pkts++, cksum_mask >>= 1 ) {

			bool csum_err = cksum_mask & 1;

			pkt_len = cpsi->pkt_len - (ip_hdr_size + proto_hdr_size);

			if ( csum_err || ! gro ) {

				if ( csum_err ) {
					ipa3_ctx->stats.coal.coal_csum_err++;
				}

				/*
				 * If there are previously queued packets, send them
				 * now...
				 */
				if ( in_pkts_sub ) {

					_prep_and_send_skb(
						NULL,
						ep, metadata, ucp,
						&hdr_data,
						in_pkts,
						in_pkts_sub,
						aggr_payload_size,
						tot_pkts,
						!cksum_is_zero);

					in_pkts_sub = aggr_payload_size = 0;
				}

				/*
				 * Now send the singleton...
				 */
				in_pkts[in_pkts_sub].pkt     = pkt_data;
				in_pkts[in_pkts_sub].pkt_len = pkt_len;

				aggr_payload_size += in_pkts[in_pkts_sub].pkt_len;
				pkt_data          += in_pkts[in_pkts_sub].pkt_len;

				in_pkts_sub++;

				_prep_and_send_skb(
					NULL,
					ep, metadata, ucp,
					&hdr_data,
					in_pkts,
					in_pkts_sub,
					aggr_payload_size,
					tot_pkts,
					(csum_err) ? false : !cksum_is_zero);

				in_pkts_sub = aggr_payload_size = 0;

				continue;
			}

			in_pkts[in_pkts_sub].pkt     = pkt_data;
			in_pkts[in_pkts_sub].pkt_len = pkt_len;

			aggr_payload_size += in_pkts[in_pkts_sub].pkt_len;
			pkt_data          += in_pkts[in_pkts_sub].pkt_len;

			in_pkts_sub++;
		}

		if ( in_pkts_sub ) {

			_prep_and_send_skb(
				NULL,
				ep, metadata, ucp,
				&hdr_data,
				in_pkts,
				in_pkts_sub,
				aggr_payload_size,
				tot_pkts,
				!cksum_is_zero);
		}
	}

process_done:
	/*
	 * One way or the other, we no longer need the skb, hence...
	 */
	dev_kfree_skb_any(rx_skb);
}

static void ipa3_recycle_rx_wrapper(struct ipa3_rx_pkt_wrapper *rx_pkt)
{
	rx_pkt->data.dma_addr = 0;
	/* skb recycle was moved to pyld_hdlr */
	INIT_LIST_HEAD(&rx_pkt->link);
	spin_lock_bh(&rx_pkt->sys->spinlock);
	list_add_tail(&rx_pkt->link, &rx_pkt->sys->rcycl_list);
	spin_unlock_bh(&rx_pkt->sys->spinlock);
}

static void ipa3_recycle_rx_page_wrapper(struct ipa3_rx_pkt_wrapper *rx_pkt)
{
	struct ipa_rx_page_data rx_page;

	rx_page = rx_pkt->page_data;

	/* Free rx_wrapper only for tmp alloc pages*/
	if (rx_page.is_tmp_alloc)
		kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rx_pkt);
}

/**
 * handle_skb_completion()- Handle event completion EOB or EOT and prep the skb
 *
 * if eob: Set skb values, put rx_pkt at the end of the list and return NULL
 *
 * if eot: Set skb values, put skb at the end of the list. Then update the
 * length and chain the skbs together while also freeing and unmapping the
 * corresponding rx pkt. Once finished return the head_skb to be sent up the
 * network stack.
 */
static struct sk_buff *handle_skb_completion(
	struct gsi_chan_xfer_notify *notify,
	bool                         update_truesize,
	struct ipa3_rx_pkt_wrapper **rx_pkt_ptr )
{
	struct ipa3_rx_pkt_wrapper *rx_pkt, *tmp;
	struct sk_buff *rx_skb, *next_skb = NULL;
	struct list_head *head;
	struct ipa3_sys_context *sys;

	sys = (struct ipa3_sys_context *) notify->chan_user_data;
	rx_pkt = (struct ipa3_rx_pkt_wrapper *) notify->xfer_user_data;

	if ( rx_pkt_ptr ) {
		*rx_pkt_ptr = rx_pkt;
	}

	spin_lock_bh(&rx_pkt->sys->spinlock);
	rx_pkt->sys->len--;
	spin_unlock_bh(&rx_pkt->sys->spinlock);

	if (notify->bytes_xfered)
		rx_pkt->len = notify->bytes_xfered;

	/*Drop packets when WAN consumer channel receive EOB event*/
	if ((notify->evt_id == GSI_CHAN_EVT_EOB ||
		sys->skip_eot) &&
		sys->ep->client == IPA_CLIENT_APPS_WAN_CONS) {
		dma_unmap_single(ipa3_ctx->pdev, rx_pkt->data.dma_addr,
			sys->rx_buff_sz, DMA_FROM_DEVICE);
		sys->free_skb(rx_pkt->data.skb);
		sys->free_rx_wrapper(rx_pkt);
		sys->eob_drop_cnt++;
		if (notify->evt_id == GSI_CHAN_EVT_EOB) {
			IPADBG("EOB event on WAN consumer channel, drop\n");
			sys->skip_eot = true;
		} else {
			IPADBG("Reset skip eot flag.\n");
			sys->skip_eot = false;
		}
		return NULL;
	}

	rx_skb = rx_pkt->data.skb;
	skb_set_tail_pointer(rx_skb, rx_pkt->len);
	rx_skb->len = rx_pkt->len;

	if (update_truesize) {
		*(unsigned int *)rx_skb->cb = rx_skb->len;
		rx_skb->truesize = rx_pkt->len + sizeof(struct sk_buff);
	}

	if (notify->veid >= GSI_VEID_MAX) {
		WARN_ON(1);
		return NULL;
	}

	head = &rx_pkt->sys->pending_pkts[notify->veid];

	INIT_LIST_HEAD(&rx_pkt->link);
	list_add_tail(&rx_pkt->link, head);

	/* Check added for handling LAN consumer packet without EOT flag */
	if (notify->evt_id == GSI_CHAN_EVT_EOT ||
		sys->ep->client == IPA_CLIENT_APPS_LAN_CONS ||
		sys->ep->client == IPA_CLIENT_APPS_LAN_COAL_CONS) {
		/* go over the list backward to save computations on updating length */
		list_for_each_entry_safe_reverse(rx_pkt, tmp, head, link) {
			rx_skb = rx_pkt->data.skb;

			list_del(&rx_pkt->link);
			dma_unmap_single(ipa3_ctx->pdev, rx_pkt->data.dma_addr,
				sys->rx_buff_sz, DMA_FROM_DEVICE);
			sys->free_rx_wrapper(rx_pkt);

			if (next_skb) {
				skb_shinfo(rx_skb)->frag_list = next_skb;
				rx_skb->len += next_skb->len;
				rx_skb->data_len += next_skb->len;
			}
			next_skb = rx_skb;
		}
	} else {
		return NULL;
	}
	return rx_skb;
}

/**
 * handle_page_completion()- Handle event completion EOB or EOT
 * and prep the skb
 *
 * if eob: Set skb values, put rx_pkt at the end of the list and return NULL
 *
 * if eot: Set skb values, put skb at the end of the list. Then update the
 * length and put the page together to the frags while also
 * freeing and unmapping the corresponding rx pkt. Once finished
 * return the head_skb to be sent up the network stack.
 */
static struct sk_buff *handle_page_completion(struct gsi_chan_xfer_notify
		*notify, bool update_truesize)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt, *tmp;
	struct sk_buff *rx_skb;
	struct list_head *head;
	struct ipa3_sys_context *sys;
	struct ipa_rx_page_data rx_page;
	int size;

	sys = (struct ipa3_sys_context *) notify->chan_user_data;
	rx_pkt = (struct ipa3_rx_pkt_wrapper *) notify->xfer_user_data;
	rx_page = rx_pkt->page_data;

	spin_lock_bh(&rx_pkt->sys->spinlock);
	rx_pkt->sys->len--;
	spin_unlock_bh(&rx_pkt->sys->spinlock);

	if (likely(notify->bytes_xfered))
		rx_pkt->data_len = notify->bytes_xfered;
	else {
		IPAERR_RL("unexpected 0 byte_xfered\n");
		rx_pkt->data_len = rx_pkt->len;
	}

	if (notify->veid >= GSI_VEID_MAX) {
		IPAERR("notify->veid > GSI_VEID_MAX\n");
		if (!rx_page.is_tmp_alloc) {
			init_page_count(rx_page.page);
			spin_lock_bh(&rx_pkt->sys->common_sys->spinlock);
			/* Add the element to head. */
			list_add(&rx_pkt->link,
				&rx_pkt->sys->page_recycle_repl->page_repl_head);
			spin_unlock_bh(&rx_pkt->sys->common_sys->spinlock);
		} else {
			dma_unmap_page(ipa3_ctx->pdev, rx_page.dma_addr,
					rx_pkt->len, DMA_FROM_DEVICE);
			__free_pages(rx_pkt->page_data.page, rx_pkt->page_data.page_order);
		}
		rx_pkt->sys->free_rx_wrapper(rx_pkt);
		IPA_STATS_INC_CNT(ipa3_ctx->stats.rx_page_drop_cnt);
		return NULL;
	}

	head = &rx_pkt->sys->pending_pkts[notify->veid];

	INIT_LIST_HEAD(&rx_pkt->link);
	list_add_tail(&rx_pkt->link, head);

	/* Check added for handling LAN consumer packet without EOT flag */
	if (notify->evt_id == GSI_CHAN_EVT_EOT ||
		sys->ep->client == IPA_CLIENT_APPS_LAN_CONS) {
		rx_skb = alloc_skb(0, GFP_ATOMIC);
		if (unlikely(!rx_skb)) {
			IPAERR("skb alloc failure, free all pending pages\n");
			list_for_each_entry_safe(rx_pkt, tmp, head, link) {
				rx_page = rx_pkt->page_data;
				size = rx_pkt->data_len;
				list_del_init(&rx_pkt->link);
				if (!rx_page.is_tmp_alloc) {
					init_page_count(rx_page.page);
					spin_lock_bh(&rx_pkt->sys->common_sys->spinlock);
					/* Add the element to head. */
					list_add(&rx_pkt->link,
						&rx_pkt->sys->page_recycle_repl->page_repl_head);
					spin_unlock_bh(&rx_pkt->sys->common_sys->spinlock);
				} else {
					dma_unmap_page(ipa3_ctx->pdev, rx_page.dma_addr,
						rx_pkt->len, DMA_FROM_DEVICE);
					__free_pages(rx_pkt->page_data.page, rx_pkt->page_data.page_order);
				}
				rx_pkt->sys->free_rx_wrapper(rx_pkt);
			}
			IPA_STATS_INC_CNT(ipa3_ctx->stats.rx_page_drop_cnt);
			return NULL;
		}
		list_for_each_entry_safe(rx_pkt, tmp, head, link) {
			rx_page = rx_pkt->page_data;
			size = rx_pkt->data_len;

			list_del_init(&rx_pkt->link);
			if (rx_page.is_tmp_alloc) {
				dma_unmap_page(ipa3_ctx->pdev, rx_page.dma_addr,
					rx_pkt->len, DMA_FROM_DEVICE);
			} else {
				spin_lock_bh(&rx_pkt->sys->common_sys->spinlock);
				/* Add the element back to tail. */
				list_add_tail(&rx_pkt->link,
					&rx_pkt->sys->page_recycle_repl->page_repl_head);
				spin_unlock_bh(&rx_pkt->sys->common_sys->spinlock);
				dma_sync_single_for_cpu(ipa3_ctx->pdev,
					rx_page.dma_addr,
					rx_pkt->len, DMA_FROM_DEVICE);
			}
			rx_pkt->sys->free_rx_wrapper(rx_pkt);

			skb_add_rx_frag(rx_skb,
				skb_shinfo(rx_skb)->nr_frags,
				rx_page.page, 0,
				size,
				PAGE_SIZE << rx_page.page_order);

			trace_handle_page_completion(rx_page.page,
				rx_skb, notify->bytes_xfered,
				rx_page.is_tmp_alloc, sys->ep->client);
		}
	} else {
		return NULL;
	}
	return rx_skb;
}

static void ipa3_wq_rx_common(
	struct ipa3_sys_context     *sys,
	struct gsi_chan_xfer_notify *notify)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	struct sk_buff             *rx_skb;

	if (!notify) {
		IPAERR_RL("gsi_chan_xfer_notify is null\n");
		return;
	}

	rx_skb = handle_skb_completion(notify, true, &rx_pkt);

	if (rx_skb) {
		rx_pkt->sys->pyld_hdlr(rx_skb, rx_pkt->sys);
		rx_pkt->sys->repl_hdlr(rx_pkt->sys);
	}
}

static void ipa3_rx_napi_chain(struct ipa3_sys_context *sys,
		struct gsi_chan_xfer_notify *notify, uint32_t num)
{
	struct ipa3_sys_context *wan_def_sys;
	int i, ipa_ep_idx;
	struct sk_buff *rx_skb, *first_skb = NULL, *prev_skb = NULL,
		*second_skb = NULL;

	/* non-coalescing case (SKB chaining enabled) */
	/* Chain is created as follows: first_skb->frag_list = second_skb
	 * After that the next skb's are added to second_skb->next .i.e
	 * first_skb->frag_list->next->next->next etc..*/
	if (sys->ep->client != IPA_CLIENT_APPS_WAN_COAL_CONS) {
		for (i = 0; i < num; i++) {
			if (!ipa3_ctx->ipa_wan_skb_page)
				rx_skb = handle_skb_completion(
					&notify[i], false, NULL);
			else
				rx_skb = handle_page_completion(
					&notify[i], false);

			/* this is always true for EOTs */
			if (rx_skb) {
				if (!first_skb) {
					first_skb = rx_skb;
				} else if (!second_skb) {
					second_skb = rx_skb;
					skb_shinfo(first_skb)->frag_list =
						second_skb;
				} else if (prev_skb) {
					prev_skb->next = rx_skb;
				}
				prev_skb = rx_skb;
				trace_ipa3_rx_napi_chain(first_skb,
							 prev_skb,
							 rx_skb);
			}
		}
		if (prev_skb) {
			skb_shinfo(prev_skb)->frag_list = NULL;
			sys->pyld_hdlr(first_skb, sys);
		}
	} else {
		if (!ipa3_ctx->ipa_wan_skb_page) {
			/* TODO: add chaining for coal case */
			for (i = 0; i < num; i++) {
				rx_skb = handle_skb_completion(
					&notify[i], false, NULL);
				if (rx_skb) {
					sys->pyld_hdlr(rx_skb, sys);
					/*
					 * For coalescing, we have 2 transfer
					 * rings to replenish
					 */
					ipa_ep_idx = ipa_get_ep_mapping(
						IPA_CLIENT_APPS_WAN_CONS);
					if (ipa_ep_idx ==
						IPA_EP_NOT_ALLOCATED) {
						IPAERR("Invalid client.\n");
						return;
					}
					wan_def_sys =
						ipa3_ctx->ep[ipa_ep_idx].sys;
					wan_def_sys->repl_hdlr(wan_def_sys);
					sys->repl_hdlr(sys);
				}
			}
		} else {
			for (i = 0; i < num; i++) {
				rx_skb = handle_page_completion(
					&notify[i], false);

				/* this is always true for EOTs */
				if (rx_skb) {
					if (!first_skb) {
						first_skb = rx_skb;
					} else if (!second_skb) {
						second_skb = rx_skb;
						skb_shinfo(first_skb)->frag_list =
							second_skb;
					} else if (prev_skb) {
						prev_skb->next = rx_skb;
					}
					prev_skb = rx_skb;
					trace_ipa3_rx_napi_chain(first_skb,
								 prev_skb,
								 rx_skb);
				}
			}
			if (prev_skb) {
				skb_shinfo(prev_skb)->frag_list = NULL;
				sys->pyld_hdlr(first_skb, sys);
			}
		}
	}
}

static void ipa3_wlan_wq_rx_common(struct ipa3_sys_context *sys,
	struct gsi_chan_xfer_notify *notify)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt_expected;
	struct sk_buff *rx_skb;

	rx_pkt_expected = (struct ipa3_rx_pkt_wrapper *) notify->xfer_user_data;

	sys->len--;

	if (notify->bytes_xfered)
		rx_pkt_expected->len = notify->bytes_xfered;

	rx_skb = rx_pkt_expected->data.skb;
	skb_set_tail_pointer(rx_skb, rx_pkt_expected->len);
	rx_skb->len = rx_pkt_expected->len;
	rx_skb->truesize = rx_pkt_expected->len + sizeof(struct sk_buff);
	sys->ep->wstats.tx_pkts_rcvd++;
	if (sys->len <= IPA_WLAN_RX_POOL_SZ_LOW_WM) {
		ipa_free_skb(&rx_pkt_expected->data);
		sys->ep->wstats.tx_pkts_dropped++;
	} else {
		sys->ep->wstats.tx_pkts_sent++;
		sys->ep->client_notify(sys->ep->priv, IPA_RECEIVE,
				(unsigned long)(&rx_pkt_expected->data));
	}
	ipa3_replenish_wlan_rx_cache(sys);
}

static void ipa3_dma_memcpy_notify(struct ipa3_sys_context *sys)
{
	IPADBG_LOW("ENTER.\n");
	if (unlikely(list_empty(&sys->head_desc_list))) {
		IPAERR("descriptor list is empty!\n");
		WARN_ON(1);
		return;
	}
	sys->ep->client_notify(sys->ep->priv, IPA_RECEIVE, 0);
	IPADBG_LOW("EXIT\n");
}

static void ipa3_wq_rx_avail(struct work_struct *work)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;
	struct ipa3_sys_context *sys;

	rx_pkt = container_of(work, struct ipa3_rx_pkt_wrapper, work);
	WARN(unlikely(rx_pkt == NULL), "rx pkt is null");
	sys = rx_pkt->sys;
	ipa3_wq_rx_common(sys, 0);
}

static int ipa3_odu_rx_pyld_hdlr(struct sk_buff *rx_skb,
	struct ipa3_sys_context *sys)
{
	if (sys->ep->client_notify) {
		sys->ep->client_notify(sys->ep->priv, IPA_RECEIVE,
			(unsigned long)(rx_skb));
	} else {
		dev_kfree_skb_any(rx_skb);
		WARN(1, "client notify is null");
	}

	return 0;
}

static int ipa3_odl_dpl_rx_pyld_hdlr(struct sk_buff *rx_skb,
	struct ipa3_sys_context *sys)
{
	if (WARN(!sys->ep->client_notify, "sys->ep->client_notify is NULL\n")) {
		dev_kfree_skb_any(rx_skb);
	} else {
		sys->ep->client_notify(sys->ep->priv, IPA_RECEIVE,
			(unsigned long)(rx_skb));
		/*Recycle the SKB before reusing it*/
		ipa3_skb_recycle(rx_skb);
	}

	return 0;
}
static void ipa3_free_rx_wrapper(struct ipa3_rx_pkt_wrapper *rk_pkt)
{
	kmem_cache_free(ipa3_ctx->rx_pkt_wrapper_cache, rk_pkt);
}

static void ipa3_set_aggr_limit(struct ipa_sys_connect_params *in,
		struct ipa3_sys_context *sys)
{
	u32 *aggr_byte_limit = &in->ipa_ep_cfg.aggr.aggr_byte_limit;
	u32 adjusted_sz;

	if (ipa3_ctx->ipa_wan_skb_page) {
		IPAERR("set rx_buff_sz config from netmngr %lu\n", (unsigned long)
			sys->buff_size);
		sys->rx_buff_sz = IPA_GENERIC_RX_BUFF_SZ(sys->buff_size);
		*aggr_byte_limit = IPA_ADJUST_AGGR_BYTE_LIMIT(*aggr_byte_limit);
	} else {
		adjusted_sz = ipa_adjust_ra_buff_base_sz(*aggr_byte_limit);
		IPAERR("get close-by %u\n", adjusted_sz);
		IPAERR("set default rx_buff_sz %lu\n", (unsigned long)
				IPA_GENERIC_RX_BUFF_SZ(adjusted_sz));
		sys->rx_buff_sz = IPA_GENERIC_RX_BUFF_SZ(adjusted_sz);
		*aggr_byte_limit = sys->rx_buff_sz < *aggr_byte_limit ?
		IPA_ADJUST_AGGR_BYTE_LIMIT(sys->rx_buff_sz) :
		IPA_ADJUST_AGGR_BYTE_LIMIT(*aggr_byte_limit);
	}

	/* disable ipa_status */
	sys->ep->status.status_en = false;

	if (in->client == IPA_CLIENT_APPS_WAN_COAL_CONS ||
		(in->client == IPA_CLIENT_APPS_WAN_CONS &&
			ipa3_ctx->ipa_hw_type <= IPA_HW_v4_2))
		in->ipa_ep_cfg.aggr.aggr_hard_byte_limit_en = 1;

	IPADBG("set aggr_limit %lu\n", (unsigned long) *aggr_byte_limit);
}

static int ipa3_assign_policy(struct ipa_sys_connect_params *in,
		struct ipa3_sys_context *sys)
{
	bool apps_wan_cons_agg_gro_flag;
	unsigned long aggr_byte_limit;

	if (in->client == IPA_CLIENT_APPS_CMD_PROD ||
		in->client == IPA_CLIENT_APPS_WAN_LOW_LAT_PROD) {
		sys->policy = IPA_POLICY_INTR_MODE;
		sys->use_comm_evt_ring = false;
		return 0;
	}

	if (in->client == IPA_CLIENT_APPS_WAN_PROD ||
		in->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_PROD) {
		sys->policy = IPA_POLICY_INTR_MODE;
		if (ipa3_ctx->ipa_hw_type >= IPA_HW_v5_0)
			sys->use_comm_evt_ring = false;
		else
			sys->use_comm_evt_ring = true;
		INIT_WORK(&sys->work, ipa3_send_nop_desc);
		atomic_set(&sys->workqueue_flushed, 0);

		/*
		 * enable source notification status for exception packets
		 * (i.e. QMAP commands) to be routed to modem.
		 */
		sys->ep->status.status_en = true;
		sys->ep->status.status_ep =
			ipa_get_ep_mapping(IPA_CLIENT_Q6_WAN_CONS);
		/* Enable status supression to disable sending status for
		 * every packet.
		 */
		sys->ep->status.status_pkt_suppress = true;
		return 0;
	}

	if (IPA_CLIENT_IS_MEMCPY_DMA_PROD(in->client)) {
		sys->policy = IPA_POLICY_NOINTR_MODE;
		return 0;
	}

	apps_wan_cons_agg_gro_flag =
		ipa3_ctx->ipa_client_apps_wan_cons_agg_gro;
	aggr_byte_limit = in->ipa_ep_cfg.aggr.aggr_byte_limit;

	if (IPA_CLIENT_IS_PROD(in->client)) {
		if (sys->ep->skip_ep_cfg) {
			sys->policy = IPA_POLICY_INTR_POLL_MODE;
			sys->use_comm_evt_ring = true;
			atomic_set(&sys->curr_polling_state, 0);
		} else {
			sys->policy = IPA_POLICY_INTR_MODE;
			sys->use_comm_evt_ring = true;
			INIT_WORK(&sys->work, ipa3_send_nop_desc);
			atomic_set(&sys->workqueue_flushed, 0);
		}
	} else {
		if (IPA_CLIENT_IS_LAN_CONS(in->client) ||
		    IPA_CLIENT_IS_WAN_CONS(in->client) ||
		    in->client == IPA_CLIENT_APPS_WAN_LOW_LAT_CONS ||
		    in->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS) {
			sys->ep->status.status_en = true;
			sys->policy = IPA_POLICY_INTR_POLL_MODE;
			INIT_WORK(&sys->work, ipa3_wq_handle_rx);
			INIT_DELAYED_WORK(&sys->switch_to_intr_work,
				ipa3_switch_to_intr_rx_work_func);
			INIT_DELAYED_WORK(&sys->replenish_rx_work,
					ipa3_replenish_rx_work_func);
			atomic_set(&sys->curr_polling_state, 0);
			sys->rx_buff_sz = IPA_GENERIC_RX_BUFF_SZ(
				IPA_GENERIC_RX_BUFF_BASE_SZ);
			sys->get_skb = ipa3_get_skb_ipa_rx;
			sys->free_skb = ipa_free_skb_rx;
			if (IPA_CLIENT_IS_APPS_COAL_CONS(in->client))
				in->ipa_ep_cfg.aggr.aggr = IPA_COALESCE;
			else
				in->ipa_ep_cfg.aggr.aggr = IPA_GENERIC;
			if (IPA_CLIENT_IS_LAN_CONS(in->client)) {
				INIT_WORK(&sys->repl_work, ipa3_wq_repl_rx);
				sys->pyld_hdlr = ipa3_lan_rx_pyld_hdlr;
				sys->repl_hdlr =
					ipa3_replenish_rx_cache_recycle;
				sys->free_rx_wrapper =
					ipa3_recycle_rx_wrapper;
				sys->rx_pool_sz =
					ipa3_ctx->lan_rx_ring_size;
				in->ipa_ep_cfg.aggr.aggr_en = IPA_ENABLE_AGGR;
				in->ipa_ep_cfg.aggr.aggr_byte_limit =
				IPA_GENERIC_AGGR_BYTE_LIMIT;
				in->ipa_ep_cfg.aggr.aggr_pkt_limit =
				IPA_GENERIC_AGGR_PKT_LIMIT;
				in->ipa_ep_cfg.aggr.aggr_time_limit =
					IPA_GENERIC_AGGR_TIME_LIMIT;
				if (in->client == IPA_CLIENT_APPS_LAN_COAL_CONS) {
					in->ipa_ep_cfg.aggr.aggr_coal_l2 = true;
					in->ipa_ep_cfg.aggr.aggr_hard_byte_limit_en = 1;
				}
			} else if (IPA_CLIENT_IS_WAN_CONS(in->client) ||
				in->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS) {
				in->ipa_ep_cfg.aggr.aggr_en = IPA_ENABLE_AGGR;
				if (!in->ext_ioctl_v2)
					in->ipa_ep_cfg.aggr.aggr_time_limit =
						IPA_GENERIC_AGGR_TIME_LIMIT;
				if ((ipa3_ctx->ipa_wan_skb_page
					&& in->napi_obj) ||
					in->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS) {
					INIT_WORK(&sys->repl_work,
							ipa3_wq_page_repl);
					sys->pyld_hdlr = ipa3_wan_rx_pyld_hdlr;
					sys->free_rx_wrapper =
						ipa3_recycle_rx_page_wrapper;
					sys->repl_hdlr =
						ipa3_replenish_rx_page_recycle;
					sys->rx_pool_sz =
						ipa3_ctx->wan_rx_ring_size;
				} else {
					INIT_WORK(&sys->repl_work,
						ipa3_wq_repl_rx);
					sys->pyld_hdlr = ipa3_wan_rx_pyld_hdlr;
					sys->free_rx_wrapper =
						ipa3_free_rx_wrapper;
					sys->rx_pool_sz =
						ipa3_ctx->wan_rx_ring_size;
					if (nr_cpu_ids > 1) {
						sys->repl_hdlr =
						ipa3_fast_replenish_rx_cache;
					} else {
						sys->repl_hdlr =
						ipa3_replenish_rx_cache;
					}
					if (in->napi_obj && in->recycle_enabled)
						sys->repl_hdlr =
						ipa3_replenish_rx_cache_recycle;
				}
				in->ipa_ep_cfg.aggr.aggr_sw_eof_active
						= true;
				if (apps_wan_cons_agg_gro_flag)
					ipa3_set_aggr_limit(in, sys);
				else {
					in->ipa_ep_cfg.aggr.aggr_byte_limit
						= IPA_GENERIC_AGGR_BYTE_LIMIT;
					in->ipa_ep_cfg.aggr.aggr_pkt_limit
						= IPA_GENERIC_AGGR_PKT_LIMIT;
				}
			} else if (in->client ==
				IPA_CLIENT_APPS_WAN_LOW_LAT_CONS) {
				INIT_WORK(&sys->repl_work, ipa3_wq_repl_rx);
				sys->ep->status.status_en = false;
				sys->rx_buff_sz = IPA_GENERIC_RX_BUFF_SZ(
					IPA_QMAP_RX_BUFF_BASE_SZ);
				sys->pyld_hdlr = ipa3_low_lat_rx_pyld_hdlr;
				sys->repl_hdlr =
					ipa3_fast_replenish_rx_cache;
				sys->free_rx_wrapper =
					ipa3_free_rx_wrapper;
				sys->rx_pool_sz =
					ipa3_ctx->wan_rx_ring_size;
			}
		} else if (IPA_CLIENT_IS_WLAN_CONS(in->client)) {
			IPADBG("assigning policy to client:%d",
				in->client);

			sys->policy = IPA_POLICY_INTR_POLL_MODE;
			INIT_WORK(&sys->work, ipa3_wq_handle_rx);
			INIT_DELAYED_WORK(&sys->switch_to_intr_work,
				ipa3_switch_to_intr_rx_work_func);
			INIT_DELAYED_WORK(&sys->replenish_rx_work,
				ipa3_replenish_rx_work_func);
			atomic_set(&sys->curr_polling_state, 0);
			sys->rx_buff_sz = IPA_WLAN_RX_BUFF_SZ;
			sys->rx_pool_sz = in->desc_fifo_sz /
				IPA_FIFO_ELEMENT_SIZE - 1;
			if (sys->rx_pool_sz > IPA_WLAN_RX_POOL_SZ)
				sys->rx_pool_sz = IPA_WLAN_RX_POOL_SZ;
			sys->pyld_hdlr = NULL;
			sys->repl_hdlr = ipa3_replenish_wlan_rx_cache;
			sys->get_skb = ipa3_get_skb_ipa_rx;
			sys->free_skb = ipa_free_skb_rx;
			sys->free_rx_wrapper = ipa3_free_rx_wrapper;
			in->ipa_ep_cfg.aggr.aggr_en = IPA_BYPASS_AGGR;
		} else if (IPA_CLIENT_IS_ODU_CONS(in->client)) {
			IPADBG("assigning policy to client:%d",
				in->client);

			sys->policy = IPA_POLICY_INTR_POLL_MODE;
			INIT_WORK(&sys->work, ipa3_wq_handle_rx);
			INIT_DELAYED_WORK(&sys->switch_to_intr_work,
				ipa3_switch_to_intr_rx_work_func);
			INIT_DELAYED_WORK(&sys->replenish_rx_work,
				ipa3_replenish_rx_work_func);
			atomic_set(&sys->curr_polling_state, 0);
			sys->rx_pool_sz = in->desc_fifo_sz /
				IPA_FIFO_ELEMENT_SIZE - 1;
			if (sys->rx_pool_sz > IPA_ODU_RX_POOL_SZ)
				sys->rx_pool_sz = IPA_ODU_RX_POOL_SZ;
			sys->pyld_hdlr = ipa3_odu_rx_pyld_hdlr;
			sys->get_skb = ipa3_get_skb_ipa_rx;
			sys->free_skb = ipa_free_skb_rx;
			/* recycle skb for GSB use case */
			if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
				sys->free_rx_wrapper =
					ipa3_free_rx_wrapper;
				sys->repl_hdlr =
					ipa3_replenish_rx_cache;
				/* Overwrite buffer size & aggr limit for GSB */
				sys->rx_buff_sz = IPA_GENERIC_RX_BUFF_SZ(
					IPA_GSB_RX_BUFF_BASE_SZ);
				in->ipa_ep_cfg.aggr.aggr_byte_limit =
					IPA_GSB_AGGR_BYTE_LIMIT;
			} else {
				sys->free_rx_wrapper =
					ipa3_free_rx_wrapper;
				sys->repl_hdlr = ipa3_replenish_rx_cache;
				sys->rx_buff_sz = IPA_ODU_RX_BUFF_SZ;
			}
		} else if (in->client ==
				IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS) {
			IPADBG("assigning policy to client:%d",
				in->client);

			sys->policy = IPA_POLICY_INTR_POLL_MODE;
			INIT_WORK(&sys->work, ipa3_wq_handle_rx);
			INIT_DELAYED_WORK(&sys->switch_to_intr_work,
				ipa3_switch_to_intr_rx_work_func);
		} else if (in->client ==
				IPA_CLIENT_MEMCPY_DMA_SYNC_CONS) {
			IPADBG("assigning policy to client:%d",
				in->client);

			sys->policy = IPA_POLICY_NOINTR_MODE;
		}  else if (in->client == IPA_CLIENT_ODL_DPL_CONS) {
			IPADBG("assigning policy to ODL client:%d\n",
				in->client);
			/* Status enabling is needed for DPLv2 with
			 * IPA versions < 4.5.
			 * Dont enable ipa_status for APQ, since MDM IPA
			 * has IPA >= 4.5 with DPLv3.
			 */
			if ((ipa3_ctx->platform_type == IPA_PLAT_TYPE_APQ &&
				ipa3_is_mhip_offload_enabled()) ||
				(ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5))
					sys->ep->status.status_en = false;
			else
				sys->ep->status.status_en = true;
			sys->policy = IPA_POLICY_INTR_POLL_MODE;
			INIT_WORK(&sys->work, ipa3_wq_handle_rx);
			INIT_DELAYED_WORK(&sys->switch_to_intr_work,
				ipa3_switch_to_intr_rx_work_func);
			INIT_DELAYED_WORK(&sys->replenish_rx_work,
				ipa3_replenish_rx_work_func);
			atomic_set(&sys->curr_polling_state, 0);
			sys->rx_buff_sz =
				IPA_GENERIC_RX_BUFF_SZ(IPA_ODL_RX_BUFF_SZ);
			sys->pyld_hdlr = ipa3_odl_dpl_rx_pyld_hdlr;
			sys->get_skb = ipa3_get_skb_ipa_rx;
			sys->free_skb = ipa_free_skb_rx;
			sys->free_rx_wrapper = ipa3_recycle_rx_wrapper;
			sys->repl_hdlr = ipa3_replenish_rx_cache_recycle;
			sys->rx_pool_sz = in->desc_fifo_sz /
					IPA_FIFO_ELEMENT_SIZE - 1;
		} else {
			WARN(1, "Need to install a RX pipe hdlr\n");
			return -EINVAL;
		}
	}

	return 0;
}

/**
 * ipa3_tx_client_rx_notify_release() - Callback function
 * which will call the user supplied callback function to
 * release the skb, or release it on its own if no callback
 * function was supplied
 *
 * @user1: [in] - Data Descriptor
 * @user2: [in] - endpoint idx
 *
 * This notified callback is for the destination client
 * This function is supplied in ipa3_tx_dp_mul
 */
static void ipa3_tx_client_rx_notify_release(void *user1, int user2)
{
	struct ipa_tx_data_desc *dd = (struct ipa_tx_data_desc *)user1;
	int ep_idx = user2;

	IPADBG_LOW("Received data desc anchor:%pK\n", dd);

	atomic_inc(&ipa3_ctx->ep[ep_idx].avail_fifo_desc);
	ipa3_ctx->ep[ep_idx].wstats.rx_pkts_status_rcvd++;

  /* wlan host driver waits till tx complete before unload */
	IPADBG_LOW("ep=%d fifo_desc_free_count=%d\n",
		ep_idx, atomic_read(&ipa3_ctx->ep[ep_idx].avail_fifo_desc));
	IPADBG_LOW("calling client notify callback with priv:%pK\n",
		ipa3_ctx->ep[ep_idx].priv);

	if (ipa3_ctx->ep[ep_idx].client_notify) {
		ipa3_ctx->ep[ep_idx].client_notify(ipa3_ctx->ep[ep_idx].priv,
				IPA_WRITE_DONE, (unsigned long)user1);
		ipa3_ctx->ep[ep_idx].wstats.rx_hd_reply++;
	}
}
/**
 * ipa3_tx_client_rx_pkt_status() - Callback function
 * which will call the user supplied callback function to
 * increase the available fifo descriptor
 *
 * @user1: [in] - Data Descriptor
 * @user2: [in] - endpoint idx
 *
 * This notified callback is for the destination client
 * This function is supplied in ipa3_tx_dp_mul
 */
static void ipa3_tx_client_rx_pkt_status(void *user1, int user2)
{
	int ep_idx = user2;

	atomic_inc(&ipa3_ctx->ep[ep_idx].avail_fifo_desc);
	ipa3_ctx->ep[ep_idx].wstats.rx_pkts_status_rcvd++;
}


/**
 * ipa3_tx_dp_mul() - Data-path tx handler for multiple packets
 * @src: [in] - Client that is sending data
 * @ipa_tx_data_desc:	[in] data descriptors from wlan
 *
 * this is used for to transfer data descriptors that received
 * from WLAN1_PROD pipe to IPA HW
 *
 * The function will send data descriptors from WLAN1_PROD (one
 * at a time). Will set EOT flag for last descriptor Once this send was done
 * from transport point-of-view the IPA driver will get notified by the
 * supplied callback - ipa_gsi_irq_tx_notify_cb()
 *
 * ipa_gsi_irq_tx_notify_cb will call to the user supplied callback
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_tx_dp_mul(enum ipa_client_type src,
			struct ipa_tx_data_desc *data_desc)
{
	/* The second byte in wlan header holds qmap id */
#define IPA_WLAN_HDR_QMAP_ID_OFFSET 1
	struct ipa_tx_data_desc *entry;
	struct ipa3_sys_context *sys;
	struct ipa3_desc desc[2];
	u32 num_desc, cnt;
	int ep_idx;

	IPADBG_LOW("Received data desc anchor:%pK\n", data_desc);

	spin_lock_bh(&ipa3_ctx->wc_memb.ipa_tx_mul_spinlock);

	ep_idx = ipa_get_ep_mapping(src);
	if (unlikely(ep_idx == -1)) {
		IPAERR("dest EP does not exist.\n");
		goto fail_send;
	}
	IPADBG_LOW("ep idx:%d\n", ep_idx);
	sys = ipa3_ctx->ep[ep_idx].sys;

	if (unlikely(ipa3_ctx->ep[ep_idx].valid == 0)) {
		IPAERR("dest EP not valid.\n");
		goto fail_send;
	}
	sys->ep->wstats.rx_hd_rcvd++;

	/* Calculate the number of descriptors */
	num_desc = 0;
	list_for_each_entry(entry, &data_desc->link, link) {
		num_desc++;
	}
	IPADBG_LOW("Number of Data Descriptors:%d", num_desc);

	if (atomic_read(&sys->ep->avail_fifo_desc) < num_desc) {
		IPAERR("Insufficient data descriptors available\n");
		goto fail_send;
	}

	/* Assign callback only for last data descriptor */
	cnt = 0;
	list_for_each_entry(entry, &data_desc->link, link) {
		memset(desc, 0, 2 * sizeof(struct ipa3_desc));

		IPADBG_LOW("Parsing data desc :%d\n", cnt);
		cnt++;
		((u8 *)entry->pyld_buffer)[IPA_WLAN_HDR_QMAP_ID_OFFSET] =
			(u8)sys->ep->cfg.meta.qmap_id;

		/* the tag field will be populated in ipa3_send() function */
		desc[0].is_tag_status = true;
		desc[1].pyld = entry->pyld_buffer;
		desc[1].len = entry->pyld_len;
		desc[1].type = IPA_DATA_DESC_SKB;
		desc[1].user1 = data_desc;
		desc[1].user2 = ep_idx;
		IPADBG_LOW("priv:%pK pyld_buf:0x%pK pyld_len:%d\n",
			entry->priv, desc[1].pyld, desc[1].len);

		/* In case of last descriptor populate callback */
		if (cnt == num_desc) {
			IPADBG_LOW("data desc:%pK\n", data_desc);
			desc[1].callback = ipa3_tx_client_rx_notify_release;
		} else {
			desc[1].callback = ipa3_tx_client_rx_pkt_status;
		}

		IPADBG_LOW("calling ipa3_send()\n");
		if (ipa3_send(sys, 2, desc, true)) {
			IPAERR_RL("fail to send skb\n");
			sys->ep->wstats.rx_pkt_leak += (cnt-1);
			sys->ep->wstats.rx_dp_fail++;
			goto fail_send;
		}

		if (atomic_read(&sys->ep->avail_fifo_desc) >= 0)
			atomic_dec(&sys->ep->avail_fifo_desc);

		sys->ep->wstats.rx_pkts_rcvd++;
		IPADBG_LOW("ep=%d fifo desc=%d\n",
			ep_idx, atomic_read(&sys->ep->avail_fifo_desc));
	}

	sys->ep->wstats.rx_hd_processed++;
	spin_unlock_bh(&ipa3_ctx->wc_memb.ipa_tx_mul_spinlock);
	return 0;

fail_send:
	spin_unlock_bh(&ipa3_ctx->wc_memb.ipa_tx_mul_spinlock);
	return -EFAULT;

}

void ipa_free_skb(struct ipa_rx_data *data)
{
	struct ipa3_rx_pkt_wrapper *rx_pkt;

	spin_lock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);

	ipa3_ctx->wc_memb.total_tx_pkts_freed++;
	rx_pkt = container_of(data, struct ipa3_rx_pkt_wrapper, data);

	ipa3_skb_recycle(rx_pkt->data.skb);
	(void)skb_put(rx_pkt->data.skb, IPA_WLAN_RX_BUFF_SZ);

	list_add_tail(&rx_pkt->link,
		&ipa3_ctx->wc_memb.wlan_comm_desc_list);
	ipa3_ctx->wc_memb.wlan_comm_free_cnt++;

	spin_unlock_bh(&ipa3_ctx->wc_memb.wlan_spinlock);
}
EXPORT_SYMBOL(ipa_free_skb);

/* Functions added to support kernel tests */

int ipa3_sys_setup(struct ipa_sys_connect_params *sys_in,
			unsigned long *ipa_transport_hdl,
			u32 *ipa_pipe_num, u32 *clnt_hdl, bool en_status)
{
	struct ipa3_ep_context *ep;
	int ipa_ep_idx;
	int result = -EINVAL;

	if (sys_in == NULL || clnt_hdl == NULL) {
		IPAERR("NULL args\n");
		goto fail_gen;
	}

	if (ipa_transport_hdl == NULL || ipa_pipe_num == NULL) {
		IPAERR("NULL args\n");
		goto fail_gen;
	}
	if (sys_in->client >= IPA_CLIENT_MAX) {
		IPAERR("bad parm client:%d\n", sys_in->client);
		goto fail_gen;
	}

	ipa_ep_idx = ipa_get_ep_mapping(sys_in->client);
	if (ipa_ep_idx == -1) {
		IPAERR("Invalid client :%d\n", sys_in->client);
		goto fail_gen;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];
	IPA_ACTIVE_CLIENTS_INC_EP(sys_in->client);

	if (ep->valid == 1) {
		if (sys_in->client != IPA_CLIENT_APPS_WAN_PROD) {
			IPAERR("EP %d already allocated\n", ipa_ep_idx);
			goto fail_and_disable_clocks;
		} else {
			if (ipa3_cfg_ep_hdr(ipa_ep_idx,
						&sys_in->ipa_ep_cfg.hdr)) {
				IPAERR("fail to configure hdr prop of EP %d\n",
						ipa_ep_idx);
				result = -EFAULT;
				goto fail_and_disable_clocks;
			}
			if (ipa3_cfg_ep_hdr_ext(ipa_ep_idx,
						&sys_in->ipa_ep_cfg.hdr_ext)) {
				IPAERR("fail config hdr_ext prop of EP %d\n",
						ipa_ep_idx);
				result = -EFAULT;
				goto fail_and_disable_clocks;
			}
			if (ipa3_cfg_ep_cfg(ipa_ep_idx,
						&sys_in->ipa_ep_cfg.cfg)) {
				IPAERR("fail to configure cfg prop of EP %d\n",
						ipa_ep_idx);
				result = -EFAULT;
				goto fail_and_disable_clocks;
			}
			IPAERR("client %d (ep: %d) overlay ok sys=%pK\n",
					sys_in->client, ipa_ep_idx, ep->sys);
			ep->client_notify = sys_in->notify;
			ep->priv = sys_in->priv;
			*clnt_hdl = ipa_ep_idx;
			if (!ep->keep_ipa_awake)
				IPA_ACTIVE_CLIENTS_DEC_EP(sys_in->client);

			return 0;
		}
	}

	memset(ep, 0, offsetof(struct ipa3_ep_context, sys));

	ep->valid = 1;
	ep->client = sys_in->client;
	ep->client_notify = sys_in->notify;
	ep->priv = sys_in->priv;
	ep->keep_ipa_awake = true;
	if (en_status) {
		ep->status.status_en = true;
		ep->status.status_ep = ipa_ep_idx;
	}

	result = ipa3_enable_data_path(ipa_ep_idx);
	if (result) {
		IPAERR("enable data path failed res=%d clnt=%d.\n",
				 result, ipa_ep_idx);
		goto fail_gen2;
	}

	if (!ep->skip_ep_cfg) {
		if (ipa3_cfg_ep(ipa_ep_idx, &sys_in->ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto fail_gen2;
		}
		if (ipa3_cfg_ep_status(ipa_ep_idx, &ep->status)) {
			IPAERR("fail to configure status of EP.\n");
			goto fail_gen2;
		}
		IPADBG("ep configuration successful\n");
	} else {
		IPADBG("skipping ep configuration\n");
	}

	*clnt_hdl = ipa_ep_idx;

	*ipa_pipe_num = ipa_ep_idx;
	*ipa_transport_hdl = ipa3_ctx->gsi_dev_hdl;

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(sys_in->client);

	ipa3_ctx->skip_ep_cfg_shadow[ipa_ep_idx] = ep->skip_ep_cfg;
	IPADBG("client %d (ep: %d) connected sys=%pK\n", sys_in->client,
			ipa_ep_idx, ep->sys);

	return 0;

fail_gen2:
fail_and_disable_clocks:
	IPA_ACTIVE_CLIENTS_DEC_EP(sys_in->client);
fail_gen:
	return result;
}
EXPORT_SYMBOL(ipa3_sys_setup);

int ipa3_sys_teardown(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm(Either endpoint or client hdl invalid)\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_disable_data_path(clnt_hdl);
	ep->valid = 0;

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPADBG("client (ep: %d) disconnected\n", clnt_hdl);

	return 0;
}
EXPORT_SYMBOL(ipa3_sys_teardown);

int ipa3_sys_update_gsi_hdls(u32 clnt_hdl, unsigned long gsi_ch_hdl,
	unsigned long gsi_ev_hdl)
{
	struct ipa3_ep_context *ep;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm(Either endpoint or client hdl invalid)\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	ep->gsi_chan_hdl = gsi_ch_hdl;
	ep->gsi_evt_ring_hdl = gsi_ev_hdl;

	return 0;
}
EXPORT_SYMBOL(ipa3_sys_update_gsi_hdls);

static void ipa_gsi_evt_ring_err_cb(struct gsi_evt_err_notify *notify)
{
	switch (notify->evt_id) {
	case GSI_EVT_OUT_OF_BUFFERS_ERR:
		IPAERR("Got GSI_EVT_OUT_OF_BUFFERS_ERR\n");
		break;
	case GSI_EVT_OUT_OF_RESOURCES_ERR:
		IPAERR("Got GSI_EVT_OUT_OF_RESOURCES_ERR\n");
		break;
	case GSI_EVT_UNSUPPORTED_INTER_EE_OP_ERR:
		IPAERR("Got GSI_EVT_UNSUPPORTED_INTER_EE_OP_ERR\n");
		break;
	case GSI_EVT_EVT_RING_EMPTY_ERR:
		IPAERR("Got GSI_EVT_EVT_RING_EMPTY_ERR\n");
		break;
	default:
		IPAERR("Unexpected err evt: %d\n", notify->evt_id);
	}
}

static void ipa_gsi_chan_err_cb(struct gsi_chan_err_notify *notify)
{
	switch (notify->evt_id) {
	case GSI_CHAN_INVALID_TRE_ERR:
		IPAERR("Got GSI_CHAN_INVALID_TRE_ERR\n");
		break;
	case GSI_CHAN_NON_ALLOCATED_EVT_ACCESS_ERR:
		IPAERR("Got GSI_CHAN_NON_ALLOCATED_EVT_ACCESS_ERR\n");
		break;
	case GSI_CHAN_OUT_OF_BUFFERS_ERR:
		IPAERR("Got GSI_CHAN_OUT_OF_BUFFERS_ERR\n");
		break;
	case GSI_CHAN_OUT_OF_RESOURCES_ERR:
		IPAERR("Got GSI_CHAN_OUT_OF_RESOURCES_ERR\n");
		break;
	case GSI_CHAN_UNSUPPORTED_INTER_EE_OP_ERR:
		IPAERR("Got GSI_CHAN_UNSUPPORTED_INTER_EE_OP_ERR\n");
		break;
	case GSI_CHAN_HWO_1_ERR:
		IPAERR("Got GSI_CHAN_HWO_1_ERR\n");
		break;
	default:
		IPAERR("Unexpected err evt: %d\n", notify->evt_id);
	}
}

static void ipa_gsi_irq_tx_notify_cb(struct gsi_chan_xfer_notify *notify)
{
	struct ipa3_tx_pkt_wrapper *tx_pkt;
	struct ipa3_sys_context *sys;

	IPADBG_LOW("event %d notified\n", notify->evt_id);

	switch (notify->evt_id) {
	case GSI_CHAN_EVT_EOT:
		atomic_set(&ipa3_ctx->transport_pm.eot_activity, 1);
		tx_pkt = notify->xfer_user_data;
		tx_pkt->xmit_done = true;
		sys = tx_pkt->sys;
		if (sys->tx_poll) {
			if (!atomic_read(&sys->curr_polling_state)) {
				/* dummy vote to prevent NoC error */
				if(IPA_ACTIVE_CLIENTS_INC_EP_NO_BLOCK(
					sys->ep->client)) {
					IPAERR_RL("clk off, event likely handled in NAPI contxt");
					return;
				}
				/* put the producer event ring into polling mode */
				gsi_config_channel_mode(sys->ep->gsi_chan_hdl,
							GSI_CHAN_MODE_POLL);
				atomic_set(&sys->curr_polling_state, 1);
				__ipa3_update_curr_poll_state(sys->ep->client, 1);
				napi_schedule(&tx_pkt->sys->napi_tx);
			}
		} else if (ipa_net_initialized && sys->napi_tx_enable) {
			if(!atomic_cmpxchg(&tx_pkt->sys->in_napi_context, 0, 1))
				napi_schedule(&tx_pkt->sys->napi_tx);
		} else {
			atomic_inc(&tx_pkt->sys->xmit_eot_cnt);
			tasklet_schedule(&tx_pkt->sys->tasklet);
		}
		break;
	default:
		IPAERR("received unexpected event id %d\n", notify->evt_id);
	}
}

void __ipa_gsi_irq_rx_scedule_poll(struct ipa3_sys_context *sys)
{
	bool clk_off = true;
	enum ipa_client_type client_type;

	atomic_set(&sys->curr_polling_state, 1);
	__ipa3_update_curr_poll_state(sys->ep->client, 1);

	ipa3_inc_acquire_wakelock();
	/*
	 * Mark client as WAN_COAL_CONS only as
	 * NAPI only use sys of WAN_COAL_CONS.
	 */
	if (IPA_CLIENT_IS_WAN_CONS(sys->ep->client))
		client_type = IPA_CLIENT_APPS_WAN_COAL_CONS;
	else if (IPA_CLIENT_IS_LAN_CONS(sys->ep->client))
		client_type = IPA_CLIENT_APPS_LAN_COAL_CONS;
	else
		client_type = sys->ep->client;
	/*
	 * Have race condition to use PM on poll to isr
	 * switch. Use the active no block instead
	 * where we would have ref counts.
	 */
	if ((ipa_net_initialized && sys->napi_obj) ||
		IPA_CLIENT_IS_LOW_LAT_CONS(sys->ep->client) ||
		(sys->ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS))
		clk_off = IPA_ACTIVE_CLIENTS_INC_EP_NO_BLOCK(client_type);
	if (!clk_off && ipa_net_initialized && sys->napi_obj) {
		trace_ipa3_napi_schedule(sys->ep->client);
		napi_schedule(sys->napi_obj);
	} else if (!clk_off &&
		(sys->ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS)) {
		trace_ipa3_napi_schedule(sys->ep->client);
		napi_schedule(&sys->napi_rx);
	} else if (!clk_off &&
		IPA_CLIENT_IS_LOW_LAT_CONS(sys->ep->client)) {
		tasklet_schedule(&sys->tasklet);
	} else
		queue_work(sys->wq, &sys->work);
}

static void ipa_gsi_irq_rx_notify_cb(struct gsi_chan_xfer_notify *notify)
{
	struct ipa3_sys_context *sys;

	if (!notify) {
		IPAERR("gsi notify is NULL.\n");
		return;
	}
	IPADBG_LOW("event %d notified\n", notify->evt_id);

	sys = (struct ipa3_sys_context *)notify->chan_user_data;

	/*
	 * In suspend just before stopping the channel possible to receive
	 * the IEOB interrupt and xfer pointer will not be processed in this
	 * mode and moving channel poll mode. In resume after starting the
	 * channel will receive the IEOB interrupt and xfer pointer will be
	 * overwritten. To avoid this process all data in polling context.
	 */
	sys->ep->xfer_notify_valid = false;
	sys->ep->xfer_notify = *notify;

	switch (notify->evt_id) {
	case GSI_CHAN_EVT_EOT:
	case GSI_CHAN_EVT_EOB:
		atomic_set(&ipa3_ctx->transport_pm.eot_activity, 1);
		if (!atomic_read(&sys->curr_polling_state)) {
			/* put the gsi channel into polling mode */
			gsi_config_channel_mode(sys->ep->gsi_chan_hdl,
				GSI_CHAN_MODE_POLL);
			__ipa_gsi_irq_rx_scedule_poll(sys);
		}
		break;
	default:
		IPAERR("received unexpected event id %d\n", notify->evt_id);
	}
}

static void ipa_dma_gsi_irq_rx_notify_cb(struct gsi_chan_xfer_notify *notify)
{
	struct ipa3_sys_context *sys;

	if (!notify) {
		IPAERR("gsi notify is NULL.\n");
		return;
	}
	IPADBG_LOW("event %d notified\n", notify->evt_id);

	sys = (struct ipa3_sys_context *)notify->chan_user_data;
	if (sys->ep->client == IPA_CLIENT_MEMCPY_DMA_SYNC_CONS) {
		IPAERR("IRQ_RX Callback was called for DMA_SYNC_CONS.\n");
		return;
	}

	sys->ep->xfer_notify_valid = false;
	sys->ep->xfer_notify = *notify;

	switch (notify->evt_id) {
	case GSI_CHAN_EVT_EOT:
		if (!atomic_read(&sys->curr_polling_state)) {
			/* put the gsi channel into polling mode */
			gsi_config_channel_mode(sys->ep->gsi_chan_hdl,
				GSI_CHAN_MODE_POLL);
			ipa3_inc_acquire_wakelock();
			atomic_set(&sys->curr_polling_state, 1);
			queue_work(sys->wq, &sys->work);
		}
		break;
	default:
		IPAERR("received unexpected event id %d\n", notify->evt_id);
	}
}

void ipa3_dealloc_common_event_ring(void)
{
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	gsi_dealloc_evt_ring(ipa3_ctx->gsi_evt_comm_hdl);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
}

int ipa3_alloc_common_event_ring(void)
{
	struct gsi_evt_ring_props gsi_evt_ring_props;
	dma_addr_t evt_dma_addr = 0;
	dma_addr_t evt_rp_dma_addr = 0;
	int result;

	memset(&gsi_evt_ring_props, 0, sizeof(gsi_evt_ring_props));
	gsi_evt_ring_props.intf = GSI_EVT_CHTYPE_GPI_EV;
	gsi_evt_ring_props.intr = GSI_INTR_IRQ;
	gsi_evt_ring_props.re_size = GSI_EVT_RING_RE_SIZE_16B;

	gsi_evt_ring_props.ring_len = IPA_COMMON_EVENT_RING_SIZE;

	gsi_evt_ring_props.ring_base_vaddr =
		dma_alloc_coherent(ipa3_ctx->pdev,
		gsi_evt_ring_props.ring_len, &evt_dma_addr, GFP_KERNEL);
	if (!gsi_evt_ring_props.ring_base_vaddr) {
		IPAERR("fail to dma alloc %u bytes\n",
			gsi_evt_ring_props.ring_len);
		return -ENOMEM;
	}
	gsi_evt_ring_props.ring_base_addr = evt_dma_addr;
	gsi_evt_ring_props.int_modt = 0;
	gsi_evt_ring_props.int_modc = 1; /* moderation comes from channel*/

	if (ipa3_ctx->ipa_gpi_event_rp_ddr) {
		gsi_evt_ring_props.rp_update_vaddr =
			dma_alloc_coherent(ipa3_ctx->pdev,
					   IPA_GSI_EVENT_RP_SIZE,
					   &evt_rp_dma_addr, GFP_KERNEL);
		if (!gsi_evt_ring_props.rp_update_vaddr) {
			IPAERR("fail to dma alloc %u bytes\n",
			       IPA_GSI_EVENT_RP_SIZE);
			result = -ENOMEM;
			goto fail_alloc_rp;
		}
		gsi_evt_ring_props.rp_update_addr = evt_rp_dma_addr;
	} else {
		gsi_evt_ring_props.rp_update_addr = 0;
	}

	gsi_evt_ring_props.exclusive = false;
	gsi_evt_ring_props.err_cb = ipa_gsi_evt_ring_err_cb;
	gsi_evt_ring_props.user_data = NULL;

	result = gsi_alloc_evt_ring(&gsi_evt_ring_props,
		ipa3_ctx->gsi_dev_hdl, &ipa3_ctx->gsi_evt_comm_hdl);
	if (result) {
		IPAERR("gsi_alloc_evt_ring failed %d\n", result);
		goto fail_alloc_evt_ring;
	}
	ipa3_ctx->gsi_evt_comm_ring_rem = IPA_COMMON_EVENT_RING_SIZE;

	return 0;
fail_alloc_evt_ring:
	if (gsi_evt_ring_props.rp_update_vaddr) {
		dma_free_coherent(ipa3_ctx->pdev, IPA_GSI_EVENT_RP_SIZE,
				  gsi_evt_ring_props.rp_update_vaddr,
				  evt_rp_dma_addr);
	}
fail_alloc_rp:
	dma_free_coherent(ipa3_ctx->pdev, gsi_evt_ring_props.ring_len,
			  gsi_evt_ring_props.ring_base_vaddr,
			  evt_dma_addr);
	return result;
}

static int ipa_gsi_setup_channel(struct ipa_sys_connect_params *in,
	struct ipa3_ep_context *ep)
{
	u32 ring_size;
	int result;
	gfp_t mem_flag = GFP_KERNEL;
	u32 wan_coal_ep_id, lan_coal_ep_id;

	if (IPA_CLIENT_IS_WAN_CONS(in->client) ||
		IPA_CLIENT_IS_LAN_CONS(in->client) ||
		in->client == IPA_CLIENT_APPS_WAN_LOW_LAT_CONS ||
		in->client == IPA_CLIENT_APPS_WAN_LOW_LAT_PROD ||
		in->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS ||
		in->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_PROD ||
		in->client == IPA_CLIENT_APPS_WAN_PROD)
		mem_flag = GFP_ATOMIC;

	if (!ep) {
		IPAERR("EP context is empty\n");
		return -EINVAL;
	}

	/*
	 * GSI ring length is calculated based on the desc_fifo_sz
	 * which was meant to define the BAM desc fifo. GSI descriptors
	 * are 16B as opposed to 8B for BAM.
	 */
	ring_size = 2 * in->desc_fifo_sz;
	ep->gsi_evt_ring_hdl = ~0;
	if (ep->sys && ep->sys->use_comm_evt_ring) {
		if (ipa3_ctx->gsi_evt_comm_ring_rem < ring_size) {
			IPAERR("not enough space in common event ring\n");
			IPAERR("available: %d needed: %d\n",
				ipa3_ctx->gsi_evt_comm_ring_rem,
				ring_size);
			WARN_ON(1);
			return -EFAULT;
		}
		ipa3_ctx->gsi_evt_comm_ring_rem -= (ring_size);
		ep->gsi_evt_ring_hdl = ipa3_ctx->gsi_evt_comm_hdl;
	} else if (in->client == IPA_CLIENT_APPS_WAN_COAL_CONS) {
		result = ipa_gsi_setup_event_ring(ep,
				IPA_COMMON_EVENT_RING_SIZE, mem_flag);
		if (result)
			goto fail_setup_event_ring;

	} else if (in->client == IPA_CLIENT_APPS_WAN_CONS &&
		IPA_CLIENT_IS_MAPPED_VALID(IPA_CLIENT_APPS_WAN_COAL_CONS, wan_coal_ep_id)) {
		IPADBG("Wan consumer pipe configured\n");
		result = ipa_gsi_setup_coal_def_channel(in, ep,
					&ipa3_ctx->ep[wan_coal_ep_id]);
		if (result) {
			IPAERR("Failed to setup default coal GSI channel\n");
			goto fail_setup_event_ring;
		}
		return result;
	} else if (in->client == IPA_CLIENT_APPS_LAN_COAL_CONS) {
		result = ipa_gsi_setup_event_ring(ep,
				IPA_COMMON_EVENT_RING_SIZE, mem_flag);
		if (result)
			goto fail_setup_event_ring;
	} else if (in->client == IPA_CLIENT_APPS_LAN_CONS &&
		IPA_CLIENT_IS_MAPPED_VALID(IPA_CLIENT_APPS_LAN_COAL_CONS, lan_coal_ep_id)) {
		IPADBG("Lan consumer pipe configured\n");
		result = ipa_gsi_setup_coal_def_channel(in, ep,
					&ipa3_ctx->ep[lan_coal_ep_id]);
		if (result) {
			IPAERR("Failed to setup default coal GSI channel\n");
			goto fail_setup_event_ring;
		}
		return result;
	} else if ((ep->sys && ep->sys->policy != IPA_POLICY_NOINTR_MODE) ||
			IPA_CLIENT_IS_CONS(ep->client)) {
		result = ipa_gsi_setup_event_ring(ep, ring_size, mem_flag);
		if (result)
			goto fail_setup_event_ring;
	}
	result = ipa_gsi_setup_transfer_ring(ep, ring_size,
		ep->sys, mem_flag);
	if (result)
		goto fail_setup_transfer_ring;

	if (ep->client == IPA_CLIENT_MEMCPY_DMA_SYNC_CONS)
		gsi_config_channel_mode(ep->gsi_chan_hdl,
				GSI_CHAN_MODE_POLL);
	return 0;

fail_setup_transfer_ring:
	if (ep->gsi_mem_info.evt_ring_base_vaddr)
		dma_free_coherent(ipa3_ctx->pdev, ep->gsi_mem_info.evt_ring_len,
			ep->gsi_mem_info.evt_ring_base_vaddr,
			ep->gsi_mem_info.evt_ring_base_addr);
fail_setup_event_ring:
	IPAERR("Return with err: %d\n", result);
	return result;
}

static void *ipa3_ring_alloc(struct device *dev, size_t size,
	dma_addr_t *dma_handle, gfp_t gfp)
{
	void *va_addr;
	int retry_cnt = 0;

alloc:
	va_addr = dma_alloc_coherent(dev, size, dma_handle, gfp);
	if (!va_addr) {
		if (retry_cnt < IPA_MEM_ALLOC_RETRY) {
			IPADBG("Fail to dma alloc retry cnt = %d\n",
				retry_cnt);
			retry_cnt++;
			goto alloc;
		}

		if (gfp == GFP_ATOMIC) {
			gfp = GFP_KERNEL;
			goto alloc;
		}
		IPAERR("fail to dma alloc %u bytes\n", size);
		ipa_assert();
	}

	return va_addr;
}

static int ipa_gsi_setup_event_ring(struct ipa3_ep_context *ep,
	u32 ring_size, gfp_t mem_flag)
{
	struct gsi_evt_ring_props gsi_evt_ring_props;
	dma_addr_t evt_dma_addr;
	dma_addr_t evt_rp_dma_addr;
	int result;

	evt_dma_addr = 0;
	evt_rp_dma_addr = 0;
	memset(&gsi_evt_ring_props, 0, sizeof(gsi_evt_ring_props));
	gsi_evt_ring_props.intf = GSI_EVT_CHTYPE_GPI_EV;
	if ((ipa3_ctx->gsi_msi_addr) &&
		(ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS ||
		ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_CONS))
		gsi_evt_ring_props.intr = GSI_INTR_MSI; // intvec chosen dynamically.
	else gsi_evt_ring_props.intr = GSI_INTR_IRQ;
	gsi_evt_ring_props.re_size = GSI_EVT_RING_RE_SIZE_16B;
	gsi_evt_ring_props.ring_len = ring_size;
	gsi_evt_ring_props.ring_base_vaddr =
		ipa3_ring_alloc(ipa3_ctx->pdev, gsi_evt_ring_props.ring_len,
		&evt_dma_addr, mem_flag);
	gsi_evt_ring_props.ring_base_addr = evt_dma_addr;

	/* copy mem info */
	ep->gsi_mem_info.evt_ring_len = gsi_evt_ring_props.ring_len;
	ep->gsi_mem_info.evt_ring_base_addr =
		gsi_evt_ring_props.ring_base_addr;
	ep->gsi_mem_info.evt_ring_base_vaddr =
		gsi_evt_ring_props.ring_base_vaddr;

	if (ep->sys && ep->sys->napi_obj) {
		gsi_evt_ring_props.int_modt = IPA_GSI_EVT_RING_INT_MODT;
		gsi_evt_ring_props.int_modc = IPA_GSI_EVT_RING_INT_MODC;
	} else {
		gsi_evt_ring_props.int_modt = IPA_GSI_EVT_RING_INT_MODT;
		gsi_evt_ring_props.int_modc = 1;
	}

	if ((ep->sys && ep->sys->ext_ioctl_v2) &&
		((ep->client == IPA_CLIENT_APPS_WAN_PROD) ||
		(ep->client == IPA_CLIENT_APPS_WAN_CONS) ||
		(ep->client == IPA_CLIENT_APPS_WAN_COAL_CONS) ||
		(ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_PROD) ||
		(ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_CONS) ||
		(ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_PROD) ||
		(ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS))) {
		gsi_evt_ring_props.int_modt = ep->sys->int_modt;
		gsi_evt_ring_props.int_modc = ep->sys->int_modc;
	}

	IPADBG("client=%d moderation threshold cycles=%u cnt=%u\n",
		ep->client,
		gsi_evt_ring_props.int_modt,
		gsi_evt_ring_props.int_modc);
	if (ipa3_ctx->ipa_gpi_event_rp_ddr) {
		gsi_evt_ring_props.rp_update_vaddr =
			dma_alloc_coherent(ipa3_ctx->pdev,
					   IPA_GSI_EVENT_RP_SIZE,
					   &evt_rp_dma_addr, GFP_KERNEL);
		if (!gsi_evt_ring_props.rp_update_vaddr) {
			IPAERR("fail to dma alloc %u bytes\n",
				IPA_GSI_EVENT_RP_SIZE);
			result = -ENOMEM;
			goto fail_alloc_rp;
		}
		gsi_evt_ring_props.rp_update_addr = evt_rp_dma_addr;
	} else {
		gsi_evt_ring_props.rp_update_addr = 0;
	}

	/* copy mem info */
	ep->gsi_mem_info.evt_ring_rp_addr =
		gsi_evt_ring_props.rp_update_addr;
	ep->gsi_mem_info.evt_ring_rp_vaddr =
		gsi_evt_ring_props.rp_update_vaddr;

	gsi_evt_ring_props.exclusive = true;
	gsi_evt_ring_props.err_cb = ipa_gsi_evt_ring_err_cb;
	gsi_evt_ring_props.user_data = NULL;

	result = gsi_alloc_evt_ring(&gsi_evt_ring_props,
		ipa3_ctx->gsi_dev_hdl, &ep->gsi_evt_ring_hdl);
	if (result != GSI_STATUS_SUCCESS)
		goto fail_alloc_evt_ring;

	return 0;

fail_alloc_evt_ring:
	if (gsi_evt_ring_props.rp_update_vaddr) {
		dma_free_coherent(ipa3_ctx->pdev, IPA_GSI_EVENT_RP_SIZE,
				  gsi_evt_ring_props.rp_update_vaddr,
				  evt_rp_dma_addr);
		ep->gsi_mem_info.evt_ring_rp_addr = 0;
		ep->gsi_mem_info.evt_ring_rp_vaddr = 0;
	}
fail_alloc_rp:
	if (ep->gsi_mem_info.evt_ring_base_vaddr)
		dma_free_coherent(ipa3_ctx->pdev, ep->gsi_mem_info.evt_ring_len,
			ep->gsi_mem_info.evt_ring_base_vaddr,
			ep->gsi_mem_info.evt_ring_base_addr);
	IPAERR("Return with err: %d\n", result);
	return result;
}

static int ipa_gsi_setup_transfer_ring(struct ipa3_ep_context *ep,
	u32 ring_size, struct ipa3_sys_context *user_data, gfp_t mem_flag)
{
	dma_addr_t dma_addr;
	union __packed gsi_channel_scratch ch_scratch;
	struct gsi_chan_props gsi_channel_props;
	const struct ipa_gsi_ep_config *gsi_ep_info;
	int result;

	memset(&gsi_channel_props, 0, sizeof(gsi_channel_props));
	if (IPA_CLIENT_IS_APPS_COAL_CONS(ep->client))
		gsi_channel_props.prot = GSI_CHAN_PROT_GCI;
	else
		gsi_channel_props.prot = GSI_CHAN_PROT_GPI;
	if (IPA_CLIENT_IS_PROD(ep->client)) {
		gsi_channel_props.dir = CHAN_DIR_TO_GSI;
		if(ep->client == IPA_CLIENT_APPS_WAN_PROD ||
		   ep->client == IPA_CLIENT_APPS_LAN_PROD ||
		   ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_PROD)
			gsi_channel_props.tx_poll = ipa3_ctx->tx_poll;
		else
			gsi_channel_props.tx_poll = false;
	} else {
		gsi_channel_props.dir = CHAN_DIR_FROM_GSI;
		if (ep->sys)
			gsi_channel_props.max_re_expected = ep->sys->rx_pool_sz;
	}

	gsi_ep_info = ipa_get_gsi_ep_info(ep->client);
	if (!gsi_ep_info) {
		IPAERR("Failed getting GSI EP info for client=%d\n",
		       ep->client);
		result = -EINVAL;
		goto fail_get_gsi_ep_info;
	} else {
		gsi_channel_props.ch_id = gsi_ep_info->ipa_gsi_chan_num;
	}

	gsi_channel_props.evt_ring_hdl = ep->gsi_evt_ring_hdl;
	gsi_channel_props.re_size = GSI_CHAN_RE_SIZE_16B;
	gsi_channel_props.ring_len = ring_size;

	gsi_channel_props.ring_base_vaddr =
		ipa3_ring_alloc(ipa3_ctx->pdev, gsi_channel_props.ring_len,
			&dma_addr, mem_flag);
	gsi_channel_props.ring_base_addr = dma_addr;

	/* copy mem info */
	ep->gsi_mem_info.chan_ring_len = gsi_channel_props.ring_len;
	ep->gsi_mem_info.chan_ring_base_addr =
		gsi_channel_props.ring_base_addr;
	ep->gsi_mem_info.chan_ring_base_vaddr =
		gsi_channel_props.ring_base_vaddr;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0)
		gsi_channel_props.use_db_eng = GSI_CHAN_DIRECT_MODE;
	else
		gsi_channel_props.use_db_eng = GSI_CHAN_DB_MODE;
	gsi_channel_props.max_prefetch = GSI_ONE_PREFETCH_SEG;
	if (ep->client == IPA_CLIENT_APPS_CMD_PROD)
		gsi_channel_props.low_weight = IPA_GSI_MAX_CH_LOW_WEIGHT;
	else
		gsi_channel_props.low_weight = 1;
	gsi_channel_props.db_in_bytes = 1;
	/* Configure Low Latency Mode. */
	if (ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_PROD ||
		ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS)
		gsi_channel_props.low_latency_en = 1;
	gsi_channel_props.prefetch_mode = gsi_ep_info->prefetch_mode;
	gsi_channel_props.empty_lvl_threshold = gsi_ep_info->prefetch_threshold;
	gsi_channel_props.chan_user_data = user_data;
	gsi_channel_props.err_cb = ipa_gsi_chan_err_cb;
	if (IPA_CLIENT_IS_PROD(ep->client))
		gsi_channel_props.xfer_cb = ipa_gsi_irq_tx_notify_cb;
	else
		gsi_channel_props.xfer_cb = ipa_gsi_irq_rx_notify_cb;
	if (IPA_CLIENT_IS_MEMCPY_DMA_CONS(ep->client))
		gsi_channel_props.xfer_cb = ipa_dma_gsi_irq_rx_notify_cb;

	if (IPA_CLIENT_IS_CONS(ep->client))
		gsi_channel_props.cleanup_cb = free_rx_pkt;

	/* overwrite the cleanup_cb for page recycling */
	if (ipa3_ctx->ipa_wan_skb_page &&
		(IPA_CLIENT_IS_WAN_CONS(ep->client) ||
		(ep->client == IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS)))
		gsi_channel_props.cleanup_cb = free_rx_page;

	result = gsi_alloc_channel(&gsi_channel_props, ipa3_ctx->gsi_dev_hdl,
		&ep->gsi_chan_hdl);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("Failed to alloc GSI chan.\n");
		goto fail_alloc_channel;
	}

	memset(&ch_scratch, 0, sizeof(ch_scratch));
	/*
	 * Update scratch for MCS smart prefetch:
	 * Starting IPA4.5, smart prefetch implemented by H/W.
	 * At IPA 4.0/4.1/4.2, we do not use MCS smart prefetch
	 *  so keep the fields zero.
	 */
	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		ch_scratch.gpi.max_outstanding_tre =
			gsi_ep_info->ipa_if_tlv * GSI_CHAN_RE_SIZE_16B;
		ch_scratch.gpi.outstanding_threshold =
			2 * GSI_CHAN_RE_SIZE_16B;
	}
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5)
		ch_scratch.gpi.dl_nlo_channel = 0;
	result = gsi_write_channel_scratch(ep->gsi_chan_hdl, ch_scratch);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("failed to write scratch %d\n", result);
		goto fail_write_channel_scratch;
	}
	return 0;

fail_write_channel_scratch:
	if (gsi_dealloc_channel(ep->gsi_chan_hdl)
		!= GSI_STATUS_SUCCESS) {
		IPAERR("Failed to dealloc GSI chan.\n");
		WARN_ON(1);
	}
fail_alloc_channel:
	dma_free_coherent(ipa3_ctx->pdev, ep->gsi_mem_info.chan_ring_len,
			ep->gsi_mem_info.chan_ring_base_vaddr,
			ep->gsi_mem_info.chan_ring_base_addr);
fail_get_gsi_ep_info:
	if (ep->gsi_evt_ring_hdl != ~0) {
		gsi_dealloc_evt_ring(ep->gsi_evt_ring_hdl);
		ep->gsi_evt_ring_hdl = ~0;
	}
	return result;
}

static int ipa_gsi_setup_coal_def_channel(struct ipa_sys_connect_params *in,
	struct ipa3_ep_context *ep, struct ipa3_ep_context *coal_ep)
{
	u32 ring_size;
	int result;

	ring_size = 2 * in->desc_fifo_sz;

	/* copy event ring handle */
	ep->gsi_evt_ring_hdl = coal_ep->gsi_evt_ring_hdl;

	result = ipa_gsi_setup_transfer_ring(ep, ring_size,
		coal_ep->sys, GFP_ATOMIC);
	if (result) {
		if (ep->gsi_mem_info.evt_ring_base_vaddr)
			dma_free_coherent(ipa3_ctx->pdev,
					ep->gsi_mem_info.chan_ring_len,
					ep->gsi_mem_info.chan_ring_base_vaddr,
					ep->gsi_mem_info.chan_ring_base_addr);
		IPAERR("Destroying WAN_COAL_CONS evt_ring");
		if (ep->gsi_evt_ring_hdl != ~0) {
			gsi_dealloc_evt_ring(ep->gsi_evt_ring_hdl);
			ep->gsi_evt_ring_hdl = ~0;
		}
		IPAERR("Return with err: %d\n", result);
		return result;
	}
	return 0;
}

static int ipa_populate_tag_field(struct ipa3_desc *desc,
		struct ipa3_tx_pkt_wrapper *tx_pkt,
		struct ipahal_imm_cmd_pyld **tag_pyld_ret)
{
	struct ipahal_imm_cmd_pyld *tag_pyld;
	struct ipahal_imm_cmd_ip_packet_tag_status tag_cmd = {0};

	/* populate tag field only if it is NULL */
	if (desc->pyld == NULL) {
		tag_cmd.tag = pointer_to_tag_wa(tx_pkt);
		tag_pyld = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_IP_PACKET_TAG_STATUS, &tag_cmd, true);
		if (unlikely(!tag_pyld)) {
			IPAERR("Failed to construct ip_packet_tag_status\n");
			return -EFAULT;
		}
		/*
		 * This is for 32-bit pointer, will need special
		 * handling if 64-bit pointer is used
		 */
		IPADBG_LOW("tx_pkt sent in tag: 0x%pK\n", tx_pkt);
		desc->pyld = tag_pyld->data;
		desc->opcode = tag_pyld->opcode;
		desc->len = tag_pyld->len;
		desc->user1 = tag_pyld;
		desc->type = IPA_IMM_CMD_DESC;
		desc->callback = ipa3_tag_destroy_imm;

		*tag_pyld_ret = tag_pyld;
	}
	return 0;
}

static int ipa_poll_gsi_pkt(struct ipa3_sys_context *sys,
		struct gsi_chan_xfer_notify *notify)
{
	int unused_var;

	return ipa_poll_gsi_n_pkt(sys, notify, 1, &unused_var);
}


static int ipa_poll_gsi_n_pkt(struct ipa3_sys_context *sys,
		struct gsi_chan_xfer_notify *notify,
		int expected_num, int *actual_num)
{
	int ret;
	int idx = 0;
	int poll_num = 0;

	/* Parameters validity isn't checked as this is a static function */

	if (sys->ep->xfer_notify_valid) {
		*notify = sys->ep->xfer_notify;
		sys->ep->xfer_notify_valid = false;
		idx++;
	}
	if (expected_num == idx) {
		*actual_num = idx;
		return GSI_STATUS_SUCCESS;
	}

	ret = gsi_poll_n_channel(sys->ep->gsi_chan_hdl,
		&notify[idx], expected_num - idx, &poll_num);
	if (ret == GSI_STATUS_POLL_EMPTY) {
		if (idx) {
			*actual_num = idx;
			return GSI_STATUS_SUCCESS;
		}
		*actual_num = 0;
		return ret;
	} else if (ret != GSI_STATUS_SUCCESS) {
		if (idx) {
			*actual_num = idx;
			return GSI_STATUS_SUCCESS;
		}
		*actual_num = 0;
		IPAERR("Poll channel err: %d\n", ret);
		return ret;
	}

	*actual_num = idx + poll_num;
	return ret;
}

/**
 * ipa3_lan_rx_poll() - Poll the LAN rx packets from IPA HW.
 * This function is executed in the softirq context
 *
 * if input budget is zero, the driver switches back to
 * interrupt mode.
 *
 * return number of polled packets, on error 0(zero)
 */
int ipa3_lan_rx_poll(u32 clnt_hdl, int weight)
{
	struct ipa3_ep_context *ep;
	int ret;
	int cnt = 0;
	int num = 0;
	int i;
	int remain_aggr_weight;

	if (unlikely(clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0)) {
		IPAERR("bad param 0x%x\n", clnt_hdl);
		return cnt;
	}
	remain_aggr_weight = weight / IPA_LAN_AGGR_PKT_CNT;
	if (unlikely(remain_aggr_weight > IPA_LAN_NAPI_MAX_FRAMES)) {
		IPAERR("NAPI weight is higher than expected\n");
		IPAERR("expected %d got %d\n",
			IPA_LAN_NAPI_MAX_FRAMES, remain_aggr_weight);
		return cnt;
	}
	ep = &ipa3_ctx->ep[clnt_hdl];

start_poll:
	/*
	 * it is guaranteed we already have clock here.
	 * This is mainly for clock scaling.
	 */
	ipa_pm_activate(ep->sys->pm_hdl);
	while (remain_aggr_weight > 0 &&
			atomic_read(&ep->sys->curr_polling_state)) {
		atomic_set(&ipa3_ctx->transport_pm.eot_activity, 1);
		ret = ipa_poll_gsi_n_pkt(ep->sys, g_lan_rx_notify,
				remain_aggr_weight, &num);
		if (ret)
			break;

		for (i = 0; i < num; i++) {
			if (IPA_CLIENT_IS_MEMCPY_DMA_CONS(ep->client))
				ipa3_dma_memcpy_notify(ep->sys);
			else if (IPA_CLIENT_IS_WLAN_CONS(ep->client))
				ipa3_wlan_wq_rx_common(ep->sys, g_lan_rx_notify + i);
			else
				ipa3_wq_rx_common(ep->sys, g_lan_rx_notify + i);
		}

		remain_aggr_weight -= num;
		if (ep->sys->len == 0) {
			if (remain_aggr_weight == 0)
				cnt--;
			break;
		}
	}
	cnt += weight - remain_aggr_weight * IPA_LAN_AGGR_PKT_CNT;
	if (cnt < weight) {
		napi_complete(ep->sys->napi_obj);
		ret = ipa3_rx_switch_to_intr_mode(ep->sys);
		if (ret == -GSI_STATUS_PENDING_IRQ &&
				napi_reschedule(ep->sys->napi_obj))
			goto start_poll;

		IPA_ACTIVE_CLIENTS_DEC_EP_NO_BLOCK(ep->client);
	}

	return cnt;
}

/**
 * ipa3_rx_poll() - Poll the WAN rx packets from IPA HW. This
 * function is exectued in the softirq context
 *
 * if input budget is zero, the driver switches back to
 * interrupt mode.
 *
 * return number of polled packets, on error 0(zero)
 */
int ipa3_rx_poll(u32 clnt_hdl, int weight)
{
	struct ipa3_ep_context *ep;
	struct ipa3_sys_context *wan_def_sys;
	int ret;
	int cnt = 0;
	int num = 0;
	int remain_aggr_weight;
	int ipa_ep_idx;
	struct ipa_active_client_logging_info log;
	static struct gsi_chan_xfer_notify notify[IPA_WAN_NAPI_MAX_FRAMES];

	IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log, "NAPI");

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm 0x%x\n", clnt_hdl);
		return cnt;
	}

	ipa_ep_idx = ipa_get_ep_mapping(
		IPA_CLIENT_APPS_WAN_CONS);
	if (ipa_ep_idx ==
		IPA_EP_NOT_ALLOCATED) {
		IPAERR("Invalid client.\n");
		return cnt;
	}
	ep = &ipa3_ctx->ep[clnt_hdl];

	trace_ipa3_napi_poll_entry(ep->client);

	wan_def_sys = ipa3_ctx->ep[ipa_ep_idx].sys;
	remain_aggr_weight = weight / ipa3_ctx->ipa_wan_aggr_pkt_cnt;
	if (remain_aggr_weight > IPA_WAN_NAPI_MAX_FRAMES) {
		IPAERR("NAPI weight is higher than expected\n");
		IPAERR("expected %d got %d\n",
			IPA_WAN_NAPI_MAX_FRAMES, remain_aggr_weight);
		return -EINVAL;
	}

	ep->sys->common_sys->napi_sort_page_thrshld_cnt++;
start_poll:
	/*
	 * it is guaranteed we already have clock here.
	 * This is mainly for clock scaling.
	 */
	ipa_pm_activate(ep->sys->pm_hdl);
	while (remain_aggr_weight > 0 &&
			atomic_read(&ep->sys->curr_polling_state)) {
		atomic_set(&ipa3_ctx->transport_pm.eot_activity, 1);
		if (ipa3_ctx->enable_napi_chain) {
			ret = ipa_poll_gsi_n_pkt(ep->sys, notify,
				remain_aggr_weight, &num);
		} else {
			ret = ipa_poll_gsi_n_pkt(ep->sys, notify,
				1, &num);
		}
		if (ret)
			break;

		trace_ipa3_napi_rx_poll_num(ep->client, num);
		ipa3_rx_napi_chain(ep->sys, notify, num);
		remain_aggr_weight -= num;

		trace_ipa3_napi_rx_poll_cnt(ep->client, ep->sys->len);
		if (ep->sys->len == 0) {
			if (remain_aggr_weight == 0)
				cnt--;
			break;
		}
	}
	cnt += weight - remain_aggr_weight * ipa3_ctx->ipa_wan_aggr_pkt_cnt;
	/* call repl_hdlr before napi_reschedule / napi_complete */
	ep->sys->repl_hdlr(ep->sys);
	wan_def_sys->repl_hdlr(wan_def_sys);
	/* Scheduling WAN and COAL collect stats work wueue */
	queue_delayed_work(ipa3_ctx->collect_recycle_stats_wq,
		&ipa3_collect_default_coal_recycle_stats_wq_work, msecs_to_jiffies(10));
	/* When not able to replenish enough descriptors, keep in polling
	 * mode, wait for napi-poll and replenish again.
	 */
	if (cnt < weight && ep->sys->len > IPA_DEFAULT_SYS_YELLOW_WM &&
		wan_def_sys->len > IPA_DEFAULT_SYS_YELLOW_WM) {
		napi_complete(ep->sys->napi_obj);
		ret = ipa3_rx_switch_to_intr_mode(ep->sys);
		if (ret == -GSI_STATUS_PENDING_IRQ &&
				napi_reschedule(ep->sys->napi_obj))
			goto start_poll;
		IPA_ACTIVE_CLIENTS_DEC_EP_NO_BLOCK(ep->client);
	} else {
		cnt = weight;
		IPADBG_LOW("Client = %d not replenished free descripotrs\n",
				ep->client);
	}
	trace_ipa3_napi_poll_exit(ep->client);
	return cnt;
}

static unsigned long tag_to_pointer_wa(uint64_t tag)
{
	return 0xFFFF000000000000 | (unsigned long) tag;
}

static uint64_t pointer_to_tag_wa(struct ipa3_tx_pkt_wrapper *tx_pkt)
{
	u16 temp;
	/* Add the check but it might have throughput issue */
	if (BITS_PER_LONG == 64) {
		temp = (u16) (~((unsigned long) tx_pkt &
			0xFFFF000000000000) >> 48);
		if (temp) {
			IPAERR("The 16 prefix is not all 1s (%pK)\n",
			tx_pkt);
			/*
			 * We need all addresses starting at 0xFFFF to
			 * pass it to HW.
			 */
			ipa_assert();
		}
	}
	return (unsigned long)tx_pkt & 0x0000FFFFFFFFFFFF;
}

/**
 * ipa_gsi_ch20_wa() - software workaround for IPA GSI channel 20
 *
 * A hardware limitation requires to avoid using GSI physical channel 20.
 * This function allocates GSI physical channel 20 and holds it to prevent
 * others to use it.
 *
 * Return codes: 0 on success, negative on failure
 */
int ipa_gsi_ch20_wa(void)
{
	struct gsi_chan_props gsi_channel_props;
	dma_addr_t dma_addr;
	int result;
	int i;
	unsigned long chan_hdl[IPA_GSI_CH_20_WA_NUM_CH_TO_ALLOC];
	unsigned long chan_hdl_to_keep;


	memset(&gsi_channel_props, 0, sizeof(gsi_channel_props));
	gsi_channel_props.prot = GSI_CHAN_PROT_GPI;
	gsi_channel_props.dir = CHAN_DIR_TO_GSI;
	gsi_channel_props.evt_ring_hdl = ~0;
	gsi_channel_props.re_size = GSI_CHAN_RE_SIZE_16B;
	gsi_channel_props.ring_len = 4 * gsi_channel_props.re_size;
	gsi_channel_props.ring_base_vaddr =
		dma_alloc_coherent(ipa3_ctx->pdev, gsi_channel_props.ring_len,
		&dma_addr, 0);
	gsi_channel_props.ring_base_addr = dma_addr;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0)
		gsi_channel_props.use_db_eng = GSI_CHAN_DIRECT_MODE;
	else
		gsi_channel_props.use_db_eng = GSI_CHAN_DB_MODE;

	gsi_channel_props.db_in_bytes = 1;
	gsi_channel_props.max_prefetch = GSI_ONE_PREFETCH_SEG;
	gsi_channel_props.low_weight = 1;
	gsi_channel_props.err_cb = ipa_gsi_chan_err_cb;
	gsi_channel_props.xfer_cb = ipa_gsi_irq_tx_notify_cb;

	/* first allocate channels up to channel 20 */
	for (i = 0; i < IPA_GSI_CH_20_WA_NUM_CH_TO_ALLOC; i++) {
		gsi_channel_props.ch_id = i;
		result = gsi_alloc_channel(&gsi_channel_props,
			ipa3_ctx->gsi_dev_hdl,
			&chan_hdl[i]);
		if (result != GSI_STATUS_SUCCESS) {
			IPAERR("failed to alloc channel %d err %d\n",
				i, result);
			return result;
		}
	}

	/* allocate channel 20 */
	gsi_channel_props.ch_id = IPA_GSI_CH_20_WA_VIRT_CHAN;
	result = gsi_alloc_channel(&gsi_channel_props, ipa3_ctx->gsi_dev_hdl,
		&chan_hdl_to_keep);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR("failed to alloc channel %d err %d\n",
			i, result);
		return result;
	}

	/* release all other channels */
	for (i = 0; i < IPA_GSI_CH_20_WA_NUM_CH_TO_ALLOC; i++) {
		result = gsi_dealloc_channel(chan_hdl[i]);
		if (result != GSI_STATUS_SUCCESS) {
			IPAERR("failed to dealloc channel %d err %d\n",
				i, result);
			return result;
		}
	}

	/* DMA memory shall not be freed as it is used by channel 20 */
	return 0;
}

/**
 * ipa_adjust_ra_buff_base_sz()
 *
 * Return value: the largest power of two which is smaller
 * than the input value
 */
static u32 ipa_adjust_ra_buff_base_sz(u32 aggr_byte_limit)
{
	aggr_byte_limit += IPA_MTU;
	aggr_byte_limit += IPA_GENERIC_RX_BUFF_LIMIT;
	aggr_byte_limit--;
	aggr_byte_limit |= aggr_byte_limit >> 1;
	aggr_byte_limit |= aggr_byte_limit >> 2;
	aggr_byte_limit |= aggr_byte_limit >> 4;
	aggr_byte_limit |= aggr_byte_limit >> 8;
	aggr_byte_limit |= aggr_byte_limit >> 16;
	aggr_byte_limit++;
	return aggr_byte_limit >> 1;
}

static void ipa3_tasklet_rx_notify(unsigned long data)
{
	struct ipa3_sys_context *sys;
	struct sk_buff *rx_skb;
	struct gsi_chan_xfer_notify notify;
	int ret;

	sys = (struct ipa3_sys_context *)data;
	atomic_set(&ipa3_ctx->transport_pm.eot_activity, 1);
start_poll:
	/*
	 * it is guaranteed we already have clock here.
	 * This is mainly for clock scaling.
	 */
	ipa_pm_activate(sys->pm_hdl);
	while (1) {
		ret = ipa_poll_gsi_pkt(sys, &notify);
		if (ret)
			break;
		rx_skb = handle_skb_completion(&notify, true, NULL);
		if (rx_skb) {
			sys->pyld_hdlr(rx_skb, sys);
			sys->repl_hdlr(sys);
		}
	}

	ret = ipa3_rx_switch_to_intr_mode(sys);
	if (ret == -GSI_STATUS_PENDING_IRQ)
		goto start_poll;
	IPA_ACTIVE_CLIENTS_DEC_EP_NO_BLOCK(sys->ep->client);
}

static int ipa3_rmnet_ll_rx_poll(struct napi_struct *napi_rx, int budget)
{
	struct ipa3_sys_context *sys = container_of(napi_rx,
		struct ipa3_sys_context, napi_rx);
	int remain_aggr_weight;
	int ret;
	int cnt = 0;
	int num = 0;
	struct ipa_active_client_logging_info log;
	static struct gsi_chan_xfer_notify notify[IPA_WAN_NAPI_MAX_FRAMES];

	IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log, "NAPI_LL");

	remain_aggr_weight = budget / ipa3_ctx->ipa_wan_aggr_pkt_cnt;
	if (remain_aggr_weight > IPA_WAN_NAPI_MAX_FRAMES) {
		IPAERR("NAPI weight is higher than expected\n");
		IPAERR("expected %d got %d\n",
			IPA_WAN_NAPI_MAX_FRAMES, remain_aggr_weight);
		return -EINVAL;
	}

	sys->napi_sort_page_thrshld_cnt++;

	trace_ipa3_napi_poll_entry(sys->ep->client);
start_poll:
	/*
	 * it is guaranteed we already have clock here.
	 * This is mainly for clock scaling.
	 */
	ipa_pm_activate(sys->pm_hdl);
	while (remain_aggr_weight > 0 &&
		atomic_read(&sys->curr_polling_state)) {
		atomic_set(&ipa3_ctx->transport_pm.eot_activity, 1);
		ret = ipa_poll_gsi_n_pkt(sys, notify,
			remain_aggr_weight, &num);
		if (ret)
			break;

		trace_ipa3_napi_rx_poll_num(sys->ep->client, num);
		ipa3_rx_napi_chain(sys, notify, num);
		remain_aggr_weight -= num;

		trace_ipa3_napi_rx_poll_cnt(sys->ep->client, sys->len);
		if (sys->len == 0) {
			if (remain_aggr_weight == 0)
				cnt--;
			break;
		}
	}
	cnt += budget - remain_aggr_weight * ipa3_ctx->ipa_wan_aggr_pkt_cnt;
	/* call repl_hdlr before napi_reschedule / napi_complete */
	sys->repl_hdlr(sys);
	/* Scheduling RMNET LOW LAT DATA collect stats work queue */
	queue_delayed_work(ipa3_ctx->collect_recycle_stats_wq,
		&ipa3_collect_low_lat_data_recycle_stats_wq_work, msecs_to_jiffies(10));
	/* When not able to replenish enough descriptors, keep in polling
	 * mode, wait for napi-poll and replenish again.
	 */
	if (cnt < budget && (sys->len > IPA_DEFAULT_SYS_YELLOW_WM)) {
		napi_complete(napi_rx);
		ret = ipa3_rx_switch_to_intr_mode(sys);
		if (ret == -GSI_STATUS_PENDING_IRQ &&
				napi_reschedule(napi_rx))
			goto start_poll;
		IPA_ACTIVE_CLIENTS_DEC_EP_NO_BLOCK(sys->ep->client);
	} else {
		cnt = budget;
		IPADBG_LOW("Client = %d not replenished free descripotrs\n",
				sys->ep->client);
	}
	return cnt;
}
