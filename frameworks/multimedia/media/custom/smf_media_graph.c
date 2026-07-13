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

#include <nuttx/fs/fs.h>
#include <nuttx/sched_note.h>

#include <assert.h>
#include <fcntl.h>
#include <media_api.h>
#include <sys/eventfd.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

#include "media_common.h"
#include "media_server.h"
#include "pthread.h"

#include "smf_media_graph.api.h"
#include "smf_frame.h"

#include "smf_stream_api.h"
#include "smf_stream_api_default.h"
#include "smf_media_arg_parse.h"

#ifdef SMF_ALGO_EXTRADATA
#define RECORD_DATA_SIZE 2048
static char record_data[RECORD_DATA_SIZE];
#endif

#define SMF_RECV_DATA_SIZE 1024

static void* smf_media_socket_recv_data(void* arg){
    smf_media_priv_t* priv = (smf_media_priv_t*)arg;
    MEDIA_INFO("recv data thread start");
    if(!priv){
        MEDIA_ERR("priv is null\n");
        return NULL;
    }
    char* recv_data = zalloc(SMF_RECV_DATA_SIZE);
    if(!recv_data){
        MEDIA_ERR("malloc failed \n");
        return NULL;
    }
    static int idx = 0;
    smf_frame_t frm;
    memset(&frm, 0, sizeof(frm));
    bool opus_play = false;
    int opt_len = strlen(priv->smf_opt);
    //MEDIA_INFO("opt %s len: %d\n",priv->smf_opt, opt_len);
    int media_len = strlen("oMediaScript=[codec=");
    if(opt_len>media_len){
        if( strncmp(priv->smf_opt + media_len, "opus", 4) == 0){
            opus_play = true;
        }
    }
    int recv_size = 0;
    if(opus_play){
        recv_size = 80; //(bitrate/8)*frame_ms/1000
    }else{
        recv_size = SMF_RECV_DATA_SIZE;
    }
    // MEDIA_INFO("recv_size %d \n", recv_size);

    while(priv->sockfd>=0){
        ssize_t len = recv(priv->sockfd, recv_data, recv_size, MSG_WAITALL);
        MEDIA_INFO("recv len %d \n", len);
        if (len<=0) {
            perror("recv");
            if(priv->sockfd>=0){
                close(priv->sockfd);
                priv->sockfd = -1;
            }
            smf_stream_stop(priv->smf_media_id);
            priv->smf_media_id = 0;
            break;
        }else{
            if(priv->smf_media_id && priv->status == SMF_STREAM_STATUS_play ){
                frm.buff = recv_data;
				frm.max = len;
				frm.size = len;
				frm.offset = 0;
				frm.index = idx++;
				// frm.flags = frm.index >= 99 ? SMF_FRAME_IS_EOS : 0;
                bool ret = smf_stream_push(priv->smf_media_id, &frm); //hold
                if(!ret){
                    if(priv->sockfd>=0){
                        close(priv->sockfd);
                        priv->sockfd = -1;
                    }
                    smf_stream_stop(priv->smf_media_id);
                    priv->smf_media_id = 0;
                    break;
                }
                if(opus_play)usleep(20*1000);
            }else{
                if(priv->sockfd>=0){
                    close(priv->sockfd);
                    priv->sockfd = -1;
                }
                smf_stream_stop(priv->smf_media_id);
                priv->smf_media_id = 0;
                break;
            }
        }
    }
    if(recv_data)free(recv_data);
    MEDIA_INFO("recv data thread exit");
    return NULL;
}

static int smf_media_socket_recv_data_thread_create(smf_media_priv_t* priv){
    pthread_t pid;
    pthread_attr_t pattr;
    struct sched_param sparam;
    pthread_attr_init(&pattr);
    pthread_attr_setstacksize(&pattr, 4096);
    sparam.sched_priority = sched_get_priority_max(SCHED_FIFO) - 9;
    pthread_attr_setschedparam(&pattr, &sparam);

    int ret = pthread_create(&pid, &pattr, smf_media_socket_recv_data, (void*)priv);
    if (ret){
        MEDIA_ERR("create smf media thread error %d\n", ret);
        return -1;
    }
    pthread_setname_np(pid, "smf_recv_data");
    pthread_attr_destroy(&pattr);
    pthread_detach(pid);
    MEDIA_INFO("smf_recv_data create");
    return 0;
}

static int smf_media_get_sockaddr(smf_media_priv_t* priv){
    if(!priv){
        MEDIA_ERR("priv is null\n");
        return -1;
    }
    struct sockaddr_un addr;
    priv->sockfd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (priv->sockfd < 0) {
        perror("socket");
        return -errno;
    }
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, priv->smf_url, sizeof(addr.sun_path) - 1);

    if (connect(priv->sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0) {
        perror("connect");
        close(priv->sockfd);
        priv->sockfd = -1;
        return -1;
    }
    return 0;
}

static bool smf_media_audio_recorder_callback(smf_media_priv_t* priv, smf_frame_t* frame){
    void* buffer = frame->buff;
    uint32_t buffer_size = frame->size;
    if(!priv){
        MEDIA_ERR("priv is null\n");
        return false;
    }
    int sockfd = priv->sockfd;
    if(sockfd < 0){
        MEDIA_ERR("audio sockfd is error %d\n", sockfd);
        return false;
    }
    if(priv->status == SMF_STREAM_STATUS_pause){
        frame->size = 0;
        return true;
    }
    int size = 0;
    int rsize = 0;
    #ifdef SMF_ALGO_EXTRADATA
    size = buffer_size + sizeof(frame->ext);
    if( size <= RECORD_DATA_SIZE){
        memcpy(record_data, &frame->ext, sizeof(frame->ext));
        memcpy(record_data + sizeof(frame->ext), buffer, buffer_size);
    }else{
        size = 0;
    }
    rsize = send(sockfd, record_data, size, 0);
    frame->size -= (rsize-sizeof(frame->ext));
    #else
    size = buffer_size;
    rsize = send(sockfd, buffer, size, 0);
    frame->size -= rsize;
    #endif
    MEDIA_INFO("send size %d \n", size);
    if(rsize > 0){
        if(rsize != size){
            MEDIA_ERR("audio send rsize %d size %d\n", rsize, size);
        }
    }else if(rsize<0){
        perror("send");
        if(sockfd>=0){
            close(sockfd);
            priv->sockfd = -1;
        }
        frame->size = 0;
        return true;
    }else{
        MEDIA_INFO("send size 0\n");
    }
    return true;
}

static bool smf_media_get_volume(smf_media_priv_t* priv){
    int smf_vol = 0;
    switch (priv->smf_media_stream_type)
    {
    case SMF_MEDIA_STREAM_VPLAYBACK:
    case SMF_MEDIA_STREAM_APLAYBACK:{
            smf_media_policy_t* policy = (smf_media_policy_t*)smf_media_policy_list_get("VolMusic");
            if(strlen(policy->arg) == 0){
                smf_vol = SMF_VOLUME_MAX;
            }else{
                int vol = atoi(policy->arg);
                smf_vol = (int)( (float)vol/10*SMF_VOLUME_MAX );
            }
            break;
        }
    case SMF_MEDIA_STREAM_PROMPT:{
            smf_media_policy_t* policy = (smf_media_policy_t*)smf_media_policy_list_get("VolRing");
            if(strlen(policy->arg) == 0){
                smf_vol = SMF_VOLUME_MAX;
            }else{
                int vol = atoi(policy->arg);
                smf_vol = (int)( (float)vol/10*SMF_VOLUME_MAX );
            }
            break;
        }
    // case SMF_MEDIA_STREAM_APLAYBACK:
    //     /* code */
    //     break;
    // case SMF_MEDIA_STREAM_APLAYBACK:
    //     /* code */
    //     break;
    // case SMF_MEDIA_STREAM_APLAYBACK:
    //     /* code */
    //     break;
    default:
        break;
    }
    priv->volume = smf_vol;
    MEDIA_INFO("smf vol %d\n", smf_vol);
    return true;
}
/****************************************************************************
 * Public Functions
 ****************************************************************************/
bool smf_media_graph_open(smf_media_thread_t* params)
{
    smf_media_priv_t* priv = zalloc(sizeof(smf_media_priv_t));
    if(!priv){
        MEDIA_ERR("prv zalloc failed\n");
        return false;
    }
    priv->sockfd = -1;
    // MEDIA_INFO("priv %p", priv);
    if(!params->arg){
        MEDIA_ERR("param arg is null\n");
        return false;
    }
    if(!strcmp(params->arg, "Ring")){
        MEDIA_INFO("Ring ");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_PROMPT;
    }else if(!strcmp(params->arg, "Alarm")){
        MEDIA_INFO("Alarm");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_APLAYBACK;
    }else if(!strcmp(params->arg, "Enforced")){
        MEDIA_INFO("Enforced");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_APLAYBACK;
    }else if(!strcmp(params->arg, "Notify")){
        MEDIA_INFO("Notify");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_PROMPT;
    }else if(!strcmp(params->arg, "Record")){
        MEDIA_INFO("Record");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_APLAYBACK;
    }else if(!strcmp(params->arg, "TTS")){
        MEDIA_INFO("TTS");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_APLAYBACK;
    }else if(!strcmp(params->arg, "Health")){
        MEDIA_INFO("Health");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_APLAYBACK;
    }else if(!strcmp(params->arg, "Sport")){
        MEDIA_INFO("Sport");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_PROMPT;
    }else if(!strcmp(params->arg, "Info")){
        MEDIA_INFO("Info");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_APLAYBACK;
    }else if(!strcmp(params->arg, "Music")){
        MEDIA_INFO("Music");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_APLAYBACK;
    }else if(!strcmp(params->arg, "Emergency")){
        MEDIA_INFO("Emergency");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_APLAYBACK;
    }else if(!strcmp(params->arg, "CallRing")){
        MEDIA_INFO("CallRing");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_APLAYBACK;
    }else if(!strcmp(params->arg, "Capture")){
        MEDIA_INFO("Capture");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_ACAPTURE;
    }else if(!strcmp(params->arg, "Media")){
        MEDIA_INFO("Media");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_VPLAYBACK;
    }else if(!strcmp(params->arg, "Rtsp")){
        MEDIA_INFO("Rtsp");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_RTSP;
    }else if(!strcmp(params->arg, "Vad")){
        MEDIA_INFO("Vad");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_VAD;
    }else if(!strcmp(params->arg, "Hwvad")){
        MEDIA_INFO("Hwvad");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_HWVAD;
    }else if(!strcmp(params->arg, "Voicerec")){
        MEDIA_INFO("Voicerec");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_VOICE_REC;
    }else if(!strcmp(params->arg, "Videorec")){
        MEDIA_INFO("Videorec");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_VCAPTURE;
    }else if(!strcmp(params->arg, "Phototake")){
        MEDIA_INFO("phototake");
        priv->smf_media_stream_type = SMF_MEDIA_STREAM_PCAPTURE;
    }else{
        MEDIA_ERR("%s not supported", params->arg);
        free(priv);
        return false;
    }
    priv->cookie = params->cookie;
    priv->volume_ratio = 1.0;
    media_server_set_data(params->cookie, priv);
    // MEDIA_INFO("cookie %p priv %p ", params->cookie, priv);
    return true;
}
bool smf_media_graph_prepare(smf_media_thread_t* params)
{
    smf_media_priv_t* priv = media_server_get_data(params->cookie);
    if(!priv){
        MEDIA_ERR("priv is null\n");
        return false;
    }
    if(!params->arg){
        MEDIA_ERR("param arg is null\n");
        return false;
    }
    int len = strlen(params->arg);
    if(len > SMF_PRIV_URL_LEN){
        MEDIA_ERR("arg len %d > %d \n", len, SMF_PRIV_URL_LEN);
        return false;
    }
    if( !strncmp(params->arg, "med", 3) ){
        priv->smf_media_stream_mode_type = SMF_MEDIA_STREAM_MODE_BUF;
        memcpy(priv->smf_url, params->arg, len);
        priv->smf_url[len] = '\0';
    }else if( !strncmp(params->arg, "i2s", 3) ){
        priv->smf_media_stream_mode_type = SMF_MEDIA_STREAM_MODE_I2S;
    }else if( !strncmp(params->arg, "tdm", 3) ){
        priv->smf_media_stream_mode_type = SMF_MEDIA_STREAM_MODE_TDM;
    }else if (!strncmp(params->arg, "server", 6)) {
        priv->smf_media_stream_mode_type = SMF_MEDIA_STREAM_MODE_SERVER;
    }else if (!strncmp(params->arg, "client", 6)) {
        priv->smf_media_stream_mode_type = SMF_MEDIA_STREAM_MODE_CLIENT;
    }else{
        priv->smf_media_stream_mode_type = SMF_MEDIA_STREAM_MODE_URL;
        memcpy(priv->smf_url, params->arg, len);
        priv->smf_url[len] = '\0';
    }

    MEDIA_INFO("priv %p, stream type %d, stream mode %d",priv, priv->smf_media_stream_type, priv->smf_media_stream_mode_type);
    if(priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_BUF){
        int ret = smf_media_get_sockaddr(priv);
        if(ret < 0){
            MEDIA_ERR("smf media audio player buffer mode connet failed\n");
            return false;
        }
    }
    return true;
}
bool smf_media_graph_start(smf_media_thread_t* params)
{
    smf_media_priv_t* priv = media_server_get_data(params->cookie);
    if(!priv){
        MEDIA_ERR("priv is null\n");
        return false;
    }
    // MEDIA_INFO("priv %p, stream type %d, stream mode %d",priv, priv->smf_media_stream_type, priv->smf_media_stream_mode_type);
    if( (priv->smf_media_id) && (priv->status == SMF_STREAM_STATUS_pause) ){
        if(priv->smf_media_stream_type == SMF_MEDIA_STREAM_APLAYBACK){
            return smf_media_audio_player_resume(priv);
        }else if(priv->smf_media_stream_type == SMF_MEDIA_STREAM_ACAPTURE){
            // return smf_media_audio_recorder_resume(priv);
            priv->status = SMF_STREAM_STATUS_play;
            return true;
        }else{
            return false;
        }
    }
    if(priv->smf_media_stream_type == SMF_MEDIA_STREAM_APLAYBACK){
        smf_media_get_volume(priv);
        if(priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_URL){
            if(!smf_media_audio_player_url_start(priv)){
                MEDIA_ERR("audio player start failed\n");
                return false;
            }
        }else if(priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_BUF){
            if(!smf_media_audio_player_buffer_start(priv)){
                MEDIA_ERR("audio player start failed\n");
                return false;
            }
            int ret = smf_media_socket_recv_data_thread_create(priv);
            if( ret<0 ){
                MEDIA_ERR("audio player buf mode failed \n");
                smf_media_audio_player_stop(priv);
                return false;
            }
        }else if(priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_I2S){
            if(!smf_media_audio_player_i2s_start(priv)){
                MEDIA_ERR("audio player start failed\n");
                return false;
            }
        }else{
            MEDIA_ERR("audio player type not supported %d\n",priv->smf_media_stream_mode_type);
            return false;
        }
    }else if(priv->smf_media_stream_type == SMF_MEDIA_STREAM_ACAPTURE){
        if(priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_URL){
            if(!smf_media_audio_recorder_url_start(priv)){
                MEDIA_ERR("audio recorder start failed\n");
                return false;
            }
        }else if(priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_BUF){
            priv->callback = &smf_media_audio_recorder_callback;
            priv->callback_priv = priv;
            if(!smf_media_audio_recorder_buffer_start(priv)){
                MEDIA_ERR("audio recorder start failed\n");
                return false;
            }
        }else if(priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_I2S){
            if(!smf_media_audio_recorder_i2s_start(priv)){
                MEDIA_ERR("audio recorder start failed\n");
                return false;
            }
        }else if(priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_TDM){
            if(!smf_media_audio_recorder_tdm_start(priv)){
                MEDIA_ERR("audio recorder start failed\n");
                return false;
            }
        }else{
            MEDIA_ERR("audio recorder type not supported %d\n",priv->smf_media_stream_mode_type);
            return false;
        }
    }else if(priv->smf_media_stream_type == SMF_MEDIA_STREAM_VPLAYBACK){
        smf_media_get_volume(priv);
        if(priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_URL){
            if(!smf_media_video_player_url_start(priv)){
                MEDIA_ERR("video player start failed\n");
                return false;
            }
        }else if(priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_BUF){
            if(!smf_media_video_player_buffer_start(priv)){
                MEDIA_ERR("video player buffer start failed\n");
                return false;
            }
        }else{
            MEDIA_ERR("video player type not supported %d\n",priv->smf_media_stream_mode_type);
            return false;
        }
    }else if(priv->smf_media_stream_type == SMF_MEDIA_STREAM_RTSP){
        if (priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_CLIENT) {
            if ((priv->smf_media_id) && (priv->status == SMF_STREAM_STATUS_pause)) {
                if (!smf_media_rtsp_client_resume(priv)) {
                    MEDIA_ERR("rtsp client resume failed\n");
                    return false;
                }
            }
            else if (!smf_media_rtsp_client_start(priv)) {
                MEDIA_ERR("rtsp client start failed\n");
                return false;
            }
        }
        else {
            if (!smf_media_rtsp_server_start(priv)) {
                MEDIA_ERR("rtsp server start failed\n");
                return false;
            }
        }
    }else if (priv->smf_media_stream_type == SMF_MEDIA_STREAM_VAD) {
        priv->callback = &smf_media_audio_recorder_callback;
        priv->callback_priv = priv;
        if (!smf_media_vad_start(priv)) {
            MEDIA_ERR("audio recorder start failed\n");
            return false;
        }
    }else if (priv->smf_media_stream_type == SMF_MEDIA_STREAM_HWVAD) {
        priv->callback = &smf_media_audio_recorder_callback;
        priv->callback_priv = priv;
        if (!smf_media_vad_start(priv)) {
            MEDIA_ERR("audio recorder start failed\n");
            return false;
        }
    }else if (priv->smf_media_stream_type == SMF_MEDIA_STREAM_VOICE_REC) {
        priv->callback = &smf_media_audio_recorder_callback;
        priv->callback_priv = priv;
        if (!smf_media_audio_recorder_voice_rec_start(priv)) {
            MEDIA_ERR("audio recorder start failed\n");
            return false;
        }
    }else if (priv->smf_media_stream_type == SMF_MEDIA_STREAM_VCAPTURE) {
        if(!smf_media_video_recorder_start(priv)){
            MEDIA_ERR("video recorder start failed\n");
            return false;
        }
    }else if (priv->smf_media_stream_type == SMF_MEDIA_STREAM_PROMPT) {
        smf_media_get_volume(priv);
        if(priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_URL){
            if(!smf_media_audio_player_prompt_start(priv)){
                MEDIA_ERR("prompt player start failed\n");
                return false;
            }
        }else{
            MEDIA_ERR("video player type not supported %d\n",priv->smf_media_stream_mode_type);
            return false;
        }
    }else if (priv->smf_media_stream_type == SMF_MEDIA_STREAM_PCAPTURE) {
        if(!smf_media_photo_take_start(priv)){
            MEDIA_ERR("photo take start failed\n");
            return false;
        }
    }else{
        MEDIA_ERR("audio type not supported %d\n", priv->smf_media_stream_type);
        return false;
    }
    priv->status = SMF_STREAM_STATUS_play;
    return true;
}
bool smf_media_graph_stop(smf_media_thread_t* params)
{
    smf_media_priv_t* priv = media_server_get_data(params->cookie);
    if(!priv){
        MEDIA_ERR("priv is null\n");
        return false;
    }
    if(priv->smf_media_stream_type == SMF_MEDIA_STREAM_APLAYBACK){
        smf_media_audio_player_stop(priv);
    }else if(priv->smf_media_stream_type == SMF_MEDIA_STREAM_ACAPTURE){
        if(priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_TDM){
            smf_media_audio_recorder_tdm_stop(priv);
        }else{
            smf_media_audio_recorder_stop(priv);
            if(priv->sockfd>=0){
                close(priv->sockfd);
                priv->sockfd = -1;
            }
        }
    }else if(priv->smf_media_stream_type == SMF_MEDIA_STREAM_VPLAYBACK){
        if(priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_URL){
            smf_media_video_player_stop(priv);
        }
    }else if(priv->smf_media_stream_type == SMF_MEDIA_STREAM_VPLAYBACK){
        if(priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_BUF){
            smf_media_video_player_buffer_stop(priv);
        }
    }else if (priv->smf_media_stream_type == SMF_MEDIA_STREAM_VAD) {
        smf_media_vad_stop(priv);
        if(priv->sockfd>=0){
            close(priv->sockfd);
            priv->sockfd = -1;
        }
    }else if (priv->smf_media_stream_type == SMF_MEDIA_STREAM_HWVAD) {
        smf_media_vad_stop(priv);
        if(priv->sockfd>=0){
            close(priv->sockfd);
            priv->sockfd = -1;
        }
    }else if (priv->smf_media_stream_type == SMF_MEDIA_STREAM_VOICE_REC) {
        smf_media_audio_recorder_stop(priv);
        if(priv->sockfd>=0){
            close(priv->sockfd);
            priv->sockfd = -1;
        }
    }else if (priv->smf_media_stream_type == SMF_MEDIA_STREAM_VCAPTURE) {
        smf_media_video_recorder_stop(priv);
    }else if (priv->smf_media_stream_type == SMF_MEDIA_STREAM_RTSP) {
        if (priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_CLIENT) {
            smf_media_rtsp_client_stop(priv);
        }else {
            smf_media_rtsp_server_stop(priv);
        }
    }else if (priv->smf_media_stream_type == SMF_MEDIA_STREAM_PROMPT) {
        if(priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_URL){
            smf_media_audio_player_stop(priv);
        }
    }else if (priv->smf_media_stream_type == SMF_MEDIA_STREAM_PCAPTURE) {
        smf_media_photo_take_stop(priv);
    }else{
        MEDIA_ERR("stream type not supported %d\n", priv->smf_media_stream_type);
        return false;
    }
    priv->status = SMF_STREAM_STATUS_null;
    return true;
}
bool smf_media_graph_close(smf_media_thread_t* params)
{
    smf_media_priv_t* priv = media_server_get_data(params->cookie);
    if(!priv){
        MEDIA_ERR("priv is null\n");
        return false;
    }

    smf_media_ids_stop_all(priv);

    if(priv->sockfd>=0){
        bool ret = smf_media_graph_stop(params);
        if(!ret){
            MEDIA_ERR("audio stop failed\n");
            return false;
        }
    }
    int pending;
    if (params->arg)
        sscanf(params->arg, "%d", &pending);

    if (!params->arg || !pending)
        media_stub_notify_finalize(&params->cookie);

    priv->smf_media_stream_type = SMF_MEDIA_STREAM_DEF;
    priv->smf_media_stream_mode_type = SMF_MEDIA_STREAM_MODE_DEF;
    if(priv->sockfd>=0){
        close(priv->sockfd);
        priv->sockfd = -1;
    }
    priv->smf_media_id = 0;
    free(priv);
    return true;
}
bool smf_media_graph_set_volume(smf_media_thread_t* params)
{
    smf_media_priv_t* priv = media_server_get_data(params->cookie);
    if(!priv){
        MEDIA_ERR("priv is null\n");
        return false;
    }
    smf_media_get_volume(priv);
    double num = strtod(params->arg, NULL);
    priv->volume_ratio = num;
    if(priv->volume_ratio > 1.0) priv->volume_ratio = 1.0;
    int vol = (int)(priv->volume_ratio * priv->volume);
    MEDIA_INFO("vol %s %d %d\n", params->arg, priv->volume, vol);
    return smf_media_audio_player_set_volume(priv, vol);
}

bool smf_media_graph_get_volume(smf_media_thread_t* params)
{
    smf_media_priv_t* priv = media_server_get_data(params->cookie);
    if (!priv) {
        MEDIA_ERR("priv is null\n");
        return false;
    }
    sprintf(params->res, "vol:%f", priv->volume_ratio);
    return true;
}

bool smf_media_graph_set_mute(smf_media_thread_t* params)
{
    smf_media_priv_t* priv = media_server_get_data(params->cookie);
    if (!priv) {
        MEDIA_ERR("priv is null\n");
        return false;
    }
    int mute = strtoul(params->arg, NULL, 0);
    MEDIA_INFO("mute %s %d", params->arg, mute);
    return smf_media_audio_player_set_mute(priv, (bool)mute);
}
bool smf_media_graph_set_options(smf_media_thread_t* params){
    smf_media_priv_t* priv = media_server_get_data(params->cookie);
    if(!priv){
        MEDIA_ERR("priv is null\n");
        return false;
    }
    if(!params->arg){
        MEDIA_ERR("param arg is null\n");
        return false;
    }
    int len = strlen(params->arg);
    memset(priv->smf_opt, 0, SMF_PRIV_URL_LEN);
    if(len > SMF_PRIV_URL_LEN){
        MEDIA_ERR("arg len %d > %d \n", len, SMF_PRIV_URL_LEN);
        return false;
    }
    memcpy(priv->smf_opt, params->arg, len);
    priv->smf_opt[len] = '\0';
    return true;
}
bool smf_media_graph_get_duration(smf_media_thread_t* params){
    smf_media_priv_t* priv = media_server_get_data(params->cookie);
    if (!priv) {
        MEDIA_ERR("priv is null\n");
        return false;
    }
    int duration = 0;
    if (priv->meta.timepoint) {
        duration = priv->meta.timepoint->end;
    }
    MEDIA_INFO("duration %u", duration);
    sprintf(params->res, "%d", duration);
    return true;
}
bool smf_media_graph_pause(smf_media_thread_t* params)
{
    smf_media_priv_t* priv = media_server_get_data(params->cookie);
    if(!priv){
        MEDIA_ERR("priv is null\n");
        return false;
    }
    if(priv->smf_media_stream_type == SMF_MEDIA_STREAM_APLAYBACK){
        return smf_media_audio_player_pause(priv);
    }else if(priv->smf_media_stream_type == SMF_MEDIA_STREAM_ACAPTURE){
        priv->status = SMF_STREAM_STATUS_pause;
        return true;
        // return smf_media_audio_recorder_pause(priv);
    }
    else if (priv->smf_media_stream_type == SMF_MEDIA_STREAM_RTSP) {
        if (priv->smf_media_stream_mode_type == SMF_MEDIA_STREAM_MODE_CLIENT) {
            return smf_media_rtsp_client_pause(priv);
        }
        return false;
    }else{
        return false;
    }

}
bool smf_media_graph_seek(smf_media_thread_t* params)
{
    smf_media_priv_t* priv = media_server_get_data(params->cookie);
    if (!priv) {
        MEDIA_ERR("priv is null\n");
        media_stub_notify_event(params->cookie, MEDIA_EVENT_SEEKED, -1, 0);
        return false;
    }
    uint32_t timepoint = strtod(params->arg, NULL);;//parse the param for timepoint(ms).
    bool rst = smf_media_audio_player_seek(priv, timepoint);
    media_stub_notify_event(params->cookie, MEDIA_EVENT_SEEKED, 0, 0);
    return rst;
}
bool smf_media_graph_get_position(smf_media_thread_t* params)
{
    smf_media_priv_t* priv = media_server_get_data(params->cookie);
    if (!priv) {
        MEDIA_ERR("priv is null\n");
        return false;
    }
    int position = 0;
    if (priv->meta.timepoint) {
        position = priv->meta.timepoint->current;
    }
    MEDIA_INFO("position %u", position);
    sprintf(params->res, "%d", position);
    return true;
}
static void smf_media_process(smf_media_thread_t* param)
{
    if(!param || !param->cmd){
        MEDIA_ERR("param is null\n");
        return;
    }
    MEDIA_INFO("cmd %s %d\n", param->cmd, strlen(param->cmd));
    if (!strcmp(param->cmd, "open")) {
        smf_media_graph_open(param);
    }else if (!strcmp(param->cmd, "prepare")){
        if(!smf_media_graph_prepare(param)){
            media_stub_notify_event(param->cookie, MEDIA_EVENT_PREPARED, -1, 0);
        }else{
            media_stub_notify_event(param->cookie, MEDIA_EVENT_PREPARED, 0, 0);
        }
    }else if (!strcmp(param->cmd, "start")){
        if(!smf_media_graph_start(param)){
            media_stub_notify_event(param->cookie, MEDIA_EVENT_STARTED, -1, 0);
        }
    }else if (!strcmp(param->cmd, "stop")){
        if(!smf_media_graph_stop(param)){
            media_stub_notify_event(param->cookie, MEDIA_EVENT_STOPPED, -1, 0);
        }
    }else if (!strcmp(param->cmd, "close")){
        smf_media_graph_close(param);
    }else if (!strcmp(param->cmd, "pause")){
        if(!smf_media_graph_pause(param)){
            media_stub_notify_event(param->cookie, MEDIA_EVENT_PAUSED, -1, 0);
        }else{
            media_stub_notify_event(param->cookie, MEDIA_EVENT_PAUSED, 0, 0);
        }
    }else if (!strcmp(param->cmd, "seek")){
        smf_media_graph_seek(param);
    }else if (!strcmp(param->cmd, "get_position")) {
        smf_media_graph_get_position(param);
    }else if (!strcmp(param->cmd, "volume")){
        smf_media_graph_set_volume(param);
    }else if (!strcmp(param->cmd, "get_volume")){
        smf_media_graph_get_volume(param);
    }else if (!strcmp(param->cmd, "mute")) {
        smf_media_graph_set_mute(param);
    }else if (!strcmp(param->cmd, "set_options")){
        smf_media_graph_set_options(param);
    }else if (!strcmp(param->cmd, "get_duration")){
        smf_media_graph_get_duration(param);
    }else{
        MEDIA_INFO("cmd %s\n", param->cmd);
    }
    return;
}
static int smf_media_common_handler(void* cookie, const char* target, const char* cmd, const char* arg,
    char* res, int res_len, bool player)
{
    MEDIA_INFO("cookie %p, target:%s, cmd:%s ,arg:%s, res:%s, res_len:%d, player:%d\n", cookie, target, cmd, arg, res, res_len, player);

    smf_media_thread_t param;
    param.cmd = cmd;
    param.arg = arg;
    param.player = player;
    param.cookie = cookie;
    param.res = res;
    smf_media_process(&param);
    MEDIA_INFO("process exit %s\n", cmd);
    return 0;
}

int smf_media_player_handler(void* cookie, const char* target, const char* cmd,
    const char* arg, char* res, int res_len)
{
    return smf_media_common_handler(cookie, target, cmd, arg, res, res_len, true);
}
int smf_media_recorder_handler(void* cookie, const char* target, const char* cmd,
    const char* arg, char* res, int res_len)
{
    return smf_media_common_handler(cookie, target, cmd, arg, res, res_len, false);
}
