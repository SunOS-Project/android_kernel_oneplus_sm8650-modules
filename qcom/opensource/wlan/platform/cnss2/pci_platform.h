/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved. */

#ifndef _CNSS_PCI_PLATFORM_H
#define _CNSS_PCI_PLATFORM_H

#include "pci.h"

#if IS_ENABLED(CONFIG_PCI_MSM) || IS_ENABLED(CONFIG_PCIE_QCOM_ECAM)
/**
 * _cnss_pci_enumerate() - Enumerate PCIe endpoints
 * @plat_priv: driver platform context pointer
 * @rc_num: root complex index that an endpoint connects to
 *
 * This function shall call corresponding PCIe root complex driver APIs
 * to power on root complex and enumerate the endpoint connected to it.
 *
 * Return: 0 for success, negative value for error
 */
int _cnss_pci_enumerate(struct cnss_plat_data *plat_priv, u32 rc_num);

/**
 * cnss_pci_assert_perst() - Assert PCIe PERST GPIO
 * @pci_priv: driver PCI bus context pointer
 *
 * This function shall call corresponding PCIe root complex driver APIs
 * to assert PCIe PERST GPIO.
 *
 * Return: 0 for success, negative value for error
 */
int cnss_pci_assert_perst(struct cnss_pci_data *pci_priv);

/**
 * cnss_pci_disable_pc() - Disable PCIe link power collapse from RC driver
 * @pci_priv: driver PCI bus context pointer
 * @vote: value to indicate disable (true) or enable (false)
 *
 * This function shall call corresponding PCIe root complex driver APIs
 * to disable PCIe power collapse. The purpose of this API is to avoid
 * root complex driver still controlling PCIe link from callbacks of
 * system suspend/resume. Device driver itself should take full control
 * of the link in such cases.
 *
 * Return: 0 for success, negative value for error
 */
int cnss_pci_disable_pc(struct cnss_pci_data *pci_priv, bool vote);

/**
 * cnss_pci_set_link_bandwidth() - Update number of lanes and speed of
 *                                 PCIe link
 * @pci_priv: driver PCI bus context pointer
 * @link_speed: PCIe link gen speed
 * @link_width: number of lanes for PCIe link
 *
 * This function shall call corresponding PCIe root complex driver APIs
 * to update number of lanes and speed of the link.
 *
 * Return: 0 for success, negative value for error
 */
int cnss_pci_set_link_bandwidth(struct cnss_pci_data *pci_priv,
				u16 link_speed, u16 link_width);

/**
 * cnss_pci_set_max_link_speed() - Set the maximum speed PCIe can link up with
 * @pci_priv: driver PCI bus context pointer
 * @rc_num: root complex index that an endpoint connects to
 * @link_speed: PCIe link gen speed
 *
 * This function shall call corresponding PCIe root complex driver APIs
 * to update the maximum speed that PCIe can link up with.
 *
 * Return: 0 for success, negative value for error
 */
int cnss_pci_set_max_link_speed(struct cnss_pci_data *pci_priv,
				u32 rc_num, u16 link_speed);

/**
 * cnss_reg_pci_event() - Register for PCIe events
 * @pci_priv: driver PCI bus context pointer
 *
 * This function shall call corresponding PCIe root complex driver APIs
 * to register for PCIe events like link down or WAKE GPIO toggling etc.
 * The events should be based on PCIe root complex driver's capability.
 *
 * Return: 0 for success, negative value for error
 */
int cnss_reg_pci_event(struct cnss_pci_data *pci_priv);
void cnss_dereg_pci_event(struct cnss_pci_data *pci_priv);

/**
 * cnss_wlan_adsp_pc_enable: Control ADSP power collapse setup
 * @dev: Platform driver pci private data structure
 * @control: Power collapse enable / disable
 *
 * This function controls ADSP power collapse (PC). It must be called
 * based on wlan state.  ADSP power collapse during wlan RTPM suspend state
 * results in delay during periodic QMI stats PCI link up/down. This delay
 * causes additional power consumption.
 *
 * Result: 0 Success. negative error codes.
 */
int cnss_wlan_adsp_pc_enable(struct cnss_pci_data *pci_priv,
			     bool control);
int cnss_set_pci_link(struct cnss_pci_data *pci_priv, bool link_up);
int cnss_pci_prevent_l1(struct device *dev);
void cnss_pci_allow_l1(struct device *dev);
int cnss_pci_get_msi_assignment(struct cnss_pci_data *pci_priv);
int cnss_pci_get_iommu_addr(struct cnss_pci_data *pci_priv, struct device_node *of_node);
int cnss_pci_init_smmu(struct cnss_pci_data *pci_priv);
void cnss_pci_update_drv_supported(struct cnss_pci_data *pci_priv);
int cnss_pci_dsp_link_control(struct cnss_pci_data *pci_priv,
			      bool link_enable);
/**
 * _cnss_pci_get_reg_dump() - Dump PCIe RC registers for debug
 * @pci_priv: driver PCI bus context pointer
 * @buf: destination buffer pointer
 * @len: length of the buffer
 *
 * This function shall call corresponding PCIe root complex driver API
 * to dump PCIe RC registers for debug purpose.
 *
 * Return: 0 for success, negative value for error
 */
int _cnss_pci_get_reg_dump(struct cnss_pci_data *pci_priv,
			   u8 *buf, u32 len);
int cnss_pci_set_dsp_link_status(struct cnss_pci_data *pci_priv,
				 bool link_enable);
int cnss_pci_get_dsp_link_status(struct cnss_pci_data *pci_priv);
int cnss_pci_dsp_link_enable(struct cnss_pci_data *pci_priv);
int cnss_pci_dsp_link_retrain(struct cnss_pci_data *pci_priv,
			      u16 target_link_speed);
#else
int _cnss_pci_enumerate(struct cnss_plat_data *plat_priv, u32 rc_num)
{
	return -EOPNOTSUPP;
}

int cnss_pci_assert_perst(struct cnss_pci_data *pci_priv)
{
	return -EOPNOTSUPP;
}

int cnss_pci_disable_pc(struct cnss_pci_data *pci_priv, bool vote)
{
	return 0;
}

int cnss_pci_set_link_bandwidth(struct cnss_pci_data *pci_priv,
				u16 link_speed, u16 link_width)
{
	return 0;
}

int cnss_pci_set_max_link_speed(struct cnss_pci_data *pci_priv,
				u32 rc_num, u16 link_speed)
{
	return 0;
}

int cnss_reg_pci_event(struct cnss_pci_data *pci_priv)
{
	return 0;
}

void cnss_dereg_pci_event(struct cnss_pci_data *pci_priv) {}

int cnss_wlan_adsp_pc_enable(struct cnss_pci_data *pci_priv, bool control)
{
	return 0;
}

int cnss_set_pci_link(struct cnss_pci_data *pci_priv, bool link_up)
{
	return 0;
}

int cnss_pci_prevent_l1(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(cnss_pci_prevent_l1);

void cnss_pci_allow_l1(struct device *dev)
{
}
EXPORT_SYMBOL(cnss_pci_allow_l1);

int cnss_pci_get_msi_assignment(struct cnss_pci_data *pci_priv)
{
	return 0;
}

int cnss_pci_get_iommu_addr(struct cnss_pci_data *pci_priv, struct device_node *of_node)
{
	return 0;
}

int cnss_pci_init_smmu(struct cnss_pci_data *pci_priv)
{
	return 0;
}

int _cnss_pci_get_reg_dump(struct cnss_pci_data *pci_priv,
			   u8 *buf, u32 len)
{
	return 0;
}

void cnss_pci_update_drv_supported(struct cnss_pci_data *pci_priv)
{
	pci_priv->drv_supported = false;
}

int cnss_pci_dsp_link_control(struct cnss_pci_data *pci_priv,
			      bool link_enable)
{
	return -EOPNOTSUPP;
}

int cnss_pci_set_dsp_link_status(struct cnss_pci_data *pci_priv,
				 bool link_enable)
{
	return -EOPNOTSUPP;
}

int cnss_pci_get_dsp_link_status(struct cnss_pci_data *pci_priv)
{
	return -EOPNOTSUPP;
}

int cnss_pci_dsp_link_enable(struct cnss_pci_data *pci_priv)
{
	return -EOPNOTSUPP;
}

int cnss_pci_dsp_link_retrain(struct cnss_pci_data *pci_priv,
			      u16 target_link_speed)
{
	return -EOPNOTSUPP;
}
#endif /* CONFIG_PCI_MSM */

static inline bool cnss_pci_get_drv_supported(struct cnss_pci_data *pci_priv)
{
	return pci_priv->drv_supported;
}

/**
 * cnss_pci_is_sync_probe(): check whether PCIe device
 * need to be present before registering cnss_pci_driver
 *
 * Currently SCMI power/PCIe enumeration is controlled
 * by low level GearVM system, and upstream PCIe driver
 * doesn't export enumeration API, like msm_pci_enumerate.
 * So we have to power wlan power before PCIe, otherwise
 * there doesn't have chances to do link training for wlan.
 * It means PCIe wlan device isn't ready when register
 * cnss_pci_driver. On the contrary, PCIe device should
 * be present in downstream MSM PCIe driver when register
 * cnss_pci_driver. This API is used to distinguish
 * downstream/upstream PCIe driver case.
 *
 * Return: true for sync mode, false for unsync mode
 */
bool cnss_pci_is_sync_probe(void);

#endif /* _CNSS_PCI_PLATFORM_H*/
