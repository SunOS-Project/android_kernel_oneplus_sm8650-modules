/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _UAPI_MSM_KGSL_H
#define _UAPI_MSM_KGSL_H

#include <linux/types.h>
#include <linux/ioctl.h>

/*
 * The KGSL version has proven not to be very useful in userspace if features
 * are cherry picked into other trees out of order so it is frozen as of 3.14.
 * It is left here for backward compatibility and as a reminder that
 * software releases are never linear. Also, I like pie.
 */

#define KGSL_VERSION_MAJOR        3
#define KGSL_VERSION_MINOR        14

/*
 * We have traditionally mixed context and issueibcmds / command batch flags
 * together into a big flag stew. This worked fine until we started adding a
 * lot more command batch flags and we started running out of bits. Turns out
 * we have a bit of room in the context type / priority mask that we could use
 * for command batches, but that means we need to split out the flags into two
 * coherent sets.
 *
 * If any future definitions are for both context and cmdbatch add both defines
 * and link the cmdbatch to the context define as we do below. Otherwise feel
 * free to add exclusive bits to either set.
 */

/* --- context flags --- */
#define KGSL_CONTEXT_SAVE_GMEM		0x00000001
#define KGSL_CONTEXT_NO_GMEM_ALLOC	0x00000002
/* This is a cmdbatch exclusive flag - use the CMDBATCH equivalent instead */
#define KGSL_CONTEXT_SUBMIT_IB_LIST	0x00000004
#define KGSL_CONTEXT_CTX_SWITCH		0x00000008
#define KGSL_CONTEXT_PREAMBLE		0x00000010
#define KGSL_CONTEXT_TRASH_STATE	0x00000020
#define KGSL_CONTEXT_PER_CONTEXT_TS	0x00000040
#define KGSL_CONTEXT_USER_GENERATED_TS	0x00000080
/* This is a cmdbatch exclusive flag - use the CMDBATCH equivalent instead */
#define KGSL_CONTEXT_END_OF_FRAME	0x00000100
#define KGSL_CONTEXT_NO_FAULT_TOLERANCE 0x00000200
/* This is a cmdbatch exclusive flag - use the CMDBATCH equivalent instead */
#define KGSL_CONTEXT_SYNC               0x00000400
#define KGSL_CONTEXT_PWR_CONSTRAINT     0x00000800
#define KGSL_CONTEXT_PRIORITY_MASK      0x0000F000
#define KGSL_CONTEXT_PRIORITY_SHIFT     12
#define KGSL_CONTEXT_PRIORITY_UNDEF     0

#define KGSL_CONTEXT_IFH_NOP            0x00010000
#define KGSL_CONTEXT_SECURE             0x00020000
#define KGSL_CONTEXT_NO_SNAPSHOT        0x00040000
#define KGSL_CONTEXT_SPARSE             0x00080000

#define KGSL_CONTEXT_PREEMPT_STYLE_MASK       0x0E000000
#define KGSL_CONTEXT_PREEMPT_STYLE_SHIFT      25
#define KGSL_CONTEXT_PREEMPT_STYLE_DEFAULT    0x0
#define KGSL_CONTEXT_PREEMPT_STYLE_RINGBUFFER 0x1
#define KGSL_CONTEXT_PREEMPT_STYLE_FINEGRAIN  0x2

#define KGSL_CONTEXT_TYPE_MASK          0x01F00000
#define KGSL_CONTEXT_TYPE_SHIFT         20
#define KGSL_CONTEXT_TYPE_ANY		0
#define KGSL_CONTEXT_TYPE_GL		1
#define KGSL_CONTEXT_TYPE_CL		2
#define KGSL_CONTEXT_TYPE_C2D		3
#define KGSL_CONTEXT_TYPE_RS		4
#define KGSL_CONTEXT_TYPE_VK		5
#define KGSL_CONTEXT_TYPE_UNKNOWN	0x1E

#define KGSL_CONTEXT_INVALIDATE_ON_FAULT 0x10000000
#define KGSL_CONTEXT_LPAC 0x20000000
#define KGSL_CONTEXT_FAULT_INFO	  0x40000000

#define KGSL_CONTEXT_INVALID 0xffffffff

/*
 * --- command batch flags ---
 * The bits that are linked to a KGSL_CONTEXT equivalent are either legacy
 * definitions or bits that are valid for both contexts and cmdbatches.  To be
 * safe the other 8 bits that are still available in the context field should be
 * omitted here in case we need to share - the other bits are available for
 * cmdbatch only flags as needed
 */
#define KGSL_CMDBATCH_MEMLIST		0x00000001
#define KGSL_CMDBATCH_MARKER		0x00000002
#define KGSL_CMDBATCH_SUBMIT_IB_LIST	KGSL_CONTEXT_SUBMIT_IB_LIST /* 0x004 */
#define KGSL_CMDBATCH_CTX_SWITCH	KGSL_CONTEXT_CTX_SWITCH     /* 0x008 */
#define KGSL_CMDBATCH_PROFILING		0x00000010
/*
 * KGSL_CMDBATCH_PROFILING must also be set for KGSL_CMDBATCH_PROFILING_KTIME
 * to take effect, as the latter only affects the time data returned.
 */
#define KGSL_CMDBATCH_PROFILING_KTIME	0x00000020
#define KGSL_CMDBATCH_END_OF_FRAME	KGSL_CONTEXT_END_OF_FRAME   /* 0x100 */
#define KGSL_CMDBATCH_SYNC		KGSL_CONTEXT_SYNC           /* 0x400 */
#define KGSL_CMDBATCH_PWR_CONSTRAINT	KGSL_CONTEXT_PWR_CONSTRAINT /* 0x800 */
#define KGSL_CMDBATCH_SPARSE	    0x1000 /* 0x1000 */
/* RECURRING bits must be set for LSR workload with IOCTL_KGSL_RECURRING_COMMAND. */
#define KGSL_CMDBATCH_START_RECURRING	0x00100000
#define KGSL_CMDBATCH_STOP_RECURRING	0x00200000

/*
 * Reserve bits [16:19] and bits [28:31] for possible bits shared between
 * contexts and command batches.  Update this comment as new flags are added.
 */

/*
 * gpu_command_object flags - these flags communicate the type of command or
 * memory object being submitted for a GPU command
 */

/* Flags for GPU command objects */
#define KGSL_CMDLIST_IB                  0x00000001U
#define KGSL_CMDLIST_CTXTSWITCH_PREAMBLE 0x00000002U
#define KGSL_CMDLIST_IB_PREAMBLE         0x00000004U

/* Flags for GPU command memory objects */
#define KGSL_OBJLIST_MEMOBJ  0x00000008U
#define KGSL_OBJLIST_PROFILE 0x00000010U

/* Flags for GPU command sync points */
#define KGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP 0
#define KGSL_CMD_SYNCPOINT_TYPE_FENCE 1
#define KGSL_CMD_SYNCPOINT_TYPE_TIMELINE 2

/* --- Memory allocation flags --- */

/* General allocation hints */
#define KGSL_MEMFLAGS_SECURE       (1ULL << 3)
#define KGSL_MEMFLAGS_GPUREADONLY  (1ULL << 24)
#define KGSL_MEMFLAGS_GPUWRITEONLY (1ULL << 25)
#define KGSL_MEMFLAGS_FORCE_32BIT  (1ULL << 32)

/* Flag for binding all the virt range to single phys data */
#define KGSL_SPARSE_BIND_MULTIPLE_TO_PHYS 0x400000000ULL
#define KGSL_SPARSE_BIND 0x1ULL
#define KGSL_SPARSE_UNBIND 0x2ULL

/* Memory caching hints */
#define KGSL_CACHEMODE_MASK       0x0C000000U
#define KGSL_CACHEMODE_SHIFT 26

#define KGSL_CACHEMODE_WRITECOMBINE 0
#define KGSL_CACHEMODE_UNCACHED 1
#define KGSL_CACHEMODE_WRITETHROUGH 2
#define KGSL_CACHEMODE_WRITEBACK 3

#define KGSL_MEMFLAGS_USE_CPU_MAP     (1ULL << 28)
#define KGSL_MEMFLAGS_SPARSE_PHYS     (1ULL << 29)
#define KGSL_MEMFLAGS_SPARSE_VIRT     (1ULL << 30)
#define KGSL_MEMFLAGS_IOCOHERENT      (1ULL << 31)
#define KGSL_MEMFLAGS_GUARD_PAGE      (1ULL << 33)
#define KGSL_MEMFLAGS_VBO             (1ULL << 34)
#define KGSL_MEMFLAGS_VBO_NO_MAP_ZERO (1ULL << 35)

/* Memory types for which allocations are made */
#define KGSL_MEMTYPE_MASK		0x0000FF00
#define KGSL_MEMTYPE_SHIFT		8

#define KGSL_MEMTYPE_OBJECTANY			0
#define KGSL_MEMTYPE_FRAMEBUFFER		1
#define KGSL_MEMTYPE_RENDERBUFFER		2
#define KGSL_MEMTYPE_ARRAYBUFFER		3
#define KGSL_MEMTYPE_ELEMENTARRAYBUFFER		4
#define KGSL_MEMTYPE_VERTEXARRAYBUFFER		5
#define KGSL_MEMTYPE_TEXTURE			6
#define KGSL_MEMTYPE_SURFACE			7
#define KGSL_MEMTYPE_EGL_SURFACE		8
#define KGSL_MEMTYPE_GL				9
#define KGSL_MEMTYPE_CL				10
#define KGSL_MEMTYPE_CL_BUFFER_MAP		11
#define KGSL_MEMTYPE_CL_BUFFER_NOMAP		12
#define KGSL_MEMTYPE_CL_IMAGE_MAP		13
#define KGSL_MEMTYPE_CL_IMAGE_NOMAP		14
#define KGSL_MEMTYPE_CL_KERNEL_STACK		15
#define KGSL_MEMTYPE_COMMAND			16
#define KGSL_MEMTYPE_2D				17
#define KGSL_MEMTYPE_EGL_IMAGE			18
#define KGSL_MEMTYPE_EGL_SHADOW			19
#define KGSL_MEMTYPE_MULTISAMPLE		20
#define KGSL_MEMTYPE_KERNEL			255

/*
 * Alignment hint, passed as the power of 2 exponent.
 * i.e 4k (2^12) would be 12, 64k (2^16)would be 16.
 */
#define KGSL_MEMALIGN_MASK		0x00FF0000
#define KGSL_MEMALIGN_SHIFT		16

enum kgsl_user_mem_type {
	KGSL_USER_MEM_TYPE_PMEM		= 0x00000000,
	KGSL_USER_MEM_TYPE_ASHMEM	= 0x00000001,
	KGSL_USER_MEM_TYPE_ADDR		= 0x00000002,
	KGSL_USER_MEM_TYPE_ION		= 0x00000003,
	/*
	 * ION type is retained for backward compatibility but ION buffers are
	 * dma-bufs so try to use that naming if we can
	 */
	KGSL_USER_MEM_TYPE_DMABUF       = 0x00000003,
	KGSL_USER_MEM_TYPE_MAX		= 0x00000007,
};
#define KGSL_MEMFLAGS_USERMEM_MASK 0x000000e0
#define KGSL_MEMFLAGS_USERMEM_SHIFT 5

/*
 * Unfortunately, enum kgsl_user_mem_type starts at 0 which does not
 * leave a good value for allocated memory. In the flags we use
 * 0 to indicate allocated memory and thus need to add 1 to the enum
 * values.
 */
#define KGSL_USERMEM_FLAG(x) (((x) + 1) << KGSL_MEMFLAGS_USERMEM_SHIFT)

#define KGSL_MEMFLAGS_NOT_USERMEM 0
#define KGSL_MEMFLAGS_USERMEM_PMEM KGSL_USERMEM_FLAG(KGSL_USER_MEM_TYPE_PMEM)
#define KGSL_MEMFLAGS_USERMEM_ASHMEM \
		KGSL_USERMEM_FLAG(KGSL_USER_MEM_TYPE_ASHMEM)
#define KGSL_MEMFLAGS_USERMEM_ADDR KGSL_USERMEM_FLAG(KGSL_USER_MEM_TYPE_ADDR)
#define KGSL_MEMFLAGS_USERMEM_ION KGSL_USERMEM_FLAG(KGSL_USER_MEM_TYPE_ION)

/* --- generic KGSL flag values --- */

#define KGSL_FLAGS_NORMALMODE  0x00000000
#define KGSL_FLAGS_SAFEMODE    0x00000001
#define KGSL_FLAGS_INITIALIZED0 0x00000002
#define KGSL_FLAGS_INITIALIZED 0x00000004
#define KGSL_FLAGS_STARTED     0x00000008
#define KGSL_FLAGS_ACTIVE      0x00000010
#define KGSL_FLAGS_RESERVED0   0x00000020
#define KGSL_FLAGS_RESERVED1   0x00000040
#define KGSL_FLAGS_RESERVED2   0x00000080
#define KGSL_FLAGS_SOFT_RESET  0x00000100
#define KGSL_FLAGS_PER_CONTEXT_TIMESTAMPS 0x00000200

/* Server Side Sync Timeout in milliseconds */
#define KGSL_SYNCOBJ_SERVER_TIMEOUT 2000

/* UBWC Modes */
#define KGSL_UBWC_NONE	0
#define KGSL_UBWC_1_0	1
#define KGSL_UBWC_2_0	2
#define KGSL_UBWC_3_0	3
#define KGSL_UBWC_4_0	4
#define KGSL_UBWC_5_0	5

/*
 * Reset status values for context
 */
enum kgsl_ctx_reset_stat {
	KGSL_CTX_STAT_NO_ERROR				= 0x00000000,
	KGSL_CTX_STAT_GUILTY_CONTEXT_RESET_EXT		= 0x00000001,
	KGSL_CTX_STAT_INNOCENT_CONTEXT_RESET_EXT	= 0x00000002,
	KGSL_CTX_STAT_UNKNOWN_CONTEXT_RESET_EXT		= 0x00000003
};

#define KGSL_CONVERT_TO_MBPS(val) \
	(val*1000*1000U)

struct kgsl_devinfo {

	unsigned int device_id;
	/*
	 * chip revision id
	 * coreid:8 majorrev:8 minorrev:8 patch:8
	 */
	unsigned int chip_id;
	unsigned int mmu_enabled;
	unsigned long gmem_gpubaseaddr;
	/*
	 * This field contains the adreno revision
	 * number 200, 205, 220, etc...
	 */
	unsigned int gpu_id;
	__kernel_size_t gmem_sizebytes;
};

/*
 * struct kgsl_devmemstore - this structure defines the region of memory
 * that can be mmap()ed from this driver. The timestamp fields are volatile
 * because they are written by the GPU
 * @soptimestamp: Start of pipeline timestamp written by GPU before the
 * commands in concern are processed
 * @sbz: Unused, kept for 8 byte alignment
 * @eoptimestamp: End of pipeline timestamp written by GPU after the
 * commands in concern are processed
 * @sbz2: Unused, kept for 8 byte alignment
 * @preempted: Indicates if the context was preempted
 * @sbz3: Unused, kept for 8 byte alignment
 * @ref_wait_ts: Timestamp on which to generate interrupt, unused now.
 * @sbz4: Unused, kept for 8 byte alignment
 * @current_context: The current context the GPU is working on
 * @sbz5: Unused, kept for 8 byte alignment
 */
struct kgsl_devmemstore {
	volatile unsigned int soptimestamp;
	unsigned int sbz;
	volatile unsigned int eoptimestamp;
	unsigned int sbz2;
	volatile unsigned int preempted;
	unsigned int sbz3;
	volatile unsigned int ref_wait_ts;
	unsigned int sbz4;
	unsigned int current_context;
	unsigned int sbz5;
};

#define KGSL_MEMSTORE_OFFSET(ctxt_id, field) \
	((ctxt_id)*sizeof(struct kgsl_devmemstore) + \
	 offsetof(struct kgsl_devmemstore, field))

/* timestamp id*/
enum kgsl_timestamp_type {
	KGSL_TIMESTAMP_CONSUMED = 0x00000001, /* start-of-pipeline timestamp */
	KGSL_TIMESTAMP_RETIRED  = 0x00000002, /* end-of-pipeline timestamp*/
	KGSL_TIMESTAMP_QUEUED   = 0x00000003,
};

/* property types - used with kgsl_device_getproperty */
#define KGSL_PROP_DEVICE_INFO		0x1
#define KGSL_PROP_DEVICE_SHADOW		0x2
#define KGSL_PROP_DEVICE_POWER		0x3
#define KGSL_PROP_SHMEM			0x4
#define KGSL_PROP_SHMEM_APERTURES	0x5
#define KGSL_PROP_MMU_ENABLE		0x6
#define KGSL_PROP_INTERRUPT_WAITS	0x7
#define KGSL_PROP_VERSION		0x8
#define KGSL_PROP_GPU_RESET_STAT	0x9
#define KGSL_PROP_PWRCTRL		0xE
#define KGSL_PROP_PWR_CONSTRAINT	0x12
#define KGSL_PROP_UCHE_GMEM_VADDR	0x13
#define KGSL_PROP_SP_GENERIC_MEM	0x14
#define KGSL_PROP_UCODE_VERSION		0x15
#define KGSL_PROP_GPMU_VERSION		0x16
#define KGSL_PROP_HIGHEST_BANK_BIT	0x17
#define KGSL_PROP_DEVICE_BITNESS	0x18
#define KGSL_PROP_DEVICE_QDSS_STM	0x19
#define KGSL_PROP_MIN_ACCESS_LENGTH	0x1A
#define KGSL_PROP_UBWC_MODE		0x1B
#define KGSL_PROP_DEVICE_QTIMER		0x20
#define KGSL_PROP_L3_PWR_CONSTRAINT     0x22
#define KGSL_PROP_SECURE_BUFFER_ALIGNMENT 0x23
#define KGSL_PROP_SECURE_CTXT_SUPPORT 0x24
#define KGSL_PROP_SPEED_BIN		0x25
#define KGSL_PROP_GAMING_BIN		0x26
#define KGSL_PROP_QUERY_CAPABILITIES	0x27
#define KGSL_PROP_CONTEXT_PROPERTY	0x28
#define KGSL_PROP_GPU_MODEL		0x29
#define KGSL_PROP_VK_DEVICE_ID		0x2A
#define KGSL_PROP_IS_LPAC_ENABLED	0x2B
#define KGSL_PROP_GPU_VA64_SIZE		0x2C
#define KGSL_PROP_IS_RAYTRACING_ENABLED	0x2D
#define KGSL_PROP_IS_FASTBLEND_ENABLED		0x2E
#define KGSL_PROP_UCHE_TRAP_BASE	0x2F
#define KGSL_PROP_IS_AQE_ENABLED	0x30
#define KGSL_PROP_GPU_SECURE_VA_SIZE	0x31
#define KGSL_PROP_GPU_SECURE_VA_INUSE	0x32

/*
 * kgsl_capabilities_properties returns a list of supported properties.
 * If the user passes 0 for 'count' the kernel will set it to the number of
 * supported properties. The list is expected to be 'count * sizeof(__u32)'
 * bytes long. The kernel will return the actual number of entries copied into
 * list via 'count'.
 */
struct kgsl_capabilities_properties {
	__u64 list;
	__u32 count;
};

/*
 * KGSL_QUERY_CAPS_PROPERTIES returns a list of the valid properties in the
 * kernel.  The subtype data should be struct kgsl_capabilities_properties
 */
#define KGSL_QUERY_CAPS_PROPERTIES 1

/*
 * kgsl_capabilities allows the user to query kernel capabilities. The 'data'
 * type should be set appropriately for the querytype (see above). Pass 0 to
 * 'size' and the kernel will set it to the expected size of 'data' that is
 * appropriate for querytype (in bytes).
 */
struct kgsl_capabilities {
	__u64 data;
	__u64 size;
	__u32 querytype;
};

struct kgsl_shadowprop {
	unsigned long gpuaddr;
	__kernel_size_t size;
	unsigned int flags; /* contains KGSL_FLAGS_ values */
};

struct kgsl_qdss_stm_prop {
	__u64 gpuaddr;
	__u64 size;
};

struct kgsl_qtimer_prop {
	__u64 gpuaddr;
	__u64 size;
};

struct kgsl_version {
	unsigned int drv_major;
	unsigned int drv_minor;
	unsigned int dev_major;
	unsigned int dev_minor;
};

struct kgsl_sp_generic_mem {
	__u64 local;
	__u64 pvt;
};

struct kgsl_ucode_version {
	unsigned int pfp;
	unsigned int pm4;
};

struct kgsl_gpmu_version {
	unsigned int major;
	unsigned int minor;
	unsigned int features;
};

struct kgsl_context_property {
	__u64 data;
	__u32 size;
	__u32 type;
	__u32 contextid;
};

struct kgsl_context_property_fault {
	__s32 faults;
	__u32 timestamp;
};

struct kgsl_gpu_model {
	char gpu_model[32];
};

/* Context property sub types */
#define KGSL_CONTEXT_PROP_FAULTS 1

/* Performance counter groups */

#define KGSL_PERFCOUNTER_GROUP_CP 0x0
#define KGSL_PERFCOUNTER_GROUP_RBBM 0x1
#define KGSL_PERFCOUNTER_GROUP_PC 0x2
#define KGSL_PERFCOUNTER_GROUP_VFD 0x3
#define KGSL_PERFCOUNTER_GROUP_HLSQ 0x4
#define KGSL_PERFCOUNTER_GROUP_VPC 0x5
#define KGSL_PERFCOUNTER_GROUP_TSE 0x6
#define KGSL_PERFCOUNTER_GROUP_RAS 0x7
#define KGSL_PERFCOUNTER_GROUP_UCHE 0x8
#define KGSL_PERFCOUNTER_GROUP_TP 0x9
#define KGSL_PERFCOUNTER_GROUP_SP 0xA
#define KGSL_PERFCOUNTER_GROUP_RB 0xB
#define KGSL_PERFCOUNTER_GROUP_PWR 0xC
#define KGSL_PERFCOUNTER_GROUP_VBIF 0xD
#define KGSL_PERFCOUNTER_GROUP_VBIF_PWR 0xE
#define KGSL_PERFCOUNTER_GROUP_MH 0xF
#define KGSL_PERFCOUNTER_GROUP_PA_SU 0x10
#define KGSL_PERFCOUNTER_GROUP_SQ 0x11
#define KGSL_PERFCOUNTER_GROUP_SX 0x12
#define KGSL_PERFCOUNTER_GROUP_TCF 0x13
#define KGSL_PERFCOUNTER_GROUP_TCM 0x14
#define KGSL_PERFCOUNTER_GROUP_TCR 0x15
#define KGSL_PERFCOUNTER_GROUP_L2 0x16
#define KGSL_PERFCOUNTER_GROUP_VSC 0x17
#define KGSL_PERFCOUNTER_GROUP_CCU 0x18
#define KGSL_PERFCOUNTER_GROUP_LRZ 0x19
#define KGSL_PERFCOUNTER_GROUP_CMP 0x1A
#define KGSL_PERFCOUNTER_GROUP_ALWAYSON 0x1B
#define KGSL_PERFCOUNTER_GROUP_SP_PWR 0x1C
#define KGSL_PERFCOUNTER_GROUP_TP_PWR 0x1D
#define KGSL_PERFCOUNTER_GROUP_RB_PWR 0x1E
#define KGSL_PERFCOUNTER_GROUP_CCU_PWR 0x1F
#define KGSL_PERFCOUNTER_GROUP_UCHE_PWR 0x20
#define KGSL_PERFCOUNTER_GROUP_CP_PWR 0x21
#define KGSL_PERFCOUNTER_GROUP_GPMU_PWR 0x22
#define KGSL_PERFCOUNTER_GROUP_ALWAYSON_PWR 0x23
#define KGSL_PERFCOUNTER_GROUP_GLC 0x24
#define KGSL_PERFCOUNTER_GROUP_FCHE 0x25
#define KGSL_PERFCOUNTER_GROUP_MHUB 0x26
#define KGSL_PERFCOUNTER_GROUP_GMU_XOCLK 0x27
#define KGSL_PERFCOUNTER_GROUP_GMU_GMUCLK  0x28
#define KGSL_PERFCOUNTER_GROUP_GMU_PERF  0x29
#define KGSL_PERFCOUNTER_GROUP_SW 0x2a
#define KGSL_PERFCOUNTER_GROUP_UFC 0x2b
#define KGSL_PERFCOUNTER_GROUP_BV_CP 0x2c
#define KGSL_PERFCOUNTER_GROUP_BV_PC 0x2d
#define KGSL_PERFCOUNTER_GROUP_BV_VFD 0x2e
#define KGSL_PERFCOUNTER_GROUP_BV_VPC 0x2f
#define KGSL_PERFCOUNTER_GROUP_BV_TP 0x30
#define KGSL_PERFCOUNTER_GROUP_BV_SP 0x31
#define KGSL_PERFCOUNTER_GROUP_BV_UFC 0x32
#define KGSL_PERFCOUNTER_GROUP_BV_TSE 0x33
#define KGSL_PERFCOUNTER_GROUP_BV_RAS 0x34
#define KGSL_PERFCOUNTER_GROUP_BV_LRZ 0x35
#define KGSL_PERFCOUNTER_GROUP_BV_HLSQ 0x36
#define KGSL_PERFCOUNTER_GROUP_MAX 0x37

#define KGSL_PERFCOUNTER_NOT_USED 0xFFFFFFFF
#define KGSL_PERFCOUNTER_BROKEN 0xFFFFFFFE

/* structure holds list of ibs */
struct kgsl_ibdesc {
	unsigned long gpuaddr;
	unsigned long __pad;
	__kernel_size_t sizedwords;
	unsigned int ctrl;
};

/**
 * struct kgsl_cmdbatch_profiling_buffer
 * @wall_clock_s: Ringbuffer submission time (seconds).
 *                If KGSL_CMDBATCH_PROFILING_KTIME is set, time is provided
 *                in kernel clocks, otherwise wall clock time is used.
 * @wall_clock_ns: Ringbuffer submission time (nanoseconds).
 *                 If KGSL_CMDBATCH_PROFILING_KTIME is set time is provided
 *                 in kernel clocks, otherwise wall clock time is used.
 * @gpu_ticks_queued: GPU ticks at ringbuffer submission
 * @gpu_ticks_submitted: GPU ticks when starting cmdbatch execution
 * @gpu_ticks_retired: GPU ticks when finishing cmdbatch execution
 *
 * This structure defines the profiling buffer used to measure cmdbatch
 * execution time
 */
struct kgsl_cmdbatch_profiling_buffer {
	__u64 wall_clock_s;
	__u64 wall_clock_ns;
	__u64 gpu_ticks_queued;
	__u64 gpu_ticks_submitted;
	__u64 gpu_ticks_retired;
};

/* ioctls */
#define KGSL_IOC_TYPE 0x09

/*
 * get misc info about the GPU
 * type should be a value from enum kgsl_property_type
 * value points to a structure that varies based on type
 * sizebytes is sizeof() that structure
 * for KGSL_PROP_DEVICE_INFO, use struct kgsl_devinfo
 * this structure contaings hardware versioning info.
 * for KGSL_PROP_DEVICE_SHADOW, use struct kgsl_shadowprop
 * this is used to find mmap() offset and sizes for mapping
 * struct kgsl_memstore into userspace.
 */
struct kgsl_device_getproperty {
	unsigned int type;
	void __user *value;
	__kernel_size_t sizebytes;
};

#define IOCTL_KGSL_DEVICE_GETPROPERTY \
	_IOWR(KGSL_IOC_TYPE, 0x2, struct kgsl_device_getproperty)

/* IOCTL_KGSL_DEVICE_READ (0x3) - removed 03/2012
 */

/* block until the GPU has executed past a given timestamp
 * timeout is in milliseconds.
 */
struct kgsl_device_waittimestamp {
	unsigned int timestamp;
	unsigned int timeout;
};

#define IOCTL_KGSL_DEVICE_WAITTIMESTAMP \
	_IOW(KGSL_IOC_TYPE, 0x6, struct kgsl_device_waittimestamp)

struct kgsl_device_waittimestamp_ctxtid {
	unsigned int context_id;
	unsigned int timestamp;
	unsigned int timeout;
};

#define IOCTL_KGSL_DEVICE_WAITTIMESTAMP_CTXTID \
	_IOW(KGSL_IOC_TYPE, 0x7, struct kgsl_device_waittimestamp_ctxtid)

/* DEPRECATED: issue indirect commands to the GPU.
 * drawctxt_id must have been created with IOCTL_KGSL_DRAWCTXT_CREATE
 * ibaddr and sizedwords must specify a subset of a buffer created
 * with IOCTL_KGSL_SHAREDMEM_FROM_PMEM
 * flags may be a mask of KGSL_CONTEXT_ values
 * timestamp is a returned counter value which can be passed to
 * other ioctls to determine when the commands have been executed by
 * the GPU.
 *
 * This function is deprecated - consider using IOCTL_KGSL_SUBMIT_COMMANDS
 * instead
 */
struct kgsl_ringbuffer_issueibcmds {
	unsigned int drawctxt_id;
	unsigned long ibdesc_addr;
	unsigned int numibs;
	unsigned int timestamp; /*output param */
	unsigned int flags;
};

#define IOCTL_KGSL_RINGBUFFER_ISSUEIBCMDS \
	_IOWR(KGSL_IOC_TYPE, 0x10, struct kgsl_ringbuffer_issueibcmds)

/* read the most recently executed timestamp value
 * type should be a value from enum kgsl_timestamp_type
 */
struct kgsl_cmdstream_readtimestamp {
	unsigned int type;
	unsigned int timestamp; /*output param */
};

#define IOCTL_KGSL_CMDSTREAM_READTIMESTAMP_OLD \
	_IOR(KGSL_IOC_TYPE, 0x11, struct kgsl_cmdstream_readtimestamp)

#define IOCTL_KGSL_CMDSTREAM_READTIMESTAMP \
	_IOWR(KGSL_IOC_TYPE, 0x11, struct kgsl_cmdstream_readtimestamp)

/* free memory when the GPU reaches a given timestamp.
 * gpuaddr specify a memory region created by a
 * IOCTL_KGSL_SHAREDMEM_FROM_PMEM call
 * type should be a value from enum kgsl_timestamp_type
 */
struct kgsl_cmdstream_freememontimestamp {
	unsigned long gpuaddr;
	unsigned int type;
	unsigned int timestamp;
};

#define IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP \
	_IOW(KGSL_IOC_TYPE, 0x12, struct kgsl_cmdstream_freememontimestamp)

/*
 * Previous versions of this header had incorrectly defined
 * IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP as a read-only ioctl instead
 * of a write only ioctl.  To ensure binary compatibility, the following
 * #define will be used to intercept the incorrect ioctl
 */

#define IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP_OLD \
	_IOR(KGSL_IOC_TYPE, 0x12, struct kgsl_cmdstream_freememontimestamp)

/* create a draw context, which is used to preserve GPU state.
 * The flags field may contain a mask KGSL_CONTEXT_*  values
 */
struct kgsl_drawctxt_create {
	unsigned int flags;
	unsigned int drawctxt_id; /*output param */
};

#define IOCTL_KGSL_DRAWCTXT_CREATE \
	_IOWR(KGSL_IOC_TYPE, 0x13, struct kgsl_drawctxt_create)

/* destroy a draw context */
struct kgsl_drawctxt_destroy {
	unsigned int drawctxt_id;
};

#define IOCTL_KGSL_DRAWCTXT_DESTROY \
	_IOW(KGSL_IOC_TYPE, 0x14, struct kgsl_drawctxt_destroy)

/*
 * add a block of pmem, fb, ashmem or user allocated address
 * into the GPU address space
 */
struct kgsl_map_user_mem {
	int fd;
	unsigned long gpuaddr;   /*output param */
	__kernel_size_t len;
	__kernel_size_t offset;
	unsigned long hostptr;   /*input param */
	enum kgsl_user_mem_type memtype;
	unsigned int flags;
};

#define IOCTL_KGSL_MAP_USER_MEM \
	_IOWR(KGSL_IOC_TYPE, 0x15, struct kgsl_map_user_mem)

struct kgsl_cmdstream_readtimestamp_ctxtid {
	unsigned int context_id;
	unsigned int type;
	unsigned int timestamp; /*output param */
};

#define IOCTL_KGSL_CMDSTREAM_READTIMESTAMP_CTXTID \
	_IOWR(KGSL_IOC_TYPE, 0x16, struct kgsl_cmdstream_readtimestamp_ctxtid)

struct kgsl_cmdstream_freememontimestamp_ctxtid {
	unsigned int context_id;
	unsigned long gpuaddr;
	unsigned int type;
	unsigned int timestamp;
};

#define IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP_CTXTID \
	_IOW(KGSL_IOC_TYPE, 0x17, \
	struct kgsl_cmdstream_freememontimestamp_ctxtid)

/* add a block of pmem or fb into the GPU address space */
struct kgsl_sharedmem_from_pmem {
	int pmem_fd;
	unsigned long gpuaddr;  /*output param */
	unsigned int len;
	unsigned int offset;
};

#define IOCTL_KGSL_SHAREDMEM_FROM_PMEM \
	_IOWR(KGSL_IOC_TYPE, 0x20, struct kgsl_sharedmem_from_pmem)

/* remove memory from the GPU's address space */
struct kgsl_sharedmem_free {
	unsigned long gpuaddr;
};

#define IOCTL_KGSL_SHAREDMEM_FREE \
	_IOW(KGSL_IOC_TYPE, 0x21, struct kgsl_sharedmem_free)

struct kgsl_cff_user_event {
	unsigned char cff_opcode;
	unsigned int op1;
	unsigned int op2;
	unsigned int op3;
	unsigned int op4;
	unsigned int op5;
	unsigned int __pad[2];
};

#define IOCTL_KGSL_CFF_USER_EVENT \
	_IOW(KGSL_IOC_TYPE, 0x31, struct kgsl_cff_user_event)

struct kgsl_gmem_desc {
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
};

struct kgsl_buffer_desc {
	void		*hostptr;
	unsigned long	gpuaddr;
	int		size;
	unsigned int	format;
	unsigned int	pitch;
	unsigned int	enabled;
};

struct kgsl_bind_gmem_shadow {
	unsigned int drawctxt_id;
	struct kgsl_gmem_desc gmem_desc;
	unsigned int shadow_x;
	unsigned int shadow_y;
	struct kgsl_buffer_desc shadow_buffer;
	unsigned int buffer_id;
};

#define IOCTL_KGSL_DRAWCTXT_BIND_GMEM_SHADOW \
	_IOW(KGSL_IOC_TYPE, 0x22, struct kgsl_bind_gmem_shadow)

/* add a block of memory into the GPU address space */

/*
 * IOCTL_KGSL_SHAREDMEM_FROM_VMALLOC deprecated 09/2012
 * use IOCTL_KGSL_GPUMEM_ALLOC instead
 */

struct kgsl_sharedmem_from_vmalloc {
	unsigned long gpuaddr;	/*output param */
	unsigned int hostptr;
	unsigned int flags;
};

#define IOCTL_KGSL_SHAREDMEM_FROM_VMALLOC \
	_IOWR(KGSL_IOC_TYPE, 0x23, struct kgsl_sharedmem_from_vmalloc)

/*
 * This is being deprecated in favor of IOCTL_KGSL_GPUMEM_CACHE_SYNC which
 * supports both directions (flush and invalidate). This code will still
 * work, but by definition it will do a flush of the cache which might not be
 * what you want to have happen on a buffer following a GPU operation.  It is
 * safer to go with IOCTL_KGSL_GPUMEM_CACHE_SYNC
 */

#define IOCTL_KGSL_SHAREDMEM_FLUSH_CACHE \
	_IOW(KGSL_IOC_TYPE, 0x24, struct kgsl_sharedmem_free)

struct kgsl_drawctxt_set_bin_base_offset {
	unsigned int drawctxt_id;
	unsigned int offset;
};

#define IOCTL_KGSL_DRAWCTXT_SET_BIN_BASE_OFFSET \
	_IOW(KGSL_IOC_TYPE, 0x25, struct kgsl_drawctxt_set_bin_base_offset)

enum kgsl_cmdwindow_type {
	KGSL_CMDWINDOW_MIN     = 0x00000000,
	KGSL_CMDWINDOW_2D      = 0x00000000,
	KGSL_CMDWINDOW_3D      = 0x00000001, /* legacy */
	KGSL_CMDWINDOW_MMU     = 0x00000002,
	KGSL_CMDWINDOW_ARBITER = 0x000000FF,
	KGSL_CMDWINDOW_MAX     = 0x000000FF,
};

/* write to the command window */
struct kgsl_cmdwindow_write {
	enum kgsl_cmdwindow_type target;
	unsigned int addr;
	unsigned int data;
};

#define IOCTL_KGSL_CMDWINDOW_WRITE \
	_IOW(KGSL_IOC_TYPE, 0x2e, struct kgsl_cmdwindow_write)

struct kgsl_gpumem_alloc {
	unsigned long gpuaddr; /* output param */
	__kernel_size_t size;
	unsigned int flags;
};

#define IOCTL_KGSL_GPUMEM_ALLOC \
	_IOWR(KGSL_IOC_TYPE, 0x2f, struct kgsl_gpumem_alloc)

struct kgsl_cff_syncmem {
	unsigned long gpuaddr;
	__kernel_size_t len;
	unsigned int __pad[2]; /* For future binary compatibility */
};

#define IOCTL_KGSL_CFF_SYNCMEM \
	_IOW(KGSL_IOC_TYPE, 0x30, struct kgsl_cff_syncmem)

/*
 * A timestamp event allows the user space to register an action following an
 * expired timestamp. Note IOCTL_KGSL_TIMESTAMP_EVENT has been redefined to
 * _IOWR to support fences which need to return a fd for the priv parameter.
 */

struct kgsl_timestamp_event {
	int type;                /* Type of event (see list below) */
	unsigned int timestamp;  /* Timestamp to trigger event on */
	unsigned int context_id; /* Context for the timestamp */
	void __user *priv;	 /* Pointer to the event specific blob */
	__kernel_size_t len;              /* Size of the event specific blob */
};

#define IOCTL_KGSL_TIMESTAMP_EVENT_OLD \
	_IOW(KGSL_IOC_TYPE, 0x31, struct kgsl_timestamp_event)

/* A genlock timestamp event releases an existing lock on timestamp expire */

#define KGSL_TIMESTAMP_EVENT_GENLOCK 1

struct kgsl_timestamp_event_genlock {
	int handle; /* Handle of the genlock lock to release */
};

/* A fence timestamp event releases an existing lock on timestamp expire */

#define KGSL_TIMESTAMP_EVENT_FENCE 2

struct kgsl_timestamp_event_fence {
	int fence_fd; /* Fence to signal */
};

/*
 * Set a property within the kernel.  Uses the same structure as
 * IOCTL_KGSL_GETPROPERTY
 */

#define IOCTL_KGSL_SETPROPERTY \
	_IOW(KGSL_IOC_TYPE, 0x32, struct kgsl_device_getproperty)

#define IOCTL_KGSL_TIMESTAMP_EVENT \
	_IOWR(KGSL_IOC_TYPE, 0x33, struct kgsl_timestamp_event)

/**
 * struct kgsl_gpumem_alloc_id - argument to IOCTL_KGSL_GPUMEM_ALLOC_ID
 * @id: returned id value for this allocation.
 * @flags: mask of KGSL_MEM* values requested and actual flags on return.
 * @size: requested size of the allocation and actual size on return.
 * @mmapsize: returned size to pass to mmap() which may be larger than 'size'
 * @gpuaddr: returned GPU address for the allocation
 *
 * Allocate memory for access by the GPU. The flags and size fields are echoed
 * back by the kernel, so that the caller can know if the request was
 * adjusted.
 *
 * Supported flags:
 * KGSL_MEMFLAGS_GPUREADONLY: the GPU will be unable to write to the buffer
 * KGSL_MEMTYPE*: usage hint for debugging aid
 * KGSL_MEMALIGN*: alignment hint, may be ignored or adjusted by the kernel.
 * KGSL_MEMFLAGS_USE_CPU_MAP: If set on call and return, the returned GPU
 * address will be 0. Calling mmap() will set the GPU address.
 */
struct kgsl_gpumem_alloc_id {
	unsigned int id;
	unsigned int flags;
	__kernel_size_t size;
	__kernel_size_t mmapsize;
	unsigned long gpuaddr;
/* private: reserved for future use*/
	unsigned long __pad[2];
};

#define IOCTL_KGSL_GPUMEM_ALLOC_ID \
	_IOWR(KGSL_IOC_TYPE, 0x34, struct kgsl_gpumem_alloc_id)

/**
 * struct kgsl_gpumem_free_id - argument to IOCTL_KGSL_GPUMEM_FREE_ID
 * @id: GPU allocation id to free
 *
 * Free an allocation by id, in case a GPU address has not been assigned or
 * is unknown. Freeing an allocation by id with this ioctl or by GPU address
 * with IOCTL_KGSL_SHAREDMEM_FREE are equivalent.
 */
struct kgsl_gpumem_free_id {
	unsigned int id;
/* private: reserved for future use*/
	unsigned int __pad;
};

#define IOCTL_KGSL_GPUMEM_FREE_ID \
	_IOWR(KGSL_IOC_TYPE, 0x35, struct kgsl_gpumem_free_id)

/**
 * struct kgsl_gpumem_get_info - argument to IOCTL_KGSL_GPUMEM_GET_INFO
 * @gpuaddr: GPU address to query. Also set on return.
 * @id: GPU allocation id to query. Also set on return.
 * @flags: returned mask of KGSL_MEM* values.
 * @size: returned size of the allocation.
 * @mmapsize: returned size to pass mmap(), which may be larger than 'size'
 * @useraddr: returned address of the userspace mapping for this buffer
 *
 * This ioctl allows querying of all user visible attributes of an existing
 * allocation, by either the GPU address or the id returned by a previous
 * call to IOCTL_KGSL_GPUMEM_ALLOC_ID. Legacy allocation ioctls may not
 * return all attributes so this ioctl can be used to look them up if needed.
 *
 */
struct kgsl_gpumem_get_info {
	unsigned long gpuaddr;
	unsigned int id;
	unsigned int flags;
	__kernel_size_t size;
	__kernel_size_t mmapsize;
	unsigned long useraddr;
/* private: reserved for future use*/
	unsigned long __pad[4];
};

#define IOCTL_KGSL_GPUMEM_GET_INFO\
	_IOWR(KGSL_IOC_TYPE, 0x36, struct kgsl_gpumem_get_info)

/**
 * struct kgsl_gpumem_sync_cache - argument to IOCTL_KGSL_GPUMEM_SYNC_CACHE
 * @gpuaddr: GPU address of the buffer to sync.
 * @id: id of the buffer to sync. Either gpuaddr or id is sufficient.
 * @op: a mask of KGSL_GPUMEM_CACHE_* values
 * @offset: offset into the buffer
 * @length: number of bytes starting from offset to perform
 * the cache operation on
 *
 * Sync the L2 cache for memory headed to and from the GPU - this replaces
 * KGSL_SHAREDMEM_FLUSH_CACHE since it can handle cache management for both
 * directions
 *
 */
struct kgsl_gpumem_sync_cache {
	unsigned long gpuaddr;
	unsigned int id;
	unsigned int op;
	__kernel_size_t offset;
	__kernel_size_t length;
};

#define KGSL_GPUMEM_CACHE_CLEAN (1 << 0)
#define KGSL_GPUMEM_CACHE_TO_GPU KGSL_GPUMEM_CACHE_CLEAN

#define KGSL_GPUMEM_CACHE_INV (1 << 1)
#define KGSL_GPUMEM_CACHE_FROM_GPU KGSL_GPUMEM_CACHE_INV

#define KGSL_GPUMEM_CACHE_FLUSH \
	(KGSL_GPUMEM_CACHE_CLEAN | KGSL_GPUMEM_CACHE_INV)

/* Flag to ensure backward compatibility of kgsl_gpumem_sync_cache struct */
#define KGSL_GPUMEM_CACHE_RANGE (1 << 31U)

#define IOCTL_KGSL_GPUMEM_SYNC_CACHE \
	_IOW(KGSL_IOC_TYPE, 0x37, struct kgsl_gpumem_sync_cache)

/**
 * struct kgsl_perfcounter_get - argument to IOCTL_KGSL_PERFCOUNTER_GET
 * @groupid: Performance counter group ID
 * @countable: Countable to select within the group
 * @offset: Return offset of the reserved LO counter
 * @offset_hi: Return offset of the reserved HI counter
 *
 * Get an available performance counter from a specified groupid.  The offset
 * of the performance counter will be returned after successfully assigning
 * the countable to the counter for the specified group.  An error will be
 * returned and an offset of 0 if the groupid is invalid or there are no
 * more counters left.  After successfully getting a perfcounter, the user
 * must call kgsl_perfcounter_put(groupid, contable) when finished with
 * the perfcounter to clear up perfcounter resources.
 *
 */
struct kgsl_perfcounter_get {
	unsigned int groupid;
	unsigned int countable;
	unsigned int offset;
	unsigned int offset_hi;
/* private: reserved for future use */
	unsigned int __pad; /* For future binary compatibility */
};

#define IOCTL_KGSL_PERFCOUNTER_GET \
	_IOWR(KGSL_IOC_TYPE, 0x38, struct kgsl_perfcounter_get)

/**
 * struct kgsl_perfcounter_put - argument to IOCTL_KGSL_PERFCOUNTER_PUT
 * @groupid: Performance counter group ID
 * @countable: Countable to release within the group
 *
 * Put an allocated performance counter to allow others to have access to the
 * resource that was previously taken.  This is only to be called after
 * successfully getting a performance counter from kgsl_perfcounter_get().
 *
 */
struct kgsl_perfcounter_put {
	unsigned int groupid;
	unsigned int countable;
/* private: reserved for future use */
	unsigned int __pad[2]; /* For future binary compatibility */
};

#define IOCTL_KGSL_PERFCOUNTER_PUT \
	_IOW(KGSL_IOC_TYPE, 0x39, struct kgsl_perfcounter_put)

/**
 * struct kgsl_perfcounter_query - argument to IOCTL_KGSL_PERFCOUNTER_QUERY
 * @groupid: Performance counter group ID
 * @countables: Return active countables array
 * @count: Number of entries in @countables
 * @max_counters: Return total number counters for the group ID
 *
 * Query the available performance counters given a groupid.  The array
 * *countables is used to return the current active countables in counters.
 * The size of the array is passed in so the kernel will only write at most
 * size or counter->size for the group id.  The total number of available
 * counters for the group ID is returned in max_counters.
 * If the array or size passed in are invalid, then only the maximum number
 * of counters will be returned, no data will be written to *countables.
 * If the groupid is invalid an error code will be returned.
 *
 */
struct kgsl_perfcounter_query {
	unsigned int groupid;
	/* Array to return the current countable for up to size counters */
	unsigned int __user *countables;
	unsigned int count;
	unsigned int max_counters;
/* private: reserved for future use */
	unsigned int __pad[2]; /* For future binary compatibility */
};

#define IOCTL_KGSL_PERFCOUNTER_QUERY \
	_IOWR(KGSL_IOC_TYPE, 0x3A, struct kgsl_perfcounter_query)

/**
 * struct kgsl_perfcounter_read_group - argument to IOCTL_KGSL_PERFCOUNTER_QUERY
 * @groupid: Performance counter group IDs
 * @countable: Performance counter countable IDs
 * @value: Return performance counter reads
 *
 * Read in the current value of a performance counter given by the groupid
 * and countable.
 *
 */

struct kgsl_perfcounter_read_group {
	unsigned int groupid;
	unsigned int countable;
	unsigned long long value;
};

struct kgsl_perfcounter_read {
	struct kgsl_perfcounter_read_group __user *reads;
	unsigned int count;
/* private: reserved for future use */
	unsigned int __pad[2]; /* For future binary compatibility */
};

#define IOCTL_KGSL_PERFCOUNTER_READ \
	_IOWR(KGSL_IOC_TYPE, 0x3B, struct kgsl_perfcounter_read)
/*
 * struct kgsl_gpumem_sync_cache_bulk - argument to
 * IOCTL_KGSL_GPUMEM_SYNC_CACHE_BULK
 * @id_list: list of GPU buffer ids of the buffers to sync
 * @count: number of GPU buffer ids in id_list
 * @op: a mask of KGSL_GPUMEM_CACHE_* values
 *
 * Sync the cache for memory headed to and from the GPU. Certain
 * optimizations can be made on the cache operation based on the total
 * size of the working set of memory to be managed.
 */
struct kgsl_gpumem_sync_cache_bulk {
	unsigned int __user *id_list;
	unsigned int count;
	unsigned int op;
/* private: reserved for future use */
	unsigned int __pad[2]; /* For future binary compatibility */
};

#define IOCTL_KGSL_GPUMEM_SYNC_CACHE_BULK \
	_IOWR(KGSL_IOC_TYPE, 0x3C, struct kgsl_gpumem_sync_cache_bulk)

/*
 * struct kgsl_cmd_syncpoint_timestamp
 * @context_id: ID of a KGSL context
 * @timestamp: GPU timestamp
 *
 * This structure defines a syncpoint comprising a context/timestamp pair. A
 * list of these may be passed by IOCTL_KGSL_SUBMIT_COMMANDS to define
 * dependencies that must be met before the command can be submitted to the
 * hardware
 */
struct kgsl_cmd_syncpoint_timestamp {
	unsigned int context_id;
	unsigned int timestamp;
};

struct kgsl_cmd_syncpoint_fence {
	int fd;
};

/*
 * struct kgsl_cmd_syncpoint_timeline
 * @timelines: Address of an array of &struct kgsl_timeline_val
 * @count: Number of entries in @timelines
 * @timelines_size: Size of each entry in @timelines
 *
 * Define a syncpoint for a number of timelines.  This syncpoint will
 * be satisfied when all of the specified timelines are signaled.
 */
struct kgsl_cmd_syncpoint_timeline {
	__u64 timelines;
	__u32 count;
	__u32 timelines_size;
};

/**
 * struct kgsl_cmd_syncpoint - Define a sync point for a command batch
 * @type: type of sync point defined here
 * @priv: Pointer to the type specific buffer
 * @size: Size of the type specific buffer
 *
 * This structure contains pointers defining a specific command sync point.
 * The pointer and size should point to a type appropriate structure.
 */
struct kgsl_cmd_syncpoint {
	int type;
	void __user *priv;
	__kernel_size_t size;
};

/* Flag to indicate that the cmdlist may contain memlists */
#define KGSL_IBDESC_MEMLIST 0x1

/* Flag to point out the cmdbatch profiling buffer in the memlist */
#define KGSL_IBDESC_PROFILING_BUFFER 0x2

/**
 * struct kgsl_submit_commands - Argument to IOCTL_KGSL_SUBMIT_COMMANDS
 * @context_id: KGSL context ID that owns the commands
 * @flags:
 * @cmdlist: User pointer to a list of kgsl_ibdesc structures
 * @numcmds: Number of commands listed in cmdlist
 * @synclist: User pointer to a list of kgsl_cmd_syncpoint structures
 * @numsyncs: Number of sync points listed in synclist
 * @timestamp: On entry the a user defined timestamp, on exist the timestamp
 * assigned to the command batch
 *
 * This structure specifies a command to send to the GPU hardware.  This is
 * similar to kgsl_issueibcmds expect that it doesn't support the legacy way to
 * submit IB lists and it adds sync points to block the IB until the
 * dependencies are satisified.  This entry point is the new and preferred way
 * to submit commands to the GPU. The memory list can be used to specify all
 * memory that is referrenced in the current set of commands.
 */

struct kgsl_submit_commands {
	unsigned int context_id;
	unsigned int flags;
	struct kgsl_ibdesc __user *cmdlist;
	unsigned int numcmds;
	struct kgsl_cmd_syncpoint __user *synclist;
	unsigned int numsyncs;
	unsigned int timestamp;
/* private: reserved for future use */
	unsigned int __pad[4];
};

#define IOCTL_KGSL_SUBMIT_COMMANDS \
	_IOWR(KGSL_IOC_TYPE, 0x3D, struct kgsl_submit_commands)

/**
 * struct kgsl_device_constraint - device constraint argument
 * @context_id: KGSL context ID
 * @type: type of constraint i.e pwrlevel/none
 * @data: constraint data
 * @size: size of the constraint data
 */
struct kgsl_device_constraint {
	unsigned int type;
	unsigned int context_id;
	void __user *data;
	__kernel_size_t size;
};

/* Constraint Type*/
#define KGSL_CONSTRAINT_NONE 0
#define KGSL_CONSTRAINT_PWRLEVEL 1

/* L3 constraint Type */
#define KGSL_CONSTRAINT_L3_NONE	2
#define KGSL_CONSTRAINT_L3_PWRLEVEL	3

/* PWRLEVEL constraint level*/
/* set to min frequency */
#define KGSL_CONSTRAINT_PWR_MIN    0
/* set to max frequency */
#define KGSL_CONSTRAINT_PWR_MAX    1

struct kgsl_device_constraint_pwrlevel {
	unsigned int level;
};

/**
 * struct kgsl_syncsource_create - Argument to IOCTL_KGSL_SYNCSOURCE_CREATE
 * @id: returned id for the syncsource that was created.
 *
 * This ioctl creates a userspace sync timeline.
 */

struct kgsl_syncsource_create {
	unsigned int id;
/* private: reserved for future use */
	unsigned int __pad[3];
};

#define IOCTL_KGSL_SYNCSOURCE_CREATE \
	_IOWR(KGSL_IOC_TYPE, 0x40, struct kgsl_syncsource_create)

/**
 * struct kgsl_syncsource_destroy - Argument to IOCTL_KGSL_SYNCSOURCE_DESTROY
 * @id: syncsource id to destroy
 *
 * This ioctl creates a userspace sync timeline.
 */

struct kgsl_syncsource_destroy {
	unsigned int id;
/* private: reserved for future use */
	unsigned int __pad[3];
};

#define IOCTL_KGSL_SYNCSOURCE_DESTROY \
	_IOWR(KGSL_IOC_TYPE, 0x41, struct kgsl_syncsource_destroy)

/**
 * struct kgsl_syncsource_create_fence - Argument to
 *     IOCTL_KGSL_SYNCSOURCE_CREATE_FENCE
 * @id: syncsource id
 * @fence_fd: returned sync_fence fd
 *
 * Create a fence that may be signaled by userspace by calling
 * IOCTL_KGSL_SYNCSOURCE_SIGNAL_FENCE. There are no order dependencies between
 * these fences.
 */
struct kgsl_syncsource_create_fence {
	unsigned int id;
	int fence_fd;
/* private: reserved for future use */
	unsigned int __pad[4];
};

/**
 * struct kgsl_syncsource_signal_fence - Argument to
 *     IOCTL_KGSL_SYNCSOURCE_SIGNAL_FENCE
 * @id: syncsource id
 * @fence_fd: sync_fence fd to signal
 *
 * Signal a fence that was created by a IOCTL_KGSL_SYNCSOURCE_CREATE_FENCE
 * call using the same syncsource id. This allows a fence to be shared
 * to other processes but only signaled by the process owning the fd
 * used to create the fence.
 */
#define IOCTL_KGSL_SYNCSOURCE_CREATE_FENCE \
	_IOWR(KGSL_IOC_TYPE, 0x42, struct kgsl_syncsource_create_fence)

struct kgsl_syncsource_signal_fence {
	unsigned int id;
	int fence_fd;
/* private: reserved for future use */
	unsigned int __pad[4];
};

#define IOCTL_KGSL_SYNCSOURCE_SIGNAL_FENCE \
	_IOWR(KGSL_IOC_TYPE, 0x43, struct kgsl_syncsource_signal_fence)

/**
 * struct kgsl_cff_sync_gpuobj - Argument to IOCTL_KGSL_CFF_SYNC_GPUOBJ
 * @offset: Offset into the GPU object to sync
 * @length: Number of bytes to sync
 * @id: ID of the GPU object to sync
 */
struct kgsl_cff_sync_gpuobj {
	__u64 offset;
	__u64 length;
	unsigned int id;
};

#define IOCTL_KGSL_CFF_SYNC_GPUOBJ \
	_IOW(KGSL_IOC_TYPE, 0x44, struct kgsl_cff_sync_gpuobj)

/**
 * struct kgsl_gpuobj_alloc - Argument to IOCTL_KGSL_GPUOBJ_ALLOC
 * @size: Size in bytes of the object to allocate
 * @flags: mask of KGSL_MEMFLAG_* bits
 * @va_len: Size in bytes of the virtual region to allocate
 * @mmapsize: Returns the mmap() size of the object
 * @id: Returns the GPU object ID of the new object
 * @metadata_len: Length of the metdata to copy from the user
 * @metadata: Pointer to the user specified metadata to store for the object
 */
struct kgsl_gpuobj_alloc {
	__u64 size;
	__u64 flags;
	__u64 va_len;
	__u64 mmapsize;
	unsigned int id;
	unsigned int metadata_len;
	__u64 metadata;
};

/* Let the user know that this header supports the gpuobj metadata */
#define KGSL_GPUOBJ_ALLOC_METADATA_MAX 64

#define IOCTL_KGSL_GPUOBJ_ALLOC \
	_IOWR(KGSL_IOC_TYPE, 0x45, struct kgsl_gpuobj_alloc)

/**
 * struct kgsl_gpuobj_free - Argument to IOCTL_KGLS_GPUOBJ_FREE
 * @flags: Mask of: KGSL_GUPOBJ_FREE_ON_EVENT
 * @priv: Pointer to the private object if KGSL_GPUOBJ_FREE_ON_EVENT is
 * specified
 * @id: ID of the GPU object to free
 * @type: If KGSL_GPUOBJ_FREE_ON_EVENT is specified, the type of asynchronous
 * event to free on
 * @len: Length of the data passed in priv
 */
struct kgsl_gpuobj_free {
	__u64 flags;
	__u64 __user priv;
	unsigned int id;
	unsigned int type;
	unsigned int len;
};

#define KGSL_GPUOBJ_FREE_ON_EVENT 1

#define KGSL_GPU_EVENT_TIMESTAMP 1
#define KGSL_GPU_EVENT_FENCE     2

/**
 * struct kgsl_gpu_event_timestamp - Specifies a timestamp event to free a GPU
 * object on
 * @context_id: ID of the timestamp event to wait for
 * @timestamp: Timestamp of the timestamp event to wait for
 */
struct kgsl_gpu_event_timestamp {
	unsigned int context_id;
	unsigned int timestamp;
};

/**
 * struct kgsl_gpu_event_fence - Specifies a fence ID to free a GPU object on
 * @fd: File descriptor for the fence
 */
struct kgsl_gpu_event_fence {
	int fd;
};

#define IOCTL_KGSL_GPUOBJ_FREE \
	_IOW(KGSL_IOC_TYPE, 0x46, struct kgsl_gpuobj_free)

/**
 * struct kgsl_gpuobj_info - argument to IOCTL_KGSL_GPUOBJ_INFO
 * @gpuaddr: GPU address of the object
 * @flags: Current flags for the object
 * @size: Size of the object
 * @va_len: VA size of the object
 * @va_addr: Virtual address of the object (if it is mapped)
 * id - GPU object ID of the object to query
 */
struct kgsl_gpuobj_info {
	__u64 gpuaddr;
	__u64 flags;
	__u64 size;
	__u64 va_len;
	__u64 va_addr;
	unsigned int id;
};

#define IOCTL_KGSL_GPUOBJ_INFO \
	_IOWR(KGSL_IOC_TYPE, 0x47, struct kgsl_gpuobj_info)

/**
 * struct kgsl_gpuobj_import - argument to IOCTL_KGSL_GPUOBJ_IMPORT
 * @priv: Pointer to the private data for the import type
 * @priv_len: Length of the private data
 * @flags: Mask of KGSL_MEMFLAG_ flags
 * @type: Type of the import (KGSL_USER_MEM_TYPE_*)
 * @id: Returns the ID of the new GPU object
 */
struct kgsl_gpuobj_import {
	__u64 __user priv;
	__u64 priv_len;
	__u64 flags;
	unsigned int type;
	unsigned int id;
};

/**
 * struct kgsl_gpuobj_import_dma_buf - import a dmabuf object
 * @fd: File descriptor for the dma-buf object
 */
struct kgsl_gpuobj_import_dma_buf {
	int fd;
};

/**
 * struct kgsl_gpuobj_import_useraddr - import an object based on a useraddr
 * @virtaddr: Virtual address of the object to import
 */
struct kgsl_gpuobj_import_useraddr {
	__u64 virtaddr;
};

#define IOCTL_KGSL_GPUOBJ_IMPORT \
	_IOWR(KGSL_IOC_TYPE, 0x48, struct kgsl_gpuobj_import)

/**
 * struct kgsl_gpuobj_sync_obj - Individual GPU object to sync
 * @offset: Offset within the GPU object to sync
 * @length: Number of bytes to sync
 * @id: ID of the GPU object to sync
 * @op: Cache operation to execute
 */

struct kgsl_gpuobj_sync_obj {
	__u64 offset;
	__u64 length;
	unsigned int id;
	unsigned int op;
};

/**
 * struct kgsl_gpuobj_sync - Argument for IOCTL_KGSL_GPUOBJ_SYNC
 * @objs: Pointer to an array of kgsl_gpuobj_sync_obj structs
 * @obj_len: Size of each item in the array
 * @count: Number of items in the array
 */

struct kgsl_gpuobj_sync {
	__u64 __user objs;
	unsigned int obj_len;
	unsigned int count;
};

#define IOCTL_KGSL_GPUOBJ_SYNC \
	_IOW(KGSL_IOC_TYPE, 0x49, struct kgsl_gpuobj_sync)

/**
 * struct kgsl_command_object - GPU command object
 * @offset: GPU address offset of the object
 * @gpuaddr: GPU address of the object
 * @size: Size of the object
 * @flags: Current flags for the object
 * @id - GPU command object ID
 */
struct kgsl_command_object {
	__u64 offset;
	__u64 gpuaddr;
	__u64 size;
	unsigned int flags;
	unsigned int id;
};

/**
 * struct kgsl_command_syncpoint - GPU syncpoint object
 * @priv: Pointer to the type specific buffer
 * @size: Size of the type specific buffer
 * @type: type of sync point defined here
 */
struct kgsl_command_syncpoint {
	__u64 __user priv;
	__u64 size;
	unsigned int type;
};

/**
 * struct kgsl_gpu_command - Argument for IOCTL_KGSL_GPU_COMMAND
 * @flags: Current flags for the object
 * @cmdlist: List of kgsl_command_objects for submission
 * @cmdsize: Size of kgsl_command_objects structure
 * @numcmds: Number of kgsl_command_objects in command list
 * @objlist: List of kgsl_command_objects for tracking
 * @objsize: Size of kgsl_command_objects structure
 * @numobjs: Number of kgsl_command_objects in object list
 * @synclist: List of kgsl_command_syncpoints
 * @syncsize: Size of kgsl_command_syncpoint structure
 * @numsyncs: Number of kgsl_command_syncpoints in syncpoint list
 * @context_id: Context ID submitting the kgsl_gpu_command
 * @timestamp: Timestamp for the submitted commands
 */
struct kgsl_gpu_command {
	__u64 flags;
	__u64 __user cmdlist;
	unsigned int cmdsize;
	unsigned int numcmds;
	__u64 __user objlist;
	unsigned int objsize;
	unsigned int numobjs;
	__u64 __user synclist;
	unsigned int syncsize;
	unsigned int numsyncs;
	unsigned int context_id;
	unsigned int timestamp;
};

#define IOCTL_KGSL_GPU_COMMAND \
	_IOWR(KGSL_IOC_TYPE, 0x4A, struct kgsl_gpu_command)

/**
 * struct kgsl_preemption_counters_query - argument to
 * IOCTL_KGSL_PREEMPTIONCOUNTER_QUERY
 * @counters: Return preemption counters array
 * @size_user: Size allocated by userspace
 * @size_priority_level: Size of preemption counters for each
 * priority level
 * @max_priority_level: Return max number of priority levels
 *
 * Query the available preemption counters. The array counters
 * is used to return preemption counters. The size of the array
 * is passed in so the kernel will only write at most size_user
 * or max available preemption counters.  The total number of
 * preemption counters is returned in max_priority_level. If the
 * array or size passed in are invalid, then an error is
 * returned back.
 */
struct kgsl_preemption_counters_query {
	__u64 __user counters;
	unsigned int size_user;
	unsigned int size_priority_level;
	unsigned int max_priority_level;
};

#define IOCTL_KGSL_PREEMPTIONCOUNTER_QUERY \
	_IOWR(KGSL_IOC_TYPE, 0x4B, struct kgsl_preemption_counters_query)

/**
 * struct kgsl_gpuobj_set_info - argument for IOCTL_KGSL_GPUOBJ_SET_INFO
 * @flags: Flags to indicate which parameters to change
 * @metadata:  If KGSL_GPUOBJ_SET_INFO_METADATA is set, a pointer to the new
 * metadata
 * @id: GPU memory object ID to change
 * @metadata_len:  If KGSL_GPUOBJ_SET_INFO_METADATA is set, the length of the
 * new metadata string
 * @type: If KGSL_GPUOBJ_SET_INFO_TYPE is set, the new type of the memory object
 */

#define KGSL_GPUOBJ_SET_INFO_METADATA (1 << 0)
#define KGSL_GPUOBJ_SET_INFO_TYPE (1 << 1)

struct kgsl_gpuobj_set_info {
	__u64 flags;
	__u64 metadata;
	unsigned int id;
	unsigned int metadata_len;
	unsigned int type;
};

#define IOCTL_KGSL_GPUOBJ_SET_INFO \
	_IOW(KGSL_IOC_TYPE, 0x4C, struct kgsl_gpuobj_set_info)

/**
 * struct kgsl_sparse_phys_alloc - Argument for IOCTL_KGSL_SPARSE_PHYS_ALLOC
 * @size: Size in bytes to back
 * @pagesize: Pagesize alignment required
 * @flags: Flags for this allocation
 * @id: Returned ID for this allocation
 */
struct kgsl_sparse_phys_alloc {
	__u64 size;
	__u64 pagesize;
	__u64 flags;
	unsigned int id;
};

#define IOCTL_KGSL_SPARSE_PHYS_ALLOC \
	_IOWR(KGSL_IOC_TYPE, 0x50, struct kgsl_sparse_phys_alloc)

/**
 * struct kgsl_sparse_phys_free - Argument for IOCTL_KGSL_SPARSE_PHYS_FREE
 * @id: ID to free
 */
struct kgsl_sparse_phys_free {
	unsigned int id;
};

#define IOCTL_KGSL_SPARSE_PHYS_FREE \
	_IOW(KGSL_IOC_TYPE, 0x51, struct kgsl_sparse_phys_free)

/**
 * struct kgsl_sparse_virt_alloc - Argument for IOCTL_KGSL_SPARSE_VIRT_ALLOC
 * @size: Size in bytes to reserve
 * @pagesize: Pagesize alignment required
 * @flags: Flags for this allocation
 * @id: Returned ID for this allocation
 * @gpuaddr: Returned GPU address for this allocation
 */
struct kgsl_sparse_virt_alloc {
	__u64 size;
	__u64 pagesize;
	__u64 flags;
	__u64 gpuaddr;
	unsigned int id;
};

#define IOCTL_KGSL_SPARSE_VIRT_ALLOC \
	_IOWR(KGSL_IOC_TYPE, 0x52, struct kgsl_sparse_virt_alloc)

/**
 * struct kgsl_sparse_virt_free - Argument for IOCTL_KGSL_SPARSE_VIRT_FREE
 * @id: ID to free
 */
struct kgsl_sparse_virt_free {
	unsigned int id;
};

#define IOCTL_KGSL_SPARSE_VIRT_FREE \
	_IOW(KGSL_IOC_TYPE, 0x53, struct kgsl_sparse_virt_free)

/**
 * struct kgsl_sparse_binding_object - Argument for kgsl_sparse_bind
 * @virtoffset: Offset into the virtual ID
 * @physoffset: Offset into the physical ID (bind only)
 * @size: Size in bytes to reserve
 * @flags: Flags for this kgsl_sparse_binding_object
 * @id: Physical ID to bind (bind only)
 */
struct kgsl_sparse_binding_object {
	__u64 virtoffset;
	__u64 physoffset;
	__u64 size;
	__u64 flags;
	unsigned int id;
};

/**
 * struct kgsl_sparse_bind - Argument for IOCTL_KGSL_SPARSE_BIND
 * @list: List of kgsl_sparse_bind_objects to bind/unbind
 * @id: Virtual ID to bind/unbind
 * @size: Size of kgsl_sparse_bind_object
 * @count: Number of elements in list
 *
 */
struct kgsl_sparse_bind {
	__u64 __user list;
	unsigned int id;
	unsigned int size;
	unsigned int count;
};

#define IOCTL_KGSL_SPARSE_BIND \
	_IOW(KGSL_IOC_TYPE, 0x54, struct kgsl_sparse_bind)

/**
 * struct kgsl_gpu_sparse_command - Argument for
 * IOCTL_KGSL_GPU_SPARSE_COMMAND
 * @flags: Current flags for the object
 * @sparselist: List of kgsl_sparse_binding_object to bind/unbind
 * @synclist: List of kgsl_command_syncpoints
 * @sparsesize: Size of kgsl_sparse_binding_object
 * @numsparse: Number of elements in list
 * @sync_size: Size of kgsl_command_syncpoint structure
 * @numsyncs: Number of kgsl_command_syncpoints in syncpoint list
 * @context_id: Context ID submitting the kgsl_gpu_command
 * @timestamp: Timestamp for the submitted commands
 * @id: Virtual ID to bind/unbind
 */
struct kgsl_gpu_sparse_command {
	__u64 flags;
	__u64 __user sparselist;
	__u64 __user synclist;
	unsigned int sparsesize;
	unsigned int numsparse;
	unsigned int syncsize;
	unsigned int numsyncs;
	unsigned int context_id;
	unsigned int timestamp;
	unsigned int id;
};

#define IOCTL_KGSL_GPU_SPARSE_COMMAND \
	_IOWR(KGSL_IOC_TYPE, 0x55, struct kgsl_gpu_sparse_command)

#define KGSL_GPUMEM_RANGE_OP_BIND 1
#define KGSL_GPUMEM_RANGE_OP_UNBIND 2

/**
 * struct kgsl_gpumem_bind_range - specifies a bind operation for a virtual
 * buffer object
 * @child_offset: Offset to the start of memory within the child buffer object
 * (not used for KGSL_GPUMEM_RANGE_OP_UNBIND operations)
 * @target_offset: GPU address offset within the target VBO
 * @length: Amount of memory to map/unmap (in bytes)
 * @child_id: The GPU buffer ID for the child object to map/unmap in the VBO
 * @op: One of KGSL_GPUMEM_RANGE_OP_BIND or KGSL_GPUMEM_RANGE_OP_UNBIND
 *
 * This defines a specific bind operation to a virtual buffer object specified
 * in &struct kgsl_gpumem_bind_ranges. When @op is KGSL_GPUMEM_RANGE_OP_BIND the
 * physical memory starting at @child_offset in the memory object identified by
 * @child_id will be mapped into the target virtual buffer object starting at
 * @offset for @length bytes.
 *
 * When @op is KGSL_GPUMEM_RANGE_OP_UNBIND any entries in the target virtual
 * buffer object between @offset and @length that belong to @child_id will be
 * removed.
 */
struct kgsl_gpumem_bind_range {
	__u64 child_offset;
	__u64 target_offset;
	__u64 length;
	__u32 child_id;
	__u32 op;
};

#define KGSL_GPUMEM_BIND_ASYNC		(1UL << 0)
#define KGSL_GPUMEM_BIND_FENCE_OUT	(1UL << 1)

/**
 * struct kgsl_gpumem_bind_ranges - Argument to IOCTL_KGSL_GPUMEM_BIND_RANGES to
 * either map or unmap a child buffer object into a virtual buffer object.
 * @ranges: User memory pointer to an array of range operations of type &struct
 * kgsl_gpumem_bind_range
 * @ranges_nents: Number of entries in @ranges
 * @ranges_size: Size of each entry in @ranges in bytes
 * @id: GPU buffer object identifier for the target virtual buffer object
 * @flags: Bitmap of KGSL_GPUMEM_BIND_ASYNC and KGSL_GPUMEM_BIND_FENCE_OUT
 * @fence_id: If KGSL_GPUMEM_BIND_FENCE_OUT is set in @flags contains the
 * identifier for the sync fence that will be signaled after the operation
 * completes
 *
 * Describes a number of range operations to perform on a virtual buffer object
 * identified by @id. Ranges should be a __u64 representation of an array of
 * &struct kgsl_gpumem_bind_range entries. @ranges_nents will contain the number
 * of entries in the array, and @ranges_size will contain the size of each entry
 * in the array. If KGSL_GPUMEM_BIND_ASYNC is set the operation will be
 * performed asynchronously and the operation will immediately return to the
 * user. Otherwise the calling context will block until the operation has
 * completed.
 *
 * If KGSL_GPUMEM_BIND_ASYNC and KGSL_GPUMEM_BIND_FENCE_OUT are both set a sync
 * fence will be created and returned in @fence_id. The fence will be signaled
 * when the bind operation has completed.
 */
struct kgsl_gpumem_bind_ranges {
	__u64 ranges;
	__u32 ranges_nents;
	__u32 ranges_size;
	__u32 id;
	__u32 flags;
	int fence_id;
	/* private: 64 bit compatibility */
	__u32 padding;
};

#define IOCTL_KGSL_GPUMEM_BIND_RANGES \
	_IOWR(KGSL_IOC_TYPE, 0x56, struct kgsl_gpumem_bind_ranges)

#define KGSL_GPU_AUX_COMMAND_BIND	(1 << 0)
#define KGSL_GPU_AUX_COMMAND_TIMELINE	(1 << 1)
/* Reuse the same flag that GPU COMMAND uses */
#define KGSL_GPU_AUX_COMMAND_SYNC	KGSL_CMDBATCH_SYNC

/**
 * struct kgsl_gpu_aux_command_bind - Descriptor for a GPU AUX bind command
 * @rangeslist: Pointer to a list of &struct kgsl_gpumem_bind_range items
 * @numranges Number of entries in @rangeslist
 * @rangesize: Size of each entry in @rangeslist
 * @target: The GPU memory ID for the target virtual buffer object
 *
 * Describe a GPU AUX command to bind ranges in a virtual buffer object.
 * @rangeslist points to a &struct kgsl_gpumem_bind_ranges which is the same
 * struct that is used by IOCTl_KGSL_GPUMEM_BIND_RANGES. @numrages is the size
 * of the array in @rangeslist and @rangesize is the size of each entity in
 * @rangeslist. @target points to the GPU ID for the target VBO object.
 */
struct kgsl_gpu_aux_command_bind {
	__u64 rangeslist;
	__u64 numranges;
	__u64 rangesize;
	__u32 target;
/* private: Padding for 64 bit compatibility */
	__u32 padding;
};

/**
 * struct kgsl_gpu_aux_command_generic - Container for an AUX command
 * @priv: Pointer to the type specific buffer
 * @size: Size of the type specific buffer
 * @type: type of sync point defined here
 *
 * Describes a generic container for GPU aux commands. @priv is a user pointer
 * to the command struct matching @type of size @size.
 */
struct kgsl_gpu_aux_command_generic {
	__u64 priv;
	__u64 size;
	__u32 type;
/* private: Padding for 64 bit compatibility */
	__u32 padding;
};

/**
 * struct kgsl_gpu_aux_command - Argument for IOCTL_KGSL_GPU_AUX_COMMAND
 * @flags: flags for the object
 * @cmdlist: List of &struct kgsl_gpu_aux_command_generic objects
 * @cmd_size: Size of each entry in @cmdlist
 * @numcmds: Number of entries in @cmdlist
 * @synclist: List of &struct kgsl_command_syncpoint objects
 * @syncsize: Size of each entry in @synclist
 * @numsyncs: Number of entries in @synclist
 * @context_id: ID of the context submitting the aux command
 * @timestamp: Timestamp for the command submission
 *
 * Describe a GPU auxiliary command. Auxiliary commands are tasks that are not
 * performed on hardware but can be queued like normal GPU commands. Like GPU
 * commands AUX commands are assigned a timestamp and processed in order in the
 * queue. They can also have standard sync objects attached. The only
 * difference is that AUX commands usually perform some sort of administrative
 * task in the CPU and are retired in the dispatcher.
 *
 * For bind operations flags must have one of the KGSL_GPU_AUX_COMMAND_* flags
 * set. If sync objects are attached KGSL_GPU_AUX_COMMAND_SYNC must be set.
 * @cmdlist points to an array of &struct kgsl_gpu_aux_command_generic structs
 * which in turn will have a pointer to a specific command type.
 * @numcmds is the number of commands in the list and @cmdsize is the size
 * of each entity in @cmdlist.
 *
 * If KGSL_GPU_AUX_COMMAND_SYNC is specified @synclist will point to an array of
 * &struct kgsl_command_syncpoint items in the same fashion as a GPU hardware
 * command. @numsyncs and @syncsize describe the list.
 *
 * @context_id is the context that is submitting the command and @timestamp
 * contains the timestamp for the operation.
 */
struct kgsl_gpu_aux_command {
	__u64 flags;
	__u64 cmdlist;
	__u32 cmdsize;
	__u32 numcmds;
	__u64 synclist;
	__u32 syncsize;
	__u32 numsyncs;
	__u32 context_id;
	__u32 timestamp;
};

#define IOCTL_KGSL_GPU_AUX_COMMAND \
	_IOWR(KGSL_IOC_TYPE, 0x57, struct kgsl_gpu_aux_command)

/**
 * struct kgsl_timeline_create - Argument for IOCTL_KGSL_TIMELINE_CREATE
 * @seqno: Initial sequence number for the timeline
 * @id: Timeline identifier [out]
 *
 * Create a new semaphore timeline and return the identifier in @id.
 * The identifier is global for the device and can be used to
 * identify the timeline in all subsequent commands.
 */
struct kgsl_timeline_create {
	__u64 seqno;
	__u32 id;
/* private: padding for 64 bit compatibility */
	__u32 padding;
};

#define IOCTL_KGSL_TIMELINE_CREATE \
	_IOWR(KGSL_IOC_TYPE, 0x58, struct kgsl_timeline_create)

/**
 * struct kgsl_timeline_val - A container to store a timeline/sequence number
 * pair.
 * @seqno: Sequence number to signal/query
 * @timeline: The timeline identifier to signal/query
 *
 * A container to store a timeline/seqno pair used by the query and signal
 * ioctls.
 */
struct kgsl_timeline_val {
	__u64 seqno;
	__u32 timeline;
/* private: padding for 64 bit compatibility */
	__u32 padding;
};

#define KGSL_TIMELINE_WAIT_ALL 1
#define KGSL_TIMELINE_WAIT_ANY 2

/**
 * struct kgsl_timeline_wait - Argument for IOCTL_KGSL_TIMELINE_WAIT
 * @tv_sec: Number of seconds to wait for the signal
 * @tv_nsec: Number of nanoseconds to wait for the signal
 * @timelines: Address of an array of &struct kgsl_timeline_val entries
 * @count: Number of entries in @timeline
 * @timelines_size: Size of each entry in @timelines
 * @flags: One of KGSL_TIMELINE_WAIT_ALL or KGSL_TIMELINE_WAIT_ANY
 *
 * Wait for the timelines listed in @timelines to be signaled. If @flags is
 * equal to KGSL_TIMELINE_WAIT_ALL then wait for all timelines or if
 * KGSL_TIMELINE_WAIT_ANY is specified then wait for any of the timelines to
 * signal. @tv_sec and @tv_nsec indicates the number of seconds and nanoseconds
 * that the process should be blocked waiting for the signal.
 */
struct kgsl_timeline_wait {
	__s64 tv_sec;
	__s64 tv_nsec;
	__u64 timelines;
	__u32 count;
	__u32 timelines_size;
	__u32 flags;
/* private: padding for 64 bit compatibility */
	__u32 padding;
};

#define IOCTL_KGSL_TIMELINE_WAIT \
	_IOW(KGSL_IOC_TYPE, 0x59, struct kgsl_timeline_wait)

#define IOCTL_KGSL_TIMELINE_QUERY \
	_IOWR(KGSL_IOC_TYPE, 0x5A, struct kgsl_timeline_val)

/**
 * struct kgsl_timeline_signal - argument for IOCTL_KGSL_TIMELINE_SIGNAL
 * @timelines: Address of an array of &struct kgsl_timeline_val entries
 * @count: Number of entries in @timelines
 * @timelines_size: Size of each entry in @timelines
 *
 * Signal an array of timelines of type @struct kgsl_timeline_val.
 */
struct kgsl_timeline_signal {
	__u64 timelines;
	__u32 count;
	__u32 timelines_size;
};

#define IOCTL_KGSL_TIMELINE_SIGNAL \
	_IOW(KGSL_IOC_TYPE, 0x5B, struct kgsl_timeline_signal)

/**
 * struct kgsl_timeline_fence_get - argument for IOCTL_KGSL_TIMELINE_FENCE_GET
 * @seqno: Sequence number for the fence
 * @timeline: Timeline to create the fence on
 * @handle: Contains the fence fd for a successful operation [out]
 *
 * Create a sync file descriptor for the seqnum on the timeline and return it in
 * @handle.  Can be polled and queried just like any other sync file descriptor
 */
struct kgsl_timeline_fence_get {
	__u64 seqno;
	__u32 timeline;
	int handle;
};

#define IOCTL_KGSL_TIMELINE_FENCE_GET \
	_IOWR(KGSL_IOC_TYPE, 0x5C, struct kgsl_timeline_fence_get)
/**
 * IOCTL_KGSL_TIMELINE_DESTROY takes a u32 identifier for the timeline to
 * destroy
 */
#define IOCTL_KGSL_TIMELINE_DESTROY _IOW(KGSL_IOC_TYPE, 0x5D, __u32)

/**
 * struct kgsl_gpu_aux_command_timeline - An aux command for timeline signals
 * @timelines: An array of &struct kgsl_timeline_val elements
 * @count: The number of entries in @timelines
 * @timelines_size: The size of each element in @timelines
 *
 * An aux command for timeline signals that can be pointed to by
 * &struct kgsl_gpu_aux_command_generic when the type is
 * KGSL_GPU_AUX_COMMAND_TIMELINE.
 */
struct kgsl_gpu_aux_command_timeline {
	__u64 timelines;
	__u32 count;
	__u32 timelines_size;
};

/* Macros for fault type used in kgsl_fault structure */
#define KGSL_FAULT_TYPE_NO_FAULT    0
#define KGSL_FAULT_TYPE_PAGEFAULT   1
#define KGSL_FAULT_TYPE_MAX         2

/* Macros to be used in kgsl_pagefault_report structure */
#define KGSL_PAGEFAULT_TYPE_NONE                  0
#define KGSL_PAGEFAULT_TYPE_READ                  (1 << 0)
#define KGSL_PAGEFAULT_TYPE_WRITE                 (1 << 1)
#define KGSL_PAGEFAULT_TYPE_TRANSLATION           (1 << 2)
#define KGSL_PAGEFAULT_TYPE_PERMISSION            (1 << 3)
#define KGSL_PAGEFAULT_TYPE_EXTERNAL              (1 << 4)
#define KGSL_PAGEFAULT_TYPE_TRANSACTION_STALLED   (1 << 5)

/**
 * struct kgsl_pagefault_report - Descriptor for each page fault
 * @fault_addr: page fault address
 * @fault_type: type of page fault
 *
 * Contains information about supported GPU page fault.
 * Supported fault type: KGSL_PAGEFAULT_TYPE_*
 */
struct kgsl_pagefault_report {
	__u64 fault_addr;
	/* private: reserved for future use */
	__u64 reserved[2];
	__u32 fault_type;
	/* private: padding for 64 bit compatibility */
	__u32 __pad;
};

/**
 * struct kgsl_fault - Descriptor for each GPU fault type
 * @fault: User memory pointer to list of specific fault type
 * @type: Type of gpu fault
 * @count: Number of entries in @fault
 * @size: Size of each entry in @fault in bytes
 *
 * Contains information about each GPU fault type. If user passes 0 for all the fields, KGSL
 * will return the @count and @type of fault. Based on this, user can allocate a buffer for
 * specific fault type, fill the @fault and specify the structure size of type specific fault
 * in @size. User can walk through @fault list to parse the fault type specific information.
 *
 * Supported type: KGSL_FAULT_TYPE_*
 */
struct kgsl_fault {
	__u64 fault;
	__u32 type;
	__u32 count;
	__u32 size;
	/* private: padding for 64 bit compatibility */
	__u32 padding;
};

/**
 * struct kgsl_fault_report - Container for list of GPU faults
 * @faultlist: User memory pointer to list of fault descriptor &struct kgsl_fault
 * @faultnents: Number of entries in @faultlist. Each entry corresponds to a fault type i.e.
 * KGSL_FAULT_TYPE_*
 * @faultsize: Size of each entry in @faultlist in bytes
 * @context_id: ID of a KGSL context
 *
 * Returns a list of GPU faults for a context identified by @context_id. If the user specifies
 * @context_id only, then KGSL will set the @faultnents to the number of fault types it has
 * for that context.
 *
 * User is expected to allocate an array of @struct kgsl_fault with @faultnents number of entries
 * and fill the @faultlist field. On calling @IOCTL_KGSL_GET_FAULT_REPORT, KGSL will return the
 * type and count for each fault. Based on this, user needs to update the @kgsl_fault structure.
 * Then, it should call the @IOCTL_KGSL_GET_FAULT_REPORT again for kernel to fill the fault
 * information.
 */
struct kgsl_fault_report {
	__u64 faultlist;
	__u32 faultnents;
	__u32 faultsize;
	__u32 context_id;
	/* private: padding for 64 bit compatibility */
	__u32 padding;
};

#define IOCTL_KGSL_GET_FAULT_REPORT \
	_IOWR(KGSL_IOC_TYPE, 0x5E, struct kgsl_fault_report)

/**
 * struct kgsl_recurring_object - Argument for IOCTL_KGSL_RECURRING_COMMAND
 * @flags: Current flags for the object
 * @cmdlist: List of kgsl_command_objects for submission
 * @cmd_size: Size of kgsl_command_objects structure
 * @numcmds: Number of kgsl_command_objects in command list
 * @objlist: List of kgsl_command_objects for tracking
 * @obj_size: Size of kgsl_command_objects structure
 * @numobjs: Number of kgsl_command_objects in object list
 * @context_id: Context ID submitting the kgsl_recurring_command
 */
struct kgsl_recurring_command {
	__u64 flags;
	__u64 __user cmdlist;
	__u32 cmdsize;
	__u32 numcmds;
	__u64 __user objlist;
	__u32 objsize;
	__u32 numobjs;
	__u32 context_id;
	/* private: Padding for 64 bit compatibility */
	__u32 padding;
};

#define IOCTL_KGSL_RECURRING_COMMAND \
	_IOWR(KGSL_IOC_TYPE, 0x5F, struct kgsl_recurring_command)

enum kgsl_calibrated_time_domain {
	KGSL_CALIBRATED_TIME_DOMAIN_DEVICE = 0,
	KGSL_CALIBRATED_TIME_DOMAIN_MONOTONIC = 1,
	KGSL_CALIBRATED_TIME_DOMAIN_MONOTONIC_RAW = 2,
	KGSL_CALIBRATED_TIME_DOMAIN_MAX,
};

/**
 * struct kgsl_read_calibrated_timestamps - Argument for IOCTL_KGSL_READ_CALIBRATED_TIMESTAMPS
 * @sources: List of time domains of type enum kgsl_calibrated_time_domain
 * @ts: List of calibrated timestamps
 * @deviation: Deviation between timestamp samples in nsecs
 * @count: Number of timestamps to read
 *
 * Returns a list of calibrated timestamps corresponding to an input list of time domains to
 * query.
 */
struct kgsl_read_calibrated_timestamps {
	__u64 __user sources;
	__u64 __user ts;
	__u64 deviation;
	__u32 count;
	/* private: Padding for 64 bit compatibility */
	__u32 padding;
};

#define IOCTL_KGSL_READ_CALIBRATED_TIMESTAMPS \
	_IOWR(KGSL_IOC_TYPE, 0x60, struct kgsl_read_calibrated_timestamps)

#endif /* _UAPI_MSM_KGSL_H */
