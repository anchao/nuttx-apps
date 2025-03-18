/***************************************************************************
 *
 * Copyright 2015-2024 BES.
 * All rights reserved. All unpublished rights reserved.
 *
 * No part of this work may be used or reproduced in any form or by any
 * means, or stored in a database or retrieval system, without prior written
 * permission of BES.
 *
 * Use of this work is governed by a license granted by BES.
 * This work contains confidential and proprietary information of
 * BES. which is protected by copyright, trade secret,
 * trademark and other intellectual property rights.
 *
 * @brief xxx.
 *
 ****************************************************************************/
#define LOG_TAG "sal_bes_avrcp"

/****************************** header include ********************************/
#include <stdint.h> // For uint8_t, uint32_t types
#include "bt_status.h"
#include "bluetooth.h"
#include "utils/log.h"

#include "sal_avrcp_control_interface.h"
#include "sal_avrcp_target_interface.h"

#include "api/bth_api_avrcp.h"

/***************************** external declaration *****************************/

/***************************** macro defination *******************************/

/*****************************  type defination ********************************/

/*****************************  variable defination *****************************/

uint8_t abs_volume_label;

/*****************************  function declaration ****************************/

static void bes_sal_avcp_ct_passthrough_rsp_cb(const bth_address_t* bd_addr, int id, int key_state)
{
    BT_LOGD("id = %d key_state = %d", id, key_state);

    avrcp_msg_t *msg = avrcp_msg_new(AVRC_REGISTER_NOTIFICATION_RSP, (bt_address_t *)bd_addr);
    msg->data.passthr_rsp.rsp = id;

    msg->data.passthr_rsp.rsp = AVRCP_RESPONSE_ACCEPTED;
    if (key_state == BTH_RC_KEY_STATE_PRESSED) {
        msg->data.passthr_rsp.state = AVRCP_KEY_PRESSED;
    } else {
        msg->data.passthr_rsp.state = AVRCP_KEY_RELEASED;
    }

    bt_sal_avrcp_control_event_callback(msg);
}

static void bes_sal_avcp_ct_conn_state_cb(bool rc_connect, bool br_connect, const bth_address_t* bd_addr)
{
    BT_LOGD("rc connect %d br connect %d", rc_connect, br_connect);
    avrcp_msg_t *msg = avrcp_msg_new(AVRC_CONNECTION_STATE_CHANGED, (bt_address_t *)bd_addr);
    msg->data.conn_state.reason = PROFILE_REASON_UNSPECIFIED;
    msg->data.conn_state.conn_state = rc_connect ? PROFILE_STATE_CONNECTED : PROFILE_STATE_DISCONNECTED;
    bt_sal_avrcp_control_event_callback(msg);
}

static void bes_sal_avcp_ct_get_cap_cb(const bth_address_t* bd_addr, bth_rc_capabilities_t *cap)
{
    avrcp_msg_t *msg = avrcp_msg_new(AVRC_GET_CAPABILITY_RSP, (bt_address_t *)bd_addr);
    msg->data.cap.cap_count = cap->count;
    for(int i = 0; i < cap->count; i++) {
        msg->data.cap.capabilities[i] = cap->capabilities[i];
    }
    bt_sal_avrcp_control_event_callback(msg);
}

static void bes_sal_avcp_ct_setabsvol_cmd_cb(const bth_address_t* bd_addr, uint8_t abs_vol, uint8_t label)
{
    UNUSED(label);
    BT_LOGD("abs vol %d", abs_vol)
    avrcp_msg_t *msg = avrcp_msg_new(AVRC_SET_ABSOLUTE_VOLUME, (bt_address_t *)bd_addr);
    msg->data.absvol.volume = abs_vol;
    bt_sal_avrcp_control_event_callback(msg);
}

static void bes_sal_avcp_ct_reg_abs_vol_cb(const bth_address_t* bd_addr, uint8_t label)
{
    BT_LOGD("volume changed notify label = %d", label);
    //TODO vela framework not process this message
    abs_volume_label = label;
    avrcp_msg_t *msg = avrcp_msg_new(AVRC_REGISTER_NOTIFICATION_REQ, (bt_address_t *)bd_addr);
    msg->data.notify_req.event = NOTIFICATION_EVT_VOLUME_CHANGED;
    bt_sal_avrcp_control_event_callback(msg);
}

static void bes_sal_avcp_ct_get_element_attrs_cb(const bth_address_t* bd_addr, uint8_t num_attr, bth_rc_element_attr_val_t* p_attrs)
{
    BT_LOGD("nocopy get_element_attrs num attrs = %d", num_attr);

    avrcp_msg_t *msg = avrcp_msg_new(AVRC_GET_ELEMENT_ATTRIBUTES_RSP, (bt_address_t*)bd_addr);
    rc_element_attrs_t *attrs = &msg->data.attrs;
    attrs->count = num_attr;

    for (int i = 0; i < num_attr; i++, p_attrs++) {
        attrs->chr_sets[i] = p_attrs->char_set;
        attrs->types[i] = p_attrs->attr_id;
        attrs->attrs[i] = p_attrs->text == NULL ? NULL : strdup((char *)p_attrs->text);
    }

    bt_sal_avrcp_control_event_callback(msg);
}

static void bes_sal_avcp_ct_track_chg_cb(const bth_address_t* bd_addr, uint8_t *uid)
{
    UNUSED(uid);
    BT_LOGD("");
    avrcp_msg_t *msg = avrcp_msg_new(AVRC_REGISTER_NOTIFICATION_RSP, (bt_address_t*)bd_addr);
    msg->data.notify_rsp.event = NOTIFICATION_EVT_TRACK_CHANGED;
    bt_sal_avrcp_control_event_callback(msg);
}

static void bes_sal_avcp_ct_play_pos_chg_cb(const bth_address_t* bd_addr, uint32_t song_pos)
{
    BT_LOGD("song pos %ld", song_pos);
    avrcp_msg_t *msg = avrcp_msg_new(AVRC_REGISTER_NOTIFICATION_RSP, (bt_address_t*)bd_addr);
    msg->data.notify_rsp.event = NOTIFICATION_EVT_PLAY_POS_CHANGED;
    msg->data.notify_rsp.value = song_pos;
    bt_sal_avrcp_control_event_callback(msg);
}

static void bes_sal_avcp_ct_get_play_status_cb(const bth_address_t* bd_addr, BTH_RC_PLAY_STATUS_TYPE_E status,
                                               uint32_t song_pos, uint32_t song_len)
{
    BT_LOGD("status %d song pos %ld song len %ld", status, song_pos, song_len);
    avrcp_msg_t *msg = avrcp_msg_new(AVRC_GET_PLAY_STATUS_RSP, (bt_address_t *)bd_addr);
    msg->data.playstatus.status = (uint8_t)status;
    msg->data.playstatus.song_len = song_len;
    msg->data.playstatus.song_pos = song_pos;
    bt_sal_avrcp_control_event_callback(msg);
}

static void bes_sal_avcp_ct_play_status_chg_cb(const bth_address_t* bd_addr, BTH_RC_PLAY_STATUS_TYPE_E play_status)
{
    BT_LOGD("play status %d", play_status);
    avrcp_msg_t *msg = avrcp_msg_new(AVRC_REGISTER_NOTIFICATION_RSP, (bt_address_t *)bd_addr);
    msg->data.notify_rsp.event = NOTIFICATION_EVT_PALY_STATUS_CHANGED;
    msg->data.notify_rsp.value = play_status;
    bt_sal_avrcp_control_event_callback(msg);
}

static bth_rc_ctrl_callbacks_t bes_sal_avcp_ct_cb =
{
    .passthrough_rsp_cb                  = bes_sal_avcp_ct_passthrough_rsp_cb,
    .connection_state_cb                 = bes_sal_avcp_ct_conn_state_cb,
    .get_rc_cap_cb                       = bes_sal_avcp_ct_get_cap_cb,
    .set_abs_vol_cmd_cb                  = bes_sal_avcp_ct_setabsvol_cmd_cb,
    .reg_abs_vol_cb                      = bes_sal_avcp_ct_reg_abs_vol_cb,
    .track_changed_cb                    = bes_sal_avcp_ct_track_chg_cb,
    .play_pos_changed_cb                 = bes_sal_avcp_ct_play_pos_chg_cb,
    .play_status_changed_cb              = bes_sal_avcp_ct_play_status_chg_cb,
    .get_element_attr_cb                 = bes_sal_avcp_ct_get_element_attrs_cb,
    .get_play_status_cb                  = bes_sal_avcp_ct_get_play_status_cb,
    .get_features_cb                     = NULL,
    .group_nav_rsp_cb                    = NULL,
    .get_folder_items_cb                 = NULL,
    .change_folder_path_cb               = NULL,
    .set_browsed_player_cb               = NULL,
    .set_addressed_player_cb             = NULL,
    .addressed_player_changed_cb         = NULL,
    .now_playing_contents_changed_cb     = NULL,
    .available_player_changed_cb         = NULL,
    .get_cover_art_psm_cb                = NULL,
    .set_player_setting_rsp_cb           = NULL,
    .player_setting_cb                   = NULL,
    .player_setting_changed_cb           = NULL,
};

static void bes_sal_avcp_tg_conn_state_cb(bool rc_connect, bool br_connect, const bth_address_t* bd_addr)
{
    BT_LOGD("rc connect %d br connect %d", rc_connect, br_connect);
    avrcp_msg_t *msg = avrcp_msg_new(AVRC_CONNECTION_STATE_CHANGED, (bt_address_t *)bd_addr);
    msg->data.conn_state.reason = PROFILE_REASON_UNSPECIFIED;
    msg->data.conn_state.conn_state = rc_connect ? PROFILE_STATE_CONNECTED : PROFILE_STATE_DISCONNECTED;
    bt_sal_avrcp_target_event_callback(msg);
}

static void bes_sal_avcp_tg_get_play_status_cb(const bth_address_t* bd_addr)
{
    BT_LOGD("");
    avrcp_msg_t *msg = avrcp_msg_new(AVRC_GET_PLAY_STATUS_REQ, (bt_address_t*)bd_addr);
    bt_sal_avrcp_target_event_callback(msg);
}

static void bes_sal_avcp_tg_reg_notification_cb(BTH_RC_EVENT_ID_TYPE_E event_id,
    uint32_t param, const bth_address_t* bd_addr)
{
    BT_LOGD("event id = %d interval = %u", event_id, param);
    avrcp_msg_t *msg = avrcp_msg_new(AVRC_REGISTER_NOTIFICATION_REQ, (bt_address_t*)bd_addr);
    switch (event_id) {
        case BTH_RC_EVT_PLAY_STATUS_CHANGED:
            msg->data.notify_req.event = NOTIFICATION_EVT_PALY_STATUS_CHANGED;
            break;
        case BTH_RC_EVT_TRACK_CHANGE:
            msg->data.notify_req.event = NOTIFICATION_EVT_TRACK_CHANGED;
            break;
        case BTH_RC_EVT_PLAY_POS_CHANGED:
            msg->data.notify_req.event = NOTIFICATION_EVT_PLAY_POS_CHANGED;
            msg->data.notify_req.interval = param;
        case BTH_RC_EVT_VOL_CHANGED:
            msg->data.notify_req.event = NOTIFICATION_EVT_VOLUME_CHANGED;
            break;
        default:
            return;
    }
    bt_sal_avrcp_target_event_callback(msg);
}

static void bes_sal_avcp_tg_passthrough_cmd_cb(int id, int key_state, const bth_address_t* bd_addr)
{
    BT_LOGD("id %d key state %d", id, key_state);

    avrcp_msg_t *msg = avrcp_msg_new(AVRC_PASSTHROUHT_CMD, (bt_address_t*)bd_addr);
    msg->data.passthr_cmd.opcode = id;
    msg->data.passthr_cmd.state = key_state == BTH_RC_KEY_STATE_PRESSED ? AVRCP_KEY_PRESSED : AVRCP_KEY_RELEASED;
    bt_sal_avrcp_target_event_callback(msg);
}

static bth_rc_tg_callbacks_t bes_sal_avcp_tg_cb =
{
    .connection_state_cb           = bes_sal_avcp_tg_conn_state_cb,
    .get_play_status_cb            = bes_sal_avcp_tg_get_play_status_cb,
    .register_notification_cb      = bes_sal_avcp_tg_reg_notification_cb,
    .passthrough_cmd_cb            = bes_sal_avcp_tg_passthrough_cmd_cb,
    .get_element_attr_cb           = NULL,
    .volume_change_cb              = NULL,
    .list_player_app_attr_cb       = NULL,
    .list_player_app_values_cb     = NULL,
    .get_player_app_value_cb       = NULL,
    .get_player_app_attrs_text_cb  = NULL,
    .get_player_app_values_text_cb = NULL,
    .set_player_app_value_cb       = NULL,
    .set_addressed_player_cb       = NULL,
    .set_browsed_player_cb         = NULL,
    .get_folder_items_cb           = NULL,
    .change_path_cb                = NULL,
    .get_item_attr_cb              = NULL,
    .play_item_cb                  = NULL,
    .get_total_num_of_items_cb     = NULL,
    .search_cb                     = NULL,
    .add_to_now_playing_cb         = NULL,
    .remote_features_cb            = NULL,
};

bt_status_t bt_sal_avrcp_control_init(void)
{
    int ret;

    bes_sal_avcp_ct_cb.size = sizeof(bes_sal_avcp_ct_cb);
    ret = bth_rc_ct_init(&bes_sal_avcp_ct_cb);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

void bt_sal_avrcp_control_cleanup(void)
{
    bth_cleanup_rc_ct();
}

bt_status_t bt_sal_avrcp_control_send_pass_through_cmd(bt_controller_id_t id,
                                                       bt_address_t* bd_addr,
                                                       avrcp_passthr_cmd_t key_code,
                                                       avrcp_key_state_t key_state)
{
    UNUSED(id);
    int ret;

    ret = bth_rc_ct_send_passthrough_cmd((bth_address_t*)bd_addr, key_code, key_state);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_avrcp_control_get_playback_state(bt_controller_id_t id, bt_address_t* bd_addr)
{
    UNUSED(id);
    int ret;

    ret = bth_rc_ct_get_playback_state_cmd((bth_address_t*)bd_addr);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_avrcp_control_volume_changed_notify(bt_controller_id_t id,
                                                       bt_address_t* bd_addr,
                                                       uint8_t volume)
{
    UNUSED(id);
    int ret;

    ret = bth_rc_ct_volume_change_notification_rsp((bth_address_t*)bd_addr,
            BTH_RC_NOTIFICATION_TYPE_CHANGED, volume, abs_volume_label);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_avrcp_control_connect(bt_controller_id_t id, bt_address_t* bd_addr)
{
    UNUSED(id);
    bth_bt_status_t ret;

    ret = bth_rc_connect((bth_address_t*)bd_addr);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_avrcp_control_disconnect(bt_controller_id_t id, bt_address_t* bd_addr)
{
    UNUSED(id);
    bth_bt_status_t ret;

    ret = bth_rc_disconnect((bth_address_t*)bd_addr);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_avrcp_control_get_capabilities(bt_controller_id_t id,
                                                  bt_address_t* bd_addr,
                                                  uint8_t cap_id)
{
    UNUSED(id);
    bth_bt_status_t ret;

    ret = bth_rc_ct_get_capabilities((bth_address_t*)bd_addr, cap_id);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_avrcp_control_register_notification(bt_controller_id_t id,
                                                       bt_address_t* bd_addr,
                                                       avrcp_notification_event_t event,
                                                       uint32_t interval)
{
    UNUSED(id);
    UNUSED(bd_addr);
    UNUSED(event);
    UNUSED(interval);
    //register by stack
    BT_LOGD("reg event %d", event);
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_avrcp_control_get_element_attributes(bt_controller_id_t id,
                                                        bt_address_t* bd_addr,
                                                        uint8_t attrs_count,
                                                        avrcp_media_attr_type_t* types)
{
    UNUSED(id);
    BT_LOGD("attrs count = %d", attrs_count);
    bth_bt_status_t ret;

    ret = bth_rc_ct_get_element_attr_cmd((bth_address_t*)bd_addr, attrs_count, (BTH_RC_MEDIA_ATTR_TYPE_E *)types);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_avrcp_target_init(void)
{
    int ret;

    bes_sal_avcp_tg_cb.size = sizeof(bes_sal_avcp_tg_cb);
    ret = bth_rc_tg_init(&bes_sal_avcp_tg_cb);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

void bt_sal_avrcp_target_cleanup(void)
{
    bth_cleanup_rc_tg();
}

bt_status_t bt_sal_avrcp_target_get_play_status_rsp(bt_controller_id_t id,
                                                    bt_address_t* addr,
                                                    avrcp_play_status_t status,
                                                    uint32_t song_len,
                                                    uint32_t song_pos)
{
    UNUSED(id);
    int ret;

    ret = bth_rc_tg_get_play_status_rsp((bth_address_t*)addr, (BTH_RC_PLAY_STATUS_TYPE_E)status,song_len, song_pos);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_avrcp_target_play_status_notify(bt_controller_id_t id,
                                                   bt_address_t* addr,
                                                   avrcp_play_status_t status)
{
    UNUSED(id);

    int ret;
    bth_rc_register_notification_t p_param = {0};

    p_param.play_status = (BTH_RC_PLAY_STATUS_TYPE_E)status;
    ret = bth_rc_tg_register_notification_rsp((const bth_address_t*)addr, BTH_RC_EVT_PLAY_STATUS_CHANGED,
            BTH_RC_NOTIFICATION_TYPE_AUTO, &p_param);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_avrcp_target_set_absolute_volume(bt_controller_id_t id,
                                                    bt_address_t* addr,
                                                    uint8_t volume)
{
    UNUSED(id);
    int ret;

    ret = bth_rc_tg_set_volume((const bth_address_t*)addr, volume);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_avrcp_target_notify_track_changed(bt_controller_id_t id,
                                                     bt_address_t* addr,
                                                     bool selected)
{
    UNUSED(id);
    int ret;
    bth_rc_register_notification_t p_param = {0};
    uint8_t value = selected ? 0x00 : 0xFF;
    memset(p_param.track, value, BTH_RC_UID_SIZE);
    ret = bth_rc_tg_register_notification_rsp((const bth_address_t*)addr, BTH_RC_EVT_TRACK_CHANGE,
            BTH_RC_NOTIFICATION_TYPE_AUTO, &p_param);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;

}

bt_status_t bt_sal_avrcp_target_notify_play_position_changed(bt_controller_id_t id,
                                                             bt_address_t* addr,
                                                             uint32_t position)
{
    UNUSED(id);
    int ret;
    bth_rc_register_notification_t p_param = {0};

    p_param.song_pos = position;
    ret = bth_rc_tg_register_notification_rsp((const bth_address_t*)addr, BTH_RC_EVT_PLAY_POS_CHANGED,
            BTH_RC_NOTIFICATION_TYPE_AUTO, &p_param);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

//vela not used this function
bt_status_t bt_sal_avrcp_target_register_volume_changed(bt_controller_id_t id,
                                                        bt_address_t* addr)
{
    UNUSED(id);
    UNUSED(addr);
    return BT_STATUS_SUCCESS;
}
