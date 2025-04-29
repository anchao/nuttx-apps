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

#define SMF_RECV_DATA_SIZE 1024

static float default_volume = 0.9;

static void smf_media_socket_recv_data(smf_media_priv_t* priv){
    if(!priv){
        MEDIA_ERR("priv is null\n");
        return;
    }
    char* recv_data = zalloc(SMF_RECV_DATA_SIZE);
    if(!recv_data){
        MEDIA_ERR("malloc failed \n");
        return;
    }
    while(priv->sockfd>0){
        ssize_t len = recv(priv->sockfd, recv_data, SMF_RECV_DATA_SIZE, 0);
        MEDIA_INFO("recv len %d \n", len);
        if (len<=0) {
            perror("recv");
            if(priv->sockfd>0){
                close(priv->sockfd);
                priv->sockfd = -1;
            }
            if(priv->audio_id)smf_media_audio_player_stop(priv->audio_id);
            priv->audio_id = 0;
            break;
        }else{
            if(priv->audio_id)smf_audio_player_frame_push(priv->audio_id, recv_data, len);
        }
    }
    if(recv_data)free(recv_data);
    MEDIA_INFO("recv data thread exit");
}

static int smf_media_socket_recv_data_thread_create(smf_media_priv_t* priv){
    pthread_t pid;
    pthread_attr_t pattr;
    struct sched_param sparam;
    pthread_attr_init(&pattr);
    pthread_attr_setstacksize(&pattr, 4096);
    sparam.sched_priority = sched_get_priority_max(SCHED_FIFO) - 9;
    pthread_attr_setschedparam(&pattr, &sparam);

    int ret = pthread_create(&pid, &pattr, smf_media_socket_recv_data, priv);
    if (ret){
        MEDIA_ERR("create smf media thread error %d\n", ret);
        return -1;
    }
    pthread_setname_np(pid, "smf_recv_data");
    pthread_attr_destroy(&pattr);
    pthread_detach(pid);
    return 0;
}

static int smf_media_get_sockaddr(smf_media_priv_t* priv){
    if(!priv){
        MEDIA_ERR("priv is null\n");
        return;
    }
    struct sockaddr_un addr;
    priv->sockfd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (priv->sockfd <= 0) {
        perror("socket");
        return -errno;
    }
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, priv->audio_url, sizeof(addr.sun_path) - 1);

    if (connect(priv->sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0) {
        perror("connect");
        close(priv->sockfd);
        priv->sockfd = -1;
        return -1;
    }
    return 0;
}

static void smf_media_audio_recorder_callback(uint64_t id, void* buffer, uint32_t size, void* priv){
    if(!priv){
        MEDIA_ERR("audio sockfd priv is null\n");
        return;
    }
    // MEDIA_INFO("priv %p \n", priv);
    smf_media_priv_t* cbpriv = (smf_media_priv_t*)priv;
    
    if(cbpriv->sockfd <= 0){
        MEDIA_ERR("audio sockfd is error %d\n", cbpriv->sockfd);
        return;
    }
    MEDIA_INFO("send size %d \n", size);
    int rsize = send(cbpriv->sockfd, buffer, size, 0);
    if(rsize > 0){
        if(rsize != size){
            MEDIA_ERR("audio send rsize %d size %d\n", rsize, size);
        }
    }else if(rsize<0){
        perror("send");
        if(cbpriv->sockfd){
            close(cbpriv->sockfd);
            cbpriv->sockfd = -1;
        }
        return;
    }else{
        MEDIA_INFO("send size 0\n");
    }
}

static void smf_media_audio_player_callback(uint64_t id, SMF_AUDIO_PLAYER_STATUS cmd_id, void* priv){
    if(!priv){
        MEDIA_ERR("audio priv is null\n");
        return;
    }
    switch (cmd_id){
    case SMF_AUDIO_PLAYER_STATUS_START:
        MEDIA_INFO("audio player start \n");
        media_stub_notify_event(priv, MEDIA_EVENT_STARTED, 0, 0);
        break;
    case SMF_AUDIO_PLAYER_STATUS_PAUSE:
        MEDIA_INFO("audio player pause \n");
        media_stub_notify_event(priv, MEDIA_EVENT_PAUSED, 0, 0);
        break;
    case SMF_AUDIO_PLAYER_STATUS_RESUME:
        MEDIA_INFO("audio player resume \n");
        media_stub_notify_event(priv, MEDIA_EVENT_STARTED, 0, 0);
        break;
    case SMF_AUDIO_PLAYER_STATUS_STOP:
        MEDIA_INFO("audio player stop \n");
        media_stub_notify_event(priv, MEDIA_EVENT_STOPPED, 0, 0);
        break;
    case SMF_AUDIO_PLAYER_STATUS_EOS:
        MEDIA_INFO("audio player eos \n");
        media_stub_notify_event(priv, MEDIA_EVENT_COMPLETED, 0, 0);
        break;
    default:
        MEDIA_INFO("audio msg cmd id %d", cmd_id);
        break;
    }
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
    if(!params->arg){
        MEDIA_ERR("param arg is null\n");
        return false;
    }
    if(!strcmp(params->arg, "Music")){
        priv->audio_type = SMF_MEDIA_AUDIO_PLAYER;
    }else if(!strcmp(params->arg, "Capture")){
        priv->audio_type = SMF_MEDIA_AUDIO_RECORDER;
    }else{
    }
    media_server_set_data(params->cookie, priv);
    MEDIA_INFO("default_volume %.1f \n", default_volume);
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
    int asize = sizeof(priv->audio_url);
    if(len > asize){
        MEDIA_ERR("arg len > %d \n", asize);
        return false;
    }
    memcpy(priv->audio_url, params->arg, len);
    priv->audio_url[len] = '\0';
    if(priv->audio_type == SMF_MEDIA_AUDIO_PLAYER){
        if(!strncmp(params->arg, "med", 3)){
            priv->audio_player_type = SMF_MEDIA_AUDIO_PLAYE_BUF;
        }else{
            priv->audio_player_type = SMF_MEDIA_AUDIO_PLAYE_URL;
        }
        
    }else if(priv->audio_type == SMF_MEDIA_AUDIO_RECORDER){
        if(!strncmp(params->arg, "med", 3)){
            priv->audio_recorder_type = SMF_MEDIA_AUDIO_RECORDER_BUF;
        }else{
            priv->audio_recorder_type = SMF_MEDIA_AUDIO_RECORDER_URL;
        }
    }
    if( (priv->audio_player_type == SMF_MEDIA_AUDIO_PLAYE_BUF) || (priv->audio_recorder_type == SMF_MEDIA_AUDIO_RECORDER_BUF) ){
        int ret = smf_media_get_sockaddr(priv);
        if(ret < 0){
            MEDIA_ERR("smf media audio player buffer mode connet failed\n");
            return false;
        }
    }
    media_stub_notify_event(params->cookie, MEDIA_EVENT_PREPARED, 0, 0);
    return true;
    
}
bool smf_media_graph_start(smf_media_thread_t* params)
{
    smf_media_priv_t* priv = media_server_get_data(params->cookie);
    if(!priv){
        MEDIA_ERR("priv is null\n");
        return false;
    }
    if(priv->audio_id){
        uint32_t sts = smf_audio_player_get_status(priv->audio_id);
        if(sts == 3){
            return smf_audio_player_resume(priv->audio_id);
        }
    }
    if(memcmp(priv->audio_url, "\0", sizeof(priv->audio_url)) == 0){
        MEDIA_ERR("audio audio_url is null\n");
        return false;
    }
    int vol = (int)(default_volume*SMF_VOLUME_MAX);
    MEDIA_INFO("default_volume %.1f %d \n", default_volume, vol);
    if(priv->audio_type == SMF_MEDIA_AUDIO_PLAYER){
        bool ret = smf_media_audio_output_config();
        if(!ret){
            MEDIA_ERR("audio player set output failed\n");
            return false;
        }
        if(priv->audio_player_type == SMF_MEDIA_AUDIO_PLAYE_URL){
            priv->audio_id = smf_media_audio_player_url_start(&smf_media_audio_player_callback, priv->audio_url, params->cookie, vol);
            if(!priv->audio_id){
                MEDIA_ERR("audio player start failed\n");
                return false;
            }
        }else if(priv->audio_player_type == SMF_MEDIA_AUDIO_PLAYE_BUF){
            priv->audio_id = smf_media_audio_player_buffer_start(vol);
            if(!priv->audio_id){
                MEDIA_ERR("audio player start failed\n");
                return false;
            }
            int ret = smf_media_socket_recv_data_thread_create(priv);
            if( ret<0 ){
                MEDIA_ERR("audio player buf mode failed \n");
                smf_media_audio_player_stop(priv->audio_id);
                return false;
            }
        }else{
            MEDIA_ERR("audio player type not supported %d\n",priv->audio_player_type);
            return false;
        }
        // smf_media_hook_start(0);
    }else if(priv->audio_type == SMF_MEDIA_AUDIO_RECORDER){
        bool ret = smf_media_audio_input_config();
        if(!ret){
            MEDIA_ERR("audio recorder set input failed\n");
            return false;
        }
        if(priv->audio_recorder_type == SMF_MEDIA_AUDIO_RECORDER_URL){
            priv->audio_id = smf_media_audio_recorder_url_start(priv->audio_url, "pcm");
            if(!priv->audio_id){
                MEDIA_ERR("audio recorder start failed\n");
                return false;
            }
        }else if(priv->audio_recorder_type == SMF_MEDIA_AUDIO_RECORDER_BUF){
            MEDIA_INFO("priv %p \n", priv);
            priv->audio_id = smf_media_audio_recorder_buffer_start(&smf_media_audio_recorder_callback, "pcm", (void*)priv);
            if(!priv->audio_id){
                MEDIA_ERR("audio recorder start failed\n");
                return false;
            }
        }else{
            MEDIA_ERR("audio recorder type not supported %d\n",priv->audio_recorder_type);
            return false;
        }
    }else{
        MEDIA_ERR("audio type not supported %d\n", priv->audio_type);
        return false;
    }
    return true;
}
bool smf_media_graph_stop(smf_media_thread_t* params)
{
    smf_media_priv_t* priv = media_server_get_data(params->cookie);
    if(!priv){
        MEDIA_ERR("priv is null\n");
        return false;
    }
    if(priv->audio_type == SMF_MEDIA_AUDIO_PLAYER){
        // smf_media_hook_stop(0);
        if(priv->audio_player_type == SMF_MEDIA_AUDIO_PLAYE_URL){
            if(priv->audio_id){
                smf_media_audio_player_stop(priv->audio_id);
                priv->audio_id = 0;
            }
        }
        // }else if(priv->audio_player_type == SMF_MEDIA_AUDIO_PLAYE_BUF){

        // }else{

        // }

    }else if(priv->audio_type == SMF_MEDIA_AUDIO_RECORDER){
        if(priv->audio_id){
            smf_audio_recorder_stop(priv->audio_id);
            priv->audio_id = 0;
        }
        if(priv->sockfd){
            close(priv->sockfd);
            priv->sockfd = -1;
        }
    }else{
        MEDIA_ERR("audio type not supported %d\n", priv->audio_type);
        return false;
    }
    return true;
}
bool smf_media_graph_close(smf_media_thread_t* params)
{
    smf_media_priv_t* priv = media_server_get_data(params->cookie);
    if(!priv){
        MEDIA_ERR("priv is null\n");
        return false;
    }
    if(priv->audio_id || priv->sockfd>0){
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

    priv->audio_type = SMF_MEDIA_AUDIO_DEF;
    priv->audio_player_type = SMF_MEDIA_AUDIO_PLAYE_DEF;
    if(priv->sockfd>0){
        close(priv->sockfd);
        priv->sockfd = -1;
    }
    priv->audio_id = 0;
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
    double num = strtod(params->arg, NULL);
    default_volume = num;
    int vol = (int)(num*SMF_VOLUME_MAX);
    MEDIA_INFO("vol %s %d\n", params->arg, vol);
    if(priv->audio_type == SMF_MEDIA_AUDIO_PLAYER){
        if(priv->audio_id)smf_audio_player_set_volume(priv->audio_id, vol);
    }else{
        MEDIA_ERR("audio type not supported %d\n", priv->audio_type);
        return false;
    }
    MEDIA_INFO("default_volume %.1f \n", default_volume);
    return true;
}
bool smf_media_graph_pause(smf_media_thread_t* params)
{
    smf_media_priv_t* priv = media_server_get_data(params->cookie);
    if(!priv){
        MEDIA_ERR("priv is null\n");
        return false;
    }
    if(priv->audio_id)smf_audio_player_pause(priv->audio_id);
    return true;
}
bool smf_media_graph_seek(smf_media_thread_t* params)
{
    MEDIA_ERR("Seek not supported \n");
    media_stub_notify_event(params->cookie, MEDIA_EVENT_SEEKED, -1, 0);
    return false;
}
static void smf_media_thread_process(smf_media_thread_t* param)
{
    if(!param || !param->cmd){
        MEDIA_ERR("param is null\n");
        return;
    }
    MEDIA_INFO("cmd %s %d\n", param->cmd, strlen(param->cmd));
    if (!strcmp(param->cmd, "open")) {
        smf_media_graph_open(param);
    }else if (!strcmp(param->cmd, "prepare")){
        smf_media_graph_prepare(param);
    }else if (!strcmp(param->cmd, "start")){
        smf_media_graph_start(param);
    }else if (!strcmp(param->cmd, "stop")){
        smf_media_graph_stop(param);
    }else if (!strcmp(param->cmd, "close")){
        smf_media_graph_close(param);
    }else if (!strcmp(param->cmd, "pause")){
        smf_media_graph_pause(param);
    }else if (!strcmp(param->cmd, "seek")){
        smf_media_graph_seek(param);
    }else if (!strcmp(param->cmd, "volume")){
        smf_media_graph_set_volume(param);
    }else{
        MEDIA_INFO("cmd %s\n", param->cmd);
    }
    return;
}
static int smf_media_common_handler(void* cookie, const char* target, const char* cmd, const char* arg,
    char* res, int res_len, bool player)
{
    MEDIA_INFO("target:%s, cmd:%s ,arg:%s, res:%s, res_len:%d, player:%d\n", target, cmd, arg, res, res_len, player);

    if (!strcmp(cmd, "get_volume")){
        sprintf(res, "vol:%f", default_volume);
        return 0;
    }

    pthread_t smf_pid;
    smf_media_thread_t param;
    pthread_attr_t pattr;
    struct sched_param sparam;
    pthread_attr_init(&pattr);
    pthread_attr_setstacksize(&pattr, 4096);
    sparam.sched_priority = sched_get_priority_max(SCHED_FIFO) - 9;
    pthread_attr_setschedparam(&pattr, &sparam);

    param.cmd = cmd;
    param.arg = arg;
    param.player = player;
    param.cookie = cookie;
    int ret = pthread_create(&smf_pid, &pattr, smf_media_thread_process, (pthread_addr_t)&param);
    if (ret){
        MEDIA_ERR("create smf media thread error %d\n", ret);
        return -1;
    }
    pthread_setname_np(smf_pid, "smf_media_thread");
    pthread_attr_destroy(&pattr);
    pthread_join(smf_pid, NULL);

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
