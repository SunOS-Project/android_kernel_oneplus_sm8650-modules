/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _IPA3_I_H_
#define _IPA3_I_H_

#include <linux/bitops.h>
#include <linux/cdev.h>
#include <linux/export.h>
#include <linux/idr.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include "ipa.h"
#include <linux/ipa_usb.h>
#include "ipa_qdss.h"
#include <linux/iommu.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0))
#include <linux/qcom-iommu-util.h>
#endif
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include "ipa_qmi_service.h"
#include "ipahal_reg.h"
#include "ipahal.h"
#include "ipahal_fltrt.h"
#include "ipahal_hw_stats.h"
#include "ipa_common_i.h"
#include "ipa_uc_offload_i.h"
#include "ipa_pm.h"
#include "ipa_defs.h"
#include <linux/mailbox_client.h>
#include <linux/mailbox/qmp.h>
#include <linux/rmnet_ipa_fd_ioctl.h>
#include "ipa_uc_holb_monitor.h"
#include <soc/qcom/minidump.h>
#ifdef CONFIG_IPA_RTP
#include "ipa_rtp_genl.h"
#endif
#include <linux/dma-buf.h>

#define IPA_DEV_NAME_MAX_LEN 15
#define DRV_NAME "ipa"

#define IPA_v4_USB0_EP_ID		11
#define IPA_v4_USB1_EP_ID		12

#define IPA_v4_PCIE0_EP_ID		21
#define IPA_v4_PCIE1_EP_ID		22

#define IPA_v5_PCIE0_EP_ID		4

#define IPA_COOKIE 0x57831603
#define IPA_RT_RULE_COOKIE 0x57831604
#define IPA_RT_TBL_COOKIE 0x57831605
#define IPA_FLT_COOKIE 0x57831606
#define IPA_HDR_COOKIE 0x57831607
#define IPA_PROC_HDR_COOKIE 0x57831608

#define MTU_BYTE 1500

#define IPA_EP_NOT_ALLOCATED (-1)
#define IPA3_MAX_NUM_PIPES 31
#define IPA5_PIPES_NUM 36
#define IPA5_PIPE_REG_NUM 2
#define IPA5_MAX_NUM_PIPES (IPA5_PIPES_NUM)
#define IPA_SYS_DESC_FIFO_SZ 0x800
#define IPA_SYS_TX_DATA_DESC_FIFO_SZ 0x1000
#define IPA_SYS_TX_DATA_DESC_FIFO_SZ_8K 0x2000
#define IPA_SYS_TPUT_EP_DESC_FIFO_SZ 0x10
#define IPA_COMMON_EVENT_RING_SIZE 0x7C00
#define IPA_LAN_RX_HEADER_LENGTH (2)
#define IPA_QMAP_HEADER_LENGTH (4)
#define IPA_DL_CHECKSUM_LENGTH (8)
#define IPA_NUM_DESC_PER_SW_TX (3)
#define IPA_GENERIC_RX_POOL_SZ_WAN 224
#define IPA_GENERIC_RX_POOL_SZ 192
#define IPA_GENERIC_RX_PAGE_POOL_SZ_FACTOR 2
#define IPA_GENERIC_RX_CMN_PAGE_POOL_SZ_FACTOR 5
#define IPA_GENERIC_RX_CMN_TEMP_POOL_SZ_FACTOR 3
#define IPA_UC_FINISH_MAX 6
#define IPA_UC_WAIT_MIN_SLEEP 1000
#define IPA_UC_WAII_MAX_SLEEP 1200
#define IPA_HOLB_TMR_DIS 0x0
#define IPA_HOLB_TMR_EN 0x1
#define IPA_HOLB_TMR_VAL_4_5 31
#define IPA_IMM_IP_PACKET_INIT_EX_CMD_NUM (IPA5_MAX_NUM_PIPES + 1)

#define IPA_Q6_FNR_START_IDX (128)
#define IPA_Q6_FNR_IDX_CNT (52)
#define IPA_Q6_FNR_END_IDX (IPA_Q6_FNR_START_IDX+IPA_Q6_FNR_IDX_CNT-1)
#define IPA_Q6_FNR_STATS_SIZE (IPA_Q6_FNR_IDX_CNT * 16)
#define IPA_MPM_MAX_RING_LEN 64
#define IPA_MAX_TETH_AGGR_BYTE_LIMIT 24
#define IPA_MPM_MAX_UC_THRESH 4

/* ULSO Constants */
enum {
	ENDP_INIT_ULSO_CFG_IP_ID_MIN_MAX_VAL_IDX_LINUX,
	ENDP_INIT_ULSO_CFG_IP_ID_MIN_MAX_VAL_IDX_FREE1,
	ENDP_INIT_ULSO_CFG_IP_ID_MIN_MAX_VAL_IDX_FREE2,
	ENDP_INIT_ULSO_CFG_IP_ID_MIN_MAX_VAL_IDX_MAX
};

#define QMAP_HDR_LEN 8

#define IPA_HOLB_TMR_DIS 0x0
#define IPA_HOLB_TMR_EN 0x1
/*
 * The transport descriptor size was changed to GSI_CHAN_RE_SIZE_16B, but
 * IPA users still use sps_iovec size as FIFO element size.
 */
#define IPA_FIFO_ELEMENT_SIZE 8

#define IPA_MAX_STATUS_STAT_NUM 30

#define IPA_IPC_LOG_PAGES 50

#define IPA_MAX_NUM_REQ_CACHE 10

#define NAPI_WEIGHT 64

#define NAPI_TX_WEIGHT 64

#define IPA_WAN_AGGR_PKT_CNT 1

#define IPA_PAGE_POLL_DEFAULT_THRESHOLD 15
#define IPA_PAGE_POLL_THRESHOLD_MAX 30

#define NTN3_CLIENTS_NUM 2

#define IPA_MAX_NAPI_SORT_PAGE_THRSHLD 3
#define IPA_MAX_PAGE_WQ_RESCHED_TIME 2

#define IPA_WDI2_OVER_GSI() (ipa3_ctx->ipa_wdi2_over_gsi \
		&& (ipa_get_wdi_version() == IPA_WDI_2))

#define WLAN_IPA_EVENT(m) (m == WLAN_STA_CONNECT || \
		m == WLAN_AP_CONNECT || \
		m == WLAN_CLIENT_CONNECT_EX || \
		m == WLAN_CLIENT_CONNECT || \
		m == WLAN_STA_DISCONNECT || \
		m == WLAN_AP_DISCONNECT || \
		m == WLAN_CLIENT_DISCONNECT)

#define IPADBG(fmt, args...) \
	do { \
		pr_debug(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
		if (ipa3_ctx) { \
			IPA_IPC_LOGGING(ipa3_ctx->logbuf, \
				DRV_NAME " %s:%d " fmt, ## args); \
			IPA_IPC_LOGGING(ipa3_ctx->logbuf_low, \
				DRV_NAME " %s:%d " fmt, ## args); \
		} \
	} while (0)

#define IPADBG_LOW(fmt, args...) \
	do { \
		pr_debug(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
		if (ipa3_ctx) \
			IPA_IPC_LOGGING(ipa3_ctx->logbuf_low, \
				DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPADBG_CLK(fmt, args...) \
	do { \
		pr_debug(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
		if (ipa3_ctx) \
			IPA_IPC_LOGGING(ipa3_ctx->logbuf_clk, \
				DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPAERR(fmt, args...) \
	do { \
		pr_err(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
		if (ipa3_ctx) { \
			IPA_IPC_LOGGING(ipa3_ctx->logbuf, \
				DRV_NAME " %s:%d " fmt, ## args); \
			IPA_IPC_LOGGING(ipa3_ctx->logbuf_low, \
				DRV_NAME " %s:%d " fmt, ## args); \
		} \
	} while (0)

#define IPAERR_RL(fmt, args...) \
	do { \
		pr_err_ratelimited_ipa(DRV_NAME " %s:%d " fmt, __func__,\
		__LINE__, ## args);\
		if (ipa3_ctx) { \
			IPA_IPC_LOGGING(ipa3_ctx->logbuf, \
				DRV_NAME " %s:%d " fmt, ## args); \
			IPA_IPC_LOGGING(ipa3_ctx->logbuf_low, \
				DRV_NAME " %s:%d " fmt, ## args); \
		} \
	} while (0)

#define IPALOG_VnP_ADDRS(ptr) \
	do { \
		phys_addr_t b = (phys_addr_t) virt_to_phys(ptr); \
		IPAERR("%s: VIRT: %pK PHYS: %pa\n", \
			   #ptr, ptr, &b); \
	} while (0)

/* round addresses for closes page per SMMU requirements */
#define IPA_SMMU_ROUND_TO_PAGE(iova, pa, size, iova_p, pa_p, size_p) \
	do { \
		(iova_p) = rounddown((iova), PAGE_SIZE); \
		(pa_p) = rounddown((pa), PAGE_SIZE); \
		(size_p) = roundup((size) + (pa) - (pa_p), PAGE_SIZE); \
	} while (0)

#define WLAN_AMPDU_TX_EP 15
#define WLAN_PROD_TX_EP  19
#define WLAN1_CONS_RX_EP  14
#define WLAN2_CONS_RX_EP  16
#define WLAN3_CONS_RX_EP  17
#define WLAN4_CONS_RX_EP  18

#define IPA_RAM_NAT_OFST \
	IPA_MEM_PART(nat_tbl_ofst)
#define IPA_RAM_NAT_SIZE \
	IPA_MEM_PART(nat_tbl_size)
#define IPA_RAM_IPV6CT_OFST 0
#define IPA_RAM_IPV6CT_SIZE 0
#define IPA_MEM_CANARY_VAL 0xdeadbeef

#define IS_IPV6CT_MEM_DEV(d) \
	(((void *) (d) == (void *) &ipa3_ctx->ipv6ct_mem))

#define IS_NAT_MEM_DEV(d) \
	(((void *) (d) == (void *) &ipa3_ctx->nat_mem))

#define IPA_STATS

#ifdef IPA_STATS
#define IPA_STATS_INC_CNT(val) (++val)
#define IPA_STATS_DEC_CNT(val) (--val)
#define IPA_STATS_EXCP_CNT(__excp, __base) do {				\
	if (__excp < 0 || __excp >= IPAHAL_PKT_STATUS_EXCEPTION_MAX)	\
		break;							\
	++__base[__excp];						\
	} while (0)
#else
#define IPA_STATS_INC_CNT(x) do { } while (0)
#define IPA_STATS_DEC_CNT(x)
#define IPA_STATS_EXCP_CNT(__excp, __base) do { } while (0)
#endif

#define IPA_HDR_BIN0 0
#define IPA_HDR_BIN1 1
#define IPA_HDR_BIN2 2
#define IPA_HDR_BIN3 3
#define IPA_HDR_BIN4 4
#define IPA_HDR_BIN5 5
#define IPA_HDR_BIN_MAX 6

enum hdr_tbl_storage {
	HDR_TBL_LCL,
	HDR_TBL_SYS,
	HDR_TBLS_TOTAL,
};

#define IPA_HDR_TO_DDR_PATTERN 0x2DDA

#define IPA_HDR_PROC_CTX_BIN0 0
#define IPA_HDR_PROC_CTX_BIN1 1
#define IPA_HDR_PROC_CTX_BIN_MAX 2

#define IPA_RX_POOL_CEIL 32
#define IPA_RX_SKB_SIZE 1792

#define IPA_A5_MUX_HDR_NAME "ipa_excp_hdr"
#define IPA_LAN_RX_HDR_NAME "ipa_lan_hdr"
#define IPA_INVALID_L4_PROTOCOL 0xFF

#define IPA_HDR_PROC_CTX_TABLE_ALIGNMENT_BYTE 8
#define IPA_HDR_PROC_CTX_TABLE_ALIGNMENT(start_ofst) \
	(((start_ofst) + IPA_HDR_PROC_CTX_TABLE_ALIGNMENT_BYTE - 1) & \
	~(IPA_HDR_PROC_CTX_TABLE_ALIGNMENT_BYTE - 1))

#define MAX_RESOURCE_TO_CLIENTS (IPA_CLIENT_MAX)
#define IPA_MEM_PART(x_) (ipa3_ctx->ctrl->mem_partition->x_)

#define IPA_GSI_CHANNEL_STOP_MAX_RETRY 10
#define IPA_GSI_CHANNEL_STOP_PKT_SIZE 1

#define IPA_GSI_CHANNEL_EMPTY_MAX_RETRY 15
#define IPA_GSI_CHANNEL_EMPTY_SLEEP_MIN_USEC (1000)
#define IPA_GSI_CHANNEL_EMPTY_SLEEP_MAX_USEC (2000)

#define IPA_SLEEP_CLK_RATE_KHZ (32)

#define IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES 120
#define IPA3_ACTIVE_CLIENTS_LOG_LINE_LEN 96
#define IPA3_ACTIVE_CLIENTS_LOG_HASHTABLE_SIZE 50
#define IPA3_ACTIVE_CLIENTS_LOG_NAME_LEN 40
#define SMEM_IPA_FILTER_TABLE 497
#define IPA_TX_WRAPPER_CACHE_MAX_THRESHOLD 2000

enum {
	SMEM_APPS,
	SMEM_MODEM,
	SMEM_Q6,
	SMEM_DSPS,
	SMEM_WCNSS,
	SMEM_CDSP,
	SMEM_RPM,
	SMEM_TZ,
	SMEM_SPSS,
	SMEM_HYP,
	NUM_SMEM_SUBSYSTEMS,
};

#define IPA_WDI_RX_RING_RES            0
#define IPA_WDI_RX_RING_RP_RES         1
#define IPA_WDI_RX_COMP_RING_RES       2
#define IPA_WDI_RX_COMP_RING_WP_RES    3
#define IPA_WDI_RX2_RING_RES           4
#define IPA_WDI_RX2_RING_RP_RES        5
#define IPA_WDI_RX2_COMP_RING_RES      6
#define IPA_WDI_RX2_COMP_RING_WP_RES   7
#define IPA_WDI_TX_RING_RES            8
#define IPA_WDI_CE_RING_RES            9
#define IPA_WDI_CE_DB_RES              10
#define IPA_WDI_TX_DB_RES              11
#define IPA_WDI_TX1_RING_RES           12
#define IPA_WDI_CE1_RING_RES           13
#define IPA_WDI_CE1_DB_RES             14
#define IPA_WDI_TX1_DB_RES             15
#define IPA_WDI_TX2_RING_RES           16
#define IPA_WDI_CE2_RING_RES           17
#define IPA_WDI_CE2_DB_RES             18
#define IPA_WDI_TX2_DB_RES             19
#define IPA_WDI_MAX_RES                20

#define IPA_WDI3_TX2_DIR 4
#define IPA_WDI3_RX2_DIR 5

/* use QMAP header reserved bit to identify tethered traffic */
#define IPA_QMAP_TETH_BIT (1 << 30)

#ifdef CONFIG_ARM64
/* Outer caches unsupported on ARM64 platforms */
# define outer_flush_range(x, y)
# define __cpuc_flush_dcache_area __flush_dcache_area
#endif

#define IPA_APP_VOTE_MAX 500

#define IPA_SMP2P_OUT_CLK_RSP_CMPLT_IDX 0
#define IPA_SMP2P_OUT_CLK_VOTE_IDX 1
#define IPA_SMP2P_SMEM_STATE_MASK 3


#define IPA_SUMMING_THRESHOLD (0x10)
#define IPA_PIPE_MEM_START_OFST (0x0)
#define IPA_PIPE_MEM_SIZE (0x0)
#define IPA_MOBILE_AP_MODE(x) (x == IPA_MODE_MOBILE_AP_ETH || \
				   x == IPA_MODE_MOBILE_AP_WAN || \
				   x == IPA_MODE_MOBILE_AP_WLAN)
#define IPA_CNOC_CLK_RATE (75 * 1000 * 1000UL)
#define IPA_A5_MUX_HEADER_LENGTH (8)

#define IPA_AGGR_MAX_STR_LENGTH (10)

#define CLEANUP_TAG_PROCESS_TIMEOUT 1000

#define IPA_AGGR_STR_IN_BYTES(str) \
	(strnlen((str), IPA_AGGR_MAX_STR_LENGTH - 1) + 1)

#define IPA_ADJUST_AGGR_BYTE_HARD_LIMIT(X) (X/1000)

#define IPA_TRANSPORT_PROD_TIMEOUT_MSEC 100

#define IPA3_ACTIVE_CLIENTS_TABLE_BUF_SIZE 4096

#define IPA_UC_ACT_TBL_SIZE 1000

#define IPA3_ACTIVE_CLIENT_LOG_TYPE_EP 0
#define IPA3_ACTIVE_CLIENT_LOG_TYPE_SIMPLE 1
#define IPA3_ACTIVE_CLIENT_LOG_TYPE_RESOURCE 2
#define IPA3_ACTIVE_CLIENT_LOG_TYPE_SPECIAL 3

#define IPA_MHI_GSI_EVENT_RING_ID_START 10
#define IPA_MHI_GSI_EVENT_RING_ID_END 12

#define IPA_SMEM_SIZE (8 * 1024)

#define IPA_GSI_CHANNEL_HALT_MIN_SLEEP 5000
#define IPA_GSI_CHANNEL_HALT_MAX_SLEEP 10000
#define IPA_GSI_CHANNEL_HALT_MAX_TRY 10

#define XR_IPA_UC_INIT_TIMEOUT_MSEC 100

/* round addresses for closes page per SMMU requirements */
#define IPA_SMMU_ROUND_TO_PAGE(iova, pa, size, iova_p, pa_p, size_p) \
	do { \
		(iova_p) = rounddown((iova), PAGE_SIZE); \
		(pa_p) = rounddown((pa), PAGE_SIZE); \
		(size_p) = roundup((size) + (pa) - (pa_p), PAGE_SIZE); \
	} while (0)


/* The relative location in /lib/firmware where the FWs will reside */
#define IPA_FWS_PATH "ipa/ipa_fws.elf"
/*
 * The following paths below are used when building the system for the
 * emulation environment.
 *
 * As new hardware platforms are added into the emulation environment,
 * please add the appropriate paths here for their firmwares.
 */
#define IPA_FWS_PATH_4_0     "ipa/4.0/ipa_fws.elf"
#define IPA_FWS_PATH_3_5_1   "ipa/3.5.1/ipa_fws.elf"
#define IPA_FWS_PATH_4_5     "ipa/4.5/ipa_fws.elf"

/*
 * The following will be used for determining/using access control
 * policy.
 */
#define USE_SCM            0 /* use scm call to determine policy */
#define OVERRIDE_SCM_TRUE  1 /* override scm call with true */
#define OVERRIDE_SCM_FALSE 2 /* override scm call with false */

#define SD_ENABLED  0 /* secure debug enabled. */
#define SD_DISABLED 1 /* secure debug disabled. */

#define IPA_MEM_INIT_VAL 0xFFFFFFFF

#ifdef CONFIG_COMPAT
#define IPA_IOC_COAL_EVICT_POLICY32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_COAL_EVICT_POLICY, \
					compat_uptr_t)
#define IPA_IOC_ADD_HDR32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_ADD_HDR, \
					compat_uptr_t)
#define IPA_IOC_DEL_HDR32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_DEL_HDR, \
					compat_uptr_t)
#define IPA_IOC_ADD_RT_RULE32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_ADD_RT_RULE, \
					compat_uptr_t)
#define IPA_IOC_DEL_RT_RULE32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_DEL_RT_RULE, \
					compat_uptr_t)
#define IPA_IOC_ADD_FLT_RULE32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_ADD_FLT_RULE, \
					compat_uptr_t)
#define IPA_IOC_DEL_FLT_RULE32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_DEL_FLT_RULE, \
					compat_uptr_t)
#define IPA_IOC_GET_RT_TBL32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GET_RT_TBL, \
				compat_uptr_t)
#define IPA_IOC_COPY_HDR32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_COPY_HDR, \
				compat_uptr_t)
#define IPA_IOC_QUERY_INTF32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_QUERY_INTF, \
				compat_uptr_t)
#define IPA_IOC_QUERY_INTF_TX_PROPS32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_QUERY_INTF_TX_PROPS, \
				compat_uptr_t)
#define IPA_IOC_QUERY_INTF_RX_PROPS32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_QUERY_INTF_RX_PROPS, \
					compat_uptr_t)
#define IPA_IOC_QUERY_INTF_EXT_PROPS32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_QUERY_INTF_EXT_PROPS, \
					compat_uptr_t)
#define IPA_IOC_GET_HDR32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GET_HDR, \
				compat_uptr_t)
#define IPA_IOC_ALLOC_NAT_MEM32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_ALLOC_NAT_MEM, \
				compat_uptr_t)
#define IPA_IOC_ALLOC_NAT_TABLE32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_ALLOC_NAT_TABLE, \
				compat_uptr_t)
#define IPA_IOC_ALLOC_IPV6CT_TABLE32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_ALLOC_IPV6CT_TABLE, \
				compat_uptr_t)
#define IPA_IOC_V4_INIT_NAT32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_V4_INIT_NAT, \
				compat_uptr_t)
#define IPA_IOC_INIT_IPV6CT_TABLE32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_INIT_IPV6CT_TABLE, \
				compat_uptr_t)
#define IPA_IOC_TABLE_DMA_CMD32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_TABLE_DMA_CMD, \
				compat_uptr_t)
#define IPA_IOC_V4_DEL_NAT32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_V4_DEL_NAT, \
				compat_uptr_t)
#define IPA_IOC_DEL_NAT_TABLE32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_DEL_NAT_TABLE, \
				compat_uptr_t)
#define IPA_IOC_DEL_IPV6CT_TABLE32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_DEL_IPV6CT_TABLE, \
				compat_uptr_t)
#define IPA_IOC_NAT_MODIFY_PDN32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_NAT_MODIFY_PDN, \
				compat_uptr_t)
#define IPA_IOC_GET_NAT_OFFSET32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GET_NAT_OFFSET, \
				compat_uptr_t)
#define IPA_IOC_PULL_MSG32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_PULL_MSG, \
				compat_uptr_t)
#define IPA_IOC_RM_ADD_DEPENDENCY32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_RM_ADD_DEPENDENCY, \
				compat_uptr_t)
#define IPA_IOC_RM_DEL_DEPENDENCY32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_RM_DEL_DEPENDENCY, \
				compat_uptr_t)
#define IPA_IOC_GENERATE_FLT_EQ32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GENERATE_FLT_EQ, \
				compat_uptr_t)
#define IPA_IOC_QUERY_RT_TBL_INDEX32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_QUERY_RT_TBL_INDEX, \
				compat_uptr_t)
#define IPA_IOC_WRITE_QMAPID32  _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_WRITE_QMAPID, \
				compat_uptr_t)
#define IPA_IOC_MDFY_FLT_RULE32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_MDFY_FLT_RULE, \
				compat_uptr_t)
#define IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_ADD32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_NOTIFY_WAN_UPSTREAM_ROUTE_ADD, \
				compat_uptr_t)
#define IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_DEL32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_NOTIFY_WAN_UPSTREAM_ROUTE_DEL, \
				compat_uptr_t)
#define IPA_IOC_NOTIFY_WAN_EMBMS_CONNECTED32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_NOTIFY_WAN_EMBMS_CONNECTED, \
					compat_uptr_t)
#define IPA_IOC_ADD_HDR_PROC_CTX32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_ADD_HDR_PROC_CTX, \
				compat_uptr_t)
#define IPA_IOC_DEL_HDR_PROC_CTX32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_DEL_HDR_PROC_CTX, \
				compat_uptr_t)
#define IPA_IOC_MDFY_RT_RULE32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_MDFY_RT_RULE, \
				compat_uptr_t)
#define IPA_IOC_GET_NAT_IN_SRAM_INFO32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GET_NAT_IN_SRAM_INFO, \
				compat_uptr_t)
#define IPA_IOC_APP_CLOCK_VOTE32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_APP_CLOCK_VOTE, \
				compat_uptr_t)
#define IPA_IOC_ADD_EoGRE_MAPPING32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_ADD_EoGRE_MAPPING, \
				compat_uptr_t)
#define IPA_IOC_DEL_EoGRE_MAPPING32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_DEL_EoGRE_MAPPING, \
				compat_uptr_t)
#define IPA_IOC_SET_NAT_EXC_RT_TBL_IDX32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_SET_NAT_EXC_RT_TBL_IDX, \
				compat_uptr_t)
#define IPA_IOC_SET_CONN_TRACK_EXC_RT_TBL_IDX32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_SET_CONN_TRACK_EXC_RT_TBL_IDX, \
				compat_uptr_t)
#endif /* #ifdef CONFIG_COMPAT */

#define IPA_TZ_UNLOCK_ATTRIBUTE 0x0C0311

#define MBOX_TOUT_MS 100

#define IPA_RULE_CNT_MAX 512

/* XR-IPA uC temp buffers sizes */
#define TEMP_BUFF_SIZE	0x300000
/* XR-IPA uC no. of temp buffers */
#define NO_OF_BUFFS	0x04
/* Max number of RTP streams supported */
#define MAX_STREAMS 2

/* miscellaneous for rmnet_ipa and qmi_service */
enum ipa_type_mode {
	IPA_HW_TYPE,
	PLATFORM_TYPE,
	IPA3_HW_MODE,
};

enum ipa_flag {
	IPA_ENDP_DELAY_WA_EN,
	IPA_HW_STATS_EN,
	IPA_MHI_EN,
	IPA_FLTRT_NOT_HASHABLE_EN,
};

enum ipa_icc_level {
	IPA_ICC_NONE,
	IPA_ICC_SVS2,
	IPA_ICC_SVS,
	IPA_ICC_NOMINAL,
	IPA_ICC_TURBO,
	IPA_ICC_LVL_MAX,
};

enum ipa_icc_path {
	IPA_ICC_IPA_TO_LLCC,
	IPA_ICC_LLCC_TO_EBIL,
	IPA_ICC_IPA_TO_IMEM,
	IPA_ICC_APSS_TO_IPA,
	IPA_ICC_PATH_MAX,
};

enum ipa_icc_type {
	IPA_ICC_AB,
	IPA_ICC_IB,
	IPA_ICC_TYPE_MAX,
};

#define IPA_ICC_MAX (IPA_ICC_PATH_MAX*IPA_ICC_TYPE_MAX)


#define IPA_MHI_CTRL_NOT_SETUP (0)
#define IPA_MHI_CTRL_UL_SETUP (1 << 1)
#define IPA_MHI_CTRL_DL_SETUP (1 << 2)
#define IPA_MHI_CTRL_SETUP_ALL (IPA_MHI_CTRL_UL_SETUP | IPA_MHI_CTRL_DL_SETUP)

/**
 * struct  ipa_rx_page_data - information needed
 * to send to wlan driver on receiving data from ipa hw
 * @page: skb page
 * @dma_addr: DMA address of this Rx packet
 * @is_tmp_alloc: skb page from tmp_alloc or recycle_list
 * @page_order: page order associated with the page.
 */
struct ipa_rx_page_data {
	struct page *page;
	dma_addr_t dma_addr;
	bool is_tmp_alloc;
	u32 page_order;
};

struct ipa3_active_client_htable_entry {
	struct hlist_node list;
	char id_string[IPA3_ACTIVE_CLIENTS_LOG_NAME_LEN];
	int count;
	enum ipa_active_client_log_type type;
};

struct ipa3_active_clients_log_ctx {
	spinlock_t lock;
	char *log_buffer[IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES];
	int log_head;
	int log_tail;
	bool log_rdy;
	struct hlist_head htable[IPA3_ACTIVE_CLIENTS_LOG_HASHTABLE_SIZE];
};

struct ipa3_client_names {
	enum ipa_client_type names[MAX_RESOURCE_TO_CLIENTS];
	int length;
};

struct ipa_smmu_cb_ctx {
	bool valid;
	struct device *dev;
	struct iommu_domain *iommu_domain;
	unsigned long next_addr;
	u32 va_start;
	u32 va_size;
	u32 va_end;
	u32 geometry_start;
	u32 geometry_end;
	bool shared;
	bool is_cache_coherent;
	bool done;
};

/**
 * struct ipa_flt_rule_add_i - filtering rule descriptor
 * includes in and out parameters
 * @rule: actual rule to be added
 * @at_rear: add at back of filtering table?
 * @flt_rule_hdl: out parameter, handle to rule, valid when status is 0
 * @status:	output parameter, status of filtering rule add   operation,
 *		0 for success,
 *		-1 for failure
 *
 */
struct ipa_flt_rule_add_i {
	u8 at_rear;
	u32 flt_rule_hdl;
	int status;
	struct ipa_flt_rule_i rule;
};

/**
 * struct ipa_flt_rule_mdfy_i - filtering rule descriptor
 * includes in and out parameters
 * @rule: actual rule to be added
 * @flt_rule_hdl: handle to rule
 * @status:	output parameter, status of filtering rule modify  operation,
 *		0 for success,
 *		-1 for failure
 *
 */
struct ipa_flt_rule_mdfy_i {
	u32 rule_hdl;
	int status;
	struct ipa_flt_rule_i rule;
};

/**
 * struct ipa_rt_rule_add_i - routing rule descriptor includes
 * in and out parameters
 * @rule: actual rule to be added
 * @at_rear:	add at back of routing table, it is NOT possible to add rules at
 *		the rear of the "default" routing tables
 * @rt_rule_hdl: output parameter, handle to rule, valid when status is 0
 * @status:	output parameter, status of routing rule add operation,
 *		0 for success,
 *		-1 for failure
 */
struct ipa_rt_rule_add_i {
	u8 at_rear;
	u32 rt_rule_hdl;
	int status;
	struct ipa_rt_rule_i rule;
};

/**
 * struct ipa_rt_rule_mdfy_i - routing rule descriptor includes
 * in and out parameters
 * @rule: actual rule to be added
 * @rt_rule_hdl: handle to rule which supposed to modify
 * @status:	output parameter, status of routing rule modify  operation,
 *		0 for success,
 *		-1 for failure
 *
 */
struct ipa_rt_rule_mdfy_i {
	u32 rt_rule_hdl;
	int status;
	struct ipa_rt_rule_i rule;
};

/**
 * struct ipa_rt_rule_add_ext_i - routing rule descriptor
 * includes in and out parameters
 * @rule: actual rule to be added
 * @at_rear:	add at back of routing table, it is NOT possible to add rules at
 *		the rear of the "default" routing tables
 * @rt_rule_hdl: output parameter, handle to rule, valid when status is 0
 * @status:	output parameter, status of routing rule add operation,
 * @rule_id: rule_id to be assigned to the routing rule. In case client
 *  specifies rule_id as 0 the driver will assign a new rule_id
 *		0 for success,
 *		-1 for failure
 */
struct ipa_rt_rule_add_ext_i {
	uint8_t at_rear;
	uint32_t rt_rule_hdl;
	int status;
	uint16_t rule_id;
	struct ipa_rt_rule_i rule;
};

/**
 * struct ipa3_flt_entry - IPA filtering table entry
 * @link: entry's link in global filtering enrties list
 * @rule: filter rule
 * @cookie: cookie used for validity check
 * @tbl: filter table
 * @rt_tbl: routing table
 * @hw_len: entry's size
 * @id: rule handle - globally unique
 * @prio: rule 10bit priority which defines the order of the rule
 *  among other rules at the same integrated table
 * @rule_id: rule 10bit ID to be returned in packet status
 * @cnt_idx: stats counter index
 * @ipacm_installed: indicate if installed by ipacm
 */
struct ipa3_flt_entry {
	struct list_head link;
	u32 cookie;
	struct ipa_flt_rule_i rule;
	struct ipa3_flt_tbl *tbl;
	struct ipa3_rt_tbl *rt_tbl;
	u32 hw_len;
	int id;
	u16 prio;
	u16 rule_id;
	u8 cnt_idx;
	bool ipacm_installed;
};

/**
 * struct ipa3_rt_tbl - IPA routing table
 * @link: table's link in global routing tables list
 * @head_rt_rule_list: head of routing rules list
 * @name: routing table name
 * @idx: routing table index
 * @rule_cnt: number of rules in routing table
 * @ref_cnt: reference counter of routing table
 * @set: collection of routing tables
 * @cookie: cookie used for validity check
 * @in_sys: flag indicating if the table is located in system memory
 * @sz: the size of the routing table
 * @curr_mem: current routing tables block in sys memory
 * @prev_mem: previous routing table block in sys memory
 * @id: routing table id
 * @rule_ids: common idr structure that holds the rule_id for each rule
 */
struct ipa3_rt_tbl {
	struct list_head link;
	u32 cookie;
	struct list_head head_rt_rule_list;
	char name[IPA_RESOURCE_NAME_MAX];
	u32 idx;
	u32 rule_cnt;
	u32 ref_cnt;
	struct ipa3_rt_tbl_set *set;
	bool in_sys[IPA_RULE_TYPE_MAX];
	u32 sz[IPA_RULE_TYPE_MAX];
	struct ipa_mem_buffer curr_mem[IPA_RULE_TYPE_MAX];
	struct ipa_mem_buffer prev_mem[IPA_RULE_TYPE_MAX];
	int id;
	struct idr *rule_ids;
};

/**
 * struct ipa3_hdr_entry - IPA header table entry
 * @link: entry's link in global header table entries list
 * @hdr: the header
 * @hdr_len: header length
 * @name: name of header table entry
 * @type: l2 header type
 * @is_partial: flag indicating if header table entry is partial
 * @proc_ctx: processing context header
 * @offset_entry: entry's offset
 * @cookie: cookie used for validity check
 * @ref_cnt: reference counter of routing table
 * @id: header entry id
 * @is_eth2_ofst_valid: is eth2_ofst field valid?
 * @eth2_ofst: offset to start of Ethernet-II/802.3 header
 * @user_deleted: is the header deleted by the user?
 * @ipacm_installed: indicate if installed by ipacm
 * @is_lcl: is the entry in the SRAM?
 */
struct ipa3_hdr_entry {
	struct list_head link;
	u32 cookie;
	u8 hdr[IPA_HDR_MAX_SIZE];
	u32 hdr_len;
	char name[IPA_RESOURCE_NAME_MAX];
	enum ipa_hdr_l2_type type;
	u8 is_partial;
	struct ipa3_hdr_proc_ctx_entry *proc_ctx;
	struct ipa_hdr_offset_entry *offset_entry;
	u32 ref_cnt;
	int id;
	u8 is_eth2_ofst_valid;
	u16 eth2_ofst;
	bool user_deleted;
	bool ipacm_installed;
	bool is_lcl;
};

/**
 * struct ipa3_hdr_tbl - IPA header table
 * @head_hdr_entry_list: header entries list
 * @head_offset_list: header offset list
 * @head_free_offset_list: header free offset list
 * @hdr_cnt: number of headers
 * @end: the last header index
 */
struct ipa3_hdr_tbl {
	struct list_head head_hdr_entry_list;
	struct list_head head_offset_list[IPA_HDR_BIN_MAX];
	struct list_head head_free_offset_list[IPA_HDR_BIN_MAX];
	u32 hdr_cnt;
	u32 end;
};

/**
 * struct ipa3_hdr_offset_entry - IPA header offset entry
 * @link: entry's link in global processing context header offset entries list
 * @offset: the offset
 * @bin: bin
 * @ipacm_installed: indicate if installed by ipacm
 */
struct ipa3_hdr_proc_ctx_offset_entry {
	struct list_head link;
	u32 offset;
	u32 bin;
	bool ipacm_installed;
};

/**
 * struct ipa3_hdr_proc_ctx_entry - IPA processing context header table entry
 * @link: entry's link in global header table entries list
 * @type: header processing context type
 * @l2tp_params: L2TP parameters
 * @generic_params: generic proc_ctx params
 * @rtp_params: ipa rtp proc_ctx params
 * @offset_entry: entry's offset
 * @hdr: the header
 * @cookie: cookie used for validity check
 * @ref_cnt: reference counter of routing table
 * @id: processing context header entry id
 * @user_deleted: is the hdr processing context deleted by the user?
 * @ipacm_installed: indicate if installed by ipacm
 */
struct ipa3_hdr_proc_ctx_entry {
	struct list_head link;
	u32 cookie;
	enum ipa_hdr_proc_type type;
	struct ipa_l2tp_hdr_proc_ctx_params l2tp_params;
	struct ipa_eogre_hdr_proc_ctx_params eogre_params;
	struct ipa_eth_II_to_eth_II_ex_procparams generic_params;
	struct ipa_rtp_hdr_proc_ctx_params rtp_params;
	struct ipa3_hdr_proc_ctx_offset_entry *offset_entry;
	struct ipa3_hdr_entry *hdr;
	u32 ref_cnt;
	int id;
	bool user_deleted;
	bool ipacm_installed;
};

/**
 * struct ipa3_hdr_proc_ctx_tbl - IPA processing context header table
 * @head_proc_ctx_entry_list: header entries list
 * @head_offset_list: header offset list
 * @head_free_offset_list: header free offset list
 * @proc_ctx_cnt: number of processing context headers
 * @end: the last processing context header index
 * @start_offset: offset in words of processing context header table
 */
struct ipa3_hdr_proc_ctx_tbl {
	struct list_head head_proc_ctx_entry_list;
	struct list_head head_offset_list[IPA_HDR_PROC_CTX_BIN_MAX];
	struct list_head head_free_offset_list[IPA_HDR_PROC_CTX_BIN_MAX];
	u32 proc_ctx_cnt;
	u32 end;
	u32 start_offset;
};

/**
 * struct ipa3_flt_tbl - IPA filter table
 * @head_flt_rule_list: filter rules list
 * @rule_cnt: number of filter rules
 * @in_sys: flag indicating if filter table is located in system memory
 * @sz: the size of the filter tables
 * @curr_mem: current filter tables block in sys memory
 * @prev_mem: previous filter table block in sys memory
 * @rule_ids: common idr structure that holds the rule_id for each rule
 * @force_sys: flag indicating if filter table is forced to be
			located in system memory
 */
struct ipa3_flt_tbl {
	struct list_head head_flt_rule_list;
	u32 rule_cnt;
	bool in_sys[IPA_RULE_TYPE_MAX];
	u32 sz[IPA_RULE_TYPE_MAX];
	struct ipa_mem_buffer curr_mem[IPA_RULE_TYPE_MAX];
	struct ipa_mem_buffer prev_mem[IPA_RULE_TYPE_MAX];
	bool sticky_rear;
	struct idr *rule_ids;
	bool force_sys[IPA_RULE_TYPE_MAX];
};

struct ipa3_flt_tbl_nhash_lcl {
	struct list_head link;
	struct ipa3_flt_tbl *tbl;
};

/**
 * struct ipa3_rt_entry - IPA routing table entry
 * @link: entry's link in global routing table entries list
 * @rule: routing rule
 * @cookie: cookie used for validity check
 * @tbl: routing table
 * @hdr: header table
 * @proc_ctx: processing context table
 * @hw_len: the length of the table
 * @id: rule handle - globaly unique
 * @prio: rule 10bit priority which defines the order of the rule
 *  among other rules at the integrated same table
 * @rule_id: rule 10bit ID to be returned in packet status
 * @rule_id_valid: indicate if rule_id_valid valid or not?
 * @cnt_idx: stats counter index
 * @ipacm_installed: indicate if installed by ipacm
 */
struct ipa3_rt_entry {
	struct list_head link;
	u32 cookie;
	struct ipa_rt_rule_i rule;
	struct ipa3_rt_tbl *tbl;
	struct ipa3_hdr_entry *hdr;
	struct ipa3_hdr_proc_ctx_entry *proc_ctx;
	u32 hw_len;
	int id;
	u16 prio;
	u16 rule_id;
	u16 rule_id_valid;
	u8 cnt_idx;
	bool ipacm_installed;
};

/**
 * struct ipa3_rt_tbl_set - collection of routing tables
 * @head_rt_tbl_list: collection of routing tables
 * @tbl_cnt: number of routing tables
 * @rule_ids: idr structure that holds the rule_id for each rule
 */
struct ipa3_rt_tbl_set {
	struct list_head head_rt_tbl_list;
	u32 tbl_cnt;
	struct idr rule_ids;
};

/**
 * struct ipa3_wlan_stats - Wlan stats for each wlan endpoint
 * @rx_pkts_rcvd: Packets sent by wlan driver
 * @rx_pkts_status_rcvd: Status packets received from ipa hw
 * @rx_hd_processed: Data Descriptors processed by IPA Driver
 * @rx_hd_reply: Data Descriptors recycled by wlan driver
 * @rx_hd_rcvd: Data Descriptors sent by wlan driver
 * @rx_pkt_leak: Packet count that are not recycled
 * @rx_dp_fail: Packets failed to transfer to IPA HW
 * @tx_pkts_rcvd: SKB Buffers received from ipa hw
 * @tx_pkts_sent: SKB Buffers sent to wlan driver
 * @tx_pkts_dropped: Dropped packets count
 */
struct ipa3_wlan_stats {
	u32 rx_pkts_rcvd;
	u32 rx_pkts_status_rcvd;
	u32 rx_hd_processed;
	u32 rx_hd_reply;
	u32 rx_hd_rcvd;
	u32 rx_pkt_leak;
	u32 rx_dp_fail;
	u32 tx_pkts_rcvd;
	u32 tx_pkts_sent;
	u32 tx_pkts_dropped;
};

/**
 * struct ipa3_wlan_comm_memb - Wlan comm members
 * @wlan_spinlock: protects wlan comm buff list and its size
 * @ipa_tx_mul_spinlock: protects tx dp mul transfer
 * @wlan_comm_total_cnt: wlan common skb buffers allocated count
 * @wlan_comm_free_cnt: wlan common skb buffer free count
 * @total_tx_pkts_freed: Recycled Buffer count
 * @wlan_comm_desc_list: wlan common skb buffer list
 */
struct ipa3_wlan_comm_memb {
	spinlock_t wlan_spinlock;
	spinlock_t ipa_tx_mul_spinlock;
	u32 wlan_comm_total_cnt;
	u32 wlan_comm_free_cnt;
	u32 total_tx_pkts_freed;
	struct list_head wlan_comm_desc_list;
	atomic_t active_clnt_cnt;
};

struct ipa_gsi_ep_mem_info {
	u32 evt_ring_len;
	u64 evt_ring_base_addr;
	void *evt_ring_base_vaddr;
	u32 chan_ring_len;
	u64 chan_ring_base_addr;
	void *chan_ring_base_vaddr;
	u64 evt_ring_rp_addr;
	void *evt_ring_rp_vaddr;
};

struct ipa3_status_stats {
	struct ipahal_pkt_status status[IPA_MAX_STATUS_STAT_NUM];
	unsigned int curr;
};

/**
 * struct ipa3_ep_context - IPA end point context
 * @valid: flag indicating id EP context is valid
 * @client: EP client type
 * @gsi_chan_hdl: EP's GSI channel handle
 * @gsi_evt_ring_hdl: EP's GSI channel event ring handle
 * @gsi_mem_info: EP's GSI channel rings info
 * @chan_scratch: EP's GSI channel scratch info
 * @cfg: EP cionfiguration
 * @dst_pipe_index: destination pipe index
 * @rt_tbl_idx: routing table index
 * @priv: user provided information which will forwarded once the user is
 *        notified for new data avail
 * @client_notify: user provided CB for EP events notification, the event is
 *                 data revived.
 * @skip_ep_cfg: boolean field that determines if EP should be configured
 *  by IPA driver
 * @keep_ipa_awake: when true, IPA will not be clock gated
 * @disconnect_in_progress: Indicates client disconnect in progress.
 * @qmi_request_sent: Indicates whether QMI request to enable clear data path
 *					request is sent or not.
 * @client_lock_unlock: callback function to take mutex lock/unlock for USB
 *				clients
 */
struct ipa3_ep_context {
	int valid;
	enum ipa_client_type client;
	unsigned long gsi_chan_hdl;
	unsigned long gsi_evt_ring_hdl;
	struct ipa_gsi_ep_mem_info gsi_mem_info;
	union __packed gsi_channel_scratch chan_scratch;
	struct gsi_chan_xfer_notify xfer_notify;
	bool xfer_notify_valid;
	struct ipa_ep_cfg cfg;
	struct ipa_ep_cfg_holb holb;
	struct ipahal_reg_ep_cfg_status status;
	u32 dst_pipe_index;
	u32 rt_tbl_idx;
	void *priv;
	void (*client_notify)(void *priv, enum ipa_dp_evt_type evt,
		       unsigned long data);
	atomic_t avail_fifo_desc;
	u32 dflt_flt4_rule_hdl;
	u32 dflt_flt6_rule_hdl;
	u32 dl_flt4_rule_hdl;
	u32 dl_flt6_rule_hdl;
	bool skip_ep_cfg;
	bool keep_ipa_awake;
	struct ipa3_wlan_stats wstats;
	u32 uc_offload_state;
	u32 gsi_offload_state;
	atomic_t disconnect_in_progress;
	u32 qmi_request_sent;
	u32 eot_in_poll_err;
	bool ep_delay_set;

	/* sys MUST be the last element of this struct */
	struct ipa3_sys_context *sys;
};

/**
 * ipa_usb_xdci_chan_params - xDCI channel related properties
 *
 * @ipa_ep_cfg:          IPA EP configuration
 * @client:              type of "client"
 * @priv:                callback cookie
 * @notify:              callback
 *           priv - callback cookie evt - type of event data - data relevant
 *           to event.  May not be valid. See event_type enum for valid
 *           cases.
 * @skip_ep_cfg:         boolean field that determines if EP should be
 *                       configured by IPA driver
 * @keep_ipa_awake:      when true, IPA will not be clock gated
 * @evt_ring_params:     parameters for the channel's event ring
 * @evt_scratch:         parameters for the channel's event ring scratch
 * @chan_params:         parameters for the channel
 * @chan_scratch:        parameters for the channel's scratch
 *
 */
struct ipa_request_gsi_channel_params {
	struct ipa_ep_cfg ipa_ep_cfg;
	enum ipa_client_type client;
	void *priv;
	ipa_notify_cb notify;
	bool skip_ep_cfg;
	bool keep_ipa_awake;
	struct gsi_evt_ring_props evt_ring_params;
	union __packed gsi_evt_scratch evt_scratch;
	struct gsi_chan_props chan_params;
	union __packed gsi_channel_scratch chan_scratch;
};

enum ipa3_sys_pipe_policy {
	IPA_POLICY_INTR_MODE,
	IPA_POLICY_NOINTR_MODE,
	IPA_POLICY_INTR_POLL_MODE,
};

struct ipa3_repl_ctx {
	struct ipa3_rx_pkt_wrapper **cache;
	atomic_t head_idx;
	atomic_t tail_idx;
	u32 capacity;
	atomic_t pending;
};

struct ipa3_page_repl_ctx {
	struct list_head page_repl_head;
	u32 capacity;
	atomic_t pending;
};

/**
 * struct ipa3_sys_context - IPA GPI pipes context
 * @head_desc_list: header descriptors list
 * @len: the size of the above list
 * @spinlock: protects the list and its size
 * @ep: IPA EP context
 * @xmit_eot_cnt: count of pending eot for tasklet to process
 * @tasklet: tasklet for eot write_done handle (tx_complete)
 * @napi_tx: napi for eot write done handle (tx_complete) - to replace tasklet
 * @napi_rx: napi for eot write done handle (rx_complete) - to replace tasklet
 * @in_napi_context: an atomic variable used for non-blocking locking,
 * preventing from multiple napi_sched to be called.
 * @int_modt: GSI event ring interrupt moderation timer
 * @int_modc: GSI event ring interrupt moderation counter
 * @buff_size: rx packet length
 * @page_order: page order of the rx pipe based on the ioctl version
 * @ext_ioctl_v2: specifies if it's new version of ingress/egress ioctl
 *
 * IPA context specific to the GPI pipes a.k.a LAN IN/OUT and WAN
 */
struct ipa3_sys_context {
	u32 len;
	atomic_t curr_polling_state;
	atomic_t workqueue_flushed;
	struct delayed_work switch_to_intr_work;
	enum ipa3_sys_pipe_policy policy;
	bool use_comm_evt_ring;
	bool nop_pending;
	int (*pyld_hdlr)(struct sk_buff *skb, struct ipa3_sys_context *sys);
	struct sk_buff * (*get_skb)(unsigned int len, gfp_t flags);
	void (*free_skb)(struct sk_buff *skb);
	void (*free_rx_wrapper)(struct ipa3_rx_pkt_wrapper *rk_pkt);
	u32 rx_buff_sz;
	u32 rx_pool_sz;
	struct sk_buff *prev_skb;
	unsigned int len_rem;
	unsigned int len_pad;
	unsigned int len_partial;
	bool drop_packet;
	struct work_struct work;
	struct delayed_work replenish_rx_work;
	struct work_struct repl_work;
	void (*repl_hdlr)(struct ipa3_sys_context *sys);
	struct ipa3_repl_ctx *repl;
	u32 pkt_sent;
	struct napi_struct *napi_obj;
	struct list_head pending_pkts[GSI_VEID_MAX];
	atomic_t xmit_eot_cnt;
	struct tasklet_struct tasklet;
	bool skip_eot;
	u32 eob_drop_cnt;
	struct napi_struct napi_tx;
	struct napi_struct napi_rx;
	bool tx_poll;
	bool napi_tx_enable;
	atomic_t in_napi_context;
	u32 int_modt;
	u32 int_modc;
	u32 buff_size;
	u32 page_order;
	bool ext_ioctl_v2;
	bool common_buff_pool;
	atomic_t page_avilable;
	u32 napi_sort_page_thrshld_cnt;

	/* ordering is important - mutable fields go above */
	struct ipa3_ep_context *ep;
	struct list_head head_desc_list;
	struct list_head rcycl_list;
	struct list_head avail_tx_wrapper_list;
	u32 avail_tx_wrapper;
	spinlock_t spinlock;
	struct hrtimer db_timer;
	struct workqueue_struct *wq;
	struct workqueue_struct *repl_wq;
	struct ipa3_status_stats *status_stat;
	u32 pm_hdl;
	struct ipa3_page_repl_ctx *page_recycle_repl;
	struct workqueue_struct *freepage_wq;
	struct delayed_work freepage_work;
	struct tasklet_struct tasklet_find_freepage;
	struct ipa3_sys_context *common_sys;
	/* ordering is important - other immutable fields go below */
};

/**
 * enum ipa3_desc_type - IPA decriptors type
 *
 * IPA decriptors type, IPA supports DD and ICD but no CD
 */
enum ipa3_desc_type {
	IPA_DATA_DESC,
	IPA_DATA_DESC_SKB,
	IPA_DATA_DESC_SKB_PAGED,
	IPA_IMM_CMD_DESC,
};

/**
 * struct ipa3_tx_pkt_wrapper - IPA Tx packet wrapper
 * @type: specify if this packet is for the skb or immediate command
 * @mem: memory buffer used by this Tx packet
 * @link: linked to the wrappers on that pipe
 * @callback: IPA client provided callback
 * @user1: cookie1 for above callback
 * @user2: cookie2 for above callback
 * @sys: corresponding IPA sys context
 * @cnt: 1 for single transfers,
 * >1 and <0xFFFF for first of a "multiple" transfer,
 * 0xFFFF for last desc, 0 for rest of "multiple' transfer
 * @bounce: va of bounce buffer
 * @unmap_dma: in case this is true, the buffer will not be dma unmapped
 * @xmit_done: flag to indicate the last desc got tx complete on each ieob
 *
 * This struct can wrap both data packet and immediate command packet.
 */
struct ipa3_tx_pkt_wrapper {
	enum ipa3_desc_type type;
	struct ipa_mem_buffer mem;
	struct list_head link;
	void (*callback)(void *user1, int user2);
	void *user1;
	int user2;
	struct ipa3_sys_context *sys;
	u32 cnt;
	void *bounce;
	bool no_unmap_dma;
	bool xmit_done;
};

/**
 * struct ipa3_dma_xfer_wrapper - IPADMA transfer descr wrapper
 * @phys_addr_src: physical address of the source data to copy
 * @phys_addr_dest: physical address to store the copied data
 * @len: len in bytes to copy
 * @link: linked to the wrappers list on the proper(sync/async) cons pipe
 * @xfer_done: completion object for sync_memcpy completion
 * @callback: IPADMA client provided completion callback
 * @user1: cookie1 for above callback
 *
 * This struct can wrap both sync and async memcpy transfers descriptors.
 */
struct ipa3_dma_xfer_wrapper {
	u64 phys_addr_src;
	u64 phys_addr_dest;
	u16 len;
	struct list_head link;
	struct completion xfer_done;
	void (*callback)(void *user1);
	void *user1;
};

/**
 * struct ipa3_desc - IPA descriptor
 * @type: skb or immediate command or plain old data
 * @pyld: points to skb
 * @frag: points to paged fragment
 * or kmalloc'ed immediate command parameters/plain old data
 * @dma_address: dma mapped address of pyld
 * @dma_address_valid: valid field for dma_address
 * @is_tag_status: flag for IP_PACKET_TAG_STATUS imd cmd
 * @len: length of the pyld
 * @opcode: for immediate commands
 * @callback: IPA client provided completion callback
 * @user1: cookie1 for above callback
 * @user2: cookie2 for above callback
 * @xfer_done: completion object for sync completion
 * @skip_db_ring: specifies whether GSI doorbell should not be rang
 */
struct ipa3_desc {
	enum ipa3_desc_type type;
	void *pyld;
	skb_frag_t *frag;
	dma_addr_t dma_address;
	bool dma_address_valid;
	bool is_tag_status;
	u16 len;
	u16 opcode;
	void (*callback)(void *user1, int user2);
	void *user1;
	int user2;
	struct completion xfer_done;
	bool skip_db_ring;
};

/**
 * struct ipa3_rx_pkt_wrapper - IPA Rx packet wrapper
 * @skb: skb
 * @dma_address: DMA address of this Rx packet
 * @link: linked to the Rx packets on that pipe
 * @len: fixed allocated skb length (i.e. times of page size)
 * @data_len: how many bytes are copied into skb's flat buffer
 */
struct ipa3_rx_pkt_wrapper {
	struct list_head link;
	union {
		struct ipa_rx_data data;
		struct ipa_rx_page_data page_data;
	};
	u32 len;
	u32 data_len;
	struct work_struct work;
	struct ipa3_sys_context *sys;
};

/**
 * struct ipa3_nat_ipv6ct_tmp_mem - NAT/IPv6CT temporary memory
 *
 * In case NAT/IPv6CT table are destroyed the HW is provided with the
 * temporary memory
 *
 * @vaddr: the address of the temporary memory
 * @dma_handle: the handle of the temporary memory
 */
struct ipa3_nat_ipv6ct_tmp_mem {
	void *vaddr;
	dma_addr_t dma_handle;
};

/**
 * struct ipa3_nat_ipv6ct_common_mem - IPA NAT/IPv6CT memory device
 * @name: the device name
 * @lock: memory mutex
 * @class: pointer to the struct class
 * @dev: the dev_t of the device
 * @cdev: cdev of the device
 * @dev_num: device number
 * @is_nat_mem: is the memory for v4 nat
 * @is_ipv6ct_mem: is the memory for v6 nat
 * @is_dev_init: flag indicating if device is initialized
 * @is_hw_init: flag indicating if the corresponding HW is initialized
 * @is_mapped: flag indicating if memory is mapped
 * @phys_mem_size: the physical size in the shared memory
 * @phys_mem_ofst: the offset in the shared memory
 * @table_alloc_size: size (bytes) of table
 * @vaddr: the virtual address in the system memory
 * @dma_handle: the system memory DMA handle
 * @base_address: table virtual address
 * @base_table_addr: base table address
 * @expansion_table_addr: expansion table address
 * @table_entries: num of entries in the base table
 * @expn_table_entries: num of entries in the expansion table
 * @tmp_mem: temporary memory used to always provide HW with a legal memory
 */
struct ipa3_nat_ipv6ct_common_mem {
	char           name[IPA_DEV_NAME_MAX_LEN];
	struct mutex   lock;
	struct class  *class;
	struct device *dev;
	struct cdev    cdev;
	dev_t          dev_num;

	bool           is_nat_mem;
	bool           is_ipv6ct_mem;

	bool           is_dev_init;
	bool           is_hw_init;
	bool           is_mapped;

	u32            phys_mem_size;
	u32            phys_mem_ofst;
	size_t         table_alloc_size;

	void          *vaddr;
	dma_addr_t     dma_handle;
	void          *base_address;
	char          *base_table_addr;
	char          *expansion_table_addr;
	u32            table_entries;
	u32            expn_table_entries;

	struct ipa3_nat_ipv6ct_tmp_mem *tmp_mem;
};

/**
 * struct ipa3_nat_mem_loc_data - memory specific info per table memory type
 * @is_mapped: has the memory been mapped?
 * @io_vaddr: the virtual address in the sram memory
 * @vaddr: the virtual address in the system memory
 * @dma_handle: the system memory DMA handle
 * @phys_addr: physical sram memory location
 * @table_alloc_size: size (bytes) of table
 * @table_entries: number of entries in table
 * @expn_table_entries: number of entries in expansion table
 * @base_address: same as vaddr above
 * @base_table_addr: base table address
 * @expansion_table_addr: base table's expansion table address
 * @index_table_addr: index table address
 * @index_table_expansion_addr: index table's expansion table address
 */
struct ipa3_nat_mem_loc_data {
	bool          is_mapped;

	void __iomem *io_vaddr;

	void         *vaddr;
	dma_addr_t    dma_handle;

	unsigned long phys_addr;

	size_t        table_alloc_size;

	u32           table_entries;
	u32           expn_table_entries;

	void         *base_address;

	char         *base_table_addr;
	char         *expansion_table_addr;

	char         *index_table_addr;
	char         *index_table_expansion_addr;
};

/**
 * struct ipa3_nat_mem - IPA NAT memory description
 * @dev: the memory device structure
 * @public_ip_addr: ip address of nat table
 * @pdn_mem: pdn config table SW cache memory structure
 * @is_tmp_mem_allocated: indicate if tmp mem has been allocated
 * @last_alloc_loc: last memory type allocated
 * @active_table: which table memory type is currently active
 * @switch2ddr_cnt: how many times we've switched focust to ddr
 * @switch2sram_cnt: how many times we've switched focust to sram
 * @ddr_in_use: is there table in ddr
 * @sram_in_use: is there table in sram
 * @mem_loc: memory specific info per table memory type
 */
struct ipa3_nat_mem {
	struct ipa3_nat_ipv6ct_common_mem dev; /* this item must be first */

	u32                          public_ip_addr;
	struct ipa_mem_buffer        pdn_mem;

	bool                         is_tmp_mem_allocated;

	enum ipa3_nat_mem_in         last_alloc_loc;

	enum ipa3_nat_mem_in         active_table;
	u32                          switch2ddr_cnt;
	u32                          switch2sram_cnt;

	bool                         ddr_in_use;
	bool                         sram_in_use;

	struct ipa3_nat_mem_loc_data mem_loc[IPA_NAT_MEM_IN_MAX];
};

/**
 * struct ipa3_ipv6ct_mem - IPA IPv6 connection tracking memory description
 * @dev: the memory device structure
 */
struct ipa3_ipv6ct_mem {
	struct ipa3_nat_ipv6ct_common_mem dev; /* this item must be first */
};

/**
 * enum ipa3_hw_mode - IPA hardware mode
 * @IPA_HW_Normal: Regular IPA hardware
 * @IPA_HW_Virtual: IPA hardware supporting virtual memory allocation
 * @IPA_HW_PCIE: IPA hardware supporting memory allocation over PCIE Bridge
 * @IPA_HW_Emulation: IPA emulation hardware
 * @IPA_HW_Test: Regular IPA hardware in test mode (for
 *             kernel-tests)
 */
enum ipa3_hw_mode {
	IPA_HW_MODE_NORMAL    = 0,
	IPA_HW_MODE_VIRTUAL   = 1,
	IPA_HW_MODE_PCIE      = 2,
	IPA_HW_MODE_EMULATION = 3,
	IPA_HW_MODE_TEST      = 4,
};

#define IPA_IS_REGULAR_CLK_MODE(hw_mode) \
	((hw_mode == IPA_HW_MODE_NORMAL) || (hw_mode == IPA_HW_MODE_TEST))

/*
 * enum ipa3_platform_type - Platform type
 * @IPA_PLAT_TYPE_MDM: MDM platform (usually 32bit single core CPU platform)
 * @IPA_PLAT_TYPE_MSM: MSM SOC platform (usually 64bit multi-core platform)
 * @IPA_PLAT_TYPE_APQ: Similar to MSM but without modem
 */
enum ipa3_platform_type {
	IPA_PLAT_TYPE_MDM	= 0,
	IPA_PLAT_TYPE_MSM	= 1,
	IPA_PLAT_TYPE_APQ	= 2,
	IPA_PLAT_TYPE_XR	= 3,
};

enum ipa3_config_this_ep {
	IPA_CONFIGURE_THIS_EP,
	IPA_DO_NOT_CONFIGURE_THIS_EP,
};

struct ipa3_page_recycle_stats {
	u64 total_replenished;
	u64 page_recycled;
	u64 tmp_alloc;
};

struct ipa3_cache_recycle_stats {
	u64 pkt_allocd;
	u64 pkt_found;
	u64 tot_pkt_replenished;
};

struct lan_coal_stats {
	u64 coal_rx;
	u64 coal_left_as_is;
	u64 coal_reconstructed;
	u64 coal_pkts;
	u64 coal_hdr_qmap_err;
	u64 coal_hdr_nlo_err;
	u64 coal_hdr_pkt_err;
	u64 coal_csum_err;
	u64 coal_ip_invalid;
	u64 coal_trans_invalid;
	u64 coal_veid[GSI_VEID_MAX];
	u64 coal_tcp;
	u64 coal_tcp_bytes;
	u64 coal_udp;
	u64 coal_udp_bytes;
};

struct ipa3_stats {
	u32 tx_sw_pkts;
	u32 tx_hw_pkts;
	u32 rx_pkts;
	u32 rx_excp_pkts[IPAHAL_PKT_STATUS_EXCEPTION_MAX];
	u32 rx_repl_repost;
	u32 tx_pkts_compl;
	u32 rx_q_len;
	u32 msg_w[IPA_EVENT_MAX_NUM];
	u32 msg_r[IPA_EVENT_MAX_NUM];
	u32 stat_compl;
	u32 aggr_close;
	u32 wan_aggr_close;
	u32 wan_rx_empty;
	u32 wan_rx_empty_coal;
	u32 wan_repl_rx_empty;
	u32 rmnet_ll_rx_empty;
	u32 rmnet_ll_repl_rx_empty;
	u32 lan_rx_empty;
	u32 lan_rx_empty_coal;
	u32 lan_repl_rx_empty;
	u32 low_lat_rx_empty;
	u32 low_lat_repl_rx_empty;
	u32 flow_enable;
	u32 flow_disable;
	u32 tx_non_linear;
	u32 rx_page_drop_cnt;
	u64 lower_order;
	u32 pipe_setup_fail_cnt;
	struct ipa3_page_recycle_stats page_recycle_stats[3];
	struct ipa3_cache_recycle_stats cache_recycle_stats[3];
	u64 page_recycle_cnt[3][IPA_PAGE_POLL_THRESHOLD_MAX];
	atomic_t num_buff_above_thresh_for_def_pipe_notified;
	atomic_t num_buff_above_thresh_for_coal_pipe_notified;
	atomic_t num_buff_below_thresh_for_def_pipe_notified;
	atomic_t num_buff_below_thresh_for_coal_pipe_notified;
	atomic_t num_buff_above_thresh_for_ll_pipe_notified;
	atomic_t num_buff_below_thresh_for_ll_pipe_notified;
	atomic_t num_free_page_task_scheduled;
	struct lan_coal_stats coal;
	u64 num_sort_tasklet_sched[3];
	u64 num_of_times_wq_reschd;
	u64 page_recycle_cnt_in_tasklet;
	u32 ttl_cnt;
};

/* offset for each stats */
#define IPA3_UC_DEBUG_STATS_RINGFULL_OFF (0)
#define IPA3_UC_DEBUG_STATS_RINGEMPTY_OFF (4)
#define IPA3_UC_DEBUG_STATS_RINGUSAGEHIGH_OFF (8)
#define IPA3_UC_DEBUG_STATS_RINGUSAGELOW_OFF (12)
#define IPA3_UC_DEBUG_STATS_RINGUTILCOUNT_OFF (16)
#define IPA3_UC_DEBUG_STATS_OFF (20)
#define IPA3_UC_DEBUG_STATS_TRCOUNT_OFF (20)
#define IPA3_UC_DEBUG_STATS_ERCOUNT_OFF (24)
#define IPA3_UC_DEBUG_STATS_AOSCOUNT_OFF (28)
#define IPA3_UC_DEBUG_STATS_BUSYTIME_OFF (32)
#define IPA3_UC_DEBUG_STATS_RTK_OFF (40)


/**
 * struct ipa3_uc_dbg_stats - uC dbg stats for offloading
 * protocols
 * @uc_dbg_stats_ofst: offset to SRAM base
 * @uc_dbg_stats_size: stats size for all channels
 * @uc_dbg_stats_mmio: mmio offset
 */
struct ipa3_uc_dbg_stats {
	u32 uc_dbg_stats_ofst;
	u16 uc_dbg_stats_size;
	void __iomem *uc_dbg_stats_mmio;
};

struct ipa3_active_clients {
	struct mutex mutex;
	atomic_t cnt;
	int bus_vote_idx;
};

struct ipa3_wakelock_ref_cnt {
	spinlock_t spinlock;
	int cnt;
};

struct ipa3_tag_completion {
	struct completion comp;
	atomic_t cnt;
};

struct ipa3_controller;

enum ipa_ees {
	IPA_EE_AP = 0,
	IPA_EE_Q6 = 1,
	IPA_EE_UC = 2,
};

/**
 * struct ipa3_uc_hdlrs - IPA uC callback functions
 * @ipa_uc_loaded_hdlr: Function handler when uC is loaded
 * @ipa_uc_event_hdlr: Event handler function
 * @ipa3_uc_response_hdlr: Response handler function
 * @ipa_uc_event_log_info_hdlr: Log event handler function
 * @ipa_uc_holb_enabled_hdlr: Function handler when uC HOLB is enabled
 */
struct ipa3_uc_hdlrs {
	void (*ipa_uc_loaded_hdlr)(void);

	void (*ipa_uc_event_hdlr)
		(struct IpaHwSharedMemCommonMapping_t *uc_sram_mmio);

	int (*ipa3_uc_response_hdlr)
		(struct IpaHwSharedMemCommonMapping_t *uc_sram_mmio,
		u32 *uc_status);

	void (*ipa_uc_event_log_info_hdlr)
		(struct IpaHwEventLogInfoData_t *uc_event_top_mmio);

	void (*ipa_uc_holb_enabled_hdlr)(void);
};

/**
 * enum ipa3_hw_flags - flags which defines the behavior of HW
 *
 * @IPA_HW_FLAG_HALT_SYSTEM_ON_ASSERT_FAILURE: Halt system in case of assert
 *	failure.
 * @IPA_HW_FLAG_NO_REPORT_MHI_CHANNEL_ERORR: Channel error would be reported
 *	in the event ring only. No event to CPU.
 * @IPA_HW_FLAG_NO_REPORT_MHI_CHANNEL_WAKE_UP: No need to report event
 *	IPA_HW_2_CPU_EVENT_MHI_WAKE_UP_REQUEST
 * @IPA_HW_FLAG_WORK_OVER_DDR: Perform all transaction to external addresses by
 *	QMB (avoid memcpy)
 * @IPA_HW_FLAG_NO_REPORT_OOB: If set do not report that the device is OOB in
 *	IN Channel
 * @IPA_HW_FLAG_NO_REPORT_DB_MODE: If set, do not report that the device is
 *	entering a mode where it expects a doorbell to be rung for OUT Channel
 * @IPA_HW_FLAG_NO_START_OOB_TIMER
 */
enum ipa3_hw_flags {
	IPA_HW_FLAG_HALT_SYSTEM_ON_ASSERT_FAILURE	= 0x01,
	IPA_HW_FLAG_NO_REPORT_MHI_CHANNEL_ERORR		= 0x02,
	IPA_HW_FLAG_NO_REPORT_MHI_CHANNEL_WAKE_UP	= 0x04,
	IPA_HW_FLAG_WORK_OVER_DDR			= 0x08,
	IPA_HW_FLAG_NO_REPORT_OOB			= 0x10,
	IPA_HW_FLAG_NO_REPORT_DB_MODE			= 0x20,
	IPA_HW_FLAG_NO_START_OOB_TIMER			= 0x40
};

/**
 * struct ipa3_uc_ctx - IPA uC context
 * @uc_inited: Indicates if uC interface has been initialized
 * @uc_loaded: Indicates if uC has loaded
 * @uc_failed: Indicates if uC has failed / returned an error
 * @uc_holb_enabled: Indicates if uC HOLB enable cmd is sent.
 * @uc_lock: uC interface lock to allow only one uC interaction at a time
 * @uc_spinlock: same as uc_lock but for irq contexts
 * @uc_completation: Completion mechanism to wait for uC commands
 * @uc_sram_mmio: Pointer to uC mapped memory
 * @pending_cmd: The last command sent waiting to be ACKed
 * @uc_status: The last status provided by the uC
 * @uc_error_type: error type from uC error event
 * @uc_error_timestamp: tag timer sampled after uC crashed
 * @ipa_use_uc_holb_monitor: Indicates if uC HOLB feature is enabled
 * @ipa_holb_monitor: Struct with all info needed for uC HOLB feature
 */
struct ipa3_uc_ctx {
	bool uc_inited;
	bool uc_loaded;
	bool uc_failed;
	bool uc_holb_enabled;
	struct mutex uc_lock;
	spinlock_t uc_spinlock;
	struct completion uc_completion;
	struct IpaHwSharedMemCommonMapping_t *uc_sram_mmio;
	struct IpaHwEventLogInfoData_t *uc_event_top_mmio;
	u32 uc_event_top_ofst;
	u32 pending_cmd;
	u32 uc_status;
	u32 uc_error_type;
	u32 uc_error_timestamp;
	phys_addr_t rdy_ring_base_pa;
	phys_addr_t rdy_ring_rp_pa;
	u32 rdy_ring_size;
	phys_addr_t rdy_comp_ring_base_pa;
	phys_addr_t rdy_comp_ring_wp_pa;
	u32 rdy_comp_ring_size;
	u32 *rdy_ring_rp_va;
	u32 *rdy_comp_ring_wp_va;
	bool uc_event_ring_valid;
	struct ipa_mem_buffer event_ring;
	u32 ering_wp_local;
	u32 ering_rp_local;
	u32 ering_wp;
	u32 ering_rp;
	bool ipa_use_uc_holb_monitor;
	struct ipa_holb_monitor holb_monitor;
};

/**
 * struct ipa3_uc_wdi_ctx
 * @wdi_uc_top_ofst:
 * @wdi_uc_top_mmio:
 * @wdi_uc_stats_ofst:
 * @wdi_uc_stats_mmio:
 */
struct ipa3_uc_wdi_ctx {
	/* WDI specific fields */
	u32 wdi_uc_stats_ofst;
	struct IpaHwStatsWDIInfoData_t *wdi_uc_stats_mmio;
	void *priv;
	ipa_uc_ready_cb uc_ready_cb;
	/* for AP+STA stats update */
#ifdef IPA_WAN_MSG_IPv6_ADDR_GW_LEN
	ipa_wdi_meter_notifier_cb stats_notify;
#endif
};

/**
 * struct ipa3_uc_wigig_ctx
 * @priv: wigig driver private data
 * @uc_ready_cb: wigig driver uc ready callback
 * @int_notify: wigig driver misc interrupt callback
 */
struct ipa3_uc_wigig_ctx {
	void *priv;
	ipa_uc_ready_cb uc_ready_cb;
	ipa_wigig_misc_int_cb misc_notify_cb;
};

/**
 * struct ipa3_wdi2_ctx - IPA wdi2 context
 */
struct ipa3_wdi2_ctx {
	phys_addr_t rdy_ring_base_pa;
	phys_addr_t rdy_ring_rp_pa;
	u32 rdy_ring_size;
	phys_addr_t rdy_comp_ring_base_pa;
	phys_addr_t rdy_comp_ring_wp_pa;
	u32 rdy_comp_ring_size;
	u32 *rdy_ring_rp_va;
	u32 *rdy_comp_ring_wp_va;
	struct ipa3_uc_dbg_stats dbg_stats;
};

/**
 * struct ipa3_wdi3_ctx - IPA wdi3 context
 */
struct ipa3_wdi3_ctx {
	struct ipa3_uc_dbg_stats dbg_stats;
};

/**
 * struct ipa3_usb_ctx - IPA usb context
 */
struct ipa3_usb_ctx {
	struct ipa3_uc_dbg_stats dbg_stats;
};

/**
 * struct ipa3_mhip_ctx - IPA mhip context
 */
struct ipa3_mhip_ctx {
	struct ipa3_uc_dbg_stats dbg_stats;
};

/**
 * struct ipa3_aqc_ctx - IPA aqc context
 */
struct ipa3_aqc_ctx {
	struct ipa3_uc_dbg_stats dbg_stats;
};

/**
 * struct ipa3_rtk_ctx - IPA rtk context
 */
struct ipa3_rtk_ctx {
	struct ipa3_uc_dbg_stats dbg_stats;
};

/**
* struct ipa3_ntn_ctx - IPA ntn context
*/
struct ipa3_ntn_ctx {
	struct ipa3_uc_dbg_stats dbg_stats;
};

/**
 * struct ipa3_transport_pm - transport power management related members
 * @transport_pm_mutex: Mutex to protect the transport_pm functionality.
 */
struct ipa3_transport_pm {
	atomic_t dec_clients;
	atomic_t eot_activity;
	struct mutex transport_pm_mutex;
};

/**
 * struct ipa3cm_client_info - the client-info indicated from IPACM
 * @ipacm_client_enum: the enum to indicate tether-client
 * @ipacm_client_uplink: the bool to indicate pipe for uplink
 */
struct ipa3cm_client_info {
	enum ipacm_client_enum client_enum;
	bool uplink;
};

/**
 * struct ipacm_fnr_info - the fnr-info indicated from IPACM
 * @ipacm_client_enum: the enum to indicate tether-client
 * @ipacm_client_uplink: the bool to indicate pipe for uplink
 */
struct ipacm_fnr_info {
	bool valid;
	uint8_t hw_counter_offset;
	uint8_t sw_counter_offset;
};

struct ipa3_smp2p_info {
	u32 out_base_id;
	u32 in_base_id;
	bool ipa_clk_on;
	bool res_sent;
	unsigned int smem_bit;
	struct qcom_smem_state *smem_state;
};

struct ipa_dma_task_info {
	struct ipa_mem_buffer mem;
	struct ipahal_imm_cmd_pyld *cmd_pyld;
};

struct ipa_quota_stats {
	u64 num_ipv4_bytes;
	u64 num_ipv6_bytes;
	u32 num_ipv4_pkts;
	u32 num_ipv6_pkts;
};

struct ipa_quota_stats_all {
	struct ipa_quota_stats client[IPA5_PIPES_NUM];
};

struct ipa_drop_stats {
	u32 drop_packet_cnt;
	u32 drop_byte_cnt;
};

struct ipa_drop_stats_all {
	struct ipa_drop_stats client[IPA_CLIENT_MAX];
};

struct ipa_hw_stats_quota {
	struct ipahal_stats_init_quota init;
	struct ipa_quota_stats_all stats;
};

struct ipa_hw_stats_teth {
	struct ipahal_stats_init_tethering init;
	struct ipa_quota_stats_all prod_stats_sum[IPA5_PIPES_NUM];
	struct ipa_quota_stats_all prod_stats[IPA5_PIPES_NUM];
};

struct ipa_hw_stats_flt_rt {
	struct ipahal_stats_init_flt_rt flt_v4_init;
	struct ipahal_stats_init_flt_rt flt_v6_init;
	struct ipahal_stats_init_flt_rt rt_v4_init;
	struct ipahal_stats_init_flt_rt rt_v6_init;
};

struct ipa_hw_stats_drop {
	struct ipahal_stats_init_drop init;
	struct ipa_drop_stats_all stats;
};

struct ipa_hw_stats {
	bool enabled;
	struct ipa_hw_stats_quota quota;
	struct ipa_hw_stats_teth teth;
	struct ipa_hw_stats_flt_rt flt_rt;
	struct ipa_hw_stats_drop drop;
	bool teth_stats_enabled;
};

struct ipa_cne_evt {
	struct ipa_wan_msg wan_msg;
	struct ipa_msg_meta msg_meta;
};

enum ipa_smmu_cb_type {
	IPA_SMMU_CB_AP,
	IPA_SMMU_CB_WLAN,
	IPA_SMMU_CB_WLAN1,
	IPA_SMMU_CB_UC,
	IPA_SMMU_CB_11AD,
	IPA_SMMU_CB_ETH,
	IPA_SMMU_CB_ETH1,
	IPA_SMMU_CB_RTP,
	IPA_SMMU_CB_MAX
};

#define VALID_IPA_SMMU_CB_TYPE(t) \
	((t) >= IPA_SMMU_CB_AP && (t) < IPA_SMMU_CB_MAX)

enum ipa_client_cb_type {
	IPA_USB_CLNT,
	IPA_MHI_CLNT,
	IPA_MAX_CLNT
};

/**
 * struct ipa_flt_rt_counter - IPA flt rt counters management
 * @hdl: idr structure to manage hdl per request
 * @used_hw: boolean array to track used hw counters
 * @used_sw: boolean array to track used sw counters
 * @hdl_lock: spinlock for flt_rt handle
 */
struct ipa_flt_rt_counter {
	struct idr hdl;
	bool used_hw[IPA_FLT_RT_HW_COUNTER];
	bool used_sw[IPA_FLT_RT_SW_COUNTER];
	spinlock_t hdl_lock;
};

/**
 * struct ipa3_char_device_context - IPA character device
 * @class: pointer to the struct class
 * @dev_num: device number
 * @dev: the dev_t of the device
 * @cdev: cdev of the device
 */
struct ipa3_char_device_context {
	struct class *class;
	dev_t dev_num;
	struct device *dev;
	struct cdev cdev;
};

struct ipa3_pc_mbox_data {
	struct mbox_client mbox_client;
	struct mbox_chan *mbox;
};

enum ipa_fw_load_state {
	IPA_FW_LOAD_STATE_INIT,
	IPA_FW_LOAD_STATE_FWFILE_READY,
	IPA_FW_LOAD_STATE_SMMU_DONE,
	IPA_FW_LOAD_STATE_LOAD_READY,
	IPA_FW_LOAD_STATE_LOADED,
};

enum ipa_fw_load_event {
	IPA_FW_LOAD_EVNT_FWFILE_READY,
	IPA_FW_LOAD_EVNT_SMMU_DONE,
};

struct ipa_fw_load_data {
	enum ipa_fw_load_state state;
	struct mutex lock;
};

struct ipa3_app_clock_vote {
	struct mutex mutex;
	u32 cnt;
};

struct ipa_eth_client_mapping {
	enum ipa_client_type type;
	int pipe_id;
	int pipe_hdl;
	int ch_id;
	bool valid;
};

struct ipa3_eth_info {
	u8 num_ch;
	struct ipa_eth_client_mapping map[IPA_MAX_CH_STATS_SUPPORTED];
};

struct ipa3_eth_error_stats {
	int rp;
	int wp;
	u32 err;
};

struct ipa_ntn3_stats_rx {
	int rp;
	int wp;
	bool pending_db_after_rollback;
	u32 msi_db_idx;
	u32 chain_cnt;
	u32 err_cnt;
	u32 tres_handled;
	u32 rollbacks_cnt;
	u32 msi_db_cnt;
};

struct ipa_ntn3_stats_tx {
	int rp;
	int wp;
	bool pending_db_after_rollback;
	u32 msi_db_idx;
	u32 derr_cnt;
	u32 oob_cnt;
	u32 tres_handled;
	u32 rollbacks_cnt;
	u32 msi_db_cnt;
};

struct ipa_ntn3_client_stats {
	struct ipa_ntn3_stats_rx rx_stats;
	struct ipa_ntn3_stats_tx tx_stats;
};
#if defined(CONFIG_IPA_TSP)
struct ipa3_tsp_ctx {
	u8 ingr_tc_max;
	u8 egr_ep_max;
	u8 egr_tc_max;
	enum ipa_client_type *egr_ep_config;
	u32 egr_tc_range_mask;
	struct ipa_mem_buffer ingr_tc_tbl;
	struct ipa_mem_buffer egr_ep_tbl;
	struct ipa_mem_buffer egr_tc_tbl;
	struct ipa_mem_buffer qm_tlv_mem;
};
#endif

#if IS_ENABLED(CONFIG_QCOM_VA_MINIDUMP)
struct ipa_minidump_data {
	struct list_head entry;
	struct va_md_entry data;
};
#endif

struct ipa_notifier_block_data {
	struct list_head entry;
	struct notifier_block ipa_rmnet_notifier;
};

/* Peripheral stats for Q6, should be in the same order, defined by Q6 */
enum ipa_per_stats_type_e {
	IPA_PER_STATS_TYPE_NUM_PERS,
	IPA_PER_STATS_TYPE_NUM_PERS_WWAN,
	IPA_PER_STATS_TYPE_ACT_PER_TYPE,
	IPA_PER_STATS_TYPE_PCIE_GEN,
	IPA_PER_STATS_TYPE_PCIE_WIDTH,
	IPA_PER_STATS_TYPE_PCIE_MAX_SPEED,
	IPA_PER_STATS_TYPE_PCIE_NUM_LPM,
	IPA_PER_STATS_TYPE_USB_TYPE,
	IPA_PER_STATS_TYPE_USB_PROT,
	IPA_PER_STATS_TYPE_USB_MAX_SPEED,
	IPA_PER_STATS_TYPE_USB_PIPO,
	IPA_PER_STATS_TYPE_WIFI_ENUM_TYPE,
	IPA_PER_STATS_TYPE_WIFI_MAX_SPEED,
	IPA_PER_STATS_TYPE_WIFI_DUAL_BAND_EN,
	IPA_PER_STATS_TYPE_ETH_CLIENT,
	IPA_PER_STATS_TYPE_ETH_MAX_SPEED,
	IPA_PER_STATS_TYPE_IPA_DMA_BYTES,
	IPA_PER_STATS_TYPE_WIFI_HOLB_UC,
	IPA_PER_STATS_TYPE_ETH_HOLB_UC,
	IPA_PER_STATS_TYPE_USB_HOLB_UC,
	IPA_PER_STATS_TYPE_MAX
};

enum ipa_per_type_bitmask_e {
	IPA_PER_TYPE_BITMASK_NONE 		= 0,
	IPA_PER_TYPE_BITMASK_PCIE_EP 	= 1,
	IPA_PER_TYPE_BITMASK_USB 		= 2,
	IPA_PER_TYPE_BITMASK_WIFI 		= 4,
	IPA_PER_TYPE_BITMASK_ETH 		= 8
};

enum ipa_per_pcie_speed_type_e {
	PCIE_LINK_SPEED_DEF  = 0, 	/** < -- Core's default speed */
	PCIE_LINK_SPEED_GEN1 = 1,	/** < -- Gen1 Speed - 2.5GT/s */
	PCIE_LINK_SPEED_GEN2 = 2,	/** < -- Gen2 Speed - 5.0GT/s */
	PCIE_LINK_SPEED_GEN3 = 3,	/** < -- Gen3 Speed - 8.0GT/s */
	PCIE_LINK_SPEED_GEN4 = 4	/** < -- Gen4 Speed - 16.0GT/s*/
};

enum ipa_per_pcie_width_type_e {
	PCIE_LINK_WIDTH_DEF = 0,		/** < -- Link Width Default */
	PCIE_LINK_WIDTH_X1  = 1,		/** < -- Link Width x1 */
	PCIE_LINK_WIDTH_X2  = 2,		/** < -- Link Width x2 */
	PCIE_LINK_WIDTH_X4  = 4,		/** < -- Link Width x4 */
	PCIE_LINK_WIDTH_X8  = 8,		/** < -- Link Width x8 */
	PCIE_LINK_WIDTH_X16 = 16,		/** < -- Link Width x16 */
	PCIE_LINK_WIDTH_MAX = 32		/** < -- Link Width Max */
};

enum ipa_per_usb_prot_type_e {
	IPA_PER_USB_PROT_TYPE_INVALID,
	IPA_PER_USB_PROT_TYPE_RMNET,
	IPA_PER_USB_PROT_TYPE_RNDIS,
	IPA_PER_USB_PROT_TYPE_ECM,
	IPA_PER_USB_PROT_TYPE_MAX
};

enum ipa_per_wifi_enum_type_e {
	IPA_PER_WIFI_ENUM_TYPE_INVALID,
	IPA_PER_WIFI_ENUM_TYPE_802_11_ABG,
	IPA_PER_WIFI_ENUM_TYPE_802_11_AC,
	IPA_PER_WIFI_ENUM_TYPE_802_11_AD,
	IPA_PER_WIFI_ENUM_TYPE_802_11_AX,
	IPA_PER_WIFI_ENUM_TYPE_MAX
};

enum ipa_per_usb_enum_type_e {
	IPA_PER_USB_ENUM_TYPE_INVALID,
	IPA_PER_USB_ENUM_TYPE_FS,
	IPA_PER_USB_ENUM_TYPE_2_0_HS,
	IPA_PER_USB_ENUM_TYPE_SS_GEN_1,
	IPA_PER_USB_ENUM_TYPE_SS_GEN_2,
	IPA_PER_USB_ENUM_TYPE_SS_GEN_2x2,
	IPA_PER_USB_ENUM_TYPE_MAX
};

/**
 * struct ipa3_context - IPA context
 * @cdev: cdev context
 * @ep: list of all end points
 * @skip_ep_cfg_shadow: state to update filter table correctly across power-save
 * @ep_flt_bitmap: End-points supporting filtering bitmap
 * @ep_flt_num: End-points supporting filtering number
 * @resume_on_connect: resume ep on ipa connect
 * @flt_tbl: list of all IPA filter tables
 * @flt_rule_ids: idr structure that holds the rule_id for each rule
 * @mode: IPA operating mode
 * @mmio: iomem
 * @ipa_wrapper_base: IPA wrapper base address
 * @ipa_wrapper_size: size of the memory pointed to by ipa_wrapper_base
 * @ipa_cfg_offset: offset from IPA_WRAPPER_BASE to IPA registers
 * @hdr_tbl: IPA header table
 * @hdr_proc_ctx_tbl: IPA processing context table
 * @rt_tbl_set: list of routing tables each of which is a list of rules
 * @reap_rt_tbl_set: list of sys mem routing tables waiting to be reaped
 * @flt_rule_cache: filter rule cache
 * @rt_rule_cache: routing rule cache
 * @hdr_cache: header cache
 * @hdr_offset_cache: header offset cache
 * @fnr_stats_cache: FnR stats cache
 * @hdr_proc_ctx_cache: processing context cache
 * @hdr_proc_ctx_offset_cache: processing context offset cache
 * @rt_tbl_cache: routing table cache
 * @tx_pkt_wrapper_cache: Tx packets cache
 * @rx_pkt_wrapper_cache: Rx packets cache
 * @rt_idx_bitmap: routing table index bitmap
 * @lock: this does NOT protect the linked lists within ipa3_sys_context
 * @smem_sz: shared memory size available for SW use starting
 *  from non-restricted bytes
 * @smem_restricted_bytes: the bytes that SW should not use in the shared mem
 * @nat_mem: NAT memory
 * @ipv6ct_mem: IPv6CT memory
 * @excp_hdr_hdl: exception header handle
 * @dflt_v4_rt_rule_hdl: default v4 routing rule handle
 * @dflt_v6_rt_rule_hdl: default v6 routing rule handle
 * @aggregation_type: aggregation type used on USB client endpoint
 * @aggregation_byte_limit: aggregation byte limit used on USB client endpoint
 * @aggregation_time_limit: aggregation time limit used on USB client endpoint
 * @hdr_proc_ctx_tbl_lcl: where proc_ctx tbl resides true-local, false-system
 * @hdr_mem: header memory
 * @hdr_proc_ctx_mem: processing context memory
 * @ip4_rt_tbl_lcl: where ip4 rt tables reside 1-local; 0-system
 * @ip6_rt_tbl_lcl: where ip6 rt tables reside 1-local; 0-system
 * @ip4_flt_tbl_lcl: where ip4 flt tables reside 1-local; 0-system
 * @ip6_flt_tbl_lcl: where ip6 flt tables reside 1-local; 0-system
 * @power_mgmt_wq: workqueue for power management
 * @transport_power_mgmt_wq: workqueue transport related power management
 * @xr_uc_init_wq: workqueue for uc initializations
 * @tag_process_before_gating: indicates whether to start tag process before
 *  gating IPA clocks
 * @transport_pm: transport power management related information
 * @disconnect_lock: protects LAN_CONS packet receive notification CB
 * @ipa3_active_clients: structure for reference counting connected IPA clients
 * @ipa_hw_type: type of IPA HW type (e.g. IPA 1.0, IPA 1.1 etc')
 * @ipa_hw_type_index: index of IPA HW type (e.g. IPA_4_0, IPA_4_0_MHI etc')
 * @ipa3_hw_mode: mode of IPA HW mode (e.g. Normal, Virtual or over PCIe)
 * @gsi_ver: version of GSI
 * @use_ipa_teth_bridge: use tethering bridge driver
 * @modem_cfg_emb_pipe_flt: modem configure embedded pipe filtering rules
 * @logbuf: ipc log buffer for high priority messages
 * @logbuf_low: ipc log buffer for low priority messages
 * @logbuf_clk: ipc log buffer for ipa clock messages
 * @ipa_wdi2: using wdi-2.0
 * @ipa_config_is_auto: is this AUTO use case
 * @ipa_fltrt_not_hashable: filter/route rules not hashable
 * @use_xbl_boot: use xbl loading for IPA FW
 * @use_64_bit_dma_mask: using 64bits dma mask
 * @ctrl: holds the core specific operations based on
 *  core version (vtable like)
 * @pkt_init_imm_opcode: opcode for IP_PACKET_INIT imm cmd
 * @enable_clock_scaling: clock scaling is enabled ?
 * @curr_ipa_clk_rate: IPA current clock rate
 * @wcstats: wlan common buffer stats
 * @uc_ctx: uC interface context
 * @uc_wdi_ctx: WDI specific fields for uC interface
 * @uc_wigig_ctx: WIGIG specific fields for uC interface
 * @ipa_num_pipes: The number of pipes used by IPA HW
 * @skip_uc_pipe_reset: Indicates whether pipe reset via uC needs to be avoided
 * @mpm_ring_size_dl_cache: To cache the dl ring size configured previously
 * @mpm_ring_size_dl: MHIP all DL pipe's ring size
 * @mpm_ring_size_ul_cache: To cache the ul ring size configured previously
 * @mpm_ring_size_ul: MHIP all UL pipe's ring size
 * @mpm_teth_aggr_size: MHIP teth aggregation byte size
 * @mpm_uc_thresh: uc threshold for enabling uc flow control
 * @ipa_client_apps_wan_cons_agg_gro: RMNET_IOCTL_INGRESS_FORMAT_AGG_DATA
 * @apply_rg10_wa: Indicates whether to use register group 10 workaround
 * @gsi_ch20_wa: Indicates whether to apply GSI physical channel 20 workaround
 * @w_lock: Indicates the wakeup source.
 * @wakelock_ref_cnt: Indicates the number of times wakelock is acquired
 * @ipa_initialization_complete: Indicates that IPA is fully initialized
 * @ipa_ready_cb_list: A list of all the clients who require a CB when IPA
 *  driver is ready/initialized.
 * @init_completion_obj: Completion object to be used in case IPA driver hasn't
 * @mhi_evid_limits: MHI event rings start and end ids
 *  finished initializing. Example of use - IOCTLs to /dev/ipa
 * @flt_rt_counters: the counters usage info for flt rt stats
 * @wdi3_ctx: IPA wdi3 context
 * @gsi_info: channel/protocol info for GSI offloading uC stats
 * @app_vote: holds userspace application clock vote count
 * IPA context - holds all relevant info about IPA driver and its state
 * @lan_rx_napi_enable: flag if NAPI is enabled on the LAN dp
 * @generic_ndev: dummy netdev for LAN rx NAPI and tx NAPI
 * @napi_lan_rx: NAPI object for LAN rx
 * @ipa_wan_skb_page - page recycling enabled on wwan data path
 * @icc_num_cases - number of icc scaling level supported
 * @icc_num_paths - number of paths icc would vote for bw
 * @icc_clk - table for icc bw clock value
 * @coal_cmd_pyld: holds the coslescing close frame command payload
 * @ipa_gpi_event_rp_ddr: use DDR to access event RP for GPI channels
 * @rmnet_ctl_enable: enable pipe support fow low latency data
 * @rmnet_ll_enable: enable pipe support fow low latency data
 * @gsi_fw_file_name: GSI IPA fw file name
 * @uc_fw_file_name: uC IPA fw file name
 * @eth_info: ethernet client mapping
 * @max_num_smmu_cb: number of smmu s1 cb supported
 * @non_hash_flt_lcl_sys_switch: number of times non-hash flt table moved
 * mhi_ctrl_state: state of mhi ctrl pipes
 * @per_stats_smem_pa: Peripheral stats physical address to be passed to Q6
 * @per_stats_smem_va: Peripheral stats virtual address to update stats from Apps
 */
struct ipa3_context {
	bool coal_stopped;
	struct ipa3_char_device_context cdev;
	struct ipa3_ep_context ep[IPA5_MAX_NUM_PIPES];
	bool skip_ep_cfg_shadow[IPA5_MAX_NUM_PIPES];
	u64 ep_flt_bitmap;
	u32 ep_flt_num;
	bool resume_on_connect[IPA_CLIENT_MAX];
	struct ipa3_flt_tbl flt_tbl[IPA5_MAX_NUM_PIPES][IPA_IP_MAX];
	struct idr flt_rule_ids[IPA_IP_MAX];
	void __iomem *mmio;
	u32 ipa_wrapper_base;
	u32 ipa_wrapper_size;
	u32 ipa_cfg_offset;
	bool set_evict_reg;
	struct ipa3_hdr_tbl hdr_tbl[HDR_TBLS_TOTAL];
	struct ipa3_hdr_proc_ctx_tbl hdr_proc_ctx_tbl;
	struct ipa3_rt_tbl_set rt_tbl_set[IPA_IP_MAX];
	struct ipa3_rt_tbl_set reap_rt_tbl_set[IPA_IP_MAX];
	struct kmem_cache *flt_rule_cache;
	struct kmem_cache *rt_rule_cache;
	struct kmem_cache *hdr_cache;
	struct kmem_cache *hdr_offset_cache;
	struct kmem_cache *fnr_stats_cache;
	struct kmem_cache *hdr_proc_ctx_cache;
	struct kmem_cache *hdr_proc_ctx_offset_cache;
	struct kmem_cache *rt_tbl_cache;
	struct kmem_cache *tx_pkt_wrapper_cache;
	struct kmem_cache *rx_pkt_wrapper_cache;
	unsigned long rt_idx_bitmap[IPA_IP_MAX];
	struct mutex lock;
	u16 smem_sz;
	u16 smem_restricted_bytes;
	u16 smem_reqd_sz;
	struct ipa3_nat_mem nat_mem;
	struct ipa3_ipv6ct_mem ipv6ct_mem;
	u32 excp_hdr_hdl;
	u32 dflt_v4_rt_rule_hdl;
	u32 dflt_v6_rt_rule_hdl;
	uint aggregation_type;
	uint aggregation_byte_limit;
	uint aggregation_time_limit;
	bool hdr_proc_ctx_tbl_lcl;
	struct ipa_mem_buffer hdr_sys_mem;
	struct ipa_mem_buffer hdr_proc_ctx_mem;
	bool rt_tbl_hash_lcl[IPA_IP_MAX];
	bool rt_tbl_nhash_lcl[IPA_IP_MAX];
	bool flt_tbl_hash_lcl[IPA_IP_MAX];
	bool flt_tbl_nhash_lcl[IPA_IP_MAX];
	struct list_head flt_tbl_nhash_lcl_list[IPA_IP_MAX];
	struct ipa3_active_clients ipa3_active_clients;
	struct ipa3_active_clients_log_ctx ipa3_active_clients_logging;
	struct workqueue_struct *power_mgmt_wq;
	struct workqueue_struct *transport_power_mgmt_wq;
	bool tag_process_before_gating;
	struct ipa3_transport_pm transport_pm;
	struct workqueue_struct *xr_uc_init_wq;
	unsigned long gsi_evt_comm_hdl;
	u32 gsi_evt_comm_ring_rem;
	u32 clnt_hdl_cmd;
	u32 clnt_hdl_data_in;
	u32 clnt_hdl_data_out;
	spinlock_t disconnect_lock;
	u8 a5_pipe_index;
	struct list_head intf_list;
	struct list_head msg_list;
	struct list_head pull_msg_list;
	struct mutex msg_lock;
	struct list_head msg_wlan_client_list;
	struct mutex msg_wlan_client_lock;
	wait_queue_head_t msg_waitq;
	enum ipa_hw_type ipa_hw_type;
	u8 hw_type_index;
	enum ipa3_hw_mode ipa3_hw_mode;
	enum gsi_ver gsi_ver;
	enum ipa3_platform_type platform_type;
	bool ipa_config_is_mhi;
	bool use_ipa_teth_bridge;
	bool modem_cfg_emb_pipe_flt;
	bool ipa_wdi2;
	bool ipa_config_is_auto;
	bool ipa_wdi2_over_gsi;
	bool ipa_wdi3_over_gsi;
	bool ipa_wdi_opt_dpath;
	atomic_t ipa_xr_wdi_flt_rsv_status;
	struct completion ipa_xr_wdi_flt_rsrv_success;
	u8 rtp_stream_id_cnt;
	u32 rtp_proc_hdls[MAX_STREAMS];
	u32 rtp_rt4_tbl_hdls[MAX_STREAMS];
	u32 rtp_rt4_tbl_idxs[MAX_STREAMS];
	u32 rtp_rt4_rule_hdls[MAX_STREAMS];
	u32 rtp_flt4_rule_hdls[MAX_STREAMS];
	bool ipa_endp_delay_wa;
	bool lan_coal_enable;
	bool ipa_fltrt_not_hashable;
	bool use_xbl_boot;
	bool use_64_bit_dma_mask;
	/* featurize if memory footprint becomes a concern */
	struct ipa3_stats stats;
	void *smem_pipe_mem;
	void *logbuf;
	void *logbuf_low;
	void *logbuf_clk;
	struct ipa3_controller *ctrl;
	struct idr ipa_idr;
	struct platform_device *master_pdev;
	struct device *pdev;
	struct device *uc_pdev;
	struct device *rtp_pdev;
	spinlock_t idr_lock;
	u32 enable_clock_scaling;
	u32 enable_napi_chain;
	u32 curr_ipa_clk_rate;
	bool q6_proxy_clk_vote_valid;
	struct mutex q6_proxy_clk_vote_mutex;
	u32 q6_proxy_clk_vote_cnt;
	u32 ipa_num_pipes;
	dma_addr_t pkt_init_imm[IPA5_MAX_NUM_PIPES];
	u32 pkt_init_imm_opcode;

	struct ipa3_wlan_comm_memb wc_memb;

	struct ipa3_uc_ctx uc_ctx;

	struct ipa3_uc_wdi_ctx uc_wdi_ctx;
	struct ipa3_uc_ntn_ctx uc_ntn_ctx;
	struct ipa3_uc_wigig_ctx uc_wigig_ctx;
	u32 wan_rx_ring_size;
	u32 lan_rx_ring_size;
	bool skip_uc_pipe_reset;
	int mpm_ring_size_dl;
	int mpm_ring_size_dl_cache;
	int mpm_ring_size_ul_cache;
	int mpm_ring_size_ul;
	int mpm_teth_aggr_size;
	int mpm_uc_thresh;
	unsigned long gsi_dev_hdl;
	u32 ee;
	bool apply_rg10_wa;
	bool gsi_ch20_wa;
	bool s1_bypass_arr[IPA_SMMU_CB_MAX];
	u32 wdi_map_cnt;
	struct wakeup_source *w_lock;
	struct ipa3_wakelock_ref_cnt wakelock_ref_cnt;
	/* RMNET_IOCTL_INGRESS_FORMAT_AGG_DATA */
	bool ipa_client_apps_wan_cons_agg_gro;
	/* M-release support to know client pipes */
	struct ipa3cm_client_info ipacm_client[IPA5_MAX_NUM_PIPES];
	bool tethered_flow_control;
	bool ipa_initialization_complete;
	struct list_head ipa_ready_cb_list;
	struct completion init_completion_obj;
	struct completion uc_loaded_completion_obj;
	struct ipa3_smp2p_info smp2p_info;
	u32 mhi_evid_limits[2]; /* start and end values */
	u32 ipa_tz_unlock_reg_num;
	struct ipa_tz_unlock_reg_info *ipa_tz_unlock_reg;
	struct ipa_dma_task_info dma_task_info;
	struct ipa_hw_stats *hw_stats;
	struct ipa_flt_rt_counter flt_rt_counters;
	struct ipa_cne_evt ipa_cne_evt_req_cache[IPA_MAX_NUM_REQ_CACHE];
	int num_ipa_cne_evt_req;
	struct mutex ipa_cne_evt_lock;
	bool vlan_mode_iface[IPA_VLAN_IF_MAX];
	bool wdi_over_pcie;
	u32 entire_ipa_block_size;
	bool do_register_collection_on_crash;
	bool do_testbus_collection_on_crash;
	bool do_non_tn_collection_on_crash;
	bool do_ram_collection_on_crash;
	u32 secure_debug_check_action;
	u32 sd_state;
	void __iomem *reg_collection_base;
	struct ipa3_wdi2_ctx wdi2_ctx;
	struct ipa3_pc_mbox_data pc_mbox;
	struct ipa3_wdi3_ctx wdi3_ctx;
	struct ipa3_usb_ctx usb_ctx;
	struct ipa3_mhip_ctx mhip_ctx;
	struct ipa3_aqc_ctx aqc_ctx;
	struct ipa3_rtk_ctx rtk_ctx;
	struct ipa3_ntn_ctx ntn_ctx;
#if defined(CONFIG_IPA_TSP)
	struct ipa3_tsp_ctx tsp;
#endif
	atomic_t ipa_clk_vote;
	bool gsi_status;

	int (*client_lock_unlock[IPA_MAX_CLNT])(bool is_lock);

	struct ipa_fw_load_data fw_load_data;

	bool (*get_teth_port_state[IPA_MAX_CLNT])(void);

	atomic_t is_ssr;
	bool deepsleep;
	void *subsystem_get_retval;
	struct IpaHwOffloadStatsAllocCmdData_t
		gsi_info[IPA_HW_PROTOCOL_MAX];
	bool ipa_wan_skb_page;
	struct ipacm_fnr_info fnr_info;
	/* dummy netdev for lan RX NAPI */
	bool lan_rx_napi_enable;
	bool tx_napi_enable;
	bool tx_poll;
	struct net_device generic_ndev;
	struct napi_struct napi_lan_rx;
	u32 icc_num_cases;
	u32 icc_num_paths;
	u32 icc_clk[IPA_ICC_LVL_MAX][IPA_ICC_PATH_MAX][IPA_ICC_TYPE_MAX];
#define WAN_COAL_SUB  0
#define LAN_COAL_SUB  1
#define ULSO_COAL_SUB 2
#define MAX_CCP_SUB (ULSO_COAL_SUB + 1)
	struct ipahal_imm_cmd_pyld *coal_cmd_pyld[MAX_CCP_SUB];
	struct ipa_mem_buffer ulso_wa_cmd;
	u32 tx_wrapper_cache_max_size;
	u32 ipa_gen_rx_cmn_page_pool_sz_factor;
	u32 ipa_gen_rx_cmn_temp_pool_sz_factor;
	u32 ipa_gen_rx_ll_pool_sz_factor;
	struct ipa3_app_clock_vote app_clock_vote;
	bool clients_registered;
	bool ipa_gpi_event_rp_ddr;
	bool rmnet_ctl_enable;
	bool rmnet_ll_enable;
	char *gsi_fw_file_name;
	char *uc_fw_file_name;
	struct ipa3_eth_info
		eth_info[IPA_ETH_CLIENT_MAX][IPA_ETH_INST_ID_MAX];
	u32 ipa_wan_aggr_pkt_cnt;
	bool ipa_mhi_proxy;
	u32 num_smmu_cb_probed;
	u32 max_num_smmu_cb;
	u32 ipa_wdi3_2g_holb_timeout;
	u32 ipa_wdi3_5g_holb_timeout;
	bool is_wdi3_tx1_needed;
	bool ipa_endp_delay_wa_v2;
	u32 pkt_init_ex_imm_opcode;
	struct ipa_mem_buffer pkt_init_mem;
	struct ipa_mem_buffer pkt_init_ex_mem;
	struct ipa_mem_buffer pkt_init_ex_imm[IPA_IMM_IP_PACKET_INIT_EX_CMD_NUM];
	bool is_modem_up;
	bool ulso_supported;
	u16 ulso_ip_id_min;
	u16 ulso_ip_id_max;
	bool use_pm_wrapper;
	u8 page_poll_threshold;
	bool wan_common_page_pool;
	bool use_tput_est_ep;
	struct ipa_ioc_eogre_info eogre_cache;
	bool eogre_enabled;
	bool is_device_crashed;
	bool ulso_wa;
	u64 gsi_msi_addr;
	spinlock_t notifier_lock;
	struct raw_notifier_head *ipa_rmnet_notifier_list_internal;
	struct list_head notifier_block_list_head;
	bool ipa_rmnet_notifier_enabled;
	bool buff_above_thresh_for_def_pipe_notified;
	bool buff_above_thresh_for_coal_pipe_notified;
	bool buff_above_thresh_for_ll_pipe_notified;
	bool buff_below_thresh_for_def_pipe_notified;
	bool buff_below_thresh_for_coal_pipe_notified;
	bool buff_below_thresh_for_ll_pipe_notified;
	bool free_page_task_scheduled;
	u8 mhi_ctrl_state;
	struct ipa_mem_buffer uc_act_tbl;
	bool uc_act_tbl_valid;
	struct mutex act_tbl_lock;
	int uc_act_tbl_total;
	int uc_act_tbl_next_index;
	int ipa_pil_load;
	u32 ipa_max_napi_sort_page_thrshld;
	u32 page_wq_reschd_time;
	bool coal_ipv4_id_ignore;
	struct list_head minidump_list_head;
	phys_addr_t per_stats_smem_pa;
	void *per_stats_smem_va;
	u32 ipa_smem_size;
	bool is_dual_pine_config;
	struct workqueue_struct *collect_recycle_stats_wq;
	struct ipa_lnx_pipe_page_recycling_stats recycle_stats;
	struct ipa3_page_recycle_stats prev_coal_recycle_stats;
	struct ipa3_page_recycle_stats prev_default_recycle_stats;
	struct ipa3_page_recycle_stats prev_low_lat_data_recycle_stats;
	struct mutex recycle_stats_collection_lock;
	struct mutex ssr_lock;
	atomic_t is_suspend_mode_enabled;
};

struct ipa3_plat_drv_res {
	bool use_ipa_teth_bridge;
	u32 ipa_mem_base;
	u32 ipa_mem_size;
	u32 transport_mem_base;
	u32 transport_mem_size;
	u32 ipa_cfg_offset;
	u32 emulator_intcntrlr_mem_base;
	u32 emulator_intcntrlr_mem_size;
	u32 emulator_irq;
	u32 ipa_irq;
	u32 transport_irq;
	u32 ipa_pipe_mem_start_ofst;
	u32 ipa_pipe_mem_size;
	enum ipa_hw_type ipa_hw_type;
	enum ipa3_hw_mode ipa3_hw_mode;
	enum ipa3_platform_type platform_type;
	u32 ee;
	bool modem_cfg_emb_pipe_flt;
	bool ipa_wdi2;
	bool ipa_config_is_auto;
	bool ipa_wdi2_over_gsi;
	bool ipa_wdi3_over_gsi;
	bool ipa_fltrt_not_hashable;
	bool use_xbl_boot;
	bool use_64_bit_dma_mask;
	bool use_bw_vote;
	u32 wan_rx_ring_size;
	u32 lan_rx_ring_size;
	bool skip_uc_pipe_reset;
	bool apply_rg10_wa;
	bool gsi_ch20_wa;
	bool tethered_flow_control;
	bool lan_rx_napi_enable;
	bool tx_napi_enable;
	bool tx_poll;
	u32 mhi_evid_limits[2]; /* start and end values */
	bool ipa_mhi_dynamic_config;
	u32 ipa_tz_unlock_reg_num;
	struct ipa_tz_unlock_reg_info *ipa_tz_unlock_reg;
	struct ipa_pm_init_params pm_init;
	bool wdi_over_pcie;
	u32 entire_ipa_block_size;
	bool do_register_collection_on_crash;
	bool do_testbus_collection_on_crash;
	bool do_non_tn_collection_on_crash;
	bool do_ram_collection_on_crash;
	u32 secure_debug_check_action;
	bool ipa_endp_delay_wa;
	bool skip_ieob_mask_wa;
	bool ipa_wan_skb_page;
	u32 icc_num_cases;
	u32 icc_num_paths;
	const char *icc_path_name[IPA_ICC_PATH_MAX];
	u32 icc_clk_val[IPA_ICC_LVL_MAX][IPA_ICC_MAX];
	bool ipa_gpi_event_rp_ddr;
	bool rmnet_ctl_enable;
	bool rmnet_ll_enable;
	bool lan_coal_enable;
	bool ipa_use_uc_holb_monitor;
	u32 ipa_holb_monitor_poll_period;
	u32 ipa_holb_monitor_max_cnt_wlan;
	u32 ipa_holb_monitor_max_cnt_usb;
	u32 ipa_holb_monitor_max_cnt_11ad;
	const char *gsi_fw_file_name;
	const char *uc_fw_file_name;
	u32 tx_wrapper_cache_max_size;
	u32 ipa_gen_rx_cmn_page_pool_sz_factor;
	u32 ipa_gen_rx_cmn_temp_pool_sz_factor;
	u32 ipa_gen_rx_ll_pool_sz_factor;
	u32 ipa_wan_aggr_pkt_cnt;
	bool ipa_mhi_proxy;
	u32 max_num_smmu_cb;
	u32 ipa_wdi3_2g_holb_timeout;
	u32 ipa_wdi3_5g_holb_timeout;
	bool ipa_endp_delay_wa_v2;
	bool ulso_supported;
	u16 ulso_ip_id_min;
	u16 ulso_ip_id_max;
	bool use_pm_wrapper;
	bool use_tput_est_ep;
	bool ulso_wa;
	bool ipa_wdi_opt_dpath;
	u8 coal_ipv4_id_ignore;
};

/**
 * struct ipa3_mem_partition - represents IPA RAM Map as read from DTS
 * Order and type of members should not be changed without a suitable change
 * to DTS file or the code that reads it.
 *
 * IPA SRAM memory layout:
 * +-------------------------+
 * |    UC MEM               |
 * +-------------------------+
 * |    UC INFO              |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * | V4 FLT HDR HASHABLE     |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * | V4 FLT HDR NON-HASHABLE |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * | V6 FLT HDR HASHABLE     |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * | V6 FLT HDR NON-HASHABLE |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * | V4 RT HDR HASHABLE      |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * | V4 RT HDR NON-HASHABLE  |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * | V6 RT HDR HASHABLE      |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * | V6 RT HDR NON-HASHABLE  |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * |  MODEM HDR              |
 * +-------------------------+
 * |  APPS HDR (IPA4.5)      |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * | MODEM PROC CTX          |
 * +-------------------------+
 * | APPS PROC CTX           |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * |    CANARY (IPA4.5)      |
 * +-------------------------+
 * |    CANARY (IPA4.5)      |
 * +-------------------------+
 * | NAT TABLE (IPA4.5)      |
 * +-------------------------+
 * |    CANARY (IPA4.5)      |
 * +-------------------------+
 * |    CANARY (IPA4.5)      |
 * +-------------------------+
 * | PDN CONFIG              |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * |    CANARY               |
 * +-------------------------+
 * | QUOTA STATS             |
 * +-------------------------+
 * | TETH STATS              |
 * +-------------------------+
 * | FnR STATS               |
 * +-------------------------+
 * | DROP STATS              |
 * +-------------------------+
 * |    CANARY (IPA4.5)      |
 * +-------------------------+
 * |    CANARY (IPA4.5)      |
 * +-------------------------+
 * | MODEM MEM               |
 * +-------------------------+
 * |    Dummy (IPA4.5)       |
 * +-------------------------+
 * |    CANARY (IPA4.5)      |
 * +-------------------------+
 * | UC DESC RAM (IPA3.5)    |
 * +-------------------------+
 */
struct ipa3_mem_partition {
	u32 ofst_start;
	u32 v4_flt_hash_ofst;
	u32 v4_flt_hash_size;
	u32 v4_flt_hash_size_ddr;
	u32 v4_flt_nhash_ofst;
	u32 v4_flt_nhash_size;
	u32 v4_flt_nhash_size_ddr;
	u32 v6_flt_hash_ofst;
	u32 v6_flt_hash_size;
	u32 v6_flt_hash_size_ddr;
	u32 v6_flt_nhash_ofst;
	u32 v6_flt_nhash_size;
	u32 v6_flt_nhash_size_ddr;
	u32 v4_rt_num_index;
	u32 v4_modem_rt_index_lo;
	u32 v4_modem_rt_index_hi;
	u32 v4_apps_rt_index_lo;
	u32 v4_apps_rt_index_hi;
	u32 v4_rt_hash_ofst;
	u32 v4_rt_hash_size;
	u32 v4_rt_hash_size_ddr;
	u32 v4_rt_nhash_ofst;
	u32 v4_rt_nhash_size;
	u32 v4_rt_nhash_size_ddr;
	u32 v6_rt_num_index;
	u32 v6_modem_rt_index_lo;
	u32 v6_modem_rt_index_hi;
	u32 v6_apps_rt_index_lo;
	u32 v6_apps_rt_index_hi;
	u32 v6_rt_hash_ofst;
	u32 v6_rt_hash_size;
	u32 v6_rt_hash_size_ddr;
	u32 v6_rt_nhash_ofst;
	u32 v6_rt_nhash_size;
	u32 v6_rt_nhash_size_ddr;
	u32 modem_hdr_ofst;
	u32 modem_hdr_size;
	u32 apps_hdr_ofst;
	u32 apps_hdr_size;
	u32 apps_hdr_size_ddr;
	u32 modem_hdr_proc_ctx_ofst;
	u32 modem_hdr_proc_ctx_size;
	u32 apps_hdr_proc_ctx_ofst;
	u32 apps_hdr_proc_ctx_size;
	u32 apps_hdr_proc_ctx_size_ddr;
	u32 nat_tbl_ofst;
	u32 nat_tbl_size;
	u32 modem_comp_decomp_ofst;
	u32 modem_comp_decomp_size;
	u32 modem_ofst;
	u32 modem_size;
	u32 apps_v4_flt_hash_ofst;
	u32 apps_v4_flt_hash_size;
	u32 apps_v4_flt_nhash_ofst;
	u32 apps_v4_flt_nhash_size;
	u32 apps_v6_flt_hash_ofst;
	u32 apps_v6_flt_hash_size;
	u32 apps_v6_flt_nhash_ofst;
	u32 apps_v6_flt_nhash_size;
	u32 uc_info_ofst;
	u32 uc_info_size;
	u32 end_ofst;
	u32 apps_v4_rt_hash_ofst;
	u32 apps_v4_rt_hash_size;
	u32 apps_v4_rt_nhash_ofst;
	u32 apps_v4_rt_nhash_size;
	u32 apps_v6_rt_hash_ofst;
	u32 apps_v6_rt_hash_size;
	u32 apps_v6_rt_nhash_ofst;
	u32 apps_v6_rt_nhash_size;
	u32 uc_descriptor_ram_ofst;
	u32 uc_descriptor_ram_size;
	u32 pdn_config_ofst;
	u32 pdn_config_size;
	u32 stats_quota_q6_ofst;
	u32 stats_quota_q6_size;
	u32 stats_quota_ap_ofst;
	u32 stats_quota_ap_size;
	u32 stats_tethering_ofst;
	u32 stats_tethering_size;
	u32 stats_fnr_ofst;
	u32 stats_fnr_size;
	u32 uc_ofst;
	u32 uc_size;

	/* Irrelevant starting IPA4.5 */
	u32 stats_flt_v4_ofst;
	u32 stats_flt_v4_size;
	u32 stats_flt_v6_ofst;
	u32 stats_flt_v6_size;
	u32 stats_rt_v4_ofst;
	u32 stats_rt_v4_size;
	u32 stats_rt_v6_ofst;
	u32 stats_rt_v6_size;

	u32 stats_drop_ofst;
	u32 stats_drop_size;
};

struct ipa3_controller {
	struct ipa3_mem_partition *mem_partition;
	u32 ipa_clk_rate_turbo;
	u32 ipa_clk_rate_nominal;
	u32 ipa_clk_rate_svs;
	u32 ipa_clk_rate_svs2;
	u32 clock_scaling_bw_threshold_turbo;
	u32 clock_scaling_bw_threshold_nominal;
	u32 clock_scaling_bw_threshold_svs;
	u32 ipa_reg_base_ofst;
	u32 max_holb_tmr_val;
	void (*ipa_sram_read_settings)(void);
	int (*ipa_init_sram)(void);
	int (*ipa_init_hdr)(void);
	int (*ipa_init_rt4)(void);
	int (*ipa_init_rt6)(void);
	int (*ipa_init_flt4)(void);
	int (*ipa_init_flt6)(void);
	int (*ipa3_read_ep_reg)(char *buff, int max_len, int pipe);
	int (*ipa3_commit_flt)(enum ipa_ip_type ip);
	int (*ipa3_commit_rt)(enum ipa_ip_type ip);
	int (*ipa3_commit_hdr)(void);
	void (*ipa3_enable_clks)(void);
	void (*ipa3_disable_clks)(void);
	struct icc_path *icc_path[IPA_ICC_PATH_MAX];
};

/*
 * When data arrives on IPA_CLIENT_APPS_LAN_COAL_CONS, said data will
 * contain a qmap header followed by an array of the following.  The
 * number of them in the array is always MAX_COAL_PACKET_STATUS_INFO
 * (see below); however, only "num_nlos" (a field in the cmap heeader)
 * will be valid.  The rest are to be ignored.
 */
struct coal_packet_status_info {
	u16 pkt_len;
	u8  pkt_cksum_errs;
	u8  num_pkts;
} __aligned(1);
/*
 * This is the number of the struct coal_packet_status_info that
 * follow the qmap header.  As above, only "num_nlos" are valid.  The
 * rest are to be ignored.
 */
#define MAX_COAL_PACKET_STATUS_INFO (6)
#define VALID_NLS(nls) \
	((nls) > 0 && (nls) <= MAX_COAL_PACKET_STATUS_INFO)
/*
 * The following is the total number of bits in all the pkt_cksum_errs
 * in each of the struct coal_packet_status_info(s) that follow the
 * qmap header.  Each bit is meant to tell us if a packet is good or
 * bad, relative to a checksum. Given this, the max number of bits
 * dictates the max number of packets that can be in a buffer from the
 * IPA.
 */
#define MAX_COAL_PACKETS            (48)

extern struct ipa3_context *ipa3_ctx;
extern bool ipa_net_initialized;

/* public APIs */
/* Generic GSI channels functions */
int ipa3_request_gsi_channel(struct ipa_request_gsi_channel_params *params,
			     struct ipa_req_chan_out_params *out_params);

int ipa3_release_gsi_channel(u32 clnt_hdl);

int ipa3_reset_gsi_channel(u32 clnt_hdl);

int ipa3_reset_gsi_event_ring(u32 clnt_hdl);

/* Specific xDCI channels functions */
int ipa3_set_usb_max_packet_size(
	enum ipa_usb_max_usb_packet_size usb_max_packet_size);

int ipa3_xdci_start(u32 clnt_hdl, u8 xferrscidx, bool xferrscidx_valid);

int ipa3_xdci_connect(u32 clnt_hdl);

int ipa3_xdci_disconnect(u32 clnt_hdl, bool should_force_clear, u32 qmi_req_id);

void ipa3_xdci_ep_delay_rm(u32 clnt_hdl);
int ipa3_set_reset_client_prod_pipe_delay(bool set_reset,
		enum ipa_client_type client);
int ipa3_start_stop_client_prod_gsi_chnl(enum ipa_client_type client,
		bool start_chnl);
void ipa3_client_prod_post_shutdown_cleanup(void);

int ipa3_set_reset_client_cons_pipe_sus_holb(bool set_reset,
		enum ipa_client_type client);

int ipa3_xdci_suspend(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	bool should_force_clear, u32 qmi_req_id, bool is_dpl);

int ipa3_xdci_resume(u32 ul_clnt_hdl, u32 dl_clnt_hdl, bool is_dpl);

/*
 * Remove ep delay
 */
int ipa3_clear_endpoint_delay(u32 clnt_hdl);

/*
 * Configuration
 */
int ipa3_cfg_ep_seq(u32 clnt_hdl, const struct ipa_ep_cfg_seq *seq_cfg);

int ipa3_cfg_ep_hdr(u32 clnt_hdl, const struct ipa_ep_cfg_hdr *ipa_ep_cfg);

int ipa3_cfg_ep_hdr_ext(u32 clnt_hdl,
			const struct ipa_ep_cfg_hdr_ext *ipa_ep_cfg);

int ipa3_cfg_ep_mode(u32 clnt_hdl, const struct ipa_ep_cfg_mode *ipa_ep_cfg);

int ipa3_cfg_ep_aggr(u32 clnt_hdl, const struct ipa_ep_cfg_aggr *ipa_ep_cfg);

int ipa3_cfg_ep_deaggr(u32 clnt_hdl,
		      const struct ipa_ep_cfg_deaggr *ipa_ep_cfg);

int ipa3_cfg_ep_route(u32 clnt_hdl, const struct ipa_ep_cfg_route *ipa_ep_cfg);

int ipa3_cfg_ep_holb(u32 clnt_hdl, const struct ipa_ep_cfg_holb *ipa_ep_cfg);

void ipa3_cal_ep_holb_scale_base_val(u32 tmr_val,
				struct ipa_ep_cfg_holb *ep_holb);

int ipa3_cfg_ep_cfg(u32 clnt_hdl, const struct ipa_ep_cfg_cfg *ipa_ep_cfg);

int ipa3_cfg_ep_prod_cfg(u32 clnt_hdl, const struct ipa_ep_cfg_prod_cfg *prod_cfg);

int ipa3_force_cfg_ep_holb(u32 clnt_hdl, struct ipa_ep_cfg_holb *ipa_ep_cfg);

int ipa3_cfg_ep_metadata_mask(u32 clnt_hdl,
		const struct ipa_ep_cfg_metadata_mask *ipa_ep_cfg);

int ipa3_cfg_ep_holb_by_client(enum ipa_client_type client,
				const struct ipa_ep_cfg_holb *ipa_ep_cfg);

int ipa3_cfg_ep_ulso(u32 clnt_hdl, const struct ipa_ep_cfg_ulso *ep_ulso);

int ipa3_setup_uc_act_tbl(void);

/*
 * Header removal / addition
 */

int ipa3_del_hdr_by_user(struct ipa_ioc_del_hdr *hdls, bool by_user);

int ipa3_commit_hdr(void);

int ipa3_get_hdr_offset(char* name, u32* offset);

int ipa3_get_hdr_proc_ctx_hdl(struct ipa_ioc_get_hdr *lookup);

int ipa3_get_hdr_proc_ctx_offset(char* name, u32* offset);

int ipa3_put_hdr(u32 hdr_hdl);

int ipa3_copy_hdr(struct ipa_ioc_copy_hdr *copy);

u32 ipa3_get_hdr_bin_size(int index);

/*
 * Header Processing Context
 */

int ipa3_del_hdr_proc_ctx_by_user(struct ipa_ioc_del_hdr_proc_ctx *hdls,
	bool by_user);

/*
 * Routing
 */
int ipa3_add_rt_rule_ext(struct ipa_ioc_add_rt_rule_ext *rules);

int ipa3_add_rt_rule_ext_v2(struct ipa_ioc_add_rt_rule_ext_v2 *rules,
	bool user);

int ipa3_add_rt_rule_after(struct ipa_ioc_add_rt_rule_after *rules);

int ipa3_add_rt_rule_after_v2(struct ipa_ioc_add_rt_rule_after_v2
	*rules);

int ipa3_get_rt_tbl(struct ipa_ioc_get_rt_tbl *lookup);

int ipa3_query_rt_index(struct ipa_ioc_get_rt_tbl_indx *in);

int ipa3_mdfy_rt_rule(struct ipa_ioc_mdfy_rt_rule *rules);

int ipa3_mdfy_rt_rule_v2(struct ipa_ioc_mdfy_rt_rule_v2 *rules);

int ipa3_set_nat_conn_track_exc_rt_tbl(u32 rt_tbl_hdl, enum ipa_ip_type ip);

/*
 * Filtering
 */
int ipa3_add_flt_rule(struct ipa_ioc_add_flt_rule *rules);

int ipa3_add_flt_rule_v2(struct ipa_ioc_add_flt_rule_v2 *rules);

int ipa3_add_flt_rule_usr(struct ipa_ioc_add_flt_rule *rules,
	bool user_only);

int ipa3_add_flt_rule_usr_v2(struct ipa_ioc_add_flt_rule_v2 *rules,
	bool user_only);

int ipa3_add_flt_rule_after(struct ipa_ioc_add_flt_rule_after *rules);

int ipa3_add_flt_rule_after_v2(struct ipa_ioc_add_flt_rule_after_v2
	*rules);

int ipa3_mdfy_flt_rule(struct ipa_ioc_mdfy_flt_rule *rules);

int ipa3_mdfy_flt_rule_v2(struct ipa_ioc_mdfy_flt_rule_v2 *rules);

int ipa3_commit_flt(enum ipa_ip_type ip);

int ipa3_reset_flt(enum ipa_ip_type ip, bool user_only);

int ipa_flt_sram_set_client_prio_high(enum ipa_client_type client);

/*
 * NAT
 */
int ipa3_nat_ipv6ct_init_devices(void);
void ipa3_nat_ipv6ct_destroy_devices(void);

int ipa3_allocate_nat_device(struct ipa_ioc_nat_alloc_mem *mem);
int ipa3_allocate_nat_table(
	struct ipa_ioc_nat_ipv6ct_table_alloc *table_alloc);
int ipa3_allocate_ipv6ct_table(
	struct ipa_ioc_nat_ipv6ct_table_alloc *table_alloc);
int ipa3_nat_get_sram_info(struct ipa_nat_in_sram_info *info_ptr);
int ipa3_app_clk_vote(enum ipa_app_clock_vote_type vote_type);
void ipa3_get_default_evict_values(
	struct ipahal_reg_coal_evict_lru *evict_lru);
void ipa3_default_evict_register( void );
int ipa3_set_evict_policy(
	struct ipa_ioc_coal_evict_policy *evict_pol);
void start_coalescing( void );
void stop_coalescing( void );
bool lan_coal_enabled( void );

/*
 * Messaging
 */
int ipa3_resend_wlan_msg(void);
int ipa3_register_pull_msg(struct ipa_msg_meta *meta, ipa_msg_pull_fn callback);
int ipa3_deregister_pull_msg(struct ipa_msg_meta *meta);

/*
 * Interface
 */
int ipa3_register_intf_ext(const char *name, const struct ipa_tx_intf *tx,
		       const struct ipa_rx_intf *rx,
		       const struct ipa_ext_intf *ext);

/*
 * To transfer multiple data packets
 * While passing the data descriptor list, the anchor node
 * should be of type struct ipa_tx_data_desc not list_head
 */
int ipa3_tx_dp_mul(enum ipa_client_type dst,
			struct ipa_tx_data_desc *data_desc);

/*
 * System pipes
 */
int ipa3_setup_tput_pipe(void);
int ipa_pm_wrapper_wdi_set_perf_profile_internal(struct ipa_wdi_perf_profile *profile);
int ipa_pm_wrapper_connect_wdi_pipe(struct ipa_wdi_in_params *in,
			struct ipa_wdi_out_params *out);
int ipa_pm_wrapper_disconnect_wdi_pipe(u32 clnt_hdl);
int ipa_pm_wrapper_enable_wdi_pipe(u32 clnt_hdl);
int ipa_pm_wrapper_disable_pipe(u32 clnt_hdl);
int ipa3_enable_gsi_wdi_pipe(u32 clnt_hdl);
int ipa3_disable_gsi_wdi_pipe(u32 clnt_hdl);
int ipa3_disconnect_gsi_wdi_pipe(u32 clnt_hdl);
int ipa3_resume_gsi_wdi_pipe(u32 clnt_hdl);
int ipa3_get_wdi_gsi_stats(struct ipa_uc_dbg_ring_stats *stats);
int ipa3_get_wdi3_gsi_stats(struct ipa_uc_dbg_ring_stats *stats);
int ipa3_get_usb_gsi_stats(struct ipa_uc_dbg_ring_stats *stats);
bool ipa_usb_is_teth_prot_connected(enum ipa_usb_teth_prot usb_teth_prot);
int ipa3_get_aqc_gsi_stats(struct ipa_uc_dbg_ring_stats *stats);
int ipa3_get_rtk_gsi_stats(struct ipa_uc_dbg_ring_stats *stats);
int ipa3_get_ntn_gsi_stats(struct ipa_uc_dbg_ring_stats *stats);
u16 ipa3_get_smem_restr_bytes(void);

int ipa3_wigig_init_debugfs_i(struct dentry *dent);

/*
 * To register uC ready callback if uC not ready
 * and also check uC readiness
 * if uC not ready only, register callback
 */
int ipa3_uc_reg_rdyCB(struct ipa_wdi_uc_ready_params *param);
/*
 * To de-register uC ready callback
 */
int ipa3_uc_dereg_rdyCB(void);

int ipa_create_uc_smmu_mapping(int res_idx, bool wlan_smmu_en,
		phys_addr_t pa, struct sg_table *sgt, size_t len, bool device,
		unsigned long *iova);

int ipa_create_gsi_smmu_mapping(int res_idx, bool wlan_smmu_en,
		phys_addr_t pa, struct sg_table *sgt, size_t len, bool device,
		unsigned long *iova);

void ipa3_release_wdi3_gsi_smmu_mappings(u8 dir);

/*
 * Tethering bridge (Rmnet / MBIM)
 */

int ipa3_teth_bridge_get_pm_hdl(enum ipa_client_type client);

/*
 * Tethering client info
 */

int ipa3_get_wlan_stats(struct ipa_get_wdi_sap_stats *wdi_sap_stats);

int ipa3_set_wlan_quota(struct ipa_set_wifi_quota *wdi_quota);

int ipa3_inform_wlan_bw(struct ipa_inform_wlan_bw *wdi_bw);

/*
 * IPADMA
 */
int ipa3_dma_uc_memcpy(phys_addr_t dest, phys_addr_t src, int len);

/*
 * Miscellaneous
 */
int ipa_get_ep_mapping_from_gsi(int ch_id);

int ipa3_ctx_get_type(enum ipa_type_mode type);
bool ipa3_ctx_get_flag(enum ipa_flag flag);
u32 ipa3_ctx_get_num_pipes(void);

void ipa3_proxy_clk_vote(bool is_ssr);
void ipa3_proxy_clk_unvote(void);

bool ipa3_is_client_handle_valid(u32 clnt_hdl);

enum ipa_client_type ipa3_get_client_mapping(int pipe_idx);
enum ipa_client_type ipa3_get_client_by_pipe(int pipe_idx);

void ipa_init_ep_flt_bitmap(void);

bool ipa_is_ep_support_flt(int pipe_idx);

bool ipa3_get_modem_cfg_emb_pipe_flt(void);

u8 ipa3_get_qmb_master_sel(enum ipa_client_type client);

u8 ipa3_get_tx_instance(enum ipa_client_type client);

bool ipa3_get_qmap_pipe_enable(void);

struct device *ipa3_get_pdev(void);
int ipa3_sys_update_gsi_hdls(u32 clnt_hdl, unsigned long gsi_ch_hdl,
	unsigned long gsi_ev_hdl);
int ipa3_sys_setup(struct ipa_sys_connect_params *sys_in,
			unsigned long *ipa_transport_hdl,
			u32 *ipa_pipe_num, u32 *clnt_hdl, bool en_status);
int ipa3_sys_teardown(u32 clnt_hdl);

/* internal functions */

u8 ipa3_get_hw_type_index(void);

bool ipa_is_modem_pipe(int pipe_idx);

int ipa3_send_one(struct ipa3_sys_context *sys, struct ipa3_desc *desc,
		bool in_atomic);
int ipa3_send(struct ipa3_sys_context *sys,
		u32 num_desc,
		struct ipa3_desc *desc,
		bool in_atomic);
int ipa_get_ep_mapping(enum ipa_client_type client);
int ipa_get_ep_group(enum ipa_client_type client);

int ipa3_generate_hw_rule(enum ipa_ip_type ip,
			 const struct ipa_rule_attrib *attrib,
			 u8 **buf,
			 u16 *en_rule);
int ipa3_init_hw(void);
struct ipa3_rt_tbl *__ipa3_find_rt_tbl(enum ipa_ip_type ip, const char *name);
int ipa_set_single_ndp_per_mbim(bool enable);
void ipa3_debugfs_init(void);
void ipa3_debugfs_remove(void);
void ipa3_eth_debugfs_init(void);
void ipa3_eth_debugfs_add(struct ipa_eth_client *client);

void ipa3_dump_buff_internal(void *base, dma_addr_t phy_base, u32 size);

void ipa3_qdss_register(void);
int ipa3_conn_qdss_pipes(struct ipa_qdss_conn_in_params *in,
	struct ipa_qdss_conn_out_params *out);
int ipa3_disconn_qdss_pipes(void);

#ifdef IPA_DEBUG
#define IPA_DUMP_BUFF(base, phy_base, size) \
	ipa3_dump_buff_internal(base, phy_base, size)
#else
#define IPA_DUMP_BUFF(base, phy_base, size)
#endif
int ipa3_init_mem_partition(enum ipa_hw_type ipa_hw_type);
int ipa3_controller_static_bind(struct ipa3_controller *controller,
		enum ipa_hw_type ipa_hw_type, u32 ipa_cfg_offset);
int ipa3_cfg_route(struct ipahal_reg_route *route);
int ipa3_send_cmd_timeout(u16 num_desc, struct ipa3_desc *descr, u32 timeout);
int ipa3_send_cmd(u16 num_desc, struct ipa3_desc *descr);
int ipa3_cfg_filter(u32 disable);
int ipa3_straddle_boundary(u32 start, u32 end, u32 boundary);
struct ipa3_context *ipa3_get_ctx(void);
void ipa3_enable_clks(void);
void ipa3_disable_clks(void);
int ipa3_inc_client_enable_clks_no_block(struct ipa_active_client_logging_info
		*id);
void ipa3_dec_client_disable_clks_no_block(
	struct ipa_active_client_logging_info *id);
void ipa3_dec_client_disable_clks_delay_wq(
		struct ipa_active_client_logging_info *id, unsigned long delay);
void ipa3_active_clients_log_dec(struct ipa_active_client_logging_info *id,
		bool int_ctx);
void ipa3_active_clients_log_inc(struct ipa_active_client_logging_info *id,
		bool int_ctx);
int ipa3_active_clients_log_print_buffer(char *buf, int size);
int ipa3_active_clients_log_print_table(char *buf, int size);
void ipa3_active_clients_log_clear(void);
int ipa3_interrupts_init(u32 ipa_irq, u32 ee, struct device *ipa_dev);
void ipa3_interrupts_destroy(u32 ipa_irq, struct device *ipa_dev);
int __ipa3_del_rt_rule(u32 rule_hdl);
int __ipa3_del_hdr(u32 hdr_hdl, bool by_user);
int __ipa3_release_hdr(u32 hdr_hdl);
int __ipa3_release_hdr_proc_ctx(u32 proc_ctx_hdl);
int _ipa_read_ep_reg_v3_0(char *buf, int max_len, int pipe);
int _ipa_read_ep_reg_v4_0(char *buf, int max_len, int pipe);
int _ipa_read_ipahal_regs(void);
void _ipa_enable_clks_v3_0(void);
void _ipa_disable_clks_v3_0(void);
struct device *ipa3_get_dma_dev(void);
void ipa3_suspend_active_aggr_wa(u32 clnt_hdl);
void ipa3_suspend_handler(enum ipa_irq_type interrupt,
				void *private_data,
				void *interrupt_data);

ssize_t ipa3_read(struct file *filp, char __user *buf, size_t count,
		 loff_t *f_pos);
int ipa3_pull_msg(struct ipa_msg_meta *meta, char *buff, size_t count);
int ipa3_query_intf(struct ipa_ioc_query_intf *lookup);
int ipa3_query_intf_tx_props(struct ipa_ioc_query_intf_tx_props *tx);
int ipa3_query_intf_rx_props(struct ipa_ioc_query_intf_rx_props *rx);
int ipa3_query_intf_ext_props(struct ipa_ioc_query_intf_ext_props *ext);

int ipa3_get_max_pdn(void);

void wwan_cleanup(void);

int ipa3_teth_bridge_driver_init(void);
void ipa3_lan_rx_cb(void *priv, enum ipa_dp_evt_type evt, unsigned long data);
void ipa3_lan_coal_rx_cb(
	void                *priv,
	enum ipa_dp_evt_type evt,
	unsigned long        data);

int _ipa_init_sram_v3(void);
int _ipa_init_hdr_v3_0(void);
int _ipa_init_rt4_v3(void);
int _ipa_init_rt6_v3(void);
int _ipa_init_flt4_v3(void);
int _ipa_init_flt6_v3(void);

int __ipa_commit_flt_v3(enum ipa_ip_type ip);
int __ipa_commit_rt_v3(enum ipa_ip_type ip);

int __ipa_commit_hdr_v3_0(void);
void ipa3_skb_recycle(struct sk_buff *skb);
void ipa3_install_dflt_flt_rules(u32 ipa_ep_idx);
void ipa3_delete_dflt_flt_rules(u32 ipa_ep_idx);
void ipa3_install_dl_opt_wdi_dpath_flt_rules(u32 ipa_ep_idx, u32 rt_tbl_idx);
void ipa3_delete_dl_opt_wdi_dpath_flt_rules(u32 ipa_ep_idx);

int ipa3_remove_secondary_flow_ctrl(int gsi_chan_hdl);
int ipa3_enable_data_path(u32 clnt_hdl);
int ipa3_disable_data_path(u32 clnt_hdl);
int ipa3_disable_gsi_data_path(u32 clnt_hdl);
int ipa3_alloc_rule_id(struct idr *rule_ids);
int ipa3_alloc_counter_id(struct ipa_ioc_flt_rt_counter_alloc *counter);
void ipa3_counter_remove_hdl(int hdl);
void ipa3_counter_id_remove_all(void);
int ipa3_id_alloc(void *ptr);
bool ipa3_check_idr_if_freed(void *ptr);
void *ipa3_id_find(u32 id);
void ipa3_id_remove(u32 id);
int ipa3_enable_force_clear(u32 request_id, bool throttle_source,
	u32 source_pipe_bitmask, u32 source_pipe_reg_idx);
int ipa3_disable_force_clear(u32 request_id);

int ipa3_cfg_ep_status(u32 clnt_hdl,
		const struct ipahal_reg_ep_cfg_status *ipa_ep_cfg);

bool ipa3_should_pipe_be_suspended(enum ipa_client_type client);
int ipa3_tag_aggr_force_close(int pipe_num);

void ipa3_active_clients_unlock(void);
int ipa3_wdi_init(void);
int ipa_get_wdi_version(void);
bool ipa_wdi_is_tx1_used(void);
int ipa3_write_qmapid_gsi_wdi_pipe(u32 clnt_hdl, u8 qmap_id);
int ipa3_write_qmapid_wdi_pipe(u32 clnt_hdl, u8 qmap_id);
int ipa3_write_qmapid_wdi3_gsi_pipe(u32 clnt_hdl, u8 qmap_id);
int ipa3_tag_process(struct ipa3_desc *desc, int num_descs,
		    unsigned long timeout);

int ipa3_usb_init(void);
void ipa3_usb_exit(void);
int ipa3_usb_register_ready_cb(void);

void ipa3_q6_pre_shutdown_cleanup(void);
void ipa3_q6_post_shutdown_cleanup(void);
void ipa3_q6_pre_powerup_cleanup(void);
void ipa3_update_ssr_state(bool is_ssr);
int ipa3_init_q6_smem(void);

int ipa3_mhi_handle_ipa_config_req(struct ipa_config_req_msg_v01 *config_req);

int ipa3_uc_interface_init(void);
int ipa3_uc_is_gsi_channel_empty(enum ipa_client_type ipa_client);
int ipa3_uc_loaded_check(void);
void ipa3_uc_load_notify(void);
int ipa3_uc_holb_enabled_check(void);
int ipa3_uc_register_ready_cb(struct notifier_block *nb);
int ipa3_uc_unregister_ready_cb(struct notifier_block *nb);
int ipa3_uc_send_cmd(u32 cmd, u32 opcode, u32 expected_status,
		    bool polling_mode, unsigned long timeout_jiffies);
void ipa3_uc_register_handlers(enum ipa3_hw_features feature,
			      struct ipa3_uc_hdlrs *hdlrs);
int ipa3_uc_notify_clk_state(bool enabled);
void ipa3_uc_interface_destroy(void);
int ipa3_dma_setup(void);
void ipa3_dma_shutdown(void);
void ipa3_dma_async_memcpy_notify_cb(void *priv,
		enum ipa_dp_evt_type evt, unsigned long data);

int ipa3_uc_update_hw_flags(u32 flags);

int ipa3_uc_mhi_init_channel(int ipa_ep_idx, int channelHandle,
	int contexArrayIndex, int channelDirection);
int ipa3_uc_mhi_resume_channel(int channelHandle, bool LPTransitionRejected);
int ipa3_uc_memcpy(phys_addr_t dest, phys_addr_t src, int len);
int ipa3_uc_send_remote_ipa_info(u32 remote_addr, uint32_t mbox_n);
int ipa3_uc_quota_monitor(uint64_t quota);
int ipa3_uc_enable_holb_monitor(uint32_t polling_period);
int ipa3_uc_add_holb_monitor(uint16_t gsi_ch, uint32_t action_mask,
	uint32_t max_stuck_count, uint8_t ee);
int ipa3_uc_del_holb_monitor(uint16_t gsi_ch, uint8_t ee);
int ipa3_uc_disable_holb_monitor(void);
int ipa3_uc_setup_event_ring(void);
void ipa3_tag_destroy_imm(void *user1, int user2);
void ipa3_uc_rg10_write_reg(enum ipahal_reg_name reg, u32 n, u32 val);

int ipa3_wigig_init_i(void);
int ipa3_wigig_deinit_i(void);

/* Hardware stats */

#define IPA_STATS_MAX_PIPE_BIT 32

struct ipa_teth_stats_endpoints {
	u32 prod_mask[IPA5_PIPE_REG_NUM];
	u32 dst_ep_mask[IPA5_PIPES_NUM][IPA5_PIPE_REG_NUM];
};

int ipa_hw_stats_init(void);

int ipa_init_flt_rt_stats(void);

int ipa_debugfs_init_stats(struct dentry *parent);

int ipa_init_quota_stats(u32 *pipe_bitmask);

int ipa_get_quota_stats(struct ipa_quota_stats_all *out);

int ipa_reset_quota_stats(enum ipa_client_type client);

int ipa_reset_all_quota_stats(void);

int ipa_drop_stats_init(void);

int ipa_init_drop_stats(u32 *pipe_bitmask);

int ipa_get_drop_stats(struct ipa_drop_stats_all *out);

int ipa_reset_drop_stats(enum ipa_client_type client);

int ipa_reset_all_drop_stats(void);

int ipa_init_teth_stats(struct ipa_teth_stats_endpoints *in);

int ipa_get_teth_stats(void);

int ipa_query_teth_stats(enum ipa_client_type prod,
	struct ipa_quota_stats_all *out, bool reset);

int ipa_reset_teth_stats(enum ipa_client_type prod, enum ipa_client_type cons);

int ipa_reset_all_cons_teth_stats(enum ipa_client_type prod);

int ipa_reset_all_teth_stats(void);

int ipa_get_flt_rt_stats(struct ipa_ioc_flt_rt_query *query);

int ipa_set_flt_rt_stats(int index, struct ipa_flt_rt_stats stats);

bool ipa_get_fnr_info(struct ipacm_fnr_info *fnr_info);

u32 ipa3_get_max_num_pipes(void);
u32 ipa3_get_num_pipes(void);
struct ipa_smmu_cb_ctx *ipa3_get_smmu_ctx(enum ipa_smmu_cb_type);
struct iommu_domain *ipa3_get_smmu_domain(void);
struct iommu_domain *ipa3_get_uc_smmu_domain(void);
struct iommu_domain *ipa3_get_wlan_smmu_domain(void);
struct iommu_domain *ipa3_get_wlan1_smmu_domain(void);
struct iommu_domain *ipa3_get_eth_smmu_domain(void);
struct iommu_domain *ipa3_get_eth1_smmu_domain(void);
struct iommu_domain *ipa3_get_smmu_domain_by_type
	(enum ipa_smmu_cb_type cb_type);
int ipa3_iommu_map(struct iommu_domain *domain, unsigned long iova,
	phys_addr_t paddr, size_t size, int prot);
int ipa3_ap_suspend(struct device *dev);
int ipa3_ap_freeze(struct device *dev);
int ipa3_ap_resume(struct device *dev);
int ipa3_init_interrupts(void);
struct iommu_domain *ipa3_get_smmu_domain(void);
int ipa3_release_wdi_mapping(u32 num_buffers, struct ipa_wdi_buffer_info *info);
int ipa3_create_wdi_mapping(u32 num_buffers, struct ipa_wdi_buffer_info *info);
int ipa3_set_flt_tuple_mask(int pipe_idx, struct ipahal_reg_hash_tuple *tuple);
int ipa3_set_rt_tuple_mask(int tbl_idx, struct ipahal_reg_hash_tuple *tuple);
void ipa3_set_resorce_groups_min_max_limits(void);
void ipa3_set_resorce_groups_config(void);
int ipa3_suspend_apps_pipes(bool suspend);
void ipa3_force_close_coal(
	bool close_wan,
	bool close_lan );
int ipa3_flt_read_tbl_from_hw(u32 pipe_idx,
	enum ipa_ip_type ip_type,
	bool hashable,
	struct ipahal_flt_rule_entry entry[],
	int *num_entry);
int ipa3_rt_read_tbl_from_hw(u32 tbl_idx,
	enum ipa_ip_type ip_type,
	bool hashable,
	struct ipahal_rt_rule_entry entry[],
	int *num_entry);
int ipa3_inject_dma_task_for_gsi(void);
int ipa3_uc_panic_notifier(struct notifier_block *this,
	unsigned long event, void *ptr);
void ipa3_inc_acquire_wakelock(void);
void ipa3_dec_release_wakelock(void);
int ipa3_load_fws(const struct firmware *firmware, phys_addr_t gsi_mem_base,
	enum gsi_ver);
int emulator_load_fws(
	const struct firmware *firmware,
	u32 transport_mem_base,
	u32 transport_mem_size,
	enum gsi_ver);
int ipa3_rmnet_ctl_init(void);
int ipa3_setup_apps_low_lat_prod_pipe(bool rmnet_config,
	struct rmnet_egress_param *egress_param);
int ipa3_setup_apps_low_lat_cons_pipe(bool rmnet_config,
	struct rmnet_ingress_param *ingress_param);
int ipa3_teardown_apps_low_lat_pipes(void);
int ipa3_rmnet_ll_init(void);
int ipa3_setup_apps_low_lat_data_prod_pipe(
	struct rmnet_egress_param *egress_param,
	struct net_device *dev);
int ipa3_setup_apps_low_lat_data_cons_pipe(
	struct rmnet_ingress_param *ingress_param,
	struct net_device *dev);
int ipa3_teardown_apps_low_lat_data_pipes(void);
const char *ipa_hw_error_str(enum ipa3_hw_errors err_type);
int ipa_gsi_ch20_wa(void);
int ipa3_lan_rx_poll(u32 clnt_hdl, int weight);
int ipa3_smmu_map_peer_reg(phys_addr_t phys_addr, bool map,
	enum ipa_smmu_cb_type cb_type);
int ipa3_smmu_map_peer_buff(u64 iova, u32 size, bool map, struct sg_table *sgt,
	enum ipa_smmu_cb_type cb_type);
void ipa3_reset_freeze_vote(void);
int ipa3_ntn_init(void);
int ipa3_get_ntn_stats(struct Ipa3HwStatsNTNInfoData_t *stats);
struct dentry *ipa_debugfs_get_root(void);
bool ipa3_is_msm_device(void);
void ipa3_enable_dcd(void);
void ipa3_disable_prefetch(enum ipa_client_type client);
void ipa3_dealloc_common_event_ring(void);
int ipa3_alloc_common_event_ring(void);
int ipa3_allocate_dma_task_for_gsi(void);
void ipa3_free_dma_task_for_gsi(void);
int ipa3_allocate_coal_close_frame(void);
void ipa3_free_coal_close_frame(void);
int ipa3_set_clock_plan_from_pm(int idx);
void __ipa_gsi_irq_rx_scedule_poll(struct ipa3_sys_context *sys);
void ipa3_init_imm_cmd_desc(struct ipa3_desc *desc,
	struct ipahal_imm_cmd_pyld *cmd_pyld);
uint ipa3_get_emulation_type(void);
int ipa3_get_transport_info(
	phys_addr_t *phys_addr_ptr,
	unsigned long *size_ptr);
irq_handler_t ipa3_get_isr(void);
void ipa_pc_qmp_enable(void);
u32 ipa3_get_r_rev_version(void);
void ipa3_notify_clients_registered(void);
#if defined(CONFIG_IPA3_REGDUMP)
int ipa_reg_save_init(u32 value);
void ipa_save_registers(void);
void ipa_save_gsi_ver(void);
#else
static inline int ipa_reg_save_init(u32 value) { return 0; }
static inline void ipa_save_registers(void) {};
static inline void ipa_save_gsi_ver(void) {};
#endif

#ifdef CONFIG_IPA_ETH
int ipa_eth_init(void);
void ipa_eth_exit(void);
#else
static inline int ipa_eth_init(void) { return 0; }
static inline void ipa_eth_exit(void) { }
#endif
void ipa3_eth_debugfs_add_node(struct ipa_eth_client *client);
int ipa3_eth_connect(
	struct ipa_eth_client_pipe_info *pipe,
	enum ipa_client_type client_type);
int ipa3_eth_disconnect(
	struct ipa_eth_client_pipe_info *pipe,
	enum ipa_client_type client_type);
#if IPA_ETH_API_VER < 2
int ipa3_eth_client_conn_evt(struct ipa_ecm_msg *msg);
int ipa3_eth_client_disconn_evt(struct ipa_ecm_msg *msg);
#endif
void ipa_eth_ntn3_get_status(struct ipa_ntn3_client_stats *s, unsigned inst_id);
void ipa3_eth_get_status(u32 client, int scratch_id,
	struct ipa3_eth_error_stats *stats);
int ipa3_get_gsi_chan_info(struct gsi_chan_info *gsi_chan_info,
	unsigned long chan_hdl);
enum ipa_client_type ipa_eth_get_ipa_client_type_from_eth_type(
	enum ipa_eth_client_type eth_client_type, enum ipa_eth_pipe_direction dir);

bool ipa_eth_client_exist(enum ipa_eth_client_type eth_client_type, int inst_id);

int ipa3_disable_apps_wan_cons_deaggr(uint32_t agg_size, uint32_t agg_count);

#if IS_ENABLED(CONFIG_IPA3_MHI_PRIME_MANAGER)
int ipa_mpm_init(void);
void ipa_mpm_exit(void);
int ipa_mpm_mhip_xdci_pipe_enable(enum ipa_usb_teth_prot prot);
int ipa_mpm_mhip_xdci_pipe_disable(enum ipa_usb_teth_prot xdci_teth_prot);
int ipa_mpm_notify_wan_state(struct wan_ioctl_notify_wan_state *state);
int ipa3_is_mhip_offload_enabled(void);
int ipa_mpm_reset_dma_mode(enum ipa_client_type src_pipe,
	enum ipa_client_type dst_pipe);
int ipa_mpm_panic_handler(char *buf, int size);
int ipa3_mpm_enable_adpl_over_odl(bool enable);
int ipa3_get_mhip_gsi_stats(struct ipa_uc_dbg_ring_stats *stats);
#else /* IS_ENABLED(CONFIG_IPA3_MHI_PRIME_MANAGER) */
static inline int ipa_mpm_init(void)
{
	return 0;
}
static inline void ipa_mpm_exit(void)
{
	return;
}
static inline int ipa_mpm_mhip_xdci_pipe_enable(
	enum ipa_usb_teth_prot prot)
{
	return 0;
}
static inline int ipa_mpm_mhip_xdci_pipe_disable(
	enum ipa_usb_teth_prot xdci_teth_prot)
{
	return 0;
}
static inline int ipa_mpm_notify_wan_state(
	struct wan_ioctl_notify_wan_state *state)
{
	return 0;
}
static inline int ipa3_is_mhip_offload_enabled(void)
{
	return 0;
}
static inline int ipa_mpm_reset_dma_mode(enum ipa_client_type src_pipe,
	enum ipa_client_type dst_pipe)
{
	return 0;
}
static inline int ipa_mpm_panic_handler(char *buf, int size)
{
	return 0;
}

static inline int ipa3_get_mhip_gsi_stats(struct ipa_uc_dbg_ring_stats *stats)
{
	return 0;
}

static inline int ipa3_mpm_enable_adpl_over_odl(bool enable)
{
	return 0;
}
#endif /* IS_ENABLED(CONFIG_IPA3_MHI_PRIME_MANAGER) */

static inline void *alloc_and_init(u32 size, u32 init_val)
{
	void *ptr = kmalloc(size, GFP_KERNEL);

	if (ptr)
		memset(ptr, init_val, size);

	return ptr;
}

/**
 * The following used as defaults for struct ipa_ioc_coal_evict_policy.
 */
#define IPA_COAL_VP_LRU_THRSHLD        0
#define IPA_COAL_EVICTION_EN           true
#define IPA_COAL_VP_LRU_GRAN_SEL       0
#define IPA_COAL_VP_LRU_UDP_THRSHLD    0
#define IPA_COAL_VP_LRU_TCP_THRSHLD    0
#define IPA_COAL_VP_LRU_UDP_THRSHLD_EN 1
#define IPA_COAL_VP_LRU_TCP_THRSHLD_EN 1
#define IPA_COAL_VP_LRU_TCP_NUM        0

/**
 * enum ipa_evict_time_gran_type - Time granularity to be used with
 * eviction timers.
 */
enum ipa_evict_time_gran_type {
	IPA_EVICT_TIME_GRAN_0,
	IPA_EVICT_TIME_GRAN_1,
	IPA_EVICT_TIME_GRAN_2,
	IPA_EVICT_TIME_GRAN_3,
};

/* query ipa APQ mode*/
bool ipa3_is_apq(void);
/* check if odl is connected */
bool ipa3_is_odl_connected(void);

int ipa3_uc_send_enable_flow_control(uint16_t gsi_chid,
	uint16_t redMarkerThreshold);
int ipa3_uc_send_disable_flow_control(void);
int ipa3_uc_send_update_flow_control(uint32_t bitmask,
	uint8_t  add_delete);

bool ipa_is_test_prod_flt_in_sram_internal(enum ipa_ip_type ip);
/* check if modem is up */
bool ipa3_is_modem_up(void);
/* set modem is up */
void ipa3_set_modem_up(bool is_up);
int ipa3_qmi_reg_dereg_for_bw(bool bw_reg_dereg);

/*
 * To check if the eogre is worthy of sending to recipients who would
 * use the data.
 */
int ipa3_check_eogre(
	struct ipa_ioc_eogre_info *eogre_info,
	bool                      *send2uC,
	bool                      *send2ipacm );

/*
 * To send map information to uC
 */
int ipa3_add_dscp_vlan_pcp_map(
	struct IpaDscpVlanPcpMap_t *map );

/*
 * To send enable/disable information to ipacm
 */
int ipa3_send_eogre_info(
	enum ipa_eogre_event etype,
	struct ipa_ioc_eogre_info *info );

/* update mhi ctrl pipe state */
void ipa3_update_mhi_ctrl_state(u8 state, bool set);
/* Send MHI endpoint info to modem using QMI indication message */
int ipa_send_mhi_endp_ind_to_modem(void);

/*
 * To pass macsec mapping to the IPACM
 */
int ipa3_send_macsec_info(enum ipa_macsec_event event_type, struct ipa_macsec_map *map);

/* Peripheral stats APIs */
/* Non periodic/Event based stats update */
int ipa3_update_usb_per_stats(enum ipa_per_stats_type_e stats_type, uint32_t data);
int ipa3_update_pcie_per_stats(enum ipa_per_stats_type_e stats_type, uint32_t data);
int ipa3_update_wifi_per_stats(enum ipa_per_stats_type_e stats_type, uint32_t data);
int ipa3_update_eth_per_stats(enum ipa_per_stats_type_e stats_type, uint32_t data);
int ipa3_update_apps_per_stats(enum ipa_per_stats_type_e stats_type, uint32_t data);
/* Periodic stats update */
int ipa3_update_client_holb_per_stats(enum ipa_per_stats_type_e stats_type, uint32_t data);
int ipa3_update_dma_per_stats(enum ipa_per_stats_type_e stats_type, uint32_t data);

/* XR-IPA API's */
#ifdef CONFIG_IPA_RTP
int ipa3_uc_send_tuple_info_cmd(struct traffic_tuple_info *data, uint8_t stream_id);
int ipa3_alloc_temp_buffs_to_uc(unsigned int size, unsigned int no_of_buffs);
int ipa3_map_buff_to_device_addr(struct map_buffer *map_buffs);
int ipa3_unmap_buff_from_device_addr(struct unmap_buffer *unmap_buffs);
int ipa3_send_bitstream_buff_info(struct bitstream_buffers *data);
int ipa3_tuple_info_cmd_to_wlan_uc(struct traffic_tuple_info *req, u32 stream_id);
int ipa3_uc_send_remove_stream_cmd(struct remove_bitstream_buffers *data);
int ipa3_create_hfi_send_uc(void);
int ipa3_allocate_uc_pipes_er_tr_send_to_uc(void);
void ipa3_free_uc_temp_buffs(unsigned int no_of_buffs);
void ipa3_free_uc_pipes_er_tr(void);
int ipa3_uc_send_add_bitstream_buffers_cmd(struct bitstream_buffers_to_uc *data);
void ipa3_synx_uninitialize(void);
#endif
#endif /* _IPA3_I_H_ */
