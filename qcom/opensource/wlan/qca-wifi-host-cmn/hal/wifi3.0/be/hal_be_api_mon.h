/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _HAL_BE_API_MON_H_
#define _HAL_BE_API_MON_H_

#include "hal_be_hw_headers.h"
#if defined(WLAN_PKT_CAPTURE_TX_2_0) || \
defined(WLAN_PKT_CAPTURE_RX_2_0)
#include <mon_ingress_ring.h>
#include <mon_destination_ring.h>
#include <mon_drop.h>
#endif
#include <hal_be_hw_headers.h>
#include "hal_api_mon.h"
#include <hal_generic_api.h>
#include <hal_generic_api.h>
#include <hal_api_mon.h>

#define HAL_RX_PPDU_START_PHY_PPDU_ID_OFFSET                        0x00000000
#define HAL_RX_PPDU_START_PHY_PPDU_ID_LSB                           0
#define HAL_RX_PPDU_START_PHY_PPDU_ID_MSB                           15
#define HAL_RX_PPDU_START_PHY_PPDU_ID_MASK                          0x0000ffff

#define HAL_RX_PPDU_START_SW_PHY_META_DATA_OFFSET                   0x00000004
#define HAL_RX_PPDU_START_SW_PHY_META_DATA_LSB                      0
#define HAL_RX_PPDU_START_SW_PHY_META_DATA_MSB                      31
#define HAL_RX_PPDU_START_SW_PHY_META_DATA_MASK                     0xffffffff

#define HAL_RX_PPDU_START_PPDU_START_TIMESTAMP_31_0_OFFSET          0x00000008
#define HAL_RX_PPDU_START_PPDU_START_TIMESTAMP_31_0_LSB             0
#define HAL_RX_PPDU_START_PPDU_START_TIMESTAMP_31_0_MSB             31
#define HAL_RX_PPDU_START_PPDU_START_TIMESTAMP_31_0_MASK            0xffffffff

#define HAL_RXPCU_PPDU_END_INFO_WB_TIMESTAMP_LOWER_32_OFFSET        0x00000000
#define HAL_RXPCU_PPDU_END_INFO_WB_TIMESTAMP_LOWER_32_LSB           0
#define HAL_RXPCU_PPDU_END_INFO_WB_TIMESTAMP_LOWER_32_MSB           31
#define HAL_RXPCU_PPDU_END_INFO_WB_TIMESTAMP_LOWER_32_MASK          0xffffffff

#define HAL_RXPCU_PPDU_END_INFO_WB_TIMESTAMP_UPPER_32_OFFSET        0x00000004
#define HAL_RXPCU_PPDU_END_INFO_WB_TIMESTAMP_UPPER_32_LSB           0
#define HAL_RXPCU_PPDU_END_INFO_WB_TIMESTAMP_UPPER_32_MSB           31
#define HAL_RXPCU_PPDU_END_INFO_WB_TIMESTAMP_UPPER_32_MASK          0xffffffff

#define HAL_RXPCU_PPDU_END_INFO_RX_ANTENNA_OFFSET                   0x00000008
#define HAL_RXPCU_PPDU_END_INFO_RX_ANTENNA_LSB                      0
#define HAL_RXPCU_PPDU_END_INFO_RX_ANTENNA_MSB                      23
#define HAL_RXPCU_PPDU_END_INFO_RX_ANTENNA_MASK                     0x00ffffff

#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_95_64_OFFSET   0x00000004
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_95_64_LSB      0
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_95_64_MSB      31
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_95_64_MASK     0xffffffff

#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_127_96_OFFSET  0x00000008
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_127_96_LSB     0
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_127_96_MSB     31
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_127_96_MASK    0xffffffff

#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_159_128_OFFSET 0x0000000c
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_159_128_LSB    0
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_159_128_MSB    31
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_159_128_MASK   0xffffffff

#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_191_160_OFFSET 0x00000010
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_191_160_LSB    0
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_191_160_MSB    31
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_191_160_MASK   0xffffffff

#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_223_192_OFFSET 0x00000014
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_223_192_LSB    0
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_223_192_MSB    31
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_223_192_MASK   0xffffffff

#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_255_224_OFFSET 0x00000018
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_255_224_LSB    0
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_255_224_MSB    31
#define HAL_RX_PPDU_END_USER_STATS_EXT_FCS_OK_BITMAP_255_224_MASK   0xffffffff

#define HAL_RXPCU_PPDU_END_INFO_RX_PPDU_DURATION_OFFSET             0x00000024
#define HAL_RXPCU_PPDU_END_INFO_RX_PPDU_DURATION_LSB                0
#define HAL_RXPCU_PPDU_END_INFO_RX_PPDU_DURATION_MSB                23
#define HAL_RXPCU_PPDU_END_INFO_RX_PPDU_DURATION_MASK               0x00ffffff

#define HAL_PHYRX_RSSI_LEGACY_RECEPTION_TYPE_OFFSET                 0x00000000
#define HAL_PHYRX_RSSI_LEGACY_RECEPTION_TYPE_LSB                    0
#define HAL_PHYRX_RSSI_LEGACY_RECEPTION_TYPE_MSB                    3
#define HAL_PHYRX_RSSI_LEGACY_RECEPTION_TYPE_MASK                   0x0000000f

#define HAL_RX_MPDU_END_FCS_ERR_OFFSET                              0x00000004
#define HAL_RX_MPDU_END_FCS_ERR_LSB                                 19
#define HAL_RX_MPDU_END_FCS_ERR_MSB                                 19
#define HAL_RX_MPDU_END_FCS_ERR_MASK                                0x00080000

#if defined(WLAN_PKT_CAPTURE_TX_2_0) || \
defined(WLAN_PKT_CAPTURE_RX_2_0) || \
defined(QCA_SINGLE_WIFI_3_0)
#define HAL_MON_BUFFER_ADDR_INFO_0_BUFFER_ADDR_31_0_OFFSET 0x00000000
#define HAL_MON_BUFFER_ADDR_INFO_0_BUFFER_ADDR_31_0_LSB 0
#define HAL_MON_BUFFER_ADDR_INFO_0_BUFFER_ADDR_31_0_MASK 0xffffffff

#define HAL_MON_BUFFER_ADDR_INFO_1_BUFFER_ADDR_39_32_OFFSET 0x00000004
#define HAL_MON_BUFFER_ADDR_INFO_1_BUFFER_ADDR_39_32_LSB 0
#define HAL_MON_BUFFER_ADDR_INFO_1_BUFFER_ADDR_39_32_MASK 0x000000ff

#define HAL_MON_MON_INGRESS_RING_BUFFER_VIRT_ADDR_31_0_OFFSET 0x00000008
#define HAL_MON_MON_INGRESS_RING_BUFFER_VIRT_ADDR_31_0_LSB 0
#define HAL_MON_MON_INGRESS_RING_BUFFER_VIRT_ADDR_31_0_MSB 31
#define HAL_MON_MON_INGRESS_RING_BUFFER_VIRT_ADDR_31_0_MASK 0xffffffff

#define HAL_MON_MON_INGRESS_RING_BUFFER_VIRT_ADDR_63_32_OFFSET 0x0000000c
#define HAL_MON_MON_INGRESS_RING_BUFFER_VIRT_ADDR_63_32_LSB 0
#define HAL_MON_MON_INGRESS_RING_BUFFER_VIRT_ADDR_63_32_MSB 31
#define HAL_MON_MON_INGRESS_RING_BUFFER_VIRT_ADDR_63_32_MASK 0xffffffff

#define HAL_MON_PADDR_LO_SET(buff_addr_info, paddr_lo) \
		((*(((unsigned int *) buff_addr_info) + \
		(HAL_MON_BUFFER_ADDR_INFO_0_BUFFER_ADDR_31_0_OFFSET >> 2))) = \
		((paddr_lo) << HAL_MON_BUFFER_ADDR_INFO_0_BUFFER_ADDR_31_0_LSB) & \
		HAL_MON_BUFFER_ADDR_INFO_0_BUFFER_ADDR_31_0_MASK)

#define HAL_MON_PADDR_HI_SET(buff_addr_info, paddr_hi) \
		((*(((unsigned int *) buff_addr_info) + \
		(HAL_MON_BUFFER_ADDR_INFO_1_BUFFER_ADDR_39_32_OFFSET >> 2))) = \
		((paddr_hi) << HAL_MON_BUFFER_ADDR_INFO_1_BUFFER_ADDR_39_32_LSB) & \
		HAL_MON_BUFFER_ADDR_INFO_1_BUFFER_ADDR_39_32_MASK)

#define HAL_MON_VADDR_LO_SET(buff_addr_info, vaddr_lo) \
		((*(((unsigned int *) buff_addr_info) + \
		(HAL_MON_MON_INGRESS_RING_BUFFER_VIRT_ADDR_31_0_OFFSET >> 2))) = \
		((vaddr_lo) << HAL_MON_MON_INGRESS_RING_BUFFER_VIRT_ADDR_31_0_LSB) & \
		HAL_MON_MON_INGRESS_RING_BUFFER_VIRT_ADDR_31_0_MASK)

#define HAL_MON_VADDR_HI_SET(buff_addr_info, vaddr_hi) \
		((*(((unsigned int *) buff_addr_info) + \
		(HAL_MON_MON_INGRESS_RING_BUFFER_VIRT_ADDR_63_32_OFFSET >> 2))) = \
		((vaddr_hi) << HAL_MON_MON_INGRESS_RING_BUFFER_VIRT_ADDR_63_32_LSB) & \
		HAL_MON_MON_INGRESS_RING_BUFFER_VIRT_ADDR_63_32_MASK)
#endif

#define UNIFIED_RXPCU_PPDU_END_INFO_8_RX_PPDU_DURATION_OFFSET \
	HAL_RXPCU_PPDU_END_INFO_RX_PPDU_DURATION_OFFSET
#define UNIFIED_RXPCU_PPDU_END_INFO_8_RX_PPDU_DURATION_MASK \
	HAL_RXPCU_PPDU_END_INFO_RX_PPDU_DURATION_MASK
#define UNIFIED_RXPCU_PPDU_END_INFO_8_RX_PPDU_DURATION_LSB \
	HAL_RXPCU_PPDU_END_INFO_RX_PPDU_DURATION_LSB
#define UNIFIED_PHYRX_HT_SIG_0_HT_SIG_INFO_PHYRX_HT_SIG_INFO_DETAILS_OFFSET \
	PHYRX_HT_SIG_PHYRX_HT_SIG_INFO_DETAILS_MCS_OFFSET
#define UNIFIED_PHYRX_L_SIG_B_0_L_SIG_B_INFO_PHYRX_L_SIG_B_INFO_DETAILS_OFFSET \
	PHYRX_L_SIG_B_PHYRX_L_SIG_B_INFO_DETAILS_RATE_OFFSET
#define UNIFIED_PHYRX_L_SIG_A_0_L_SIG_A_INFO_PHYRX_L_SIG_A_INFO_DETAILS_OFFSET \
	PHYRX_L_SIG_A_PHYRX_L_SIG_A_INFO_DETAILS_RATE_OFFSET
#define UNIFIED_PHYRX_VHT_SIG_A_0_VHT_SIG_A_INFO_PHYRX_VHT_SIG_A_INFO_DETAILS_OFFSET \
	PHYRX_VHT_SIG_A_PHYRX_VHT_SIG_A_INFO_DETAILS_BANDWIDTH_OFFSET
#define UNIFIED_PHYRX_HE_SIG_A_SU_0_HE_SIG_A_SU_INFO_PHYRX_HE_SIG_A_SU_INFO_DETAILS_OFFSET \
	PHYRX_HE_SIG_A_SU_PHYRX_HE_SIG_A_SU_INFO_DETAILS_FORMAT_INDICATION_OFFSET
#define UNIFIED_PHYRX_HE_SIG_A_MU_DL_0_HE_SIG_A_MU_DL_INFO_PHYRX_HE_SIG_A_MU_DL_INFO_DETAILS_OFFSET \
	PHYRX_HE_SIG_A_MU_DL_PHYRX_HE_SIG_A_MU_DL_INFO_DETAILS_DL_UL_FLAG_OFFSET
#define UNIFIED_PHYRX_HE_SIG_B1_MU_0_HE_SIG_B1_MU_INFO_PHYRX_HE_SIG_B1_MU_INFO_DETAILS_OFFSET \
	PHYRX_HE_SIG_B1_MU_PHYRX_HE_SIG_B1_MU_INFO_DETAILS_RU_ALLOCATION_OFFSET
#define UNIFIED_PHYRX_HE_SIG_B2_MU_0_HE_SIG_B2_MU_INFO_PHYRX_HE_SIG_B2_MU_INFO_DETAILS_OFFSET \
	PHYRX_HE_SIG_B2_MU_PHYRX_HE_SIG_B2_MU_INFO_DETAILS_STA_ID_OFFSET
#define UNIFIED_PHYRX_HE_SIG_B2_OFDMA_0_HE_SIG_B2_OFDMA_INFO_PHYRX_HE_SIG_B2_OFDMA_INFO_DETAILS_OFFSET \
	PHYRX_HE_SIG_B2_OFDMA_PHYRX_HE_SIG_B2_OFDMA_INFO_DETAILS_STA_ID_OFFSET
#define UNIFIED_PHYRX_RSSI_LEGACY_3_RECEIVE_RSSI_INFO_PRE_RSSI_INFO_DETAILS_OFFSET \
	PHYRX_RSSI_LEGACY_PRE_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN0_OFFSET
#define UNIFIED_PHYRX_RSSI_LEGACY_19_RECEIVE_RSSI_INFO_PREAMBLE_RSSI_INFO_DETAILS_OFFSET \
	PHYRX_RSSI_LEGACY_PREAMBLE_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN0_OFFSET


#define RX_MON_MPDU_START_WMASK               0x07F0
#define RX_MON_MPDU_END_WMASK                 0x7
#define RX_MON_MPDU_START_WMASK_V2            0x007F0
#define RX_MON_MPDU_END_WMASK_V2              0xFF
#define RX_MON_MSDU_END_WMASK                 0x0AE1
#define RX_MON_PPDU_END_USR_STATS_WMASK       0xB7E

#define MAX_USR_INFO_STR_CNT	4

#ifdef CONFIG_MON_WORD_BASED_TLV
#ifndef BIG_ENDIAN_HOST
struct rx_mpdu_start_mon_data {
	uint32_t peer_meta_data                    : 32;
	uint32_t rxpcu_mpdu_filter_in_category     : 2,
		 sw_frame_group_id                 : 7,
		 ndp_frame                         : 1,
		 phy_err                           : 1,
		 phy_err_during_mpdu_header        : 1,
		 protocol_version_err              : 1,
		 ast_based_lookup_valid            : 1,
		 reserved_0a                       : 2,
		 phy_ppdu_id                       : 16;
	uint32_t ast_index                         : 16,
		 sw_peer_id                        : 16;
	uint32_t mpdu_frame_control_valid          : 1,
		 mpdu_duration_valid               : 1,
		 mac_addr_ad1_valid                : 1,
		 mac_addr_ad2_valid                : 1,
		 mac_addr_ad3_valid                : 1,
		 mac_addr_ad4_valid                : 1,
		 mpdu_sequence_control_valid       : 1,
		 mpdu_qos_control_valid            : 1,
		 mpdu_ht_control_valid             : 1,
		 frame_encryption_info_valid       : 1,
		 mpdu_fragment_number              : 4,
		 more_fragment_flag                : 1,
		 reserved_11a                      : 1,
		 fr_ds                             : 1,
		 to_ds                             : 1,
		 encrypted                         : 1,
		 mpdu_retry                        : 1,
		 mpdu_sequence_number              : 12;
	uint32_t key_id_octet                      :  8,
		 new_peer_entry                    :  1,
		 decrypt_needed                    :  1,
		 decap_type                        :  2,
		 rx_insert_vlan_c_tag_padding      :  1,
		 rx_insert_vlan_s_tag_padding      :  1,
		 strip_vlan_c_tag_decap            :  1,
		 strip_vlan_s_tag_decap            :  1,
		 pre_delim_count                   : 12,
		 ampdu_flag                        :  1,
		 bar_frame                         :  1,
		 raw_mpdu                          :  1,
		 reserved_12                       :  1;
	uint32_t mpdu_length                       : 14,
		 first_mpdu                        : 1,
		 mcast_bcast                       : 1,
		 ast_index_not_found               : 1,
		 ast_index_timeout                 : 1,
		 power_mgmt                        : 1,
		 non_qos                           : 1,
		 null_data                         : 1,
		 mgmt_type                         : 1,
		 ctrl_type                         : 1,
		 more_data                         : 1,
		 eosp                              : 1,
		 fragment_flag                     : 1,
		 order                             : 1,
		 u_apsd_trigger                    : 1,
		 encrypt_required                  : 1,
		 directed                          : 1,
		 amsdu_present                     : 1,
		 reserved_13                       : 1;
	uint32_t mpdu_frame_control_field          : 16,
		 mpdu_duration_field               : 16;
	uint32_t mac_addr_ad1_31_0                 : 32;
	uint32_t mac_addr_ad1_47_32                : 16,
		 mac_addr_ad2_15_0                 : 16;
	uint32_t mac_addr_ad2_47_16                : 32;
	uint32_t mac_addr_ad3_31_0                 : 32;
	uint32_t mac_addr_ad3_47_32                : 16,
		 mpdu_sequence_control_field       : 16;
	uint32_t mac_addr_ad4_31_0                 : 32;
	uint32_t mac_addr_ad4_47_32                : 16,
		 mpdu_qos_control_field            : 16;
};

struct rx_msdu_end_mon_data {
	uint32_t rxpcu_mpdu_filter_in_category     : 2,
		 sw_frame_group_id                 : 7,
		 reserved_0                        : 7,
		 phy_ppdu_id                       : 16;
	uint32_t ip_hdr_chksum                     : 16,
		 reported_mpdu_length              : 14,
		 reserved_1a                       :  2;
	uint32_t sa_sw_peer_id                     : 16,
		 sa_idx_timeout                    :  1,
		 da_idx_timeout                    :  1,
		 to_ds                             :  1,
		 tid                               :  4,
		 sa_is_valid                       :  1,
		 da_is_valid                       :  1,
		 da_is_mcbc                        :  1,
		 l3_header_padding                 :  2,
		 first_msdu                        :  1,
		 last_msdu                         :  1,
		 fr_ds                             :  1,
		 ip_chksum_fail_copy               :  1;
	uint32_t sa_idx                            : 16,
		 da_idx_or_sw_peer_id              : 16;
	uint32_t msdu_drop                         :  1,
		 reo_destination_indication        :  5,
		 flow_idx                          : 20,
		 use_ppe                           :  1,
		 mesh_sta                          :  2,
		 vlan_ctag_stripped                :  1,
		 vlan_stag_stripped                :  1,
		 fragment_flag                     :  1;
	uint32_t fse_metadata                      : 32;
	uint32_t cce_metadata                      : 16,
		 tcp_udp_chksum                    : 16;
	uint32_t aggregation_count                 :  8,
		 flow_aggregation_continuation     :  1,
		 fisa_timeout                      :  1,
		 tcp_udp_chksum_fail_copy          :  1,
		 msdu_limit_error                  :  1,
		 flow_idx_timeout                  :  1,
		 flow_idx_invalid                  :  1,
		 cce_match                         :  1,
		 amsdu_parser_error                :  1,
		 cumulative_ip_length              : 16;
	uint32_t msdu_length                       : 14,
		 stbc                              :  1,
		 ipsec_esp                         :  1,
		 l3_offset                         :  7,
		 ipsec_ah                          :  1,
		 l4_offset                         :  8;
	uint32_t msdu_number                       :  8,
		 decap_format                      :  2,
		 ipv4_proto                        :  1,
		 ipv6_proto                        :  1,
		 tcp_proto                         :  1,
		 udp_proto                         :  1,
		 ip_frag                           :  1,
		 tcp_only_ack                      :  1,
		 da_is_bcast_mcast                 :  1,
		 toeplitz_hash_sel                 :  2,
		 ip_fixed_header_valid             :  1,
		 ip_extn_header_valid              :  1,
		 tcp_udp_header_valid              :  1,
		 mesh_control_present              :  1,
		 ldpc                              :  1,
		 ip4_protocol_ip6_next_header      :  8;
	uint32_t user_rssi                         :  8,
		 pkt_type                          :  4,
		 sgi                               :  2,
		 rate_mcs                          :  4,
		 receive_bandwidth                 :  3,
		 reception_type                    :  3,
		 mimo_ss_bitmap                    :  7,
		 msdu_done_copy                    :  1;
	uint32_t flow_id_toeplitz                  : 32;
};

struct rx_ppdu_end_user_mon_data {
	uint32_t sw_peer_id                        : 16,
		 mpdu_cnt_fcs_err                  : 11,
		 sw2rxdma0_buf_source_used         :  1,
		 fw2rxdma_pmac0_buf_source_used    :  1,
		 sw2rxdma1_buf_source_used         :  1,
		 sw2rxdma_exception_buf_source_used:  1,
		 fw2rxdma_pmac1_buf_source_used    :  1;
	uint32_t mpdu_cnt_fcs_ok                   : 11,
		 frame_control_info_valid          :  1,
		 qos_control_info_valid	           :  1,
		 ht_control_info_valid             :  1,
		 data_sequence_control_info_valid  :  1,
		 ht_control_info_null_valid        :  1,
		 rxdma2fw_pmac1_ring_used          :  1,
		 rxdma2reo_ring_used               :  1,
		 rxdma2fw_pmac0_ring_used          :  1,
		 rxdma2sw_ring_used                :  1,
		 rxdma_release_ring_used           :  1,
		 ht_control_field_pkt_type         :  4,
		 rxdma2reo_remote0_ring_used       :  1,
		 rxdma2reo_remote1_ring_used       :  1,
		 reserved_3b                       :  5;
	uint32_t ast_index                         : 16,
		 frame_control_field               : 16;
	uint32_t first_data_seq_ctrl               : 16,
		 qos_control_field                 : 16;
	uint32_t ht_control_field                  : 32;
	uint32_t fcs_ok_bitmap_31_0                : 32;
	uint32_t fcs_ok_bitmap_63_32               : 32;
	uint32_t udp_msdu_count                    : 16,
		 tcp_msdu_count                    : 16;
	uint32_t other_msdu_count                  : 16,
		 tcp_ack_msdu_count                : 16;
	uint32_t sw_response_reference_ptr         : 32;
	uint32_t received_qos_data_tid_bitmap      : 16,
		 received_qos_data_tid_eosp_bitmap : 16;
	uint32_t qosctrl_15_8_tid0                 :  8,
		 qosctrl_15_8_tid1                 :  8,
		 qosctrl_15_8_tid2                 :  8,
		 qosctrl_15_8_tid3                 :  8;
	uint32_t qosctrl_15_8_tid12                :  8,
		 qosctrl_15_8_tid13                :  8,
		 qosctrl_15_8_tid14                :  8,
		 qosctrl_15_8_tid15                :  8;
	uint32_t mpdu_ok_byte_count                : 25,
		 ampdu_delim_ok_count_6_0          :  7;
	uint32_t ampdu_delim_err_count             : 25,
		 ampdu_delim_ok_count_13_7         :  7;
	uint32_t mpdu_err_byte_count               : 25,
		 ampdu_delim_ok_count_20_14        :  7;
	uint32_t sw_response_reference_ptr_ext     : 32;
	uint32_t corrupted_due_to_fifo_delay       :  1,
		 frame_control_info_null_valid     :  1,
		 frame_control_field_null          : 16,
		 retried_mpdu_count                : 11,
		 reserved_23a                      :  3;
};
#else
struct rx_mpdu_start_mon_data {
	uint32_t peer_meta_data                    : 32;
	uint32_t phy_ppdu_id                       : 16,
		 reserved_0a                       : 2,
		 ast_based_lookup_valid            : 1,
		 protocol_version_err              : 1,
		 phy_err_during_mpdu_header        : 1,
		 phy_err                           : 1,
		 ndp_frame                         : 1,
		 sw_frame_group_id                 : 7,
		 rxpcu_mpdu_filter_in_category     : 2;
	uint32_t sw_peer_id                        : 16,
		 ast_index                         : 16;
	uint32_t mpdu_sequence_number              : 12,
		 mpdu_retry                        : 1,
		 encrypted                         : 1,
		 to_ds                             : 1,
		 fr_ds                             : 1,
		 reserved_11a                      : 1,
		 more_fragment_flag                : 1,
		 mpdu_fragment_number              : 4,
		 frame_encryption_info_valid       : 1,
		 mpdu_ht_control_valid             : 1,
		 mpdu_qos_control_valid            : 1,
		 mpdu_sequence_control_valid       : 1,
		 mac_addr_ad4_valid                : 1,
		 mac_addr_ad3_valid                : 1,
		 mac_addr_ad2_valid                : 1,
		 mac_addr_ad1_valid                : 1,
		 mpdu_duration_valid               : 1,
		 mpdu_frame_control_valid          : 1;
	uint32_t reserved_12                       :  1,
		 raw_mpdu                          :  1,
		 bar_frame                         :  1,
		 ampdu_flag                        :  1,
		 pre_delim_count                   : 12,
		 strip_vlan_s_tag_decap            :  1,
		 strip_vlan_c_tag_decap            :  1,
		 rx_insert_vlan_s_tag_padding      :  1,
		 rx_insert_vlan_c_tag_padding      :  1,
		 decap_type                        :  2,
		 decrypt_needed                    :  1,
		 new_peer_entry                    :  1,
		 key_id_octet                      :  8;
	uint32_t reserved_13                       : 1,
		 amsdu_present                     : 1,
		 directed                          : 1,
		 encrypt_required                  : 1,
		 u_apsd_trigger                    : 1,
		 order                             : 1,
		 fragment_flag                     : 1,
		 eosp                              : 1,
		 more_data                         : 1,
		 ctrl_type                         : 1,
		 mgmt_type                         : 1,
		 null_data                         : 1,
		 non_qos                           : 1,
		 power_mgmt                        : 1,
		 ast_index_timeout                 : 1,
		 ast_index_not_found               : 1,
		 mcast_bcast                       : 1,
		 first_mpdu                        : 1,
		 mpdu_length                       : 14;
	uint32_t mpdu_duration_field               : 16,
		 mpdu_frame_control_field          : 16;
	uint32_t mac_addr_ad1_31_0                 : 32;
	uint32_t mac_addr_ad2_15_0                 : 16,
		 mac_addr_ad1_47_32                : 16;
	uint32_t mac_addr_ad2_47_16                : 32;
	uint32_t mac_addr_ad3_31_0                 : 32;
	uint32_t mpdu_sequence_control_field       : 16,
		 mac_addr_ad3_47_32                : 16;
	uint32_t mac_addr_ad4_31_0                 : 32;
	uint32_t mpdu_qos_control_field            : 16,
		 mac_addr_ad4_47_32                : 16;
};

struct rx_msdu_end_mon_data {
	uint32_t phy_ppdu_id                       : 16,
		 reserved_0                        : 7,
		 sw_frame_group_id                 : 7,
		 rxpcu_mpdu_filter_in_category     : 2;
	uint32_t reserved_1a                       : 2,
		 reported_mpdu_length              : 14,
		 ip_hdr_chksum                     : 16;
	uint32_t ip_chksum_fail_copy               :  1,
		 fr_ds                             :  1,
		 last_msdu                         :  1,
		 first_msdu                        :  1,
		 l3_header_padding                 :  2,
		 da_is_mcbc                        :  1,
		 da_is_valid                       :  1,
		 sa_is_valid                       :  1,
		 tid                               :  4,
		 to_ds                             :  1,
		 da_idx_timeout                    :  1,
		 sa_idx_timeout                    :  1,
		 sa_sw_peer_id                     : 16;
	uint32_t da_idx_or_sw_peer_id              : 16,
		 sa_idx                            : 16;
	uint32_t fragment_flag                     :  1,
		 vlan_stag_stripped                :  1,
		 vlan_ctag_stripped                :  1,
		 mesh_sta                          :  2,
		 use_ppe                           :  1,
		 flow_idx                          : 20,
		 reo_destination_indication        :  5,
		 msdu_drop                         :  1;
	uint32_t fse_metadata                      : 32;
	uint32_t cce_metadata                      : 16,
		 tcp_udp_chksum                    : 16;
	uint32_t cumulative_ip_length              : 16,
		 amsdu_parser_error                :  1,
		 cce_match                         :  1,
		 flow_idx_invalid                  :  1,
		 flow_idx_timeout                  :  1,
		 msdu_limit_error                  :  1,
		 tcp_udp_chksum_fail_copy          :  1,
		 fisa_timeout                      :  1,
		 flow_aggregation_continuation     :  1,
		 aggregation_count                 :  8;
	uint32_t l4_offset                         :  8,
		 ipsec_ah                          :  1,
		 l3_offset                         :  7,
		 ipsec_esp                         :  1,
		 stbc                              :  1,
		 msdu_length                       : 14;
	uint32_t ip4_protocol_ip6_next_header      :  8,
		 ldpc                              :  1,
		 mesh_control_present              :  1,
		 tcp_udp_header_valid              :  1,
		 ip_extn_header_valid              :  1,
		 ip_fixed_header_valid             :  1,
		 toeplitz_hash_sel                 :  2,
		 da_is_bcast_mcast                 :  1,
		 tcp_only_ack                      :  1,
		 ip_frag                           :  1,
		 udp_proto                         :  1,
		 tcp_proto                         :  1,
		 ipv6_proto                        :  1,
		 ipv4_proto                        :  1,
		 decap_format                      :  2,
		 msdu_number                       :  8;
	uint32_t msdu_done_copy                    :  1,
		 mimo_ss_bitmap                    :  7,
		 reception_type                    :  3,
		 receive_bandwidth                 :  3,
		 rate_mcs                          :  4,
		 sgi                               :  2,
		 pkt_type                          :  4,
		 user_rssi                         :  8;
	uint32_t flow_id_toeplitz                  : 32;
};

struct rx_ppdu_end_user_mon_data {
	uint32_t fw2rxdma_pmac1_buf_source_used    :  1,
		 sw2rxdma_exception_buf_source_used:  1,
		 sw2rxdma1_buf_source_used         :  1,
		 fw2rxdma_pmac0_buf_source_used    :  1,
		 sw2rxdma0_buf_source_used         :  1,
		 mpdu_cnt_fcs_err                  : 11,
		 sw_peer_id                        : 16;
	uint32_t reserved_3b                       :  5,
		 rxdma2reo_remote1_ring_used       :  1,
		 rxdma2reo_remote0_ring_used       :  1,
		 ht_control_field_pkt_type         :  4,
		 rxdma_release_ring_used           :  1,
		 rxdma2sw_ring_used                :  1,
		 rxdma2fw_pmac0_ring_used          :  1,
		 rxdma2reo_ring_used               :  1,
		 rxdma2fw_pmac1_ring_used          :  1,
		 ht_control_info_null_valid        :  1,
		 data_sequence_control_info_valid  :  1,
		 ht_control_info_valid             :  1,
		 qos_control_info_valid            :  1,
		 frame_control_info_valid          :  1,
		 mpdu_cnt_fcs_ok                   : 11;
	uint32_t frame_control_field               : 16,
		 ast_index                         : 16;
	uint32_t qos_control_field                 : 16,
		 first_data_seq_ctrl               : 16;
	uint32_t ht_control_field                  : 32;
	uint32_t fcs_ok_bitmap_31_0                : 32;
	uint32_t fcs_ok_bitmap_63_32               : 32;
	uint32_t tcp_msdu_count                    : 16,
		 udp_msdu_count                    : 16;
	uint32_t tcp_ack_msdu_count                : 16,
		 other_msdu_count                  : 16;
	uint32_t sw_response_reference_ptr         : 32;
	uint32_t received_qos_data_tid_eosp_bitmap : 16,
		 received_qos_data_tid_bitmap      : 16;
	uint32_t qosctrl_15_8_tid3                 :  8,
		 qosctrl_15_8_tid2                 :  8,
		 qosctrl_15_8_tid1                 :  8,
		 qosctrl_15_8_tid0                 :  8;
	uint32_t qosctrl_15_8_tid15                :  8,
		 qosctrl_15_8_tid14                :  8,
		 qosctrl_15_8_tid13                :  8,
		 qosctrl_15_8_tid12                :  8;
	uint32_t ampdu_delim_ok_count_6_0          :  7,
		 mpdu_ok_byte_count                : 25;
	uint32_t ampdu_delim_ok_count_13_7         :  7,
		 ampdu_delim_err_count             : 25;
	uint32_t ampdu_delim_ok_count_20_14        :  7,
		 mpdu_err_byte_count               : 25;
	uint32_t sw_response_reference_ptr_ext     : 32;
	uint32_t reserved_23a                      :  3,
		 retried_mpdu_count                : 11,
		 frame_control_field_null          : 16,
		 frame_control_info_null_valid     :  1,
		 corrupted_due_to_fifo_delay       :  1;
};
#endif

struct rx_mpdu_start_mon_data_t {
	struct rx_mpdu_start_mon_data rx_mpdu_info_details;
};

struct rx_msdu_end_mon_data_t {
	struct rx_msdu_end_mon_data rx_mpdu_info_details;
};
/* TLV struct for word based Tlv */
typedef struct rx_mpdu_start_mon_data_t hal_rx_mon_mpdu_start_t;
typedef struct rx_msdu_end_mon_data hal_rx_mon_msdu_end_t;
typedef struct rx_ppdu_end_user_mon_data hal_rx_mon_ppdu_end_user_t;

#else

typedef struct rx_mpdu_start hal_rx_mon_mpdu_start_t;
typedef struct rx_msdu_end hal_rx_mon_msdu_end_t;
typedef struct rx_ppdu_end_user_stats hal_rx_mon_ppdu_end_user_t;
#endif

/*
 * struct mon_destination_drop - monitor drop descriptor
 *
 * @ppdu_drop_cnt: PPDU drop count
 * @mpdu_drop_cnt: MPDU drop count
 * @tlv_drop_cnt: TLV drop count
 * @end_of_ppdu_seen: end of ppdu seen
 * @reserved_0a: rsvd
 * @reserved_1a: rsvd
 * @ppdu_id: PPDU ID
 * @reserved_3a: rsvd
 * @initiator: initiator ppdu
 * @empty_descriptor: empty descriptor
 * @ring_id: ring id
 * @looping_count: looping count
 */
struct mon_destination_drop {
	uint32_t ppdu_drop_cnt                     : 10,
		 mpdu_drop_cnt                     : 10,
		 tlv_drop_cnt                      : 10,
		 end_of_ppdu_seen                  :  1,
		 reserved_0a                       :  1;
	uint32_t reserved_1a                       : 32;
	uint32_t ppdu_id                           : 32;
	uint32_t reserved_3a                       : 18,
		 initiator                         :  1,
		 empty_descriptor                  :  1,
		 ring_id                           :  8,
		 looping_count                     :  4;
};

#define HAL_MON_BUFFER_ADDR_31_0_GET(buff_addr_info)	\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(buff_addr_info,	\
		HAL_BUFFER_ADDR_INFO_BUFFER_ADDR_31_0_OFFSET)),	\
		HAL_BUFFER_ADDR_INFO_BUFFER_ADDR_31_0_MASK,	\
		HAL_BUFFER_ADDR_INFO_BUFFER_ADDR_31_0_LSB))

#define HAL_MON_BUFFER_ADDR_39_32_GET(buff_addr_info)			\
	(_HAL_MS((*_OFFSET_TO_WORD_PTR(buff_addr_info,			\
		HAL_BUFFER_ADDR_INFO_BUFFER_ADDR_39_32_OFFSET)),	\
		HAL_BUFFER_ADDR_INFO_BUFFER_ADDR_39_32_MASK,		\
		HAL_BUFFER_ADDR_INFO_BUFFER_ADDR_39_32_LSB))

/**
 * struct hal_rx_status_buffer_done - status buffer done tlv
 * placeholder structure
 *
 * @ppdu_start_offset: ppdu start
 * @first_ppdu_start_user_info_offset:
 * @mult_ppdu_start_user_info:
 * @end_offset:
 * @ppdu_end_detected:
 * @flush_detected:
 * @rsvd:
 */
struct hal_rx_status_buffer_done {
	uint32_t ppdu_start_offset : 3,
		 first_ppdu_start_user_info_offset : 6,
		 mult_ppdu_start_user_info : 1,
		 end_offset : 13,
		 ppdu_end_detected : 1,
		 flush_detected : 1,
		 rsvd : 7;
};

/**
 * enum hal_mon_status_end_reason - ppdu status buffer end reason
 *
 * @HAL_MON_STATUS_BUFFER_FULL: status buffer full
 * @HAL_MON_FLUSH_DETECTED: flush detected
 * @HAL_MON_END_OF_PPDU: end of ppdu detected
 * @HAL_MON_PPDU_TRUNCATED: truncated ppdu status
 */
enum hal_mon_status_end_reason {
	HAL_MON_STATUS_BUFFER_FULL,
	HAL_MON_FLUSH_DETECTED,
	HAL_MON_END_OF_PPDU,
	HAL_MON_PPDU_TRUNCATED,
};

/**
 * struct hal_mon_desc - HAL Monitor descriptor
 *
 * @buf_addr: virtual buffer address
 * @ppdu_id: ppdu id
 *	     - TxMon fills scheduler id
 *	     - RxMON fills phy_ppdu_id
 * @end_offset: offset (units in 4 bytes) where status buffer ended
 *		i.e offset of TLV + last TLV size
 * @reserved_3a: reserved bits
 * @end_reason: ppdu end reason
 *		0 - status buffer is full
 *		1 - flush detected
 *		2 - TX_FES_STATUS_END or RX_PPDU_END
 *		3 - PPDU truncated due to system error
 * @initiator:	1 - descriptor belongs to TX FES
 *		0 - descriptor belongs to TX RESPONSE
 * @empty_descriptor: 0 - this descriptor is written on a flush
 *			or end of ppdu or end of status buffer
 *			1 - descriptor provided to indicate drop
 * @ring_id: ring id for debugging
 * @looping_count: count to indicate number of times producer
 *			of entries has looped around the ring
 * @flush_detected: if flush detected
 * @end_of_ppdu_dropped: if end_of_ppdu is dropped
 * @ppdu_drop_count: PPDU drop count
 * @mpdu_drop_count: MPDU drop count
 * @tlv_drop_count: TLV drop count
 */
struct hal_mon_desc {
	uint64_t buf_addr;
	uint32_t ppdu_id;
	uint32_t end_offset:12,
		 reserved_3a:4,
		 end_reason:2,
		 initiator:1,
		 empty_descriptor:1,
		 ring_id:8,
		 looping_count:4;
	uint16_t flush_detected:1,
		 end_of_ppdu_dropped:1;
	uint32_t ppdu_drop_count;
	uint32_t mpdu_drop_count;
	uint32_t tlv_drop_count;
};

typedef struct hal_mon_desc *hal_mon_desc_t;

/**
 * struct hal_mon_buf_addr_status - HAL buffer address tlv get status
 *
 * @buffer_virt_addr_31_0: Lower 32 bits of virtual address of status buffer
 * @buffer_virt_addr_63_32: Upper 32 bits of virtual address of status buffer
 * @dma_length: DMA length
 * @reserved_2a: reserved bits
 * @msdu_continuation: is msdu size more than fragment size
 * @truncated: is msdu got truncated
 * @reserved_2b: reserved bits
 * @tlv64_padding: tlv paddding
 */
struct hal_mon_buf_addr_status {
	uint32_t buffer_virt_addr_31_0;
	uint32_t buffer_virt_addr_63_32;
	uint32_t dma_length:12,
		 reserved_2a:4,
		 msdu_continuation:1,
		 truncated:1,
		 reserved_2b:14;
	uint32_t tlv64_padding;
};

#if defined(WLAN_PKT_CAPTURE_TX_2_0) || \
defined(WLAN_PKT_CAPTURE_RX_2_0)

/**
 * hal_be_get_mon_dest_status() - Get monitor descriptor status
 * @hal_soc: HAL Soc handle
 * @hw_desc: HAL monitor descriptor
 * @status: pointer to write descriptor status
 *
 * Return: none
 */
static inline void
hal_be_get_mon_dest_status(hal_soc_handle_t hal_soc,
			   void *hw_desc,
			   struct hal_mon_desc *status)
{
	struct mon_destination_ring *desc = hw_desc;

	status->empty_descriptor = desc->empty_descriptor;
	if (status->empty_descriptor) {
		struct mon_destination_drop *drop_desc = hw_desc;

		status->buf_addr = 0;
		status->ppdu_drop_count = drop_desc->ppdu_drop_cnt;
		status->mpdu_drop_count = drop_desc->mpdu_drop_cnt;
		status->tlv_drop_count = drop_desc->tlv_drop_cnt;
		status->end_of_ppdu_dropped = drop_desc->end_of_ppdu_seen;
	} else {
		status->buf_addr = HAL_RX_GET(desc, MON_DESTINATION_RING_STAT,BUF_VIRT_ADDR_31_0) |
						(((uint64_t)HAL_RX_GET(desc,
								       MON_DESTINATION_RING_STAT,
								       BUF_VIRT_ADDR_63_32)) << 32);
		status->end_reason = desc->end_reason;
		status->end_offset = desc->end_offset;
	}
	status->ppdu_id = desc->ppdu_id;
	status->initiator = desc->initiator;
	status->looping_count = desc->looping_count;
}
#endif

#if defined(RX_PPDU_END_USER_STATS_OFDMA_INFO_VALID_OFFSET) && \
defined(RX_PPDU_END_USER_STATS_SW_RESPONSE_REFERENCE_PTR_EXT_OFFSET)

static inline void
hal_rx_handle_mu_ul_info(hal_rx_mon_ppdu_end_user_t *rx_ppdu_end_user,
			 struct mon_rx_user_status *mon_rx_user_status)
{
	mon_rx_user_status->mu_ul_user_v0_word0 =
		rx_ppdu_end_user->sw_response_reference_ptr;

	mon_rx_user_status->mu_ul_user_v0_word1 =
		rx_ppdu_end_user->sw_response_reference_ptr_ext;
}
#else
static inline void
hal_rx_handle_mu_ul_info(hal_rx_mon_ppdu_end_user_t *rx_ppdu_end_user,
			 struct mon_rx_user_status *mon_rx_user_status)
{
}
#endif

static inline void
hal_rx_populate_byte_count(hal_rx_mon_ppdu_end_user_t *rx_ppdu_end_user,
			   void *ppduinfo,
			   struct mon_rx_user_status *mon_rx_user_status)
{
	mon_rx_user_status->mpdu_ok_byte_count =
				rx_ppdu_end_user->mpdu_ok_byte_count;
	mon_rx_user_status->mpdu_err_byte_count =
				rx_ppdu_end_user->mpdu_err_byte_count;
}

static inline void
hal_rx_populate_mu_user_info(hal_rx_mon_ppdu_end_user_t *rx_ppdu_end_user,
			     void *ppduinfo, uint32_t user_id,
			     struct mon_rx_user_status *mon_rx_user_status)
{
	struct mon_rx_info *mon_rx_info;
	struct mon_rx_user_info *mon_rx_user_info;
	struct hal_rx_ppdu_info *ppdu_info =
			(struct hal_rx_ppdu_info *)ppduinfo;

	mon_rx_info = &ppdu_info->rx_info;
	mon_rx_user_info = &ppdu_info->rx_user_info[user_id];
	mon_rx_user_info->qos_control_info_valid =
		mon_rx_info->qos_control_info_valid;
	mon_rx_user_info->qos_control =  mon_rx_info->qos_control;

	mon_rx_user_status->ast_index = ppdu_info->rx_status.ast_index;
	mon_rx_user_status->tid = ppdu_info->rx_status.tid;
	mon_rx_user_status->tcp_msdu_count =
		ppdu_info->rx_status.tcp_msdu_count;
	mon_rx_user_status->udp_msdu_count =
		ppdu_info->rx_status.udp_msdu_count;
	mon_rx_user_status->other_msdu_count =
		ppdu_info->rx_status.other_msdu_count;
	mon_rx_user_status->frame_control = ppdu_info->rx_status.frame_control;
	mon_rx_user_status->frame_control_info_valid =
		ppdu_info->rx_status.frame_control_info_valid;
	mon_rx_user_status->data_sequence_control_info_valid =
		ppdu_info->rx_status.data_sequence_control_info_valid;
	mon_rx_user_status->first_data_seq_ctrl =
		ppdu_info->rx_status.first_data_seq_ctrl;
	mon_rx_user_status->preamble_type = ppdu_info->rx_status.preamble_type;
	mon_rx_user_status->ht_flags = ppdu_info->rx_status.ht_flags;
	mon_rx_user_status->rtap_flags = ppdu_info->rx_status.rtap_flags;
	mon_rx_user_status->vht_flags = ppdu_info->rx_status.vht_flags;
	if (mon_rx_user_status->vht_flags) {
		mon_rx_user_status->vht_flag_values2 =
			ppdu_info->rx_status.vht_flag_values2;
		qdf_mem_copy(mon_rx_user_status->vht_flag_values3,
			     ppdu_info->rx_status.vht_flag_values3,
			     sizeof(mon_rx_user_status->vht_flag_values3));
		mon_rx_user_status->vht_flag_values4 =
			ppdu_info->rx_status.vht_flag_values4;
		mon_rx_user_status->vht_flag_values5 =
			ppdu_info->rx_status.vht_flag_values5;
		mon_rx_user_status->vht_flag_values6 =
			ppdu_info->rx_status.vht_flag_values6;
	}
	mon_rx_user_status->he_flags = ppdu_info->rx_status.he_flags;
	mon_rx_user_status->rs_flags = ppdu_info->rx_status.rs_flags;

	mon_rx_user_status->mpdu_cnt_fcs_ok =
		ppdu_info->com_info.mpdu_cnt_fcs_ok;
	mon_rx_user_status->mpdu_cnt_fcs_err =
		ppdu_info->com_info.mpdu_cnt_fcs_err;
	qdf_mem_copy(&mon_rx_user_status->mpdu_fcs_ok_bitmap,
		     &ppdu_info->com_info.mpdu_fcs_ok_bitmap,
		     HAL_RX_NUM_WORDS_PER_PPDU_BITMAP *
		     sizeof(ppdu_info->com_info.mpdu_fcs_ok_bitmap[0]));
	mon_rx_user_status->retry_mpdu =
			ppdu_info->rx_status.mpdu_retry_cnt;
	hal_rx_populate_byte_count(rx_ppdu_end_user, ppdu_info,
				   mon_rx_user_status);
}

#define HAL_RX_UPDATE_RSSI_PER_CHAIN_BW(chain, \
					ppdu_info, rssi_info_tlv) \
	{						\
	ppdu_info->rx_status.rssi_chain[chain][0] = \
			HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO,\
				   RSSI_PRI20_CHAIN##chain); \
	ppdu_info->rx_status.rssi_chain[chain][1] = \
			HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO,\
				   RSSI_EXT20_CHAIN##chain); \
	ppdu_info->rx_status.rssi_chain[chain][2] = \
			HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO,\
				   RSSI_EXT40_LOW20_CHAIN##chain); \
	ppdu_info->rx_status.rssi_chain[chain][3] = \
			HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO,\
				   RSSI_EXT40_HIGH20_CHAIN##chain); \
	}						\

#define HAL_RX_PPDU_UPDATE_RSSI(ppdu_info, rssi_info_tlv) \
	{HAL_RX_UPDATE_RSSI_PER_CHAIN_BW(0, ppdu_info, rssi_info_tlv) \
	HAL_RX_UPDATE_RSSI_PER_CHAIN_BW(1, ppdu_info, rssi_info_tlv) \
	HAL_RX_UPDATE_RSSI_PER_CHAIN_BW(2, ppdu_info, rssi_info_tlv) \
	HAL_RX_UPDATE_RSSI_PER_CHAIN_BW(3, ppdu_info, rssi_info_tlv) \
	} \

static inline uint32_t
hal_rx_update_rssi_chain(struct hal_rx_ppdu_info *ppdu_info,
			 uint8_t *rssi_info_tlv)
{
	HAL_RX_PPDU_UPDATE_RSSI(ppdu_info, rssi_info_tlv)
	return 0;
}

#ifdef WLAN_TX_PKT_CAPTURE_ENH
static inline void
hal_get_qos_control(hal_rx_mon_ppdu_end_user_t *rx_ppdu_end_user,
		    struct hal_rx_ppdu_info *ppdu_info)
{
	ppdu_info->rx_info.qos_control_info_valid =
		rx_ppdu_end_user->qos_control_info_valid;

	if (ppdu_info->rx_info.qos_control_info_valid)
		ppdu_info->rx_info.qos_control =
			rx_ppdu_end_user->qos_control_field;
}

static inline void
hal_get_mac_addr1(hal_rx_mon_mpdu_start_t *rx_mpdu_start,
		  struct hal_rx_ppdu_info *ppdu_info)
{
	if ((ppdu_info->sw_frame_group_id
	     == HAL_MPDU_SW_FRAME_GROUP_MGMT_PROBE_REQ) ||
	    (ppdu_info->sw_frame_group_id ==
	     HAL_MPDU_SW_FRAME_GROUP_CTRL_RTS)) {
		ppdu_info->rx_info.mac_addr1_valid =
			rx_mpdu_start->rx_mpdu_info_details.mac_addr_ad1_valid;

		*(uint32_t *)&ppdu_info->rx_info.mac_addr1[0] =
			rx_mpdu_start->rx_mpdu_info_details.mac_addr_ad1_31_0;
		if (ppdu_info->sw_frame_group_id ==
		    HAL_MPDU_SW_FRAME_GROUP_CTRL_RTS) {
			*(uint16_t *)&ppdu_info->rx_info.mac_addr1[4] =
				rx_mpdu_start->rx_mpdu_info_details.mac_addr_ad1_47_32;
		}
	}
}
#else
static inline void
hal_get_qos_control(hal_rx_mon_ppdu_end_user_t *rx_ppdu_end_user,
		    struct hal_rx_ppdu_info *ppdu_info)
{
}

static inline void
hal_get_mac_addr1(hal_rx_mon_mpdu_start_t *rx_mpdu_start,
		  struct hal_rx_ppdu_info *ppdu_info)
{
}
#endif

#ifdef QCA_SUPPORT_SCAN_SPCL_VAP_STATS
static inline void
hal_update_frame_type_cnt(hal_rx_mon_mpdu_start_t *rx_mpdu_start,
			  struct hal_rx_ppdu_info *ppdu_info)
{
	uint16_t frame_ctrl;
	uint8_t fc_type;

	if (rx_mpdu_start->rx_mpdu_info_details.mpdu_frame_control_valid) {
		frame_ctrl = rx_mpdu_start->rx_mpdu_info_details.mpdu_frame_control_field;
		fc_type = HAL_RX_GET_FRAME_CTRL_TYPE(frame_ctrl);
		if (fc_type == HAL_RX_FRAME_CTRL_TYPE_MGMT)
			ppdu_info->frm_type_info.rx_mgmt_cnt++;
		else if (fc_type == HAL_RX_FRAME_CTRL_TYPE_CTRL)
			ppdu_info->frm_type_info.rx_ctrl_cnt++;
		else if (fc_type == HAL_RX_FRAME_CTRL_TYPE_DATA)
			ppdu_info->frm_type_info.rx_data_cnt++;
	}
}
#else
static inline void
hal_update_frame_type_cnt(hal_rx_mon_mpdu_start_t *rx_mpdu_start,
			  struct hal_rx_ppdu_info *ppdu_info)
{
}
#endif

#if defined(WLAN_PKT_CAPTURE_TX_2_0) || \
defined(WLAN_PKT_CAPTURE_RX_2_0)
/**
 * hal_mon_buff_addr_info_set() - set desc address in cookie
 * @hal_soc_hdl: HAL Soc handle
 * @mon_entry: monitor srng
 * @mon_desc_addr: HAL monitor descriptor virtual address
 * @phy_addr: HAL monitor descriptor physical address
 *
 * Return: none
 */
static inline
void hal_mon_buff_addr_info_set(hal_soc_handle_t hal_soc_hdl,
				void *mon_entry,
				unsigned long long mon_desc_addr,
				qdf_dma_addr_t phy_addr)
{
	uint32_t paddr_lo = ((uintptr_t)phy_addr & 0x00000000ffffffff);
	uint32_t paddr_hi = ((uintptr_t)phy_addr & 0xffffffff00000000) >> 32;
	uint32_t vaddr_lo = ((unsigned long long)mon_desc_addr & 0x00000000ffffffff);
	uint32_t vaddr_hi = ((unsigned long long)mon_desc_addr & 0xffffffff00000000) >> 32;

	HAL_MON_PADDR_LO_SET(mon_entry, paddr_lo);
	HAL_MON_PADDR_HI_SET(mon_entry, paddr_hi);
	HAL_MON_VADDR_LO_SET(mon_entry, vaddr_lo);
	HAL_MON_VADDR_HI_SET(mon_entry, vaddr_hi);
}
#endif

#ifdef WLAN_PKT_CAPTURE_TX_2_0

/* TX monitor */
#define TX_MON_STATUS_BUF_SIZE 2048

#define HAL_INVALID_PPDU_ID    0xFFFFFFFF

#define HAL_MAX_DL_MU_USERS	37
#define HAL_MAX_RU_INDEX	7

enum hal_tx_tlv_status {
	HAL_MON_TX_FES_SETUP,
	HAL_MON_TX_FES_STATUS_END,
	HAL_MON_RX_RESPONSE_REQUIRED_INFO,
	HAL_MON_RESPONSE_END_STATUS_INFO,

	HAL_MON_TX_PCU_PPDU_SETUP_INIT,

	HAL_MON_TX_MPDU_START,
	HAL_MON_TX_MSDU_START,
	HAL_MON_TX_BUFFER_ADDR,
	HAL_MON_TX_DATA,

	HAL_MON_TX_FES_STATUS_START,

	HAL_MON_TX_FES_STATUS_PROT,
	HAL_MON_TX_FES_STATUS_START_PROT,

	HAL_MON_TX_FES_STATUS_START_PPDU,
	HAL_MON_TX_FES_STATUS_USER_PPDU,
	HAL_MON_TX_QUEUE_EXTENSION,

	HAL_MON_RX_FRAME_BITMAP_ACK,
	HAL_MON_RX_FRAME_BITMAP_BLOCK_ACK_256,
	HAL_MON_RX_FRAME_BITMAP_BLOCK_ACK_1K,
	HAL_MON_COEX_TX_STATUS,

	HAL_MON_MACTX_HE_SIG_A_SU,
	HAL_MON_MACTX_HE_SIG_A_MU_DL,
	HAL_MON_MACTX_HE_SIG_B1_MU,
	HAL_MON_MACTX_HE_SIG_B2_MU,
	HAL_MON_MACTX_HE_SIG_B2_OFDMA,
	HAL_MON_MACTX_L_SIG_A,
	HAL_MON_MACTX_L_SIG_B,
	HAL_MON_MACTX_HT_SIG,
	HAL_MON_MACTX_VHT_SIG_A,

	HAL_MON_MACTX_USER_DESC_PER_USER,
	HAL_MON_MACTX_USER_DESC_COMMON,
	HAL_MON_MACTX_PHY_DESC,

	HAL_MON_TX_FW2SW,
	HAL_MON_TX_STATUS_PPDU_NOT_DONE,
};

enum txmon_coex_tx_status_reason {
	COEX_FES_TX_START,
	COEX_FES_TX_END,
	COEX_FES_END,
	COEX_RESPONSE_TX_START,
	COEX_RESPONSE_TX_END,
	COEX_NO_TX_ONGOING,
};

enum txmon_transmission_type {
	TXMON_SU_TRANSMISSION = 0,
	TXMON_MU_TRANSMISSION,
	TXMON_MU_SU_TRANSMISSION,
	TXMON_MU_MIMO_TRANSMISSION = 1,
	TXMON_MU_OFDMA_TRANMISSION
};

enum txmon_he_ppdu_subtype {
	TXMON_HE_SUBTYPE_SU = 0,
	TXMON_HE_SUBTYPE_TRIG,
	TXMON_HE_SUBTYPE_MU,
	TXMON_HE_SUBTYPE_EXT_SU
};

enum txmon_pkt_type {
	TXMON_PKT_TYPE_11A = 0,
	TXMON_PKT_TYPE_11B,
	TXMON_PKT_TYPE_11N_MM,
	TXMON_PKT_TYPE_11AC,
	TXMON_PKT_TYPE_11AX,
	TXMON_PKT_TYPE_11BA,
	TXMON_PKT_TYPE_11BE,
	TXMON_PKT_TYPE_11AZ
};

enum txmon_generated_response {
	TXMON_GEN_RESP_SELFGEN_ACK = 0,
	TXMON_GEN_RESP_SELFGEN_CTS,
	TXMON_GEN_RESP_SELFGEN_BA,
	TXMON_GEN_RESP_SELFGEN_MBA,
	TXMON_GEN_RESP_SELFGEN_CBF,
	TXMON_GEN_RESP_SELFGEN_TRIG,
	TXMON_GEN_RESP_SELFGEN_NDP_LMR
};

#ifdef MONITOR_TLV_RECORDING_ENABLE

/*
 * Please make sure that the maximum total size of fields in each TLV
 * is 22 bits.
 * 10 bits are reserved for tlv_tag
 */
struct hal_ppdu_start_tlv_record {
	uint32_t ppdu_id:10;
};

struct hal_ppdu_start_user_info_tlv_record {
	uint32_t user_id:6,
		rate_mcs:4,
		nss:3,
		reception_type:3,
		sgi:2;
};

struct hal_mpdu_start_tlv_record {
	uint32_t user_id:6,
		wrap_flag:1;
};

struct hal_mpdu_end_tlv_record {
	uint32_t user_id:6,
		fcs_err:1,
		wrap_flag:1;
};

struct hal_header_tlv_record {
	uint32_t wrap_flag:1;
};

struct hal_msdu_end_tlv_record {
	uint32_t user_id:6,
		msdu_num:8,
		tid:4,
		tcp_proto:1,
		udp_proto:1,
		wrap_flag:1;
};

struct hal_mon_buffer_addr_tlv_record {
	uint32_t dma_length:12,
		truncation:1,
		continuation:1,
		wrap_flag:1;
};

struct hal_phy_location_tlv_record {
	uint32_t rtt_cfr_status:8,
		rtt_num_streams:8,
		rx_location_info_valid:1;
};

struct hal_ppdu_end_user_stats_tlv_record {
	uint32_t ast_index:16,
		 pkt_type:4;
};

struct hal_pcu_ppdu_end_info_tlv_record {
	uint32_t dialog_topken:8,
		 bb_captured_reason:3,
		 bb_captured_channel:1,
		 bb_captured_timeout:1,
		 mpdu_delimiter_error_seen:1;
};

struct hal_phy_rx_ht_sig_tlv_record {
	uint32_t crc:8,
		 mcs:7,
		 stbc:2,
		 aggregation:1,
		 short_gi:1,
		 fes_coding:1,
		 cbw:1;
};

/* Tx TLVs - structs of Tx TLV with fields to be added here*/

/*
 * enum hal_ppdu_tlv_category - Categories of TLV
 * @PPDU_START: PPDU start level TLV
 * @MPDU: MPDU level TLV
 * @PPDU_END: PPDU end level TLV
 *
 */
enum hal_ppdu_tlv_category {
	CATEGORY_PPDU_START = 1,
	CATEGORY_MPDU,
	CATEGORY_PPDU_END
};
#endif

/**
 * struct hal_txmon_user_desc_per_user - user desc per user information
 * @psdu_length: PSDU length of the user in octet
 * @ru_start_index: RU number to which user is assigned
 * @ru_size: Size of the RU for that user
 * @ofdma_mu_mimo_enabled: mu mimo transmission within the RU
 * @nss: Number of spatial stream occupied by the user
 * @stream_offset: Stream Offset from which the User occupies the Streams
 * @mcs: Modulation Coding Scheme for the User
 * @dcm: Indicates whether dual sub-carrier modulation is applied
 * @fec_type: Indicates whether it is BCC or LDPC
 * @user_bf_type: user beamforming type
 * @drop_user_cbf: frame dropped because of CBF FCS failure
 * @ldpc_extra_symbol: LDPC encoding process
 * @force_extra_symbol: force an extra OFDM symbol
 * @reserved: reserved
 * @sw_peer_id: user sw peer id
 * @per_user_subband_mask: Per user sub band mask
 */
struct hal_txmon_user_desc_per_user {
	uint32_t psdu_length;
	uint32_t ru_start_index		:8,
		 ru_size		:4,
		 ofdma_mu_mimo_enabled	:1,
		 nss			:3,
		 stream_offset		:3,
		 mcs			:4,
		 dcm			:1,
		 fec_type		:1,
		 user_bf_type		:2,
		 drop_user_cbf		:1,
		 ldpc_extra_symbol	:1,
		 force_extra_symbol	:1,
		 reserved		:2;
	uint32_t sw_peer_id		:16,
		 per_user_subband_mask	:16;
};

/**
 * struct hal_txmon_usr_desc_common - user desc common information
 * @num_users: Number of users
 * @ltf_size: LTF size
 * @pkt_extn_pe: packet extension duration of the trigger-based PPDU
 * @a_factor: packet extension duration of the trigger-based PPDU
 * @center_ru_0: Center RU is occupied in the lower 80 MHz band
 * @center_ru_1: Center RU is occupied in the upper 80 MHz band
 * @num_ltf_symbols: number of LTF symbols
 * @doppler_indication: doppler indication
 * @reserved: reserved
 * @spatial_reuse: spatial reuse
 * @ru_channel_0: RU arrangement for band 0
 * @ru_channel_1: RU arrangement for band 1
 */
struct hal_txmon_usr_desc_common {
	uint32_t num_users		:6,
		 ltf_size		:2,
		 pkt_extn_pe		:1,
		 a_factor		:2,
		 center_ru_0		:1,
		 center_ru_1		:1,
		 num_ltf_symbols	:16,
		 doppler_indication	:1,
		 reserved		:2;
	uint16_t spatial_reuse;
	uint16_t ru_channel_0[8];
	uint16_t ru_channel_1[8];
};

#define IS_MULTI_USERS(num_users)	(!!(0xFFFE & num_users))

#define TXMON_HAL(hal_tx_ppdu_info, field)		\
			hal_tx_ppdu_info->field
#define TXMON_HAL_STATUS(hal_tx_ppdu_info, field)	\
			hal_tx_ppdu_info->rx_status.field
#define TXMON_HAL_USER(hal_tx_ppdu_info, user_id, field)		\
			hal_tx_ppdu_info->rx_user_status[user_id].field

#define TXMON_STATUS_INFO(hal_tx_status_info, field)	\
			hal_tx_status_info->field

#ifdef MONITOR_TLV_RECORDING_ENABLE
struct hal_tx_tlv_info {
	uint32_t tlv_tag;
	uint8_t tlv_category;
	uint8_t is_data_ppdu_info;
};
#endif

/**
 * struct hal_tx_status_info - status info that wasn't populated in rx_status
 * @reception_type: su or uplink mu reception type
 * @transmission_type: su or mu transmission type
 * @medium_prot_type: medium protection type
 * @generated_response: Generated frame in response window
 * @band_center_freq1:
 * @band_center_freq2:
 * @freq:
 * @phy_mode:
 * @schedule_id:
 * @no_bitmap_avail: Bitmap available flag
 * @explicit_ack: Explicit Acknowledge flag
 * @explicit_ack_type: Explicit Acknowledge type
 * @r2r_end_status_follow: Response to Response status flag
 * @response_type: Response type in response window
 * @ndp_frame: NDP frame
 * @num_users: number of users
 * @reserved: reserved bits
 * @mba_count: MBA count
 * @mba_fake_bitmap_count: MBA fake bitmap count
 * @sw_frame_group_id: software frame group ID
 * @r2r_to_follow: Response to Response follow flag
 * @phy_abort_reason: Reason for PHY abort
 * @phy_abort_user_number: User number for PHY abort
 * @buffer: Packet buffer pointer address
 * @offset: Packet buffer offset
 * @length: Packet buffer length
 * @protection_addr: Protection Address flag
 * @addr1: MAC address 1
 * @addr2: MAC address 2
 * @addr3: MAC address 3
 * @addr4: MAC address 4
 */
struct hal_tx_status_info {
	uint8_t reception_type;
	uint8_t transmission_type;
	uint8_t medium_prot_type;
	uint8_t generated_response;

	uint16_t band_center_freq1;
	uint16_t band_center_freq2;
	uint16_t freq;
	uint16_t phy_mode;
	uint32_t schedule_id;

	uint32_t no_bitmap_avail	:1,
		explicit_ack		:1,
		explicit_ack_type	:4,
		r2r_end_status_follow	:1,
		response_type		:5,
		ndp_frame		:2,
		num_users		:8,
		reserved		:10;

	uint8_t mba_count;
	uint8_t mba_fake_bitmap_count;

	uint8_t sw_frame_group_id;
	uint32_t r2r_to_follow;

	uint16_t phy_abort_reason;
	uint8_t phy_abort_user_number;

	void *buffer;
	uint32_t offset;
	uint32_t length;

	uint8_t protection_addr;
	uint8_t addr1[QDF_MAC_ADDR_SIZE];
	uint8_t addr2[QDF_MAC_ADDR_SIZE];
	uint8_t addr3[QDF_MAC_ADDR_SIZE];
	uint8_t addr4[QDF_MAC_ADDR_SIZE];
};

/**
 * struct hal_tx_ppdu_info - tx monitor ppdu information
 * @ppdu_id:  Id of the PLCP protocol data unit
 * @num_users: number of users
 * @is_used: boolean flag to identify valid ppdu info
 * @is_data: boolean flag to identify data frame
 * @cur_usr_idx: Current user index of the PPDU
 * @reserved: for future purpose
 * @prot_tlv_status: protection tlv status
 * @tx_tlv_info: store tx tlv info for recording
 * @packet_info: packet information
 * @rx_status: monitor mode rx status information
 * @rx_user_status: monitor mode rx user status information
 */
struct hal_tx_ppdu_info {
	uint32_t ppdu_id;
	uint32_t num_users	:8,
		 is_used	:1,
		 is_data	:1,
		 cur_usr_idx	:8,
		 reserved	:15;

	uint32_t prot_tlv_status;

#ifdef MONITOR_TLV_RECORDING_ENABLE
	struct hal_tx_tlv_info tx_tlv_info;
#endif
	/* placeholder to hold packet buffer info */
	struct hal_mon_packet_info packet_info;
	struct mon_rx_status rx_status;
	struct mon_rx_user_status rx_user_status[];
};

/**
 * hal_tx_status_get_next_tlv() - get next tx status TLV
 * @tx_tlv: pointer to TLV header
 * @is_tlv_hdr_64_bit: Flag to indicate tlv hdr 64 bit
 *
 * Return: pointer to next tlv info
 */
static inline uint8_t*
hal_tx_status_get_next_tlv(uint8_t *tx_tlv, bool is_tlv_hdr_64_bit) {
	uint32_t tlv_len, tlv_hdr_size;

	tlv_len = HAL_RX_GET_USER_TLV32_LEN(tx_tlv);
	tlv_hdr_size = is_tlv_hdr_64_bit ? HAL_RX_TLV64_HDR_SIZE :
					   HAL_RX_TLV32_HDR_SIZE;

	return (uint8_t *)(uintptr_t)qdf_align((uint64_t)((uintptr_t)tx_tlv +
							  tlv_len +
							  tlv_hdr_size),
					       tlv_hdr_size);
}

/**
 * hal_txmon_status_parse_tlv() - process transmit info TLV
 * @hal_soc_hdl: HAL soc handle
 * @data_ppdu_info: pointer to hal data ppdu info
 * @prot_ppdu_info: pointer to hal prot ppdu info
 * @data_status_info: pointer to data status info
 * @prot_status_info: pointer to prot status info
 * @tx_tlv_hdr: pointer to TLV header
 * @status_frag: pointer to status frag
 *
 * Return: HAL_TLV_STATUS_PPDU_NOT_DONE
 */
static inline uint32_t
hal_txmon_status_parse_tlv(hal_soc_handle_t hal_soc_hdl,
			   void *data_ppdu_info,
			   void *prot_ppdu_info,
			   void *data_status_info,
			   void *prot_status_info,
			   void *tx_tlv_hdr,
			   qdf_frag_t status_frag)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	return hal_soc->ops->hal_txmon_status_parse_tlv(data_ppdu_info,
							prot_ppdu_info,
							data_status_info,
							prot_status_info,
							tx_tlv_hdr,
							status_frag);
}

/**
 * hal_txmon_status_get_num_users() - api to get num users from start of fes
 * window
 * @hal_soc_hdl: HAL soc handle
 * @tx_tlv_hdr: pointer to TLV header
 * @num_users: reference to number of user
 *
 * Return: status
 */
static inline uint32_t
hal_txmon_status_get_num_users(hal_soc_handle_t hal_soc_hdl,
			       void *tx_tlv_hdr, uint8_t *num_users)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	return hal_soc->ops->hal_txmon_status_get_num_users(tx_tlv_hdr,
							    num_users);
}

/**
 * hal_tx_status_get_tlv_tag() - api to get tlv tag
 * @tx_tlv_hdr: pointer to TLV header
 *
 * Return tlv_tag
 */
static inline uint32_t
hal_tx_status_get_tlv_tag(void *tx_tlv_hdr)
{
	uint32_t tlv_tag = 0;

	tlv_tag = HAL_RX_GET_USER_TLV32_TYPE(tx_tlv_hdr);

	return tlv_tag;
}

/**
 * hal_txmon_get_word_mask() - api to get word mask for tx monitor
 * @hal_soc_hdl: HAL soc handle
 * @wmask: pointer to hal_txmon_word_mask_config_t
 *
 * Return: bool
 */
static inline bool
hal_txmon_get_word_mask(hal_soc_handle_t hal_soc_hdl,
			hal_txmon_word_mask_config_t *wmask)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	if (hal_soc->ops->hal_txmon_get_word_mask) {
		hal_soc->ops->hal_txmon_get_word_mask(wmask);
		return true;
	}

	return false;
}

/**
 * hal_txmon_is_mon_buf_addr_tlv() - api to find packet buffer addr tlv
 * @hal_soc_hdl: HAL soc handle
 * @tx_tlv_hdr: pointer to TLV header
 *
 * Return: bool
 */
static inline bool
hal_txmon_is_mon_buf_addr_tlv(hal_soc_handle_t hal_soc_hdl, void *tx_tlv_hdr)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	if (qdf_unlikely(!hal_soc->ops->hal_txmon_is_mon_buf_addr_tlv))
		return false;

	return hal_soc->ops->hal_txmon_is_mon_buf_addr_tlv(tx_tlv_hdr);
}

/**
 * hal_txmon_populate_packet_info() - api to populate packet info
 * @hal_soc_hdl: HAL soc handle
 * @tx_tlv_hdr: pointer to TLV header
 * @packet_info: pointer to placeholder for packet info
 *
 * Return void
 */
static inline void
hal_txmon_populate_packet_info(hal_soc_handle_t hal_soc_hdl,
			       void *tx_tlv_hdr,
			       void *packet_info)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	if (qdf_unlikely(!hal_soc->ops->hal_txmon_populate_packet_info))
		return;

	hal_soc->ops->hal_txmon_populate_packet_info(tx_tlv_hdr, packet_info);
}
#endif

static inline uint32_t
hal_rx_parse_u_sig_cmn(struct hal_soc *hal_soc, void *rx_tlv,
		       struct hal_rx_ppdu_info *ppdu_info)
{
	struct hal_mon_usig_hdr *usig = (struct hal_mon_usig_hdr *)rx_tlv;
	struct hal_mon_usig_cmn *usig_1 = &usig->usig_1;
	uint8_t bad_usig_crc;

	bad_usig_crc = HAL_RX_MON_USIG_GET_RX_INTEGRITY_CHECK_PASSED(rx_tlv) ?
			0 : 1;
	ppdu_info->rx_status.usig_common |=
			QDF_MON_STATUS_USIG_PHY_VERSION_KNOWN |
			QDF_MON_STATUS_USIG_BW_KNOWN |
			QDF_MON_STATUS_USIG_UL_DL_KNOWN |
			QDF_MON_STATUS_USIG_BSS_COLOR_KNOWN |
			QDF_MON_STATUS_USIG_TXOP_KNOWN;

	ppdu_info->rx_status.usig_common |= (usig_1->phy_version <<
				   QDF_MON_STATUS_USIG_PHY_VERSION_SHIFT);
	ppdu_info->rx_status.usig_common |= (usig_1->bw <<
					   QDF_MON_STATUS_USIG_BW_SHIFT);
	ppdu_info->rx_status.usig_common |= (usig_1->ul_dl <<
					   QDF_MON_STATUS_USIG_UL_DL_SHIFT);
	ppdu_info->rx_status.usig_common |= (usig_1->bss_color <<
					   QDF_MON_STATUS_USIG_BSS_COLOR_SHIFT);
	ppdu_info->rx_status.usig_common |= (usig_1->txop <<
					   QDF_MON_STATUS_USIG_TXOP_SHIFT);
	ppdu_info->rx_status.usig_common |= bad_usig_crc;

	ppdu_info->u_sig_info.ul_dl = usig_1->ul_dl;
	ppdu_info->u_sig_info.bw = usig_1->bw;
	ppdu_info->rx_status.bw = usig_1->bw;

	return HAL_TLV_STATUS_PPDU_NOT_DONE;
}

static inline uint32_t
hal_rx_parse_u_sig_tb(struct hal_soc *hal_soc, void *rx_tlv,
		      struct hal_rx_ppdu_info *ppdu_info)
{
	struct hal_mon_usig_hdr *usig = (struct hal_mon_usig_hdr *)rx_tlv;
	struct hal_mon_usig_tb *usig_tb = &usig->usig_2.tb;

	ppdu_info->rx_status.usig_mask |=
			QDF_MON_STATUS_USIG_DISREGARD_KNOWN |
			QDF_MON_STATUS_USIG_PPDU_TYPE_N_COMP_MODE_KNOWN |
			QDF_MON_STATUS_USIG_VALIDATE_KNOWN |
			QDF_MON_STATUS_USIG_TB_SPATIAL_REUSE_1_KNOWN |
			QDF_MON_STATUS_USIG_TB_SPATIAL_REUSE_2_KNOWN |
			QDF_MON_STATUS_USIG_TB_DISREGARD1_KNOWN |
			QDF_MON_STATUS_USIG_CRC_KNOWN |
			QDF_MON_STATUS_USIG_TAIL_KNOWN;

	ppdu_info->rx_status.usig_value |= (0x3F <<
				QDF_MON_STATUS_USIG_DISREGARD_SHIFT);
	ppdu_info->rx_status.usig_value |= (usig_tb->ppdu_type_comp_mode <<
			QDF_MON_STATUS_USIG_PPDU_TYPE_N_COMP_MODE_SHIFT);
	ppdu_info->rx_status.usig_value |= (0x1 <<
				QDF_MON_STATUS_USIG_VALIDATE_SHIFT);
	ppdu_info->rx_status.usig_value |= (usig_tb->spatial_reuse_1 <<
				QDF_MON_STATUS_USIG_TB_SPATIAL_REUSE_1_SHIFT);
	ppdu_info->rx_status.usig_value |= (usig_tb->spatial_reuse_2 <<
				QDF_MON_STATUS_USIG_TB_SPATIAL_REUSE_2_SHIFT);
	ppdu_info->rx_status.usig_value |= (0x1F <<
				QDF_MON_STATUS_USIG_TB_DISREGARD1_SHIFT);
	ppdu_info->rx_status.usig_value |= (usig_tb->crc <<
				QDF_MON_STATUS_USIG_CRC_SHIFT);
	ppdu_info->rx_status.usig_value |= (usig_tb->tail <<
				QDF_MON_STATUS_USIG_TAIL_SHIFT);

	ppdu_info->u_sig_info.ppdu_type_comp_mode =
						usig_tb->ppdu_type_comp_mode;

	return HAL_TLV_STATUS_PPDU_NOT_DONE;
}

static inline uint32_t
hal_rx_parse_u_sig_mu(struct hal_soc *hal_soc, void *rx_tlv,
		      struct hal_rx_ppdu_info *ppdu_info)
{
	struct hal_mon_usig_hdr *usig = (struct hal_mon_usig_hdr *)rx_tlv;
	struct hal_mon_usig_mu *usig_mu = &usig->usig_2.mu;

	ppdu_info->rx_status.usig_mask |=
			QDF_MON_STATUS_USIG_DISREGARD_KNOWN |
			QDF_MON_STATUS_USIG_PPDU_TYPE_N_COMP_MODE_KNOWN |
			QDF_MON_STATUS_USIG_VALIDATE_KNOWN |
			QDF_MON_STATUS_USIG_MU_VALIDATE1_KNOWN |
			QDF_MON_STATUS_USIG_MU_PUNCTURE_CH_INFO_KNOWN |
			QDF_MON_STATUS_USIG_MU_VALIDATE2_KNOWN |
			QDF_MON_STATUS_USIG_MU_EHT_SIG_MCS_KNOWN |
			QDF_MON_STATUS_USIG_MU_NUM_EHT_SIG_SYM_KNOWN |
			QDF_MON_STATUS_USIG_CRC_KNOWN |
			QDF_MON_STATUS_USIG_TAIL_KNOWN;

	ppdu_info->rx_status.usig_value |= (0x1F <<
				QDF_MON_STATUS_USIG_DISREGARD_SHIFT);
	ppdu_info->rx_status.usig_value |= (0x1 <<
				QDF_MON_STATUS_USIG_MU_VALIDATE1_SHIFT);
	ppdu_info->rx_status.usig_value |= (usig_mu->ppdu_type_comp_mode <<
			QDF_MON_STATUS_USIG_PPDU_TYPE_N_COMP_MODE_SHIFT);
	ppdu_info->rx_status.usig_value |= (0x1 <<
				QDF_MON_STATUS_USIG_VALIDATE_SHIFT);
	ppdu_info->rx_status.usig_value |= (usig_mu->punc_ch_info <<
				QDF_MON_STATUS_USIG_MU_PUNCTURE_CH_INFO_SHIFT);
	ppdu_info->rx_status.usig_value |= (0x1 <<
				QDF_MON_STATUS_USIG_MU_VALIDATE2_SHIFT);
	ppdu_info->rx_status.usig_value |= (usig_mu->eht_sig_mcs <<
				QDF_MON_STATUS_USIG_MU_EHT_SIG_MCS_SHIFT);
	ppdu_info->rx_status.usig_value |= (usig_mu->num_eht_sig_sym <<
				QDF_MON_STATUS_USIG_MU_NUM_EHT_SIG_SYM_SHIFT);
	ppdu_info->rx_status.usig_value |= (usig_mu->crc <<
				QDF_MON_STATUS_USIG_CRC_SHIFT);
	ppdu_info->rx_status.usig_value |= (usig_mu->tail <<
				QDF_MON_STATUS_USIG_TAIL_SHIFT);

	ppdu_info->u_sig_info.ppdu_type_comp_mode =
						usig_mu->ppdu_type_comp_mode;
	ppdu_info->u_sig_info.eht_sig_mcs = usig_mu->eht_sig_mcs;
	ppdu_info->u_sig_info.num_eht_sig_sym = usig_mu->num_eht_sig_sym;

	return HAL_TLV_STATUS_PPDU_NOT_DONE;
}

static inline uint32_t
hal_rx_parse_u_sig_hdr(struct hal_soc *hal_soc, void *rx_tlv,
		       struct hal_rx_ppdu_info *ppdu_info)
{
	struct hal_mon_usig_hdr *usig = (struct hal_mon_usig_hdr *)rx_tlv;
	struct hal_mon_usig_cmn *usig_1 = &usig->usig_1;

	ppdu_info->rx_status.usig_flags = 1;

	ppdu_info->rx_status.user_info_skip = 1;

	hal_rx_parse_u_sig_cmn(hal_soc, rx_tlv, ppdu_info);

	if (HAL_RX_MON_USIG_GET_PPDU_TYPE_N_COMP_MODE(rx_tlv) == 0 &&
	    usig_1->ul_dl == 1)
		return hal_rx_parse_u_sig_tb(hal_soc, rx_tlv, ppdu_info);
	else
		return hal_rx_parse_u_sig_mu(hal_soc, rx_tlv, ppdu_info);
}

static inline uint32_t
hal_rx_parse_usig_overflow(struct hal_soc *hal_soc, void *tlv,
			   struct hal_rx_ppdu_info *ppdu_info)
{
	struct hal_eht_sig_cc_usig_overflow *usig_ovflow =
		(struct hal_eht_sig_cc_usig_overflow *)tlv;

	ppdu_info->rx_status.eht_known |=
		QDF_MON_STATUS_EHT_SPATIAL_REUSE_KNOWN |
		QDF_MON_STATUS_EHT_EHT_LTF_KNOWN |
		QDF_MON_STATUS_EHT_LDPC_EXTRA_SYMBOL_SEG_KNOWN |
		QDF_MON_STATUS_EHT_PRE_FEC_PADDING_FACTOR_KNOWN |
		QDF_MON_STATUS_EHT_PE_DISAMBIGUITY_KNOWN |
		QDF_MON_STATUS_EHT_DISREARD_KNOWN;

	ppdu_info->rx_status.eht_data[0] |= (usig_ovflow->spatial_reuse <<
				QDF_MON_STATUS_EHT_SPATIAL_REUSE_SHIFT);
	/*
	 * GI and LTF size are separately indicated in radiotap header
	 * and hence will be parsed from other TLV
	 **/
	ppdu_info->rx_status.eht_data[0] |= (usig_ovflow->num_ltf_sym <<
				QDF_MON_STATUS_EHT_EHT_LTF_SHIFT);
	ppdu_info->rx_status.eht_data[0] |= (usig_ovflow->ldpc_extra_sym <<
				QDF_MON_STATUS_EHT_LDPC_EXTRA_SYMBOL_SEG_SHIFT);
	ppdu_info->rx_status.eht_data[0] |= (usig_ovflow->pre_fec_pad_factor <<
			QDF_MON_STATUS_EHT_PRE_FEC_PADDING_FACTOR_SHIFT);
	ppdu_info->rx_status.eht_data[0] |= (usig_ovflow->pe_disambiguity <<
				QDF_MON_STATUS_EHT_PE_DISAMBIGUITY_SHIFT);
	ppdu_info->rx_status.eht_data[0] |= (0xF <<
				QDF_MON_STATUS_EHT_DISREGARD_SHIFT);

	return HAL_TLV_STATUS_PPDU_NOT_DONE;
}

static inline uint32_t
hal_rx_parse_non_ofdma_users(struct hal_soc *hal_soc, void *tlv,
			     struct hal_rx_ppdu_info *ppdu_info)
{
	struct hal_eht_sig_non_ofdma_cmn_eb *non_ofdma_cmn_eb =
				(struct hal_eht_sig_non_ofdma_cmn_eb *)tlv;

	ppdu_info->rx_status.eht_known |=
				QDF_MON_STATUS_EHT_NUM_NON_OFDMA_USERS_KNOWN;

	ppdu_info->rx_status.eht_data[7] |= (non_ofdma_cmn_eb->num_users <<
				QDF_MON_STATUS_EHT_NUM_NON_OFDMA_USERS_SHIFT);

	return HAL_TLV_STATUS_PPDU_NOT_DONE;
}

static inline void
hal_rx_parse_eht_mumimo_user_info(uint32_t *eht_user_info,
				  struct hal_eht_sig_mu_mimo_user_info
				  *user_info)
{
	*eht_user_info |=  QDF_MON_STATUS_EHT_USER_STA_ID_KNOWN |
			   QDF_MON_STATUS_EHT_USER_MCS_KNOWN |
			   QDF_MON_STATUS_EHT_USER_CODING_KNOWN |
			   QDF_MON_STATUS_EHT_USER_SPATIAL_CONFIG_KNOWN;

	*eht_user_info |= (user_info->sta_id <<
			   QDF_MON_STATUS_EHT_USER_STA_ID_SHIFT);
	*eht_user_info |= (user_info->mcs <<
			   QDF_MON_STATUS_EHT_USER_MCS_SHIFT);

	*eht_user_info |= (user_info->coding <<
			   QDF_MON_STATUS_EHT_USER_CODING_SHIFT);
	*eht_user_info |= (user_info->spatial_coding <<
			   QDF_MON_STATUS_EHT_USER_SPATIAL_CONFIG_SHIFT);
}

static inline uint32_t
hal_rx_parse_eht_sig_mumimo_user_info(struct hal_soc *hal_soc, void *tlv,
				      struct hal_rx_ppdu_info *ppdu_info)
{
	struct hal_eht_sig_mu_mimo_user_info *user_info;
	struct mon_rx_status *rx_status;
	struct mon_rx_user_status *rx_user_status;
	uint32_t *eht_user_info;
	uint32_t user_idx, i;
	uint32_t *user_field;

	i = 0;
	rx_status = &ppdu_info->rx_status;
	user_field = (uint32_t *)((uint8_t *)tlv + ppdu_info->tlv_aggr.rd_idx);

	while ((i++ < MAX_USR_INFO_STR_CNT) &&
	       (ppdu_info->tlv_aggr.rd_idx < ppdu_info->tlv_aggr.cur_len)) {
		user_idx = rx_status->num_eht_user_info_valid;
		rx_user_status = &ppdu_info->rx_user_status[user_idx];
		user_info = (struct hal_eht_sig_mu_mimo_user_info *)user_field;
		eht_user_info = &rx_user_status->eht_user_info;

		hal_rx_parse_eht_mumimo_user_info(eht_user_info, user_info);
		rx_status->mcs = user_info->mcs;

		/* CRC for matched user block */
		rx_user_status->eht_known |=
			QDF_MON_STATUS_EHT_USER_ENC_BLOCK_CRC_KNOWN |
			QDF_MON_STATUS_EHT_USER_ENC_BLOCK_TAIL_KNOWN;
		rx_user_status->eht_data[7] |=
			(user_info->crc <<
			 QDF_MON_STATUS_EHT_USER_ENC_BLOCK_CRC_SHIFT);

		ppdu_info->tlv_aggr.rd_idx += 4;
		user_field++;
		rx_status->num_eht_user_info_valid++;
	}

	return HAL_TLV_STATUS_PPDU_NOT_DONE;
}

static inline void
hal_rx_parse_eht_sig_mumimo_all_user_info(struct hal_soc *hal_soc, void *tlv,
					  struct hal_rx_ppdu_info *ppdu_info)
{
	struct hal_eht_sig_mu_mimo_user_info *user_info;
	uint32_t *eht_user_info;
	uint32_t user_idx = ppdu_info->rx_status.num_eht_all_user_info_valid;

	user_info = (struct hal_eht_sig_mu_mimo_user_info *)tlv;

	eht_user_info = &ppdu_info->rx_status.eht_user_info[user_idx];

	hal_rx_parse_eht_mumimo_user_info(eht_user_info, user_info);

	ppdu_info->rx_status.num_eht_all_user_info_valid++;
}

static inline void
hal_rx_parse_eht_non_mumimo_user_info(uint32_t *eht_user_info,
				      struct hal_eht_sig_non_mu_mimo_user_info
				      *user_info)
{
	*eht_user_info |= QDF_MON_STATUS_EHT_USER_STA_ID_KNOWN |
			  QDF_MON_STATUS_EHT_USER_MCS_KNOWN |
			  QDF_MON_STATUS_EHT_USER_CODING_KNOWN |
			  QDF_MON_STATUS_EHT_USER_NSS_KNOWN |
			  QDF_MON_STATUS_EHT_USER_BEAMFORMING_KNOWN;
	*eht_user_info |= (user_info->sta_id <<
			   QDF_MON_STATUS_EHT_USER_STA_ID_SHIFT);
	*eht_user_info |= (user_info->mcs <<
			   QDF_MON_STATUS_EHT_USER_MCS_SHIFT);
	*eht_user_info |= (user_info->nss <<
			   QDF_MON_STATUS_EHT_USER_NSS_SHIFT);
	*eht_user_info |= (user_info->beamformed <<
			   QDF_MON_STATUS_EHT_USER_BEAMFORMING_SHIFT);
	*eht_user_info |= (user_info->coding <<
			   QDF_MON_STATUS_EHT_USER_CODING_SHIFT);
}

static inline void
hal_rx_parse_eht_sig_non_mumimo_user_info(struct hal_soc *hal_soc, void *tlv,
					  struct hal_rx_ppdu_info *ppdu_info)
{
	struct hal_eht_sig_non_mu_mimo_user_info *user_info;
	struct mon_rx_status *rx_status;
	struct mon_rx_user_status *rx_user_status;
	uint32_t *eht_user_info;
	uint32_t user_idx, i;
	uint32_t *user_field;

	i = 0;
	rx_status = &ppdu_info->rx_status;
	user_field = (uint32_t *)((uint8_t *)tlv + ppdu_info->tlv_aggr.rd_idx);

	while ((i++ < MAX_USR_INFO_STR_CNT) &&
	       (ppdu_info->tlv_aggr.rd_idx < ppdu_info->tlv_aggr.cur_len)) {
		user_idx = rx_status->num_eht_user_info_valid;

		rx_user_status = &ppdu_info->rx_user_status[user_idx];
		user_info =
			(struct hal_eht_sig_non_mu_mimo_user_info *)user_field;
		eht_user_info = &rx_user_status->eht_user_info;
		hal_rx_parse_eht_non_mumimo_user_info(eht_user_info, user_info);

		ppdu_info->rx_status.mcs = user_info->mcs;

		/* CRC for matched user block */
		rx_user_status->eht_known |=
			QDF_MON_STATUS_EHT_USER_ENC_BLOCK_CRC_KNOWN |
			QDF_MON_STATUS_EHT_USER_ENC_BLOCK_TAIL_KNOWN;
		rx_user_status->eht_data[7] |=
			(user_info->crc <<
			 QDF_MON_STATUS_EHT_USER_ENC_BLOCK_CRC_SHIFT);

		ppdu_info->tlv_aggr.rd_idx += 4;
		user_field++;
		rx_status->num_eht_user_info_valid++;
	}
}

static inline void
hal_rx_parse_eht_sig_non_mumimo_all_user_info(struct hal_soc *hal_soc,
					      void *tlv, struct hal_rx_ppdu_info
					      *ppdu_info)
{
	struct hal_eht_sig_non_mu_mimo_user_info *user_info;
	uint32_t *eht_user_info;
	uint32_t user_idx = ppdu_info->rx_status.num_eht_all_user_info_valid;

	user_info = (struct hal_eht_sig_non_mu_mimo_user_info *)tlv;

	eht_user_info = &ppdu_info->rx_status.eht_user_info[user_idx];

	hal_rx_parse_eht_non_mumimo_user_info(eht_user_info, user_info);

	ppdu_info->rx_status.num_eht_all_user_info_valid++;
}

static inline bool hal_rx_is_ofdma(struct hal_soc *hal_soc,
				   struct hal_rx_ppdu_info *ppdu_info)
{
	if (ppdu_info->u_sig_info.ppdu_type_comp_mode == 0 &&
	    ppdu_info->u_sig_info.ul_dl == 0)
		return true;

	return false;
}

static inline bool hal_rx_is_non_ofdma(struct hal_soc *hal_soc,
				       struct hal_rx_ppdu_info *ppdu_info)
{
	uint32_t ppdu_type_comp_mode =
				ppdu_info->u_sig_info.ppdu_type_comp_mode;
	uint32_t ul_dl = ppdu_info->u_sig_info.ul_dl;

	if ((ppdu_type_comp_mode == 1 && ul_dl == 0) ||
	    (ppdu_type_comp_mode == 2 && ul_dl == 0) ||
	    (ppdu_type_comp_mode == 1 && ul_dl == 1))
		return true;

	return false;
}

static inline bool hal_rx_is_mu_mimo_user(struct hal_soc *hal_soc,
					  struct hal_rx_ppdu_info *ppdu_info)
{
	if (ppdu_info->u_sig_info.ppdu_type_comp_mode == 2 &&
	    ppdu_info->u_sig_info.ul_dl == 0)
		return true;

	return false;
}

static inline bool
hal_rx_is_frame_type_ndp(struct hal_soc *hal_soc,
			 struct hal_rx_ppdu_info *ppdu_info)
{
	if (ppdu_info->u_sig_info.ppdu_type_comp_mode == 1 &&
	    ppdu_info->u_sig_info.eht_sig_mcs == 0 &&
	    ppdu_info->u_sig_info.num_eht_sig_sym == 0)
		return true;

	return false;
}

static inline uint32_t
hal_rx_parse_eht_sig_ndp(struct hal_soc *hal_soc, void *tlv,
			 struct hal_rx_ppdu_info *ppdu_info)
{
	struct hal_eht_sig_ndp_cmn_eb *eht_sig_ndp =
				(struct hal_eht_sig_ndp_cmn_eb *)tlv;

	ppdu_info->rx_status.eht_known |=
		QDF_MON_STATUS_EHT_SPATIAL_REUSE_KNOWN |
		QDF_MON_STATUS_EHT_EHT_LTF_KNOWN |
		QDF_MON_STATUS_EHT_NDP_NSS_KNOWN |
		QDF_MON_STATUS_EHT_NDP_BEAMFORMED_KNOWN |
		QDF_MON_STATUS_EHT_NDP_DISREGARD_KNOWN |
		QDF_MON_STATUS_EHT_CRC1_KNOWN |
		QDF_MON_STATUS_EHT_TAIL1_KNOWN;

	ppdu_info->rx_status.eht_data[0] |= (eht_sig_ndp->spatial_reuse <<
				QDF_MON_STATUS_EHT_SPATIAL_REUSE_SHIFT);
	/*
	 * GI and LTF size are separately indicated in radiotap header
	 * and hence will be parsed from other TLV
	 **/
	ppdu_info->rx_status.eht_data[0] |= (eht_sig_ndp->num_ltf_sym <<
				QDF_MON_STATUS_EHT_EHT_LTF_SHIFT);
	ppdu_info->rx_status.eht_data[0] |= (0xF <<
				QDF_MON_STATUS_EHT_NDP_DISREGARD_SHIFT);

	ppdu_info->rx_status.eht_data[7] |= (eht_sig_ndp->nss <<
				QDF_MON_STATUS_EHT_NDP_NSS_SHIFT);
	ppdu_info->rx_status.eht_data[7] |= (eht_sig_ndp->beamformed <<
				QDF_MON_STATUS_EHT_NDP_BEAMFORMED_SHIFT);

	ppdu_info->rx_status.eht_data[0] |= (eht_sig_ndp->crc <<
					QDF_MON_STATUS_EHT_CRC1_SHIFT);

	return HAL_TLV_STATUS_PPDU_NOT_DONE;
}

#ifdef WLAN_FEATURE_11BE
static inline void
hal_rx_parse_punctured_pattern(struct phyrx_common_user_info *cmn_usr_info,
			       struct hal_rx_ppdu_info *ppdu_info)
{
	ppdu_info->rx_status.punctured_pattern = cmn_usr_info->puncture_bitmap;
}
#else
static inline void
hal_rx_parse_punctured_pattern(struct phyrx_common_user_info *cmn_usr_info,
			       struct hal_rx_ppdu_info *ppdu_info)
{
}
#endif
static inline uint32_t
hal_rx_parse_cmn_usr_info(struct hal_soc *hal_soc, uint8_t *tlv,
			  struct hal_rx_ppdu_info *ppdu_info)
{
	struct phyrx_common_user_info *cmn_usr_info =
				(struct phyrx_common_user_info *)tlv;

	ppdu_info->rx_status.eht_known |=
				QDF_MON_STATUS_EHT_GUARD_INTERVAL_KNOWN |
				QDF_MON_STATUS_EHT_LTF_KNOWN;

	ppdu_info->rx_status.eht_data[0] |= (cmn_usr_info->cp_setting <<
					     QDF_MON_STATUS_EHT_GI_SHIFT);
	if (!ppdu_info->rx_status.sgi)
		ppdu_info->rx_status.sgi = cmn_usr_info->cp_setting;

	ppdu_info->rx_status.eht_data[0] |= (cmn_usr_info->ltf_size <<
					     QDF_MON_STATUS_EHT_LTF_SHIFT);
	if (!ppdu_info->rx_status.ltf_size)
		ppdu_info->rx_status.ltf_size = cmn_usr_info->ltf_size;

	hal_rx_parse_punctured_pattern(cmn_usr_info, ppdu_info);

	return HAL_TLV_STATUS_PPDU_NOT_DONE;
}

#ifdef WLAN_FEATURE_11BE
static inline void
hal_rx_ul_ofdma_ru_size_to_width(uint32_t ru_size,
				 uint32_t *ru_width)
{
	uint32_t width;

	width = 0;
	switch (ru_size) {
	case IEEE80211_EHT_RU_26:
		width = RU_26;
		break;
	case IEEE80211_EHT_RU_52:
		width = RU_52;
		break;
	case IEEE80211_EHT_RU_52_26:
		width = RU_52_26;
		break;
	case IEEE80211_EHT_RU_106:
		width = RU_106;
		break;
	case IEEE80211_EHT_RU_106_26:
		width = RU_106_26;
		break;
	case IEEE80211_EHT_RU_242:
		width = RU_242;
		break;
	case IEEE80211_EHT_RU_484:
		width = RU_484;
		break;
	case IEEE80211_EHT_RU_484_242:
		width = RU_484_242;
		break;
	case IEEE80211_EHT_RU_996:
		width = RU_996;
		break;
	case IEEE80211_EHT_RU_996_484:
		width = RU_996_484;
		break;
	case IEEE80211_EHT_RU_996_484_242:
		width = RU_996_484_242;
		break;
	case IEEE80211_EHT_RU_996x2:
		width = RU_2X996;
		break;
	case IEEE80211_EHT_RU_996x2_484:
		width = RU_2X996_484;
		break;
	case IEEE80211_EHT_RU_996x3:
		width = RU_3X996;
		break;
	case IEEE80211_EHT_RU_996x3_484:
		width = RU_3X996_484;
		break;
	case IEEE80211_EHT_RU_996x4:
		width = RU_4X996;
		break;
	default:
		hal_err_rl("RU size(%d) to width convert err", ru_size);
		break;
	}
	*ru_width = width;
}
#else
static inline void
hal_rx_ul_ofdma_ru_size_to_width(uint32_t ru_size,
				 uint32_t *ru_width)
{
	*ru_width = 0;
}
#endif

static inline enum ieee80211_eht_ru_size
hal_rx_mon_hal_ru_size_to_ieee80211_ru_size(struct hal_soc *hal_soc,
					    uint32_t hal_ru_size)
{
	switch (hal_ru_size) {
	case HAL_EHT_RU_26:
		return IEEE80211_EHT_RU_26;
	case HAL_EHT_RU_52:
		return IEEE80211_EHT_RU_52;
	case HAL_EHT_RU_78:
		return IEEE80211_EHT_RU_52_26;
	case HAL_EHT_RU_106:
		return IEEE80211_EHT_RU_106;
	case HAL_EHT_RU_132:
		return IEEE80211_EHT_RU_106_26;
	case HAL_EHT_RU_242:
		return IEEE80211_EHT_RU_242;
	case HAL_EHT_RU_484:
		return IEEE80211_EHT_RU_484;
	case HAL_EHT_RU_726:
		return IEEE80211_EHT_RU_484_242;
	case HAL_EHT_RU_996:
		return IEEE80211_EHT_RU_996;
	case HAL_EHT_RU_996x2:
		return IEEE80211_EHT_RU_996x2;
	case HAL_EHT_RU_996x3:
		return IEEE80211_EHT_RU_996x3;
	case HAL_EHT_RU_996x4:
		return IEEE80211_EHT_RU_996x4;
	case HAL_EHT_RU_NONE:
		return IEEE80211_EHT_RU_INVALID;
	case HAL_EHT_RU_996_484:
		return IEEE80211_EHT_RU_996_484;
	case HAL_EHT_RU_996x2_484:
		return IEEE80211_EHT_RU_996x2_484;
	case HAL_EHT_RU_996x3_484:
		return IEEE80211_EHT_RU_996x3_484;
	case HAL_EHT_RU_996_484_242:
		return IEEE80211_EHT_RU_996_484_242;
	default:
		return IEEE80211_EHT_RU_INVALID;
	}
}

#define HAL_SET_RU_PER80(ru_320mhz, ru_per80, ru_idx_per80mhz, num_80mhz) \
	((ru_320mhz) |= ((uint64_t)(ru_per80) << \
		       (((num_80mhz) * NUM_RU_BITS_PER80) + \
			((ru_idx_per80mhz) * NUM_RU_BITS_PER20))))

static inline uint32_t
hal_rx_parse_receive_user_info(struct hal_soc *hal_soc, uint8_t *tlv,
			       struct hal_rx_ppdu_info *ppdu_info,
			       uint32_t user_id)
{
	struct receive_user_info *rx_usr_info = (struct receive_user_info *)tlv;
	struct mon_rx_user_status *mon_rx_user_status = NULL;
	uint64_t ru_index_320mhz = 0;
	uint16_t ru_index_per80mhz;
	uint32_t ru_size = 0, num_80mhz_with_ru = 0;
	uint32_t ru_index = HAL_EHT_RU_INVALID;
	uint32_t rtap_ru_size = IEEE80211_EHT_RU_INVALID;
	uint32_t ru_width;

	if (ppdu_info->rx_status.user_info_skip)
		return HAL_TLV_STATUS_PPDU_NOT_DONE;

	switch (rx_usr_info->reception_type) {
	case HAL_RECEPTION_TYPE_SU:
		ppdu_info->rx_status.reception_type = HAL_RX_TYPE_SU;
		break;
	case HAL_RECEPTION_TYPE_DL_MU_MIMO:
		ppdu_info->rx_status.mu_dl_ul = HAL_RX_TYPE_DL;
		ppdu_info->rx_status.reception_type = HAL_RX_TYPE_MU_MIMO;
		break;
	case HAL_RECEPTION_TYPE_UL_MU_MIMO:
		ppdu_info->rx_status.mu_dl_ul = HAL_RX_TYPE_UL;
		ppdu_info->rx_status.reception_type = HAL_RX_TYPE_MU_MIMO;
		break;
	case HAL_RECEPTION_TYPE_DL_MU_OFMA:
		ppdu_info->rx_status.mu_dl_ul = HAL_RX_TYPE_DL;
		ppdu_info->rx_status.reception_type = HAL_RX_TYPE_MU_OFDMA;
		break;
	case HAL_RECEPTION_TYPE_UL_MU_OFDMA:
		ppdu_info->rx_status.mu_dl_ul = HAL_RX_TYPE_UL;
		ppdu_info->rx_status.reception_type = HAL_RX_TYPE_MU_OFDMA;
		break;
	case HAL_RECEPTION_TYPE_DL_MU_OFDMA_MIMO:
		ppdu_info->rx_status.mu_dl_ul = HAL_RX_TYPE_DL;
		ppdu_info->rx_status.reception_type = HAL_RX_TYPE_MU_OFDMA_MIMO;
		break;
	case HAL_RECEPTION_TYPE_UL_MU_OFDMA_MIMO:
		ppdu_info->rx_status.mu_dl_ul = HAL_RX_TYPE_UL;
		ppdu_info->rx_status.reception_type = HAL_RX_TYPE_MU_OFDMA_MIMO;
		break;
	}

	ppdu_info->start_user_info_cnt++;

	ppdu_info->rx_status.is_stbc = rx_usr_info->stbc;
	ppdu_info->rx_status.ldpc = rx_usr_info->ldpc;
	ppdu_info->rx_status.dcm = rx_usr_info->sta_dcm;
	ppdu_info->rx_status.mcs = rx_usr_info->rate_mcs;
	ppdu_info->rx_status.nss = rx_usr_info->nss + 1;

	if (user_id < HAL_MAX_UL_MU_USERS) {
		mon_rx_user_status =
			&ppdu_info->rx_user_status[user_id];
		mon_rx_user_status->eht_known |=
				QDF_MON_STATUS_EHT_CONTENT_CH_INDEX_KNOWN;
		mon_rx_user_status->eht_data[0] |=
				(rx_usr_info->dl_ofdma_content_channel <<
				 QDF_MON_STATUS_EHT_CONTENT_CH_INDEX_SHIFT);

		mon_rx_user_status->is_stbc =
			ppdu_info->rx_status.is_stbc;
		mon_rx_user_status->ldpc =
			ppdu_info->rx_status.ldpc;
		mon_rx_user_status->dcm =
			ppdu_info->rx_status.dcm;
		mon_rx_user_status->mcs = ppdu_info->rx_status.mcs;
		mon_rx_user_status->nss = ppdu_info->rx_status.nss;
	}

	if (!(ppdu_info->rx_status.reception_type == HAL_RX_TYPE_MU_MIMO ||
	      ppdu_info->rx_status.reception_type == HAL_RX_TYPE_MU_OFDMA ||
	      ppdu_info->rx_status.reception_type == HAL_RX_TYPE_MU_OFDMA_MIMO))
		return HAL_TLV_STATUS_PPDU_NOT_DONE;

	/* RU allocation present only for OFDMA reception */
	if (rx_usr_info->ru_type_80_0 != HAL_EHT_RU_NONE) {
		ru_size += rx_usr_info->ru_type_80_0;
		ru_index = ru_index_per80mhz = rx_usr_info->ru_start_index_80_0;
		HAL_SET_RU_PER80(ru_index_320mhz, rx_usr_info->ru_type_80_0,
				 ru_index_per80mhz, 0);
		num_80mhz_with_ru++;
	}

	if (rx_usr_info->ru_type_80_1 != HAL_EHT_RU_NONE) {
		ru_size += rx_usr_info->ru_type_80_1;
		ru_index = ru_index_per80mhz = rx_usr_info->ru_start_index_80_1;
		HAL_SET_RU_PER80(ru_index_320mhz, rx_usr_info->ru_type_80_1,
				 ru_index_per80mhz, 1);
		num_80mhz_with_ru++;
	}

	if (rx_usr_info->ru_type_80_2 != HAL_EHT_RU_NONE) {
		ru_size += rx_usr_info->ru_type_80_2;
		ru_index = ru_index_per80mhz = rx_usr_info->ru_start_index_80_2;
		HAL_SET_RU_PER80(ru_index_320mhz, rx_usr_info->ru_type_80_2,
				 ru_index_per80mhz, 2);
		num_80mhz_with_ru++;
	}

	if (rx_usr_info->ru_type_80_3 != HAL_EHT_RU_NONE) {
		ru_size += rx_usr_info->ru_type_80_3;
		ru_index = ru_index_per80mhz = rx_usr_info->ru_start_index_80_3;
		HAL_SET_RU_PER80(ru_index_320mhz, rx_usr_info->ru_type_80_3,
				 ru_index_per80mhz, 3);
		num_80mhz_with_ru++;
	}

	if (num_80mhz_with_ru > 1) {
		/* Calculate the MRU index */
		switch (ru_index_320mhz) {
		case HAL_EHT_RU_996_484_0:
		case HAL_EHT_RU_996x2_484_0:
		case HAL_EHT_RU_996x3_484_0:
			ru_index = 0;
			break;
		case HAL_EHT_RU_996_484_1:
		case HAL_EHT_RU_996x2_484_1:
		case HAL_EHT_RU_996x3_484_1:
			ru_index = 1;
			break;
		case HAL_EHT_RU_996_484_2:
		case HAL_EHT_RU_996x2_484_2:
		case HAL_EHT_RU_996x3_484_2:
			ru_index = 2;
			break;
		case HAL_EHT_RU_996_484_3:
		case HAL_EHT_RU_996x2_484_3:
		case HAL_EHT_RU_996x3_484_3:
			ru_index = 3;
			break;
		case HAL_EHT_RU_996_484_4:
		case HAL_EHT_RU_996x2_484_4:
		case HAL_EHT_RU_996x3_484_4:
			ru_index = 4;
			break;
		case HAL_EHT_RU_996_484_5:
		case HAL_EHT_RU_996x2_484_5:
		case HAL_EHT_RU_996x3_484_5:
			ru_index = 5;
			break;
		case HAL_EHT_RU_996_484_6:
		case HAL_EHT_RU_996x2_484_6:
		case HAL_EHT_RU_996x3_484_6:
			ru_index = 6;
			break;
		case HAL_EHT_RU_996_484_7:
		case HAL_EHT_RU_996x2_484_7:
		case HAL_EHT_RU_996x3_484_7:
			ru_index = 7;
			break;
		case HAL_EHT_RU_996x2_484_8:
			ru_index = 8;
			break;
		case HAL_EHT_RU_996x2_484_9:
			ru_index = 9;
			break;
		case HAL_EHT_RU_996x2_484_10:
			ru_index = 10;
			break;
		case HAL_EHT_RU_996x2_484_11:
			ru_index = 11;
			break;
		default:
			ru_index = HAL_EHT_RU_INVALID;
			dp_debug("Invalid RU index");
			qdf_assert(0);
			break;
		}
		ru_size += 4;
	}

	rtap_ru_size = hal_rx_mon_hal_ru_size_to_ieee80211_ru_size(hal_soc,
								   ru_size);

	if (rx_usr_info->pkt_type == HAL_RX_PKT_TYPE_11AX &&
	    mon_rx_user_status) {
		if (ru_index != HAL_EHT_RU_INVALID) {
			mon_rx_user_status->he_data2 |=
				QDF_MON_STATUS_RU_ALLOCATION_OFFSET_KNOWN;
			mon_rx_user_status->he_data2 |=
				ru_index << QDF_MON_STATUS_RU_ALLOCATION_SHIFT;
		}
	}

	if (rx_usr_info->pkt_type == HAL_RX_PKT_TYPE_11BE &&
	    mon_rx_user_status) {
		if (rtap_ru_size != IEEE80211_EHT_RU_INVALID) {
			mon_rx_user_status->eht_known |=
					QDF_MON_STATUS_EHT_RU_MRU_SIZE_KNOWN;
			mon_rx_user_status->eht_data[1] |= (rtap_ru_size <<
					QDF_MON_STATUS_EHT_RU_MRU_SIZE_SHIFT);
		}

		if (ru_index != HAL_EHT_RU_INVALID) {
			mon_rx_user_status->eht_known |=
					QDF_MON_STATUS_EHT_RU_MRU_INDEX_KNOWN;
			mon_rx_user_status->eht_data[1] |= (ru_index <<
					QDF_MON_STATUS_EHT_RU_MRU_INDEX_SHIFT);
		}
	}

	if (rx_usr_info->pkt_type == HAL_RX_PKT_TYPE_11AX &&
	    mon_rx_user_status) {
		if (ppdu_info->rx_status.reception_type ==
		    HAL_RX_TYPE_MU_MIMO) {
			ppdu_info->rx_status.he_mu_flags = 1;

			/* HE-data1 */
			mon_rx_user_status->he_data1 |=
				QDF_MON_STATUS_HE_MCS_KNOWN |
				QDF_MON_STATUS_HE_CODING_KNOWN;

			/* HE-data2 */

			/* HE-data3 */
			mon_rx_user_status->he_data3 |=
				mon_rx_user_status->mcs <<
				QDF_MON_STATUS_TRANSMIT_MCS_SHIFT;

			mon_rx_user_status->he_data3 |=
				ppdu_info->rx_status.ldpc <<
				QDF_MON_STATUS_CODING_SHIFT;

			/* HE-data4 */
			mon_rx_user_status->he_data4 |=
				mon_rx_user_status->sta_id <<
				QDF_MON_STATUS_STA_ID_SHIFT;
			/* HE-data5 */

			/* HE-data6 */
			mon_rx_user_status->he_data6 |=
				mon_rx_user_status->nss <<
				QDF_MON_STATUS_HE_DATA_6_NSS_SHIFT;
		}

		if (ppdu_info->rx_status.reception_type ==
		    HAL_RX_TYPE_MU_OFDMA) {
			ppdu_info->rx_status.he_mu_flags = 1;

			/* HE-data1 */
			mon_rx_user_status->he_data1 |=
				QDF_MON_STATUS_HE_MCS_KNOWN |
				QDF_MON_STATUS_HE_DCM_KNOWN |
				QDF_MON_STATUS_HE_CODING_KNOWN;

			/* HE-data2 */

			/* HE-data3 */
			mon_rx_user_status->he_data3 |=
				mon_rx_user_status->mcs <<
				QDF_MON_STATUS_TRANSMIT_MCS_SHIFT;

			mon_rx_user_status->he_data3 |=
				mon_rx_user_status->dcm <<
				QDF_MON_STATUS_DCM_SHIFT;

			mon_rx_user_status->he_data3 |=
				mon_rx_user_status->ldpc <<
				QDF_MON_STATUS_CODING_SHIFT;

			/* HE-data4 */
			mon_rx_user_status->he_data4 |=
				mon_rx_user_status->sta_id <<
				QDF_MON_STATUS_STA_ID_SHIFT;

			/* HE-data5 */
			//txbf not exist
			mon_rx_user_status->he_data5 |=
				mon_rx_user_status->beamformed <<
				QDF_MON_STATUS_TXBF_SHIFT;

			/* HE-data6 */
			mon_rx_user_status->he_data6 |=
				mon_rx_user_status->nss <<
				QDF_MON_STATUS_HE_DATA_6_NSS_SHIFT;

			mon_rx_user_status->he_flags1 =
				ppdu_info->rx_status.he_flags1;
			mon_rx_user_status->he_flags2 =
				ppdu_info->rx_status.he_flags2;
		}
	}

	if (mon_rx_user_status && ru_index != HAL_EHT_RU_INVALID &&
	    rtap_ru_size != IEEE80211_EHT_RU_INVALID) {
		mon_rx_user_status->ofdma_ru_start_index = ru_index;
		mon_rx_user_status->ofdma_ru_size = rtap_ru_size;
		hal_rx_ul_ofdma_ru_size_to_width(rtap_ru_size, &ru_width);
		mon_rx_user_status->ofdma_ru_width = ru_width;
		mon_rx_user_status->mu_ul_info_valid = 1;
	}

	return HAL_TLV_STATUS_PPDU_NOT_DONE;
}

#ifdef WLAN_PKT_CAPTURE_RX_2_0
static inline void
hal_rx_status_get_mpdu_retry_cnt(struct hal_rx_ppdu_info *ppdu_info,
				 hal_rx_mon_ppdu_end_user_t *rx_ppdu_end_user)
{
		ppdu_info->rx_status.mpdu_retry_cnt =
			rx_ppdu_end_user->retried_mpdu_count;
}

static inline void
hal_rx_status_get_mon_buf_addr(uint8_t *rx_tlv,
			       struct hal_rx_ppdu_info *ppdu_info)
{
	struct mon_buffer_addr *addr = (struct mon_buffer_addr *)rx_tlv;

	ppdu_info->packet_info.sw_cookie =
			(((uint64_t)addr->buffer_virt_addr_63_32 << 32) |
			(addr->buffer_virt_addr_31_0));
	/* HW DMA length is '-1' of actual DMA length*/
	ppdu_info->packet_info.dma_length = addr->dma_length + 1;
	ppdu_info->packet_info.msdu_continuation = addr->msdu_continuation;
	ppdu_info->packet_info.truncated = addr->truncated;

}

static inline void
hal_rx_update_ppdu_drop_cnt(uint8_t *rx_tlv,
			    struct hal_rx_ppdu_info *ppdu_info)
{
	struct mon_drop *drop_cnt = (struct mon_drop *)rx_tlv;

	ppdu_info->drop_cnt.ppdu_drop_cnt = drop_cnt->ppdu_drop_cnt;
	ppdu_info->drop_cnt.mpdu_drop_cnt = drop_cnt->mpdu_drop_cnt;
	ppdu_info->drop_cnt.end_of_ppdu_drop_cnt = drop_cnt->end_of_ppdu_seen;
	ppdu_info->drop_cnt.tlv_drop_cnt = drop_cnt->tlv_drop_cnt;
}
#else
static inline void
hal_rx_status_get_mpdu_retry_cnt(struct hal_rx_ppdu_info *ppdu_info,
				 hal_rx_mon_ppdu_end_user_t *rx_ppdu_end_user)
{
		ppdu_info->rx_status.mpdu_retry_cnt = 0;
}
static inline void
hal_rx_status_get_mon_buf_addr(uint8_t *rx_tlv,
			       struct hal_rx_ppdu_info *ppdu_info)
{
}

static inline void
hal_rx_update_ppdu_drop_cnt(uint8_t *rx_tlv,
			    struct hal_rx_ppdu_info *ppdu_info)
{
}
#endif

#ifdef WLAN_SUPPORT_CTRL_FRAME_STATS
static inline void
hal_update_rx_ctrl_frame_stats(struct hal_rx_ppdu_info *ppdu_info,
			       uint32_t user_id)
{
	uint16_t fc = ppdu_info->nac_info.frame_control;

	if (HAL_RX_GET_FRAME_CTRL_TYPE(fc) == HAL_RX_FRAME_CTRL_TYPE_CTRL) {
		if ((fc & QDF_IEEE80211_FC0_SUBTYPE_MASK) ==
		    QDF_IEEE80211_FC0_SUBTYPE_VHT_NDP_AN)
			ppdu_info->ctrl_frm_info[user_id].ndpa = 1;
		if ((fc & QDF_IEEE80211_FC0_SUBTYPE_MASK) ==
		    QDF_IEEE80211_FC0_SUBTYPE_BAR)
			ppdu_info->ctrl_frm_info[user_id].bar = 1;
	}
}
#else
static inline void
hal_update_rx_ctrl_frame_stats(struct hal_rx_ppdu_info *ppdu_info,
			       uint32_t user_id)
{
}
#endif /* WLAN_SUPPORT_CTRL_FRAME_STATS */

#ifdef MONITOR_TLV_RECORDING_ENABLE
/**
 * hal_rx_record_tlv_info() - Record received TLV info
 * @ppdu_info: pointer to ppdu_info
 * @tlv_tag: TLV tag of the TLV to record
 *
 * Return
 */
static inline void
hal_rx_record_tlv_info(struct hal_rx_ppdu_info *ppdu_info, uint32_t tlv_tag) {
	ppdu_info->rx_tlv_info.tlv_tag = tlv_tag;
	switch (tlv_tag) {
	case WIFIRX_PPDU_START_E:
	case WIFIRX_PPDU_START_USER_INFO_E:
		ppdu_info->rx_tlv_info.tlv_category = CATEGORY_PPDU_START;
		break;

	case WIFIRX_HEADER_E:
	case WIFIRX_MPDU_START_E:
	case WIFIMON_BUFFER_ADDR_E:
	case WIFIRX_MSDU_END_E:
	case WIFIRX_MPDU_END_E:
		ppdu_info->rx_tlv_info.tlv_category = CATEGORY_MPDU;
		break;

	case WIFIRX_USER_PPDU_END_E:
	case WIFIRX_PPDU_END_E:
	case WIFIPHYRX_RSSI_LEGACY_E:
	case WIFIPHYRX_L_SIG_B_E:
	case WIFIPHYRX_COMMON_USER_INFO_E:
	case WIFIPHYRX_DATA_DONE_E:
	case WIFIPHYRX_PKT_END_PART1_E:
	case WIFIPHYRX_PKT_END_E:
	case WIFIRXPCU_PPDU_END_INFO_E:
	case WIFIRX_PPDU_END_USER_STATS_E:
	case WIFIRX_PPDU_END_STATUS_DONE_E:
		ppdu_info->rx_tlv_info.tlv_category = CATEGORY_PPDU_END;
		break;
	}
}
#else
static inline void
hal_rx_record_tlv_info(struct hal_rx_ppdu_info *ppdu_info, uint32_t tlv_tag) {
}
#endif

#ifdef HE_SIG_A_MU_UL_INFO_FORMAT_INDICATION_OFFSET
/**
 * hal_rx_he_sig_a_mu_ul_e_handle() - handle TLV info for he_sig_a_mu_ul_info
 * @ppdu_info: pointer to ppdu_info
 * @rx_tlv: pointer to tlv
 *
 * Return
 */
static inline void
hal_rx_he_sig_a_mu_ul_e_handle(struct hal_rx_ppdu_info *ppdu_info,
			       uint8_t *rx_tlv) {
	uint32_t value;
	uint8_t *he_sig_a_mu_ul_info = (uint8_t *)rx_tlv;

	ppdu_info->rx_status.he_flags = 1;

	ppdu_info->rx_status.user_info_skip = 1;

	value = HAL_RX_GET(he_sig_a_mu_ul_info, HE_SIG_A_MU_UL_INFO,
			   FORMAT_INDICATION);
	if (value == 0) {
		ppdu_info->rx_status.he_data1 =
			QDF_MON_STATUS_HE_TRIG_FORMAT_TYPE;
	} else {
		ppdu_info->rx_status.he_data1 =
			 QDF_MON_STATUS_HE_SU_FORMAT_TYPE;
	}

	/* data1 */
	ppdu_info->rx_status.he_data1 |=
		QDF_MON_STATUS_HE_BSS_COLOR_KNOWN |
		QDF_MON_STATUS_HE_DATA_BW_RU_KNOWN;

	/* data2 */
	ppdu_info->rx_status.he_data2 |=
		QDF_MON_STATUS_TXOP_KNOWN;

	/* data3 */
	value = HAL_RX_GET(he_sig_a_mu_ul_info,
			   HE_SIG_A_MU_UL_INFO, BSS_COLOR_ID);
	ppdu_info->rx_status.he_data3 = value;

	/* data4 */

	/* data5 */
	value = HAL_RX_GET(he_sig_a_mu_ul_info,
			   HE_SIG_A_MU_UL_INFO, TRANSMIT_BW);
	ppdu_info->rx_status.he_data5 = value;
	ppdu_info->rx_status.bw = value;

	/* data6 */
	value = HAL_RX_GET(he_sig_a_mu_ul_info, HE_SIG_A_MU_UL_INFO,
			   TXOP_DURATION);
	value = value << QDF_MON_STATUS_TXOP_SHIFT;
	ppdu_info->rx_status.he_data6 |= value;

	ppdu_info->rx_status.mu_dl_ul = HAL_RX_TYPE_UL;
	ppdu_info->rx_status.reception_type = HAL_RX_TYPE_MU_MIMO;
}
#else
static inline void
hal_rx_he_sig_a_mu_ul_e_handle(struct hal_rx_ppdu_info *ppdu_info,
			       uint8_t *rx_tlv) {
}
#endif

/**
 * hal_rx_status_get_tlv_info_generic_be() - process receive info TLV
 * @rx_tlv_hdr: pointer to TLV header
 * @ppduinfo: pointer to ppdu_info
 * @hal_soc_hdl: HAL version of the SOC pointer
 * @nbuf: Network buffer
 *
 * Return: HAL_TLV_STATUS_PPDU_NOT_DONE or HAL_TLV_STATUS_PPDU_DONE from tlv
 */
static inline uint32_t
hal_rx_status_get_tlv_info_generic_be(void *rx_tlv_hdr, void *ppduinfo,
				      hal_soc_handle_t hal_soc_hdl,
				      qdf_nbuf_t nbuf)
{
	struct hal_soc *hal = (struct hal_soc *)hal_soc_hdl;
	uint32_t tlv_tag, user_id, tlv_len, value;
	uint8_t group_id = 0;
	uint8_t he_dcm = 0;
	uint8_t he_stbc = 0;
	uint16_t he_gi = 0;
	uint16_t he_ltf = 0;
	void *rx_tlv;
	struct mon_rx_user_status *mon_rx_user_status;
	struct hal_rx_ppdu_info *ppdu_info =
			(struct hal_rx_ppdu_info *)ppduinfo;

	tlv_tag = HAL_RX_GET_USER_TLV32_TYPE(rx_tlv_hdr);
	user_id = HAL_RX_GET_USER_TLV32_USERID(rx_tlv_hdr);
	tlv_len = HAL_RX_GET_USER_TLV32_LEN(rx_tlv_hdr);

	rx_tlv = (uint8_t *)rx_tlv_hdr + HAL_RX_TLV_HDR_SIZE;

	ppdu_info->user_id = user_id;
	switch (tlv_tag) {
	case WIFIRX_PPDU_START_E:
	{
		if (qdf_unlikely(ppdu_info->com_info.last_ppdu_id ==
		    HAL_RX_GET(rx_tlv, HAL_RX_PPDU_START, PHY_PPDU_ID)))
			hal_err("Matching ppdu_id(%u) detected",
				ppdu_info->com_info.last_ppdu_id);

		ppdu_info->com_info.last_ppdu_id =
			ppdu_info->com_info.ppdu_id =
				HAL_RX_GET(rx_tlv, HAL_RX_PPDU_START,
					   PHY_PPDU_ID);

		/* channel number is set in PHY meta data */
		ppdu_info->rx_status.chan_num =
			(HAL_RX_GET(rx_tlv, HAL_RX_PPDU_START,
				       SW_PHY_META_DATA) & 0x0000FFFF);
		ppdu_info->rx_status.chan_freq =
			(HAL_RX_GET(rx_tlv, HAL_RX_PPDU_START,
				       SW_PHY_META_DATA) & 0xFFFF0000) >> 16;
		if (ppdu_info->rx_status.chan_num &&
		    ppdu_info->rx_status.chan_freq) {
			ppdu_info->rx_status.chan_freq =
				hal_rx_radiotap_num_to_freq(
				ppdu_info->rx_status.chan_num,
				ppdu_info->rx_status.chan_freq);
		}

		ppdu_info->com_info.ppdu_timestamp =
			HAL_RX_GET(rx_tlv, HAL_RX_PPDU_START,
				   PPDU_START_TIMESTAMP_31_0);
		ppdu_info->rx_status.ppdu_timestamp =
			ppdu_info->com_info.ppdu_timestamp;
		ppdu_info->rx_state = HAL_RX_MON_PPDU_START;

		break;
	}

	case WIFIRX_PPDU_START_USER_INFO_E:
		hal_rx_parse_receive_user_info(hal, rx_tlv, ppdu_info, user_id);
		break;

	case WIFIRX_PPDU_END_E:
		/* This is followed by sub-TLVs of PPDU_END */
		ppdu_info->rx_state = HAL_RX_MON_PPDU_END;
		break;

	case WIFIPHYRX_LOCATION_E:
		hal_rx_get_rtt_info(hal_soc_hdl, rx_tlv, ppdu_info);
		break;

	case WIFIRXPCU_PPDU_END_INFO_E:
		ppdu_info->rx_status.rx_antenna =
			HAL_RX_GET(rx_tlv, HAL_RXPCU_PPDU_END_INFO, RX_ANTENNA);
		ppdu_info->rx_status.tsft =
			HAL_RX_GET(rx_tlv, HAL_RXPCU_PPDU_END_INFO,
				   WB_TIMESTAMP_UPPER_32);
		ppdu_info->rx_status.tsft = (ppdu_info->rx_status.tsft << 32) |
			HAL_RX_GET(rx_tlv, HAL_RXPCU_PPDU_END_INFO,
				   WB_TIMESTAMP_LOWER_32);
		ppdu_info->rx_status.duration =
			HAL_RX_GET(rx_tlv, UNIFIED_RXPCU_PPDU_END_INFO_8,
				   RX_PPDU_DURATION);
		hal_rx_get_bb_info(hal_soc_hdl, rx_tlv, ppdu_info);
		break;

	/*
	 * WIFIRX_PPDU_END_USER_STATS_E comes for each user received.
	 * for MU, based on num users we see this tlv that many times.
	 */
	case WIFIRX_PPDU_END_USER_STATS_E:
	{
		hal_rx_mon_ppdu_end_user_t *rx_ppdu_end_user = rx_tlv;
		unsigned long tid = 0;
		uint16_t seq = 0;

		ppdu_info->rx_status.ast_index =
				rx_ppdu_end_user->ast_index;

		tid = rx_ppdu_end_user->received_qos_data_tid_bitmap;
		ppdu_info->rx_status.tid = qdf_find_first_bit(&tid,
							      sizeof(tid) * 8);

		if (ppdu_info->rx_status.tid == (sizeof(tid) * 8))
			ppdu_info->rx_status.tid = HAL_TID_INVALID;

		ppdu_info->rx_status.tcp_msdu_count =
			rx_ppdu_end_user->tcp_msdu_count +
			rx_ppdu_end_user->tcp_ack_msdu_count;

		ppdu_info->rx_status.udp_msdu_count =
			rx_ppdu_end_user->udp_msdu_count;

		ppdu_info->rx_status.other_msdu_count =
			rx_ppdu_end_user->other_msdu_count;

		hal_rx_status_get_mpdu_retry_cnt(ppdu_info, rx_ppdu_end_user);

		if (ppdu_info->sw_frame_group_id
		    != HAL_MPDU_SW_FRAME_GROUP_NULL_DATA) {
			ppdu_info->rx_status.frame_control_info_valid =
				rx_ppdu_end_user->frame_control_info_valid;

			if (ppdu_info->rx_status.frame_control_info_valid)
				ppdu_info->rx_status.frame_control =
					rx_ppdu_end_user->frame_control_field;

			hal_get_qos_control(rx_ppdu_end_user, ppdu_info);
		}

		ppdu_info->rx_status.data_sequence_control_info_valid =
			rx_ppdu_end_user->data_sequence_control_info_valid;

		seq = rx_ppdu_end_user->first_data_seq_ctrl;

		if (ppdu_info->rx_status.data_sequence_control_info_valid)
			ppdu_info->rx_status.first_data_seq_ctrl = seq;

		ppdu_info->rx_status.preamble_type =
			rx_ppdu_end_user->ht_control_field_pkt_type;

		ppdu_info->end_user_stats_cnt++;

		switch (ppdu_info->rx_status.preamble_type) {
		case HAL_RX_PKT_TYPE_11N:
			ppdu_info->rx_status.ht_flags = 1;
			ppdu_info->rx_status.rtap_flags |= HT_SGI_PRESENT;
			break;
		case HAL_RX_PKT_TYPE_11AC:
			ppdu_info->rx_status.vht_flags = 1;
			break;
		case HAL_RX_PKT_TYPE_11AX:
			ppdu_info->rx_status.he_flags = 1;
			break;
		case HAL_RX_PKT_TYPE_11BE:
			ppdu_info->rx_status.eht_flags = 1;
			break;
		default:
			break;
		}

		ppdu_info->com_info.mpdu_cnt_fcs_ok =
			rx_ppdu_end_user->mpdu_cnt_fcs_ok;
		ppdu_info->com_info.mpdu_cnt_fcs_err =
			rx_ppdu_end_user->mpdu_cnt_fcs_err;
		if ((ppdu_info->com_info.mpdu_cnt_fcs_ok |
			ppdu_info->com_info.mpdu_cnt_fcs_err) > 1)
			ppdu_info->rx_status.rs_flags |= IEEE80211_AMPDU_FLAG;
		else
			ppdu_info->rx_status.rs_flags &=
				(~IEEE80211_AMPDU_FLAG);

		ppdu_info->com_info.mpdu_fcs_ok_bitmap[0] =
				rx_ppdu_end_user->fcs_ok_bitmap_31_0;

		ppdu_info->com_info.mpdu_fcs_ok_bitmap[1] =
				rx_ppdu_end_user->fcs_ok_bitmap_63_32;

		if (user_id < HAL_MAX_UL_MU_USERS) {
			mon_rx_user_status =
				&ppdu_info->rx_user_status[user_id];

			hal_rx_handle_mu_ul_info(rx_ppdu_end_user,
						 mon_rx_user_status);

			ppdu_info->com_info.num_users++;

			hal_rx_populate_mu_user_info(rx_ppdu_end_user, ppdu_info,
						     user_id,
						     mon_rx_user_status);
		}
		break;
	}

	case WIFIRX_PPDU_END_USER_STATS_EXT_E:
		ppdu_info->com_info.mpdu_fcs_ok_bitmap[2] =
			HAL_RX_GET(rx_tlv, HAL_RX_PPDU_END_USER_STATS_EXT,
				   FCS_OK_BITMAP_95_64);

		ppdu_info->com_info.mpdu_fcs_ok_bitmap[3] =
			 HAL_RX_GET(rx_tlv, HAL_RX_PPDU_END_USER_STATS_EXT,
				    FCS_OK_BITMAP_127_96);

		ppdu_info->com_info.mpdu_fcs_ok_bitmap[4] =
			HAL_RX_GET(rx_tlv, HAL_RX_PPDU_END_USER_STATS_EXT,
				   FCS_OK_BITMAP_159_128);

		ppdu_info->com_info.mpdu_fcs_ok_bitmap[5] =
			 HAL_RX_GET(rx_tlv, HAL_RX_PPDU_END_USER_STATS_EXT,
				    FCS_OK_BITMAP_191_160);

		ppdu_info->com_info.mpdu_fcs_ok_bitmap[6] =
			HAL_RX_GET(rx_tlv, HAL_RX_PPDU_END_USER_STATS_EXT,
				   FCS_OK_BITMAP_223_192);

		ppdu_info->com_info.mpdu_fcs_ok_bitmap[7] =
			 HAL_RX_GET(rx_tlv, HAL_RX_PPDU_END_USER_STATS_EXT,
				    FCS_OK_BITMAP_255_224);
		break;

	case WIFIRX_PPDU_END_STATUS_DONE_E:
		hal_rx_record_tlv_info(ppdu_info, tlv_tag);
		return HAL_TLV_STATUS_PPDU_DONE;

	case WIFIPHYRX_PKT_END_E:
		break;

	case WIFIDUMMY_E:
		hal_rx_record_tlv_info(ppdu_info, tlv_tag);
		return HAL_TLV_STATUS_BUF_DONE;

	case WIFIPHYRX_HT_SIG_E:
	{
		uint8_t *ht_sig_info = (uint8_t *)rx_tlv +
				HAL_RX_OFFSET(UNIFIED_PHYRX_HT_SIG_0,
					      HT_SIG_INFO_PHYRX_HT_SIG_INFO_DETAILS);
		value = HAL_RX_GET(ht_sig_info, HT_SIG_INFO, FEC_CODING);
		ppdu_info->rx_status.ldpc = (value == HAL_SU_MU_CODING_LDPC) ?
			1 : 0;
		ppdu_info->rx_status.mcs = HAL_RX_GET(ht_sig_info,
						      HT_SIG_INFO, MCS);
		ppdu_info->rx_status.ht_mcs = ppdu_info->rx_status.mcs;
		ppdu_info->rx_status.bw = HAL_RX_GET(ht_sig_info,
						     HT_SIG_INFO, CBW);
		ppdu_info->rx_status.sgi = HAL_RX_GET(ht_sig_info,
						      HT_SIG_INFO, SHORT_GI);
		ppdu_info->rx_status.reception_type = HAL_RX_TYPE_SU;
		ppdu_info->rx_status.nss = ((ppdu_info->rx_status.mcs) >>
				HT_SIG_SU_NSS_SHIFT) + 1;
		ppdu_info->rx_status.mcs &= ((1 << HT_SIG_SU_NSS_SHIFT) - 1);
		break;
	}

	case WIFIPHYRX_L_SIG_B_E:
	{
		uint8_t *l_sig_b_info = (uint8_t *)rx_tlv +
				HAL_RX_OFFSET(UNIFIED_PHYRX_L_SIG_B_0,
					      L_SIG_B_INFO_PHYRX_L_SIG_B_INFO_DETAILS);

		value = HAL_RX_GET(l_sig_b_info, L_SIG_B_INFO, RATE);
		ppdu_info->rx_status.l_sig_b_info = *((uint32_t *)l_sig_b_info);
		switch (value) {
		case 1:
			ppdu_info->rx_status.rate = HAL_11B_RATE_3MCS;
			ppdu_info->rx_status.mcs = HAL_LEGACY_MCS3;
			break;
		case 2:
			ppdu_info->rx_status.rate = HAL_11B_RATE_2MCS;
			ppdu_info->rx_status.mcs = HAL_LEGACY_MCS2;
			break;
		case 3:
			ppdu_info->rx_status.rate = HAL_11B_RATE_1MCS;
			ppdu_info->rx_status.mcs = HAL_LEGACY_MCS1;
			break;
		case 4:
			ppdu_info->rx_status.rate = HAL_11B_RATE_0MCS;
			ppdu_info->rx_status.mcs = HAL_LEGACY_MCS0;
			break;
		case 5:
			ppdu_info->rx_status.rate = HAL_11B_RATE_6MCS;
			ppdu_info->rx_status.mcs = HAL_LEGACY_MCS6;
			break;
		case 6:
			ppdu_info->rx_status.rate = HAL_11B_RATE_5MCS;
			ppdu_info->rx_status.mcs = HAL_LEGACY_MCS5;
			break;
		case 7:
			ppdu_info->rx_status.rate = HAL_11B_RATE_4MCS;
			ppdu_info->rx_status.mcs = HAL_LEGACY_MCS4;
			break;
		default:
			break;
		}
		ppdu_info->rx_status.cck_flag = 1;
	break;
	}

	case WIFIPHYRX_L_SIG_A_E:
	{
		uint8_t *l_sig_a_info = (uint8_t *)rx_tlv +
				HAL_RX_OFFSET(UNIFIED_PHYRX_L_SIG_A_0,
					      L_SIG_A_INFO_PHYRX_L_SIG_A_INFO_DETAILS);

		value = HAL_RX_GET(l_sig_a_info, L_SIG_A_INFO, RATE);
		ppdu_info->rx_status.l_sig_a_info = *((uint32_t *)l_sig_a_info);
		switch (value) {
		case 8:
			ppdu_info->rx_status.rate = HAL_11A_RATE_0MCS;
			ppdu_info->rx_status.mcs = HAL_LEGACY_MCS0;
			break;
		case 9:
			ppdu_info->rx_status.rate = HAL_11A_RATE_1MCS;
			ppdu_info->rx_status.mcs = HAL_LEGACY_MCS1;
			break;
		case 10:
			ppdu_info->rx_status.rate = HAL_11A_RATE_2MCS;
			ppdu_info->rx_status.mcs = HAL_LEGACY_MCS2;
			break;
		case 11:
			ppdu_info->rx_status.rate = HAL_11A_RATE_3MCS;
			ppdu_info->rx_status.mcs = HAL_LEGACY_MCS3;
			break;
		case 12:
			ppdu_info->rx_status.rate = HAL_11A_RATE_4MCS;
			ppdu_info->rx_status.mcs = HAL_LEGACY_MCS4;
			break;
		case 13:
			ppdu_info->rx_status.rate = HAL_11A_RATE_5MCS;
			ppdu_info->rx_status.mcs = HAL_LEGACY_MCS5;
			break;
		case 14:
			ppdu_info->rx_status.rate = HAL_11A_RATE_6MCS;
			ppdu_info->rx_status.mcs = HAL_LEGACY_MCS6;
			break;
		case 15:
			ppdu_info->rx_status.rate = HAL_11A_RATE_7MCS;
			ppdu_info->rx_status.mcs = HAL_LEGACY_MCS7;
			break;
		default:
			break;
		}
		ppdu_info->rx_status.ofdm_flag = 1;
	break;
	}

	case WIFIPHYRX_VHT_SIG_A_E:
	{
		uint8_t *vht_sig_a_info = (uint8_t *)rx_tlv +
				HAL_RX_OFFSET(UNIFIED_PHYRX_VHT_SIG_A_0,
					      VHT_SIG_A_INFO_PHYRX_VHT_SIG_A_INFO_DETAILS);

		value = HAL_RX_GET(vht_sig_a_info, VHT_SIG_A_INFO,
				   SU_MU_CODING);
		ppdu_info->rx_status.ldpc = (value == HAL_SU_MU_CODING_LDPC) ?
			1 : 0;
		group_id = HAL_RX_GET(vht_sig_a_info, VHT_SIG_A_INFO, GROUP_ID);
		ppdu_info->rx_status.vht_flag_values5 = group_id;
		ppdu_info->rx_status.mcs = HAL_RX_GET(vht_sig_a_info,
						      VHT_SIG_A_INFO, MCS);
		ppdu_info->rx_status.sgi = HAL_RX_GET(vht_sig_a_info,
						      VHT_SIG_A_INFO,
						      GI_SETTING);

		switch (hal->target_type) {
		case TARGET_TYPE_QCA8074:
		case TARGET_TYPE_QCA8074V2:
		case TARGET_TYPE_QCA6018:
		case TARGET_TYPE_QCA5018:
		case TARGET_TYPE_QCN9000:
		case TARGET_TYPE_QCN6122:
		case TARGET_TYPE_QCN6432:
#ifdef QCA_WIFI_QCA6390
		case TARGET_TYPE_QCA6390:
#endif
			ppdu_info->rx_status.is_stbc =
				HAL_RX_GET(vht_sig_a_info,
					   VHT_SIG_A_INFO, STBC);
			value =  HAL_RX_GET(vht_sig_a_info,
					    VHT_SIG_A_INFO, N_STS);
			value = value & VHT_SIG_SU_NSS_MASK;
			if (ppdu_info->rx_status.is_stbc && (value > 0))
				value = ((value + 1) >> 1) - 1;
			ppdu_info->rx_status.nss =
				((value & VHT_SIG_SU_NSS_MASK) + 1);

			break;
		case TARGET_TYPE_QCA6290:
#if !defined(QCA_WIFI_QCA6290_11AX)
			ppdu_info->rx_status.is_stbc =
				HAL_RX_GET(vht_sig_a_info,
					   VHT_SIG_A_INFO, STBC);
			value =  HAL_RX_GET(vht_sig_a_info,
					    VHT_SIG_A_INFO, N_STS);
			value = value & VHT_SIG_SU_NSS_MASK;
			if (ppdu_info->rx_status.is_stbc && (value > 0))
				value = ((value + 1) >> 1) - 1;
			ppdu_info->rx_status.nss =
				((value & VHT_SIG_SU_NSS_MASK) + 1);
#else
			ppdu_info->rx_status.nss = 0;
#endif
			break;
		case TARGET_TYPE_KIWI:
		case TARGET_TYPE_MANGO:
		case TARGET_TYPE_PEACH:
			ppdu_info->rx_status.is_stbc =
				HAL_RX_GET(vht_sig_a_info,
					   VHT_SIG_A_INFO, STBC);
			value =  HAL_RX_GET(vht_sig_a_info,
					    VHT_SIG_A_INFO, N_STS);
			value = value & VHT_SIG_SU_NSS_MASK;
			if (ppdu_info->rx_status.is_stbc && (value > 0))
				value = ((value + 1) >> 1) - 1;
			ppdu_info->rx_status.nss =
				((value & VHT_SIG_SU_NSS_MASK) + 1);

			break;
		case TARGET_TYPE_QCA6490:
		case TARGET_TYPE_QCA6750:
			ppdu_info->rx_status.nss = 0;
			break;
		default:
			break;
		}
		ppdu_info->rx_status.vht_flag_values3[0] =
				(((ppdu_info->rx_status.mcs) << 4)
				| ppdu_info->rx_status.nss);
		ppdu_info->rx_status.bw = HAL_RX_GET(vht_sig_a_info,
						     VHT_SIG_A_INFO, BANDWIDTH);
		ppdu_info->rx_status.vht_flag_values2 =
			ppdu_info->rx_status.bw;
		ppdu_info->rx_status.vht_flag_values4 =
			HAL_RX_GET(vht_sig_a_info,
				   VHT_SIG_A_INFO, SU_MU_CODING);

		ppdu_info->rx_status.beamformed = HAL_RX_GET(vht_sig_a_info,
							     VHT_SIG_A_INFO,
							     BEAMFORMED);
		if (group_id == 0 || group_id == 63)
			ppdu_info->rx_status.reception_type = HAL_RX_TYPE_SU;
		else
			ppdu_info->rx_status.reception_type =
				HAL_RX_TYPE_MU_MIMO;

		break;
	}
	case WIFIPHYRX_HE_SIG_A_SU_E:
	{
		uint8_t *he_sig_a_su_info = (uint8_t *)rx_tlv +
			HAL_RX_OFFSET(UNIFIED_PHYRX_HE_SIG_A_SU_0,
				      HE_SIG_A_SU_INFO_PHYRX_HE_SIG_A_SU_INFO_DETAILS);
		ppdu_info->rx_status.he_flags = 1;
		value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO,
				   FORMAT_INDICATION);
		if (value == 0) {
			ppdu_info->rx_status.he_data1 =
				QDF_MON_STATUS_HE_TRIG_FORMAT_TYPE;
		} else {
			ppdu_info->rx_status.he_data1 =
				 QDF_MON_STATUS_HE_SU_FORMAT_TYPE;
		}

		/* data1 */
		ppdu_info->rx_status.he_data1 |=
			QDF_MON_STATUS_HE_BSS_COLOR_KNOWN |
			QDF_MON_STATUS_HE_BEAM_CHANGE_KNOWN |
			QDF_MON_STATUS_HE_DL_UL_KNOWN |
			QDF_MON_STATUS_HE_MCS_KNOWN |
			QDF_MON_STATUS_HE_DCM_KNOWN |
			QDF_MON_STATUS_HE_CODING_KNOWN |
			QDF_MON_STATUS_HE_LDPC_EXTRA_SYMBOL_KNOWN |
			QDF_MON_STATUS_HE_STBC_KNOWN |
			QDF_MON_STATUS_HE_DATA_BW_RU_KNOWN |
			QDF_MON_STATUS_HE_DOPPLER_KNOWN;

		/* data2 */
		ppdu_info->rx_status.he_data2 =
			QDF_MON_STATUS_HE_GI_KNOWN;
		ppdu_info->rx_status.he_data2 |=
			QDF_MON_STATUS_TXBF_KNOWN |
			QDF_MON_STATUS_PE_DISAMBIGUITY_KNOWN |
			QDF_MON_STATUS_TXOP_KNOWN |
			QDF_MON_STATUS_LTF_SYMBOLS_KNOWN |
			QDF_MON_STATUS_PRE_FEC_PADDING_KNOWN |
			QDF_MON_STATUS_MIDABLE_PERIODICITY_KNOWN;

		/* data3 */
		value = HAL_RX_GET(he_sig_a_su_info,
				   HE_SIG_A_SU_INFO, BSS_COLOR_ID);
		ppdu_info->rx_status.he_data3 = value;
		value = HAL_RX_GET(he_sig_a_su_info,
				   HE_SIG_A_SU_INFO, BEAM_CHANGE);
		value = value << QDF_MON_STATUS_BEAM_CHANGE_SHIFT;
		ppdu_info->rx_status.he_data3 |= value;
		value = HAL_RX_GET(he_sig_a_su_info,
				   HE_SIG_A_SU_INFO, DL_UL_FLAG);
		ppdu_info->rx_status.mu_dl_ul = HAL_RX_TYPE_DL;

		value = value << QDF_MON_STATUS_DL_UL_SHIFT;
		ppdu_info->rx_status.he_data3 |= value;

		value = HAL_RX_GET(he_sig_a_su_info,
				   HE_SIG_A_SU_INFO, TRANSMIT_MCS);
		ppdu_info->rx_status.mcs = value;
		value = value << QDF_MON_STATUS_TRANSMIT_MCS_SHIFT;
		ppdu_info->rx_status.he_data3 |= value;

		value = HAL_RX_GET(he_sig_a_su_info,
				   HE_SIG_A_SU_INFO, DCM);
		he_dcm = value;
		value = value << QDF_MON_STATUS_DCM_SHIFT;
		ppdu_info->rx_status.he_data3 |= value;
		value = HAL_RX_GET(he_sig_a_su_info,
				   HE_SIG_A_SU_INFO, CODING);
		ppdu_info->rx_status.ldpc = (value == HAL_SU_MU_CODING_LDPC) ?
			1 : 0;
		value = value << QDF_MON_STATUS_CODING_SHIFT;
		ppdu_info->rx_status.he_data3 |= value;
		value = HAL_RX_GET(he_sig_a_su_info,
				   HE_SIG_A_SU_INFO,
				   LDPC_EXTRA_SYMBOL);
		value = value << QDF_MON_STATUS_LDPC_EXTRA_SYMBOL_SHIFT;
		ppdu_info->rx_status.he_data3 |= value;
		value = HAL_RX_GET(he_sig_a_su_info,
				   HE_SIG_A_SU_INFO, STBC);
		he_stbc = value;
		value = value << QDF_MON_STATUS_STBC_SHIFT;
		ppdu_info->rx_status.he_data3 |= value;

		/* data4 */
		value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO,
				   SPATIAL_REUSE);
		ppdu_info->rx_status.he_data4 = value;

		/* data5 */
		value = HAL_RX_GET(he_sig_a_su_info,
				   HE_SIG_A_SU_INFO, TRANSMIT_BW);
		ppdu_info->rx_status.he_data5 = value;
		ppdu_info->rx_status.bw = value;
		value = HAL_RX_GET(he_sig_a_su_info,
				   HE_SIG_A_SU_INFO, CP_LTF_SIZE);
		switch (value) {
		case 0:
				he_gi = HE_GI_0_8;
				he_ltf = HE_LTF_1_X;
				break;
		case 1:
				he_gi = HE_GI_0_8;
				he_ltf = HE_LTF_2_X;
				break;
		case 2:
				he_gi = HE_GI_1_6;
				he_ltf = HE_LTF_2_X;
				break;
		case 3:
				if (he_dcm && he_stbc) {
					he_gi = HE_GI_0_8;
					he_ltf = HE_LTF_4_X;
				} else {
					he_gi = HE_GI_3_2;
					he_ltf = HE_LTF_4_X;
				}
				break;
		}
		ppdu_info->rx_status.sgi = he_gi;
		ppdu_info->rx_status.ltf_size = he_ltf;
		hal_get_radiotap_he_gi_ltf(&he_gi, &he_ltf);
		value = he_gi << QDF_MON_STATUS_GI_SHIFT;
		ppdu_info->rx_status.he_data5 |= value;
		value = he_ltf << QDF_MON_STATUS_HE_LTF_SIZE_SHIFT;
		ppdu_info->rx_status.he_data5 |= value;

		value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO, NSTS);
		value = (value << QDF_MON_STATUS_HE_LTF_SYM_SHIFT);
		ppdu_info->rx_status.he_data5 |= value;

		value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO,
				   PACKET_EXTENSION_A_FACTOR);
		value = value << QDF_MON_STATUS_PRE_FEC_PAD_SHIFT;
		ppdu_info->rx_status.he_data5 |= value;

		value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO, TXBF);
		value = value << QDF_MON_STATUS_TXBF_SHIFT;
		ppdu_info->rx_status.he_data5 |= value;
		value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO,
				   PACKET_EXTENSION_PE_DISAMBIGUITY);
		value = value << QDF_MON_STATUS_PE_DISAMBIGUITY_SHIFT;
		ppdu_info->rx_status.he_data5 |= value;

		/* data6 */
		value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO, NSTS);
		value++;
		ppdu_info->rx_status.nss = value;
		ppdu_info->rx_status.he_data6 = value;
		value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO,
				   DOPPLER_INDICATION);
		value = value << QDF_MON_STATUS_DOPPLER_SHIFT;
		ppdu_info->rx_status.he_data6 |= value;
		value = HAL_RX_GET(he_sig_a_su_info, HE_SIG_A_SU_INFO,
				   TXOP_DURATION);
		value = value << QDF_MON_STATUS_TXOP_SHIFT;
		ppdu_info->rx_status.he_data6 |= value;

		ppdu_info->rx_status.beamformed = HAL_RX_GET(he_sig_a_su_info,
							     HE_SIG_A_SU_INFO,
							     TXBF);
		ppdu_info->rx_status.reception_type = HAL_RX_TYPE_SU;
		break;
	}
	case WIFIPHYRX_HE_SIG_A_MU_DL_E:
	{
		uint8_t *he_sig_a_mu_dl_info = (uint8_t *)rx_tlv +
			HAL_RX_OFFSET(UNIFIED_PHYRX_HE_SIG_A_MU_DL_0,
				      HE_SIG_A_MU_DL_INFO_PHYRX_HE_SIG_A_MU_DL_INFO_DETAILS);

		ppdu_info->rx_status.he_mu_flags = 1;

		/* HE Flags */
		/*data1*/
		ppdu_info->rx_status.he_data1 =
					QDF_MON_STATUS_HE_MU_FORMAT_TYPE;
		ppdu_info->rx_status.he_data1 |=
			QDF_MON_STATUS_HE_BSS_COLOR_KNOWN |
			QDF_MON_STATUS_HE_DL_UL_KNOWN |
			QDF_MON_STATUS_HE_LDPC_EXTRA_SYMBOL_KNOWN |
			QDF_MON_STATUS_HE_STBC_KNOWN |
			QDF_MON_STATUS_HE_DATA_BW_RU_KNOWN |
			QDF_MON_STATUS_HE_DOPPLER_KNOWN;

		/* data2 */
		ppdu_info->rx_status.he_data2 =
			QDF_MON_STATUS_HE_GI_KNOWN;
		ppdu_info->rx_status.he_data2 |=
			QDF_MON_STATUS_LTF_SYMBOLS_KNOWN |
			QDF_MON_STATUS_PRE_FEC_PADDING_KNOWN |
			QDF_MON_STATUS_PE_DISAMBIGUITY_KNOWN |
			QDF_MON_STATUS_TXOP_KNOWN |
			QDF_MON_STATUS_MIDABLE_PERIODICITY_KNOWN;

		/*data3*/
		value = HAL_RX_GET(he_sig_a_mu_dl_info,
				   HE_SIG_A_MU_DL_INFO, BSS_COLOR_ID);
		ppdu_info->rx_status.he_data3 = value;

		value = HAL_RX_GET(he_sig_a_mu_dl_info,
				   HE_SIG_A_MU_DL_INFO, DL_UL_FLAG);
		value = value << QDF_MON_STATUS_DL_UL_SHIFT;
		ppdu_info->rx_status.he_data3 |= value;

		value = HAL_RX_GET(he_sig_a_mu_dl_info,
				   HE_SIG_A_MU_DL_INFO,
				   LDPC_EXTRA_SYMBOL);
		value = value << QDF_MON_STATUS_LDPC_EXTRA_SYMBOL_SHIFT;
		ppdu_info->rx_status.he_data3 |= value;

		value = HAL_RX_GET(he_sig_a_mu_dl_info,
				   HE_SIG_A_MU_DL_INFO, STBC);
		he_stbc = value;
		value = value << QDF_MON_STATUS_STBC_SHIFT;
		ppdu_info->rx_status.he_data3 |= value;

		/*data4*/
		value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO,
				   SPATIAL_REUSE);
		ppdu_info->rx_status.he_data4 = value;

		/*data5*/
		value = HAL_RX_GET(he_sig_a_mu_dl_info,
				   HE_SIG_A_MU_DL_INFO, TRANSMIT_BW);
		ppdu_info->rx_status.he_data5 = value;
		ppdu_info->rx_status.bw = value;

		value = HAL_RX_GET(he_sig_a_mu_dl_info,
				   HE_SIG_A_MU_DL_INFO, CP_LTF_SIZE);
		switch (value) {
		case 0:
			he_gi = HE_GI_0_8;
			he_ltf = HE_LTF_4_X;
			break;
		case 1:
			he_gi = HE_GI_0_8;
			he_ltf = HE_LTF_2_X;
			break;
		case 2:
			he_gi = HE_GI_1_6;
			he_ltf = HE_LTF_2_X;
			break;
		case 3:
			he_gi = HE_GI_3_2;
			he_ltf = HE_LTF_4_X;
			break;
		}
		ppdu_info->rx_status.sgi = he_gi;
		ppdu_info->rx_status.ltf_size = he_ltf;
		hal_get_radiotap_he_gi_ltf(&he_gi, &he_ltf);
		value = he_gi << QDF_MON_STATUS_GI_SHIFT;
		ppdu_info->rx_status.he_data5 |= value;

		value = he_ltf << QDF_MON_STATUS_HE_LTF_SIZE_SHIFT;
		ppdu_info->rx_status.he_data5 |= value;

		value = HAL_RX_GET(he_sig_a_mu_dl_info,
				   HE_SIG_A_MU_DL_INFO, NUM_LTF_SYMBOLS);
		value = (value << QDF_MON_STATUS_HE_LTF_SYM_SHIFT);
		ppdu_info->rx_status.he_data5 |= value;

		value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO,
				   PACKET_EXTENSION_A_FACTOR);
		value = value << QDF_MON_STATUS_PRE_FEC_PAD_SHIFT;
		ppdu_info->rx_status.he_data5 |= value;

		value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO,
				   PACKET_EXTENSION_PE_DISAMBIGUITY);
		value = value << QDF_MON_STATUS_PE_DISAMBIGUITY_SHIFT;
		ppdu_info->rx_status.he_data5 |= value;

		/*data6*/
		value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO,
				   DOPPLER_INDICATION);
		value = value << QDF_MON_STATUS_DOPPLER_SHIFT;
		ppdu_info->rx_status.he_data6 |= value;

		value = HAL_RX_GET(he_sig_a_mu_dl_info, HE_SIG_A_MU_DL_INFO,
				   TXOP_DURATION);
		value = value << QDF_MON_STATUS_TXOP_SHIFT;
		ppdu_info->rx_status.he_data6 |= value;

		/* HE-MU Flags */
		/* HE-MU-flags1 */
		ppdu_info->rx_status.he_flags1 =
			QDF_MON_STATUS_SIG_B_MCS_KNOWN |
			QDF_MON_STATUS_SIG_B_DCM_KNOWN |
			QDF_MON_STATUS_SIG_B_COMPRESSION_FLAG_1_KNOWN |
			QDF_MON_STATUS_SIG_B_SYM_NUM_KNOWN |
			QDF_MON_STATUS_RU_0_KNOWN;

		value = HAL_RX_GET(he_sig_a_mu_dl_info,
				   HE_SIG_A_MU_DL_INFO, MCS_OF_SIG_B);
		ppdu_info->rx_status.he_flags1 |= value;
		value = HAL_RX_GET(he_sig_a_mu_dl_info,
				   HE_SIG_A_MU_DL_INFO, DCM_OF_SIG_B);
		value = value << QDF_MON_STATUS_DCM_FLAG_1_SHIFT;
		ppdu_info->rx_status.he_flags1 |= value;

		/* HE-MU-flags2 */
		ppdu_info->rx_status.he_flags2 =
			QDF_MON_STATUS_BW_KNOWN;

		value = HAL_RX_GET(he_sig_a_mu_dl_info,
				   HE_SIG_A_MU_DL_INFO, TRANSMIT_BW);
		ppdu_info->rx_status.he_flags2 |= value;
		value = HAL_RX_GET(he_sig_a_mu_dl_info,
				   HE_SIG_A_MU_DL_INFO, COMP_MODE_SIG_B);
		value = value << QDF_MON_STATUS_SIG_B_COMPRESSION_FLAG_2_SHIFT;
		ppdu_info->rx_status.he_flags2 |= value;
		value = HAL_RX_GET(he_sig_a_mu_dl_info,
				   HE_SIG_A_MU_DL_INFO, NUM_SIG_B_SYMBOLS);
		value = value - 1;
		value = value << QDF_MON_STATUS_NUM_SIG_B_SYMBOLS_SHIFT;
		ppdu_info->rx_status.he_flags2 |= value;
		ppdu_info->rx_status.reception_type = HAL_RX_TYPE_MU_MIMO;
		break;
	}
	case WIFIPHYRX_HE_SIG_A_MU_UL_E:
	{
		hal_rx_he_sig_a_mu_ul_e_handle(ppdu_info, rx_tlv);
		break;
	}
	case WIFIPHYRX_HE_SIG_B1_MU_E:
	{
		uint8_t *he_sig_b1_mu_info = (uint8_t *)rx_tlv +
			HAL_RX_OFFSET(UNIFIED_PHYRX_HE_SIG_B1_MU_0,
				      HE_SIG_B1_MU_INFO_PHYRX_HE_SIG_B1_MU_INFO_DETAILS);

		ppdu_info->rx_status.he_sig_b_common_known |=
			QDF_MON_STATUS_HE_SIG_B_COMMON_KNOWN_RU0;
		/* TODO: Check on the availability of other fields in
		 * sig_b_common
		 */

		value = HAL_RX_GET(he_sig_b1_mu_info,
				   HE_SIG_B1_MU_INFO, RU_ALLOCATION);
		ppdu_info->rx_status.he_RU[0] = value;
		ppdu_info->rx_status.reception_type = HAL_RX_TYPE_MU_MIMO;
		break;
	}
	case WIFIPHYRX_HE_SIG_B2_MU_E:
	{
		uint8_t *he_sig_b2_mu_info = (uint8_t *)rx_tlv +
			HAL_RX_OFFSET(UNIFIED_PHYRX_HE_SIG_B2_MU_0,
				      HE_SIG_B2_MU_INFO_PHYRX_HE_SIG_B2_MU_INFO_DETAILS);
		/*
		 * Not all "HE" fields can be updated from
		 * WIFIPHYRX_HE_SIG_A_MU_DL_E TLV. Use WIFIPHYRX_HE_SIG_B2_MU_E
		 * to populate rest of the "HE" fields for MU scenarios.
		 */

		/* HE-data1 */
		ppdu_info->rx_status.he_data1 |=
			QDF_MON_STATUS_HE_MCS_KNOWN |
			QDF_MON_STATUS_HE_CODING_KNOWN;

		/* HE-data2 */

		/* HE-data3 */
		value = HAL_RX_GET(he_sig_b2_mu_info,
				   HE_SIG_B2_MU_INFO, STA_MCS);
		ppdu_info->rx_status.mcs = value;
		value = value << QDF_MON_STATUS_TRANSMIT_MCS_SHIFT;
		ppdu_info->rx_status.he_data3 |= value;

		value = HAL_RX_GET(he_sig_b2_mu_info,
				   HE_SIG_B2_MU_INFO, STA_CODING);
		value = value << QDF_MON_STATUS_CODING_SHIFT;
		ppdu_info->rx_status.he_data3 |= value;

		/* HE-data4 */
		value = HAL_RX_GET(he_sig_b2_mu_info,
				   HE_SIG_B2_MU_INFO, STA_ID);
		value = value << QDF_MON_STATUS_STA_ID_SHIFT;
		ppdu_info->rx_status.he_data4 |= value;

		/* HE-data5 */

		/* HE-data6 */
		value = HAL_RX_GET(he_sig_b2_mu_info,
				   HE_SIG_B2_MU_INFO, NSTS);
		/* value n indicates n+1 spatial streams */
		value++;
		ppdu_info->rx_status.nss = value;
		ppdu_info->rx_status.he_data6 |= value;

		break;
	}
	case WIFIPHYRX_HE_SIG_B2_OFDMA_E:
	{
		uint8_t *he_sig_b2_ofdma_info =
		(uint8_t *)rx_tlv +
		HAL_RX_OFFSET(UNIFIED_PHYRX_HE_SIG_B2_OFDMA_0,
			      HE_SIG_B2_OFDMA_INFO_PHYRX_HE_SIG_B2_OFDMA_INFO_DETAILS);

		/*
		 * Not all "HE" fields can be updated from
		 * WIFIPHYRX_HE_SIG_A_MU_DL_E TLV. Use WIFIPHYRX_HE_SIG_B2_MU_E
		 * to populate rest of "HE" fields for MU OFDMA scenarios.
		 */

		/* HE-data1 */
		ppdu_info->rx_status.he_data1 |=
			QDF_MON_STATUS_HE_MCS_KNOWN |
			QDF_MON_STATUS_HE_DCM_KNOWN |
			QDF_MON_STATUS_HE_CODING_KNOWN;

		/* HE-data2 */
		ppdu_info->rx_status.he_data2 |=
					QDF_MON_STATUS_TXBF_KNOWN;

		/* HE-data3 */
		value = HAL_RX_GET(he_sig_b2_ofdma_info,
				   HE_SIG_B2_OFDMA_INFO, STA_MCS);
		ppdu_info->rx_status.mcs = value;
		value = value << QDF_MON_STATUS_TRANSMIT_MCS_SHIFT;
		ppdu_info->rx_status.he_data3 |= value;

		value = HAL_RX_GET(he_sig_b2_ofdma_info,
				   HE_SIG_B2_OFDMA_INFO, STA_DCM);
		he_dcm = value;
		value = value << QDF_MON_STATUS_DCM_SHIFT;
		ppdu_info->rx_status.he_data3 |= value;

		value = HAL_RX_GET(he_sig_b2_ofdma_info,
				   HE_SIG_B2_OFDMA_INFO, STA_CODING);
		value = value << QDF_MON_STATUS_CODING_SHIFT;
		ppdu_info->rx_status.he_data3 |= value;

		/* HE-data4 */
		value = HAL_RX_GET(he_sig_b2_ofdma_info,
				   HE_SIG_B2_OFDMA_INFO, STA_ID);
		value = value << QDF_MON_STATUS_STA_ID_SHIFT;
		ppdu_info->rx_status.he_data4 |= value;

		/* HE-data5 */
		value = HAL_RX_GET(he_sig_b2_ofdma_info,
				   HE_SIG_B2_OFDMA_INFO, TXBF);
		value = value << QDF_MON_STATUS_TXBF_SHIFT;
		ppdu_info->rx_status.he_data5 |= value;

		/* HE-data6 */
		value = HAL_RX_GET(he_sig_b2_ofdma_info,
				   HE_SIG_B2_OFDMA_INFO, NSTS);
		/* value n indicates n+1 spatial streams */
		value++;
		ppdu_info->rx_status.nss = value;
		ppdu_info->rx_status.he_data6 |= value;
		ppdu_info->rx_status.reception_type = HAL_RX_TYPE_MU_OFDMA;
		break;
	}
	case WIFIPHYRX_RSSI_LEGACY_E:
	{
		uint8_t reception_type;
		int8_t rssi_value;
		uint8_t *rssi_info_tlv = (uint8_t *)rx_tlv +
			HAL_RX_OFFSET(UNIFIED_PHYRX_RSSI_LEGACY_19,
				      RECEIVE_RSSI_INFO_PREAMBLE_RSSI_INFO_DETAILS);

		ppdu_info->rx_status.rssi_comb =
				hal_rx_phy_legacy_get_rssi(hal_soc_hdl, rx_tlv);

		ppdu_info->rx_status.bw = hal->ops->hal_rx_get_tlv(rx_tlv);
		ppdu_info->rx_status.he_re = 0;

		reception_type = HAL_RX_GET(rx_tlv, HAL_PHYRX_RSSI_LEGACY,
					    RECEPTION_TYPE);
		switch (reception_type) {
		case QDF_RECEPTION_TYPE_ULOFMDA:
			ppdu_info->rx_status.ulofdma_flag = 1;
			ppdu_info->rx_status.he_data1 =
				QDF_MON_STATUS_HE_TRIG_FORMAT_TYPE;
			break;
		case QDF_RECEPTION_TYPE_ULMIMO:
			ppdu_info->rx_status.he_data1 =
				QDF_MON_STATUS_HE_MU_FORMAT_TYPE;
			break;
		default:
			break;
		}

		ppdu_info->rx_status.ul_mu_type = reception_type;

		hal_rx_update_rssi_chain(ppdu_info, rssi_info_tlv);
		rssi_value = HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO,
					RSSI_PRI20_CHAIN0);
		ppdu_info->rx_status.rssi[0] = rssi_value;

		rssi_value = HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO,
					RSSI_PRI20_CHAIN1);
		ppdu_info->rx_status.rssi[1] = rssi_value;

		rssi_value = HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO,
					RSSI_PRI20_CHAIN2);
		ppdu_info->rx_status.rssi[2] = rssi_value;

		rssi_value = HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO,
					RSSI_PRI20_CHAIN3);
		ppdu_info->rx_status.rssi[3] = rssi_value;

#ifdef DP_BE_NOTYET_WAR
		// TODO - this is not preset for kiwi
		rssi_value = HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO,
					RSSI_PRI20_CHAIN4);
		ppdu_info->rx_status.rssi[4] = rssi_value;

		rssi_value = HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO,
					RSSI_PRI20_CHAIN5);
		ppdu_info->rx_status.rssi[5] = rssi_value;

		rssi_value = HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO,
					RSSI_PRI20_CHAIN6);
		ppdu_info->rx_status.rssi[6] = rssi_value;

		rssi_value = HAL_RX_GET(rssi_info_tlv, RECEIVE_RSSI_INFO,
					RSSI_PRI20_CHAIN7);
		ppdu_info->rx_status.rssi[7] = rssi_value;
#endif
		break;
	}
	case WIFIPHYRX_OTHER_RECEIVE_INFO_E:
		hal_rx_proc_phyrx_other_receive_info_tlv(hal, rx_tlv_hdr,
							 ppdu_info);
		break;
	case WIFIPHYRX_GENERIC_U_SIG_E:
		hal_rx_parse_u_sig_hdr(hal, rx_tlv, ppdu_info);
		break;
	case WIFIPHYRX_COMMON_USER_INFO_E:
		hal_rx_parse_cmn_usr_info(hal, rx_tlv, ppdu_info);
		break;
	case WIFIRX_HEADER_E:
	{
		struct hal_rx_ppdu_common_info *com_info = &ppdu_info->com_info;

		if (ppdu_info->fcs_ok_cnt >=
		    HAL_RX_MAX_MPDU_H_PER_STATUS_BUFFER) {
			hal_err("Number of MPDUs(%d) per status buff exceeded",
				ppdu_info->fcs_ok_cnt);
			break;
		}

		/* Update first_msdu_payload for every mpdu and increment
		 * com_info->mpdu_cnt for every WIFIRX_HEADER_E TLV
		 */
		ppdu_info->ppdu_msdu_info[ppdu_info->fcs_ok_cnt].first_msdu_payload =
			rx_tlv;
		ppdu_info->ppdu_msdu_info[ppdu_info->fcs_ok_cnt].payload_len = tlv_len;
		ppdu_info->msdu_info.first_msdu_payload = rx_tlv;
		ppdu_info->msdu_info.payload_len = tlv_len;
		ppdu_info->user_id = user_id;
		ppdu_info->hdr_len = tlv_len;
		ppdu_info->data = rx_tlv;
		ppdu_info->data += 4;

		/* for every RX_HEADER TLV increment mpdu_cnt */
		com_info->mpdu_cnt++;
		hal_rx_record_tlv_info(ppdu_info, tlv_tag);
		return HAL_TLV_STATUS_HEADER;
	}
	case WIFIRX_MPDU_START_E:
	{
		hal_rx_mon_mpdu_start_t *rx_mpdu_start = rx_tlv;
		uint32_t ppdu_id = rx_mpdu_start->rx_mpdu_info_details.phy_ppdu_id;
		uint8_t filter_category = 0;

		ppdu_info->nac_info.fc_valid =
				rx_mpdu_start->rx_mpdu_info_details.mpdu_frame_control_valid;

		ppdu_info->nac_info.to_ds_flag =
				rx_mpdu_start->rx_mpdu_info_details.to_ds;

		ppdu_info->nac_info.frame_control =
			rx_mpdu_start->rx_mpdu_info_details.mpdu_frame_control_field;

		ppdu_info->sw_frame_group_id =
			rx_mpdu_start->rx_mpdu_info_details.sw_frame_group_id;

		ppdu_info->rx_user_status[user_id].sw_peer_id =
			rx_mpdu_start->rx_mpdu_info_details.sw_peer_id;

		hal_update_rx_ctrl_frame_stats(ppdu_info, user_id);

		if (ppdu_info->sw_frame_group_id ==
		    HAL_MPDU_SW_FRAME_GROUP_NULL_DATA) {
			ppdu_info->rx_status.frame_control_info_valid =
				ppdu_info->nac_info.fc_valid;
			ppdu_info->rx_status.frame_control =
				ppdu_info->nac_info.frame_control;
		}

		hal_get_mac_addr1(rx_mpdu_start,
				  ppdu_info);

		ppdu_info->nac_info.mac_addr2_valid =
				rx_mpdu_start->rx_mpdu_info_details.mac_addr_ad2_valid;

		*(uint16_t *)&ppdu_info->nac_info.mac_addr2[0] =
			 rx_mpdu_start->rx_mpdu_info_details.mac_addr_ad2_15_0;

		*(uint32_t *)&ppdu_info->nac_info.mac_addr2[2] =
			rx_mpdu_start->rx_mpdu_info_details.mac_addr_ad2_47_16;

		if (ppdu_info->rx_status.prev_ppdu_id != ppdu_id) {
			ppdu_info->rx_status.prev_ppdu_id = ppdu_id;
			ppdu_info->rx_status.ppdu_len =
				rx_mpdu_start->rx_mpdu_info_details.mpdu_length;
		} else {
			ppdu_info->rx_status.ppdu_len +=
				rx_mpdu_start->rx_mpdu_info_details.mpdu_length;
		}

		filter_category =
			rx_mpdu_start->rx_mpdu_info_details.rxpcu_mpdu_filter_in_category;

		if (filter_category == 0)
			ppdu_info->rx_status.rxpcu_filter_pass = 1;
		else if (filter_category == 1)
			ppdu_info->rx_status.monitor_direct_used = 1;

		ppdu_info->rx_user_status[user_id].filter_category = filter_category;

		ppdu_info->nac_info.mcast_bcast =
			rx_mpdu_start->rx_mpdu_info_details.mcast_bcast;
		ppdu_info->mpdu_info[user_id].decap_type =
			rx_mpdu_start->rx_mpdu_info_details.decap_type;

		hal_rx_record_tlv_info(ppdu_info, tlv_tag);
		return HAL_TLV_STATUS_MPDU_START;
	}
	case WIFIRX_MPDU_END_E:
		ppdu_info->user_id = user_id;
		ppdu_info->fcs_err =
			HAL_RX_GET(rx_tlv, HAL_RX_MPDU_END, FCS_ERR);

		ppdu_info->mpdu_info[user_id].fcs_err = ppdu_info->fcs_err;
		hal_rx_record_tlv_info(ppdu_info, tlv_tag);
		return HAL_TLV_STATUS_MPDU_END;
	case WIFIRX_MSDU_END_E: {
		hal_rx_mon_msdu_end_t *rx_msdu_end = rx_tlv;

		if (user_id < HAL_MAX_UL_MU_USERS) {
			ppdu_info->rx_msdu_info[user_id].cce_metadata =
				rx_msdu_end->cce_metadata;
			ppdu_info->rx_msdu_info[user_id].fse_metadata =
				rx_msdu_end->fse_metadata;
			ppdu_info->rx_msdu_info[user_id].is_flow_idx_timeout =
				rx_msdu_end->flow_idx_timeout;
			ppdu_info->rx_msdu_info[user_id].is_flow_idx_invalid =
				rx_msdu_end->flow_idx_invalid;
			ppdu_info->rx_msdu_info[user_id].flow_idx =
				rx_msdu_end->flow_idx;
			ppdu_info->msdu[user_id].first_msdu =
				rx_msdu_end->first_msdu;
			ppdu_info->msdu[user_id].last_msdu =
				rx_msdu_end->last_msdu;
			ppdu_info->msdu[user_id].msdu_len =
				rx_msdu_end->msdu_length;
			ppdu_info->msdu[user_id].user_rssi =
				rx_msdu_end->user_rssi;
			ppdu_info->msdu[user_id].reception_type =
				rx_msdu_end->reception_type;
		}
		hal_rx_record_tlv_info(ppdu_info, tlv_tag);
		return HAL_TLV_STATUS_MSDU_END;
		}
	case WIFIMON_BUFFER_ADDR_E:
		hal_rx_status_get_mon_buf_addr(rx_tlv, ppdu_info);
		hal_rx_record_tlv_info(ppdu_info, tlv_tag);
		return HAL_TLV_STATUS_MON_BUF_ADDR;
	case WIFIMON_DROP_E:
		hal_rx_update_ppdu_drop_cnt(rx_tlv, ppdu_info);
		hal_rx_record_tlv_info(ppdu_info, tlv_tag);
		return HAL_TLV_STATUS_MON_DROP;
	case 0:
		hal_rx_record_tlv_info(ppdu_info, tlv_tag);
		return HAL_TLV_STATUS_PPDU_DONE;
	case WIFIRX_STATUS_BUFFER_DONE_E:
	case WIFIPHYRX_DATA_DONE_E:
	case WIFIPHYRX_PKT_END_PART1_E:
		hal_rx_record_tlv_info(ppdu_info, tlv_tag);
		return HAL_TLV_STATUS_PPDU_NOT_DONE;

	default:
		hal_debug("unhandled tlv tag %d", tlv_tag);
	}

	hal_rx_record_tlv_info(ppdu_info, tlv_tag);
	return HAL_TLV_STATUS_PPDU_NOT_DONE;
}

static uint32_t
hal_rx_status_process_aggr_tlv(struct hal_soc *hal_soc,
			       struct hal_rx_ppdu_info *ppdu_info)
{
	uint32_t aggr_tlv_tag = ppdu_info->tlv_aggr.tlv_tag;

	switch (aggr_tlv_tag) {
	case WIFIPHYRX_GENERIC_EHT_SIG_E:
		hal_rx_parse_eht_sig_hdr(hal_soc, ppdu_info->tlv_aggr.buf,
					 ppdu_info);
		break;
	default:
		/* Aggregated TLV cannot be handled */
		qdf_assert(0);
		break;
	}

	ppdu_info->tlv_aggr.in_progress = 0;
	ppdu_info->tlv_aggr.cur_len = 0;

	return HAL_TLV_STATUS_PPDU_NOT_DONE;
}

static inline bool
hal_rx_status_tlv_should_aggregate(struct hal_soc *hal_soc, uint32_t tlv_tag)
{
	switch (tlv_tag) {
	case WIFIPHYRX_GENERIC_EHT_SIG_E:
		return true;
	}

	return false;
}

static inline uint32_t
hal_rx_status_aggr_tlv(struct hal_soc *hal_soc, void *rx_tlv_hdr,
		       struct hal_rx_ppdu_info *ppdu_info,
		       qdf_nbuf_t nbuf)
{
	uint32_t tlv_tag, user_id, tlv_len;
	void *rx_tlv;

	tlv_tag = HAL_RX_GET_USER_TLV32_TYPE(rx_tlv_hdr);
	user_id = HAL_RX_GET_USER_TLV32_USERID(rx_tlv_hdr);
	tlv_len = HAL_RX_GET_USER_TLV32_LEN(rx_tlv_hdr);

	rx_tlv = (uint8_t *)rx_tlv_hdr + HAL_RX_TLV_HDR_SIZE;

	if (tlv_len <= HAL_RX_MON_MAX_AGGR_SIZE - ppdu_info->tlv_aggr.cur_len) {
		qdf_mem_copy(ppdu_info->tlv_aggr.buf +
			     ppdu_info->tlv_aggr.cur_len,
			     rx_tlv, tlv_len);
		ppdu_info->tlv_aggr.cur_len += tlv_len;
	} else {
		dp_err("Length of TLV exceeds max aggregation length");
		qdf_assert(0);
	}

	return HAL_TLV_STATUS_PPDU_NOT_DONE;
}

static inline uint32_t
hal_rx_status_start_new_aggr_tlv(struct hal_soc *hal_soc, void *rx_tlv_hdr,
				 struct hal_rx_ppdu_info *ppdu_info,
				 qdf_nbuf_t nbuf)
{
	uint32_t tlv_tag, user_id, tlv_len;

	tlv_tag = HAL_RX_GET_USER_TLV32_TYPE(rx_tlv_hdr);
	user_id = HAL_RX_GET_USER_TLV32_USERID(rx_tlv_hdr);
	tlv_len = HAL_RX_GET_USER_TLV32_LEN(rx_tlv_hdr);

	ppdu_info->tlv_aggr.in_progress = 1;
	ppdu_info->tlv_aggr.tlv_tag = tlv_tag;
	ppdu_info->tlv_aggr.cur_len = 0;
	ppdu_info->tlv_aggr.rd_idx = 0;

	return hal_rx_status_aggr_tlv(hal_soc, rx_tlv_hdr, ppdu_info, nbuf);
}

static inline uint32_t
hal_rx_status_get_tlv_info_wrapper_be(void *rx_tlv_hdr, void *ppduinfo,
				      hal_soc_handle_t hal_soc_hdl,
				      qdf_nbuf_t nbuf)
{
	struct hal_soc *hal = (struct hal_soc *)hal_soc_hdl;
	uint32_t tlv_tag, user_id, tlv_len;
	struct hal_rx_ppdu_info *ppdu_info =
			(struct hal_rx_ppdu_info *)ppduinfo;

	tlv_tag = HAL_RX_GET_USER_TLV32_TYPE(rx_tlv_hdr);
	user_id = HAL_RX_GET_USER_TLV32_USERID(rx_tlv_hdr);
	tlv_len = HAL_RX_GET_USER_TLV32_LEN(rx_tlv_hdr);

	/*
	 * Handle the case where aggregation is in progress
	 * or the current TLV is one of the TLVs which should be
	 * aggregated
	 */
	if (ppdu_info->tlv_aggr.in_progress) {
		if (ppdu_info->tlv_aggr.tlv_tag == tlv_tag) {
			return hal_rx_status_aggr_tlv(hal, rx_tlv_hdr,
						      ppdu_info, nbuf);
		} else {
			/* Finish aggregation of current TLV */
			hal_rx_status_process_aggr_tlv(hal, ppdu_info);
		}
	}

	if (hal_rx_status_tlv_should_aggregate(hal, tlv_tag)) {
		return hal_rx_status_start_new_aggr_tlv(hal, rx_tlv_hdr,
							ppduinfo, nbuf);
	}

	return hal_rx_status_get_tlv_info_generic_be(rx_tlv_hdr, ppduinfo,
						     hal_soc_hdl, nbuf);
}
#endif /* _HAL_BE_API_MON_H_ */
