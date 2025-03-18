
#pragma once

#include "stdint.h"
#include "smf_media_api.h"
#include "smf_media_api_def.h"

#define SMF_VOLUME_MAX 32768
#define SMF_AUDIO_KFIFO_SIZE    (16*1024)
#define SMF_POLICY_LIST 32
#define SMF_POLICY_DATA_LEN 32

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
    SMF_MEDIA_AUDIO_PLAYE_DEF = 0,
    SMF_MEDIA_AUDIO_PLAYE_URL = 1,
    SMF_MEDIA_AUDIO_PLAYE_BUF = 2,
}SMF_MEDIA_AUDIO_PLAYER_TYPE;

typedef enum {
    SMF_MEDIA_AUDIO_RECORDER_DEF = 0,
    SMF_MEDIA_AUDIO_RECORDER_URL = 1,
    SMF_MEDIA_AUDIO_RECORDER_BUF = 2,
}SMF_MEDIA_AUDIO_RECORDER_TYPE;

typedef enum {
    SMF_MEDIA_AUDIO_DEF = 0,
    SMF_MEDIA_AUDIO_PLAYER = 1,
    SMF_MEDIA_AUDIO_RECORDER = 2,
}SMF_MEDIA_AUDIO_TYPE;

typedef struct {
    const char* cmd;
    const char* arg;
    bool player;
    void* cookie;
}smf_media_thread_t;

typedef struct {
    int sockfd;
    SMF_MEDIA_AUDIO_TYPE audio_type;
    SMF_MEDIA_AUDIO_PLAYER_TYPE audio_player_type;
    SMF_MEDIA_AUDIO_RECORDER_TYPE audio_recorder_type;
    uint64_t audio_id;
    char audio_url[64];
    uint32_t cur_postion;
}smf_media_priv_t;

bool smf_media_audio_output_config();
bool smf_media_audio_output_remove(SMF_AUDIO_OUTPUT_TYPE type);

uint64_t smf_media_audio_player_url_start(SmfAudioPlayerCallback* player_func, char* url, void* priv, int vol);
uint64_t smf_media_audio_player_buffer_start(int vol);
void smf_media_audio_player_stop(uint64_t id);

uint64_t smf_media_audio_player_a2dp_start(const char* codec, int vol);
void smf_media_audio_player_a2dp_stop(uint64_t id);

bool smf_media_audio_input_config();
bool smf_media_audio_input_remove(SMF_AUDIO_INPUT_TYPE type);

uint64_t smf_media_audio_recorder_url_start(char* url, const char* codec);
uint64_t smf_media_audio_recorder_buffer_start(SmfAudioRecordCallback* record_func, const char* codec, void* priv);

uint64_t smf_media_audio_btsco_start(uint8_t type, uint32_t vol);//1 8000 cvsd 2 16000 msbc
bool smf_media_audio_btsco_stop(uint64_t id);

uint32_t smf_media_kfifo_data_pull(void* buffer, uint32_t len);
uint32_t smf_media_kfifo_data_push(void* buffer, uint32_t len);
uint32_t smf_media_kfifo_data_peek(void* buffer, uint32_t len);
uint32_t smf_media_kfifo_data_size();
uint32_t smf_media_kfifo_data_free_size();

bool smf_media_policy_list_join(const char* target, const char* cmd,const char* arg );
void* smf_media_policy_list_get(const char* target);
void smf_media_policy_list_clean();