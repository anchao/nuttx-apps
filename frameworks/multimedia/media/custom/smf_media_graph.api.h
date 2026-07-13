
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "smf_media_api.h"
#include "smf_media_api_def.h"
#include "smf_stream_api_default.h"

#define SMF_VOLUME_MAX 32768
#define SMF_AUDIO_KFIFO_SIZE    (16*1024)
#define SMF_POLICY_LIST 64
#define SMF_POLICY_DATA_LEN 128
#define SMF_PRIV_URL_LEN 128

typedef struct {
    uint64_t hash;
    char target[SMF_POLICY_DATA_LEN];
    char cmd[SMF_POLICY_DATA_LEN];
    char arg[SMF_POLICY_DATA_LEN];
}smf_media_policy_t;

typedef struct {
    uint16_t    total_len;
    uint16_t    seqnumber;
    uint16_t    frame_send_time;
    uint16_t    frame_len;
    uint32_t    frame_num;
    uint32_t    codec_type;                //sbc:0, aac:2 pcm:4
    uint8_t     frame_buffer[1024];        //larger than 3M pack
}smf_media_a2dp_source_send_info_t;

typedef enum {
    SMF_MEDIA_STREAM_MODE_DEF = 0,
    SMF_MEDIA_STREAM_MODE_URL = 1,
    SMF_MEDIA_STREAM_MODE_BUF = 2,
    SMF_MEDIA_STREAM_MODE_I2S = 3,
    SMF_MEDIA_STREAM_MODE_TDM = 4,
    SMF_MEDIA_STREAM_MODE_SERVER = 5,
    SMF_MEDIA_STREAM_MODE_CLIENT = 6,
}SMF_MEDIA_STREAM_MODE_TYPE;

typedef enum {
    SMF_MEDIA_STREAM_DEF = 0,
    SMF_MEDIA_STREAM_APLAYBACK = 1,
    SMF_MEDIA_STREAM_ACAPTURE = 2,
    SMF_MEDIA_STREAM_VPLAYBACK = 3,
    SMF_MEDIA_STREAM_VCAPTURE = 4,
    SMF_MEDIA_STREAM_VAD = 5,
    SMF_MEDIA_STREAM_RTSP = 6,
    SMF_MEDIA_STREAM_VOICE_REC = 7,
    SMF_MEDIA_STREAM_HWVAD = 8,
    SMF_MEDIA_STREAM_PROMPT = 9,
    SMF_MEDIA_STREAM_PCAPTURE = 10,
}SMF_MEDIA_STREAM_TYPE;

typedef struct {
    const char* cmd;
    const char* arg;
    void* cookie;
    bool player;
    void* res;
}smf_media_thread_t;

typedef struct {
    int sockfd;
    SMF_MEDIA_STREAM_TYPE smf_media_stream_type;
    SMF_MEDIA_STREAM_MODE_TYPE smf_media_stream_mode_type;
    smf_stream_status_e status;
    int volume;
    uint32_t smf_media_id;
    uint32_t smf_media_id_ctrl;
    uint8_t smf_media_ids[8];
    char smf_url[SMF_PRIV_URL_LEN];
    char smf_opt[SMF_PRIV_URL_LEN];
    void* callback;
    void* callback_priv;
    void* cookie;
    smf_meta_info_t meta;
    float volume_ratio;
    uint8_t device_id;
}smf_media_priv_t;

typedef struct {
    int position;
    int total;
}smf_media_progress_t;

typedef enum {
    SMF_MEDIA_DYN_PATH_NORMAL_DEMO = 0,
    SMF_MEDIA_DYN_PATH_THIRDLIB_EXT = 1,
    SMF_MEDIA_DYN_PATH_AK_BSS_EXT = 2,
    SMF_MEDIA_DYN_PATH_AK_KWS_EXT = 3,
    SMF_MEDIA_DYN_PATH_MAX = 4,
}SMF_MEDIA_DYN_PATH_TYPE;

bool smf_media_ids_add(smf_media_priv_t* priv, uint8_t id);
bool smf_media_ids_remove(smf_media_priv_t* priv, uint8_t id);
bool smf_media_ids_clear(smf_media_priv_t* priv);
bool smf_media_ids_stop_all(smf_media_priv_t* priv);
bool smf_media_ids_start(smf_media_priv_t* priv, uint8_t id, const char* param);
bool smf_media_ids_start_param(smf_media_priv_t* priv, uint8_t id, void* param, uint32_t size);
bool smf_media_ids_start_wait(smf_media_priv_t* priv, uint8_t id, const char* param, uint32_t timeout);
bool smf_media_ids_stop(smf_media_priv_t* priv, uint8_t id);
smf_media_priv_t* smf_media_ids_get(uint8_t id);

bool smf_media_audio_output_config(smf_media_priv_t* priv);
bool smf_media_audio_output_remove(smf_media_priv_t* priv);
void* smf_media_audio_output_switch(void* arg);

bool smf_media_audio_output_volume(smf_media_priv_t* priv, uint32_t volume);
bool smf_media_audio_output_mute(smf_media_priv_t* priv, bool mute);

bool smf_media_audio_default_remove(void);
bool smf_media_audio_default_config(void);

bool smf_media_audio_player_url_start(smf_media_priv_t* priv);
bool smf_media_audio_player_buffer_start(smf_media_priv_t* priv);
bool smf_media_audio_player_i2s_start(smf_media_priv_t* priv);
bool smf_media_audio_player_stop(smf_media_priv_t* priv);
bool smf_media_audio_player_set_volume(smf_media_priv_t* priv, int vol);
bool smf_media_audio_player_set_mute(smf_media_priv_t* priv, bool mute);
bool smf_media_audio_player_pause(smf_media_priv_t* priv);
bool smf_media_audio_player_resume(smf_media_priv_t* priv);
bool smf_media_audio_player_seek(smf_media_priv_t* priv, uint32_t timepoint);
bool smf_media_audio_player_get_position(smf_media_priv_t* priv, smf_media_progress_t* progress);

smf_media_priv_t* smf_media_audio_player_a2dp_start(const char* codec);
bool smf_media_audio_player_a2dp_set_volume(smf_media_priv_t* priv, uint32_t volume);
void smf_media_audio_player_a2dp_stop(smf_media_priv_t* priv);

bool smf_media_audio_input_config(smf_media_priv_t* priv);
bool smf_media_audio_input_remove(smf_media_priv_t* priv);

bool smf_media_audio_recorder_url_start(smf_media_priv_t* priv);
bool smf_media_audio_recorder_buffer_start(smf_media_priv_t* priv);
bool smf_media_audio_recorder_i2s_start(smf_media_priv_t* priv);
bool smf_media_audio_recorder_voice_rec_start(smf_media_priv_t* priv);
bool smf_media_audio_recorder_tdm_start(smf_media_priv_t* priv);
bool smf_media_audio_recorder_tdm_stop(smf_media_priv_t* priv);
bool smf_media_audio_recorder_stop(smf_media_priv_t* priv);
bool smf_media_audio_recorder_set_mute(smf_media_priv_t* priv, bool mute);
bool smf_media_audio_recorder_set_volume(smf_media_priv_t* priv, int vol);
bool smf_media_audio_recorder_resume(smf_media_priv_t* priv);
bool smf_media_audio_recorder_pause(smf_media_priv_t* priv);

smf_media_priv_t* smf_media_audio_btsco_start(uint8_t type);//1 8000 cvsd 2 16000 msbc
bool smf_media_audio_btsco_stop(smf_media_priv_t* priv);
bool smf_media_audio_btsco_set_downvolme(smf_media_priv_t* priv, uint32_t vol);
bool smf_media_audio_btsco_set_mic_mute(smf_media_priv_t* priv, bool mute);

smf_media_priv_t* smf_media_audio_agsco_start(uint8_t type, uint32_t vol);//1 8000 cvsd 2 16000 msbc
bool smf_media_audio_agsco_stop(smf_media_priv_t* priv);
bool smf_media_audio_agsco_set_downvolme(smf_media_priv_t* priv);
bool smf_media_audio_agsco_set_mic_mute(smf_media_priv_t* priv);

bool smf_media_video_output_config(smf_media_priv_t* priv);
bool smf_media_video_output_remove(smf_media_priv_t* priv);
bool smf_media_video_input_config(smf_media_priv_t* priv);
bool smf_media_video_input_remove(smf_media_priv_t* priv);

bool smf_media_video_player_url_start(smf_media_priv_t* priv);
bool smf_media_video_player_buffer_start(smf_media_priv_t* priv);
bool smf_media_video_player_stop(smf_media_priv_t* priv);
bool smf_media_video_player_buffer_stop(smf_media_priv_t* priv);

bool smf_media_video_recorder_start(smf_media_priv_t* priv);
bool smf_media_video_recorder_stop(smf_media_priv_t* priv);

bool smf_media_photo_take_start(smf_media_priv_t* priv);
bool smf_media_photo_take_stop(smf_media_priv_t* priv);

void smf_media_kfifo_deinit(void);
uint32_t smf_media_kfifo_data_pull(void* buffer, uint32_t len);
uint32_t smf_media_kfifo_data_push(void* buffer, uint32_t len);
uint32_t smf_media_kfifo_data_peek(void* buffer, uint32_t len);
uint32_t smf_media_kfifo_data_size(void);
uint32_t smf_media_kfifo_data_free_size(void);

bool smf_media_policy_list_join(const char* target, const char* cmd,const char* arg );
void* smf_media_policy_list_get(const char* target);
void smf_media_policy_list_clean(void);

bool smf_media_callback_register(void);

bool smf_media_vad_start(smf_media_priv_t* priv);
bool smf_media_vad_stop(smf_media_priv_t* priv);
bool smf_media_vad_ready(void *param);
bool smf_media_vad_predata_done(void *param);
bool smf_media_vad_record_start(void *param);
bool smf_media_vad_record_stop(void *param);

bool smf_media_audio_a2dp_output_start_send_bt(void);
bool smf_media_audio_a2dp_output_close_send_bt(void);
bool smf_media_audio_output_a2dpsink(smf_media_priv_t* priv, const void* info);

bool smf_media_volume_set(uint64_t type, const char* arg);

bool smf_media_rtsp_server_start(smf_media_priv_t* priv);
bool smf_media_rtsp_server_stop(smf_media_priv_t* priv);
bool smf_media_rtsp_client_start(smf_media_priv_t* priv);
bool smf_media_rtsp_client_stop(smf_media_priv_t* priv);
bool smf_media_rtsp_client_pause(smf_media_priv_t* priv);
bool smf_media_rtsp_client_resume(smf_media_priv_t* priv);
bool smf_media_rtsp_client_set_volume(smf_media_priv_t* priv, int vol);
bool smf_media_rtsp_client_set_mute(smf_media_priv_t* priv, bool mute);


