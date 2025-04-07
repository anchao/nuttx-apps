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
#define LOG_TAG "sal_bes_av"

/****************************** header include ********************************/
#include <stdint.h> // For uint8_t type
#include "bt_status.h"
#include "bluetooth.h"
#include "utils/log.h"

#include "sal_a2dp_sink_interface.h"
#include "sal_a2dp_source_interface.h"
#include "bt_utils.h"
#include "a2dp/a2dp_codec.h"
#include "sal_bes.h"

#include <api/bth_api_a2dp.h>

/***************************** external declaration *****************************/

/***************************** macro defination *******************************/
#define BT_SAL_A2DP_STREAM_LEN_MAX     1000

/*****************************  type defination ********************************/

/*****************************  variable defination *****************************/

/*****************************  function declaration ****************************/

static a2dp_codec_index_t bes_codec_2_sal_codec(uint8_t codec_type)
{
    if (codec_type == BTH_A2DP_CODEC_INDEX_SOURCE_SBC) {
        return BTS_A2DP_TYPE_SBC;
    } else if (codec_type == BTH_A2DP_CODEC_INDEX_SOURCE_AAC) {
        return BTS_A2DP_TYPE_MPEG2_4_AAC;
    } else {
        BT_LOGW("not supported codec type = %d", codec_type);
    }

    return BTS_A2DP_TYPE_NON_A2DP;
}

static a2dp_codec_sample_rate_t bes_audio_sample_rate_2_sal_sample_rate(BTH_A2DP_CODEC_SAMPLE_RATE_TYPE_E sample_rate)
{
    switch (sample_rate) {
        case BTH_A2DP_CODEC_SAMPLE_RATE_44100: return 44100;
        case BTH_A2DP_CODEC_SAMPLE_RATE_48000: return 48000;
        case BTH_A2DP_CODEC_SAMPLE_RATE_88200: return 88200;
        case BTH_A2DP_CODEC_SAMPLE_RATE_96000: return 96000;
        case BTH_A2DP_CODEC_SAMPLE_RATE_176400: return 176400;
        case BTH_A2DP_CODEC_SAMPLE_RATE_192000: return 192000;
        case BTH_A2DP_CODEC_SAMPLE_RATE_16000: return 16000;
        case BTH_A2DP_CODEC_SAMPLE_RATE_24000: return 24000;
        default:
            BT_LOGE("invalid sample rate = %d", sample_rate);
            return 44100;
    }
}

static a2dp_codec_channel_mode_t bes_audio_channel_mode_2_sal_channel_mode(BTH_A2DP_CODEC_CHANNELMODE_TYPE_E channel_mode)
{
    switch (channel_mode) {
        case BTH_A2DP_CODEC_CHANNEL_MODE_MONO:
            return BTS_A2DP_CODEC_CHANNEL_MODE_MONO;
        case BTH_A2DP_CODEC_CHANNEL_MODE_STEREO:
            return BTS_A2DP_CODEC_CHANNEL_MODE_STEREO;
        default:
            BT_LOGW("%s, invalid channel mode: 0x%x", __func__, channel_mode);
            return BTS_A2DP_CODEC_CHANNEL_MODE_STEREO;
    }
}

/*******************************************************************************
 *  AV Sink Functions
 ******************************************************************************/
static void bes_bt_a2dp_sink_connection_state_cb(const bth_address_t* bd_addr,
                                                 uint8_t state,
                                                 const bth_a2dp_error_t* error)
{
    if (bd_addr == NULL) {
        BT_LOGE("%s: Invalid input parameter - bd_addr is NULL", __func__);
        return;
    }

    bt_address_t peer_addr;
    a2dp_event_type_t event_type;

    BT_LOGD(" state = %d", state);

    memcpy(peer_addr.addr, bd_addr->address, sizeof(bd_addr->address));
    switch (state) {
        case BTH_A2DP_CONNECTION_STATE_CONNECTED:
            event_type = CONNECTED_EVT;
            break;
        case BTH_A2DP_CONNECTION_STATE_DISCONNECTED:
            event_type = DISCONNECTED_EVT;
            break;
        default:
            BT_LOGW("no need to report");
            return;
    }

    a2dp_event_t* event = a2dp_event_new(event_type, &peer_addr);
    if (event == NULL) {
        BT_LOGE("%s: Failed to create a2dp event", __func__);
        return;
    }

    bt_sal_a2dp_sink_event_callback(event);
}

static void bes_bt_a2dp_sink_audio_state_cb(const bth_address_t* bd_addr, uint8_t state)
{
    if (bd_addr == NULL) {
        BT_LOGE("%s: Invalid input parameter - bd_addr is NULL", __func__);
        return;
    }

    bt_address_t peer_addr;
    a2dp_event_type_t event;

    BT_LOGD("state = %d", state);

    memcpy(peer_addr.addr, bd_addr->address, sizeof(bd_addr->address));

    switch (state) {
        case BTH_A2DP_AUDIO_STATE_REMOTE_SUSPEND:
            event = STREAM_SUSPENDED_EVT;
            break;
        case BTH_A2DP_AUDIO_STATE_STARTED:
            event = STREAM_STARTED_EVT;
            break;
        case BTH_A2DP_AUDIO_STATE_STOPPED:
            event = STREAM_CLOSED_EVT;
            break;
        default:
            BT_LOGE("unknown audio state = %d", state);
            return;
    }

    a2dp_event_t* new_event = a2dp_event_new(event, &peer_addr);
    if (new_event == NULL) {
        BT_LOGE("%s: Failed to create a2dp event", __func__);
        return;
    }

    bt_sal_a2dp_sink_event_callback(new_event);
}

static void bes_bt_a2dp_sink_audio_config_cb(const bth_address_t* bd_addr, bth_codec_array_t config)
{
    bt_address_t peer_addr;
    a2dp_event_t* event;
    a2dp_codec_config_t codec_config;
    bth_a2dp_codec_config_t *from_config = NULL;

    if (bd_addr == NULL || config.codecs == NULL || config.size != 1) {
        BT_LOGE("%s: Invalid input parameters. bd_addr: %s, codecs: %s, size: %d",
                __func__, bd_addr ? "not NULL" : "NULL", config.codecs ? "not NULL" : "NULL", config.size);
        return;
    }

    from_config = config.codecs;
    memcpy(&peer_addr, bd_addr, BT_ADDRESS_LEN);
    codec_config.codec_type = bes_codec_2_sal_codec(from_config->codec_type);

    switch (codec_config.codec_type) {
        case BTS_A2DP_TYPE_SBC:
        case BTS_A2DP_TYPE_MPEG2_4_AAC:
            codec_config.sample_rate = bes_audio_sample_rate_2_sal_sample_rate(from_config->sample_rate);
            codec_config.bits_per_sample = BTS_A2DP_CODEC_BITS_PER_SAMPLE_16;
            codec_config.channel_mode = bes_audio_channel_mode_2_sal_channel_mode(from_config->channel_mode);
            codec_config.packet_size = 1024;
            memcpy(codec_config.specific_info, &from_config->codec_specific_1, sizeof(from_config->codec_specific_1));
            break;
        default:
            BT_LOGE("%s, codec not supported: 0x%x", __func__, codec_config.codec_type);
            return;
    }

    event = a2dp_event_new(CODEC_CONFIG_EVT, &peer_addr);
    if (event == NULL) {
        BT_LOGE("%s: Failed to create a2dp_event.", __func__);
        return;
    }

    event->event_data.data = malloc(sizeof(codec_config));
    if (event->event_data.data == NULL) {
        BT_LOGE("%s: Failed to allocate memory for event data.", __func__);
        a2dp_event_destory(event);
        return;
    }

    memcpy(event->event_data.data, &codec_config, sizeof(codec_config));
    bt_sal_a2dp_sink_event_callback(event);
}

void bes_bt_a2dp_sink_audio_data_cb(const bth_address_t* bd_addr, uint8_t* data, uint16_t len)
{
    if (bd_addr == NULL) {
        BT_LOGE("%s: Invalid input parameter. bd_addr is NULL.", __func__);
        return;
    }
    if (data == NULL) {
        BT_LOGE("%s: Invalid input parameter. data is NULL.", __func__);
        return;
    }

    a2dp_event_t* event;
    a2dp_sink_packet_t* packet;
    uint8_t* p;
    uint8_t offset;
    uint16_t seq;
    uint32_t timestamp;

    p = data;
    offset = 12 + (*p & 0x0F) * 4; // rtp header + ssrc + csrc
    if (len < offset) {
        BT_LOGE("%s, invalid length: %d", __func__, len);
        return;
    }

    p += 2;
    BE_STREAM_TO_UINT16(seq, p);
    BE_STREAM_TO_UINT32(timestamp, p);

    packet = a2dp_sink_new_packet(timestamp, seq, data+offset, len-offset);
    if (packet == NULL) {
        BT_LOGE("%s, packet malloc failed", __func__);
        return;
    }
    event = a2dp_event_new(DATA_IND_EVT, (bt_address_t*)bd_addr);
    if (event == NULL) {
        BT_LOGE("%s, event malloc failed", __func__);
        return;
    }

    event->event_data.packet = packet;
    bt_sal_a2dp_sink_event_callback(event);
}

static bth_a2dp_sink_callbacks_t bes_a2dp_sink_cb =
{
    .connection_state_cb = bes_bt_a2dp_sink_connection_state_cb,
    .audio_state_cb      = bes_bt_a2dp_sink_audio_state_cb,
    .audio_config_cb     = bes_bt_a2dp_sink_audio_config_cb,
    .audio_data_cb       = bes_bt_a2dp_sink_audio_data_cb,
};

bt_status_t bt_sal_a2dp_sink_init(uint8_t max_connection)
{
    bth_bt_status_t ret;

    bes_a2dp_sink_cb.size = sizeof(bes_a2dp_sink_cb);
    ret = bth_a2dp_sink_init(&bes_a2dp_sink_cb, max_connection);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

void bt_sal_a2dp_sink_cleanup(void)
{
    bth_a2dp_sink_cleanup();
}

bt_status_t bt_sal_a2dp_sink_connect(bt_controller_id_t id, bt_address_t* addr)
{
    bth_bt_status_t ret;

    ret = bth_a2dp_sink_connect_src((bth_address_t*)addr);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_a2dp_sink_disconnect(bt_controller_id_t id, bt_address_t* addr)
{
    bth_bt_status_t ret;

    ret = bth_a2dp_sink_disconnect_src((bth_address_t*)addr);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_a2dp_sink_start_stream(bt_controller_id_t id, bt_address_t* addr)
{
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

/*******************************************************************************
 *  AV Source Functions
 ******************************************************************************/
void bes_bt_a2dp_source_connection_state_cb(const bth_address_t* bd_addr,
                                          uint8_t state,
                                          const bth_a2dp_error_t* error)
{
    if (bd_addr == NULL) {
        BT_LOGE("Input bd_addr is NULL");
        return;
    }

    bt_address_t peer_addr;
    a2dp_event_type_t event;

    memcpy(peer_addr.addr, bd_addr->address, sizeof(bd_addr->address));
    BT_LOGD(" state = %d", state);

    switch (state) {
        case BTH_A2DP_CONNECTION_STATE_CONNECTED:
            event = CONNECTED_EVT;
            break;
        case BTH_A2DP_CONNECTION_STATE_DISCONNECTED:
            event = DISCONNECTED_EVT;
            break;
        default:
            BT_LOGE("Unexpected connection state = %d", state);
            return;
    }

    a2dp_event_t *new_event = a2dp_event_new(event, &peer_addr);
    if (new_event == NULL) {
        BT_LOGE("Failed to create a2dp event");
        return;
    }

    bt_sal_a2dp_source_event_callback(new_event);
}

void bes_bt_a2dp_source_audio_state_cb(const bth_address_t* bd_addr, uint8_t state)
{
    if (bd_addr == NULL) {
        BT_LOGE("Input bd_addr is NULL");
        return;
    }

    bt_address_t peer_addr;
    a2dp_event_type_t event;

    memcpy(peer_addr.addr, bd_addr->address, sizeof(bd_addr->address));
    BT_LOGD(" state = %d", state);

    switch (state) {
        case BTH_A2DP_AUDIO_STATE_REMOTE_SUSPEND:
            event = STREAM_SUSPENDED_EVT;
            break;
        case BTH_A2DP_AUDIO_STATE_STARTED:
            event = STREAM_STARTED_EVT;
            break;
        case BTH_A2DP_AUDIO_STATE_STOPPED:
            event = STREAM_CLOSED_EVT;
            break;
        default:
            BT_LOGE("unknown audio state = %d", state);
            return;
    }

    a2dp_event_t *new_event = a2dp_event_new(event, &peer_addr);
    if (new_event == NULL) {
        BT_LOGE("Failed to create a2dp event");
        return;
    }

    bt_sal_a2dp_source_event_callback(new_event);
}

void bes_bt_a2dp_source_audio_source_config_cb(const bth_address_t* bd_addr,
                                             bth_codec_array_t codec_config,
                                             bth_codec_array_t codecs_local_capabilities,
                                             bth_codec_array_t codecs_selectable_capabilities)
{
    if (bd_addr == NULL) {
        BT_LOGE("Input bd_addr is NULL");
        return;
    }

    if (codec_config.size != 1) {
        BT_LOGE("Invalid codec_config size: %d, expected 1", codec_config.size);
        return;
    }

    bt_address_t peer_addr;
    a2dp_codec_config_t a2dp_codec_config;
    bth_a2dp_codec_config_t *from_config = codec_config.codecs;

    memcpy(&peer_addr, bd_addr, BT_ADDRESS_LEN);
    a2dp_codec_config.codec_type = bes_codec_2_sal_codec(from_config->codec_type);

    switch (a2dp_codec_config.codec_type) {
        case BTS_A2DP_TYPE_SBC:
        case BTS_A2DP_TYPE_MPEG2_4_AAC:
            a2dp_codec_config.sample_rate = bes_audio_sample_rate_2_sal_sample_rate(from_config->sample_rate);
            a2dp_codec_config.bits_per_sample = BTS_A2DP_CODEC_BITS_PER_SAMPLE_16;
            a2dp_codec_config.channel_mode = bes_audio_channel_mode_2_sal_channel_mode(from_config->channel_mode);
            a2dp_codec_config.packet_size = 1024;
            memcpy(a2dp_codec_config.specific_info, &from_config->codec_specific_1, sizeof(from_config->codec_specific_1));
            break;
        default:
            BT_LOGE("%s, codec not supported: 0x%x", __func__, a2dp_codec_config.codec_type);
            return;
    }

    a2dp_event_t *new_event = a2dp_event_new(CODEC_CONFIG_EVT, &peer_addr);
    if (new_event == NULL) {
        BT_LOGE("Failed to create a2dp event");
        return;
    }
    new_event->event_data.data = malloc(sizeof(a2dp_codec_config));
    if (new_event->event_data.data == NULL) {
        BT_LOGE("Failed to allocate memory for event data");
        a2dp_event_destory(new_event);
        return;
    }

    memcpy(new_event->event_data.data, &a2dp_codec_config, sizeof(a2dp_codec_config));
    bt_sal_a2dp_source_event_callback(new_event);

    // Set A2DP source stream data max len
    new_event = a2dp_event_new(STREAM_MTU_CONFIG_EVT, &peer_addr);
    if (new_event == NULL) {
        BT_LOGE("Failed to create a2dp event");
        return;
    }
    new_event->event_data.mtu = BT_SAL_A2DP_STREAM_LEN_MAX;

    bt_sal_a2dp_source_event_callback(new_event);
}

static bth_a2dp_source_callbacks_t bes_a2dp_source_cb =
{
    .connection_state_cb = bes_bt_a2dp_source_connection_state_cb,
    .audio_state_cb      = bes_bt_a2dp_source_audio_state_cb,
    .audio_config_cb     = bes_bt_a2dp_source_audio_source_config_cb,
    .mandatory_codec_preferred_cb = NULL,
};

bt_status_t bt_sal_a2dp_source_init(uint8_t max_connection)
{
    bth_bt_status_t ret;

    bes_a2dp_source_cb.size = sizeof(bes_a2dp_source_cb);
    ret = bth_a2dp_source_init(&bes_a2dp_source_cb, max_connection);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

void bt_sal_a2dp_source_cleanup(void)
{
    bth_a2dp_src_cleanup();
}

bt_status_t bt_sal_a2dp_source_connect(bt_controller_id_t id, bt_address_t* addr)
{
    if (addr == NULL) {
        BT_LOGE("[%s][%d]: Input addr is NULL", __FUNCTION__, __LINE__);
        return BT_STATUS_FAIL;
    }

    int ret = bth_a2dp_src_connect_sink((bth_address_t*)addr);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_a2dp_source_disconnect(bt_controller_id_t id, bt_address_t* addr)
{
    if (addr == NULL) {
        BT_LOGE("[%s][%d]: Input addr is NULL", __FUNCTION__, __LINE__);
        return BT_STATUS_FAIL;
    }

    int ret = bth_a2dp_src_disconnect_sink((bth_address_t*)addr);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_a2dp_source_start_stream(bt_controller_id_t id, bt_address_t* remote_addr)
{
    if (remote_addr == NULL) {
        BT_LOGE("[%s][%d]: Input remote_addr is NULL", __FUNCTION__, __LINE__);
        return BT_STATUS_FAIL;
    }

    int ret = bth_a2dp_src_set_active_sink((bth_address_t*)remote_addr);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_a2dp_source_suspend_stream(bt_controller_id_t id, bt_address_t* remote_addr)
{
    if (remote_addr == NULL) {
        BT_LOGE("[%s][%d]: Input remote_addr is NULL", __FUNCTION__, __LINE__);
        return BT_STATUS_FAIL;
    }

    int ret = bth_a2dp_src_suspend_stream((bth_address_t*)remote_addr);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_a2dp_source_send_data(bt_controller_id_t id, bt_address_t* remote_addr,
                                         uint8_t* buf, uint16_t nbytes, uint8_t nb_frames,
                                         uint64_t timestamp, uint32_t seq)
{
    if (!remote_addr || !buf) {
        BT_LOGE("[%s][%d]: Invalid input parameter: remote_addr=%p, buf=%p",
                __FUNCTION__, __LINE__, remote_addr, buf);
        return BT_STATUS_FAIL;
    }

    uint8_t* data = buf;
    uint16_t len = nbytes + AVDTP_RTP_HEADER_LEN;

    data[0] = 0x80; /* Version 0b10, Padding 0b0, Extension 0b0, CSRC 0b0000 */
    data[1] = 0x60; /* Marker 0b0, Payload Type 0b1100000 */
    data[2] = (uint8_t)(seq >> 8);
    data[3] = (uint8_t)(seq);
    data[4] = (uint8_t)(timestamp >> 24);
    data[5] = (uint8_t)(timestamp >> 16);
    data[6] = (uint8_t)(timestamp >> 8);
    data[7] = (uint8_t)(timestamp);
    data[8] = 0x00; /* SSRC(MSB) */
    data[9] = 0x00; /* SSRC */
    data[10] = 0x00; /* SSRC */
    data[11] = 0x01; /* SSRC(LSB) */

    int ret = bth_a2dp_src_send_audio_data((bth_address_t*)remote_addr, data, len);
    if (ret != BTH_STATUS_SUCCESS) {
        BT_LOGE("[%s][%d]: Failed to send audio data, ret: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}