/*
 * wpa_supplicant - Robust AV procedures
 * Copyright (c) 2020, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "common/wpa_ctrl.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "bss.h"


void wpas_populate_mscs_descriptor_ie(struct robust_av_data *robust_av,
				      struct wpabuf *buf)
{
	u8 *len, *len1;

	/* MSCS descriptor element */
	wpabuf_put_u8(buf, WLAN_EID_EXTENSION);
	len = wpabuf_put(buf, 1);
	wpabuf_put_u8(buf, WLAN_EID_EXT_MSCS_DESCRIPTOR);
	wpabuf_put_u8(buf, robust_av->request_type);
	wpabuf_put_u8(buf, robust_av->up_bitmap);
	wpabuf_put_u8(buf, robust_av->up_limit);
	wpabuf_put_le32(buf, robust_av->stream_timeout);

	if (robust_av->request_type != SCS_REQ_REMOVE) {
		/* TCLAS mask element */
		wpabuf_put_u8(buf, WLAN_EID_EXTENSION);
		len1 = wpabuf_put(buf, 1);
		wpabuf_put_u8(buf, WLAN_EID_EXT_TCLAS_MASK);

		/* Frame classifier */
		wpabuf_put_data(buf, robust_av->frame_classifier,
				robust_av->frame_classifier_len);
		*len1 = (u8 *) wpabuf_put(buf, 0) - len1 - 1;
	}

	*len = (u8 *) wpabuf_put(buf, 0) - len - 1;
}


int wpas_send_mscs_req(struct wpa_supplicant *wpa_s)
{
	struct wpabuf *buf;
	const u8 *ext_capab = NULL;
	size_t buf_len;
	int ret;

	if (wpa_s->wpa_state != WPA_COMPLETED || !wpa_s->current_ssid)
		return 0;

	if (wpa_s->current_bss)
		ext_capab = wpa_bss_get_ie(wpa_s->current_bss,
					   WLAN_EID_EXT_CAPAB);

	if (!ext_capab || ext_capab[1] < 11 || !(ext_capab[12] & 0x20)) {
		wpa_dbg(wpa_s, MSG_INFO,
			"AP does not support MSCS - could not send MSCS Req");
		return -1;
	}

	buf_len = 3 +	/* Action frame header */
		  3 +	/* MSCS descriptor IE header */
		  1 +	/* Request type */
		  2 +	/* User priority control */
		  4 +	/* Stream timeout */
		  3 +	/* TCLAS Mask IE header */
		  wpa_s->robust_av.frame_classifier_len;

	buf = wpabuf_alloc(buf_len);
	if (!buf) {
		wpa_printf(MSG_ERROR, "Failed to allocate MSCS req");
		return -1;
	}

	wpabuf_put_u8(buf, WLAN_ACTION_ROBUST_AV_STREAMING);
	wpabuf_put_u8(buf, ROBUST_AV_MSCS_REQ);
	wpa_s->robust_av.dialog_token++;
	wpabuf_put_u8(buf, wpa_s->robust_av.dialog_token);

	/* MSCS descriptor element */
	wpas_populate_mscs_descriptor_ie(&wpa_s->robust_av, buf);

	wpa_hexdump_buf(MSG_MSGDUMP, "MSCS Request", wpabuf_head(buf));
	ret = wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				  wpa_s->own_addr, wpa_s->bssid,
				  wpabuf_head(buf), wpabuf_len(buf), 0);
	if (ret < 0)
		wpa_dbg(wpa_s, MSG_INFO, "MSCS: Failed to send MSCS Request");

	wpabuf_free(buf);
	return ret;
}
