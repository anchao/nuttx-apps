/****************************************************************************
 * frameworks/media/server/media_graph.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

/****************************************************************************
 * Type defination
 ****************************************************************************/
typedef enum
{
    SMF_MEDIA_AUDIO_BT_UNKONW = 0x00,
    SMF_MEDIA_AUDIO_BT_A2DP   = 0x01,
    SMF_MEDIA_AUDIO_BT_LEA    = 0x02,
    SMF_MEDIA_AUDIO_BT_SCO    = 0x04,
    SMF_MEDIA_AUDIO_BT_MAX,
}SMF_MEDIA_AUDIO_BT_PATH_TYPE;

typedef enum
{
    SMF_MEDIA_AUDIO_BT_CTRL_START = 0,
    SMF_MEDIA_AUDIO_BT_CTRL_STOP,
    SMF_MEDIA_AUDIO_BT_CTRL_MAX,
}SMF_MEDIA_AUDIO_BT_CTRL_TYPE;

typedef struct
{
    ///see@a2dp_codec_config_t, byte len 29
    // 0: invalid, 1: valid
    uint8_t  valid_code;
    // see@a2dp_codec_index_t
    uint32_t codec_type;
    // see@a2dp_codec_sample_rate_t
    uint32_t sample_rate;
    // see@a2dp_codec_bits_per_sample_t
    uint32_t bits_per_sample;
    // see@a2dp_codec_channel_mode_t
    uint32_t channel_mode;
    uint32_t bit_rate;
    uint32_t frame_size;
    uint32_t packet_size;
    union {
        struct {
            /// see@sbc_param_t, byte len 20
            // mono, dual, streo or joint streo
            int32_t s16ChannelMode;
            // 4, 8, 12 or 16
            int32_t s16NumOfBlocks;
            // 4 or 8
            int32_t s16NumOfSubBands;
            // loudness or SNR
            int32_t s16AllocationMethod;
            // 16*numOfSb for mono & dual
            // 32*numOfSb for stereo & joint stereo
            int32_t s16BitPool;
        } sbc;
        struct {
            /// see@aac_encoder_param_t, byte len 8
            uint32_t u16ObjectType;
            uint32_t u16VariableBitRate;
        } aac;
    } diff_param;
} smf_media_audio_bt_codec_a2dp_t;

typedef struct
{
    ///see@a2dp_codec_config_t, byte len 29
    // 0: invalid, 1: valid
    uint8_t  valid_code;
    // see@a2dp_codec_index_t
    uint32_t codec_type;
    // see@a2dp_codec_sample_rate_t
    uint32_t sample_rate;
    // see@a2dp_codec_bits_per_sample_t
    uint32_t bits_per_sample;
    // see@a2dp_codec_channel_mode_t
    uint32_t channel_mode;
    uint32_t bit_rate;
    uint32_t frame_size;
    uint32_t packet_size;
} smf_media_audio_bt_codec_lea_t;

typedef struct
{
    int sample_rate;
} smf_media_audio_bt_codec_sco_t;

typedef struct
{
    SMF_MEDIA_AUDIO_BT_PATH_TYPE type;
    union
    {
        smf_media_audio_bt_codec_a2dp_t a2dp;
        smf_media_audio_bt_codec_lea_t  lea;
        smf_media_audio_bt_codec_sco_t  sco;
    } codec_param;
} smf_media_audio_bt_codec_cfg_t;

/****************************************************************************
 * Function declaration
 ****************************************************************************/
int smf_media_audio_bt_open(SMF_MEDIA_AUDIO_BT_PATH_TYPE path_type);

int smf_media_audio_bt_close(SMF_MEDIA_AUDIO_BT_PATH_TYPE path_type);

int smf_media_audio_bt_ctrl_send(SMF_MEDIA_AUDIO_BT_CTRL_TYPE ctrl_type);

void smf_media_audio_bt_set_sco_param(int sample_rate);

const smf_media_audio_bt_codec_cfg_t* smf_media_audio_bt_get_codec_info(void);