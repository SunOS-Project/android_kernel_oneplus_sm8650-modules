// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2017-2020, Pixelworks, Inc.
 *
 * These files contain modifications made by Pixelworks, Inc., in 2019-2020.
 */
#include <dsi_drm.h>
#include <sde_trace.h>
#include "dsi_panel.h"
#include "dsi_iris.h"
#include "dsi_iris_api.h"
#include "dsi_iris_lightup.h"
#include "dsi_iris_lightup_ocp.h"
#include "dsi_iris_lp.h"
#include "dsi_iris_memc.h"
#include "dsi_iris_dts_fw.h"
#include "dsi_iris_timing_switch_def.h"
#include "oplus_display_interface.h"

#define to_dsi_display(x) container_of(x, struct dsi_display, host)

enum {
	SWITCH_ABYP_TO_ABYP = 0,
	SWITCH_ABYP_TO_PT,
	SWITCH_PT_TO_ABYP,
	SWITCH_PT_TO_PT,
	SWITCH_NONE,
};

#define SWITCH_CASE(case)[SWITCH_##case] = #case
static const char * const switch_case_name[] = {
	SWITCH_CASE(ABYP_TO_ABYP),
	SWITCH_CASE(ABYP_TO_PT),
	SWITCH_CASE(PT_TO_ABYP),
	SWITCH_CASE(PT_TO_PT),
	SWITCH_CASE(NONE),
};
#undef SWITCH_CASE

enum {
	TIMING_SWITCH_RES_SEQ,
	TIMING_SWITCH_FPS_SEQ,
	TIMING_SWITCH_FPS_CLK_SEQ,
	TIMING_SWITCH_SEQ_CNT,
};


static uint32_t switch_case;
static struct iris_ctrl_seq tm_switch_seq[TIMING_SWITCH_SEQ_CNT];
static uint32_t cmd_list_index;
static uint32_t panel_tm_num;
static uint32_t iris_cmd_list_cnt;
static uint8_t *tm_cmd_map_arry;
static uint8_t *master_tm_cmd_map_arry;
static struct dsi_mode_info *panel_tm_arry;
static uint32_t cur_tm_index;
static uint32_t new_tm_index;
static uint32_t last_pt_tm_index;
static uint32_t log_level;
static enum iris_chip_type chip_type = CHIP_UNKNOWN;
static bool _iris_is_same_res(const struct dsi_mode_info *new_timing,
		const struct dsi_mode_info *old_timing);


void iris_init_timing_switch(void)
{
	IRIS_LOGI("%s()", __func__);

	chip_type = iris_get_chip_type();
	switch_case = SWITCH_ABYP_TO_ABYP;
	cmd_list_index = IRIS_DTSI_PIP_IDX_START;

	switch (chip_type) {
	case CHIP_IRIS7P:
		iris_init_timing_switch_i7p();
		break;
	case CHIP_IRIS7:
		iris_init_timing_switch_i7();
		break;
	case CHIP_IRIS8:
		iris_init_timing_switch_i8();
		break;
	default:
		IRIS_LOGE("%s(), doesn't support for chip type: %#x",
				__func__, chip_type);
	}
}

void iris_deinit_timing_switch(void)
{
	if (panel_tm_arry) {
		kvfree(panel_tm_arry);
		panel_tm_arry = NULL;
	}

	if (tm_cmd_map_arry) {
		kvfree(tm_cmd_map_arry);
		tm_cmd_map_arry = NULL;
	}

	if (master_tm_cmd_map_arry) {
		kvfree(master_tm_cmd_map_arry);
		master_tm_cmd_map_arry = NULL;
	}
}

static bool _iris_support_timing_switch(void)
{
	if (panel_tm_arry == NULL || panel_tm_num == 0)
		return false;

	return true;
}

void iris_set_panel_timing(struct dsi_display *display, uint32_t index,
		const struct dsi_mode_info *timing)
{
	if (!iris_is_chip_supported())
		return;

	if (display == NULL || timing == NULL || index >= panel_tm_num)
		return;

	// for primary display only, skip secondary
	if (strcmp(display->display_type, "primary"))
		return;

	if (!_iris_support_timing_switch())
		return;

	IRIS_LOGI("%s(), timing@%u: %ux%u@%uHz",
			__func__, index,
			timing->h_active, timing->v_active, timing->refresh_rate);
	memcpy(&panel_tm_arry[index], timing, sizeof(struct dsi_mode_info));
}

static void _iris_init_param(struct device_node *np)
{
	int32_t pnl_tm_num = 0;
	struct iris_dts_ops *p_dts_ops = iris_get_dts_ops();

	if (!p_dts_ops)
		return;

	pnl_tm_num = p_dts_ops->count_u8_elems(np, "pxlw,timing-cmd-map");
	if (pnl_tm_num < 1)
		pnl_tm_num = 0;

	panel_tm_num = pnl_tm_num;
	panel_tm_arry = NULL;
	tm_cmd_map_arry = NULL;
	master_tm_cmd_map_arry = NULL;
	cur_tm_index = 0;
	new_tm_index = 0;
	last_pt_tm_index = 0;

	IRIS_LOGI("%s(), panel timing num: %d", __func__, pnl_tm_num);
	if (panel_tm_num > 1) {
		int32_t master_tm_num = 0;
		u32 buf_size = panel_tm_num * sizeof(struct dsi_mode_info);

		panel_tm_arry = kvzalloc(buf_size, GFP_KERNEL);
		tm_cmd_map_arry = kvzalloc(panel_tm_num, GFP_KERNEL);

		master_tm_num = p_dts_ops->count_u8_elems(np, "pxlw,master-timing-cmd-map");
		if (master_tm_num > 0) {
			IRIS_LOGI("%s(), master timing map number: %d",
					__func__, master_tm_num);
			if (master_tm_num == panel_tm_num) {
				master_tm_cmd_map_arry = kvzalloc(panel_tm_num, GFP_KERNEL);
				IRIS_LOGI("%s(), support master timing", __func__);
			}
		} else {
			IRIS_LOGI("%s(), don't support master timing", __func__);
		}
	}
}

static int32_t _iris_parse_timing_cmd_map(struct device_node *np)
{
	int32_t rc = 0;
	uint32_t i = 0;
	uint32_t j = 0;
	struct iris_dts_ops *p_dts_ops = iris_get_dts_ops();

	if (!p_dts_ops)
		return -EINVAL;

	rc = p_dts_ops->read_u8_array(np, "pxlw,timing-cmd-map",
			tm_cmd_map_arry, panel_tm_num);

	if (rc != 0) {
		IRIS_LOGE("%s(), failed to parse timing cmd map", __func__);
		return rc;
	}

	for (i = 0; i < panel_tm_num; i++) {
		IRIS_LOGI("%s(), cmd list %u for timing@%u",
				__func__, tm_cmd_map_arry[i], i);

		if (tm_cmd_map_arry[i] != IRIS_DTSI_NONE) {
			bool redup = false;

			for (j = 0; j < i; j++) {
				if (tm_cmd_map_arry[j] == tm_cmd_map_arry[i]) {
					redup = true;
					break;
				}
			}

			if (!redup)
				iris_cmd_list_cnt++;
		}
	}

	IRIS_LOGI("%s(), valid cmd list count is %u", __func__, iris_cmd_list_cnt);

	return rc;
}

static int32_t _iris_parse_master_timing_cmd_map(struct device_node *np)
{
	int32_t rc = 0;
	struct iris_dts_ops *p_dts_ops = iris_get_dts_ops();

	if (!p_dts_ops)
		return -EINVAL;
	if (master_tm_cmd_map_arry == NULL)
		return rc;

	rc = p_dts_ops->read_u8_array(np, "pxlw,master-timing-cmd-map",
			master_tm_cmd_map_arry, panel_tm_num);
	if (rc != 0) {
		IRIS_LOGE("%s(), failed to parse master timing cmd map", __func__);
		return rc;
	}

	if (LOG_NORMAL_INFO) {
		uint32_t i = 0;

		for (i = 0; i < panel_tm_num; i++) {
			IRIS_LOGI("%s(), master timing map[%u] = %u", __func__,
					i, master_tm_cmd_map_arry[i]);
		}
	}

	return rc;
}

static int32_t _iris_parse_res_switch_seq(struct device_node *np)
{
	int32_t rc = 0;
	const uint8_t *key = "pxlw,iris-res-switch-sequence";


	rc = iris_parse_optional_seq(np, key, &tm_switch_seq[TIMING_SWITCH_RES_SEQ]);
	IRIS_LOGE_IF(rc != 0, "%s(), failed to parse %s seq", __func__, key);

	return rc;
}

static int32_t _iris_parse_fps_switch_seq(struct device_node *np)
{
	int32_t rc = 0;
	const uint8_t *key = "pxlw,iris-fps-switch-sequence";

	rc = iris_parse_optional_seq(np, key, &tm_switch_seq[TIMING_SWITCH_FPS_SEQ]);
	IRIS_LOGE_IF(rc != 0, "%s(), failed to parse %s seq", __func__, key);

	return rc;
}

static int32_t _iris_parse_fps_clk_switch_seq(struct device_node *np)
{
	int32_t rc = 0;
	const uint8_t *key = "pxlw,iris-fps-clk-switch-sequence";

	rc = iris_parse_optional_seq(np, key, &tm_switch_seq[TIMING_SWITCH_FPS_CLK_SEQ]);
	IRIS_LOGE_IF(rc != 0, "%s(), failed to parse %s seq", __func__, key);

	return rc;
}

uint32_t iris_get_cmd_list_cnt(void)
{
	if (iris_cmd_list_cnt == 0)
		return IRIS_DTSI_PIP_IDX_CNT;

	return iris_cmd_list_cnt;
}

int32_t iris_parse_timing_switch_info(struct device_node *np)
{
	int32_t rc = 0;

	_iris_init_param(np);

	if (panel_tm_num == 0 || panel_tm_num == 1)
		return 0;

	rc = _iris_parse_timing_cmd_map(np);
	IRIS_LOGI_IF(rc != 0,
			"%s(), [optional] have not timing cmd map", __func__);

	rc = _iris_parse_master_timing_cmd_map(np);
	IRIS_LOGI_IF(rc != 0,
			"%s(), [optional] have not master timing cmd map", __func__);

	rc = _iris_parse_res_switch_seq(np);
	IRIS_LOGI_IF(rc != 0,
			"%s(), [optional] have not resolution switch sequence", __func__);

	rc = _iris_parse_fps_switch_seq(np);
	IRIS_LOGI_IF(rc != 0,
			"%s(), [optional] have not fps switch sequence", __func__);

	rc = _iris_parse_fps_clk_switch_seq(np);
	IRIS_LOGI_IF(rc != 0,
			"%s(), [optional] have not fps clk switch sequence", __func__);

	return 0;
}

static void _iris_send_res_switch_pkt(void)
{
	struct iris_ctrl_seq *pseq = &tm_switch_seq[TIMING_SWITCH_RES_SEQ];
	struct iris_ctrl_opt *arr = NULL;
	ktime_t ktime = 0;

	SDE_ATRACE_BEGIN(__func__);
	IRIS_LOGI_IF(LOG_DEBUG_INFO,
			"%s(), cmd list index: %02x", __func__, cmd_list_index);

	if (pseq == NULL) {
		IRIS_LOGE("%s(), seq is NULL", __func__);
		SDE_ATRACE_END(__func__);
		return;
	}

	arr = pseq->ctrl_opt;

	if (LOG_VERBOSE_INFO) {
		int32_t i = 0;

		for (i = 0; i < pseq->cnt; i++) {
			IRIS_LOGI("%s(), i_p: %02x, opt: %02x, chain: %02x", __func__,
					arr[i].ip, arr[i].opt_id, arr[i].chain);
		}
	}

	if (LOG_DEBUG_INFO)
		ktime = ktime_get();
	iris_send_assembled_pkt(arr, pseq->cnt);
	IRIS_LOGI_IF(LOG_DEBUG_INFO,
			"%s(), send sequence cost '%d us'", __func__,
			(u32)(ktime_to_us(ktime_get()) - ktime_to_us(ktime)));

	usleep_range(100, 101);
	SDE_ATRACE_END(__func__);
}

static bool _iris_need_fps_clk_seq(void)
{
	if (tm_switch_seq[TIMING_SWITCH_FPS_CLK_SEQ].ctrl_opt == NULL)
		return false;

	if (panel_tm_arry[new_tm_index].clk_rate_hz
			!= panel_tm_arry[last_pt_tm_index].clk_rate_hz) {
		IRIS_LOGI_IF(LOG_DEBUG_INFO,
				"%s(), switch with different clk, from %llu to %llu",
				__func__,
				panel_tm_arry[last_pt_tm_index].clk_rate_hz,
				panel_tm_arry[new_tm_index].clk_rate_hz);
		return true;
	}

	return false;
}

static void _iris_send_fps_switch_pkt(void)
{
	struct iris_ctrl_seq *pseq = &tm_switch_seq[TIMING_SWITCH_FPS_SEQ];
	struct iris_ctrl_opt *arr = NULL;
	ktime_t ktime = 0;

	IRIS_LOGI_IF(LOG_DEBUG_INFO,
			"%s(), cmd list index: %02x", __func__, cmd_list_index);

	if (pseq == NULL) {
		IRIS_LOGE("%s(), seq is NULL", __func__);
		return;
	}

	if (_iris_need_fps_clk_seq()) {
		pseq = &tm_switch_seq[TIMING_SWITCH_FPS_CLK_SEQ];
		IRIS_LOGI("with different clk");
	}

	arr = pseq->ctrl_opt;

	if (LOG_VERBOSE_INFO) {
		int32_t i = 0;

		for (i = 0; i < pseq->cnt; i++) {
			IRIS_LOGI("%s(), i_p: %02x, opt: %02x, chain: %02x", __func__,
					arr[i].ip, arr[i].opt_id, arr[i].chain);
		}
	}

	SDE_ATRACE_BEGIN(__func__);
	if (LOG_DEBUG_INFO)
		ktime = ktime_get();

	iris_send_assembled_pkt(arr, pseq->cnt);
	IRIS_LOGI_IF(LOG_DEBUG_INFO,
			"%s(), send dtsi seq cost '%d us'", __func__,
			(u32)(ktime_to_us(ktime_get()) - ktime_to_us(ktime)));

	SDE_ATRACE_END(__func__);
}

static uint32_t _iris_get_timing_index(const struct dsi_mode_info *timing)
{
	uint32_t i = 0;

	if (!_iris_support_timing_switch())
		return 0;

	for (i = 0; i < panel_tm_num; i++) {
		struct dsi_mode_info *t = &panel_tm_arry[i];

		if (timing->v_active == t->v_active &&
				timing->h_active == t->h_active &&
				timing->refresh_rate == t->refresh_rate)
			return i;
	}

	return 0;
}

static uint32_t _iris_generate_switch_case(struct dsi_panel *panel,
		const struct dsi_mode_info *new_timing)
{
	bool cur_pt_mode = false;
	u32 new_cmd_list_idx = 0;
	struct dsi_mode_info *last_pt_timing = &panel_tm_arry[last_pt_tm_index];

	if (!_iris_support_timing_switch())
		return SWITCH_ABYP_TO_ABYP;

	cur_pt_mode = iris_is_pt_mode(panel);
	new_tm_index = _iris_get_timing_index(new_timing);
	new_cmd_list_idx = tm_cmd_map_arry[new_tm_index];

	if (new_cmd_list_idx != IRIS_DTSI_NONE)
		cmd_list_index = new_cmd_list_idx;

	if (cur_pt_mode) {
		if (new_cmd_list_idx == IRIS_DTSI_NONE)
			return SWITCH_PT_TO_ABYP;
		if (!_iris_is_same_res(new_timing, last_pt_timing)) {
			IRIS_LOGI("%s(), RES switch in PT mode", __func__);
			return SWITCH_PT_TO_ABYP;
		}

		return SWITCH_PT_TO_PT;
	}

	return SWITCH_ABYP_TO_ABYP;
}

static bool _iris_is_same_res(const struct dsi_mode_info *new_timing,
		const struct dsi_mode_info *old_timing)
{
	IRIS_LOGI_IF(LOG_VERBOSE_INFO,
			"%s(), switch from %ux%u to %ux%u",
			__func__,
			old_timing->h_active, old_timing->v_active,
			new_timing->h_active, new_timing->v_active);

	if (old_timing->h_active == new_timing->h_active
			&& old_timing->v_active == new_timing->v_active)
		return true;

	return false;
}

static bool _iris_is_same_fps(const struct dsi_mode_info *new_timing,
		const struct dsi_mode_info *old_timing)
{
	IRIS_LOGI_IF(LOG_VERBOSE_INFO,
			"%s(), switch from %u to %u",
			__func__,
			old_timing->refresh_rate, new_timing->refresh_rate);

	if (new_timing->refresh_rate == old_timing->refresh_rate)
		return true;

	return false;
}

void iris_update_last_pt_timing(const struct dsi_mode_info *pt_timing)
{
	if (!_iris_support_timing_switch())
		return;

	last_pt_tm_index = _iris_get_timing_index(pt_timing);
}

void iris_update_panel_timing(const struct dsi_mode_info *panel_timing)
{
	u32 new_cmd_list_idx = 0;

	if (!_iris_support_timing_switch())
		return;

	new_tm_index = _iris_get_timing_index(panel_timing);
	new_cmd_list_idx = tm_cmd_map_arry[new_tm_index];

	if (new_cmd_list_idx != IRIS_DTSI_NONE)
		cmd_list_index = new_cmd_list_idx;

	cur_tm_index = new_tm_index;
}

bool iris_is_abyp_timing(const struct dsi_mode_info *new_timing)
{
	uint32_t tm_index = 0;

	if (!new_timing)
		return false;

	if (tm_cmd_map_arry == NULL)
		return false;

	tm_index = _iris_get_timing_index(new_timing);
	if (tm_cmd_map_arry[tm_index] == IRIS_DTSI_NONE)
		return true;

	return false;
}

static void _iris_abyp_ctrl_switch(void)
{
	struct iris_cfg *pcfg = iris_get_cfg();
	uint32_t *payload = NULL;
	uint8_t val = 0;

	/* Follow _iris_abyp_ctrl_init() */
	payload = iris_get_ipopt_payload_data(IRIS_IP_SYS,
			pcfg->id_sys_abyp_ctrl, 2);
	if (payload == NULL) {
		IRIS_LOGE("%s(), failed to find: %02x, %02x",
				__func__, IRIS_IP_SYS, pcfg->id_sys_abyp_ctrl);
		return;
	}

	if (pcfg->lp_ctrl.abyp_lp == 1)
		val = 2;
	else if (pcfg->lp_ctrl.abyp_lp == 2)
		val = 1;
	else
		val = 0;

	payload[0] = BITS_SET(payload[0], 2, 22, val);
}

static void _iris_pre_switch_proc(void)
{
	_iris_abyp_ctrl_switch();
	if (chip_type == CHIP_IRIS8)
		iris_pre_switch_proc_i8();
}

static void _iris_post_switch_proc(void)
{
	if (chip_type == CHIP_IRIS8)
		iris_post_switch_proc_i8();
}

static void _iris_switch_fps_impl(void)
{
	_iris_pre_switch_proc();
	_iris_send_fps_switch_pkt();

	switch (chip_type) {
	case CHIP_IRIS7P:
		iris_send_dynamic_seq_i7p();
		break;
	case CHIP_IRIS7:
		iris_send_dynamic_seq_i7();
		break;
	case CHIP_IRIS8:
		iris_send_dynamic_seq_i8();
		break;
	default:
		IRIS_LOGE("%s(), doesn't support for chip type: %#x",
				__func__, chip_type);
	}

	_iris_post_switch_proc();
}

uint32_t iris_get_cmd_list_index(void)
{
	return cmd_list_index;
}

bool iris_is_master_timing_supported(void)
{
	if (master_tm_cmd_map_arry != NULL)
		return true;

	return false;
}

uint8_t iris_get_master_timing_type(void)
{
	if (iris_is_master_timing_supported())
		return master_tm_cmd_map_arry[cur_tm_index];

	return IRIS_DTSI_NONE;
}

static uint32_t _iris_cmd_to_timing(uint32_t cmd_index)
{
	uint32_t i = 0;

	for (i = 0; i < panel_tm_num; i++) {
		if (tm_cmd_map_arry[i] == cmd_index)
			return i;
	}

	return cur_tm_index;
}

static bool _iris_skip_sync(uint32_t cmd_index)
{
	uint32_t cur_cmd_index = iris_get_cmd_list_index();
	struct dsi_mode_info *cur_tm = NULL;
	struct dsi_mode_info *tm = NULL;

	IRIS_LOGI_IF(LOG_VERY_VERBOSE_INFO,
			"%s(), cmd index: %u, current cmd index: %u", __func__,
			cmd_index, cur_cmd_index);

	if (cmd_index == cur_cmd_index)
		return true;

	tm = &panel_tm_arry[_iris_cmd_to_timing(cmd_index)];
	cur_tm = &panel_tm_arry[_iris_cmd_to_timing(cur_cmd_index)];

	IRIS_LOGI_IF(LOG_VERY_VERBOSE_INFO,
			"%s(), timing: %ux%u@%u, current timing: %ux%u@%u",
			__func__,
			tm->h_active, tm->v_active, tm->refresh_rate,
			cur_tm->h_active, cur_tm->v_active, cur_tm->refresh_rate);

	if (tm->v_active == cur_tm->v_active &&
			tm->h_active == cur_tm->h_active &&
			tm->refresh_rate != cur_tm->refresh_rate)
		return false;

	return true;
}

void iris_sync_payload(uint8_t ip, uint8_t opt_id, int32_t pos, uint32_t value)
{
	struct dsi_cmd_desc *pdesc = NULL;
	struct iris_cfg *pcfg = NULL;
	uint32_t *pvalue = NULL;
	uint32_t type = 0;

	if (!_iris_support_timing_switch())
		return;

	if (iris_is_master_timing_supported())
		return;

	if (ip >= LUT_IP_START && ip < LUT_IP_END)
		return;

	for (type = IRIS_DTSI_PIP_IDX_START; type < iris_get_cmd_list_cnt(); type++) {
		if (_iris_skip_sync(type))
			continue;

		pdesc = iris_get_specific_desc_from_ipopt(ip, opt_id, pos, type);
		if (!pdesc) {
			IRIS_LOGE("%s(), failed to find right desc.", __func__);
			return;
		}

		pcfg = iris_get_cfg();
		pvalue = (uint32_t *)((uint8_t *)pdesc->msg.tx_buf + (pos * 4) % pcfg->split_pkt_size);
		pvalue[0] = value;
	}
}

void iris_sync_bitmask(struct iris_update_regval *pregval)
{
	int32_t ip = 0;
	int32_t opt_id = 0;
	uint32_t orig_val = 0;
	uint32_t *data = NULL;
	uint32_t val = 0;
	struct iris_ip_opt *popt = NULL;
	uint32_t type = 0;

	if (!_iris_support_timing_switch())
		return;

	if (iris_is_master_timing_supported())
		return;

	if (!pregval) {
		IRIS_LOGE("%s(), invalid input", __func__);
		return;
	}

	ip = pregval->ip;
	opt_id = pregval->opt_id;
	if (ip >= LUT_IP_START && ip < LUT_IP_END)
		return;

	for (type = IRIS_DTSI_PIP_IDX_START; type < iris_get_cmd_list_cnt(); type++) {
		if (_iris_skip_sync(type))
			continue;

		popt = iris_find_specific_ip_opt(ip, opt_id, type);
		if (popt == NULL) {
			IRIS_LOGW("%s(), can't find i_p: 0x%02x opt: 0x%02x, from type: %u",
					__func__, ip, opt_id, type);
			continue;
		} else if (popt->cmd_cnt != 1) {
			IRIS_LOGW("%s(), invalid bitmask for i_p: 0x%02x, opt: 0x%02x, type: %u, popt len: %d",
					__func__, ip, opt_id, type, popt->cmd_cnt);
			continue;
		}

		data = (uint32_t *)popt->cmd[0].msg.tx_buf;
		orig_val = cpu_to_le32(data[2]);
		val = orig_val & (~pregval->mask);
		val |= (pregval->value & pregval->mask);
		data[2] = val;
	}
}

void iris_sync_current_ipopt(uint8_t ip, uint8_t opt_id)
{
	struct iris_ip_opt *popt = NULL;
	struct iris_ip_opt *spec_popt = NULL;
	uint32_t type = 0;
	int i = 0;
	ktime_t ktime = 0;

	if (LOG_DEBUG_INFO)
		ktime = ktime_get();

	if (!_iris_support_timing_switch())
		return;

	if (iris_is_master_timing_supported())
		return;

	if (ip >= LUT_IP_START && ip < LUT_IP_END)
		return;

	popt = iris_find_ip_opt(ip, opt_id);
	if (popt == NULL)
		return;

	for (type = IRIS_DTSI_PIP_IDX_START; type < iris_get_cmd_list_cnt(); type++) {
		if (_iris_skip_sync(type))
			continue;

		spec_popt = iris_find_specific_ip_opt(ip, opt_id, type);
		if (spec_popt == NULL)
			continue;

		for (i = 0; i < popt->cmd_cnt; i++) {
			memcpy((void *)spec_popt->cmd[i].msg.tx_buf,
					popt->cmd[i].msg.tx_buf,
					popt->cmd[i].msg.tx_len);
			if (LOG_VERBOSE_INFO)
				print_hex_dump(KERN_ERR, "", DUMP_PREFIX_NONE, 32, 4,
						popt->cmd[i].msg.tx_buf, popt->cmd[i].msg.tx_len, false);
		}
	}

	IRIS_LOGI_IF(LOG_DEBUG_INFO,
			"%s(), for i_p: %02x opt: 0x%02x, cost '%d us'",
			__func__, ip, opt_id,
			(u32)(ktime_to_us(ktime_get()) - ktime_to_us(ktime)));
}

void iris_force_sync_payload(uint8_t ip, uint8_t opt_id, int32_t pos, uint32_t value)
{
	struct dsi_cmd_desc *pdesc = NULL;
	struct iris_cfg *pcfg = NULL;
	uint32_t *pvalue = NULL;
	uint32_t type = 0;

	if (!_iris_support_timing_switch())
		return;

	if (ip >= LUT_IP_START && ip < LUT_IP_END)
		return;

	for (type = IRIS_DTSI_PIP_IDX_START; type < iris_get_cmd_list_cnt(); type++) {
		if (_iris_skip_sync(type))
			continue;

		pdesc = iris_get_specific_desc_from_ipopt(ip, opt_id, pos, type);
		if (!pdesc) {
			IRIS_LOGE("%s(), failed to find right desc.", __func__);
			return;
		}

		pcfg = iris_get_cfg();
		pvalue = (uint32_t *)((uint8_t *)pdesc->msg.tx_buf + (pos * 4) % pcfg->split_pkt_size);
		pvalue[0] = value;

	}
}

void iris_force_sync_bitmask(uint8_t ip, uint8_t opt_id, int32_t reg_pos,
		int32_t bits, int32_t offset, uint32_t value)
{
	struct dsi_cmd_desc *pdesc = NULL;
	struct iris_cfg *pcfg = NULL;
	uint32_t *pvalue = NULL;
	uint32_t type = 0;

	if (!_iris_support_timing_switch())
		return;

	if (ip >= LUT_IP_START && ip < LUT_IP_END)
		return;

	for (type = IRIS_DTSI_PIP_IDX_START; type < iris_get_cmd_list_cnt(); type++) {
		if (_iris_skip_sync(type))
			continue;

		pdesc = iris_get_specific_desc_from_ipopt(ip, opt_id, reg_pos, type);
		if (!pdesc) {
			IRIS_LOGE("%s(), failed to find right desc.", __func__);
			return;
		}

		pcfg = iris_get_cfg();
		pvalue = (uint32_t *)((uint8_t *)pdesc->msg.tx_buf + (reg_pos * 4) % pcfg->split_pkt_size);
		pvalue[0] = BITS_SET(pvalue[0], bits, offset, value);
	}
}

void iris_pre_switch(struct dsi_panel *panel,
		struct dsi_mode_info *new_timing)
{
	if (panel == NULL || new_timing == NULL)
		return;

	SDE_ATRACE_BEGIN(__func__);
	if (chip_type == CHIP_IRIS8 && iris_is_pt_mode(panel) &&
			_iris_support_timing_switch())
		iris_pre_switch_i8();

	switch_case = _iris_generate_switch_case(panel, new_timing);

	IRIS_LOGI("%s(), timing@%u, %ux%u@%uHz, cmd list: %u, case: %s",
			__func__,
			new_tm_index,
			new_timing->h_active,
			new_timing->v_active,
			new_timing->refresh_rate,
			cmd_list_index,
			switch_case_name[switch_case]);
	IRIS_LOGI_IF(iris_is_pt_mode(panel),
			"%s(), FRC: %s, DUAL: %s",
			__func__,
			iris_get_cfg()->frc_enabled ? "true" : "false",
			iris_get_cfg()->dual_enabled ? "true" : "false");

	cur_tm_index = new_tm_index;
	SDE_ATRACE_END(__func__);

	IRIS_LOGI_IF(LOG_VERBOSE_INFO,
			"%s(), exit.", __func__);
}

static void _iris_switch_proc(struct dsi_mode_info *new_timing)
{
	struct dsi_mode_info *last_pt_timing = &panel_tm_arry[last_pt_tm_index];

	if (new_timing == NULL || last_pt_timing == NULL)
		return;

	if (!_iris_is_same_res(new_timing, last_pt_timing)) {
		IRIS_LOGI("%s(), RES switch.", __func__);
		_iris_send_res_switch_pkt();
		return;
	}

	if (!_iris_is_same_fps(new_timing, last_pt_timing)) {
		IRIS_LOGI("%s(), FPS switch.", __func__);
		_iris_switch_fps_impl();
	}
}

int iris_switch(struct dsi_panel *panel,
		struct dsi_panel_cmd_set *switch_cmds,
		struct dsi_mode_info *new_timing)
{
	int rc = 0;
	int lightup_opt = iris_lightup_opt_get();
	u32 refresh_rate = new_timing->refresh_rate;
	ktime_t ktime = 0;

	IRIS_LOGI_IF(LOG_DEBUG_INFO,
			"%s(), new timing index: timing@%u", __func__, new_tm_index);

	if (LOG_NORMAL_INFO)
		ktime = ktime_get();

	SDE_ATRACE_BEGIN(__func__);
	iris_update_panel_ap_te(refresh_rate);

	if (lightup_opt & 0x8) {
		rc = iris_abyp_send_panel_cmd(panel, switch_cmds);
		IRIS_LOGI("%s(), force switch from ABYP to ABYP, total cost '%d us'",
				__func__,
				(u32)(ktime_to_us(ktime_get()) - ktime_to_us(ktime)));
		SDE_ATRACE_END(__func__);
		return rc;
	}
	oplus_panel_cmdq_pack_handle(panel, switch_cmds->type, true);
	switch (switch_case) {
	case SWITCH_ABYP_TO_ABYP:
		SDE_ATRACE_BEGIN("iris_abyp_send_panel_cmd");
		rc = iris_abyp_send_panel_cmd(panel, switch_cmds);
		SDE_ATRACE_END("iris_abyp_send_panel_cmd");
		break;
	case SWITCH_PT_TO_PT:
		{
		ktime_t ktime_0 = ktime_get();

		SDE_ATRACE_BEGIN("iris_pt_send_panel_cmd");
		rc = iris_pt_send_panel_cmd(panel, switch_cmds);
		SDE_ATRACE_END("iris_pt_send_panel_cmd");
		IRIS_LOGI("%s(), send panel cmd cost '%d us'", __func__,
				(u32)(ktime_to_us(ktime_get()) - ktime_to_us(ktime_0)));

		_iris_switch_proc(new_timing);
		if (panel->qsync_mode > 0)
			iris_qsync_set(true);
		last_pt_tm_index = new_tm_index;
		}
		break;
	case SWITCH_PT_TO_ABYP:
		iris_abyp_switch_proc(to_dsi_display(panel->host), ANALOG_BYPASS_MODE);
		rc = iris_abyp_send_panel_cmd(panel, switch_cmds);
		break;
	default:
		IRIS_LOGE("%s(), invalid case: %u", __func__, switch_case);
		break;
	}
	oplus_panel_cmdq_pack_handle(panel, switch_cmds->type, false);
	SDE_ATRACE_END(__func__);
	IRIS_LOGI("%s(), return %d, total cost '%d us'",
			__func__,
			rc, (u32)(ktime_to_us(ktime_get()) - ktime_to_us(ktime)));

	return rc;
}

uint32_t iris_get_cont_type_with_timing_switch(struct dsi_panel *panel)
{
	struct iris_cfg *pcfg = iris_get_cfg();
	uint32_t type = IRIS_CONT_SPLASH_NONE;
	uint32_t sw_case = SWITCH_NONE;

	if (pcfg->valid >= PARAM_PARSED)
		sw_case = _iris_generate_switch_case(panel, &panel->cur_mode->timing);

	IRIS_LOGI("%s(), switch case: %s, %ux%u@%u",
			__func__,
			switch_case_name[sw_case],
			panel->cur_mode->timing.h_active,
			panel->cur_mode->timing.v_active,
			panel->cur_mode->timing.refresh_rate);

	switch (sw_case) {
	case SWITCH_PT_TO_PT:
		type = IRIS_CONT_SPLASH_LK;
		break;
	case SWITCH_ABYP_TO_ABYP:
	case SWITCH_ABYP_TO_PT:
		type = IRIS_CONT_SPLASH_BYPASS_PRELOAD;
		break;
	case SWITCH_PT_TO_ABYP:
		// This case does not happen
	default:
		type = IRIS_CONT_SPLASH_NONE;
		break;
	}

	return type;
}

void iris_switch_from_abyp_to_pt(void)
{
	ktime_t ktime = 0;
	struct dsi_mode_info *cur_timing = NULL;

	if (!_iris_support_timing_switch())
		return;

	IRIS_LOGI("%s(), current timing@%u, last pt timing@%u",
			__func__, cur_tm_index, last_pt_tm_index);

	if (cur_tm_index == last_pt_tm_index)
		return;

	if (tm_cmd_map_arry[cur_tm_index] == IRIS_DTSI_NONE)
		return;

	if (LOG_NORMAL_INFO)
		ktime = ktime_get();

	cur_timing = &panel_tm_arry[cur_tm_index];

	SDE_ATRACE_BEGIN(__func__);
	iris_update_panel_ap_te(cur_timing->refresh_rate);
	_iris_switch_proc(cur_timing);
	last_pt_tm_index = cur_tm_index;
	SDE_ATRACE_END(__func__);

	IRIS_LOGI("%s(), total cost '%d us'",
			__func__,
			(u32)(ktime_to_us(ktime_get()) - ktime_to_us(ktime)));
}


void iris_dump_cmdlist(uint32_t val)
{
	int ip_type = 0;
	int ip_index = 0;
	int opt_index = 0;
	int desc_index = 0;
	struct iris_ip_index *pip_index = NULL;

	if (val == 0)
		return;

	IRIS_LOGW("%s() enter.", __func__);

	for (ip_type = 0; ip_type < iris_get_cmd_list_cnt(); ip_type++) {
		pip_index = iris_get_ip_idx(ip_type);
		if (pip_index == NULL)
			continue;
		pr_err("\n");
		if (ip_type == cmd_list_index)
			pr_err("*iris-cmd-list-%d*\n", ip_type);
		else
			pr_err("iris-cmd-list-%d\n", ip_type);

		for (ip_index = 0; ip_index < IRIS_IP_CNT; ip_index++) {
			if (pip_index[ip_index].opt_cnt == 0 || pip_index[ip_index].opt == NULL)
				continue;

			for (opt_index = 0; opt_index < pip_index[ip_index].opt_cnt; opt_index++) {
				if (pip_index[ip_index].opt[opt_index].cmd_cnt == 0 ||
						pip_index[ip_index].opt[opt_index].cmd == NULL)
					continue;

				pr_err("\n");
				pr_err("%02x %02x\n", ip_index, pip_index[ip_index].opt[opt_index].opt_id);
				for (desc_index = 0; desc_index < pip_index[ip_index].opt[opt_index].cmd_cnt; desc_index++) {
					if (pip_index[ip_index].opt[opt_index].cmd[desc_index].msg.tx_buf == NULL ||
							pip_index[ip_index].opt[opt_index].cmd[desc_index].msg.tx_len == 0)
						continue;

					print_hex_dump(KERN_ERR, "", DUMP_PREFIX_NONE, 32, 4,
							pip_index[ip_index].opt[opt_index].cmd[desc_index].msg.tx_buf,
							pip_index[ip_index].opt[opt_index].cmd[desc_index].msg.tx_len, false);
				}
			}
		}
	}

	IRIS_LOGW("%s() exit.", __func__);
}

void iris_set_tm_sw_loglevel(uint32_t level)
{
	if (level == LOG_DUMP_CMDLIST_LEVEL) {
		iris_dump_cmdlist(1);
		return;
	}

	log_level = level;
}

uint32_t iris_get_tm_sw_loglevel(void)
{
	return log_level;
}
