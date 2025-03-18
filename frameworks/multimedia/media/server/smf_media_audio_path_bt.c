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

/****************************** header include ********************************/
#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include <uv.h>

#include "media_common.h"
#include "smf_media_graph.api.h"
#include "smf_media_audio_path_bt.h"

/***************************** external declaration ***************************/

/***************************** macro defination *******************************/
#define SMF_MEDIA_AUDIO_BT_THREAD_STACK_NAME        "smf_media_audio_bt"
#define SMF_MEDIA_AUDIO_BT_THREAD_STACK_PRIORITY    103
#define SMF_MEDIA_AUDIO_BT_THREAD_STACK_SIZE        3*1024
#define SMF_MEDIA_AUDIO_BT_A2DP_CTRL_CFG_LEN        29
#define SMF_MEDIA_AUDIO_BT_A2DP_SRC_SEND_INTERVAL   10
#define SMF_MEDIA_AUDIO_BT_A2DP_SRC_SEND_REPEAT     20

// sync@LOAS_HDRSIZE
#define SMF_MEDIA_AUDIO_BT_A2DP_DATA_HDRSIZE   3
#define SMF_MEDIA_AUDIO_BT_A2DP_DATA_HEAD      0x56E0
#define SMF_MEDIA_AUDIO_BT_A2DP_DATA_HEAD_MASK 0xFFE0
#define SMF_MEDIA_AUDIO_BT_A2DP_DATA_LEN_MASK  0x1FFF

#define CONFIG_BLUETOOTH_A2DP_SINK_CTRL_PATH   "a2dp_sink_ctrl"
#define CONFIG_BLUETOOTH_A2DP_SINK_DATA_PATH   "a2dp_sink_data"
#define CONFIG_BLUETOOTH_A2DP_SOURCE_CTRL_PATH "a2dp_source_ctrl"
#define CONFIG_BLUETOOTH_A2DP_SOURCE_DATA_PATH "a2dp_source_data"
#define CONFIG_BLUETOOTH_LEA_SINK_CTRL_PATH    "lea_sink_ctrl"
#define CONFIG_BLUETOOTH_LEA_SINK_DATA_PATH    "lea_sink_data"
#define CONFIG_BLUETOOTH_LEA_SOURCE_CTRL_PATH  "lea_source_ctrl"
#define CONFIG_BLUETOOTH_LEA_SOURCE_DATA_PATH  "lea_source_data"
#define CONFIG_BLUETOOTH_SCO_CTRL_PATH         "sco_ctrl"

#define STREAM_SET_DATA_HEADER(p)   {p[0] = (SMF_MEDIA_AUDIO_BT_A2DP_DATA_HEAD >> 8); p[1]=SMF_MEDIA_AUDIO_BT_A2DP_DATA_HEAD&0x00FF;}
#define STREAM_SET_DATA_LEN(p, len) {p[1] |= ((len&SMF_MEDIA_AUDIO_BT_A2DP_DATA_LEN_MASK) >> 8); p[2]=len&0x00FF;}
#define STREAM_GET_DATA_HEADER(p)   (((p[0] << 8) | p[1]) & SMF_MEDIA_AUDIO_BT_A2DP_DATA_HEAD_MASK)
#define STREAM_GET_DATA_LEN(p)      (((p[1] << 8) | p[2]) & SMF_MEDIA_AUDIO_BT_A2DP_DATA_LEN_MASK)

#define STREAM_TO_UINT8(u8, p)  \
    {                           \
        (u8) = (uint8_t)(*(p)); \
        (p) += 1;               \
    }
#define STREAM_TO_UINT32(u32, p)                                                                                                                    \
    {                                                                                                                                               \
        (u32) = (((uint32_t)(*(p))) + ((((uint32_t)(*((p) + 1)))) << 8) + ((((uint32_t)(*((p) + 2)))) << 16) + ((((uint32_t)(*((p) + 3)))) << 24)); \
        (p) += 4;                                                                                                                                   \
    }

#ifndef OFFSETOF
#define OFFSETOF(type, member) ((unsigned int) &((type *)0)->member)
#endif

#ifndef CONTAINER_OF
#define CONTAINER_OF(ptr, type, member) ((type *)( (char *)ptr - OFFSETOF(type,member) ))
#endif

/*****************************  type defination ********************************/
typedef enum
{
    // sync@a2dp_ctrl_evt_t
    // sync@audio_ctrl_evt_t
    BT_AUDIO_CTRL_EVT_STARTED,
    BT_AUDIO_CTRL_EVT_START_FAIL,
    BT_AUDIO_CTRL_EVT_STOPPED,
    BT_AUDIO_CTRL_EVT_UPDATE_CONFIG
} A2DP_CTRL_EVT_T;

typedef enum
{
    //sync@a2dp_ctrl_cmd_t
    A2DP_CTRL_CMD_START,
    A2DP_CTRL_CMD_STOP,
    A2DP_CTRL_CMD_CONFIG_DONE
}  A2DP_CTRL_CMD_T;

typedef enum {
    BTS_A2DP_TYPE_SBC,
    BTS_A2DP_TYPE_MPEG1_2_AUDIO,
    BTS_A2DP_TYPE_MPEG2_4_AAC,
    BTS_A2DP_TYPE_ATRAC,
    BTS_A2DP_TYPE_OPUS,
    BTS_A2DP_TYPE_H263,
    BTS_A2DP_TYPE_MPEG4_VSP,
    BTS_A2DP_TYPE_H263_PROF3,
    BTS_A2DP_TYPE_H263_PROF8,
    BTS_A2DP_TYPE_LHDC,
    BTS_A2DP_TYPE_NON_A2DP
} A2DP_CODE_TYPE_T;

typedef enum
{
     SMF_BT_PIPE_A2DP_SRC_CTRL = 0,
     SMF_BT_PIPE_A2DP_SRC_DATA,
     SMF_BT_PIPE_A2DP_SINK_CTRL,
     SMF_BT_PIPE_A2DP_SINK_DATA,
     SMF_BT_PIPE_LEA_SRC_CTRL,
     SMF_BT_PIPE_LEA_SRC_DATA,
     SMF_BT_PIPE_LEA_SINK_CTRL,
     SMF_BT_PIPE_LEA_SINK_DATA,
     SMF_BT_PIPE_SCO_CTRL,
     SMF_BT_PIPE_MAX,
} SMF_BT_PIPE_TYPE;

typedef struct {
    // sysnc@smf_media_a2dp_source_send_info_t
    uint16_t    total_len;
    uint16_t    seqnumber;
    uint16_t    frame_send_time;
    uint16_t    frame_len;
    uint32_t    frame_num;
    uint32_t    codec_type;                //sbc:0, aac:2 pcm:4
}smf_media_a2dp_source_header_t;

typedef struct
{
    uint8_t     started_ignore;
    uint64_t    media_id;
    pthread_mutex_t lock;
    smf_media_audio_bt_codec_cfg_t* codec_cfg;

    uv_connect_t conn;
    uv_pipe_t    hdl;
    uv_read_cb   read_cb;
    char*        name;
    uint8_t*     read_buf;
    uint8_t*     write_buf;
    uv_buf_t     pdu_cache;
} smf_media_audio_bt_pipe_t;

typedef struct
{
    uv_write_t req;
    uv_buf_t   buf;
    smf_media_audio_bt_pipe_t* pipe;
} smf_media_audio_bt_write_pkt_t;

typedef struct
{
    SMF_MEDIA_AUDIO_BT_PATH_TYPE path_map;
    // Thread
    pthread_attr_t thread_attr;
    pthread_t      thread_id;
    /// Pipe
    uv_loop_t*     uv_loop;
    uv_timer_t     uv_timer;
    smf_media_audio_bt_pipe_t pipe[SMF_BT_PIPE_MAX];
} smf_media_audio_bt_env_t;
/*****************************  variable defination *****************************/
static smf_media_audio_bt_env_t smf_media_audio_bt_env = {0};

/*****************************  function declaration ****************************/
static void smf_media_audio_bt_read_malloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    smf_media_audio_bt_pipe_t* bt_pipe = NULL;

    buf->base = (char*)malloc(suggested_size);
    buf->len = suggested_size;

    bt_pipe = CONTAINER_OF(handle, smf_media_audio_bt_pipe_t, hdl);

    pthread_mutex_lock(&bt_pipe->lock);
    bt_pipe->read_buf = (uint8_t *)buf->base;
    pthread_mutex_unlock(&bt_pipe->lock);
}

static void smf_media_audio_bt_read_free(uv_handle_t *handle, const uv_buf_t *buf)
{
    smf_media_audio_bt_pipe_t* bt_pipe = NULL;

    if ((!buf) || (!buf->base))
    {
        return;
    }

    free(buf->base);

    bt_pipe = CONTAINER_OF(handle, smf_media_audio_bt_pipe_t, hdl);
    pthread_mutex_lock(&bt_pipe->lock);
    bt_pipe->read_buf = NULL;
    pthread_mutex_unlock(&bt_pipe->lock);
}

static void smf_media_audio_bt_write_done(uv_write_t* req, int status)
{
    smf_media_audio_bt_write_pkt_t *write_pkt = NULL;

    if(status)
    {
        MEDIA_INFO("Write error, %p, %d \n", req, status);
    }

    write_pkt= CONTAINER_OF(req, smf_media_audio_bt_write_pkt_t, req);

    pthread_mutex_lock(&write_pkt->pipe->lock);
    write_pkt->pipe->write_buf = NULL;
    pthread_mutex_unlock(&write_pkt->pipe->lock);

    free(req);
}

static void* smf_media_audio_bt_send_buf_new(smf_media_audio_bt_pipe_t* pipe, uint32_t data_len)
{
    uint8_t* temp_buf = NULL;
    smf_media_audio_bt_write_pkt_t *write_pkt = NULL;

    write_pkt = (smf_media_audio_bt_write_pkt_t *)malloc(sizeof(smf_media_audio_bt_write_pkt_t) + data_len);
    if (!write_pkt)
    {
        MEDIA_WARN("malloc fail! \n");
        return temp_buf;
    }

    // save write buf
    pthread_mutex_lock(&pipe->lock);
    pipe->write_buf = (uint8_t *)write_pkt;
    write_pkt->pipe = pipe;
    pthread_mutex_unlock(&pipe->lock);

    // send packet
    temp_buf = (uint8_t *)(write_pkt+1);
    write_pkt->buf = uv_buf_init((char *)temp_buf, data_len);

    return temp_buf;
}

static void smf_media_audio_bt_send_buf_len_update(uint8_t *buf, uint32_t len)
{
    smf_media_audio_bt_write_pkt_t *write_pkt = NULL;

    if(!buf)
    {
        MEDIA_WARN("buf=%p! \n", buf);
        return;
    }
    write_pkt = (smf_media_audio_bt_write_pkt_t*)(buf - sizeof(smf_media_audio_bt_write_pkt_t));
    write_pkt->buf.len = len;
}

static void smf_media_audio_bt_send_buf_free(uint8_t *buf)
{
    smf_media_audio_bt_write_pkt_t *write_pkt = NULL;

    if(!buf)
    {
        MEDIA_WARN("buf=%p! \n", buf);
        return;
    }
    write_pkt = (smf_media_audio_bt_write_pkt_t*)(buf - sizeof(smf_media_audio_bt_write_pkt_t));

    free(write_pkt);
}

static bool smf_media_audio_bt_send_buf(uint8_t *buf)
{
    int ret = 0;
    smf_media_audio_bt_write_pkt_t *write_pkt = NULL;
    smf_media_audio_bt_pipe_t* pipe = NULL;

    if (!buf)
    {
        MEDIA_WARN("buf=%p! \n", buf);
        return false;
    }
    write_pkt = (smf_media_audio_bt_write_pkt_t*)(buf - sizeof(smf_media_audio_bt_write_pkt_t));
    pipe = write_pkt->pipe;

    ret = uv_write((uv_write_t*)write_pkt, (uv_stream_t*)&write_pkt->pipe->hdl,
            &write_pkt->buf, 1, smf_media_audio_bt_write_done);
    if (ret)
    {
        pthread_mutex_lock(&pipe->lock);
        pipe->write_buf = NULL;
        pthread_mutex_unlock(&pipe->lock);
        free(write_pkt);
    }

    return ret? false:true;
}

static bool smf_media_audio_bt_send_cmd(smf_media_audio_bt_pipe_t* pipe, uint8_t cmd)
{
    uint8_t* buf = NULL;

    buf = smf_media_audio_bt_send_buf_new(pipe, sizeof(cmd));
    if (!buf)
    {
        MEDIA_WARN("new buffer fail! \n");
        return false;
    }

    *buf = cmd;
    return smf_media_audio_bt_send_buf(buf);
}

static void smf_media_audio_bt_conn_done(uv_connect_t* req, int status)
{
    int ret;
    smf_media_audio_bt_pipe_t* bt_pipe = NULL;

    bt_pipe = CONTAINER_OF(req, smf_media_audio_bt_pipe_t, conn);
    if(status)
    {
        MEDIA_ERR("%s, %d \n", bt_pipe->name, status);
    }
    else
    {
        MEDIA_INFO("%s, %d \n", bt_pipe->name, status);
    }

    ret = uv_read_start((uv_stream_t*)&bt_pipe->hdl,
                  smf_media_audio_bt_read_malloc, bt_pipe->read_cb);
    if (ret)
    {
        MEDIA_WARN("pipe fail! %d \n", ret);
    }
}

static void smf_media_audio_bt_disconn_done(uv_handle_t* handle)
{
    smf_media_audio_bt_pipe_t* bt_pipe = NULL;

    bt_pipe = CONTAINER_OF(handle, smf_media_audio_bt_pipe_t, hdl);
    MEDIA_INFO("%p \n", bt_pipe);
}

static bool smf_media_audio_bt_creat_pipe(smf_media_audio_bt_pipe_t* pipe, char* name, uv_read_cb read_cb)
{
    int ret;
    smf_media_audio_bt_env_t* smf_bt_env = &smf_media_audio_bt_env;

    if (pipe->name)
    {
         MEDIA_INFO("already pipe=%s \n", pipe->name);
         return true;
    }

    pipe->name    = name;
    pipe->read_cb = read_cb;
    ret =pthread_mutex_init(&pipe->lock, NULL);
    if (ret)
    {
        MEDIA_WARN("mutex creat fail! \n");
    }

    ret = uv_pipe_init(smf_bt_env->uv_loop, &pipe->hdl, 1);
    if (ret)
    {
        MEDIA_WARN("pipe fail! %d \n", ret);
    }

    uv_pipe_connect(&pipe->conn, &pipe->hdl,
                    pipe->name, smf_media_audio_bt_conn_done);

    return true;
}

static bool smf_media_audio_bt_delete_pipe(smf_media_audio_bt_pipe_t *pipe)
{
    if(!pipe->name)
    {
        MEDIA_WARN("not creat, %p! \n", pipe);
        return false;
    }

    pthread_mutex_lock(&pipe->lock);
    uv_read_stop((uv_stream_t*)&pipe->hdl);
    uv_close((uv_handle_t*)&pipe->hdl, smf_media_audio_bt_disconn_done);

    if(pipe->codec_cfg)
    {
        free(pipe->codec_cfg);
    }
    if (pipe->read_buf)
    {
        free(pipe->read_buf);
    }
    if (pipe->write_buf)
    {
        free(pipe->write_buf);
    }
    if(pipe->pdu_cache.base)
    {
        free(pipe->pdu_cache.base);
    }
    pthread_mutex_unlock(&pipe->lock);
    pthread_mutex_destroy(&pipe->lock);
    memset(pipe, 0, sizeof(smf_media_audio_bt_pipe_t));

    return true;
}

static uint32_t smf_media_audio_bt_a2dp_codec_update(smf_media_audio_bt_codec_cfg_t** cfg_ptr, uint8_t* param_data)
{
    uint8_t* data = param_data;
    smf_media_audio_bt_codec_cfg_t* cfg_param = *cfg_ptr;

    if(!cfg_param)
    {
        cfg_param = (smf_media_audio_bt_codec_cfg_t *)malloc(sizeof(smf_media_audio_bt_codec_cfg_t));
        if (!cfg_param)
        {
            MEDIA_INFO("%p \n", cfg_param);
            return 0;
        }
        *cfg_ptr = cfg_param;

        cfg_param->type = SMF_MEDIA_AUDIO_BT_A2DP;
    }

    STREAM_TO_UINT8(cfg_param->codec_param.a2dp.valid_code, data);
    if(!cfg_param->codec_param.a2dp.valid_code)
    {
        return data - param_data;
    }

    STREAM_TO_UINT32(cfg_param->codec_param.a2dp.codec_type, data);
    STREAM_TO_UINT32(cfg_param->codec_param.a2dp.sample_rate, data);
    STREAM_TO_UINT32(cfg_param->codec_param.a2dp.bits_per_sample, data);
    STREAM_TO_UINT32(cfg_param->codec_param.a2dp.channel_mode, data);
    STREAM_TO_UINT32(cfg_param->codec_param.a2dp.bit_rate, data);
    STREAM_TO_UINT32(cfg_param->codec_param.a2dp.frame_size, data);
    STREAM_TO_UINT32(cfg_param->codec_param.a2dp.packet_size, data);
    if (cfg_param->codec_param.a2dp.codec_type == BTS_A2DP_TYPE_SBC)
    {
        STREAM_TO_UINT32(cfg_param->codec_param.a2dp.diff_param.sbc.s16ChannelMode, data);
        STREAM_TO_UINT32(cfg_param->codec_param.a2dp.diff_param.sbc.s16NumOfBlocks, data);
        STREAM_TO_UINT32(cfg_param->codec_param.a2dp.diff_param.sbc.s16NumOfSubBands, data);
        STREAM_TO_UINT32(cfg_param->codec_param.a2dp.diff_param.sbc.s16AllocationMethod, data);
        STREAM_TO_UINT32(cfg_param->codec_param.a2dp.diff_param.sbc.s16BitPool, data);
    }
    else if (cfg_param->codec_param.a2dp.codec_type == BTS_A2DP_TYPE_MPEG2_4_AAC)
    {
        STREAM_TO_UINT32(cfg_param->codec_param.a2dp.diff_param.aac.u16ObjectType, data);
        STREAM_TO_UINT32(cfg_param->codec_param.a2dp.diff_param.aac.u16VariableBitRate, data);
    }

    return data - param_data;
}

static void smf_media_audio_bt_a2dp_src_send_data(uv_timer_t* handle)
{
    uint8_t* send_buf = NULL;
    uint32_t fifo_size;
    uint32_t send_len;
    uint32_t header_len;
    uint32_t data_len;
    smf_media_audio_bt_env_t* smf_bt_env = &smf_media_audio_bt_env;
    smf_media_a2dp_source_header_t media_audio;
    A2DP_CODE_TYPE_T  codec_type = 0;

    while(1)
    {
        fifo_size = smf_media_kfifo_data_size();
        if (fifo_size < sizeof(smf_media_a2dp_source_send_info_t))
        {
            return;
        }
        header_len = OFFSETOF(smf_media_a2dp_source_send_info_t, frame_buffer);

        send_len = smf_media_kfifo_data_pull((uint8_t *)&media_audio, header_len);
        if (send_len != header_len)
        {
            MEDIA_ERR("read fail! %ld, %ld \n", send_len, header_len);
            continue;
        }
        // unpack media data
        if (media_audio.total_len != (media_audio.frame_num * media_audio.frame_len))
        {
            MEDIA_WARN("data header error! %d, %ld, %d \n",
                media_audio.total_len, media_audio.frame_num, media_audio.frame_len);
        }

        data_len = sizeof(smf_media_a2dp_source_send_info_t) - header_len;
        send_buf = smf_media_audio_bt_send_buf_new(&smf_bt_env->pipe[SMF_BT_PIPE_A2DP_SRC_DATA],
            data_len);
        if (!send_buf)
        {
            MEDIA_ERR("malloc fail! %p \n", send_buf);
            return;
        }

        codec_type = smf_bt_env->pipe[SMF_BT_PIPE_A2DP_SRC_CTRL].codec_cfg->codec_param.a2dp.codec_type;
        if (codec_type == BTS_A2DP_TYPE_SBC)
        {
            send_len = smf_media_kfifo_data_pull(send_buf, data_len);
        }
        else
        {
            send_len = smf_media_kfifo_data_pull(send_buf + SMF_MEDIA_AUDIO_BT_A2DP_DATA_HDRSIZE, data_len);
        }
        if (send_len != data_len)
        {
            MEDIA_ERR("read fail! %ld, %ld \n", send_len, data_len);
            smf_media_audio_bt_send_buf_free(send_buf);
            return;
        }

        if (codec_type == BTS_A2DP_TYPE_SBC)
        {
            smf_media_audio_bt_send_buf_len_update(send_buf, media_audio.total_len);
        }
        else
        {
            STREAM_SET_DATA_HEADER(send_buf);
            STREAM_SET_DATA_LEN(send_buf, media_audio.total_len);
            smf_media_audio_bt_send_buf_len_update(send_buf, media_audio.total_len + SMF_MEDIA_AUDIO_BT_A2DP_DATA_HDRSIZE);
        }

        smf_media_audio_bt_send_buf(send_buf);
    }


}


static void smf_media_audio_bt_a2dp_src_data_event(uv_stream_t* stream_hdl, ssize_t nread, const uv_buf_t* buf)
{
    //TO DO
    smf_media_audio_bt_read_free((uv_handle_t *)stream_hdl, buf);
}

static void smf_media_audio_bt_a2dp_src_ctrl_event(uv_stream_t* stream_hdl, ssize_t nread, const uv_buf_t* buf)
{
    uint8_t event_type;
    uint8_t* data = (uint8_t*)buf->base;
    smf_media_audio_bt_pipe_t* bt_pipe = NULL;
    smf_media_audio_bt_env_t* smf_bt_env = &smf_media_audio_bt_env;

    bt_pipe = CONTAINER_OF(stream_hdl, smf_media_audio_bt_pipe_t, hdl);

    while (data < (uint8_t *)(buf->base + nread))
    {
        // data format
        // ctrl_stream: |           byte0            |                     byte n                        |
        //              | event_type@A2DP_CTRL_EVT_T | event param(only BT_AUDIO_CTRL_EVT_UPDATE_CONFIG) |
        STREAM_TO_UINT8(event_type, data);

        MEDIA_INFO("pipe=%s, evt=%d\n", bt_pipe->name, event_type);

        switch (event_type)
        {
            case BT_AUDIO_CTRL_EVT_STARTED:
            {
            } break;
            case BT_AUDIO_CTRL_EVT_START_FAIL:
            {
            } break;
            case BT_AUDIO_CTRL_EVT_STOPPED:
            {
            } break;
            case BT_AUDIO_CTRL_EVT_UPDATE_CONFIG:
            {
                data += smf_media_audio_bt_a2dp_codec_update(&bt_pipe->codec_cfg, data);
                smf_media_audio_bt_send_cmd(bt_pipe, A2DP_CTRL_CMD_CONFIG_DONE);

                smf_media_audio_bt_creat_pipe(&smf_bt_env->pipe[SMF_BT_PIPE_A2DP_SRC_DATA],
                    CONFIG_BLUETOOTH_A2DP_SOURCE_DATA_PATH, smf_media_audio_bt_a2dp_src_data_event);
            } break;
            default:
        }
    }
    smf_media_audio_bt_read_free((uv_handle_t *)stream_hdl, buf);
}

static void smf_media_audio_bt_a2dp_sink_data_event(uv_stream_t* stream_hdl, ssize_t nread, const uv_buf_t* buf)
{
    uint8_t* pkt_pdu    = NULL;
    uint16_t pkt_header = 0;
    uint16_t pdu_len    = 0;
    uint16_t sdu_offset = 0;
    smf_media_audio_bt_pipe_t* bt_pipe = NULL;

    bt_pipe = CONTAINER_OF(stream_hdl, smf_media_audio_bt_pipe_t, hdl);

    //MEDIA_INFO("data nb=%d, len=%d, data=%x,%x,%x,%x \n",
    //    nread, buf->len, buf->base[0], buf->base[1], buf->base[2], buf->base[3]);

    // Splicing data packets
    if (bt_pipe->pdu_cache.base)
    {
        if(bt_pipe->pdu_cache.len < SMF_MEDIA_AUDIO_BT_A2DP_DATA_HDRSIZE)
        {
            sdu_offset = SMF_MEDIA_AUDIO_BT_A2DP_DATA_HDRSIZE - bt_pipe->pdu_cache.len;
            if(nread < sdu_offset)
            {
                memcpy(bt_pipe->pdu_cache.base + bt_pipe->pdu_cache.len, buf->base, nread);
                bt_pipe->pdu_cache.len += nread;
                smf_media_audio_bt_read_free((uv_handle_t *)stream_hdl, buf);
                return;
            }
            else
            {
                memcpy(bt_pipe->pdu_cache.base + bt_pipe->pdu_cache.len, buf->base, sdu_offset);
                bt_pipe->pdu_cache.len += sdu_offset;
            }
        }

        sdu_offset = STREAM_GET_DATA_LEN(bt_pipe->pdu_cache.base) -
            (bt_pipe->pdu_cache.len - SMF_MEDIA_AUDIO_BT_A2DP_DATA_HDRSIZE);
        if (sdu_offset > nread)
        {
            memcpy(bt_pipe->pdu_cache.base + bt_pipe->pdu_cache.len, buf->base, nread);
            bt_pipe->pdu_cache.len += nread;
            smf_media_audio_bt_read_free((uv_handle_t *)stream_hdl, buf);
            return;
        }
        else
        {
            memcpy(bt_pipe->pdu_cache.base + bt_pipe->pdu_cache.len, buf->base, sdu_offset);
            bt_pipe->pdu_cache.len += sdu_offset;
        }
    }

    // Play the spliced data packet
    if(bt_pipe->pdu_cache.base)
    {
        pkt_pdu = (uint8_t *)bt_pipe->pdu_cache.base;
        pkt_header = STREAM_GET_DATA_HEADER(pkt_pdu);
        pdu_len    = STREAM_GET_DATA_LEN(pkt_pdu) + SMF_MEDIA_AUDIO_BT_A2DP_DATA_HDRSIZE;

        if ((pkt_header == SMF_MEDIA_AUDIO_BT_A2DP_DATA_HEAD) &&
            (bt_pipe->pdu_cache.len == pdu_len))
        {
            if (bt_pipe->media_id)
            {
                //MEDIA_INFO("data len=%d, data=%x,%x,%x,%x \n",
                //    pdu_len, pkt_pdu[0], pkt_pdu[1], pkt_pdu[2], pkt_pdu[3]);
                smf_media_kfifo_data_push(pkt_pdu + SMF_MEDIA_AUDIO_BT_A2DP_DATA_HDRSIZE,
                    pdu_len - SMF_MEDIA_AUDIO_BT_A2DP_DATA_HDRSIZE);
            }
            else
            {
                MEDIA_WARN("media not ready! \n");
            }
        }
        else
        {
            MEDIA_ERR("error pkt drop, 0x%x, %d, %d\n", pkt_header,
                pdu_len, bt_pipe->pdu_cache.len);
        }

        smf_media_audio_bt_read_free((uv_handle_t *)stream_hdl, &bt_pipe->pdu_cache);
        bt_pipe->pdu_cache.base = 0;
        bt_pipe->pdu_cache.len  = 0;
    }

    // Play the complete PDU from the SDUs
    while(sdu_offset < nread)
    {
        pkt_pdu = (uint8_t *)buf->base + sdu_offset;
        pkt_header = STREAM_GET_DATA_HEADER(pkt_pdu);
        pdu_len    = STREAM_GET_DATA_LEN(pkt_pdu) + SMF_MEDIA_AUDIO_BT_A2DP_DATA_HDRSIZE;

        if((nread - sdu_offset) < pdu_len)
        {
            break;
        }

        if (pkt_header == SMF_MEDIA_AUDIO_BT_A2DP_DATA_HEAD)
        {
            if (bt_pipe->media_id)
            {
                //MEDIA_INFO("data len=%d, data=%x,%x,%x,%x \n",
                //    pdu_len, pkt_pdu[0], pkt_pdu[1], pkt_pdu[2], pkt_pdu[3]);
                smf_media_kfifo_data_push(pkt_pdu + SMF_MEDIA_AUDIO_BT_A2DP_DATA_HDRSIZE,
                    pdu_len - SMF_MEDIA_AUDIO_BT_A2DP_DATA_HDRSIZE);
            }
            else
            {
                MEDIA_WARN("media not ready! \n");
            }
        }
        else
        {
            MEDIA_ERR("error pkt drop, header=%x \n", pkt_header);
        }
        sdu_offset += pdu_len;
    }

    // Cache incomplete PDU data
    if (sdu_offset == nread)
    {
        smf_media_audio_bt_read_free((uv_handle_t *)stream_hdl, buf);
    }
    else if (!bt_pipe->pdu_cache.base)
    {
        bt_pipe->pdu_cache = *buf;
        bt_pipe->pdu_cache.len = nread - sdu_offset;
        memcpy(bt_pipe->pdu_cache.base,
               bt_pipe->pdu_cache.base + sdu_offset,
               bt_pipe->pdu_cache.len);
    }
}

static void smf_media_audio_bt_a2dp_sink_ctrl_event(uv_stream_t* stream_hdl, ssize_t nread, const uv_buf_t* buf)
{
    uint8_t event_type;
    uint8_t* data = (uint8_t*)buf->base;
    smf_media_audio_bt_pipe_t* bt_pipe = NULL;
    smf_media_audio_bt_env_t* smf_bt_env = &smf_media_audio_bt_env;

    bt_pipe = CONTAINER_OF(stream_hdl, smf_media_audio_bt_pipe_t, hdl);
    while (data < (uint8_t *)(buf->base + nread))
    {
        // data format
        // ctrl_stream: |           byte0            |                     byte n                        |
        //              | event_type@A2DP_CTRL_EVT_T | event param(only BT_AUDIO_CTRL_EVT_UPDATE_CONFIG) |
        STREAM_TO_UINT8(event_type, data);

        MEDIA_INFO("pipe=%s, read_len=%d, evt=%d\n", bt_pipe->name, nread, event_type);

        switch (event_type)
        {
            case BT_AUDIO_CTRL_EVT_STARTED:
            {
                if (bt_pipe->media_id)
                {
                    break;
                }
                if (bt_pipe->started_ignore)
                {
                    bt_pipe->started_ignore--;
                    break;
                }

                smf_media_audio_output_config();
                if (bt_pipe->codec_cfg->codec_param.a2dp.codec_type == BTS_A2DP_TYPE_SBC)
                {
                    bt_pipe->media_id =
                    smf_media_audio_player_a2dp_start("sbc", SMF_VOLUME_MAX);
                }
                else if (bt_pipe->codec_cfg->codec_param.a2dp.codec_type == BTS_A2DP_TYPE_MPEG2_4_AAC)
                {
                    bt_pipe->media_id =
                    smf_media_audio_player_a2dp_start("aac", SMF_VOLUME_MAX);
                }
                else
                {
                    MEDIA_ERR("unkonw code_type=%ld\n", bt_pipe->codec_cfg->codec_param.a2dp.codec_type);
                }
                MEDIA_INFO("media_id=%llu, codec_type=%ld\n", bt_pipe->media_id, bt_pipe->codec_cfg->codec_param.a2dp.codec_type);
                smf_bt_env->pipe[SMF_BT_PIPE_A2DP_SINK_DATA].media_id = bt_pipe->media_id;
            } break;
            case BT_AUDIO_CTRL_EVT_START_FAIL:
            {
                smf_media_audio_player_a2dp_stop(bt_pipe->media_id);
                bt_pipe->media_id = 0;
                smf_bt_env->pipe[SMF_BT_PIPE_A2DP_SINK_DATA].media_id = 0;
            } break;
            case BT_AUDIO_CTRL_EVT_STOPPED:
            {
                smf_media_audio_player_a2dp_stop(bt_pipe->media_id);
                bt_pipe->media_id = 0;
                smf_bt_env->pipe[SMF_BT_PIPE_A2DP_SINK_DATA].media_id = 0;
            } break;
            case BT_AUDIO_CTRL_EVT_UPDATE_CONFIG:
            {
                bt_pipe->started_ignore = 2;
                data += smf_media_audio_bt_a2dp_codec_update(&bt_pipe->codec_cfg, data);
                smf_media_audio_bt_send_cmd(bt_pipe, A2DP_CTRL_CMD_CONFIG_DONE);

                smf_media_audio_bt_creat_pipe(&smf_bt_env->pipe[SMF_BT_PIPE_A2DP_SINK_DATA],
                    CONFIG_BLUETOOTH_A2DP_SINK_DATA_PATH, smf_media_audio_bt_a2dp_sink_data_event);

                smf_media_audio_bt_send_cmd(bt_pipe, A2DP_CTRL_CMD_START);
            } break;
            default:
                MEDIA_WARN("unkonw event= %d \n", event_type);
        }
    }

    smf_media_audio_bt_read_free((uv_handle_t *)stream_hdl, buf);
}

static void smf_media_audio_bt_sco_ctrl_event(uv_stream_t* stream_hdl, ssize_t nread, const uv_buf_t* buf)
{
    uint8_t event_type;
    uint8_t* data = (uint8_t*)buf->base;
    smf_media_audio_bt_pipe_t* bt_pipe = NULL;

    bt_pipe = CONTAINER_OF(stream_hdl, smf_media_audio_bt_pipe_t, hdl);

    while (data < (uint8_t *)(buf->base + nread))
    {
        // data format
        // ctrl_stream: |           byte0            |                     byte n                        |
        //              | event_type@A2DP_CTRL_EVT_T | event param(only BT_AUDIO_CTRL_EVT_UPDATE_CONFIG) |
        STREAM_TO_UINT8(event_type, data);

        MEDIA_INFO("pipe=%s, evt=%d\n", bt_pipe->name, event_type);

        switch (event_type)
        {
            case BT_AUDIO_CTRL_EVT_STARTED:
            {
                if (bt_pipe->codec_cfg->codec_param.sco.sample_rate == 8000)
                {
                    bt_pipe->media_id = smf_media_audio_btsco_start(1, SMF_VOLUME_MAX);
                }
                else if (bt_pipe->codec_cfg->codec_param.sco.sample_rate == 16000)
                {
                    bt_pipe->media_id = smf_media_audio_btsco_start(2, SMF_VOLUME_MAX);
                }
            } break;
            case BT_AUDIO_CTRL_EVT_START_FAIL:
            {
                smf_media_audio_btsco_stop(bt_pipe->media_id);
            } break;
            case BT_AUDIO_CTRL_EVT_STOPPED:
            {
                smf_media_audio_btsco_stop(bt_pipe->media_id);
            } break;
            case BT_AUDIO_CTRL_EVT_UPDATE_CONFIG:
            {

            } break;
            default:
                MEDIA_WARN("unkonw event= %d \n", event_type);
        }
    }
    smf_media_audio_bt_read_free((uv_handle_t *)stream_hdl, buf);
}

static void* smf_media_audio_bt_thread(void* arg)
{
    smf_media_audio_bt_env_t* smf_bt_env = (smf_media_audio_bt_env_t*)arg;

    uv_run(smf_bt_env->uv_loop, UV_RUN_DEFAULT);

    return NULL;
}

int smf_media_audio_bt_open(SMF_MEDIA_AUDIO_BT_PATH_TYPE path_type)
{
    int ret;
    struct sched_param param;
    smf_media_audio_bt_env_t* smf_bt_env = &smf_media_audio_bt_env;

    if ((path_type == SMF_MEDIA_AUDIO_BT_UNKONW) || (path_type >= SMF_MEDIA_AUDIO_BT_MAX))
    {
        MEDIA_ERR("%d \n", path_type);
        return -EINVAL;
    }

    if (smf_bt_env->path_map & path_type)
    {
        MEDIA_INFO("0x%x, 0x%x \n", smf_bt_env->path_map, path_type);
        return 0;
    }

    if (smf_bt_env->path_map == SMF_MEDIA_AUDIO_BT_UNKONW)
    {
        smf_bt_env->uv_loop = uv_default_loop();
        uv_timer_init(smf_bt_env->uv_loop, &smf_bt_env->uv_timer);

        pthread_attr_init(&smf_bt_env->thread_attr);
        pthread_attr_setstacksize(&smf_bt_env->thread_attr, SMF_MEDIA_AUDIO_BT_THREAD_STACK_SIZE);
        ret = pthread_create(&smf_bt_env->thread_id,
            &smf_bt_env->thread_attr, smf_media_audio_bt_thread, smf_bt_env);
        pthread_setname_np(smf_bt_env->thread_id, SMF_MEDIA_AUDIO_BT_THREAD_STACK_NAME);

        pthread_attr_getschedparam(&smf_bt_env->thread_attr, &param);
        param.sched_priority = SMF_MEDIA_AUDIO_BT_THREAD_STACK_PRIORITY;
        pthread_attr_setschedparam(&smf_bt_env->thread_attr, &param);

        if (ret < 0)
        {
            return -EINVAL;
        }
    }

    if (path_type == SMF_MEDIA_AUDIO_BT_A2DP)
    {
        //Creating a pipeline will fail when if(pathotype==SMF_MEDIA_SAUDIO_SCO)
        smf_media_audio_bt_creat_pipe(&smf_bt_env->pipe[SMF_BT_PIPE_SCO_CTRL],
            CONFIG_BLUETOOTH_SCO_CTRL_PATH, smf_media_audio_bt_sco_ctrl_event);

        smf_media_audio_bt_creat_pipe(&smf_bt_env->pipe[SMF_BT_PIPE_A2DP_SRC_CTRL],
            CONFIG_BLUETOOTH_A2DP_SOURCE_CTRL_PATH, smf_media_audio_bt_a2dp_src_ctrl_event);
        smf_media_audio_bt_creat_pipe(&smf_bt_env->pipe[SMF_BT_PIPE_A2DP_SINK_CTRL],
            CONFIG_BLUETOOTH_A2DP_SINK_CTRL_PATH, smf_media_audio_bt_a2dp_sink_ctrl_event);
    }
    else if (path_type == SMF_MEDIA_AUDIO_BT_LEA)
    {
        //TO DO
    }
    else if (path_type == SMF_MEDIA_AUDIO_BT_SCO)
    {
        smf_media_audio_bt_pipe_t* sco_pipe = &smf_bt_env->pipe[SMF_BT_PIPE_SCO_CTRL];
        if (sco_pipe->codec_cfg->codec_param.sco.sample_rate == 8000)
        {
            sco_pipe->media_id = smf_media_audio_btsco_start(1, SMF_VOLUME_MAX);
        }
        else if (sco_pipe->codec_cfg->codec_param.sco.sample_rate == 16000)
        {
            sco_pipe->media_id = smf_media_audio_btsco_start(2, SMF_VOLUME_MAX);
        }
    }

    smf_bt_env->path_map |= path_type;

    return 0;
}

int smf_media_audio_bt_close(SMF_MEDIA_AUDIO_BT_PATH_TYPE path_type)
{
    int ret;
    smf_media_audio_bt_env_t* smf_bt_env = &smf_media_audio_bt_env;

    if (!(smf_bt_env->path_map &= path_type))
    {
        MEDIA_ERR("%d \n", smf_bt_env->path_map);
        return -EINVAL;
    }

    if (path_type == SMF_MEDIA_AUDIO_BT_A2DP)
    {
        smf_media_audio_bt_delete_pipe(&smf_bt_env->pipe[SMF_BT_PIPE_A2DP_SRC_CTRL]);
        smf_media_audio_bt_delete_pipe(&smf_bt_env->pipe[SMF_BT_PIPE_A2DP_SINK_CTRL]);

        //Delete a pipeline will fail when if(pathotype==SMF_MEDIA_SAUDIO_SCO)
        smf_media_audio_bt_delete_pipe(&smf_bt_env->pipe[SMF_BT_PIPE_SCO_CTRL]);
    }
    else if (path_type == SMF_MEDIA_AUDIO_BT_LEA)
    {
        //TO DO
    }
    else if (path_type == SMF_MEDIA_AUDIO_BT_SCO)
    {
        smf_media_audio_bt_pipe_t* sco_pipe = &smf_bt_env->pipe[SMF_BT_PIPE_SCO_CTRL];
        smf_media_audio_btsco_stop(sco_pipe->media_id);
    }

    smf_bt_env->path_map &= (~path_type);
    if (smf_bt_env->path_map == SMF_MEDIA_AUDIO_BT_UNKONW)
    {
        uv_stop(smf_bt_env->uv_loop);

        ret = pthread_join(smf_bt_env->thread_id, NULL);
        if (ret != 0)
        {
            MEDIA_ERR("Failed to join thread \n");
            return -EINVAL;
        }

        memset(smf_bt_env, 0, sizeof(smf_media_audio_bt_env_t));
    }

    return 0;
}

int smf_media_audio_bt_ctrl_send(SMF_MEDIA_AUDIO_BT_CTRL_TYPE ctrl_type)
{
    bool ret = 0;
    smf_media_audio_bt_env_t* smf_bt_env = &smf_media_audio_bt_env;

    if (ctrl_type == SMF_MEDIA_AUDIO_BT_CTRL_START)
    {
        if (smf_bt_env->pipe[SMF_BT_PIPE_A2DP_SRC_CTRL].codec_cfg)
        {
            uv_timer_start(&smf_bt_env->uv_timer, smf_media_audio_bt_a2dp_src_send_data,
                SMF_MEDIA_AUDIO_BT_A2DP_SRC_SEND_INTERVAL, SMF_MEDIA_AUDIO_BT_A2DP_SRC_SEND_REPEAT);
            ret = smf_media_audio_bt_send_cmd(&smf_bt_env->pipe[SMF_BT_PIPE_A2DP_SRC_CTRL], A2DP_CTRL_CMD_START);
        }
        else if (smf_bt_env->pipe[SMF_BT_PIPE_LEA_SRC_CTRL].codec_cfg)
        {
            //TO DO
        }
    }
    else if (ctrl_type == SMF_MEDIA_AUDIO_BT_CTRL_STOP)
    {
        uv_timer_stop(&smf_bt_env->uv_timer);
        if (smf_bt_env->pipe[SMF_BT_PIPE_A2DP_SRC_CTRL].codec_cfg)
        {
            ret = smf_media_audio_bt_send_cmd(&smf_bt_env->pipe[SMF_BT_PIPE_A2DP_SRC_CTRL], A2DP_CTRL_CMD_STOP);
        }
        else if (smf_bt_env->pipe[SMF_BT_PIPE_LEA_SRC_CTRL].codec_cfg)
        {
            //TO DO
        }
    }
    else
    {
        MEDIA_ERR("Unkonw strl_type=%d \n", ctrl_type);
        return -EINVAL;
    }

    if (!ret)
    {
        return -EINVAL;
    }

    return 0;
}

const smf_media_audio_bt_codec_cfg_t* smf_media_audio_bt_get_codec_info(void)
{
    return smf_media_audio_bt_env.pipe[SMF_BT_PIPE_A2DP_SRC_CTRL].codec_cfg;
}

void smf_media_audio_bt_set_sco_param(int sample_rate)
{
    if(!smf_media_audio_bt_env.pipe[SMF_BT_PIPE_SCO_CTRL].codec_cfg)
    {
        smf_media_audio_bt_env.pipe[SMF_BT_PIPE_SCO_CTRL].codec_cfg =
            (smf_media_audio_bt_codec_cfg_t *)malloc(sizeof(smf_media_audio_bt_codec_cfg_t));
        smf_media_audio_bt_env.pipe[SMF_BT_PIPE_SCO_CTRL].codec_cfg->type = SMF_MEDIA_AUDIO_BT_SCO;
    }

    smf_media_audio_bt_env.pipe[SMF_BT_PIPE_SCO_CTRL].codec_cfg->codec_param.sco.sample_rate = sample_rate;
}

