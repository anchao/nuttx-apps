#define LOG_TAG "sal_hfp_ag"

#include <api/bth_api_hfp_ag.h>

#include <stdint.h> // For uint8_t type
#include "bt_status.h"
#include "utils/log.h"

#include "sal_hfp_ag_interface.h"

#define TO_BTH_ADDRESS(addr) (bth_address_t *)(addr)
#define TO_BT_ADDRESS(addr) (bt_address_t *)(addr)

void sal_hfp_ag_conn_state_callback(const bth_address_t* bd_addr, bth_hfp_ag_conn_state_t state, uint32_t remote_features)
{
    BT_LOGD("%s:",__func__);
    profile_connection_state_t conn_state;
    switch(state)
    {
        case BTH_HFP_AG_CONNECTION_STATE_DISCONNECTED:
            conn_state = PROFILE_STATE_DISCONNECTED;
            break;
        case BTH_HFP_AG_CONNECTION_STATE_CONNECTING:
            conn_state = PROFILE_STATE_CONNECTING;
            break;
        case BTH_HFP_AG_CONNECTION_STATE_CONNECTED:
            conn_state = PROFILE_STATE_CONNECTED;
            break;
        case BTH_HFP_AG_CONNECTION_STATE_DISCONNECTING:
            conn_state = PROFILE_STATE_DISCONNECTING;
            break;
        default:
            return;
    }
    hfp_ag_on_connection_state_changed(TO_BT_ADDRESS(bd_addr), conn_state, PROFILE_REASON_SUCCESS, remote_features);
}

void sal_hfp_ag_audio_state_callback(const bth_address_t* bd_addr, bth_hfp_audio_codec_t codec, bth_hfp_audio_state_t state, uint16_t sco_handle)
{
    BT_LOGD("%s:",__func__);
    hfp_codec_config_t config;
    config.codec = HFP_CODEC_UNKONWN;
    config.bit_width = 0;
    config.reserved1 = 0;
    config.reserved2 = 0;
    config.sample_rate = 0;
    switch (codec) {
        case BTH_HFP_AUDIO_CODEC_CVSD:
            config.codec = HFP_CODEC_CVSD;
            break;
        case BTH_HFP_AUDIO_CODEC_MSBC:
            config.codec = HFP_CODEC_MSBC;
            break;
        case BTH_HFP_AUDIO_CODEC_LC3:
            BT_LOGW("not supported");
        default:
            break;
    }
    hfp_ag_on_codec_changed(TO_BT_ADDRESS(bd_addr), &config);

    hfp_audio_state_t audio_state;
    switch(state)
    {
        case BTH_HFP_AUDIO_STATE_DISCONNECTED:
            audio_state = HFP_AUDIO_STATE_DISCONNECTED;
            break;
        case BTH_HFP_AUDIO_STATE_CONNECTING:
            audio_state = HFP_AUDIO_STATE_CONNECTING;
            break;
        case BTH_HFP_AUDIO_STATE_DISCONNECTING:
            audio_state = HFP_AUDIO_STATE_DISCONNECTING;
            break;
        case BTH_HFP_AUDIO_STATE_CONNECTED:
            audio_state = HFP_AUDIO_STATE_CONNECTED;
            break;
        default:
            BT_LOGE("unknown audio state %d", state);
            return;
    }
    hfp_ag_on_audio_state_changed(TO_BT_ADDRESS(bd_addr),  audio_state, sco_handle);
}

void sal_hfp_ag_vr_callback(const bth_address_t* bd_addr, bth_hfp_ag_vr_state_t state)
{

    BT_LOGD("%s:",__func__);
    hfp_ag_on_voice_recognition_state_changed(TO_BT_ADDRESS(bd_addr), state == BTH_HFP_AG_VR_STATE_STOPPED ? false : true);
}

void sal_hfp_ag_answer_call_callback(const bth_address_t* bd_addr)
{
    BT_LOGD("%s:",__func__);
    hfp_ag_on_answer_call(TO_BT_ADDRESS(bd_addr));
}

void sal_hfp_ag_hangup_call_callback(const bth_address_t* bd_addr)
{
    BT_LOGD("%s:",__func__);
    hfp_ag_on_hangup_call(TO_BT_ADDRESS(bd_addr));
}

void sal_hfp_ag_volume_control_callback(const bth_address_t* bd_addr, bth_hfp_volume_type_t type,
                                                   int volume)
{
    BT_LOGD("%s:",__func__);
    hfp_ag_on_volume_changed(TO_BT_ADDRESS(bd_addr), (hfp_volume_type_t) type, volume);
}

void sal_hfp_ag_dial_number_callback(const bth_address_t* bd_addr, const char* number)
{
    BT_LOGD("%s:",__func__);
    hfp_ag_on_dial_number(TO_BT_ADDRESS(bd_addr), (char*)number, number != NULL ? strlen(number) : 0);
}

void sal_hfp_ag_dial_memory_callback(const bth_address_t* bd_addr, uint32_t location)
{
    BT_LOGD("%s:",__func__);
    hfp_ag_on_dial_memory(TO_BT_ADDRESS(bd_addr), location);
}

void sal_hfp_ag_dtmf_cmd_callback(const bth_address_t* bd_addr, char tone)
{
    BT_LOGD("%s:",__func__);
    hfp_ag_on_received_dtmf(TO_BT_ADDRESS(bd_addr), tone);
}

void sal_hfp_ag_noise_reduction_callback(const bth_address_t* bd_addr, bth_hfp_ag_nrec_t nrec)
{
    BT_LOGD("%s:",__func__);
    hfp_ag_on_received_nrec_request(TO_BT_ADDRESS(bd_addr), nrec);
}

void sal_hfp_ag_wbs_callback(const bth_address_t* bd_addr, bth_hfp_ag_wbs_config_t wbs)
{
    BT_LOGD("%s:",__func__);
}

void sal_hfp_ag_swb_callback(const bth_address_t* bd_addr, bth_hfp_ag_swb_codec_t codec,
                                        bth_hfp_ag_swb_config_t swb)
{
    BT_LOGD("%s:",__func__);
}

void sal_hfp_ag_at_chld_callback(const bth_address_t* bd_addr, bth_hfp_ag_chld_type_t chld)
{
    BT_LOGD("%s:",__func__);
    hfp_ag_on_call_control(TO_BT_ADDRESS(bd_addr), (hfp_call_control_t) chld);
}

void sal_hfp_ag_at_cnum_callback(const bth_address_t* bd_addr)
{
    BT_LOGD("%s:",__func__);
}

void sal_hfp_ag_at_cind_callback(const bth_address_t* bd_addr)
{
    BT_LOGD("%s:",__func__);
    hfp_ag_on_received_cind_request(TO_BT_ADDRESS(bd_addr));
}

void sal_hfp_ag_at_cops_callback(const bth_address_t* bd_addr)
{
    BT_LOGD("%s:",__func__);
    hfp_ag_on_received_cops_request(TO_BT_ADDRESS(bd_addr));
}

void sal_hfp_ag_at_clcc_callback(const bth_address_t* bd_addr)
{
    BT_LOGD("%s:",__func__);
    hfp_ag_on_received_clcc_request(TO_BT_ADDRESS(bd_addr));
}

void sal_hfp_ag_unknown_at_callback(const bth_address_t* bd_addr, const char* at_string)
{
    BT_LOGD("%s:",__func__);
    hfp_ag_on_received_at_cmd(TO_BT_ADDRESS(bd_addr), (char*)at_string, at_string ? strlen(at_string) : 0);
}

void sal_hfp_ag_key_pressed_callback(const bth_address_t* bd_addr)
{
    BT_LOGD("%s:",__func__);
}

void sal_hfp_ag_at_bind_callback(const bth_address_t* bd_addr, const char* at_string)
{
    BT_LOGD("%s:",__func__);
}

void sal_hfp_ag_at_biev_callback(const bth_address_t* bd_addr, bth_hfp_ag_hf_ind_type_t ind_id,
                                            int ind_value)
{
    BT_LOGD("%s:",__func__);
    if (ind_id == BTH_HFP_AG_HF_IND_BATTERY_LEVEL_STATUS)
    {
        hfp_ag_on_remote_battery_level_update(TO_BT_ADDRESS(bd_addr), ind_value);
        return;
    }
    BT_LOGW("%s, unsuported indicators id = %d", __func__, ind_id);
}

void sal_hfp_ag_at_bia_callback(const bth_address_t* bd_addr, bool service, bool roam, bool signal, bool battery)
{
    BT_LOGD("%s:",__func__);
}

bth_hfp_ag_callback_t ag_callbacks =
{
    .answer_call_cb             = sal_hfp_ag_answer_call_callback,
    .hangup_call_cb             = sal_hfp_ag_hangup_call_callback,
    .at_bia_cb                  = sal_hfp_ag_at_bia_callback,
    .at_biev_cb                 = sal_hfp_ag_at_biev_callback,
    .at_bind_cb                 = sal_hfp_ag_at_bind_callback,
    .at_chld_cb                 = sal_hfp_ag_at_chld_callback,
    .at_cind_cb                 = sal_hfp_ag_at_cind_callback,
    .at_clcc_cb                 = sal_hfp_ag_at_clcc_callback,
    .at_cnum_cb                 = sal_hfp_ag_at_cnum_callback,
    .at_cops_cb                 = sal_hfp_ag_at_cops_callback,
    .audio_state_cb             = sal_hfp_ag_audio_state_callback,
    .dial_memory_cb             = sal_hfp_ag_dial_memory_callback,
    .dial_number_cb             = sal_hfp_ag_dial_number_callback,
    .conn_state_cb              = sal_hfp_ag_conn_state_callback,
    .dtmf_cmd_cb                = sal_hfp_ag_dtmf_cmd_callback,
    .key_pressed_cb             = sal_hfp_ag_key_pressed_callback,
    .wbs_cb                     = sal_hfp_ag_wbs_callback,
    .vr_cb                      = sal_hfp_ag_vr_callback,
    .swb_cb                     = sal_hfp_ag_swb_callback,
    .unknown_at_cb              = sal_hfp_ag_unknown_at_callback,
    .noise_reduction_cb         = sal_hfp_ag_noise_reduction_callback,
    .volume_control_cb          = sal_hfp_ag_volume_control_callback,
};

bt_status_t bt_sal_hfp_ag_init(uint32_t ag_feature, uint8_t max_connection)
{
    BT_LOGD("%s: ag_feature %lud max_conn %u", __func__, ag_feature, max_connection);
    bth_bt_status_t ret = BTH_STATUS_FAIL;
    ret = bth_hfp_ag_init(&ag_callbacks, max_connection, false);
    if (ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_cleanup()
{
    BT_LOGD("%s", __func__);
    bth_hfp_ag_cleanup();
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_connect(const bt_address_t *bd_addr)
{
    BT_LOGD("%s", __func__);
    bth_bt_status_t ret = BTH_STATUS_FAIL;
    ret = bth_hfp_ag_connect(TO_BTH_ADDRESS(bd_addr));
    if (ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_disconnect(const bt_address_t *bd_addr)
{
    BT_LOGD("%s", __func__);
    bth_bt_status_t ret = BTH_STATUS_FAIL;
    ret = bth_hfp_ag_disconnect(TO_BTH_ADDRESS(bd_addr));
    if (ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_connect_audio(const bt_address_t *bd_addr)
{
    BT_LOGD("%s", __func__);
    bth_bt_status_t ret = BTH_STATUS_FAIL;
    ret = bth_hfp_ag_connect_audio(TO_BTH_ADDRESS(bd_addr), 0);
    if (ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_disconnect_audio(const bt_address_t *bd_addr)
{
    BT_LOGD("%s", __func__);
    bth_bt_status_t ret = BTH_STATUS_FAIL;
    ret = bth_hfp_ag_disconnect_audio(TO_BTH_ADDRESS(bd_addr));
    if (ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_manufacture_id_response(const bt_address_t *bd_addr, char* data, uint16_t len)
{
    BT_LOGD("%s", __func__);
    return BT_STATUS_NOT_SUPPORTED;
}

bt_status_t bt_sal_hfp_ag_model_id_response(const bt_address_t *bd_addr, char* data, uint16_t len)
{
    BT_LOGD("%s", __func__);
    return BT_STATUS_NOT_SUPPORTED;
}

bt_status_t bt_sal_hfp_ag_clcc_response(const bt_address_t *bd_addr, int idx, bool incoming,
                                        uint8_t call_state, uint8_t call_mode, bool multiparty,
                                        uint8_t call_addr_type, char* line_identification)
{
    BT_LOGD("%s", __func__);
    bth_bt_status_t ret = BTH_STATUS_FAIL;
    bth_hfp_call_direction_t dir = incoming ? BTH_HFP_CALL_DIRECTION_INCOMING : BTH_HFP_CALL_DIRECTION_OUTGOING;
    bth_hfp_call_state_t state = (bth_hfp_call_state_t) call_state;
    bth_hfp_ag_call_mode_t mode = (bth_hfp_ag_call_mode_t) call_mode;
    bth_hfp_call_mpty_type_t mpty = multiparty ? BTH_HFP_CALL_MPTY_TYPE_MULTI : BTH_HFP_CALL_MPTY_TYPE_SINGLE;
    bth_hfp_ag_call_addrtype_t addr_type = (bth_hfp_ag_call_addrtype_t) call_addr_type;
    char *number = line_identification;

    ret = bth_hfp_ag_clcc_response(TO_BTH_ADDRESS(bd_addr), idx, dir, state, mode, mpty, number, addr_type);

    if (ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_send_at_cmd(const bt_address_t *bd_addr, char* str, uint16_t len)
{
    BT_LOGD("%s", __func__);
    bth_bt_status_t ret = BTH_STATUS_FAIL;
    ret = bth_hfp_ag_formatted_at_response(TO_BTH_ADDRESS(bd_addr), str);
    if (ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_cind_response(const bt_address_t *bd_addr, hfp_ag_cind_resopnse_t *resp)
{
    BT_LOGD("%s", __func__);
    bth_bt_status_t ret = BTH_STATUS_FAIL;
    bth_hfp_ag_cind_rsp_t rsp;
    rsp.batt_chg = resp->battery;
    rsp.roam = resp->roam;
    rsp.signal = resp->signal;
    rsp.svc = resp->network;
    rsp.call_state = resp->call == HFP_CALL_CALLS_IN_PROGRESS ?  BTH_HFP_CALL_CALLS_IN_PROGRESS : BTH_HFP_CALL_NO_CALLS_IN_PROGRESS;
    rsp.call_held = resp->call_held == HFP_CALLHELD_HELD ? BTH_HFP_CALLHELD_HOLD : BTH_HFP_CALLHELD_NONE;
    switch (resp->call_setup) {
        case HFP_CALLSETUP_ALERTING:
            rsp.call_setup_state = BTH_HFP_CALLSETUP_ALERTING;
            break;
        case HFP_CALLSETUP_INCOMING:
            rsp.call_setup_state = BTH_HFP_CALLSETUP_INCOMING;
            break;
        case HFP_CALLSETUP_OUTGOING:
            rsp.call_setup_state = BTH_HFP_CALLSETUP_OUTGOING;
            break;
        case HFP_CALLSETUP_NONE:
        default:
            rsp.call_setup_state = BTH_HFP_CALLSETUP_NONE;
    }
    ret = bth_hfp_ag_cind_response(TO_BTH_ADDRESS(bd_addr), &rsp);
    if (ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_error_response(const bt_address_t *bd_adr, uint8_t error)
{
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_start_voice_recognition(const bt_address_t *bd_addr)
{
    BT_LOGD("%s", __func__);
    bth_bt_status_t ret = BTH_STATUS_FAIL;
    bt_address_t copy_addr;
    bt_addr_set(&copy_addr, bd_addr->addr);
    ret = bth_hfp_ag_start_voice_recognition(TO_BTH_ADDRESS(bd_addr));
    if (ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    // TODO may be not right
    hfp_ag_on_voice_recognition_state_changed(&copy_addr, true);
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_stop_voice_recognition(const bt_address_t *bd_addr)
{
    BT_LOGD("%s", __func__);
    bth_bt_status_t ret = BTH_STATUS_FAIL;
    bt_address_t copy_addr;
    bt_addr_set(&copy_addr, bd_addr->addr);
    ret = bth_hfp_ag_stop_voice_recognition(TO_BTH_ADDRESS(bd_addr));
    if (ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    // TODO may be not right
    hfp_ag_on_voice_recognition_state_changed(&copy_addr, false);
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_phone_state_change(const bt_address_t *bd_addr, uint8_t num_active, uint8_t num_held,
                                             hfp_ag_call_state_t call_state, hfp_call_addrtype_t addr_type,
                                             char *str1, char* str2)
{
    BT_LOGD("%s", __func__);
    bth_bt_status_t ret = BTH_STATUS_FAIL;
    bth_hfp_callsetup_t callsetup_state = BTH_HFP_CALLSETUP_NONE;
    bth_hfp_ag_call_addrtype_t bth_addr_type = addr_type == HFP_CALL_ADDRTYPE_INTERNATIONAL ?
                                               BTH_HFP_AG_CALL_ADDRTYPE_INTERNATIONAL : BTH_HFP_AG_CALL_ADDRTYPE_UNKNOWN;
    switch (call_state) {
        case HFP_AG_CALL_STATE_INCOMING:
            callsetup_state = BTH_HFP_CALLSETUP_INCOMING;
            break;
        case HFP_AG_CALL_STATE_DIALING:
            callsetup_state =  BTH_HFP_CALLSETUP_OUTGOING;
            break;
        case HFP_AG_CALL_STATE_ALERTING:
            callsetup_state =  BTH_HFP_CALLSETUP_ALERTING;
            break;
        default:
            callsetup_state =  BTH_HFP_CALLSETUP_NONE;
    }

    ret = bth_hfp_ag_phone_state_change(TO_BTH_ADDRESS(bd_addr), num_active, num_held,
                                        callsetup_state, str1,
                                        bth_addr_type, str2 );
    if (ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }

    if (callsetup_state != BTH_HFP_CALLSETUP_NONE)
    {
        bth_hfp_ag_connect_audio(TO_BTH_ADDRESS(bd_addr), 0);
    }
    else if (!num_active && !num_held)
    {
        bth_hfp_ag_disconnect_audio(TO_BTH_ADDRESS(bd_addr));
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_notify_device_status_changed(const bt_address_t *bd_addr, uint32_t net, uint32_t roam, uint32_t singal, uint32_t batt_chg)
{
    BT_LOGD("%s", __func__);
    bth_bt_status_t ret = BTH_STATUS_FAIL;
    ret = bth_hfp_ag_device_status_notification(TO_BTH_ADDRESS(bd_addr), net, roam, singal, batt_chg);
    if (ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_set_inband_ring_enable(const bt_address_t *bd_addr, bool ring)
{

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_dial_response(const bt_address_t *bd_addr, uint32_t val)
{
    BT_LOGD("%s", __func__);
    bth_bt_status_t ret = BTH_STATUS_FAIL;
    bth_hfp_ag_at_response_t error_rsp = val == HFP_ATCMD_RESULT_OK ? BTH_HFP_AG_AT_RESPONSE_OK : BTH_HFP_AG_AT_RESPONSE_ERROR;

    ret = bth_hfp_ag_at_response(TO_BTH_ADDRESS(bd_addr), error_rsp, 0);
    if (ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_cops_response(const bt_address_t *bd_addr, char* operator, uint16_t len)
{
    BT_LOGD("%s", __func__);
    bth_bt_status_t ret = BTH_STATUS_FAIL;;
    ret = bth_hfp_ag_cops_response(TO_BTH_ADDRESS(bd_addr), operator);
    if (ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_ag_set_volume(const bt_address_t *bd_addr, hfp_volume_type_t type, uint8_t vol)
{
        BT_LOGD("%s", __func__);
    bth_bt_status_t ret = BTH_STATUS_FAIL;;
    ret = bth_hfp_ag_set_volume(TO_BTH_ADDRESS(bd_addr), (bth_hfp_volume_type_t) type, vol);
    if (ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}
