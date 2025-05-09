/***************************************************************
** Copyright (C), 2022, OPLUS Mobile Comm Corp., Ltd
** File : oplus_display_panel_common.c
** Description : oplus display panel common feature
** Version : 1.0
** Date : 2020/06/13
** Author : Display
******************************************************************/
#include "oplus_display_panel_common.h"
#include "oplus_display_panel.h"
#include "oplus_display_panel_seed.h"
#include <linux/notifier.h>
#include <linux/msm_drm_notify.h>
#include <linux/soc/qcom/panel_event_notifier.h>
#include "oplus_display_private_api.h"
#include "oplus_display_interface.h"
#include "sde_trace.h"
#include "oplus_display_high_frequency_pwm.h"

#if defined(CONFIG_PXLW_IRIS)
#include "dsi_iris_loop_back.h"
#include "dsi_iris_api.h"
#endif

#define DSI_PANEL_OPLUS_DUMMY_VENDOR_NAME  "PanelVendorDummy"
#define DSI_PANEL_OPLUS_DUMMY_MANUFACTURE_NAME  "dummy1024"

bool oplus_temp_compensation_wait_for_vsync_set = false;
int oplus_debug_max_brightness = 0;
int oplus_dither_enable = 0;
int oplus_dre_status = 0;
int oplus_cabc_status = OPLUS_DISPLAY_CABC_UI;
extern int lcd_closebl_flag;
extern int shutdown_flag;
extern int oplus_display_audio_ready;
char oplus_rx_reg[PANEL_TX_MAX_BUF] = {0x0};
char oplus_rx_len = 0;
extern int spr_mode;
extern int dynamic_osc_clock;
extern int oplus_hw_partial_round;
int mca_mode = 1;
int dcc_flags = 0;
bool apollo_backlight_enable = false;
extern int dither_enable;
u32 g_oplus_clk_vreg_ctrl_value = 0;
EXPORT_SYMBOL(g_oplus_clk_vreg_ctrl_value);
bool g_oplus_clk_vreg_ctrl_config = false;
EXPORT_SYMBOL(g_oplus_clk_vreg_ctrl_config);
bool g_oplus_vreg_ctrl_config = false;
EXPORT_SYMBOL(g_oplus_vreg_ctrl_config);
bool oplus_enhance_mipi_strength = false;
EXPORT_SYMBOL(oplus_enhance_mipi_strength);
EXPORT_SYMBOL(oplus_debug_max_brightness);
EXPORT_SYMBOL(oplus_dither_enable);
EXPORT_SYMBOL(dcc_flags);

extern int dsi_display_read_panel_reg(struct dsi_display *display, u8 cmd,
		void *data, size_t len);
extern int __oplus_display_set_spr(int mode);
extern int dsi_display_spr_mode(struct dsi_display *display, int mode);
extern int dsi_panel_spr_mode(struct dsi_panel *panel, int mode);
extern int __oplus_display_set_dither(int mode);
extern unsigned int is_project(int project);

enum {
	REG_WRITE = 0,
	REG_READ,
	REG_X,
};

struct LCM_setting_table {
	unsigned int count;
	u8 *para_list;
};

int oplus_display_panel_get_id(void *buf)
{
	struct dsi_display *display = get_main_display();
	int ret = 0;
	unsigned char read[30];
	struct panel_id *panel_rid = buf;
	int panel_id = panel_rid->DA;

	if (panel_id == 1)
		display = get_sec_display();

	if (!display || !display->panel) {
		LCD_ERR("display is null\n");
		ret = -1;
		return ret;
	}
	/* if (__oplus_get_power_status() == OPLUS_DISPLAY_POWER_ON) { */
	if (display->panel->power_mode == SDE_MODE_DPMS_ON) {
		if (!strcmp(display->panel->oplus_priv.vendor_name, "A0005")) {
			mutex_lock(&display->display_lock);
			mutex_lock(&display->panel->panel_lock);
			ret = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_PANEL_INFO_SWITCH_PAGE);
			mutex_unlock(&display->panel->panel_lock);
			mutex_unlock(&display->display_lock);
			if (ret < 0) {
				DSI_ERR("Read AA545/AC090 P 3 A0005 panel id switch page failed!\n");
			}
		}
		mutex_lock(&display->display_lock);
		ret = dsi_display_read_panel_reg(display, 0xDA, read, 1);
		mutex_unlock(&display->display_lock);

		if (ret < 0) {
			LCD_ERR("failed to read DA ret=%d\n", ret);
			return -EINVAL;
		}

		panel_rid->DA = (uint32_t)read[0];
		mutex_lock(&display->display_lock);
		ret = dsi_display_read_panel_reg(display, 0xDB, read, 1);
		mutex_unlock(&display->display_lock);

		if (ret < 0) {
			LCD_ERR("failed to read DB ret=%d\n", ret);
			return -EINVAL;
		}

		panel_rid->DB = (uint32_t)read[0];
		mutex_lock(&display->display_lock);
		ret = dsi_display_read_panel_reg(display, 0xDC, read, 1);
		mutex_unlock(&display->display_lock);

		if (ret < 0) {
			LCD_ERR("failed to read DC ret=%d\n", ret);
			return -EINVAL;
		}

		panel_rid->DC = (uint32_t)read[0];
	} else {
		LCD_WARN("display panel status is not on\n");
		return -EINVAL;
	}

	return ret;
}

int oplus_display_panel_get_oplus_max_brightness(void *buf)
{
	uint32_t *max_brightness = buf;
	int panel_id = (*max_brightness >> 12);
	struct dsi_display *display = get_main_display();
	if (panel_id == 1)
		display = get_sec_display();

	(*max_brightness) = display->panel->bl_config.bl_normal_max_level;

	return 0;
}

int oplus_display_panel_get_max_brightness(void *buf)
{
	uint32_t *max_brightness = buf;
	int panel_id = (*max_brightness >> 12);
	struct dsi_display *display = get_main_display();
	if (panel_id == 1)
		display = get_sec_display();

	if (oplus_debug_max_brightness == 0) {
		(*max_brightness) = display->panel->bl_config.bl_normal_max_level;
	} else {
		(*max_brightness) = oplus_debug_max_brightness;
	}

	return 0;
}

int oplus_display_panel_set_max_brightness(void *buf)
{
	uint32_t *max_brightness = buf;

	oplus_debug_max_brightness = (*max_brightness);

	return 0;
}

int oplus_display_panel_get_lcd_max_brightness(void *buf)
{
	uint32_t *lcd_max_backlight = buf;
	int panel_id = (*lcd_max_backlight >> 12);
	struct dsi_display *display = get_main_display();
	if (panel_id == 1)
		display = get_sec_display();

	(*lcd_max_backlight) = display->panel->bl_config.bl_max_level;

	LCD_INFO("[%s] get lcd max backlight: %d\n",
			display->panel->oplus_priv.vendor_name,
			*lcd_max_backlight);

	return 0;
}

int oplus_display_panel_get_brightness(void *buf)
{
	uint32_t *brightness = buf;
	int panel_id = (*brightness >> 12);
	struct dsi_display *display = get_main_display();
	if (panel_id == 1)
		display = get_sec_display();

	(*brightness) = display->panel->bl_config.bl_level;

	return 0;
}

int oplus_display_panel_set_brightness(void *buf)
{
	int rc = 0;
	struct dsi_display *display = oplus_display_get_current_display();
	struct dsi_panel *panel = NULL;
	uint32_t *backlight = buf;

	if (!display || !display->drm_conn || !display->panel) {
		LCD_ERR("Invalid display params\n");
		return -EINVAL;
	}
	panel = display->panel;

	if (*backlight > panel->bl_config.bl_max_level ||
			*backlight < 0) {
		LCD_WARN("falied to set backlight: %d, it is out of range\n",
				*backlight);
		return -EFAULT;
	}

	LCD_INFO("[%s] set backlight: %d\n", panel->oplus_priv.vendor_name, *backlight);

	rc = dsi_display_set_backlight(display->drm_conn, display, *backlight);

	return rc;
}

int oplus_display_panel_get_vendor(void *buf)
{
	struct panel_info *p_info = buf;
	struct dsi_display *display = NULL;
	char *vendor = NULL;
	char *manu_name = NULL;
	int panel_id = p_info->version[0];

	display = get_main_display();
	if (1 == panel_id)
		display = get_sec_display();

	if (!display || !display->panel ||
			!display->panel->oplus_priv.vendor_name ||
			!display->panel->oplus_priv.manufacture_name) {
		LCD_ERR("failed to config lcd proc device\n");
		return -EINVAL;
	}

#if defined(CONFIG_PXLW_IRIS)
	if (iris_is_chip_supported() && (!strcmp(display->panel->type, "secondary"))) {
		LCD_INFO("iris secondary panel no need config\n");
		return -EINVAL;
	}
#endif

	vendor = (char *)display->panel->oplus_priv.vendor_name;
	manu_name = (char *)display->panel->oplus_priv.manufacture_name;

	strncpy(p_info->version, vendor, sizeof(p_info->version));
	strncpy(p_info->manufacture, manu_name, sizeof(p_info->manufacture));

	return 0;
}

int oplus_display_panel_get_panel_name(void *buf)
{
	struct panel_name *p_name = buf;
	struct dsi_display *display = NULL;
	char *name = NULL;
	int panel_id = p_name->name[0];

	display = get_main_display();
	if (1 == panel_id) {
		display = get_sec_display();
	}

	if (!display || !display->panel || !display->panel->name) {
		LCD_ERR("failed to config lcd panel name\n");
		return -EINVAL;
	}

#if defined(CONFIG_PXLW_IRIS)
	if (iris_is_chip_supported() && (!strcmp(display->panel->type, "secondary"))) {
		LCD_INFO("iris secondary panel no need config\n");
		return -EINVAL;
	}
#endif

	name = (char *)display->panel->name;

	memcpy(p_name->name, name,
			strlen(name) >= 49 ? 49 : (strlen(name) + 1));

	return 0;
}

int oplus_display_panel_get_panel_bpp(void *buf)
{
	uint32_t *panel_bpp = buf;
	int bpp = 0;
	int rc = 0;
	int panel_id = (*panel_bpp >> 12);
	struct dsi_display *display = get_main_display();
	struct dsi_parser_utils *utils = NULL;

	if (panel_id == 1) {
		display = get_sec_display();
	}

	if (!display || !display->panel) {
		LCD_ERR("display or panel is null\n");
		return -EINVAL;
	}

#if defined(CONFIG_PXLW_IRIS)
	if (iris_is_chip_supported() && (!strcmp(display->panel->type, "secondary"))) {
		LCD_INFO("iris secondary panel no need config\n");
		return -EINVAL;
	}
#endif


	utils = &display->panel->utils;
	if (!utils) {
		LCD_ERR("utils is null\n");
		return -EINVAL;
	}

	rc = utils->read_u32(utils->data, "qcom,mdss-dsi-bpp", &bpp);

	if (rc) {
		LCD_INFO("failed to read qcom,mdss-dsi-bpp, rc=%d\n", rc);
		return -EINVAL;
	}

	*panel_bpp = bpp / 3;

	return 0;
}

int oplus_display_panel_get_ccd_check(void *buf)
{
	struct dsi_display *display = get_main_display();
	struct mipi_dsi_device *mipi_device;
	int rc = 0;
	unsigned int *ccd_check = buf;
	char value3[] = { 0x5A, 0x5A };
	char value4[] = { 0x02 };
	char value5[] = { 0x44, 0x50 };
	char value6[] = { 0x05 };
	char value7[] = { 0xA5, 0xA5 };
	unsigned char read1[10];

	if (!display || !display->panel) {
		LCD_ERR("display is null\n");
		return -EINVAL;
	}

	/* if (__oplus_get_power_status() != OPLUS_DISPLAY_POWER_ON) { */
	if (display->panel->power_mode != SDE_MODE_DPMS_ON) {
		LCD_WARN("display panel in off status\n");
		return -EFAULT;
	}

	if (display->panel->panel_mode != DSI_OP_CMD_MODE) {
		LCD_ERR("only supported for command mode\n");
		return -EFAULT;
	}

	mipi_device = &display->panel->mipi_device;

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	if (!dsi_panel_initialized(display->panel)) {
		rc = -EINVAL;
		goto unlock;
	}

	rc = dsi_display_cmd_engine_enable(display);

	if (rc) {
		LCD_ERR("cmd engine enable failed\n");
		goto unlock;
	}

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_ON);
	}

	rc = mipi_dsi_dcs_write(mipi_device, 0xF0, value3, sizeof(value3));
	rc = mipi_dsi_dcs_write(mipi_device, 0xB0, value4, sizeof(value4));
	rc = mipi_dsi_dcs_write(mipi_device, 0xCC, value5, sizeof(value5));
	usleep_range(1000, 1100);
	rc = mipi_dsi_dcs_write(mipi_device, 0xB0, value6, sizeof(value6));

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);
	}

	dsi_display_cmd_engine_disable(display);

	mutex_unlock(&display->panel->panel_lock);
	rc = dsi_display_read_panel_reg(display, 0xCC, read1, 1);
	LCD_ERR("read ccd_check value = 0x%x rc=%d\n", read1[0], rc);
	(*ccd_check) = read1[0];
	mutex_lock(&display->panel->panel_lock);

	if (!dsi_panel_initialized(display->panel)) {
		rc = -EINVAL;
		goto unlock;
	}

	rc = dsi_display_cmd_engine_enable(display);

	if (rc) {
		LCD_ERR("cmd engine enable failed\n");
		goto unlock;
	}

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_ON);
	}

	rc = mipi_dsi_dcs_write(mipi_device, 0xF0, value7, sizeof(value7));

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);
	}

	dsi_display_cmd_engine_disable(display);
unlock:

	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);
	LCD_ERR("[%s] ccd_check = %d\n",  display->panel->oplus_priv.vendor_name,
			(*ccd_check));
	return 0;
}

int oplus_display_panel_get_serial_number(void *buf)
{
	int ret = 0, i;
	unsigned char read[30] = {0};
	PANEL_SERIAL_INFO panel_serial_info;
	uint64_t serial_number = 0;
	struct panel_serial_number *panel_rnum = buf;
	struct dsi_display *display = get_main_display();
	struct dsi_display_ctrl *m_ctrl = NULL;
	int panel_id = panel_rnum->serial_number[0];

	if (!display || !display->panel) {
		LCD_ERR("display is null\n");
		return -1;
	}

	if (0 == panel_id && display->enabled == false) {
		LCD_INFO("panel is disabled\n");
		return -1;
	}

	if (1 == panel_id) {
		display = get_sec_display();
		if (!display) {
			LCD_ERR("display is null\n");
			return -1;
		}
		if (display->enabled == false) {
			LCD_INFO("second panel is disabled\n");
			return -1;
		}
	}

	/* if (__oplus_get_power_status() != OPLUS_DISPLAY_POWER_ON) { */
	if (display->panel->power_mode != SDE_MODE_DPMS_ON) {
		LCD_INFO("display panel in off status\n");
		return ret;
	}

	if (!display->panel->panel_initialized) {
		LCD_ERR("panel initialized = false\n");
		return ret;
	}

	if (!display->panel->oplus_ser.serial_number_support) {
		ret = scnprintf(panel_rnum->serial_number,
				PANEL_IOCTL_BUF_MAX,
				"Get panel serial number: %llx",
				serial_number);
		LCD_INFO("display panel serial number not support, ret=%d\n", ret);
		return ret;
	}

	m_ctrl = &display->ctrl[display->cmd_master_idx];

	/*
	 * for some unknown reason, the panel_serial_info may read dummy,
	 * retry when found panel_serial_info is abnormal.
	 */
	for (i = 0; i < 5; i++) {
		if (display->panel->power_mode != SDE_MODE_DPMS_ON) {
			LCD_ERR("display panel in off status\n");
			return ret;
		}
		if (!display->panel->panel_initialized) {
			LCD_ERR("panel initialized = false\n");
			return ret;
		}

		if (display->panel->oplus_ser.is_switch_page) {
			mutex_lock(&display->display_lock);
			mutex_lock(&display->panel->panel_lock);
			ret = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_PANEL_DATE_SWITCH);
			mutex_unlock(&display->panel->panel_lock);
			mutex_unlock(&display->display_lock);
			if (ret) {
				LCD_ERR("get serial number switch page failed\n");
				return -1;
			}
		} else if (!display->panel->oplus_ser.is_reg_lock) {
			/* unknow what this case want to do */
		} else {
			mutex_lock(&display->display_lock);
			mutex_lock(&display->panel->panel_lock);
			if (display->config.panel_mode == DSI_OP_CMD_MODE) {
				dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_ON);
			}
			 {
				char value[] = {0x5A, 0x5A};
				ret = mipi_dsi_dcs_write(&display->panel->mipi_device, 0xF0, value, sizeof(value));
			 }
			if (display->config.panel_mode == DSI_OP_CMD_MODE) {
				dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_OFF);
			}
			mutex_unlock(&display->panel->panel_lock);
			mutex_unlock(&display->display_lock);
			if (ret < 0) {
				ret = scnprintf(buf,
						PANEL_IOCTL_BUF_MAX,
						"Get panel serial number failed, reason:%d",
						ret);
				msleep(20);
				continue;
			}
		}
		mutex_lock(&display->display_lock);
		ret = dsi_display_read_panel_reg(display, display->panel->oplus_ser.serial_number_reg,
				read, display->panel->oplus_ser.serial_number_conut);
		mutex_unlock(&display->display_lock);

		/*  0xA1               11th        12th    13th    14th    15th
		 *  HEX                0x32        0x0C    0x0B    0x29    0x37
		 *  Bit           [D7:D4][D3:D0] [D5:D0] [D5:D0] [D5:D0] [D5:D0]
		 *  exp              3      2       C       B       29      37
		 *  Yyyy,mm,dd      2014   2m      12d     11h     41min   55sec
		*/
		panel_serial_info.reg_index = display->panel->oplus_ser.serial_number_index;

		panel_serial_info.year = (read[panel_serial_info.reg_index] & 0xF0) >> 0x4;
		if (!strcmp(display->panel->name, "AC172 P 7 A0001 dsc cmd mode panel")) {
			panel_serial_info.year += 10;
		}

		panel_serial_info.month		= read[panel_serial_info.reg_index]	& 0x0F;
		panel_serial_info.day		= read[panel_serial_info.reg_index + 1]	& 0x1F;
		panel_serial_info.hour		= read[panel_serial_info.reg_index + 2]	& 0x1F;
		panel_serial_info.minute	= read[panel_serial_info.reg_index + 3]	& 0x3F;
		panel_serial_info.second	= read[panel_serial_info.reg_index + 4]	& 0x3F;
		panel_serial_info.reserved[0] = read[panel_serial_info.reg_index + 5];
		panel_serial_info.reserved[1] = read[panel_serial_info.reg_index + 6];

		serial_number = (panel_serial_info.year		<< 56)\
				+ (panel_serial_info.month		<< 48)\
				+ (panel_serial_info.day		<< 40)\
				+ (panel_serial_info.hour		<< 32)\
				+ (panel_serial_info.minute	<< 24)\
				+ (panel_serial_info.second	<< 16)\
				+ (panel_serial_info.reserved[0] << 8)\
				+ (panel_serial_info.reserved[1]);

		if (!panel_serial_info.year) {
			/*
			 * the panel we use always large than 2011, so
			 * force retry when year is 2011
			 */
			msleep(20);
			continue;
		}

		if (display->panel->oplus_ser.is_switch_page) {
			/* switch default page */
			mutex_lock(&display->display_lock);
			mutex_lock(&display->panel->panel_lock);
			ret = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_DEFAULT_SWITCH_PAGE);
			if (ret) {
				printk(KERN_ERR"%s Failed to set DSI_CMD_DEFAULT_SWITCH_PAGE !!\n", __func__);
				mutex_unlock(&display->panel->panel_lock);
				mutex_unlock(&display->display_lock);
				return -1;
			}
			mutex_unlock(&display->panel->panel_lock);
			mutex_unlock(&display->display_lock);
		}

		ret = scnprintf(panel_rnum->serial_number,
				PANEL_IOCTL_BUF_MAX,
				"Get panel serial number: %016llX",
				serial_number);

		LCD_INFO("serial_number = [%s]", panel_rnum->serial_number);
		break;
	}

	return ret;
}

extern unsigned int oplus_dsi_log_type;
int oplus_display_panel_set_qcom_loglevel(void *data)
{
	struct kernel_loglevel *k_loginfo = data;
	if (k_loginfo == NULL) {
		LCD_ERR("k_loginfo is null pointer\n");
		return -EINVAL;
	}

	if (k_loginfo->enable) {
		oplus_dsi_log_type |= OPLUS_DEBUG_LOG_BACKLIGHT;
		oplus_dsi_log_type |= OPLUS_DEBUG_LOG_COMMON;
		oplus_display_trace_enable |= OPLUS_DISPLAY_TRACE_ALL;
	} else {
		oplus_dsi_log_type &= ~OPLUS_DEBUG_LOG_BACKLIGHT;
		oplus_dsi_log_type &= ~OPLUS_DEBUG_LOG_COMMON;
		oplus_display_trace_enable &= ~OPLUS_DISPLAY_TRACE_ALL;
	}

	LCD_INFO("Set qcom kernel log, enable:0x%X, level:0x%X, current:0x%X\n",
			k_loginfo->enable,
			k_loginfo->log_level,
			oplus_dsi_log_type);
	return 0;
}

int oplus_big_endian_copy(void *dest, void *src, int count)
{
	int index = 0, knum = 0, rc = 0;
	uint32_t *u_dest = (uint32_t*) dest;
	char *u_src = (char*) src;

	if (dest == NULL || src == NULL) {
		LCD_ERR("null pointer\n");
		return -EINVAL;
	}

	if (dest == src) {
		return rc;
	}

	while (count > 0) {
		u_dest[index] = ((u_src[knum] << 24) | (u_src[knum+1] << 16) | (u_src[knum+2] << 8) | u_src[knum+3]);
		index += 1;
		knum += 4;
		count = count - 1;
	}

	return rc;
}

int oplus_display_panel_get_softiris_color_status(void *data)
{
	struct softiris_color *iris_color_status = data;
	bool color_vivid_status = false;
	bool color_srgb_status = false;
	bool color_softiris_status = false;
	bool color_dual_panel_status = false;
	bool color_dual_brightness_status = false;
	bool color_oplus_calibrate_status = false;
	bool color_samsung_status = false;
	bool color_loading_status = false;
	bool color_2nit_status = false;
	bool color_nature_profession_status = false;
	struct dsi_parser_utils *utils = NULL;
	struct dsi_panel *panel = NULL;
	int display_id = iris_color_status->color_dual_panel_status;

	struct dsi_display *display = get_main_display();
	if (1 == display_id)
		display = get_sec_display();
	if (!display) {
		LCD_ERR("display is null\n");
		return -EINVAL;
	}

	panel = display->panel;
	if (!panel) {
		LCD_ERR("panel is null\n");
		return -EINVAL;
	}

	utils = &panel->utils;
	if (!utils) {
		LCD_ERR("utils is null\n");
		return -EINVAL;
	}

#if defined(CONFIG_PXLW_IRIS)
	if (iris_is_chip_supported() && (!strcmp(panel->type, "secondary"))) {
		LCD_INFO("iris secondary panel no need config\n");
		return 0;
	}
#endif

	color_vivid_status = utils->read_bool(utils->data, "oplus,color_vivid_status");
	LCD_INFO("oplus,color_vivid_status: %s\n", color_vivid_status ? "true" : "false");

	color_srgb_status = utils->read_bool(utils->data, "oplus,color_srgb_status");
	LCD_INFO("oplus,color_srgb_status: %s\n", color_srgb_status ? "true" : "false");

	color_softiris_status = utils->read_bool(utils->data, "oplus,color_softiris_status");
	LCD_INFO("oplus,color_softiris_status: %s\n", color_softiris_status ? "true" : "false");

	color_dual_panel_status = utils->read_bool(utils->data, "oplus,color_dual_panel_status");
	LCD_INFO("oplus,color_dual_panel_status: %s\n", color_dual_panel_status ? "true" : "false");

	color_dual_brightness_status = utils->read_bool(utils->data, "oplus,color_dual_brightness_status");
	LCD_INFO("oplus,color_dual_brightness_status: %s\n", color_dual_brightness_status ? "true" : "false");

	color_oplus_calibrate_status = utils->read_bool(utils->data, "oplus,color_oplus_calibrate_status");
	LCD_INFO("oplus,color_oplus_calibrate_status: %s\n", color_oplus_calibrate_status ? "true" : "false");

	color_samsung_status = utils->read_bool(utils->data, "oplus,color_samsung_status");
	LCD_INFO("oplus,color_samsung_status: %s\n", color_samsung_status ? "true" : "false");

	color_loading_status = utils->read_bool(utils->data, "oplus,color_loading_status");
	LCD_INFO("oplus,color_loading_status: %s\n", color_loading_status ? "true" : "false");

	color_2nit_status = utils->read_bool(utils->data, "oplus,color_2nit_status");
	LCD_INFO("oplus,color_2nit_status: %s\n", color_2nit_status ? "true" : "false");

	color_nature_profession_status = utils->read_bool(utils->data, "oplus,color_nature_profession_status");
	LCD_INFO("oplus,color_nature_profession_status: %s\n", color_nature_profession_status ? "true" : "false");

	if (is_project(22111) || is_project(22112)) {
		color_softiris_status = false;
		color_oplus_calibrate_status = true;
		color_nature_profession_status = false;
		LCD_INFO("Factory Calibration settings on 22111 or 22112\n");
		LCD_INFO("oplus,color_softiris_status: %s\n", color_softiris_status ? "true" : "false");
		LCD_INFO("oplus,color_oplus_calibrate_status: %s\n", color_oplus_calibrate_status ? "true" : "false");
		LCD_INFO("oplus,color_nature_profession_status: %s\n", color_nature_profession_status ? "true" : "false");
	}

	iris_color_status->color_vivid_status = (uint32_t)color_vivid_status;
	iris_color_status->color_srgb_status = (uint32_t)color_srgb_status;
	iris_color_status->color_softiris_status = (uint32_t)color_softiris_status;
	iris_color_status->color_dual_panel_status = (uint32_t)color_dual_panel_status;
	iris_color_status->color_dual_brightness_status = (uint32_t)color_dual_brightness_status;
	iris_color_status->color_oplus_calibrate_status = (uint32_t)color_oplus_calibrate_status;
	iris_color_status->color_samsung_status = (uint32_t)color_samsung_status;
	iris_color_status->color_loading_status = (uint32_t)color_loading_status;
	iris_color_status->color_2nit_status = (uint32_t)color_2nit_status;
	iris_color_status->color_nature_profession_status = (uint32_t)color_nature_profession_status;

	return 0;
}

int oplus_display_panel_get_panel_type(void *data)
{
	int ret = 0;
	uint32_t *temp_save = data;
	uint32_t panel_id = (*temp_save >> 12);
	uint32_t panel_type = 0;

	struct dsi_panel *panel = NULL;
	struct dsi_parser_utils *utils = NULL;
	struct dsi_display *display = get_main_display();
	if (1 == panel_id) {
		display = get_sec_display();
	}

	if (!display) {
		LCD_ERR("display is null\n");
		return -EINVAL;
	}
	panel = display->panel;
	if (!panel) {
		LCD_ERR("panel is null\n");
		return -EINVAL;
	}

	utils = &panel->utils;
	if (!utils) {
		LCD_ERR("utils is null\n");
		return -EINVAL;
	}

	ret = utils->read_u32(utils->data, "oplus,mdss-dsi-panel-type", &panel_type);
	LCD_INFO("oplus,mdss-dsi-panel-type: %d\n", panel_type);

	*temp_save = panel_type;

	return ret;
}

int oplus_display_panel_get_id2(void)
{
	struct dsi_display *display = get_main_display();
	int ret = 0;
	unsigned char read[30];
	if(!display || !display->panel) {
		LCD_ERR("display is null\n");
		return 0;
	}

	/* if(__oplus_get_power_status() == OPLUS_DISPLAY_POWER_ON) { */
	if (display->panel->power_mode == SDE_MODE_DPMS_ON) {
		if (!strcmp(display->panel->name, "AA545 P 1 A0006 dsc cmd mode panel")) {
			mutex_lock(&display->display_lock);
			ret = dsi_display_read_panel_reg(display, 0xDB, read, 1);
			mutex_unlock(&display->display_lock);
			if (ret < 0) {
				LCD_ERR("failed to read DB ret=%d\n", ret);
				return -EINVAL;
			}
			ret = (int)read[0];
		}
	} else {
		LCD_WARN("display panel status is not on\n");
		return 0;
	}

	return ret;
}

int oplus_display_panel_hbm_lightspot_check(void)
{
	int rc = 0;
	char value[] = { 0xE0 };
	char value1[] = { 0x0F, 0xFF };
	struct dsi_display *display = get_main_display();
	struct mipi_dsi_device *mipi_device;

	if (!display || !display->panel) {
		LCD_ERR("display is null\n");
		return -EINVAL;
	}

	/* if (__oplus_get_power_status() != OPLUS_DISPLAY_POWER_ON) { */
	if (display->panel->power_mode != SDE_MODE_DPMS_ON) {
		LCD_WARN("display panel in off status\n");
		return -EFAULT;
	}

	mipi_device = &display->panel->mipi_device;

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	if (!dsi_panel_initialized(display->panel)) {
		LCD_ERR("dsi_panel_initialized failed\n");
		rc = -EINVAL;
		goto unlock;
	}

	rc = dsi_display_cmd_engine_enable(display);

	if (rc) {
		LCD_ERR("cmd engine enable failed\n");
		rc = -EINVAL;
		goto unlock;
	}

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_ON);
	}

	rc = mipi_dsi_dcs_write(mipi_device, 0x53, value, sizeof(value));
	usleep_range(1000, 1100);
	rc = mipi_dsi_dcs_write(mipi_device, 0x51, value1, sizeof(value1));
	usleep_range(1000, 1100);
	LCD_ERR("[%s] hbm_lightspot_check successfully\n",  display->panel->oplus_priv.vendor_name);

	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);
	}

	dsi_display_cmd_engine_disable(display);

unlock:

	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);
	return 0;
}

int oplus_display_panel_get_dp_support(void *buf)
{
	struct dsi_display *display = NULL;
	struct dsi_panel *d_panel = NULL;
	uint32_t *dp_support = buf;

	if (!dp_support) {
		LCD_ERR("oplus_display_panel_get_dp_support error dp_support is null\n");
		return -EINVAL;
	}

	display = get_main_display();
	if (!display) {
		LCD_ERR("display is null\n");
		return -EINVAL;
	}

	d_panel = display->panel;
	if (!d_panel) {
		LCD_ERR("panel is null\n");
		return -EINVAL;
	}

	*dp_support = d_panel->oplus_priv.dp_support;

	return 0;
}

int oplus_display_panel_set_audio_ready(void *data) {
	uint32_t *audio_ready = data;

	oplus_display_audio_ready = (*audio_ready);
	LCD_INFO("oplus_display_audio_ready = %d\n", oplus_display_audio_ready);

	return 0;
}

int oplus_display_panel_dump_info(void *data) {
	int ret = 0;
	struct dsi_display * temp_display;
	struct display_timing_info *timing_info = data;

	temp_display = get_main_display();

	if (temp_display == NULL) {
		LCD_ERR("display is null\n");
		ret = -1;
		return ret;
	}

	if(temp_display->modes == NULL) {
		LCD_ERR("display modes is null\n");
		ret = -1;
		return ret;
	}

	timing_info->h_active = temp_display->modes->timing.h_active;
	timing_info->v_active = temp_display->modes->timing.v_active;
	timing_info->refresh_rate = temp_display->modes->timing.refresh_rate;
	timing_info->clk_rate_hz_l32 = (uint32_t)(temp_display->modes->timing.clk_rate_hz & 0x00000000FFFFFFFF);
	timing_info->clk_rate_hz_h32 = (uint32_t)(temp_display->modes->timing.clk_rate_hz >> 32);

	return 0;
}

int oplus_display_panel_get_dsc(void *data) {
	int ret = 0;
	uint32_t *reg_read = data;
	unsigned char read[30];
	struct dsi_display *display = get_main_display();

	if (!display || !display->panel) {
		LCD_ERR("display is null\n");
		return -EINVAL;
	}

	/* if (__oplus_get_power_status() == OPLUS_DISPLAY_POWER_ON) { */
	if (display->panel->power_mode == SDE_MODE_DPMS_ON) {
		mutex_lock(&display->display_lock);
		ret = dsi_display_read_panel_reg(get_main_display(), 0x03, read, 1);
		mutex_unlock(&display->display_lock);
		if (ret < 0) {
			LCD_ERR("read panel dsc reg error = %d\n", ret);
			ret = -1;
		} else {
			(*reg_read) = read[0];
			ret = 0;
		}
	} else {
		LCD_WARN("display panel status is not on\n");
		ret = -1;
	}

	return ret;
}

int oplus_display_panel_get_closebl_flag(void *data)
{
	uint32_t *closebl_flag = data;

	(*closebl_flag) = lcd_closebl_flag;
	LCD_INFO("oplus_display_get_closebl_flag = %d\n", lcd_closebl_flag);

	return 0;
}

int oplus_display_panel_set_closebl_flag(void *data)
{
	uint32_t *closebl = data;

	LCD_ERR("lcd_closebl_flag = %d\n", (*closebl));
	if (1 != (*closebl))
		lcd_closebl_flag = 0;
	LCD_ERR("oplus_display_set_closebl_flag = %d\n", lcd_closebl_flag);

	return 0;
}

int oplus_display_panel_get_reg(void *data)
{
	struct dsi_display *display = get_main_display();
	struct panel_reg_get *panel_reg = data;
	uint32_t u32_bytes = sizeof(uint32_t)/sizeof(char);

	if (!display) {
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);

	u32_bytes = oplus_rx_len%u32_bytes ? (oplus_rx_len/u32_bytes + 1) : oplus_rx_len/u32_bytes;
	oplus_big_endian_copy(panel_reg->reg_rw, oplus_rx_reg, u32_bytes);
	panel_reg->lens = oplus_rx_len;

	mutex_unlock(&display->display_lock);

	return 0;
}

int oplus_display_panel_set_reg(void *data)
{
	char reg[PANEL_TX_MAX_BUF] = {0x0};
	char payload[PANEL_TX_MAX_BUF] = {0x0};
	u32 index = 0, value = 0;
	int ret = 0;
	int len = 0;
	struct dsi_display *display = get_main_display();
	struct panel_reg_rw *reg_rw = data;

	if (!display || !display->panel) {
		LCD_ERR("display is null\n");
		return -EFAULT;
	}

	if (reg_rw->lens > PANEL_IOCTL_BUF_MAX) {
		LCD_ERR("error: wrong input reg len\n");
		return -EINVAL;
	}

	if (reg_rw->rw_flags == REG_READ) {
		value = reg_rw->cmd;
		len = reg_rw->lens;
		mutex_lock(&display->display_lock);
		dsi_display_read_panel_reg(get_main_display(), value, reg, len);
		mutex_unlock(&display->display_lock);

		for (index=0; index < len; index++) {
			LCD_INFO("reg[%d] = %x\n", index, reg[index]);
		}
		mutex_lock(&display->display_lock);
		memcpy(oplus_rx_reg, reg, PANEL_TX_MAX_BUF);
		oplus_rx_len = len;
		mutex_unlock(&display->display_lock);
		return 0;
	}

	if (reg_rw->rw_flags == REG_WRITE) {
		memcpy(payload, reg_rw->value, reg_rw->lens);
		reg[0] = reg_rw->cmd;
		len = reg_rw->lens;
		for (index=0; index < len; index++) {
			reg[index + 1] = payload[index];
		}

		/* if(__oplus_get_power_status() == OPLUS_DISPLAY_POWER_ON) { */
		if (display->panel->power_mode != SDE_MODE_DPMS_OFF) {
				/* enable the clk vote for CMD mode panels */
			mutex_lock(&display->display_lock);
			mutex_lock(&display->panel->panel_lock);

			if (display->panel->panel_initialized) {
				if (display->config.panel_mode == DSI_OP_CMD_MODE) {
					dsi_display_clk_ctrl(display->dsi_clk_handle,
							DSI_ALL_CLKS, DSI_CLK_ON);
				}
				ret = mipi_dsi_dcs_write(&display->panel->mipi_device, reg[0],
						payload, len);
				if (display->config.panel_mode == DSI_OP_CMD_MODE) {
					dsi_display_clk_ctrl(display->dsi_clk_handle,
							DSI_ALL_CLKS, DSI_CLK_OFF);
				}
			}

			mutex_unlock(&display->panel->panel_lock);
			mutex_unlock(&display->display_lock);

			if (ret < 0) {
				return ret;
			}
		}
		return 0;
	}
	LCD_ERR("error: please check the args\n");
	return -1;
}

int oplus_display_panel_notify_blank(void *data)
{
	uint32_t *temp_save_user = data;
	int temp_save = (*temp_save_user);

	LCD_INFO("oplus_display_notify_panel_blank = %d\n", temp_save);

	if(temp_save == 1) {
		oplus_event_data_notifier_trigger(DRM_PANEL_EVENT_UNBLANK, 0, true);
	} else if (temp_save == 0) {
		oplus_event_data_notifier_trigger(DRM_PANEL_EVENT_BLANK, 0, true);
	}
	return 0;
}

int oplus_display_panel_get_spr(void *data)
{
	uint32_t *spr_mode_user = data;

	LCD_INFO("oplus_display_get_spr = %d\n", spr_mode);
	*spr_mode_user = spr_mode;

	return 0;
}

int oplus_display_panel_set_spr(void *data)
{
	uint32_t *temp_save_user = data;
	int temp_save = (*temp_save_user);
	struct dsi_display *display = get_main_display();

	if (!display || !display->panel) {
		LCD_ERR("display is null\n");
		return -EINVAL;
	}

	LCD_INFO("oplus_display_set_spr = %d\n", temp_save);

	__oplus_display_set_spr(temp_save);
	/* if(__oplus_get_power_status() == OPLUS_DISPLAY_POWER_ON) { */
	if (display->panel->power_mode == SDE_MODE_DPMS_ON) {
		if(get_main_display() == NULL) {
			LCD_ERR("display is null\n");
			return 0;
		}

		dsi_display_spr_mode(get_main_display(), spr_mode);
	} else {
		LCD_WARN("oplus_display_set_spr = %d, but now display panel status is not on\n",
				temp_save);
	}
	return 0;
}

int oplus_display_panel_get_dither(void *data)
{
	uint32_t *dither_mode_user = data;
	LCD_ERR("oplus_display_get_dither = %d\n", dither_enable);
	*dither_mode_user = dither_enable;
	return 0;
}

int oplus_display_panel_set_dither(void *data)
{
	uint32_t *temp_save_user = data;
	int temp_save = (*temp_save_user);
	LCD_INFO("oplus_display_set_dither = %d\n", temp_save);
	__oplus_display_set_dither(temp_save);
	return 0;
}

int oplus_display_panel_get_roundcorner(void *data)
{
	uint32_t *round_corner = data;
	bool roundcorner = true;

	*round_corner = roundcorner;

	return 0;
}

int oplus_display_panel_set_osc_track(u32 osc_status)
{
	struct dsi_display *display = get_main_display();
	int rc = 0;

	if (!display||!display->panel) {
		LCD_ERR("display is null\n");
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	if (!dsi_panel_initialized(display->panel)) {
		rc = -EINVAL;
		goto unlock;
	}

	/* enable the clk vote for CMD mode panels */
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_ON);
	}

	if (osc_status) {
		rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_OSC_TRACK_ON);
	} else {
		rc = dsi_panel_tx_cmd_set(display->panel, DSI_CMD_OSC_TRACK_OFF);
	}
	if (display->config.panel_mode == DSI_OP_CMD_MODE) {
		rc = dsi_display_clk_ctrl(display->dsi_clk_handle,
				DSI_CORE_CLK, DSI_CLK_OFF);
	}

unlock:
	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	return rc;
}

int oplus_display_panel_get_dynamic_osc_clock(void *data)
{
	int rc = 0;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = NULL;
	uint32_t *osc_rate = data;

	if (!display || !display->panel) {
		LCD_ERR("Invalid display or panel\n");
		rc = -EINVAL;
		return rc;
	}
	panel = display->panel;

	if (!display->panel->oplus_priv.ffc_enabled) {
		LCD_WARN("FFC is disabled, failed to get osc rate\n");
		rc = -EFAULT;
		return rc;
	}

	mutex_lock(&display->display_lock);
	mutex_lock(&panel->panel_lock);

	*osc_rate = panel->oplus_priv.osc_rate_cur;

	mutex_unlock(&panel->panel_lock);
	mutex_unlock(&display->display_lock);
	LCD_INFO("Get osc rate=%d\n", *osc_rate);

	return rc;
}

int oplus_display_panel_set_dynamic_osc_clock(void *data)
{
	int rc = 0;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = NULL;
	uint32_t *osc_rate = data;

	if (!display || !display->panel) {
		LCD_ERR("Invalid display or panel\n");
		rc = -EINVAL;
		return rc;
	}
	panel = display->panel;

	if (!display->panel->oplus_priv.ffc_enabled) {
		LCD_WARN("FFC is disabled, failed to set osc rate\n");
		rc = -EFAULT;
		return rc;
	}

	if(display->panel->power_mode != SDE_MODE_DPMS_ON) {
		LCD_WARN("display panel is not on\n");
		rc = -EFAULT;
		return rc;
	}

	LCD_INFO("Set osc rate=%d\n", *osc_rate);
	mutex_lock(&display->display_lock);

	rc = oplus_display_update_osc_ffc(display, *osc_rate);
	if (!rc) {
		mutex_lock(&panel->panel_lock);
		rc = oplus_panel_set_ffc_mode_unlock(panel);
		mutex_unlock(&panel->panel_lock);
	}

	mutex_unlock(&display->display_lock);

	return rc;
}

int oplus_display_panel_get_cabc_status(void *buf)
{
	int rc = 0;
	uint32_t *cabc_status = buf;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = NULL;

	if (!display || !display->panel) {
		LCD_ERR("Invalid display or panel\n");
		rc = -EINVAL;
		return rc;
	}
	panel = display->panel;

	mutex_lock(&display->display_lock);
	mutex_lock(&panel->panel_lock);

	if(panel->oplus_priv.cabc_enabled) {
		*cabc_status = oplus_cabc_status;
	} else {
		*cabc_status = OPLUS_DISPLAY_CABC_OFF;
	}

	mutex_unlock(&panel->panel_lock);
	mutex_unlock(&display->display_lock);
	LCD_INFO("Get cabc status: %d\n", *cabc_status);

	return rc;
}

int oplus_display_panel_set_cabc_status(void *buf)
{
	int rc = 0;
	uint32_t *cabc_status = buf;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = NULL;
	u32 cmd_index = 0;

	if (!display || !display->panel) {
		LCD_ERR("Invalid display or panel\n");
		rc = -EINVAL;
		return rc;
	}
	panel = display->panel;

	if (!panel->oplus_priv.cabc_enabled) {
		LCD_WARN("This project don't support cabc\n");
		rc = -EFAULT;
		return rc;
	}

	if (*cabc_status >= OPLUS_DISPLAY_CABC_UNKNOW) {
		LCD_ERR("Unknow cabc status: %d\n", *cabc_status);
		rc = -EINVAL;
		return rc;
	}

	if(display->panel->power_mode != SDE_MODE_DPMS_ON) {
		LCD_WARN("display panel is not on, buf=[%s]\n", buf);
		rc = -EFAULT;
		return rc;
	}

	LCD_INFO("Set cabc status: %d, buf=[%s]\n", *cabc_status, buf);
	mutex_lock(&display->display_lock);
	mutex_lock(&panel->panel_lock);

	cmd_index = DSI_CMD_CABC_OFF + *cabc_status;
	rc = dsi_panel_tx_cmd_set(panel, cmd_index);
	oplus_cabc_status = *cabc_status;

	mutex_unlock(&panel->panel_lock);
	mutex_unlock(&display->display_lock);

	return rc;
}

int oplus_display_panel_get_dre_status(void *buf)
{
	int rc = 0;
	uint32_t *dre_status = buf;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = NULL;

	if (!display || !display->panel) {
		LCD_ERR("Invalid display or panel\n");
		rc = -EINVAL;
		return rc;
	}
	panel = display->panel;

	if(panel->oplus_priv.dre_enabled) {
		*dre_status = oplus_dre_status;
	} else {
		*dre_status = OPLUS_DISPLAY_DRE_OFF;
	}

	return rc;
}

int oplus_display_panel_set_dre_status(void *buf)
{
	int rc = 0;
	uint32_t *dre_status = buf;
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel = NULL;

	if (!display || !display->panel) {
		LCD_ERR("Invalid display or panel\n");
		rc = -EINVAL;
		return rc;
	}
	panel = display->panel;

	if(!panel->oplus_priv.dre_enabled) {
		LCD_ERR("This project don't support dre\n");
		return -EFAULT;
	}

	if (*dre_status >= OPLUS_DISPLAY_DRE_UNKNOW) {
		LCD_ERR("Unknow DRE status = [%d]\n", *dre_status);
		return -EINVAL;
	}

	if(__oplus_get_power_status() == OPLUS_DISPLAY_POWER_ON) {
		if (*dre_status == OPLUS_DISPLAY_DRE_ON) {
			/* if(mtk)  */
			/*	disp_aal_set_dre_en(0);   MTK AAL api */
		} else {
			/* if(mtk) */
			/*	disp_aal_set_dre_en(1);  MTK AAL api */
		}
		oplus_dre_status = *dre_status;
		LCD_INFO("buf = [%s], oplus_dre_status = %d\n",
				buf, oplus_dre_status);
	} else {
		LCD_WARN("buf = [%s], but display panel status is not on\n",
				*dre_status);
	}

	return rc;
}

int oplus_display_panel_get_dither_status(void *buf)
{
	uint32_t *dither_enable = buf;
	*dither_enable = oplus_dither_enable;

	return 0;
}

int oplus_display_panel_set_dither_status(void *buf)
{
	uint32_t *dither_enable = buf;
	oplus_dither_enable = *dither_enable;
	LCD_INFO("buf = [%s], oplus_dither_enable = %d\n",
			buf, oplus_dither_enable);

	return 0;
}

int oplus_panel_set_ffc_mode_unlock(struct dsi_panel *panel)
{
	int rc = 0;
	u32 cmd_index = DSI_CMD_SET_MAX;

	if (panel->oplus_priv.ffc_mode_index >= FFC_MODE_MAX_COUNT) {
		LCD_ERR("Invalid ffc_mode_index=%d\n",
				panel->oplus_priv.ffc_mode_index);
		rc = -EINVAL;
		return rc;
	}

	cmd_index = DSI_CMD_FFC_MODE0 + panel->oplus_priv.ffc_mode_index;
	rc = dsi_panel_tx_cmd_set(panel, cmd_index);

	return rc;
}

int oplus_panel_set_ffc_kickoff_lock(struct dsi_panel *panel)
{
	int rc = 0;

	mutex_lock(&panel->oplus_ffc_lock);
	panel->oplus_priv.ffc_delay_frames--;
	if (panel->oplus_priv.ffc_delay_frames) {
		mutex_unlock(&panel->oplus_ffc_lock);
		return rc;
	}

	mutex_lock(&panel->panel_lock);
	rc = oplus_panel_set_ffc_mode_unlock(panel);
	mutex_unlock(&panel->panel_lock);

	mutex_unlock(&panel->oplus_ffc_lock);

	return rc;
}

int oplus_panel_check_ffc_config(struct dsi_panel *panel,
		struct oplus_clk_osc *clk_osc_pending)
{
	int rc = 0;
	int index;
	struct oplus_clk_osc *seq = panel->oplus_priv.clk_osc_seq;
	u32 count = panel->oplus_priv.ffc_mode_count;
	u32 last_index = panel->oplus_priv.ffc_mode_index;

	if (!seq || !count) {
		LCD_ERR("Invalid clk_osc_seq or ffc_mode_count\n");
		rc = -EINVAL;
		return rc;
	}

	for (index = 0; index < count; index++) {
		if (seq->clk_rate == clk_osc_pending->clk_rate &&
				seq->osc_rate == clk_osc_pending->osc_rate) {
			break;
		}
		seq++;
	}

	if (index < count) {
		LCD_INFO("Update ffc config: index:[%d -> %d], clk=%d, osc=%d\n",
				last_index,
				index,
				clk_osc_pending->clk_rate,
				clk_osc_pending->osc_rate);

		panel->oplus_priv.ffc_mode_index = index;
		panel->oplus_priv.clk_rate_cur = clk_osc_pending->clk_rate;
		panel->oplus_priv.osc_rate_cur = clk_osc_pending->osc_rate;
	} else {
		rc = -EINVAL;
	}

	return rc;
}

int oplus_display_update_clk_ffc(struct dsi_display *display,
		struct dsi_display_mode *cur_mode, struct dsi_display_mode *adj_mode)
{
	int rc = 0;
	struct dsi_panel *panel = display->panel;
	struct oplus_clk_osc clk_osc_pending;

	DSI_MM_INFO("DisplayDriverID@@426$$Switching ffc mode, clk:[%d -> %d]",
			display->cached_clk_rate,
			display->dyn_bit_clk);

	if (display->cached_clk_rate == display->dyn_bit_clk) {
		DSI_MM_WARN("DisplayDriverID@@427$$Ignore duplicated clk ffc setting, clk=%d",
				display->dyn_bit_clk);
		return rc;
	}

	mutex_lock(&panel->oplus_ffc_lock);

	clk_osc_pending.clk_rate = display->dyn_bit_clk;
	clk_osc_pending.osc_rate = panel->oplus_priv.osc_rate_cur;

	rc = oplus_panel_check_ffc_config(panel, &clk_osc_pending);
	if (!rc) {
		panel->oplus_priv.ffc_delay_frames = FFC_DELAY_MAX_FRAMES;
	} else {
		DSI_MM_ERR("DisplayDriverID@@427$$Failed to find ffc mode index, clk=%d, osc=%d",
				clk_osc_pending.clk_rate,
				clk_osc_pending.osc_rate);
	}

	mutex_unlock(&panel->oplus_ffc_lock);

	return rc;
}

int oplus_display_update_osc_ffc(struct dsi_display *display,
		u32 osc_rate)
{
	int rc = 0;
	struct dsi_panel *panel = display->panel;
	struct oplus_clk_osc clk_osc_pending;

	DSI_MM_INFO("DisplayDriverID@@428$$Switching ffc mode, osc:[%d -> %d]",
			panel->oplus_priv.osc_rate_cur,
			osc_rate);

	if (osc_rate == panel->oplus_priv.osc_rate_cur) {
		DSI_MM_WARN("DisplayDriverID@@429$$Ignore duplicated osc ffc setting, osc=%d",
				panel->oplus_priv.osc_rate_cur);
		return rc;
	}

	mutex_lock(&panel->oplus_ffc_lock);

	clk_osc_pending.clk_rate = panel->oplus_priv.clk_rate_cur;
	clk_osc_pending.osc_rate = osc_rate;
	rc = oplus_panel_check_ffc_config(panel, &clk_osc_pending);
	if (rc) {
		DSI_MM_ERR("DisplayDriverID@@429$$Failed to find ffc mode index, clk=%d, osc=%d",
				clk_osc_pending.clk_rate,
				clk_osc_pending.osc_rate);
	}

	mutex_unlock(&panel->oplus_ffc_lock);

	return rc;
}

int oplus_panel_parse_ffc_config(struct dsi_panel *panel)
{
	int rc = 0;
	int i;
	u32 length = 0;
	u32 count = 0;
	u32 size = 0;
	u32 *arr_32 = NULL;
	const u32 *arr;
	struct dsi_parser_utils *utils = &panel->utils;
	struct oplus_clk_osc *seq;

	if (panel->host_config.ext_bridge_mode)
		return 0;

	panel->oplus_priv.ffc_enabled = utils->read_bool(utils->data,
			"oplus,ffc-enabled");
	if (!panel->oplus_priv.ffc_enabled) {
		rc = -EFAULT;
		goto error;
	}

	arr = utils->get_property(utils->data,
			"oplus,clk-osc-sequence", &length);
	if (!arr) {
		LCD_ERR("[%s] oplus,clk-osc-sequence not found\n",
				panel->oplus_priv.vendor_name);
		rc = -EINVAL;
		goto error;
	}
	if (length & 0x1) {
		LCD_ERR("[%s] syntax error for oplus,clk-osc-sequence\n",
				panel->oplus_priv.vendor_name);
		rc = -EINVAL;
		goto error;
	}

	length = length / sizeof(u32);
	LCD_INFO("[%s] oplus,clk-osc-sequence length=%d\n",
			panel->oplus_priv.vendor_name, length);

	size = length * sizeof(u32);
	arr_32 = kzalloc(size, GFP_KERNEL);
	if (!arr_32) {
		rc = -ENOMEM;
		goto error;
	}

	rc = utils->read_u32_array(utils->data, "oplus,clk-osc-sequence",
					arr_32, length);
	if (rc) {
		LCD_ERR("[%s] cannot read oplus,clk-osc-sequence\n",
				panel->oplus_priv.vendor_name);
		goto error_free_arr_32;
	}

	count = length / 2;
	if (count > FFC_MODE_MAX_COUNT) {
		LCD_ERR("[%s] invalid ffc mode count:%d, greater than maximum:%d\n",
				panel->oplus_priv.vendor_name, count, FFC_MODE_MAX_COUNT);
		rc = -EINVAL;
		goto error_free_arr_32;
	}

	size = count * sizeof(*seq);
	seq = kzalloc(size, GFP_KERNEL);
	if (!seq) {
		rc = -ENOMEM;
		goto error_free_arr_32;
	}

	panel->oplus_priv.clk_osc_seq = seq;
	panel->oplus_priv.ffc_delay_frames = 0;
	panel->oplus_priv.ffc_mode_count = count;
	panel->oplus_priv.ffc_mode_index = 0;
	panel->oplus_priv.clk_rate_cur = arr_32[0];
	panel->oplus_priv.osc_rate_cur = arr_32[1];

	for (i = 0; i < length; i += 2) {
		LCD_INFO("[%s] clk osc seq: index=%d <%d %d>\n",
				panel->oplus_priv.vendor_name, i / 2, arr_32[i], arr_32[i+1]);
		seq->clk_rate = arr_32[i];
		seq->osc_rate = arr_32[i + 1];
		seq++;
	}

error_free_arr_32:
	kfree(arr_32);
error:
	if (rc) {
		panel->oplus_priv.ffc_enabled = false;
	}

	LCD_INFO("[%s] oplus,ffc-enabled: %s\n",
			panel->oplus_priv.vendor_name,
			panel->oplus_priv.ffc_enabled ? "true" : "false");

	return rc;
}

int oplus_panel_parse_config(struct dsi_panel *panel)
{
	struct dsi_parser_utils *utils = &panel->utils;
	int ret = 0;

	/* Add for parse ffc config */
	oplus_panel_parse_ffc_config(panel);

	panel->oplus_priv.vendor_name = utils->get_property(utils->data,
			"oplus,mdss-dsi-vendor-name", NULL);

	if (!panel->oplus_priv.vendor_name) {
		LCD_ERR("Failed to found panel name, using dumming name\n");
		panel->oplus_priv.vendor_name = DSI_PANEL_OPLUS_DUMMY_VENDOR_NAME;
	}

	panel->oplus_priv.manufacture_name = utils->get_property(utils->data,
			"oplus,mdss-dsi-manufacture", NULL);

	if (!panel->oplus_priv.manufacture_name) {
		LCD_ERR("Failed to found panel name, using dumming name\n");
		panel->oplus_priv.manufacture_name = DSI_PANEL_OPLUS_DUMMY_MANUFACTURE_NAME;
	}

	panel->oplus_priv.gpio_pre_on = utils->read_bool(utils->data,
			"oplus,gpio-pre-on");
	LCD_INFO("oplus,gpio-pre-on: %s\n",
		panel->oplus_priv.gpio_pre_on ? "true" : "false");

	panel->oplus_priv.is_pxlw_iris5 = utils->read_bool(utils->data,
			"oplus,is_pxlw_iris5");
	LCD_INFO("is_pxlw_iris5: %s\n",
			panel->oplus_priv.is_pxlw_iris5 ? "true" : "false");

	panel->oplus_priv.iris_pw_enable = utils->read_bool(utils->data,
			"oplus,iris-pw-enabled");
	LCD_INFO("iris-pw-enabled: %s\n", panel->oplus_priv.iris_pw_enable ? "true" : "false");
	if (panel->oplus_priv.iris_pw_enable) {
		panel->oplus_priv.iris_pw_rst_gpio = utils->get_named_gpio(utils->data, "oplus,iris-pw-rst-gpio", 0);
		if (!gpio_is_valid(panel->oplus_priv.iris_pw_rst_gpio)) {
			LCD_ERR("failed get pw rst gpio\n");
		}
		panel->oplus_priv.iris_pw_0p9_en_gpio = utils->get_named_gpio(utils->data, "oplus,iris-pw-0p9-gpio", 0);
		if (!gpio_is_valid(panel->oplus_priv.iris_pw_0p9_en_gpio)) {
			LCD_ERR("failed get pw 0p9 en gpio\n");
		}
	}

	panel->oplus_priv.is_osc_support = utils->read_bool(utils->data, "oplus,osc-support");
	LCD_INFO("osc mode support: %s\n", panel->oplus_priv.is_osc_support ? "Yes" : "Not");

	if (panel->oplus_priv.is_osc_support) {
		ret = utils->read_u32(utils->data, "oplus,mdss-dsi-osc-clk-mode0-rate",
				&panel->oplus_priv.osc_clk_mode0_rate);
		if (ret) {
			LCD_ERR("failed get panel parameter: oplus,mdss-dsi-osc-clk-mode0-rate\n");
			panel->oplus_priv.osc_clk_mode0_rate = 0;
		}
		dynamic_osc_clock = panel->oplus_priv.osc_clk_mode0_rate;

		ret = utils->read_u32(utils->data, "oplus,mdss-dsi-osc-clk-mode1-rate",
				&panel->oplus_priv.osc_clk_mode1_rate);
		if (ret) {
			LCD_ERR("failed get panel parameter: oplus,mdss-dsi-osc-clk-mode1-rate\n");
			panel->oplus_priv.osc_clk_mode1_rate = 0;
		}
	}

#if defined(CONFIG_PXLW_IRIS)
	if (iris_is_chip_supported() && (!strcmp(panel->type, "secondary"))) {
		LCD_INFO("iris secondary panel no need config\n");
		return 0;
	}
#endif

	/* Add for apollo */
	panel->oplus_priv.is_apollo_support = utils->read_bool(utils->data, "oplus,apollo_backlight_enable");
	apollo_backlight_enable = panel->oplus_priv.is_apollo_support;
	LCD_INFO("apollo_backlight_enable: %s\n", panel->oplus_priv.is_apollo_support ? "true" : "false");

	if (panel->oplus_priv.is_apollo_support) {
		ret = utils->read_u32(utils->data, "oplus,apollo-sync-brightness-level",
				&panel->oplus_priv.sync_brightness_level);

		if (ret) {
			LCD_INFO("failed to get panel parameter: oplus,apollo-sync-brightness-level\n");
			/* Default sync brightness level is set to 200 */
			panel->oplus_priv.sync_brightness_level = 200;
		}
		panel->oplus_priv.dc_apollo_sync_enable = utils->read_bool(utils->data, "oplus,dc_apollo_sync_enable");
		if (panel->oplus_priv.dc_apollo_sync_enable) {
			ret = utils->read_u32(utils->data, "oplus,dc-apollo-backlight-sync-level",
					&panel->oplus_priv.dc_apollo_sync_brightness_level);
			if (ret) {
				LCD_INFO("failed to get panel parameter: oplus,dc-apollo-backlight-sync-level\n");
				panel->oplus_priv.dc_apollo_sync_brightness_level = 397;
			}
			ret = utils->read_u32(utils->data, "oplus,dc-apollo-backlight-sync-level-pcc-max",
					&panel->oplus_priv.dc_apollo_sync_brightness_level_pcc);
			if (ret) {
				LCD_INFO("failed to get panel parameter: oplus,dc-apollo-backlight-sync-level-pcc-max\n");
				panel->oplus_priv.dc_apollo_sync_brightness_level_pcc = 30000;
			}
			ret = utils->read_u32(utils->data, "oplus,dc-apollo-backlight-sync-level-pcc-min",
					&panel->oplus_priv.dc_apollo_sync_brightness_level_pcc_min);
			if (ret) {
				LCD_INFO("failed to get panel parameter: oplus,dc-apollo-backlight-sync-level-pcc-min\n");
				panel->oplus_priv.dc_apollo_sync_brightness_level_pcc_min = 29608;
			}
			LCD_INFO("dc apollo sync enable(%d,%d,%d)\n", panel->oplus_priv.dc_apollo_sync_brightness_level,
					panel->oplus_priv.dc_apollo_sync_brightness_level_pcc, panel->oplus_priv.dc_apollo_sync_brightness_level_pcc_min);
		}
	}

	panel->oplus_priv.enhance_mipi_strength = utils->read_bool(utils->data, "oplus,enhance_mipi_strength");
	oplus_enhance_mipi_strength = panel->oplus_priv.enhance_mipi_strength;
	LCD_INFO("lcm enhance_mipi_strength: %s\n", panel->oplus_priv.enhance_mipi_strength ? "true" : "false");

	panel->oplus_priv.oplus_vreg_ctrl_flag = utils->read_bool(utils->data, "oplus,vreg_ctrl_flag");
        g_oplus_vreg_ctrl_config = panel->oplus_priv.oplus_vreg_ctrl_flag;
        LCD_INFO("lcm oplus_vreg_ctrl_flag: %s\n", panel->oplus_priv.oplus_vreg_ctrl_flag ? "true" : "false");

	panel->oplus_priv.oplus_clk_vreg_ctrl_flag = utils->read_bool(utils->data, "oplus,clk_vreg_ctrl_flag");
	g_oplus_clk_vreg_ctrl_config = panel->oplus_priv.oplus_clk_vreg_ctrl_flag;
	LCD_INFO("lcm oplus_clk_vreg_ctrl_flag: %s\n", panel->oplus_priv.oplus_clk_vreg_ctrl_flag ? "true" : "false");

	ret = utils->read_u32(utils->data, "oplus,clk_vreg_ctrl_value", &panel->oplus_priv.oplus_clk_vreg_ctrl_value);
	if (ret) {
		LCD_INFO("failed to get panel parameter: oplus,clk_vreg_ctrl_value\n");
		panel->oplus_priv.oplus_clk_vreg_ctrl_value = 0x44;
	}
	g_oplus_clk_vreg_ctrl_value = panel->oplus_priv.oplus_clk_vreg_ctrl_value;
	LCD_INFO("lcm g_oplus_clk_vreg_ctrl_value:0x%x \n", panel->oplus_priv.oplus_clk_vreg_ctrl_value);


	utils->read_u32(utils->data, "oplus,dsi-serial-number-reg", &panel->oplus_ser.serial_number_reg);

	ret = utils->read_u32(utils->data, "oplus,wait-te-config", &panel->oplus_priv.wait_te_config);
	if (ret) {
		LCD_INFO("failed to get panel parameter: oplus,wait-te-config\n");
		panel->oplus_priv.wait_te_config = 0;
	}

	panel->oplus_priv.pwm_create_thread = utils->read_bool(utils->data,
			"oplus,pwm-create-thread-disable-duty");
	LCD_INFO("oplus,pwm-create-thread-disable-duty: %s\n",
			panel->oplus_priv.pwm_create_thread ? "true" : "false");

	panel->oplus_priv.oplus_bl_demura_dbv_support = utils->read_bool(utils->data,
			"oplus,bl_denura-dbv-switch-support");
	LCD_INFO("oplus,bl_denura-dbv-switch-support: %s\n",
		panel->oplus_priv.oplus_bl_demura_dbv_support ? "true" : "false");
	panel->oplus_priv.bl_demura_mode = 0;

	panel->oplus_priv.cmdq_pack_support = utils->read_bool(utils->data,
			"oplus,cmdq-pack-support");
	LCD_INFO("oplus,cmdq-pack-support: %s\n",
		panel->oplus_priv.cmdq_pack_support ? "true" : "false");
	panel->oplus_priv.cmdq_pack_state = false;

	return 0;
}
EXPORT_SYMBOL(oplus_panel_parse_config);

int oplus_display_tx_cmd_set_lock(struct dsi_display *display, enum dsi_cmd_set_type type)
{
	int rc = 0;

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);
	rc = dsi_panel_tx_cmd_set(display->panel, type);
	mutex_unlock(&display->panel->panel_lock);
	mutex_unlock(&display->display_lock);

	return rc;
}

int oplus_display_panel_get_iris_loopback_status(void *buf)
{
#if defined(CONFIG_PXLW_IRIS)
	uint32_t *status = buf;

	*status = iris_loop_back_validate();
#endif
	return 0;
}

unsigned char Skip_frame_Para[12][17]=
{
	/* 120HZ-DUTY 90HZ-DUTY 120HZ-DUTY 120HZ-VREF2 90HZ-VREF2 144HZ-VREF2 vdata DBV */
	{32, 40, 48, 32, 40, 32, 40, 48, 55, 55, 55, 55, 55, 55, 55, 55, 55}, /*HBM*/
	{32, 40, 48, 32, 40, 32, 40, 48, 27, 27, 36, 29, 29, 38, 27, 27, 36}, /*2315<=DBV<3515*/
	{32, 40, 48, 32, 40, 32, 40, 48, 27, 27, 36, 29, 29, 38, 27, 27, 36}, /*1604<=DBV<2315*/
	{8, 8, 8, 4, 4, 8, 8, 8, 30, 30, 30, 31, 31, 31, 30, 30, 30}, /*1511<=DBV<1604*/
	{8, 8, 8, 4, 4, 8, 8, 8, 30, 30, 30, 31, 31, 31, 30, 30, 30}, /*1419<=DBV<1511*/
	{4, 8, 8, 4, 4, 4, 8, 8, 30, 30, 30, 31, 31, 31, 30, 30, 30}, /*1328<=DBV<1419*/
	{4, 8, 8, 4, 4, 4, 8, 8, 30, 30, 30, 31, 31, 31, 30, 30, 30}, /*1212<=DBV<1328*/
	{4, 4, 4, 4, 4, 4, 4, 4, 29, 29, 29, 30, 30, 30, 29, 29, 29}, /*1096<=DBV<1212*/
	{4, 4, 4, 4, 4, 4, 4, 4, 29, 29, 29, 30, 30, 30, 29, 29, 29}, /*950<=DBV<1096*/
	{0, 4, 4, 0, 0, 0, 4, 4, 28, 28, 28, 30, 30, 30, 28, 28, 28}, /*761<=DBV<950*/
	{0, 0, 0, 0, 0, 0, 0, 0, 28, 28, 28, 28, 28, 28, 28, 28, 28}, /*544<=DBV<761*/
	{0, 0, 0, 0, 0, 0, 0, 0, 27, 27, 27, 28, 28, 28, 27, 27, 27}, /*8<=DBV<544*/
};

int oplus_display_update_dbv(struct dsi_panel *panel)
{
	int i = 0;
	int rc = 0;
	int a_size = 0;
	unsigned int bl_lvl;
	unsigned char para[17];
	struct dsi_display_mode *mode;
	struct dsi_cmd_desc *cmds;
	struct LCM_setting_table temp_dbv_cmd[50];
	uint8_t voltage1, voltage2, voltage3, voltage4;
	unsigned short vpark = 0;
	unsigned char voltage = 0;

	if (IS_ERR_OR_NULL(panel)) {
		pr_info("[DISP][INFO][%s:%d]Invalid params\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(&(panel->bl_config))) {
		pr_info("[DISP][INFO][%s:%d]Invalid params\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(panel->cur_mode)) {
		pr_info("[DISP][INFO][%s:%d]Invalid params\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!strcmp(panel->type, "secondary")) {
		return rc;
	}

	mode = panel->cur_mode;
	bl_lvl = panel->bl_config.bl_level;

	if (IS_ERR_OR_NULL(mode->priv_info)) {
		pr_info("[DISP][INFO][%s:%d]Invalid params\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(&(mode->priv_info->cmd_sets[DSI_CMD_SKIPFRAME_DBV]))) {
		pr_info("[DISP][INFO][%s:%d]Invalid params\n", __func__, __LINE__);
		return -EINVAL;
	}

	cmds = mode->priv_info->cmd_sets[DSI_CMD_SKIPFRAME_DBV].cmds;
	a_size = mode->priv_info->cmd_sets[DSI_CMD_SKIPFRAME_DBV].count;

	for(i = 0; i < a_size; i++) {
		temp_dbv_cmd[i].count = cmds[i].msg.tx_len;
		temp_dbv_cmd[i].para_list = (u8 *)cmds[i].msg.tx_buf;
	}

	if (bl_lvl > 3515) {
		for(i = 0; i < 17; i++) {
			para[i] = Skip_frame_Para[0][i];
		}
	} else if (bl_lvl >= 2315) {
		for(i = 0; i < 17; i++) {
			para[i] = Skip_frame_Para[1][i];
		}
	} else if (bl_lvl >= 1604) {
		for(i = 0; i < 17; i++) {
			para[i] = Skip_frame_Para[2][i];
		}
	} else if (bl_lvl >= 1511) {
		for(i = 0; i < 17; i++) {
			para[i] = Skip_frame_Para[3][i];
		}
	} else if (bl_lvl >= 1419) {
		for(i = 0; i < 17; i++) {
			para[i] = Skip_frame_Para[4][i];
		}
	} else if (bl_lvl >= 1328) {
		for(i = 0; i < 17; i++) {
			para[i] = Skip_frame_Para[5][i];
		}
	} else if (bl_lvl >= 1212) {
		for(i = 0; i < 17; i++) {
			para[i] = Skip_frame_Para[6][i];
		}
	} else if (bl_lvl >= 1096) {
		for(i = 0; i < 17; i++) {
			para[i] = Skip_frame_Para[7][i];
		}
	} else if (bl_lvl >= 950) {
		for(i = 0; i < 17; i++) {
			para[i] = Skip_frame_Para[8][i];
		}
	} else if (bl_lvl >= 761) {
		for(i = 0; i < 17; i++) {
			para[i] = Skip_frame_Para[9][i];
		}
	} else if (bl_lvl >= 544) {
		for(i = 0; i < 17; i++) {
			para[i] = Skip_frame_Para[10][i];
		}
	} else {
		for(i = 0; i < 17; i++) {
			para[i] = Skip_frame_Para[11][i];
		}
	}

	for(i = 0;i < 3;i++) {
		temp_dbv_cmd[2].para_list[4+i+1] = para[0];
		temp_dbv_cmd[2].para_list[8+i+1] = para[1];
		temp_dbv_cmd[2].para_list[12+i+1] = para[2];
		temp_dbv_cmd[4].para_list[4+i+1] = para[3];
		temp_dbv_cmd[4].para_list[8+i+1] = para[4];
		temp_dbv_cmd[6].para_list[4+i+1] = para[5];
		temp_dbv_cmd[6].para_list[8+i+1] = para[6];
	}
	for(i = 0;i < 3;i++) {
		temp_dbv_cmd[8].para_list[i+1] = para[8+i];
		temp_dbv_cmd[8].para_list[9+i+1] = para[11+i];
		temp_dbv_cmd[8].para_list[18+i+1] = para[14+i];
	}

	voltage = 69;
	vpark = (69 - voltage) * 1024 / (69 - 10);
	voltage1 = ((vpark & 0xFF00) >> 8) + ((vpark & 0xFF00) >> 6) + ((vpark & 0xFF00) >> 4);
	voltage2 = vpark & 0xFF;
	voltage3 = vpark & 0xFF;
	voltage4 = vpark & 0xFF;
	temp_dbv_cmd[16].para_list[0+1] = voltage1;
	temp_dbv_cmd[16].para_list[1+1] = voltage2;
	temp_dbv_cmd[16].para_list[2+1] = voltage3;
	temp_dbv_cmd[16].para_list[3+1] = voltage4;

	if (bl_lvl > 0x643) {
		temp_dbv_cmd[9].para_list[0+1] = 0xB2;
		temp_dbv_cmd[11].para_list[0+1] = 0xB2;
		temp_dbv_cmd[13].para_list[0+1] = 0xB2;
		temp_dbv_cmd[19].para_list[0+1] = 0x02;
		temp_dbv_cmd[19].para_list[1+1] = 0x03;
		temp_dbv_cmd[19].para_list[2+1] = 0x42;
	} else {
		temp_dbv_cmd[9].para_list[0+1] = 0xD2;
		temp_dbv_cmd[11].para_list[0+1] = 0xE2;
		temp_dbv_cmd[13].para_list[0+1] = 0xD2;
		temp_dbv_cmd[19].para_list[0+1] = 0x0F;
		temp_dbv_cmd[19].para_list[1+1] = 0x17;
		temp_dbv_cmd[19].para_list[2+1] = 0x4E;
	}

	temp_dbv_cmd[20].para_list[0+1] = (bl_lvl >> 8);
	temp_dbv_cmd[20].para_list[1+1] = (bl_lvl & 0xff);

	rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SKIPFRAME_DBV);
	if (rc < 0)
		DSI_ERR("Failed to set DSI_CMD_SKIPFRAME_DBV \n");

	return rc;
}

extern int sde_encoder_resource_control(struct drm_encoder *drm_enc, u32 sw_event);

int oplus_sde_early_wakeup(struct dsi_panel *panel)
{
	struct dsi_display *d_display = get_main_display();
	struct drm_encoder *drm_enc;

	if(!strcmp(panel->type, "secondary")) {
		d_display = get_sec_display();
	}

	if (!d_display || !d_display->bridge) {
		DSI_ERR("invalid display params\n");
		return -EINVAL;
	}
	drm_enc = d_display->bridge->base.encoder;
	if (!drm_enc) {
		DSI_ERR("invalid encoder params\n");
		return -EINVAL;
	}
	sde_encoder_resource_control(drm_enc,
			7 /*SDE_ENC_RC_EVENT_EARLY_WAKEUP*/);
	return 0;
}

int oplus_display_wait_for_event(struct dsi_display *display,
		enum msm_event_wait event)
{
	int rc = 0;
	char tag_name[64] = {0};
	struct drm_encoder *drm_enc = NULL;

	if (!display || !display->bridge) {
		LCD_INFO("invalid display params\n");
		return -ENODEV;
	}

	if (display->panel->power_mode != SDE_MODE_DPMS_ON || !display->panel->panel_initialized) {
		LCD_INFO("display panel in off status\n");
		return -ENODEV;
	}
	drm_enc = display->bridge->base.encoder;

	if (!drm_enc) {
		LCD_INFO("invalid encoder params\n");
		return -ENODEV;
	}

	if (sde_encoder_is_disabled(drm_enc)) {
		LCD_INFO("%s encoder is disabled\n", __func__);
		return -ENODEV;
	}

	sde_encoder_resource_control(drm_enc,
			7 /* SDE_ENC_RC_EVENT_EARLY_WAKEUP */);

	snprintf(tag_name, sizeof(tag_name), "oplus_display_wait_for_event_%d", event);
	SDE_ATRACE_BEGIN(tag_name);
	sde_encoder_wait_for_event(drm_enc, event);
	SDE_ATRACE_END(tag_name);

	return rc;
}
EXPORT_SYMBOL(oplus_display_wait_for_event);

void oplus_save_te_timestamp(struct sde_connector *c_conn, ktime_t timestamp)
{
	struct dsi_display *display = c_conn->display;
	if (!display || !display->panel)
		return;
	display->panel->te_timestamp = timestamp;
}

void oplus_need_to_sync_te(struct dsi_panel *panel)
{
	s64 us_per_frame;
	u32 vsync_width;
	ktime_t last_te_timestamp;
	int delay;
	int left_time;

	us_per_frame = panel->cur_mode->priv_info->vsync_period;
	vsync_width = panel->cur_mode->priv_info->vsync_width;
	/* add 700us to vsync width(first half of frame time) to
	 * 1. avoid command sent in the middle of TE cycle
	 * 2. compensate the TE shift period */
	vsync_width += 700;
	last_te_timestamp = panel->te_timestamp;

	SDE_ATRACE_BEGIN("oplus_need_to_sync_te");
	delay = vsync_width - (ktime_to_us(ktime_sub(ktime_get(), last_te_timestamp)) % us_per_frame);
	if (delay > 0) {
		SDE_EVT32(us_per_frame, last_te_timestamp, delay);
		usleep_range(delay, delay + 100);
	} else {
		/* detect the left time for command sending in current frame,
		 * if it isn't enough for completing cmd sending,
		 * defer sending the command until the second half of the next frame. */
		left_time = us_per_frame - (vsync_width - delay);
		if (panel->pwm_params.directional_onepulse_switch) {
			if (left_time < 1000) {
				delay = left_time + vsync_width;
				usleep_range(delay, delay + 100);
			}
		} else {
			if (left_time < 2000) {
				delay = left_time + vsync_width;
				usleep_range(delay, delay + 100);
			}
		}
	}
	SDE_ATRACE_END("oplus_need_to_sync_te");

	return;
}

int oplus_wait_for_vsync(struct dsi_panel *panel)
{
	int rc = 0;
	struct dsi_display *d_display = get_main_display();
	struct drm_encoder *drm_enc = NULL;

	if (!panel || !panel->cur_mode) {
		DSI_ERR("Oplus Features config No panel device\n");
		return -ENODEV;
	}

	if (panel->power_mode == SDE_MODE_DPMS_OFF || !panel->panel_initialized) {
		LCD_WARN("display panel in off status\n");
		return -ENODEV;
	}

	if(!strcmp(panel->type, "secondary")) {
		d_display = get_sec_display();
	}

	if (!d_display || !d_display->bridge) {
		DSI_ERR("invalid display params\n");
		return -ENODEV;
	}

	drm_enc = d_display->bridge->base.encoder;

	if (!drm_enc) {
		DSI_ERR("invalid encoder params\n");
		return -ENODEV;
	}

	sde_encoder_wait_for_event(drm_enc, MSM_ENC_VBLANK);

	return rc;
}

void oplus_apollo_async_bl_delay(struct dsi_panel *panel)
{
	s64 us_per_frame;
	s64 duration;
	u32 async_bl_delay;
	ktime_t last_te_timestamp;
	int delay;
	char tag_name[128];
	u32 debounce_time = 3000;
	u32 frame_end;
	struct dsi_display *display = NULL;
	struct sde_encoder_virt *sde_enc;

	us_per_frame = panel->cur_mode->priv_info->vsync_period;
	async_bl_delay = panel->cur_mode->priv_info->async_bl_delay;
	last_te_timestamp = panel->te_timestamp;

	if(!strcmp(panel->type, "primary")) {
		display = get_main_display();
	} else if (!strcmp(panel->type, "secondary")) {
		display = get_sec_display();
	} else {
		LCD_ERR("[DISP][ERR][%s:%d]dsi_display error\n", __func__, __LINE__);
		return;
	}
	sde_enc = to_sde_encoder_virt(display->bridge->base.encoder);
	if (!sde_enc) {
		DSI_ERR("invalid encoder params\n");
		return;
	}

	duration = ktime_to_us(ktime_sub(ktime_get(), last_te_timestamp));
	if(duration > 3 * us_per_frame || sde_enc->rc_state == 4) {
		SDE_ATRACE_BEGIN("bl_delay_prepare");
		oplus_sde_early_wakeup(panel);
		if (duration > 16 * us_per_frame) {
			oplus_wait_for_vsync(panel);
		}
		SDE_ATRACE_END("bl_delay_prepare");
	}

	last_te_timestamp = panel->te_timestamp;
	duration = ktime_to_us(ktime_sub(ktime_get(), last_te_timestamp));
	delay = async_bl_delay - (duration % us_per_frame);
	snprintf(tag_name, sizeof(tag_name), "async_bl_delay: delay %d us, last te: %lld", delay, ktime_to_us(last_te_timestamp));

	if (delay > 0) {
		SDE_ATRACE_BEGIN(tag_name);
		SDE_EVT32(us_per_frame, last_te_timestamp, delay);
		usleep_range(delay, delay + 100);
		SDE_ATRACE_END(tag_name);
	}

	frame_end = us_per_frame - (ktime_to_us(ktime_sub(ktime_get(), last_te_timestamp)) % us_per_frame);

	if (frame_end < debounce_time) {
		delay = frame_end + async_bl_delay;
		snprintf(tag_name, sizeof(tag_name), "async_bl_delay: delay %d us to next frame, last te: %lld", delay, ktime_to_us(last_te_timestamp));
		SDE_ATRACE_BEGIN(tag_name);
		usleep_range(delay, delay + 100);
		SDE_ATRACE_END(tag_name);
	}

	return;
}

void oplus_disable_bl_delay_with_frame(struct dsi_panel *panel, u32 disable_frames)
{
	if (disable_frames) {
		if (panel->oplus_priv.disable_delay_bl_count < disable_frames) {
			panel->oplus_priv.disable_delay_bl_count = disable_frames;
			DSI_INFO("the bl_delay of the next %d frames will be disabled\n", panel->oplus_priv.disable_delay_bl_count);
		}
	}
	return;
}

int oplus_display_panel_set_hbm_max(void *data)
{
	int rc = 0;
	static u32 last_bl = 0;
	u32 *buf = data;
	u32 hbm_max_state = *buf & 0xF;
	int panel_id = (*buf >> 12);
	struct dsi_panel *panel = NULL;
	struct dsi_display *display = get_main_display();

	if (panel_id == 1)
		display = get_sec_display();

	if (!display || !display->panel) {
		LCD_ERR("Invalid display or panel\n");
		rc = -EINVAL;
		return rc;
	}

	panel = display->panel;

	if (display->panel->power_mode != SDE_MODE_DPMS_ON) {
		LCD_WARN("display panel is not on\n");
		rc = -EFAULT;
		return rc;
	}

	LCD_INFO("Set hbm max state=%d\n", hbm_max_state);

	if ((!strcmp(panel->name, "P 3 AB781 dsc cmd mode panel")
		|| !strcmp(panel->name, "P 3 AB714 dsc cmd mode panel")
		|| !strcmp(panel->name, "P 7 AB715 dsc cmd mode panel"))
		&& oplus_panel_pwm_onepulse_is_enabled(panel)) {
		LCD_WARN("panel onepulse is enable, can't set hbm max\n");
		rc = -EFAULT;
		return rc;
	}

	mutex_lock(&display->display_lock);

	last_bl = oplus_last_backlight;
	if (hbm_max_state) {
		if (panel->cur_mode->priv_info->cmd_sets[DSI_CMD_HBM_MAX].count) {
			mutex_lock(&panel->panel_lock);
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_HBM_MAX);
			mutex_unlock(&panel->panel_lock);
		}
		else {
			LCD_WARN("DSI_CMD_HBM_MAX is undefined, set max backlight: %d\n",
					panel->bl_config.bl_max_level);
			rc = dsi_display_set_backlight(display->drm_conn,
					display, panel->bl_config.bl_max_level);
		}
	}
	else {
		if (panel->cur_mode->priv_info->cmd_sets[DSI_CMD_EXIT_HBM_MAX].count) {
			mutex_lock(&panel->panel_lock);
			rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_EXIT_HBM_MAX);
			mutex_unlock(&panel->panel_lock);
		} else {
			rc = dsi_display_set_backlight(display->drm_conn,
					display, last_bl);
		}
	}
	panel->oplus_priv.hbm_max_state = hbm_max_state;

	mutex_unlock(&display->display_lock);

	return rc;
}

int oplus_display_panel_get_hbm_max(void *data)
{
	int rc = 0;
	u32 *hbm_max_state = data;
	int panel_id = (*hbm_max_state >> 12);
	struct dsi_panel *panel = NULL;
	struct dsi_display *display = get_main_display();

	if (panel_id == 1)
		display = get_sec_display();

	if (!display || !display->panel) {
		LCD_ERR("Invalid display or panel\n");
		rc = -EINVAL;
		return rc;
	}

	panel = display->panel;

	mutex_lock(&display->display_lock);
	mutex_lock(&panel->panel_lock);

	*hbm_max_state = panel->oplus_priv.hbm_max_state;

	mutex_unlock(&panel->panel_lock);
	mutex_unlock(&display->display_lock);
	LCD_INFO("Get hbm max state: %d\n", *hbm_max_state);

	return rc;
}

void oplus_panel_switch_to_sync_te(struct dsi_panel *panel)
{
	s64 us_per_frame;
	s64 duration;
	u32 vsync_width;
	ktime_t last_te_timestamp;
	int delay = 0;
	u32 vsync_cost = 0;
	u32 debounce_time = 500;
	u32 frame_end = 0;
	struct dsi_display *display = NULL;
	struct sde_encoder_virt *sde_enc;


	if (panel->power_mode != SDE_MODE_DPMS_ON || !panel->panel_initialized) {
		LCD_INFO("display panel in off status\n");
		return;
	}

#if defined(CONFIG_PXLW_IRIS)
	if (iris_is_chip_supported() && (!strcmp(panel->type, "secondary"))) {
		LCD_INFO("iris secondary panel no need config\n");
		return;
	}
#endif

	us_per_frame = panel->last_us_per_frame;
	vsync_width = panel->last_vsync_width;
	last_te_timestamp = panel->te_timestamp;

	if(!strcmp(panel->type, "primary")) {
		display = get_main_display();
	} else if (!strcmp(panel->type, "secondary")) {
		display = get_sec_display();
	} else {
		LCD_ERR("[DISP][ERR][%s:%d]dsi_display error\n", __func__, __LINE__);
		return;
	}

	sde_enc = to_sde_encoder_virt(display->bridge->base.encoder);
	if (!sde_enc) {
		DSI_ERR("invalid encoder params\n");
		return;
	}

	duration = ktime_to_us(ktime_sub(ktime_get(), last_te_timestamp));
	if(duration > 3 * us_per_frame || sde_enc->rc_state == 4) {
		SDE_ATRACE_BEGIN("timing_delay_prepare");
		oplus_sde_early_wakeup(panel);
		if (duration > 12 * us_per_frame) {
			oplus_wait_for_vsync(panel);
		}
		SDE_ATRACE_END("timing_delay_prepare");
	}

	last_te_timestamp = panel->te_timestamp;
	vsync_cost = ktime_to_us(ktime_sub(ktime_get(), last_te_timestamp)) % us_per_frame;
	delay = vsync_width - vsync_cost;

	SDE_ATRACE_BEGIN("oplus_panel_switch_to_sync_te");
	if (delay >= 0) {
		if (panel->last_refresh_rate == 120) {
			if (vsync_cost < 1000)
				usleep_range(1 * 1000, (1 * 1000) + 100);
		} else if (panel->last_refresh_rate == 60) {
			usleep_range(delay + 200, delay + 300);
		} else if (panel->last_refresh_rate == 90) {
			if((2100 < vsync_cost) && (vsync_cost < 3100))
				usleep_range(2 * 1000, (2 * 1000) + 100);
		}
	} else if (vsync_cost > vsync_width) {
		frame_end = us_per_frame - vsync_cost;
		if (frame_end < debounce_time) {
			if (panel->last_refresh_rate == 60) {
				usleep_range(9 * 1000, (9 * 1000) + 100);
			} else if (panel->last_refresh_rate == 120) {
				usleep_range(2 * 1000, (2 * 1000) + 100);
			}
		}
	}
	SDE_ATRACE_END("oplus_panel_switch_to_sync_te");

	return;
}

void oplus_set_pwm_switch_cmd_te_flag(struct sde_connector *c_conn)
{
	struct dsi_display *display = c_conn->display;

	if (!display || !display->panel || !display->panel->pwm_params.directional_onepulse_switch)
		return;

	if (display->panel->oplus_priv.pwm_sw_cmd_te_cnt > 0) {
		display->panel->oplus_priv.pwm_sw_cmd_te_cnt--;
	}
}

int oplus_display_set_shutdown_flag(void *buf)
{
	shutdown_flag = 1;
	pr_err("debug for %s, buf = [%s], shutdown_flag = %d\n",
			__func__, buf, shutdown_flag);

	return 0;
}

int oplus_display_panel_set_dc_compensate(void *data)
{
	uint32_t *dc_compensate = data;
	dcc_flags = (int)(*dc_compensate);
	DSI_INFO("DCCompensate set dc %d\n", dcc_flags);
#ifdef OPLUS_TRACKPOINT_REPORT
	if (dcc_flags == FILE_DESTROY) {
		EXCEPTION_TRACKPOINT_REPORT("DisplayDriverID@@431$$DCCompensate file destroied!!");
	}
#endif /* OPLUS_TRACKPOINT_REPORT */
	return 0;
}
