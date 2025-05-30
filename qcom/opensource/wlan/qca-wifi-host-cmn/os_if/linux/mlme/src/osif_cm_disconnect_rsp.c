/*
 * Copyright (c) 2012-2015, 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: osif_cm_disconnect_rsp.c
 *
 * This file maintains definitaions of disconnect response
 * functions.
 */

#include <wlan_cfg80211.h>
#include <linux/wireless.h>
#include "osif_cm_rsp.h"
#include "wlan_osif_priv.h"
#include "osif_cm_util.h"
#include "wlan_mlo_mgr_sta.h"

#define DRIVER_DISCONNECT_REASON \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_DRIVER_DISCONNECT_REASON
#define DRIVER_DISCONNECT_REASON_INDEX \
	QCA_NL80211_VENDOR_SUBCMD_DRIVER_DISCONNECT_REASON_INDEX
/**
 * osif_validate_disconnect_and_reset_src_id() - Validate disconnection
 * and resets source and id
 * @osif_priv: Pointer to vdev osif priv
 * @rsp: Disconnect response from connectin manager
 *
 * This function validates disconnect response and if the disconnect
 * response is valid, resets the source and id of the command
 *
 * Context: Any context. Takes and releases cmd id spinlock.
 * Return: QDF_STATUS
 */

static QDF_STATUS
osif_validate_disconnect_and_reset_src_id(struct vdev_osif_priv *osif_priv,
					  struct wlan_cm_discon_rsp *rsp)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/* Always drop internal disconnect */
	qdf_spinlock_acquire(&osif_priv->cm_info.cmd_id_lock);
	if (rsp->req.req.source == CM_INTERNAL_DISCONNECT ||
	    rsp->req.req.source == CM_MLO_ROAM_INTERNAL_DISCONNECT ||
	    ucfg_cm_is_link_switch_disconnect_resp(rsp)) {
		osif_debug("ignore internal disconnect");
		status = QDF_STATUS_E_INVAL;
		goto rel_lock;
	}

	/*
	 * Send to kernel only if last osif cmd type is disconnect and
	 * cookie match else drop. If cookie match reset the cookie
	 * and source
	 */
	if (rsp->req.cm_id != osif_priv->cm_info.last_id ||
	    rsp->req.req.source != osif_priv->cm_info.last_source) {
		osif_debug("Ignore as cm_id(0x%x)/src(%d) didn't match stored cm_id(0x%x)/src(%d)",
			   rsp->req.cm_id, rsp->req.req.source,
			   osif_priv->cm_info.last_id,
			   osif_priv->cm_info.last_source);
		status = QDF_STATUS_E_INVAL;
		goto rel_lock;
	}

	osif_cm_reset_id_and_src_no_lock(osif_priv);
rel_lock:
	qdf_spinlock_release(&osif_priv->cm_info.cmd_id_lock);

	return status;
}

#if defined(CFG80211_DISCONNECTED_V2) || \
(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0))
#ifdef CONN_MGR_ADV_FEATURE
static void
osif_cm_indicate_disconnect_result(struct net_device *dev,
				   enum ieee80211_reasoncode reason,
				   const u8 *ie, size_t ie_len,
				   bool locally_generated, int link_id,
				   gfp_t gfp)
{
	cfg80211_disconnected(dev, reason, ie,
			      ie_len, locally_generated, gfp);
}
#else
#ifdef WLAN_SUPPORT_CFG80211_DISCONNECT_LINK_PARAM
static void
osif_cm_indicate_disconnect_result(struct net_device *dev,
				   enum ieee80211_reasoncode reason,
				   const u8 *ie, size_t ie_len,
				   bool locally_generated, int link_id,
				   gfp_t gfp)
{
	cfg80211_disconnected(dev, reason, ie,
			      ie_len, locally_generated, link_id, gfp);
}
#else
static void
osif_cm_indicate_disconnect_result(struct net_device *dev,
				   enum ieee80211_reasoncode reason,
				   const u8 *ie, size_t ie_len,
				   bool locally_generated, int link_id,
				   gfp_t gfp)
{
	cfg80211_disconnected(dev, reason, ie,
			      ie_len, locally_generated, gfp);
}
#endif /* WLAN_SUPPORT_CFG80211_DISCONNECT_LINK_PARAM */
#endif

#ifdef WLAN_FEATURE_11BE_MLO
#ifdef WLAN_FEATURE_11BE_MLO_ADV_FEATURE
void
osif_cm_indicate_disconnect(struct wlan_objmgr_vdev *vdev,
			    struct net_device *dev,
			    enum ieee80211_reasoncode reason,
			    bool locally_generated, const u8 *ie,
			    size_t ie_len, int link_id, gfp_t gfp)
{
	if (wlan_vdev_mlme_is_mlo_vdev(vdev)) {
		if (!wlan_vdev_mlme_is_mlo_link_vdev(vdev))
			osif_cm_indicate_disconnect_result(
					dev, reason, ie,
					ie_len, locally_generated,
					link_id, gfp);
	} else {
		osif_cm_indicate_disconnect_result(
				dev, reason, ie,
				ie_len, locally_generated,
				link_id, gfp);
	}
}
#else /* WLAN_FEATURE_11BE_MLO_ADV_FEATURE */

/**
 * osif_cm_get_anchor_vdev() - API to get the anchor vdev
 * @vdev: Pointer to vdev
 *
 * Return: If the assoc vdev is available, return it. Otherwise, if the MLD is
 * disconnected, return the current vdev. If neither is available, return NULL.
 */
static struct wlan_objmgr_vdev *osif_cm_get_anchor_vdev(
		struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_vdev *assoc_vdev = NULL;

	assoc_vdev = ucfg_mlo_get_assoc_link_vdev(vdev);
	if (assoc_vdev)
		return assoc_vdev;
	else if (ucfg_mlo_is_mld_disconnected(vdev))
		return vdev;
	else
		return NULL;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 213)) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0))
/**
 * osif_cm_indicate_disconnect_for_non_assoc_link() - Wrapper API to clear
 * current bss param of non-assoc link
 * @netdev: Pointer to netdev of non-assoc link vdev
 * @vdev: Pointer to non-assoc link vdev
 *
 * Return: None
 */
static void osif_cm_indicate_disconnect_for_non_assoc_link(
		struct net_device *netdev,
		struct wlan_objmgr_vdev *vdev)
{
	int ret;

	ret = cfg80211_clear_current_bss(netdev);
	if (ret)
		osif_err("cfg80211_clear_current_bss failed for psoc:%d pdev:%d vdev:%d",
			 wlan_vdev_get_psoc_id(vdev),
			 wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev)),
			 wlan_vdev_get_id(vdev));
}
#else
static void osif_cm_indicate_disconnect_for_non_assoc_link(
		struct net_device *netdev,
		struct wlan_objmgr_vdev *vdev)
{
}
#endif

void
osif_cm_indicate_disconnect(struct wlan_objmgr_vdev *vdev,
			    struct net_device *dev,
			    enum ieee80211_reasoncode reason,
			    bool locally_generated, const u8 *ie,
			    size_t ie_len, int link_id, gfp_t gfp)
{
	struct net_device *netdev = dev;
	struct vdev_osif_priv *osif_priv = NULL;
	struct wlan_objmgr_vdev *anchor_vdev;

	if (!wlan_vdev_mlme_is_mlo_vdev(vdev) || (link_id != -1)) {
		osif_cm_indicate_disconnect_result(
				netdev, reason, ie, ie_len,
				locally_generated, link_id, gfp);
		return;
	}

	anchor_vdev = osif_cm_get_anchor_vdev(vdev);

	if (vdev != anchor_vdev)
		osif_cm_indicate_disconnect_for_non_assoc_link(netdev, vdev);

	if (anchor_vdev && ucfg_mlo_is_mld_disconnected(vdev)) {
		/**
		 * Kernel maintains some extra state on the assoc netdev.
		 * If the assoc vdev exists, send disconnected event on the
		 * assoc netdev so that kernel cleans up the extra state.
		 * If the assoc vdev was already removed, kernel would have
		 * already cleaned up the extra state while processing the
		 * disconnected event sent as part of the link removal.
		 */
		osif_priv = wlan_vdev_get_ospriv(anchor_vdev);
		netdev = osif_priv->wdev->netdev;

		osif_cm_indicate_disconnect_result(
				netdev, reason,
				ie, ie_len,
				locally_generated, link_id, gfp);
	}
}
#endif /* WLAN_FEATURE_11BE_MLO_ADV_FEATURE */
#else /* WLAN_FEATURE_11BE_MLO */
void
osif_cm_indicate_disconnect(struct wlan_objmgr_vdev *vdev,
			    struct net_device *dev,
			    enum ieee80211_reasoncode reason,
			    bool locally_generated, const u8 *ie,
			    size_t ie_len, int link_id, gfp_t gfp)
{
	osif_cm_indicate_disconnect_result(dev, reason, ie,
					   ie_len, locally_generated,
					   link_id, gfp);
}
#endif /* WLAN_FEATURE_11BE_MLO */
#else
void
osif_cm_indicate_disconnect(struct wlan_objmgr_vdev *vdev,
			    struct net_device *dev,
			    enum ieee80211_reasoncode reason,
			    bool locally_generated, const u8 *ie,
			    size_t ie_len, int link_id, gfp_t gfp)
{
	cfg80211_disconnected(dev, reason, ie, ie_len, gfp);
}
#endif

static enum ieee80211_reasoncode
osif_cm_get_disconnect_reason(struct vdev_osif_priv *osif_priv, uint16_t reason)
{
	enum ieee80211_reasoncode ieee80211_reason = WLAN_REASON_UNSPECIFIED;

	if (reason < REASON_PROP_START)
		ieee80211_reason = reason;
	/*
	 * Applications expect reason code as 0 for beacon miss failure
	 * due to backward compatibility. So send ieee80211_reason as 0.
	 */
	if (reason == REASON_BEACON_MISSED)
		ieee80211_reason = 0;

	return ieee80211_reason;
}

#ifdef CONN_MGR_ADV_FEATURE
static inline bool
osif_is_disconnect_locally_generated(struct wlan_cm_discon_rsp *rsp)
{
	if (rsp->req.req.source == CM_PEER_DISCONNECT)
		return false;

	return true;
}
#else
static inline bool
osif_is_disconnect_locally_generated(struct wlan_cm_discon_rsp *rsp)
{
	if (rsp->req.req.source == CM_PEER_DISCONNECT ||
	    rsp->req.req.source == CM_SB_DISCONNECT)
		return false;

	return true;
}
#endif

#ifdef CONN_MGR_ADV_FEATURE
/**
 * osif_cm_indicate_qca_reason: Send driver disconnect reason to user space
 * @osif_priv: osif_priv pointer
 * @qca_reason: qca disconnect reason codes
 *
 * Return: void
 */

static void
osif_cm_indicate_qca_reason(struct vdev_osif_priv *osif_priv,
			    enum qca_disconnect_reason_codes qca_reason)
{
	struct sk_buff *vendor_event;

	vendor_event = wlan_cfg80211_vendor_event_alloc(
					osif_priv->wdev->wiphy, osif_priv->wdev,
					NLMSG_HDRLEN + sizeof(qca_reason) +
					NLMSG_HDRLEN,
					DRIVER_DISCONNECT_REASON_INDEX,
					GFP_KERNEL);
	if (!vendor_event) {
		osif_err("cfg80211_vendor_event_alloc failed");
		return;
	}
	if (nla_put_u32(vendor_event, DRIVER_DISCONNECT_REASON, qca_reason)) {
		osif_err("DISCONNECT_REASON put fail");
		kfree_skb(vendor_event);
		return;
	}

	wlan_cfg80211_vendor_event(vendor_event, GFP_KERNEL);
}
#else
static inline void
osif_cm_indicate_qca_reason(struct vdev_osif_priv *osif_priv,
			    enum qca_disconnect_reason_codes qca_reason)
{
}
#endif

QDF_STATUS osif_disconnect_handler(struct wlan_objmgr_vdev *vdev,
				   struct wlan_cm_discon_rsp *rsp)
{
	enum ieee80211_reasoncode ieee80211_reason;
	struct vdev_osif_priv *osif_priv = wlan_vdev_get_ospriv(vdev);
	bool locally_generated;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	enum qca_disconnect_reason_codes qca_reason;
	int link_id = -1;

	qca_reason = osif_cm_mac_to_qca_reason(rsp->req.req.reason_code);
	ieee80211_reason =
		osif_cm_get_disconnect_reason(osif_priv,
					      rsp->req.req.reason_code);

	locally_generated = osif_is_disconnect_locally_generated(rsp);

	osif_nofl_info("%s(vdevid-%d): " QDF_MAC_ADDR_FMT " %s disconnect " QDF_MAC_ADDR_FMT " cmid 0x%x src %d reason:%u %s vendor:%u %s",
		       osif_priv->wdev->netdev->name,
		       rsp->req.req.vdev_id,
		       QDF_MAC_ADDR_REF(wlan_vdev_mlme_get_macaddr(vdev)),
		       locally_generated ? "locally-generated" : "",
		       QDF_MAC_ADDR_REF(rsp->req.req.bssid.bytes),
		       rsp->req.cm_id, rsp->req.req.source, ieee80211_reason,
		       ucfg_cm_reason_code_to_str(rsp->req.req.reason_code),
		       qca_reason,
		       osif_cm_qca_reason_to_str(qca_reason));

	/* Unlink bss if disconnect is from peer or south bound */
	if (rsp->req.req.source == CM_PEER_DISCONNECT ||
	    rsp->req.req.source == CM_SB_DISCONNECT)
		osif_cm_unlink_bss(vdev, &rsp->req.req.bssid);

	status = osif_validate_disconnect_and_reset_src_id(osif_priv, rsp);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_cm_disconnect_comp_ind(vdev, rsp, OSIF_NOT_HANDLED);
		return status;
	}

	/* Send driver disconnect Reason */
	osif_cm_indicate_qca_reason(osif_priv, qca_reason);

	/* If disconnect due to ML Reconfig, fill link id */
	if (rsp->req.req.reason_code == REASON_HOST_TRIGGERED_LINK_DELETE)
		link_id = wlan_vdev_get_link_id(vdev);

	osif_cm_disconnect_comp_ind(vdev, rsp, OSIF_PRE_USERSPACE_UPDATE);
	osif_cm_indicate_disconnect(vdev, osif_priv->wdev->netdev,
				    ieee80211_reason,
				    locally_generated, rsp->ap_discon_ie.ptr,
				    rsp->ap_discon_ie.len,
				    link_id,
				    qdf_mem_malloc_flags());

	osif_cm_disconnect_comp_ind(vdev, rsp, OSIF_POST_USERSPACE_UPDATE);

	return status;
}
