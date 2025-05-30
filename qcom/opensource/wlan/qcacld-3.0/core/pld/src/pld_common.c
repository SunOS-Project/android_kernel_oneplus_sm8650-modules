/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
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

#define pr_fmt(fmt) "wlan_pld:%s:%d:: " fmt, __func__, __LINE__

#include <linux/printk.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/pm.h>

#ifdef CONFIG_PLD_SDIO_CNSS
#include <net/cnss.h>
#endif
#ifdef CONFIG_PLD_PCIE_CNSS
#ifdef CONFIG_CNSS_OUT_OF_TREE
#include "cnss2.h"
#else
#include <net/cnss2.h>
#endif
#endif

#ifdef CONFIG_CNSS_OUT_OF_TREE
#ifdef CONFIG_PLD_SNOC_ICNSS
#ifdef CONFIG_PLD_SNOC_ICNSS2
#include "icnss2.h"
#else
#include "icnss.h"
#endif
#endif
#ifdef CONFIG_PLD_IPCI_ICNSS
#include "icnss2.h"
#endif
#else
#ifdef CONFIG_PLD_SNOC_ICNSS
#ifdef CONFIG_PLD_SNOC_ICNSS2
#include <soc/qcom/icnss2.h>
#else
#include <soc/qcom/icnss.h>
#endif
#endif
#ifdef CONFIG_PLD_IPCI_ICNSS
#include <soc/qcom/icnss2.h>
#endif
#endif

#include "pld_pcie.h"
#include "pld_ipci.h"
#include "pld_pcie_fw_sim.h"
#include "pld_snoc_fw_sim.h"
#include "pld_snoc.h"
#include "pld_sdio.h"
#include "pld_usb.h"
#include "qwlan_version.h"

#define PLD_PCIE_REGISTERED BIT(0)
#define PLD_SNOC_REGISTERED BIT(1)
#define PLD_SDIO_REGISTERED BIT(2)
#define PLD_USB_REGISTERED BIT(3)
#define PLD_SNOC_FW_SIM_REGISTERED BIT(4)
#define PLD_PCIE_FW_SIM_REGISTERED BIT(5)
#define PLD_IPCI_REGISTERED BIT(6)

#define PLD_BUS_MASK 0xf

static struct pld_context *pld_ctx;

int pld_init(void)
{
	struct pld_context *pld_context;

	pld_context = kzalloc(sizeof(*pld_context), GFP_KERNEL);
	if (!pld_context)
		return -ENOMEM;

	spin_lock_init(&pld_context->pld_lock);

	INIT_LIST_HEAD(&pld_context->dev_list);

	pld_ctx = pld_context;

	return 0;
}

void pld_deinit(void)
{
	struct dev_node *dev_node;
	struct pld_context *pld_context;
	unsigned long flags;

	pld_context = pld_ctx;
	if (!pld_context) {
		pld_ctx = NULL;
		return;
	}

	spin_lock_irqsave(&pld_context->pld_lock, flags);
	while (!list_empty(&pld_context->dev_list)) {
		dev_node = list_first_entry(&pld_context->dev_list,
					    struct dev_node, list);
		list_del(&dev_node->list);
		kfree(dev_node);
	}
	spin_unlock_irqrestore(&pld_context->pld_lock, flags);

	kfree(pld_context);

	pld_ctx = NULL;
}

int pld_set_mode(u8 mode)
{
	if (!pld_ctx)
		return -ENOMEM;

	pld_ctx->mode = mode;
	return 0;
}

struct pld_context *pld_get_global_context(void)
{
	return pld_ctx;
}

int pld_add_dev(struct pld_context *pld_context,
		struct device *dev, struct device *ifdev,
		enum pld_bus_type type)
{
	unsigned long flags;
	struct dev_node *dev_node;

	dev_node = kzalloc(sizeof(*dev_node), GFP_KERNEL);
	if (!dev_node)
		return -ENOMEM;

	dev_node->dev = dev;
	dev_node->ifdev = ifdev;
	dev_node->bus_type = type;

	spin_lock_irqsave(&pld_context->pld_lock, flags);
	list_add_tail(&dev_node->list, &pld_context->dev_list);
	spin_unlock_irqrestore(&pld_context->pld_lock, flags);

	return 0;
}

void pld_del_dev(struct pld_context *pld_context,
		 struct device *dev)
{
	unsigned long flags;
	struct dev_node *dev_node, *tmp;

	spin_lock_irqsave(&pld_context->pld_lock, flags);
	list_for_each_entry_safe(dev_node, tmp, &pld_context->dev_list, list) {
		if (dev_node->dev == dev) {
			list_del(&dev_node->list);
			kfree(dev_node);
		}
	}
	spin_unlock_irqrestore(&pld_context->pld_lock, flags);
}

static struct dev_node *pld_get_dev_node(struct device *dev)
{
	struct pld_context *pld_context;
	struct dev_node *dev_node;
	unsigned long flags;

	pld_context = pld_get_global_context();

	if (!dev || !pld_context) {
		pr_err("Invalid info: dev %pK, context %pK\n",
		       dev, pld_context);
		return NULL;
	}

	spin_lock_irqsave(&pld_context->pld_lock, flags);
	list_for_each_entry(dev_node, &pld_context->dev_list, list) {
		if (dev_node->dev == dev) {
			spin_unlock_irqrestore(&pld_context->pld_lock, flags);
			return dev_node;
		}
	}
	spin_unlock_irqrestore(&pld_context->pld_lock, flags);

	return NULL;
}

enum pld_bus_type pld_get_bus_type(struct device *dev)
{
	struct dev_node *dev_node = pld_get_dev_node(dev);

	if (dev_node)
		return dev_node->bus_type;
	else
		return PLD_BUS_TYPE_NONE;
}

/**
 * pld_get_if_dev() - Bus interface/pipe dev of the device
 * @dev: device
 *
 * Return: Bus sub-interface or pipe dev.
 */
static struct device *pld_get_if_dev(struct device *dev)
{
	struct dev_node *dev_node = pld_get_dev_node(dev);

	if (dev_node)
		return dev_node->ifdev;
	else
		return NULL;
}

int pld_register_driver(struct pld_driver_ops *ops)
{
	int ret = 0;
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();

	if (!pld_context) {
		pr_err("global context is NULL\n");
		ret = -ENODEV;
		goto out;
	}

	if (pld_context->ops) {
		pr_err("driver already registered\n");
		ret = -EEXIST;
		goto out;
	}

	if (!ops || !ops->probe || !ops->remove ||
	    !ops->suspend || !ops->resume) {
		pr_err("Required callback functions are missing\n");
		ret = -EINVAL;
		goto out;
	}

	pld_context->ops = ops;
	pld_context->pld_driver_state = 0;

	ret = pld_pcie_register_driver();
	if (ret) {
		pld_err_rl("Fail to register pcie driver\n");
		goto fail_pcie;
	}
	pld_context->pld_driver_state |= PLD_PCIE_REGISTERED;

	ret = pld_snoc_register_driver();
	if (ret) {
		pld_err_rl("Fail to register snoc driver\n");
		goto fail_snoc;
	}
	pld_context->pld_driver_state |= PLD_SNOC_REGISTERED;

	ret = pld_sdio_register_driver();
	if (ret) {
		pr_err("Fail to register sdio driver\n");
		goto fail_sdio;
	}
	pld_context->pld_driver_state |= PLD_SDIO_REGISTERED;

	ret = pld_snoc_fw_sim_register_driver();
	if (ret) {
		pr_err("Fail to register snoc fw sim driver\n");
		goto fail_snoc_fw_sim;
	}
	pld_context->pld_driver_state |= PLD_SNOC_FW_SIM_REGISTERED;

	ret = pld_pcie_fw_sim_register_driver();
	if (ret) {
		pr_err("Fail to register pcie fw sim driver\n");
		goto fail_pcie_fw_sim;
	}
	pld_context->pld_driver_state |= PLD_PCIE_FW_SIM_REGISTERED;

	ret = pld_usb_register_driver();
	if (ret) {
		pr_err("Fail to register usb driver\n");
		goto fail_usb;
	}
	pld_context->pld_driver_state |= PLD_USB_REGISTERED;

	ret = pld_ipci_register_driver();
	if (ret) {
		pld_err_rl("Fail to register ipci driver\n");
		goto fail_ipci;
	}
	pld_context->pld_driver_state |= PLD_IPCI_REGISTERED;

	return ret;

fail_ipci:
	pld_usb_unregister_driver();
fail_usb:
	pld_pcie_fw_sim_unregister_driver();
fail_pcie_fw_sim:
	pld_snoc_fw_sim_unregister_driver();
fail_snoc_fw_sim:
	pld_sdio_unregister_driver();
fail_sdio:
	pld_snoc_unregister_driver();
fail_snoc:
	pld_pcie_unregister_driver();
fail_pcie:
	pld_context->pld_driver_state = 0;
	pld_context->ops = NULL;
out:
	return ret;
}

void pld_unregister_driver(void)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();

	if (!pld_context) {
		pr_err("global context is NULL\n");
		return;
	}

	if (!pld_context->ops) {
		pr_err("driver not registered\n");
		return;
	}

	pld_pcie_unregister_driver();
	pld_snoc_fw_sim_unregister_driver();
	pld_pcie_fw_sim_unregister_driver();
	pld_snoc_unregister_driver();
	pld_sdio_unregister_driver();
	pld_usb_unregister_driver();
	pld_ipci_unregister_driver();

	pld_context->pld_driver_state = 0;

	pld_context->ops = NULL;
}

int pld_wlan_enable(struct device *dev, struct pld_wlan_enable_cfg *config,
		    enum pld_driver_mode mode)
{
	int ret = 0;
	struct device *ifdev;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_wlan_enable(dev, config, mode, QWLAN_VERSIONSTR);
		break;
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_wlan_enable(dev, config, mode, QWLAN_VERSIONSTR);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		ret = pld_snoc_fw_sim_wlan_enable(dev, config, mode,
						  QWLAN_VERSIONSTR);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		ret = pld_pcie_fw_sim_wlan_enable(dev, config, mode,
						  QWLAN_VERSIONSTR);
		break;
	case PLD_BUS_TYPE_SDIO:
		ret = pld_sdio_wlan_enable(dev, config, mode, QWLAN_VERSIONSTR);
		break;
	case PLD_BUS_TYPE_USB:
		ifdev = pld_get_if_dev(dev);
		ret = pld_usb_wlan_enable(ifdev, config, mode,
					  QWLAN_VERSIONSTR);
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_wlan_enable(dev, config, mode, QWLAN_VERSIONSTR);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_wlan_disable(struct device *dev, enum pld_driver_mode mode)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_wlan_disable(dev, mode);
		break;
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_wlan_disable(dev, mode);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		ret = pld_snoc_fw_sim_wlan_disable(dev, mode);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		ret = pld_pcie_fw_sim_wlan_disable(dev, mode);
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_wlan_disable(dev, mode);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_wlan_hw_enable(void)
{
	return pld_pcie_wlan_hw_enable();
}

int pld_set_fw_log_mode(struct device *dev, u8 fw_log_mode)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_set_fw_log_mode(dev, fw_log_mode);
		break;
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_set_fw_log_mode(dev, fw_log_mode);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SDIO:
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_set_fw_log_mode(dev, fw_log_mode);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

void pld_get_default_fw_files(struct pld_fw_files *pfw_files)
{
	memset(pfw_files, 0, sizeof(*pfw_files));

	strlcpy(pfw_files->image_file, PREFIX PLD_IMAGE_FILE,
		PLD_MAX_FILE_NAME);
	strlcpy(pfw_files->board_data, PREFIX PLD_BOARD_DATA_FILE,
		PLD_MAX_FILE_NAME);
	strlcpy(pfw_files->otp_data, PREFIX PLD_OTP_FILE,
		PLD_MAX_FILE_NAME);
	strlcpy(pfw_files->utf_file, PREFIX PLD_UTF_FIRMWARE_FILE,
		PLD_MAX_FILE_NAME);
	strlcpy(pfw_files->utf_board_data, PREFIX PLD_BOARD_DATA_FILE,
		PLD_MAX_FILE_NAME);
	strlcpy(pfw_files->epping_file, PREFIX PLD_EPPING_FILE,
		PLD_MAX_FILE_NAME);
	strlcpy(pfw_files->setup_file, PREFIX PLD_SETUP_FILE,
		PLD_MAX_FILE_NAME);
}

int pld_get_fw_files_for_target(struct device *dev,
				struct pld_fw_files *pfw_files,
				u32 target_type, u32 target_version)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_get_fw_files_for_target(dev, pfw_files,
						       target_type,
						       target_version);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		ret = pld_sdio_get_fw_files_for_target(pfw_files,
						       target_type,
						       target_version);
		break;
	case PLD_BUS_TYPE_USB:
	ret = pld_usb_get_fw_files_for_target(pfw_files,
					      target_type,
					      target_version);
	break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_prevent_l1(struct device *dev)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_prevent_l1(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_prevent_l1(dev);
		break;
	default:
		ret = -EINVAL;
		pr_err("Invalid device type\n");
		break;
	}

	return ret;
}

void pld_allow_l1(struct device *dev)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		pld_pcie_allow_l1(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
		break;
	case PLD_BUS_TYPE_IPCI:
		pld_ipci_allow_l1(dev);
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

int pld_get_mhi_state(struct device *dev)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
		ret = PLD_MHI_STATE_L0;
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_mhi_state(dev);
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
	return ret;
}

int pld_set_pcie_gen_speed(struct device *dev, u8 pcie_gen_speed)
{
	int ret = -EINVAL;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_set_gen_speed(dev, pcie_gen_speed);
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
	return ret;
}

void pld_is_pci_link_down(struct device *dev)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		break;
	case PLD_BUS_TYPE_PCIE:
		pld_pcie_link_down(dev);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_IPCI:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

void pld_get_bus_reg_dump(struct device *dev, uint8_t *buf, uint32_t len)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE_FW_SIM:
		break;
	case PLD_BUS_TYPE_PCIE:
		pld_pcie_get_reg_dump(dev, buf, len);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_IPCI:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

void pld_schedule_recovery_work(struct device *dev,
				enum pld_recovery_reason reason)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		pld_pcie_schedule_recovery_work(dev, reason);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_IPCI:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

int pld_wlan_pm_control(struct device *dev, bool vote)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_wlan_pm_control(dev, vote);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

void *pld_get_virt_ramdump_mem(struct device *dev, unsigned long *size)
{
	void *mem = NULL;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		mem = pld_pcie_get_virt_ramdump_mem(dev, size);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		mem = pld_sdio_get_virt_ramdump_mem(dev, size);
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}

	return mem;
}

void pld_release_virt_ramdump_mem(struct device *dev, void *address)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		pld_pcie_release_virt_ramdump_mem(address);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		pld_sdio_release_virt_ramdump_mem(address);
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

void pld_device_crashed(struct device *dev)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		pld_pcie_device_crashed(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		pld_sdio_device_crashed(dev);
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

void pld_device_self_recovery(struct device *dev,
			      enum pld_recovery_reason reason)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		pld_pcie_device_self_recovery(dev, reason);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		pld_sdio_device_self_recovery(dev);
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

void pld_intr_notify_q6(struct device *dev)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		pld_pcie_intr_notify_q6(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_IPCI:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

void pld_request_pm_qos(struct device *dev, u32 qos_val)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		pld_pcie_request_pm_qos(dev, qos_val);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		/* To do Add call cns API */
		break;
	case PLD_BUS_TYPE_USB:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

void pld_remove_pm_qos(struct device *dev)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		pld_pcie_remove_pm_qos(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		/* To do Add call cns API */
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

int pld_request_bus_bandwidth(struct device *dev, int bandwidth)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_request_bus_bandwidth(dev, bandwidth);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		/* To do Add call cns API */
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

bool pld_is_direct_link_supported(struct device *dev)
{
	bool ret = false;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_is_direct_link_supported(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_IPCI:
	case PLD_BUS_TYPE_SDIO:
	default:
		break;
	}

	return ret;
}

bool pld_ce_cmn_cfg_supported(struct device *dev)
{
	bool ret = false;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_ce_cmn_cfg_supported(dev);
		break;
	default:
		break;
	}

	return ret;
}

int pld_get_platform_cap(struct device *dev, struct pld_platform_cap *cap)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_get_platform_cap(dev, cap);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		ret = pld_pcie_fw_sim_get_platform_cap(dev, cap);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_get_sha_hash(struct device *dev, const u8 *data,
		     u32 data_len, u8 *hash_idx, u8 *out)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_get_sha_hash(dev, data, data_len,
					    hash_idx, out);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

void *pld_get_fw_ptr(struct device *dev)
{
	void *ptr = NULL;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ptr = pld_pcie_get_fw_ptr(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}

	return ptr;
}

int pld_auto_suspend(struct device *dev)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_auto_suspend(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_auto_resume(struct device *dev)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_auto_resume(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_force_wake_request(struct device *dev)
{
	int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_force_wake_request(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_force_wake_request(dev);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_force_wake_request_sync(struct device *dev, int timeout_us)
{
	int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_force_wake_request_sync(dev, timeout_us);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_IPCI:
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_exit_power_save(struct device *dev)
{
	int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
		break;
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		ret = pld_pcie_fw_sim_exit_power_save(dev);
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_exit_power_save(dev);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_is_device_awake(struct device *dev)
{
	int ret = true;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_is_device_awake(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_is_device_awake(dev);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_is_pci_ep_awake(struct device *dev)
{
	int ret = true;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
		ret = -ENOTSUPP;
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_is_pci_ep_awake(dev);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_force_wake_release(struct device *dev)
{
	int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_force_wake_release(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_force_wake_release(dev);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_ce_request_irq(struct device *dev, unsigned int ce_id,
		       irqreturn_t (*handler)(int, void *),
		       unsigned long flags, const char *name, void *ctx)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_ce_request_irq(dev, ce_id,
					      handler, flags, name, ctx);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		ret = pld_snoc_fw_sim_ce_request_irq(dev, ce_id,
						     handler, flags, name, ctx);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_PCIE:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_ce_free_irq(struct device *dev, unsigned int ce_id, void *ctx)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_ce_free_irq(dev, ce_id, ctx);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		ret = pld_snoc_fw_sim_ce_free_irq(dev, ce_id, ctx);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_PCIE:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

void pld_enable_irq(struct device *dev, unsigned int ce_id)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_SNOC:
		pld_snoc_enable_irq(dev, ce_id);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		pld_snoc_fw_sim_enable_irq(dev, ce_id);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_PCIE:
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

void pld_disable_irq(struct device *dev, unsigned int ce_id)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_SNOC:
		pld_snoc_disable_irq(dev, ce_id);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		pld_snoc_fw_sim_disable_irq(dev, ce_id);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_PCIE:
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

int pld_get_soc_info(struct device *dev, struct pld_soc_info *info)
{
	int ret = 0;
	memset(info, 0, sizeof(*info));

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_get_soc_info(dev, info);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		ret = pld_snoc_fw_sim_get_soc_info(dev, info);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		ret = pld_pcie_fw_sim_get_soc_info(dev, info);
		break;
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_get_soc_info(dev, info);
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_get_soc_info(dev, info);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_get_ce_id(struct device *dev, int irq)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_get_ce_id(dev, irq);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		ret = pld_snoc_fw_sim_get_ce_id(dev, irq);
		break;
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_get_ce_id(dev, irq);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_get_irq(struct device *dev, int ce_id)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_get_irq(dev, ce_id);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		ret = pld_snoc_fw_sim_get_irq(dev, ce_id);
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_get_irq(dev, ce_id);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		ret = pld_pcie_fw_sim_get_irq(dev, ce_id);
		break;
	case PLD_BUS_TYPE_PCIE:
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

void pld_lock_reg_window(struct device *dev, unsigned long *flags)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		pld_pcie_lock_reg_window(dev, flags);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	case PLD_BUS_TYPE_USB:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

void pld_unlock_reg_window(struct device *dev, unsigned long *flags)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		pld_pcie_unlock_reg_window(dev, flags);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	case PLD_BUS_TYPE_USB:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

int pld_get_pci_slot(struct device *dev)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_get_pci_slot(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_IPCI:
		break;
	default:
		pr_err("Invalid device type\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_power_on(struct device *dev)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		/* cnss platform driver handles PCIe SoC
		 * power on/off sequence so let CNSS driver
		 * handle the power on sequence for PCIe SoC
		 */
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_power_on(dev);
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_power_on(dev);
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}

	return ret;
}

int pld_power_off(struct device *dev)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		/* cnss platform driver handles PCIe SoC
		 * power on/off sequence so let CNSS driver
		 * handle the power off sequence for PCIe SoC
		 */
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_power_off(dev);
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_power_off(dev);
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}

	return ret;
}

int pld_athdiag_read(struct device *dev, uint32_t offset,
		     uint32_t memtype, uint32_t datalen,
		     uint8_t *output)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_athdiag_read(dev, offset, memtype,
					    datalen, output);
		break;
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_athdiag_read(dev, offset, memtype,
					    datalen, output);
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	case PLD_BUS_TYPE_USB:
		ret = pld_usb_athdiag_read(dev, offset, memtype,
					   datalen, output);
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_athdiag_read(dev, offset, memtype,
					    datalen, output);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_athdiag_write(struct device *dev, uint32_t offset,
		      uint32_t memtype, uint32_t datalen,
		      uint8_t *input)
{
	int ret = 0;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_athdiag_write(dev, offset, memtype,
					     datalen, input);
		break;
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_athdiag_write(dev, offset, memtype,
					     datalen, input);
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	case PLD_BUS_TYPE_USB:
		ret = pld_usb_athdiag_write(dev, offset, memtype,
					    datalen, input);
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_athdiag_write(dev, offset, memtype,
					     datalen, input);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
void *pld_smmu_get_domain(struct device *dev)
{
	void *ptr = NULL;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_SNOC:
		ptr = pld_snoc_smmu_get_domain(dev);
		break;
	case PLD_BUS_TYPE_PCIE:
		ptr = pld_pcie_smmu_get_domain(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_IPCI:
		ptr = pld_ipci_smmu_get_domain(dev);
		break;
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
		pr_err("Not supported on type %d\n", type);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		break;
	}

	return ptr;
}
#else
void *pld_smmu_get_mapping(struct device *dev)
{
	void *ptr = NULL;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_SNOC:
		ptr = pld_snoc_smmu_get_mapping(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_PCIE:
		ptr = pld_pcie_smmu_get_mapping(dev);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		break;
	}

	return ptr;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
struct kobject *pld_get_wifi_kobj(struct device *dev)
{
	struct kobject *wifi_kobj = NULL;

	wifi_kobj = pld_pcie_get_wifi_kobj(dev);

	return wifi_kobj;
}
#endif

int pld_smmu_map(struct device *dev, phys_addr_t paddr,
		 uint32_t *iova_addr, size_t size)
{
	int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_smmu_map(dev, paddr, iova_addr, size);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_smmu_map(dev, paddr, iova_addr, size);
		break;
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_smmu_map(dev, paddr, iova_addr, size);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_SMMU_S1_UNMAP
int pld_smmu_unmap(struct device *dev,
		   uint32_t iova_addr, size_t size)
{
	int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_smmu_unmap(dev, iova_addr, size);
		break;
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_smmu_unmap(dev, iova_addr, size);
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_smmu_unmap(dev, iova_addr, size);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		pr_err("Not supported on type %d\n", type);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}
#endif

int pld_get_user_msi_assignment(struct device *dev, char *user_name,
				int *num_vectors, uint32_t *user_base_data,
				uint32_t *base_vector)
{
	int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_get_user_msi_assignment(dev, user_name,
						       num_vectors,
						       user_base_data,
						       base_vector);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		ret = pld_pcie_fw_sim_get_user_msi_assignment(dev, user_name,
							      num_vectors,
							      user_base_data,
							      base_vector);
		break;
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		pr_err("Not supported on type %d\n", type);
		ret = -ENODEV;
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_get_user_msi_assignment(dev, user_name,
						       num_vectors,
						       user_base_data,
						       base_vector);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_srng_devm_request_irq(struct device *dev, int irq,
			      irq_handler_t handler,
			      unsigned long irqflags,
			      const char *devname,
			      void *dev_data)
{
	int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
		ret = devm_request_irq(dev, irq, handler, irqflags,
				       devname, dev_data);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		ret = pld_pcie_fw_sim_request_irq(dev, irq, handler,
						  irqflags, devname,
						  dev_data);
		break;
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		pr_err("Not supported on type %d\n", type);
		ret = -ENODEV;
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = devm_request_irq(dev, irq, handler, irqflags,
				       devname, dev_data);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_srng_request_irq(struct device *dev, int irq, irq_handler_t handler,
			 unsigned long irqflags,
			 const char *devname,
			 void *dev_data)
{
	int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
		ret = request_irq(irq, handler, irqflags, devname, dev_data);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		ret = pld_pcie_fw_sim_request_irq(dev, irq, handler,
						  irqflags, devname,
						  dev_data);
		break;
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		pr_err("Not supported on type %d\n", type);
		ret = -ENODEV;
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = request_irq(irq, handler, irqflags, devname, dev_data);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_srng_free_irq(struct device *dev, int irq, void *dev_data)
{
	int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
		free_irq(irq, dev_data);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		ret = pld_pcie_fw_sim_free_irq(dev, irq, dev_data);
		break;
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		pr_err("Not supported on type %d\n", type);
		ret = -ENODEV;
		break;
	case PLD_BUS_TYPE_IPCI:
		free_irq(irq, dev_data);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

void pld_srng_enable_irq(struct device *dev, int irq)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		pld_pcie_fw_sim_enable_irq(dev, irq);
		break;
	case PLD_BUS_TYPE_PCIE:
		enable_irq(irq);
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	case PLD_BUS_TYPE_IPCI:
		enable_irq(irq);
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

void pld_srng_disable_irq(struct device *dev, int irq)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		pld_pcie_fw_sim_disable_irq(dev, irq);
		break;
	case PLD_BUS_TYPE_PCIE:
		disable_irq_nosync(irq);
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	case PLD_BUS_TYPE_IPCI:
		disable_irq_nosync(irq);
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

void pld_srng_disable_irq_sync(struct device *dev, int irq)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		pld_pcie_fw_sim_disable_irq(dev, irq);
		break;
	case PLD_BUS_TYPE_PCIE:
	case PLD_BUS_TYPE_IPCI:
		disable_irq(irq);
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

int pld_pci_read_config_word(struct pci_dev *pdev, int offset, uint16_t *val)
{
	int ret = 0;

	switch (pld_get_bus_type(&pdev->dev)) {
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		ret = pld_pcie_fw_sim_read_config_word(&pdev->dev, offset, val);
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_PCIE:
		ret = pci_read_config_word(pdev, offset, val);
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}

	return ret;
}

int pld_pci_write_config_word(struct pci_dev *pdev, int offset, uint16_t val)
{
	int ret = 0;

	switch (pld_get_bus_type(&pdev->dev)) {
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		break;
	case PLD_BUS_TYPE_PCIE:
		ret = pci_write_config_word(pdev, offset, val);
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}

	return ret;
}

int pld_pci_read_config_dword(struct pci_dev *pdev, int offset, uint32_t *val)
{
	int ret = 0;

	switch (pld_get_bus_type(&pdev->dev)) {
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		break;
	case PLD_BUS_TYPE_PCIE:
		ret = pci_read_config_dword(pdev, offset, val);
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}

	return ret;
}

int pld_pci_write_config_dword(struct pci_dev *pdev, int offset, uint32_t val)
{
	int ret = 0;

	switch (pld_get_bus_type(&pdev->dev)) {
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		break;
	case PLD_BUS_TYPE_PCIE:
		ret = pci_write_config_dword(pdev, offset, val);
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}

	return ret;
}

int pld_get_msi_irq(struct device *dev, unsigned int vector)
{
	int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_get_msi_irq(dev, vector);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		ret = pld_pcie_fw_sim_get_msi_irq(dev, vector);
		break;
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		pr_err("Not supported on type %d\n", type);
		ret = -ENODEV;
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_get_msi_irq(dev, vector);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

void pld_get_msi_address(struct device *dev, uint32_t *msi_addr_low,
			 uint32_t *msi_addr_high)
{
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
		pld_pcie_get_msi_address(dev, msi_addr_low, msi_addr_high);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		pld_pcie_fw_sim_get_msi_address(dev, msi_addr_low,
						msi_addr_high);
		break;
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		pr_err("Not supported on type %d\n", type);
		break;
	case PLD_BUS_TYPE_IPCI:
		pld_ipci_get_msi_address(dev, msi_addr_low, msi_addr_high);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		break;
	}
}

int pld_is_drv_connected(struct device *dev)
{
	enum pld_bus_type type = pld_get_bus_type(dev);
	int ret = 0;

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_is_drv_connected(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_IPCI:
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

unsigned int pld_socinfo_get_serial_number(struct device *dev)
{
	unsigned int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_socinfo_get_serial_number(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_PCIE:
		pr_err("Not supported on type %d\n", type);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		break;
	}

	return ret;
}

int pld_is_qmi_disable(struct device *dev)
{
	int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_is_qmi_disable(dev);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_IPCI:
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_PCIE:
	case PLD_BUS_TYPE_SDIO:
		pr_err("Not supported on type %d\n", type);
		ret = -EINVAL;
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int pld_is_fw_down(struct device *dev)
{
	int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);
	struct device *ifdev;

	switch (type) {
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_is_fw_down(dev);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		ret = pld_snoc_fw_sim_is_fw_down(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		break;
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_is_fw_down(dev);
		break;
	case PLD_BUS_TYPE_SDIO:
		break;
	case PLD_BUS_TYPE_USB:
		ifdev = pld_get_if_dev(dev);
		ret = pld_usb_is_fw_down(ifdev);
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = pld_ipci_is_fw_down(dev);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_ENABLE_LOW_POWER_MODE
int pld_is_low_power_mode(struct device *dev)
{
	int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_is_low_power_mode(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_IPCI:
	default:
		break;
	}
	return ret;
}
#endif

int pld_force_assert_target(struct device *dev)
{
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_SNOC:
		return pld_snoc_force_assert_target(dev);
	case PLD_BUS_TYPE_PCIE:
		return pld_pcie_force_assert_target(dev);
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		return -EOPNOTSUPP;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SDIO:
		return -EINVAL;
	case PLD_BUS_TYPE_IPCI:
		return pld_ipci_force_assert_target(dev);
	default:
		pr_err("Invalid device type %d\n", type);
		return -EINVAL;
	}
}

int pld_force_collect_target_dump(struct device *dev)
{
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
		return pld_pcie_collect_rddm(dev);
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_IPCI:
		return -EOPNOTSUPP;
	default:
		pr_err("Invalid device type %d\n", type);
		return -EINVAL;
	}
}

int pld_qmi_send_get(struct device *dev)
{
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
		return pld_pcie_qmi_send_get(dev);
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_IPCI:
		return 0;
	default:
		pr_err("Invalid device type %d\n", type);
		return -EINVAL;
	}
}

int pld_qmi_send_put(struct device *dev)
{
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
		return pld_pcie_qmi_send_put(dev);
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_IPCI:
		return 0;
	default:
		pr_err("Invalid device type %d\n", type);
		return -EINVAL;
	}
}

int pld_qmi_send(struct device *dev, int type, void *cmd,
		 int cmd_len, void *cb_ctx,
		 int (*cb)(void *ctx, void *event, int event_len))
{
	enum pld_bus_type bus_type = pld_get_bus_type(dev);

	switch (bus_type) {
	case PLD_BUS_TYPE_PCIE:
		return pld_pcie_qmi_send(dev, type, cmd, cmd_len, cb_ctx, cb);
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
		return -EINVAL;
	case PLD_BUS_TYPE_IPCI:
		return pld_ipci_qmi_send(dev, type, cmd, cmd_len, cb_ctx, cb);
	default:
		pr_err("Invalid device type %d\n", bus_type);
		return -EINVAL;
	}
}

int pld_qmi_indication(struct device *dev, void *cb_ctx,
		       int (*cb)(void *ctx, uint16_t type,
				 void *event, int event_len))
{
	enum pld_bus_type bus_type = pld_get_bus_type(dev);

	switch (bus_type) {
	case PLD_BUS_TYPE_PCIE:
		return pld_pcie_register_qmi_ind(dev, cb_ctx, cb);
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
		return -EINVAL;
	case PLD_BUS_TYPE_IPCI:
		return pld_ipci_register_qmi_ind(dev, cb_ctx, cb);
	default:
		pr_err("Invalid device type %d\n", bus_type);
		return -EINVAL;
	}
}

bool pld_is_fw_dump_skipped(struct device *dev)
{
	bool ret = false;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_SDIO:
		ret = pld_sdio_is_fw_dump_skipped();
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_IPCI:
	default:
		break;
	}
	return ret;
}

int pld_is_pdr(struct device *dev)
{
	int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_is_pdr();
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_IPCI:
	default:
		break;
	}
	return ret;
}

int pld_is_fw_rejuvenate(struct device *dev)
{
	int ret = 0;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_is_fw_rejuvenate();
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_IPCI:
	default:
		break;
	}
	return ret;
}

bool pld_have_platform_driver_support(struct device *dev)
{
	bool ret = false;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_platform_driver_support();
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		ret = true;
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
		break;
	case PLD_BUS_TYPE_IPCI:
		ret = true;
		break;
	case PLD_BUS_TYPE_SDIO:
		ret = pld_sdio_platform_driver_support();
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}

	return ret;
}

int pld_idle_shutdown(struct device *dev,
		      int (*shutdown_cb)(struct device *dev))
{
	int errno = -EINVAL;
	enum pld_bus_type type;

	if (!shutdown_cb)
		return -EINVAL;

	type = pld_get_bus_type(dev);
	switch (type) {
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
		errno = shutdown_cb(dev);
		break;
	case PLD_BUS_TYPE_SNOC:
		errno = pld_snoc_idle_shutdown(dev);
		break;
	case PLD_BUS_TYPE_PCIE:
		errno = pld_pcie_idle_shutdown(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		errno = pld_pcie_fw_sim_idle_shutdown(dev);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		errno = pld_snoc_fw_sim_idle_shutdown(dev);
		break;
	case PLD_BUS_TYPE_IPCI:
		errno = pld_ipci_idle_shutdown(dev);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		break;
	}

	return errno;
}

int pld_idle_restart(struct device *dev,
		     int (*restart_cb)(struct device *dev))
{
	int errno = -EINVAL;
	enum pld_bus_type type;

	if (!restart_cb)
		return -EINVAL;

	type = pld_get_bus_type(dev);
	switch (type) {
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
		errno = restart_cb(dev);
		break;
	case PLD_BUS_TYPE_SNOC:
		errno = pld_snoc_idle_restart(dev);
		break;
	case PLD_BUS_TYPE_PCIE:
		errno = pld_pcie_idle_restart(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		errno = pld_pcie_fw_sim_idle_restart(dev);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		errno = pld_snoc_fw_sim_idle_restart(dev);
		break;
	case PLD_BUS_TYPE_IPCI:
		errno = pld_ipci_idle_restart(dev);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		break;
	}

	return errno;
}

int pld_thermal_register(struct device *dev,
			 unsigned long max_state, int mon_id)
{
	int errno = -EINVAL;
	enum pld_bus_type type;

	type = pld_get_bus_type(dev);
	switch (type) {
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_SNOC:
		break;
	case PLD_BUS_TYPE_PCIE:
		errno = pld_pci_thermal_register(dev, max_state, mon_id);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
		break;
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		errno = pld_pcie_fw_sim_thermal_register(dev, max_state,
							 mon_id);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_IPCI:
		errno = pld_ipci_thermal_register(dev, max_state, mon_id);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		break;
	}

	return errno;
}

void pld_thermal_unregister(struct device *dev, int mon_id)
{
	enum pld_bus_type type;

	type = pld_get_bus_type(dev);
	switch (type) {
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_SNOC:
		break;
	case PLD_BUS_TYPE_PCIE:
		pld_pci_thermal_unregister(dev, mon_id);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
		break;
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		pld_pcie_fw_sim_thermal_unregister(dev, mon_id);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_IPCI:
		pld_ipci_thermal_unregister(dev, mon_id);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		break;
	}
}

int pld_set_wfc_mode(struct device *dev, enum pld_wfc_mode wfc_mode)
{
	int errno = -ENOTSUPP;
	enum pld_bus_type type;

	type = pld_get_bus_type(dev);
	switch (type) {
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_IPCI:
	case PLD_BUS_TYPE_PCIE_FW_SIM:
		break;
	case PLD_BUS_TYPE_PCIE:
		errno = pld_pcie_set_wfc_mode(dev, wfc_mode);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		break;
	}

	return errno;
}
const char *pld_bus_width_type_to_str(enum pld_bus_width_type level)
{
	switch (level) {
	/* initialize the wlan sub system */
	case PLD_BUS_WIDTH_NONE:
		return "NONE";
	case PLD_BUS_WIDTH_IDLE:
		return "IDLE";
	case PLD_BUS_WIDTH_LOW:
		return "LOW";
	case PLD_BUS_WIDTH_MEDIUM:
		return "MEDIUM";
	case PLD_BUS_WIDTH_HIGH:
		return "HIGH";
	case PLD_BUS_WIDTH_MID_HIGH:
		return "MID_HIGH";
	case PLD_BUS_WIDTH_VERY_HIGH:
		return "VERY_HIGH";
	case PLD_BUS_WIDTH_ULTRA_HIGH:
		return "ULTRA_HIGH";
	case PLD_BUS_WIDTH_LOW_LATENCY:
		return "LOW_LAT";
	default:
		if (level > PLD_BUS_WIDTH_ULTRA_HIGH)
			return "SUPER_HIGH";
		else
			return "INVAL";
	}
}

int pld_get_thermal_state(struct device *dev, unsigned long *thermal_state,
			  int mon_id)
{
	int errno = -EINVAL;
	enum pld_bus_type type;

	type = pld_get_bus_type(dev);
	switch (type) {
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_SNOC:
		break;
	case PLD_BUS_TYPE_PCIE:
		errno = pld_pci_get_thermal_state(dev, thermal_state, mon_id);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
		break;
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		errno = pld_pcie_fw_sim_get_thermal_state(dev, thermal_state,
							  mon_id);
		break;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
		break;
	case PLD_BUS_TYPE_IPCI:
		errno = pld_ipci_get_thermal_state(dev, thermal_state, mon_id);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		break;
	}

	return errno;
}

#ifdef CNSS_UTILS_VENDOR_UNSAFE_CHAN_API_SUPPORT
int pld_get_wlan_unsafe_channel_sap(
	struct device *dev, struct pld_ch_avoid_ind_type *ch_avoid_ranges)
{
	struct cnss_ch_avoid_ind_type cnss_ch_avoid;
	int ret;
	int i;

	if (!ch_avoid_ranges)
		return -EINVAL;
	cnss_ch_avoid.ch_avoid_range_cnt = 0;
	ret = cnss_utils_get_wlan_unsafe_channel_sap(dev, &cnss_ch_avoid);
	if (ret)
		return ret;

	for (i = 0;
	     i < PLD_CH_AVOID_MAX_RANGE &&
	     i < cnss_ch_avoid.ch_avoid_range_cnt; i++) {
		ch_avoid_ranges->avoid_freq_range[i].start_freq =
			cnss_ch_avoid.avoid_freq_range[i].start_freq;
		ch_avoid_ranges->avoid_freq_range[i].end_freq =
			cnss_ch_avoid.avoid_freq_range[i].end_freq;
	}
	ch_avoid_ranges->ch_avoid_range_cnt = i;
	if (i < cnss_ch_avoid.ch_avoid_range_cnt)
		pr_err("unexpected cnss ch_avoid_range_cnt %d",
		       cnss_ch_avoid.ch_avoid_range_cnt);

	return 0;
}
#endif

void pld_set_tsf_sync_period(struct device *dev, u32 val)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		pld_pcie_set_tsf_sync_period(dev, val);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_IPCI:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

void pld_reset_tsf_sync_period(struct device *dev)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		pld_pcie_reset_tsf_sync_period(dev);
		break;
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
	case PLD_BUS_TYPE_IPCI:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
		break;
	default:
		pr_err("Invalid device type\n");
		break;
	}
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
int pld_is_ipa_offload_disabled(struct device *dev)
{
	unsigned long dev_cfg = 0;

	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_SNOC:
		dev_cfg = pld_snoc_get_device_config();
		break;
	case PLD_BUS_TYPE_IPCI:
	case PLD_BUS_TYPE_PCIE:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
		pr_err("Not supported on type %d\n", type);
		break;
	default:
		pr_err("Invalid device type %d\n", type);
		break;
	}

	return test_bit(PLD_IPA_DISABLED, &dev_cfg);
}
#endif

#ifdef FEATURE_WLAN_TIME_SYNC_FTM
int pld_get_audio_wlan_timestamp(struct device *dev,
				 enum pld_wlan_time_sync_trigger_type type,
				 uint64_t *ts)
{
	int ret = 0;
	enum pld_bus_type bus_type;

	bus_type = pld_get_bus_type(dev);
	switch (bus_type) {
	case PLD_BUS_TYPE_SNOC:
		ret = pld_snoc_get_audio_wlan_timestamp(dev, type, ts);
		break;
	case PLD_BUS_TYPE_PCIE:
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_SDIO:
	case PLD_BUS_TYPE_USB:
	case PLD_BUS_TYPE_IPCI:
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
#endif /* FEATURE_WLAN_TIME_SYNC_FTM */

bool pld_is_one_msi(struct device *dev)
{
	bool ret = false;
	enum pld_bus_type type = pld_get_bus_type(dev);

	switch (type) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_is_one_msi(dev);
		break;
	default:
		break;
	}

	return ret;
}

#ifdef CONFIG_AFC_SUPPORT
int pld_send_buffer_to_afcmem(struct device *dev, const uint8_t *afcdb,
			      uint32_t len, uint8_t slotid)
{
	return cnss_send_buffer_to_afcmem(dev, afcdb, len, slotid);
}

int pld_reset_afcmem(struct device *dev, uint8_t slotid)
{
	return cnss_reset_afcmem(dev, slotid);
}
#endif

#ifdef FEATURE_DIRECT_LINK
int pld_audio_smmu_map(struct device *dev, phys_addr_t paddr, dma_addr_t iova,
		       size_t size)
{
	int ret;

	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		ret = pld_pcie_audio_smmu_map(dev, paddr, iova, size);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

void pld_audio_smmu_unmap(struct device *dev, dma_addr_t iova, size_t size)
{
	switch (pld_get_bus_type(dev)) {
	case PLD_BUS_TYPE_PCIE:
		pld_pcie_audio_smmu_unmap(dev, iova, size);
		break;
	default:
		break;
	}
}
#endif
