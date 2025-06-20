// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <net/ip.h>
#include <linux/genalloc.h>	/* gen_pool_alloc() */
#include <linux/io.h>
#include <linux/ratelimit.h>
#include <linux/interconnect.h>
#include <linux/msm_gsi.h>
#include <linux/elf.h>
#include "ipa_i.h"
#include "ipahal.h"
#include "ipahal_nat.h"
#include "ipahal_fltrt.h"
#include "ipahal_hw_stats.h"
#include "ipa_rm_i.h"
#include "gsi.h"

/*
 * The following for adding code (ie. for EMULATION) not found on x86.
 */
#if defined(CONFIG_IPA_EMULATION)
# include "ipa_emulation_stubs.h"
#endif

#define IPA_V3_0_CLK_RATE_SVS2 (37.5 * 1000 * 1000UL)
#define IPA_V3_0_CLK_RATE_SVS (75 * 1000 * 1000UL)
#define IPA_V3_0_CLK_RATE_NOMINAL (150 * 1000 * 1000UL)
#define IPA_V3_0_CLK_RATE_TURBO (200 * 1000 * 1000UL)

#define IPA_V3_5_CLK_RATE_SVS2 (100 * 1000 * 1000UL)
#define IPA_V3_5_CLK_RATE_SVS (200 * 1000 * 1000UL)
#define IPA_V3_5_CLK_RATE_NOMINAL (400 * 1000 * 1000UL)
#define IPA_V3_5_CLK_RATE_TURBO (42640 * 10 * 1000UL)

#define IPA_V4_0_CLK_RATE_SVS2 (60 * 1000 * 1000UL)
#define IPA_V4_0_CLK_RATE_SVS (125 * 1000 * 1000UL)
#define IPA_V4_0_CLK_RATE_NOMINAL (220 * 1000 * 1000UL)
#define IPA_V4_0_CLK_RATE_TURBO (250 * 1000 * 1000UL)

#define IPA_V5_0_CLK_RATE_SVS2 (120 * 1000 * 1000UL)
#define IPA_V5_0_CLK_RATE_SVS (240 * 1000 * 1000UL)
#define IPA_V5_0_CLK_RATE_NOMINAL (500 * 1000 * 1000UL)
#define IPA_V5_0_CLK_RATE_TURBO (600 * 1000 * 1000UL)

#define IPA_MAX_HOLB_TMR_VAL (4294967296 - 1)

#define IPA_V3_0_BW_THRESHOLD_TURBO_MBPS (1000)
#define IPA_V3_0_BW_THRESHOLD_NOMINAL_MBPS (600)
#define IPA_V3_0_BW_THRESHOLD_SVS_MBPS (310)

#define IPA_ENDP_INIT_HDR_METADATA_n_MUX_ID_BMASK 0xFF0000
#define IPA_ENDP_INIT_HDR_METADATA_n_MUX_ID_SHFT 0x10

/* Max pipes + ICs for TAG process */
#define IPA_TAG_MAX_DESC (IPA3_MAX_NUM_PIPES + 6)

#define IPA_TAG_SLEEP_MIN_USEC (1000)
#define IPA_TAG_SLEEP_MAX_USEC (2000)
#define IPA_FORCE_CLOSE_TAG_PROCESS_TIMEOUT (10 * HZ)
#define IPA_BCR_REG_VAL_v3_0 (0x00000001)
#define IPA_BCR_REG_VAL_v3_5 (0x0000003B)
#define IPA_BCR_REG_VAL_v4_0 (0x00000039)
#define IPA_BCR_REG_VAL_v4_2 (0x00000000)
#define IPA_AGGR_GRAN_MIN (1)
#define IPA_AGGR_GRAN_MAX (32)
#define IPA_EOT_COAL_GRAN_MIN (1)
#define IPA_EOT_COAL_GRAN_MAX (16)

#define IPA_FILT_ROUT_HASH_REG_VAL_v4_2 (0x00000000)
#define IPA_DMA_TASK_FOR_GSI_TIMEOUT_MSEC (15)
#define IPA_COAL_CLOSE_FRAME_CMD_TIMEOUT_MSEC (500)

#define IPA_AGGR_BYTE_LIMIT (\
		IPA_ENDP_INIT_AGGR_N_AGGR_BYTE_LIMIT_BMSK >> \
		IPA_ENDP_INIT_AGGR_N_AGGR_BYTE_LIMIT_SHFT)
#define IPA_AGGR_PKT_LIMIT (\
		IPA_ENDP_INIT_AGGR_n_AGGR_PKT_LIMIT_BMSK >> \
		IPA_ENDP_INIT_AGGR_n_AGGR_PKT_LIMIT_SHFT)

/* In IPAv3 only endpoints 0-3 can be configured to deaggregation */
#define IPA_EP_SUPPORTS_DEAGGR(idx) ((idx) >= 0 && (idx) <= 3)

#define IPA_TAG_TIMER_TIMESTAMP_SHFT (14) /* ~0.8msec */
#define IPA_NAT_TIMER_TIMESTAMP_SHFT (24) /* ~0.8sec */

/*
 * Units of time per a specific granularity
 * The limitation based on H/W HOLB/AGGR time limit field width
 */
#define IPA_TIMER_SCALED_TIME_LIMIT 31

/* HPS, DPS sequencers Types*/

/* DMA Only */
#define IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY  0x00000000
/* Packet Processing + no decipher + uCP (for Ethernet Bridging) */
#define IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP 0x00000002
/* Packet Processing + no decipher + no uCP */
#define IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP 0x00000006
/* 2 Packet Processing pass + no decipher + uCP */
#define IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP 0x00000004
/* 2 Packet Processing pass + decipher + uCP
 * Deprecated since IPA 5.0
 */
#define IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP 0x00000015
/* 2 Packet Processing pass + no decipher + uCP + HPS REP DMA Parser. */
#define IPA_DPS_HPS_REP_SEQ_TYPE_2PKT_PROC_PASS_NO_DEC_UCP_DMAP 0x00000804
/* Packet Processing + no decipher + no uCP + HPS REP DMA Parser.*/
#define IPA_DPS_HPS_REP_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP_DMAP 0x00000806
/* COMP/DECOMP */
#define IPA_DPS_HPS_SEQ_TYPE_DMA_COMP_DECOMP 0x00000020
/* 2 Packet Processing + no decipher + 2 uCP */
#define IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_2ND_UCP 0x0000000a
/* 3 Packet Processing + no decipher + 2 uCP */
#define IPA_DPS_HPS_SEQ_TYPE_3RD_PKT_PROCESS_PASS_NO_DEC_2ND_UCP 0x0000000c
/* 2 Packet Processing + no decipher + 2 uCP + HPS REP DMA Parser */
#define IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_2ND_UCP_DMAP 0x0000080a
/* 3 Packet Processing + no decipher + 2 uCP + HPS REP DMA Parser */
#define IPA_DPS_HPS_SEQ_TYPE_3RD_PKT_PROCESS_PASS_NO_DEC_2ND_UCP_DMAP 0x0000080c
/* Invalid sequencer type */
#define IPA_DPS_HPS_SEQ_TYPE_INVALID 0xFFFFFFFF

#define IPA_DPS_HPS_SEQ_TYPE_IS_DMA(seq_type) \
	(seq_type == IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY || \
	seq_type == IPA_DPS_HPS_SEQ_TYPE_DMA_COMP_DECOMP)


/* Resource Group index*/
#define IPA_v3_0_GROUP_UL		(0)
#define IPA_v3_0_GROUP_DL		(1)
#define IPA_v3_0_GROUP_DPL		IPA_v3_0_GROUP_DL
#define IPA_v3_0_GROUP_DIAG		(2)
#define IPA_v3_0_GROUP_DMA		(3)
#define IPA_v3_0_GROUP_IMM_CMD		IPA_v3_0_GROUP_UL
#define IPA_v3_0_GROUP_Q6ZIP		(4)
#define IPA_v3_0_GROUP_Q6ZIP_GENERAL	IPA_v3_0_GROUP_Q6ZIP
#define IPA_v3_0_GROUP_UC_RX_Q		(5)
#define IPA_v3_0_GROUP_Q6ZIP_ENGINE	IPA_v3_0_GROUP_UC_RX_Q
#define IPA_v3_0_GROUP_MAX		(6)

#define IPA_v3_5_GROUP_LWA_DL		(0) /* currently not used */
#define IPA_v3_5_MHI_GROUP_PCIE	IPA_v3_5_GROUP_LWA_DL
#define IPA_v3_5_GROUP_UL_DL		(1)
#define IPA_v3_5_MHI_GROUP_DDR		IPA_v3_5_GROUP_UL_DL
#define IPA_v3_5_MHI_GROUP_DMA		(2)
#define IPA_v3_5_GROUP_UC_RX_Q		(3) /* currently not used */
#define IPA_v3_5_SRC_GROUP_MAX		(4)
#define IPA_v3_5_DST_GROUP_MAX		(3)

#define IPA_v4_0_GROUP_LWA_DL		(0)
#define IPA_v4_0_MHI_GROUP_PCIE		(0)
#define IPA_v4_0_ETHERNET		(0)
#define IPA_v4_0_GROUP_UL_DL		(1)
#define IPA_v4_0_MHI_GROUP_DDR		(1)
#define IPA_v4_0_MHI_GROUP_DMA		(2)
#define IPA_v4_0_GROUP_UC_RX_Q		(3)
#define IPA_v4_0_SRC_GROUP_MAX		(4)
#define IPA_v4_0_DST_GROUP_MAX		(4)

#define IPA_v4_2_GROUP_UL_DL		(0)
#define IPA_v4_2_SRC_GROUP_MAX		(1)
#define IPA_v4_2_DST_GROUP_MAX		(1)

#define IPA_v4_5_MHI_GROUP_PCIE		(0)
#define IPA_v4_5_GROUP_UL_DL		(1)
#define IPA_v4_5_MHI_GROUP_DDR		(1)
#define IPA_v4_5_MHI_GROUP_DMA		(2)
#define IPA_v4_5_GROUP_CV2X			(2)
#define IPA_v4_5_MHI_GROUP_QDSS		(3)
#define IPA_v4_5_GROUP_UC_RX_Q		(4)
#define IPA_v4_5_SRC_GROUP_MAX		(5)
#define IPA_v4_5_DST_GROUP_MAX		(5)

#define IPA_v4_7_GROUP_UL_DL		(0)
#define IPA_v4_7_SRC_GROUP_MAX		(1)
#define IPA_v4_7_DST_GROUP_MAX		(1)

#define IPA_v4_9_GROUP_UL_DL		(0)
#define IPA_v4_9_GROUP_DMA		(1)
#define IPA_v4_9_GROUP_UC_RX		(2)
#define IPA_v4_9_GROUP_DRB_IP		(3)
#define IPA_v4_9_SRC_GROUP_MAX		(3)
#define IPA_v4_9_DST_GROUP_MAX		(4)

#define IPA_v4_11_GROUP_UL_DL		(0)
#define IPA_v4_11_GROUP_NOT_USE		(1)
#define IPA_v4_11_GROUP_DRB_IP		(2)
#define IPA_v4_11_SRC_GROUP_MAX		(3)
#define IPA_v4_11_DST_GROUP_MAX		(3)

#define IPA_v5_0_GROUP_UL			(0)
#define IPA_v5_0_GROUP_DL			(1)
#define IPA_v5_0_GROUP_DMA			(2)
#define IPA_v5_0_GROUP_QDSS			(3)
#define IPA_v5_0_GROUP_URLLC		(4)
#define IPA_v5_0_GROUP_CV2X			(4)
#define IPA_v5_0_GROUP_UC			(5)
#define IPA_v5_0_GROUP_DRB_IP		(6)
#define IPA_v5_0_SRC_GROUP_MAX		(6)
#define IPA_v5_0_DST_GROUP_MAX		(7)
#define IPA_v5_0_GROUP_MAX			(7)

#define IPA_v5_2_GROUP_UL		(0)
#define IPA_v5_2_GROUP_DL		(1)
#define IPA_v5_2_GROUP_URLLC		(2)
#define IPA_v5_2_GROUP_DRB_IP		(3)
#define IPA_v5_2_SRC_GROUP_MAX		(3)
#define IPA_v5_2_DST_GROUP_MAX		(4)

#define IPA_v5_5_GROUP_UL			(0)
#define IPA_v5_5_GROUP_DL			(1)
#define IPA_v5_5_GROUP_DMA			(2)
#define IPA_v5_5_GROUP_QDSS			(3)
#define IPA_v5_5_GROUP_URLLC		(4)
#define IPA_v5_5_GROUP_CV2X			(4)
#define IPA_v5_5_GROUP_UC			(5)
#define IPA_v5_5_GROUP_DRB_IP		(6)
#define IPA_v5_5_SRC_GROUP_MAX		(6)
#define IPA_v5_5_DST_GROUP_MAX		(7)
#define IPA_v5_5_GROUP_MAX			(7)

#define IPA_GROUP_MAX IPA_v5_5_GROUP_MAX

enum ipa_rsrc_grp_type_src {
	IPA_v3_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS,
	IPA_v3_0_RSRC_GRP_TYPE_SRC_HDR_SECTORS,
	IPA_v3_0_RSRC_GRP_TYPE_SRC_HDRI1_BUFFER,
	IPA_v3_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS,
	IPA_v3_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF,
	IPA_v3_0_RSRC_GRP_TYPE_SRC_HDRI2_BUFFERS,
	IPA_v3_0_RSRC_GRP_TYPE_SRC_HPS_DMARS,
	IPA_v3_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES,
	IPA_v3_0_RSRC_GRP_TYPE_SRC_MAX,

	IPA_v3_5_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS = 0,
	IPA_v3_5_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS,
	IPA_v3_5_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF,
	IPA_v3_5_RSRC_GRP_TYPE_SRC_HPS_DMARS,
	IPA_v3_5_RSRC_GRP_TYPE_SRC_ACK_ENTRIES,
	IPA_v3_5_RSRC_GRP_TYPE_SRC_MAX,

	IPA_v4_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS = 0,
	IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS,
	IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF,
	IPA_v4_0_RSRC_GRP_TYPE_SRC_HPS_DMARS,
	IPA_v4_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES,
	IPA_v4_0_RSRC_GRP_TYPE_SRC_MAX,

	IPA_v5_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS = 0,
	IPA_v5_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS,
	IPA_v5_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF,
	IPA_v5_0_RSRC_GRP_TYPE_SRC_HPS_DMARS,
	IPA_v5_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES,
	IPA_v5_0_RSRC_GRP_TYPE_SRC_MAX
};

#define IPA_RSRC_GRP_TYPE_SRC_MAX IPA_v3_0_RSRC_GRP_TYPE_SRC_MAX

enum ipa_rsrc_grp_type_dst {
	IPA_v3_0_RSRC_GRP_TYPE_DST_DATA_SECTORS,
	IPA_v3_0_RSRC_GRP_TYPE_DST_DATA_SECTOR_LISTS,
	IPA_v3_0_RSRC_GRP_TYPE_DST_DPS_DMARS,
	IPA_v3_0_RSRC_GRP_TYPE_DST_MAX,

	IPA_v3_5_RSRC_GRP_TYPE_DST_DATA_SECTORS = 0,
	IPA_v3_5_RSRC_GRP_TYPE_DST_DPS_DMARS,
	IPA_v3_5_RSRC_GRP_TYPE_DST_MAX,

	IPA_v4_0_RSRC_GRP_TYPE_DST_DATA_SECTORS = 0,
	IPA_v4_0_RSRC_GRP_TYPE_DST_DPS_DMARS,
	IPA_v4_0_RSRC_GRP_TYPE_DST_MAX,

	IPA_v5_0_RSRC_GRP_TYPE_DST_DATA_SECTORS = 0,
	IPA_v5_0_RSRC_GRP_TYPE_DST_DPS_DMARS,
	IPA_v5_0_RSRC_GRP_TYPE_DST_ULSO_SEGMENTS,
	IPA_v5_0_RSRC_GRP_TYPE_DST_MAX
};
#define IPA_RSRC_GRP_TYPE_DST_MAX IPA_v3_0_RSRC_GRP_TYPE_DST_MAX

enum ipa_rsrc_grp_type_rx {
	IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ,
	IPA_RSRC_GRP_TYPE_RX_MAX
};

enum ipa_rsrc_grp_rx_hps_weight_config {
	IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG,
	IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_MAX
};

struct rsrc_min_max {
	u32 min;
	u32 max;
};

struct ipa_rsrc_cfg {
	u8 src_grp_index;
	bool src_grp_valid;
	u8 dst_pipe_index;
	bool dst_pipe_valid;
	u8 dst_grp_index;
	bool dst_grp_valid;
	u8 src_grp_2nd_prio_index;
	bool src_grp_2nd_prio_valid;
};

enum ipa_ver {
	IPA_3_0,
	IPA_3_5,
	IPA_3_5_MHI,
	IPA_3_5_1,
	IPA_4_0,
	IPA_4_0_MHI,
	IPA_4_1,
	IPA_4_1_APQ,
	IPA_4_2,
	IPA_4_5,
	IPA_4_5_MHI,
	IPA_4_5_APQ,
	IPA_4_5_AUTO,
	IPA_4_5_AUTO_MHI,
	IPA_4_7,
	IPA_4_9,
	IPA_4_11,
	IPA_5_0,
	IPA_5_0_MHI,
	IPA_5_1,
	IPA_5_1_APQ,
	IPA_5_2,
	IPA_5_5,
	IPA_5_5_XR,
	IPA_VER_MAX,
};


static const struct rsrc_min_max ipa3_rsrc_src_grp_config
	[IPA_VER_MAX][IPA_RSRC_GRP_TYPE_SRC_MAX][IPA_GROUP_MAX] = {
	[IPA_3_0] = {
		/* UL	DL	DIAG	DMA	Not Used	uC Rx */
		[IPA_v3_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{3, 255}, {3, 255}, {1, 255}, {1, 255}, {1, 255}, {2, 255} },
		[IPA_v3_0_RSRC_GRP_TYPE_SRC_HDR_SECTORS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255} },
		[IPA_v3_0_RSRC_GRP_TYPE_SRC_HDRI1_BUFFER] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255} },
		[IPA_v3_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{14, 14}, {16, 16}, {5, 5}, {5, 5},  {0, 0}, {8, 8} },
		[IPA_v3_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{19, 19}, {26, 26}, {3, 3}, {7, 7}, {0, 0}, {8, 8} },
		[IPA_v3_0_RSRC_GRP_TYPE_SRC_HDRI2_BUFFERS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255} },
		[IPA_v3_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255} },
		[IPA_v3_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{14, 14}, {16, 16}, {5, 5}, {5, 5}, {0, 0}, {8, 8} },
	},
	[IPA_3_5] = {
		/* LWA_DL  UL_DL    unused  UC_RX_Q, other are invalid */
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{0, 0}, {1, 255}, {0, 0}, {1, 255}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{0, 0}, {10, 10}, {0, 0}, {8, 8}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{0, 0}, {14, 14}, {0, 0}, {8, 8}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255},  {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{0, 0}, {20, 20}, {0, 0}, {14, 14}, {0, 0}, {0, 0} },
	},
	[IPA_3_5_MHI] = {
		/* PCIE  DDR     DMA  unused, other are invalid */
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{4, 4}, {5, 5}, {1, 1}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{10, 10}, {10, 10}, {8, 8}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{12, 12}, {12, 12}, {8, 8}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255},  {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{14, 14}, {14, 14}, {14, 14}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_3_5_1] = {
		/* LWA_DL  UL_DL    unused  UC_RX_Q, other are invalid */
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{1, 255}, {1, 255}, {0, 0}, {1, 255}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{10, 10}, {10, 10}, {0, 0}, {8, 8}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{12, 12}, {14, 14}, {0, 0}, {8, 8}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255},  {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{14, 14}, {20, 20}, {0, 0}, {14, 14}, {0, 0}, {0, 0} },
	},
	[IPA_4_0] = {
		/* LWA_DL  UL_DL    unused  UC_RX_Q, other are invalid */
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{1, 255}, {1, 255}, {0, 0}, {1, 255}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{10, 10}, {10, 10}, {0, 0}, {8, 8}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{12, 12}, {14, 14}, {0, 0}, {8, 8}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255},  {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{14, 14}, {20, 20}, {0, 0}, {14, 14}, {0, 0}, {0, 0} },
	},
	[IPA_4_0_MHI] = {
		/* PCIE  DDR     DMA  unused, other are invalid */
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{4, 4}, {5, 5}, {1, 1}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{10, 10}, {10, 10}, {8, 8}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{12, 12}, {12, 12}, {8, 8}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255},  {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{14, 14}, {14, 14}, {14, 14}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_4_1] = {
		/* LWA_DL  UL_DL    unused  UC_RX_Q, other are invalid */
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{1, 63}, {1, 63}, {0, 0}, {1, 63}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{10, 10}, {10, 10}, {0, 0}, {8, 8}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{12, 12}, {14, 14}, {0, 0}, {8, 8}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 63}, {0, 63}, {0, 63}, {0, 63},  {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{14, 14}, {20, 20}, {0, 0}, {14, 14}, {0, 0}, {0, 0} },
	},
	[IPA_4_2] = {
		/* UL_DL   other are invalid */
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{3, 63}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{3, 3}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{10, 10}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{1, 1}, {0, 0}, {0, 0},  {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{5, 5}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_4_5] = {
		/* unused  UL_DL  unused  unused  UC_RX_Q N/A */
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{0, 0}, {1, 11}, {0, 0}, {0, 0}, {1, 63}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{0, 0}, {14, 14}, {0, 0}, {0, 0}, {3, 3}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{0, 0}, {18, 18}, {0, 0}, {0, 0}, {8, 8}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 63}, {0, 63}, {0, 63}, {0, 63},  {0, 63}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{0, 0}, {24, 24}, {0, 0}, {0, 0}, {8, 8}, {0, 0} },
	},
	[IPA_4_5_MHI] = {
		/* PCIE  DDR  DMA  QDSS  unused  N/A */
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{3, 8}, {4, 11}, {1, 6}, {1, 1}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{9, 9}, {12, 12}, {2, 2}, {2, 2}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{9, 9}, {14, 14}, {4, 4}, {4, 4}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 63}, {0, 63}, {0, 63}, {0, 63},  {0, 63}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{22, 22}, {16, 16}, {6, 6}, {2, 2}, {0, 0}, {0, 0} },
	},
	[IPA_4_5_APQ] = {
		/* unused  UL_DL  unused  unused  UC_RX_Q N/A */
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{0, 0}, {1, 11}, {0, 0}, {0, 0}, {1, 63}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{0, 0}, {14, 14}, {0, 0}, {0, 0}, {3, 3}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{0, 0}, {18, 18}, {0, 0}, {0, 0}, {8, 8}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 63}, {0, 63}, {0, 63}, {0, 63},  {0, 63}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{0, 0}, {24, 24}, {0, 0}, {0, 0}, {8, 8}, {0, 0} },
	},
	[IPA_4_5_AUTO] = {
		/* unused  UL_DL  DMA/CV2X  unused  UC_RX_Q N/A */
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{0, 0}, {1, 11}, {1, 1}, {0, 0}, {1, 63}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{0, 0}, {14, 14}, {2, 2}, {0, 0}, {3, 3}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{0, 0}, {18, 18}, {4, 4}, {0, 0}, {8, 8}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 63}, {0, 63}, {0, 63}, {0, 63},  {0, 63}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{0, 0}, {24, 24}, {6, 6}, {0, 0}, {8, 8}, {0, 0} },
	},
	[IPA_4_5_AUTO_MHI] = {
		/* PCIE  DDR  DMA/CV2X  QDSS  unused  N/A */
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{3, 8}, {4, 11}, {1, 6}, {1, 1}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{9, 9}, {12, 12}, {2, 2}, {2, 2}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{9, 9}, {14, 14}, {4, 4}, {4, 4}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 63}, {0, 63}, {0, 63}, {0, 63},  {0, 63}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{22, 22}, {16, 16}, {6, 6}, {2, 2}, {0, 0}, {0, 0} },
	},
	[IPA_4_7] = {
		/* UL_DL   other are invalid */
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{8, 8}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{8, 8}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{18, 18}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{2, 2}, {0, 0}, {0, 0},  {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{15, 15}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_4_9] = {
		/* UL_DL  DMA  UC_RX_Q  unused  unused  N/A */
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{1, 12}, {1, 1}, {1, 12}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{20, 20}, {2, 2}, {3, 3}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{38, 38}, {4, 4}, {8, 8}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 4}, {0, 4}, {0, 4},  {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{30, 30}, {8, 8}, {8, 8}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_4_11] = {
		/* UL_DL   other are invalid */
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{6, 6}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{8, 8}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{18, 18}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{2, 2}, {0, 0}, {0, 0},  {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{15, 15}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_5_0] = {
		/* UL  DL  unused  unused  URLLC UC_RX_Q N/A */
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{3, 9}, {4, 10}, {0, 0}, {0, 0}, {1, 63}, {0, 63}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{9, 9}, {12, 12}, {0, 0}, {0, 0}, {10, 10}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{9, 9}, {24, 24}, {0, 0}, {0, 0}, {20, 20}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 63}, {0, 63}, {0, 63}, {0, 63}, {1, 63}, {0, 63}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{22, 22}, {16, 16}, {0, 0}, {0, 0}, {16, 16}, {0, 0}, {0, 0},  },
	},
	[IPA_5_0_MHI] = {
		/* UL  DL  unused  unused  URLLC UC_RX_Q N/A */
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{3, 9}, {4, 10}, {1, 1}, {1, 1}, {1, 63}, {0, 63}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{9, 9}, {12, 12}, {2, 2}, {2, 2}, {10, 10}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{9, 9}, {24, 24}, {4, 4}, {4, 4}, {20, 20}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 63}, {0, 63}, {0, 63}, {0, 63}, {1, 63}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{22, 22}, {16, 16}, {6, 6}, {2, 2}, {16, 16}, {0, 0}, {0, 0},  },
	},

	[IPA_5_1] = {
		/* UL  DL  unused  unused  URLLC UC_RX_Q N/A */
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{7, 12}, {0, 0}, {0, 0}, {0, 0}, {1, 63}, {0, 63}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{21, 21}, {0, 0}, {0, 0}, {0, 0}, {10, 10}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{33, 33}, {0, 0}, {0, 0}, {0, 0}, {20, 20}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 63}, {0, 0}, {0, 63}, {0, 63}, {1, 63}, {0, 63}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{38, 38}, {0, 0}, {0, 0}, {0, 0}, {16, 16}, {0, 0}, {0, 0},  },
	},

	[IPA_5_1_APQ] = {
		/* UL  DL  unused  unused  URLLC UC_RX_Q N/A */
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{3, 9}, {4, 10}, {0, 0}, {0, 0}, {1, 63}, {0, 63}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{9, 9}, {12, 12}, {0, 0}, {0, 0}, {10, 10}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{9, 9}, {24, 24}, {0, 0}, {0, 0}, {20, 20}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 63}, {0, 63}, {0, 63}, {0, 63}, {1, 63}, {0, 63}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{22, 22}, {16, 16}, {0, 0}, {0, 0}, {16, 16}, {0, 0}, {0, 0},  },
	},

	[IPA_5_2] = {
		/* what does above comment mean. */
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{1, 7}, {1, 7}, {0, 5}, {0, 0}, {0, 0}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{8, 8}, {8, 8}, {8, 8}, {0, 0}, {0, 0}, {0, 0}, {0, 0}  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{10, 10}, {12, 12}, {12, 12}, {0, 0}, {0, 0}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 63}, {0, 63}, {0, 63}, {0, 0}, {0, 0}, {0, 0}, {0, 0}  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{15, 15}, {15, 15}, {12, 12}, {0, 0}, {0, 0}, {0, 0}, {0, 0}  },
	},

	[IPA_5_5] = {
		/* UL  DL  unused  unused  URLLC UC_RX_Q N/A */
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{3, 9}, {4, 10}, {0, 0}, {0, 0}, {1, 63}, {0, 63}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{9, 9}, {12, 12}, {0, 0}, {0, 0}, {10, 10}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{9, 9}, {24, 24}, {0, 0}, {0, 0}, {20, 20}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 63}, {0, 63}, {0, 63}, {0, 63}, {1, 63}, {0, 63}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{22, 22}, {16, 16}, {0, 0}, {0, 0}, {16, 16}, {0, 0}, {0, 0},  },
	},

	[IPA_5_5_XR] = {
		/* UL  DL  DMA  QDSS  URLLC UC_RX_Q N/A */
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_PKT_CONTEXTS] = {
		{3, 9}, {4, 10}, {0, 0}, {0, 0}, {3, 0x3f}, {0, 0x3f}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_LISTS] = {
		{9, 9}, {12, 12}, {0, 0}, {0, 0}, {10, 10}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_DESCRIPTOR_BUFF] = {
		{9, 9}, {24, 24}, {0, 0}, {0, 0}, {20, 20}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_HPS_DMARS] = {
		{0, 0x3f}, {0, 0x3f}, {0, 0x3f}, {0, 0x3f}, {1, 0x3f}, {0, 0x3f}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_SRC_ACK_ENTRIES] = {
		{22, 22}, {16, 16}, {0, 0}, {0, 0}, {16, 16}, {0, 0}, {0, 0},  },
	},
};

static const struct rsrc_min_max ipa3_rsrc_dst_grp_config
	[IPA_VER_MAX][IPA_RSRC_GRP_TYPE_DST_MAX][IPA_GROUP_MAX] = {
	[IPA_3_0] = {
		/* UL	DL/DPL	DIAG	DMA  Q6zip_gen Q6zip_eng */
		[IPA_v3_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{2, 2}, {3, 3}, {0, 0}, {2, 2}, {3, 3}, {3, 3} },
		[IPA_v3_0_RSRC_GRP_TYPE_DST_DATA_SECTOR_LISTS] = {
		{0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255}, {0, 255} },
		[IPA_v3_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {0, 0} },
	},
	[IPA_3_5] = {
		/* unused UL/DL/DPL unused N/A    N/A     N/A */
		[IPA_v3_5_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{4, 4}, {4, 4}, {3, 3}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{2, 255}, {1, 255}, {1, 2}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_3_5_MHI] = {
		/* PCIE  DDR     DMA     N/A     N/A     N/A */
		[IPA_v3_5_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{4, 4}, {4, 4}, {3, 3}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{2, 255}, {1, 255}, {1, 2}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_3_5_1] = {
		/* LWA_DL UL/DL/DPL unused N/A   N/A     N/A */
		[IPA_v3_5_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{4, 4}, {4, 4}, {3, 3}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v3_5_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{2, 255}, {1, 255}, {1, 2}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_4_0] = {
		/* LWA_DL UL/DL/DPL uC, other are invalid */
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{4, 4}, {4, 4}, {3, 3}, {2, 2}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{2, 255}, {1, 255}, {1, 2}, {0, 2}, {0, 0}, {0, 0} },
	},
	[IPA_4_0_MHI] = {
		/* LWA_DL UL/DL/DPL uC, other are invalid */
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{4, 4}, {4, 4}, {3, 3}, {2, 2}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{2, 255}, {1, 255}, {1, 2}, {0, 2}, {0, 0}, {0, 0} },
	},
	[IPA_4_1] = {
		/* LWA_DL UL/DL/DPL uC, other are invalid */
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{4, 4}, {4, 4}, {3, 3}, {2, 2}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{2, 63}, {1, 63}, {1, 2}, {0, 2}, {0, 0}, {0, 0} },
	},
	[IPA_4_2] = {
		/* UL/DL/DPL, other are invalid */
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{3, 3}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{1, 63}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_4_5] = {
		/* unused  UL/DL/DPL unused  unused  uC  N/A */
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{0, 0}, {16, 16}, {2, 2}, {2, 2}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{0, 0}, {2, 63}, {1, 2}, {1, 2}, {0, 2}, {0, 0} },
	},
	[IPA_4_5_MHI] = {
		/* PCIE/DPL  DDR  DMA/CV2X  QDSS  uC  N/A */
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{16, 16}, {5, 5}, {2, 2}, {2, 2}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{2, 63}, {1, 63}, {1, 2}, {1, 2}, {0, 2}, {0, 0} },
	},
	[IPA_4_5_APQ] = {
		/* unused  UL/DL/DPL unused  unused  uC  N/A */
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{0, 0}, {16, 16}, {2, 2}, {2, 2}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{0, 0}, {2, 63}, {1, 2}, {1, 2}, {0, 2}, {0, 0} },
	},
	[IPA_4_5_AUTO] = {
		/* unused  UL/DL/DPL DMA/CV2X  unused  uC  N/A */
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{0, 0}, {16, 16}, {2, 2}, {2, 2}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{0, 0}, {2, 63}, {1, 2}, {1, 2}, {0, 2}, {0, 0} },
	},
	[IPA_4_5_AUTO_MHI] = {
		/* PCIE/DPL  DDR  DMA/CV2X  QDSS  uC  N/A */
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{16, 16}, {5, 5}, {2, 2}, {2, 2}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{2, 63}, {1, 63}, {1, 2}, {1, 2}, {0, 2}, {0, 0} },
	},
	[IPA_4_7] = {
		/* UL/DL/DPL, other are invalid */
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{7, 7}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{2, 2}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_4_9] = {
		/*UL/DL/DPL DM  uC  DRB IP unused unused */
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{9, 9}, {1, 1}, {1, 1}, {39, 39}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{2, 3}, {1, 2}, {0, 2}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_4_11] = {
		/* UL/DL/DPL, other are invalid */
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{3,3}, {0, 0}, {25, 25}, {0, 0}, {0, 0}, {0, 0} },
		[IPA_v4_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{2, 2}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_5_0] = {
		/* UL  DL  unused  unused unused  UC_RX_Q DRBIP N/A */
		[IPA_v5_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{6, 6}, {5, 5}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {39, 39},  },
		[IPA_v5_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{0, 3}, {0, 3}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_DST_ULSO_SEGMENTS] = {
		{0, 0x3f}, {0, 0x3f}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},  },
	},
	[IPA_5_0_MHI] = {
		/* UL DL IPADMA QDSS unused unused CV2X */
		[IPA_v5_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{6, 6}, {5, 5}, {2, 2}, {2, 2}, {0, 0}, {0, 0}, {30, 39},  },
		[IPA_v5_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{0, 3}, {0, 3}, {1, 2}, {1, 1}, {0, 0}, {0, 0}, {0, 0},  },
	},

	[IPA_5_1] = {
		/* UL  DL  unused  unused unused  UC_RX_Q DRBIP N/A */
		[IPA_v5_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{6, 6}, {5, 5}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {39, 39},  },
		[IPA_v5_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{0, 3}, {0, 3}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_DST_ULSO_SEGMENTS] = {
		{0, 0x3f}, {0, 0x3f}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},  },
	},

	[IPA_5_1_APQ] = {
		/* UL  DL  unused  unused unused  UC_RX_Q DRBIP N/A */
		[IPA_v5_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{6, 6}, {5, 5}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {39, 39},  },
		[IPA_v5_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{0, 3}, {0, 3}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},  },
	},

	[IPA_5_2] = {
		/* UL  DL  unused  unused unused  UC_RX_Q DRBIP N/A */
		[IPA_v5_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{3, 3}, {3, 3}, {0, 0}, {23, 23}, {0, 0}, {0, 0}, {0, 0},  },

		[IPA_v5_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{1, 2}, {1, 2}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_DST_ULSO_SEGMENTS] = {
		{1, 63}, {1, 63}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},  },
	},

	[IPA_5_5] = {
		/* UL  DL  unused  unused unused  UC_RX_Q DRBIP N/A */
		[IPA_v5_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{6, 6}, {5, 5}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {39, 39},  },
		[IPA_v5_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{0, 3}, {0, 3}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_DST_ULSO_SEGMENTS] = {
		{0, 0x3f}, {0, 0x3f}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},  },
	},

	[IPA_5_5_XR] = {
		/* UL  DL  DMA  QDSS unused  UC_RX_Q DRBIP N/A */
		[IPA_v5_0_RSRC_GRP_TYPE_DST_DATA_SECTORS] = {
		{6, 6}, {6, 6}, {0, 0}, {0, 0}, {10, 10}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_DST_DPS_DMARS] = {
		{0, 3}, {0, 3}, {0, 0}, {0, 0}, {1, 3}, {0, 0}, {0, 0},  },
		[IPA_v5_0_RSRC_GRP_TYPE_DST_ULSO_SEGMENTS] = {
		{0, 0x3f}, {0, 0x3f}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},  },
	},
};

static const struct rsrc_min_max ipa3_rsrc_rx_grp_config
	[IPA_VER_MAX][IPA_RSRC_GRP_TYPE_RX_MAX][IPA_GROUP_MAX] = {
	[IPA_3_0] = {
		/* UL	DL	DIAG	DMA	unused	uC Rx */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{16, 16}, {24, 24}, {8, 8}, {8, 8}, {0, 0}, {8, 8} },
	},
	[IPA_3_5] = {
		/* unused UL_DL	unused UC_RX_Q   N/A     N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{0, 0}, {7, 7}, {0, 0}, {2, 2}, {0, 0}, {0, 0} },
	},
	[IPA_3_5_MHI] = {
		/* PCIE   DDR	 DMA   unused   N/A     N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {7, 7}, {2, 2}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_3_5_1] = {
		/* LWA_DL UL_DL	unused   UC_RX_Q N/A     N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {7, 7}, {0, 0}, {2, 2}, {0, 0}, {0, 0} },
	},
	[IPA_4_0] = {
		/* LWA_DL UL_DL	unused UC_RX_Q, other are invalid */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {7, 7}, {0, 0}, {2, 2}, {0, 0}, {0, 0} },
	},
	[IPA_4_0_MHI] = {
		/* PCIE   DDR	  DMA     unused   N/A     N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {7, 7}, {2, 2}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_4_1] = {
		/* LWA_DL UL_DL	unused UC_RX_Q, other are invalid */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {7, 7}, {0, 0}, {2, 2}, {0, 0}, {0, 0} },
	},
	[IPA_4_2] = {
		/* UL_DL, other are invalid */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{4, 4}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_4_5] = {
		/* unused  UL_DL  unused unused  UC_RX_Q  N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{0, 0}, {3, 3}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_4_5_MHI] = {
		/* PCIE  DDR  DMA  QDSS  unused  N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {3, 3}, {3, 3}, {3, 3}, {0, 0}, {0, 0} },
	},
	[IPA_4_5_APQ] = {
		/* unused  UL_DL  unused unused  UC_RX_Q  N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{0, 0}, {3, 3}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_4_5_AUTO] = {
		/* unused  UL_DL DMA/CV2X  unused  UC_RX_Q  N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{0, 0}, {3, 3}, {3, 3}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_4_5_AUTO_MHI] = {
		/* PCIE  DDR  DMA  QDSS  unused  N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{ 3, 3 }, {3, 3}, {3, 3}, {3, 3}, {0, 0}, { 0, 0 } },
	},
	[IPA_4_7] = {
		/* unused  UL_DL  unused unused  UC_RX_Q  N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_4_9] = {
		/* unused  UL_DL  unused unused  UC_RX_Q  N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {3, 3}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_4_11] = {
		/* unused  UL_DL  unused unused  UC_RX_Q  N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} },
	},
	[IPA_5_0] = {
		/* UL  DL  unused  unused  URLLC UC_RX_Q */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {3, 3}, {0, 0}, {0, 0}, {3, 3}, {0, 0}  },
	},
	[IPA_5_0_MHI] = {
		/* UL  DL  unused  unused  URLLC UC_RX_Q */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {0, 0}  },
	},

	[IPA_5_1] = {
		/* UL  DL  unused  unused  URLLC UC_RX_Q */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {3, 3}, {0, 0}, {0, 0}, {3, 3}, {0, 0}  },
	},

	[IPA_5_1_APQ] = {
		/* UL  DL  unused  unused  URLLC UC_RX_Q */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {3, 3}, {0, 0}, {0, 0}, {3, 3}, {0, 0}  },
	},
	[IPA_5_2] = {
		/* UL  DL  unused  unused  URLLC UC_RX_Q */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {3, 3}, {3, 3}, {0, 0}, {0, 0}, {0, 0}  },
	},

	[IPA_5_5] = {
		/* UL  DL  unused  unused  URLLC UC_RX_Q */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {3, 3}, {0, 0}, {0, 0}, {3, 3}, {0, 0}  },
	},

	[IPA_5_5_XR] = {
		/* UL  DL  DMA  QDSS  URLLC UC_RX_Q */
		[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ] = {
		{3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {0, 0}  },
	},
};

static const u32 ipa3_rsrc_rx_grp_hps_weight_config
	[IPA_VER_MAX][IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_MAX][IPA_GROUP_MAX] = {
	[IPA_3_0] = {
		/* UL	DL	DIAG	DMA	unused	uC Rx */
		[IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG] = { 0, 0, 0, 0, 0, 0 },
	},
	[IPA_3_5] = {
		/* unused UL_DL	unused UC_RX_Q   N/A     N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG] = { 1, 1, 1, 1, 0, 0 },
	},
	[IPA_3_5_MHI] = {
		/* PCIE   DDR	     DMA       unused   N/A        N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG] = { 3, 5, 1, 1, 0, 0 },
	},
	[IPA_3_5_1] = {
		/* LWA_DL UL_DL	unused   UC_RX_Q N/A     N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG] = { 1, 1, 1, 1, 0, 0 },
	},
	[IPA_4_0] = {
		/* LWA_DL UL_DL	unused UC_RX_Q N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG] = { 1, 1, 1, 1, 0, 0 },
	},
	[IPA_4_0_MHI] = {
		/* PCIE   DDR	     DMA       unused   N/A        N/A */
		[IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG] = { 3, 5, 1, 1, 0, 0 },
	},
	[IPA_4_1] = {
		/* LWA_DL UL_DL	unused UC_RX_Q, other are invalid */
		[IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG] = { 1, 1, 1, 1, 0, 0 },
	},
};

static const struct ipa_rsrc_cfg ipa_rsrc_config[IPA_VER_MAX] = {
	[IPA_5_0] = {
		.src_grp_index          = 4,
		.src_grp_valid          = 1,
		.dst_pipe_index         = 0,
		.dst_pipe_valid         = 0,
		.dst_grp_index          = 0,
		.dst_grp_valid          = 0,
		.src_grp_2nd_prio_index = 1,
		.src_grp_2nd_prio_valid = 1,
	},
	[IPA_5_1] = {
		.src_grp_index          = 4,
		.src_grp_valid          = 1,
		.dst_pipe_index         = 0,
		.dst_pipe_valid         = 0,
		.dst_grp_index          = 0,
		.dst_grp_valid          = 0,
		.src_grp_2nd_prio_index = 1,
		.src_grp_2nd_prio_valid = 1,
	},
	[IPA_5_2] = {
		.src_grp_index          = 2,
		.src_grp_valid          = 1,
		.dst_pipe_index         = 0,
		.dst_pipe_valid         = 0,
		.dst_grp_index          = 0,
		.dst_grp_valid          = 0,
		.src_grp_2nd_prio_index = 0,
		.src_grp_2nd_prio_valid = 0,
	},
};

enum ipa_qmb_instance_type {
	IPA_QMB_INSTANCE_DDR = 0,
	IPA_QMB_INSTANCE_PCIE = 1,
	IPA_QMB_INSTANCE_MAX
};

#define QMB_MASTER_SELECT_DDR IPA_QMB_INSTANCE_DDR
#define QMB_MASTER_SELECT_PCIE IPA_QMB_INSTANCE_PCIE

struct ipa_qmb_outstanding {
	u16 ot_reads;
	u16 ot_writes;
	u16 ot_read_beats;
};

/*TODO: Update correct values of max_read_beats for all targets*/

static const struct ipa_qmb_outstanding ipa3_qmb_outstanding
		[IPA_VER_MAX][IPA_QMB_INSTANCE_MAX] = {
	[IPA_3_0][IPA_QMB_INSTANCE_DDR]		= {8, 8, 0},
	[IPA_3_0][IPA_QMB_INSTANCE_PCIE]	= {8, 2, 0},
	[IPA_3_5][IPA_QMB_INSTANCE_DDR]		= {8, 8, 0},
	[IPA_3_5][IPA_QMB_INSTANCE_PCIE]	= {12, 4, 0},
	[IPA_3_5_MHI][IPA_QMB_INSTANCE_DDR]	= {8, 8, 0},
	[IPA_3_5_MHI][IPA_QMB_INSTANCE_PCIE]	= {12, 4, 0},
	[IPA_3_5_1][IPA_QMB_INSTANCE_DDR]	= {8, 8, 0},
	[IPA_3_5_1][IPA_QMB_INSTANCE_PCIE]	= {12, 4, 0},
	[IPA_4_0][IPA_QMB_INSTANCE_DDR]		= {12, 8, 120},
	[IPA_4_0][IPA_QMB_INSTANCE_PCIE]	= {12, 4, 0},
	[IPA_4_0_MHI][IPA_QMB_INSTANCE_DDR]	= {12, 8, 0},
	[IPA_4_0_MHI][IPA_QMB_INSTANCE_PCIE]	= {12, 4, 0},
	[IPA_4_1][IPA_QMB_INSTANCE_DDR]		= {12, 8, 120},
	[IPA_4_1][IPA_QMB_INSTANCE_PCIE]	= {12, 4, 0},
	[IPA_4_2][IPA_QMB_INSTANCE_DDR]		= {12, 8, 0},
	[IPA_4_5][IPA_QMB_INSTANCE_DDR]		= {16, 8, 120},
	[IPA_4_5][IPA_QMB_INSTANCE_PCIE]	= {12, 8, 0},
	[IPA_4_5_MHI][IPA_QMB_INSTANCE_DDR]	= {16, 8, 120},
	[IPA_4_5_MHI][IPA_QMB_INSTANCE_PCIE]	= {12, 8, 0},
	[IPA_4_5_APQ][IPA_QMB_INSTANCE_DDR]	= {16, 8, 120},
	[IPA_4_5_APQ][IPA_QMB_INSTANCE_PCIE]	= {12, 8, 0},
	[IPA_4_5_AUTO][IPA_QMB_INSTANCE_DDR]	= {16, 8, 0},
	[IPA_4_5_AUTO][IPA_QMB_INSTANCE_PCIE]	= {12, 8, 0},
	[IPA_4_5_AUTO_MHI][IPA_QMB_INSTANCE_DDR]	= {16, 8, 0},
	[IPA_4_5_AUTO_MHI][IPA_QMB_INSTANCE_PCIE]	= {12, 8, 0},
	[IPA_4_7][IPA_QMB_INSTANCE_DDR]	        = {13, 12, 120},
	[IPA_4_9][IPA_QMB_INSTANCE_DDR]	        = {16, 8, 120},
	[IPA_4_11][IPA_QMB_INSTANCE_DDR] = {13, 12, 120},
	[IPA_5_2][IPA_QMB_INSTANCE_DDR]		= {13, 13, 0},
	[IPA_5_5][IPA_QMB_INSTANCE_DDR]		= {16, 12, 0},
	[IPA_5_5][IPA_QMB_INSTANCE_PCIE]	= {16, 8, 0},
	[IPA_5_5_XR][IPA_QMB_INSTANCE_DDR]	= {16, 12, 0},
	[IPA_5_5_XR][IPA_QMB_INSTANCE_PCIE]	= {16, 8, 0},
};

enum ipa_tx_instance {
	IPA_TX_INSTANCE_UL = 0,
	IPA_TX_INSTANCE_DL = 1,
	IPA_TX_INSTANCE_NA = 0xff
};

struct ipa_ep_configuration {
	bool valid;
	int group_num;
	bool support_flt;
	int sequencer_type;
	u8 qmb_master_sel;
	struct ipa_gsi_ep_config ipa_gsi_ep_info;
	u8 tx_instance;
};

/* clients not included in the list below are considered as invalid */
static const struct ipa_ep_configuration ipa3_ep_mapping
					[IPA_VER_MAX][IPA_CLIENT_MAX] = {
	[IPA_3_0][IPA_CLIENT_WLAN1_PROD]          = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 10, 1, 8, 16, IPA_EE_UC }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_USB_PROD]            = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 3, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_APPS_LAN_PROD] = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 14, 11, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_APPS_WAN_PROD] = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 5, 16, 32, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v3_0_GROUP_IMM_CMD, false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 22, 6, 18, 28, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_ODU_PROD]            = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 12, 9, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_MHI_PROD]            = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_PCIE,
			{ 0, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_Q6_LAN_PROD]         = {
			true, IPA_v3_0_GROUP_UL, false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 4, 8, 12, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v3_0_GROUP_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 5, 0, 16, 32, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_Q6_CMD_PROD] = {
			true, IPA_v3_0_GROUP_IMM_CMD, false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 1, 18, 28, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_Q6_DECOMP_PROD]      = {
			true, IPA_v3_0_GROUP_Q6ZIP,
			false, IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 7, 2, 0, 0, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_Q6_DECOMP2_PROD]     = {
			true, IPA_v3_0_GROUP_Q6ZIP,
			false, IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 3, 0, 0, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_MEMCPY_DMA_SYNC_PROD] = {
			true, IPA_v3_0_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_PCIE,
			{ 12, 9, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD] = {
			true, IPA_v3_0_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_PCIE,
			{ 13, 10, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_ETHERNET_PROD]          = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{2, 0, 8, 16, IPA_EE_UC}, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	[IPA_3_0][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 3, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_TEST1_PROD]          = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 3, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 5, 16, 32, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 12, 9, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v3_0_GROUP_UL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 13, 10, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	[IPA_3_0][IPA_CLIENT_WLAN1_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 25, 4, 8, 8, IPA_EE_UC }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_WLAN2_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 27, 4, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_WLAN3_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 28, 13, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_WLAN4_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 29, 14, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 12, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v3_0_GROUP_DPL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 2, 8, 12, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v3_0_GROUP_UL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 7, 8, 12, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 8, 8, 12, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_ODU_EMB_CONS]        = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 23, 1, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_MHI_CONS]            = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 23, 1, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 6, 8, 12, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v3_0_GROUP_UL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 5, 8, 12, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_Q6_DUN_CONS]         = {
			true, IPA_v3_0_GROUP_DIAG, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 30, 7, 4, 4, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_Q6_DECOMP_CONS] = {
			true, IPA_v3_0_GROUP_Q6ZIP, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 8, 4, 4, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_Q6_DECOMP2_CONS] = {
			true, IPA_v3_0_GROUP_Q6ZIP, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 4, 9, 4, 4, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_MEMCPY_DMA_SYNC_CONS] = {
			true, IPA_v3_0_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 28, 13, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS] = {
			true, IPA_v3_0_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 29, 14, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_ETHERNET_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{24, 3, 8, 8, IPA_EE_UC}, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	[IPA_3_0][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 12, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_TEST1_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 12, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 27, 4, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 28, 13, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_0][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 29, 14, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_3_0][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v3_0_GROUP_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	/* IPA_3_5 */
	[IPA_3_5][IPA_CLIENT_WLAN1_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 1, 8, 16, IPA_EE_UC }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_USB_PROD]            = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 7, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_APPS_LAN_PROD]   = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 9, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_APPS_WAN_PROD] = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 3, 16, 32, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 5, 4, 20, 23, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_ODU_PROD]            = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_UC }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_Q6_LAN_PROD]         = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 0, 16, 32, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_Q6_CMD_PROD]	  = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 1, 20, 23, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	[IPA_3_5][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 7, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_TEST1_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 7, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{7, 8, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 9, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	[IPA_3_5][IPA_CLIENT_WLAN1_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 3, 8, 8, IPA_EE_UC }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_WLAN2_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 12, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_WLAN3_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 13, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 17, 11, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 10, 4, 6, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 9, 5, 8, 12, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 10, 6, 8, 12, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_ODU_EMB_CONS]        = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 1, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 3, 8, 12, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 2, 8, 12, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	/* MBIM aggregation test pipes should have the same QMB as USB_CONS */
	[IPA_3_5][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 15, 1, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_TEST1_CONS]           = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 1, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 17, 11, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 12, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 19, 13, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_3_5][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 31, 31, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	/* IPA_3_5_MHI */
	[IPA_3_5_MHI][IPA_CLIENT_USB_PROD]            = {
			false, IPA_EP_NOT_ALLOCATED, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ -1, -1, -1, -1, -1 }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_APPS_WAN_PROD]   = {
			true, IPA_v3_5_MHI_GROUP_DDR, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 3, 16, 32, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v3_5_MHI_GROUP_DDR, false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 5, 4, 20, 23, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_MHI_PROD]            = {
			true, IPA_v3_5_MHI_GROUP_PCIE, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_PCIE,
			{ 1, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_Q6_LAN_PROD]         = {
			true, IPA_v3_5_MHI_GROUP_DDR, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 0, 16, 32, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v3_5_MHI_GROUP_DDR, true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 4, 10, 30, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_Q6_CMD_PROD]	  = {
			true, IPA_v3_5_MHI_GROUP_PCIE, false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 1, 20, 23, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_MEMCPY_DMA_SYNC_PROD] = {
			true, IPA_v3_5_MHI_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 7, 8, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD] = {
			true, IPA_v3_5_MHI_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 8, 9, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	[IPA_3_5_MHI][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v3_5_MHI_GROUP_DDR, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 7, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_TEST1_PROD]          = {
			0, IPA_v3_5_MHI_GROUP_DDR, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 7, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v3_5_MHI_GROUP_PCIE, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_PCIE,
			{ 1, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v3_5_MHI_GROUP_DMA, true,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 7, 8, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v3_5_MHI_GROUP_DMA, true,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 8, 9, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	[IPA_3_5_MHI][IPA_CLIENT_WLAN1_CONS]          = {
			true, IPA_v3_5_MHI_GROUP_DDR, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 3, 8, 8, IPA_EE_UC }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_USB_CONS]            = {
			false, IPA_EP_NOT_ALLOCATED, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ -1, -1, -1, -1, -1 }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_USB_DPL_CONS]        = {
			false, IPA_EP_NOT_ALLOCATED, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ -1, -1, -1, -1, -1 }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v3_5_MHI_GROUP_DDR, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 9, 5, 8, 12, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v3_5_MHI_GROUP_DDR, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 10, 6, 8, 12, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_MHI_CONS]            = {
			true, IPA_v3_5_MHI_GROUP_PCIE, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 15, 1, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v3_5_MHI_GROUP_DDR, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 3, 8, 12, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v3_5_MHI_GROUP_DDR, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 2, 8, 12, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_MEMCPY_DMA_SYNC_CONS] = {
			true, IPA_v3_5_MHI_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 18, 12, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS] = {
			true, IPA_v3_5_MHI_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 19, 13, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	[IPA_3_5_MHI][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v3_5_MHI_GROUP_PCIE, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 15, 1, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_TEST1_CONS]           = {
			true, IPA_v3_5_MHI_GROUP_PCIE, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 15, 1, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v3_5_MHI_GROUP_DDR, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 11, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v3_5_MHI_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 18, 12, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_MHI][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v3_5_MHI_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 19, 13, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_3_5_MHI][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v3_5_MHI_GROUP_DMA, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 31, 31, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	/* IPA_3_5_1 */
	[IPA_3_5_1][IPA_CLIENT_WLAN1_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_REP_SEQ_TYPE_2PKT_PROC_PASS_NO_DEC_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 7, 1, 8, 16, IPA_EE_UC }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_USB_PROD]            = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_REP_SEQ_TYPE_2PKT_PROC_PASS_NO_DEC_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_APPS_LAN_PROD] = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 7, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_APPS_WAN_PROD] = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_REP_SEQ_TYPE_2PKT_PROC_PASS_NO_DEC_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 3, 16, 32, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_APPS_CMD_PROD]		= {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 5, 4, 20, 23, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_Q6_LAN_PROD]         = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 0, 16, 32, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 4, 12, 30, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_Q6_CMD_PROD]	    = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 1, 20, 23, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	[IPA_3_5_1][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_TEST1_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 3, 16, 32, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 1, 20, 23, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v3_5_GROUP_UL_DL, true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_UC }, IPA_TX_INSTANCE_NA },

	[IPA_3_5_1][IPA_CLIENT_WLAN1_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 11, 8, 8, IPA_EE_UC }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_WLAN2_CONS]          =  {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 9, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_WLAN3_CONS]          =  {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 10, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 8, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 2, 4, 6, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 9, 5, 8, 12, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 10, 6, 8, 12, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 3, 8, 12, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v3_5_GROUP_UL_DL, false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 2, 8, 12, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	[IPA_3_5_1][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v3_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 8, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_TEST1_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 8, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 9, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 10, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_3_5_1][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 2, 4, 6, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_3_5_1][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v3_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	/* IPA_4_0 */
	[IPA_4_0][IPA_CLIENT_WLAN1_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_REP_SEQ_TYPE_2PKT_PROC_PASS_NO_DEC_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 2, 8, 16, IPA_EE_UC }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_USB_PROD]            = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_REP_SEQ_TYPE_2PKT_PROC_PASS_NO_DEC_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 8, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_APPS_LAN_PROD]   = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 10, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_APPS_WAN_PROD] = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_REP_SEQ_TYPE_2PKT_PROC_PASS_NO_DEC_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 3, 16, 32, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 5, 4, 20, 24, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_ODU_PROD]            = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_REP_SEQ_TYPE_2PKT_PROC_PASS_NO_DEC_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_ETHERNET_PROD]	  = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_REP_SEQ_TYPE_2PKT_PROC_PASS_NO_DEC_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 0, 8, 16, IPA_EE_UC }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 0, 16, 32, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_Q6_CMD_PROD]	  = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 1, 20, 24, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	[IPA_4_0][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 8, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_TEST1_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 8, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 7, 9, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{8, 10, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },


	[IPA_4_0][IPA_CLIENT_WLAN1_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 3, 6, 9, IPA_EE_UC }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_WLAN2_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 13, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_WLAN3_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 14, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 12, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 7, 5, 5, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 10, 5, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 6, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_ODU_EMB_CONS]        = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 1, 17, 17, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_ETHERNET_CONS]	  = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 22, 1, 17, 17, IPA_EE_UC }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 4, 9, 9, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 3, 9, 9, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_Q6_LTE_WIFI_AGGR_CONS] = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 5, 9, 9, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	/* MBIM aggregation test pipes should have the same QMB as USB_CONS */
	[IPA_4_0][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 6, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_TEST1_CONS]           = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 6, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 2, 5, 5, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 12, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 14, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_4_0][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	/* IPA_4_0_MHI */
	[IPA_4_0_MHI][IPA_CLIENT_APPS_WAN_PROD]   = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 3, 16, 32, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 5, 4, 20, 24, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_MHI_PROD]            = {
			true, IPA_v4_0_MHI_GROUP_PCIE,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_PCIE,
			{ 1, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 0, 16, 32, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_Q6_CMD_PROD]	  = {
			true, IPA_v4_0_MHI_GROUP_PCIE,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 1, 20, 24, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_MEMCPY_DMA_SYNC_PROD] = {
			true, IPA_v4_0_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 7, 9, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD] = {
			true, IPA_v4_0_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 8, 10, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	[IPA_4_0_MHI][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 8, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0][IPA_CLIENT_TEST1_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 8, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 7, 9, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 10, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 10, 5, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 6, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_MHI_CONS]            = {
			true, IPA_v4_0_MHI_GROUP_PCIE,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 17, 1, 17, 17, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 4, 9, 9, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 3, 9, 9, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_MEMCPY_DMA_SYNC_CONS] = {
			true, IPA_v4_0_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 20, 13, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS] = {
			true, IPA_v4_0_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 21, 14, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_Q6_LTE_WIFI_AGGR_CONS] = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 5, 9, 9, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v4_0_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 7, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_MHI_DPL_CONS]        = {
			true, IPA_v4_0_MHI_GROUP_PCIE,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 12, 2, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	[IPA_4_0_MHI][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 11, 6, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_TEST1_CONS]           = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 11, 6, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 2, 5, 5, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 19, 12, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_0_MHI][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 21, 14, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_4_0_MHI][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	/* IPA_4_1 */
	[IPA_4_1][IPA_CLIENT_WLAN1_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_REP_SEQ_TYPE_2PKT_PROC_PASS_NO_DEC_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 2, 8, 16, IPA_EE_UC }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_WLAN2_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_REP_SEQ_TYPE_2PKT_PROC_PASS_NO_DEC_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 7, 9, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_USB_PROD]            = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_REP_SEQ_TYPE_2PKT_PROC_PASS_NO_DEC_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 8, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_APPS_LAN_PROD]   = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 10, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_APPS_WAN_PROD] = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_REP_SEQ_TYPE_2PKT_PROC_PASS_NO_DEC_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 3, 16, 32, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 5, 4, 20, 24, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_ODU_PROD]            = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_REP_SEQ_TYPE_2PKT_PROC_PASS_NO_DEC_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_ETHERNET_PROD] = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_REP_SEQ_TYPE_2PKT_PROC_PASS_NO_DEC_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 0, 8, 16, IPA_EE_UC }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 0, 16, 32, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_Q6_CMD_PROD]	  = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 1, 20, 24, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	[IPA_4_1][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 8, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_TEST1_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 8, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{7, 9, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 10, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },


	[IPA_4_1][IPA_CLIENT_WLAN1_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 3, 9, 9, IPA_EE_UC }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_WLAN2_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 1, 8, 13, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_WLAN3_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 14, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 12, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 7, 5, 5, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 10, 5, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 6, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_ODL_DPL_CONS]        = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 2, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_ETHERNET_CONS]	  = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 22, 1, 9, 9, IPA_EE_UC }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 4, 9, 9, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 3, 9, 9, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_Q6_LTE_WIFI_AGGR_CONS] = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 5, 9, 9, IPA_EE_Q6 }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	/* MBIM aggregation test pipes should have the same QMB as USB_CONS */
	[IPA_4_1][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 6, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_TEST1_CONS]           = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 6, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 2, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 12, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 14, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_4_1][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	/* MHI PRIME PIPES - Client producer / IPA Consumer pipes */
	[IPA_4_1_APQ][IPA_CLIENT_MHI_PRIME_DPL_PROD] = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{7, 9, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1_APQ][IPA_CLIENT_MHI_PRIME_TETH_PROD] = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1_APQ][IPA_CLIENT_MHI_PRIME_RMNET_PROD] = {
			true, IPA_v4_0_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 2, 3, 16, 32, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	/* MHI PRIME PIPES - Client Consumer / IPA Producer pipes */
	[IPA_4_1_APQ][IPA_CLIENT_MHI_PRIME_TETH_CONS] = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 13, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_1_APQ][IPA_CLIENT_MHI_PRIME_RMNET_CONS] = {
			true, IPA_v4_0_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 14, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	/* IPA_4_2 */
	[IPA_4_2][IPA_CLIENT_WLAN1_PROD]          = {
			true, IPA_v4_2_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_REP_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 7, 6, 7, IPA_EE_AP, GSI_USE_PREFETCH_BUFS}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_USB_PROD]            = {
			true, IPA_v4_2_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_REP_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 5, 8, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_APPS_LAN_PROD]   = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 6, 8, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_APPS_WAN_PROD] = {
			true, IPA_v4_2_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_REP_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP_DMAP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 12, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 6, 1, 20, 20, IPA_EE_AP, GSI_USE_PREFETCH_BUFS}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v4_2_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 0, 8, 12, IPA_EE_Q6, GSI_USE_PREFETCH_BUFS}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_Q6_CMD_PROD]	  = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 5, 1, 20, 20, IPA_EE_Q6, GSI_USE_PREFETCH_BUFS}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_ETHERNET_PROD] = {
			true, IPA_v4_2_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 7, 0, 8, 10, IPA_EE_UC, GSI_USE_PREFETCH_BUFS}, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	[IPA_4_2][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v4_2_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 5, 8, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_TEST1_PROD]          = {
			true, IPA_v4_2_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 5, 8, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v4_2_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 7, 6, 7, IPA_EE_AP, GSI_USE_PREFETCH_BUFS}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v4_2_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP,
			QMB_MASTER_SELECT_DDR,
			{1, 0, 8, 12, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v4_2_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 7, 0, 8, 10, IPA_EE_AP, GSI_USE_PREFETCH_BUFS}, IPA_TX_INSTANCE_NA },


	[IPA_4_2][IPA_CLIENT_WLAN1_CONS]          = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 8, 6, 9, IPA_EE_AP, GSI_USE_PREFETCH_BUFS}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 9, 6, 6, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 4, 4, 4, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 8, 2, 6, 6, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 9, 3, 6, 6, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 3, 6, 6, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 10, 2, 6, 6, IPA_EE_Q6,  GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_Q6_LTE_WIFI_AGGR_CONS] = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 4, 6, 6, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_ETHERNET_CONS] = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 1, 6, 6, IPA_EE_UC, GSI_USE_PREFETCH_BUFS}, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	/* MBIM aggregation test pipes should have the same QMB as USB_CONS */
	[IPA_4_2][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 9, 6, 6, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_TEST1_CONS]           = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 9, 6, 6, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 4, 4, 4, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 8, 6, 9, IPA_EE_AP, GSI_USE_PREFETCH_BUFS}, IPA_TX_INSTANCE_NA },
	[IPA_4_2][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 9, 3, 6, 6, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY}, IPA_TX_INSTANCE_NA },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_4_2][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v4_2_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP, GSI_USE_PREFETCH_BUFS}, IPA_TX_INSTANCE_NA },

	/* IPA_4_5 */
	[IPA_4_5][IPA_CLIENT_WLAN2_PROD]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 12, 8, 16, IPA_EE_AP, GSI_FREE_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_USB_PROD]            = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_APPS_LAN_PROD]	  = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 11, 14, 10, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_APPS_WAN_PROD]	  = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 7, 16, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 7 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 7, 9, 20, 24, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_ODU_PROD]            = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 5, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_ETHERNET_PROD]	  = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 12, 0, 8, 16, IPA_EE_UC, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 5, 0, 16, 28, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_Q6_CMD_PROD]	  = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 1, 20, 24, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_Q6_DL_NLO_DATA_PROD] = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 2, 27, 32, IPA_EE_Q6, GSI_FREE_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_RTK_ETHERNET_PROD] = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 10, 13, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA  },
	/* Only for test purpose */
	[IPA_4_5][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_TEST1_PROD]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 5, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 12, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 11, 14, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_WLAN2_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 24, 3, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_WLAN2_CONS1]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 27, 18, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA},
	[IPA_4_5][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 17, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 15, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_ODL_DPL_CONS]        = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 22, 2, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 10, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_APPS_WAN_COAL_CONS]       = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 4, 8, 11, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 1, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_ODU_EMB_CONS]        = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 30, 6, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_ETHERNET_CONS]	  = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 28, 1, 9, 9, IPA_EE_UC, GSI_SMART_PRE_FETCH, 4 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 3, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 7, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_Q6_UL_NLO_DATA_CONS] = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 5, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_Q6_UL_NLO_ACK_CONS]  = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 6, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_Q6_QBAP_STATUS_CONS] = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 4, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_RTK_ETHERNET_CONS] = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 23, 8, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4 }, IPA_TX_INSTANCE_NA  },
	/* Only for test purpose */
	/* MBIM aggregation test pipes should have the same QMB as USB_CONS */
	[IPA_4_5][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 1, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_TEST1_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 1, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 24, 3, 8, 14, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 17, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 27, 18, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_4_5][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5][IPA_CLIENT_TPUT_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 25, 16, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4 } },
	/* IPA_4_5_MHI */
	[IPA_4_5_MHI][IPA_CLIENT_APPS_CMD_PROD]		= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 7, 9, 20, 24, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_Q6_WAN_PROD]		= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 5, 0, 16, 28, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_Q6_CMD_PROD]		= {
			true, IPA_v4_5_MHI_GROUP_PCIE,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 1, 20, 24, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_Q6_DL_NLO_DATA_PROD]	= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 2, 27, 32, IPA_EE_Q6, GSI_FREE_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_Q6_AUDIO_DMA_MHI_PROD]	= {
			true, IPA_v4_5_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 4, 8, 8, 16, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_MHI_PROD]		= {
			true, IPA_v4_5_MHI_GROUP_PCIE,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_PCIE,
			{ 1, 0, 16, 20, IPA_EE_AP, GSI_SMART_PRE_FETCH, 7 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_MEMCPY_DMA_SYNC_PROD]	= {
			true, IPA_v4_5_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 9, 12, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD]	= {
			true, IPA_v4_5_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 10, 13, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_MHI_LOW_LAT_PROD] = {
			true, IPA_v4_5_MHI_GROUP_PCIE,
			false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_PCIE,
			{ 3, 5, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA  },
	[IPA_4_5_MHI][IPA_CLIENT_QDSS_PROD] = {
			true, IPA_v4_5_MHI_GROUP_QDSS,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 11, 14, 10, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },
	/* Only for test purpose */
	[IPA_4_5_MHI][IPA_CLIENT_TEST_PROD]           = {
			true, QMB_MASTER_SELECT_DDR,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	[IPA_4_5_MHI][IPA_CLIENT_APPS_LAN_CONS]		= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 10, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 15, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_Q6_LAN_CONS]		= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 3, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_Q6_WAN_CONS]		= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 7, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_Q6_UL_NLO_DATA_CONS]	= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 5, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_Q6_UL_NLO_ACK_CONS]	= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 6, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_Q6_QBAP_STATUS_CONS]	= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 4, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_Q6_AUDIO_DMA_MHI_CONS]	= {
			true, IPA_v4_5_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 29, 9, 9, 9, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 4 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_MEMCPY_DMA_SYNC_CONS]	= {
			true, IPA_v4_5_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 26, 17, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS]	= {
			true, IPA_v4_5_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 27, 18, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_MHI_CONS]		= {
			true, IPA_v4_5_MHI_GROUP_PCIE,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 14, 1, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_MHI_DPL_CONS]		= {
			true, IPA_v4_5_MHI_GROUP_PCIE,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 22, 2, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_MHI_LOW_LAT_CONS] = {
			true, IPA_v4_5_MHI_GROUP_PCIE,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 30, 6, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_MHI][IPA_CLIENT_MHI_QDSS_CONS] = {
			true, IPA_v4_5_MHI_GROUP_QDSS,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 24, 3, 8, 14, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_4_5_MHI][IPA_CLIENT_DUMMY_CONS]          = {
			true, QMB_MASTER_SELECT_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	/* IPA_4_5_AUTO */
	[IPA_4_5_AUTO][IPA_CLIENT_WLAN2_PROD]          = {
			false, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 12, 8, 16, IPA_EE_AP, GSI_FREE_PRE_FETCH, 2 } },
	[IPA_4_5_AUTO][IPA_CLIENT_WLAN1_PROD]          = {
			false, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 12, 8, 16, IPA_EE_AP, GSI_FREE_PRE_FETCH, 2 } },
	[IPA_4_5_AUTO][IPA_CLIENT_USB_PROD]            = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO][IPA_CLIENT_APPS_LAN_PROD]	  = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 11, 14, 10, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2 } },
	[IPA_4_5_AUTO][IPA_CLIENT_APPS_WAN_PROD]	  = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 7, 16, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 7 } },
	[IPA_4_5_AUTO][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 7, 9, 20, 24, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO][IPA_CLIENT_USB2_PROD]            = {
			true, IPA_v4_5_GROUP_CV2X,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 5, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 } },
	[IPA_4_5_AUTO][IPA_CLIENT_ETHERNET_PROD]	  = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 12, 0, 8, 16, IPA_EE_UC, GSI_SMART_PRE_FETCH, 3 } },
	[IPA_4_5_AUTO][IPA_CLIENT_ETHERNET2_PROD]	= {
			true, IPA_v4_5_GROUP_CV2X,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 10, 13, 8, 16, IPA_EE_UC, GSI_SMART_PRE_FETCH, 3 } },
	[IPA_4_5_AUTO][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 5, 0, 16, 28, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 } },
	[IPA_4_5_AUTO][IPA_CLIENT_Q6_CMD_PROD]	  = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 1, 20, 24, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO][IPA_CLIENT_Q6_DL_NLO_DATA_PROD] = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 2, 27, 32, IPA_EE_Q6, GSI_FREE_PRE_FETCH, 3 } },
	[IPA_4_5_AUTO][IPA_CLIENT_Q6_CV2X_PROD] = {
			true, IPA_v4_5_GROUP_CV2X,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 8, 4, 8, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 } },
	[IPA_4_5_AUTO][IPA_CLIENT_AQC_ETHERNET_PROD] = {
			false, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 10, 13, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 } },
	/* Only for test purpose */
	[IPA_4_5_AUTO][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP } },
	[IPA_4_5_AUTO][IPA_CLIENT_TEST1_PROD]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP } },
	[IPA_4_5_AUTO][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 5, 8, 16, IPA_EE_AP } },
	[IPA_4_5_AUTO][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 12, 8, 16, IPA_EE_AP } },
	[IPA_4_5_AUTO][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 11, 14, 8, 16, IPA_EE_AP } },

	[IPA_4_5_AUTO][IPA_CLIENT_WLAN2_CONS]          = {
			false, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 24, 18, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 } },
	[IPA_4_5_AUTO][IPA_CLIENT_WLAN1_CONS]          = {
			false, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 24, 18, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 } },
	[IPA_4_5_AUTO][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 3, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 15, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO][IPA_CLIENT_ODL_DPL_CONS]        = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 22, 2, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 10, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 1, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4 } },
	[IPA_4_5_AUTO][IPA_CLIENT_USB2_CONS]        = {
			true, IPA_v4_5_GROUP_CV2X,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 30, 6, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4 } },
	[IPA_4_5_AUTO][IPA_CLIENT_ETHERNET_CONS]	  = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 28, 1, 9, 9, IPA_EE_UC, GSI_SMART_PRE_FETCH, 4 } },
	[IPA_4_5_AUTO][IPA_CLIENT_ETHERNET2_CONS]	= {
			true, IPA_v4_5_GROUP_CV2X,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 25, 16, 9, 9, IPA_EE_UC, GSI_SMART_PRE_FETCH, 4 } },
	[IPA_4_5_AUTO][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 3, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO][IPA_CLIENT_Q6_CV2X_CONS]         = {
			true, IPA_v4_5_GROUP_CV2X,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 29, 9, 9, 9, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 4 } },
	[IPA_4_5_AUTO][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 7, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO][IPA_CLIENT_Q6_UL_NLO_DATA_CONS] = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 5, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 } },
	[IPA_4_5_AUTO][IPA_CLIENT_Q6_UL_NLO_ACK_CONS]  = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 6, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 } },
	[IPA_4_5_AUTO][IPA_CLIENT_Q6_QBAP_STATUS_CONS] = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 4, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO][IPA_CLIENT_AQC_ETHERNET_CONS] = {
			false, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 23, 17, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4 } },
	/* Only for test purpose */
	/* MBIM aggregation test pipes should have the same QMB as USB_CONS */
	[IPA_4_5_AUTO][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 1, 9, 9, IPA_EE_AP } },
	[IPA_4_5_AUTO][IPA_CLIENT_TEST1_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 1, 9, 9, IPA_EE_AP } },
	[IPA_4_5_AUTO][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 24, 3, 8, 14, IPA_EE_AP } },
	[IPA_4_5_AUTO][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 17, 9, 9, IPA_EE_AP } },
	[IPA_4_5_AUTO][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 27, 18, 9, 9, IPA_EE_AP } },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_4_5_AUTO][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP } },

	/* IPA_4_5_AUTO_MHI */
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_APPS_CMD_PROD]		= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 7, 9, 20, 24, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_APPS_LAN_PROD]	  = {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 11, 14, 10, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_APPS_WAN_PROD]	  = {
			true, IPA_v4_5_MHI_GROUP_DDR,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 7, 16, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 7 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_MHI2_PROD]            = {
			true, IPA_v4_5_GROUP_CV2X,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_PCIE,
			{ 3, 5, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_Q6_CV2X_PROD] = {
			true, IPA_v4_5_GROUP_CV2X,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 8, 10, 16, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_ETHERNET_PROD]	  = {
			true, IPA_v4_5_MHI_GROUP_DDR,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 12, 0, 8, 16, IPA_EE_UC, GSI_SMART_PRE_FETCH, 3 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_Q6_WAN_PROD]		= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 5, 0, 16, 28, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_USB_PROD]			= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{0, 11, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3} },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_Q6_CMD_PROD]		= {
			true, IPA_v4_5_MHI_GROUP_PCIE,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 1, 20, 24, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_Q6_DL_NLO_DATA_PROD]	= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 2, 27, 32, IPA_EE_Q6, GSI_FREE_PRE_FETCH, 3 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_Q6_AUDIO_DMA_MHI_PROD]	= {
			true, IPA_v4_5_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 4, 8, 8, 16, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 3 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_MHI_PROD]		= {
			true, IPA_v4_5_MHI_GROUP_PCIE,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_PCIE,
			{ 1, 0, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 7 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_MEMCPY_DMA_SYNC_PROD]	= {
			true, IPA_v4_5_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 9, 12, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD]	= {
			true, IPA_v4_5_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 10, 13, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },
	/* Only for test purpose */
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_TEST_PROD]           = {
			true, QMB_MASTER_SELECT_DDR,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP } },

	[IPA_4_5_AUTO_MHI][IPA_CLIENT_APPS_LAN_CONS]		= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 10, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_APPS_WAN_CONS]		= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 25, 16, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_ETHERNET_CONS]	  = {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 28, 1, 9, 9, IPA_EE_UC, GSI_SMART_PRE_FETCH, 4 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 15, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_Q6_LAN_CONS]		= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 3, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_Q6_CV2X_CONS]         = {
			true, IPA_v4_5_GROUP_CV2X,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 29, 9, 9, 9, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 4 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_MHI2_CONS]        = {
			true, IPA_v4_5_GROUP_CV2X,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 30, 6, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_Q6_WAN_CONS]		= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 7, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_USB_CONS]			= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{13, 4, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4} },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_Q6_UL_NLO_DATA_CONS]	= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 5, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_Q6_UL_NLO_ACK_CONS]	= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 6, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_Q6_QBAP_STATUS_CONS]	= {
			true, IPA_v4_5_MHI_GROUP_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 4, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_Q6_AUDIO_DMA_MHI_CONS]	= {
			true, IPA_v4_5_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 29, 9, 9, 9, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 4 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_MEMCPY_DMA_SYNC_CONS]	= {
			true, IPA_v4_5_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 23, 17, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS]	= {
			true, IPA_v4_5_MHI_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 24, 18, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_MHI_CONS]		= {
			true, IPA_v4_5_MHI_GROUP_PCIE,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 14, 1, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4 } },
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_MHI_DPL_CONS]		= {
			true, IPA_v4_5_MHI_GROUP_PCIE,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 22, 2, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 } },

	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_4_5_AUTO_MHI][IPA_CLIENT_DUMMY_CONS]          = {
			true, QMB_MASTER_SELECT_DDR,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP } },

	/* IPA_4_5 APQ */
	[IPA_4_5_APQ][IPA_CLIENT_WLAN2_PROD]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 3, 8, 16, IPA_EE_AP, GSI_FREE_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_WIGIG_PROD]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 1, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_USB_PROD]            = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_APPS_LAN_PROD]	  = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 11, 4, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 7, 12, 20, 24, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	/* MHI PRIME PIPES - Client producer / IPA Consumer pipes */
	[IPA_4_5_APQ][IPA_CLIENT_MHI_PRIME_DPL_PROD] = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{3, 2, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_MHI_PRIME_TETH_PROD] = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 7, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_MHI_PRIME_RMNET_PROD] = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 4, 11, 16, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 7 }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	[IPA_4_5_APQ][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_TEST1_PROD]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 1, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 3, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 10, 10, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	[IPA_4_5_APQ][IPA_CLIENT_WLAN2_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 23, 8, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_WIGIG1_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 14, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_WIGIG2_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 18, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_WIGIG3_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 22, 5, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_WIGIG4_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 29, 10, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 24, 9, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 16, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 13, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_ODL_DPL_CONS]       = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 19, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	/* MHI PRIME PIPES - Client Consumer / IPA Producer pipes */
	[IPA_4_5_APQ][IPA_CLIENT_MHI_PRIME_TETH_CONS] = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 28, 6, 8, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_MHI_PRIME_RMNET_CONS] = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 17, 8, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4 }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	/* MBIM aggregation test pipes should have the same QMB as USB_CONS */
	[IPA_4_5_APQ][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 16, 5, 5, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_TEST1_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 16, 5, 5, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 22, 5, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 24, 9, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_5_APQ][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 23, 8, 8, 13, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_4_5_APQ][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v4_5_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	/* IPA_4_7 */
	[IPA_4_7][IPA_CLIENT_WLAN1_PROD]          = {
			true, IPA_v4_7_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 3, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_USB_PROD]            = {
			true, IPA_v4_7_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_APPS_LAN_PROD]	  = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 4, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_APPS_WAN_PROD]	  = {
			true, IPA_v4_7_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 2, 16, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 7 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 7, 5, 20, 24, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 8 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v4_7_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 5, 0, 16, 28, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_Q6_CMD_PROD]	  = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 1, 20, 24, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 8 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_Q6_DL_NLO_DATA_PROD] = {
			true, IPA_v4_7_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 2, 27, 32, IPA_EE_Q6, GSI_FREE_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	[IPA_4_7][IPA_CLIENT_TEST_PROD]           = {
			true, IPA_v4_7_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_TEST1_PROD]          = {
			true, IPA_v4_7_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_TEST2_PROD]          = {
			true, IPA_v4_7_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 1, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_TEST3_PROD]          = {
			true, IPA_v4_7_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 2, 16, 32, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_TEST4_PROD]          = {
			true, IPA_v4_7_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 1, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	[IPA_4_7][IPA_CLIENT_WLAN1_CONS]          = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 9, 8, 13, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 10, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_USB_DPL_CONS]        = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 8, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_ODL_DPL_CONS]        = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 22, 13, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 9, 14, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 7, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_APPS_WAN_COAL_CONS]       = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 6, 8, 11, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 10, 3, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 7, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_Q6_UL_NLO_DATA_CONS] = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 5, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_Q6_UL_NLO_ACK_CONS]  = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 6, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_Q6_QBAP_STATUS_CONS] = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 4, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	/* MBIM aggregation test pipes should have the same QMB as USB_CONS */
	[IPA_4_7][IPA_CLIENT_TEST_CONS]           = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 7, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_TEST1_CONS]          = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 7, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_TEST2_CONS]          = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 12, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_TEST3_CONS]          = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 10, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_7][IPA_CLIENT_TEST4_CONS]          = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 11, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_4_7][IPA_CLIENT_DUMMY_CONS]          = {
			true, IPA_v4_7_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	/* IPA_4_9 */
	[IPA_4_9][IPA_CLIENT_USB_PROD]          = {
			true, IPA_v4_9_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_APPS_WAN_PROD]	  = {
			true, IPA_v4_9_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 2, 16, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 8 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_APPS_WAN_LOW_LAT_PROD]	  = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 1, 1, 4, 4, IPA_EE_AP, GSI_SMART_PRE_FETCH, 1 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_WLAN2_PROD]          = {
			true, IPA_v4_9_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 3, 8, 16, IPA_EE_AP, GSI_FREE_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_APPS_LAN_PROD]	  = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 4, 10, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_WIGIG_PROD]          = {
			true, IPA_v4_9_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 5, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 7, 6, 20, 24, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v4_9_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 5, 0, 16, 28, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_Q6_CMD_PROD]	  = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 1, 20, 24, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_Q6_DL_NLO_DATA_PROD] = {
			true, IPA_v4_9_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 2, 27, 32, IPA_EE_Q6, GSI_FREE_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },


	[IPA_4_9][IPA_CLIENT_APPS_WAN_COAL_CONS]       = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 11, 8, 11, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_APPS_WAN_CONS]       = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 12, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_APPS_WAN_LOW_LAT_CONS]       = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 9, 6, 6, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_USB_DPL_CONS]            = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 13, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_ODL_DPL_CONS]        = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 22, 14, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_WIGIG1_CONS]          = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 23, 15, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_WLAN2_CONS]          = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 24, 16, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_USB_CONS]            = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 25, 17, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_WIGIG2_CONS]          = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 18, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_WIGIG3_CONS]          = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 27, 19, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_WIGIG4_CONS]          = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 28, 20, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_APPS_LAN_CONS]       = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 7, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 3, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_Q6_QBAP_STATUS_CONS] = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 4, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_Q6_UL_NLO_DATA_CONS] = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 5, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_Q6_UL_NLO_ACK_CONS]  = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 6, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_9][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v4_9_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 7, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },

	/* IPA_4_11 */
	[IPA_4_11][IPA_CLIENT_WLAN1_PROD]		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 3, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_USB_PROD] 		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_APPS_LAN_PROD]	  = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 4, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_APPS_WAN_PROD]	  = {
			true, IPA_v4_11_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 2, 16, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 7 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_APPS_CMD_PROD]	  = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 7, 5, 20, 24, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 8 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_APPS_WAN_LOW_LAT_PROD]	  = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 1, 1, 4, 4, IPA_EE_AP, GSI_SMART_PRE_FETCH, 1 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_Q6_WAN_PROD]		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 5, 0, 16, 28, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_Q6_CMD_PROD]			= {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 1, 20, 24, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 8 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_Q6_DL_NLO_DATA_PROD]  = {
			true, IPA_v4_11_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 2, 24, 32, IPA_EE_Q6, GSI_FREE_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	/* Only for test purpose */
	[IPA_4_11][IPA_CLIENT_TEST_PROD]		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_TEST1_PROD]		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_TEST2_PROD]		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 1, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_TEST3_PROD]		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 2, 16, 32, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_TEST4_PROD]		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 1, 8, 16, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_WLAN1_CONS]		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 9, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_USB_CONS] 		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 10, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_USB_DPL_CONS] 	   = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 8, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_ODL_DPL_CONS] 	   = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 22, 13, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_APPS_LAN_CONS]	   = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 9, 14, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_APPS_WAN_CONS]	   = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 7, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_APPS_WAN_COAL_CONS]		= {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 6, 8, 11, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_APPS_WAN_LOW_LAT_CONS] =           {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 11, 4, 4, IPA_EE_AP, GSI_SMART_PRE_FETCH, 1 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_Q6_LAN_CONS]		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 10, 3, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_Q6_WAN_CONS]		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 7, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_Q6_UL_NLO_DATA_CONS] = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 5, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_Q6_UL_NLO_ACK_CONS]  = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 6, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_Q6_QBAP_STATUS_CONS] = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 4, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 }, IPA_TX_INSTANCE_NA },
/* Only for test purpose */
	/* MBIM aggregation test pipes should have the same QMB as USB_CONS */
	[IPA_4_11][IPA_CLIENT_TEST_CONS]		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 7, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_TEST1_CONS]		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 7, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_TEST2_CONS]		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 12, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_TEST3_CONS]		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 10, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	[IPA_4_11][IPA_CLIENT_TEST4_CONS]		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 11, 9, 9, IPA_EE_AP }, IPA_TX_INSTANCE_NA },
	/* Dummy consumer (pipe 31) is used in L2TP rt rule */
	[IPA_4_11][IPA_CLIENT_DUMMY_CONS]		   = {
			true, IPA_v4_11_GROUP_UL_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 31, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	/* IPA_5_0 */
	[IPA_5_0][IPA_CLIENT_USB_PROD] = {
			true,   IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0 , 14 , 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_APPS_WAN_PROD] = {
			true,   IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2 , 11, 25, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_APPS_WAN_LOW_LAT_PROD] = {
			true,   IPA_v5_0_GROUP_URLLC,
			false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4 , 9 , 16, 24, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_WLAN2_PROD] ={
			true,   IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6 , 16, 8 , 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_USB2_PROD] = {
			true,   IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 7 , 17, 8 , 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_ETHERNET_PROD] = {
			true,   IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8 , 18, 8 , 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_AQC_ETHERNET_PROD] = {
			true,   IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8 , 18, 8 , 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_RTK_ETHERNET_PROD] = {
			true,   IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8 , 18, 8 , 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_ETHERNET_PROD] = {
			true,   IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8 , 18, 8 , 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_ETHERNET2_PROD] = {
			true,   IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3 , 7, 8 , 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_APPS_LAN_PROD] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 9 , 19, 25, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_ODU_PROD]  = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 7, 17, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_WLAN3_PROD] = {
			true,   IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1 , 0, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_APPS_CMD_PROD] = {
			true,   IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 14, 12, 20, 24, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_Q6_WAN_PROD]  = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 12, 0, 16, 28, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 },
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_Q6_CMD_PROD]  = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 13, 1, 20, 24, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 8 },
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_Q6_DL_NLO_DATA_PROD]  = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 15, 2, 28, 32, IPA_EE_Q6, GSI_FREE_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_Q6_DL_NLO_LL_DATA_PROD] = {
			true, IPA_v5_0_GROUP_URLLC,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 5, 8, 28, 32, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_UL },
	[IPA_5_0][IPA_CLIENT_TEST_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 14, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_TEST1_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 15, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_TEST2_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 16, 24, IPA_EE_AP, GSI_SMART_PRE_FETCH, 7},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_TEST3_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 10, 5, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0][IPA_CLIENT_TEST4_PROD] = {
			true,	IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 7, 17, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_NA },

	[IPA_5_0][IPA_CLIENT_APPS_LAN_CONS] = {
			true,   IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 13, 9 , 9 , IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_UL },
	[IPA_5_0][IPA_CLIENT_APPS_WAN_COAL_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 22, 4 , 8 , 11, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_APPS_WAN_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 23, 1 , 9 , 9 , IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_USB_DPL_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 24, 20, 5 , 5 , IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_ODL_DPL_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 25, 2 , 5 , 5 , IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_DL },
	 [IPA_5_0][IPA_CLIENT_AQC_ETHERNET_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 21, 9 , 9 , IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	 [IPA_5_0][IPA_CLIENT_RTK_ETHERNET_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 21, 9 , 9 , IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	 [IPA_5_0][IPA_CLIENT_ETHERNET_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 21, 9 , 9 , IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	 [IPA_5_0][IPA_CLIENT_ETHERNET2_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 30, 24, 9 , 9 , IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_WLAN2_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 27, 3 , 8 , 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_WLAN2_CONS1] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 28, 22 , 8 , 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_USB_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 29, 23, 9 , 9 , IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_APPS_WAN_LOW_LAT_CONS] = {
			true, IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 32, 10, 9 , 9 , IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_USB2_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 34, 8, 9 , 9 , IPA_EE_AP, GSI_SMART_PRE_FETCH, 4},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_WLAN4_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 35, 26 , 8 , 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_ODU_EMB_CONS] = {
			true, IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 34, 8, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4 },
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_TEST_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 28, 22, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 1},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_TEST1_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 30, 24, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_TEST2_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 33, 6, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_TEST3_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 29, 23, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_TEST4_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 34, 8, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_Q6_LAN_CONS]   = {
			true, IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 3, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_DL },
	[IPA_5_0][IPA_CLIENT_Q6_WAN_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 7, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_UL },
	[IPA_5_0][IPA_CLIENT_Q6_UL_NLO_DATA_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 5, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 },
			IPA_TX_INSTANCE_UL },
	[IPA_5_0][IPA_CLIENT_Q6_UL_NLO_ACK_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 6, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 },
			IPA_TX_INSTANCE_UL },
	[IPA_5_0][IPA_CLIENT_Q6_QBAP_STATUS_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 4, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_UL },
	[IPA_5_0][IPA_CLIENT_TPUT_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 33, 6, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },

	[IPA_5_0][IPA_CLIENT_DUMMY_CONS]		   = {
			true, IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 36, 36, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },


	/* IPA_5_0_MHI */
	[IPA_5_0_MHI][IPA_CLIENT_USB_PROD] = {
			true,   IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0 , 14 , 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0_MHI][IPA_CLIENT_MHI_PROD] = {
			true,   IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_PCIE,
			{ 1 , 0 , 16, 24, IPA_EE_AP, GSI_SMART_PRE_FETCH, 7},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0_MHI][IPA_CLIENT_APPS_WAN_PROD] = {
			true,   IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2 , 11, 25, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0_MHI][IPA_CLIENT_MHI_LOW_LAT_PROD] = {
			true, IPA_v5_0_GROUP_URLLC,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_PCIE,
			{ 10, 5, 10, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0_MHI][IPA_CLIENT_MEMCPY_DMA_SYNC_PROD] ={
			true,   IPA_v5_0_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 6 , 16, 8 , 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0_MHI][IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD] = {
			true,   IPA_v5_0_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 7 , 17, 8 , 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0_MHI][IPA_CLIENT_QDSS_PROD] = {
			true,   IPA_v5_0_GROUP_QDSS,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 8 , 18, 4 , 8, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0_MHI][IPA_CLIENT_APPS_CMD_PROD] = {
			true,   IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 14, 12, 20, 24, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0_MHI][IPA_CLIENT_ODU_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 14, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },
	[IPA_5_0_MHI][IPA_CLIENT_Q6_WAN_PROD]  = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 12, 0, 16, 28, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 },
			IPA_TX_INSTANCE_NA },
	[IPA_5_0_MHI][IPA_CLIENT_Q6_CMD_PROD]  = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 13, 1, 20, 24, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 8 },
			IPA_TX_INSTANCE_NA },
	[IPA_5_0_MHI][IPA_CLIENT_Q6_DL_NLO_DATA_PROD]  = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 15, 2, 28, 32, IPA_EE_Q6, GSI_FREE_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_NA },
	[IPA_5_0_MHI][IPA_CLIENT_Q6_DL_NLO_LL_DATA_PROD] = {
			true, IPA_v5_0_GROUP_URLLC,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 5, 8, 28, 32, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_UL },
	[IPA_5_0_MHI][IPA_CLIENT_TEST_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 14, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },

	[IPA_5_0_MHI][IPA_CLIENT_APPS_LAN_CONS] = {
			true,   IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 13, 9 , 9 , IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_UL },
	[IPA_5_0_MHI][IPA_CLIENT_APPS_WAN_COAL_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 22, 4 , 8 , 11, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0_MHI][IPA_CLIENT_APPS_WAN_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 21 , 9 , 9 , IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_DL},
	[IPA_5_0_MHI][IPA_CLIENT_MHI_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 23, 1 , 9 , 9 , IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0_MHI][IPA_CLIENT_MHI_DPL_CONS] = {
			true, IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 25, 2, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_DL },
	[IPA_5_0_MHI][IPA_CLIENT_USB_DPL_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 24, 20, 5 , 5 , IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0_MHI][IPA_CLIENT_MHI_QDSS_CONS] = {
			true,   IPA_v5_0_GROUP_QDSS,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 27, 3 , 5 , 5 , IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0_MHI][IPA_CLIENT_USB_CONS] = {
			true,   IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 28, 22 , 9 , 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0_MHI][IPA_CLIENT_MEMCPY_DMA_SYNC_CONS] = {
			true,   IPA_v5_0_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 29, 23, 5 , 5 , IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0_MHI][IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS] = {
			true,   IPA_v5_0_GROUP_DMA,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 30, 24, 5 , 5 , IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0_MHI][IPA_CLIENT_MHI_LOW_LAT_CONS] = {
			true, IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 33, 6, 9 , 9 , IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0_MHI][IPA_CLIENT_ODU_EMB_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 28, 22, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 1},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0_MHI][IPA_CLIENT_TEST_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 28, 22, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 1},
			IPA_TX_INSTANCE_DL },
	[IPA_5_0_MHI][IPA_CLIENT_Q6_LAN_CONS]   = {
			true, IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 3, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_DL },
	[IPA_5_0_MHI][IPA_CLIENT_Q6_WAN_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 7, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_UL },
	[IPA_5_0_MHI][IPA_CLIENT_Q6_UL_NLO_DATA_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 5, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 },
			IPA_TX_INSTANCE_UL },
	[IPA_5_0_MHI][IPA_CLIENT_Q6_UL_NLO_ACK_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 6, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 },
			IPA_TX_INSTANCE_UL },
	[IPA_5_0_MHI][IPA_CLIENT_Q6_QBAP_STATUS_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 4, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_UL },

	[IPA_5_0_MHI][IPA_CLIENT_DUMMY_CONS]		   = {
			true, IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 36, 36, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	/* IPA_5_1 */
	[IPA_5_1][IPA_CLIENT_USB_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0	},
			IPA_TX_INSTANCE_NA },

	[IPA_5_1][IPA_CLIENT_APPS_WAN_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 11, 25, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_NA },

	[IPA_5_1][IPA_CLIENT_WLAN2_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 16, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2	},
			IPA_TX_INSTANCE_NA },

	[IPA_5_1][IPA_CLIENT_WIGIG_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 7, 17, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2	},
			IPA_TX_INSTANCE_NA },

	[IPA_5_1][IPA_CLIENT_APPS_LAN_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 19, 26, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4	},
			IPA_TX_INSTANCE_NA },

	[IPA_5_1][IPA_CLIENT_APPS_CMD_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 14, 12, 20, 24, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0	},
			IPA_TX_INSTANCE_NA },

	[IPA_5_1][IPA_CLIENT_APPS_WAN_LOW_LAT_PROD] = {
			true, IPA_v5_0_GROUP_URLLC,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 4, 9, 16, 24, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_NA },

	[IPA_5_1][IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_PROD] = {
			true, IPA_v5_0_GROUP_URLLC,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 10, 5, 10, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_NA },

	[IPA_5_1][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 12, 0, 16, 28, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_1][IPA_CLIENT_Q6_CMD_PROD]	  = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 13, 1, 20, 24, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_1][IPA_CLIENT_Q6_DL_NLO_DATA_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 15, 2, 28, 32, IPA_EE_Q6, GSI_FREE_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_1][IPA_CLIENT_Q6_DL_NLO_LL_DATA_PROD] = {
			true, IPA_v5_0_GROUP_URLLC,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 5, 8, 28, 32, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_UL },

	[IPA_5_1][IPA_CLIENT_APPS_LAN_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 13, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0	},
			IPA_TX_INSTANCE_UL },

	[IPA_5_1][IPA_CLIENT_APPS_WAN_COAL_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 22, 4, 8, 11, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_APPS_WAN_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 23, 1, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_USB_DPL_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 24, 20, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_ODL_DPL_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 25, 2, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_WIGIG1_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 21, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_WLAN1_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 27, 3, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_WLAN2_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 28, 22, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_USB_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 29, 23, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_WIGIG2_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 30, 24, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_WIGIG3_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 34, 25, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_WIGIG4_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 35, 26, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_APPS_WAN_LOW_LAT_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 32, 10, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS] = {
			true, IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 33, 6, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 3, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_Q6_QBAP_STATUS_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 4, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_UL },

	[IPA_5_1][IPA_CLIENT_Q6_UL_NLO_DATA_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 5, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 },
			IPA_TX_INSTANCE_UL },

	[IPA_5_1][IPA_CLIENT_Q6_UL_NLO_ACK_CONS]  = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 6, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 },
			IPA_TX_INSTANCE_UL },

	[IPA_5_1][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 7, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_UL },

	[IPA_5_1][IPA_CLIENT_DUMMY_CONS]		   = {
			true, IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 36, 36, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	/*For test purposes only*/
	[IPA_5_1][IPA_CLIENT_TEST_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 14, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_1][IPA_CLIENT_TEST1_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 15, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_1][IPA_CLIENT_TEST2_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 16, 24, IPA_EE_AP, GSI_SMART_PRE_FETCH, 7 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_1][IPA_CLIENT_TEST3_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 10, 5, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_1][IPA_CLIENT_TEST4_PROD] = {
			true,	IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 7, 17, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_1][IPA_CLIENT_TEST_CONS] =	{
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 32, 8, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 1 },
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_TEST1_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 30, 24, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_TEST2_CONS] =	{
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 33, 6, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_TEST3_CONS] =	{
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 29, 23, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_DL },

	[IPA_5_1][IPA_CLIENT_TEST4_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_PCIE,
			{ 34, 25, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_DL },

	/* IPA_5_1_APQ */
	[IPA_5_1_APQ][IPA_CLIENT_MHI_PRIME_DPL_PROD] = {
			true, IPA_v5_0_GROUP_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{8, 18, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_NA },
	[IPA_5_1_APQ][IPA_CLIENT_MHI_PRIME_RMNET_PROD] = {
			true, IPA_v5_0_GROUP_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 3, 15, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_NA },
	[IPA_5_1_APQ][IPA_CLIENT_USB_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_NA },
	[IPA_5_1_APQ][IPA_CLIENT_APPS_WAN_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 11, 25, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_NA },
	[IPA_5_1_APQ][IPA_CLIENT_APPS_LAN_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 19, 26, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4},
			IPA_TX_INSTANCE_NA },
	[IPA_5_1_APQ][IPA_CLIENT_APPS_CMD_PROD] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 14, 12, 20, 24, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_NA },

	[IPA_5_1_APQ][IPA_CLIENT_MHI_PRIME_RMNET_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 8, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_UL },
	[IPA_5_1_APQ][IPA_CLIENT_APPS_LAN_CONS] = {
			true, IPA_v5_0_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 13, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_UL },
	[IPA_5_1_APQ][IPA_CLIENT_USB_DPL_CONS] = {
			true, IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 24, 20, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_DL },
	[IPA_5_1_APQ][IPA_CLIENT_ODL_DPL_CONS] = {
			true, IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 25, 2, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_DL },
	[IPA_5_1_APQ][IPA_CLIENT_USB_CONS] = {
			true, IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 29, 23, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },

	[IPA_5_1_APQ][IPA_CLIENT_DUMMY_CONS]		   = {
			true, IPA_v5_0_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 36, 36, 8, 8, IPA_EE_AP }, IPA_TX_INSTANCE_NA },

	/* IPA_5_2 */
	[IPA_5_2][IPA_CLIENT_USB_PROD] = {
			true, IPA_v5_2_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0}, IPA_TX_INSTANCE_NA },

	[IPA_5_2][IPA_CLIENT_APPS_WAN_LOW_LAT_PROD] = {
			true, IPA_v5_2_GROUP_URLLC,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 3, 2, 12, 20, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3}, IPA_TX_INSTANCE_NA },

	[IPA_5_2][IPA_CLIENT_Q6_DL_NLO_LL_DATA_PROD] = {
			true, IPA_v5_2_GROUP_URLLC,
			false,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 8, 28, 32, IPA_EE_Q6, GSI_FREE_PRE_FETCH, 3}, IPA_TX_INSTANCE_NA },

	[IPA_5_2][IPA_CLIENT_WLAN2_PROD] = {
			true, IPA_v5_2_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 5, 3, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2}, IPA_TX_INSTANCE_NA },

	[IPA_5_2][IPA_CLIENT_APPS_LAN_PROD] = {
			true, IPA_v5_2_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 4, 26, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4}, IPA_TX_INSTANCE_NA },


	[IPA_5_2][IPA_CLIENT_APPS_WAN_PROD] = {
			true, IPA_v5_2_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 5, 25, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3}, IPA_TX_INSTANCE_NA },

	[IPA_5_2][IPA_CLIENT_Q6_WAN_PROD] = {
			true, IPA_v5_2_GROUP_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 7, 0, 16, 28, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2}, IPA_TX_INSTANCE_NA },

	[IPA_5_2][IPA_CLIENT_Q6_CMD_PROD] = {
			true, IPA_v5_2_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 8, 1, 20, 24, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0}, IPA_TX_INSTANCE_NA },

	[IPA_5_2][IPA_CLIENT_APPS_CMD_PROD] = {
			true, IPA_v5_2_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 9, 6, 20, 24, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0}, IPA_TX_INSTANCE_NA },
	[IPA_5_2][IPA_CLIENT_Q6_DL_NLO_DATA_PROD] = {
			true, IPA_v5_2_GROUP_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 10, 2, 28, 32, IPA_EE_Q6, GSI_FREE_PRE_FETCH, 3}, IPA_TX_INSTANCE_NA },

	[IPA_5_2][IPA_CLIENT_APPS_WAN_COAL_CONS] = {
			true, IPA_v5_2_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 8, 8, 11, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3}, IPA_TX_INSTANCE_UL },

	[IPA_5_2][IPA_CLIENT_APPS_WAN_CONS] = {
			true, IPA_v5_2_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 9, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3}, IPA_TX_INSTANCE_UL },

	[IPA_5_2][IPA_CLIENT_USB_DPL_CONS] = {
			true, IPA_v5_2_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 10, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0}, IPA_TX_INSTANCE_UL },

	[IPA_5_2][IPA_CLIENT_ODL_DPL_CONS] = {
			true, IPA_v5_2_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 11, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0}, IPA_TX_INSTANCE_UL },

	[IPA_5_2][IPA_CLIENT_WLAN2_CONS] = {
			true, IPA_v5_2_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 12, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3}, IPA_TX_INSTANCE_UL },

	[IPA_5_2][IPA_CLIENT_WLAN2_CONS1] = {
			true, IPA_v5_2_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 22, 13, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3}, IPA_TX_INSTANCE_UL },

	[IPA_5_2][IPA_CLIENT_USB_CONS] = {
			true, IPA_v5_2_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 23, 14, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3}, IPA_TX_INSTANCE_UL },


	[IPA_5_2][IPA_CLIENT_APPS_WAN_LOW_LAT_CONS] = {
			true, IPA_v5_2_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 24, 15, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3}, IPA_TX_INSTANCE_UL },


	[IPA_5_2][IPA_CLIENT_APPS_LAN_CONS] = {
			true, IPA_v5_2_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 11, 7, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0}, IPA_TX_INSTANCE_UL },

	[IPA_5_2][IPA_CLIENT_Q6_LAN_CONS] = {
			true, IPA_v5_2_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 12, 3, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0}, IPA_TX_INSTANCE_UL },

	[IPA_5_2][IPA_CLIENT_Q6_QBAP_STATUS_CONS] = {
			true, IPA_v5_2_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 13, 4, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0}, IPA_TX_INSTANCE_UL },

	[IPA_5_2][IPA_CLIENT_Q6_UL_NLO_DATA_CONS] = {
			true, IPA_v5_2_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 14, 5, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2}, IPA_TX_INSTANCE_UL },

	[IPA_5_2][IPA_CLIENT_Q6_UL_NLO_ACK_CONS] = {
			true, IPA_v5_2_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 15, 6, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2}, IPA_TX_INSTANCE_UL },

	[IPA_5_2][IPA_CLIENT_Q6_WAN_CONS] = {
			true, IPA_v5_2_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 7, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0}, IPA_TX_INSTANCE_UL },

	[IPA_5_2][IPA_CLIENT_TEST_PROD] = {
			true, IPA_v5_2_GROUP_URLLC,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 2, 12, 20, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3}, IPA_TX_INSTANCE_NA },

	[IPA_5_2][IPA_CLIENT_TEST1_PROD] = {
			true, IPA_v5_2_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 1, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3}, IPA_TX_INSTANCE_NA },

	[IPA_5_2][IPA_CLIENT_TEST2_PROD] = {
			true, IPA_v5_2_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 0, 0, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0}, IPA_TX_INSTANCE_NA },

	[IPA_5_2][IPA_CLIENT_TEST3_PROD] = {
			true, IPA_v5_2_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 5, 3, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3}, IPA_TX_INSTANCE_NA },

	[IPA_5_2][IPA_CLIENT_TEST4_PROD] = {
			true, IPA_v5_2_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 5, 25, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3}, IPA_TX_INSTANCE_NA },

	[IPA_5_2][IPA_CLIENT_TEST_CONS] = {
			true, IPA_v5_2_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 10, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0}, IPA_TX_INSTANCE_UL },

	[IPA_5_2][IPA_CLIENT_TEST1_CONS] = {
			true, IPA_v5_2_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 12, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3}, IPA_TX_INSTANCE_UL },

	[IPA_5_2][IPA_CLIENT_TEST2_CONS] = {
			true, IPA_v5_2_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 22, 13, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3}, IPA_TX_INSTANCE_DL },

	[IPA_5_2][IPA_CLIENT_TEST3_CONS] = {
			true, IPA_v5_2_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 23, 14, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0}, IPA_TX_INSTANCE_UL },

	[IPA_5_2][IPA_CLIENT_TEST4_CONS] = {
			true, IPA_v5_2_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR, //UPDATE AS DDR
			{ 20, 11, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0}, IPA_TX_INSTANCE_UL },

	/* IPA_5_5 */
	[IPA_5_5][IPA_CLIENT_USB_PROD] = {
			true, IPA_v5_5_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0	},
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_APPS_WAN_PROD] = {
			true, IPA_v5_5_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 11, 25, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_APPS_WAN_LOW_LAT_PROD] = {
			true, IPA_v5_5_GROUP_URLLC,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 4, 9, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_WLAN2_PROD] = {
			true, IPA_v5_5_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 16, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2	},
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_WIGIG_PROD] = {
			true, IPA_v5_5_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 7, 17, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2	},
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_APPS_LAN_PROD] = {
			true, IPA_v5_5_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 19, 26, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4	},
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_PROD] = {
			true, IPA_v5_5_GROUP_URLLC,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 10, 5, 10, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_APPS_CMD_PROD] = {
			true, IPA_v5_5_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 14, 12, 20, 24, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0	},
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_Q6_WAN_PROD]         = {
			true, IPA_v5_5_GROUP_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 12, 0, 16, 28, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_Q6_CMD_PROD]	  = {
			true, IPA_v5_5_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 13, 1, 20, 24, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_Q6_DL_NLO_DATA_PROD] = {
			true, IPA_v5_5_GROUP_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 15, 2, 28, 32, IPA_EE_Q6, GSI_FREE_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_Q6_DL_NLO_LL_DATA_PROD] = {
			true, IPA_v5_5_GROUP_URLLC,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 5, 8, 28, 32, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_APPS_LAN_COAL_CONS] = {
			true, IPA_v5_5_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 16, 13, 8, 23, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_UL },

	[IPA_5_5][IPA_CLIENT_APPS_LAN_CONS] = {
			true, IPA_v5_5_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 14, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0	},
			IPA_TX_INSTANCE_UL },

	[IPA_5_5][IPA_CLIENT_APPS_WAN_COAL_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 23, 4, 8, 23, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_APPS_WAN_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 24, 1, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_USB_DPL_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 25, 20, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_ODL_DPL_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 2, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_WIGIG1_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 27, 21, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_WLAN2_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 28, 3, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_WLAN2_CONS1] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 29, 22, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_USB_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 30, 23, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_APPS_WAN_LOW_LAT_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 32, 8, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 33, 10, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_WIGIG2_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 34, 6, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_WIGIG3_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 35, 25, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3	},
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_Q6_LAN_CONS]         = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 18, 3, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_Q6_QBAP_STATUS_CONS] = {
			true, IPA_v5_5_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 19, 4, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_UL },

	[IPA_5_5][IPA_CLIENT_Q6_UL_NLO_DATA_CONS] = {
			true, IPA_v5_5_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 20, 5, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 },
			IPA_TX_INSTANCE_UL },

	[IPA_5_5][IPA_CLIENT_Q6_UL_NLO_ACK_CONS]  = {
			true, IPA_v5_5_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 21, 6, 5, 5, IPA_EE_Q6, GSI_SMART_PRE_FETCH, 2 },
			IPA_TX_INSTANCE_UL },

	[IPA_5_5][IPA_CLIENT_Q6_WAN_CONS]         = {
			true, IPA_v5_5_GROUP_UL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 22, 7, 9, 9, IPA_EE_Q6, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_UL },

	/*For test purposes only*/
	[IPA_5_5][IPA_CLIENT_TEST_PROD] = {
			true, IPA_v5_5_GROUP_URLLC,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 4, 9, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_TEST1_PROD] = {
			true, IPA_v5_5_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 3, 7, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_TEST2_PROD] = {
			true, IPA_v5_5_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 1, 0, 8, 16, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_TEST3_PROD] = {
			true, IPA_v5_5_GROUP_URLLC,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 10, 5, 10, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_TEST4_PROD] = {
			true, IPA_v5_5_GROUP_UL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 2, 11, 25, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_NA },

	[IPA_5_5][IPA_CLIENT_TEST_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 32, 8, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_TEST1_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 26, 2, 5, 5, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0 },
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_TEST2_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 33, 10, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_TEST3_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 30, 23, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_DL },

	[IPA_5_5][IPA_CLIENT_TEST4_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 24, 1, 9, 9, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3 },
			IPA_TX_INSTANCE_DL },

	/* IPA_5_5_XR */

	[IPA_5_5_XR][IPA_CLIENT_APPS_LAN_PROD] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 9, 19, 26, 32, IPA_EE_AP, GSI_SMART_PRE_FETCH, 4},
			IPA_TX_INSTANCE_NA },
	[IPA_5_5_XR][IPA_CLIENT_APPS_CMD_PROD] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY,
			QMB_MASTER_SELECT_DDR,
			{ 14, 11, 20, 24, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_NA },
	[IPA_5_5_XR][IPA_CLIENT_WLAN2_PROD] = {
			true, IPA_v5_5_GROUP_DL,
			true,
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP,
			QMB_MASTER_SELECT_DDR,
			{ 6, 16, 8, 16, IPA_EE_AP, GSI_SMART_PRE_FETCH, 2},
			IPA_TX_INSTANCE_NA },

	[IPA_5_5_XR][IPA_CLIENT_APPS_LAN_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 17, 13, 9, 9, IPA_EE_AP, GSI_ESCAPE_BUF_ONLY, 0},
			IPA_TX_INSTANCE_UL },

	[IPA_5_5_XR][IPA_CLIENT_WLAN2_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 28, 3, 8, 14, IPA_EE_AP, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL },

	[IPA_5_5_XR][IPA_CLIENT_UC_RTP1_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 31, 2, 9, 9, IPA_EE_UC, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL},

	[IPA_5_5_XR][IPA_CLIENT_UC_RTP2_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 32, 3, 9, 9, IPA_EE_UC, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL},

	[IPA_5_5_XR][IPA_CLIENT_UC_RTP3_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 33, 4, 9, 9, IPA_EE_UC, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL},

	[IPA_5_5_XR][IPA_CLIENT_UC_RTP4_CONS] = {
			true, IPA_v5_5_GROUP_DL,
			false,
			IPA_DPS_HPS_SEQ_TYPE_INVALID,
			QMB_MASTER_SELECT_DDR,
			{ 34, 5, 9, 9, IPA_EE_UC, GSI_SMART_PRE_FETCH, 3},
			IPA_TX_INSTANCE_DL},
};

static struct ipa3_mem_partition ipa_3_0_mem_part = {
	.uc_ofst = 0x0,
	.uc_size = 0x80,
	.ofst_start = 0x280,
	.v4_flt_hash_ofst = 0x288,
	.v4_flt_hash_size = 0x78,
	.v4_flt_hash_size_ddr = 0x4000,
	.v4_flt_nhash_ofst = 0x308,
	.v4_flt_nhash_size = 0x78,
	.v4_flt_nhash_size_ddr = 0x4000,
	.v6_flt_hash_ofst = 0x388,
	.v6_flt_hash_size = 0x78,
	.v6_flt_hash_size_ddr = 0x4000,
	.v6_flt_nhash_ofst = 0x408,
	.v6_flt_nhash_size = 0x78,
	.v6_flt_nhash_size_ddr = 0x4000,
	.v4_rt_num_index = 0xf,
	.v4_modem_rt_index_lo = 0x0,
	.v4_modem_rt_index_hi = 0x7,
	.v4_apps_rt_index_lo = 0x8,
	.v4_apps_rt_index_hi = 0xe,
	.v4_rt_hash_ofst = 0x488,
	.v4_rt_hash_size = 0x78,
	.v4_rt_hash_size_ddr = 0x4000,
	.v4_rt_nhash_ofst = 0x508,
	.v4_rt_nhash_size = 0x78,
	.v4_rt_nhash_size_ddr = 0x4000,
	.v6_rt_num_index = 0xf,
	.v6_modem_rt_index_lo = 0x0,
	.v6_modem_rt_index_hi = 0x7,
	.v6_apps_rt_index_lo = 0x8,
	.v6_apps_rt_index_hi = 0xe,
	.v6_rt_hash_ofst = 0x588,
	.v6_rt_hash_size = 0x78,
	.v6_rt_hash_size_ddr = 0x4000,
	.v6_rt_nhash_ofst = 0x608,
	.v6_rt_nhash_size = 0x78,
	.v6_rt_nhash_size_ddr = 0x4000,
	.modem_hdr_ofst = 0x688,
	.modem_hdr_size = 0x140,
	.apps_hdr_ofst = 0x7c8,
	.apps_hdr_size = 0x0,
	.apps_hdr_size_ddr = 0x800,
	.modem_hdr_proc_ctx_ofst = 0x7d0,
	.modem_hdr_proc_ctx_size = 0x200,
	.apps_hdr_proc_ctx_ofst = 0x9d0,
	.apps_hdr_proc_ctx_size = 0x200,
	.apps_hdr_proc_ctx_size_ddr = 0x0,
	.modem_comp_decomp_ofst = 0x0,
	.modem_comp_decomp_size = 0x0,
	.modem_ofst = 0xBD8,
	.modem_size = 0x1424,
	.apps_v4_flt_hash_ofst = 0x2000,
	.apps_v4_flt_hash_size = 0x0,
	.apps_v4_flt_nhash_ofst = 0x2000,
	.apps_v4_flt_nhash_size = 0x0,
	.apps_v6_flt_hash_ofst = 0x2000,
	.apps_v6_flt_hash_size = 0x0,
	.apps_v6_flt_nhash_ofst = 0x2000,
	.apps_v6_flt_nhash_size = 0x0,
	.uc_info_ofst = 0x80,
	.uc_info_size = 0x200,
	.end_ofst = 0x2000,
	.apps_v4_rt_hash_ofst = 0x2000,
	.apps_v4_rt_hash_size = 0x0,
	.apps_v4_rt_nhash_ofst = 0x2000,
	.apps_v4_rt_nhash_size = 0x0,
	.apps_v6_rt_hash_ofst = 0x2000,
	.apps_v6_rt_hash_size = 0x0,
	.apps_v6_rt_nhash_ofst = 0x2000,
	.apps_v6_rt_nhash_size = 0x0,
	.uc_descriptor_ram_ofst = 0x2000,
	.uc_descriptor_ram_size = 0x0,
	.pdn_config_ofst = 0x2000,
	.pdn_config_size = 0x0,
	.stats_quota_q6_ofst = 0x2000,
	.stats_quota_q6_size = 0x0,
	.stats_quota_ap_ofst = 0,
	.stats_quota_ap_size = 0,
	.stats_tethering_ofst = 0x2000,
	.stats_tethering_size = 0x0,
	.stats_flt_v4_ofst = 0x2000,
	.stats_flt_v4_size = 0x0,
	.stats_flt_v6_ofst = 0x2000,
	.stats_flt_v6_size = 0x0,
	.stats_rt_v4_ofst = 0x2000,
	.stats_rt_v4_size = 0x0,
	.stats_rt_v6_ofst = 0x2000,
	.stats_rt_v6_size = 0x0,
	.stats_drop_ofst = 0x2000,
	.stats_drop_size = 0x0,
};

static struct ipa3_mem_partition ipa_4_1_mem_part = {
	.uc_ofst				= 0x0,
	.uc_size				= 0x80,
	.ofst_start				= 0x280,
	.v4_flt_hash_ofst		= 0x288,
	.v4_flt_hash_size		=  0x78,
	.v4_flt_hash_size_ddr		= 0x4000,
	.v4_flt_nhash_ofst		= 0x308,
	.v4_flt_nhash_size		= 0x78,
	.v4_flt_nhash_size_ddr		= 0x4000,
	.v6_flt_hash_ofst		= 0x388,
	.v6_flt_hash_size		= 0x78,
	.v6_flt_hash_size_ddr		= 0x4000,
	.v6_flt_nhash_ofst		= 0x408,
	.v6_flt_nhash_size		= 0x78,
	.v6_flt_nhash_size_ddr		= 0x4000,
	.v4_rt_num_index		= 0xf,
	.v4_modem_rt_index_lo		= 0x0,
	.v4_modem_rt_index_hi		= 0x7,
	.v4_apps_rt_index_lo		= 0x8,
	.v4_apps_rt_index_hi		= 0xe,
	.v4_rt_hash_ofst		= 0x488,
	.v4_rt_hash_size		= 0x78,
	.v4_rt_hash_size_ddr		= 0x4000,
	.v4_rt_nhash_ofst		= 0x508,
	.v4_rt_nhash_size		= 0x78,
	.v4_rt_nhash_size_ddr		= 0x4000,
	.v6_rt_num_index		= 0xf,
	.v6_modem_rt_index_lo		= 0x0,
	.v6_modem_rt_index_hi		= 0x7,
	.v6_apps_rt_index_lo		= 0x8,
	.v6_apps_rt_index_hi		= 0xe,
	.v6_rt_hash_ofst		= 0x588,
	.v6_rt_hash_size		= 0x78,
	.v6_rt_hash_size_ddr		= 0x4000,
	.v6_rt_nhash_ofst		= 0x608,
	.v6_rt_nhash_size		= 0x78,
	.v6_rt_nhash_size_ddr		= 0x4000,
	.modem_hdr_ofst			= 0x688,
	.modem_hdr_size			= 0x140,
	.apps_hdr_ofst			= 0x7c8,
	.apps_hdr_size			= 0x0,
	.apps_hdr_size_ddr		= 0x800,
	.modem_hdr_proc_ctx_ofst	= 0x7d0,
	.modem_hdr_proc_ctx_size	= 0x200,
	.apps_hdr_proc_ctx_ofst		= 0x9d0,
	.apps_hdr_proc_ctx_size		= 0x200,
	.apps_hdr_proc_ctx_size_ddr	= 0x0,
	.modem_comp_decomp_ofst		= 0x0,
	.modem_comp_decomp_size		= 0x0,
	.modem_ofst			= 0x13f0,
	.modem_size			= 0x100c,
	.apps_v4_flt_hash_ofst		= 0x23fc,
	.apps_v4_flt_hash_size		= 0x0,
	.apps_v4_flt_nhash_ofst		= 0x23fc,
	.apps_v4_flt_nhash_size		= 0x0,
	.apps_v6_flt_hash_ofst		= 0x23fc,
	.apps_v6_flt_hash_size		= 0x0,
	.apps_v6_flt_nhash_ofst		= 0x23fc,
	.apps_v6_flt_nhash_size		= 0x0,
	.uc_info_ofst			= 0x80,
	.uc_info_size			= 0x200,
	.end_ofst			= 0x2800,
	.apps_v4_rt_hash_ofst		= 0x23fc,
	.apps_v4_rt_hash_size		= 0x0,
	.apps_v4_rt_nhash_ofst		= 0x23fc,
	.apps_v4_rt_nhash_size		= 0x0,
	.apps_v6_rt_hash_ofst		= 0x23fc,
	.apps_v6_rt_hash_size		= 0x0,
	.apps_v6_rt_nhash_ofst		= 0x23fc,
	.apps_v6_rt_nhash_size		= 0x0,
	.uc_descriptor_ram_ofst		= 0x2400,
	.uc_descriptor_ram_size		= 0x400,
	.pdn_config_ofst		= 0xbd8,
	.pdn_config_size		= 0x50,
	.stats_quota_q6_ofst		= 0xc30,
	.stats_quota_q6_size		= 0x60,
	.stats_quota_ap_ofst		= 0,
	.stats_quota_ap_size		= 0,
	.stats_tethering_ofst		= 0xc90,
	.stats_tethering_size		= 0x140,
	.stats_flt_v4_ofst		= 0xdd0,
	.stats_flt_v4_size		= 0x180,
	.stats_flt_v6_ofst		= 0xf50,
	.stats_flt_v6_size		= 0x180,
	.stats_rt_v4_ofst		= 0x10d0,
	.stats_rt_v4_size		= 0x180,
	.stats_rt_v6_ofst		= 0x1250,
	.stats_rt_v6_size		= 0x180,
	.stats_drop_ofst		= 0x13d0,
	.stats_drop_size		= 0x20,
};

static struct ipa3_mem_partition ipa_4_2_mem_part = {
	.uc_ofst				= 0x0,
	.uc_size				= 0x80,
	.ofst_start				= 0x280,
	.v4_flt_hash_ofst		= 0x288,
	.v4_flt_hash_size		= 0x0,
	.v4_flt_hash_size_ddr		= 0x0,
	.v4_flt_nhash_ofst		= 0x290,
	.v4_flt_nhash_size		= 0x78,
	.v4_flt_nhash_size_ddr		= 0x4000,
	.v6_flt_hash_ofst		= 0x310,
	.v6_flt_hash_size		= 0x0,
	.v6_flt_hash_size_ddr		= 0x0,
	.v6_flt_nhash_ofst		= 0x318,
	.v6_flt_nhash_size		= 0x78,
	.v6_flt_nhash_size_ddr		= 0x4000,
	.v4_rt_num_index		= 0xf,
	.v4_modem_rt_index_lo		= 0x0,
	.v4_modem_rt_index_hi		= 0x7,
	.v4_apps_rt_index_lo		= 0x8,
	.v4_apps_rt_index_hi		= 0xe,
	.v4_rt_hash_ofst		= 0x398,
	.v4_rt_hash_size		= 0x0,
	.v4_rt_hash_size_ddr		= 0x0,
	.v4_rt_nhash_ofst		= 0x3A0,
	.v4_rt_nhash_size		= 0x78,
	.v4_rt_nhash_size_ddr		= 0x4000,
	.v6_rt_num_index		= 0xf,
	.v6_modem_rt_index_lo		= 0x0,
	.v6_modem_rt_index_hi		= 0x7,
	.v6_apps_rt_index_lo		= 0x8,
	.v6_apps_rt_index_hi		= 0xe,
	.v6_rt_hash_ofst		= 0x420,
	.v6_rt_hash_size		= 0x0,
	.v6_rt_hash_size_ddr		= 0x0,
	.v6_rt_nhash_ofst		= 0x428,
	.v6_rt_nhash_size		= 0x78,
	.v6_rt_nhash_size_ddr		= 0x4000,
	.modem_hdr_ofst			= 0x4A8,
	.modem_hdr_size			= 0x140,
	.apps_hdr_ofst			= 0x5E8,
	.apps_hdr_size			= 0x0,
	.apps_hdr_size_ddr		= 0x800,
	.modem_hdr_proc_ctx_ofst	= 0x5F0,
	.modem_hdr_proc_ctx_size	= 0x200,
	.apps_hdr_proc_ctx_ofst		= 0x7F0,
	.apps_hdr_proc_ctx_size		= 0x200,
	.apps_hdr_proc_ctx_size_ddr	= 0x0,
	.modem_comp_decomp_ofst		= 0x0,
	.modem_comp_decomp_size		= 0x0,
	.modem_ofst			= 0xbf0,
	.modem_size			= 0x140c,
	.apps_v4_flt_hash_ofst		= 0x1bfc,
	.apps_v4_flt_hash_size		= 0x0,
	.apps_v4_flt_nhash_ofst		= 0x1bfc,
	.apps_v4_flt_nhash_size		= 0x0,
	.apps_v6_flt_hash_ofst		= 0x1bfc,
	.apps_v6_flt_hash_size		= 0x0,
	.apps_v6_flt_nhash_ofst		= 0x1bfc,
	.apps_v6_flt_nhash_size		= 0x0,
	.uc_info_ofst			= 0x80,
	.uc_info_size			= 0x200,
	.end_ofst			= 0x2000,
	.apps_v4_rt_hash_ofst		= 0x1bfc,
	.apps_v4_rt_hash_size		= 0x0,
	.apps_v4_rt_nhash_ofst		= 0x1bfc,
	.apps_v4_rt_nhash_size		= 0x0,
	.apps_v6_rt_hash_ofst		= 0x1bfc,
	.apps_v6_rt_hash_size		= 0x0,
	.apps_v6_rt_nhash_ofst		= 0x1bfc,
	.apps_v6_rt_nhash_size		= 0x0,
	.uc_descriptor_ram_ofst		= 0x2000,
	.uc_descriptor_ram_size		= 0x0,
	.pdn_config_ofst		= 0x9F8,
	.pdn_config_size		= 0x50,
	.stats_quota_q6_ofst		= 0xa50,
	.stats_quota_q6_size		= 0x60,
	.stats_quota_ap_ofst		= 0,
	.stats_quota_ap_size		= 0,
	.stats_tethering_ofst		= 0xab0,
	.stats_tethering_size		= 0x140,
	.stats_flt_v4_ofst		= 0xbf0,
	.stats_flt_v4_size		= 0x0,
	.stats_flt_v6_ofst		= 0xbf0,
	.stats_flt_v6_size		= 0x0,
	.stats_rt_v4_ofst		= 0xbf0,
	.stats_rt_v4_size		= 0x0,
	.stats_rt_v6_ofst		= 0xbf0,
	.stats_rt_v6_size		= 0x0,
	.stats_drop_ofst		= 0xbf0,
	.stats_drop_size		= 0x0,
};

static struct ipa3_mem_partition ipa_4_5_mem_part = {
	.uc_ofst				= 0x0,
	.uc_size				= 0x80,
	.uc_info_ofst			= 0x80,
	.uc_info_size			= 0x200,
	.ofst_start			= 0x280,
	.v4_flt_hash_ofst		= 0x288,
	.v4_flt_hash_size		= 0x78,
	.v4_flt_hash_size_ddr		= 0x4000,
	.v4_flt_nhash_ofst		= 0x308,
	.v4_flt_nhash_size		= 0x78,
	.v4_flt_nhash_size_ddr		= 0x4000,
	.v6_flt_hash_ofst		= 0x388,
	.v6_flt_hash_size		= 0x78,
	.v6_flt_hash_size_ddr		= 0x4000,
	.v6_flt_nhash_ofst		= 0x408,
	.v6_flt_nhash_size		= 0x78,
	.v6_flt_nhash_size_ddr		= 0x4000,
	.v4_rt_num_index		= 0xf,
	.v4_modem_rt_index_lo		= 0x0,
	.v4_modem_rt_index_hi		= 0x7,
	.v4_apps_rt_index_lo		= 0x8,
	.v4_apps_rt_index_hi		= 0xe,
	.v4_rt_hash_ofst		= 0x488,
	.v4_rt_hash_size		= 0x78,
	.v4_rt_hash_size_ddr		= 0x4000,
	.v4_rt_nhash_ofst		= 0x508,
	.v4_rt_nhash_size		= 0x78,
	.v4_rt_nhash_size_ddr		= 0x4000,
	.v6_rt_num_index		= 0xf,
	.v6_modem_rt_index_lo		= 0x0,
	.v6_modem_rt_index_hi		= 0x7,
	.v6_apps_rt_index_lo		= 0x8,
	.v6_apps_rt_index_hi		= 0xe,
	.v6_rt_hash_ofst		= 0x588,
	.v6_rt_hash_size		= 0x78,
	.v6_rt_hash_size_ddr		= 0x4000,
	.v6_rt_nhash_ofst		= 0x608,
	.v6_rt_nhash_size		= 0x78,
	.v6_rt_nhash_size_ddr		= 0x4000,
	.modem_hdr_ofst			= 0x688,
	.modem_hdr_size			= 0x240,
	.apps_hdr_ofst			= 0x8c8,
	.apps_hdr_size			= 0x200,
	.apps_hdr_size_ddr		= 0x800,
	.modem_hdr_proc_ctx_ofst	= 0xad0,
	.modem_hdr_proc_ctx_size	= 0xb20,
	.apps_hdr_proc_ctx_ofst		= 0x15f0,
	.apps_hdr_proc_ctx_size		= 0x200,
	.apps_hdr_proc_ctx_size_ddr	= 0x0,
	.nat_tbl_ofst			= 0x1800,
	.nat_tbl_size			= 0xd00,
	.stats_quota_q6_ofst		= 0x2510,
	.stats_quota_q6_size		= 0x30,
	.stats_quota_ap_ofst		= 0x2540,
	.stats_quota_ap_size		= 0x48,
	.stats_tethering_ofst		= 0x2588,
	.stats_tethering_size		= 0x238,
	.stats_flt_v4_ofst		= 0,
	.stats_flt_v4_size		= 0,
	.stats_flt_v6_ofst		= 0,
	.stats_flt_v6_size		= 0,
	.stats_rt_v4_ofst		= 0,
	.stats_rt_v4_size		= 0,
	.stats_rt_v6_ofst		= 0,
	.stats_rt_v6_size		= 0,
	.stats_fnr_ofst			= 0x27c0,
	.stats_fnr_size			= 0x800,
	.stats_drop_ofst		= 0x2fc0,
	.stats_drop_size		= 0x20,
	.modem_comp_decomp_ofst		= 0x0,
	.modem_comp_decomp_size		= 0x0,
	.modem_ofst			= 0x2fe8,
	.modem_size			= 0x800,
	.apps_v4_flt_hash_ofst	= 0x2718,
	.apps_v4_flt_hash_size	= 0x0,
	.apps_v4_flt_nhash_ofst	= 0x2718,
	.apps_v4_flt_nhash_size	= 0x0,
	.apps_v6_flt_hash_ofst	= 0x2718,
	.apps_v6_flt_hash_size	= 0x0,
	.apps_v6_flt_nhash_ofst	= 0x2718,
	.apps_v6_flt_nhash_size	= 0x0,
	.apps_v4_rt_hash_ofst	= 0x2718,
	.apps_v4_rt_hash_size	= 0x0,
	.apps_v4_rt_nhash_ofst	= 0x2718,
	.apps_v4_rt_nhash_size	= 0x0,
	.apps_v6_rt_hash_ofst	= 0x2718,
	.apps_v6_rt_hash_size	= 0x0,
	.apps_v6_rt_nhash_ofst	= 0x2718,
	.apps_v6_rt_nhash_size	= 0x0,
	.uc_descriptor_ram_ofst	= 0x3800,
	.uc_descriptor_ram_size	= 0x1000,
	.pdn_config_ofst	= 0x4800,
	.pdn_config_size	= 0x50,
	.end_ofst		= 0x4850,
};

static struct ipa3_mem_partition ipa_4_7_mem_part = {
	.uc_ofst				= 0x0,
	.uc_size				= 0x80,
	.uc_info_ofst			= 0x80,
	.uc_info_size			= 0x200,
	.ofst_start			= 0x280,
	.v4_flt_hash_ofst		= 0x288,
	.v4_flt_hash_size		=  0x78,
	.v4_flt_hash_size_ddr		= 0x4000,
	.v4_flt_nhash_ofst		= 0x308,
	.v4_flt_nhash_size		= 0x78,
	.v4_flt_nhash_size_ddr		= 0x4000,
	.v6_flt_hash_ofst		= 0x388,
	.v6_flt_hash_size		= 0x78,
	.v6_flt_hash_size_ddr		= 0x4000,
	.v6_flt_nhash_ofst		= 0x408,
	.v6_flt_nhash_size		= 0x78,
	.v6_flt_nhash_size_ddr		= 0x4000,
	.v4_rt_num_index		= 0xf,
	.v4_modem_rt_index_lo		= 0x0,
	.v4_modem_rt_index_hi		= 0x7,
	.v4_apps_rt_index_lo		= 0x8,
	.v4_apps_rt_index_hi		= 0xe,
	.v4_rt_hash_ofst		= 0x488,
	.v4_rt_hash_size		= 0x78,
	.v4_rt_hash_size_ddr		= 0x4000,
	.v4_rt_nhash_ofst		= 0x508,
	.v4_rt_nhash_size		= 0x78,
	.v4_rt_nhash_size_ddr		= 0x4000,
	.v6_rt_num_index		= 0xf,
	.v6_modem_rt_index_lo		= 0x0,
	.v6_modem_rt_index_hi		= 0x7,
	.v6_apps_rt_index_lo		= 0x8,
	.v6_apps_rt_index_hi		= 0xe,
	.v6_rt_hash_ofst		= 0x588,
	.v6_rt_hash_size		= 0x78,
	.v6_rt_hash_size_ddr		= 0x4000,
	.v6_rt_nhash_ofst		= 0x608,
	.v6_rt_nhash_size		= 0x78,
	.v6_rt_nhash_size_ddr		= 0x4000,
	.modem_hdr_ofst			= 0x688,
	.modem_hdr_size			= 0x240,
	.apps_hdr_ofst			= 0x8c8,
	.apps_hdr_size			= 0x200,
	.apps_hdr_size_ddr		= 0x800,
	.modem_hdr_proc_ctx_ofst	= 0xad0,
	.modem_hdr_proc_ctx_size	= 0x200,
	.apps_hdr_proc_ctx_ofst		= 0xcd0,
	.apps_hdr_proc_ctx_size		= 0x200,
	.apps_hdr_proc_ctx_size_ddr	= 0x0,
	.nat_tbl_ofst			= 0xee0,
	.nat_tbl_size			= 0xd00,
	.pdn_config_ofst		= 0x1be8,
	.pdn_config_size		= 0x50,
	.stats_quota_q6_ofst		= 0x1c40,
	.stats_quota_q6_size		= 0x30,
	.stats_quota_ap_ofst		= 0x1c70,
	.stats_quota_ap_size		= 0x48,
	.stats_tethering_ofst		= 0x1cb8,
	.stats_tethering_size		= 0x238,
	.stats_flt_v4_ofst		= 0,
	.stats_flt_v4_size		= 0,
	.stats_flt_v6_ofst		= 0,
	.stats_flt_v6_size		= 0,
	.stats_rt_v4_ofst		= 0,
	.stats_rt_v4_size		= 0,
	.stats_rt_v6_ofst		= 0,
	.stats_rt_v6_size		= 0,
	.stats_fnr_ofst			= 0x1ef0,
	.stats_fnr_size			= 0x0,
	.stats_drop_ofst		= 0x1ef0,
	.stats_drop_size		= 0x20,
	.modem_comp_decomp_ofst		= 0x0,
	.modem_comp_decomp_size		= 0x0,
	.modem_ofst			= 0x1f18,
	.modem_size			= 0x100c,
	.apps_v4_flt_hash_ofst	= 0x1f18,
	.apps_v4_flt_hash_size	= 0x0,
	.apps_v4_flt_nhash_ofst	= 0x1f18,
	.apps_v4_flt_nhash_size	= 0x0,
	.apps_v6_flt_hash_ofst	= 0x1f18,
	.apps_v6_flt_hash_size	= 0x0,
	.apps_v6_flt_nhash_ofst	= 0x1f18,
	.apps_v6_flt_nhash_size	= 0x0,
	.apps_v4_rt_hash_ofst	= 0x1f18,
	.apps_v4_rt_hash_size	= 0x0,
	.apps_v4_rt_nhash_ofst	= 0x1f18,
	.apps_v4_rt_nhash_size	= 0x0,
	.apps_v6_rt_hash_ofst	= 0x1f18,
	.apps_v6_rt_hash_size	= 0x0,
	.apps_v6_rt_nhash_ofst	= 0x1f18,
	.apps_v6_rt_nhash_size	= 0x0,
	.uc_descriptor_ram_ofst	= 0x3000,
	.uc_descriptor_ram_size	= 0x0000,
	.end_ofst		= 0x3000,
};

static struct ipa3_mem_partition ipa_4_9_mem_part = {
	.uc_ofst				= 0x0,
	.uc_size				= 0x80,
	.uc_info_ofst			= 0x80,
	.uc_info_size			= 0x200,
	.ofst_start			= 0x280,
	.v4_flt_hash_ofst		= 0x288,
	.v4_flt_hash_size		= 0x78,
	.v4_flt_hash_size_ddr		= 0x4000,
	.v4_flt_nhash_ofst		= 0x308,
	.v4_flt_nhash_size		= 0x78,
	.v4_flt_nhash_size_ddr		= 0x4000,
	.v6_flt_hash_ofst		= 0x388,
	.v6_flt_hash_size		= 0x78,
	.v6_flt_hash_size_ddr		= 0x4000,
	.v6_flt_nhash_ofst		= 0x408,
	.v6_flt_nhash_size		= 0x78,
	.v6_flt_nhash_size_ddr		= 0x4000,
	.v4_rt_num_index		= 0xf,
	.v4_modem_rt_index_lo		= 0x0,
	.v4_modem_rt_index_hi		= 0x7,
	.v4_apps_rt_index_lo		= 0x8,
	.v4_apps_rt_index_hi		= 0xe,
	.v4_rt_hash_ofst		= 0x488,
	.v4_rt_hash_size		= 0x78,
	.v4_rt_hash_size_ddr		= 0x4000,
	.v4_rt_nhash_ofst		= 0x508,
	.v4_rt_nhash_size		= 0x78,
	.v4_rt_nhash_size_ddr		= 0x4000,
	.v6_rt_num_index		= 0xf,
	.v6_modem_rt_index_lo		= 0x0,
	.v6_modem_rt_index_hi		= 0x7,
	.v6_apps_rt_index_lo		= 0x8,
	.v6_apps_rt_index_hi		= 0xe,
	.v6_rt_hash_ofst		= 0x588,
	.v6_rt_hash_size		= 0x78,
	.v6_rt_hash_size_ddr		= 0x4000,
	.v6_rt_nhash_ofst		= 0x608,
	.v6_rt_nhash_size		= 0x78,
	.v6_rt_nhash_size_ddr		= 0x4000,
	.modem_hdr_ofst			= 0x688,
	.modem_hdr_size			= 0x240,
	.apps_hdr_ofst			= 0x8c8,
	.apps_hdr_size			= 0x200,
	.apps_hdr_size_ddr		= 0x800,
	.modem_hdr_proc_ctx_ofst	= 0xad0,
	.modem_hdr_proc_ctx_size	= 0xb20,
	.apps_hdr_proc_ctx_ofst		= 0x15f0,
	.apps_hdr_proc_ctx_size		= 0x200,
	.apps_hdr_proc_ctx_size_ddr	= 0x0,
	.nat_tbl_ofst            = 0x1800,
	.nat_tbl_size            = 0xd00,
	.stats_quota_q6_ofst		= 0x2510,
	.stats_quota_q6_size		= 0x30,
	.stats_quota_ap_ofst		= 0x2540,
	.stats_quota_ap_size		= 0x48,
	.stats_tethering_ofst		= 0x2588,
	.stats_tethering_size		= 0x238,
	.stats_flt_v4_ofst		= 0,
	.stats_flt_v4_size		= 0,
	.stats_flt_v6_ofst		= 0,
	.stats_flt_v6_size		= 0,
	.stats_rt_v4_ofst		= 0,
	.stats_rt_v4_size		= 0,
	.stats_rt_v6_ofst		= 0,
	.stats_rt_v6_size		= 0,
	.stats_fnr_ofst			= 0x27c0,
	.stats_fnr_size			= 0x800,
	.stats_drop_ofst		= 0x2fc0,
	.stats_drop_size		= 0x20,
	.modem_comp_decomp_ofst		= 0x0,
	.modem_comp_decomp_size		= 0x0,
	.modem_ofst			= 0x2fe8,
	.modem_size			= 0x800,
	.apps_v4_flt_hash_ofst	= 0x2718,
	.apps_v4_flt_hash_size	= 0x0,
	.apps_v4_flt_nhash_ofst	= 0x2718,
	.apps_v4_flt_nhash_size	= 0x0,
	.apps_v6_flt_hash_ofst	= 0x2718,
	.apps_v6_flt_hash_size	= 0x0,
	.apps_v6_flt_nhash_ofst	= 0x2718,
	.apps_v6_flt_nhash_size	= 0x0,
	.apps_v4_rt_hash_ofst	= 0x2718,
	.apps_v4_rt_hash_size	= 0x0,
	.apps_v4_rt_nhash_ofst	= 0x2718,
	.apps_v4_rt_nhash_size	= 0x0,
	.apps_v6_rt_hash_ofst	= 0x2718,
	.apps_v6_rt_hash_size	= 0x0,
	.apps_v6_rt_nhash_ofst	= 0x2718,
	.apps_v6_rt_nhash_size	= 0x0,
	.uc_descriptor_ram_ofst	= 0x3800,
	.uc_descriptor_ram_size	= 0x1000,
	.pdn_config_ofst	= 0x4800,
	.pdn_config_size	= 0x50,
	.end_ofst		= 0x4850,
};

static struct ipa3_mem_partition ipa_4_11_mem_part = {
        .uc_ofst                        = 0x0,
        .uc_size                        = 0x80,
        .uc_info_ofst                   = 0x80,
        .uc_info_size                   = 0x200,
        .ofst_start                     = 0x280,
        .v4_flt_hash_ofst               = 0x288,
        .v4_flt_hash_size               =  0x78,
        .v4_flt_hash_size_ddr           = 0x4000,
        .v4_flt_nhash_ofst              = 0x308,
        .v4_flt_nhash_size              = 0x78,
        .v4_flt_nhash_size_ddr          = 0x4000,
        .v6_flt_hash_ofst               = 0x388,
        .v6_flt_hash_size               = 0x78,
        .v6_flt_hash_size_ddr           = 0x4000,
        .v6_flt_nhash_ofst              = 0x408,
        .v6_flt_nhash_size              = 0x78,
        .v6_flt_nhash_size_ddr          = 0x4000,
        .v4_rt_num_index                = 0xf,
        .v4_modem_rt_index_lo           = 0x0,
        .v4_modem_rt_index_hi           = 0x7,
        .v4_apps_rt_index_lo            = 0x8,
        .v4_apps_rt_index_hi            = 0xe,
        .v4_rt_hash_ofst                = 0x488,
        .v4_rt_hash_size                = 0x78,
        .v4_rt_hash_size_ddr            = 0x4000,
        .v4_rt_nhash_ofst               = 0x508,
        .v4_rt_nhash_size               = 0x78,
        .v4_rt_nhash_size_ddr           = 0x4000,
        .v6_rt_num_index                = 0xf,
        .v6_modem_rt_index_lo           = 0x0,
        .v6_modem_rt_index_hi           = 0x7,
        .v6_apps_rt_index_lo            = 0x8,
        .v6_apps_rt_index_hi            = 0xe,
        .v6_rt_hash_ofst                = 0x588,
        .v6_rt_hash_size                = 0x78,
        .v6_rt_hash_size_ddr            = 0x4000,
        .v6_rt_nhash_ofst               = 0x608,
        .v6_rt_nhash_size               = 0x78,
        .v6_rt_nhash_size_ddr           = 0x4000,
        .modem_hdr_ofst                 = 0x688,
        .modem_hdr_size                 = 0x240,
        .apps_hdr_ofst                  = 0x8c8,
        .apps_hdr_size                  = 0x200,
        .apps_hdr_size_ddr              = 0x800,
        .modem_hdr_proc_ctx_ofst        = 0xad0,
        .modem_hdr_proc_ctx_size        = 0xAC0,
        .apps_hdr_proc_ctx_ofst         = 0x1590,
        .apps_hdr_proc_ctx_size         = 0x200,
        .apps_hdr_proc_ctx_size_ddr     = 0x0,
        .nat_tbl_ofst                   = 0x17A0,
        .nat_tbl_size                   = 0x800,
        .pdn_config_ofst                = 0x24A8,
        .pdn_config_size                = 0x50,
        .stats_quota_q6_ofst            = 0x2500,
        .stats_quota_q6_size            = 0x30,
        .stats_quota_ap_ofst            = 0x2530,
        .stats_quota_ap_size            = 0x48,
        .stats_tethering_ofst           = 0x2578,
        .stats_tethering_size           = 0x238,
        .stats_flt_v4_ofst              = 0,
        .stats_flt_v4_size              = 0,
        .stats_flt_v6_ofst              = 0,
        .stats_flt_v6_size              = 0,
        .stats_rt_v4_ofst               = 0,
        .stats_rt_v4_size               = 0,
        .stats_rt_v6_ofst               = 0,
        .stats_rt_v6_size               = 0,
        .stats_fnr_ofst                 = 0x27B0,
        .stats_fnr_size                 = 0x0,
        .stats_drop_ofst                = 0x27B0,
        .stats_drop_size                = 0x20,
        .modem_comp_decomp_ofst         = 0x0,
        .modem_comp_decomp_size         = 0x0,
        .modem_ofst                     = 0x27D8,
        .modem_size                     = 0x800,
        .apps_v4_flt_hash_ofst  = 0x27B0,
        .apps_v4_flt_hash_size  = 0x0,
        .apps_v4_flt_nhash_ofst = 0x27B0,
        .apps_v4_flt_nhash_size = 0x0,
        .apps_v6_flt_hash_ofst  = 0x27B0,
        .apps_v6_flt_hash_size  = 0x0,
        .apps_v6_flt_nhash_ofst = 0x27B0,
        .apps_v6_flt_nhash_size = 0x0,
        .apps_v4_rt_hash_ofst   = 0x27B0,
        .apps_v4_rt_hash_size   = 0x0,
        .apps_v4_rt_nhash_ofst  = 0x27B0,
        .apps_v4_rt_nhash_size  = 0x0,
        .apps_v6_rt_hash_ofst   = 0x27B0,
        .apps_v6_rt_hash_size   = 0x0,
        .apps_v6_rt_nhash_ofst  = 0x27B0,
        .apps_v6_rt_nhash_size  = 0x0,
        .uc_descriptor_ram_ofst = 0x3000,
        .uc_descriptor_ram_size = 0x0000,
        .end_ofst               = 0x3000,
};

static struct ipa3_mem_partition ipa_5_0_mem_part = {
	.uc_descriptor_ram_ofst = 0x0,
	.uc_descriptor_ram_size = 0x1000,
	.uc_ofst = 0x1000,
	.uc_size = 0x80,
	.uc_info_ofst = 0x1080,
	.uc_info_size = 0x200,
	.ofst_start = 0x1280,
	.v4_flt_hash_ofst = 0x1288,
	.v4_flt_hash_size = 0x78,
	.v4_flt_hash_size_ddr = 0x4000,
	.v4_flt_nhash_ofst = 0x1308,
	.v4_flt_nhash_size = 0x78,
	.v4_flt_nhash_size_ddr = 0x4000,
	.v6_flt_hash_ofst = 0x1388,
	.v6_flt_hash_size = 0x78,
	.v6_flt_hash_size_ddr = 0x4000,
	.v6_flt_nhash_ofst = 0x1408,
	.v6_flt_nhash_size = 0x78,
	.v6_flt_nhash_size_ddr = 0x4000,
	.v4_rt_num_index = 0x13,
	.v4_modem_rt_index_lo = 0x0,
	.v4_modem_rt_index_hi = 0xa,
	.v4_apps_rt_index_lo = 0xb,
	.v4_apps_rt_index_hi = 0x12,
	.v4_rt_hash_ofst = 0x1488,
	.v4_rt_hash_size = 0x98,
	.v4_rt_hash_size_ddr = 0x4000,
	.v4_rt_nhash_ofst = 0x1528,
	.v4_rt_nhash_size = 0x98,
	.v4_rt_nhash_size_ddr = 0x4000,
	.v6_rt_num_index = 0x13,
	.v6_modem_rt_index_lo = 0x0,
	.v6_modem_rt_index_hi = 0xa,
	.v6_apps_rt_index_lo = 0xb,
	.v6_apps_rt_index_hi = 0x12,
	.v6_rt_hash_ofst = 0x15c8,
	.v6_rt_hash_size = 0x98,
	.v6_rt_hash_size_ddr = 0x4000,
	.v6_rt_nhash_ofst = 0x1668,
	.v6_rt_nhash_size = 0x098,
	.v6_rt_nhash_size_ddr = 0x4000,
	.modem_hdr_ofst = 0x1708,
	.modem_hdr_size = 0x240,
	.apps_hdr_ofst = 0x1948,
	.apps_hdr_size = 0x1e0,
	.apps_hdr_size_ddr = 0x800,
	.modem_hdr_proc_ctx_ofst = 0x1b40,
	.modem_hdr_proc_ctx_size = 0xb20,
	.apps_hdr_proc_ctx_ofst = 0x2660,
	.apps_hdr_proc_ctx_size = 0x200,
	.apps_hdr_proc_ctx_size_ddr = 0x0,
	.stats_quota_q6_ofst = 0x2868,
	.stats_quota_q6_size = 0x60,
	.stats_quota_ap_ofst = 0x28C8,
	.stats_quota_ap_size = 0x48,
	.stats_tethering_ofst = 0x2910,
	.stats_tethering_size = 0x0,
	.apps_v4_flt_nhash_ofst = 0x2918,
	.apps_v4_flt_nhash_size = 0x188,
	.apps_v6_flt_nhash_ofst = 0x2aa0,
	.apps_v6_flt_nhash_size = 0x228,
	.stats_flt_v4_ofst = 0,
	.stats_flt_v4_size = 0,
	.stats_flt_v6_ofst = 0,
	.stats_flt_v6_size = 0,
	.stats_rt_v4_ofst = 0,
	.stats_rt_v4_size = 0,
	.stats_rt_v6_ofst = 0,
	.stats_rt_v6_size = 0,
	.stats_fnr_ofst = 0x2cd0,
	.stats_fnr_size = 0xba0,
	.stats_drop_ofst = 0x3870,
	.stats_drop_size = 0x20,
	.modem_comp_decomp_ofst = 0x0,
	.modem_comp_decomp_size = 0x0,
	.modem_ofst = 0x3898,
	.modem_size = 0xd48,
	.nat_tbl_ofst = 0x45e0,
	.nat_tbl_size = 0x900,
	.apps_v4_flt_hash_ofst = 0x2718,
	.apps_v4_flt_hash_size = 0x0,
	.apps_v6_flt_hash_ofst = 0x2718,
	.apps_v6_flt_hash_size = 0x0,
	.apps_v4_rt_hash_ofst = 0x2718,
	.apps_v4_rt_hash_size = 0x0,
	.apps_v4_rt_nhash_ofst = 0x2718,
	.apps_v4_rt_nhash_size = 0x0,
	.apps_v6_rt_hash_ofst = 0x2718,
	.apps_v6_rt_hash_size = 0x0,
	.apps_v6_rt_nhash_ofst = 0x2718,
	.apps_v6_rt_nhash_size = 0x0,
	.pdn_config_ofst = 0x4ee8,
	.pdn_config_size = 0x100,
	.end_ofst = 0x4fe8,
};

static struct ipa3_mem_partition ipa_5_1_mem_part = {
	.uc_descriptor_ram_ofst = 0x0,
	.uc_descriptor_ram_size = 0x1000,
	.uc_ofst = 0x1000,
	.uc_size = 0x80,
	.uc_info_ofst = 0x1080,
	.uc_info_size = 0x200,
	.ofst_start = 0x1280,
	.v4_flt_hash_ofst = 0x1288,
	.v4_flt_hash_size = 0x78,
	.v4_flt_hash_size_ddr = 0x4000,
	.v4_flt_nhash_ofst = 0x1308,
	.v4_flt_nhash_size = 0x78,
	.v4_flt_nhash_size_ddr = 0x4000,
	.v6_flt_hash_ofst = 0x1388,
	.v6_flt_hash_size = 0x78,
	.v6_flt_hash_size_ddr = 0x4000,
	.v6_flt_nhash_ofst = 0x1408,
	.v6_flt_nhash_size = 0x78,
	.v6_flt_nhash_size_ddr = 0x4000,
	.v4_rt_num_index = 0x13,
	.v4_modem_rt_index_lo = 0x0,
	.v4_modem_rt_index_hi = 0xa,
	.v4_apps_rt_index_lo = 0xb,
	.v4_apps_rt_index_hi = 0x12,
	.v4_rt_hash_ofst = 0x1488,
	.v4_rt_hash_size = 0x98,
	.v4_rt_hash_size_ddr = 0x4000,
	.v4_rt_nhash_ofst = 0x1528,
	.v4_rt_nhash_size = 0x98,
	.v4_rt_nhash_size_ddr = 0x4000,
	.v6_rt_num_index = 0x13,
	.v6_modem_rt_index_lo = 0x0,
	.v6_modem_rt_index_hi = 0xa,
	.v6_apps_rt_index_lo = 0xb,
	.v6_apps_rt_index_hi = 0x12,
	.v6_rt_hash_ofst = 0x15c8,
	.v6_rt_hash_size = 0x98,
	.v6_rt_hash_size_ddr = 0x4000,
	.v6_rt_nhash_ofst = 0x1668,
	.v6_rt_nhash_size = 0x098,
	.v6_rt_nhash_size_ddr = 0x4000,
	.modem_hdr_ofst = 0x1708,
	.modem_hdr_size = 0x240,
	.apps_hdr_ofst = 0x1948,
	.apps_hdr_size = 0x1e0,
	.apps_hdr_size_ddr = 0x800,
	.modem_hdr_proc_ctx_ofst = 0x1b40,
	.modem_hdr_proc_ctx_size = 0xb20,
	.apps_hdr_proc_ctx_ofst = 0x2660,
	.apps_hdr_proc_ctx_size = 0x200,
	.apps_hdr_proc_ctx_size_ddr = 0x0,
	.stats_quota_q6_ofst = 0x2868,
	.stats_quota_q6_size = 0x60,
	.stats_quota_ap_ofst = 0x28C8,
	.stats_quota_ap_size = 0x48,
	.stats_tethering_ofst = 0x2910,
	.stats_tethering_size = 0x3c0,
	.stats_flt_v4_ofst = 0,
	.stats_flt_v4_size = 0,
	.stats_flt_v6_ofst = 0,
	.stats_flt_v6_size = 0,
	.stats_rt_v4_ofst = 0,
	.stats_rt_v4_size = 0,
	.stats_rt_v6_ofst = 0,
	.stats_rt_v6_size = 0,
	.stats_fnr_ofst = 0x2cd0,
	.stats_fnr_size = 0xba0,
	.stats_drop_ofst = 0x3870,
	.stats_drop_size = 0x20,
	.modem_comp_decomp_ofst = 0x0,
	.modem_comp_decomp_size = 0x0,
	.modem_ofst = 0x3898,
	.modem_size = 0xd48,
	.nat_tbl_ofst = 0x45e0,
	.nat_tbl_size = 0x900,
	.apps_v4_flt_hash_ofst = 0x2718,
	.apps_v4_flt_hash_size = 0x0,
	.apps_v4_flt_nhash_ofst = 0x2718,
	.apps_v4_flt_nhash_size = 0x0,
	.apps_v6_flt_hash_ofst = 0x2718,
	.apps_v6_flt_hash_size = 0x0,
	.apps_v6_flt_nhash_ofst = 0x2718,
	.apps_v6_flt_nhash_size = 0x0,
	.apps_v4_rt_hash_ofst = 0x2718,
	.apps_v4_rt_hash_size = 0x0,
	.apps_v4_rt_nhash_ofst = 0x2718,
	.apps_v4_rt_nhash_size = 0x0,
	.apps_v6_rt_hash_ofst = 0x2718,
	.apps_v6_rt_hash_size = 0x0,
	.apps_v6_rt_nhash_ofst = 0x2718,
	.apps_v6_rt_nhash_size = 0x0,
	.pdn_config_ofst = 0x4ee8,
	.pdn_config_size = 0x100,
	.end_ofst = 0x4fe8,
};

static struct ipa3_mem_partition ipa_5_2_mem_part = {
	.uc_ofst = 0x0,
	.uc_size = 0x80,
	.uc_info_ofst = 0x80,
	.uc_info_size = 0x200,
	.ofst_start = 0x280,
	.v4_flt_hash_ofst = 0x288,
	.v4_flt_hash_size = 0x78,
	.v4_flt_hash_size_ddr = 0x4000,
	.v4_flt_nhash_ofst = 0x308,
	.v4_flt_nhash_size = 0x78,
	.v4_flt_nhash_size_ddr = 0x4000,
	.v6_flt_hash_ofst = 0x388,
	.v6_flt_hash_size = 0x78,
	.v6_flt_hash_size_ddr = 0x4000,
	.v6_flt_nhash_ofst = 0x408,
	.v6_flt_nhash_size = 0x78,
	.v6_flt_nhash_size_ddr = 0x4000,
	.v4_rt_num_index = 0x13,
	.v4_modem_rt_index_lo = 0x0,
	.v4_modem_rt_index_hi = 0xa,
	.v4_apps_rt_index_lo = 0xb,
	.v4_apps_rt_index_hi = 0x12,
	.v4_rt_hash_ofst = 0x488,
	.v4_rt_hash_size = 0x98,
	.v4_rt_hash_size_ddr = 0x4000,
	.v4_rt_nhash_ofst = 0x528,
	.v4_rt_nhash_size = 0x98,
	.v4_rt_nhash_size_ddr = 0x4000,
	.v6_rt_num_index = 0x13,
	.v6_modem_rt_index_lo = 0x0,
	.v6_modem_rt_index_hi = 0xa,
	.v6_apps_rt_index_lo = 0xb,
	.v6_apps_rt_index_hi = 0x12,
	.v6_rt_hash_ofst = 0x5c8,
	.v6_rt_hash_size = 0x98,
	.v6_rt_hash_size_ddr = 0x4000,
	.v6_rt_nhash_ofst = 0x668,
	.v6_rt_nhash_size = 0x098,
	.v6_rt_nhash_size_ddr = 0x4000,
	.modem_hdr_ofst = 0x708,
	.modem_hdr_size = 0x240,
	.apps_hdr_ofst = 0x948,
	.apps_hdr_size = 0x1e0,
	.apps_hdr_size_ddr = 0x800,
	.modem_hdr_proc_ctx_ofst = 0xb40,
	.modem_hdr_proc_ctx_size = 0xb20,
	.apps_hdr_proc_ctx_ofst = 0x1660,
	.apps_hdr_proc_ctx_size = 0x200,
	.apps_hdr_proc_ctx_size_ddr = 0x0,
	.stats_quota_q6_ofst = 0x1868,
	.stats_quota_q6_size = 0x60,
	.stats_quota_ap_ofst = 0x18C8,
	.stats_quota_ap_size = 0x48,
	.stats_tethering_ofst = 0x1910,
	.stats_tethering_size = 0x3c0,
	.stats_flt_v4_ofst = 0,
	.stats_flt_v4_size = 0,
	.stats_flt_v6_ofst = 0,
	.stats_flt_v6_size = 0,
	.stats_rt_v4_ofst = 0,
	.stats_rt_v4_size = 0,
	.stats_rt_v6_ofst = 0,
	.stats_rt_v6_size = 0,
	.stats_fnr_ofst = 0x1cd0,
	.stats_fnr_size = 0xba0,
	.stats_drop_ofst = 0x2870,
	.stats_drop_size = 0x20,
	.modem_comp_decomp_ofst = 0x0,
	.modem_comp_decomp_size = 0x0,
	.modem_ofst = 0x2898,
	.modem_size = 0xd48,
	.nat_tbl_ofst = 0x35e0,
	.nat_tbl_size = 0x900,
	.apps_v4_flt_hash_ofst = 0x2718,
	.apps_v4_flt_hash_size = 0x0,
	.apps_v6_flt_hash_ofst = 0x2718,
	.apps_v6_flt_hash_size = 0x0,
	.apps_v4_flt_nhash_ofst = 0x2718,
	.apps_v4_flt_nhash_size = 0x0,
	.apps_v6_flt_nhash_ofst = 0x2718,
	.apps_v6_flt_nhash_size = 0x0,
	.apps_v4_rt_hash_ofst = 0x2718,
	.apps_v4_rt_hash_size = 0x0,
	.apps_v4_rt_nhash_ofst = 0x2718,
	.apps_v4_rt_nhash_size = 0x0,
	.apps_v6_rt_hash_ofst = 0x2718,
	.apps_v6_rt_hash_size = 0x0,
	.apps_v6_rt_nhash_ofst = 0x2718,
	.apps_v6_rt_nhash_size = 0x0,
	.pdn_config_ofst = 0x3ee8,
	.pdn_config_size = 0x100,
	.end_ofst = 0x3fe8,
};

static struct ipa3_mem_partition ipa_5_5_mem_part = {
	.uc_descriptor_ram_ofst = 0x0,
	.uc_descriptor_ram_size = 0x1000,
	.uc_ofst = 0x1000,
	.uc_size = 0x80,
	.uc_info_ofst = 0x1080,
	.uc_info_size = 0x200,
	.ofst_start = 0x1280,
	.v4_flt_hash_ofst = 0x1288,
	.v4_flt_hash_size = 0x78,
	.v4_flt_hash_size_ddr = 0x4000,
	.v4_flt_nhash_ofst = 0x1308,
	.v4_flt_nhash_size = 0x78,
	.v4_flt_nhash_size_ddr = 0x4000,
	.v6_flt_hash_ofst = 0x1388,
	.v6_flt_hash_size = 0x78,
	.v6_flt_hash_size_ddr = 0x4000,
	.v6_flt_nhash_ofst = 0x1408,
	.v6_flt_nhash_size = 0x78,
	.v6_flt_nhash_size_ddr = 0x4000,
	.v4_rt_num_index = 0x13,
	.v4_modem_rt_index_lo = 0x0,
	.v4_modem_rt_index_hi = 0xa,
	.v4_apps_rt_index_lo = 0xb,
	.v4_apps_rt_index_hi = 0x12,
	.v4_rt_hash_ofst = 0x1488,
	.v4_rt_hash_size = 0x98,
	.v4_rt_hash_size_ddr = 0x4000,
	.v4_rt_nhash_ofst = 0x1528,
	.v4_rt_nhash_size = 0x98,
	.v4_rt_nhash_size_ddr = 0x4000,
	.v6_rt_num_index = 0x13,
	.v6_modem_rt_index_lo = 0x0,
	.v6_modem_rt_index_hi = 0xa,
	.v6_apps_rt_index_lo = 0xb,
	.v6_apps_rt_index_hi = 0x12,
	.v6_rt_hash_ofst = 0x15c8,
	.v6_rt_hash_size = 0x98,
	.v6_rt_hash_size_ddr = 0x4000,
	.v6_rt_nhash_ofst = 0x1668,
	.v6_rt_nhash_size = 0x098,
	.v6_rt_nhash_size_ddr = 0x4000,
	.modem_hdr_ofst = 0x1708,
	.modem_hdr_size = 0x240,
	.apps_hdr_ofst = 0x1948,
	.apps_hdr_size = 0x1e0,
	.apps_hdr_size_ddr = 0x800,
	.modem_hdr_proc_ctx_ofst = 0x1b40,
	.modem_hdr_proc_ctx_size = 0xb20,
	.apps_hdr_proc_ctx_ofst = 0x2660,
	.apps_hdr_proc_ctx_size = 0x200,
	.apps_hdr_proc_ctx_size_ddr = 0x0,
	.stats_quota_q6_ofst = 0x2868,
	.stats_quota_q6_size = 0x60,
	.stats_quota_ap_ofst = 0x28C8,
	.stats_quota_ap_size = 0x48,
	.stats_tethering_ofst = 0x2910,
	.stats_tethering_size = 0x3c0,
	.stats_flt_v4_ofst = 0,
	.stats_flt_v4_size = 0,
	.stats_flt_v6_ofst = 0,
	.stats_flt_v6_size = 0,
	.stats_rt_v4_ofst = 0,
	.stats_rt_v4_size = 0,
	.stats_rt_v6_ofst = 0,
	.stats_rt_v6_size = 0,
	.stats_fnr_ofst = 0x2cd0,
	.stats_fnr_size = 0xba0,
	.stats_drop_ofst = 0x3870,
	.stats_drop_size = 0x20,
	.modem_comp_decomp_ofst = 0x0,
	.modem_comp_decomp_size = 0x0,
	.modem_ofst = 0x3898,
	.modem_size = 0xd48,
	.nat_tbl_ofst = 0x45e0,
	.nat_tbl_size = 0x900,
	.apps_v4_flt_hash_ofst = 0x2718,
	.apps_v4_flt_hash_size = 0x0,
	.apps_v4_flt_nhash_ofst = 0x2718,
	.apps_v4_flt_nhash_size = 0x0,
	.apps_v6_flt_hash_ofst = 0x2718,
	.apps_v6_flt_hash_size = 0x0,
	.apps_v6_flt_nhash_ofst = 0x2718,
	.apps_v6_flt_nhash_size = 0x0,
	.apps_v4_rt_hash_ofst = 0x2718,
	.apps_v4_rt_hash_size = 0x0,
	.apps_v4_rt_nhash_ofst = 0x2718,
	.apps_v4_rt_nhash_size = 0x0,
	.apps_v6_rt_hash_ofst = 0x2718,
	.apps_v6_rt_hash_size = 0x0,
	.apps_v6_rt_nhash_ofst = 0x2718,
	.apps_v6_rt_nhash_size = 0x0,
	.pdn_config_ofst = 0x4ee8,
	.pdn_config_size = 0x100,
	.end_ofst = 0x4fe8,
};

const char *ipa_clients_strings[IPA_CLIENT_MAX] = {
	__stringify(IPA_CLIENT_HSIC1_PROD),
	__stringify(IPA_CLIENT_HSIC1_CONS),
	__stringify(IPA_CLIENT_HSIC2_PROD),
	__stringify(IPA_CLIENT_HSIC2_CONS),
	__stringify(IPA_CLIENT_HSIC3_PROD),
	__stringify(IPA_CLIENT_HSIC3_CONS),
	__stringify(IPA_CLIENT_HSIC4_PROD),
	__stringify(IPA_CLIENT_HSIC4_CONS),
	__stringify(IPA_CLIENT_HSIC5_PROD),
	__stringify(IPA_CLIENT_HSIC5_CONS),
	__stringify(IPA_CLIENT_WLAN1_PROD),
	__stringify(IPA_CLIENT_WLAN1_CONS),
	__stringify(IPA_CLIENT_WLAN2_PROD),
	__stringify(IPA_CLIENT_WLAN2_CONS),
	__stringify(IPA_CLIENT_WLAN3_PROD),
	__stringify(IPA_CLIENT_WLAN3_CONS),
	__stringify(RESERVED_PROD_16),
	__stringify(IPA_CLIENT_WLAN4_CONS),
	__stringify(IPA_CLIENT_USB_PROD),
	__stringify(IPA_CLIENT_USB_CONS),
	__stringify(IPA_CLIENT_USB2_PROD),
	__stringify(IPA_CLIENT_USB2_CONS),
	__stringify(IPA_CLIENT_USB3_PROD),
	__stringify(IPA_CLIENT_USB3_CONS),
	__stringify(IPA_CLIENT_USB4_PROD),
	__stringify(IPA_CLIENT_USB4_CONS),
	__stringify(IPA_CLIENT_UC_USB_PROD),
	__stringify(IPA_CLIENT_USB_DPL_CONS),
	__stringify(IPA_CLIENT_A2_EMBEDDED_PROD),
	__stringify(IPA_CLIENT_A2_EMBEDDED_CONS),
	__stringify(IPA_CLIENT_A2_TETHERED_PROD),
	__stringify(IPA_CLIENT_A2_TETHERED_CONS),
	__stringify(IPA_CLIENT_APPS_LAN_PROD),
	__stringify(IPA_CLIENT_APPS_LAN_CONS),
	__stringify(IPA_CLIENT_APPS_WAN_PROD),
	__stringify(IPA_CLIENT_APPS_WAN_CONS),
	__stringify(IPA_CLIENT_APPS_CMD_PROD),
	__stringify(IPA_CLIENT_A5_LAN_WAN_CONS),
	__stringify(IPA_CLIENT_ODU_PROD),
	__stringify(IPA_CLIENT_ODU_EMB_CONS),
	__stringify(RESERVED_PROD_40),
	__stringify(IPA_CLIENT_ODU_TETH_CONS),
	__stringify(IPA_CLIENT_MHI_PROD),
	__stringify(IPA_CLIENT_MHI_CONS),
	__stringify(IPA_CLIENT_MEMCPY_DMA_SYNC_PROD),
	__stringify(IPA_CLIENT_MEMCPY_DMA_SYNC_CONS),
	__stringify(IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD),
	__stringify(IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS),
	__stringify(IPA_CLIENT_ETHERNET_PROD),
	__stringify(IPA_CLIENT_ETHERNET_CONS),
	__stringify(IPA_CLIENT_Q6_LAN_PROD),
	__stringify(IPA_CLIENT_Q6_LAN_CONS),
	__stringify(IPA_CLIENT_Q6_WAN_PROD),
	__stringify(IPA_CLIENT_Q6_WAN_CONS),
	__stringify(IPA_CLIENT_Q6_CMD_PROD),
	__stringify(IPA_CLIENT_Q6_DUN_CONS),
	__stringify(IPA_CLIENT_Q6_DECOMP_PROD),
	__stringify(IPA_CLIENT_Q6_DECOMP_CONS),
	__stringify(IPA_CLIENT_Q6_DECOMP2_PROD),
	__stringify(IPA_CLIENT_Q6_DECOMP2_CONS),
	__stringify(RESERVED_PROD_60),
	__stringify(IPA_CLIENT_Q6_LTE_WIFI_AGGR_CONS),
	__stringify(IPA_CLIENT_TEST_PROD),
	__stringify(IPA_CLIENT_TEST_CONS),
	__stringify(IPA_CLIENT_TEST1_PROD),
	__stringify(IPA_CLIENT_TEST1_CONS),
	__stringify(IPA_CLIENT_TEST2_PROD),
	__stringify(IPA_CLIENT_TEST2_CONS),
	__stringify(IPA_CLIENT_TEST3_PROD),
	__stringify(IPA_CLIENT_TEST3_CONS),
	__stringify(IPA_CLIENT_TEST4_PROD),
	__stringify(IPA_CLIENT_TEST4_CONS),
	__stringify(RESERVED_PROD_72),
	__stringify(IPA_CLIENT_DUMMY_CONS),
	__stringify(IPA_CLIENT_Q6_DL_NLO_DATA_PROD),
	__stringify(IPA_CLIENT_Q6_UL_NLO_DATA_CONS),
	__stringify(RESERVED_PROD_76),
	__stringify(IPA_CLIENT_Q6_UL_NLO_ACK_CONS),
	__stringify(RESERVED_PROD_78),
	__stringify(IPA_CLIENT_Q6_QBAP_STATUS_CONS),
	__stringify(RESERVED_PROD_80),
	__stringify(IPA_CLIENT_MHI_DPL_CONS),
	__stringify(RESERVED_PROD_82),
	__stringify(IPA_CLIENT_ODL_DPL_CONS),
	__stringify(IPA_CLIENT_Q6_AUDIO_DMA_MHI_PROD),
	__stringify(IPA_CLIENT_Q6_AUDIO_DMA_MHI_CONS),
	__stringify(IPA_CLIENT_WIGIG_PROD),
	__stringify(IPA_CLIENT_WIGIG1_CONS),
	__stringify(RESERVERD_PROD_88),
	__stringify(IPA_CLIENT_WIGIG2_CONS),
	__stringify(RESERVERD_PROD_90),
	__stringify(IPA_CLIENT_WIGIG3_CONS),
	__stringify(RESERVERD_PROD_92),
	__stringify(IPA_CLIENT_WIGIG4_CONS),
	__stringify(RESERVERD_PROD_94),
	__stringify(IPA_CLIENT_APPS_WAN_COAL_CONS),
	__stringify(IPA_CLIENT_MHI_PRIME_TETH_PROD),
	__stringify(IPA_CLIENT_MHI_PRIME_TETH_CONS),
	__stringify(IPA_CLIENT_MHI_PRIME_RMNET_PROD),
	__stringify(IPA_CLIENT_MHI_PRIME_RMNET_CONS),
	__stringify(IPA_CLIENT_MHI_PRIME_DPL_PROD),
	__stringify(RESERVERD_CONS_101),
	__stringify(IPA_CLIENT_AQC_ETHERNET_PROD),
	__stringify(IPA_CLIENT_AQC_ETHERNET_CONS),
	__stringify(IPA_CLIENT_APPS_WAN_LOW_LAT_PROD),
	__stringify(IPA_CLIENT_APPS_WAN_LOW_LAT_CONS),
	__stringify(IPA_CLIENT_QDSS_PROD),
	__stringify(IPA_CLIENT_MHI_QDSS_CONS),
	__stringify(IPA_CLIENT_RTK_ETHERNET_PROD),
	__stringify(IPA_CLIENT_RTK_ETHERNET_CONS),
	__stringify(IPA_CLIENT_MHI_LOW_LAT_PROD),
	__stringify(IPA_CLIENT_MHI_LOW_LAT_CONS),
	__stringify(IPA_CLIENT_MHI2_PROD),
	__stringify(IPA_CLIENT_MHI2_CONS),
	__stringify(IPA_CLIENT_Q6_CV2X_PROD),
	__stringify(IPA_CLIENT_Q6_CV2X_CONS),
	__stringify(IPA_CLIENT_ETHERNET2_PROD),
	__stringify(IPA_CLIENT_ETHERNET2_CONS),
	__stringify(RESERVERD_PROD_118),
	__stringify(IPA_CLIENT_WLAN2_CONS1),
	__stringify(IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_PROD),
	__stringify(IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS),
	__stringify(IPA_CLIENT_Q6_DL_NLO_LL_DATA_PROD),
	__stringify(RESERVERD_CONS_123),
	__stringify(RESERVERD_PROD_124),
	__stringify(IPA_CLIENT_TPUT_CONS),
	__stringify(RESERVERD_PROD_126),
	__stringify(IPA_CLIENT_APPS_LAN_COAL_CONS),
};
EXPORT_SYMBOL(ipa_clients_strings);

static void _set_coalescing_disposition(
	bool force_to_default )
{
	if ( ipa3_ctx->ipa_initialization_complete
		 &&
		 ipa3_ctx->ipa_hw_type >= IPA_HW_v5_5 ) {

		struct ipahal_reg_coal_master_cfg master_cfg;

		memset(&master_cfg, 0, sizeof(master_cfg));

		ipahal_read_reg_fields(IPA_COAL_MASTER_CFG, &master_cfg);

		master_cfg.coal_force_to_default = force_to_default;

		ipahal_write_reg_fields(IPA_COAL_MASTER_CFG, &master_cfg);
	}
}

void start_coalescing(void)
{
	if ( ipa3_ctx->coal_stopped ) {
		_set_coalescing_disposition(false);
		ipa3_ctx->coal_stopped = false;
	}
}

void stop_coalescing(void)
{
	if ( ! ipa3_ctx->coal_stopped ) {
		_set_coalescing_disposition(true);
		ipa3_ctx->coal_stopped = true;
	}
}

bool lan_coal_enabled(void)
{
	if ( ipa3_ctx->ipa_initialization_complete && ipa3_ctx->lan_coal_enable) {
		int ep_idx;
		if ( IPA_CLIENT_IS_MAPPED_VALID(IPA_CLIENT_APPS_LAN_COAL_CONS, ep_idx) ) {
			return true;
		}
	}
	return false;
}

int ipa3_set_evict_policy(
	struct ipa_ioc_coal_evict_policy *evict_pol)
{
	if (!evict_pol) {
		IPAERR_RL("Bad arg evict_pol(%p)\n", evict_pol);
		return -1;
	}

	if ( ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5 )
	{
		struct ipahal_reg_coal_evict_lru evict_lru_reg;

		memset(&evict_lru_reg, 0, sizeof(evict_lru_reg));

		evict_lru_reg.coal_vp_lru_thrshld =
			evict_pol->coal_vp_thrshld;
		evict_lru_reg.coal_eviction_en =
			evict_pol->coal_eviction_en;
		evict_lru_reg.coal_vp_lru_gran_sel =
			evict_pol->coal_vp_gran_sel;
		evict_lru_reg.coal_vp_lru_udp_thrshld =
			evict_pol->coal_vp_udp_thrshld;
		evict_lru_reg.coal_vp_lru_tcp_thrshld =
			evict_pol->coal_vp_tcp_thrshld;
		evict_lru_reg.coal_vp_lru_udp_thrshld_en =
			evict_pol->coal_vp_udp_thrshld_en;
		evict_lru_reg.coal_vp_lru_tcp_thrshld_en =
			evict_pol->coal_vp_tcp_thrshld_en;
		evict_lru_reg.coal_vp_lru_tcp_num =
			evict_pol->coal_vp_tcp_num;

		ipahal_write_reg_fields(IPA_COAL_EVICT_LRU, &evict_lru_reg);
	}

	return 0;
}

/**
 * ipa_get_version_string() - Get string representation of IPA version
 * @ver: IPA version
 *
 * Return: Constant string representation
 */
const char *ipa_get_version_string(enum ipa_hw_type ver)
{
	const char *str;

	switch (ver) {
	case IPA_HW_v1_0:
		str = "1.0";
		break;
	case IPA_HW_v1_1:
		str = "1.1";
		break;
	case IPA_HW_v2_0:
		str = "2.0";
		break;
	case IPA_HW_v2_1:
		str = "2.1";
		break;
	case IPA_HW_v2_5:
		str = "2.5/2.6";
		break;
	case IPA_HW_v2_6L:
		str = "2.6L";
		break;
	case IPA_HW_v3_0:
		str = "3.0";
		break;
	case IPA_HW_v3_1:
		str = "3.1";
		break;
	case IPA_HW_v3_5:
		str = "3.5";
		break;
	case IPA_HW_v3_5_1:
		str = "3.5.1";
		break;
	case IPA_HW_v4_0:
		str = "4.0";
		break;
	case IPA_HW_v4_1:
		str = "4.1";
		break;
	case IPA_HW_v4_2:
		str = "4.2";
		break;
	case IPA_HW_v4_5:
		str = "4.5";
		break;
	case IPA_HW_v4_7:
		str = "4.7";
		break;
	case IPA_HW_v4_9:
		str = "4.9";
		break;
	case IPA_HW_v4_11:
		str = "4.11";
		break;
	case IPA_HW_v5_0:
		str = "5.0";
		break;
	case IPA_HW_v5_1:
		str = "5.1";
		fallthrough;
	case IPA_HW_v5_2:
		str = "5.2";
		fallthrough;
	case IPA_HW_v5_5:
		str = "5.5";
		fallthrough;
	default:
		str = "Invalid version";
		break;
	}

	return str;
}
EXPORT_SYMBOL(ipa_get_version_string);

/**
 * ipa3_get_clients_from_rm_resource() - get IPA clients which are related to an
 * IPA_RM resource
 *
 * @resource: [IN] IPA Resource Manager resource
 * @clients: [OUT] Empty array which will contain the list of clients. The
 *         caller must initialize this array.
 *
 * Return codes: 0 on success, negative on failure.
 */
int ipa3_get_clients_from_rm_resource(
	enum ipa_rm_resource_name resource,
	struct ipa3_client_names *clients)
{
	int i = 0;

	if (resource < 0 ||
	    resource >= IPA_RM_RESOURCE_MAX ||
	    !clients) {
		IPAERR("Bad parameters\n");
		return -EINVAL;
	}

	switch (resource) {
	case IPA_RM_RESOURCE_USB_CONS:
		if (ipa_get_ep_mapping(IPA_CLIENT_USB_CONS) != -1)
			clients->names[i++] = IPA_CLIENT_USB_CONS;
		break;
	case IPA_RM_RESOURCE_USB_DPL_CONS:
		if (ipa_get_ep_mapping(IPA_CLIENT_USB_DPL_CONS) != -1)
			clients->names[i++] = IPA_CLIENT_USB_DPL_CONS;
		break;
	case IPA_RM_RESOURCE_HSIC_CONS:
		clients->names[i++] = IPA_CLIENT_HSIC1_CONS;
		break;
	case IPA_RM_RESOURCE_WLAN_CONS:
		clients->names[i++] = IPA_CLIENT_WLAN1_CONS;
		clients->names[i++] = IPA_CLIENT_WLAN2_CONS;
		clients->names[i++] = IPA_CLIENT_WLAN3_CONS;
		clients->names[i++] = IPA_CLIENT_WLAN2_CONS1;
		break;
	case IPA_RM_RESOURCE_MHI_CONS:
		clients->names[i++] = IPA_CLIENT_MHI_CONS;
		break;
	case IPA_RM_RESOURCE_ODU_ADAPT_CONS:
		clients->names[i++] = IPA_CLIENT_ODU_EMB_CONS;
		clients->names[i++] = IPA_CLIENT_ODU_TETH_CONS;
		break;
	case IPA_RM_RESOURCE_ETHERNET_CONS:
		clients->names[i++] = IPA_CLIENT_ETHERNET_CONS;
		break;
	case IPA_RM_RESOURCE_USB_PROD:
		if (ipa_get_ep_mapping(IPA_CLIENT_USB_PROD) != -1)
			clients->names[i++] = IPA_CLIENT_USB_PROD;
		break;
	case IPA_RM_RESOURCE_HSIC_PROD:
		clients->names[i++] = IPA_CLIENT_HSIC1_PROD;
		break;
	case IPA_RM_RESOURCE_MHI_PROD:
		clients->names[i++] = IPA_CLIENT_MHI_PROD;
		break;
	case IPA_RM_RESOURCE_ODU_ADAPT_PROD:
		clients->names[i++] = IPA_CLIENT_ODU_PROD;
		break;
	case IPA_RM_RESOURCE_ETHERNET_PROD:
		clients->names[i++] = IPA_CLIENT_ETHERNET_PROD;
		break;
	default:
		break;
	}
	clients->length = i;

	return 0;
}

/**
 * ipa3_should_pipe_be_suspended() - returns true when the client's pipe should
 * be suspended during a power save scenario. False otherwise.
 *
 * @client: [IN] IPA client
 */
bool ipa3_should_pipe_be_suspended(enum ipa_client_type client)
{
	struct ipa3_ep_context *ep;
	int ipa_ep_idx;

	ipa_ep_idx = ipa_get_ep_mapping(client);
	if (ipa_ep_idx == -1) {
		IPAERR("Invalid client.\n");
		WARN_ON(1);
		return false;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	/*
	 * starting IPA 4.0 pipe no longer can be suspended. Instead,
	 * the corresponding GSI channel should be stopped. Usually client
	 * driver will take care of stopping the channel. For client drivers
	 * that are not stopping the channel, IPA RM will do that based on
	 * ipa3_should_pipe_channel_be_stopped().
	 */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0)
		return false;

	if (ep->keep_ipa_awake)
		return false;

	if (client == IPA_CLIENT_USB_CONS     ||
		client == IPA_CLIENT_USB2_CONS    ||
		client == IPA_CLIENT_USB_DPL_CONS ||
		client == IPA_CLIENT_MHI_CONS     ||
		client == IPA_CLIENT_MHI_DPL_CONS ||
		client == IPA_CLIENT_MHI_QDSS_CONS ||
		client == IPA_CLIENT_HSIC1_CONS   ||
		client == IPA_CLIENT_WLAN1_CONS   ||
		client == IPA_CLIENT_WLAN2_CONS   ||
		client == IPA_CLIENT_WLAN3_CONS   ||
		client == IPA_CLIENT_WLAN2_CONS1  ||
		client == IPA_CLIENT_WLAN4_CONS   ||
		client == IPA_CLIENT_ODU_EMB_CONS ||
		client == IPA_CLIENT_ODU_TETH_CONS ||
		client == IPA_CLIENT_ETHERNET_CONS ||
		client == IPA_CLIENT_ETHERNET2_CONS)
		return true;

	return false;
}

/**
 * ipa3_should_pipe_channel_be_stopped() - returns true when the client's
 * channel should be stopped during a power save scenario. False otherwise.
 * Most client already stops the GSI channel on suspend, and are not included
 * in the list below.
 *
 * @client: [IN] IPA client
 */
static bool ipa3_should_pipe_channel_be_stopped(enum ipa_client_type client)
{
	struct ipa3_ep_context *ep;
	int ipa_ep_idx;

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0)
		return false;

	ipa_ep_idx = ipa_get_ep_mapping(client);
	if (ipa_ep_idx == -1) {
		IPAERR("Invalid client.\n");
		WARN_ON(1);
		return false;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	if (ep->keep_ipa_awake)
		return false;

	if (client == IPA_CLIENT_ODU_EMB_CONS ||
	    client == IPA_CLIENT_ODU_TETH_CONS)
		return true;

	return false;
}

/**
 * ipa3_suspend_resource_sync() - suspend client endpoints related to the IPA_RM
 * resource and decrement active clients counter, which may result in clock
 * gating of IPA clocks.
 *
 * @resource: [IN] IPA Resource Manager resource
 *
 * Return codes: 0 on success, negative on failure.
 */
int ipa3_suspend_resource_sync(enum ipa_rm_resource_name resource)
{
	struct ipa3_client_names clients;
	int res;
	int index;
	struct ipa_ep_cfg_ctrl suspend;
	enum ipa_client_type client;
	int ipa_ep_idx;
	bool pipe_suspended = false;

	memset(&clients, 0, sizeof(clients));
	res = ipa3_get_clients_from_rm_resource(resource, &clients);
	if (res) {
		IPAERR("Bad params.\n");
		return res;
	}

	for (index = 0; index < clients.length; index++) {
		client = clients.names[index];
		ipa_ep_idx = ipa_get_ep_mapping(client);
		if (ipa_ep_idx == -1) {
			IPAERR("Invalid client.\n");
			res = -EINVAL;
			continue;
		}
		ipa3_ctx->resume_on_connect[client] = false;
		if (ipa3_ctx->ep[ipa_ep_idx].client == client &&
		    ipa3_should_pipe_be_suspended(client)) {
			if (ipa3_ctx->ep[ipa_ep_idx].valid) {
				/* suspend endpoint */
				memset(&suspend, 0, sizeof(suspend));
				suspend.ipa_ep_suspend = true;
				ipa_cfg_ep_ctrl(ipa_ep_idx, &suspend);
				pipe_suspended = true;
			}
		}

		if (ipa3_ctx->ep[ipa_ep_idx].client == client &&
			ipa3_should_pipe_channel_be_stopped(client)) {
			if (ipa3_ctx->ep[ipa_ep_idx].valid) {
				/* Stop GSI channel */
				res = ipa_stop_gsi_channel(ipa_ep_idx);
				if (res) {
					IPAERR("failed stop gsi ch %lu\n",
					ipa3_ctx->ep[ipa_ep_idx].gsi_chan_hdl);
					return res;
				}
			}
		}
	}
	/* Sleep ~1 msec */
	if (pipe_suspended)
		usleep_range(1000, 2000);

	/* before gating IPA clocks do TAG process */
	ipa3_ctx->tag_process_before_gating = true;
	IPA_ACTIVE_CLIENTS_DEC_RESOURCE(ipa_rm_resource_str(resource));

	return 0;
}

/**
 * ipa3_suspend_resource_no_block() - suspend client endpoints related to the
 * IPA_RM resource and decrement active clients counter. This function is
 * guaranteed to avoid sleeping.
 *
 * @resource: [IN] IPA Resource Manager resource
 *
 * Return codes: 0 on success, negative on failure.
 */
int ipa3_suspend_resource_no_block(enum ipa_rm_resource_name resource)
{
	int res;
	struct ipa3_client_names clients;
	int index;
	enum ipa_client_type client;
	struct ipa_ep_cfg_ctrl suspend;
	int ipa_ep_idx;
	struct ipa_active_client_logging_info log_info;

	memset(&clients, 0, sizeof(clients));
	res = ipa3_get_clients_from_rm_resource(resource, &clients);
	if (res) {
		IPAERR(
			"ipa3_get_clients_from_rm_resource() failed, name = %d.\n",
			resource);
		goto bail;
	}

	for (index = 0; index < clients.length; index++) {
		client = clients.names[index];
		ipa_ep_idx = ipa_get_ep_mapping(client);
		if (ipa_ep_idx == -1) {
			IPAERR("Invalid client.\n");
			res = -EINVAL;
			continue;
		}
		ipa3_ctx->resume_on_connect[client] = false;
		if (ipa3_ctx->ep[ipa_ep_idx].client == client &&
		    ipa3_should_pipe_be_suspended(client)) {
			if (ipa3_ctx->ep[ipa_ep_idx].valid) {
				/* suspend endpoint */
				memset(&suspend, 0, sizeof(suspend));
				suspend.ipa_ep_suspend = true;
				ipa_cfg_ep_ctrl(ipa_ep_idx, &suspend);
			}
		}

		if (ipa3_ctx->ep[ipa_ep_idx].client == client &&
			ipa3_should_pipe_channel_be_stopped(client)) {
			res = -EPERM;
			goto bail;
		}
	}

	if (res == 0) {
		IPA_ACTIVE_CLIENTS_PREP_RESOURCE(log_info,
				ipa_rm_resource_str(resource));
		/* before gating IPA clocks do TAG process */
		ipa3_ctx->tag_process_before_gating = true;
		ipa3_dec_client_disable_clks_no_block(&log_info);
	}
bail:
	return res;
}

/**
 * ipa3_resume_resource() - resume client endpoints related to the IPA_RM
 * resource.
 *
 * @resource: [IN] IPA Resource Manager resource
 *
 * Return codes: 0 on success, negative on failure.
 */
int ipa3_resume_resource(enum ipa_rm_resource_name resource)
{

	struct ipa3_client_names clients;
	int res;
	int index;
	struct ipa_ep_cfg_ctrl suspend;
	enum ipa_client_type client;
	int ipa_ep_idx;

	memset(&clients, 0, sizeof(clients));
	res = ipa3_get_clients_from_rm_resource(resource, &clients);
	if (res) {
		IPAERR("ipa3_get_clients_from_rm_resource() failed.\n");
		return res;
	}

	for (index = 0; index < clients.length; index++) {
		client = clients.names[index];
		ipa_ep_idx = ipa_get_ep_mapping(client);
		if (ipa_ep_idx == -1) {
			IPAERR("Invalid client.\n");
			res = -EINVAL;
			continue;
		}
		/*
		 * The related ep, will be resumed on connect
		 * while its resource is granted
		 */
		ipa3_ctx->resume_on_connect[client] = true;
		IPADBG("%d will be resumed on connect.\n", client);
		if (ipa3_ctx->ep[ipa_ep_idx].client == client &&
		    ipa3_should_pipe_be_suspended(client)) {
			if (ipa3_ctx->ep[ipa_ep_idx].valid) {
				memset(&suspend, 0, sizeof(suspend));
				suspend.ipa_ep_suspend = false;
				ipa_cfg_ep_ctrl(ipa_ep_idx, &suspend);
			}
		}

		if (ipa3_ctx->ep[ipa_ep_idx].client == client &&
			ipa3_should_pipe_channel_be_stopped(client)) {
			if (ipa3_ctx->ep[ipa_ep_idx].valid) {
				res = gsi_start_channel(
					ipa3_ctx->ep[ipa_ep_idx].gsi_chan_hdl);
				if (res) {
					IPAERR("failed to start gsi ch %lu\n",
					ipa3_ctx->ep[ipa_ep_idx].gsi_chan_hdl);
					return res;
				}
			}
		}
	}

	return res;
}

/**
 * ipa3_get_hw_type_index() - Get HW type index which is used as the entry index
 *	for ep\resource groups related arrays .
 *
 * Return value: HW type index
 */
u8 ipa3_get_hw_type_index(void)
{
	u8 hw_type_index;

	switch (ipa3_ctx->ipa_hw_type) {
	case IPA_HW_v3_0:
	case IPA_HW_v3_1:
		hw_type_index = IPA_3_0;
		break;
	case IPA_HW_v3_5:
		hw_type_index = IPA_3_5;
		/*
		 *this flag is initialized only after fw load trigger from
		 * user space (ipa3_write)
		 */
		if (ipa3_ctx->ipa_config_is_mhi)
			hw_type_index = IPA_3_5_MHI;
		break;
	case IPA_HW_v3_5_1:
		hw_type_index = IPA_3_5_1;
		break;
	case IPA_HW_v4_0:
		hw_type_index = IPA_4_0;
		/*
		 *this flag is initialized only after fw load trigger from
		 * user space (ipa3_write)
		 */
		if (ipa3_ctx->ipa_config_is_mhi)
			hw_type_index = IPA_4_0_MHI;
		break;
	case IPA_HW_v4_1:
		hw_type_index = IPA_4_1;
		break;
	case IPA_HW_v4_2:
		hw_type_index = IPA_4_2;
		break;
	case IPA_HW_v4_5:
		hw_type_index = IPA_4_5;
		if (ipa3_ctx->ipa_config_is_mhi)
			hw_type_index = IPA_4_5_MHI;
		if (ipa3_ctx->platform_type == IPA_PLAT_TYPE_APQ)
			hw_type_index = IPA_4_5_APQ;
		if (ipa3_ctx->ipa_config_is_auto)
			hw_type_index = IPA_4_5_AUTO;
		if (ipa3_ctx->ipa_config_is_auto &&
			ipa3_ctx->ipa_config_is_mhi)
			hw_type_index = IPA_4_5_AUTO_MHI;
		break;
	case IPA_HW_v4_7:
		hw_type_index = IPA_4_7;
		break;
	case IPA_HW_v4_9:
		hw_type_index = IPA_4_9;
		break;
	case IPA_HW_v4_11:
		hw_type_index = IPA_4_11;
		break;
	case IPA_HW_v5_0:
		hw_type_index = IPA_5_0;
		if (ipa3_ctx->ipa_config_is_mhi)
			hw_type_index = IPA_5_0_MHI;
		break;
	case IPA_HW_v5_1:
		hw_type_index = IPA_5_1;
		if (ipa3_ctx->platform_type == IPA_PLAT_TYPE_APQ)
			hw_type_index = IPA_5_1_APQ;
		break;
	case IPA_HW_v5_2:
		hw_type_index = IPA_5_2;
		break;
	case IPA_HW_v5_5:
		hw_type_index = IPA_5_5;
		if (ipa3_ctx->platform_type == IPA_PLAT_TYPE_XR)
			hw_type_index = IPA_5_5_XR;
		break;
	default:
		IPAERR("Incorrect IPA version %d\n", ipa3_ctx->ipa_hw_type);
		hw_type_index = IPA_3_0;
		break;
	}

	return hw_type_index;
}

/**
 * _ipa_sram_settings_read_v3_0() - Read SRAM settings from HW
 *
 * Returns:	None
 */
void _ipa_sram_settings_read_v3_0(void)
{
	struct ipahal_reg_shared_mem_size smem_sz;

	memset(&smem_sz, 0, sizeof(smem_sz));

	ipahal_read_reg_fields(IPA_SHARED_MEM_SIZE, &smem_sz);

	ipa3_ctx->smem_restricted_bytes = smem_sz.shared_mem_baddr;
	ipa3_ctx->smem_sz = smem_sz.shared_mem_sz;

	/* reg fields are in 8B units */
	ipa3_ctx->smem_restricted_bytes *= 8;
	ipa3_ctx->smem_sz *= 8;
	ipa3_ctx->smem_reqd_sz = IPA_MEM_PART(end_ofst);
	ipa3_ctx->hdr_proc_ctx_tbl_lcl = true;

	/*
	 * when proc ctx table is located in internal memory,
	 * modem entries resides first.
	 */
	if (ipa3_ctx->hdr_proc_ctx_tbl_lcl) {
		ipa3_ctx->hdr_proc_ctx_tbl.start_offset =
			IPA_MEM_PART(modem_hdr_proc_ctx_size);
	}

	ipa3_ctx->rt_tbl_hash_lcl[IPA_IP_v4] = false;
	ipa3_ctx->rt_tbl_nhash_lcl[IPA_IP_v4] = false;
	ipa3_ctx->rt_tbl_hash_lcl[IPA_IP_v6] = false;
	ipa3_ctx->rt_tbl_nhash_lcl[IPA_IP_v6] = false;
	ipa3_ctx->flt_tbl_hash_lcl[IPA_IP_v4] = false;
	ipa3_ctx->flt_tbl_hash_lcl[IPA_IP_v6] = false;

	if (ipa3_ctx->ipa_hw_type == IPA_HW_v5_0) {
		ipa3_ctx->flt_tbl_nhash_lcl[IPA_IP_v4] = true;
		ipa3_ctx->flt_tbl_nhash_lcl[IPA_IP_v6] = true;
	} else {
		ipa3_ctx->flt_tbl_nhash_lcl[IPA_IP_v4] = false;
		ipa3_ctx->flt_tbl_nhash_lcl[IPA_IP_v6] = false;
	}
}

/**
 * ipa3_cfg_route() - configure IPA route
 * @route: IPA route
 *
 * Return codes:
 * 0: success
 */
int ipa3_cfg_route(struct ipahal_reg_route *route)
{

	IPADBG("disable_route_block=%d, default_pipe=%d, default_hdr_tbl=%d\n",
		route->route_dis,
		route->route_def_pipe,
		route->route_def_hdr_table);
	IPADBG("default_hdr_ofst=%d, default_frag_pipe=%d\n",
		route->route_def_hdr_ofst,
		route->route_frag_def_pipe);

	IPADBG("default_retain_hdr=%d\n",
		route->route_def_retain_hdr);

	if (route->route_dis) {
		IPAERR("Route disable is not supported!\n");
		return -EPERM;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	ipahal_write_reg_fields(IPA_ROUTE, route);

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return 0;
}

/**
 * ipa3_cfg_filter() - configure filter
 * @disable: disable value
 *
 * Return codes:
 * 0: success
 */
int ipa3_cfg_filter(u32 disable)
{
	IPAERR_RL("Filter disable is not supported!\n");
	return -EPERM;
}

/**
 * ipa_disable_hashing_rt_flt_v4_2() - Disable filer and route hashing.
 *
 * Return codes: 0 for success, negative value for failure
 */
static int ipa_disable_hashing_rt_flt_v4_2(void)
{
	/*
	 * note this register deprecated starting IPAv5 if need to disable
	 * use alternative
	 */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v5_0) {
		IPAERR("reg deprecated\n");
		WARN_ON(1);
		return -EPERM;
	}
	IPADBG("Disable hashing for filter and route table in IPA 4.2 HW\n");
	ipahal_write_reg(IPA_FILT_ROUT_HASH_EN,
					IPA_FILT_ROUT_HASH_REG_VAL_v4_2);
	return 0;
}


/**
 * ipa_comp_cfg() - Configure QMB/Master port selection
 *
 * Returns:	None
 */
static void ipa_comp_cfg(void)
{
	struct ipahal_reg_comp_cfg comp_cfg;

	/* IPAv4 specific, on NON-MHI config*/
	if ((ipa3_ctx->ipa_hw_type == IPA_HW_v4_0) &&
		(!ipa3_ctx->ipa_config_is_mhi)) {

		ipahal_read_reg_fields(IPA_COMP_CFG, &comp_cfg);
		IPADBG("Before comp config\n");
		IPADBG("ipa_qmb_select_by_address_global_en = %d\n",
			comp_cfg.ipa_qmb_select_by_address_global_en);

		IPADBG("ipa_qmb_select_by_address_prod_en = %d\n",
				comp_cfg.ipa_qmb_select_by_address_prod_en);

		IPADBG("ipa_qmb_select_by_address_cons_en = %d\n",
				comp_cfg.ipa_qmb_select_by_address_cons_en);

		comp_cfg.ipa_qmb_select_by_address_global_en = false;
		comp_cfg.ipa_qmb_select_by_address_prod_en = false;
		comp_cfg.ipa_qmb_select_by_address_cons_en = false;

		ipahal_write_reg_fields(IPA_COMP_CFG, &comp_cfg);

		ipahal_read_reg_fields(IPA_COMP_CFG, &comp_cfg);
		IPADBG("After comp config\n");
		IPADBG("ipa_qmb_select_by_address_global_en = %d\n",
			comp_cfg.ipa_qmb_select_by_address_global_en);

		IPADBG("ipa_qmb_select_by_address_prod_en = %d\n",
				comp_cfg.ipa_qmb_select_by_address_prod_en);

		IPADBG("ipa_qmb_select_by_address_cons_en = %d\n",
				comp_cfg.ipa_qmb_select_by_address_cons_en);
	}

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		ipahal_read_reg_fields(IPA_COMP_CFG, &comp_cfg);
		IPADBG("Before comp config\n");
		IPADBG("gsi_multi_inorder_rd_dis = %d\n",
			comp_cfg.gsi_multi_inorder_rd_dis);

		IPADBG("gsi_multi_inorder_wr_dis = %d\n",
			comp_cfg.gsi_multi_inorder_wr_dis);

		comp_cfg.gsi_multi_inorder_rd_dis = true;
		comp_cfg.gsi_multi_inorder_wr_dis = true;

		ipahal_write_reg_fields(IPA_COMP_CFG, &comp_cfg);

		ipahal_read_reg_fields(IPA_COMP_CFG, &comp_cfg);
		IPADBG("After comp config\n");
		IPADBG("gsi_multi_inorder_rd_dis = %d\n",
			comp_cfg.gsi_multi_inorder_rd_dis);

		IPADBG("gsi_multi_inorder_wr_dis = %d\n",
			comp_cfg.gsi_multi_inorder_wr_dis);
	}

	/* set GSI_MULTI_AXI_MASTERS_DIS = true after HW.4.1 */
	if ((ipa3_ctx->ipa_hw_type == IPA_HW_v4_1) ||
		(ipa3_ctx->ipa_hw_type == IPA_HW_v4_2)) {
		ipahal_read_reg_fields(IPA_COMP_CFG, &comp_cfg);
		IPADBG("Before comp config\n");
		IPADBG("gsi_multi_axi_masters_dis = %d\n",
			comp_cfg.gsi_multi_axi_masters_dis);

		comp_cfg.gsi_multi_axi_masters_dis = true;

		ipahal_write_reg_fields(IPA_COMP_CFG, &comp_cfg);

		ipahal_read_reg_fields(IPA_COMP_CFG, &comp_cfg);
		IPADBG("After comp config\n");
		IPADBG("gsi_multi_axi_masters_dis = %d\n",
			comp_cfg.gsi_multi_axi_masters_dis);
	}
}

/**
 * ipa3_cfg_qsb() - Configure IPA QSB maximal reads and writes
 *
 * Returns:	None
 */
static void ipa3_cfg_qsb(void)
{
	u8 hw_type_idx;
	const struct ipa_qmb_outstanding *qmb_ot;
	struct ipahal_reg_qsb_max_reads max_reads = { 0 };
	struct ipahal_reg_qsb_max_writes max_writes = { 0 };

	hw_type_idx = ipa3_ctx->hw_type_index;

	/*
	 * Read the register values before writing to them to ensure
	 * other values are not overwritten
	 */
	ipahal_read_reg_fields(IPA_QSB_MAX_WRITES, &max_writes);
	ipahal_read_reg_fields(IPA_QSB_MAX_READS, &max_reads);

	qmb_ot = &(ipa3_qmb_outstanding[hw_type_idx][IPA_QMB_INSTANCE_DDR]);
	max_reads.qmb_0_max_reads = qmb_ot->ot_reads;
	max_writes.qmb_0_max_writes = qmb_ot->ot_writes;
	max_reads.qmb_0_max_read_beats = qmb_ot->ot_read_beats;

	qmb_ot = &(ipa3_qmb_outstanding[hw_type_idx][IPA_QMB_INSTANCE_PCIE]);
	max_reads.qmb_1_max_reads = qmb_ot->ot_reads;
	max_writes.qmb_1_max_writes = qmb_ot->ot_writes;

	ipahal_write_reg_fields(IPA_QSB_MAX_WRITES, &max_writes);
	ipahal_write_reg_fields(IPA_QSB_MAX_READS, &max_reads);
}

/* relevant starting IPA4.5 */
static void ipa_cfg_qtime(void)
{
	struct ipahal_reg_qtime_timestamp_cfg ts_cfg;
	struct ipahal_reg_timers_pulse_gran_cfg gran_cfg;
	struct ipahal_reg_timers_xo_clk_div_cfg div_cfg;
	u32 val;

	/* Configure timestamp resolution */
	memset(&ts_cfg, 0, sizeof(ts_cfg));
	ts_cfg.dpl_timestamp_lsb = IPA_TAG_TIMER_TIMESTAMP_SHFT;
	ts_cfg.dpl_timestamp_sel = true;
	ts_cfg.tag_timestamp_lsb = IPA_TAG_TIMER_TIMESTAMP_SHFT;
	ts_cfg.nat_timestamp_lsb = IPA_NAT_TIMER_TIMESTAMP_SHFT;
	val = ipahal_read_reg(IPA_QTIME_TIMESTAMP_CFG);
	IPADBG("qtime timestamp before cfg: 0x%x\n", val);
	ipahal_write_reg_fields(IPA_QTIME_TIMESTAMP_CFG, &ts_cfg);
	val = ipahal_read_reg(IPA_QTIME_TIMESTAMP_CFG);
	IPADBG("qtime timestamp after cfg: 0x%x\n", val);

	/* Configure timers pulse generators granularity */
	memset(&gran_cfg, 0, sizeof(gran_cfg));
	if (ipa3_ctx->ipa_hw_type < IPA_HW_v5_0)
	{
		gran_cfg.gran_0 = IPA_TIMERS_TIME_GRAN_100_USEC;
		gran_cfg.gran_1 = IPA_TIMERS_TIME_GRAN_1_MSEC;
		gran_cfg.gran_2 = IPA_TIMERS_TIME_GRAN_1_MSEC;
		gran_cfg.gran_3 = IPA_TIMERS_TIME_GRAN_1_MSEC;
	}
	else
	{
		gran_cfg.gran_0 = IPA_TIMERS_TIME_GRAN_100_USEC;
		gran_cfg.gran_1 = IPA_TIMERS_TIME_GRAN_1_MSEC;
		gran_cfg.gran_2 = IPA_TIMERS_TIME_GRAN_10_MSEC;
		gran_cfg.gran_3 = IPA_TIMERS_TIME_GRAN_10_MSEC;
	}
	val = ipahal_read_reg(IPA_TIMERS_PULSE_GRAN_CFG);
	IPADBG("timer pulse granularity before cfg: 0x%x\n", val);
	ipahal_write_reg_fields(IPA_TIMERS_PULSE_GRAN_CFG, &gran_cfg);
	val = ipahal_read_reg(IPA_TIMERS_PULSE_GRAN_CFG);
	IPADBG("timer pulse granularity after cfg: 0x%x\n", val);

	/* Configure timers XO Clock divider */
	memset(&div_cfg, 0, sizeof(div_cfg));
	ipahal_read_reg_fields(IPA_TIMERS_XO_CLK_DIV_CFG, &div_cfg);
	IPADBG("timer XO clk divider before cfg: enabled=%d divider=%u\n",
		div_cfg.enable, div_cfg.value);

	/* Make sure divider is disabled */
	if (div_cfg.enable) {
		div_cfg.enable = false;
		ipahal_write_reg_fields(IPA_TIMERS_XO_CLK_DIV_CFG, &div_cfg);
	}

	/* At emulation systems XO clock is lower than on real target.
	 * (e.g. 19.2Mhz compared to 96Khz)
	 * Use lowest possible divider.
	 */
	if (ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_VIRTUAL ||
		ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_EMULATION) {
		div_cfg.value = 0;
	}

	div_cfg.enable = true; /* Enable the divider */
	ipahal_write_reg_fields(IPA_TIMERS_XO_CLK_DIV_CFG, &div_cfg);
	ipahal_read_reg_fields(IPA_TIMERS_XO_CLK_DIV_CFG, &div_cfg);
	IPADBG("timer XO clk divider after cfg: enabled=%d divider=%u\n",
		div_cfg.enable, div_cfg.value);
}

/**
 * ipa3_init_hw() - initialize HW
 *
 * Return codes:
 * 0: success
 */
int ipa3_init_hw(void)
{
	u32 ipa_version = 0;
	struct ipahal_reg_counter_cfg cnt_cfg;
	struct ipahal_reg_coal_master_cfg master_cfg;

	/* Read IPA version and make sure we have access to the registers */
	ipa_version = ipahal_read_reg(IPA_VERSION);
	IPADBG("IPA_VERSION=%u\n", ipa_version);
	if (ipa_version == 0)
		return -EFAULT;

	switch (ipa3_ctx->ipa_hw_type) {
	case IPA_HW_v3_0:
	case IPA_HW_v3_1:
		ipahal_write_reg(IPA_BCR, IPA_BCR_REG_VAL_v3_0);
		break;
	case IPA_HW_v3_5:
	case IPA_HW_v3_5_1:
		ipahal_write_reg(IPA_BCR, IPA_BCR_REG_VAL_v3_5);
		break;
	case IPA_HW_v4_0:
	case IPA_HW_v4_1:
		ipahal_write_reg(IPA_BCR, IPA_BCR_REG_VAL_v4_0);
		break;
	case IPA_HW_v4_2:
		ipahal_write_reg(IPA_BCR, IPA_BCR_REG_VAL_v4_2);
		break;
	default:
		IPADBG("Do not update BCR - hw_type=%d\n",
			ipa3_ctx->ipa_hw_type);
		break;
	}

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0 &&
		ipa3_ctx->ipa_hw_type < IPA_HW_v4_5) {
		struct ipahal_reg_clkon_cfg clkon_cfg;
		struct ipahal_reg_tx_cfg tx_cfg;

		memset(&clkon_cfg, 0, sizeof(clkon_cfg));

		/*enable open global clocks*/
		clkon_cfg.open_global_2x_clk = true;
		clkon_cfg.open_global = true;
		ipahal_write_reg_fields(IPA_CLKON_CFG, &clkon_cfg);

		ipahal_read_reg_fields(IPA_TX_CFG, &tx_cfg);
		/* disable PA_MASK_EN to allow holb drop */
		tx_cfg.pa_mask_en = 0;
		ipahal_write_reg_fields(IPA_TX_CFG, &tx_cfg);
	}

	ipa3_cfg_qsb();

	/* IPA version  <3.5 IPA_COUNTER_CFG register config not required */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v3_5) {
		if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_5) {
			/* set aggr granularity for 0.5 msec*/
			cnt_cfg.aggr_granularity = GRAN_VALUE_500_USEC;
			ipahal_write_reg_fields(IPA_COUNTER_CFG, &cnt_cfg);
		} else {
			ipa_cfg_qtime();
		}
	}

	if (ipa3_is_ulso_supported()) {
		ipahal_write_reg_n(IPA_ULSO_CFG_IP_ID_MIN_VALUE_n, 0,
			ipa3_ctx->ulso_ip_id_min);
		ipahal_write_reg_n(IPA_ULSO_CFG_IP_ID_MAX_VALUE_n, 0,
		ipa3_ctx->ulso_ip_id_max);
	}

	/* Configure COAL_MASTER_CFG */
	if(ipa3_ctx->ipa_hw_type >= IPA_HW_v5_5) {
		memset(&master_cfg, 0, sizeof(master_cfg));
		ipahal_read_reg_fields(IPA_COAL_MASTER_CFG, &master_cfg);
		master_cfg.coal_ipv4_id_ignore = ipa3_ctx->coal_ipv4_id_ignore;
		ipahal_write_reg_fields(IPA_COAL_MASTER_CFG, &master_cfg);

		IPADBG(
			": coal-ipv4-id-ignore = %s\n",
			master_cfg.coal_ipv4_id_ignore ?
			"True" : "False");
	}

	ipa_comp_cfg();

	/*
	 * In IPA 4.2 filter and routing hashing not supported
	 * disabling hash enable register.
	 */
	if (ipa3_ctx->ipa_fltrt_not_hashable)
		ipa_disable_hashing_rt_flt_v4_2();

	return 0;
}

/**
 * ipa_get_ep_mapping() - provide endpoint mapping
 * @client: client type
 *
 * Return value: endpoint mapping
 */
int ipa_get_ep_mapping(enum ipa_client_type client)
{
	int ipa_ep_idx;
	u8 hw_idx;

	hw_idx = ipa3_ctx->hw_type_index;

	if (client >= IPA_CLIENT_MAX || client < 0) {
		IPAERR_RL("Bad client number! client =%d\n", client);
		return IPA_EP_NOT_ALLOCATED;
	}

	if (!ipa3_ep_mapping[hw_idx][client].valid)
		return IPA_EP_NOT_ALLOCATED;

	ipa_ep_idx =
		ipa3_ep_mapping[hw_idx][client].ipa_gsi_ep_info.ipa_ep_num;
	if (ipa_ep_idx < 0 || (ipa_ep_idx >= ipa3_get_max_num_pipes()
		&& client != IPA_CLIENT_DUMMY_CONS))
		return IPA_EP_NOT_ALLOCATED;

	return ipa_ep_idx;
}
EXPORT_SYMBOL(ipa_get_ep_mapping);

/**
 * ipa_get_ep_mapping_from_gsi() - provide endpoint mapping
 * @ch_id: GSI Virt CH id
 *
 * Return value: endpoint mapping
 */
int ipa_get_ep_mapping_from_gsi(int ch_id)
{
	int ipa_ep_idx = IPA_EP_NOT_ALLOCATED;
	u8 hw_idx;
	int i = 0;

	hw_idx = ipa3_ctx->hw_type_index;

	if (ch_id >= GSI_CHAN_MAX || ch_id < 0) {
		IPAERR_RL("Bad ch_id number! ch_id =%d\n", ch_id);
		return IPA_EP_NOT_ALLOCATED;
	}

	for (i = 0; i < IPA_CLIENT_MAX; i++) {
		if (ipa3_ep_mapping[hw_idx][i].valid &&
			ipa3_ep_mapping[hw_idx][i].ipa_gsi_ep_info.ipa_gsi_chan_num
			== ch_id) {
			ipa_ep_idx = ipa3_ep_mapping[hw_idx][i].ipa_gsi_ep_info.ipa_ep_num;
			break;
		}
	}

	return ipa_ep_idx;
}

/**
 * ipa_get_gsi_ep_info() - provide gsi ep information
 * @client: IPA client value
 *
 * Return value: pointer to ipa_gsi_ep_info
 */
const struct ipa_gsi_ep_config *ipa_get_gsi_ep_info
	(enum ipa_client_type client)
{
	int ep_idx;
	u8 hw_idx;

	hw_idx = ipa3_ctx->hw_type_index;

	ep_idx = ipa_get_ep_mapping(client);
	if (ep_idx == IPA_EP_NOT_ALLOCATED)
		return NULL;

	if (!ipa3_ep_mapping[hw_idx][client].valid)
		return NULL;

	return &(ipa3_ep_mapping[hw_idx]
		[client].ipa_gsi_ep_info);
}
EXPORT_SYMBOL(ipa_get_gsi_ep_info);

/**
 * ipa_get_ep_group() - provide endpoint group by client
 * @client: client type
 *
 * Return value: endpoint group
 */
int ipa_get_ep_group(enum ipa_client_type client)
{
	u8 hw_idx;

	hw_idx = ipa3_ctx->hw_type_index;

	if (client >= IPA_CLIENT_MAX || client < 0) {
		IPAERR("Bad client number! client =%d\n", client);
		return -EINVAL;
	}

	if (!ipa3_ep_mapping[hw_idx][client].valid)
		return -EINVAL;

	return ipa3_ep_mapping[hw_idx][client].group_num;
}

/**
 * ipa3_get_qmb_master_sel() - provide QMB master selection for the client
 * @client: client type
 *
 * Return value: QMB master index
 */
u8 ipa3_get_qmb_master_sel(enum ipa_client_type client)
{
	u8 hw_idx;

	hw_idx = ipa3_ctx->hw_type_index;

	if (client >= IPA_CLIENT_MAX || client < 0) {
		IPAERR("Bad client number! client =%d\n", client);
		return -EINVAL;
	}

	if (!ipa3_ep_mapping[hw_idx][client].valid)
		return -EINVAL;

	return ipa3_ep_mapping[hw_idx]
		[client].qmb_master_sel;
}

/**
 * ipa3_get_tx_instance() - provide TX instance selection for the client
 * @client: client type
 *
 * Return value: TX instance
 */
u8 ipa3_get_tx_instance(enum ipa_client_type client)
{
	u8 hw_idx;

	hw_idx = ipa3_ctx->hw_type_index;

	IPADBG("ipa_get_ep_group: hw_idx = %d\n", hw_idx);

	if (client >= IPA_CLIENT_MAX || client < 0) {
		IPAERR("Bad client number! client =%d\n", client);
		return -EINVAL;
	}

	if (!ipa3_ep_mapping[hw_idx][client].valid)
		return -EINVAL;

	return ipa3_ep_mapping[hw_idx]
		[client].tx_instance;
}

/**
 * ipa3_set_client() - provide client mapping
 * @client: client type
 *
 * Return value: none
 */

void ipa3_set_client(int index, enum ipacm_client_enum client, bool uplink)
{
	if (client > IPACM_CLIENT_MAX || client < IPACM_CLIENT_USB) {
		IPAERR("Bad client number! client =%d\n", client);
	} else if (index >= ipa3_get_max_num_pipes() || index < 0) {
		IPAERR("Bad pipe index! index =%d\n", index);
	} else {
		ipa3_ctx->ipacm_client[index].client_enum = client;
		ipa3_ctx->ipacm_client[index].uplink = uplink;
	}
}
/**
 * ipa3_get_wlan_stats() - get ipa wifi stats
 *
 * Return value: success or failure
 */
int ipa3_get_wlan_stats(struct ipa_get_wdi_sap_stats *wdi_sap_stats)
{
	if (ipa3_ctx->uc_wdi_ctx.stats_notify) {
		ipa3_ctx->uc_wdi_ctx.stats_notify(IPA_GET_WDI_SAP_STATS,
			wdi_sap_stats);
	} else {
		IPAERR_RL("uc_wdi_ctx.stats_notify NULL\n");
		return -EFAULT;
	}
	return 0;
}

/**
 * ipa3_set_wlan_quota() - set ipa wifi quota
 * @wdi_quota: quota requirement
 *
 * Return value: success or failure
 */
int ipa3_set_wlan_quota(struct ipa_set_wifi_quota *wdi_quota)
{
	if (ipa3_ctx->uc_wdi_ctx.stats_notify) {
		ipa3_ctx->uc_wdi_ctx.stats_notify(IPA_SET_WIFI_QUOTA,
			wdi_quota);
	} else {
		IPAERR("uc_wdi_ctx.stats_notify NULL\n");
		return -EFAULT;
	}
	return 0;
}

/**
 * ipa3_inform_wlan_bw() - inform wlan bw-index
 *
 * Return value: success or failure
 */
int ipa3_inform_wlan_bw(struct ipa_inform_wlan_bw *wdi_bw)
{
	if (ipa3_ctx->uc_wdi_ctx.stats_notify) {
		ipa3_ctx->uc_wdi_ctx.stats_notify(IPA_INFORM_WLAN_BW,
			wdi_bw);
	} else {
		IPAERR("uc_wdi_ctx.stats_notify NULL\n");
		return -EFAULT;
	}
	return 0;
}

/**
 * ipa3_get_client() - provide client mapping
 * @client: client type
 *
 * Return value: client mapping enum
 */
enum ipacm_client_enum ipa3_get_client(int pipe_idx)
{
	if (pipe_idx >= ipa3_get_max_num_pipes() || pipe_idx < 0) {
		IPAERR("Bad pipe index! pipe_idx =%d\n", pipe_idx);
		return IPACM_CLIENT_MAX;
	} else {
		return ipa3_ctx->ipacm_client[pipe_idx].client_enum;
	}
}
EXPORT_SYMBOL(ipa3_get_client);

/**
 * ipa2_get_client_uplink() - provide client mapping
 * @client: client type
 *
 * Return value: none
 */
bool ipa3_get_client_uplink(int pipe_idx)
{
	if (pipe_idx < 0 || pipe_idx >= ipa3_get_max_num_pipes()) {
		IPAERR("invalid pipe idx %d\n", pipe_idx);
		return false;
	}

	return ipa3_ctx->ipacm_client[pipe_idx].uplink;
}


/**
 * ipa3_get_client_mapping() - provide client mapping
 * @pipe_idx: IPA end-point number
 *
 * Return value: client mapping
 */
enum ipa_client_type ipa3_get_client_mapping(int pipe_idx)
{
	if (pipe_idx >= ipa3_ctx->ipa_num_pipes || pipe_idx < 0) {
		IPAERR("Bad pipe index!\n");
		WARN_ON(1);
		return -EINVAL;
	}

	return ipa3_ctx->ep[pipe_idx].client;
}
EXPORT_SYMBOL(ipa3_get_client_mapping);

/**
 * ipa3_get_client_by_pipe() - return client type relative to pipe
 * index
 * @pipe_idx: IPA end-point number
 *
 * Return value: client type
 */
enum ipa_client_type ipa3_get_client_by_pipe(int pipe_idx)
{
	int j = 0;
	u8 hw_type_idx;

	hw_type_idx = ipa3_ctx->hw_type_index;

	for (j = 0; j < IPA_CLIENT_MAX; j++) {
		const struct ipa_ep_configuration *iec_ptr =
			&(ipa3_ep_mapping[hw_type_idx][j]);
		if (iec_ptr->valid &&
		    iec_ptr->ipa_gsi_ep_info.ipa_ep_num == pipe_idx)
			break;
	}

	return j;
}

/**
 * ipa_init_ep_flt_bitmap() - Initialize the bitmap
 * that represents the End-points that supports filtering
 */
void ipa_init_ep_flt_bitmap(void)
{
	enum ipa_client_type cl;
	u8 hw_idx;
	u64 bitmap;
	u32 pipe_num;
	const struct ipa_gsi_ep_config *gsi_ep_ptr;

	hw_idx = ipa3_ctx->hw_type_index;
	bitmap = 0;
	if (ipa3_ctx->ep_flt_bitmap) {
		IPADBG("EP Filter bitmap is already initialized\n");
		return;
	}

	for (cl = 0; cl < IPA_CLIENT_MAX ; cl++) {
		/* In normal mode don't add filter support test pipes*/
		if (ipa3_ep_mapping[hw_idx][cl].support_flt &&
		    (!IPA_CLIENT_IS_TEST(cl) ||
		     ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_VIRTUAL ||
		     ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_EMULATION ||
		     ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_TEST)) {
			gsi_ep_ptr =
				&ipa3_ep_mapping[hw_idx][cl].ipa_gsi_ep_info;
			pipe_num = gsi_ep_ptr->ipa_ep_num;
			bitmap |= (1ULL << pipe_num);
			if (bitmap != ipa3_ctx->ep_flt_bitmap) {
				ipa3_ctx->ep_flt_bitmap = bitmap;
				ipa3_ctx->ep_flt_num++;
			}
		}
	}
}

/**
 * ipa_is_ep_support_flt() - Given an End-point check
 * whether it supports filtering or not.
 *
 * @pipe_idx:
 *
 * Return values:
 * true if supports and false if not
 */
bool ipa_is_ep_support_flt(int pipe_idx)
{
	if (pipe_idx >= ipa3_ctx->ipa_num_pipes || pipe_idx < 0) {
		IPAERR("Bad pipe index!\n");
		return false;
	}

	return ipa3_ctx->ep_flt_bitmap & (1ULL<<pipe_idx);
}

/**
 * ipa3_cfg_ep_seq() - IPA end-point HPS/DPS sequencer type configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_seq(u32 clnt_hdl, const struct ipa_ep_cfg_seq *seq_cfg)
{
	int type;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad param, clnt_hdl = %d", clnt_hdl);
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(ipa3_ctx->ep[clnt_hdl].client)) {
		IPAERR("SEQ does not apply to IPA consumer EP %d\n", clnt_hdl);
		return -EINVAL;
	}

	/*
	 * Skip Configure sequencers type for test clients.
	 * These are configured dynamically in ipa3_cfg_ep_mode
	 */
	if (IPA_CLIENT_IS_TEST(ipa3_ctx->ep[clnt_hdl].client)) {
		IPADBG("Skip sequencers configuration for test clients\n");
		return 0;
	}

	if (seq_cfg->set_dynamic)
		type = seq_cfg->seq_type;
	else
		type = ipa3_ep_mapping[ipa3_ctx->hw_type_index]
			[ipa3_ctx->ep[clnt_hdl].client].sequencer_type;

	if (type != IPA_DPS_HPS_SEQ_TYPE_INVALID) {
		if (ipa3_ctx->ep[clnt_hdl].cfg.mode.mode == IPA_DMA &&
			!IPA_DPS_HPS_SEQ_TYPE_IS_DMA(type)) {
			IPAERR("Configuring non-DMA SEQ type to DMA pipe\n");
			WARN_ON(1);
			return -EINVAL;
		}
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));
		/* Configure sequencers type*/

		IPADBG("set sequencers to sequence 0x%x, ep = %d\n", type,
				clnt_hdl);
		ipahal_write_reg_n(IPA_ENDP_INIT_SEQ_n, clnt_hdl, type);

		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	} else {
		IPADBG("should not set sequencer type of ep = %d\n", clnt_hdl);
	}

	return 0;
}

/**
 * ipa3_cfg_ep - IPA end-point configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * This includes nat, IPv6CT, header, mode, aggregation and route settings and
 * is a one shot API to configure the IPA end-point fully
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep(u32 clnt_hdl, const struct ipa_ep_cfg *ipa_ep_cfg)
{
	int result = -EINVAL;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ipa_ep_cfg == NULL) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	result = ipa3_cfg_ep_hdr(clnt_hdl, &ipa_ep_cfg->hdr);
	if (result)
		return result;

	result = ipa3_cfg_ep_hdr_ext(clnt_hdl, &ipa_ep_cfg->hdr_ext);
	if (result)
		return result;

	result = ipa3_cfg_ep_aggr(clnt_hdl, &ipa_ep_cfg->aggr);
	if (result)
		return result;

	result = ipa3_cfg_ep_cfg(clnt_hdl, &ipa_ep_cfg->cfg);
	if (result)
		return result;

	if (ipa3_is_ulso_supported()) {
		result = ipa3_cfg_ep_ulso(clnt_hdl,
			&ipa_ep_cfg->ulso);
		if (result)
			return result;
	}

	if (IPA_CLIENT_IS_PROD(ipa3_ctx->ep[clnt_hdl].client)) {
		result = ipa3_cfg_ep_nat(clnt_hdl, &ipa_ep_cfg->nat);
		if (result)
			return result;

		if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
			result = ipa3_cfg_ep_conn_track(clnt_hdl,
				&ipa_ep_cfg->conn_track);
			if (result)
				return result;
		}

		result = ipa3_cfg_ep_mode(clnt_hdl, &ipa_ep_cfg->mode);
		if (result)
			return result;

		result = ipa3_cfg_ep_seq(clnt_hdl, &ipa_ep_cfg->seq);
		if (result)
			return result;

		result = ipa3_cfg_ep_route(clnt_hdl, &ipa_ep_cfg->route);
		if (result)
			return result;

		result = ipa3_cfg_ep_deaggr(clnt_hdl, &ipa_ep_cfg->deaggr);
		if (result)
			return result;
	} else {
		result = ipa3_cfg_ep_metadata_mask(clnt_hdl,
				&ipa_ep_cfg->metadata_mask);
		if (result)
			return result;

		if (ipa3_ctx->ipa_hw_type >= IPA_HW_v5_5) {
			result = ipa3_cfg_ep_prod_cfg(clnt_hdl, &ipa_ep_cfg->prod_cfg);
			if (result)
				return result;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ipa3_cfg_ep);

static const char *ipa3_get_nat_en_str(enum ipa_nat_en_type nat_en)
{
	switch (nat_en) {
	case (IPA_BYPASS_NAT):
		return "NAT disabled";
	case (IPA_SRC_NAT):
		return "Source NAT";
	case (IPA_DST_NAT):
		return "Dst NAT";
	}

	return "undefined";
}

static const char *ipa3_get_ipv6ct_en_str(enum ipa_ipv6ct_en_type ipv6ct_en)
{
	switch (ipv6ct_en) {
	case (IPA_BYPASS_IPV6CT):
		return "ipv6ct disabled";
	case (IPA_ENABLE_IPV6CT):
		return "ipv6ct enabled";
	}

	return "undefined";
}

/**
 * ipa3_cfg_ep_nat() - IPA end-point NAT configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ep_nat:	[in] IPA NAT end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_nat(u32 clnt_hdl, const struct ipa_ep_cfg_nat *ep_nat)
{
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_nat == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl,
					ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(ipa3_ctx->ep[clnt_hdl].client)) {
		IPAERR("NAT does not apply to IPA out EP %d\n", clnt_hdl);
		return -EINVAL;
	}

	IPADBG("pipe=%d, nat_en=%d(%s), nat_exc_suppress=%d\n",
			clnt_hdl,
			ep_nat->nat_en,
			ipa3_get_nat_en_str(ep_nat->nat_en),
			ep_nat->nat_exc_suppress);

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.nat = *ep_nat;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_NAT_n, clnt_hdl, ep_nat);

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v5_5) {
		ipahal_write_reg_n_fields(IPA_ENDP_INIT_NAT_EXC_SUPPRESS_n,
			clnt_hdl, ep_nat);
	}

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

/**
 * ipa3_cfg_ep_conn_track() - IPA end-point IPv6CT configuration
 * @clnt_hdl:		[in] opaque client handle assigned by IPA to client
 * @ep_conn_track:	[in] IPA IPv6CT end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_conn_track(u32 clnt_hdl,
	const struct ipa_ep_cfg_conn_track *ep_conn_track)
{
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_conn_track == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
			clnt_hdl,
			ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(ipa3_ctx->ep[clnt_hdl].client)) {
		IPAERR("IPv6CT does not apply to IPA out EP %d\n", clnt_hdl);
		return -EINVAL;
	}

	IPADBG("pipe=%d, conn_track_en=%d(%s)\n",
		clnt_hdl,
		ep_conn_track->conn_track_en,
		ipa3_get_ipv6ct_en_str(ep_conn_track->conn_track_en));

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.conn_track = *ep_conn_track;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_CONN_TRACK_n, clnt_hdl,
		ep_conn_track);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}


/**
 * ipa3_cfg_ep_status() - IPA end-point status configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_status(u32 clnt_hdl,
	const struct ipahal_reg_ep_cfg_status *ep_status)
{
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_status == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl,
					ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d, status_en=%d status_ep=%d status_location=%d\n",
			clnt_hdl,
			ep_status->status_en,
			ep_status->status_ep,
			ep_status->status_location);

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].status = *ep_status;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipahal_write_reg_n_fields(IPA_ENDP_STATUS_n, clnt_hdl, ep_status);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

/**
 * ipa3_cfg_ep_cfg_pipe_replicate() - IPA end-point cfg
 * pipe replication
 * @clnt_hdl:   [in] opaque client handle assigned by IPA to client
 *
 * Return value: none
 */
void ipa3_cfg_ep_cfg_pipe_replicate(u32 clnt_hdl)
{
	/* Enable ADPL v6 Feature for certain IPA clients */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v5_5) {
		switch (ipa3_get_client_mapping(clnt_hdl)) {
		case IPA_CLIENT_USB_PROD:
		case IPA_CLIENT_APPS_WAN_PROD:
		case IPA_CLIENT_WLAN2_PROD:
		case IPA_CLIENT_WIGIG_PROD:
		case IPA_CLIENT_APPS_LAN_PROD:
		case IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_PROD:
		case IPA_CLIENT_APPS_LAN_COAL_CONS:
		case IPA_CLIENT_APPS_LAN_CONS:
		case IPA_CLIENT_APPS_WAN_COAL_CONS:
		case IPA_CLIENT_APPS_WAN_CONS:
		case IPA_CLIENT_WIGIG1_CONS:
		case IPA_CLIENT_WLAN2_CONS:
		case IPA_CLIENT_WLAN2_CONS1:
		case IPA_CLIENT_USB_CONS:
		case IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS:
		case IPA_CLIENT_WIGIG2_CONS:
		case IPA_CLIENT_WIGIG3_CONS:
		case IPA_CLIENT_WLAN3_PROD:
		case IPA_CLIENT_ETHERNET2_PROD:
		case IPA_CLIENT_USB2_PROD:
		case IPA_CLIENT_ETHERNET_PROD:
		case IPA_CLIENT_ETHERNET_CONS:
		case IPA_CLIENT_ETHERNET2_CONS:
		case IPA_CLIENT_USB2_CONS:
		case IPA_CLIENT_WLAN4_CONS:
			ipa3_ctx->ep[clnt_hdl].cfg.cfg.pipe_replicate_en = 1;
			break;
		default:
			ipa3_ctx->ep[clnt_hdl].cfg.cfg.pipe_replicate_en = 0;
		}
	}
}

/**
 * ipa3_cfg_ep_cfg() - IPA end-point cfg configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_cfg(u32 clnt_hdl, const struct ipa_ep_cfg_cfg *cfg)
{
	u8 qmb_master_sel;
	u8 tx_instance;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || cfg == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl,
					ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.cfg = *cfg;

	ipa3_cfg_ep_cfg_pipe_replicate(clnt_hdl);

	/* Override QMB master selection */
	qmb_master_sel = ipa3_get_qmb_master_sel(ipa3_ctx->ep[clnt_hdl].client);
	ipa3_ctx->ep[clnt_hdl].cfg.cfg.gen_qmb_master_sel = qmb_master_sel;
	IPADBG(
	       "pipe=%d, frag_ofld_en=%d cs_ofld_en=%d mdata_hdr_ofst=%d "
	       "gen_qmb_master_sel=%d pipe_replicate_en=%d\n",
			clnt_hdl,
			ipa3_ctx->ep[clnt_hdl].cfg.cfg.frag_offload_en,
			ipa3_ctx->ep[clnt_hdl].cfg.cfg.cs_offload_en,
			ipa3_ctx->ep[clnt_hdl].cfg.cfg.cs_metadata_hdr_offset,
			ipa3_ctx->ep[clnt_hdl].cfg.cfg.gen_qmb_master_sel,
			ipa3_ctx->ep[clnt_hdl].cfg.cfg.pipe_replicate_en);

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_CFG_n, clnt_hdl,
				  &ipa3_ctx->ep[clnt_hdl].cfg.cfg);

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v5_0 &&
		IPA_CLIENT_IS_CONS(ipa3_ctx->ep[clnt_hdl].client)) {
		tx_instance = ipa3_get_tx_instance(ipa3_ctx->ep[clnt_hdl].client);
		if (tx_instance == -EINVAL) {
			IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
				clnt_hdl,
				ipa3_ctx->ep[clnt_hdl].valid);
			IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
			return -EINVAL;
		}
		ipa3_ctx->ep[clnt_hdl].cfg.cfg.tx_instance = tx_instance;
		ipahal_write_reg_n(IPA_ENDP_INIT_PROD_CFG_n, clnt_hdl,
			ipa3_ctx->ep[clnt_hdl].cfg.cfg.tx_instance);
	}

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

/**
 * ipa3_cfg_ep_prod_cfg() - IPA Producer end-point configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @prod_cfg:	[in] Producer specific configuration
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_prod_cfg(u32 clnt_hdl, const struct ipa_ep_cfg_prod_cfg *prod_cfg)
{
	u8 tx_instance;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 ||
	    IPA_CLIENT_IS_PROD(ipa3_ctx->ep[clnt_hdl].client) ||
	    prod_cfg == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl,
					ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.prod_cfg = *prod_cfg;

	tx_instance = ipa3_get_tx_instance(ipa3_ctx->ep[clnt_hdl].client);
	if (tx_instance == -EINVAL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
			clnt_hdl,
			ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}
	ipa3_ctx->ep[clnt_hdl].cfg.cfg.tx_instance = tx_instance;
	ipa3_ctx->ep[clnt_hdl].cfg.prod_cfg.tx_instance = tx_instance;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_PROD_CFG_n, clnt_hdl,
		&ipa3_ctx->ep[clnt_hdl].cfg.prod_cfg);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}


/**
 * ipa3_cfg_ep_metadata_mask() - IPA end-point meta-data mask configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_metadata_mask(u32 clnt_hdl,
		const struct ipa_ep_cfg_metadata_mask
		*metadata_mask)
{
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || metadata_mask == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl,
					ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d, metadata_mask=0x%x\n",
			clnt_hdl,
			metadata_mask->metadata_mask);

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.metadata_mask = *metadata_mask;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_HDR_METADATA_MASK_n,
		clnt_hdl, metadata_mask);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

/**
 * ipa3_cfg_ep_hdr() -  IPA end-point header configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_hdr(u32 clnt_hdl, const struct ipa_ep_cfg_hdr *ep_hdr)
{
	struct ipa3_ep_context *ep;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_hdr == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
				clnt_hdl, ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}
	IPADBG("pipe=%d metadata_reg_valid=%d\n",
		clnt_hdl,
		ep_hdr->hdr_metadata_reg_valid);

	IPADBG("remove_additional=%d, a5_mux=%d, ofst_pkt_size=0x%x\n",
		ep_hdr->hdr_remove_additional,
		ep_hdr->hdr_a5_mux,
		ep_hdr->hdr_ofst_pkt_size);

	IPADBG("ofst_pkt_size_valid=%d, additional_const_len=0x%x\n",
		ep_hdr->hdr_ofst_pkt_size_valid,
		ep_hdr->hdr_additional_const_len);

	IPADBG("ofst_metadata=0x%x, ofst_metadata_valid=%d, len=0x%x\n",
		ep_hdr->hdr_ofst_metadata,
		ep_hdr->hdr_ofst_metadata_valid,
		ep_hdr->hdr_len);

	ep = &ipa3_ctx->ep[clnt_hdl];

	/* copy over EP cfg */
	ep->cfg.hdr = *ep_hdr;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_HDR_n, clnt_hdl, &ep->cfg.hdr);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

/**
 * ipa3_cfg_ep_hdr_ext() -  IPA end-point extended header configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ep_hdr_ext:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_hdr_ext(u32 clnt_hdl,
		       const struct ipa_ep_cfg_hdr_ext *ep_hdr_ext)
{
	struct ipa3_ep_context *ep;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_hdr_ext == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
				clnt_hdl, ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d hdr_pad_to_alignment=%d\n",
		clnt_hdl,
		ep_hdr_ext->hdr_pad_to_alignment);

	IPADBG("hdr_total_len_or_pad_offset=%d\n",
		ep_hdr_ext->hdr_total_len_or_pad_offset);

	IPADBG("hdr_payload_len_inc_padding=%d hdr_total_len_or_pad=%d\n",
		ep_hdr_ext->hdr_payload_len_inc_padding,
		ep_hdr_ext->hdr_total_len_or_pad);

	IPADBG("hdr_total_len_or_pad_valid=%d hdr_little_endian=%d\n",
		ep_hdr_ext->hdr_total_len_or_pad_valid,
		ep_hdr_ext->hdr_little_endian);

	ep = &ipa3_ctx->ep[clnt_hdl];

	/* copy over EP cfg */
	ep->cfg.hdr_ext = *ep_hdr_ext;
	ep->cfg.hdr_ext.hdr = &ep->cfg.hdr;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_HDR_EXT_n, clnt_hdl,
		&ep->cfg.hdr_ext);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

/**
 * ipa3_cfg_ep_ulso() -  IPA end-point ulso configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ep_ulso:	[in] IPA end-point ulso configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_ulso(u32 clnt_hdl, const struct ipa_ep_cfg_ulso *ep_ulso)
{
	struct ipa3_ep_context *ep;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_ulso == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
				clnt_hdl, ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d ipid_min_max_idx=%d is_ulso_pipe=%d\n",
		clnt_hdl, ep_ulso->ipid_min_max_idx, ep_ulso->is_ulso_pipe);

	ep = &ipa3_ctx->ep[clnt_hdl];

	/* copy over EP cfg */
	ep->cfg.ulso = *ep_ulso;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipahal_write_reg_n(IPA_ENDP_INIT_ULSO_CFG_n, clnt_hdl,
		ep->cfg.ulso.ipid_min_max_idx);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

/**
 * ipa_cfg_ep_ctrl() -  IPA end-point Control configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg_ctrl:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_cfg_ep_ctrl(u32 clnt_hdl, const struct ipa_ep_cfg_ctrl *ep_ctrl)
{
	int code = 0, result;
	struct ipa3_ep_context *ep;
	bool primary_secondry;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes || ep_ctrl == NULL) {
		IPAERR("bad parm, clnt_hdl = %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0 && ep_ctrl->ipa_ep_suspend) {
		IPAERR("pipe suspend is not supported\n");
		WARN_ON(1);
		return -EPERM;
	}

	if (ipa3_ctx->ipa_endp_delay_wa) {
		IPAERR("pipe setting delay is not supported\n");
		return 0;
	}

	IPADBG("pipe=%d ep_suspend=%d, ep_delay=%d\n",
		clnt_hdl,
		ep_ctrl->ipa_ep_suspend,
		ep_ctrl->ipa_ep_delay);
	ep = &ipa3_ctx->ep[clnt_hdl];

	if (ipa3_ctx->ipa_endp_delay_wa_v2 &&
		IPA_CLIENT_IS_PROD(ep->client)) {

		IPADBG("Configuring flow control for pipe = %d\n", clnt_hdl);
		/* Configure enhanced flow control instead of delay
		 * Q6 controlled AP pipes(USB PROD and MHI_PROD) configuring the
		 * secondary flow control.
		 * AP controlled pipe configuring primary flow control.
		 */
		if (ep->client == IPA_CLIENT_USB_PROD ||
			ep->client == IPA_CLIENT_MHI_PROD ||
			ep->client == IPA_CLIENT_MHI_LOW_LAT_PROD)
			primary_secondry = true;
		else
			primary_secondry = false;

		result = gsi_flow_control_ee(ep->gsi_chan_hdl, clnt_hdl, 0,
				ep_ctrl->ipa_ep_delay, primary_secondry, &code);
		if (result == GSI_STATUS_SUCCESS) {
			IPADBG("flow control sussess gsi ch %d with code %d\n",
					ep->gsi_chan_hdl, code);
		} else {
			IPADBG("failed to flow control gsi ch %d code %d\n",
					ep->gsi_chan_hdl, code);
		}
		return 0;
	}

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_CTRL_n, clnt_hdl, ep_ctrl);

	if (ep_ctrl->ipa_ep_suspend == true &&
			IPA_CLIENT_IS_CONS(ipa3_ctx->ep[clnt_hdl].client))
		ipa3_suspend_active_aggr_wa(clnt_hdl);

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_ctrl);

const char *ipa3_get_mode_type_str(enum ipa_mode_type mode)
{
	switch (mode) {
	case (IPA_BASIC):
		return "Basic";
	case (IPA_ENABLE_FRAMING_HDLC):
		return "HDLC framing";
	case (IPA_ENABLE_DEFRAMING_HDLC):
		return "HDLC de-framing";
	case (IPA_DMA):
		return "DMA";
	}

	return "undefined";
}

/**
 * ipa3_cfg_ep_mode() - IPA end-point mode configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_mode(u32 clnt_hdl, const struct ipa_ep_cfg_mode *ep_mode)
{
	int ep;
	int type;
	struct ipahal_reg_endp_init_mode init_mode;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_mode == NULL) {
		IPAERR("bad params clnt_hdl=%d , ep_valid=%d ep_mode=%pK\n",
				clnt_hdl, ipa3_ctx->ep[clnt_hdl].valid,
				ep_mode);
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(ipa3_ctx->ep[clnt_hdl].client)) {
		IPAERR("MODE does not apply to IPA out EP %d\n", clnt_hdl);
		return -EINVAL;
	}

	ep = ipa_get_ep_mapping(ep_mode->dst);
	if (ep == -1 && ep_mode->mode == IPA_DMA) {
		IPAERR("dst %d does not exist in DMA mode\n", ep_mode->dst);
		return -EINVAL;
	}

	WARN_ON(ep_mode->mode == IPA_DMA && IPA_CLIENT_IS_PROD(ep_mode->dst));

	if (!IPA_CLIENT_IS_CONS(ep_mode->dst))
		ep = ipa_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS);

	IPADBG("pipe=%d mode=%d(%s), dst_client_number=%d\n",
			clnt_hdl,
			ep_mode->mode,
			ipa3_get_mode_type_str(ep_mode->mode),
			ep_mode->dst);

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.mode = *ep_mode;
	ipa3_ctx->ep[clnt_hdl].dst_pipe_index = ep;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	init_mode.dst_pipe_number = ipa3_ctx->ep[clnt_hdl].dst_pipe_index;
	init_mode.ep_mode = *ep_mode;
	ipahal_write_reg_n_fields(IPA_ENDP_INIT_MODE_n, clnt_hdl, &init_mode);

	 /* Configure sequencers type for test clients*/
	if (IPA_CLIENT_IS_TEST(ipa3_ctx->ep[clnt_hdl].client)) {
		if (ep_mode->mode == IPA_DMA)
			type = IPA_DPS_HPS_SEQ_TYPE_DMA_ONLY;
		else
			/* In IPA4.2 only single pass only supported*/
			if (ipa3_ctx->ipa_hw_type == IPA_HW_v4_2)
				type =
				IPA_DPS_HPS_SEQ_TYPE_PKT_PROCESS_NO_DEC_NO_UCP;
			else
				type =
			IPA_DPS_HPS_SEQ_TYPE_2ND_PKT_PROCESS_PASS_NO_DEC_UCP;

		IPADBG(" set sequencers to sequance 0x%x, ep = %d\n", type,
				clnt_hdl);
		ipahal_write_reg_n(IPA_ENDP_INIT_SEQ_n, clnt_hdl, type);
	}
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

const char *ipa3_get_aggr_enable_str(enum ipa_aggr_en_type aggr_en)
{
	switch (aggr_en) {
	case (IPA_BYPASS_AGGR):
			return "no aggregation";
	case (IPA_ENABLE_AGGR):
			return "aggregation enabled";
	case (IPA_ENABLE_DEAGGR):
		return "de-aggregation enabled";
	}

	return "undefined";
}

const char *ipa3_get_aggr_type_str(enum ipa_aggr_type aggr_type)
{
	switch (aggr_type) {
	case (IPA_MBIM_16):
			return "MBIM_16";
	case (IPA_HDLC):
		return "HDLC";
	case (IPA_TLP):
			return "TLP";
	case (IPA_RNDIS):
			return "RNDIS";
	case (IPA_GENERIC):
			return "GENERIC";
	case (IPA_QCMAP):
			return "QCMAP";
	case (IPA_COALESCE):
			return "COALESCE";
	}
	return "undefined";
}

static u32 ipa3_time_gran_usec_step(enum ipa_timers_time_gran_type gran)
{
	switch (gran) {
	case IPA_TIMERS_TIME_GRAN_10_USEC:		return 10;
	case IPA_TIMERS_TIME_GRAN_20_USEC:		return 20;
	case IPA_TIMERS_TIME_GRAN_50_USEC:		return 50;
	case IPA_TIMERS_TIME_GRAN_100_USEC:		return 100;
	case IPA_TIMERS_TIME_GRAN_1_MSEC:		return 1000;
	case IPA_TIMERS_TIME_GRAN_10_MSEC:		return 10000;
	case IPA_TIMERS_TIME_GRAN_100_MSEC:		return 100000;
	case IPA_TIMERS_TIME_GRAN_NEAR_HALF_SEC:	return 655350;
	default:
		IPAERR("Invalid granularity time unit %d\n", gran);
		ipa_assert();
		break;
	}

	return 100;
}

/*
 * ipa3_process_timer_cfg() - Check and produce timer config
 *
 * Relevant for IPA 4.5 and above
 *
 * Assumes clocks are voted
 */
static int ipa3_process_timer_cfg(u32 time_us,
	u8 *pulse_gen, u8 *time_units)
{
	struct ipahal_reg_timers_pulse_gran_cfg gran_cfg;
	u32 gran0_step, gran1_step, gran2_step;

	IPADBG("time in usec=%u\n", time_us);

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_5) {
		IPAERR("Invalid IPA version %d\n", ipa3_ctx->ipa_hw_type);
		return -EPERM;
	}

	if (!time_us) {
		*pulse_gen = 0;
		*time_units = 0;
		return 0;
	}

	ipahal_read_reg_fields(IPA_TIMERS_PULSE_GRAN_CFG, &gran_cfg);

	gran0_step = ipa3_time_gran_usec_step(gran_cfg.gran_0);
	gran1_step = ipa3_time_gran_usec_step(gran_cfg.gran_1);
	gran2_step = ipa3_time_gran_usec_step(gran_cfg.gran_2);
	/* gran_3 is not used by AP */

	IPADBG("gran0 usec step=%u  gran1 usec step=%u gran2 usec step=%u\n",
		gran0_step, gran1_step, gran2_step);

	/* Lets try pulse generator #0 granularity */
	if (!(time_us % gran0_step)) {
		if ((time_us / gran0_step) <= IPA_TIMER_SCALED_TIME_LIMIT) {
			*pulse_gen = 0;
			*time_units = time_us / gran0_step;
			IPADBG("Matched: generator=0, units=%u\n",
				*time_units);
			return 0;
		}
		IPADBG("gran0 cannot be used due to range limit\n");
	}

	/* Lets try pulse generator #1 granularity */
	if (!(time_us % gran1_step)) {
		if ((time_us / gran1_step) <= IPA_TIMER_SCALED_TIME_LIMIT) {
			*pulse_gen = 1;
			*time_units = time_us / gran1_step;
			IPADBG("Matched: generator=1, units=%u\n",
				*time_units);
			return 0;
		}
		IPADBG("gran1 cannot be used due to range limit\n");
	}

	/* Lets try pulse generator #2 granularity */
	if (!(time_us % gran2_step)) {
		if ((time_us / gran2_step) <= IPA_TIMER_SCALED_TIME_LIMIT) {
			*pulse_gen = 2;
			*time_units = time_us / gran2_step;
			IPADBG("Matched: generator=2, units=%u\n",
				*time_units);
			return 0;
		}
		IPADBG("gran2 cannot be used due to range limit\n");
	}

	IPAERR("Cannot match requested time to configured granularities\n");
	return -EPERM;
}

/**
 * ipa3_cfg_ep_aggr() - IPA end-point aggregation configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_aggr(u32 clnt_hdl, const struct ipa_ep_cfg_aggr *ep_aggr)
{
	int res = 0;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_aggr == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
			clnt_hdl, ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	if (ep_aggr->aggr_en == IPA_ENABLE_DEAGGR &&
	    !IPA_EP_SUPPORTS_DEAGGR(clnt_hdl)) {
		IPAERR("pipe=%d cannot be configured to DEAGGR\n", clnt_hdl);
		WARN_ON(1);
		return -EINVAL;
	}

	IPADBG("pipe=%d en=%d(%s), type=%d(%s), byte_limit=%d, time_limit=%d\n",
			clnt_hdl,
			ep_aggr->aggr_en,
			ipa3_get_aggr_enable_str(ep_aggr->aggr_en),
			ep_aggr->aggr,
			ipa3_get_aggr_type_str(ep_aggr->aggr),
			ep_aggr->aggr_byte_limit,
			ep_aggr->aggr_time_limit);
	IPADBG("hard_byte_limit_en=%d aggr_sw_eof_active=%d\n",
		ep_aggr->aggr_hard_byte_limit_en,
		ep_aggr->aggr_sw_eof_active);

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.aggr = *ep_aggr;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5) {
		res = ipa3_process_timer_cfg(ep_aggr->aggr_time_limit,
			&ipa3_ctx->ep[clnt_hdl].cfg.aggr.pulse_generator,
			&ipa3_ctx->ep[clnt_hdl].cfg.aggr.scaled_time);
		if (res) {
			IPAERR("failed to process AGGR timer tmr=%u\n",
				ep_aggr->aggr_time_limit);
			res = -EINVAL;
			goto complete;
		}
		/*
		 * HW bug on IPA4.5 where gran is used from pipe 0 instead of
		 * coal pipe. Add this check to make sure that RSC pipe will use
		 * gran 0 per the requested time needed; pipe 0 will use always
		 * gran 0 as gran 0 is the POR value of it and s/w never change
		 * it.
		 */
		if (ipa3_ctx->ipa_hw_type == IPA_HW_v4_5 &&
		    ipa3_get_client_mapping(clnt_hdl) ==
		    IPA_CLIENT_APPS_WAN_COAL_CONS &&
		    ipa3_ctx->ep[clnt_hdl].cfg.aggr.pulse_generator != 0) {
			IPAERR("coal pipe using GRAN_SEL = %d\n",
			       ipa3_ctx->ep[clnt_hdl].cfg.aggr.pulse_generator);
			ipa_assert();
		}
	} else {
		/*
		 * Global aggregation granularity is 0.5msec.
		 * So if H/W programmed with 1msec, it will be
		 *  0.5msec defacto.
		 * So finest granularity is 0.5msec
		 */
		if (ep_aggr->aggr_time_limit % 500) {
			IPAERR("given time limit %u is not in 0.5msec\n",
				ep_aggr->aggr_time_limit);
			WARN_ON(1);
			res = -EINVAL;
			goto complete;
		}

		/* Due to described above global granularity */
		ipa3_ctx->ep[clnt_hdl].cfg.aggr.aggr_time_limit *= 2;
	}

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_AGGR_n, clnt_hdl,
			&ipa3_ctx->ep[clnt_hdl].cfg.aggr);
complete:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return res;
}

/**
 * ipa3_cfg_ep_route() - IPA end-point routing configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_route(u32 clnt_hdl, const struct ipa_ep_cfg_route *ep_route)
{
	struct ipahal_reg_endp_init_route init_rt;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_route == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
			clnt_hdl, ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(ipa3_ctx->ep[clnt_hdl].client)) {
		IPAERR("ROUTE does not apply to IPA out EP %d\n",
				clnt_hdl);
		return -EINVAL;
	}

	/*
	 * if DMA mode was configured previously for this EP, return with
	 * success
	 */
	if (ipa3_ctx->ep[clnt_hdl].cfg.mode.mode == IPA_DMA) {
		IPADBG("DMA enabled for ep %d, dst pipe is part of DMA\n",
				clnt_hdl);
		return 0;
	}

	if (ep_route->rt_tbl_hdl)
		IPAERR("client specified non-zero RT TBL hdl - ignore it\n");

	IPADBG("pipe=%d, rt_tbl_hdl=%d\n",
			clnt_hdl,
			ep_route->rt_tbl_hdl);

	/* always use "default" routing table when programming EP ROUTE reg */
	ipa3_ctx->ep[clnt_hdl].rt_tbl_idx =
		IPA_MEM_PART(v4_apps_rt_index_lo);

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

		init_rt.route_table_index = ipa3_ctx->ep[clnt_hdl].rt_tbl_idx;
		ipahal_write_reg_n_fields(IPA_ENDP_INIT_ROUTE_n,
			clnt_hdl, &init_rt);

		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	}

	return 0;
}

#define MAX_ALLOWED_BASE_VAL 0x1f
#define MAX_ALLOWED_SCALE_VAL 0x1f

/**
 * ipa3_cal_ep_holb_scale_base_val - calculate base and scale value from tmr_val
 *
 * In IPA4.2 HW version need configure base and scale value in HOL timer reg
 * @tmr_val: [in] timer value for HOL timer
 * @ipa_ep_cfg: [out] Fill IPA end-point configuration base and scale value
 *			and return
 */
void ipa3_cal_ep_holb_scale_base_val(u32 tmr_val,
				struct ipa_ep_cfg_holb *ep_holb)
{
	u32 base_val, scale, scale_val = 1, base = 2;

	for (scale = 0; scale <= MAX_ALLOWED_SCALE_VAL; scale++) {
		base_val = tmr_val/scale_val;
		if (scale != 0)
			scale_val *= base;
		if (base_val <= MAX_ALLOWED_BASE_VAL)
			break;
	}
	ep_holb->base_val = base_val;
	ep_holb->scale = scale_val;

}

/**
 * ipa3_cfg_ep_holb() - IPA end-point holb configuration
 *
 * If an IPA producer pipe is full, IPA HW by default will block
 * indefinitely till space opens up. During this time no packets
 * including those from unrelated pipes will be processed. Enabling
 * HOLB means IPA HW will be allowed to drop packets as/when needed
 * and indefinite blocking is avoided.
 *
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_cfg_ep_holb(u32 clnt_hdl, const struct ipa_ep_cfg_holb *ep_holb)
{
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_holb == NULL ||
	    ep_holb->tmr_val > ipa3_ctx->ctrl->max_holb_tmr_val ||
	    ep_holb->en > 1) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_PROD(ipa3_ctx->ep[clnt_hdl].client)) {
		IPAERR("HOLB does not apply to IPA in EP %d\n", clnt_hdl);
		return -EINVAL;
	}

	ipa3_ctx->ep[clnt_hdl].holb = *ep_holb;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	if (ep_holb->en == IPA_HOLB_TMR_DIS) {
		ipahal_write_reg_n_fields(IPA_ENDP_INIT_HOL_BLOCK_EN_n,
			clnt_hdl, ep_holb);
		goto success;
	}

	/* Follow HPG sequence to DIS_HOLB, Configure Timer, and HOLB_EN */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5) {
		ipa3_ctx->ep[clnt_hdl].holb.en = IPA_HOLB_TMR_DIS;
		ipahal_write_reg_n_fields(IPA_ENDP_INIT_HOL_BLOCK_EN_n,
			clnt_hdl, ep_holb);
	}

	/* Configure timer */
	if (ipa3_ctx->ipa_hw_type == IPA_HW_v4_2) {
		ipa3_cal_ep_holb_scale_base_val(ep_holb->tmr_val,
			&ipa3_ctx->ep[clnt_hdl].holb);
	}
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5) {
		int res;

		res = ipa3_process_timer_cfg(ep_holb->tmr_val * 1000,
			&ipa3_ctx->ep[clnt_hdl].holb.pulse_generator,
			&ipa3_ctx->ep[clnt_hdl].holb.scaled_time);
		if (res) {
			IPAERR("failed to process HOLB timer tmr=%u\n",
				ep_holb->tmr_val);
			ipa_assert();
			return res;
		}
	}

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_HOL_BLOCK_TIMER_n,
		clnt_hdl, &ipa3_ctx->ep[clnt_hdl].holb);

	/* Enable HOLB */
	ipa3_ctx->ep[clnt_hdl].holb.en = IPA_HOLB_TMR_EN;
	ipahal_write_reg_n_fields(IPA_ENDP_INIT_HOL_BLOCK_EN_n,
		clnt_hdl, ep_holb);
	/* IPA4.5 issue requires HOLB_EN to be written twice */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0)
		ipahal_write_reg_n_fields(IPA_ENDP_INIT_HOL_BLOCK_EN_n,
			clnt_hdl, ep_holb);

success:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	IPADBG("cfg holb %u ep=%d tmr=%d\n", ep_holb->en, clnt_hdl,
		ep_holb->tmr_val);
	return 0;
}
EXPORT_SYMBOL(ipa3_cfg_ep_holb);

/**
 * ipa3_force_cfg_ep_holb() - IPA end-point holb configuration
 *			for QDSS_MHI_CONS pipe
 *
 * If an IPA producer pipe is full, IPA HW by default will block
 * indefinitely till space opens up. During this time no packets
 * including those from unrelated pipes will be processed. Enabling
 * HOLB means IPA HW will be allowed to drop packets as/when needed
 * and indefinite blocking is avoided.
 *
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_force_cfg_ep_holb(u32 clnt_hdl,
	struct ipa_ep_cfg_holb *ep_holb)
{
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ep_holb == NULL) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	if (ep_holb->en == IPA_HOLB_TMR_DIS) {
		ipahal_write_reg_n_fields(IPA_ENDP_INIT_HOL_BLOCK_EN_n,
			clnt_hdl, ep_holb);
		goto success;
	}

	/* Follow HPG sequence to DIS_HOLB, Configure Timer, and HOLB_EN */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5) {
		ep_holb->en = IPA_HOLB_TMR_DIS;
		ipahal_write_reg_n_fields(IPA_ENDP_INIT_HOL_BLOCK_EN_n,
			clnt_hdl, ep_holb);
	}

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5) {
		int res;

		res = ipa3_process_timer_cfg(ep_holb->tmr_val * 1000,
			&ep_holb->pulse_generator,
			&ep_holb->scaled_time);
		if (res) {
			IPAERR("failed to process HOLB timer tmr=%u\n",
				ep_holb->tmr_val);
			ipa_assert();
			return res;
		}
	}

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_HOL_BLOCK_TIMER_n,
		clnt_hdl, ep_holb);

	/* Enable HOLB */
	ep_holb->en = IPA_HOLB_TMR_EN;
	ipahal_write_reg_n_fields(IPA_ENDP_INIT_HOL_BLOCK_EN_n,
		clnt_hdl, ep_holb);
	/* IPA4.5 issue requires HOLB_EN to be written twice */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5)
		ipahal_write_reg_n_fields(IPA_ENDP_INIT_HOL_BLOCK_EN_n,
			clnt_hdl, ep_holb);

success:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	IPADBG("cfg holb %u ep=%d tmr=%d\n", ep_holb->en, clnt_hdl,
		ep_holb->tmr_val);
	return 0;
}

/**
 * ipa3_cfg_ep_holb_by_client() - IPA end-point holb configuration
 *
 * Wrapper function for ipa3_cfg_ep_holb() with client name instead of
 * client handle. This function is used for clients that does not have
 * client handle.
 *
 * @client:	[in] client name
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa3_cfg_ep_holb_by_client(enum ipa_client_type client,
				const struct ipa_ep_cfg_holb *ep_holb)
{
	return ipa3_cfg_ep_holb(ipa_get_ep_mapping(client), ep_holb);
}

/**
 * ipa3_cfg_ep_deaggr() -  IPA end-point deaggregation configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ep_deaggr:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_deaggr(u32 clnt_hdl,
			const struct ipa_ep_cfg_deaggr *ep_deaggr)
{
	struct ipa3_ep_context *ep;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
	    ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_deaggr == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
				clnt_hdl, ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d deaggr_hdr_len=%d\n",
		clnt_hdl,
		ep_deaggr->deaggr_hdr_len);

	IPADBG("syspipe_err_detection=%d\n",
		ep_deaggr->syspipe_err_detection);

	IPADBG("packet_offset_valid=%d\n",
		ep_deaggr->packet_offset_valid);

	IPADBG("packet_offset_location=%d max_packet_len=%d\n",
		ep_deaggr->packet_offset_location,
		ep_deaggr->max_packet_len);

	IPADBG("ignore_min_pkt_err=%d\n",
		ep_deaggr->ignore_min_pkt_err);

	ep = &ipa3_ctx->ep[clnt_hdl];

	/* copy over EP cfg */
	ep->cfg.deaggr = *ep_deaggr;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipahal_write_reg_n_fields(IPA_ENDP_INIT_DEAGGR_n, clnt_hdl,
		&ep->cfg.deaggr);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

/**
 * ipa3_cfg_ep_metadata() - IPA end-point metadata configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_cfg_ep_metadata(u32 clnt_hdl, const struct ipa_ep_cfg_metadata *ep_md)
{
	u32 qmap_id = 0;
	struct ipa_ep_cfg_metadata ep_md_reg_wrt;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0 || ep_md == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl, ipa3_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d, mux id=%d\n", clnt_hdl, ep_md->qmap_id);

	/* copy over EP cfg */
	ipa3_ctx->ep[clnt_hdl].cfg.meta = *ep_md;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	if (ipa3_ctx->eogre_enabled) {
		/* reconfigure ep metadata reg to override mux-id */
		ipa3_ctx->ep[clnt_hdl].cfg.hdr.hdr_ofst_metadata_valid = 0;
		ipa3_ctx->ep[clnt_hdl].cfg.hdr.hdr_ofst_metadata = 0;
		ipa3_ctx->ep[clnt_hdl].cfg.hdr.hdr_metadata_reg_valid = 1;
		ipahal_write_reg_n_fields(IPA_ENDP_INIT_HDR_n, clnt_hdl,
			&ipa3_ctx->ep[clnt_hdl].cfg.hdr);
	}

	ep_md_reg_wrt = *ep_md;
	qmap_id = (ep_md->qmap_id <<
		IPA_ENDP_INIT_HDR_METADATA_n_MUX_ID_SHFT) &
		IPA_ENDP_INIT_HDR_METADATA_n_MUX_ID_BMASK;

	/* mark tethering bit for remote modem */
	if (ipa3_ctx->ipa_hw_type == IPA_HW_v4_1)
		qmap_id |= IPA_QMAP_TETH_BIT;

	ep_md_reg_wrt.qmap_id = qmap_id;
	ipahal_write_reg_n_fields(IPA_ENDP_INIT_HDR_METADATA_n, clnt_hdl,
		&ep_md_reg_wrt);
	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_5) {
		ipa3_ctx->ep[clnt_hdl].cfg.hdr.hdr_metadata_reg_valid = 1;
		ipahal_write_reg_n_fields(IPA_ENDP_INIT_HDR_n, clnt_hdl,
			&ipa3_ctx->ep[clnt_hdl].cfg.hdr);
	}

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return 0;
}

int ipa3_write_qmap_id(struct ipa_ioc_write_qmapid *param_in)
{
	struct ipa_ep_cfg_metadata meta;
	struct ipa3_ep_context *ep;
	int ipa_ep_idx;
	int result = -EINVAL;

	if (param_in->client  >= IPA_CLIENT_MAX) {
		IPAERR_RL("bad parm client:%d\n", param_in->client);
		goto fail;
	}

	ipa_ep_idx = ipa_get_ep_mapping(param_in->client);
	if (ipa_ep_idx == -1) {
		IPAERR_RL("Invalid client.\n");
		goto fail;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];
	if (!ep->valid) {
		IPAERR_RL("EP not allocated.\n");
		goto fail;
	}

	meta.qmap_id = param_in->qmap_id;
	if (param_in->client == IPA_CLIENT_USB_PROD ||
		param_in->client == IPA_CLIENT_USB2_PROD ||
	    param_in->client == IPA_CLIENT_HSIC1_PROD ||
	    param_in->client == IPA_CLIENT_ODU_PROD ||
	    param_in->client == IPA_CLIENT_ETHERNET_PROD ||
	    param_in->client == IPA_CLIENT_ETHERNET2_PROD ||
		param_in->client == IPA_CLIENT_WIGIG_PROD ||
		param_in->client == IPA_CLIENT_AQC_ETHERNET_PROD ||
		param_in->client == IPA_CLIENT_RTK_ETHERNET_PROD) {
		result = ipa3_cfg_ep_metadata(ipa_ep_idx, &meta);
	} else if (param_in->client == IPA_CLIENT_WLAN1_PROD ||
			   param_in->client == IPA_CLIENT_WLAN2_PROD) {
		ipa3_ctx->ep[ipa_ep_idx].cfg.meta = meta;
		if (param_in->client == IPA_CLIENT_WLAN2_PROD)
			result = ipa3_write_qmapid_wdi3_gsi_pipe(
				ipa_ep_idx, meta.qmap_id);
		else
			result = ipa3_write_qmapid_wdi_pipe(
				ipa_ep_idx, meta.qmap_id);
		if (result)
			IPAERR_RL("qmap_id %d write failed on ep=%d\n",
					meta.qmap_id, ipa_ep_idx);
		result = 0;
	}

fail:
	return result;
}

/**
 * ipa3_dump_buff_internal() - dumps buffer for debug purposes
 * @base: buffer base address
 * @phy_base: buffer physical base address
 * @size: size of the buffer
 */
void ipa3_dump_buff_internal(void *base, dma_addr_t phy_base, u32 size)
{
	int i;
	u32 *cur = (u32 *)base;
	u8 *byt;

	IPADBG("system phys addr=%pa len=%u\n", &phy_base, size);
	for (i = 0; i < size / 4; i++) {
		byt = (u8 *)(cur + i);
		IPADBG("%2d %08x   %02x %02x %02x %02x\n", i, *(cur + i),
				byt[0], byt[1], byt[2], byt[3]);
	}
	IPADBG("END\n");
}

/**
 * ipa_set_aggr_mode() - Set the aggregation mode which is a global setting
 * @mode:	[in] the desired aggregation mode for e.g. straight MBIM, QCNCM,
 * etc
 *
 * Returns:	0 on success
 */
int ipa_set_aggr_mode(enum ipa_aggr_mode mode)
{
	struct ipahal_reg_qcncm qcncm;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		if (mode != IPA_MBIM_AGGR) {
			IPAERR("Only MBIM mode is supported staring 4.0\n");
			return -EPERM;
		}
	} else {
		IPA_ACTIVE_CLIENTS_INC_SIMPLE();
		ipahal_read_reg_fields(IPA_QCNCM, &qcncm);
		qcncm.mode_en = mode;
		ipahal_write_reg_fields(IPA_QCNCM, &qcncm);
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	}

	return 0;
}
EXPORT_SYMBOL(ipa_set_aggr_mode);

/**
 * ipa_set_qcncm_ndp_sig() - Set the NDP signature used for QCNCM aggregation
 * mode
 * @sig:	[in] the first 3 bytes of QCNCM NDP signature (expected to be
 * "QND")
 *
 * Set the NDP signature used for QCNCM aggregation mode. The fourth byte
 * (expected to be 'P') needs to be set using the header addition mechanism
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_set_qcncm_ndp_sig(char sig[3])
{
	struct ipahal_reg_qcncm qcncm;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		IPAERR("QCNCM mode is not supported staring 4.0\n");
		return -EPERM;
	}

	if (sig == NULL) {
		IPAERR("bad argument\n");
		return -EINVAL;
	}
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	ipahal_read_reg_fields(IPA_QCNCM, &qcncm);
	qcncm.mode_val = ((sig[0] << 16) | (sig[1] << 8) | sig[2]);
	ipahal_write_reg_fields(IPA_QCNCM, &qcncm);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return 0;
}
EXPORT_SYMBOL(ipa_set_qcncm_ndp_sig);

/**
 * ipa_set_single_ndp_per_mbim() - Enable/disable single NDP per MBIM frame
 * configuration
 * @enable:	[in] true for single NDP/MBIM; false otherwise
 *
 * Returns:	0 on success
 */
int ipa_set_single_ndp_per_mbim(bool enable)
{
	struct ipahal_reg_single_ndp_mode mode;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		IPAERR("QCNCM mode is not supported staring 4.0\n");
		return -EPERM;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	ipahal_read_reg_fields(IPA_SINGLE_NDP_MODE, &mode);
	mode.single_ndp_en = enable;
	ipahal_write_reg_fields(IPA_SINGLE_NDP_MODE, &mode);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return 0;
}
EXPORT_SYMBOL(ipa_set_single_ndp_per_mbim);

/**
 * ipa3_straddle_boundary() - Checks whether a memory buffer straddles a
 * boundary
 * @start: start address of the memory buffer
 * @end: end address of the memory buffer
 * @boundary: boundary
 *
 * Return value:
 * 1: if the interval [start, end] straddles boundary
 * 0: otherwise
 */
int ipa3_straddle_boundary(u32 start, u32 end, u32 boundary)
{
	u32 next_start;
	u32 prev_end;

	IPADBG("start=%u end=%u boundary=%u\n", start, end, boundary);

	next_start = (start + (boundary - 1)) & ~(boundary - 1);
	prev_end = ((end + (boundary - 1)) & ~(boundary - 1)) - boundary;

	while (next_start < prev_end)
		next_start += boundary;

	if (next_start == prev_end)
		return 1;
	else
		return 0;
}

/**
 * ipa3_init_mem_partition() - Assigns the static memory partition
 * based on the IPA version
 *
 * Returns:	0 on success
 */
int ipa3_init_mem_partition(enum ipa_hw_type type)
{
	switch (type) {
	case IPA_HW_v4_1:
		ipa3_ctx->ctrl->mem_partition = &ipa_4_1_mem_part;
		break;
	case IPA_HW_v4_2:
		ipa3_ctx->ctrl->mem_partition = &ipa_4_2_mem_part;
		break;
	case IPA_HW_v4_5:
		ipa3_ctx->ctrl->mem_partition = &ipa_4_5_mem_part;
		break;
	case IPA_HW_v4_7:
		ipa3_ctx->ctrl->mem_partition = &ipa_4_7_mem_part;
		break;
	case IPA_HW_v4_9:
		ipa3_ctx->ctrl->mem_partition = &ipa_4_9_mem_part;
		break;
	case IPA_HW_v4_11:
		ipa3_ctx->ctrl->mem_partition = &ipa_4_11_mem_part;
		break;
	case IPA_HW_v5_0:
		ipa3_ctx->ctrl->mem_partition = &ipa_5_0_mem_part;
		break;
	case IPA_HW_v5_1:
		ipa3_ctx->ctrl->mem_partition = &ipa_5_1_mem_part;
		break;
	case IPA_HW_v5_2:
		ipa3_ctx->ctrl->mem_partition = &ipa_5_2_mem_part;
		break;
	case IPA_HW_v5_5:
		ipa3_ctx->ctrl->mem_partition = &ipa_5_5_mem_part;
		break;
	case IPA_HW_None:
	case IPA_HW_v1_0:
	case IPA_HW_v1_1:
	case IPA_HW_v2_0:
	case IPA_HW_v2_1:
	case IPA_HW_v2_5:
	case IPA_HW_v2_6L:
	case IPA_HW_v3_0:
		ipa3_ctx->ctrl->mem_partition = &ipa_3_0_mem_part;
		break;
	case IPA_HW_v3_1:
	case IPA_HW_v3_5:
	case IPA_HW_v3_5_1:
	case IPA_HW_v4_0:
	default:
		IPAERR("unsupported version %d\n", type);
		return -EPERM;
	}

	IPADBG("UC OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(uc_ofst), IPA_MEM_PART(uc_size));

	if (IPA_MEM_PART(uc_info_ofst) & 3) {
		IPAERR("UC INFO OFST 0x%x is unaligned\n",
			IPA_MEM_PART(uc_info_ofst));
		return -ENODEV;
	}

	IPADBG("UC INFO OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(uc_info_ofst), IPA_MEM_PART(uc_info_size));

	IPADBG("RAM OFST 0x%x\n", IPA_MEM_PART(ofst_start));

	if (IPA_MEM_PART(v4_flt_hash_ofst) & 7) {
		IPAERR("V4 FLT HASHABLE OFST 0x%x is unaligned\n",
			IPA_MEM_PART(v4_flt_hash_ofst));
		return -ENODEV;
	}

	IPADBG("V4 FLT HASHABLE OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v4_flt_hash_ofst),
		IPA_MEM_PART(v4_flt_hash_size),
		IPA_MEM_PART(v4_flt_hash_size_ddr));

	if (IPA_MEM_PART(v4_flt_nhash_ofst) & 7) {
		IPAERR("V4 FLT NON-HASHABLE OFST 0x%x is unaligned\n",
			IPA_MEM_PART(v4_flt_nhash_ofst));
		return -ENODEV;
	}

	IPADBG("V4 FLT NON-HASHABLE OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v4_flt_nhash_ofst),
		IPA_MEM_PART(v4_flt_nhash_size),
		IPA_MEM_PART(v4_flt_nhash_size_ddr));

	if (IPA_MEM_PART(v6_flt_hash_ofst) & 7) {
		IPAERR("V6 FLT HASHABLE OFST 0x%x is unaligned\n",
			IPA_MEM_PART(v6_flt_hash_ofst));
		return -ENODEV;
	}

	IPADBG("V6 FLT HASHABLE OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v6_flt_hash_ofst), IPA_MEM_PART(v6_flt_hash_size),
		IPA_MEM_PART(v6_flt_hash_size_ddr));

	if (IPA_MEM_PART(v6_flt_nhash_ofst) & 7) {
		IPAERR("V6 FLT NON-HASHABLE OFST 0x%x is unaligned\n",
			IPA_MEM_PART(v6_flt_nhash_ofst));
		return -ENODEV;
	}

	IPADBG("V6 FLT NON-HASHABLE OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v6_flt_nhash_ofst),
		IPA_MEM_PART(v6_flt_nhash_size),
		IPA_MEM_PART(v6_flt_nhash_size_ddr));

	IPADBG("V4 RT NUM INDEX 0x%x\n", IPA_MEM_PART(v4_rt_num_index));

	IPADBG("V4 RT MODEM INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v4_modem_rt_index_lo),
		IPA_MEM_PART(v4_modem_rt_index_hi));

	IPADBG("V4 RT APPS INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v4_apps_rt_index_lo),
		IPA_MEM_PART(v4_apps_rt_index_hi));

	if (IPA_MEM_PART(v4_rt_hash_ofst) & 7) {
		IPAERR("V4 RT HASHABLE OFST 0x%x is unaligned\n",
			IPA_MEM_PART(v4_rt_hash_ofst));
		return -ENODEV;
	}

	IPADBG("V4 RT HASHABLE OFST 0x%x\n", IPA_MEM_PART(v4_rt_hash_ofst));

	IPADBG("V4 RT HASHABLE SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v4_rt_hash_size),
		IPA_MEM_PART(v4_rt_hash_size_ddr));

	if (IPA_MEM_PART(v4_rt_nhash_ofst) & 7) {
		IPAERR("V4 RT NON-HASHABLE OFST 0x%x is unaligned\n",
			IPA_MEM_PART(v4_rt_nhash_ofst));
		return -ENODEV;
	}

	IPADBG("V4 RT NON-HASHABLE OFST 0x%x\n",
		IPA_MEM_PART(v4_rt_nhash_ofst));

	IPADBG("V4 RT HASHABLE SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v4_rt_nhash_size),
		IPA_MEM_PART(v4_rt_nhash_size_ddr));

	IPADBG("V6 RT NUM INDEX 0x%x\n", IPA_MEM_PART(v6_rt_num_index));

	IPADBG("V6 RT MODEM INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v6_modem_rt_index_lo),
		IPA_MEM_PART(v6_modem_rt_index_hi));

	IPADBG("V6 RT APPS INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v6_apps_rt_index_lo),
		IPA_MEM_PART(v6_apps_rt_index_hi));

	if (IPA_MEM_PART(v6_rt_hash_ofst) & 7) {
		IPAERR("V6 RT HASHABLE OFST 0x%x is unaligned\n",
			IPA_MEM_PART(v6_rt_hash_ofst));
		return -ENODEV;
	}

	IPADBG("V6 RT HASHABLE OFST 0x%x\n", IPA_MEM_PART(v6_rt_hash_ofst));

	IPADBG("V6 RT HASHABLE SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v6_rt_hash_size),
		IPA_MEM_PART(v6_rt_hash_size_ddr));

	if (IPA_MEM_PART(v6_rt_nhash_ofst) & 7) {
		IPAERR("V6 RT NON-HASHABLE OFST 0x%x is unaligned\n",
			IPA_MEM_PART(v6_rt_nhash_ofst));
		return -ENODEV;
	}

	IPADBG("V6 RT NON-HASHABLE OFST 0x%x\n",
		IPA_MEM_PART(v6_rt_nhash_ofst));

	IPADBG("V6 RT NON-HASHABLE SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v6_rt_nhash_size),
		IPA_MEM_PART(v6_rt_nhash_size_ddr));

	if (IPA_MEM_PART(modem_hdr_ofst) & 7) {
		IPAERR("MODEM HDR OFST 0x%x is unaligned\n",
			IPA_MEM_PART(modem_hdr_ofst));
		return -ENODEV;
	}

	IPADBG("MODEM HDR OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(modem_hdr_ofst), IPA_MEM_PART(modem_hdr_size));

	if (IPA_MEM_PART(apps_hdr_ofst) & 7) {
		IPAERR("APPS HDR OFST 0x%x is unaligned\n",
			IPA_MEM_PART(apps_hdr_ofst));
		return -ENODEV;
	}

	IPADBG("APPS HDR OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(apps_hdr_ofst), IPA_MEM_PART(apps_hdr_size),
		IPA_MEM_PART(apps_hdr_size_ddr));

	if (IPA_MEM_PART(modem_hdr_proc_ctx_ofst) & 7) {
		IPAERR("MODEM HDR PROC CTX OFST 0x%x is unaligned\n",
			IPA_MEM_PART(modem_hdr_proc_ctx_ofst));
		return -ENODEV;
	}

	IPADBG("MODEM HDR PROC CTX OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst),
		IPA_MEM_PART(modem_hdr_proc_ctx_size));

	if (IPA_MEM_PART(apps_hdr_proc_ctx_ofst) & 7) {
		IPAERR("APPS HDR PROC CTX OFST 0x%x is unaligned\n",
			IPA_MEM_PART(apps_hdr_proc_ctx_ofst));
		return -ENODEV;
	}

	IPADBG("APPS HDR PROC CTX OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(apps_hdr_proc_ctx_ofst),
		IPA_MEM_PART(apps_hdr_proc_ctx_size),
		IPA_MEM_PART(apps_hdr_proc_ctx_size_ddr));

	if (IPA_MEM_PART(pdn_config_ofst) & 7) {
		IPAERR("PDN CONFIG OFST 0x%x is unaligned\n",
			IPA_MEM_PART(pdn_config_ofst));
		return -ENODEV;
	}

	/*
	 * Routing rules points to hdr_proc_ctx in 32byte offsets from base.
	 * Base is modem hdr_proc_ctx first address.
	 * AP driver install APPS hdr_proc_ctx starting at the beginning of
	 * apps hdr_proc_ctx part.
	 * So first apps hdr_proc_ctx offset at some routing
	 * rule will be modem_hdr_proc_ctx_size >> 5 (32B).
	 */
	if (IPA_MEM_PART(modem_hdr_proc_ctx_size) & 31) {
		IPAERR("MODEM HDR PROC CTX SIZE 0x%x is not 32B aligned\n",
			IPA_MEM_PART(modem_hdr_proc_ctx_size));
		return -ENODEV;
	}

	/*
	 * AP driver when installing routing rule, it calcs the hdr_proc_ctx
	 * offset by local offset (from base of apps part) +
	 * modem_hdr_proc_ctx_size. This is to get offset from modem part base.
	 * Thus apps part must be adjacent to modem part
	 */
	if (IPA_MEM_PART(apps_hdr_proc_ctx_ofst) !=
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst) +
		IPA_MEM_PART(modem_hdr_proc_ctx_size)) {
		IPAERR("APPS HDR PROC CTX SIZE not adjacent to MODEM one!\n");
		return -ENODEV;
	}

	IPADBG("NAT TBL OFST 0x%x SIZE 0x%x\n",
		   IPA_MEM_PART(nat_tbl_ofst),
		   IPA_MEM_PART(nat_tbl_size));

	if (IPA_MEM_PART(nat_tbl_ofst) & 31) {
		IPAERR("NAT TBL OFST 0x%x is not aligned properly\n",
			   IPA_MEM_PART(nat_tbl_ofst));
		return -ENODEV;
	}

	IPADBG("PDN CONFIG OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(pdn_config_ofst),
		IPA_MEM_PART(pdn_config_size));

	if (IPA_MEM_PART(pdn_config_ofst) & 7) {
		IPAERR("PDN CONFIG OFST 0x%x is unaligned\n",
			IPA_MEM_PART(pdn_config_ofst));
		return -ENODEV;
	}

	IPADBG("Q6 QUOTA STATS OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(stats_quota_q6_ofst),
		IPA_MEM_PART(stats_quota_q6_size));

	if (IPA_MEM_PART(stats_quota_q6_ofst) & 7) {
		IPAERR("Q6 QUOTA STATS OFST 0x%x is unaligned\n",
			IPA_MEM_PART(stats_quota_q6_ofst));
		return -ENODEV;
	}

	IPADBG("AP QUOTA STATS OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(stats_quota_ap_ofst),
		IPA_MEM_PART(stats_quota_ap_size));

	if (IPA_MEM_PART(stats_quota_ap_ofst) & 7) {
		IPAERR("AP QUOTA STATS OFST 0x%x is unaligned\n",
			IPA_MEM_PART(stats_quota_ap_ofst));
		return -ENODEV;
	}

	IPADBG("TETHERING STATS OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(stats_tethering_ofst),
		IPA_MEM_PART(stats_tethering_size));

	if (IPA_MEM_PART(stats_tethering_ofst) & 7) {
		IPAERR("TETHERING STATS OFST 0x%x is unaligned\n",
			IPA_MEM_PART(stats_tethering_ofst));
		return -ENODEV;
	}

	IPADBG("FILTER AND ROUTING STATS OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(stats_fnr_ofst),
		IPA_MEM_PART(stats_fnr_size));

	if (IPA_MEM_PART(stats_fnr_ofst) & 7) {
		IPAERR("FILTER AND ROUTING STATS OFST 0x%x is unaligned\n",
			IPA_MEM_PART(stats_fnr_ofst));
		return -ENODEV;
	}

	IPADBG("DROP STATS OFST 0x%x SIZE 0x%x\n",
	IPA_MEM_PART(stats_drop_ofst),
		IPA_MEM_PART(stats_drop_size));

	if (IPA_MEM_PART(stats_drop_ofst) & 7) {
		IPAERR("DROP STATS OFST 0x%x is unaligned\n",
			IPA_MEM_PART(stats_drop_ofst));
		return -ENODEV;
	}

	IPADBG("V4 APPS HASHABLE FLT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v4_flt_hash_ofst),
		IPA_MEM_PART(apps_v4_flt_hash_size));

	IPADBG("V4 APPS NON-HASHABLE FLT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v4_flt_nhash_ofst),
		IPA_MEM_PART(apps_v4_flt_nhash_size));

	IPADBG("V6 APPS HASHABLE FLT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v6_flt_hash_ofst),
		IPA_MEM_PART(apps_v6_flt_hash_size));

	IPADBG("V6 APPS NON-HASHABLE FLT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v6_flt_nhash_ofst),
		IPA_MEM_PART(apps_v6_flt_nhash_size));

	IPADBG("RAM END OFST 0x%x\n",
		IPA_MEM_PART(end_ofst));

	IPADBG("V4 APPS HASHABLE RT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v4_rt_hash_ofst),
		IPA_MEM_PART(apps_v4_rt_hash_size));

	IPADBG("V4 APPS NON-HASHABLE RT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v4_rt_nhash_ofst),
		IPA_MEM_PART(apps_v4_rt_nhash_size));

	IPADBG("V6 APPS HASHABLE RT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v6_rt_hash_ofst),
		IPA_MEM_PART(apps_v6_rt_hash_size));

	IPADBG("V6 APPS NON-HASHABLE RT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v6_rt_nhash_ofst),
		IPA_MEM_PART(apps_v6_rt_nhash_size));

	if (IPA_MEM_PART(modem_ofst) & 7) {
		IPAERR("MODEM OFST 0x%x is unaligned\n",
			IPA_MEM_PART(modem_ofst));
		return -ENODEV;
	}

	IPADBG("MODEM OFST 0x%x SIZE 0x%x\n", IPA_MEM_PART(modem_ofst),
		IPA_MEM_PART(modem_size));

	if (IPA_MEM_PART(uc_descriptor_ram_ofst) & 1023) {
		IPAERR("UC DESCRIPTOR RAM OFST 0x%x is unaligned\n",
			IPA_MEM_PART(uc_descriptor_ram_ofst));
		return -ENODEV;
	}

	IPADBG("UC DESCRIPTOR RAM OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(uc_descriptor_ram_ofst),
		IPA_MEM_PART(uc_descriptor_ram_size));

	return 0;
}

/**
 * ipa_ctrl_static_bind() - set the appropriate methods for
 *  IPA Driver based on the HW version
 *
 *  @ctrl: data structure which holds the function pointers
 *  @hw_type: the HW type in use
 *
 *  This function can avoid the runtime assignment by using C99 special
 *  struct initialization - hard decision... time.vs.mem
 */
int ipa3_controller_static_bind(struct ipa3_controller *ctrl,
		enum ipa_hw_type hw_type, u32 ipa_cfg_offset)
{
	if (hw_type >= IPA_HW_v5_0) {
		ctrl->ipa_clk_rate_turbo = IPA_V5_0_CLK_RATE_TURBO;
		ctrl->ipa_clk_rate_nominal = IPA_V5_0_CLK_RATE_NOMINAL;
		ctrl->ipa_clk_rate_svs = IPA_V5_0_CLK_RATE_SVS;
		ctrl->ipa_clk_rate_svs2 = IPA_V5_0_CLK_RATE_SVS2;
	} else if (hw_type >= IPA_HW_v4_0) {
		ctrl->ipa_clk_rate_turbo = IPA_V4_0_CLK_RATE_TURBO;
		ctrl->ipa_clk_rate_nominal = IPA_V4_0_CLK_RATE_NOMINAL;
		ctrl->ipa_clk_rate_svs = IPA_V4_0_CLK_RATE_SVS;
		ctrl->ipa_clk_rate_svs2 = IPA_V4_0_CLK_RATE_SVS2;
	} else if (hw_type >= IPA_HW_v3_5) {
		ctrl->ipa_clk_rate_turbo = IPA_V3_5_CLK_RATE_TURBO;
		ctrl->ipa_clk_rate_nominal = IPA_V3_5_CLK_RATE_NOMINAL;
		ctrl->ipa_clk_rate_svs = IPA_V3_5_CLK_RATE_SVS;
		ctrl->ipa_clk_rate_svs2 = IPA_V3_5_CLK_RATE_SVS2;
	} else {
		ctrl->ipa_clk_rate_turbo = IPA_V3_0_CLK_RATE_TURBO;
		ctrl->ipa_clk_rate_nominal = IPA_V3_0_CLK_RATE_NOMINAL;
		ctrl->ipa_clk_rate_svs = IPA_V3_0_CLK_RATE_SVS;
		ctrl->ipa_clk_rate_svs2 = IPA_V3_0_CLK_RATE_SVS2;
	}

	ctrl->ipa_init_rt4 = _ipa_init_rt4_v3;
	ctrl->ipa_init_rt6 = _ipa_init_rt6_v3;
	ctrl->ipa_init_flt4 = _ipa_init_flt4_v3;
	ctrl->ipa_init_flt6 = _ipa_init_flt6_v3;
	ctrl->ipa3_read_ep_reg = _ipa_read_ep_reg_v3_0;
	ctrl->ipa3_commit_flt = __ipa_commit_flt_v3;
	ctrl->ipa3_commit_rt = __ipa_commit_rt_v3;
	ctrl->ipa3_commit_hdr = __ipa_commit_hdr_v3_0;
	ctrl->ipa3_enable_clks = _ipa_enable_clks_v3_0;
	ctrl->ipa3_disable_clks = _ipa_disable_clks_v3_0;
	ctrl->clock_scaling_bw_threshold_svs =
		IPA_V3_0_BW_THRESHOLD_SVS_MBPS;
	ctrl->clock_scaling_bw_threshold_nominal =
		IPA_V3_0_BW_THRESHOLD_NOMINAL_MBPS;
	ctrl->clock_scaling_bw_threshold_turbo =
		IPA_V3_0_BW_THRESHOLD_TURBO_MBPS;
	ctrl->ipa_reg_base_ofst = ipa_cfg_offset == 0 ?
						ipahal_get_reg_base() : ipa_cfg_offset;
	ctrl->ipa_init_sram = _ipa_init_sram_v3;
	ctrl->ipa_sram_read_settings = _ipa_sram_settings_read_v3_0;
	ctrl->ipa_init_hdr = _ipa_init_hdr_v3_0;
	ctrl->max_holb_tmr_val = IPA_MAX_HOLB_TMR_VAL;

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0)
		ctrl->ipa3_read_ep_reg = _ipa_read_ep_reg_v4_0;

	return 0;
}

void ipa3_skb_recycle(struct sk_buff *skb)
{
	struct skb_shared_info *shinfo;

	shinfo = skb_shinfo(skb);
	memset(shinfo, 0, offsetof(struct skb_shared_info, dataref));
	atomic_set(&shinfo->dataref, 1);

	memset(skb, 0, offsetof(struct sk_buff, tail));
	skb->data = skb->head + NET_SKB_PAD;
	skb_reset_tail_pointer(skb);
}

int ipa3_alloc_rule_id(struct idr *rule_ids)
{
	/* There is two groups of rule-Ids, Modem ones and Apps ones.
	 * Distinction by high bit: Modem Ids are high bit asserted.
	 */
	return idr_alloc(rule_ids, NULL,
		ipahal_get_low_rule_id(),
		ipahal_get_rule_id_hi_bit(),
		GFP_KERNEL);
}

static int __ipa3_alloc_counter_hdl
	(struct ipa_ioc_flt_rt_counter_alloc *counter)
{
	int id;

	/* assign a handle using idr to this counter block */
	id = idr_alloc(&ipa3_ctx->flt_rt_counters.hdl, counter,
		ipahal_get_low_hdl_id(), ipahal_get_high_hdl_id(),
		GFP_ATOMIC);

	return id;
}

int ipa3_alloc_counter_id(struct ipa_ioc_flt_rt_counter_alloc *header)
{
	int i, unused_cnt, unused_max, unused_start_id;
	struct ipa_ioc_flt_rt_counter_alloc *counter;

	counter = kmem_cache_zalloc(ipa3_ctx->fnr_stats_cache, GFP_KERNEL);
	if (!counter) {
		IPAERR_RL("failed to alloc fnr stats counter object\n");
		spin_unlock(&ipa3_ctx->flt_rt_counters.hdl_lock);
		return -ENOMEM;
	}

	idr_preload(GFP_KERNEL);
	spin_lock(&ipa3_ctx->flt_rt_counters.hdl_lock);
	memcpy(counter, header, sizeof(struct ipa_ioc_flt_rt_counter_alloc));

	/* allocate hw counters */
	counter->hw_counter.start_id = 0;
	counter->hw_counter.end_id = 0;
	unused_cnt = 0;
	unused_max = 0;
	unused_start_id = 0;
	if (counter->hw_counter.num_counters == 0)
		goto sw_counter_alloc;
	/* find the start id which can be used for the block */
	for (i = 0; i < IPA_FLT_RT_HW_COUNTER; i++) {
		if (!ipa3_ctx->flt_rt_counters.used_hw[i])
			unused_cnt++;
		else {
			/* tracking max unused block in case allow less */
			if (unused_cnt > unused_max) {
				unused_start_id = i - unused_cnt + 2;
				unused_max = unused_cnt;
			}
			unused_cnt = 0;
		}
		/* find it, break and use this 1st possible block */
		if (unused_cnt == counter->hw_counter.num_counters) {
			counter->hw_counter.start_id = i - unused_cnt + 2;
			counter->hw_counter.end_id = i + 1;
			break;
		}
	}
	if (counter->hw_counter.start_id == 0) {
		/* if not able to find such a block but allow less */
		if (counter->hw_counter.allow_less && unused_max) {
			/* give the max possible unused blocks */
			counter->hw_counter.num_counters = unused_max;
			counter->hw_counter.start_id = unused_start_id;
			counter->hw_counter.end_id =
				unused_start_id + unused_max - 1;
		} else {
			/* not able to find such a block */
			counter->hw_counter.num_counters = 0;
			counter->hw_counter.start_id = 0;
			counter->hw_counter.end_id = 0;
			goto err;
		}
	}

sw_counter_alloc:
	/* allocate sw counters */
	counter->sw_counter.start_id = 0;
	counter->sw_counter.end_id = 0;
	unused_cnt = 0;
	unused_max = 0;
	unused_start_id = 0;
	if (counter->sw_counter.num_counters == 0)
		goto mark_hw_cnt;
	/* find the start id which can be used for the block */
	for (i = 0; i < IPA_FLT_RT_SW_COUNTER; i++) {
		if (!ipa3_ctx->flt_rt_counters.used_sw[i])
			unused_cnt++;
		else {
			/* tracking max unused block in case allow less */
			if (unused_cnt > unused_max) {
				unused_start_id = i - unused_cnt +
					2 + IPA_FLT_RT_HW_COUNTER;
				unused_max = unused_cnt;
			}
			unused_cnt = 0;
		}
		/* find it, break and use this 1st possible block */
		if (unused_cnt == counter->sw_counter.num_counters) {
			counter->sw_counter.start_id = i - unused_cnt +
				2 + IPA_FLT_RT_HW_COUNTER;
			counter->sw_counter.end_id =
				i + 1 + IPA_FLT_RT_HW_COUNTER;
			break;
		}
	}
	if (counter->sw_counter.start_id == 0) {
		/* if not able to find such a block but allow less */
		if (counter->sw_counter.allow_less && unused_max) {
			/* give the max possible unused blocks */
			counter->sw_counter.num_counters = unused_max;
			counter->sw_counter.start_id = unused_start_id;
			counter->sw_counter.end_id =
				unused_start_id + unused_max - 1;
		} else {
			/* not able to find such a block */
			counter->sw_counter.num_counters = 0;
			counter->sw_counter.start_id = 0;
			counter->sw_counter.end_id = 0;
			goto err;
		}
	}

mark_hw_cnt:
	/* add hw counters, set used to 1 */
	if (counter->hw_counter.num_counters == 0)
		goto mark_sw_cnt;
	unused_start_id = counter->hw_counter.start_id;
	if (unused_start_id < 1 ||
		unused_start_id > IPA_FLT_RT_HW_COUNTER) {
		IPAERR_RL("unexpected hw_counter start id %d\n",
			   unused_start_id);
		goto err;
	}
	for (i = 0; i < counter->hw_counter.num_counters; i++)
		ipa3_ctx->flt_rt_counters.used_hw[unused_start_id + i - 1]
			= true;
mark_sw_cnt:
	/* add sw counters, set used to 1 */
	if (counter->sw_counter.num_counters == 0)
		goto done;
	unused_start_id = counter->sw_counter.start_id
		- IPA_FLT_RT_HW_COUNTER;
	if (unused_start_id < 1 ||
		unused_start_id > IPA_FLT_RT_SW_COUNTER) {
		IPAERR_RL("unexpected sw_counter start id %d\n",
			   unused_start_id);
		goto err;
	}
	for (i = 0; i < counter->sw_counter.num_counters; i++)
		ipa3_ctx->flt_rt_counters.used_sw[unused_start_id + i - 1]
			= true;
done:
	/* get a handle from idr for dealloc */
	counter->hdl = __ipa3_alloc_counter_hdl(counter);
	memcpy(header, counter, sizeof(struct ipa_ioc_flt_rt_counter_alloc));
	spin_unlock(&ipa3_ctx->flt_rt_counters.hdl_lock);
	idr_preload_end();
	return 0;

err:
	counter->hdl = -1;
	kmem_cache_free(ipa3_ctx->fnr_stats_cache, counter);
	spin_unlock(&ipa3_ctx->flt_rt_counters.hdl_lock);
	idr_preload_end();
	return -ENOMEM;
}

void ipa3_counter_remove_hdl(int hdl)
{
	struct ipa_ioc_flt_rt_counter_alloc *counter;
	int offset = 0;

	spin_lock(&ipa3_ctx->flt_rt_counters.hdl_lock);
	counter = idr_find(&ipa3_ctx->flt_rt_counters.hdl, hdl);
	if (counter == NULL) {
		IPAERR_RL("unexpected hdl %d\n", hdl);
		goto err;
	}
	/* remove counters belong to this hdl, set used back to 0 */
	offset = counter->hw_counter.start_id - 1;
	if (offset >= 0 && (offset + counter->hw_counter.num_counters)
		< IPA_FLT_RT_HW_COUNTER) {
		memset(&ipa3_ctx->flt_rt_counters.used_hw[offset],
			   0, counter->hw_counter.num_counters * sizeof(bool));
	} else {
		IPAERR_RL("unexpected hdl %d\n", hdl);
		goto err;
	}
	offset = counter->sw_counter.start_id - 1 - IPA_FLT_RT_HW_COUNTER;
	if (offset >= 0 && (offset + counter->sw_counter.num_counters)
		< IPA_FLT_RT_SW_COUNTER) {
		memset(&ipa3_ctx->flt_rt_counters.used_sw[offset],
		   0, counter->sw_counter.num_counters * sizeof(bool));
	} else {
		IPAERR_RL("unexpected hdl %d\n", hdl);
		goto err;
	}
	/* remove the handle */
	idr_remove(&ipa3_ctx->flt_rt_counters.hdl, hdl);
	kmem_cache_free(ipa3_ctx->fnr_stats_cache, counter);
err:
	spin_unlock(&ipa3_ctx->flt_rt_counters.hdl_lock);
}

void ipa3_counter_id_remove_all(void)
{
	struct ipa_ioc_flt_rt_counter_alloc *counter;
	int hdl;

	spin_lock(&ipa3_ctx->flt_rt_counters.hdl_lock);
	/* remove all counters, set used back to 0 */
	memset(&ipa3_ctx->flt_rt_counters.used_hw, 0,
		   sizeof(ipa3_ctx->flt_rt_counters.used_hw));
	memset(&ipa3_ctx->flt_rt_counters.used_sw, 0,
		   sizeof(ipa3_ctx->flt_rt_counters.used_sw));
	/* remove all handles */
	idr_for_each_entry(&ipa3_ctx->flt_rt_counters.hdl, counter, hdl) {
		idr_remove(&ipa3_ctx->flt_rt_counters.hdl, hdl);
		kmem_cache_free(ipa3_ctx->fnr_stats_cache, counter);
	}
	spin_unlock(&ipa3_ctx->flt_rt_counters.hdl_lock);
}

int ipa3_id_alloc(void *ptr)
{
	int id;

	idr_preload(GFP_KERNEL);
	spin_lock(&ipa3_ctx->idr_lock);
	id = idr_alloc(&ipa3_ctx->ipa_idr, ptr, 0, 0, GFP_NOWAIT);
	spin_unlock(&ipa3_ctx->idr_lock);
	idr_preload_end();

	return id;
}

void *ipa3_id_find(u32 id)
{
	void *ptr;

	spin_lock(&ipa3_ctx->idr_lock);
	ptr = idr_find(&ipa3_ctx->ipa_idr, id);
	spin_unlock(&ipa3_ctx->idr_lock);

	return ptr;
}

bool ipa3_check_idr_if_freed(void *ptr)
{
	int id;
	void *iter_ptr;

	spin_lock(&ipa3_ctx->idr_lock);
	idr_for_each_entry(&ipa3_ctx->ipa_idr, iter_ptr, id) {
		if ((uintptr_t)ptr == (uintptr_t)iter_ptr) {
			spin_unlock(&ipa3_ctx->idr_lock);
			return false;
		}
	}
	spin_unlock(&ipa3_ctx->idr_lock);
	return true;
}

void ipa3_id_remove(u32 id)
{
	spin_lock(&ipa3_ctx->idr_lock);
	idr_remove(&ipa3_ctx->ipa_idr, id);
	spin_unlock(&ipa3_ctx->idr_lock);
}

void ipa3_tag_destroy_imm(void *user1, int user2)
{
	ipahal_destroy_imm_cmd(user1);
}

static void ipa3_tag_free_skb(void *user1, int user2)
{
	dev_kfree_skb_any((struct sk_buff *)user1);
}

#define REQUIRED_TAG_PROCESS_DESCRIPTORS 4
#define MAX_RETRY_ALLOC 10
#define ALLOC_MIN_SLEEP_RX 100000
#define ALLOC_MAX_SLEEP_RX 200000

/* ipa3_tag_process() - Initiates a tag process. Incorporates the input
 * descriptors
 *
 * @desc:	descriptors with commands for IC
 * @desc_size:	amount of descriptors in the above variable
 *
 * Note: The descriptors are copied (if there's room), the client needs to
 * free his descriptors afterwards
 *
 * Return: 0 or negative in case of failure
 */
int ipa3_tag_process(struct ipa3_desc desc[],
	int descs_num,
	unsigned long timeout)
{
	struct ipa3_sys_context *sys;
	struct ipa3_desc *tag_desc;
	int desc_idx = 0;
	struct ipahal_imm_cmd_ip_packet_init pktinit_cmd;
	struct ipahal_imm_cmd_pyld *cmd_pyld = NULL;
	struct ipahal_imm_cmd_ip_packet_tag_status status;
	int i;
	struct sk_buff *dummy_skb;
	int res = 0;
	struct ipa3_tag_completion *comp;
	int ep_idx;
	u32 retry_cnt = 0;
	struct ipahal_reg_valmask valmask;
	struct ipahal_imm_cmd_register_write reg_write_coal_close;
	struct ipahal_imm_cmd_register_read dummy_reg_read;
	int req_num_tag_desc = REQUIRED_TAG_PROCESS_DESCRIPTORS;
	struct ipa_mem_buffer cmd;
	u32 offset = 0;

	memset(&cmd, 0, sizeof(struct ipa_mem_buffer));
	/**
	 * We use a descriptor for closing coalsceing endpoint
	 * by immediate command. So, REQUIRED_TAG_PROCESS_DESCRIPTORS
	 * should be incremented by 1 to overcome buffer overflow.
	 */
	if (ipa_get_ep_mapping(IPA_CLIENT_APPS_WAN_COAL_CONS) != -1)
		req_num_tag_desc += 1;

	/* Not enough room for the required descriptors for the tag process */
	if (IPA_TAG_MAX_DESC - descs_num < req_num_tag_desc) {
		IPAERR("up to %d descriptors are allowed (received %d)\n",
		       IPA_TAG_MAX_DESC - req_num_tag_desc,
		       descs_num);
		return -ENOMEM;
	}

	ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_CMD_PROD);
	if (-1 == ep_idx) {
		IPAERR("Client %u is not mapped\n",
			IPA_CLIENT_APPS_CMD_PROD);
		return -EFAULT;
	}
	sys = ipa3_ctx->ep[ep_idx].sys;

	tag_desc = kzalloc(sizeof(*tag_desc) * IPA_TAG_MAX_DESC, GFP_KERNEL);
	if (!tag_desc) {
		IPAERR("failed to allocate memory\n");
		return -ENOMEM;
	}

	/* Copy the required descriptors from the client now */
	if (desc) {
		memcpy(&(tag_desc[0]), desc, descs_num *
			sizeof(tag_desc[0]));
		desc_idx += descs_num;
	} else {
		res = -EFAULT;
		IPAERR("desc is NULL\n");
		goto fail_free_tag_desc;
	}

	/* IC to close the coal frame before HPS Clear if coal is enabled */
	if (ipa_get_ep_mapping(IPA_CLIENT_APPS_WAN_COAL_CONS) != -1) {
		ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_WAN_COAL_CONS);
		reg_write_coal_close.skip_pipeline_clear = false;
		if (ipa3_ctx->ulso_wa) {
			reg_write_coal_close.pipeline_clear_options = IPAHAL_SRC_GRP_CLEAR;
		} else {
			reg_write_coal_close.pipeline_clear_options = IPAHAL_HPS_CLEAR;
		}
		if (ipa3_ctx->ipa_hw_type < IPA_HW_v5_0)
			offset = ipahal_get_reg_ofst(
				IPA_AGGR_FORCE_CLOSE);
		else
			offset = ipahal_get_ep_reg_offset(
				IPA_AGGR_FORCE_CLOSE_n, ep_idx);
		reg_write_coal_close.offset = offset;
		ipahal_get_aggr_force_close_valmask(ep_idx, &valmask);
		reg_write_coal_close.value = valmask.val;
		reg_write_coal_close.value_mask = valmask.mask;
		cmd_pyld = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_REGISTER_WRITE,
			&reg_write_coal_close, false);
		if (!cmd_pyld) {
			IPAERR("failed to construct coal close IC\n");
			res = -ENOMEM;
			goto fail_free_tag_desc;
		}
		ipa3_init_imm_cmd_desc(&tag_desc[desc_idx], cmd_pyld);
		tag_desc[desc_idx].callback = ipa3_tag_destroy_imm;
		tag_desc[desc_idx].user1 = cmd_pyld;
		++desc_idx;
	}
	if (ipa3_ctx->ulso_wa) {
		/* dummary regsiter read IC with HPS clear*/
		cmd.size = 4;
		cmd.base = dma_alloc_coherent(ipa3_ctx->pdev, cmd.size,
			&cmd.phys_base, GFP_KERNEL);
		if (cmd.base == NULL) {
			res = -ENOMEM;
			goto fail_free_desc;
		}
		offset = ipahal_get_reg_n_ofst(IPA_STAT_QUOTA_BASE_n,
			ipa3_ctx->ee);
		dummy_reg_read.skip_pipeline_clear = false;
		dummy_reg_read.pipeline_clear_options = IPAHAL_HPS_CLEAR;
		dummy_reg_read.offset = offset;
		dummy_reg_read.sys_addr = cmd.phys_base;
		cmd_pyld = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_REGISTER_READ,
			&dummy_reg_read, false);
		if (!cmd_pyld) {
			IPAERR("failed to construct DUMMY READ IC\n");
			res = -ENOMEM;
			goto fail_free_desc;
		}
		ipa3_init_imm_cmd_desc(&tag_desc[desc_idx], cmd_pyld);
		tag_desc[desc_idx].callback = ipa3_tag_destroy_imm;
		tag_desc[desc_idx].user1 = cmd_pyld;
		++desc_idx;
	}

	/* NO-OP IC for ensuring that IPA pipeline is empty */
	if (!ipa3_ctx->ulso_wa)
	{
		cmd_pyld = ipahal_construct_nop_imm_cmd(
			false, IPAHAL_FULL_PIPELINE_CLEAR, false);
		if (!cmd_pyld) {
			IPAERR("failed to construct NOP imm cmd\n");
			res = -ENOMEM;
			goto fail_free_desc;
		}
		ipa3_init_imm_cmd_desc(&tag_desc[desc_idx], cmd_pyld);
		tag_desc[desc_idx].callback = ipa3_tag_destroy_imm;
		tag_desc[desc_idx].user1 = cmd_pyld;
		++desc_idx;
	}

	/* IP_PACKET_INIT IC for tag status to be sent to apps */
	pktinit_cmd.destination_pipe_index =
		ipa_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS);
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_IP_PACKET_INIT, &pktinit_cmd, false);
	if (!cmd_pyld) {
		IPAERR("failed to construct ip_packet_init imm cmd\n");
		res = -ENOMEM;
		goto fail_free_desc;
	}
	ipa3_init_imm_cmd_desc(&tag_desc[desc_idx], cmd_pyld);
	tag_desc[desc_idx].callback = ipa3_tag_destroy_imm;
	tag_desc[desc_idx].user1 = cmd_pyld;
	++desc_idx;

	/* status IC */
	status.tag = IPA_COOKIE;
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_IP_PACKET_TAG_STATUS, &status, false);
	if (!cmd_pyld) {
		IPAERR("failed to construct ip_packet_tag_status imm cmd\n");
		res = -ENOMEM;
		goto fail_free_desc;
	}
	ipa3_init_imm_cmd_desc(&tag_desc[desc_idx], cmd_pyld);
	tag_desc[desc_idx].callback = ipa3_tag_destroy_imm;
	tag_desc[desc_idx].user1 = cmd_pyld;
	++desc_idx;

	comp = kzalloc(sizeof(*comp), GFP_KERNEL);
	if (!comp) {
		IPAERR("no mem\n");
		res = -ENOMEM;
		goto fail_free_desc;
	}
	init_completion(&comp->comp);

	/* completion needs to be released from both here and rx handler */
	atomic_set(&comp->cnt, 2);

	/* dummy packet to send to IPA. packet payload is a completion object */
	dummy_skb = alloc_skb(sizeof(comp), GFP_KERNEL);
	if (!dummy_skb) {
		IPAERR("failed to allocate memory\n");
		res = -ENOMEM;
		goto fail_free_comp;
	}

	memcpy(skb_put(dummy_skb, sizeof(comp)), &comp, sizeof(comp));

	if (desc_idx >= IPA_TAG_MAX_DESC) {
		IPAERR("number of commands is out of range\n");
		res = -ENOBUFS;
		goto fail_free_skb;
	}

	tag_desc[desc_idx].pyld = dummy_skb->data;
	tag_desc[desc_idx].len = dummy_skb->len;
	tag_desc[desc_idx].type = IPA_DATA_DESC_SKB;
	tag_desc[desc_idx].callback = ipa3_tag_free_skb;
	tag_desc[desc_idx].user1 = dummy_skb;
	desc_idx++;
retry_alloc:
	/* send all descriptors to IPA with single EOT */
	res = ipa3_send(sys, desc_idx, tag_desc, true);
	if (res) {
		if (res == -ENOMEM) {
			if (retry_cnt < MAX_RETRY_ALLOC) {
				IPADBG(
				"failed to alloc memory retry cnt = %d\n",
					retry_cnt);
				retry_cnt++;
				usleep_range(ALLOC_MIN_SLEEP_RX,
					ALLOC_MAX_SLEEP_RX);
				goto retry_alloc;
			}

		}
		IPAERR("failed to send TAG packets %d\n", res);
		res = -ENOMEM;
		goto fail_free_skb;
	}
	kfree(tag_desc);
	tag_desc = NULL;
	ipa3_ctx->tag_process_before_gating = false;

	IPADBG("waiting for TAG response\n");
	res = wait_for_completion_timeout(&comp->comp, timeout);
	if (res == 0) {
		IPAERR("timeout (%lu msec) on waiting for TAG response\n",
			timeout);
		WARN_ON(1);
		if (atomic_dec_return(&comp->cnt) == 0)
			kfree(comp);
		if (cmd.base) {
			dma_free_coherent(ipa3_ctx->pdev, cmd.size,
				cmd.base, cmd.phys_base);
		}
		return -ETIME;
	}

	IPADBG("TAG response arrived!\n");
	if (atomic_dec_return(&comp->cnt) == 0)
		kfree(comp);

	if (cmd.base) {
		dma_free_coherent(ipa3_ctx->pdev, cmd.size,
			cmd.base, cmd.phys_base);
	}

	/*
	 * sleep for short period to ensure IPA wrote all packets to
	 * the transport
	 */
	usleep_range(IPA_TAG_SLEEP_MIN_USEC, IPA_TAG_SLEEP_MAX_USEC);

	return 0;

fail_free_skb:
	kfree_skb(dummy_skb);
fail_free_comp:
	kfree(comp);
fail_free_desc:
	/*
	 * Free only the first descriptors allocated here.
	 * [nop, pkt_init, status, dummy_skb]
	 * The user is responsible to free his allocations
	 * in case of failure.
	 * The min is required because we may fail during
	 * of the initial allocations above
	 */
	for (i = descs_num;
		i < min(req_num_tag_desc, desc_idx); i++)
		if (tag_desc[i].callback)
			tag_desc[i].callback(tag_desc[i].user1,
				tag_desc[i].user2);
	if (cmd.base) {
		dma_free_coherent(ipa3_ctx->pdev, cmd.size,
			cmd.base, cmd.phys_base);
	}
fail_free_tag_desc:
	kfree(tag_desc);
	return res;
}

/**
 * ipa3_tag_generate_force_close_desc() - generate descriptors for force close
 *					 immediate command
 *
 * @desc: descriptors for IC
 * @desc_size: desc array size
 * @start_pipe: first pipe to close aggregation
 * @end_pipe: last (non-inclusive) pipe to close aggregation
 *
 * Return: number of descriptors written or negative in case of failure
 */
static int ipa3_tag_generate_force_close_desc(struct ipa3_desc desc[],
	int desc_size, int start_pipe, int end_pipe)
{
	int i;
	struct ipa_ep_cfg_aggr ep_aggr;
	int desc_idx = 0;
	int res;
	struct ipahal_imm_cmd_register_write reg_write_agg_close;
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	struct ipahal_reg_valmask valmask;
	u32 offset = 0;

	for (i = start_pipe; i < end_pipe; i++) {
		ipahal_read_reg_n_fields(IPA_ENDP_INIT_AGGR_n, i, &ep_aggr);
		if (!ep_aggr.aggr_en)
			continue;
		/* Skip Coalescing pipe when ulso wa is enabled. */
		if (ipa3_ctx->ulso_wa &&
			(i == ipa_get_ep_mapping(IPA_CLIENT_APPS_WAN_COAL_CONS)))
			continue;
		IPADBG("Force close ep: %d\n", i);
		if (desc_idx + 1 > desc_size) {
			IPAERR("Internal error - no descriptors\n");
			res = -EFAULT;
			goto fail_no_desc;
		}

		if (!ipa3_ctx->ulso_wa) {
			reg_write_agg_close.skip_pipeline_clear = false;
			reg_write_agg_close.pipeline_clear_options =
				IPAHAL_FULL_PIPELINE_CLEAR;
		} else {
			reg_write_agg_close.skip_pipeline_clear = true;
		}

		if (ipa3_ctx->ipa_hw_type < IPA_HW_v5_0)
			offset = ipahal_get_reg_ofst(
				IPA_AGGR_FORCE_CLOSE);
		else
			offset = ipahal_get_ep_reg_offset(
				IPA_AGGR_FORCE_CLOSE_n, i);
		reg_write_agg_close.offset = offset;
		ipahal_get_aggr_force_close_valmask(i, &valmask);
		reg_write_agg_close.value = valmask.val;
		reg_write_agg_close.value_mask = valmask.mask;
		cmd_pyld = ipahal_construct_imm_cmd(IPA_IMM_CMD_REGISTER_WRITE,
			&reg_write_agg_close, false);
		if (!cmd_pyld) {
			IPAERR("failed to construct register_write imm cmd\n");
			res = -ENOMEM;
			goto fail_alloc_reg_write_agg_close;
		}

		ipa3_init_imm_cmd_desc(&desc[desc_idx], cmd_pyld);
		desc[desc_idx].callback = ipa3_tag_destroy_imm;
		desc[desc_idx].user1 = cmd_pyld;
		++desc_idx;
	}

	return desc_idx;

fail_alloc_reg_write_agg_close:
	for (i = 0; i < desc_idx; ++i)
		if (desc[desc_idx].callback)
			desc[desc_idx].callback(desc[desc_idx].user1,
				desc[desc_idx].user2);
fail_no_desc:
	return res;
}

/**
 * ipa3_tag_aggr_force_close() - Force close aggregation
 *
 * @pipe_num: pipe number or -1 for all pipes
 */
int ipa3_tag_aggr_force_close(int pipe_num)
{
	struct ipa3_desc *desc;
	int res = -1;
	int start_pipe;
	int end_pipe;
	int num_descs;
	int num_aggr_descs;

	if (pipe_num < -1 || pipe_num >= (int)ipa3_ctx->ipa_num_pipes) {
		IPAERR("Invalid pipe number %d\n", pipe_num);
		return -EINVAL;
	}

	if (pipe_num == -1) {
		start_pipe = 0;
		end_pipe = ipa3_ctx->ipa_num_pipes;
	} else {
		start_pipe = pipe_num;
		end_pipe = pipe_num + 1;
	}

	num_descs = end_pipe - start_pipe;

	desc = kcalloc(num_descs, sizeof(*desc), GFP_KERNEL);
	if (!desc) {
		IPAERR("no mem\n");
		return -ENOMEM;
	}

	/* Force close aggregation on all valid pipes with aggregation */
	num_aggr_descs = ipa3_tag_generate_force_close_desc(desc, num_descs,
						start_pipe, end_pipe);
	if (num_aggr_descs < 0) {
		IPAERR("ipa3_tag_generate_force_close_desc failed %d\n",
			num_aggr_descs);
		goto fail_free_desc;
	}

	res = ipa3_tag_process(desc, num_aggr_descs,
			      IPA_FORCE_CLOSE_TAG_PROCESS_TIMEOUT);

fail_free_desc:
	kfree(desc);

	return res;
}

/**
 * ipa_is_ready() - check if IPA module was initialized
 * successfully
 *
 * Return value: true for yes; false for no
 */
bool ipa_is_ready(void)
{
	bool complete;

	if (ipa3_ctx == NULL)
		return false;
	mutex_lock(&ipa3_ctx->lock);
	complete = ipa3_ctx->ipa_initialization_complete;
	mutex_unlock(&ipa3_ctx->lock);
	return complete;
}
EXPORT_SYMBOL(ipa_is_ready);

/**
 * ipa3_is_client_handle_valid() - check if IPA client handle is valid handle
 *
 * Return value: true for yes; false for no
 */
bool ipa3_is_client_handle_valid(u32 clnt_hdl)
{
	if (clnt_hdl >= 0 && clnt_hdl < ipa3_ctx->ipa_num_pipes)
		return true;
	return false;
}
EXPORT_SYMBOL(ipa3_is_client_handle_valid);

/**
 * ipa3_proxy_clk_unvote() - called to remove IPA clock proxy vote
 *
 * Return value: none
 */
void ipa3_proxy_clk_unvote(void)
{
	if (ipa3_ctx == NULL)
		return;
	mutex_lock(&ipa3_ctx->q6_proxy_clk_vote_mutex);
	if (ipa3_ctx->q6_proxy_clk_vote_valid) {
		IPA_ACTIVE_CLIENTS_DEC_SPECIAL("PROXY_CLK_VOTE");
		ipa3_ctx->q6_proxy_clk_vote_cnt--;
		if (ipa3_ctx->q6_proxy_clk_vote_cnt == 0)
			ipa3_ctx->q6_proxy_clk_vote_valid = false;
	}
	mutex_unlock(&ipa3_ctx->q6_proxy_clk_vote_mutex);
}

/**
 * ipa3_proxy_clk_vote() - called to add IPA clock proxy vote
 *
 * Return value: none
 */
void ipa3_proxy_clk_vote(bool is_ssr)
{
	if (ipa3_ctx == NULL)
		return;

	/* Avoid duplicate votes in case we are in SSR even before uC is loaded. */
	if (is_ssr && !ipa3_uc_loaded_check()) {
		IPADBG("Dup proxy vote. Ignore as uC is not yet loaded\n");
		return;
	}
	mutex_lock(&ipa3_ctx->q6_proxy_clk_vote_mutex);
	if (!ipa3_ctx->q6_proxy_clk_vote_valid ||
		(ipa3_ctx->q6_proxy_clk_vote_cnt > 0)) {
		IPA_ACTIVE_CLIENTS_INC_SPECIAL("PROXY_CLK_VOTE");
		ipa3_ctx->q6_proxy_clk_vote_cnt++;
		ipa3_ctx->q6_proxy_clk_vote_valid = true;
	}
	mutex_unlock(&ipa3_ctx->q6_proxy_clk_vote_mutex);
}
EXPORT_SYMBOL(ipa3_proxy_clk_vote);

/**
 * ipa3_get_smem_restr_bytes()- Return IPA smem restricted bytes
 *
 * Return value: u16 - number of IPA smem restricted bytes
 */
u16 ipa3_get_smem_restr_bytes(void)
{
	if (ipa3_ctx)
		return ipa3_ctx->smem_restricted_bytes;

	IPAERR("IPA Driver not initialized\n");

	return 0;
}

/**
 * ipa3_get_modem_cfg_emb_pipe_flt()- Return ipa3_ctx->modem_cfg_emb_pipe_flt
 *
 * Return value: true if modem configures embedded pipe flt, false otherwise
 */
bool ipa3_get_modem_cfg_emb_pipe_flt(void)
{
	if (ipa3_ctx)
		return ipa3_ctx->modem_cfg_emb_pipe_flt;

	IPAERR("IPA driver has not been initialized\n");

	return false;
}

/**
 * ipa3_get_transport_type()
 *
 * Return value: enum ipa_transport_type
 */
enum ipa_transport_type ipa3_get_transport_type(void)
{
	return IPA_TRANSPORT_TYPE_GSI;
}
EXPORT_SYMBOL(ipa3_get_transport_type);

/**
 * ipa3_get_max_num_pipes()
 *
 * Return value: maximal possible pipes num per hw_ver (not necessarily the
 *	actual pipes num)
 */
u32 ipa3_get_max_num_pipes(void)
{
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v5_0)
		return IPA5_PIPES_NUM;
	else
		return IPA3_MAX_NUM_PIPES;
}

u32 ipa3_get_num_pipes(void)
{
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v5_0) {
		struct ipahal_ipa_flavor_0 ipa_flavor;

		ipahal_read_reg_fields(IPA_FLAVOR_0, &ipa_flavor);
		return ipa_flavor.ipa_pipes;
	}
	return ipahal_read_reg(IPA_ENABLED_PIPES);
}

/**
 * ipa3_disable_apps_wan_cons_deaggr()-
 * set ipa3_ctx->ipa_client_apps_wan_cons_agg_gro
 *
 * Return value: 0 or negative in case of failure
 */
int ipa3_disable_apps_wan_cons_deaggr(uint32_t agg_size, uint32_t agg_count)
{
	int res = -1;

	/* ipahal will adjust limits based on HW capabilities */

	if (ipa3_ctx) {
		ipa3_ctx->ipa_client_apps_wan_cons_agg_gro = true;
		return 0;
	}
	return res;
}

void *ipa3_get_ipc_logbuf(void)
{
	if (ipa3_ctx)
		return ipa3_ctx->logbuf;

	return NULL;
}
EXPORT_SYMBOL(ipa3_get_ipc_logbuf);

void *ipa3_get_ipc_logbuf_low(void)
{
	if (ipa3_ctx)
		return ipa3_ctx->logbuf_low;

	return NULL;
}
EXPORT_SYMBOL(ipa3_get_ipc_logbuf_low);

void ipa3_get_holb(int ep_idx, struct ipa_ep_cfg_holb *holb)
{
	*holb = ipa3_ctx->ep[ep_idx].holb;
}

void ipa3_set_tag_process_before_gating(bool val)
{
	ipa3_ctx->tag_process_before_gating = val;
}
EXPORT_SYMBOL(ipa3_set_tag_process_before_gating);

/**
 * ipa_is_vlan_mode - check if a LAN driver should load in VLAN mode
 * @iface - type of vlan capable device
 * @res - query result: true for vlan mode, false for non vlan mode
 *
 * API must be called after ipa_is_ready() returns true, otherwise it will fail
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_is_vlan_mode(enum ipa_vlan_ifaces iface, bool *res)
{
	if (!res) {
		IPAERR("NULL out param\n");
		return -EINVAL;
	}

	if (iface < 0 || iface >= IPA_VLAN_IF_MAX) {
		IPAERR("invalid iface %d\n", iface);
		return -EINVAL;
	}

	if (!ipa_is_ready()) {
		IPAERR("IPA is not ready yet\n");
		return -ENODEV;
	}

	*res = ipa3_ctx->vlan_mode_iface[iface];

	IPADBG("Driver %d vlan mode is %d\n", iface, *res);
	return 0;
}
EXPORT_SYMBOL(ipa_is_vlan_mode);

/**
 * ipa_is_modem_pipe()- Checks if pipe is owned by the modem
 *
 * @pipe_idx: pipe number
 * Return value: true if owned by modem, false otherwize
 */
bool ipa_is_modem_pipe(int pipe_idx)
{
	int client_idx;

	if (pipe_idx >= ipa3_ctx->ipa_num_pipes || pipe_idx < 0) {
		IPAERR("Bad pipe index!\n");
		return false;
	}

	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		if (!IPA_CLIENT_IS_Q6_CONS(client_idx) &&
			!IPA_CLIENT_IS_Q6_PROD(client_idx))
			continue;
		if (ipa_get_ep_mapping(client_idx) == pipe_idx)
			return true;
	}

	return false;
}

static void ipa3_write_rsrc_grp_type_reg(int group_index,
			enum ipa_rsrc_grp_type_src n, bool src,
			struct ipahal_reg_rsrc_grp_xy_cfg *val)
{
	u8 hw_type_idx;

	hw_type_idx = ipa3_ctx->hw_type_index;

	switch (hw_type_idx) {
	case IPA_3_0:
		if (src) {
			switch (group_index) {
			case IPA_v3_0_GROUP_UL:
			case IPA_v3_0_GROUP_DL:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v3_0_GROUP_DIAG:
			case IPA_v3_0_GROUP_DMA:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v3_0_GROUP_Q6ZIP:
			case IPA_v3_0_GROUP_UC_RX_Q:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_45_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid source resource group,index #%d\n",
				group_index);
				break;
			}
		} else {
			switch (group_index) {
			case IPA_v3_0_GROUP_UL:
			case IPA_v3_0_GROUP_DL:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v3_0_GROUP_DIAG:
			case IPA_v3_0_GROUP_DMA:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v3_0_GROUP_Q6ZIP_GENERAL:
			case IPA_v3_0_GROUP_Q6ZIP_ENGINE:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_45_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid destination resource group,index #%d\n",
				group_index);
				break;
			}
		}
		break;
	case IPA_3_5:
	case IPA_3_5_MHI:
	case IPA_3_5_1:
		if (src) {
			switch (group_index) {
			case IPA_v3_5_GROUP_LWA_DL:
			case IPA_v3_5_GROUP_UL_DL:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v3_5_MHI_GROUP_DMA:
			case IPA_v3_5_GROUP_UC_RX_Q:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid source resource group,index #%d\n",
				group_index);
				break;
			}
		} else {
			switch (group_index) {
			case IPA_v3_5_GROUP_LWA_DL:
			case IPA_v3_5_GROUP_UL_DL:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v3_5_MHI_GROUP_DMA:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid destination resource group,index #%d\n",
				group_index);
				break;
			}
		}
		break;
	case IPA_4_0:
	case IPA_4_0_MHI:
	case IPA_4_1:
		if (src) {
			switch (group_index) {
			case IPA_v4_0_GROUP_LWA_DL:
				fallthrough;
			case IPA_v4_0_GROUP_UL_DL:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v4_0_MHI_GROUP_DMA:
				fallthrough;
			case IPA_v4_0_GROUP_UC_RX_Q:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid source resource group,index #%d\n",
				group_index);
				break;
			}
		} else {
			switch (group_index) {
			case IPA_v4_0_GROUP_LWA_DL:
				fallthrough;
			case IPA_v4_0_GROUP_UL_DL:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v4_0_MHI_GROUP_DMA:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid destination resource group,index #%d\n",
				group_index);
				break;
			}
		}
		break;
	case IPA_4_2:
		if (src) {
			switch (group_index) {
			case IPA_v4_2_GROUP_UL_DL:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid source resource group,index #%d\n",
				group_index);
				break;
			}
		} else {
			switch (group_index) {
			case IPA_v4_2_GROUP_UL_DL:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid destination resource group,index #%d\n",
				group_index);
				break;
			}
		}
		break;
	case IPA_4_5:
	case IPA_4_5_MHI:
	case IPA_4_5_APQ:
	case IPA_4_5_AUTO:
	case IPA_4_5_AUTO_MHI:
		if (src) {
			switch (group_index) {
			case IPA_v4_5_MHI_GROUP_PCIE:
			case IPA_v4_5_GROUP_UL_DL:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v4_5_MHI_GROUP_DMA:
			case IPA_v4_5_MHI_GROUP_QDSS:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v4_5_GROUP_UC_RX_Q:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_45_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid source resource group,index #%d\n",
				group_index);
				break;
			}
		} else {
			switch (group_index) {
			case IPA_v4_5_MHI_GROUP_PCIE:
			case IPA_v4_5_GROUP_UL_DL:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v4_5_MHI_GROUP_DMA:
			case IPA_v4_5_MHI_GROUP_QDSS:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v4_5_GROUP_UC_RX_Q:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_45_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid destination resource group,index #%d\n",
				group_index);
				break;
			}
		}
		break;
	case IPA_4_7:
		if (src) {
			switch (group_index) {
			case IPA_v4_7_GROUP_UL_DL:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid source resource group,index #%d\n",
				group_index);
				break;
			}
		} else {
			switch (group_index) {
			case IPA_v4_7_GROUP_UL_DL:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid destination resource group,index #%d\n",
				group_index);
				break;
			}
		}
		break;
	case IPA_4_9:
		if (src) {
			switch (group_index) {
			case IPA_v4_9_GROUP_UL_DL:
			case IPA_v4_9_GROUP_DMA:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v4_9_GROUP_UC_RX:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid source resource group,index #%d\n",
				group_index);
				break;
			}
		} else {
			switch (group_index) {
			case IPA_v4_9_GROUP_UL_DL:
			case IPA_v4_9_GROUP_DMA:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v4_9_GROUP_UC_RX:
			case IPA_v4_9_GROUP_DRB_IP:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid destination resource group,index #%d\n",
				group_index);
				break;
			}
		}
		break;
	case IPA_4_11:
			if (src) {
				switch (group_index) {
				case IPA_v4_11_GROUP_UL_DL:
					ipahal_write_reg_n_fields(
						IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n,
						n, val);
					break;
				default:
					IPAERR(
					" Invalid source resource group,index #%d\n",
					group_index);
					break;
				}
			} else {
				switch (group_index) {
				case IPA_v4_11_GROUP_UL_DL:
					ipahal_write_reg_n_fields(
						IPA_DST_RSRC_GRP_01_RSRC_TYPE_n,
						n, val);
					break;
				case IPA_v4_11_GROUP_DRB_IP:
					ipahal_write_reg_n_fields(
							IPA_DST_RSRC_GRP_23_RSRC_TYPE_n,
							n, val);
					break;
				default:
					IPAERR(
					" Invalid destination resource group,index #%d\n",
					group_index);
					break;
				}
			}
			break;
	case IPA_5_0:
	case IPA_5_0_MHI:
	case IPA_5_1:
	case IPA_5_1_APQ:
		if (src) {
			switch (group_index) {
			case IPA_v5_0_GROUP_UL:
			case IPA_v5_0_GROUP_DL:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v5_0_GROUP_DMA:
			case IPA_v5_0_GROUP_QDSS:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v5_0_GROUP_URLLC:
			case IPA_v5_0_GROUP_UC:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_45_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid source resource group,index #%d\n",
				group_index);
				break;
			}
		} else {
			switch (group_index) {
			case IPA_v5_0_GROUP_UL:
			case IPA_v5_0_GROUP_DL:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v5_0_GROUP_DMA:
			case IPA_v5_0_GROUP_QDSS:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v5_0_GROUP_URLLC:
			case IPA_v5_0_GROUP_UC:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_45_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v5_0_GROUP_DRB_IP:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_67_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid destination resource group,index #%d\n",
				group_index);
				break;
			}
		}
		break;

	case IPA_5_2:
		if (src) {
			switch (group_index) {
			case IPA_v5_2_GROUP_UL:
			case IPA_v5_2_GROUP_DL:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v5_2_GROUP_URLLC:
			case IPA_v5_2_GROUP_DRB_IP:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid source resource group,index #%d\n",
				group_index);
				break;
			}
		} else {
			switch (group_index) {
			case IPA_v5_2_GROUP_UL:
			case IPA_v5_2_GROUP_DL:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v5_2_GROUP_URLLC:
			case IPA_v5_2_GROUP_DRB_IP:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid destination resource group,index #%d\n",
				group_index);
				break;
			}
		}
		break;

	case IPA_5_5:
	case IPA_5_5_XR:
		if (src) {
			switch (group_index) {
			case IPA_v5_5_GROUP_UL:
			case IPA_v5_5_GROUP_DL:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v5_5_GROUP_DMA:
			case IPA_v5_5_GROUP_QDSS:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v5_5_GROUP_URLLC:
			case IPA_v5_5_GROUP_UC:
				ipahal_write_reg_n_fields(
					IPA_SRC_RSRC_GRP_45_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid source resource group,index #%d\n",
				group_index);
				break;
			}
		} else {
			switch (group_index) {
			case IPA_v5_5_GROUP_UL:
			case IPA_v5_5_GROUP_DL:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_01_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v5_5_GROUP_DMA:
			case IPA_v5_5_GROUP_QDSS:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_23_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v5_5_GROUP_URLLC:
			case IPA_v5_5_GROUP_UC:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_45_RSRC_TYPE_n,
					n, val);
				break;
			case IPA_v5_5_GROUP_DRB_IP:
				ipahal_write_reg_n_fields(
					IPA_DST_RSRC_GRP_67_RSRC_TYPE_n,
					n, val);
				break;
			default:
				IPAERR(
				" Invalid destination resource group,index #%d\n",
				group_index);
				break;
			}
		}
		break;

	default:
		IPAERR("invalid hw type\n");
		WARN_ON(1);
		return;
	}
}

static void ipa3_configure_rx_hps_clients(int depth,
	int max_clnt_in_depth, int base_index, bool min)
{
	int i;
	struct ipahal_reg_rx_hps_clients val;
	u8 hw_type_idx;

	hw_type_idx = ipa3_ctx->hw_type_index;

	for (i = 0 ; i < max_clnt_in_depth ; i++) {
		if (min)
			val.client_minmax[i] =
				ipa3_rsrc_rx_grp_config
				[hw_type_idx]
				[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ]
				[i + base_index].min;
		else
			val.client_minmax[i] =
				ipa3_rsrc_rx_grp_config
				[hw_type_idx]
				[IPA_RSRC_GRP_TYPE_RX_HPS_CMDQ]
				[i + base_index].max;
	}
	if (depth) {
		ipahal_write_reg_fields(min ? IPA_RX_HPS_CLIENTS_MIN_DEPTH_1 :
					IPA_RX_HPS_CLIENTS_MAX_DEPTH_1,
					&val);
	} else {
		ipahal_write_reg_fields(min ? IPA_RX_HPS_CLIENTS_MIN_DEPTH_0 :
					IPA_RX_HPS_CLIENTS_MAX_DEPTH_0,
					&val);
	}
}

static void ipa3_configure_rx_hps_weight(void)
{
	struct ipahal_reg_rx_hps_weights val;
	u8 hw_type_idx;

	hw_type_idx = ipa3_ctx->hw_type_index;

	val.hps_queue_weight_0 =
			ipa3_rsrc_rx_grp_hps_weight_config
			[hw_type_idx][IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG]
			[0];
	val.hps_queue_weight_1 =
			ipa3_rsrc_rx_grp_hps_weight_config
			[hw_type_idx][IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG]
			[1];
	val.hps_queue_weight_2 =
			ipa3_rsrc_rx_grp_hps_weight_config
			[hw_type_idx][IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG]
			[2];
	val.hps_queue_weight_3 =
			ipa3_rsrc_rx_grp_hps_weight_config
			[hw_type_idx][IPA_RSRC_GRP_TYPE_RX_HPS_WEIGHT_CONFIG]
			[3];

	ipahal_write_reg_fields(IPA_HPS_FTCH_ARB_QUEUE_WEIGHT, &val);
}

static void ipa3_configure_rx_hps(void)
{
	int rx_hps_max_clnt_in_depth0;

	IPADBG("Assign RX_HPS CMDQ rsrc groups min-max limits\n");

	/* Starting IPA4.5 we have 5 RX_HPS_CMDQ */
	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_5)
		rx_hps_max_clnt_in_depth0 = 4;
	else
		rx_hps_max_clnt_in_depth0 = 5;

	ipa3_configure_rx_hps_clients(0, rx_hps_max_clnt_in_depth0, 0, true);
	ipa3_configure_rx_hps_clients(0, rx_hps_max_clnt_in_depth0, 0, false);

	/*
	 * IPA 3.0/3.1 uses 6 RX_HPS_CMDQ and needs depths1 for that
	 * which has two clients
	 */
	if (ipa3_ctx->ipa_hw_type <= IPA_HW_v3_1) {
		ipa3_configure_rx_hps_clients(1, 2, rx_hps_max_clnt_in_depth0,
			true);
		ipa3_configure_rx_hps_clients(1, 2, rx_hps_max_clnt_in_depth0,
			false);
	}

	/* Starting IPA4.2 no support to HPS weight config */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v3_5 &&
		(ipa3_ctx->ipa_hw_type < IPA_HW_v4_2))
		ipa3_configure_rx_hps_weight();
}

void ipa3_set_resorce_groups_min_max_limits(void)
{
	int i;
	int j;
	int src_rsrc_type_max;
	int dst_rsrc_type_max;
	int src_grp_idx_max;
	int dst_grp_idx_max;
	struct ipahal_reg_rsrc_grp_xy_cfg val;
	u8 hw_type_idx;

	IPADBG("ENTER\n");

	hw_type_idx = ipa3_ctx->hw_type_index;

	switch (hw_type_idx) {
	case IPA_3_0:
		src_rsrc_type_max = IPA_v3_0_RSRC_GRP_TYPE_SRC_MAX;
		dst_rsrc_type_max = IPA_v3_0_RSRC_GRP_TYPE_DST_MAX;
		src_grp_idx_max = IPA_v3_0_GROUP_MAX;
		dst_grp_idx_max = IPA_v3_0_GROUP_MAX;
		break;
	case IPA_3_5:
	case IPA_3_5_MHI:
	case IPA_3_5_1:
		src_rsrc_type_max = IPA_v3_5_RSRC_GRP_TYPE_SRC_MAX;
		dst_rsrc_type_max = IPA_v3_5_RSRC_GRP_TYPE_DST_MAX;
		src_grp_idx_max = IPA_v3_5_SRC_GROUP_MAX;
		dst_grp_idx_max = IPA_v3_5_DST_GROUP_MAX;
		break;
	case IPA_4_0:
	case IPA_4_0_MHI:
	case IPA_4_1:
		src_rsrc_type_max = IPA_v4_0_RSRC_GRP_TYPE_SRC_MAX;
		dst_rsrc_type_max = IPA_v4_0_RSRC_GRP_TYPE_DST_MAX;
		src_grp_idx_max = IPA_v4_0_SRC_GROUP_MAX;
		dst_grp_idx_max = IPA_v4_0_DST_GROUP_MAX;
		break;
	case IPA_4_2:
		src_rsrc_type_max = IPA_v4_0_RSRC_GRP_TYPE_SRC_MAX;
		dst_rsrc_type_max = IPA_v4_0_RSRC_GRP_TYPE_DST_MAX;
		src_grp_idx_max = IPA_v4_2_SRC_GROUP_MAX;
		dst_grp_idx_max = IPA_v4_2_DST_GROUP_MAX;
		break;
	case IPA_4_5:
	case IPA_4_5_MHI:
	case IPA_4_5_APQ:
		src_rsrc_type_max = IPA_v4_0_RSRC_GRP_TYPE_SRC_MAX;
		dst_rsrc_type_max = IPA_v4_0_RSRC_GRP_TYPE_DST_MAX;
		src_grp_idx_max = IPA_v4_5_SRC_GROUP_MAX;
		dst_grp_idx_max = IPA_v4_5_DST_GROUP_MAX;
		break;
	case IPA_4_5_AUTO:
	case IPA_4_5_AUTO_MHI:
		src_rsrc_type_max = IPA_v4_0_RSRC_GRP_TYPE_SRC_MAX;
		dst_rsrc_type_max = IPA_v4_0_RSRC_GRP_TYPE_DST_MAX;
		src_grp_idx_max = IPA_v4_5_SRC_GROUP_MAX;
		dst_grp_idx_max = IPA_v4_5_DST_GROUP_MAX;
		break;
	case IPA_4_7:
		src_rsrc_type_max = IPA_v4_0_RSRC_GRP_TYPE_SRC_MAX;
		dst_rsrc_type_max = IPA_v4_0_RSRC_GRP_TYPE_DST_MAX;
		src_grp_idx_max = IPA_v4_7_SRC_GROUP_MAX;
		dst_grp_idx_max = IPA_v4_7_DST_GROUP_MAX;
		break;
	case IPA_4_9:
		src_rsrc_type_max = IPA_v4_0_RSRC_GRP_TYPE_SRC_MAX;
		dst_rsrc_type_max = IPA_v4_0_RSRC_GRP_TYPE_DST_MAX;
		src_grp_idx_max = IPA_v4_9_SRC_GROUP_MAX;
		dst_grp_idx_max = IPA_v4_9_DST_GROUP_MAX;
		break;
	case IPA_4_11:
		src_rsrc_type_max = IPA_v4_0_RSRC_GRP_TYPE_SRC_MAX;
		dst_rsrc_type_max = IPA_v4_0_RSRC_GRP_TYPE_DST_MAX;
		src_grp_idx_max = IPA_v4_11_SRC_GROUP_MAX;
		dst_grp_idx_max = IPA_v4_11_DST_GROUP_MAX;
		break;
	case IPA_5_0:
	case IPA_5_0_MHI:
	case IPA_5_1:
	case IPA_5_1_APQ:
		src_rsrc_type_max = IPA_v5_0_RSRC_GRP_TYPE_SRC_MAX;
		dst_rsrc_type_max = IPA_v5_0_RSRC_GRP_TYPE_DST_MAX;
		src_grp_idx_max = IPA_v5_0_SRC_GROUP_MAX;
		dst_grp_idx_max = IPA_v5_0_DST_GROUP_MAX;
		break;
	case IPA_5_2:
		src_rsrc_type_max = IPA_v5_0_RSRC_GRP_TYPE_SRC_MAX;
		dst_rsrc_type_max = IPA_v5_0_RSRC_GRP_TYPE_DST_MAX;
		src_grp_idx_max = IPA_v5_2_SRC_GROUP_MAX;
		dst_grp_idx_max = IPA_v5_2_DST_GROUP_MAX;
		break;
	case IPA_5_5:
	case IPA_5_5_XR:
		src_rsrc_type_max = IPA_v5_0_RSRC_GRP_TYPE_SRC_MAX;
		dst_rsrc_type_max = IPA_v5_0_RSRC_GRP_TYPE_DST_MAX;
		src_grp_idx_max = IPA_v5_5_SRC_GROUP_MAX;
		dst_grp_idx_max = IPA_v5_5_DST_GROUP_MAX;
		break;
	default:
		IPAERR("invalid hw type index\n");
		WARN_ON(1);
		return;
	}

	IPADBG("Assign source rsrc groups min-max limits\n");
	for (i = 0; i < src_rsrc_type_max; i++) {
		for (j = 0; j < src_grp_idx_max; j = j + 2) {
			val.x_min =
			ipa3_rsrc_src_grp_config[hw_type_idx][i][j].min;
			val.x_max =
			ipa3_rsrc_src_grp_config[hw_type_idx][i][j].max;
			if ((j + 1) < IPA_GROUP_MAX) {
				val.y_min =
				ipa3_rsrc_src_grp_config[hw_type_idx][i][j + 1].min;
				val.y_max =
				ipa3_rsrc_src_grp_config[hw_type_idx][i][j + 1].max;
			}
			ipa3_write_rsrc_grp_type_reg(j, i, true, &val);
		}
	}

	IPADBG("Assign destination rsrc groups min-max limits\n");
	for (i = 0; i < dst_rsrc_type_max; i++) {
		for (j = 0; j < dst_grp_idx_max; j = j + 2) {
			val.x_min =
			ipa3_rsrc_dst_grp_config[hw_type_idx][i][j].min;
			val.x_max =
			ipa3_rsrc_dst_grp_config[hw_type_idx][i][j].max;
			if ((j + 1) < IPA_GROUP_MAX) {
				val.y_min =
				ipa3_rsrc_dst_grp_config[hw_type_idx][i][j + 1].min;
				val.y_max =
				ipa3_rsrc_dst_grp_config[hw_type_idx][i][j + 1].max;
			}
			ipa3_write_rsrc_grp_type_reg(j, i, false, &val);
		}
	}

	/* move rx_hps resource group configuration from HLOS to TZ
	 * on real platform with IPA 3.1 or later
	 */
	if (ipa3_ctx->ipa_hw_type < IPA_HW_v3_1 ||
		ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_VIRTUAL ||
		ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_EMULATION) {
		ipa3_configure_rx_hps();
	}

	IPADBG("EXIT\n");
}

void ipa3_set_resorce_groups_config(void)
{
	struct ipahal_reg_rsrc_grp_cfg cfg;
	struct ipahal_reg_rsrc_grp_cfg_ext cfg_ext;

	IPADBG("ENTER\n");

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v5_0) {
		cfg.src_grp_index = ipa_rsrc_config[ipa3_ctx->hw_type_index].src_grp_index;
		cfg.src_grp_valid = ipa_rsrc_config[ipa3_ctx->hw_type_index].src_grp_valid;
		cfg.dst_pipe_index = ipa_rsrc_config[ipa3_ctx->hw_type_index].dst_pipe_index;
		cfg.dst_pipe_valid = ipa_rsrc_config[ipa3_ctx->hw_type_index].dst_pipe_valid;
		cfg.dst_grp_index = ipa_rsrc_config[ipa3_ctx->hw_type_index].dst_grp_index;
		cfg.src_grp_valid = ipa_rsrc_config[ipa3_ctx->hw_type_index].src_grp_valid;
		cfg_ext.index = ipa_rsrc_config[ipa3_ctx->hw_type_index].src_grp_2nd_prio_index;
		cfg_ext.valid = ipa_rsrc_config[ipa3_ctx->hw_type_index].src_grp_2nd_prio_valid;

		IPADBG("Write IPA_RSRC_GRP_CFG\n");
		ipahal_write_reg_fields(IPA_RSRC_GRP_CFG, &cfg);
		IPADBG("Write IPA_RSRC_GRP_CFG_EXT\n");
		ipahal_write_reg_fields(IPA_RSRC_GRP_CFG_EXT, &cfg_ext);
	}
	IPADBG("EXIT\n");
}

static void ipa3_gsi_poll_after_suspend(struct ipa3_ep_context *ep)
{
	bool empty;

	IPADBG("switch ch %ld to poll\n", ep->gsi_chan_hdl);
	gsi_config_channel_mode(ep->gsi_chan_hdl, GSI_CHAN_MODE_POLL);
	gsi_is_channel_empty(ep->gsi_chan_hdl, &empty);
	if (!empty) {
		IPADBG("ch %ld not empty\n", ep->gsi_chan_hdl);
		/* queue a work to start polling if don't have one */
		atomic_set(&ipa3_ctx->transport_pm.eot_activity, 1);
		if (!atomic_read(&ep->sys->curr_polling_state)) {
			ipa3_inc_acquire_wakelock();
			atomic_set(&ep->sys->curr_polling_state, 1);
			queue_work(ep->sys->wq, &ep->sys->work);
		}
	}
}


static bool ipa3_gsi_channel_is_quite(struct ipa3_ep_context *ep)
{
	bool empty;

	gsi_is_channel_empty(ep->gsi_chan_hdl, &empty);
	if (!empty)
		IPADBG("ch %ld not empty\n", ep->gsi_chan_hdl);
	/*Schedule NAPI only from interrupt context to avoid race conditions*/
	return empty;
}

static int __ipa_stop_gsi_channel(u32 clnt_hdl)
{
	struct ipa_mem_buffer mem;
	int res = 0;
	int result = 0;
	int i;
	struct ipa3_ep_context *ep;
	enum ipa_client_type client_type;
	struct IpaHwOffloadStatsAllocCmdData_t *gsi_info;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];
	client_type = ipa3_get_client_mapping(clnt_hdl);
	if (IPA_CLIENT_IS_HOLB_CONS(client_type)) {
		res = ipa3_uc_client_del_holb_monitor(ep->gsi_chan_hdl,
							IPA_EE_AP);
		if (res)
			IPAERR("Delete HOLB monitor failed for ch %d\n",
					ep->gsi_chan_hdl);
		/* Set HOLB back if it was set previously.
		 * There is a possibility that uC will reset as part of HOLB
		 * monitoring.
		 */
		 if (ep->holb.en) {
		 	ipa3_cfg_ep_holb(clnt_hdl, &ep->holb);
		 }
	}
	memset(&mem, 0, sizeof(mem));

	/* stop uC gsi dbg stats monitor */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5 &&
		ipa3_ctx->ipa_hw_type != IPA_HW_v4_7 &&
		ipa3_ctx->ipa_hw_type != IPA_HW_v4_11 &&
		ipa3_ctx->ipa_hw_type != IPA_HW_v5_2) {
		switch (client_type) {
		case IPA_CLIENT_MHI_PRIME_TETH_PROD:
			gsi_info = &ipa3_ctx->gsi_info[IPA_HW_PROTOCOL_MHIP];
			gsi_info->ch_id_info[0].ch_id = 0xff;
			gsi_info->ch_id_info[0].dir = DIR_PRODUCER;
			ipa3_uc_debug_stats_alloc(*gsi_info);
			break;
		case IPA_CLIENT_MHI_PRIME_TETH_CONS:
			gsi_info = &ipa3_ctx->gsi_info[IPA_HW_PROTOCOL_MHIP];
			gsi_info->ch_id_info[1].ch_id = 0xff;
			gsi_info->ch_id_info[1].dir = DIR_CONSUMER;
			ipa3_uc_debug_stats_alloc(*gsi_info);
			break;
		case IPA_CLIENT_MHI_PRIME_RMNET_PROD:
			gsi_info = &ipa3_ctx->gsi_info[IPA_HW_PROTOCOL_MHIP];
			gsi_info->ch_id_info[2].ch_id = 0xff;
			gsi_info->ch_id_info[2].dir = DIR_PRODUCER;
			ipa3_uc_debug_stats_alloc(*gsi_info);
			break;
		case IPA_CLIENT_MHI_PRIME_RMNET_CONS:
			gsi_info = &ipa3_ctx->gsi_info[IPA_HW_PROTOCOL_MHIP];
			gsi_info->ch_id_info[3].ch_id = 0xff;
			gsi_info->ch_id_info[3].dir = DIR_CONSUMER;
			ipa3_uc_debug_stats_alloc(*gsi_info);
			break;
		case IPA_CLIENT_USB_PROD:
			gsi_info = &ipa3_ctx->gsi_info[IPA_HW_PROTOCOL_USB];
			gsi_info->ch_id_info[0].ch_id = 0xff;
			gsi_info->ch_id_info[0].dir = DIR_PRODUCER;
			ipa3_uc_debug_stats_alloc(*gsi_info);
			break;
		case IPA_CLIENT_USB_CONS:
			gsi_info = &ipa3_ctx->gsi_info[IPA_HW_PROTOCOL_USB];
			gsi_info->ch_id_info[1].ch_id = 0xff;
			gsi_info->ch_id_info[1].dir = DIR_CONSUMER;
			ipa3_uc_debug_stats_alloc(*gsi_info);
			break;
		default:
			IPADBG("client_type %d not supported\n",
				client_type);
		}
	}

	/*
	 * Apply the GSI stop retry logic if GSI returns err code to retry.
	 * Apply the retry logic for ipa_client_prod as well as ipa_client_cons.
	 */
	for (i = 0; i < IPA_GSI_CHANNEL_STOP_MAX_RETRY; i++) {
		IPADBG("Calling gsi_stop_channel ch:%lu\n",
			ep->gsi_chan_hdl);
		res = gsi_stop_channel(ep->gsi_chan_hdl);
		IPADBG("gsi_stop_channel ch: %lu returned %d\n",
			ep->gsi_chan_hdl, res);
		if (res != -GSI_STATUS_AGAIN && res != -GSI_STATUS_TIMED_OUT)
			return res;
		/*
		 * From >=IPA4.0 version not required to send dma send command,
		 * this issue was fixed in latest versions.
		 */
		if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
			IPADBG("Inject a DMA_TASK with 1B packet to IPA\n");
			/* Send a 1B packet DMA_TASK to IPA and try again */
			result = ipa3_inject_dma_task_for_gsi();
			if (result) {
				IPAERR("Failed to inject DMA TASk for GSI\n");
				return result;
			}
		}
		/* sleep for short period to flush IPA */
		usleep_range(IPA_GSI_CHANNEL_STOP_SLEEP_MIN_USEC,
			IPA_GSI_CHANNEL_STOP_SLEEP_MAX_USEC);
	}

	IPAERR("Failed  to stop GSI channel with retries\n");
	return res;
}

/**
 * ipa_stop_gsi_channel()- Stops a GSI channel in IPA
 * @chan_hdl: GSI channel handle
 *
 * This function implements the sequence to stop a GSI channel
 * in IPA. This function returns when the channel is in STOP state.
 *
 * Return value: 0 on success, negative otherwise
 */
int ipa_stop_gsi_channel(u32 clnt_hdl)
{
	int res;

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));
	res = __ipa_stop_gsi_channel(clnt_hdl);
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	return res;
}
EXPORT_SYMBOL(ipa_stop_gsi_channel);

static int _ipa_suspend_resume_pipe(enum ipa_client_type client, bool suspend)
{
	struct ipa_ep_cfg_ctrl cfg;
	int ipa_ep_idx, wan_coal_ep_idx, lan_coal_ep_idx;
	struct ipa3_ep_context *ep;
	int res;
	struct ipa_ep_cfg_holb holb_cfg;

	ipa_ep_idx = ipa_get_ep_mapping(client);
	if (ipa_ep_idx < 0) {
		IPADBG("client %d not configured\n", client);
		return 0;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];
	if (!ep->valid)
		return 0;

	IPADBG("%s pipe %d\n", suspend ? "suspend" : "unsuspend", ipa_ep_idx);

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		if(client == IPA_CLIENT_APPS_WAN_CONS ||
			client == IPA_CLIENT_APPS_LAN_CONS) {
			memset(&cfg, 0, sizeof(cfg));
			cfg.ipa_ep_suspend = suspend;
			ipa_cfg_ep_ctrl(ipa_ep_idx, &cfg);
			if (suspend)
				ipa3_gsi_poll_after_suspend(ep);
			else if (!atomic_read(&ep->sys->curr_polling_state))
				gsi_config_channel_mode(ep->gsi_chan_hdl,
					GSI_CHAN_MODE_CALLBACK);
		}
		return 0;
	}

	/*
	 * Configure the callback mode only one time after starting the channel
	 * otherwise observing IEOB interrupt received before configure callmode
	 * second time. It was leading race condition in updating current
	 * polling state.
	 */

	if (suspend) {
		res = __ipa_stop_gsi_channel(ipa_ep_idx);
		if (res) {
			IPAERR("failed to stop LAN channel\n");
			ipa_assert();
		}
	} else {
		res = gsi_start_channel(ep->gsi_chan_hdl);
		if (res) {
			IPAERR("failed to start LAN channel\n");
			ipa_assert();
		}
	}

	if ((ipa3_ctx->ipa_hw_type >= IPA_HW_v5_2 && client == IPA_CLIENT_APPS_WAN_CONS)
			|| (ipa3_ctx->ipa_hw_type >= IPA_HW_v5_5 &&
			 client == IPA_CLIENT_APPS_WAN_COAL_CONS) ||
			client == IPA_CLIENT_ODL_DPL_CONS) {
		ipa_ep_idx = ipa_get_ep_mapping(client);
		if (ipa_ep_idx != IPA_EP_NOT_ALLOCATED && ipa3_ctx->ep[ipa_ep_idx].valid) {
			memset(&holb_cfg, 0, sizeof(holb_cfg));
			if (suspend)
				holb_cfg.en = 0;
			else
				holb_cfg.en = 1;
			IPADBG("Endpoint = %d HOLB mode = %d\n", ipa_ep_idx, holb_cfg.en);
			ipahal_write_reg_n_fields(IPA_ENDP_INIT_HOL_BLOCK_EN_n,
					ipa_ep_idx, &holb_cfg);
			/* IPA4.5 issue requires HOLB_EN to be written twice */
			if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5 && holb_cfg.en)
				ipahal_write_reg_n_fields(
						IPA_ENDP_INIT_HOL_BLOCK_EN_n,
						ipa_ep_idx, &holb_cfg);
		}
	}

	/* Apps prod pipes use common event ring so cannot configure mode*/

	/*
	 * Skipping to configure mode for default [w|l]an pipe,
	 * as both pipes using commong event ring. if both pipes
	 * configure same event ring observing race condition in
	 * updating current polling state.
	 */

	if (IPA_CLIENT_IS_APPS_PROD(client) ||
		(client == IPA_CLIENT_APPS_WAN_CONS &&
		 IPA_CLIENT_IS_MAPPED(IPA_CLIENT_APPS_WAN_COAL_CONS, wan_coal_ep_idx)) ||
		(client == IPA_CLIENT_APPS_LAN_CONS &&
		 IPA_CLIENT_IS_MAPPED(IPA_CLIENT_APPS_LAN_COAL_CONS, lan_coal_ep_idx)))
		return 0;

	if (suspend) {
		IPADBG("switch ch %ld to poll\n", ep->gsi_chan_hdl);
		gsi_config_channel_mode(ep->gsi_chan_hdl, GSI_CHAN_MODE_POLL);
		if (!ipa3_gsi_channel_is_quite(ep))
			return -EAGAIN;
	} else if (!atomic_read(&ep->sys->curr_polling_state)) {
		IPADBG("switch ch %ld to callback\n", ep->gsi_chan_hdl);
		gsi_config_channel_mode(ep->gsi_chan_hdl,
			GSI_CHAN_MODE_CALLBACK);
	}

	return 0;
}

void ipa3_force_close_coal(
	bool close_wan,
	bool close_lan )
{
	struct ipa3_desc desc[ MAX_CCP_SUB ];

	int ep_idx, num_desc = 0;

	if ( close_wan
		 &&
		 IPA_CLIENT_IS_MAPPED_VALID(IPA_CLIENT_APPS_WAN_COAL_CONS, ep_idx)
		 &&
		 ipa3_ctx->coal_cmd_pyld[WAN_COAL_SUB] ) {

		ipa3_init_imm_cmd_desc(
			&desc[num_desc],
			ipa3_ctx->coal_cmd_pyld[WAN_COAL_SUB]);

		num_desc++;
	}

	if ( close_lan
		 &&
		 IPA_CLIENT_IS_MAPPED_VALID(IPA_CLIENT_APPS_LAN_COAL_CONS, ep_idx)
		 &&
		 ipa3_ctx->coal_cmd_pyld[LAN_COAL_SUB] ) {

		ipa3_init_imm_cmd_desc(
			&desc[num_desc],
			ipa3_ctx->coal_cmd_pyld[LAN_COAL_SUB]);

		num_desc++;
	}

	if (ipa3_ctx->ulso_wa && ipa3_ctx->coal_cmd_pyld[ULSO_COAL_SUB] ) {
		ipa3_init_imm_cmd_desc(
			&desc[num_desc],
			ipa3_ctx->coal_cmd_pyld[ULSO_COAL_SUB]);

		num_desc++;
	}

	if ( num_desc ) {
		IPADBG("Sending %d descriptor(s) for coal force close\n", num_desc);
		if ( ipa3_send_cmd_timeout(
				 num_desc,
				 desc,
				 IPA_COAL_CLOSE_FRAME_CMD_TIMEOUT_MSEC) ) {
			IPADBG("ipa3_send_cmd_timeout timedout\n");
		}
	}
}

int ipa3_suspend_apps_pipes(bool suspend)
{
	int res, i;

	if (suspend) {
		stop_coalescing();
		ipa3_force_close_coal(true, true);
	}

	/* As per HPG first need start/stop coalescing channel
	 * then default one. Coalescing client number was greater then
	 * default one so starting the last client.
	 */
	res = _ipa_suspend_resume_pipe(IPA_CLIENT_APPS_WAN_COAL_CONS, suspend);
	if (res == -EAGAIN) {
		if (suspend) start_coalescing();
		goto undo_coal_cons;
	}

	res = _ipa_suspend_resume_pipe(IPA_CLIENT_APPS_WAN_CONS, suspend);
	if (res == -EAGAIN) {
		if (suspend) start_coalescing();
		goto undo_wan_cons;
	}

	res = _ipa_suspend_resume_pipe(IPA_CLIENT_APPS_LAN_COAL_CONS, suspend);
	if (res == -EAGAIN) {
		if (suspend) start_coalescing();
		goto undo_lan_coal_cons;
	}

	res = _ipa_suspend_resume_pipe(IPA_CLIENT_APPS_LAN_CONS, suspend);
	if (res == -EAGAIN) {
		if (suspend) start_coalescing();
		goto undo_lan_cons;
	}

	if (suspend) start_coalescing();

	res = _ipa_suspend_resume_pipe(IPA_CLIENT_ODL_DPL_CONS, suspend);
	if (res == -EAGAIN) {
		goto undo_odl_cons;
	}

	res = _ipa_suspend_resume_pipe(IPA_CLIENT_APPS_WAN_LOW_LAT_CONS,
		suspend);
	if (res == -EAGAIN) {
		goto undo_qmap_cons;
	}

	res = _ipa_suspend_resume_pipe(IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS,
		suspend);
	if (res == -EAGAIN) {
		goto undo_low_lat_data_cons;
	}

	if (suspend) {
		struct ipahal_reg_tx_wrapper tx;
		int ep_idx;

		ep_idx = ipa_get_ep_mapping(IPA_CLIENT_APPS_WAN_COAL_CONS);
		if (ep_idx == IPA_EP_NOT_ALLOCATED ||
				(!ipa3_ctx->ep[ep_idx].valid))
			goto do_prod;

		ipahal_read_reg_fields(IPA_STATE_TX_WRAPPER, &tx);
		if (tx.coal_slave_open_frame != 0) {
			IPADBG("COAL frame is open 0x%x\n",
				tx.coal_slave_open_frame);
			res = -EAGAIN;
			goto undo_low_lat_data_cons;
		}

		usleep_range(IPA_TAG_SLEEP_MIN_USEC, IPA_TAG_SLEEP_MAX_USEC);

		if (ipa3_ctx->ipa_hw_type >= IPA_HW_v5_0) {
			for (i = 0; i < IPA_EP_ARR_SIZE; i++) {
				res = ipahal_read_reg_nk(
					IPA_SUSPEND_IRQ_INFO_EE_n_REG_k,
					ipa3_ctx->ee, i);
				if (res) {
					IPADBG("suspend irq is pending 0x%x\n",
						res);
					goto undo_low_lat_data_cons;
				}
			}
		} else {
			res = ipahal_read_reg_n(IPA_SUSPEND_IRQ_INFO_EE_n,
				ipa3_ctx->ee);
			if (res) {
				IPADBG("suspend irq is pending 0x%x\n", res);
				goto undo_qmap_cons;
			}
		}
	}
do_prod:
	res = _ipa_suspend_resume_pipe(IPA_CLIENT_APPS_LAN_PROD, suspend);
	if (res == -EAGAIN)
		goto undo_lan_prod;
	res = _ipa_suspend_resume_pipe(IPA_CLIENT_APPS_WAN_LOW_LAT_PROD,
		suspend);
	if (res == -EAGAIN)
		goto undo_qmap_prod;
	res = _ipa_suspend_resume_pipe(IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_PROD,
		suspend);
	if (res == -EAGAIN)
		goto undo_low_lat_data_prod;
	res = _ipa_suspend_resume_pipe(IPA_CLIENT_APPS_WAN_PROD, suspend);
	if (res == -EAGAIN)
		goto undo_wan_prod;
	return 0;

undo_wan_prod:
	_ipa_suspend_resume_pipe(IPA_CLIENT_APPS_WAN_PROD, !suspend);
undo_low_lat_data_prod:
	_ipa_suspend_resume_pipe(IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_PROD,
		!suspend);
undo_qmap_prod:
	_ipa_suspend_resume_pipe(IPA_CLIENT_APPS_WAN_LOW_LAT_PROD,
		!suspend);
undo_lan_prod:
	_ipa_suspend_resume_pipe(IPA_CLIENT_APPS_LAN_PROD, !suspend);
undo_low_lat_data_cons:
	_ipa_suspend_resume_pipe(IPA_CLIENT_APPS_WAN_LOW_LAT_DATA_CONS,
		!suspend);
undo_qmap_cons:
	_ipa_suspend_resume_pipe(IPA_CLIENT_APPS_WAN_LOW_LAT_CONS,
		!suspend);
undo_odl_cons:
	_ipa_suspend_resume_pipe(IPA_CLIENT_ODL_DPL_CONS, !suspend);
undo_lan_cons:
	_ipa_suspend_resume_pipe(IPA_CLIENT_APPS_LAN_CONS, !suspend);
undo_lan_coal_cons:
	_ipa_suspend_resume_pipe(IPA_CLIENT_APPS_LAN_COAL_CONS, !suspend);
undo_wan_cons:
	_ipa_suspend_resume_pipe(IPA_CLIENT_APPS_WAN_COAL_CONS, !suspend);
	_ipa_suspend_resume_pipe(IPA_CLIENT_APPS_WAN_CONS, !suspend);
	return res;

undo_coal_cons:
	_ipa_suspend_resume_pipe(IPA_CLIENT_APPS_WAN_COAL_CONS, !suspend);

	return res;
}

int ipa3_allocate_dma_task_for_gsi(void)
{
	struct ipahal_imm_cmd_dma_task_32b_addr cmd = { 0 };

	IPADBG("Allocate mem\n");
	ipa3_ctx->dma_task_info.mem.size = IPA_GSI_CHANNEL_STOP_PKT_SIZE;
	ipa3_ctx->dma_task_info.mem.base = dma_alloc_coherent(ipa3_ctx->pdev,
		ipa3_ctx->dma_task_info.mem.size,
		&ipa3_ctx->dma_task_info.mem.phys_base,
		GFP_KERNEL);
	if (!ipa3_ctx->dma_task_info.mem.base) {
		IPAERR("no mem\n");
		return -EFAULT;
	}

	cmd.flsh = true;
	cmd.size1 = ipa3_ctx->dma_task_info.mem.size;
	cmd.addr1 = ipa3_ctx->dma_task_info.mem.phys_base;
	cmd.packet_size = ipa3_ctx->dma_task_info.mem.size;
	ipa3_ctx->dma_task_info.cmd_pyld = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_DMA_TASK_32B_ADDR, &cmd, false);
	if (!ipa3_ctx->dma_task_info.cmd_pyld) {
		IPAERR("failed to construct dma_task_32b_addr cmd\n");
		dma_free_coherent(ipa3_ctx->pdev,
			ipa3_ctx->dma_task_info.mem.size,
			ipa3_ctx->dma_task_info.mem.base,
			ipa3_ctx->dma_task_info.mem.phys_base);
		memset(&ipa3_ctx->dma_task_info, 0,
			sizeof(ipa3_ctx->dma_task_info));
		return -EFAULT;
	}

	return 0;
}

void ipa3_free_dma_task_for_gsi(void)
{
	dma_free_coherent(ipa3_ctx->pdev,
		ipa3_ctx->dma_task_info.mem.size,
		ipa3_ctx->dma_task_info.mem.base,
		ipa3_ctx->dma_task_info.mem.phys_base);
	ipahal_destroy_imm_cmd(ipa3_ctx->dma_task_info.cmd_pyld);
	memset(&ipa3_ctx->dma_task_info, 0, sizeof(ipa3_ctx->dma_task_info));
}

int ipa3_allocate_coal_close_frame(void)
{
	struct ipahal_imm_cmd_register_write reg_write_cmd = { 0 };
	struct ipahal_imm_cmd_register_read dummy_reg_read = { 0 };
	struct ipahal_reg_valmask valmask;
	u32 offset = 0;
	int ep_idx, num_desc = 0;

	if ( IPA_CLIENT_IS_MAPPED(IPA_CLIENT_APPS_WAN_COAL_CONS, ep_idx) ) {

		IPADBG("Allocate wan coal close frame cmd\n");

		reg_write_cmd.skip_pipeline_clear = false;
		if (ipa3_ctx->ulso_wa) {
			reg_write_cmd.pipeline_clear_options = IPAHAL_SRC_GRP_CLEAR;
		} else {
			reg_write_cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
		}
		if (ipa3_ctx->ipa_hw_type < IPA_HW_v5_0)
			offset = ipahal_get_reg_ofst(
				IPA_AGGR_FORCE_CLOSE);
		else
			offset = ipahal_get_ep_reg_offset(
				IPA_AGGR_FORCE_CLOSE_n, ep_idx);
		reg_write_cmd.offset = offset;
		ipahal_get_aggr_force_close_valmask(ep_idx, &valmask);
		reg_write_cmd.value = valmask.val;
		reg_write_cmd.value_mask = valmask.mask;
		ipa3_ctx->coal_cmd_pyld[WAN_COAL_SUB] =
			ipahal_construct_imm_cmd(
				IPA_IMM_CMD_REGISTER_WRITE,
				&reg_write_cmd, false);
		if (!ipa3_ctx->coal_cmd_pyld[WAN_COAL_SUB]) {
			IPAERR("fail construct register_write imm cmd\n");
			ipa_assert();
			return 0;
		}
		num_desc++;
	}

	if ( IPA_CLIENT_IS_MAPPED(IPA_CLIENT_APPS_LAN_COAL_CONS, ep_idx) ) {

		IPADBG("Allocate lan coal close frame cmd\n");

		reg_write_cmd.skip_pipeline_clear = false;
		if (ipa3_ctx->ulso_wa) {
			reg_write_cmd.pipeline_clear_options = IPAHAL_SRC_GRP_CLEAR;
		} else {
			reg_write_cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
		}
		if (ipa3_ctx->ipa_hw_type < IPA_HW_v5_0)
			offset = ipahal_get_reg_ofst(
				IPA_AGGR_FORCE_CLOSE);
		else
			offset = ipahal_get_ep_reg_offset(
				IPA_AGGR_FORCE_CLOSE_n, ep_idx);
		reg_write_cmd.offset = offset;
		ipahal_get_aggr_force_close_valmask(ep_idx, &valmask);
		reg_write_cmd.value = valmask.val;
		reg_write_cmd.value_mask = valmask.mask;
		ipa3_ctx->coal_cmd_pyld[LAN_COAL_SUB] =
			ipahal_construct_imm_cmd(
				IPA_IMM_CMD_REGISTER_WRITE,
				&reg_write_cmd, false);
		if (!ipa3_ctx->coal_cmd_pyld[LAN_COAL_SUB]) {
			IPAERR("fail construct register_write imm cmd\n");
			ipa_assert();
			return 0;
		}
		num_desc++;
	}

	if ( ipa3_ctx->ulso_wa ) {
		/*
		 * Dummy regsiter read IC with HPS clear
		 */
		ipa3_ctx->ulso_wa_cmd.size = 4;
		ipa3_ctx->ulso_wa_cmd.base =
			dma_alloc_coherent(
				ipa3_ctx->pdev,
				ipa3_ctx->ulso_wa_cmd.size,
				&ipa3_ctx->ulso_wa_cmd.phys_base, GFP_KERNEL);
		if (ipa3_ctx->ulso_wa_cmd.base == NULL) {
			ipa_assert();
		}
		offset = ipahal_get_reg_n_ofst(
			IPA_STAT_QUOTA_BASE_n,
			ipa3_ctx->ee);
		dummy_reg_read.skip_pipeline_clear = false;
		dummy_reg_read.pipeline_clear_options = IPAHAL_HPS_CLEAR;
		dummy_reg_read.offset = offset;
		dummy_reg_read.sys_addr = ipa3_ctx->ulso_wa_cmd.phys_base;
		ipa3_ctx->coal_cmd_pyld[ULSO_COAL_SUB] =
			ipahal_construct_imm_cmd(
				IPA_IMM_CMD_REGISTER_READ,
				&dummy_reg_read, false);
		if (!ipa3_ctx->coal_cmd_pyld[ULSO_COAL_SUB]) {
			IPAERR("failed to construct DUMMY READ IC\n");
			ipa_assert();
		}
	}

	return 0;
}

void ipa3_free_coal_close_frame(void)
{
	if (ipa3_ctx->coal_cmd_pyld[WAN_COAL_SUB]) {
		ipahal_destroy_imm_cmd(ipa3_ctx->coal_cmd_pyld[WAN_COAL_SUB]);
	}

	if (ipa3_ctx->coal_cmd_pyld[LAN_COAL_SUB]) {
		ipahal_destroy_imm_cmd(ipa3_ctx->coal_cmd_pyld[LAN_COAL_SUB]);
	}

	if (ipa3_ctx->coal_cmd_pyld[ULSO_COAL_SUB]) {
		ipahal_destroy_imm_cmd(ipa3_ctx->coal_cmd_pyld[ULSO_COAL_SUB]);
	}

	if ( ipa3_ctx->ulso_wa_cmd.base ) {
		dma_free_coherent(
			ipa3_ctx->pdev,
			ipa3_ctx->ulso_wa_cmd.size,
			ipa3_ctx->ulso_wa_cmd.base,
			ipa3_ctx->ulso_wa_cmd.phys_base);
	}
}

/**
 * ipa3_inject_dma_task_for_gsi()- Send DMA_TASK to IPA for GSI stop channel
 *
 * Send a DMA_TASK of 1B to IPA to unblock GSI channel in STOP_IN_PROG.
 * Return value: 0 on success, negative otherwise
 */
int ipa3_inject_dma_task_for_gsi(void)
{
	struct ipa3_desc desc;

	ipa3_init_imm_cmd_desc(&desc, ipa3_ctx->dma_task_info.cmd_pyld);

	IPADBG("sending 1B packet to IPA\n");
	if (ipa3_send_cmd_timeout(1, &desc,
		IPA_DMA_TASK_FOR_GSI_TIMEOUT_MSEC)) {
		IPAERR("ipa3_send_cmd failed\n");
		return -EFAULT;
	}

	return 0;
}

static int ipa3_load_single_fw(const struct firmware *firmware,
	const struct elf32_phdr *phdr)
{
	uint32_t *fw_mem_base;
	int index;
	const uint32_t *elf_data_ptr;

	if (phdr->p_offset > firmware->size) {
		IPAERR("Invalid ELF: offset=%u is beyond elf_size=%zu\n",
			phdr->p_offset, firmware->size);
		return -EINVAL;
	}
	if ((firmware->size - phdr->p_offset) < phdr->p_filesz) {
		IPAERR("Invalid ELF: offset=%u filesz=%u elf_size=%zu\n",
			phdr->p_offset, phdr->p_filesz, firmware->size);
		return -EINVAL;
	}

	if (phdr->p_memsz % sizeof(uint32_t)) {
		IPAERR("FW mem size %u doesn't align to 32bit\n",
			phdr->p_memsz);
		return -EFAULT;
	}

	if (phdr->p_filesz > phdr->p_memsz) {
		IPAERR("FW image too big src_size=%u dst_size=%u\n",
			phdr->p_filesz, phdr->p_memsz);
		return -EFAULT;
	}

	fw_mem_base = ioremap(phdr->p_vaddr, phdr->p_memsz);
	if (!fw_mem_base) {
		IPAERR("Failed to map 0x%x for the size of %u\n",
			phdr->p_vaddr, phdr->p_memsz);
		return -ENOMEM;
	}

	/* Set the entire region to 0s */
	memset(fw_mem_base, 0, phdr->p_memsz);

	elf_data_ptr = (uint32_t *)(firmware->data + phdr->p_offset);

	/* Write the FW */
	for (index = 0; index < phdr->p_filesz/sizeof(uint32_t); index++) {
		writel_relaxed(*elf_data_ptr, &fw_mem_base[index]);
		elf_data_ptr++;
	}

	iounmap(fw_mem_base);

	return 0;
}

struct ipa3_hps_dps_areas_info {
	u32 dps_abs_addr;
	u32 dps_sz;
	u32 hps_abs_addr;
	u32 hps_sz;
};

static void ipa3_get_hps_dps_areas_absolute_addr_and_sz(
	struct ipa3_hps_dps_areas_info *info)
{
	u32 dps_area_start;
	u32 dps_area_end;
	u32 hps_area_start;
	u32 hps_area_end;

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_5) {
		dps_area_start = ipahal_get_reg_ofst(IPA_DPS_SEQUENCER_FIRST);
		dps_area_end = ipahal_get_reg_ofst(IPA_DPS_SEQUENCER_LAST);
		hps_area_start = ipahal_get_reg_ofst(IPA_HPS_SEQUENCER_FIRST);
		hps_area_end = ipahal_get_reg_ofst(IPA_HPS_SEQUENCER_LAST);

		info->dps_abs_addr = ipa3_ctx->ipa_wrapper_base +
			ipahal_get_reg_base() + dps_area_start;
		info->hps_abs_addr = ipa3_ctx->ipa_wrapper_base +
			ipahal_get_reg_base() + hps_area_start;
	} else {
		dps_area_start = ipahal_read_reg(IPA_DPS_SEQUENCER_FIRST);
		dps_area_end = ipahal_read_reg(IPA_DPS_SEQUENCER_LAST);
		hps_area_start = ipahal_read_reg(IPA_HPS_SEQUENCER_FIRST);
		hps_area_end = ipahal_read_reg(IPA_HPS_SEQUENCER_LAST);

		info->dps_abs_addr = ipa3_ctx->ipa_wrapper_base +
			dps_area_start;
		info->hps_abs_addr = ipa3_ctx->ipa_wrapper_base +
			hps_area_start;
	}

	info->dps_sz = dps_area_end - dps_area_start + sizeof(u32);
	info->hps_sz = hps_area_end - hps_area_start + sizeof(u32);

	IPADBG("dps area: start offset=0x%x end offset=0x%x\n",
		dps_area_start, dps_area_end);
	IPADBG("hps area: start offset=0x%x end offset=0x%x\n",
		hps_area_start, hps_area_end);
}

/**
 * emulator_load_single_fw() - load firmware into emulator's memory
 *
 * @firmware: Structure which contains the FW data from the user space.
 * @phdr: ELF program header
 * @loc_to_map: physical location to map into virtual space
 * @size_to_map: the size of memory to map into virtual space
 *
 * Return value: 0 on success, negative otherwise
 */
static int emulator_load_single_fw(
	const struct firmware   *firmware,
	const struct elf32_phdr *phdr,
	u32                      loc_to_map,
	u32                      size_to_map)
{
	int index;
	uint32_t ofb;
	const uint32_t *elf_data_ptr;
	void __iomem *fw_base;

	IPADBG("firmware(%pK) phdr(%pK) loc_to_map(0x%X) size_to_map(%u)\n",
	       firmware, phdr, loc_to_map, size_to_map);

	if (phdr->p_offset > firmware->size) {
		IPAERR("Invalid ELF: offset=%u is beyond elf_size=%zu\n",
			phdr->p_offset, firmware->size);
		return -EINVAL;
	}
	if ((firmware->size - phdr->p_offset) < phdr->p_filesz) {
		IPAERR("Invalid ELF: offset=%u filesz=%u elf_size=%zu\n",
			phdr->p_offset, phdr->p_filesz, firmware->size);
		return -EINVAL;
	}

	if (phdr->p_memsz % sizeof(uint32_t)) {
		IPAERR("FW mem size %u doesn't align to 32bit\n",
			phdr->p_memsz);
		return -EFAULT;
	}

	if (phdr->p_filesz > phdr->p_memsz) {
		IPAERR("FW image too big src_size=%u dst_size=%u\n",
			phdr->p_filesz, phdr->p_memsz);
		return -EFAULT;
	}

	IPADBG("ELF: p_memsz(0x%x) p_filesz(0x%x) p_filesz/4(0x%x)\n",
	       (uint32_t) phdr->p_memsz,
	       (uint32_t) phdr->p_filesz,
	       (uint32_t) (phdr->p_filesz/sizeof(uint32_t)));

	fw_base = ioremap(loc_to_map, size_to_map);
	if (!fw_base) {
		IPAERR("Failed to map 0x%X for the size of %u\n",
		       loc_to_map, size_to_map);
		return -ENOMEM;
	}

	IPADBG("Physical base(0x%X) mapped to virtual (%pK) with len (%u)\n",
	       loc_to_map,
	       fw_base,
	       size_to_map);

	/* Set the entire region to 0s */
	ofb = 0;
	for (index = 0; index < phdr->p_memsz/sizeof(uint32_t); index++) {
		writel_relaxed(0, fw_base + ofb);
		ofb += sizeof(uint32_t);
	}

	elf_data_ptr = (uint32_t *)(firmware->data + phdr->p_offset);

	/* Write the FW */
	ofb = 0;
	for (index = 0; index < phdr->p_filesz/sizeof(uint32_t); index++) {
		writel_relaxed(*elf_data_ptr, fw_base + ofb);
		elf_data_ptr++;
		ofb += sizeof(uint32_t);
	}

	iounmap(fw_base);

	return 0;
}

/**
 * ipa3_load_fws() - Load the IPAv3 FWs into IPA&GSI SRAM.
 *
 * @firmware: Structure which contains the FW data from the user space.
 * @gsi_mem_base: GSI base address
 * @gsi_ver: GSI Version
 *
 * Return value: 0 on success, negative otherwise
 *
 */
int ipa3_load_fws(const struct firmware *firmware, phys_addr_t gsi_mem_base,
	enum gsi_ver gsi_ver)
{
	const struct elf32_hdr *ehdr;
	const struct elf32_phdr *phdr;
	unsigned long gsi_iram_ofst;
	unsigned long gsi_iram_size;
	int rc;
	struct ipa3_hps_dps_areas_info dps_hps_info;

	if (gsi_ver == GSI_VER_ERR) {
		IPAERR("Invalid GSI Version\n");
		return -EINVAL;
	}

	if (!gsi_mem_base) {
		IPAERR("Invalid GSI base address\n");
		return -EINVAL;
	}

	ipa_assert_on(!firmware);
	/* One program header per FW image: GSI, DPS and HPS */
	if (firmware->size < (sizeof(*ehdr) + 3 * sizeof(*phdr))) {
		IPAERR("Missing ELF and Program headers firmware size=%zu\n",
			firmware->size);
		return -EINVAL;
	}

	ehdr = (struct elf32_hdr *) firmware->data;
	ipa_assert_on(!ehdr);
	if (ehdr->e_phnum != 3 && ehdr->e_phnum != 5) {
		IPAERR("Unexpected number of ELF program headers\n");
		return -EINVAL;
	}

	phdr = (struct elf32_phdr *)(firmware->data + sizeof(*ehdr));

	if (ehdr->e_phnum == 5)
		phdr = phdr + 2;
	/*
	 * Each ELF program header represents a FW image and contains:
	 *  p_vaddr : The starting address to which the FW needs to loaded.
	 *  p_memsz : The size of the IRAM (where the image loaded)
	 *  p_filesz: The size of the FW image embedded inside the ELF
	 *  p_offset: Absolute offset to the image from the head of the ELF
	 */

	/* Load GSI FW image */
	gsi_get_inst_ram_offset_and_size(&gsi_iram_ofst, &gsi_iram_size,
		gsi_ver);
	if (phdr->p_vaddr != (gsi_mem_base + gsi_iram_ofst)) {
		IPAERR(
			"Invalid GSI FW img load addr vaddr=0x%x gsi_mem_base=%pa gsi_iram_ofst=0x%lx\n"
			, phdr->p_vaddr, &gsi_mem_base, gsi_iram_ofst);
		return -EINVAL;
	}
	if (phdr->p_memsz > gsi_iram_size) {
		IPAERR("Invalid GSI FW img size memsz=%d gsi_iram_size=%lu\n",
			phdr->p_memsz, gsi_iram_size);
		return -EINVAL;
	}
	rc = ipa3_load_single_fw(firmware, phdr);
	if (rc)
		return rc;

	phdr++;
	ipa3_get_hps_dps_areas_absolute_addr_and_sz(&dps_hps_info);

	/* Load IPA DPS FW image */
	if (phdr->p_vaddr != dps_hps_info.dps_abs_addr) {
		IPAERR(
			"Invalid IPA DPS img load addr vaddr=0x%x dps_abs_addr=0x%x\n"
			, phdr->p_vaddr, dps_hps_info.dps_abs_addr);
		return -EINVAL;
	}
	if (phdr->p_memsz > dps_hps_info.dps_sz) {
		IPAERR("Invalid IPA DPS img size memsz=%d dps_area_size=%u\n",
			phdr->p_memsz, dps_hps_info.dps_sz);
		return -EINVAL;
	}
	rc = ipa3_load_single_fw(firmware, phdr);
	if (rc)
		return rc;

	phdr++;

	/* Load IPA HPS FW image */
	if (phdr->p_vaddr != dps_hps_info.hps_abs_addr) {
		IPAERR(
			"Invalid IPA HPS img load addr vaddr=0x%x hps_abs_addr=0x%x\n"
			, phdr->p_vaddr, dps_hps_info.hps_abs_addr);
		return -EINVAL;
	}
	if (phdr->p_memsz > dps_hps_info.hps_sz) {
		IPAERR("Invalid IPA HPS img size memsz=%d hps_area_size=%u\n",
			phdr->p_memsz, dps_hps_info.hps_sz);
		return -EINVAL;
	}
	rc = ipa3_load_single_fw(firmware, phdr);
	if (rc)
		return rc;

	IPADBG("IPA FWs (GSI FW, DPS and HPS) loaded successfully\n");
	return 0;
}

/*
 * The following needed for the EMULATION system. On a non-emulation
 * system (ie. the real UE), this functionality is done in the
 * TZ...
 */

static void ipa_gsi_setup_reg(void)
{
	u32 reg_val, start;
	int i;
	const struct ipa_gsi_ep_config *gsi_ep_info_cfg;
	enum ipa_client_type type;

	IPADBG("Setting up registers in preparation for firmware download\n");

	/* setup IPA_ENDP_GSI_CFG_TLV_n reg */
	start = 0;
	ipa3_ctx->ipa_num_pipes = ipa3_get_num_pipes();
	IPADBG("ipa_num_pipes=%u\n", ipa3_ctx->ipa_num_pipes);

	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		type = ipa3_get_client_by_pipe(i);
		gsi_ep_info_cfg = ipa_get_gsi_ep_info(type);
		IPAERR("for ep %d client is %d gsi_ep_info_cfg=%pK\n",
			i, type, gsi_ep_info_cfg);
		if (!gsi_ep_info_cfg)
			continue;
		reg_val = ((gsi_ep_info_cfg->ipa_if_tlv << 16) & 0x00FF0000);
		reg_val += (start & 0xFFFF);
		start += gsi_ep_info_cfg->ipa_if_tlv;
		ipahal_write_reg_n(IPA_ENDP_GSI_CFG_TLV_n, i, reg_val);
	}

	/* setup IPA_ENDP_GSI_CFG_AOS_n reg */
	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		type = ipa3_get_client_by_pipe(i);
		gsi_ep_info_cfg = ipa_get_gsi_ep_info(type);
		if (!gsi_ep_info_cfg)
			continue;
		reg_val = ((gsi_ep_info_cfg->ipa_if_aos << 16) & 0x00FF0000);
		reg_val += (start & 0xFFFF);
		start += gsi_ep_info_cfg->ipa_if_aos;
		ipahal_write_reg_n(IPA_ENDP_GSI_CFG_AOS_n, i, reg_val);
	}

	/* setup GSI_MAP_EE_n_CH_k_VP_TABLE reg */
	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		type = ipa3_get_client_by_pipe(i);
		gsi_ep_info_cfg = ipa_get_gsi_ep_info(type);
		if (!gsi_ep_info_cfg)
			continue;
		reg_val = i & 0xFF;
		gsi_map_virtual_ch_to_per_ep(
			gsi_ep_info_cfg->ee,
			gsi_ep_info_cfg->ipa_gsi_chan_num,
			reg_val);
	}

	/* setup IPA_ENDP_GSI_CFG1_n reg */
	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		type = ipa3_get_client_by_pipe(i);
		gsi_ep_info_cfg = ipa_get_gsi_ep_info(type);
		if (!gsi_ep_info_cfg)
			continue;
		reg_val = (1 << 31) + (1 << 16);
		ipahal_write_reg_n(IPA_ENDP_GSI_CFG1_n, i, 1<<16);
		ipahal_write_reg_n(IPA_ENDP_GSI_CFG1_n, i, reg_val);
		ipahal_write_reg_n(IPA_ENDP_GSI_CFG1_n, i, 1<<16);
	}
}

/**
 * emulator_load_fws() - Load the IPAv3 FWs into IPA&GSI SRAM.
 *
 * @firmware: Structure which contains the FW data from the user space.
 * @transport_mem_base: Where to load
 * @transport_mem_size: Space available to load into
 * @gsi_ver: Version of the gsi
 *
 * Return value: 0 on success, negative otherwise
 */
int emulator_load_fws(
	const struct firmware *firmware,
	u32 transport_mem_base,
	u32 transport_mem_size,
	enum gsi_ver gsi_ver)
{
	const struct elf32_hdr *ehdr;
	const struct elf32_phdr *phdr;
	unsigned long gsi_offset, gsi_ram_size;
	struct ipa3_hps_dps_areas_info dps_hps_info;
	int rc;

	IPADBG("Loading firmware(%pK)\n", firmware);

	if (!firmware) {
		IPAERR("firmware pointer passed to function is NULL\n");
		return -EINVAL;
	}

	/* One program header per FW image: GSI, DPS and HPS */
	if (firmware->size < (sizeof(*ehdr) + 3 * sizeof(*phdr))) {
		IPAERR(
		    "Missing ELF and Program headers firmware size=%zu\n",
		    firmware->size);
		return -EINVAL;
	}

	ehdr = (struct elf32_hdr *) firmware->data;

	ipa_assert_on(!ehdr);

	if (ehdr->e_phnum != 3) {
		IPAERR("Unexpected number of ELF program headers\n");
		return -EINVAL;
	}

	ipa3_get_hps_dps_areas_absolute_addr_and_sz(&dps_hps_info);

	/*
	 * Each ELF program header represents a FW image and contains:
	 *  p_vaddr : The starting address to which the FW needs to loaded.
	 *  p_memsz : The size of the IRAM (where the image loaded)
	 *  p_filesz: The size of the FW image embedded inside the ELF
	 *  p_offset: Absolute offset to the image from the head of the ELF
	 *
	 * NOTE WELL: On the emulation platform, the p_vaddr address
	 *            is not relevant and is unused.  This is because
	 *            on the emulation platform, the registers'
	 *            address location is mutable, since it's mapped
	 *            in via a PCIe probe.  Given this, it is the
	 *            mapped address info that's used while p_vaddr is
	 *            ignored.
	 */
	phdr = (struct elf32_phdr *)(firmware->data + sizeof(*ehdr));

	phdr += 2;

	/*
	 * Attempt to load IPA HPS FW image
	 */
	if (phdr->p_memsz > dps_hps_info.hps_sz) {
		IPAERR("Invalid IPA HPS img size memsz=%d hps_size=%u\n",
		       phdr->p_memsz, dps_hps_info.hps_sz);
		return -EINVAL;
	}
	IPADBG("Loading HPS FW\n");
	rc = emulator_load_single_fw(
		firmware, phdr,
		dps_hps_info.hps_abs_addr, dps_hps_info.hps_sz);
	if (rc)
		return rc;
	IPADBG("Loading HPS FW complete\n");

	--phdr;

	/*
	 * Attempt to load IPA DPS FW image
	 */
	if (phdr->p_memsz > dps_hps_info.dps_sz) {
		IPAERR("Invalid IPA DPS img size memsz=%d dps_size=%u\n",
		       phdr->p_memsz, dps_hps_info.dps_sz);
		return -EINVAL;
	}
	IPADBG("Loading DPS FW\n");
	rc = emulator_load_single_fw(
		firmware, phdr,
		dps_hps_info.dps_abs_addr, dps_hps_info.dps_sz);
	if (rc)
		return rc;
	IPADBG("Loading DPS FW complete\n");

	/*
	 * Run gsi register setup which is normally done in TZ on
	 * non-EMULATION systems...
	 */
	ipa_gsi_setup_reg();

	--phdr;

	gsi_get_inst_ram_offset_and_size(&gsi_offset, &gsi_ram_size, gsi_ver);

	/*
	 * Attempt to load GSI FW image
	 */
	if (phdr->p_memsz > gsi_ram_size) {
		IPAERR(
		    "Invalid GSI FW img size memsz=%d gsi_ram_size=%lu\n",
		    phdr->p_memsz, gsi_ram_size);
		return -EINVAL;
	}
	IPADBG("Loading GSI FW\n");
	rc = emulator_load_single_fw(
		firmware, phdr,
		transport_mem_base + (u32) gsi_offset, gsi_ram_size);
	if (rc)
		return rc;
	IPADBG("Loading GSI FW complete\n");

	IPADBG("IPA FWs (GSI FW, DPS and HPS) loaded successfully\n");

	return 0;
}

/**
 * ipa3_is_msm_device() - Is the running device a MSM or MDM
 * Determine according to IPA version
 *
 * Return value: true if MSM, false if MDM
 *
 */
bool ipa3_is_msm_device(void)
{
	switch (ipa3_ctx->ipa_hw_type){
	case IPA_HW_v3_0:
	case IPA_HW_v3_5:
	case IPA_HW_v4_0:
	case IPA_HW_v4_5:
	case IPA_HW_v5_0:
		return false;
	case IPA_HW_v3_1:
	case IPA_HW_v3_5_1:
	case IPA_HW_v4_1:
	case IPA_HW_v4_2:
	case IPA_HW_v4_7:
	case IPA_HW_v4_9:
	case IPA_HW_v4_11:
	case IPA_HW_v5_1:
	case IPA_HW_v5_2:
	case IPA_HW_v5_5:
		return true;
	default:
		IPAERR("unknown HW type %d\n", ipa3_ctx->ipa_hw_type);
		ipa_assert();
	}

	return false;
}

/**
 * ipa3_is_apq() - indicate apq platform or not
 *
 * Return value: true if apq, false if not apq platform
 *
 */
bool ipa3_is_apq(void)
{
	if (ipa3_ctx->platform_type == IPA_PLAT_TYPE_APQ)
		return true;
	else
		return false;
}

/**
 * ipa_get_fnr_info() - get fnr_info
 *
 * Return value: true if set, false if not set
 *
 */
bool ipa_get_fnr_info(struct ipacm_fnr_info *fnr_info)
{
	bool res = false;

	if (ipa3_ctx->fnr_info.valid) {
		fnr_info->valid = ipa3_ctx->fnr_info.valid;
		fnr_info->hw_counter_offset =
			ipa3_ctx->fnr_info.hw_counter_offset;
		fnr_info->sw_counter_offset =
			ipa3_ctx->fnr_info.sw_counter_offset;
		res = true;
	} else {
		IPAERR("fnr_info not valid!\n");
		res = false;
	}
	return res;
}

/**
 * ipa3_disable_prefetch() - disable\enable tx prefetch
 *
 * @client: the client which is related to the TX where prefetch will be
 *          disabled
 *
 * Return value: Non applicable
 *
 */
void ipa3_disable_prefetch(enum ipa_client_type client)
{
	struct ipahal_reg_tx_cfg cfg;
	u8 qmb;

	qmb = ipa3_get_qmb_master_sel(client);

	IPADBG("disabling prefetch for qmb %d\n", (int)qmb);

	ipahal_read_reg_fields(IPA_TX_CFG, &cfg);
	/* QMB0 (DDR) correlates with TX0, QMB1(PCIE) correlates with TX1 */
	if (qmb == QMB_MASTER_SELECT_DDR)
		cfg.tx0_prefetch_disable = true;
	else
		cfg.tx1_prefetch_disable = true;
	ipahal_write_reg_fields(IPA_TX_CFG, &cfg);
}

/**
 * ipa3_get_pdev() - return a pointer to IPA dev struct
 *
 * Return value: a pointer to IPA dev struct
 *
 */
struct device *ipa3_get_pdev(void)
{
	if (!ipa3_ctx)
		return NULL;

	return ipa3_ctx->pdev;
}
EXPORT_SYMBOL(ipa3_get_pdev);

/**
 * ipa3_enable_dcd() - enable dynamic clock division on IPA
 *
 * Return value: Non applicable
 *
 */
void ipa3_enable_dcd(void)
{
	struct ipahal_reg_idle_indication_cfg idle_indication_cfg;

	/* recommended values for IPA 3.5 according to IPA HPG */
	idle_indication_cfg.const_non_idle_enable = false;
	idle_indication_cfg.enter_idle_debounce_thresh = 256;

	ipahal_write_reg_fields(IPA_IDLE_INDICATION_CFG,
			&idle_indication_cfg);
}

void ipa3_init_imm_cmd_desc(struct ipa3_desc *desc,
	struct ipahal_imm_cmd_pyld *cmd_pyld)
{
	memset(desc, 0, sizeof(*desc));
	desc->opcode = cmd_pyld->opcode;
	desc->pyld = cmd_pyld->data;
	desc->len = cmd_pyld->len;
	desc->type = IPA_IMM_CMD_DESC;
}

u32 ipa3_get_r_rev_version(void)
{
	static u32 r_rev;

	if (r_rev != 0)
		return r_rev;

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	r_rev = ipahal_read_reg(IPA_VERSION);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return r_rev;
}
EXPORT_SYMBOL(ipa3_get_r_rev_version);

/**
 * ipa3_ctx_get_type() - to get platform type, hw type
 * and hw mode
 *
 * Return value: enumerated types of platform and ipa hw
 *
 */
int ipa3_ctx_get_type(enum ipa_type_mode type)
{
	switch (type) {
	case IPA_HW_TYPE:
		return ipa3_ctx->ipa_hw_type;
	case PLATFORM_TYPE:
		return ipa3_ctx->platform_type;
	case IPA3_HW_MODE:
		return ipa3_ctx->ipa3_hw_mode;
	default:
		IPAERR("cannot read ipa3_ctx types\n");
		return 0;
	}
}

/**
 * ipa3_get_gsi_stats() - Query gsi stats from uc
 * @prot_id: IPA_HW_FEATURE_OFFLOAD protocol id
 * @stats:	[inout] stats blob from client populated by driver
 *
 * @note Cannot be called from atomic context
 *
 */
void ipa3_get_gsi_stats(int prot_id,
	struct ipa_uc_dbg_ring_stats *stats)
{
	switch (prot_id) {
	case IPA_HW_PROTOCOL_AQC:
		stats->num_ch = MAX_AQC_CHANNELS;
		ipa3_get_aqc_gsi_stats(stats);
		break;
	case IPA_HW_PROTOCOL_RTK:
		stats->num_ch = MAX_RTK_CHANNELS;
		ipa3_get_rtk_gsi_stats(stats);
		break;
	case IPA_HW_PROTOCOL_11ad:
		break;
	case IPA_HW_PROTOCOL_WDI:
		stats->num_ch = MAX_WDI2_CHANNELS;
		ipa3_get_wdi_gsi_stats(stats);
		break;
	case IPA_HW_PROTOCOL_WDI3:
		stats->num_ch = MAX_WDI3_CHANNELS;
		ipa3_get_wdi3_gsi_stats(stats);
		break;
	case IPA_HW_PROTOCOL_NTN3:
		stats->num_ch = MAX_NTN_CHANNELS;
		ipa3_get_ntn_gsi_stats(stats);
		break;
	case IPA_HW_PROTOCOL_MHIP:
		stats->num_ch = MAX_MHIP_CHANNELS;
		ipa3_get_mhip_gsi_stats(stats);
		break;
	case IPA_HW_PROTOCOL_USB:
		stats->num_ch = MAX_USB_CHANNELS;
		ipa3_get_usb_gsi_stats(stats);
		break;
	default:
		IPAERR("unsupported HW feature %d\n", prot_id);
	}
}

/**
 * ipa3_ctx_get_flag() - to read some ipa3_ctx_flags
 *
 * Return value: true/false based on read value
 *
 */
bool ipa3_ctx_get_flag(enum ipa_flag flag)
{
	switch (flag) {
	case IPA_ENDP_DELAY_WA_EN:
		return ipa3_ctx->ipa_endp_delay_wa;
	case IPA_HW_STATS_EN:
		return (ipa3_ctx->hw_stats && ipa3_ctx->hw_stats->enabled);
	case IPA_MHI_EN:
		return ipa3_ctx->ipa_config_is_mhi;
	case IPA_FLTRT_NOT_HASHABLE_EN:
		return ipa3_ctx->ipa_fltrt_not_hashable;
	default:
		IPAERR("cannot read ipa3_ctx flags\n");
		return false;
	}
}

/**
 * ipa3_ctx_get_num_pipes() - to read pipe number from ipa3_ctx
 *
 * Return value: unsigned number
 *
 */
u32 ipa3_ctx_get_num_pipes(void)
{
	return ipa3_ctx->ipa_num_pipes;
}

int ipa3_app_clk_vote(
	enum ipa_app_clock_vote_type vote_type)
{
	const char *str_ptr = "APP_VOTE";
	int ret = 0;

	IPADBG("In\n");

	mutex_lock(&ipa3_ctx->app_clock_vote.mutex);

	switch (vote_type) {
	case IPA_APP_CLK_VOTE:
		if ((ipa3_ctx->app_clock_vote.cnt + 1) <= IPA_APP_VOTE_MAX) {
			ipa3_ctx->app_clock_vote.cnt++;
			IPA_ACTIVE_CLIENTS_INC_SPECIAL(str_ptr);
		} else {
			IPAERR_RL("App vote count max hit\n");
			ret = -EPERM;
			break;
		}
		break;
	case IPA_APP_CLK_DEVOTE:
		if (ipa3_ctx->app_clock_vote.cnt) {
			ipa3_ctx->app_clock_vote.cnt--;
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL(str_ptr);
		}
		break;
	case IPA_APP_CLK_RESET_VOTE:
		while (ipa3_ctx->app_clock_vote.cnt > 0) {
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL(str_ptr);
			ipa3_ctx->app_clock_vote.cnt--;
		}
		break;
	default:
		IPAERR_RL("Unknown vote_type(%u)\n", vote_type);
		ret = -EPERM;
		break;
	}

	mutex_unlock(&ipa3_ctx->app_clock_vote.mutex);

	IPADBG("Out\n");

	return ret;
}

/*
 * ipa3_get_prot_id() - Query gsi protocol id
 * @client: ipa_client_type
 *
 * return the prot_id based on the client type,
 * return -EINVAL when no such mapping exists.
 */
int ipa3_get_prot_id(enum ipa_client_type client)
{
	int prot_id = -EINVAL;

	switch (client) {
	case IPA_CLIENT_AQC_ETHERNET_CONS:
	case IPA_CLIENT_AQC_ETHERNET_PROD:
		prot_id = IPA_HW_PROTOCOL_AQC;
		break;
	case IPA_CLIENT_RTK_ETHERNET_CONS:
	case IPA_CLIENT_RTK_ETHERNET_PROD:
		prot_id = IPA_HW_PROTOCOL_RTK;
		break;
	case IPA_CLIENT_MHI_PRIME_TETH_PROD:
	case IPA_CLIENT_MHI_PRIME_TETH_CONS:
	case IPA_CLIENT_MHI_PRIME_RMNET_PROD:
	case IPA_CLIENT_MHI_PRIME_RMNET_CONS:
		prot_id = IPA_HW_PROTOCOL_MHIP;
		break;
	case IPA_CLIENT_WLAN1_PROD:
	case IPA_CLIENT_WLAN1_CONS:
		prot_id = IPA_HW_PROTOCOL_WDI;
		break;
	case IPA_CLIENT_WLAN2_PROD:
	case IPA_CLIENT_WLAN2_CONS:
	case IPA_CLIENT_WLAN2_CONS1:
		prot_id = IPA_HW_PROTOCOL_WDI3;
		break;
	case IPA_CLIENT_USB_PROD:
	case IPA_CLIENT_USB_CONS:
		prot_id = IPA_HW_PROTOCOL_USB;
		break;
	case IPA_CLIENT_ETHERNET2_PROD:
	case IPA_CLIENT_ETHERNET2_CONS:
	case IPA_CLIENT_ETHERNET_PROD:
	case IPA_CLIENT_ETHERNET_CONS:
		prot_id = IPA_HW_PROTOCOL_ETH;
		break;
	case IPA_CLIENT_WIGIG_PROD:
	case IPA_CLIENT_WIGIG1_CONS:
	case IPA_CLIENT_WIGIG2_CONS:
	case IPA_CLIENT_WIGIG3_CONS:
	case IPA_CLIENT_WIGIG4_CONS:
		prot_id = IPA_HW_PROTOCOL_11ad;
		break;
	default:
		IPAERR("unknown prot_id for client %d\n",
			client);
	}

	return prot_id;
}

void __ipa_ntn3_prod_stats_get(struct ipa_ntn3_stats_rx *stats, enum ipa_client_type client)
{
	int ch_id, ipa_ep_idx;

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	ipa_ep_idx = ipa_get_ep_mapping(client);
	if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED)
		return;
	ch_id = ipa3_ctx->ep[ipa_ep_idx].gsi_chan_hdl;

	stats->pending_db_after_rollback = gsi_ntn3_client_stats_get(ipa_ep_idx, 4, ch_id);
	stats->msi_db_idx = gsi_ntn3_client_stats_get(ipa_ep_idx, 5, ch_id);
	stats->chain_cnt = gsi_ntn3_client_stats_get(ipa_ep_idx, 6, ch_id);
	stats->err_cnt = gsi_ntn3_client_stats_get(ipa_ep_idx, 7, ch_id);
	stats->tres_handled = gsi_ntn3_client_stats_get(ipa_ep_idx, 8, ch_id);
	stats->rollbacks_cnt = gsi_ntn3_client_stats_get(ipa_ep_idx, 9, ch_id);
	stats->msi_db_cnt = gsi_ntn3_client_stats_get(ipa_ep_idx, -1, ch_id);

	stats->wp = gsi_get_refetch_reg(ch_id, false);
	stats->rp = gsi_get_refetch_reg(ch_id, true);

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

}

void __ipa_ntn3_cons_stats_get(struct ipa_ntn3_stats_tx *stats, enum ipa_client_type client)
{
	int ch_id, ipa_ep_idx;

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	ipa_ep_idx = ipa_get_ep_mapping(client);
	if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED)
		return;
	ch_id = ipa3_ctx->ep[ipa_ep_idx].gsi_chan_hdl;

	stats->pending_db_after_rollback = gsi_ntn3_client_stats_get(ipa_ep_idx, 4, ch_id);
	stats->msi_db_idx = gsi_ntn3_client_stats_get(ipa_ep_idx, 5, ch_id);
	stats->derr_cnt = gsi_ntn3_client_stats_get(ipa_ep_idx, 6, ch_id);
	stats->oob_cnt = gsi_ntn3_client_stats_get(ipa_ep_idx, 7, ch_id);
	stats->tres_handled = gsi_ntn3_client_stats_get(ipa_ep_idx, 8, ch_id);
	stats->rollbacks_cnt = gsi_ntn3_client_stats_get(ipa_ep_idx, 9, ch_id);
	stats->msi_db_cnt = gsi_ntn3_client_stats_get(ipa_ep_idx, -1, ch_id);

	stats->wp = gsi_get_refetch_reg(ch_id, false);
	stats->rp = gsi_get_refetch_reg(ch_id, true);

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

}

void ipa_eth_ntn3_get_status(struct ipa_ntn3_client_stats *s, unsigned inst_id)
{
	if (inst_id == 0) {
		__ipa_ntn3_cons_stats_get(&s->tx_stats, IPA_CLIENT_ETHERNET_CONS);
		__ipa_ntn3_prod_stats_get(&s->rx_stats, IPA_CLIENT_ETHERNET_PROD);
	} else {
		__ipa_ntn3_cons_stats_get(&s->tx_stats, IPA_CLIENT_ETHERNET2_CONS);
		__ipa_ntn3_prod_stats_get(&s->rx_stats, IPA_CLIENT_ETHERNET2_PROD);
	}

}

void ipa3_eth_get_status(u32 client, int scratch_id,
	struct ipa3_eth_error_stats *stats)
{
#define RTK_GSI_SCRATCH_ID 5
#define AQC_GSI_SCRATCH_ID 7
#define NTN_GSI_SCRATCH_ID 6

	int ch_id;
	int ipa_ep_idx;

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	ipa_ep_idx = ipa_get_ep_mapping(client);
	if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED)
		return;
	ch_id = ipa3_ctx->ep[ipa_ep_idx].gsi_chan_hdl;

	/*
	 * drop stats sometimes exist for RX and sometimes for Tx,
	 * wp sometimes acquired from ch_cntxt_6 and sometimes from refetch,
	 * depending on protocol.
	 */
	stats->err = 0;
	switch (client) {
	case IPA_CLIENT_RTK_ETHERNET_PROD:
		stats->err = gsi_get_drop_stats(ipa_ep_idx, RTK_GSI_SCRATCH_ID,
			ch_id);
		fallthrough;
	case IPA_CLIENT_RTK_ETHERNET_CONS:
		stats->wp = gsi_get_refetch_reg(ch_id, false);
		stats->rp = gsi_get_refetch_reg(ch_id, true);
		break;

	case IPA_CLIENT_AQC_ETHERNET_PROD:
		stats->err = gsi_get_drop_stats(ipa_ep_idx, AQC_GSI_SCRATCH_ID,
			ch_id);
		stats->wp = gsi_get_wp(ch_id);
		stats->rp = gsi_get_refetch_reg(ch_id, true);
		break;
	case IPA_CLIENT_AQC_ETHERNET_CONS:
		stats->wp = gsi_get_refetch_reg(ch_id, false);
		stats->rp = gsi_get_refetch_reg(ch_id, true);
		break;
	case IPA_CLIENT_ETHERNET_PROD:
		stats->wp = gsi_get_refetch_reg(ch_id, false);
		stats->rp = gsi_get_refetch_reg(ch_id, true);
		break;
	case IPA_CLIENT_ETHERNET_CONS:
		stats->err = gsi_get_drop_stats(ipa_ep_idx, NTN_GSI_SCRATCH_ID,
			ch_id);
		stats->wp = gsi_get_refetch_reg(ch_id, false);
		stats->rp = gsi_get_refetch_reg(ch_id, true);
		break;
	}
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
}

/**
 * ipa3_get_max_pdn() - get max PDN number based on hardware version
 * Returns:     IPA_MAX_PDN_NUM of IPAv4_5 and IPA_MAX_PDN_NUM_v4 for others
 *
 */

int ipa3_get_max_pdn(void)
{
	size_t pdn_entry_size;
	int max_pdn;

	ipahal_nat_entry_size(IPAHAL_NAT_IPV4_PDN, &pdn_entry_size);
	max_pdn = IPA_MEM_PART(pdn_config_size)/pdn_entry_size;
	IPADBG("IPA offload max_pdn = %d\n", max_pdn);

	return max_pdn;
}

bool ipa3_is_modem_up(void)
{
	bool is_up;

	mutex_lock(&ipa3_ctx->ssr_lock);
	is_up = ipa3_ctx->is_modem_up;
	mutex_unlock(&ipa3_ctx->ssr_lock);
	return is_up;
}

void ipa3_set_modem_up(bool is_up)
{
	mutex_lock(&ipa3_ctx->ssr_lock);
	ipa3_ctx->is_modem_up = is_up;
	mutex_unlock(&ipa3_ctx->ssr_lock);
}

/**
 * ipa3_is_ulso_supported() - Query IPA for ulso support
 *
 * Return value: true if ulso is supported, false otherwise
 *
 */
bool ipa3_is_ulso_supported(void)
{
	if (!ipa3_ctx)
		return false;

	return ipa3_ctx->ulso_supported;
}
EXPORT_SYMBOL(ipa3_is_ulso_supported);



/**
 * ipa_hdrs_hpc_destroy() - remove the IPA headers hpc
 * configuration done for the driver data path.
 *  @hdr_hdl: the hpc handle
 *
 *  Remove the header addition hpc associated with hdr_hdl.
 *
 *  Return value: 0 on success, kernel error code otherwise
 */
int ipa_hdrs_hpc_destroy(u32 hdr_hdl)
{
	struct ipa_ioc_del_hdr *del_wrapper;
	struct ipa_hdr_del *hdr_del;
	int result;

	del_wrapper = kzalloc(sizeof(*del_wrapper) + sizeof(*hdr_del), GFP_KERNEL);
	if (!del_wrapper)
		return -ENOMEM;

	del_wrapper->commit = 1;
	del_wrapper->num_hdls = 1;
	hdr_del = &del_wrapper->hdl[0];
	hdr_del->hdl = hdr_hdl;

	result = ipa3_del_hdr_hpc(del_wrapper);
	if (result || hdr_del->status)
		IPAERR("ipa_del_hdr failed\n");
	kfree(del_wrapper);

    return result;
}
EXPORT_SYMBOL(ipa_hdrs_hpc_destroy);

/**
 * qmap_encapsulate_skb() - encapsulate a given skb with a QMAP
 * header
 * @skb: the packet that will be encapsulated with QMAP header
 *
 * Return value: sk_buff encapsulated by a qmap header on
 * success, Null otherwise.
 */
struct sk_buff* qmap_encapsulate_skb(struct sk_buff *skb, const struct qmap_hdr *qh)
{
	struct qmap_hdr *qh_ptr;

	if (unlikely(!qh))
		return NULL;

	/* if there is no room in this skb, allocate a new one */
	if (unlikely(skb_headroom(skb) < sizeof(*qh))) {
		struct sk_buff *new_skb = skb_copy_expand(skb, sizeof(*qh), 0, GFP_ATOMIC);

		if (!new_skb) {
			IPAERR("no memory for skb expand\n");
			return skb;
		}
		IPADBG("skb expanded. old %pK new %pK\n", skb, new_skb);
		dev_kfree_skb_any(skb);
		skb = new_skb;
	}

	/* make room at the head of the SKB to put the QMAP header */
	qh_ptr = (struct qmap_hdr *)skb_push(skb, sizeof(*qh));
	*qh_ptr = *qh;
	qh_ptr->packet_len_with_pad = htons(skb->len);

	return skb;
}
EXPORT_SYMBOL(qmap_encapsulate_skb);

static void ipa3_eogre_info_free_cb(
	void *buff,
	u32   len,
	u32   type)
{
	if (buff) {
		kfree(buff);
	}
}

/**
 * ipa3_check_eogre() - Check if the eogre is worthy of sending to
 *                      recipients who would use the data.
 *
 * Returns: 0 on success, negative on failure
 */
int ipa3_check_eogre(
	struct ipa_ioc_eogre_info *eogre_info,
	bool                      *send2uC,
	bool                      *send2ipacm )
{
	struct ipa_ioc_eogre_info null_eogre;

	bool cache_is_null, eogre_is_null, same;

	int ret = 0;

	if (eogre_info == NULL || send2uC == NULL || send2ipacm == NULL) {
		IPAERR("NULL ptr: eogre_info(%pK) and/or "
			   "send2uC(%pK) and/or send2ipacm(%pK)\n",
			   eogre_info, send2uC, send2ipacm);
		ret = -EIO;
		goto done;
	}

	memset(&null_eogre, 0, sizeof(null_eogre));

	cache_is_null =
		!memcmp(
			&ipa3_ctx->eogre_cache,
			&null_eogre,
			sizeof(null_eogre));

	eogre_is_null =
		!memcmp(
			eogre_info,
			&null_eogre,
			sizeof(null_eogre));

	*send2uC = *send2ipacm = false;

	if (cache_is_null) {

		if (eogre_is_null) {
			IPAERR(
				"Attempting to disable EoGRE. EoGRE is "
				"already disabled. No work needs to be done.\n");
			ret = -EIO;
			goto done;
		}

		*send2uC = *send2ipacm = true;

	} else { /* (!cache_is_null) */

		if (!eogre_is_null) {
			IPAERR(
				"EoGRE is already enabled for iptype(%d). "
				"No work needs to be done.\n",
				ipa3_ctx->eogre_cache.ipgre_info.iptype);
			ret = -EIO;
			goto done;
		}

		same = !memcmp(
			&ipa3_ctx->eogre_cache.map_info,
			&eogre_info->map_info,
			sizeof(struct IpaDscpVlanPcpMap_t));

		*send2uC = !same;

		same = !memcmp(
			&ipa3_ctx->eogre_cache.ipgre_info,
			&eogre_info->ipgre_info,
			sizeof(struct ipa_ipgre_info));

		*send2ipacm = !same;
	}

	ipa3_ctx->eogre_cache = *eogre_info;

	IPADBG("send2uC(%u) send2ipacm(%u)\n",
		   *send2uC, *send2ipacm);

done:
	return ret;
}

/**
 * ipa3_send_eogre_info() - Notify ipacm of incoming eogre event
 *
 * Returns:	0 on success, negative on failure
 *
 * Note: Should not be called from atomic context
 */
int ipa3_send_eogre_info(
	enum ipa_eogre_event       etype,
	struct ipa_ioc_eogre_info *info )
{
	struct ipa_msg_meta    msg_meta;
	struct ipa_ipgre_info *eogre_info;

	int                    res = 0;

	if (!info) {
		IPAERR("Bad arg: info is NULL\n");
		res = -EIO;
		goto done;
	}

	/*
	 * Prep and send msg to ipacm
	 */
	memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));

	eogre_info = kzalloc(
		sizeof(struct ipa_ipgre_info), GFP_KERNEL);

	if (!eogre_info) {
		IPAERR("eogre_info memory allocation failed !\n");
		res = -ENOMEM;
		goto done;
	}

	memcpy(eogre_info,
		   &(info->ipgre_info),
		   sizeof(struct ipa_ipgre_info));

	msg_meta.msg_type = etype;
	msg_meta.msg_len  = sizeof(struct ipa_ipgre_info);

	/*
	 * Post event to ipacm
	 */
	res = ipa_send_msg(&msg_meta, eogre_info, ipa3_eogre_info_free_cb);

	if (res) {
		IPAERR_RL("ipa_send_msg failed: %d\n", res);
		kfree(eogre_info);
		goto done;
	}

done:
	return res;
}

/* Send MHI endpoint info to modem using QMI indication message */
int ipa_send_mhi_endp_ind_to_modem(void)
{
	struct ipa_endp_desc_indication_msg_v01 req;
	struct ipa_ep_id_type_v01 *ep_info;
	int ipa_mhi_prod_ep_idx =
		ipa_get_ep_mapping(IPA_CLIENT_MHI_LOW_LAT_PROD);
	int ipa_mhi_cons_ep_idx =
		ipa_get_ep_mapping(IPA_CLIENT_MHI_LOW_LAT_CONS);

	mutex_lock(&ipa3_ctx->lock);
	/* only modem up and MHI ctrl pipes are ready, then send QMI*/
	if (!ipa3_ctx->is_modem_up ||
		ipa3_ctx->mhi_ctrl_state != IPA_MHI_CTRL_SETUP_ALL) {
		mutex_unlock(&ipa3_ctx->lock);
		return 0;
	}
	mutex_unlock(&ipa3_ctx->lock);

	IPADBG("Sending MHI end point indication to modem\n");
	memset(&req, 0, sizeof(struct ipa_endp_desc_indication_msg_v01));
	req.ep_info_len = 2;
	req.ep_info_valid = true;
	req.num_eps_valid = true;
	req.num_eps = 2;
	ep_info = &req.ep_info[0];
	ep_info->ep_id = ipa_mhi_cons_ep_idx;
	ep_info->ic_type = DATA_IC_TYPE_MHI_V01;
	ep_info->ep_type = DATA_EP_DESC_TYPE_EMB_FLOW_CTL_PROD_V01;
	ep_info->ep_status = DATA_EP_STATUS_CONNECTED_V01;
	ep_info = &req.ep_info[1];
	ep_info->ep_id = ipa_mhi_prod_ep_idx;
	ep_info->ic_type = DATA_IC_TYPE_MHI_V01;
	ep_info->ep_type = DATA_EP_DESC_TYPE_EMB_FLOW_CTL_CONS_V01;
	ep_info->ep_status = DATA_EP_STATUS_CONNECTED_V01;
	return ipa3_qmi_send_endp_desc_indication(&req);
}

void ipa3_update_mhi_ctrl_state(u8 state, bool set)
{
	mutex_lock(&ipa3_ctx->lock);
	if (set)
		ipa3_ctx->mhi_ctrl_state |= state;
	else
		ipa3_ctx->mhi_ctrl_state &= ~state;
	mutex_unlock(&ipa3_ctx->lock);
	ipa_send_mhi_endp_ind_to_modem();
}
EXPORT_SYMBOL(ipa3_update_mhi_ctrl_state);

/**
 * ipa3_setup_uc_act_tbl() - IPA setup uc_act_tbl
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_setup_uc_act_tbl(void)
{
	int res = 0;
	struct ipa_mem_buffer *tbl;
	struct ipahal_reg_nat_uc_external_cfg nat_ex_cfg;
	struct ipahal_reg_nat_uc_shared_cfg nat_share_cfg;
	struct ipahal_reg_conn_track_uc_external_cfg ct_ex_cfg;
	struct ipahal_reg_conn_track_uc_shared_cfg ct_share_cfg;

	/* IPA version check */
	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_5) {
		IPAERR("Not support!\n");
		return -EPERM;
	}

	if (ipa3_ctx->uc_act_tbl_valid) {
		IPAERR(" already allocate uC act tbl\n");
		return -EEXIST;
	}

	tbl = &ipa3_ctx->uc_act_tbl;
	/* Allocate uc act tbl */
	tbl->size = sizeof(struct ipa_socksv5_uc_tmpl) * IPA_UC_ACT_TBL_SIZE;
	tbl->base = dma_alloc_coherent(ipa3_ctx->pdev, tbl->size,
		&tbl->phys_base, GFP_KERNEL);
	if (tbl->base == NULL)
		return -ENOMEM;
	memset(tbl->base, 0, tbl->size);

	ipa3_ctx->uc_act_tbl_valid = true;
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	/* LSB 32 bits*/
	nat_ex_cfg.nat_uc_external_table_addr_lsb =
		(u32) (tbl->phys_base & 0xFFFFFFFF);
	ipahal_write_reg_fields(IPA_NAT_UC_EXTERNAL_CFG, &nat_ex_cfg);
	/* MSB 16 bits */
	nat_share_cfg.nat_uc_external_table_addr_msb =
		(u16) (((tbl->phys_base & 0xFFFFFFFF00000000) >> 32) & 0xFFFF);
	ipahal_write_reg_fields(IPA_NAT_UC_SHARED_CFG, &nat_share_cfg);

	/* LSB 32 bits*/
	ct_ex_cfg.conn_track_uc_external_table_addr_lsb =
		(u32) (tbl->phys_base & 0xFFFFFFFF);

	ipahal_write_reg_fields(IPA_CONN_TRACK_UC_EXTERNAL_CFG, &ct_ex_cfg);
	/* MSB 16 bits */
	ct_share_cfg.conn_track_uc_external_table_addr_msb =
		(u16) (((tbl->phys_base & 0xFFFFFFFF00000000) >> 32) & 0xFFFF);
	ipahal_write_reg_fields(IPA_CONN_TRACK_UC_SHARED_CFG, &ct_share_cfg);


	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return res;
}

static void ipa3_socksv5_msg_free_cb(void *buff, u32 len, u32 type)
{
	if (!buff) {
		IPAERR("Null buffer\n");
		return;
	}

	if (type != IPA_SOCKV5_ADD &&
	    type != IPA_SOCKV5_DEL) {
		IPAERR("Wrong type given. buff %pK type %d\n", buff, type);
		kfree(buff);
		return;
	}

	kfree(buff);
}

/**
 * ipa_add_socksv5_conn() - IPA add socksv5_conn
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_add_socksv5_conn(struct ipa_socksv5_info *info)
{
	int res = 0;
	void *rp_va, *wp_va;
	struct ipa_socksv5_msg *socksv5_msg;
	struct ipa_msg_meta msg_meta;

	/* IPA version check */
	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_5) {
		IPAERR("Not support !\n");
		return -EPERM;
	}

	if (!ipa3_ctx->uc_act_tbl_valid) {
		IPAERR("uC act tbl haven't allocated\n");
		return -ENOENT;
	}

	if (!info) {
		IPAERR("Null info\n");
		return -EIO;
	}

	mutex_lock(&ipa3_ctx->act_tbl_lock);
	/* check the left # of entries */
	if (ipa3_ctx->uc_act_tbl_total
		>= IPA_UC_ACT_TBL_SIZE)	{
		IPAERR("uc act tbl is full!\n");
		res = -EFAULT;
		goto error;
	}

	/* Copied the act-info to tbl */
	wp_va = ipa3_ctx->uc_act_tbl.base +
		ipa3_ctx->uc_act_tbl_next_index
			* sizeof(struct ipa_socksv5_uc_tmpl);

	/* check entry valid */
	if ((info->ul_out.cmd_id != IPA_SOCKsv5_ADD_COM_ID)
		|| (info->dl_out.cmd_id != IPA_SOCKsv5_ADD_COM_ID)) {
		IPAERR("cmd_id not set UL%d DL%d!\n",
			info->ul_out.cmd_id,
			info->dl_out.cmd_id);
		res = -EINVAL;
		goto error;
	}

	if ((info->ul_out.cmd_param < IPA_SOCKsv5_ADD_V6_V4_COM_PM)
		|| (info->ul_out.cmd_param > IPA_SOCKsv5_ADD_V6_V6_COM_PM)) {
		IPAERR("ul cmd_param is not support%d!\n",
			info->ul_out.cmd_param);
		res = -EINVAL;
		goto error;
	}

	if ((info->dl_out.cmd_param < IPA_SOCKsv5_ADD_V6_V4_COM_PM)
		|| (info->dl_out.cmd_param > IPA_SOCKsv5_ADD_V6_V6_COM_PM)) {
		IPAERR("dl cmd_param is not support%d!\n",
			info->dl_out.cmd_param);
		res = -EINVAL;
		goto error;
	}

	/* indicate entry valid */
	info->ul_out.ipa_sockv5_mask |= IPA_SOCKSv5_ENTRY_VALID;
	info->dl_out.ipa_sockv5_mask |= IPA_SOCKSv5_ENTRY_VALID;

	memcpy(wp_va, &(info->ul_out), sizeof(info->ul_out));
	memcpy(wp_va + sizeof(struct ipa_socksv5_uc_tmpl),
		&(info->dl_out), sizeof(info->dl_out));

	/* set output handle */
	info->handle = (uint16_t) ipa3_ctx->uc_act_tbl_next_index;

	ipa3_ctx->uc_act_tbl_total += 2;

	/* send msg to ipacm */
	socksv5_msg = kzalloc(sizeof(*socksv5_msg), GFP_KERNEL);
	if (!socksv5_msg) {
		IPAERR("socksv5_msg memory allocation failed !\n");
		res = -ENOMEM;
		goto error;
	}
	memcpy(&(socksv5_msg->ul_in), &(info->ul_in), sizeof(info->ul_in));
	memcpy(&(socksv5_msg->dl_in), &(info->dl_in), sizeof(info->dl_in));
	socksv5_msg->handle = info->handle;
	socksv5_msg->ul_in.index =
		(uint16_t) ipa3_ctx->uc_act_tbl_next_index;
	socksv5_msg->dl_in.index =
		(uint16_t) ipa3_ctx->uc_act_tbl_next_index + 1;

	memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
	msg_meta.msg_type = IPA_SOCKV5_ADD;
	msg_meta.msg_len = sizeof(struct ipa_socksv5_msg);
	/* post event to ipacm*/
	res = ipa_send_msg(&msg_meta, socksv5_msg, ipa3_socksv5_msg_free_cb);
	if (res) {
		IPAERR_RL("ipa_send_msg failed: %d\n", res);
		kfree(socksv5_msg);
		goto error;
	}

	if (ipa3_ctx->uc_act_tbl_total < IPA_UC_ACT_TBL_SIZE) {
		/* find next free spot */
		do {
			ipa3_ctx->uc_act_tbl_next_index += 2;
			ipa3_ctx->uc_act_tbl_next_index %=
				IPA_UC_ACT_TBL_SIZE;

			rp_va =  ipa3_ctx->uc_act_tbl.base +
				ipa3_ctx->uc_act_tbl_next_index
					* sizeof(struct ipa_socksv5_uc_tmpl);

			if (!((((struct ipa_socksv5_uc_tmpl *) rp_va)->
				ipa_sockv5_mask) & IPA_SOCKSv5_ENTRY_VALID)) {
				IPADBG("next available entry %d, total %d\n",
				ipa3_ctx->uc_act_tbl_next_index,
				ipa3_ctx->uc_act_tbl_total);
				break;
			}
		} while (rp_va != wp_va);

		if (rp_va == wp_va) {
			/* set to max tbl size to debug */
			IPAERR("can't find available spot!\n");
			ipa3_ctx->uc_act_tbl_total = IPA_UC_ACT_TBL_SIZE;
			res = -EFAULT;
		}
	}

error:
	mutex_unlock(&ipa3_ctx->act_tbl_lock);
	return res;
}

void ipa3_default_evict_register( void )
{
	struct ipahal_reg_coal_evict_lru evict_lru;

	if ( ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5
		 &&
		 ipa3_ctx->set_evict_reg == false )
	{
		ipa3_ctx->set_evict_reg = true;

		IPADBG("Setting COAL eviction register with default values\n");

		ipa3_get_default_evict_values(&evict_lru);

		ipahal_write_reg_fields(IPA_COAL_EVICT_LRU, &evict_lru);
	}
}

/**
 * ipa_del_socksv5_conn() - IPA add socksv5_conn
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_del_socksv5_conn(uint32_t handle)
{
	int res = 0;
	void *rp_va;
	uint32_t *socksv5_handle;
	struct ipa_msg_meta msg_meta;

	/* IPA version check */
	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_5) {
		IPAERR("Not support !\n");
		return -EPERM;
	}

	if (!ipa3_ctx->uc_act_tbl_valid) {
		IPAERR("uC act tbl haven't allocated\n");
		return -ENOENT;
	}

	if (handle > IPA_UC_ACT_TBL_SIZE || handle < 0) {
		IPAERR("invalid handle!\n");
		return -EINVAL;
	}

	if ((handle % 2) != 0) {
		IPAERR("invalid handle!\n");
		return -EINVAL;
	}

	if (ipa3_ctx->uc_act_tbl_total < 2) {
		IPAERR("invalid handle, all tbl is empty!\n");
		return -EINVAL;
	}

	rp_va =  ipa3_ctx->uc_act_tbl.base +
			handle * sizeof(struct ipa_socksv5_uc_tmpl);

	/* check entry is valid or not */
	mutex_lock(&ipa3_ctx->act_tbl_lock);
	if (!((((struct ipa_socksv5_uc_tmpl *) rp_va)->
		ipa_sockv5_mask) & IPA_SOCKSv5_ENTRY_VALID)) {
		IPADBG(" entry %d already free\n", handle);
	}

	if (!((((struct ipa_socksv5_uc_tmpl *) (rp_va +
		sizeof(struct ipa_socksv5_uc_tmpl)))->
		ipa_sockv5_mask) & IPA_SOCKSv5_ENTRY_VALID)) {
		IPADBG(" entry %d already free\n", handle);
	}

	((struct ipa_socksv5_uc_tmpl *) rp_va)->ipa_sockv5_mask
		&= ~IPA_SOCKSv5_ENTRY_VALID;
	((struct ipa_socksv5_uc_tmpl *) (rp_va +
		sizeof(struct ipa_socksv5_uc_tmpl)))->ipa_sockv5_mask
			&= ~IPA_SOCKSv5_ENTRY_VALID;
	ipa3_ctx->uc_act_tbl_total -= 2;

	IPADBG("free entry %d and %d, left total %d\n",
		handle,
		handle + 1,
		ipa3_ctx->uc_act_tbl_total);

	/* send msg to ipacm */
	socksv5_handle = kzalloc(sizeof(*socksv5_handle), GFP_KERNEL);
	if (!socksv5_handle) {
		IPAERR("socksv5_handle memory allocation failed!\n");
		res = -ENOMEM;
		goto error;
	}
	memcpy(socksv5_handle, &handle, sizeof(handle));
	msg_meta.msg_type = IPA_SOCKV5_DEL;
	msg_meta.msg_len = sizeof(uint32_t);
	res = ipa_send_msg(&msg_meta, socksv5_handle,
		ipa3_socksv5_msg_free_cb);
	if (res) {
		IPAERR_RL("ipa_send_msg failed: %d\n", res);
		kfree(socksv5_handle);
	}

error:
	mutex_unlock(&ipa3_ctx->act_tbl_lock);
	return res;
}
