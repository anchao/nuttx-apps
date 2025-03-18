#define LOG_TAG "sal_hfp_hf"

#include <api/bth_api_hfp_hf.h>

#include <stdint.h> // For uint8_t type
#include "bt_status.h"
#include "utils/log.h"

#include "sal_bes.h"
#include "sal_hfp_hf_interface.h"
#include "hfp_hf_service.h"


typedef struct
{
    bth_address_t peer_addr;
    profile_connection_state_t pre_state;
} sal_conn_info;

typedef struct
{
    uint8_t max_conn;
    sal_conn_info conns[0];
} sal_hfp_hf_t;

sal_hfp_hf_t *g_hf = NULL;

void sal_hfp_hf_connection_state_callback(const bth_address_t* bd_addr, bth_hfp_hf_connection_state_t state,
                                          unsigned int peer_feat, unsigned int chld_feat)
{
    if (g_hf == NULL)
    {
        BT_LOGE("sal hfp hf not init");
        return;
    }
    UNUSED(chld_feat);
    BT_LOGD("%s: max connections %d", __func__, g_hf->max_conn);

    sal_conn_info *conn_info = NULL;
    sal_conn_info *found = NULL;
    uint8_t last_not_use = 0xFF;

    for (uint8_t i = 0; i < g_hf->max_conn; i++)
    {
        conn_info = &g_hf->conns[i];
        if(is_bt_address_equal(bd_addr, &conn_info->peer_addr))
        {
            found = conn_info;
            break;
        }
        else if(is_empty_address(&conn_info->peer_addr) && last_not_use == 0xFF)
        {
            last_not_use = i;
        }
    }

    if (found == NULL)
    {
        if (last_not_use == 0XFF)
        {
            BT_LOGE("over max connections");
            return;
        }
        found = &g_hf->conns[last_not_use];
        found->peer_addr = *bd_addr;
        found->pre_state = PROFILE_STATE_DISCONNECTED;
    }

    profile_connection_state_t conn_state;
    switch(state)
    {
        case BTH_HFP_HF_CONNECTION_STATE_DISCONNECTED:
            conn_state = PROFILE_STATE_DISCONNECTED;
            memset(&found->peer_addr, 0, sizeof(bth_address_t));
            break;
        case BTH_HFP_HF_CONNECTION_STATE_CONNECTING:
            conn_state = PROFILE_STATE_CONNECTING;
            break;
        case BTH_HFP_HF_CONNECTION_STATE_SLC_CONNECTED:
        case BTH_HFP_HF_CONNECTION_STATE_CONNECTED:
            if (found->pre_state == PROFILE_STATE_DISCONNECTED)
            {
                //remote iniatied the connection;
                conn_state = PROFILE_STATE_CONNECTING;
            }
            else
            {
                conn_state = PROFILE_STATE_CONNECTED;
            }
            break;
        case BTH_HFP_HF_CONNECTION_STATE_DISCONNECTING:
            conn_state = PROFILE_STATE_DISCONNECTING;
            break;
        default:
            return;
    }
    found->pre_state = conn_state;
    hfp_hf_on_connection_state_changed(TO_BT_ADDRESS(bd_addr), conn_state, PROFILE_REASON_SUCCESS, peer_feat);
}

void sal_hfp_hf_audio_state_callback(const bth_address_t* bd_addr, bth_hfp_audio_codec_t codec,
                                     bth_hfp_audio_state_t state, uint16_t sco_handle)
{
    BT_LOGD("%s: sco_handle %d", __func__, sco_handle);

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
    hfp_hf_on_codec_changed(TO_BT_ADDRESS(bd_addr), &config);

    hfp_audio_state_t audio_state;
    switch (state) {
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
            BT_LOGE("unknwon audio state %d",state);
            return;
    }
    hfp_hf_on_audio_connection_state_changed(TO_BT_ADDRESS(bd_addr), audio_state, sco_handle);
}


void sal_hfp_hf_vr_cmd_callback(const bth_address_t* bd_addr, bth_hfp_vr_state_t state)
{
    BT_LOGD("%s:", __func__);
    hfp_hf_on_voice_recognition_state_changed(TO_BT_ADDRESS(bd_addr), state == BTH_HFP_VR_STATE_STARTED ? true : false);
}


void sal_hfp_hf_network_state_callback(const bth_address_t* bd_addr, bth_hfp_network_state_t state)
{
    BT_LOGD("%s:", __func__);
}


void sal_hfp_hf_network_roaming_callback(const bth_address_t* bd_addr, bth_hfp_roma_type_t type)
{
    BT_LOGD("%s:", __func__);
}


void sal_hfp_hf_network_signal_callback(const bth_address_t* bd_addr, int signal_strength)
{
    BT_LOGD("%s:", __func__);
}


void sal_hfp_hf_battery_level_callback(const bth_address_t* bd_addr, int battery_level)
{
    BT_LOGD("%s:", __func__);
}


void sal_hfp_hf_current_operator_callback(const bth_address_t* bd_addr, const char* name)
{
    BT_LOGD("%s:", __func__);
}


void sal_hfp_hf_call_callback(const bth_address_t* bd_addr, bth_hfp_call_t call)
{
    BT_LOGD("%s:", __func__);
    hfp_hf_on_call_active_state_changed(TO_BT_ADDRESS(bd_addr), (hfp_call_t) call);
}


void sal_hfp_hf_callsetup_callback(const bth_address_t* bd_addr, bth_hfp_callsetup_t callsetup)
{
    BT_LOGD("%s:", __func__);
    hfp_hf_on_call_setup_state_changed(TO_BT_ADDRESS(bd_addr), (hfp_callsetup_t) callsetup);
}


void sal_hfp_hf_callheld_callback(const bth_address_t* bd_addr, bth_hfp_callheld_t callheld)
{
    BT_LOGD("%s:", __func__);
    //TODO hfp_callheld_t dont have BTH_HFP_CALLHELD_HOLD_AND_ACTIVE
    hfp_hf_on_call_held_state_changed(TO_BT_ADDRESS(bd_addr), (hfp_callheld_t) callheld);
}


void sal_hfp_hf_resp_and_hold_callback(const bth_address_t* bd_addr, bth_hfp_hf_resp_and_hold_t resp_and_hold)
{
    BT_LOGD("%s:", __func__);
}


void sal_hfp_hf_clip_callback(const bth_address_t* bd_addr, const char* number)
{
    BT_LOGD("%s:", __func__);
    //TODO name
    hfp_hf_on_clip(TO_BT_ADDRESS(bd_addr), number, NULL);
}


void sal_hfp_hf_call_waiting_callback(const bth_address_t* bd_addr,
                                            const char* number)
{
    BT_LOGD("%s:", __func__);
}


void sal_hfp_hf_current_calls(const bth_address_t* bd_addr, int index,
                                    bth_hfp_call_direction_t dir,
                                    bth_hfp_call_state_t state,
                                    bth_hfp_call_mpty_type_t mpty,
                                    const char* number)
{
    BT_LOGD("%s:", __func__);
    hfp_hf_on_current_call_response(TO_BT_ADDRESS(bd_addr), index,
                                    (hfp_call_direction_t) dir, (hfp_hf_call_state_t) state,
                                    (hfp_call_mpty_type_t) mpty, number, 0);
}


void sal_hfp_hf_volume_change_callback(const bth_address_t* bd_addr,
                                             bth_hfp_volume_type_t type,
                                             int volume)
{
    BT_LOGD("%s:", __func__);
    hfp_hf_on_volume_changed(TO_BT_ADDRESS(bd_addr), (hfp_volume_type_t) type, volume);
}


void sal_hfp_hf_cmd_complete_callback(const bth_address_t* bd_addr, bth_hfp_hf_at_cmd_t cmd,
                                      bth_hfp_hf_cmd_complete_t type,
                                      int cme)
{
    BT_LOGD("%s:", __func__);
    uint32_t at_cmd_code = 0;
    uint32_t result = type == BTH_HFP_HF_CMD_COMPLETE_OK ? HFP_ATCMD_RESULT_OK: HFP_ATCMD_RESULT_ERROR;
    if (cmd == BTH_HFP_HF_AT_CMD_ATD)
    {
        at_cmd_code = HFP_ATCMD_CODE_ATD;
    }
    else if (cmd == BTH_HFP_HF_AT_CMD_BLDN)
    {
        at_cmd_code = HFP_ATCMD_CODE_BLDN;
    }
    else if (cmd == BTH_HFP_HF_AT_CMD_CLCC)
    {
        BT_LOGD("querry current calls end!")
        hfp_hf_on_current_call_response(TO_BT_ADDRESS(bd_addr), 0, 0, 0, 0, 0, 0);
    }
    else
    {
        return;
    }
    hfp_hf_on_at_command_result_response(TO_BT_ADDRESS(bd_addr), at_cmd_code, result);
}


void sal_hfp_hf_subscriber_info_callback(const bth_address_t* bd_addr,
                                               const char* name,
                                               bth_hfp_hf_subscriber_service_type_t type)
{
    BT_LOGD("%s:", __func__);
}


void sal_hfp_hf_in_band_ring_tone_callback(const bth_address_t* bd_addr,
                                                 bth_hfp_hf_in_band_ring_state_t state)
{
    BT_LOGD("%s:", __func__);
    //TODO active?
    hfp_hf_on_ring_active_state_changed(TO_BT_ADDRESS(bd_addr), true, (hfp_in_band_ring_state_t) state);
}


void sal_hfp_hf_last_voice_tag_number_callback(const bth_address_t* bd_addr,
                                               const char* number)
{
    BT_LOGD("%s:", __func__);
}


void sal_hfp_hf_ring_indication_callback(const bth_address_t* bd_addr)
{
    BT_LOGD("%s:", __func__);
}



void sal_hfp_hf_unknown_event_callback(const bth_address_t* bd_addr,
                                       const char* unknow_event)
{
    BT_LOGD("%s:", __func__);
    hfp_hf_on_received_at_cmd_resp(TO_BT_ADDRESS(bd_addr), unknow_event, strlen(unknow_event));
}

void sal_hfp_hf_sco_conn_req_callback(const bth_address_t* bd_addr)
{
    BT_LOGD("%s:", __func__);
    hfp_hf_on_received_sco_connection_req(TO_BT_ADDRESS(bd_addr));
}

bth_hfp_hf_callbacks_t hf_callbacks =
{
    .connection_state_cb            = sal_hfp_hf_connection_state_callback,
    .audio_state_cb                 = sal_hfp_hf_audio_state_callback,
    .vr_cmd_cb                      = sal_hfp_hf_vr_cmd_callback,
    .battery_level_cb               = sal_hfp_hf_battery_level_callback,
    .call_cb                        = sal_hfp_hf_call_callback,
    .callsetup_cb                   = sal_hfp_hf_callsetup_callback,
    .callheld_cb                    = sal_hfp_hf_callheld_callback,
    .clip_cb                        = sal_hfp_hf_clip_callback,
    .current_calls_cb               = sal_hfp_hf_current_calls,
    .volume_change_cb               = sal_hfp_hf_volume_change_callback,
    .cmd_complete_cb                = sal_hfp_hf_cmd_complete_callback,
    .unknown_event_cb               = sal_hfp_hf_unknown_event_callback,
    .ring_indication_cb             = sal_hfp_hf_ring_indication_callback,
    .sco_conn_req_cb                = sal_hfp_hf_sco_conn_req_callback,
    .network_state_cb               = NULL,
    .network_roaming_cb             = NULL,
    .network_signal_cb              = NULL,
    .current_operator_cb            = NULL,
    .resp_and_hold_cb               = NULL,
    .call_waiting_cb                = NULL,
    .subscriber_info_cb             = NULL,
    .in_band_ring_tone_cb           = NULL,
    .last_voice_tag_number_cb       = NULL,
};

bt_status_t bt_sal_hfp_hf_init(uint32_t features, uint8_t max_connection)
{
    BT_LOGD("%s: features %d max_connection %d", __func__, features, max_connection);

    size_t size = sizeof(sal_hfp_hf_t) + max_connection * sizeof(sal_conn_info);
    g_hf = malloc(size);
    memset(g_hf, 0, size);
    g_hf->max_conn = max_connection;

    CHECK_BES_STACK_RETURN(bth_hfp_hf_init(&hf_callbacks))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_cleanup()
{
    BT_LOGD("%s:", __func__);
    free(g_hf);
    g_hf = NULL;
    bth_hfp_hf_cleanup();
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_connect(const bt_address_t *addr)
{
    BT_LOGD("%s:", __func__);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_connect(TO_BTH_ADDRESS(addr)))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_disconnect(const bt_address_t *addr)
{
    BT_LOGD("%s:", __func__);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_disconnect(TO_BTH_ADDRESS(addr)))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_connect_audio(const bt_address_t *addr)
{
    BT_LOGD("%s:", __func__);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_connect_audio(TO_BTH_ADDRESS(addr)))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_disconnect_audio(const bt_address_t *addr)
{
    BT_LOGD("%s:", __func__);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_disconnect_audio(TO_BTH_ADDRESS(addr)))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_answer_call(const bt_address_t *addr)
{
    BT_LOGD("%s:", __func__);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_handle_call_action(TO_BTH_ADDRESS(addr), BTH_HFP_HF_CALL_ACTION_ATA, 0))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_call_control(const bt_address_t *addr, uint8_t ctrl, uint8_t idx)
{
    BT_LOGD("%s:", __func__);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_handle_call_action(TO_BTH_ADDRESS(addr), (bth_hfp_hf_call_action_t) ctrl , idx))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_reject_call(const bt_address_t *addr)
{
    BT_LOGD("%s:", __func__);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_handle_call_action(TO_BTH_ADDRESS(addr), BTH_HFP_HF_CALL_ACTION_CHUP, 0))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_hangup_call(const bt_address_t *addr)
{
    BT_LOGD("%s:", __func__);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_handle_call_action(TO_BTH_ADDRESS(addr), BTH_HFP_HF_CALL_ACTION_CHUP, 0));
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_get_current_calls(const bt_address_t *addr)
{
    BT_LOGD("%s:", __func__);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_query_current_calls(TO_BTH_ADDRESS(addr)))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_send_at_cmd(const bt_address_t *addr, char* str, uint16_t len)
{
    BT_LOGD("%s: not supported %s", __func__, str);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_send_at_cmd_v2(TO_BTH_ADDRESS(addr), str, len))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_send_battery_level(const bt_address_t *addr, uint8_t level)
{
    BT_LOGD("%s:", __func__);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_send_battery_level(TO_BTH_ADDRESS(addr), level))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_send_dtmf(const bt_address_t *addr, char value)
{
    BT_LOGD("%s:", __func__);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_send_dtmf(TO_BTH_ADDRESS(addr), value))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_set_volume(const bt_address_t *addr, uint8_t type, uint8_t volume)
{
    BT_LOGD("%s:", __func__);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_volume_control(TO_BTH_ADDRESS(addr), (bth_hfp_volume_type_t) type, volume))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_start_voice_recognition(const bt_address_t *addr)
{
    BT_LOGD("%s:", __func__);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_start_voice_recognition(TO_BTH_ADDRESS(addr)))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_stop_voice_recognition(const bt_address_t *addr)
{
    BT_LOGD("%s:", __func__);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_stop_voice_recognition(TO_BTH_ADDRESS(addr)))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_dial_number(const bt_address_t *addr, char* number)
{
    BT_LOGD("%s:", __func__);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_dial(TO_BTH_ADDRESS(addr), number))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_hfp_hf_dial_memory(const bt_address_t *addr, int memory)
{
    BT_LOGD("%s:", __func__);
    CHECK_BES_STACK_RETURN(bth_hfp_hf_dial_memory(TO_BTH_ADDRESS(addr), memory));
    return BT_STATUS_SUCCESS;
}