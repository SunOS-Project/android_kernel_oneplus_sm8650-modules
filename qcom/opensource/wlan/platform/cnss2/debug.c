// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2016-2021, The Linux Foundation. All rights reserved. */
/* Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved. */


#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include "main.h"
#include "bus.h"
#include "debug.h"
#include "pci.h"

#define MMIO_REG_ACCESS_MEM_TYPE		0xFF
#define MMIO_REG_RAW_ACCESS_MEM_TYPE		0xFE
#define DEFAULT_KERNEL_LOG_LEVEL		INFO_LOG
#define DEFAULT_IPC_LOG_LEVEL			DEBUG_LOG

enum log_level cnss_kernel_log_level = DEFAULT_KERNEL_LOG_LEVEL;

#if IS_ENABLED(CONFIG_IPC_LOGGING)
void *cnss_ipc_log_context;
void *cnss_ipc_log_long_context;
enum log_level cnss_ipc_log_level = DEFAULT_IPC_LOG_LEVEL;

static int cnss_set_ipc_log_level(u32 val)
{
	if (val < MAX_LOG) {
		cnss_ipc_log_level = val;
		return 0;
	}

	return -EINVAL;
}

static u32 cnss_get_ipc_log_level(void)
{
	return cnss_ipc_log_level;
}
#else
static int cnss_set_ipc_log_level(int val) { return -EINVAL; }
static u32 cnss_get_ipc_log_level(void) { return MAX_LOG; }
#endif

static int cnss_pin_connect_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *cnss_priv = s->private;

	seq_puts(s, "Pin connect results\n");
	seq_printf(s, "FW power pin result: %04x\n",
		   cnss_priv->pin_result.fw_pwr_pin_result);
	seq_printf(s, "FW PHY IO pin result: %04x\n",
		   cnss_priv->pin_result.fw_phy_io_pin_result);
	seq_printf(s, "FW RF pin result: %04x\n",
		   cnss_priv->pin_result.fw_rf_pin_result);
	seq_printf(s, "Host pin result: %04x\n",
		   cnss_priv->pin_result.host_pin_result);
	seq_puts(s, "\n");

	return 0;
}

static int cnss_pin_connect_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_pin_connect_show, inode->i_private);
}

static const struct file_operations cnss_pin_connect_fops = {
	.read		= seq_read,
	.release	= single_release,
	.open		= cnss_pin_connect_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static u64 cnss_get_serial_id(struct cnss_plat_data *plat_priv)
{
	u32 msb = plat_priv->serial_id.serial_id_msb;
	u32 lsb = plat_priv->serial_id.serial_id_lsb;

	msb &= 0xFFFF;
	return (((u64)msb << 32) | lsb);
}

static int cnss_stats_show_state(struct seq_file *s,
				 struct cnss_plat_data *plat_priv)
{
	enum cnss_driver_state i;
	int skip = 0;
	unsigned long state;

	seq_printf(s, "\nSerial Number: 0x%llx",
		   cnss_get_serial_id(plat_priv));
	seq_printf(s, "\nState: 0x%lx(", plat_priv->driver_state);
	for (i = 0, state = plat_priv->driver_state; state != 0;
	     state >>= 1, i++) {
		if (!(state & 0x1))
			continue;

		if (skip++)
			seq_puts(s, " | ");

		switch (i) {
		case CNSS_QMI_WLFW_CONNECTED:
			seq_puts(s, "QMI_WLFW_CONNECTED");
			continue;
		case CNSS_FW_MEM_READY:
			seq_puts(s, "FW_MEM_READY");
			continue;
		case CNSS_FW_READY:
			seq_puts(s, "FW_READY");
			continue;
		case CNSS_IN_COLD_BOOT_CAL:
			seq_puts(s, "IN_COLD_BOOT_CAL");
			continue;
		case CNSS_DRIVER_LOADING:
			seq_puts(s, "DRIVER_LOADING");
			continue;
		case CNSS_DRIVER_UNLOADING:
			seq_puts(s, "DRIVER_UNLOADING");
			continue;
		case CNSS_DRIVER_IDLE_RESTART:
			seq_puts(s, "IDLE_RESTART");
			continue;
		case CNSS_DRIVER_IDLE_SHUTDOWN:
			seq_puts(s, "IDLE_SHUTDOWN");
			continue;
		case CNSS_DRIVER_PROBED:
			seq_puts(s, "DRIVER_PROBED");
			continue;
		case CNSS_DRIVER_RECOVERY:
			seq_puts(s, "DRIVER_RECOVERY");
			continue;
		case CNSS_FW_BOOT_RECOVERY:
			seq_puts(s, "FW_BOOT_RECOVERY");
			continue;
		case CNSS_DEV_ERR_NOTIFY:
			seq_puts(s, "DEV_ERR");
			continue;
		case CNSS_DRIVER_DEBUG:
			seq_puts(s, "DRIVER_DEBUG");
			continue;
		case CNSS_COEX_CONNECTED:
			seq_puts(s, "COEX_CONNECTED");
			continue;
		case CNSS_IMS_CONNECTED:
			seq_puts(s, "IMS_CONNECTED");
			continue;
		case CNSS_IN_SUSPEND_RESUME:
			seq_puts(s, "IN_SUSPEND_RESUME");
			continue;
		case CNSS_IN_REBOOT:
			seq_puts(s, "IN_REBOOT");
			continue;
		case CNSS_COLD_BOOT_CAL_DONE:
			seq_puts(s, "COLD_BOOT_CAL_DONE");
			continue;
		case CNSS_IN_PANIC:
			seq_puts(s, "IN_PANIC");
			continue;
		case CNSS_QMI_DEL_SERVER:
			seq_puts(s, "DEL_SERVER_IN_PROGRESS");
			continue;
		case CNSS_QMI_DMS_CONNECTED:
			seq_puts(s, "DMS_CONNECTED");
			continue;
		case CNSS_DMS_DEL_SERVER:
			seq_puts(s, "DMS_DEL_SERVER");
			continue;
		case CNSS_DAEMON_CONNECTED:
			seq_puts(s, "DAEMON_CONNECTED");
			continue;
		case CNSS_PCI_PROBE_DONE:
			seq_puts(s, "PCI PROBE DONE");
			continue;
		case CNSS_DRIVER_REGISTER:
			seq_puts(s, "DRIVER REGISTERED");
			continue;
		case CNSS_WLAN_HW_DISABLED:
			seq_puts(s, "WLAN HW DISABLED");
			continue;
		case CNSS_FS_READY:
			seq_puts(s, "FS READY");
			continue;
		case CNSS_DRIVER_REGISTERED:
			seq_puts(s, "DRIVER REGISTERED");
			continue;
		case CNSS_POWER_OFF:
			seq_puts(s, "POWER OFF");
			continue;
		}

		seq_printf(s, "UNKNOWN-%d", i);
	}
	seq_puts(s, ")\n");

	return 0;
}

static int cnss_stats_show_gpio_state(struct seq_file *s,
				      struct cnss_plat_data *plat_priv)
{
	seq_printf(s, "\nHost SOL: %d", cnss_get_host_sol_value(plat_priv));
	seq_printf(s, "\nDev SOL: %d", cnss_get_dev_sol_value(plat_priv));

	return 0;
}

static int cnss_stats_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *plat_priv = s->private;

	cnss_stats_show_state(s, plat_priv);
	cnss_stats_show_gpio_state(s, plat_priv);

	return 0;
}

static int cnss_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_stats_show, inode->i_private);
}

static const struct file_operations cnss_stats_fops = {
	.read		= seq_read,
	.release	= single_release,
	.open		= cnss_stats_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static ssize_t cnss_dev_boot_debug_write(struct file *fp,
					 const char __user *user_buf,
					 size_t count, loff_t *off)
{
	struct cnss_plat_data *plat_priv =
		((struct seq_file *)fp->private_data)->private;
	struct cnss_pci_data *pci_priv;
	char buf[64];
	char *cmd;
	unsigned int len = 0;
	char *sptr, *token;
	const char *delim = " ";
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;
	cmd = token;
	cnss_pr_dbg("Received dev_boot debug command: %s\n", cmd);

	if (sysfs_streq(cmd, "on")) {
		ret = cnss_power_on_device(plat_priv, false);
	} else if (sysfs_streq(cmd, "off")) {
		cnss_power_off_device(plat_priv);
	} else if (sysfs_streq(cmd, "enumerate")) {
		ret = cnss_pci_init(plat_priv);
	} else if (sysfs_streq(cmd, "powerup")) {
		set_bit(CNSS_DRIVER_DEBUG, &plat_priv->driver_state);
		ret = cnss_driver_event_post(plat_priv,
					     CNSS_DRIVER_EVENT_POWER_UP,
					     CNSS_EVENT_SYNC, NULL);
	} else if (sysfs_streq(cmd, "shutdown")) {
		ret = cnss_driver_event_post(plat_priv,
					     CNSS_DRIVER_EVENT_POWER_DOWN,
					     0, NULL);
		clear_bit(CNSS_DRIVER_DEBUG, &plat_priv->driver_state);
	} else if (sysfs_streq(cmd, "assert_host_sol")) {
		pci_priv = plat_priv->bus_priv;
		cnss_auto_resume(&pci_priv->pci_dev->dev);
		ret = cnss_set_host_sol_value(plat_priv, 1);
	} else if (sysfs_streq(cmd, "deassert_host_sol")) {
		ret = cnss_set_host_sol_value(plat_priv, 0);
	} else if (sysfs_streq(cmd, "pdc_update")) {
		if (!sptr)
			return -EINVAL;
		ret = cnss_aop_send_msg(plat_priv, sptr);
	} else if (sysfs_streq(cmd, "dev_check")) {
		cnss_wlan_hw_disable_check(plat_priv);
	} else if (sysfs_streq(cmd, "dev_enable")) {
		cnss_wlan_hw_enable();
	} else {
		pci_priv = plat_priv->bus_priv;
		if (!pci_priv)
			return -ENODEV;

		if (sysfs_streq(cmd, "download")) {
			set_bit(CNSS_DRIVER_DEBUG, &plat_priv->driver_state);
			ret = cnss_pci_start_mhi(pci_priv);
		} else if (sysfs_streq(cmd, "linkup")) {
			ret = cnss_resume_pci_link(pci_priv);
		} else if (sysfs_streq(cmd, "linkdown")) {
			ret = cnss_suspend_pci_link(pci_priv);
		} else if (sysfs_streq(cmd, "assert")) {
			cnss_pr_info("FW Assert triggered for debug\n");
			ret = cnss_force_fw_assert(&pci_priv->pci_dev->dev);
		} else if (sysfs_streq(cmd, "set_cbc_done")) {
			cnss_pr_dbg("Force set cold boot cal done status\n");
			set_bit(CNSS_COLD_BOOT_CAL_DONE,
				&plat_priv->driver_state);
		} else {
			cnss_pr_err("Device boot debugfs command is invalid\n");
			ret = -EINVAL;
		}
	}

	if (ret < 0)
		return ret;

	return count;
}

static int cnss_dev_boot_debug_show(struct seq_file *s, void *data)
{
	seq_puts(s, "\nUsage: echo <action> > <debugfs_path>/cnss/dev_boot\n");
	seq_puts(s, "<action> can be one of below:\n");
	seq_puts(s, "on: turn on device power, assert WLAN_EN\n");
	seq_puts(s, "off: de-assert WLAN_EN, turn off device power\n");
	seq_puts(s, "enumerate: de-assert PERST, enumerate PCIe\n");
	seq_puts(s, "download: download FW and do QMI handshake with FW\n");
	seq_puts(s, "linkup: bring up PCIe link\n");
	seq_puts(s, "linkdown: bring down PCIe link\n");
	seq_puts(s, "powerup: full power on sequence to boot device, download FW and do QMI handshake with FW\n");
	seq_puts(s, "shutdown: full power off sequence to shutdown device\n");
	seq_puts(s, "assert: trigger firmware assert\n");
	seq_puts(s, "set_cbc_done: Set cold boot calibration done status\n");
	seq_puts(s, "\npdc_update usage:");
	seq_puts(s, "1. echo pdc_update {class: wlan_pdc ss: <pdc_ss>, res: <vreg>.<mode>, <seq>: <val>} > <debugfs_path>/cnss/dev_boot\n");
	seq_puts(s, "2. echo pdc_update {class: wlan_pdc ss: <pdc_ss>, res: pdc, enable: <val>} > <debugfs_path>/cnss/dev_boot\n");
	seq_puts(s, "assert_host_sol: Assert host sol\n");
	seq_puts(s, "deassert_host_sol: Deassert host sol\n");
	seq_puts(s, "dev_check: Check whether HW is disabled or not\n");
	seq_puts(s, "dev_enable: Enable HW\n");

	return 0;
}

static int cnss_dev_boot_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_dev_boot_debug_show, inode->i_private);
}

static const struct file_operations cnss_dev_boot_debug_fops = {
	.read		= seq_read,
	.write		= cnss_dev_boot_debug_write,
	.release	= single_release,
	.open		= cnss_dev_boot_debug_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static int cnss_reg_read_debug_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *plat_priv = s->private;

	mutex_lock(&plat_priv->dev_lock);
	if (!plat_priv->diag_reg_read_buf) {
		seq_puts(s, "\nUsage: echo <mem_type> <offset> <data_len> > <debugfs_path>/cnss/reg_read\n");
		seq_puts(s, "Use mem_type = 0xff for register read by IO access, data_len will be ignored\n");
		seq_puts(s, "Use mem_type = 0xfe for register read by raw IO access which skips sanity checks, data_len will be ignored\n");
		seq_puts(s, "Use other mem_type for register read by QMI\n");
		mutex_unlock(&plat_priv->dev_lock);
		return 0;
	}

	seq_printf(s, "\nRegister read, address: 0x%x memory type: 0x%x length: 0x%x\n\n",
		   plat_priv->diag_reg_read_addr,
		   plat_priv->diag_reg_read_mem_type,
		   plat_priv->diag_reg_read_len);

	seq_hex_dump(s, "", DUMP_PREFIX_OFFSET, 32, 4,
		     plat_priv->diag_reg_read_buf,
		     plat_priv->diag_reg_read_len, false);

	plat_priv->diag_reg_read_len = 0;
	kfree(plat_priv->diag_reg_read_buf);
	plat_priv->diag_reg_read_buf = NULL;
	mutex_unlock(&plat_priv->dev_lock);

	return 0;
}

static ssize_t cnss_reg_read_debug_write(struct file *fp,
					 const char __user *user_buf,
					 size_t count, loff_t *off)
{
	struct cnss_plat_data *plat_priv =
		((struct seq_file *)fp->private_data)->private;
	char buf[64];
	char *sptr, *token;
	unsigned int len = 0;
	u32 reg_offset, mem_type;
	u32 data_len = 0, reg_val = 0;
	u8 *reg_buf = NULL;
	const char *delim = " ";
	int ret = 0;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &mem_type))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_offset))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (kstrtou32(token, 0, &data_len))
		return -EINVAL;

	if (mem_type == MMIO_REG_ACCESS_MEM_TYPE ||
	    mem_type == MMIO_REG_RAW_ACCESS_MEM_TYPE) {
		ret = cnss_bus_debug_reg_read(plat_priv, reg_offset, &reg_val,
					      mem_type ==
					      MMIO_REG_RAW_ACCESS_MEM_TYPE);
		if (ret)
			return ret;
		cnss_pr_dbg("Read 0x%x from register offset 0x%x\n", reg_val,
			    reg_offset);
		return count;
	}

	if (!test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
		cnss_pr_err("Firmware is not ready yet\n");
		return -EINVAL;
	}

	mutex_lock(&plat_priv->dev_lock);
	kfree(plat_priv->diag_reg_read_buf);
	plat_priv->diag_reg_read_buf = NULL;

	reg_buf = kzalloc(data_len, GFP_KERNEL);
	if (!reg_buf) {
		mutex_unlock(&plat_priv->dev_lock);
		return -ENOMEM;
	}

	ret = cnss_wlfw_athdiag_read_send_sync(plat_priv, reg_offset,
					       mem_type, data_len,
					       reg_buf);
	if (ret) {
		kfree(reg_buf);
		mutex_unlock(&plat_priv->dev_lock);
		return ret;
	}

	plat_priv->diag_reg_read_addr = reg_offset;
	plat_priv->diag_reg_read_mem_type = mem_type;
	plat_priv->diag_reg_read_len = data_len;
	plat_priv->diag_reg_read_buf = reg_buf;
	mutex_unlock(&plat_priv->dev_lock);

	return count;
}

static int cnss_reg_read_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_reg_read_debug_show, inode->i_private);
}

static const struct file_operations cnss_reg_read_debug_fops = {
	.read		= seq_read,
	.write		= cnss_reg_read_debug_write,
	.open		= cnss_reg_read_debug_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static int cnss_reg_write_debug_show(struct seq_file *s, void *data)
{
	seq_puts(s, "\nUsage: echo <mem_type> <offset> <reg_val> > <debugfs_path>/cnss/reg_write\n");
	seq_puts(s, "Use mem_type = 0xff for register write by IO access\n");
	seq_puts(s, "Use mem_type = 0xfe for register write by raw IO access which skips sanity checks\n");
	seq_puts(s, "Use other mem_type for register write by QMI\n");

	return 0;
}

static ssize_t cnss_reg_write_debug_write(struct file *fp,
					  const char __user *user_buf,
					  size_t count, loff_t *off)
{
	struct cnss_plat_data *plat_priv =
		((struct seq_file *)fp->private_data)->private;
	char buf[64];
	char *sptr, *token;
	unsigned int len = 0;
	u32 reg_offset, mem_type, reg_val;
	const char *delim = " ";
	int ret = 0;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &mem_type))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_offset))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_val))
		return -EINVAL;

	if (mem_type == MMIO_REG_ACCESS_MEM_TYPE ||
	    mem_type == MMIO_REG_RAW_ACCESS_MEM_TYPE) {
		ret = cnss_bus_debug_reg_write(plat_priv, reg_offset, reg_val,
					       mem_type ==
					       MMIO_REG_RAW_ACCESS_MEM_TYPE);
		if (ret)
			return ret;
		cnss_pr_dbg("Wrote 0x%x to register offset 0x%x\n", reg_val,
			    reg_offset);
		return count;
	}

	if (!test_bit(CNSS_FW_READY, &plat_priv->driver_state)) {
		cnss_pr_err("Firmware is not ready yet\n");
		return -EINVAL;
	}

	ret = cnss_wlfw_athdiag_write_send_sync(plat_priv, reg_offset, mem_type,
						sizeof(u32),
						(u8 *)&reg_val);
	if (ret)
		return ret;

	return count;
}

static int cnss_reg_write_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_reg_write_debug_show, inode->i_private);
}

static const struct file_operations cnss_reg_write_debug_fops = {
	.read		= seq_read,
	.write		= cnss_reg_write_debug_write,
	.open		= cnss_reg_write_debug_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static ssize_t cnss_runtime_pm_debug_write(struct file *fp,
					   const char __user *user_buf,
					   size_t count, loff_t *off)
{
	struct cnss_plat_data *plat_priv =
		((struct seq_file *)fp->private_data)->private;
	struct cnss_pci_data *pci_priv;
	char buf[64];
	char *cmd;
	unsigned int len = 0;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	pci_priv = plat_priv->bus_priv;
	if (!pci_priv)
		return -ENODEV;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	cmd = buf;

	cnss_pr_dbg("Received runtime_pm debug command: %s\n", cmd);

	if (sysfs_streq(cmd, "usage_count")) {
		cnss_pci_pm_runtime_show_usage_count(pci_priv);
	} else if (sysfs_streq(cmd, "request_resume")) {
		ret = cnss_pci_pm_request_resume(pci_priv);
	} else if (sysfs_streq(cmd, "resume")) {
		ret = cnss_pci_pm_runtime_resume(pci_priv);
	} else if (sysfs_streq(cmd, "get")) {
		ret = cnss_pci_pm_runtime_get(pci_priv, RTPM_ID_CNSS);
	} else if (sysfs_streq(cmd, "get_noresume")) {
		cnss_pci_pm_runtime_get_noresume(pci_priv, RTPM_ID_CNSS);
	} else if (sysfs_streq(cmd, "put_autosuspend")) {
		ret = cnss_pci_pm_runtime_put_autosuspend(pci_priv,
							  RTPM_ID_CNSS);
	} else if (sysfs_streq(cmd, "put_noidle")) {
		cnss_pci_pm_runtime_put_noidle(pci_priv, RTPM_ID_CNSS);
	} else if (sysfs_streq(cmd, "mark_last_busy")) {
		cnss_pci_pm_runtime_mark_last_busy(pci_priv);
	} else if (sysfs_streq(cmd, "resume_bus")) {
		cnss_pci_resume_bus(pci_priv);
	} else if (sysfs_streq(cmd, "suspend_bus")) {
		cnss_pci_suspend_bus(pci_priv);
	} else {
		cnss_pr_err("Runtime PM debugfs command is invalid\n");
		ret = -EINVAL;
	}

	if (ret < 0)
		return ret;

	return count;
}

static int cnss_runtime_pm_debug_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *plat_priv = s->private;
	struct cnss_pci_data *pci_priv;
	int i;

	if (!plat_priv)
		return -ENODEV;

	pci_priv = plat_priv->bus_priv;
	if (!pci_priv)
		return -ENODEV;

	seq_puts(s, "\nUsage: echo <action> > <debugfs_path>/cnss/runtime_pm\n");
	seq_puts(s, "<action> can be one of below:\n");
	seq_puts(s, "usage_count: get runtime PM usage count\n");
	seq_puts(s, "reques_resume: do async runtime PM resume\n");
	seq_puts(s, "resume: do sync runtime PM resume\n");
	seq_puts(s, "get: do runtime PM get\n");
	seq_puts(s, "get_noresume: do runtime PM get noresume\n");
	seq_puts(s, "put_noidle: do runtime PM put noidle\n");
	seq_puts(s, "put_autosuspend: do runtime PM put autosuspend\n");
	seq_puts(s, "mark_last_busy: do runtime PM mark last busy\n");
	seq_puts(s, "resume_bus: do bus resume only\n");
	seq_puts(s, "suspend_bus: do bus suspend only\n");

	seq_puts(s, "\nStats:\n");
	seq_printf(s, "%s: %u\n", "get count",
		   atomic_read(&pci_priv->pm_stats.runtime_get));
	seq_printf(s, "%s: %u\n", "put count",
		   atomic_read(&pci_priv->pm_stats.runtime_put));
	seq_printf(s, "%-10s%-10s%-10s%-15s%-15s\n",
		   "id:", "get",  "put", "get time(us)", "put time(us)");
	for (i = 0; i < RTPM_ID_MAX; i++) {
		seq_printf(s, "%d%-9s", i, ":");
		seq_printf(s, "%-10d",
			   atomic_read(&pci_priv->pm_stats.runtime_get_id[i]));
		seq_printf(s, "%-10d",
			   atomic_read(&pci_priv->pm_stats.runtime_put_id[i]));
		seq_printf(s, "%-15llu",
			   pci_priv->pm_stats.runtime_get_timestamp_id[i]);
		seq_printf(s, "%-15llu\n",
			   pci_priv->pm_stats.runtime_put_timestamp_id[i]);
	}

	return 0;
}

static int cnss_runtime_pm_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, cnss_runtime_pm_debug_show, inode->i_private);
}

static const struct file_operations cnss_runtime_pm_debug_fops = {
	.read		= seq_read,
	.write		= cnss_runtime_pm_debug_write,
	.open		= cnss_runtime_pm_debug_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static int process_drv(struct cnss_plat_data *plat_priv, bool enabled)
{
	if (test_bit(CNSS_QMI_WLFW_CONNECTED, &plat_priv->driver_state)) {
		cnss_pr_err("DRV cmd must be used before QMI ready\n");
		return -EINVAL;
	}

	enabled ? cnss_set_feature_list(plat_priv, CNSS_DRV_SUPPORT_V01) :
		  cnss_clear_feature_list(plat_priv, CNSS_DRV_SUPPORT_V01);

	cnss_pr_info("%s DRV suspend\n", enabled ? "enable" : "disable");
	return 0;
}

static int process_quirks(struct cnss_plat_data *plat_priv, u32 val)
{
	enum cnss_debug_quirks i;
	int ret = 0;
	unsigned long state;
	unsigned long quirks = 0;

	for (i = 0, state = val; i < QUIRK_MAX_VALUE; state >>= 1, i++) {
		switch (i) {
		case DISABLE_DRV:
			ret = process_drv(plat_priv, !(state & 0x1));
			if (!ret)
				quirks |= (state & 0x1) << i;
			continue;
		default:
			quirks |= (state & 0x1) << i;
			continue;
		}
	}

	plat_priv->ctrl_params.quirks = quirks;
	return 0;
}

static ssize_t cnss_control_params_debug_write(struct file *fp,
					       const char __user *user_buf,
					       size_t count, loff_t *off)
{
	struct cnss_plat_data *plat_priv =
		((struct seq_file *)fp->private_data)->private;
	char buf[64];
	char *sptr, *token;
	char *cmd;
	u32 val;
	unsigned int len = 0;
	const char *delim = " ";

	if (!plat_priv)
		return -ENODEV;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;
	if (!sptr)
		return -EINVAL;
	cmd = token;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &val))
		return -EINVAL;

	if (strcmp(cmd, "quirks") == 0)
		process_quirks(plat_priv, val);
	else if (strcmp(cmd, "mhi_timeout") == 0)
		plat_priv->ctrl_params.mhi_timeout = val;
	else if (strcmp(cmd, "mhi_m2_timeout") == 0)
		plat_priv->ctrl_params.mhi_m2_timeout = val;
	else if (strcmp(cmd, "qmi_timeout") == 0)
		plat_priv->ctrl_params.qmi_timeout = val;
	else if (strcmp(cmd, "bdf_type") == 0)
		plat_priv->ctrl_params.bdf_type = val;
	else if (strcmp(cmd, "time_sync_period") == 0)
		plat_priv->ctrl_params.time_sync_period = val;
	else if (strcmp(cmd, "kern_log_level") == 0) {
		if (val < MAX_LOG)
			cnss_kernel_log_level = val;
	} else if (strcmp(cmd, "ipc_log_level") == 0) {
		return cnss_set_ipc_log_level(val) ? -EINVAL : count;
	} else
		return -EINVAL;

	return count;
}

static int cnss_show_quirks_state(struct seq_file *s,
				  struct cnss_plat_data *plat_priv)
{
	enum cnss_debug_quirks i;
	int skip = 0;
	unsigned long state;

	seq_printf(s, "quirks: 0x%lx (", plat_priv->ctrl_params.quirks);
	for (i = 0, state = plat_priv->ctrl_params.quirks;
	     state != 0; state >>= 1, i++) {
		if (!(state & 0x1))
			continue;
		if (skip++)
			seq_puts(s, " | ");

		switch (i) {
		case LINK_DOWN_SELF_RECOVERY:
			seq_puts(s, "LINK_DOWN_SELF_RECOVERY");
			continue;
		case SKIP_DEVICE_BOOT:
			seq_puts(s, "SKIP_DEVICE_BOOT");
			continue;
		case USE_CORE_ONLY_FW:
			seq_puts(s, "USE_CORE_ONLY_FW");
			continue;
		case SKIP_RECOVERY:
			seq_puts(s, "SKIP_RECOVERY");
			continue;
		case QMI_BYPASS:
			seq_puts(s, "QMI_BYPASS");
			continue;
		case ENABLE_WALTEST:
			seq_puts(s, "WALTEST");
			continue;
		case ENABLE_PCI_LINK_DOWN_PANIC:
			seq_puts(s, "PCI_LINK_DOWN_PANIC");
			continue;
		case FBC_BYPASS:
			seq_puts(s, "FBC_BYPASS");
			continue;
		case ENABLE_DAEMON_SUPPORT:
			seq_puts(s, "DAEMON_SUPPORT");
			continue;
		case DISABLE_DRV:
			seq_puts(s, "DISABLE_DRV");
			continue;
		case DISABLE_IO_COHERENCY:
			seq_puts(s, "DISABLE_IO_COHERENCY");
			continue;
		case IGNORE_PCI_LINK_FAILURE:
			seq_puts(s, "IGNORE_PCI_LINK_FAILURE");
			continue;
		case DISABLE_TIME_SYNC:
			seq_puts(s, "DISABLE_TIME_SYNC");
			continue;
		case FORCE_ONE_MSI:
			seq_puts(s, "FORCE_ONE_MSI");
			continue;
		default:
			continue;
		}
	}
	seq_puts(s, ")\n");
	return 0;
}

static int cnss_control_params_debug_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *cnss_priv = s->private;
	u32 ipc_log_level;

	seq_puts(s, "\nUsage: echo <params_name> <value> > <debugfs_path>/cnss/control_params\n");
	seq_puts(s, "<params_name> can be one of below:\n");
	seq_puts(s, "quirks: Debug quirks for driver\n");
	seq_puts(s, "mhi_timeout: Timeout for MHI operation in milliseconds\n");
	seq_puts(s, "qmi_timeout: Timeout for QMI message in milliseconds\n");
	seq_puts(s, "bdf_type: Type of board data file to be downloaded\n");
	seq_puts(s, "time_sync_period: Time period to do time sync with device in milliseconds\n");

	seq_puts(s, "\nCurrent value:\n");
	cnss_show_quirks_state(s, cnss_priv);
	seq_printf(s, "mhi_timeout: %u\n", cnss_priv->ctrl_params.mhi_timeout);
	seq_printf(s, "mhi_m2_timeout: %u\n",
		   cnss_priv->ctrl_params.mhi_m2_timeout);
	seq_printf(s, "qmi_timeout: %u\n", cnss_priv->ctrl_params.qmi_timeout);
	seq_printf(s, "bdf_type: %u\n", cnss_priv->ctrl_params.bdf_type);
	seq_printf(s, "time_sync_period: %u\n",
		   cnss_priv->ctrl_params.time_sync_period);
	seq_printf(s, "kern_log_level: %u\n", cnss_kernel_log_level);

	ipc_log_level = cnss_get_ipc_log_level();
	if (ipc_log_level != MAX_LOG)
		seq_printf(s, "ipc_log_level: %u\n", ipc_log_level);

	return 0;
}

static int cnss_control_params_debug_open(struct inode *inode,
					  struct file *file)
{
	return single_open(file, cnss_control_params_debug_show,
			   inode->i_private);
}

#ifdef OPLUS_FEATURE_WIFI_DCS_SWITCH
//Add for wifi switch monitor
static int oplus_cnss_switch_debug_show(struct seq_file *s, void *data)
{
	seq_puts(s, "\nUsage: echo <params_name>=<value> > /sys/kernel/debug/cnss/oplus_cnss_switch_debug\n");
	seq_puts(s, "<params_name> can be one of below:\n");
	seq_puts(s, "debug_cnss: debug cnss error file flag test\n");
	seq_puts(s, "idle_shutdown: idle shut down flag test\n\n");
	seq_puts(s, "firmware_ready: firmware_ready flag test\n");
	seq_puts(s, "pcie_link_down: pcie status flag test\n");
	return 0;
}

static ssize_t oplus_cnss_switch_debug_write(struct file *fp,
					  const char __user *user_buf,
					  size_t count, loff_t *off)
{
	struct cnss_plat_data *plat_priv =
		((struct seq_file *)fp->private_data)->private;
	char buf[64];
	char *sptr, *token;
	//value for the cmd：debug_cnss, debug error log msgs
	char *cmd, *value;
	//val for other cmd
	u32 val;

	unsigned int len = 0;
	const char *delim = " ";

	if (!plat_priv)
		return -ENODEV;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	cmd = strsep(&sptr, delim);
	if (!cmd)
		return -EINVAL;
	if (!sptr)
		return -EINVAL;
	value = sptr;

	//for cmd debug_cnss
	if (strcmp(cmd, "debug_cnss") == 0) {
		cnss_pr_err(value);
	}

	//for other cmd
	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &val))
		return -EINVAL;

	if (strcmp(cmd, "idle_shutdown") == 0) {
		if (val == 1) {
			cnss_pr_err("idle_shutdown true");
			idle_shutdown = true;
		} else {
			cnss_pr_err("idle_shutdown falsa");
			idle_shutdown = false;
		}
	} else if (strcmp(cmd, "firmware_ready") == 0) {
		if (val == 1) {
			set_bit(CNSS_FW_READY, &plat_priv->driver_state);
		} else {
			clear_bit(CNSS_FW_READY, &plat_priv->driver_state);
		}
	} else if (strcmp(cmd, "pcie_link_down") == 0) {
		if (val == 1) {
			set_bit(CNSS_PCIE_LINK_DOWN,&plat_priv->pcieLinkDown);
		} else {
			clear_bit(CNSS_PCIE_LINK_DOWN,&plat_priv->pcieLinkDown);
		}
	} else
		return -EINVAL;

	return count;
}

static int oplus_cnss_switch_debug_open(struct inode *inode,
					  struct file *file)
{
	return single_open(file, oplus_cnss_switch_debug_show,
			   inode->i_private);
}

static const struct file_operations oplus_cnss_switch_debug_fops = {
	.read = seq_read,
	.write = oplus_cnss_switch_debug_write,
	.open = oplus_cnss_switch_debug_open,
	.owner = THIS_MODULE,
	.llseek = seq_lseek,
};
#endif /* OPLUS_FEATURE_WIFI_DCS_SWITCH*/

static const struct file_operations cnss_control_params_debug_fops = {
	.read = seq_read,
	.write = cnss_control_params_debug_write,
	.open = cnss_control_params_debug_open,
	.owner = THIS_MODULE,
	.llseek = seq_lseek,
};

static ssize_t cnss_dynamic_feature_write(struct file *fp,
					  const char __user *user_buf,
					  size_t count, loff_t *off)
{
	struct cnss_plat_data *plat_priv =
		((struct seq_file *)fp->private_data)->private;
	int ret = 0;
	u64 val;

	ret = kstrtou64_from_user(user_buf, count, 0, &val);
	if (ret)
		return ret;

	plat_priv->dynamic_feature = val;
	ret = cnss_wlfw_dynamic_feature_mask_send_sync(plat_priv);
	if (ret < 0)
		return ret;

	return count;
}

static int cnss_dynamic_feature_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *cnss_priv = s->private;

	seq_printf(s, "dynamic_feature: 0x%llx\n", cnss_priv->dynamic_feature);

	return 0;
}

static int cnss_dynamic_feature_open(struct inode *inode,
				     struct file *file)
{
	return single_open(file, cnss_dynamic_feature_show,
			   inode->i_private);
}

static const struct file_operations cnss_dynamic_feature_fops = {
	.read = seq_read,
	.write = cnss_dynamic_feature_write,
	.open = cnss_dynamic_feature_open,
	.owner = THIS_MODULE,
	.llseek = seq_lseek,
};

static int cnss_smmu_fault_timestamp_show(struct seq_file *s, void *data)
{
	struct cnss_plat_data *plat_priv = s->private;
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;

	if (!pci_priv)
		return -ENODEV;

	seq_printf(s, "smmu irq cb entry timestamp : %llu ns\n",
		   pci_priv->smmu_fault_timestamp[SMMU_CB_ENTRY]);
	seq_printf(s, "smmu irq cb before doorbell ring timestamp : %llu ns\n",
		   pci_priv->smmu_fault_timestamp[SMMU_CB_DOORBELL_RING]);
	seq_printf(s, "smmu irq cb after doorbell ring timestamp : %llu ns\n",
		   pci_priv->smmu_fault_timestamp[SMMU_CB_EXIT]);

	return 0;
}

static int cnss_smmu_fault_timestamp_open(struct inode *inode,
					  struct file *file)
{
	return single_open(file, cnss_smmu_fault_timestamp_show,
			   inode->i_private);
}

static const struct file_operations cnss_smmu_fault_timestamp_fops = {
	.read = seq_read,
	.release = single_release,
	.open = cnss_smmu_fault_timestamp_open,
	.owner = THIS_MODULE,
	.llseek = seq_lseek,
};

#ifdef CONFIG_DEBUG_FS
#ifdef CONFIG_CNSS2_DEBUG
static int cnss_create_debug_only_node(struct cnss_plat_data *plat_priv)
{
	struct dentry *root_dentry = plat_priv->root_dentry;

	debugfs_create_file("dev_boot", 0600, root_dentry, plat_priv,
			    &cnss_dev_boot_debug_fops);
	debugfs_create_file("reg_read", 0600, root_dentry, plat_priv,
			    &cnss_reg_read_debug_fops);
	debugfs_create_file("reg_write", 0600, root_dentry, plat_priv,
			    &cnss_reg_write_debug_fops);
	debugfs_create_file("runtime_pm", 0600, root_dentry, plat_priv,
			    &cnss_runtime_pm_debug_fops);
	debugfs_create_file("control_params", 0600, root_dentry, plat_priv,
			    &cnss_control_params_debug_fops);
	debugfs_create_file("dynamic_feature", 0600, root_dentry, plat_priv,
			    &cnss_dynamic_feature_fops);
	debugfs_create_file("cnss_smmu_fault_timestamp", 0600, root_dentry,
			    plat_priv, &cnss_smmu_fault_timestamp_fops);
	return 0;
}
#else
static int cnss_create_debug_only_node(struct cnss_plat_data *plat_priv)
{
	return 0;
}
#endif

int cnss_debugfs_create(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct dentry *root_dentry;
	char name[CNSS_FS_NAME_SIZE];

	if (cnss_is_dual_wlan_enabled())
		snprintf(name, CNSS_FS_NAME_SIZE, CNSS_FS_NAME "_%d",
			 plat_priv->plat_idx);
	else
		snprintf(name, CNSS_FS_NAME_SIZE, CNSS_FS_NAME);

	root_dentry = debugfs_create_dir(name, 0);
	if (IS_ERR(root_dentry)) {
		ret = PTR_ERR(root_dentry);
		cnss_pr_err("Unable to create debugfs %d\n", ret);
		goto out;
	}

	plat_priv->root_dentry = root_dentry;

	debugfs_create_file("pin_connect_result", 0644, root_dentry, plat_priv,
			    &cnss_pin_connect_fops);
	debugfs_create_file("stats", 0644, root_dentry, plat_priv,
			    &cnss_stats_fops);

	cnss_create_debug_only_node(plat_priv);
	#ifdef OPLUS_FEATURE_WIFI_DCS_SWITCH
	//Add for wifi switch monitor
		debugfs_create_file("oplus_cnss_switch_debug", 0600, root_dentry,
				    plat_priv, &oplus_cnss_switch_debug_fops);
	#endif /* OPLUS_FEATURE_WIFI_DCS_SWITCH*/

out:
	return ret;
}

void cnss_debugfs_destroy(struct cnss_plat_data *plat_priv)
{
	debugfs_remove_recursive(plat_priv->root_dentry);
}
#else
int cnss_debugfs_create(struct cnss_plat_data *plat_priv)
{
	plat_priv->root_dentry = NULL;
	return 0;
}

void cnss_debugfs_destroy(struct cnss_plat_data *plat_priv)
{
}
#endif

#ifdef OPLUS_FEATURE_WIFI_DCS_SWITCH
//Add for wifi switch monitor
struct cel_list *cel_head = NULL;
struct cel_list *cel_tail = NULL;
int cel_list_length = 0;
static DEFINE_SPINLOCK(cel_lock);

static u64 oplus_conn_get_local_seconds(void)
{
	u64 sec;
	sec = ktime_get_seconds();
	return sec;
}

void oplus_cnss_error_log_add(char *fmt, ...)
{
	char buffer[CNSS_ERROR_SIZE];
	struct cel_list *new_cel_list = NULL;
	va_list args;
	//char* time_str;
	u64 time_str;
	unsigned long flags = 0;

	va_start(args, fmt);
	spin_lock_irqsave(&cel_lock, flags);

	vsnprintf(buffer, CNSS_ERROR_SIZE, fmt, args);

	if (cel_list_length >= MAX_CNSS_ERROE_LIST_LENGTH) {
		struct cel_list *old_cel_list = cel_head;
		cel_head = old_cel_list->next;
		kfree(old_cel_list);
		cel_list_length--;
		cnss_pr_dbg("MAX_CNSS_ERROE_LIST_LENGTH found\n");
	}

	time_str = oplus_conn_get_local_seconds();
	new_cel_list = kmalloc(sizeof(struct cel_list), GFP_ATOMIC);
	strncpy(new_cel_list->message, buffer, CNSS_ERROR_SIZE);
	new_cel_list->time_s = time_str;
	//printf("oplus_cnss_error_log_add dt=%s,%s\n",time_str,new_cel_list->dt);
	new_cel_list->message[CNSS_ERROR_SIZE-1] = '\0';
	new_cel_list->next = NULL;

	if (cel_head == NULL) {
		cel_head = new_cel_list;
		cel_tail = new_cel_list;
	} else {
		cel_tail->next = new_cel_list;
		cel_tail = new_cel_list;
	}
	cnss_pr_dbg("add one :cel_list_length %d---->%llu---%s\n", cel_list_length,new_cel_list->time_s,new_cel_list->message);

	cel_list_length++;
	spin_unlock_irqrestore(&cel_lock, flags);
	va_end(args);
}

ssize_t icnss_show_cnss_debug(struct device_driver *driver, char *buf);

ssize_t icnss_show_cnss_debug(struct device_driver *driver, char *buf)
{
	struct cel_list *cur_cnssErrorLog;
	int ret = 0;
	int length = 0;
	//int i=0;
	int temp_length = sizeof(struct cel_list);
	char temp[CNSS_STRUCT_ITEM_LENGTH];
	unsigned long flags = 0;

	spin_lock_irqsave(&cel_lock, flags);
	//cnss_pr_dbg("temp_length %d",temp_length);
	cur_cnssErrorLog = cel_head;
	if (cur_cnssErrorLog == NULL){
		cnss_pr_dbg("Show icnss_show_cnss_debug cnssErrorLog is NULL!\n");
		ret = sprintf(buf,"%s","good");
		spin_unlock_irqrestore(&cel_lock, flags);
		return ret;
	}

	while ((cur_cnssErrorLog != NULL) && ((length + temp_length +1) < MAX_BUFFER_SIZE)){
		ret = snprintf(temp, temp_length,"[%llu]%s\n", cur_cnssErrorLog->time_s,cur_cnssErrorLog->message);
		length = length + ret;
		strncat(buf, temp, strlen(temp));
		//cnss_pr_dbg("Show :item[%d],len=%d,buffer-->%llu,%s\n", i++,strlen(temp),cur_cnssErrorLog->time_s,cur_cnssErrorLog->message);
		cur_cnssErrorLog = cur_cnssErrorLog->next;
	}
	buf[length - 1] = '\0';
	cnss_pr_dbg("Show--buffer all --> :buf[%d]--> %s\n",length,buf);
	spin_unlock_irqrestore(&cel_lock, flags);
	return length;
}

static void oplus_free_cnss_error_logs(void)
{
	struct cel_list *cur_cel_list;
	unsigned long flags = 0;

	spin_lock_irqsave(&cel_lock, flags);

	cur_cel_list = cel_head;
	while (cur_cel_list != NULL) {
		struct cel_list *next_cel_list = cur_cel_list->next;
		kfree(cur_cel_list);
		cur_cel_list = next_cel_list;
	}
	spin_unlock_irqrestore(&cel_lock, flags);
}
#endif /* OPLUS_FEATURE_WIFI_DCS_SWITCH */

#if IS_ENABLED(CONFIG_IPC_LOGGING)
void cnss_debug_ipc_log_print(void *log_ctx, char *process, const char *fn,
			      enum log_level kern_log_level,
			      enum log_level ipc_log_level, char *fmt, ...)
{
	struct va_format vaf;
	va_list va_args;

	va_start(va_args, fmt);
	vaf.fmt = fmt;
	vaf.va = &va_args;

	if (kern_log_level <= cnss_kernel_log_level) {
		switch (kern_log_level) {
		case EMERG_LOG:
			pr_emerg("cnss: %pV", &vaf);
			break;
		case ALERT_LOG:
			pr_alert("cnss: %pV", &vaf);
			break;
		case CRIT_LOG:
			pr_crit("cnss: %pV", &vaf);
			break;
		case ERR_LOG:
			pr_err("cnss: %pV", &vaf);
			break;
		case WARNING_LOG:
			pr_warn("cnss: %pV", &vaf);
			break;
		case NOTICE_LOG:
			pr_notice("cnss: %pV", &vaf);
			break;
		case INFO_LOG:
			pr_info("cnss: %pV", &vaf);
			break;
		case DEBUG_LOG:
		case DEBUG_HI_LOG:
			pr_debug("cnss: %pV", &vaf);
			break;
		default:
			break;
		}
	}

	if (ipc_log_level <= cnss_ipc_log_level)
		ipc_log_string(log_ctx, "[%s] %s: %pV", process, fn, &vaf);

	va_end(va_args);
}

static int cnss_ipc_logging_init(void)
{
	cnss_ipc_log_context = ipc_log_context_create(CNSS_IPC_LOG_PAGES,
						      "cnss", 0);
	if (!cnss_ipc_log_context) {
		cnss_pr_err("Unable to create IPC log context\n");
		return -EINVAL;
	}

	cnss_ipc_log_long_context = ipc_log_context_create(CNSS_IPC_LOG_PAGES,
							   "cnss-long", 0);
	if (!cnss_ipc_log_long_context) {
		cnss_pr_err("Unable to create IPC long log context\n");
		ipc_log_context_destroy(cnss_ipc_log_context);
		return -EINVAL;
	}

	return 0;
}

static void cnss_ipc_logging_deinit(void)
{
	if (cnss_ipc_log_long_context) {
		ipc_log_context_destroy(cnss_ipc_log_long_context);
		cnss_ipc_log_long_context = NULL;
	}

	if (cnss_ipc_log_context) {
		ipc_log_context_destroy(cnss_ipc_log_context);
		cnss_ipc_log_context = NULL;
	}
}
#else
static int cnss_ipc_logging_init(void) { return 0; }
static void cnss_ipc_logging_deinit(void) {}
void cnss_debug_ipc_log_print(void *log_ctx, char *process, const char *fn,
			      enum log_level kern_log_level,
			      enum log_level ipc_log_level, char *fmt, ...)
{
	struct va_format vaf;
	va_list va_args;

	va_start(va_args, fmt);
	vaf.fmt = fmt;
	vaf.va = &va_args;

	if (kern_log_level <= cnss_kernel_log_level) {
		switch (kern_log_level) {
		case EMERG_LOG:
			pr_emerg("cnss: %pV", &vaf);
			break;
		case ALERT_LOG:
			pr_alert("cnss: %pV", &vaf);
			break;
		case CRIT_LOG:
			pr_crit("cnss: %pV", &vaf);
			break;
		case ERR_LOG:
			pr_err("cnss: %pV", &vaf);
			break;
		case WARNING_LOG:
			pr_warn("cnss: %pV", &vaf);
			break;
		case NOTICE_LOG:
			pr_notice("cnss: %pV", &vaf);
			break;
		case INFO_LOG:
			pr_info("cnss: %pV", &vaf);
			break;
		case DEBUG_LOG:
		case DEBUG_HI_LOG:
			pr_debug("cnss: %pV", &vaf);
			break;
		default:
			break;
		}
	}

	va_end(va_args);
}
#endif

int cnss_debug_init(void)
{
	return cnss_ipc_logging_init();
}

void cnss_debug_deinit(void)
{
	cnss_ipc_logging_deinit();

#ifdef OPLUS_FEATURE_WIFI_DCS_SWITCH
//Add for wifi switch monito
	oplus_free_cnss_error_logs();
#endif /* OPLUS_FEATURE_WIFI_DCS_SWITCH */
}
