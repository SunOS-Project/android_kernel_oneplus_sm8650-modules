/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <dp_types.h>
#include <wlan_dp_main.h>
#include <wlan_dp_fisa_rx.h>
#include "hal_rx_flow.h"
#include "dp_htt.h"
#include "dp_internal.h"
#include "hif.h"

static void dp_rx_fisa_flush_flow_wrap(struct dp_fisa_rx_sw_ft *sw_ft);

/*
 * Used by FW to route RX packets to host REO2SW1 ring if IPA hit
 * RX back pressure.
 */
#define REO_DEST_IND_IPA_REROUTE 2

#if defined(FISA_DEBUG_ENABLE)
/**
 * hex_dump_skb_data() - Helper function to dump skb while debugging
 * @nbuf: Nbuf to be dumped
 * @dump: dump enable/disable dumping
 *
 * Return: NONE
 */
static void hex_dump_skb_data(qdf_nbuf_t nbuf, bool dump)
{
	qdf_nbuf_t next_nbuf;
	int i = 0;

	if (!dump)
		return;

	if (!nbuf)
		return;

	dp_fisa_debug("%ps: skb: %pK skb->next:%pK frag_list %pK skb->data:%pK len %d data_len %d",
		      (void *)QDF_RET_IP, nbuf, qdf_nbuf_next(nbuf),
		      qdf_nbuf_get_ext_list(nbuf), qdf_nbuf_data(nbuf),
		      qdf_nbuf_len(nbuf), qdf_nbuf_get_only_data_len(nbuf));
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_INFO_HIGH,
			   nbuf->data, 64);

	next_nbuf = qdf_nbuf_get_ext_list(nbuf);
	while (next_nbuf) {
		dp_fisa_debug("%d nbuf:%pK nbuf->next:%pK nbuf->data:%pK len %d",
			      i, next_nbuf, qdf_nbuf_next(next_nbuf),
			      qdf_nbuf_data(next_nbuf),
			      qdf_nbuf_len(next_nbuf));
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_INFO_HIGH,
				   qdf_nbuf_data(next_nbuf), 64);
		next_nbuf = qdf_nbuf_next(next_nbuf);
		i++;
	}
}

/**
 * dump_tlvs() - Helper function to dump TLVs of msdu
 * @hal_soc_hdl: Handle to TLV functions
 * @buf: Pointer to TLV header
 * @dbg_level: level control output of TLV dump
 *
 * Return: NONE
 */
static void dump_tlvs(hal_soc_handle_t hal_soc_hdl, uint8_t *buf,
		      uint8_t dbg_level)
{
	uint32_t fisa_aggr_count, fisa_timeout, cumulat_l4_csum, cumulat_ip_len;
	int flow_aggr_cont;

	hal_rx_dump_pkt_tlvs(hal_soc_hdl, buf, dbg_level);

	flow_aggr_cont = hal_rx_get_fisa_flow_agg_continuation(hal_soc_hdl,
							       buf);
	fisa_aggr_count = hal_rx_get_fisa_flow_agg_count(hal_soc_hdl, buf);
	fisa_timeout = hal_rx_get_fisa_timeout(hal_soc_hdl, buf);
	cumulat_l4_csum = hal_rx_get_fisa_cumulative_l4_checksum(hal_soc_hdl,
								 buf);
	cumulat_ip_len = hal_rx_get_fisa_cumulative_ip_length(hal_soc_hdl, buf);

	dp_fisa_debug("flow_aggr_cont %d, fisa_timeout %d, fisa_aggr_count %d, cumulat_l4_csum %d, cumulat_ip_len %d",
		      flow_aggr_cont, fisa_timeout, fisa_aggr_count,
		      cumulat_l4_csum, cumulat_ip_len);
}
#else
static void hex_dump_skb_data(qdf_nbuf_t nbuf, bool dump)
{
}

static void dump_tlvs(hal_soc_handle_t hal_soc_hdl, uint8_t *buf,
		      uint8_t dbg_level)
{
}
#endif

#ifdef WLAN_SUPPORT_RX_FISA_HIST
static
void dp_fisa_record_pkt(struct dp_fisa_rx_sw_ft *fisa_flow, qdf_nbuf_t nbuf,
			uint8_t *rx_tlv_hdr, uint16_t tlv_size)
{
	uint32_t index;
	uint8_t *tlv_hist_ptr;

	if (!rx_tlv_hdr || !fisa_flow || !fisa_flow->pkt_hist.tlv_hist)
		return;

	index = fisa_flow->pkt_hist.idx++ % FISA_FLOW_MAX_AGGR_COUNT;

	fisa_flow->pkt_hist.ts_hist[index] = qdf_get_log_timestamp();
	tlv_hist_ptr = fisa_flow->pkt_hist.tlv_hist + (index * tlv_size);
	qdf_mem_copy(tlv_hist_ptr, rx_tlv_hdr, tlv_size);
}
#else
static
void dp_fisa_record_pkt(struct dp_fisa_rx_sw_ft *fisa_flow, qdf_nbuf_t nbuf,
			uint8_t *rx_tlv_hdr, uint16_t tlv_size)
{
}

#endif

/**
 * wlan_dp_nbuf_skip_rx_pkt_tlv() - Function to skip the TLVs and
 *				    mac header from msdu
 * @dp_ctx: DP component handle
 * @rx_fst: FST handle
 * @nbuf: msdu for which TLVs has to be skipped
 *
 * Return: None
 */
static inline void
wlan_dp_nbuf_skip_rx_pkt_tlv(struct wlan_dp_psoc_context *dp_ctx,
			     struct dp_rx_fst *rx_fst, qdf_nbuf_t nbuf)
{
	uint8_t *rx_tlv_hdr;
	uint32_t l2_hdr_offset;

	rx_tlv_hdr = qdf_nbuf_data(nbuf);
	l2_hdr_offset = hal_rx_msdu_end_l3_hdr_padding_get(dp_ctx->hal_soc,
							   rx_tlv_hdr);
	qdf_nbuf_pull_head(nbuf, rx_fst->rx_pkt_tlv_size + l2_hdr_offset);
}

static bool
dp_rx_fisa_should_bypass(struct cdp_rx_flow_tuple_info *flow_tuple_info)
{
	if (flow_tuple_info->dest_port == DNS_SERVER_PORT ||
	    flow_tuple_info->src_port == DNS_SERVER_PORT)
		return true;

	return false;
}

static bool
dp_fisa_is_ipsec_connection(struct cdp_rx_flow_tuple_info *flow_tuple_info)
{
	if (flow_tuple_info->dest_port == IPSEC_PORT ||
	    flow_tuple_info->dest_port == IPSEC_NAT_PORT ||
	    flow_tuple_info->src_port == IPSEC_PORT ||
	    flow_tuple_info->src_port == IPSEC_NAT_PORT)
		return true;

	return false;
}

/**
 * wlan_dp_get_flow_tuple_from_nbuf() - Get the flow tuple from msdu
 * @dp_ctx: DP component handle
 * @flow_tuple_info: return argument where the flow is populated
 * @nbuf: msdu from which flow tuple is extracted.
 * @rx_tlv_hdr: Pointer to msdu TLVs
 *
 * Return: None
 */
static void
wlan_dp_get_flow_tuple_from_nbuf(struct wlan_dp_psoc_context *dp_ctx,
				 struct cdp_rx_flow_tuple_info *flow_tuple_info,
				 qdf_nbuf_t nbuf, uint8_t *rx_tlv_hdr)
{
	struct dp_rx_fst *rx_fst = dp_ctx->rx_fst;
	qdf_net_iphdr_t *iph;
	qdf_net_tcphdr_t *tcph;
	uint32_t ip_hdr_offset;
	uint32_t tcp_hdr_offset;
	uint32_t l2_hdr_offset =
			hal_rx_msdu_end_l3_hdr_padding_get(dp_ctx->hal_soc,
							   rx_tlv_hdr);

	hal_rx_get_l3_l4_offsets(dp_ctx->hal_soc, rx_tlv_hdr,
				 &ip_hdr_offset, &tcp_hdr_offset);
	flow_tuple_info->tuple_populated = true;

	qdf_nbuf_pull_head(nbuf, rx_fst->rx_pkt_tlv_size + l2_hdr_offset);

	iph = (qdf_net_iphdr_t *)(qdf_nbuf_data(nbuf) + ip_hdr_offset);
	tcph = (qdf_net_tcphdr_t *)(qdf_nbuf_data(nbuf) + ip_hdr_offset +
						tcp_hdr_offset);

	flow_tuple_info->dest_ip_31_0 = qdf_ntohl(iph->ip_daddr);
	flow_tuple_info->dest_ip_63_32 = 0;
	flow_tuple_info->dest_ip_95_64 = 0;
	flow_tuple_info->dest_ip_127_96 =
		HAL_IP_DA_SA_PREFIX_IPV4_COMPATIBLE_IPV6;

	flow_tuple_info->src_ip_31_0 = qdf_ntohl(iph->ip_saddr);
	flow_tuple_info->src_ip_63_32 = 0;
	flow_tuple_info->src_ip_95_64 = 0;
	flow_tuple_info->src_ip_127_96 =
		HAL_IP_DA_SA_PREFIX_IPV4_COMPATIBLE_IPV6;

	flow_tuple_info->dest_port = qdf_ntohs(tcph->dest);
	flow_tuple_info->src_port = qdf_ntohs(tcph->source);
	if (dp_fisa_is_ipsec_connection(flow_tuple_info))
		flow_tuple_info->is_exception = 1;
	else
		flow_tuple_info->is_exception = 0;

	flow_tuple_info->bypass_fisa =
		dp_rx_fisa_should_bypass(flow_tuple_info);

	flow_tuple_info->l4_protocol = iph->ip_proto;
	dp_fisa_debug("l4_protocol %d", flow_tuple_info->l4_protocol);

	qdf_nbuf_push_head(nbuf, rx_fst->rx_pkt_tlv_size + l2_hdr_offset);

	dp_fisa_debug("head_skb: %pK head_skb->next:%pK head_skb->data:%pK len %d data_len %d",
		      nbuf, qdf_nbuf_next(nbuf), qdf_nbuf_data(nbuf),
		      qdf_nbuf_len(nbuf), qdf_nbuf_get_only_data_len(nbuf));
}

/**
 * dp_rx_fisa_setup_hw_fse() - Populate flow so as to update DDR flow table
 * @fisa_hdl: Handle fisa context
 * @hashed_flow_idx: Index to flow table
 * @rx_flow_info: tuple to be populated in flow table
 * @flow_steer_info: REO index to which flow to be steered
 *
 * Return: Pointer to DDR flow table entry
 */
static void *
dp_rx_fisa_setup_hw_fse(struct dp_rx_fst *fisa_hdl,
			uint32_t hashed_flow_idx,
			struct cdp_rx_flow_tuple_info *rx_flow_info,
			uint32_t flow_steer_info)
{
	struct hal_rx_flow flow;
	void *hw_fse;

	flow.reo_destination_indication = flow_steer_info;
	flow.fse_metadata = 0xDEADBEEF;
	flow.tuple_info.dest_ip_127_96 = rx_flow_info->dest_ip_127_96;
	flow.tuple_info.dest_ip_95_64 = rx_flow_info->dest_ip_95_64;
	flow.tuple_info.dest_ip_63_32 =	rx_flow_info->dest_ip_63_32;
	flow.tuple_info.dest_ip_31_0 = rx_flow_info->dest_ip_31_0;
	flow.tuple_info.src_ip_127_96 =	rx_flow_info->src_ip_127_96;
	flow.tuple_info.src_ip_95_64 = rx_flow_info->src_ip_95_64;
	flow.tuple_info.src_ip_63_32 = rx_flow_info->src_ip_63_32;
	flow.tuple_info.src_ip_31_0 = rx_flow_info->src_ip_31_0;
	flow.tuple_info.dest_port = rx_flow_info->dest_port;
	flow.tuple_info.src_port = rx_flow_info->src_port;
	flow.tuple_info.l4_protocol = rx_flow_info->l4_protocol;
	flow.reo_destination_handler = HAL_RX_FSE_REO_DEST_FT;
	hw_fse = hal_rx_flow_setup_fse(fisa_hdl->dp_ctx->hal_soc,
				       fisa_hdl->hal_rx_fst, hashed_flow_idx,
				       &flow);

	return hw_fse;
}

#ifdef DP_FT_LOCK_HISTORY
struct dp_ft_lock_history ft_lock_hist[MAX_REO_DEST_RINGS];

/**
 * dp_rx_fisa_record_ft_lock_event() - Record FT lock/unlock events
 * @reo_id: REO ID
 * @func: caller function
 * @type: lock/unlock event type
 *
 * Return: None
 */
static void dp_rx_fisa_record_ft_lock_event(uint8_t reo_id, const char *func,
					    enum dp_ft_lock_event_type type)
{
	struct dp_ft_lock_history *lock_hist;
	struct dp_ft_lock_record *record;
	uint32_t record_idx;

	if (reo_id >= MAX_REO_DEST_RINGS)
		return;

	lock_hist = &ft_lock_hist[reo_id];
	record_idx = lock_hist->record_idx % DP_FT_LOCK_MAX_RECORDS;
	ft_lock_hist->record_idx++;

	record = &lock_hist->ft_lock_rec[record_idx];

	record->func = func;
	record->cpu_id = qdf_get_cpu();
	record->timestamp = qdf_get_log_timestamp();
	record->type = type;
}

/**
 * __dp_rx_fisa_acquire_ft_lock() - Acquire lock which protects SW FT entries
 * @fisa_hdl: Handle to fisa context
 * @reo_id: REO ID
 * @func: calling function name
 *
 * Return: None
 */
static inline void
__dp_rx_fisa_acquire_ft_lock(struct dp_rx_fst *fisa_hdl,
			     uint8_t reo_id, const char *func)
{
	if (!fisa_hdl->flow_deletion_supported)
		return;

	qdf_spin_lock_bh(&fisa_hdl->dp_rx_sw_ft_lock[reo_id]);
	dp_rx_fisa_record_ft_lock_event(reo_id, func, DP_FT_LOCK_EVENT);
}

/**
 * __dp_rx_fisa_release_ft_lock() - Release lock which protects SW FT entries
 * @fisa_hdl: Handle to fisa context
 * @reo_id: REO ID
 * @func: calling function name
 *
 * Return: None
 */
static inline void
__dp_rx_fisa_release_ft_lock(struct dp_rx_fst *fisa_hdl,
			     uint8_t reo_id, const char *func)
{
	if (!fisa_hdl->flow_deletion_supported)
		return;

	qdf_spin_unlock_bh(&fisa_hdl->dp_rx_sw_ft_lock[reo_id]);
	dp_rx_fisa_record_ft_lock_event(reo_id, func, DP_FT_UNLOCK_EVENT);
}

#define dp_rx_fisa_acquire_ft_lock(fisa_hdl, reo_id) \
	__dp_rx_fisa_acquire_ft_lock(fisa_hdl, reo_id, __func__)

#define dp_rx_fisa_release_ft_lock(fisa_hdl, reo_id) \
	__dp_rx_fisa_release_ft_lock(fisa_hdl, reo_id, __func__)

#else
/**
 * dp_rx_fisa_acquire_ft_lock() - Acquire lock which protects SW FT entries
 * @fisa_hdl: Handle to fisa context
 * @reo_id: REO ID
 *
 * Return: None
 */
static inline void
dp_rx_fisa_acquire_ft_lock(struct dp_rx_fst *fisa_hdl, uint8_t reo_id)
{
	if (fisa_hdl->flow_deletion_supported)
		qdf_spin_lock_bh(&fisa_hdl->dp_rx_sw_ft_lock[reo_id]);
}

/**
 * dp_rx_fisa_release_ft_lock() - Release lock which protects SW FT entries
 * @fisa_hdl: Handle to fisa context
 * @reo_id: REO ID
 *
 * Return: None
 */
static inline void
dp_rx_fisa_release_ft_lock(struct dp_rx_fst *fisa_hdl, uint8_t reo_id)
{
	if (fisa_hdl->flow_deletion_supported)
		qdf_spin_unlock_bh(&fisa_hdl->dp_rx_sw_ft_lock[reo_id]);
}
#endif /* DP_FT_LOCK_HISTORY */

/**
 * dp_rx_fisa_setup_cmem_fse() - Setup the flow search entry in HW CMEM
 * @fisa_hdl: Handle to fisa context
 * @hashed_flow_idx: Index to flow table
 * @rx_flow_info: tuple to be populated in flow table
 * @flow_steer_info: REO index to which flow to be steered
 *
 * Return: Offset to the FSE entry in CMEM
 */
static uint32_t
dp_rx_fisa_setup_cmem_fse(struct dp_rx_fst *fisa_hdl, uint32_t hashed_flow_idx,
			  struct cdp_rx_flow_tuple_info *rx_flow_info,
			  uint32_t flow_steer_info)
{
	struct dp_fisa_rx_sw_ft *sw_ft_entry;
	struct hal_rx_flow flow;

	sw_ft_entry = &(((struct dp_fisa_rx_sw_ft *)
				fisa_hdl->base)[hashed_flow_idx]);
	sw_ft_entry->metadata = ++fisa_hdl->meta_counter;

	flow.reo_destination_indication = flow_steer_info;
	flow.fse_metadata = sw_ft_entry->metadata;
	flow.tuple_info.dest_ip_127_96 = rx_flow_info->dest_ip_127_96;
	flow.tuple_info.dest_ip_95_64 = rx_flow_info->dest_ip_95_64;
	flow.tuple_info.dest_ip_63_32 =	rx_flow_info->dest_ip_63_32;
	flow.tuple_info.dest_ip_31_0 = rx_flow_info->dest_ip_31_0;
	flow.tuple_info.src_ip_127_96 =	rx_flow_info->src_ip_127_96;
	flow.tuple_info.src_ip_95_64 = rx_flow_info->src_ip_95_64;
	flow.tuple_info.src_ip_63_32 = rx_flow_info->src_ip_63_32;
	flow.tuple_info.src_ip_31_0 = rx_flow_info->src_ip_31_0;
	flow.tuple_info.dest_port = rx_flow_info->dest_port;
	flow.tuple_info.src_port = rx_flow_info->src_port;
	flow.tuple_info.l4_protocol = rx_flow_info->l4_protocol;
	flow.reo_destination_handler = HAL_RX_FSE_REO_DEST_FT;

	return hal_rx_flow_setup_cmem_fse(fisa_hdl->dp_ctx->hal_soc,
					  fisa_hdl->cmem_ba, hashed_flow_idx,
					  &flow);
}

static inline
struct wlan_dp_intf *dp_fisa_rx_get_dp_intf_for_vdev(struct dp_vdev *vdev)
{
	struct wlan_dp_link *dp_link =
				(struct wlan_dp_link *)vdev->osif_vdev;

	/* dp_link cannot be invalid if vdev is present */
	return dp_link->dp_intf;
}

/**
 * dp_rx_fisa_update_sw_ft_entry() - Helper function to update few SW FT entry
 * @sw_ft_entry: Pointer to softerware flow table entry
 * @flow_hash: flow_hash for the flow
 * @vdev: Saving dp_vdev in FT later used in the flushing the flow
 * @dp_ctx: DP component handle
 * @flow_id: Flow ID of the flow
 *
 * Return: NONE
 */
static void dp_rx_fisa_update_sw_ft_entry(struct dp_fisa_rx_sw_ft *sw_ft_entry,
					  uint32_t flow_hash,
					  struct dp_vdev *vdev,
					  struct wlan_dp_psoc_context *dp_ctx,
					  uint32_t flow_id)
{
	sw_ft_entry->flow_hash = flow_hash;
	sw_ft_entry->flow_id = flow_id;
	sw_ft_entry->vdev_id = vdev->vdev_id;
	sw_ft_entry->vdev = vdev;
	sw_ft_entry->dp_intf = dp_fisa_rx_get_dp_intf_for_vdev(vdev);
	sw_ft_entry->dp_ctx = dp_ctx;
}

/**
 * is_same_flow() - Function to compare flow tuple to decide if they match
 * @tuple1: flow tuple 1
 * @tuple2: flow tuple 2
 *
 * Return: true if they match, false if they differ
 */
static bool is_same_flow(struct cdp_rx_flow_tuple_info *tuple1,
			 struct cdp_rx_flow_tuple_info *tuple2)
{
	if ((tuple1->src_port ^ tuple2->src_port) |
	    (tuple1->dest_port ^ tuple2->dest_port) |
	    (tuple1->src_ip_31_0 ^ tuple2->src_ip_31_0) |
	    (tuple1->src_ip_63_32 ^ tuple2->src_ip_63_32) |
	    (tuple1->src_ip_95_64 ^ tuple2->src_ip_95_64) |
	    (tuple1->src_ip_127_96 ^ tuple2->src_ip_127_96) |
	    (tuple1->dest_ip_31_0 ^ tuple2->dest_ip_31_0) |
	    /* DST IP check not required? */
	    (tuple1->dest_ip_63_32 ^ tuple2->dest_ip_63_32) |
	    (tuple1->dest_ip_95_64 ^ tuple2->dest_ip_95_64) |
	    (tuple1->dest_ip_127_96 ^ tuple2->dest_ip_127_96) |
	    (tuple1->l4_protocol ^ tuple2->l4_protocol))
		return false;
	else
		return true;
}

/**
 * dp_rx_fisa_add_ft_entry() - Add new flow to HW and SW FT if it is not added
 * @vdev: Handle DP vdev to save in SW flow table
 * @fisa_hdl: handle to FISA context
 * @nbuf: nbuf belonging to new flow
 * @rx_tlv_hdr: Pointer to TLV header
 * @flow_idx_hash: Hashed flow index
 * @reo_dest_indication: Reo destination indication for nbuf
 *
 * Return: pointer to sw FT entry on success, NULL otherwise
 */
static struct dp_fisa_rx_sw_ft *
dp_rx_fisa_add_ft_entry(struct dp_vdev *vdev,
			struct dp_rx_fst *fisa_hdl,
			qdf_nbuf_t nbuf,
			uint8_t *rx_tlv_hdr,
			uint32_t flow_idx_hash,
			uint32_t reo_dest_indication)
{
	struct dp_fisa_rx_sw_ft *sw_ft_entry;
	uint32_t flow_hash;
	uint32_t hashed_flow_idx;
	uint32_t skid_count = 0, max_skid_length;
	struct cdp_rx_flow_tuple_info rx_flow_tuple_info;
	bool is_fst_updated = false;
	uint32_t reo_id = QDF_NBUF_CB_RX_CTX_ID(nbuf);
	struct hal_proto_params proto_params;

	if (hal_rx_get_proto_params(fisa_hdl->dp_ctx->hal_soc, rx_tlv_hdr,
				    &proto_params))
		return NULL;

	if (proto_params.ipv6_proto ||
	    !(proto_params.tcp_proto || proto_params.udp_proto)) {
		dp_fisa_debug("Not UDP or TCP IPV4 flow");
		return NULL;
	}

	rx_flow_tuple_info.tuple_populated = false;
	flow_hash = flow_idx_hash;
	hashed_flow_idx = flow_hash & fisa_hdl->hash_mask;
	max_skid_length = fisa_hdl->max_skid_length;

	dp_fisa_debug("flow_hash 0x%x hashed_flow_idx 0x%x", flow_hash,
		      hashed_flow_idx);
	dp_fisa_debug("max_skid_length 0x%x", max_skid_length);

	qdf_spin_lock_bh(&fisa_hdl->dp_rx_fst_lock);

	if (!rx_flow_tuple_info.tuple_populated) {
		wlan_dp_get_flow_tuple_from_nbuf(fisa_hdl->dp_ctx,
						 &rx_flow_tuple_info,
						 nbuf, rx_tlv_hdr);
		if (rx_flow_tuple_info.bypass_fisa) {
			qdf_spin_unlock_bh(&fisa_hdl->dp_rx_fst_lock);
			return NULL;
		}
	}

	do {
		sw_ft_entry = &(((struct dp_fisa_rx_sw_ft *)
					fisa_hdl->base)[hashed_flow_idx]);
		if (!sw_ft_entry->is_populated) {
			/* Add SW FT entry */
			dp_rx_fisa_update_sw_ft_entry(sw_ft_entry,
						      flow_hash, vdev,
						      fisa_hdl->dp_ctx,
						      hashed_flow_idx);

			/* Add HW FT entry */
			sw_ft_entry->hw_fse =
				dp_rx_fisa_setup_hw_fse(fisa_hdl,
							hashed_flow_idx,
							&rx_flow_tuple_info,
							reo_dest_indication);
			sw_ft_entry->is_populated = true;
			sw_ft_entry->napi_id = reo_id;
			sw_ft_entry->reo_dest_indication = reo_dest_indication;
			sw_ft_entry->flow_id_toeplitz =
						QDF_NBUF_CB_RX_FLOW_ID(nbuf);
			sw_ft_entry->flow_init_ts = qdf_get_log_timestamp();

			qdf_mem_copy(&sw_ft_entry->rx_flow_tuple_info,
				     &rx_flow_tuple_info,
				     sizeof(struct cdp_rx_flow_tuple_info));

			sw_ft_entry->is_flow_tcp = proto_params.tcp_proto;
			sw_ft_entry->is_flow_udp = proto_params.udp_proto;
			sw_ft_entry->add_timestamp = qdf_get_log_timestamp();

			is_fst_updated = true;
			fisa_hdl->add_flow_count++;
			break;
		}
		/* else */

		if (is_same_flow(&sw_ft_entry->rx_flow_tuple_info,
				 &rx_flow_tuple_info)) {
			sw_ft_entry->vdev = vdev;
			sw_ft_entry->vdev_id = vdev->vdev_id;
			sw_ft_entry->dp_intf =
					dp_fisa_rx_get_dp_intf_for_vdev(vdev);
			dp_fisa_debug("It is same flow fse entry idx %d",
				      hashed_flow_idx);
			/* Incoming flow tuple matches with existing
			 * entry. This is subsequent skbs of the same
			 * flow. Earlier entry made is not reflected
			 * yet in FSE cache
			 */
			break;
		}
		/* else */
		/* hash collision move to the next FT entry */
		dp_fisa_debug("Hash collision %d",
			      fisa_hdl->hash_collision_cnt);
		fisa_hdl->hash_collision_cnt++;
#ifdef NOT_YET /* assist Flow eviction algorithm */
	/* uint32_t lru_ft_entry_time = 0xffffffff, lru_ft_entry_idx = 0; */
		if (fisa_hdl->hw_ft_entry->timestamp < lru_ft_entry_time) {
			lru_ft_entry_time = fisa_hdl->hw_ft_entry->timestamp;
			lru_ft_entry_idx = hashed_flow_idx;
		}
#endif
		skid_count++;
		hashed_flow_idx++;
		hashed_flow_idx &= fisa_hdl->hash_mask;
	} while (skid_count <= max_skid_length);

	/*
	 * fisa_hdl->flow_eviction_cnt++;
	 * if (skid_count > max_skid_length)
	 * Remove LRU flow from HW FT
	 * Remove LRU flow from SW FT
	 */
	qdf_spin_unlock_bh(&fisa_hdl->dp_rx_fst_lock);

	if (skid_count > max_skid_length) {
		dp_fisa_debug("Max skid length reached flow cannot be added, evict exiting flow");
		return NULL;
	}

	/**
	 * Send HTT cache invalidation command to firmware to
	 * reflect the flow update
	 */
	if (is_fst_updated &&
	    fisa_hdl->fse_cache_flush_allow &&
	    (qdf_atomic_inc_return(&fisa_hdl->fse_cache_flush_posted) == 1)) {
		/* return 1 after increment implies FSE cache flush message
		 * already posted. so start restart the timer
		 */
		qdf_timer_start(&fisa_hdl->fse_cache_flush_timer,
				FSE_CACHE_FLUSH_TIME_OUT);
	}
	dp_fisa_debug("sw_ft_entry %pK", sw_ft_entry);
	return sw_ft_entry;
}

/**
 * is_flow_idx_valid() - Function to decide if flow_idx TLV is valid
 * @flow_invalid: flow invalid TLV value
 * @flow_timeout: flow timeout TLV value, set when FSE timedout flow search
 *
 * Return: True if flow_idx value is valid
 */
static bool is_flow_idx_valid(bool flow_invalid, bool flow_timeout)
{
	if (!flow_invalid && !flow_timeout)
		return true;
	else
		return false;
}

#ifdef WLAN_SUPPORT_RX_FISA_HIST
/**
 * dp_rx_fisa_save_pkt_hist() - Save pkt history from rx sw ft entry
 * @ft_entry: sw ft entry
 * @pkt_hist: pkt history ptr
 *
 * Return: None
 */
static inline void
dp_rx_fisa_save_pkt_hist(struct dp_fisa_rx_sw_ft *ft_entry,
			 struct fisa_pkt_hist *pkt_hist)
{
	/* Structure copy by assignment */
	*pkt_hist = ft_entry->pkt_hist;
}

/**
 * dp_rx_fisa_restore_pkt_hist() - Restore rx sw ft entry pkt history
 * @ft_entry: sw ft entry
 * @pkt_hist: pkt history ptr
 *
 * Return: None
 */
static inline void
dp_rx_fisa_restore_pkt_hist(struct dp_fisa_rx_sw_ft *ft_entry,
			    struct fisa_pkt_hist *pkt_hist)
{
	/* Structure copy by assignment */
	ft_entry->pkt_hist = *pkt_hist;
}
#else
static inline void
dp_rx_fisa_save_pkt_hist(struct dp_fisa_rx_sw_ft *ft_entry,
			 struct fisa_pkt_hist *pkt_hist)
{
}

static inline void
dp_rx_fisa_restore_pkt_hist(struct dp_fisa_rx_sw_ft *ft_entry,
			    struct fisa_pkt_hist *pkt_hist)
{
}
#endif

/**
 * dp_fisa_rx_delete_flow() - Delete a flow from SW and HW FST, currently
 * only applicable when FST is in CMEM
 * @fisa_hdl: handle to FISA context
 * @elem: details of the flow which is being added
 * @hashed_flow_idx: hashed flow idx of the deleting flow
 *
 * Return: None
 */
static void
dp_fisa_rx_delete_flow(struct dp_rx_fst *fisa_hdl,
		       struct dp_fisa_rx_fst_update_elem *elem,
		       uint32_t hashed_flow_idx)
{
	struct dp_fisa_rx_sw_ft *sw_ft_entry;
	struct fisa_pkt_hist pkt_hist;
	u8 reo_id;

	sw_ft_entry = &(((struct dp_fisa_rx_sw_ft *)
				fisa_hdl->base)[hashed_flow_idx]);
	reo_id = sw_ft_entry->napi_id;

	dp_rx_fisa_acquire_ft_lock(fisa_hdl, reo_id);

	/* Flush the flow before deletion */
	dp_rx_fisa_flush_flow_wrap(sw_ft_entry);

	dp_rx_fisa_save_pkt_hist(sw_ft_entry, &pkt_hist);
	/* Clear the sw_ft_entry */
	qdf_mem_zero(sw_ft_entry, sizeof(*sw_ft_entry));
	dp_rx_fisa_restore_pkt_hist(sw_ft_entry, &pkt_hist);

	dp_rx_fisa_update_sw_ft_entry(sw_ft_entry, elem->flow_idx, elem->vdev,
				      fisa_hdl->dp_ctx, hashed_flow_idx);

	/* Add HW FT entry */
	sw_ft_entry->cmem_offset = dp_rx_fisa_setup_cmem_fse(
					fisa_hdl, hashed_flow_idx,
					&elem->flow_tuple_info,
					elem->reo_dest_indication);

	sw_ft_entry->is_populated = true;
	sw_ft_entry->napi_id = elem->reo_id;
	sw_ft_entry->reo_dest_indication = elem->reo_dest_indication;
	qdf_mem_copy(&sw_ft_entry->rx_flow_tuple_info, &elem->flow_tuple_info,
		     sizeof(struct cdp_rx_flow_tuple_info));

	sw_ft_entry->is_flow_tcp = elem->is_tcp_flow;
	sw_ft_entry->is_flow_udp = elem->is_udp_flow;
	sw_ft_entry->add_timestamp = qdf_get_log_timestamp();

	fisa_hdl->add_flow_count++;
	fisa_hdl->del_flow_count++;

	dp_rx_fisa_release_ft_lock(fisa_hdl, reo_id);
}

/**
 * dp_fisa_rx_get_hw_ft_timestamp() - Get timestamp maintained in the HW FSE
 * @fisa_hdl: handle to FISA context
 * @hashed_flow_idx: hashed idx of the flow
 *
 * Return: Timestamp
 */
static uint32_t
dp_fisa_rx_get_hw_ft_timestamp(struct dp_rx_fst *fisa_hdl,
			       uint32_t hashed_flow_idx)
{
	hal_soc_handle_t hal_soc_hdl = fisa_hdl->dp_ctx->hal_soc;
	struct dp_fisa_rx_sw_ft *sw_ft_entry;

	sw_ft_entry = &(((struct dp_fisa_rx_sw_ft *)
				fisa_hdl->base)[hashed_flow_idx]);

	if (fisa_hdl->fst_in_cmem)
		return hal_rx_flow_get_cmem_fse_timestamp(
				hal_soc_hdl, sw_ft_entry->cmem_offset);

	return ((struct rx_flow_search_entry *)sw_ft_entry->hw_fse)->timestamp;
}

/**
 * dp_fisa_rx_fst_update() - Core logic which helps in Addition/Deletion
 * of flows
 * into/from SW & HW FST
 * @fisa_hdl: handle to FISA context
 * @elem: details of the flow which is being added
 *
 * Return: None
 */
static void dp_fisa_rx_fst_update(struct dp_rx_fst *fisa_hdl,
				  struct dp_fisa_rx_fst_update_elem *elem)
{
	struct cdp_rx_flow_tuple_info *rx_flow_tuple_info;
	uint32_t skid_count = 0, max_skid_length;
	struct dp_fisa_rx_sw_ft *sw_ft_entry;
	struct wlan_dp_psoc_context *dp_ctx = dp_get_context();
	struct wlan_dp_psoc_cfg *dp_cfg = &dp_ctx->dp_cfg;
	bool is_fst_updated = false;
	uint32_t hashed_flow_idx;
	uint32_t flow_hash;
	uint32_t lru_ft_entry_time = 0xffffffff;
	uint32_t lru_ft_entry_idx = 0;
	uint32_t timestamp;
	uint32_t reo_dest_indication;
	uint64_t sw_timestamp;

	/* Get the hash from TLV
	 * FSE FT Toeplitz hash is same Common parser hash available in TLV
	 * common parser toeplitz hash is same as FSE toeplitz hash as
	 * toeplitz key is same.
	 */
	flow_hash = elem->flow_idx;
	hashed_flow_idx = flow_hash & fisa_hdl->hash_mask;
	max_skid_length = fisa_hdl->max_skid_length;
	rx_flow_tuple_info = &elem->flow_tuple_info;
	reo_dest_indication = elem->reo_dest_indication;

	dp_fisa_debug("flow_hash 0x%x hashed_flow_idx 0x%x", flow_hash,
		      hashed_flow_idx);
	dp_fisa_debug("max_skid_length 0x%x", max_skid_length);

	do {
		sw_ft_entry = &(((struct dp_fisa_rx_sw_ft *)
					fisa_hdl->base)[hashed_flow_idx]);
		if (!sw_ft_entry->is_populated) {
			/* Add SW FT entry */
			dp_rx_fisa_update_sw_ft_entry(sw_ft_entry,
						      flow_hash, elem->vdev,
						      fisa_hdl->dp_ctx,
						      hashed_flow_idx);

			/* Add HW FT entry */
			sw_ft_entry->cmem_offset =
				dp_rx_fisa_setup_cmem_fse(fisa_hdl,
							  hashed_flow_idx,
							  rx_flow_tuple_info,
							  reo_dest_indication);
			sw_ft_entry->is_populated = true;
			sw_ft_entry->napi_id = elem->reo_id;
			sw_ft_entry->reo_dest_indication = reo_dest_indication;
			qdf_mem_copy(&sw_ft_entry->rx_flow_tuple_info,
				     rx_flow_tuple_info,
				     sizeof(struct cdp_rx_flow_tuple_info));

			sw_ft_entry->flow_init_ts = qdf_get_log_timestamp();
			sw_ft_entry->is_flow_tcp = elem->is_tcp_flow;
			sw_ft_entry->is_flow_udp = elem->is_udp_flow;

			sw_ft_entry->add_timestamp = qdf_get_log_timestamp();

			is_fst_updated = true;
			fisa_hdl->add_flow_count++;
			break;
		}
		/* else */
		/* hash collision move to the next FT entry */
		dp_fisa_debug("Hash collision %d",
			      fisa_hdl->hash_collision_cnt);
		fisa_hdl->hash_collision_cnt++;

		timestamp = dp_fisa_rx_get_hw_ft_timestamp(fisa_hdl,
							   hashed_flow_idx);
		if (timestamp < lru_ft_entry_time) {
			lru_ft_entry_time = timestamp;
			lru_ft_entry_idx = hashed_flow_idx;
		}
		skid_count++;
		hashed_flow_idx++;
		hashed_flow_idx &= fisa_hdl->hash_mask;
	} while (skid_count <= max_skid_length);

	/*
	 * if (skid_count > max_skid_length)
	 * Remove LRU flow from HW FT
	 * Remove LRU flow from SW FT
	 */
	if ((skid_count > max_skid_length) &&
	    wlan_dp_cfg_is_rx_fisa_lru_del_enabled(dp_cfg)) {
		dp_fisa_debug("Max skid length reached flow cannot be added, evict exiting flow");

		sw_ft_entry = &(((struct dp_fisa_rx_sw_ft *)
				fisa_hdl->base)[lru_ft_entry_idx]);
		sw_timestamp = qdf_get_log_timestamp();

		if (qdf_log_timestamp_to_usecs(sw_timestamp - sw_ft_entry->add_timestamp) >
			FISA_FT_ENTRY_AGING_US) {
			qdf_spin_unlock_bh(&fisa_hdl->dp_rx_fst_lock);
			dp_fisa_rx_delete_flow(fisa_hdl, elem, lru_ft_entry_idx);
			qdf_spin_lock_bh(&fisa_hdl->dp_rx_fst_lock);
			is_fst_updated = true;
		} else
			dp_fisa_debug("skip update due to aging not complete");
	}

	/**
	 * Send HTT cache invalidation command to firmware to
	 * reflect the flow update
	 */
	if (is_fst_updated &&
	    fisa_hdl->fse_cache_flush_allow &&
	    (qdf_atomic_inc_return(&fisa_hdl->fse_cache_flush_posted) == 1)) {
		/* return 1 after increment implies FSE cache flush message
		 * already posted. so start restart the timer
		 */
		qdf_timer_start(&fisa_hdl->fse_cache_flush_timer,
				FSE_CACHE_FLUSH_TIME_OUT);
	}
}

/**
 * dp_fisa_rx_fst_update_work() - Work functions for FST updates
 * @arg: argument passed to the work function
 *
 * Return: None
 */
void dp_fisa_rx_fst_update_work(void *arg)
{
	struct dp_fisa_rx_fst_update_elem *elem;
	struct dp_rx_fst *fisa_hdl = arg;
	qdf_list_node_t *node;
	hal_soc_handle_t hal_soc_hdl = fisa_hdl->dp_ctx->hal_soc;
	struct dp_vdev *vdev;

	if (qdf_atomic_read(&fisa_hdl->pm_suspended)) {
		dp_err_rl("WQ triggered during suspend stage, deferred update");
		DP_STATS_INC(fisa_hdl, update_deferred, 1);
		return;
	}

	if (hif_force_wake_request(((struct hal_soc *)hal_soc_hdl)->hif_handle)) {
		dp_err("Wake up request failed");
		qdf_check_state_before_panic(__func__, __LINE__);
		return;
	}

	qdf_spin_lock_bh(&fisa_hdl->dp_rx_fst_lock);
	while (qdf_list_peek_front(&fisa_hdl->fst_update_list, &node) ==
	       QDF_STATUS_SUCCESS) {
		elem = (struct dp_fisa_rx_fst_update_elem *)node;
		vdev = dp_vdev_get_ref_by_id(fisa_hdl->soc_hdl,
					     elem->vdev_id,
					     DP_MOD_ID_RX);
		/*
		 * Update fst only if current dp_vdev fetched by vdev_id is
		 * still valid and match with the original dp_vdev when fst
		 * node is queued.
		 */
		if (vdev) {
			if (vdev == elem->vdev)
				dp_fisa_rx_fst_update(fisa_hdl, elem);

			dp_vdev_unref_delete(fisa_hdl->soc_hdl, vdev,
					     DP_MOD_ID_RX);
		}
		qdf_list_remove_front(&fisa_hdl->fst_update_list, &node);
		qdf_mem_free(elem);
	}
	qdf_spin_unlock_bh(&fisa_hdl->dp_rx_fst_lock);

	if (hif_force_wake_release(((struct hal_soc *)hal_soc_hdl)->hif_handle)) {
		dp_err("Wake up release failed");
		qdf_check_state_before_panic(__func__, __LINE__);
		return;
	}
}

/**
 * dp_fisa_rx_is_fst_work_queued() - Check if work is already queued for
 * the flow
 * @fisa_hdl: handle to FISA context
 * @flow_idx: Flow index
 *
 * Return: True/False
 */
static inline bool
dp_fisa_rx_is_fst_work_queued(struct dp_rx_fst *fisa_hdl, uint32_t flow_idx)
{
	struct dp_fisa_rx_fst_update_elem *elem;
	qdf_list_node_t *cur_node, *next_node;
	QDF_STATUS status;

	status = qdf_list_peek_front(&fisa_hdl->fst_update_list, &cur_node);
	if (status == QDF_STATUS_E_EMPTY)
		return false;

	do {
		elem = (struct dp_fisa_rx_fst_update_elem *)cur_node;
		if (elem->flow_idx == flow_idx)
			return true;

		status = qdf_list_peek_next(&fisa_hdl->fst_update_list,
					    cur_node, &next_node);
		cur_node = next_node;
	} while (status == QDF_STATUS_SUCCESS);

	return false;
}

/**
 * dp_fisa_rx_queue_fst_update_work() - Queue FST update work
 * @fisa_hdl: Handle to FISA context
 * @flow_idx: Flow index
 * @nbuf: Received RX packet
 * @vdev: DP vdev handle
 *
 * Return: None
 */
static void *
dp_fisa_rx_queue_fst_update_work(struct dp_rx_fst *fisa_hdl, uint32_t flow_idx,
				 qdf_nbuf_t nbuf, struct dp_vdev *vdev)
{
	hal_soc_handle_t hal_soc_hdl = fisa_hdl->dp_ctx->hal_soc;
	struct cdp_rx_flow_tuple_info flow_tuple_info;
	uint8_t *rx_tlv_hdr = qdf_nbuf_data(nbuf);
	struct dp_fisa_rx_fst_update_elem *elem;
	struct dp_fisa_rx_sw_ft *sw_ft_entry;
	uint32_t hashed_flow_idx;
	uint32_t reo_dest_indication;
	bool found;
	struct hal_proto_params proto_params;

	if (hal_rx_get_proto_params(fisa_hdl->dp_ctx->hal_soc, rx_tlv_hdr,
				    &proto_params))
		return NULL;

	if (proto_params.ipv6_proto ||
	    !(proto_params.tcp_proto || proto_params.udp_proto)) {
		dp_fisa_debug("Not UDP or TCP IPV4 flow");
		return NULL;
	}

	hal_rx_msdu_get_reo_destination_indication(hal_soc_hdl, rx_tlv_hdr,
						   &reo_dest_indication);
	qdf_spin_lock_bh(&fisa_hdl->dp_rx_fst_lock);
	found = dp_fisa_rx_is_fst_work_queued(fisa_hdl, flow_idx);
	qdf_spin_unlock_bh(&fisa_hdl->dp_rx_fst_lock);
	if (found)
		return NULL;

	hashed_flow_idx = flow_idx & fisa_hdl->hash_mask;
	sw_ft_entry = &(((struct dp_fisa_rx_sw_ft *)
				fisa_hdl->base)[hashed_flow_idx]);

	wlan_dp_get_flow_tuple_from_nbuf(fisa_hdl->dp_ctx, &flow_tuple_info,
					 nbuf, rx_tlv_hdr);
	if (flow_tuple_info.bypass_fisa)
		return NULL;

	if (sw_ft_entry->is_populated && is_same_flow(
			&sw_ft_entry->rx_flow_tuple_info, &flow_tuple_info))
		return sw_ft_entry;

	elem = qdf_mem_malloc(sizeof(*elem));
	if (!elem) {
		dp_fisa_debug("failed to allocate memory for FST update");
		return NULL;
	}

	qdf_mem_copy(&elem->flow_tuple_info, &flow_tuple_info,
		     sizeof(struct cdp_rx_flow_tuple_info));
	elem->flow_idx = flow_idx;
	elem->is_tcp_flow = proto_params.tcp_proto;
	elem->is_udp_flow = proto_params.udp_proto;
	elem->reo_id = QDF_NBUF_CB_RX_CTX_ID(nbuf);
	elem->reo_dest_indication = reo_dest_indication;
	elem->vdev = vdev;
	elem->vdev_id = vdev->vdev_id;

	qdf_spin_lock_bh(&fisa_hdl->dp_rx_fst_lock);
	qdf_list_insert_back(&fisa_hdl->fst_update_list, &elem->node);
	qdf_spin_unlock_bh(&fisa_hdl->dp_rx_fst_lock);

	if (qdf_atomic_read(&fisa_hdl->pm_suspended)) {
		fisa_hdl->fst_wq_defer = true;
		dp_info("defer fst update task in WoW");
	} else {
		qdf_queue_work(fisa_hdl->dp_ctx->qdf_dev,
			       fisa_hdl->fst_update_wq,
			       &fisa_hdl->fst_update_work);
	}

	return NULL;
}

/**
 * dp_fisa_rx_get_sw_ft_entry() - Get SW FT entry for the flow
 * @fisa_hdl: Handle to FISA context
 * @nbuf: Received RX packet
 * @flow_idx: Flow index
 * @vdev: handle to DP vdev
 *
 * Return: SW FT entry
 */
static inline struct dp_fisa_rx_sw_ft *
dp_fisa_rx_get_sw_ft_entry(struct dp_rx_fst *fisa_hdl, qdf_nbuf_t nbuf,
			   uint32_t flow_idx, struct dp_vdev *vdev)
{
	hal_soc_handle_t hal_soc_hdl = fisa_hdl->dp_ctx->hal_soc;
	struct dp_fisa_rx_sw_ft *sw_ft_entry = NULL;
	struct dp_fisa_rx_sw_ft *sw_ft_base;
	uint32_t fse_metadata;
	uint8_t *rx_tlv_hdr;

	sw_ft_base = (struct dp_fisa_rx_sw_ft *)fisa_hdl->base;
	rx_tlv_hdr = qdf_nbuf_data(nbuf);

	if (qdf_unlikely(flow_idx >= fisa_hdl->max_entries)) {
		dp_info("flow_idx is invalid 0x%x", flow_idx);
		hal_rx_dump_pkt_tlvs(hal_soc_hdl, rx_tlv_hdr,
				     QDF_TRACE_LEVEL_INFO_HIGH);
		DP_STATS_INC(fisa_hdl, invalid_flow_index, 1);
		return NULL;
	}

	sw_ft_entry = &sw_ft_base[flow_idx];
	if (!sw_ft_entry->is_populated) {
		dp_info("Pkt rx for non configured flow idx 0x%x", flow_idx);
		DP_STATS_INC(fisa_hdl, invalid_flow_index, 1);
		return NULL;
	}

	if (!fisa_hdl->flow_deletion_supported) {
		sw_ft_entry->vdev = vdev;
		sw_ft_entry->vdev_id = vdev->vdev_id;
		sw_ft_entry->dp_intf = dp_fisa_rx_get_dp_intf_for_vdev(vdev);
		return sw_ft_entry;
	}

	/* When a flow is deleted, there could be some packets of that flow
	 * with valid flow_idx in the REO queue and arrive at a later time,
	 * compare the metadata for such packets before returning the SW FT
	 * entry to avoid packets getting aggregated with the wrong flow.
	 */
	fse_metadata = hal_rx_msdu_fse_metadata_get(hal_soc_hdl, rx_tlv_hdr);
	if (fisa_hdl->del_flow_count && fse_metadata != sw_ft_entry->metadata)
		return NULL;

	sw_ft_entry->vdev = vdev;
	sw_ft_entry->vdev_id = vdev->vdev_id;
	sw_ft_entry->dp_intf = dp_fisa_rx_get_dp_intf_for_vdev(vdev);
	return sw_ft_entry;
}

#if defined(DP_OFFLOAD_FRAME_WITH_SW_EXCEPTION)
/*
 * dp_rx_reo_dest_honor_check() - check if packet reo destination is changed
				  by FW offload and is valid
 * @fisa_hdl: handle to FISA context
 *@nbuf: RX packet nbuf
 *@tlv_reo_dest_ind: reo_dest_ind fetched from rx_packet_tlv
 *
 * Return: QDF_STATUS_SUCCESS - reo dest not change/ not valid, others - yes.
 */
static inline QDF_STATUS
dp_rx_reo_dest_honor_check(struct dp_rx_fst *fisa_hdl, qdf_nbuf_t nbuf,
			   uint32_t tlv_reo_dest_ind)
{
	uint8_t sw_exception =
			qdf_nbuf_get_rx_reo_dest_ind_or_sw_excpt(nbuf);

	if (fisa_hdl->rx_hash_enabled &&
	    (tlv_reo_dest_ind < HAL_REO_DEST_IND_START_OFFSET))
		return QDF_STATUS_E_FAILURE;
	/*
	 * If sw_exception bit is marked, then this data packet is
	 * re-injected by FW offload, reo destination will not honor
	 * the original FSE/hash selection, skip FISA.
	 */
	return sw_exception ? QDF_STATUS_E_FAILURE : QDF_STATUS_SUCCESS;
}
#elif defined(WLAN_SOFTUMAC_SUPPORT)
static inline QDF_STATUS
dp_rx_reo_dest_honor_check(struct dp_rx_fst *fisa_hdl, qdf_nbuf_t nbuf,
			   uint32_t tlv_reo_dest_ind)
{
	/* reo_dest_ind_or_sw_excpt in nubf cb is true if the packet is routed
	 * from the FW offloads, Skip FISA for those packets.
	 */
	if (qdf_nbuf_get_rx_reo_dest_ind_or_sw_excpt(nbuf) ||
	    (fisa_hdl->rx_hash_enabled &&
	     (tlv_reo_dest_ind < HAL_REO_DEST_IND_START_OFFSET)))
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
dp_rx_reo_dest_honor_check(struct dp_rx_fst *fisa_hdl, qdf_nbuf_t nbuf,
			   uint32_t tlv_reo_dest_ind)
{
	uint8_t  ring_reo_dest_ind =
			qdf_nbuf_get_rx_reo_dest_ind_or_sw_excpt(nbuf);
	/*
	 * Compare reo_destination_indication between reo ring descriptor
	 * and rx_pkt_tlvs, if they are different, then likely these kind
	 * of frames re-injected by FW or touched by other module already,
	 * skip FISA to avoid REO2SW ring mismatch issue for same flow.
	 */
	if (tlv_reo_dest_ind != ring_reo_dest_ind ||
	    REO_DEST_IND_IPA_REROUTE == ring_reo_dest_ind ||
	    (fisa_hdl->rx_hash_enabled &&
	     (tlv_reo_dest_ind < HAL_REO_DEST_IND_START_OFFSET)))
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dp_rx_get_fisa_flow() - Get FT entry corresponding to incoming nbuf
 * @fisa_hdl: handle to FISA context
 * @vdev: handle to DP vdev
 * @nbuf: incoming msdu
 *
 * Return: handle SW FT entry for nbuf flow
 */
static struct dp_fisa_rx_sw_ft *
dp_rx_get_fisa_flow(struct dp_rx_fst *fisa_hdl, struct dp_vdev *vdev,
		    qdf_nbuf_t nbuf)
{
	uint8_t *rx_tlv_hdr;
	uint32_t flow_idx_hash;
	uint32_t tlv_reo_dest_ind;
	bool flow_invalid, flow_timeout, flow_idx_valid;
	struct dp_fisa_rx_sw_ft *sw_ft_entry = NULL;
	hal_soc_handle_t hal_soc_hdl = fisa_hdl->dp_ctx->hal_soc;
	QDF_STATUS status;

	if (QDF_NBUF_CB_RX_TCP_PROTO(nbuf))
		return sw_ft_entry;

	rx_tlv_hdr = qdf_nbuf_data(nbuf);
	hal_rx_msdu_get_reo_destination_indication(hal_soc_hdl, rx_tlv_hdr,
						   &tlv_reo_dest_ind);
	status = dp_rx_reo_dest_honor_check(fisa_hdl, nbuf, tlv_reo_dest_ind);
	if (QDF_IS_STATUS_ERROR(status))
		return sw_ft_entry;

	hal_rx_msdu_get_flow_params(hal_soc_hdl, rx_tlv_hdr, &flow_invalid,
				    &flow_timeout, &flow_idx_hash);

	flow_idx_valid = is_flow_idx_valid(flow_invalid, flow_timeout);
	if (flow_idx_valid) {
		sw_ft_entry = dp_fisa_rx_get_sw_ft_entry(fisa_hdl, nbuf,
							 flow_idx_hash, vdev);
		goto print_and_return;
	}

	/* else new flow, add entry to FT */

	if (fisa_hdl->fst_in_cmem)
		return dp_fisa_rx_queue_fst_update_work(fisa_hdl, flow_idx_hash,
							nbuf, vdev);

	sw_ft_entry = dp_rx_fisa_add_ft_entry(vdev, fisa_hdl,
					      nbuf,
					      rx_tlv_hdr,
					      flow_idx_hash,
					      tlv_reo_dest_ind);

print_and_return:
	dp_fisa_debug("nbuf %pK fl_idx 0x%x fl_inv %d fl_timeout %d flow_id_toeplitz %x reo_dest_ind 0x%x",
		      nbuf, flow_idx_hash, flow_invalid, flow_timeout,
		      sw_ft_entry ? sw_ft_entry->flow_id_toeplitz : 0,
		      tlv_reo_dest_ind);

	return sw_ft_entry;
}

#ifdef NOT_YET
/**
 * dp_rx_fisa_aggr_tcp() - Aggregate incoming to TCP nbuf
 * @fisa_flow: Handle to SW flow entry, which holds the aggregated nbuf
 * @nbuf: Incoming nbuf
 *
 * Return: FISA_AGGR_DONE on successful aggregation
 */
static enum fisa_aggr_ret
dp_rx_fisa_aggr_tcp(struct dp_fisa_rx_sw_ft *fisa_flow,	qdf_nbuf_t nbuf)
{
	qdf_nbuf_t head_skb = fisa_flow->head_skb;
	qdf_net_iphdr_t *iph;
	uint32_t tcp_data_len;

	fisa_flow->bytes_aggregated += qdf_nbuf_len(nbuf);
	if (!head_skb) {
		/* First nbuf for the flow */
		dp_fisa_debug("first head skb");
		fisa_flow->head_skb = nbuf;
		return FISA_AGGR_DONE;
	}

	tcp_data_len = (qdf_ntohs(iph->ip_len) - sizeof(qdf_net_iphdr_t) -
			sizeof(qdf_net_tcphdr_t));
	qdf_nbuf_pull_head(nbuf, (qdf_nbuf_len(nbuf) - tcp_data_len));

	if (qdf_nbuf_get_ext_list(head_skb)) {
		/* this is 3rd skb after head skb, 2nd skb */
		fisa_flow->last_skb->next = nbuf;
	} else {
		/* 1st skb after head skb */
		qdf_nbuf_append_ext_list(head_skb, nbuf,
					 fisa_flow->cumulative_ip_length);
		qdf_nbuf_set_is_frag(head, 1);
	}

	fisa_flow->last_skb = nbuf;
	fisa_flow->aggr_count++;

	/* move it to while flushing the flow, that is update before flushing */
	return FISA_AGGR_DONE;
}
#else
static enum fisa_aggr_ret
dp_rx_fisa_aggr_tcp(struct dp_rx_fst *fisa_hdl,
		    struct dp_fisa_rx_sw_ft *fisa_flow,	qdf_nbuf_t nbuf)
{
	return FISA_AGGR_DONE;
}
#endif

/**
 * get_transport_payload_offset() - Get offset to payload
 * @fisa_hdl: Handle to FISA context
 * @l3_hdr_offset: layer 3 header offset
 * @l4_hdr_offset: layer 4 header offset
 *
 * Return: Offset value to transport payload
 */
static inline int get_transport_payload_offset(struct dp_rx_fst *fisa_hdl,
					       uint32_t l3_hdr_offset,
					       uint32_t l4_hdr_offset)
{
	/* ETHERNET_HDR_LEN + ip_hdr_len + UDP/TCP; */
	return (l3_hdr_offset + l4_hdr_offset + sizeof(qdf_net_udphdr_t));
}

/**
 * get_transport_header_offset() - Get transport header offset
 * @fisa_flow: Handle to FISA sw flow entry
 * @l3_hdr_offset: layer 3 header offset
 * @l4_hdr_offset: layer 4 header offset
 *
 * Return: Offset value to transport header
 */
static inline
int get_transport_header_offset(struct dp_fisa_rx_sw_ft *fisa_flow,
				uint32_t l3_hdr_offset,
				uint32_t l4_hdr_offset)

{
	/* ETHERNET_HDR_LEN + ip_hdr_len */
	return (l3_hdr_offset + l4_hdr_offset);
}

/**
 * dp_rx_fisa_aggr_udp() - Aggregate incoming to UDP nbuf
 * @fisa_hdl: Handle fisa context
 * @fisa_flow: Handle to SW flow entry, which holds the aggregated nbuf
 * @nbuf: Incoming nbuf
 *
 * Return: FISA_AGGR_DONE on successful aggregation
 */
static enum fisa_aggr_ret
dp_rx_fisa_aggr_udp(struct dp_rx_fst *fisa_hdl,
		    struct dp_fisa_rx_sw_ft *fisa_flow,	qdf_nbuf_t nbuf)
{
	qdf_nbuf_t head_skb = fisa_flow->head_skb;
	uint8_t *rx_tlv_hdr = qdf_nbuf_data(nbuf);
	uint32_t l2_hdr_offset =
		hal_rx_msdu_end_l3_hdr_padding_get(fisa_hdl->dp_ctx->hal_soc,
						   rx_tlv_hdr);
	qdf_net_udphdr_t *udp_hdr;
	uint32_t udp_len;
	uint32_t transport_payload_offset;
	uint32_t l3_hdr_offset, l4_hdr_offset;

	qdf_nbuf_pull_head(nbuf, fisa_hdl->rx_pkt_tlv_size + l2_hdr_offset);

	hal_rx_get_l3_l4_offsets(fisa_hdl->dp_ctx->hal_soc, rx_tlv_hdr,
				 &l3_hdr_offset, &l4_hdr_offset);
	udp_hdr = (qdf_net_udphdr_t *)(qdf_nbuf_data(nbuf) +
			get_transport_header_offset(fisa_flow, l3_hdr_offset,
						    l4_hdr_offset));

	udp_len = qdf_ntohs(udp_hdr->udp_len);

	/**
	 * Incoming nbuf is of size greater than ongoing aggregation
	 * then flush the aggregate and start new aggregation for nbuf
	 */
	if (head_skb &&
	    (udp_len > qdf_ntohs(fisa_flow->head_skb_udp_hdr->udp_len))) {
		/* current msdu should not take into account for flushing */
		fisa_flow->adjusted_cumulative_ip_length -=
					(udp_len - sizeof(qdf_net_udphdr_t));
		fisa_flow->cur_aggr--;
		dp_rx_fisa_flush_flow_wrap(fisa_flow);
		/* napi_flush_cumulative_ip_length  not include current msdu */
		fisa_flow->napi_flush_cumulative_ip_length -= udp_len;
		head_skb = NULL;
	}

	if (!head_skb) {
		dp_fisa_debug("first head skb nbuf %pK", nbuf);
		/* First nbuf for the flow */
		fisa_flow->head_skb = nbuf;
		fisa_flow->head_skb_udp_hdr = udp_hdr;
		fisa_flow->cur_aggr_gso_size = udp_len -
						sizeof(qdf_net_udphdr_t);
		fisa_flow->adjusted_cumulative_ip_length = udp_len;
		fisa_flow->head_skb_ip_hdr_offset = l3_hdr_offset;
		fisa_flow->head_skb_l4_hdr_offset = l4_hdr_offset;

		fisa_flow->frags_cumulative_len = 0;

		return FISA_AGGR_DONE;
	}

	transport_payload_offset =
		get_transport_payload_offset(fisa_hdl, l3_hdr_offset,
					     l4_hdr_offset);

	hex_dump_skb_data(nbuf, false);
	qdf_nbuf_pull_head(nbuf, transport_payload_offset);
	hex_dump_skb_data(nbuf, false);

	fisa_flow->bytes_aggregated += qdf_nbuf_len(nbuf);

	fisa_flow->frags_cumulative_len += (udp_len -
						sizeof(qdf_net_udphdr_t));

	if (qdf_nbuf_get_ext_list(head_skb)) {
		/*
		 * This is 3rd skb for flow.
		 * After head skb, 2nd skb in fraglist
		 */
		if (qdf_likely(fisa_flow->last_skb)) {
			qdf_nbuf_set_next(fisa_flow->last_skb, nbuf);
		} else {
			qdf_nbuf_free(nbuf);
			return FISA_AGGR_DONE;
		}
	} else {
		/* 1st skb after head skb
		 * implement qdf wrapper set_ext_list
		 */
		qdf_nbuf_append_ext_list(head_skb, nbuf, 0);
		qdf_nbuf_set_is_frag(nbuf, 1);
	}

	fisa_flow->last_skb = nbuf;
	fisa_flow->aggr_count++;

	dp_fisa_debug("Stiched head skb fisa_flow %pK", fisa_flow);
	hex_dump_skb_data(fisa_flow->head_skb, false);

	/**
	 * Incoming nbuf is of size less than ongoing aggregation
	 * then flush the aggregate
	 */
	if (udp_len < qdf_ntohs(fisa_flow->head_skb_udp_hdr->udp_len))
		dp_rx_fisa_flush_flow_wrap(fisa_flow);

	return FISA_AGGR_DONE;
}

/**
 * dp_fisa_rx_linear_skb() - Linearize fraglist skb to linear skb
 * @vdev: handle to DP vdev
 * @head_skb: non linear skb
 * @size: Total length of non linear stiched skb
 *
 * Return: Linearized skb pointer
 */
static qdf_nbuf_t dp_fisa_rx_linear_skb(struct dp_vdev *vdev,
					qdf_nbuf_t head_skb, uint32_t size)
{
	return NULL;
}

#ifdef WLAN_FEATURE_11BE
static inline struct dp_vdev *
dp_fisa_rx_get_flow_flush_vdev_ref(ol_txrx_soc_handle cdp_soc,
				   struct dp_fisa_rx_sw_ft *fisa_flow)
{
	struct dp_vdev *fisa_flow_head_skb_vdev;
	struct dp_vdev *fisa_flow_vdev;
	uint8_t vdev_id;

	vdev_id = QDF_NBUF_CB_RX_VDEV_ID(fisa_flow->head_skb);

get_new_vdev_ref:
	fisa_flow_head_skb_vdev = dp_vdev_get_ref_by_id(
						cdp_soc_t_to_dp_soc(cdp_soc),
						vdev_id, DP_MOD_ID_RX);
	if (qdf_unlikely(!fisa_flow_head_skb_vdev)) {
		qdf_nbuf_free(fisa_flow->head_skb);
		goto out;
	}

	if (qdf_unlikely(fisa_flow_head_skb_vdev != fisa_flow->vdev)) {
		if (qdf_unlikely(fisa_flow_head_skb_vdev->vdev_id ==
				 fisa_flow->vdev_id))
			goto fisa_flow_vdev_fail;

		fisa_flow_vdev = dp_vdev_get_ref_by_id(
						cdp_soc_t_to_dp_soc(cdp_soc),
						fisa_flow->vdev_id,
						DP_MOD_ID_RX);
		if (qdf_unlikely(!fisa_flow_vdev))
			goto fisa_flow_vdev_fail;

		if (qdf_unlikely(fisa_flow_vdev != fisa_flow->vdev))
			goto fisa_flow_vdev_mismatch;

		/*
		 * vdev_id may mismatch in case of MLO link switch.
		 * Check if the vdevs belong to same MLD,
		 * if yes, then submit the flow else drop the packets.
		 */
		if (qdf_unlikely(qdf_mem_cmp(
				fisa_flow_vdev->mld_mac_addr.raw,
				fisa_flow_head_skb_vdev->mld_mac_addr.raw,
				QDF_MAC_ADDR_SIZE) != 0)) {
			goto fisa_flow_vdev_mismatch;
		} else {
			fisa_flow->same_mld_vdev_mismatch++;
			/* Continue with aggregation */

			/* Release ref to old vdev */
			dp_vdev_unref_delete(cdp_soc_t_to_dp_soc(cdp_soc),
					     fisa_flow_head_skb_vdev,
					     DP_MOD_ID_RX);

			/*
			 * Update vdev_id and let it loop to find this
			 * vdev by ref.
			 */
			vdev_id = fisa_flow_vdev->vdev_id;
			dp_vdev_unref_delete(cdp_soc_t_to_dp_soc(cdp_soc),
					     fisa_flow_vdev,
					     DP_MOD_ID_RX);
			goto get_new_vdev_ref;
		}
	} else {
		goto out;
	}

fisa_flow_vdev_mismatch:
	dp_vdev_unref_delete(cdp_soc_t_to_dp_soc(cdp_soc),
			     fisa_flow_vdev,
			     DP_MOD_ID_RX);

fisa_flow_vdev_fail:
	qdf_nbuf_free(fisa_flow->head_skb);
	dp_vdev_unref_delete(cdp_soc_t_to_dp_soc(cdp_soc),
			     fisa_flow_head_skb_vdev,
			     DP_MOD_ID_RX);
	fisa_flow_head_skb_vdev = NULL;
out:
	return fisa_flow_head_skb_vdev;
}
#else
static inline struct dp_vdev *
dp_fisa_rx_get_flow_flush_vdev_ref(ol_txrx_soc_handle cdp_soc,
				   struct dp_fisa_rx_sw_ft *fisa_flow)
{
	struct dp_vdev *fisa_flow_head_skb_vdev;

	fisa_flow_head_skb_vdev = dp_vdev_get_ref_by_id(
				cdp_soc_t_to_dp_soc(cdp_soc),
				QDF_NBUF_CB_RX_VDEV_ID(fisa_flow->head_skb),
				DP_MOD_ID_RX);
	if (qdf_unlikely(!fisa_flow_head_skb_vdev ||
			 (fisa_flow_head_skb_vdev != fisa_flow->vdev))) {
		qdf_nbuf_free(fisa_flow->head_skb);
		goto out;
	}

	return fisa_flow_head_skb_vdev;

out:
	if (fisa_flow_head_skb_vdev)
		dp_vdev_unref_delete(cdp_soc_t_to_dp_soc(cdp_soc),
				     fisa_flow_head_skb_vdev,
				     DP_MOD_ID_RX);
	return NULL;
}
#endif

/**
 * dp_rx_fisa_flush_udp_flow() - Flush all aggregated nbuf of the udp flow
 * @vdev: handle to dp_vdev
 * @fisa_flow: Flow for which aggregates to be flushed
 *
 * Return: None
 */
static void
dp_rx_fisa_flush_udp_flow(struct dp_vdev *vdev,
			  struct dp_fisa_rx_sw_ft *fisa_flow)
{
	qdf_nbuf_t head_skb = fisa_flow->head_skb;
	qdf_net_iphdr_t *head_skb_iph;
	qdf_net_udphdr_t *head_skb_udp_hdr;
	qdf_nbuf_shared_info_t shinfo;
	qdf_nbuf_t linear_skb;
	struct dp_vdev *fisa_flow_vdev;
	ol_txrx_soc_handle cdp_soc = fisa_flow->dp_ctx->cdp_soc;

	dp_fisa_debug("head_skb %pK", head_skb);
	dp_fisa_debug("cumulative ip length %d",
		      fisa_flow->adjusted_cumulative_ip_length);
	if (!head_skb) {
		dp_fisa_debug("Already flushed");
		return;
	}

	qdf_nbuf_set_hash(head_skb, QDF_NBUF_CB_RX_FLOW_ID(head_skb));
	head_skb->sw_hash = 1;
	if (qdf_nbuf_get_ext_list(head_skb)) {
		__sum16 pseudo;

		shinfo = qdf_nbuf_get_shinfo(head_skb);
		/* Update the head_skb before flush */
		dp_fisa_debug("cumu ip length host order 0x%x",
			      fisa_flow->adjusted_cumulative_ip_length);
		head_skb_iph = (qdf_net_iphdr_t *)(qdf_nbuf_data(head_skb) +
					fisa_flow->head_skb_ip_hdr_offset);
		dp_fisa_debug("iph ptr %pK", head_skb_iph);

		head_skb_udp_hdr = fisa_flow->head_skb_udp_hdr;

		dp_fisa_debug("udph ptr %pK", head_skb_udp_hdr);

		dp_fisa_debug("ip_len 0x%x", qdf_ntohs(head_skb_iph->ip_len));

		/* data_len is total length of non head_skb,
		 * cumulative ip length is including head_skb ip length also
		 */
		qdf_nbuf_set_data_len(head_skb,
			      ((fisa_flow->adjusted_cumulative_ip_length) -
				      qdf_ntohs(head_skb_udp_hdr->udp_len)));

		qdf_nbuf_set_len(head_skb, (qdf_nbuf_len(head_skb) +
				 qdf_nbuf_get_only_data_len(head_skb)));

		head_skb_iph->ip_len =
			qdf_htons((fisa_flow->adjusted_cumulative_ip_length)
			+ /* IP hdr len */
			fisa_flow->head_skb_l4_hdr_offset);
		pseudo = ~qdf_csum_tcpudp_magic(head_skb_iph->ip_saddr,
				head_skb_iph->ip_daddr,
				fisa_flow->adjusted_cumulative_ip_length,
				head_skb_iph->ip_proto, 0);

		head_skb_iph->ip_check = 0;
		head_skb_iph->ip_check = qdf_ip_fast_csum(head_skb_iph,
							  head_skb_iph->ip_hl);

		head_skb_udp_hdr->udp_len =
			qdf_htons(qdf_ntohs(head_skb_iph->ip_len) -
				  fisa_flow->head_skb_l4_hdr_offset);
		head_skb_udp_hdr->udp_cksum = pseudo;
		qdf_nbuf_set_csum_start(head_skb, ((u8 *)head_skb_udp_hdr -
					qdf_nbuf_head(head_skb)));
		qdf_nbuf_set_csum_offset(head_skb,
					 offsetof(qdf_net_udphdr_t, udp_cksum));

		qdf_nbuf_set_gso_size(head_skb, fisa_flow->cur_aggr_gso_size);
		dp_fisa_debug("gso_size %d, udp_len %d\n",
			      qdf_nbuf_get_gso_size(head_skb),
			      qdf_ntohs(head_skb_udp_hdr->udp_len));
		qdf_nbuf_set_gso_segs(head_skb, fisa_flow->cur_aggr);
		qdf_nbuf_set_gso_type_udp_l4(head_skb);
		qdf_nbuf_set_ip_summed_partial(head_skb);
	}

	qdf_nbuf_set_next(fisa_flow->head_skb, NULL);
	QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(fisa_flow->head_skb) = 1;
	if (fisa_flow->last_skb)
		qdf_nbuf_set_next(fisa_flow->last_skb, NULL);

	hex_dump_skb_data(fisa_flow->head_skb, false);

	fisa_flow_vdev = dp_fisa_rx_get_flow_flush_vdev_ref(cdp_soc, fisa_flow);
	if (!fisa_flow_vdev)
		goto vdev_ref_get_fail;

	dp_fisa_debug("fisa_flow->curr_aggr %d", fisa_flow->cur_aggr);
	linear_skb = dp_fisa_rx_linear_skb(vdev, fisa_flow->head_skb, 24000);
	if (linear_skb) {
		if (!vdev->osif_rx || QDF_STATUS_SUCCESS !=
		    vdev->osif_rx(vdev->osif_vdev, linear_skb))
			qdf_nbuf_free(linear_skb);
		/* Free non linear skb */
		qdf_nbuf_free(fisa_flow->head_skb);
	} else {
		/*
		 * Sanity check head data_len should be equal to sum of
		 * all fragments length
		 */
		if (qdf_unlikely(fisa_flow->frags_cumulative_len !=
				 qdf_nbuf_get_only_data_len(fisa_flow->head_skb))) {
			qdf_assert(0);
			/* Drop the aggregate */
			qdf_nbuf_free(fisa_flow->head_skb);
			goto out;
		}

		if (!vdev->osif_rx || QDF_STATUS_SUCCESS !=
		    vdev->osif_rx(vdev->osif_vdev, fisa_flow->head_skb))
			qdf_nbuf_free(fisa_flow->head_skb);
	}

out:
	if (fisa_flow_vdev)
		dp_vdev_unref_delete(cdp_soc_t_to_dp_soc(cdp_soc),
				     fisa_flow_vdev,
				     DP_MOD_ID_RX);

vdev_ref_get_fail:
	fisa_flow->head_skb = NULL;
	fisa_flow->last_skb = NULL;

	fisa_flow->flush_count++;
}

/**
 * dp_rx_fisa_flush_tcp_flow() - Flush all aggregated nbuf of the TCP flow
 * @vdev: handle to dp_vdev
 * @fisa_flow: Flow for which aggregates to be flushed
 *
 * Return: None
 */
static void
dp_rx_fisa_flush_tcp_flow(struct dp_vdev *vdev,
			  struct dp_fisa_rx_sw_ft *fisa_flow)
{
	qdf_nbuf_t head_skb = fisa_flow->head_skb;
	qdf_net_iphdr_t *head_skb_iph;
	qdf_nbuf_shared_info_t shinfo;

	if (!head_skb) {
		dp_fisa_debug("Already flushed");
		return;
	}

	shinfo = qdf_nbuf_get_shinfo(head_skb);

	/* Update the head_skb before flush */
	head_skb->hash = fisa_flow->flow_hash;
	head_skb->sw_hash = 1;
	shinfo->gso_type = SKB_GSO_UDP_L4;

	head_skb_iph = (qdf_net_iphdr_t *)(qdf_nbuf_data(head_skb) +
					fisa_flow->head_skb_ip_hdr_offset);

	head_skb_iph->ip_len = fisa_flow->adjusted_cumulative_ip_length;
	head_skb_iph->ip_check = ip_fast_csum((u8 *)head_skb_iph,
					      head_skb_iph->ip_hl);

	qdf_nbuf_set_next(fisa_flow->head_skb, NULL);
	if (fisa_flow->last_skb)
		qdf_nbuf_set_next(fisa_flow->last_skb, NULL);
	vdev->osif_rx(vdev->osif_vdev, fisa_flow->head_skb);

	fisa_flow->head_skb = NULL;

	fisa_flow->flush_count++;
}

/**
 * dp_rx_fisa_flush_flow() - Flush all aggregated nbuf of the flow
 * @vdev: handle to dp_vdev
 * @flow: Flow for which aggregates to be flushed
 *
 * Return: None
 */
static void dp_rx_fisa_flush_flow(struct dp_vdev *vdev,
				  struct dp_fisa_rx_sw_ft *flow)
{
	dp_fisa_debug("dp_rx_fisa_flush_flow");

	if (flow->is_flow_udp)
		dp_rx_fisa_flush_udp_flow(vdev, flow);
	else
		dp_rx_fisa_flush_tcp_flow(vdev, flow);
}

/**
 * dp_fisa_aggregation_should_stop - check if fisa aggregate should stop
 * @fisa_flow: Handle SW flow entry
 * @hal_aggr_count: current aggregate count from RX PKT TLV
 * @hal_cumulative_ip_len: current cumulative ip length from RX PKT TLV
 * @rx_tlv_hdr: current msdu RX PKT TLV
 * @nbuf: incoming nbuf
 *
 * Return: true - current flow aggregation should stop,
 *	   false - continue to aggregate.
 */
static bool dp_fisa_aggregation_should_stop(
				struct dp_fisa_rx_sw_ft *fisa_flow,
				uint32_t hal_aggr_count,
				uint16_t hal_cumulative_ip_len,
				uint8_t *rx_tlv_hdr, qdf_nbuf_t nbuf)
{
	uint32_t msdu_len =
		hal_rx_msdu_start_msdu_len_get(fisa_flow->dp_ctx->hal_soc,
					       rx_tlv_hdr);
	uint32_t l3_hdr_offset, l4_hdr_offset, l2_l3_hdr_len;
	uint32_t cumulative_ip_len_delta = hal_cumulative_ip_len -
					   fisa_flow->hal_cumultive_ip_len;
	uint32_t ip_csum_err = 0;
	uint32_t tcp_udp_csum_err = 0;
	uint32_t ip_frag;

	hal_rx_tlv_csum_err_get(fisa_flow->dp_ctx->hal_soc, rx_tlv_hdr,
				&ip_csum_err, &tcp_udp_csum_err, &ip_frag);

	hal_rx_get_l3_l4_offsets(fisa_flow->dp_ctx->hal_soc, rx_tlv_hdr,
				 &l3_hdr_offset, &l4_hdr_offset);

	l2_l3_hdr_len = l3_hdr_offset + l4_hdr_offset;

	/**
	 * If l3/l4 checksum validation failed for MSDU, then data
	 * is not trust worthy to build aggregated skb, so do not
	 * allow for aggregation. And also in aggregated case it
	 * is job of driver to make sure checksum is valid before
	 * computing partial checksum for final aggregated skb.
	 *
	 * kernel network panic if UDP data length < 12 bytes get aggregated,
	 * no solid conclusion currently, as a SW WAR, only allow UDP
	 * aggregation if UDP data length >= 16 bytes.
	 *
	 * current cumulative ip length should > last cumulative_ip_len
	 * and <= last cumulative_ip_len + 1478, also current aggregate
	 * count should be equal to last aggregate count + 1,
	 * cumulative_ip_len delta should be equal to current msdu length
	 * - l4 header offset,
	 * otherwise, current fisa flow aggregation should be stopped.
	 */
	if (fisa_flow->do_not_aggregate ||
	    (ip_csum_err || tcp_udp_csum_err) ||
	    msdu_len < (l2_l3_hdr_len + FISA_MIN_L4_AND_DATA_LEN) ||
	    hal_cumulative_ip_len <= fisa_flow->hal_cumultive_ip_len ||
	    cumulative_ip_len_delta > FISA_MAX_SINGLE_CUMULATIVE_IP_LEN ||
	    (fisa_flow->last_hal_aggr_count + 1) != hal_aggr_count ||
	    cumulative_ip_len_delta != (msdu_len - l2_l3_hdr_len) ||
	    msdu_len != QDF_NBUF_CB_RX_PKT_LEN(nbuf))
		return true;

	return false;
}

/**
 * dp_add_nbuf_to_fisa_flow() - Aggregate incoming nbuf
 * @fisa_hdl: handle to fisa context
 * @vdev: handle DP vdev
 * @nbuf: Incoming nbuf
 * @fisa_flow: Handle SW flow entry
 *
 * Return: Success on aggregation
 */
static int dp_add_nbuf_to_fisa_flow(struct dp_rx_fst *fisa_hdl,
				    struct dp_vdev *vdev, qdf_nbuf_t nbuf,
				    struct dp_fisa_rx_sw_ft *fisa_flow)
{
	bool flow_aggr_cont;
	uint8_t *rx_tlv_hdr = qdf_nbuf_data(nbuf);
	uint16_t hal_cumulative_ip_len;
	hal_soc_handle_t hal_soc_hdl = fisa_hdl->dp_ctx->hal_soc;
	uint32_t hal_aggr_count;
	uint8_t napi_id = QDF_NBUF_CB_RX_CTX_ID(nbuf);
	uint32_t fse_metadata;
	bool cce_match;

	dump_tlvs(hal_soc_hdl, rx_tlv_hdr, QDF_TRACE_LEVEL_INFO_HIGH);
	dp_fisa_debug("nbuf: %pK nbuf->next:%pK nbuf->data:%pK len %d data_len %d",
		      nbuf, qdf_nbuf_next(nbuf), qdf_nbuf_data(nbuf),
		      qdf_nbuf_len(nbuf), qdf_nbuf_get_only_data_len(nbuf));

	/* Packets of the flow are arriving on a different REO than
	 * the one configured.
	 */
	if (qdf_unlikely(fisa_flow->napi_id != napi_id)) {
		fse_metadata =
			hal_rx_msdu_fse_metadata_get(hal_soc_hdl, rx_tlv_hdr);
		cce_match = hal_rx_msdu_cce_match_get(hal_soc_hdl, rx_tlv_hdr);
		/*
		 * For two cases the fse_metadata will not match the metadata
		 * from the fisa_flow_table entry
		 * 1) Flow has been evicted (lru deletion), and this packet is
		 * one of the few packets pending in the rx ring from the prev
		 * flow
		 * 2) HW flow table match fails for some packets in the
		 * currently active flow.
		 */
		if (cce_match) {
			DP_STATS_INC(fisa_hdl, reo_mismatch.allow_cce_match,
				     1);
			return FISA_AGGR_NOT_ELIGIBLE;
		}

		if (fse_metadata != fisa_flow->metadata) {
			DP_STATS_INC(fisa_hdl,
				     reo_mismatch.allow_fse_metdata_mismatch,
				     1);
			return FISA_AGGR_NOT_ELIGIBLE;
		}

		dp_err("REO id mismatch flow: %pK napi_id: %u nbuf: %pK reo_id: %u",
		       fisa_flow, fisa_flow->napi_id, nbuf, napi_id);
		DP_STATS_INC(fisa_hdl, reo_mismatch.allow_non_aggr, 1);
		QDF_BUG(0);
		return FISA_AGGR_NOT_ELIGIBLE;
	}

	hal_cumulative_ip_len = hal_rx_get_fisa_cumulative_ip_length(
								hal_soc_hdl,
								rx_tlv_hdr);
	flow_aggr_cont = hal_rx_get_fisa_flow_agg_continuation(hal_soc_hdl,
							       rx_tlv_hdr);
	hal_aggr_count = hal_rx_get_fisa_flow_agg_count(hal_soc_hdl,
							rx_tlv_hdr);

	if (!flow_aggr_cont) {
		/* Start of new aggregation for the flow
		 * Flush previous aggregates for this flow
		 */
		dp_fisa_debug("no fgc nbuf %pK, flush %pK napi %d", nbuf,
			      fisa_flow, QDF_NBUF_CB_RX_CTX_ID(nbuf));
		dp_rx_fisa_flush_flow(vdev, fisa_flow);
		/* Clear of previoud context values */
		fisa_flow->napi_flush_cumulative_l4_checksum = 0;
		fisa_flow->napi_flush_cumulative_ip_length = 0;
		fisa_flow->cur_aggr = 0;
		fisa_flow->do_not_aggregate = false;
		fisa_flow->hal_cumultive_ip_len = 0;
		fisa_flow->last_hal_aggr_count = 0;
		/* Check fisa related HW TLV correct or not */
		if (qdf_unlikely(dp_fisa_aggregation_should_stop(
						fisa_flow,
						hal_aggr_count,
						hal_cumulative_ip_len,
						rx_tlv_hdr, nbuf))) {
			qdf_assert(0);
			fisa_flow->do_not_aggregate = true;
			/*
			 * do not aggregate until next new aggregation
			 * start.
			 */
			goto invalid_fisa_assist;
		}
	} else if (qdf_unlikely(dp_fisa_aggregation_should_stop(
						fisa_flow,
						hal_aggr_count,
						hal_cumulative_ip_len,
						rx_tlv_hdr, nbuf))) {
		qdf_assert(0);
		/* Either HW cumulative ip length is wrong, or packet is missed
		 * Flush the flow and do not aggregate until next start new
		 * aggreagtion
		 */
		dp_rx_fisa_flush_flow(vdev, fisa_flow);
		fisa_flow->do_not_aggregate = true;
		fisa_flow->cur_aggr = 0;
		fisa_flow->napi_flush_cumulative_ip_length = 0;
		goto invalid_fisa_assist;
	} else {
		/* takecare to skip the udp hdr len for sub sequent cumulative
		 * length
		 */
		fisa_flow->cur_aggr++;
	}

	dp_fisa_debug("nbuf %pK cumulat_ip_length %d flow %pK fl aggr cont %d",
		      nbuf, hal_cumulative_ip_len, fisa_flow, flow_aggr_cont);

	fisa_flow->aggr_count++;
	fisa_flow->last_hal_aggr_count = hal_aggr_count;
	fisa_flow->hal_cumultive_ip_len = hal_cumulative_ip_len;

	if (!fisa_flow->head_skb) {
		/* This is start of aggregation for the flow, save the offsets*/
		fisa_flow->napi_flush_cumulative_l4_checksum = 0;
		fisa_flow->cur_aggr = 0;
	}

	fisa_flow->adjusted_cumulative_ip_length =
		/* cumulative ip len has all the aggr msdu udp header len
		 * Aggr UDP msdu has one UDP header len
		 */
		(hal_cumulative_ip_len -
		(fisa_flow->cur_aggr * sizeof(qdf_net_udphdr_t))) -
		fisa_flow->napi_flush_cumulative_ip_length;

	/**
	 * cur_aggr does not include the head_skb, so compare with
	 * FISA_FLOW_MAX_AGGR_COUNT - 1.
	 */
	if (fisa_flow->cur_aggr > (FISA_FLOW_MAX_AGGR_COUNT - 1))
		dp_err("HAL cumulative_ip_length %d", hal_cumulative_ip_len);

	dp_fisa_debug("hal cum_len 0x%x - napI_cumu_len 0x%x = flow_cum_len 0x%x cur_aggr %d",
		      hal_cumulative_ip_len,
		      fisa_flow->napi_flush_cumulative_ip_length,
		      fisa_flow->adjusted_cumulative_ip_length,
		      fisa_flow->cur_aggr);

	if (fisa_flow->adjusted_cumulative_ip_length >
	    FISA_FLOW_MAX_CUMULATIVE_IP_LEN) {
		dp_err("fisa_flow %pK nbuf %pK", fisa_flow, nbuf);
		dp_err("fisa_flow->adjusted_cumulative_ip_length %d",
		       fisa_flow->adjusted_cumulative_ip_length);
		dp_err("HAL cumulative_ip_length %d", hal_cumulative_ip_len);
		dp_err("napi_flush_cumulative_ip_length %d",
		       fisa_flow->napi_flush_cumulative_ip_length);
		qdf_assert(0);
	}

	dp_fisa_record_pkt(fisa_flow, nbuf, rx_tlv_hdr,
			   fisa_hdl->rx_pkt_tlv_size);

	if (fisa_flow->is_flow_udp) {
		dp_rx_fisa_aggr_udp(fisa_hdl, fisa_flow, nbuf);
	} else if (fisa_flow->is_flow_tcp) {
		qdf_assert(0);
		dp_rx_fisa_aggr_tcp(fisa_hdl, fisa_flow, nbuf);
	}

	fisa_flow->last_accessed_ts = qdf_get_log_timestamp();

	return FISA_AGGR_DONE;

invalid_fisa_assist:
	/* Not eligible aggregation deliver frame without FISA */
	return FISA_AGGR_NOT_ELIGIBLE;
}

/**
 * dp_is_nbuf_bypass_fisa() - FISA bypass check for RX frame
 * @nbuf: RX nbuf pointer
 *
 * Return: true if FISA should be bypassed else false
 */
static bool dp_is_nbuf_bypass_fisa(qdf_nbuf_t nbuf)
{
	/* RX frame from non-regular path or DHCP packet */
	if (QDF_NBUF_CB_RX_TCP_PROTO(nbuf) ||
	    qdf_nbuf_is_exc_frame(nbuf) ||
	    qdf_nbuf_is_ipv4_dhcp_pkt(nbuf) ||
	    qdf_nbuf_is_da_mcbc(nbuf))
		return true;

	return false;
}

/**
 * dp_rx_fisa_flush_by_intf_ctx_id() - Flush fisa aggregates per dp_interface
 *				       and rx context id
 * @dp_intf: DP interface handle
 * @rx_ctx_id: Rx context id
 *
 * Return: Success on flushing the flows for the vdev and rx ctx id
 */
static
QDF_STATUS dp_rx_fisa_flush_by_intf_ctx_id(struct wlan_dp_intf *dp_intf,
					   uint8_t rx_ctx_id)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_get_context();
	struct dp_rx_fst *fisa_hdl = dp_ctx->rx_fst;
	struct dp_fisa_rx_sw_ft *sw_ft_entry =
		(struct dp_fisa_rx_sw_ft *)fisa_hdl->base;
	int ft_size = fisa_hdl->max_entries;
	int i;

	dp_rx_fisa_acquire_ft_lock(fisa_hdl, rx_ctx_id);
	for (i = 0; i < ft_size; i++) {
		if (sw_ft_entry[i].is_populated &&
		    dp_intf == sw_ft_entry[i].dp_intf &&
		    sw_ft_entry[i].napi_id == rx_ctx_id) {
			dp_fisa_debug("flushing %d %pk dp_intf %pK napi id:%d",
				      i, &sw_ft_entry[i], dp_intf, rx_ctx_id);
			dp_rx_fisa_flush_flow_wrap(&sw_ft_entry[i]);
		}
	}
	dp_rx_fisa_release_ft_lock(fisa_hdl, rx_ctx_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_fisa_disallowed_for_vdev() - Check if fisa is allowed on vdev
 * @soc: core txrx main context
 * @vdev: Handle DP vdev
 * @rx_ctx_id: Rx context id
 *
 * Return: true if fisa is disallowed for vdev else false
 */
static bool dp_fisa_disallowed_for_vdev(struct dp_soc *soc,
					struct dp_vdev *vdev,
					uint8_t rx_ctx_id)
{
	struct wlan_dp_intf *dp_intf;

	dp_intf = dp_fisa_rx_get_dp_intf_for_vdev(vdev);
	if (!dp_intf->fisa_disallowed[rx_ctx_id]) {
		if (dp_intf->fisa_force_flushed[rx_ctx_id])
			dp_intf->fisa_force_flushed[rx_ctx_id] = 0;
		return false;
	}

	if (!dp_intf->fisa_force_flushed[rx_ctx_id]) {
		dp_rx_fisa_flush_by_intf_ctx_id(dp_intf, rx_ctx_id);
		dp_intf->fisa_force_flushed[rx_ctx_id] = 1;
	}

	return true;
}

QDF_STATUS dp_fisa_rx(struct wlan_dp_psoc_context *dp_ctx,
		      struct dp_vdev *vdev,
		      qdf_nbuf_t nbuf_list)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(dp_ctx->cdp_soc);
	struct dp_rx_fst *dp_fisa_rx_hdl = dp_ctx->rx_fst;
	qdf_nbuf_t head_nbuf;
	qdf_nbuf_t next_nbuf;
	struct dp_fisa_rx_sw_ft *fisa_flow;
	int fisa_ret;
	uint8_t rx_ctx_id = QDF_NBUF_CB_RX_CTX_ID(nbuf_list);
	uint32_t tlv_reo_dest_ind;
	uint8_t reo_id;

	head_nbuf = nbuf_list;

	while (head_nbuf) {
		next_nbuf = head_nbuf->next;
		qdf_nbuf_set_next(head_nbuf, NULL);

		/* bypass FISA check */
		if (dp_is_nbuf_bypass_fisa(head_nbuf))
			goto deliver_nbuf;

		if (dp_fisa_disallowed_for_vdev(soc, vdev, rx_ctx_id))
			goto deliver_nbuf;

		if (qdf_atomic_read(&dp_ctx->skip_fisa_param.skip_fisa)) {
			if (!dp_ctx->skip_fisa_param.fisa_force_flush[rx_ctx_id]) {
				dp_rx_fisa_flush_by_ctx_id(soc, rx_ctx_id);
				dp_ctx->skip_fisa_param.
						fisa_force_flush[rx_ctx_id] = 1;
			}
			goto deliver_nbuf;
		} else if (dp_ctx->skip_fisa_param.fisa_force_flush[rx_ctx_id]) {
			dp_ctx->skip_fisa_param.fisa_force_flush[rx_ctx_id] = 0;
		}

		qdf_nbuf_push_head(head_nbuf, dp_fisa_rx_hdl->rx_pkt_tlv_size +
				   QDF_NBUF_CB_RX_PACKET_L3_HDR_PAD(head_nbuf));

		hal_rx_msdu_get_reo_destination_indication(dp_ctx->hal_soc,
							   (uint8_t *)qdf_nbuf_data(head_nbuf),
							   &tlv_reo_dest_ind);

		/* Skip FISA aggregation and drop the frame if RDI is REO2TCL. */
		if (qdf_unlikely(tlv_reo_dest_ind == REO_REMAP_TCL)) {
			qdf_nbuf_free(head_nbuf);
			head_nbuf = next_nbuf;
			DP_STATS_INC(dp_fisa_rx_hdl, incorrect_rdi, 1);
			continue;
		}

		reo_id = QDF_NBUF_CB_RX_CTX_ID(head_nbuf);
		dp_rx_fisa_acquire_ft_lock(dp_fisa_rx_hdl, reo_id);

		/* Add new flow if the there is no ongoing flow */
		fisa_flow = dp_rx_get_fisa_flow(dp_fisa_rx_hdl, vdev,
						head_nbuf);

		/* Do not FISA aggregate IPSec packets */
		if (fisa_flow &&
		    fisa_flow->rx_flow_tuple_info.is_exception) {
			dp_rx_fisa_release_ft_lock(dp_fisa_rx_hdl, reo_id);
			goto pull_nbuf;
		}

		/* Fragmented skb do not handle via fisa
		 * get that flow and deliver that flow to rx_thread
		 */
		if (qdf_unlikely(qdf_nbuf_get_ext_list(head_nbuf))) {
			dp_fisa_debug("Fragmented skb, will not be FISAed");
			if (fisa_flow)
				dp_rx_fisa_flush_flow(vdev, fisa_flow);

			dp_rx_fisa_release_ft_lock(dp_fisa_rx_hdl, reo_id);
			goto pull_nbuf;
		}

		if (!fisa_flow) {
			dp_rx_fisa_release_ft_lock(dp_fisa_rx_hdl, reo_id);
			goto pull_nbuf;
		}

		fisa_ret = dp_add_nbuf_to_fisa_flow(dp_fisa_rx_hdl, vdev,
						    head_nbuf, fisa_flow);

		dp_rx_fisa_release_ft_lock(dp_fisa_rx_hdl, reo_id);

		if (fisa_ret == FISA_AGGR_DONE)
			goto next_msdu;

pull_nbuf:
		wlan_dp_nbuf_skip_rx_pkt_tlv(dp_ctx, dp_fisa_rx_hdl, head_nbuf);

deliver_nbuf: /* Deliver without FISA */
		QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(head_nbuf) = 1;
		qdf_nbuf_set_next(head_nbuf, NULL);
		hex_dump_skb_data(head_nbuf, false);
		if (!vdev->osif_rx || QDF_STATUS_SUCCESS !=
		    vdev->osif_rx(vdev->osif_vdev, head_nbuf))
			qdf_nbuf_free(head_nbuf);
next_msdu:
		head_nbuf = next_nbuf;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_fisa_flush_flow_wrap() - flush fisa flow by invoking
 *				  dp_rx_fisa_flush_flow()
 * @sw_ft: fisa flow for which aggregates to be flushed
 *
 * Return: None.
 */
static void dp_rx_fisa_flush_flow_wrap(struct dp_fisa_rx_sw_ft *sw_ft)
{
	/* Save the ip_len and checksum as hardware assist is
	 * always based on his start of aggregation
	 */
	sw_ft->napi_flush_cumulative_l4_checksum =
				sw_ft->cumulative_l4_checksum;
	sw_ft->napi_flush_cumulative_ip_length =
				sw_ft->hal_cumultive_ip_len;
	dp_fisa_debug("napi_flush_cumulative_ip_length 0x%x",
		      sw_ft->napi_flush_cumulative_ip_length);

	dp_rx_fisa_flush_flow(sw_ft->vdev,
			      sw_ft);
	sw_ft->cur_aggr = 0;
}

QDF_STATUS dp_rx_fisa_flush_by_ctx_id(struct dp_soc *soc, int napi_id)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_get_context();
	struct dp_rx_fst *fisa_hdl = dp_ctx->rx_fst;
	struct dp_fisa_rx_sw_ft *sw_ft_entry =
		(struct dp_fisa_rx_sw_ft *)fisa_hdl->base;
	int ft_size = fisa_hdl->max_entries;
	int i;

	dp_rx_fisa_acquire_ft_lock(fisa_hdl, napi_id);
	for (i = 0; i < ft_size; i++) {
		if (sw_ft_entry[i].napi_id == napi_id &&
		    sw_ft_entry[i].is_populated) {
			dp_fisa_debug("flushing %d %pK napi_id %d", i,
				      &sw_ft_entry[i], napi_id);
			dp_rx_fisa_flush_flow_wrap(&sw_ft_entry[i]);
		}
	}
	dp_rx_fisa_release_ft_lock(fisa_hdl, napi_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_rx_fisa_flush_by_vdev_id(struct dp_soc *soc, uint8_t vdev_id)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_get_context();
	struct dp_rx_fst *fisa_hdl = dp_ctx->rx_fst;
	struct dp_fisa_rx_sw_ft *sw_ft_entry =
		(struct dp_fisa_rx_sw_ft *)fisa_hdl->base;
	int ft_size = fisa_hdl->max_entries;
	int i;
	struct dp_vdev *vdev;
	uint8_t reo_id;

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_RX);
	if (qdf_unlikely(!vdev)) {
		dp_err("null vdev by vdev_id %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < ft_size; i++) {
		reo_id = sw_ft_entry[i].napi_id;
		if (reo_id >= MAX_REO_DEST_RINGS)
			continue;
		dp_rx_fisa_acquire_ft_lock(fisa_hdl, reo_id);
		if (vdev == sw_ft_entry[i].vdev) {
			dp_fisa_debug("flushing %d %pk vdev %pK", i,
				      &sw_ft_entry[i], vdev);

			dp_rx_fisa_flush_flow_wrap(&sw_ft_entry[i]);
		}
		dp_rx_fisa_release_ft_lock(fisa_hdl, reo_id);
	}
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_RX);

	return QDF_STATUS_SUCCESS;
}

void dp_suspend_fse_cache_flush(struct wlan_dp_psoc_context *dp_ctx)
{
	struct dp_rx_fst *dp_fst;

	dp_fst = dp_ctx->rx_fst;
	if (dp_fst) {
		if (qdf_atomic_read(&dp_fst->fse_cache_flush_posted))
			qdf_timer_sync_cancel(&dp_fst->fse_cache_flush_timer);
		dp_fst->fse_cache_flush_allow = false;
	}

	dp_info("fse cache flush suspended");
}

void dp_resume_fse_cache_flush(struct wlan_dp_psoc_context *dp_ctx)
{
	struct dp_rx_fst *dp_fst;

	dp_fst = dp_ctx->rx_fst;
	if (dp_fst) {
		qdf_atomic_set(&dp_fst->fse_cache_flush_posted, 0);
		dp_fst->fse_cache_flush_allow = true;
	}

	dp_info("fse cache flush resumed");
}

void dp_set_fisa_dynamic_aggr_size_support(bool dynamic_aggr_size_support)
{
	struct wlan_dp_psoc_context *dp_ctx = dp_get_context();

	dp_ctx->fisa_dynamic_aggr_size_support = dynamic_aggr_size_support;
}
