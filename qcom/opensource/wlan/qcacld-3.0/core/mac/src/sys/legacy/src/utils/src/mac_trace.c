/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**=========================================================================

   \file  mac_trace.c

   \brief implementation for trace related APIs

   \author Sunit Bhatia

   ========================================================================*/

/*--------------------------------------------------------------------------
   Include Files
   ------------------------------------------------------------------------*/

#include "mac_trace.h"
#include "wma_types.h"
#include "csr_internal.h"
#include "lim_global.h"
#include "lim_types.h"
#include "qdf_mem.h"
#include "qdf_trace.h"
#include "wma_if.h"
#include "wma.h"

/**
 * mac_trace_getcsr_roam_state() - Get the csr roam state
 * @csr_roam_state: State in numeric form
 *
 * This function will return a string equivalent of the state.
 *
 * Return: String equivalent of the state.
 **/
uint8_t *mac_trace_getcsr_roam_state(uint16_t csr_roam_state)
{
	switch (csr_roam_state) {
		CASE_RETURN_STRING(eCSR_ROAMING_STATE_STOP);
		CASE_RETURN_STRING(eCSR_ROAMING_STATE_IDLE);
		CASE_RETURN_STRING(eCSR_ROAMING_STATE_JOINING);
		CASE_RETURN_STRING(eCSR_ROAMING_STATE_JOINED);

	default:
		return (uint8_t *) "UNKNOWN";
		break;
	}
}

/**
 * mac_trace_getcsr_roam_sub_state() - Get the csr roam sub state
 * @csr_roam_sub_state: State in numeric form
 *
 * This function will return a string equivalent of the state.
 *
 * Return: String equivalent of the state.
 **/
uint8_t *mac_trace_getcsr_roam_sub_state(uint16_t csr_roam_sub_state)
{
	switch (csr_roam_sub_state) {
		CASE_RETURN_STRING(eCSR_ROAM_SUBSTATE_NONE);
		CASE_RETURN_STRING(eCSR_ROAM_SUBSTATE_START_BSS_REQ);
		CASE_RETURN_STRING(eCSR_ROAM_SUBSTATE_DISASSOC_REQ);
		CASE_RETURN_STRING(eCSR_ROAM_SUBSTATE_STOP_BSS_REQ);
		CASE_RETURN_STRING(eCSR_ROAM_SUBSTATE_DEAUTH_REQ);
		CASE_RETURN_STRING(eCSR_ROAM_SUBSTATE_WAIT_FOR_KEY);
	default:
		return (uint8_t *) "UNKNOWN";
		break;
	}
}

/**
 * mac_trace_get_lim_sme_state() - Get the lim sme state
 * @lim_state: State in numeric form
 *
 * This function will return a string equivalent of the state.
 *
 * Return: String equivalent of the state.
 **/
uint8_t *mac_trace_get_lim_sme_state(uint16_t lim_state)
{
	switch (lim_state) {
		CASE_RETURN_STRING(eLIM_SME_OFFLINE_STATE);
		CASE_RETURN_STRING(eLIM_SME_IDLE_STATE);
		CASE_RETURN_STRING(eLIM_SME_SUSPEND_STATE);
		CASE_RETURN_STRING(eLIM_SME_WT_JOIN_STATE);
		CASE_RETURN_STRING(eLIM_SME_WT_AUTH_STATE);
		CASE_RETURN_STRING(eLIM_SME_WT_ASSOC_STATE);
		CASE_RETURN_STRING(eLIM_SME_WT_REASSOC_STATE);
		CASE_RETURN_STRING(eLIM_SME_JOIN_FAILURE_STATE);
		CASE_RETURN_STRING(eLIM_SME_ASSOCIATED_STATE);
		CASE_RETURN_STRING(eLIM_SME_REASSOCIATED_STATE);
		CASE_RETURN_STRING(eLIM_SME_LINK_EST_STATE);
		CASE_RETURN_STRING(eLIM_SME_WT_PRE_AUTH_STATE);
		CASE_RETURN_STRING(eLIM_SME_WT_DISASSOC_STATE);
		CASE_RETURN_STRING(eLIM_SME_WT_DEAUTH_STATE);
		CASE_RETURN_STRING(eLIM_SME_WT_START_BSS_STATE);
		CASE_RETURN_STRING(eLIM_SME_WT_STOP_BSS_STATE);
		CASE_RETURN_STRING(eLIM_SME_NORMAL_STATE);

	default:
		return (uint8_t *) "UNKNOWN";
		break;
	}
}

/**
 * mac_trace_get_lim_mlm_state() - Get the lim mlm state
 * @mlm_state: State in numeric form
 *
 * This function will return a string equivalent of the state.
 *
 * Return: String equivalent of the state.
 **/
uint8_t *mac_trace_get_lim_mlm_state(uint16_t mlm_state)
{
	switch (mlm_state) {
		CASE_RETURN_STRING(eLIM_MLM_OFFLINE_STATE);
		CASE_RETURN_STRING(eLIM_MLM_IDLE_STATE);
		CASE_RETURN_STRING(eLIM_MLM_WT_JOIN_BEACON_STATE);
		CASE_RETURN_STRING(eLIM_MLM_JOINED_STATE);
		CASE_RETURN_STRING(eLIM_MLM_BSS_STARTED_STATE);
		CASE_RETURN_STRING(eLIM_MLM_WT_AUTH_FRAME2_STATE);
		CASE_RETURN_STRING(eLIM_MLM_WT_AUTH_FRAME3_STATE);
		CASE_RETURN_STRING(eLIM_MLM_WT_AUTH_FRAME4_STATE);
		CASE_RETURN_STRING(eLIM_MLM_AUTH_RSP_TIMEOUT_STATE);
		CASE_RETURN_STRING(eLIM_MLM_AUTHENTICATED_STATE);
		CASE_RETURN_STRING(eLIM_MLM_WT_ASSOC_RSP_STATE);
		CASE_RETURN_STRING(eLIM_MLM_WT_REASSOC_RSP_STATE);
		CASE_RETURN_STRING(eLIM_MLM_ASSOCIATED_STATE);
		CASE_RETURN_STRING(eLIM_MLM_REASSOCIATED_STATE);
		CASE_RETURN_STRING(eLIM_MLM_LINK_ESTABLISHED_STATE);
		CASE_RETURN_STRING(eLIM_MLM_WT_ASSOC_CNF_STATE);
		CASE_RETURN_STRING(eLIM_MLM_WT_ADD_BSS_RSP_STATE);
		CASE_RETURN_STRING(eLIM_MLM_WT_DEL_BSS_RSP_STATE);
		CASE_RETURN_STRING(eLIM_MLM_WT_ADD_BSS_RSP_ASSOC_STATE);
		CASE_RETURN_STRING(eLIM_MLM_WT_ADD_BSS_RSP_REASSOC_STATE);
		CASE_RETURN_STRING(eLIM_MLM_WT_ADD_BSS_RSP_PREASSOC_STATE);
		CASE_RETURN_STRING(eLIM_MLM_WT_ADD_STA_RSP_STATE);
		CASE_RETURN_STRING(eLIM_MLM_WT_DEL_STA_RSP_STATE);

	default:
		return (uint8_t *) "UNKNOWN";
		break;
	}
}

#ifdef TRACE_RECORD
/**
 * mac_trace_get_sme_msg_string() - Get the msg
 * @sme_msg: message type in numeric form
 *
 * This function will return a string equivalent of the message.
 *
 * Return: String equivalent of the message type.
 **/
uint8_t *mac_trace_get_sme_msg_string(uint16_t sme_msg)
{
	switch (sme_msg) {
		CASE_RETURN_STRING(eWNI_SME_SYS_READY_IND);
		CASE_RETURN_STRING(eWNI_SME_JOIN_REQ);
		CASE_RETURN_STRING(eWNI_SME_JOIN_RSP);
		CASE_RETURN_STRING(eWNI_SME_SETCONTEXT_RSP);
		CASE_RETURN_STRING(eWNI_SME_REASSOC_REQ);
		CASE_RETURN_STRING(eWNI_SME_REASSOC_RSP);
		CASE_RETURN_STRING(eWNI_SME_DISASSOC_REQ);
		CASE_RETURN_STRING(eWNI_SME_DISASSOC_RSP);
		CASE_RETURN_STRING(eWNI_SME_DISASSOC_IND);
		CASE_RETURN_STRING(eWNI_SME_DISASSOC_CNF);
		CASE_RETURN_STRING(eWNI_SME_DEAUTH_REQ);
		CASE_RETURN_STRING(eWNI_SME_DEAUTH_RSP);
		CASE_RETURN_STRING(eWNI_SME_DEAUTH_IND);
		CASE_RETURN_STRING(eWNI_SME_DISCONNECT_DONE_IND);
		CASE_RETURN_STRING(eWNI_SME_START_BSS_REQ);
		CASE_RETURN_STRING(eWNI_SME_START_BSS_RSP);
		CASE_RETURN_STRING(eWNI_SME_ASSOC_IND);
		CASE_RETURN_STRING(eWNI_SME_ASSOC_CNF);
		CASE_RETURN_STRING(eWNI_SME_SWITCH_CHL_IND);
		CASE_RETURN_STRING(eWNI_SME_STOP_BSS_REQ);
		CASE_RETURN_STRING(eWNI_SME_STOP_BSS_RSP);
		CASE_RETURN_STRING(eWNI_SME_DEAUTH_CNF);
		CASE_RETURN_STRING(eWNI_SME_MIC_FAILURE_IND);
		CASE_RETURN_STRING(eWNI_SME_ADDTS_REQ);
		CASE_RETURN_STRING(eWNI_SME_MSCS_REQ);
		CASE_RETURN_STRING(eWNI_SME_ADDTS_RSP);
		CASE_RETURN_STRING(eWNI_SME_DELTS_REQ);
		CASE_RETURN_STRING(eWNI_SME_DELTS_RSP);
		CASE_RETURN_STRING(eWNI_SME_DELTS_IND);
		CASE_RETURN_STRING(eWNI_SME_ASSOC_IND_UPPER_LAYER);
		CASE_RETURN_STRING(eWNI_SME_WPS_PBC_PROBE_REQ_IND);
		CASE_RETURN_STRING(eWNI_SME_UPPER_LAYER_ASSOC_CNF);
		CASE_RETURN_STRING(eWNI_SME_SESSION_UPDATE_PARAM);
		CASE_RETURN_STRING(eWNI_SME_CHNG_MCC_BEACON_INTERVAL);
		CASE_RETURN_STRING(eWNI_SME_GET_SNR_REQ);
		CASE_RETURN_STRING(eWNI_SME_LINK_STATUS_IND);
		CASE_RETURN_STRING(eWNI_SME_RRM_MSG_TYPE_BEGIN);
		CASE_RETURN_STRING(eWNI_SME_NEIGHBOR_REPORT_REQ_IND);
		CASE_RETURN_STRING(eWNI_SME_NEIGHBOR_REPORT_IND);
		CASE_RETURN_STRING(eWNI_SME_BEACON_REPORT_REQ_IND);
		CASE_RETURN_STRING(eWNI_SME_BEACON_REPORT_RESP_XMIT_IND);
		CASE_RETURN_STRING(eWNI_SME_CHAN_LOAD_REPORT_RESP_XMIT_IND);
		CASE_RETURN_STRING(eWNI_SME_CHAN_LOAD_REQ_IND);
		CASE_RETURN_STRING(eWNI_SME_FT_AGGR_QOS_REQ);
		CASE_RETURN_STRING(eWNI_SME_FT_AGGR_QOS_RSP);
#if defined FEATURE_WLAN_ESE
		CASE_RETURN_STRING(eWNI_SME_ESE_ADJACENT_AP_REPORT);
#endif
		CASE_RETURN_STRING(eWNI_SME_REGISTER_MGMT_FRAME_REQ);
		CASE_RETURN_STRING(eWNI_SME_GENERIC_CHANGE_COUNTRY_CODE);
		CASE_RETURN_STRING(eWNI_SME_MAX_ASSOC_EXCEEDED);
#ifdef WLAN_FEATURE_GTK_OFFLOAD
		CASE_RETURN_STRING(eWNI_PMC_GTK_OFFLOAD_GETINFO_RSP);
#endif /* WLAN_FEATURE_GTK_OFFLOAD */
		CASE_RETURN_STRING(eWNI_SME_DFS_RADAR_FOUND);
		CASE_RETURN_STRING(eWNI_SME_CHANNEL_CHANGE_REQ);
		CASE_RETURN_STRING(eWNI_SME_CHANNEL_CHANGE_RSP);
		CASE_RETURN_STRING(eWNI_SME_START_BEACON_REQ);
		CASE_RETURN_STRING(eWNI_SME_DFS_BEACON_CHAN_SW_IE_REQ);
		CASE_RETURN_STRING(eWNI_SME_DFS_CSAIE_TX_COMPLETE_IND);
		CASE_RETURN_STRING(eWNI_SME_STATS_EXT_EVENT);
		CASE_RETURN_STRING(eWNI_SME_UPDATE_ADDITIONAL_IES);
		CASE_RETURN_STRING(eWNI_SME_MODIFY_ADDITIONAL_IES);
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
		CASE_RETURN_STRING(eWNI_SME_AUTO_SHUTDOWN_IND);
#endif
		CASE_RETURN_STRING(eWNI_SME_SET_HT_2040_MODE);
#ifdef FEATURE_WLAN_TDLS
		CASE_RETURN_STRING(eWNI_SME_TDLS_SEND_MGMT_REQ);
		CASE_RETURN_STRING(eWNI_SME_TDLS_SEND_MGMT_RSP);
		CASE_RETURN_STRING(eWNI_SME_TDLS_ADD_STA_REQ);
		CASE_RETURN_STRING(eWNI_SME_TDLS_ADD_STA_RSP);
		CASE_RETURN_STRING(eWNI_SME_TDLS_DEL_STA_REQ);
		CASE_RETURN_STRING(eWNI_SME_TDLS_DEL_STA_RSP);
		CASE_RETURN_STRING(eWNI_SME_TDLS_DEL_STA_IND);
		CASE_RETURN_STRING(eWNI_SME_TDLS_DEL_ALL_PEER_IND);
		CASE_RETURN_STRING(eWNI_SME_MGMT_FRM_TX_COMPLETION_IND);
		CASE_RETURN_STRING(eWNI_SME_TDLS_LINK_ESTABLISH_REQ);
		CASE_RETURN_STRING(eWNI_SME_TDLS_LINK_ESTABLISH_RSP);
		CASE_RETURN_STRING(eWNI_SME_TDLS_SHOULD_DISCOVER);
		CASE_RETURN_STRING(eWNI_SME_TDLS_SHOULD_TEARDOWN);
		CASE_RETURN_STRING(eWNI_SME_TDLS_PEER_DISCONNECTED);
#endif
		CASE_RETURN_STRING(eWNI_SME_UNPROT_MGMT_FRM_IND);
		CASE_RETURN_STRING(eWNI_SME_GET_TSM_STATS_REQ);
		CASE_RETURN_STRING(eWNI_SME_GET_TSM_STATS_RSP);
		CASE_RETURN_STRING(eWNI_SME_TSM_IE_IND);
		CASE_RETURN_STRING(eWNI_SME_SET_HW_MODE_REQ);
		CASE_RETURN_STRING(eWNI_SME_SET_HW_MODE_RESP);
		CASE_RETURN_STRING(eWNI_SME_HW_MODE_TRANS_IND);
		CASE_RETURN_STRING(eWNI_SME_NSS_UPDATE_REQ);
		CASE_RETURN_STRING(eWNI_SME_NSS_UPDATE_RSP);
		CASE_RETURN_STRING(eWNI_SME_REGISTER_MGMT_FRAME_CB);
		CASE_RETURN_STRING(eWNI_SME_HT40_OBSS_SCAN_IND);
#ifdef WLAN_FEATURE_NAN
		CASE_RETURN_STRING(eWNI_SME_NAN_EVENT);
#endif
#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
		CASE_RETURN_STRING(eWNI_SME_READY_TO_EXTWOW_IND);
#endif
		CASE_RETURN_STRING(eWNI_SME_MSG_GET_TEMPERATURE_IND);
		CASE_RETURN_STRING(eWNI_SME_SNR_IND);
#ifdef FEATURE_WLAN_EXTSCAN
		CASE_RETURN_STRING(eWNI_SME_EXTSCAN_FULL_SCAN_RESULT_IND);
		CASE_RETURN_STRING(eWNI_SME_EPNO_NETWORK_FOUND_IND);
#endif
		CASE_RETURN_STRING(eWNI_SME_SET_THERMAL_LEVEL_IND);
		CASE_RETURN_STRING(eWNI_SME_OCB_SET_CONFIG_RSP);
		CASE_RETURN_STRING(eWNI_SME_OCB_GET_TSF_TIMER_RSP);
		CASE_RETURN_STRING(eWNI_SME_DCC_GET_STATS_RSP);
		CASE_RETURN_STRING(eWNI_SME_DCC_UPDATE_NDL_RSP);
		CASE_RETURN_STRING(eWNI_SME_DCC_STATS_EVENT);
		CASE_RETURN_STRING(eWNI_SME_SET_DUAL_MAC_CFG_REQ);
		CASE_RETURN_STRING(eWNI_SME_SET_DUAL_MAC_CFG_RESP);
		CASE_RETURN_STRING(eWNI_SME_SET_IE_REQ);
		CASE_RETURN_STRING(eWNI_SME_EXT_CHANGE_CHANNEL);
		CASE_RETURN_STRING(eWNI_SME_EXT_CHANGE_CHANNEL_IND);
		CASE_RETURN_STRING(eWNI_SME_SET_ANTENNA_MODE_REQ);
		CASE_RETURN_STRING(eWNI_SME_SET_ANTENNA_MODE_RESP);
		CASE_RETURN_STRING(eWNI_SME_TSF_EVENT);
		CASE_RETURN_STRING(eWNI_SME_MON_INIT_SESSION);
		CASE_RETURN_STRING(eWNI_SME_MON_DEINIT_SESSION);
		CASE_RETURN_STRING(eWNI_SME_PDEV_SET_HT_VHT_IE);
		CASE_RETURN_STRING(eWNI_SME_SET_VDEV_IES_PER_BAND);
		CASE_RETURN_STRING(eWNI_SME_SEND_DISASSOC_FRAME);
		CASE_RETURN_STRING(eWNI_SME_UPDATE_ACCESS_POLICY_VENDOR_IE);
		CASE_RETURN_STRING(eWNI_SME_DEFAULT_SCAN_IE);
		CASE_RETURN_STRING(eWNI_SME_LOST_LINK_INFO_IND);
		CASE_RETURN_STRING(eWNI_SME_GET_PEER_INFO_EXT_IND);
		CASE_RETURN_STRING(eWNI_SME_RSO_CMD_STATUS_IND);
		CASE_RETURN_STRING(eWNI_SME_TRIGGER_SAE);
		CASE_RETURN_STRING(eWNI_SME_SEND_MGMT_FRAME_TX);
		CASE_RETURN_STRING(eWNI_SME_SEND_SAE_MSG);
		CASE_RETURN_STRING(eWNI_SME_CSA_RESTART_REQ);
		CASE_RETURN_STRING(eWNI_SME_CSA_RESTART_RSP);
		CASE_RETURN_STRING(eWNI_SME_MSG_TYPES_END);
		CASE_RETURN_STRING(eWNI_SME_HIDDEN_SSID_RESTART_RSP);
		CASE_RETURN_STRING(eWNI_SME_STA_CSA_CONTINUE_REQ);
		CASE_RETURN_STRING(eWNI_SME_ANTENNA_ISOLATION_RSP);
	default:
		return (uint8_t *) "UNKNOWN";
		break;
	}
}
#endif

/**
 * mac_trace_get_wma_msg_string() - Get the msg
 * @wma_msg: message type in numeric form
 *
 * This function will return a string equivalent of the message.
 *
 * Return: String equivalent of the message type.
 **/
uint8_t *mac_trace_get_wma_msg_string(uint16_t wma_msg)
{
	switch (wma_msg) {
		CASE_RETURN_STRING(WMA_ADD_STA_REQ);
		CASE_RETURN_STRING(WMA_ADD_STA_RSP);
		CASE_RETURN_STRING(WMA_DELETE_STA_REQ);
		CASE_RETURN_STRING(WMA_DELETE_STA_RSP);
		CASE_RETURN_STRING(WMA_ADD_BSS_REQ);
		CASE_RETURN_STRING(WMA_DELETE_BSS_REQ);
		CASE_RETURN_STRING(WMA_DELETE_BSS_HO_FAIL_REQ);
		CASE_RETURN_STRING(WMA_DELETE_BSS_RSP);
		CASE_RETURN_STRING(WMA_DELETE_BSS_HO_FAIL_RSP);
		CASE_RETURN_STRING(WMA_SEND_BEACON_REQ);
		CASE_RETURN_STRING(WMA_SET_BSSKEY_RSP);
		CASE_RETURN_STRING(WMA_SET_STAKEY_RSP);
		CASE_RETURN_STRING(WMA_UPDATE_EDCA_PROFILE_IND);

		CASE_RETURN_STRING(WMA_UPDATE_BEACON_IND);
		CASE_RETURN_STRING(WMA_CHNL_SWITCH_REQ);
		CASE_RETURN_STRING(WMA_ADD_TS_REQ);
		CASE_RETURN_STRING(WMA_DEL_TS_REQ);
		CASE_RETURN_STRING(WMA_MISSED_BEACON_IND);

		CASE_RETURN_STRING(WMA_SWITCH_CHANNEL_RSP);
		CASE_RETURN_STRING(WMA_P2P_NOA_ATTR_IND);
		CASE_RETURN_STRING(WMA_PWR_SAVE_CFG);

		CASE_RETURN_STRING(WMA_TIMER_ADJUST_ADAPTIVE_THRESHOLD_IND);
		CASE_RETURN_STRING(WMA_SET_STA_BCASTKEY_RSP);
		CASE_RETURN_STRING(WMA_ADD_TS_RSP);
		CASE_RETURN_STRING(WMA_DPU_MIC_ERROR);

		CASE_RETURN_STRING(WMA_TIMER_CHIP_MONITOR_TIMEOUT);
		CASE_RETURN_STRING(WMA_TIMER_TRAFFIC_ACTIVITY_REQ);
		CASE_RETURN_STRING(WMA_TIMER_ADC_RSSI_STATS);
#ifdef FEATURE_WLAN_ESE
		CASE_RETURN_STRING(WMA_TSM_STATS_REQ);
		CASE_RETURN_STRING(WMA_TSM_STATS_RSP);
#endif
		CASE_RETURN_STRING(WMA_HT40_OBSS_SCAN_IND);
		CASE_RETURN_STRING(WMA_SET_MIMOPS_REQ);
		CASE_RETURN_STRING(WMA_SET_MIMOPS_RSP);
		CASE_RETURN_STRING(WMA_SYS_READY_IND);
		CASE_RETURN_STRING(WMA_SET_TX_POWER_REQ);
		CASE_RETURN_STRING(WMA_SET_TX_POWER_RSP);
		CASE_RETURN_STRING(WMA_GET_TX_POWER_REQ);

		CASE_RETURN_STRING(WMA_ENABLE_UAPSD_REQ);
		CASE_RETURN_STRING(WMA_DISABLE_UAPSD_REQ);
		CASE_RETURN_STRING(WMA_SET_KEY_DONE);

		CASE_RETURN_STRING(WMA_BTC_SET_CFG);
		CASE_RETURN_STRING(WMA_HANDLE_FW_MBOX_RSP);
		CASE_RETURN_STRING(WMA_SEND_PROBE_RSP_TMPL);
		CASE_RETURN_STRING(WMA_SET_MAX_TX_POWER_REQ);
		CASE_RETURN_STRING(WMA_SET_HOST_OFFLOAD);
		CASE_RETURN_STRING(WMA_SET_KEEP_ALIVE);
#ifdef WLAN_NS_OFFLOAD
		CASE_RETURN_STRING(WMA_SET_NS_OFFLOAD);
#endif /* WLAN_NS_OFFLOAD */
		CASE_RETURN_STRING(WMA_WLAN_SUSPEND_IND);
		CASE_RETURN_STRING(WMA_WLAN_RESUME_REQ);
#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
		CASE_RETURN_STRING(WMA_WLAN_EXT_WOW);
		CASE_RETURN_STRING(WMA_WLAN_SET_APP_TYPE1_PARAMS);
		CASE_RETURN_STRING(WMA_WLAN_SET_APP_TYPE2_PARAMS);
#endif
		CASE_RETURN_STRING(WMA_MSG_TYPES_END);
		CASE_RETURN_STRING(WMA_AGGR_QOS_REQ);
		CASE_RETURN_STRING(WMA_AGGR_QOS_RSP);
		CASE_RETURN_STRING(WMA_ROAM_PRE_AUTH_STATUS);
#ifdef WLAN_FEATURE_PACKET_FILTERING
		CASE_RETURN_STRING(WMA_8023_MULTICAST_LIST_REQ);
		CASE_RETURN_STRING(WMA_RECEIVE_FILTER_SET_FILTER_REQ);
		CASE_RETURN_STRING
			(WMA_PACKET_COALESCING_FILTER_MATCH_COUNT_REQ);
		CASE_RETURN_STRING
			(WMA_PACKET_COALESCING_FILTER_MATCH_COUNT_RSP);
		CASE_RETURN_STRING(WMA_RECEIVE_FILTER_CLEAR_FILTER_REQ);
#endif /* WLAN_FEATURE_PACKET_FILTERING */
#ifdef WLAN_FEATURE_GTK_OFFLOAD
		CASE_RETURN_STRING(WMA_GTK_OFFLOAD_REQ);
		CASE_RETURN_STRING(WMA_GTK_OFFLOAD_GETINFO_REQ);
		CASE_RETURN_STRING(WMA_GTK_OFFLOAD_GETINFO_RSP);
#endif /* WLAN_FEATURE_GTK_OFFLOAD */
		CASE_RETURN_STRING(WMA_SET_TM_LEVEL_REQ);
		CASE_RETURN_STRING(WMA_UPDATE_OP_MODE);
		CASE_RETURN_STRING(WMA_UPDATE_MEMBERSHIP);
		CASE_RETURN_STRING(WMA_UPDATE_USERPOS);
		CASE_RETURN_STRING(WMA_UPDATE_CHAN_LIST_REQ);
		CASE_RETURN_STRING(WMA_CLI_SET_CMD);
#ifndef REMOVE_PKT_LOG
		CASE_RETURN_STRING(WMA_PKTLOG_ENABLE_REQ);
#endif
#ifdef FEATURE_WLAN_ESE
		CASE_RETURN_STRING(WMA_SET_PLM_REQ);
#endif
		CASE_RETURN_STRING(WMA_RATE_UPDATE_IND);
		CASE_RETURN_STRING(WMA_INIT_BAD_PEER_TX_CTL_INFO_CMD);
#ifdef FEATURE_WLAN_TDLS
		CASE_RETURN_STRING(WMA_UPDATE_TDLS_PEER_STATE);
#endif
		CASE_RETURN_STRING(WMA_ADD_PERIODIC_TX_PTRN_IND);
		CASE_RETURN_STRING(WMA_TX_POWER_LIMIT);
		CASE_RETURN_STRING(WMA_DHCP_START_IND);
		CASE_RETURN_STRING(WMA_DHCP_STOP_IND);
#ifdef FEATURE_WLAN_CH_AVOID
		CASE_RETURN_STRING(WMA_CH_AVOID_UPDATE_REQ);
#endif
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
		CASE_RETURN_STRING(WMA_SET_AUTO_SHUTDOWN_TIMER_REQ);
#endif
		CASE_RETURN_STRING(WMA_INIT_THERMAL_INFO_CMD);
		CASE_RETURN_STRING(WMA_SET_THERMAL_LEVEL);
		CASE_RETURN_STRING(WMA_SET_SAP_INTRABSS_DIS);
		CASE_RETURN_STRING(SIR_HAL_SET_BASE_MACADDR_IND);
		CASE_RETURN_STRING(WMA_LINK_STATUS_GET_REQ);
#ifdef DHCP_SERVER_OFFLOAD
		CASE_RETURN_STRING(WMA_SET_DHCP_SERVER_OFFLOAD_CMD);
#endif
		CASE_RETURN_STRING(WMA_OCB_SET_CONFIG_CMD);
		CASE_RETURN_STRING(WMA_OCB_SET_UTC_TIME_CMD);
		CASE_RETURN_STRING(WMA_OCB_START_TIMING_ADVERT_CMD);
		CASE_RETURN_STRING(WMA_OCB_STOP_TIMING_ADVERT_CMD);
		CASE_RETURN_STRING(WMA_OCB_GET_TSF_TIMER_CMD);
		CASE_RETURN_STRING(WMA_DCC_GET_STATS_CMD);
		CASE_RETURN_STRING(WMA_DCC_CLEAR_STATS_CMD);
		CASE_RETURN_STRING(WMA_DCC_UPDATE_NDL_CMD);
		CASE_RETURN_STRING(WMA_SET_IE_INFO);
		CASE_RETURN_STRING(WMA_GW_PARAM_UPDATE_REQ);
		CASE_RETURN_STRING(WMA_ADD_BCN_FILTER_CMDID);
		CASE_RETURN_STRING(WMA_REMOVE_BCN_FILTER_CMDID);
		CASE_RETURN_STRING(WMA_SET_ADAPT_DWELLTIME_CONF_PARAMS);
		CASE_RETURN_STRING(WDA_APF_GET_CAPABILITIES_REQ);
		CASE_RETURN_STRING(WMA_ROAM_SYNC_TIMEOUT);
		CASE_RETURN_STRING(WMA_SET_PDEV_IE_REQ);
		CASE_RETURN_STRING(WMA_SEND_FREQ_RANGE_CONTROL_IND);
		CASE_RETURN_STRING(WMA_POWER_DEBUG_STATS_REQ);
		CASE_RETURN_STRING(SIR_HAL_SET_MAS);
		CASE_RETURN_STRING(SIR_HAL_SET_MIRACAST);
		CASE_RETURN_STRING(SIR_HAL_CONFIG_STATS_FACTOR);
		CASE_RETURN_STRING(SIR_HAL_CONFIG_GUARD_TIME);
		CASE_RETURN_STRING(SIR_HAL_START_STOP_LOGGING);
		CASE_RETURN_STRING(SIR_HAL_FLUSH_LOG_TO_FW);
		CASE_RETURN_STRING(SIR_HAL_SET_PCL_TO_FW);
		CASE_RETURN_STRING(SIR_HAL_PDEV_SET_HW_MODE);
		CASE_RETURN_STRING(SIR_HAL_PDEV_DUAL_MAC_CFG_REQ);
		CASE_RETURN_STRING(WMA_RADAR_DETECTED_IND);
		CASE_RETURN_STRING(WMA_TIMER_TRAFFIC_STATS_IND);
		CASE_RETURN_STRING(WMA_EXCLUDE_UNENCRYPTED_IND);
		CASE_RETURN_STRING(WMA_SET_MAX_TX_POWER_RSP);
		CASE_RETURN_STRING(WMA_SET_DTIM_PERIOD);
		CASE_RETURN_STRING(WMA_SET_MAX_TX_POWER_PER_BAND_REQ);
#ifdef FEATURE_WLAN_TDLS
		CASE_RETURN_STRING(WMA_SET_TDLS_LINK_ESTABLISH_REQ);
		CASE_RETURN_STRING(WMA_SET_TDLS_LINK_ESTABLISH_REQ_RSP);
#endif
		CASE_RETURN_STRING(WMA_CSA_OFFLOAD_EVENT);
		CASE_RETURN_STRING(WMA_UPDATE_RX_NSS);
#ifdef WLAN_FEATURE_NAN
		CASE_RETURN_STRING(WMA_NAN_REQUEST);
#endif
#ifdef WLAN_SUPPORT_TWT
		CASE_RETURN_STRING(WMA_TWT_ADD_DIALOG_REQUEST);
		CASE_RETURN_STRING(WMA_TWT_DEL_DIALOG_REQUEST);
		CASE_RETURN_STRING(WMA_TWT_PAUSE_DIALOG_REQUEST);
		CASE_RETURN_STRING(WMA_TWT_RESUME_DIALOG_REQUEST);
		CASE_RETURN_STRING(WMA_TWT_NUDGE_DIALOG_REQUEST);
#endif
		CASE_RETURN_STRING(WMA_RX_SCAN_EVENT);
		CASE_RETURN_STRING(WMA_DEL_PERIODIC_TX_PTRN_IND);
#ifdef FEATURE_WLAN_TDLS
		CASE_RETURN_STRING(WMA_TDLS_SHOULD_DISCOVER_CMD);
		CASE_RETURN_STRING(WMA_TDLS_SHOULD_TEARDOWN_CMD);
		CASE_RETURN_STRING(WMA_TDLS_PEER_DISCONNECTED_CMD);
#endif
		CASE_RETURN_STRING(WMA_DFS_BEACON_TX_SUCCESS_IND);
		CASE_RETURN_STRING(WMA_DISASSOC_TX_COMP);
		CASE_RETURN_STRING(WMA_DEAUTH_TX_COMP);
		CASE_RETURN_STRING(WMA_MODEM_POWER_STATE_IND);
#ifdef WLAN_FEATURE_STATS_EXT
		CASE_RETURN_STRING(WMA_STATS_EXT_REQUEST);
#endif
		CASE_RETURN_STRING(WMA_GET_TEMPERATURE_REQ);
#ifdef FEATURE_WLAN_EXTSCAN
		CASE_RETURN_STRING(WMA_EXTSCAN_GET_CAPABILITIES_REQ);
		CASE_RETURN_STRING(WMA_EXTSCAN_START_REQ);
		CASE_RETURN_STRING(WMA_EXTSCAN_STOP_REQ);
		CASE_RETURN_STRING(WMA_EXTSCAN_SET_BSSID_HOTLIST_REQ);
		CASE_RETURN_STRING(WMA_EXTSCAN_RESET_BSSID_HOTLIST_REQ);
		CASE_RETURN_STRING(WMA_EXTSCAN_SET_SIGNF_CHANGE_REQ);
		CASE_RETURN_STRING(WMA_EXTSCAN_RESET_SIGNF_CHANGE_REQ);
		CASE_RETURN_STRING(WMA_EXTSCAN_GET_CACHED_RESULTS_REQ);
		CASE_RETURN_STRING(WMA_SET_EPNO_LIST_REQ);
		CASE_RETURN_STRING(WMA_SET_PASSPOINT_LIST_REQ);
		CASE_RETURN_STRING(WMA_RESET_PASSPOINT_LIST_REQ);
#endif /* FEATURE_WLAN_EXTSCAN */
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
		CASE_RETURN_STRING(WMA_LINK_LAYER_STATS_CLEAR_REQ);
		CASE_RETURN_STRING(WMA_LINK_LAYER_STATS_SET_REQ);
		CASE_RETURN_STRING(WMA_LINK_LAYER_STATS_GET_REQ);
		CASE_RETURN_STRING(WMA_LINK_LAYER_STATS_RESULTS_RSP);
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */
		CASE_RETURN_STRING(WMA_SET_SCAN_MAC_OUI_REQ);
		CASE_RETURN_STRING(WMA_TSF_GPIO_PIN);
#ifdef WLAN_FEATURE_GPIO_LED_FLASHING
		CASE_RETURN_STRING(WMA_LED_FLASHING_REQ);
#endif
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
		CASE_RETURN_STRING(WMA_UPDATE_Q2Q_IE_IND);
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
		CASE_RETURN_STRING(WMA_SET_RSSI_MONITOR_REQ);
		CASE_RETURN_STRING(WMA_SET_WISA_PARAMS);
		CASE_RETURN_STRING(WMA_SET_WOW_PULSE_CMD);
		CASE_RETURN_STRING(WMA_GET_RCPI_REQ);
		CASE_RETURN_STRING(WMA_SET_DBS_SCAN_SEL_CONF_PARAMS);
		CASE_RETURN_STRING(WMA_GET_ROAM_SCAN_STATS);
		CASE_RETURN_STRING(WMA_PEER_CREATE_RESPONSE);
#ifdef FW_THERMAL_THROTTLE_SUPPORT
		CASE_RETURN_STRING(WMA_SET_THERMAL_THROTTLE_CFG);
		CASE_RETURN_STRING(WMA_SET_THERMAL_MGMT);
#endif /* FW_THERMAL_THROTTLE_SUPPORT */
		CASE_RETURN_STRING(WMA_UPDATE_EDCA_PIFS_PARAM_IND);
#ifdef FEATURE_WLAN_APF
		CASE_RETURN_STRING(WMA_ENABLE_ACTIVE_APF_MODE_IND);
		CASE_RETURN_STRING(WMA_DISABLE_ACTIVE_APF_MODE_IND);
#endif
	default:
		return (uint8_t *) "UNKNOWN";
		break;
	}
}

#ifdef TRACE_RECORD
/**
 * mac_trace_get_lim_msg_string() - Get the msg
 * @lim_msg: message type in numeric form
 *
 * This function will return a string equivalent of the message.
 *
 * Return: String equivalent of the message type.
 **/
uint8_t *mac_trace_get_lim_msg_string(uint16_t lim_msg)
{
	switch (lim_msg) {
		CASE_RETURN_STRING(SIR_BB_XPORT_MGMT_MSG);
		CASE_RETURN_STRING(SIR_LIM_DELETE_STA_CONTEXT_IND);
		CASE_RETURN_STRING(SIR_LIM_UPDATE_BEACON);
		CASE_RETURN_STRING(SIR_LIM_JOIN_FAIL_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_AUTH_FAIL_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_AUTH_RSP_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_ASSOC_FAIL_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_REASSOC_FAIL_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_HEART_BEAT_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_ADDTS_RSP_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_LINK_TEST_DURATION_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_CNF_WAIT_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_UPDATE_OLBC_CACHEL_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_WPS_OVERLAP_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_FT_PREAUTH_RSP_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_DISASSOC_ACK_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_PERIODIC_JOIN_PROBE_REQ_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_AUTH_RETRY_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_AUTH_SAE_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_RRM_STA_STATS_RSP_TIMEOUT);
		CASE_RETURN_STRING(SIR_LIM_MSG_TYPES_END);
		CASE_RETURN_STRING(LIM_MLM_SCAN_REQ);
		CASE_RETURN_STRING(LIM_MLM_SCAN_CNF);
		CASE_RETURN_STRING(LIM_MLM_START_CNF);
		CASE_RETURN_STRING(LIM_MLM_JOIN_REQ);
		CASE_RETURN_STRING(LIM_MLM_JOIN_CNF);
		CASE_RETURN_STRING(LIM_MLM_AUTH_REQ);
		CASE_RETURN_STRING(LIM_MLM_AUTH_CNF);
		CASE_RETURN_STRING(LIM_MLM_AUTH_IND);
		CASE_RETURN_STRING(LIM_MLM_ASSOC_REQ);
		CASE_RETURN_STRING(LIM_MLM_ASSOC_CNF);
		CASE_RETURN_STRING(LIM_MLM_ASSOC_IND);
		CASE_RETURN_STRING(LIM_MLM_DISASSOC_REQ);
		CASE_RETURN_STRING(LIM_MLM_DISASSOC_CNF);
		CASE_RETURN_STRING(LIM_MLM_DISASSOC_IND);
		CASE_RETURN_STRING(LIM_MLM_REASSOC_CNF);
		CASE_RETURN_STRING(LIM_MLM_REASSOC_IND);
		CASE_RETURN_STRING(LIM_MLM_DEAUTH_REQ);
		CASE_RETURN_STRING(LIM_MLM_DEAUTH_CNF);
		CASE_RETURN_STRING(LIM_MLM_DEAUTH_IND);
		CASE_RETURN_STRING(LIM_MLM_TSPEC_REQ);
		CASE_RETURN_STRING(LIM_MLM_TSPEC_CNF);
		CASE_RETURN_STRING(LIM_MLM_SETKEYS_CNF);
		CASE_RETURN_STRING(LIM_MLM_LINK_TEST_STOP_REQ);
		CASE_RETURN_STRING(LIM_MLM_PURGE_STA_IND);
	default:
		return (uint8_t *) "UNKNOWN";
		break;
	}
}

/**
 * mac_trace_get_info_log_string() - Get the log info
 * @info_log: message type in numeric form
 *
 * This function will return a string equivalent of the message.
 *
 * Return: String equivalent of the message type.
 **/
uint8_t *mac_trace_get_info_log_string(uint16_t info_log)
{
	switch (info_log) {
		CASE_RETURN_STRING(eLOG_NODROP_MISSED_BEACON_SCENARIO);
		CASE_RETURN_STRING(eLOG_PROC_DEAUTH_FRAME_SCENARIO);
	default:
		return (uint8_t *) "UNKNOWN";
		break;
	}
}

#endif
