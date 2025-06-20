/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _DSI_PANEL_H_
#define _DSI_PANEL_H_

#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/backlight.h>
#include <drm/drm_panel.h>
#include <drm/msm_drm.h>
#include <drm/msm_drm_pp.h>
#if IS_ENABLED(CONFIG_OPLUS_POWER_NOTIFIER)
#include <misc/oplus_power_notifier.h>
#endif

#include "dsi_defs.h"
#include "dsi_ctrl_hw.h"
#include "dsi_clk.h"
#include "dsi_pwr.h"
#include "dsi_parser.h"
#include "msm_drv.h"
#ifdef OPLUS_FEATURE_DISPLAY
#include "../oplus/oplus_dsi_support.h"
#include <linux/soc/qcom/panel_event_notifier.h>

struct oplus_brightness_alpha {
	u32 brightness;
	u32 alpha;
};

struct oplus_clk_osc {
	u32 clk_rate;
	u32 osc_rate;
};

/***  pwm turbo initialize params dtsi config   *****************
oplus,pwm-turbo-support;
oplus,pwm-turbo-plus-dbv=<0x643>;   			// mtk config
oplus,pwm-turbo-wait-te=<1>;
oplus,pwm-switch-backlight-threshold=<0x643>;  // qcom config
********************************************************/
/* oplus pwm turbo initialize params ***************/
struct oplus_pwm_turbo_params {
	unsigned int   config;									/* bit(0):enable or disabled, bit(1) and other spare no used oplus,pwm-turbo-support */
	unsigned int   hpwm_mode;								/* hpwm enable or not */
	unsigned int   hpwm_bl;									/* mtk switch bl plus oplus,pwm-turbo-plus-dbv */
	bool pwm_turbo_support;									/* qcom platform, oplus,pwm-turbo-support */
	bool pwm_turbo_enabled;
	bool pwm_switch_support;
	bool pwm_power_on;
	bool pwm_hbm_state;
	bool oplus_pwm_switch_state;
	bool oplus_pwm_switch_state_changed;
	u32 pwm_bl_threshold;									/* qcom switch bl plus oplus,pwm-switch-backlight-threshold */
	bool pwm_onepulse_support;
	u32 pwm_onepulse_enabled;
	bool pwm_switch_restore_support;
	bool pwm_wait_te_tx;
	bool directional_onepulse_switch;
	bool pack_backlight;
	bool pwm_switch_support_dc;
	u32 oplus_dynamic_pulse;
	u32 oplus_last_dynamic_pulse;
	int oplus_pulse_mutual_fps_flag;
	bool oplus_aod_mutual_fps_flag;
	bool pwm_switch_support_extend_mode;
	ktime_t aod_off_timestamp;
	ktime_t into_aod_timestamp;
};

enum oplus_pwm_pulse {
	THREE_EIGHTEEN_PULSE = 0,
	ONE_EIGHTEEN_PULSE,
	ONE_ONE_PULSE,
};

/* In 120hz general solution, L1, L2, L3 means 1 Pulse 3 Pulse and 18 Pulse */
enum PWM_STATE {
	PWM_STATE_L1 = 0,
	PWM_STATE_L2,
	PWM_STATE_L3,
	PWM_STATE_MAXNUM,
};
#endif /* OPLUS_FEATURE_DISPLAY */

#define MAX_BL_LEVEL 4096
#define MAX_BL_SCALE_LEVEL 1024
#define MAX_SV_BL_SCALE_LEVEL 65535
#define SV_BL_SCALE_CAP (MAX_SV_BL_SCALE_LEVEL * 4)
#define DSI_CMD_PPS_SIZE 135

#define DSI_CMD_PPS_HDR_SIZE 7
#define DSI_MODE_MAX 32

#define INTO_OUT_AOD_INTERVOL (45*1000)
/*
 * Defining custom dsi msg flag.
 * Using upper byte of flag field for custom DSI flags.
 * Lower byte flags specified in drm_mipi_dsi.h.
 */
#define MIPI_DSI_MSG_ASYNC_OVERRIDE BIT(4)
#define MIPI_DSI_MSG_CMD_DMA_SCHED BIT(5)
#define MIPI_DSI_MSG_BATCH_COMMAND BIT(6)
#define MIPI_DSI_MSG_UNICAST_COMMAND BIT(7)

enum dsi_panel_rotation {
	DSI_PANEL_ROTATE_NONE = 0,
	DSI_PANEL_ROTATE_HV_FLIP,
	DSI_PANEL_ROTATE_H_FLIP,
	DSI_PANEL_ROTATE_V_FLIP
};

enum dsi_backlight_type {
	DSI_BACKLIGHT_PWM = 0,
	DSI_BACKLIGHT_WLED,
	DSI_BACKLIGHT_DCS,
	DSI_BACKLIGHT_EXTERNAL,
	DSI_BACKLIGHT_UNKNOWN,
	DSI_BACKLIGHT_MAX,
};

enum bl_update_flag {
	BL_UPDATE_DELAY_UNTIL_FIRST_FRAME,
	BL_UPDATE_NONE,
};

enum {
	MODE_GPIO_NOT_VALID = 0,
	MODE_SEL_DUAL_PORT,
	MODE_SEL_SINGLE_PORT,
	MODE_GPIO_HIGH,
	MODE_GPIO_LOW,
};

enum dsi_dms_mode {
	DSI_DMS_MODE_DISABLED = 0,
	DSI_DMS_MODE_RES_SWITCH_IMMEDIATE,
};

enum dsi_panel_physical_type {
	DSI_DISPLAY_PANEL_TYPE_LCD = 0,
	DSI_DISPLAY_PANEL_TYPE_OLED,
	DSI_DISPLAY_PANEL_TYPE_MAX,
};

struct dsi_dfps_capabilities {
	enum dsi_dfps_type type;
	u32 min_refresh_rate;
	u32 max_refresh_rate;
	u32 *dfps_list;
	u32 dfps_list_len;
	bool dfps_support;
};

struct dsi_qsync_capabilities {
	bool qsync_support;
	u32 qsync_min_fps;
	u32 *qsync_min_fps_list;
	int qsync_min_fps_list_len;
};

struct dsi_avr_capabilities {
	u32 avr_step_fps;
	u32 *avr_step_fps_list;
	u32 avr_step_fps_list_len;
};

struct dsi_dyn_clk_caps {
	bool dyn_clk_support;
	enum dsi_dyn_clk_feature_type type;
	bool maintain_const_fps;
};

struct dsi_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *active;
	struct pinctrl_state *suspend;
	struct pinctrl_state *pwm_pin;
#ifdef OPLUS_FEATURE_DISPLAY
	/* oplus panel pinctrl */
	struct pinctrl_state *oplus_panel_active;
	struct pinctrl_state *oplus_panel_suspend;
#endif /* OPLUS_FEATURE_DISPLAY */
};

struct dsi_panel_phy_props {
	u32 panel_width_mm;
	u32 panel_height_mm;
	enum dsi_panel_rotation rotation;
};

#ifdef OPLUS_FEATURE_DISPLAY
struct dsi_panel_oplus_privite {
	const char *vendor_name;
	const char *manufacture_name;
	bool skip_mipi_last_cmd;
	bool is_pxlw_iris5;
	int bl_remap_count;
	bool is_osc_support;
	u32 osc_clk_mode0_rate;
	u32 osc_clk_mode1_rate;
	bool cabc_enabled;
	bool dre_enabled;
	bool is_apollo_support;
	u32 sync_brightness_level;
	bool dp_support;
	struct oplus_brightness_alpha *bl_remap;
	bool dc_apollo_sync_enable;
	u32 dc_apollo_sync_brightness_level;
	u32 dc_apollo_sync_brightness_level_pcc;
	u32 dc_apollo_sync_brightness_level_pcc_min;
	int iris_pw_enable;
	int iris_pw_rst_gpio;
	int iris_pw_0p9_en_gpio;
	bool ffc_enabled;
	u32 ffc_delay_frames;
	u32 ffc_mode_count;
	u32 ffc_mode_index;
	struct oplus_clk_osc *clk_osc_seq;
	u32 clk_rate_cur;
	u32 osc_rate_cur;
	bool gpio_pre_on;
	bool pinctrl_enabled;
	/* add for panel id compatibility*/
	bool panel_init_compatibility_enable;
	u32 hbm_max_state;
	bool cmdq_pack_support;
	bool cmdq_pack_state;
	u32 pwm_sw_cmd_te_cnt;
	bool directional_onepulse_switch;
/********************************************
	fp_type usage:
	bit(0):lcd capacitive fingerprint(aod/fod are not supported)
	bit(1):oled capacitive fingerprint(only support aod)
	bit(2):optical fingerprint old solution(dim layer and pressed icon are controlled by kernel)
	bit(3):optical fingerprint new solution(dim layer and pressed icon are not controlled by kernel)
	bit(4):local hbm
	bit(5):pressed icon brightness adaptive
	bit(6):ultrasonic fingerprint
	bit(7):ultra low power aod
********************************************/
	u32 fp_type;
	bool enhance_mipi_strength;
	bool oplus_vreg_ctrl_flag;
	bool oplus_clk_vreg_ctrl_flag;
	u32 oplus_clk_vreg_ctrl_value;
	/* add for all wait te demand */
	u32 wait_te_config;
	bool need_sync;
	u32 disable_delay_bl_count;
	bool pwm_create_thread;
	bool oplus_bl_demura_dbv_support;
	int bl_demura_mode;
	bool vid_timming_switch_enabled;
	bool dimming_setting_before_bl_0_enable;
	bool vidmode_backlight_async_wait_enable;
	bool set_backlight_not_do_esd_reg_read_enable;
	bool gamma_compensation_support;
	/* indicates how many frames cost from aod off cmd sent to normal frame,
	"0" means once aod off cmd sent the next frame will be normal frame */
	unsigned int aod_off_frame_cost;
};

struct dsi_panel_oplus_serial_number {
	bool serial_number_support;
	bool is_reg_lock;
	bool is_switch_page;
	u32 serial_number_reg;
	int serial_number_index;
	int serial_number_conut;
};
#endif /* OPLUS_FEATURE_DISPLAY */

struct dsi_backlight_config {
	enum dsi_backlight_type type;
	enum bl_update_flag bl_update;

	u32 bl_min_level;
	u32 bl_max_level;
	u32 brightness_max_level;
#ifdef OPLUS_FEATURE_DISPLAY
	u32 bl_normal_max_level;
	u32 brightness_normal_max_level;
	u32 brightness_default_level;
	u32 dc_backlight_threshold;
	bool oplus_dc_mode;
	u32 global_hbm_case_id;
	u32 global_hbm_threshold;
	bool global_hbm_scale_mapping;
	bool oplus_limit_max_bl_mode;
	u32 oplus_limit_max_bl;
	u32 pwm_bl_onepulse_threshold;
	bool oplus_demura2_offset_support;
	bool need_to_set_demura2_offset;
	u32 demura2_offset;
#endif /* OPLUS_FEATURE_DISPLAY */

	/* current brightness value */
	u32 brightness;
	u32 bl_level;
#ifdef OPLUS_FEATURE_DISPLAY
	u32 oplus_raw_bl;
#endif /* OPLUS_FEATURE_DISPLAY */
	u32 bl_scale;
	u32 bl_scale_sv;
	bool bl_inverted_dbv;
	/* digital dimming backlight LUT */
	struct drm_msm_dimming_bl_lut *dimming_bl_lut;
	u32 dimming_min_bl;
	u32 dimming_status;
	bool user_disable_notification;

	int en_gpio;
	/* PWM params */
	struct pwm_device *pwm_bl;
	bool pwm_enabled;
	u32 pwm_period_usecs;

	/* WLED params */
	struct led_trigger *wled;
	struct backlight_device *raw_bd;

	/* DCS params */
	bool lp_mode;
};

#ifdef OPLUS_FEATURE_DISPLAY
enum global_hbm_case {
	GLOBAL_HBM_CASE_NONE,
	GLOBAL_HBM_CASE_1,
	GLOBAL_HBM_CASE_2,
	GLOBAL_HBM_CASE_3,
	GLOBAL_HBM_CASE_4,
	GLOBAL_HBM_CASE_MAX
};
#endif /* OPLUS_FEATURE_DISPLAY */

struct dsi_reset_seq {
	u32 level;
	u32 sleep_ms;
};

struct dsi_panel_reset_config {
	struct dsi_reset_seq *sequence;
	u32 count;

	int reset_gpio;
	int disp_en_gpio;
	int lcd_mode_sel_gpio;
	u32 mode_sel_state;
#ifdef OPLUS_FEATURE_DISPLAY
	int panel_vout_gpio;
	int panel_vddr_aod_en_gpio;
#endif /* OPLUS_FEATURE_DISPLAY */
};

enum esd_check_status_mode {
	ESD_MODE_REG_READ,
	ESD_MODE_SW_BTA,
	ESD_MODE_PANEL_TE,
#ifdef OPLUS_FEATURE_DISPLAY
	/* add for esd check MIPI ERR flag mode */
	ESD_MODE_PANEL_MIPI_ERR_FLAG,
#endif /* OPLUS_FEATURE_DISPLAY */
	ESD_MODE_SW_SIM_SUCCESS,
	ESD_MODE_SW_SIM_FAILURE,
#ifdef OPLUS_FEATURE_DISPLAY
	ESD_MODE_PANEL_ERROR_FLAG,
#endif /* OPLUS_FEATURE_DISPLAY */
	ESD_MODE_MAX
};

struct drm_panel_esd_config {
	bool esd_enabled;

	enum esd_check_status_mode status_mode;
	struct dsi_panel_cmd_set status_cmd;
	u32 *status_cmds_rlen;
	u32 *status_valid_params;
	u32 *status_value;
	u8 *return_buf;
	u8 *status_buf;
	u32 groups;
#ifdef OPLUS_FEATURE_DISPLAY
	u32 status_match_modes;
	bool esd_debug_enabled;
	int esd_error_flag_gpio;
	int esd_error_flag_gpio_slave;
	/* add for esd check MIPI ERR flag mode, add gpio as irq*/
	int mipi_err_flag_gpio;
#endif /* OPLUS_FEATURE_DISPLAY */
};

struct dsi_panel_spr_info {
	bool enable;
	enum msm_display_spr_pack_type pack_type;
};

struct dsi_panel;

struct dsi_panel_ops {
	int (*pinctrl_init)(struct dsi_panel *panel);
	int (*gpio_request)(struct dsi_panel *panel);
	int (*pinctrl_deinit)(struct dsi_panel *panel);
	int (*gpio_release)(struct dsi_panel *panel);
	int (*bl_register)(struct dsi_panel *panel);
	int (*bl_unregister)(struct dsi_panel *panel);
	int (*parse_gpios)(struct dsi_panel *panel);
	int (*parse_power_cfg)(struct dsi_panel *panel);
	int (*trigger_esd_attack)(struct dsi_panel *panel);
};

struct dsi_panel_calib_data {
	char *data;
	size_t len;
};

struct dsi_panel {
	const char *name;
	const char *type;
	struct device_node *panel_of_node;
	struct mipi_dsi_device mipi_device;
	bool panel_ack_disabled;

	struct mutex panel_lock;
	struct drm_panel drm_panel;
	struct mipi_dsi_host *host;
	struct device *parent;

	struct dsi_host_common_cfg host_config;
	struct dsi_video_engine_cfg video_config;
	struct dsi_cmd_engine_cfg cmd_config;
	enum dsi_op_mode panel_mode;
	bool panel_mode_switch_enabled;
	bool poms_align_vsync;

	struct dsi_dfps_capabilities dfps_caps;
	struct dsi_dyn_clk_caps dyn_clk_caps;
	struct dsi_panel_phy_props phy_props;
	bool dsc_switch_supported;

	struct dsi_display_mode *cur_mode;
	u32 num_timing_nodes;
	u32 num_display_modes;

	struct dsi_regulator_info power_info;
	struct dsi_backlight_config bl_config;
	struct dsi_panel_reset_config reset_config;
	struct dsi_pinctrl_info pinctrl;
	struct drm_panel_hdr_properties hdr_props;
	struct drm_panel_esd_config esd_config;

	struct dsi_parser_utils utils;

	bool lp11_init;
	bool ulps_feature_enabled;
	bool ulps_suspend_enabled;
	bool allow_phy_power_off;
	bool reset_gpio_always_on;
	bool calibration_enabled;
	atomic_t esd_recovery_pending;

	bool panel_initialized;
	bool te_using_watchdog_timer;
	struct dsi_qsync_capabilities qsync_caps;
	struct dsi_avr_capabilities avr_caps;

	char dce_pps_cmd[DSI_CMD_PPS_SIZE];
	enum dsi_dms_mode dms_mode;

	struct dsi_panel_spr_info spr_info;

	bool sync_broadcast_en;
	u32 dsc_count;
	u32 lm_count;

	int panel_test_gpio;
	int power_mode;
	enum dsi_panel_physical_type panel_type;

	struct dsi_panel_ops panel_ops;
	struct dsi_panel_calib_data calib_data;
#ifdef OPLUS_FEATURE_DISPLAY
	bool need_power_on_backlight;
	struct oplus_brightness_alpha *dc_ba_seq;
	int dc_ba_count;
	struct dsi_panel_oplus_privite oplus_priv;
	struct dsi_panel_oplus_serial_number oplus_ser;
	/* add for oplus pwm turbo */
	struct oplus_pwm_turbo_params pwm_params;
	int panel_id2;
	atomic_t esd_pending;
	atomic_t vidmode_backlight_async_wait;
	struct mutex panel_tx_lock;
	struct mutex oplus_ffc_lock;
	ktime_t te_timestamp;
	struct workqueue_struct *oplus_pwm_disable_duty_set_wq;
	struct work_struct oplus_pwm_disable_duty_set_work;
	struct workqueue_struct *oplus_pwm_switch_send_next_cmdq_wq;
	struct work_struct oplus_pwm_switch_send_next_cmdq_work;
	ktime_t ts_timestamp;
	u32 last_us_per_frame;
	u32 last_vsync_width;
	u32 last_refresh_rate;
	u32 work_frame;
	bool bl_ic_ktz8866_used;
#endif /* OPLUS_FEATURE_DISPLAY */

#if defined(CONFIG_PXLW_IRIS)
	bool is_secondary;
	int hbm_mode;
	u32 qsync_mode;
#endif

#if IS_ENABLED(CONFIG_OPLUS_POWER_NOTIFIER)
	struct notifier_block oplus_power_notify_client;
	int pon_status;
#endif
};

static inline bool dsi_panel_ulps_feature_enabled(struct dsi_panel *panel)
{
	return panel->ulps_feature_enabled;
}

static inline bool dsi_panel_initialized(struct dsi_panel *panel)
{
	return panel->panel_initialized;
}

static inline void dsi_panel_acquire_panel_lock(struct dsi_panel *panel)
{
	mutex_lock(&panel->panel_lock);
}

static inline void dsi_panel_release_panel_lock(struct dsi_panel *panel)
{
	mutex_unlock(&panel->panel_lock);
}

static inline bool dsi_panel_is_type_oled(struct dsi_panel *panel)
{
	return (panel->panel_type == DSI_DISPLAY_PANEL_TYPE_OLED);
}

struct dsi_panel *dsi_panel_get(struct device *parent,
				struct device_node *of_node,
				struct device_node *parser_node,
				const char *type,
				int topology_override,
				bool trusted_vm_env);

void dsi_panel_put(struct dsi_panel *panel);

int dsi_panel_drv_init(struct dsi_panel *panel, struct mipi_dsi_host *host);

int dsi_panel_drv_deinit(struct dsi_panel *panel);

int dsi_panel_get_mode_count(struct dsi_panel *panel);

void dsi_panel_put_mode(struct dsi_display_mode *mode);

int dsi_panel_get_mode(struct dsi_panel *panel,
		       u32 index,
		       struct dsi_display_mode *mode,
		       int topology_override);

int dsi_panel_validate_mode(struct dsi_panel *panel,
			    struct dsi_display_mode *mode);

int dsi_panel_get_host_cfg_for_mode(struct dsi_panel *panel,
				    struct dsi_display_mode *mode,
				    struct dsi_host_config *config);

int dsi_panel_get_phy_props(struct dsi_panel *panel,
			    struct dsi_panel_phy_props *phy_props);
int dsi_panel_get_dfps_caps(struct dsi_panel *panel,
			    struct dsi_dfps_capabilities *dfps_caps);

int dsi_panel_pre_prepare(struct dsi_panel *panel);

int dsi_panel_set_lp1(struct dsi_panel *panel);

int dsi_panel_set_lp2(struct dsi_panel *panel);

int dsi_panel_set_nolp(struct dsi_panel *panel);

int dsi_panel_prepare(struct dsi_panel *panel);

int dsi_panel_enable(struct dsi_panel *panel);

int dsi_panel_post_enable(struct dsi_panel *panel);

int dsi_panel_pre_disable(struct dsi_panel *panel);

int dsi_panel_disable(struct dsi_panel *panel);

int dsi_panel_unprepare(struct dsi_panel *panel);

int dsi_panel_post_unprepare(struct dsi_panel *panel);

int dsi_panel_set_backlight(struct dsi_panel *panel, u32 bl_lvl);

int dsi_panel_update_pps(struct dsi_panel *panel);

int dsi_panel_send_qsync_on_dcs(struct dsi_panel *panel,
		int ctrl_idx);
int dsi_panel_send_qsync_off_dcs(struct dsi_panel *panel,
		int ctrl_idx);

int dsi_panel_send_roi_dcs(struct dsi_panel *panel, int ctrl_idx,
		struct dsi_rect *roi);

int dsi_panel_switch_video_mode_out(struct dsi_panel *panel);

int dsi_panel_switch_cmd_mode_out(struct dsi_panel *panel);

int dsi_panel_switch_cmd_mode_in(struct dsi_panel *panel);

int dsi_panel_switch_video_mode_in(struct dsi_panel *panel);

int dsi_panel_switch(struct dsi_panel *panel);

int dsi_panel_post_switch(struct dsi_panel *panel);

void dsi_dsc_pclk_param_calc(struct msm_display_dsc_info *dsc, int intf_width);

void dsi_panel_bl_handoff(struct dsi_panel *panel);

struct dsi_panel *dsi_panel_ext_bridge_get(struct device *parent,
				struct device_node *of_node,
				int topology_override);

int dsi_panel_parse_esd_reg_read_configs(struct dsi_panel *panel);

void dsi_panel_ext_bridge_put(struct dsi_panel *panel);

int dsi_panel_get_io_resources(struct dsi_panel *panel,
		struct msm_io_res *io_res);

void dsi_panel_calc_dsi_transfer_time(struct dsi_host_common_cfg *config,
		struct dsi_display_mode *mode, u32 frame_threshold_us);

int dsi_panel_get_cmd_pkt_count(const char *data, u32 length, u32 *cnt);

int dsi_panel_alloc_cmd_packets(struct dsi_panel_cmd_set *cmd,
		u32 packet_count);

int dsi_panel_create_cmd_packets(const char *data, u32 length, u32 count,
					struct dsi_cmd_desc *cmd);

void dsi_panel_destroy_cmd_packets(struct dsi_panel_cmd_set *set);

void dsi_panel_dealloc_cmd_packets(struct dsi_panel_cmd_set *set);

#ifdef OPLUS_FEATURE_DISPLAY
int dsi_panel_tx_cmd_set(struct dsi_panel *panel,
		enum dsi_cmd_set_type type);
#endif /* OPLUS_FEATURE_DISPLAY */
#endif /* _DSI_PANEL_H_ */
