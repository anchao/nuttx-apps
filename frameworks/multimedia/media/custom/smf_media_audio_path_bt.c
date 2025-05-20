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
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include <nuttx/config.h>

#include "media_common.h"
#include "smf_media_graph.api.h"
#include "uv/unix.h"
#include "smf_media_audio_path_bt.h"

/***************************** external declaration ***************************/

/***************************** macro defination *******************************/
#define SMF_MEDIA_AUDIO_BT_THREAD_STACK_NAME        "smf_media_audio_bt"
#define SMF_MEDIA_AUDIO_BT_THREAD_STACK_PRIORITY    99
#define SMF_MEDIA_AUDIO_BT_THREAD_STACK_SIZE        6 * 1024
#define SMF_MEDIA_AUDIO_BT_A2DP_CTRL_CFG_LEN        29
#define SMF_MEDIA_AUDIO_BT_A2DP_SRC_SEND_INTERVAL   10
#define SMF_MEDIA_AUDIO_BT_A2DP_SRC_SEND_REPEAT     20

// sync@LOAS_HDRSIZE
#define SMF_MEDIA_AUDIO_BT_A2DP_DATA_HDRSIZE   3
#define SMF_MEDIA_AUDIO_BT_A2DP_DATA_HEAD      0x56E0
#define SMF_MEDIA_AUDIO_BT_A2DP_DATA_HEAD_MASK 0xFFE0
#define SMF_MEDIA_AUDIO_BT_A2DP_DATA_LEN_MASK  0x1FFF

#define SMF_MEDIA_HFP_ROLE_AG   0
#define SMF_MEDIA_HFP_ROLE_HF   1
#define SMF_MEDIA_SCO_CODEC_TYPE_CVSD 1
#define SMF_MEDIA_SCO_CODEC_TYPE_MSBC 2
#define SMF_MEDIA_SCO_CODEC_SAMPLE_RATE_8000    8000
#define SMF_MEDIA_SCO_CODEC_SAMPLE_RATE_16000   16000

#define SMF_MEDIA_BT_TASK_CLOSE         1
#define SMF_MEDIA_BT_TASK_SEND          2
#define SMF_MEDIA_BT_TASK_CREATE_PIPE   3

#define SMF_MEDIA_BT_BUF_POOL_SIZE      20
#define SMF_MEDIA_BT_BUF_SIZE           1024 + 256

#define STREAM_SET_DATA_HEADER(p)   {p[0] = (SMF_MEDIA_AUDIO_BT_A2DP_DATA_HEAD >> 8); p[1] = SMF_MEDIA_AUDIO_BT_A2DP_DATA_HEAD & 0x00FF;}
#define STREAM_SET_DATA_LEN(p, len) {p[1] |= ((len & SMF_MEDIA_AUDIO_BT_A2DP_DATA_LEN_MASK) >> 8); p[2] = len & 0x00FF;}
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

typedef void (*pipe_read_cb)(void* pipe, ssize_t nread, const uv_buf_t* buf);

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
    uint8_t     head[3];
    uv_buf_t    data;
} audio_packet_t;

typedef struct
{
    uv_write_t req;
    uv_buf_t   buf;
} smf_media_audio_bt_write_pkt_t;

typedef struct
{
    uint8_t         started_ignore;
    uint64_t        media_id;
    smf_media_audio_bt_codec_cfg_t* codec_cfg;
    uv_connect_t    conn;
    uv_pipe_t       hdl;
    pipe_read_cb    read_cb;
    char*           name;
    audio_packet_t* packet;
    bool            connected;
    smf_media_audio_bt_write_pkt_t write_req;
} smf_media_audio_bt_pipe_t;

typedef struct
{
    uv_async_t     async;
    uv_sem_t       done;
    uint8_t        type;
    union {
        SMF_MEDIA_AUDIO_BT_CTRL_TYPE ctrl_type;
        SMF_MEDIA_AUDIO_BT_PATH_TYPE path_type;
        int32_t                      trans_id;
    };
    uv_mutex_t     mutex;
    int            res;
} bt_asycn_task_t;

typedef struct
{
    bool in_use;
    uint8_t padding[3];
    uint8_t buf[SMF_MEDIA_BT_BUF_SIZE];
} __attribute__((aligned(4))) smf_media_audio_bt_buf_t;

typedef struct
{
    SMF_MEDIA_AUDIO_BT_PATH_TYPE    path_map;
    pthread_t                       thread_id;
    /// Pipe
    uv_loop_t*                      uv_loop;
    bt_asycn_task_t                 task;
    uv_timer_t                      uv_timer;
    uv_sem_t                        ready;
    uint16_t                        pool_size;
    smf_media_audio_bt_buf_t        pool[SMF_MEDIA_BT_BUF_POOL_SIZE];
} smf_media_audio_bt_env_t;

/*****************************  variable defination *****************************/
static smf_media_audio_bt_env_t g_smf_media_bt = {0};

/*****************************  function declaration ****************************/
static smf_media_audio_bt_pipe_t* get_pipe(int trans_id);

static void* get_buf(uint16_t len)
{
    smf_media_audio_bt_env_t* smf_bt = &g_smf_media_bt;
    smf_media_audio_bt_buf_t* pool_buf = NULL;
    if (len > SMF_MEDIA_BT_BUF_SIZE)
    {
        MEDIA_ERR("malloc len %u is over SMF_MEDIA_BT_BUF_SIZE", len);
        return NULL;
    }

    for(uint16_t i = 0; i < SMF_MEDIA_BT_BUF_POOL_SIZE; i++)
    {
        pool_buf = &smf_bt->pool[i];
        if(!pool_buf->in_use)
        {
            smf_bt->pool_size--;
            pool_buf->in_use = true;
            // MEDIA_INFO("pool size %d p = %p", smf_bt->pool_size, pool_buf->buf);
            return pool_buf->buf;
        }
    }
    MEDIA_ERR("get buf failed no buffer");
    return NULL;
}

static void free_buf(void *buf)
{
    smf_media_audio_bt_env_t* smf_bt = &g_smf_media_bt;
    smf_media_audio_bt_buf_t* pool_buf = NULL;

    if (buf == NULL)
    {
        return;
    }

    for(uint16_t i = 0; i < SMF_MEDIA_BT_BUF_POOL_SIZE; i++)
    {
        pool_buf = &smf_bt->pool[i];
        if(pool_buf->in_use && pool_buf->buf == buf)
        {
            // MEDIA_INFO("pool size %d p = %p", smf_bt->pool_size, pool_buf->buf);
            smf_bt->pool_size++;
            pool_buf->in_use = false;
            return;
        }
    }
    MEDIA_ERR("free buf error %p", buf);
}
static void smf_media_audio_bt_read_callback(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    smf_media_audio_bt_pipe_t* pipe = NULL;

    pipe = CONTAINER_OF(stream, smf_media_audio_bt_pipe_t, hdl);
    if (pipe->read_cb)
    {
        pipe->read_cb(pipe, nread, buf);
    }
}

static void smf_media_audio_bt_malloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    smf_media_audio_bt_pipe_t* pipe = NULL;
    uv_buf_t *pkt_buf = NULL;

    pipe = CONTAINER_OF(handle, smf_media_audio_bt_pipe_t, hdl);

    if(pipe->packet)
    {
        pkt_buf = &pipe->packet->data;
        if (pkt_buf->len)
        {
            pkt_buf->base = (char*)get_buf(pkt_buf->len);
            buf->base = pkt_buf->base;
            buf->len  = pkt_buf->len;
        }
        else
        {
            buf->base = (char*)pipe->packet->head;
            buf->len = 3;
        }
    }
    else
    {
        buf->base = (char*)get_buf(SMF_MEDIA_BT_BUF_SIZE);
        buf->len = SMF_MEDIA_BT_BUF_SIZE;
    }
    // MEDIA_INFO("buf len %d p = %p\n", buf->len, buf->base);
}

static void smf_media_audio_bt_free(uv_handle_t *handle, const uv_buf_t *buf)
{
    if ((!buf) || (!buf->base))
    {
        return;
    }

    free_buf((uint8_t*)buf->base);
}

static void smf_media_audio_bt_write_done(uv_write_t* req, int status)
{
    smf_media_audio_bt_write_pkt_t *write_pkt = NULL;

    if(status)
    {
        MEDIA_INFO("Write error, %p, %d \n", req, status);
    }

    write_pkt = CONTAINER_OF(req, smf_media_audio_bt_write_pkt_t, req);
    free_buf(write_pkt);
}

static void* smf_media_audio_bt_send_buf_new(uint32_t data_len)
{
    uint8_t *buf = NULL;
    smf_media_audio_bt_write_pkt_t *write_pkt = get_buf(sizeof(smf_media_audio_bt_write_pkt_t) + data_len);
    if (write_pkt == NULL)
    {
        return NULL;
    }

    buf = (uint8_t*)(write_pkt + 1);
    write_pkt->buf = uv_buf_init((char *)buf, data_len);
    return buf;
}

static void smf_media_audio_bt_send_buf_free(uint8_t *buf)
{
    smf_media_audio_bt_write_pkt_t *write_pkt = NULL;
    if(!buf)
    {
        MEDIA_WARN("buf = %p! \n", buf);
        return;
    }

    write_pkt = (smf_media_audio_bt_write_pkt_t*)(buf - sizeof(smf_media_audio_bt_write_pkt_t));

    free_buf(write_pkt);
}

static bool smf_media_audio_bt_send_buf(smf_media_audio_bt_pipe_t* pipe, uint8_t *buf)
{
    int ret = 0;
    smf_media_audio_bt_write_pkt_t *write_pkt = NULL;

    if (!buf)
    {
        MEDIA_WARN("buf = %p! \n", buf);
        return false;
    }

    write_pkt = (smf_media_audio_bt_write_pkt_t*)(buf - sizeof(smf_media_audio_bt_write_pkt_t));
    ret = uv_write((uv_write_t*)write_pkt, (uv_stream_t*)&pipe->hdl,
            &write_pkt->buf, 1, smf_media_audio_bt_write_done);
    if (ret)
    {
        free_buf(write_pkt);
    }

    return ret ? false : true;
}

static bool smf_media_audio_bt_send_cmd(smf_media_audio_bt_pipe_t* pipe, uint8_t cmd)
{
    uint8_t* buf = NULL;

    buf = smf_media_audio_bt_send_buf_new(sizeof(cmd));
    if (!buf)
    {
        MEDIA_WARN("new buffer fail! \n");
        return false;
    }

    *buf = cmd;
    return smf_media_audio_bt_send_buf(pipe, buf);
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
                  smf_media_audio_bt_malloc, smf_media_audio_bt_read_callback);
    if (ret)
    {
        MEDIA_WARN("pipe fail! %d \n", ret);
    }
    bt_pipe->connected = true;
}

static void smf_media_audio_bt_disconn_done(uv_handle_t* handle)
{
    smf_media_audio_bt_pipe_t* pipe = NULL;
    pipe = CONTAINER_OF(handle, smf_media_audio_bt_pipe_t, hdl);
    // memset(bt_pipe, 0, sizeof(smf_media_audio_bt_pipe_t));
    pipe->connected = false;
    if(pipe->codec_cfg)
    {
        free(pipe->codec_cfg);
    }

    if (pipe->packet)
    {
        if (pipe->packet->data.base)
        {
            free_buf(pipe->packet->data.base);
        }
        free(pipe->packet);
    }

    MEDIA_INFO("%s-%p disconnect done \n", pipe->name, pipe);

}

static smf_media_audio_bt_pipe_t* smf_media_audio_bt_create_pipe_internal(int trans_id)
{
    smf_media_audio_bt_env_t* smf_bt = &g_smf_media_bt;
    smf_media_audio_bt_pipe_t* pipe = get_pipe(trans_id);
    int ret;

    if (pipe->connected)
    {
         MEDIA_INFO("already connected pipe=%s \n", pipe->name);
         return NULL;
    }

    ret = uv_pipe_init(smf_bt->uv_loop, &pipe->hdl, 1);
    if (ret)
    {
        MEDIA_WARN("create pipe fail! %d \n", ret);
        return NULL;
    }

    uv_pipe_connect(&pipe->conn, &pipe->hdl,
                    pipe->name, smf_media_audio_bt_conn_done);

    return pipe;
}

static smf_media_audio_bt_pipe_t* smf_media_audio_bt_create_pipe(int trans_id)
{
    smf_media_audio_bt_env_t* smf_bt = &g_smf_media_bt;
    bt_asycn_task_t* task = &smf_bt->task;

    if (smf_bt->uv_loop == NULL)
    {
        MEDIA_ERR(" not init");
        return 0;
    }
    uv_mutex_lock(&task->mutex);

    task->type      = SMF_MEDIA_BT_TASK_CREATE_PIPE;
    task->trans_id  = trans_id;
    uv_async_send(&task->async);
    uv_sem_wait(&task->done);

    uv_mutex_unlock(&task->mutex);
    return (smf_media_audio_bt_pipe_t*)task->res;
}

static bool smf_media_audio_bt_delete_pipe(smf_media_audio_bt_pipe_t *pipe)
{
    if (!pipe)
    {
        MEDIA_ERR("pipe is NULL");
        return false;
    }

    if (pipe && !pipe->connected)
    {
        MEDIA_ERR("pipe %s not connected!", pipe->name);
        return false;
    }

    MEDIA_INFO("remove pipe %s", pipe->name);

    uv_read_stop((uv_stream_t*)&pipe->hdl);
    uv_close((uv_handle_t*)&pipe->hdl, smf_media_audio_bt_disconn_done);
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
    uint32_t fifo_size = 0;
    uint32_t pull_len = 0;
    uint32_t offset = 0;
    uint32_t header_len = OFFSETOF(smf_media_a2dp_source_send_info_t, frame_buffer);
    uint32_t audio_packet_len = sizeof(smf_media_a2dp_source_send_info_t);
    uint32_t packet_num = 0;
    uint32_t valid_len = 0;
    uint32_t audio_data_len = 0;

    smf_media_audio_bt_env_t* smf_bt = &g_smf_media_bt;
    smf_media_audio_bt_pipe_t* pipe = get_pipe(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SOURCE_CTRL);
    smf_media_audio_bt_pipe_t* data_pipe = get_pipe(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SOURCE_AUDIO);
    smf_media_a2dp_source_header_t audio_hdr;
    A2DP_CODE_TYPE_T  codec_type = pipe->codec_cfg->codec_param.a2dp.codec_type;;

    //read audio data header

    fifo_size = smf_media_kfifo_data_size();
    if (fifo_size < audio_packet_len)
    {
        MEDIA_WARN("fifo size %d", fifo_size);
        return;
    }
    packet_num = fifo_size / audio_packet_len;
    packet_num = MIN(packet_num, smf_bt->pool_size);

    for (uint32_t i = 0; i < packet_num; i++)
    {
        pull_len = smf_media_kfifo_data_pull((uint8_t *)&audio_hdr, header_len);
        if (pull_len != header_len)
        {
            MEDIA_ERR("read audio header failed! %u, %u \n", pull_len, header_len);
            return;
        }
        if (audio_hdr.total_len != (audio_hdr.frame_num * audio_hdr.frame_len))
        {
            MEDIA_WARN("data header error! taotal len %u, frame num %u, free len %u \n",
                        audio_hdr.total_len, audio_hdr.frame_num, audio_hdr.frame_len);
            return;
        }
        valid_len = audio_hdr.total_len;
        audio_data_len = audio_packet_len - header_len;
        // MEDIA_WARN("taotal len %u, frame num %u, free len %u \n",
        //             audio_hdr.total_len , audio_hdr.frame_num, audio_hdr.frame_len);

        if (codec_type != BTS_A2DP_TYPE_SBC)
        {
            STREAM_SET_DATA_HEADER(send_buf);
            STREAM_SET_DATA_LEN(send_buf, valid_len);
            offset += SMF_MEDIA_AUDIO_BT_A2DP_DATA_HDRSIZE;
            audio_data_len += SMF_MEDIA_AUDIO_BT_A2DP_DATA_HDRSIZE;
        }
        send_buf = smf_media_audio_bt_send_buf_new(valid_len);
        if (send_buf == NULL)
        {
            return;
        }

        pull_len = smf_media_kfifo_data_pull(send_buf + offset, audio_data_len);
        if (pull_len != audio_data_len)
        {
            MEDIA_ERR("read audio data failed! pull len %u, audio data len %u \n", pull_len, audio_data_len);
            smf_media_audio_bt_send_buf_free(send_buf);
            return;
        }
        smf_media_audio_bt_send_buf(data_pipe, send_buf);

        fifo_size   = 0;
        pull_len    = 0;
        offset      = 0;
        valid_len   = 0;
        audio_data_len = 0;
    }
    MEDIA_WARN("send %d packet\n", packet_num);
}


static void smf_media_audio_bt_a2dp_src_data_event(void* priv, ssize_t nread, const uv_buf_t* buf)
{
    //TO DO
    smf_media_audio_bt_pipe_t* pipe = (smf_media_audio_bt_pipe_t*)priv;
    smf_media_audio_bt_free((uv_handle_t *)&pipe->hdl, buf);
}

static void smf_media_audio_bt_a2dp_src_ctrl_event(void* priv, ssize_t nread, const uv_buf_t* buf)
{
    uint8_t event_type;
    uint8_t* data = (uint8_t*)buf->base;
    smf_media_audio_bt_pipe_t* pipe = (smf_media_audio_bt_pipe_t*)priv;

    MEDIA_INFO("pipe=%s, read len %d\n", pipe->name, nread);
    while (data < (uint8_t *)(buf->base + nread))
    {
        // data format
        // ctrl_stream: |           byte0            |                     byte n                        |
        //              | event_type@A2DP_CTRL_EVT_T | event param(only BT_AUDIO_CTRL_EVT_UPDATE_CONFIG) |
        STREAM_TO_UINT8(event_type, data);

        MEDIA_INFO("evt=%d\n", event_type);
        switch (event_type)
        {
            case BT_AUDIO_CTRL_EVT_STARTED:
            {
                smf_media_audio_output_a2dpsink_start();
                uv_timer_start(&g_smf_media_bt.uv_timer, smf_media_audio_bt_a2dp_src_send_data,
                SMF_MEDIA_AUDIO_BT_A2DP_SRC_SEND_INTERVAL, SMF_MEDIA_AUDIO_BT_A2DP_SRC_SEND_REPEAT);
            } break;
            case BT_AUDIO_CTRL_EVT_START_FAIL:
            {
            } break;
            case BT_AUDIO_CTRL_EVT_STOPPED:
            {
            } break;
            case BT_AUDIO_CTRL_EVT_UPDATE_CONFIG:
            {
                data += smf_media_audio_bt_a2dp_codec_update(&pipe->codec_cfg, data);
                smf_media_audio_bt_send_cmd(pipe, A2DP_CTRL_CMD_CONFIG_DONE);
                if (!pipe->codec_cfg->codec_param.a2dp.valid_code)
                {
                    MEDIA_WARN("codec config not valid");
                    break;
                }
                smf_media_audio_bt_create_pipe_internal(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SOURCE_AUDIO);
            } break;
            default:
                break;
        }
    }
    smf_media_audio_bt_free((uv_handle_t *)&pipe->hdl, buf);
}

static void smf_media_audio_bt_a2dp_sink_data_event(void* priv, ssize_t nread, const uv_buf_t* buf)
{
    uint8_t* pkt_pdu    = (uint8_t*)buf->base;
    uint16_t pkt_header = 0;
    uint16_t pkt_len    = 0;
    audio_packet_t* packet = NULL;
    smf_media_audio_bt_pipe_t* pipe = (smf_media_audio_bt_pipe_t*) priv;

    packet = pipe->packet;
    if (packet == NULL)
    {
        return;
    }

    //recieve new packet header
    if (!packet->data.len)
    {
        if (nread != SMF_MEDIA_AUDIO_BT_A2DP_DATA_HDRSIZE)
        {
            return;
        }

        pkt_header = STREAM_GET_DATA_HEADER(pkt_pdu);
        pkt_len    = STREAM_GET_DATA_LEN(pkt_pdu);
        //check header
        if (pkt_header != SMF_MEDIA_AUDIO_BT_A2DP_DATA_HEAD)
        {
            //TODO restart?
            MEDIA_ERR("audio packet header err!");
            return;
        }
        packet->data.len = pkt_len;
        return;
    }

    //recieve new pakcet data
    if (packet->data.base == buf->base)
    {
        if (nread < packet->data.len)
        {
            //not complete
            MEDIA_WARN("still need %d bytes", packet->data.len - nread);
            return;
        }
        else if (nread > packet->data.len)
        {
            MEDIA_ERR("recieve wrong legnth %d excepted len %d", nread, packet->data.len);
            goto ___failed___;
        }

    }
    else
    {
        MEDIA_ERR("unknown error");
        goto ___failed___;
    }

    //recieve all packet data
    if (pipe->media_id)
    {
        smf_media_kfifo_data_push(buf->base, nread);
    }
    else
    {
        MEDIA_WARN("media not ready! \n");
    }

___failed___:
    memset(packet, 0, sizeof(audio_packet_t));
    smf_media_audio_bt_free((uv_handle_t *)&pipe->hdl, buf);
}

static void smf_media_audio_bt_a2dp_sink_ctrl_event(void* priv, ssize_t nread, const uv_buf_t* buf)
{
    uint8_t event_type;
    uint8_t* data = (uint8_t*)buf->base;
    uint32_t codec_type = 0;
    smf_media_audio_bt_pipe_t* pipe = (smf_media_audio_bt_pipe_t*)priv;
    smf_media_audio_bt_pipe_t* sink_data_pipe = get_pipe(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SINK_AUDIO);
    MEDIA_INFO("pipe=%s, read_len=%d \n", pipe->name, nread);

    while (data < (uint8_t *)(buf->base + nread))
    {
        // data format
        // ctrl_stream: |           byte0            |                     byte n                        |
        //              | event_type@A2DP_CTRL_EVT_T | event param(only BT_AUDIO_CTRL_EVT_UPDATE_CONFIG) |
        STREAM_TO_UINT8(event_type, data);
        MEDIA_INFO("event = %d\n", event_type);
        switch (event_type)
        {
            case BT_AUDIO_CTRL_EVT_STARTED:
            {
                if (pipe->media_id)
                {
                    break;
                }
                if (pipe->started_ignore)
                {
                    pipe->started_ignore--;
                    break;
                }

                smf_media_audio_output_config();
                codec_type = pipe->codec_cfg->codec_param.a2dp.codec_type;
                if (codec_type == BTS_A2DP_TYPE_SBC)
                {
                    pipe->media_id =
                        smf_media_audio_player_a2dp_start("sbc", SMF_VOLUME_MAX);
                }
                else if (codec_type == BTS_A2DP_TYPE_MPEG2_4_AAC)
                {
                    pipe->media_id =
                        smf_media_audio_player_a2dp_start("aac", SMF_VOLUME_MAX);
                }
                else
                {
                    MEDIA_ERR("unkonw code_type = %u\n", pipe->codec_cfg->codec_param.a2dp.codec_type);
                }
                MEDIA_INFO("media_id=%llu, codec_type=%u\n", pipe->media_id, pipe->codec_cfg->codec_param.a2dp.codec_type);
                sink_data_pipe->media_id = pipe->media_id;
            } break;
            case BT_AUDIO_CTRL_EVT_START_FAIL:
            {
                smf_media_audio_player_a2dp_stop(pipe->media_id);
                pipe->media_id = 0;
                sink_data_pipe->media_id = 0;
            } break;
            case BT_AUDIO_CTRL_EVT_STOPPED:
            {
                smf_media_audio_player_a2dp_stop(pipe->media_id);
                pipe->media_id = 0;
                sink_data_pipe->media_id = 0;
            } break;
            case BT_AUDIO_CTRL_EVT_UPDATE_CONFIG:
            {
                pipe->started_ignore = 2;
                data += smf_media_audio_bt_a2dp_codec_update(&pipe->codec_cfg, data);
                smf_media_audio_bt_send_cmd(pipe, A2DP_CTRL_CMD_CONFIG_DONE);

                if (!pipe->codec_cfg->codec_param.a2dp.valid_code)
                {
                    MEDIA_WARN("config not valid");
                    break;
                }

                sink_data_pipe->packet = malloc(sizeof(audio_packet_t));
                memset(sink_data_pipe->packet, 0, sizeof(audio_packet_t));

                smf_media_audio_bt_create_pipe_internal(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SINK_AUDIO);
                smf_media_audio_bt_send_cmd(pipe, A2DP_CTRL_CMD_START);
            } break;
            default:
                MEDIA_WARN("unkonw event= %d \n", event_type);
        }
    }

    smf_media_audio_bt_free((uv_handle_t *)&pipe->hdl, buf);
}

static void smf_media_audio_bt_sco_ctrl_event(void* priv, ssize_t nread, const uv_buf_t* buf)
{
    uint8_t event_type;
    uint8_t* data = (uint8_t*)buf->base;
    smf_media_audio_bt_pipe_t* pipe = (smf_media_audio_bt_pipe_t*) priv;

    while (data < (uint8_t *)(buf->base + nread))
    {
        // data format
        // ctrl_stream: |           byte0            |                     byte n                        |
        //              | event_type@A2DP_CTRL_EVT_T | event param(only BT_AUDIO_CTRL_EVT_UPDATE_CONFIG) |
        STREAM_TO_UINT8(event_type, data);

        MEDIA_INFO("pipe=%s, evt=%d\n", pipe->name, event_type);

        switch (event_type)
        {
            case BT_AUDIO_CTRL_EVT_STARTED:
            {
                if (pipe->codec_cfg->codec_param.sco.sample_rate == 8000)
                {
                    pipe->media_id = smf_media_audio_btsco_start(1, SMF_VOLUME_MAX);
                }
                else if (pipe->codec_cfg->codec_param.sco.sample_rate == 16000)
                {
                    pipe->media_id = smf_media_audio_btsco_start(2, SMF_VOLUME_MAX);
                }
            } break;
            case BT_AUDIO_CTRL_EVT_START_FAIL:
            {
                smf_media_audio_btsco_stop(pipe->media_id);
            } break;
            case BT_AUDIO_CTRL_EVT_STOPPED:
            {
                smf_media_audio_btsco_stop(pipe->media_id);
            } break;
            case BT_AUDIO_CTRL_EVT_UPDATE_CONFIG:
            {

            } break;
            default:
                MEDIA_WARN("unkonw event= %d \n", event_type);
        }
    }
    smf_media_audio_bt_free((uv_handle_t *)&pipe->hdl, buf);
}

int smf_media_audio_bt_close_internal(SMF_MEDIA_AUDIO_BT_PATH_TYPE path_type)
{
    smf_media_audio_bt_env_t* smf_bt = &g_smf_media_bt;
    smf_media_audio_bt_pipe_t* pipe = NULL;

    if (!(smf_bt->path_map & path_type))
    {
        MEDIA_ERR("path map %d \n", smf_bt->path_map);
        return -EINVAL;
    }

    smf_bt->path_map &= (~path_type);
    if (path_type == SMF_MEDIA_AUDIO_BT_A2DP)
    {
        pipe = get_pipe(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SOURCE_CTRL);
        smf_media_audio_bt_delete_pipe(pipe);
        pipe = get_pipe(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SOURCE_AUDIO);
        smf_media_audio_bt_delete_pipe(pipe);
        pipe = get_pipe(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SINK_CTRL);
        smf_media_audio_bt_delete_pipe(pipe);
        pipe = get_pipe(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SINK_AUDIO);
        smf_media_audio_bt_delete_pipe(pipe);
    }
    else if (path_type == SMF_MEDIA_AUDIO_BT_LEA)
    {
        //TO DO
    }
    else if (path_type == SMF_MEDIA_AUDIO_BT_SCO)
    {
        pipe = get_pipe(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_HFP_CTRL);
        smf_media_audio_btsco_stop(pipe->media_id);
        //Delete a pipeline will fail when if(pathotype==SMF_MEDIA_SAUDIO_SCO)
        smf_media_audio_bt_delete_pipe(pipe);
    }
    return 0;
}

int smf_media_audio_bt_ctrl_send_internal(SMF_MEDIA_AUDIO_BT_CTRL_TYPE ctrl_type)
{
    bool ret = 0;
    smf_media_audio_bt_env_t* smf_bt = &g_smf_media_bt;
    smf_media_audio_bt_pipe_t* pipe = NULL;
    if (ctrl_type == SMF_MEDIA_AUDIO_BT_CTRL_START)
    {
        pipe = get_pipe(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SOURCE_CTRL);
        if (pipe->codec_cfg)
        {
            ret = smf_media_audio_bt_send_cmd(pipe, A2DP_CTRL_CMD_START);
        }
        //TODO LEA
    }
    else if (ctrl_type == SMF_MEDIA_AUDIO_BT_CTRL_STOP)
    {
        pipe = get_pipe(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SOURCE_CTRL);
        if (pipe->codec_cfg)
        {
            uv_timer_stop(&smf_bt->uv_timer);
            ret = smf_media_audio_bt_send_cmd(pipe, A2DP_CTRL_CMD_STOP);
        }
        //TODO LEA
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

static void smf_media_audio_bt_async_task(uv_async_t* handle)
{
    bt_asycn_task_t *task = CONTAINER_OF(handle, bt_asycn_task_t, async);
    MEDIA_INFO("task cmd %d", task->type);

    if (task->type == SMF_MEDIA_BT_TASK_CLOSE)
    {
        //close pipe
        task->res = smf_media_audio_bt_close_internal(task->path_type);
    }
    else if (task->type == SMF_MEDIA_BT_TASK_SEND)
    {
        task->res = smf_media_audio_bt_ctrl_send_internal(task->ctrl_type);
    }
    else if (task->type == SMF_MEDIA_BT_TASK_CREATE_PIPE)
    {
        task->res = (uintptr_t)smf_media_audio_bt_create_pipe_internal(task->trans_id);
    }

    uv_sem_post(&task->done);
}

static void* smf_media_audio_bt_thread(void* arg)
{
    smf_media_audio_bt_env_t* smf_bt = (smf_media_audio_bt_env_t*)arg;
    bt_asycn_task_t *task = &smf_bt->task;
    uv_loop_t* loop = uv_loop_new();
    uv_timer_t* timer = NULL;

    if (smf_bt == NULL)
    {
        MEDIA_ERR("init audio bt thread failed!");
        return NULL;
    }

    timer = &smf_bt->uv_timer;
    smf_bt->uv_loop = loop;
    smf_bt->pool_size = SMF_MEDIA_BT_BUF_POOL_SIZE;

    uv_sem_init(&task->done, 0);
    uv_mutex_init(&task->mutex);
    uv_async_init(loop, &task->async, smf_media_audio_bt_async_task);

    uv_timer_init(loop, timer);
    uv_sem_post(&smf_bt->ready);

    MEDIA_INFO("audio bt thread init done!");
    uv_run(loop, UV_RUN_DEFAULT);

    uv_mutex_destroy(&task->mutex);
    uv_sem_destroy(&task->done);
    //deinit
    uv_loop_close(loop);

    memset(smf_bt, 0, sizeof(smf_media_audio_bt_env_t));
    MEDIA_INFO("audio bt thread exit");
    return NULL;
}

static int smf_media_bt_thread_create(void)
{
    smf_media_audio_bt_env_t* smf_bt = &g_smf_media_bt;
    pthread_t thread_id;
    pthread_attr_t p_attr;
    struct sched_param param;
    int ret = 0;

    uv_sem_init(&smf_bt->ready, 0);
    pthread_attr_init(&p_attr);
    pthread_attr_setstacksize(&p_attr, SMF_MEDIA_AUDIO_BT_THREAD_STACK_SIZE);
    pthread_attr_setschedpolicy(&p_attr, SCHED_RR);

    pthread_attr_getschedparam(&p_attr, &param);
    param.sched_priority = SMF_MEDIA_AUDIO_BT_THREAD_STACK_PRIORITY;
    pthread_attr_setschedparam(&p_attr, &param);

    ret = pthread_create(&thread_id, &p_attr, smf_media_audio_bt_thread, smf_bt);
    if (ret != 0) {
        MEDIA_ERR("%s create audio bt thread failed: %d", __func__, ret);
        return ret;
    }

    pthread_setname_np(thread_id, SMF_MEDIA_AUDIO_BT_THREAD_STACK_NAME);



    pthread_detach(thread_id);

    smf_bt->thread_id = thread_id;
    uv_sem_wait(&smf_bt->ready);
    MEDIA_INFO("create bt audio thread successful");
    return ret;
}

smf_media_audio_bt_pipe_t pipes[] = 
{
    {
        .name = CONFIG_BLUETOOTH_A2DP_SOURCE_CTRL_PATH,
        .read_cb = smf_media_audio_bt_a2dp_src_ctrl_event,
    }, // @CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SOURCE_CTRLL
    {
        .name = CONFIG_BLUETOOTH_A2DP_SOURCE_DATA_PATH,
        .read_cb = smf_media_audio_bt_a2dp_src_data_event,
    }, // CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SOURCE_AUDIO
    {
        .name = CONFIG_BLUETOOTH_A2DP_SINK_CTRL_PATH,
        .read_cb = smf_media_audio_bt_a2dp_sink_ctrl_event,
    }, // CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SINK_CTRL
    {
        .name = CONFIG_BLUETOOTH_A2DP_SINK_DATA_PATH,
        .read_cb = smf_media_audio_bt_a2dp_sink_data_event,
    }, // CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SINK_AUDIO
    {
        .name = CONFIG_BLUETOOTH_SCO_CTRL_PATH,
        .read_cb = smf_media_audio_bt_sco_ctrl_event,
    }, // CONFIG_BLUETOOTH_AUDIO_TRANS_ID_HFP_CTRL
};


static smf_media_audio_bt_pipe_t* get_pipe(int trans_id)
{
    smf_media_audio_bt_pipe_t* pipe = &pipes[trans_id];
    return pipe;
}

int smf_media_audio_bt_open(SMF_MEDIA_AUDIO_BT_PATH_TYPE path_type)
{
    int ret;
    MEDIA_INFO("path type %d", path_type);
    smf_media_audio_bt_env_t* smf_bt = &g_smf_media_bt;
    smf_media_audio_bt_pipe_t* pipe = NULL;

    if ((path_type == SMF_MEDIA_AUDIO_BT_UNKONW) || (path_type >= SMF_MEDIA_AUDIO_BT_MAX))
    {
        MEDIA_ERR("%d \n", path_type);
        return -EINVAL;
    }

    if (smf_bt->path_map & path_type)
    {
        MEDIA_INFO("0x%x, 0x%x \n", smf_bt->path_map, path_type);
        return 0;
    }

    if (smf_bt->uv_loop == NULL)
    {
        ret = smf_media_bt_thread_create();
        if (ret < 0)
        {
            return ret;
        }
    }

    smf_bt->path_map |= path_type;

    if (path_type == SMF_MEDIA_AUDIO_BT_A2DP)
    {
        //Creating a pipeline will fail when if(pathotype==SMF_MEDIA_SAUDIO_SCO)
        smf_media_audio_bt_create_pipe(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SOURCE_CTRL);
        smf_media_audio_bt_create_pipe(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SINK_CTRL);

    }
    else if (path_type == SMF_MEDIA_AUDIO_BT_LEA)
    {
        //TO DO
    }
    else if (path_type == SMF_MEDIA_AUDIO_BT_SCO)
    {
        pipe = smf_media_audio_bt_create_pipe(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_HFP_CTRL);
        int sample_rate = pipe->codec_cfg->codec_param.sco.sample_rate;
        uint8_t role = pipe->codec_cfg->codec_param.sco.role;
        uint8_t type = 0;


        if (sample_rate == SMF_MEDIA_SCO_CODEC_SAMPLE_RATE_8000)
        {
            type = SMF_MEDIA_SCO_CODEC_TYPE_CVSD;
        }
        else if (sample_rate == SMF_MEDIA_SCO_CODEC_SAMPLE_RATE_16000)
        {
            type = SMF_MEDIA_SCO_CODEC_TYPE_MSBC;
        }
        else
        {
            MEDIA_ERR("%s unknown sample rate: %d", __func__, sample_rate);
            return 0;
        }

        MEDIA_INFO("%s role %d type %d", __func__, role, type);

        if (role == SMF_MEDIA_HFP_ROLE_AG)
        {
            //lte

        }
        else if (role == SMF_MEDIA_HFP_ROLE_HF)
        {
            pipe->media_id = smf_media_audio_btsco_start(type, SMF_VOLUME_MAX);
        }
    }

    return 0;
}

int smf_media_audio_bt_close(SMF_MEDIA_AUDIO_BT_PATH_TYPE path_type)
{
    smf_media_audio_bt_env_t* smf_bt = &g_smf_media_bt;
    bt_asycn_task_t* task = &smf_bt->task;
    int ret;

    if (smf_bt->uv_loop == NULL)
    {
        MEDIA_ERR("not init");
        return 0;
    }

    uv_mutex_lock(&task->mutex);

    task->type      = SMF_MEDIA_BT_TASK_CLOSE;
    task->path_type = path_type;
    uv_async_send(&task->async);
    uv_sem_wait(&task->done);

    uv_mutex_unlock(&task->mutex);
    ret = task->res;
    if (smf_bt->path_map == SMF_MEDIA_AUDIO_BT_UNKONW)
    {
        MEDIA_INFO("deinit audio bt");
        uv_stop(smf_bt->uv_loop);
        smf_bt->uv_loop = NULL;
    }
    return ret;
}

int smf_media_audio_bt_ctrl_send(SMF_MEDIA_AUDIO_BT_CTRL_TYPE ctrl_type)
{
    bool ret = 0;
    smf_media_audio_bt_env_t* smf_bt = &g_smf_media_bt;
    bt_asycn_task_t* task = &smf_bt->task;

    if (pthread_self() == smf_bt->thread_id)
    {
        return smf_media_audio_bt_ctrl_send_internal(ctrl_type);
    }

    uv_mutex_lock(&task->mutex);

    task->type      = SMF_MEDIA_BT_TASK_SEND;
    task->ctrl_type = ctrl_type;
    uv_async_send(&task->async);
    uv_sem_wait(&task->done);

    uv_mutex_unlock(&task->mutex);
    ret = task->res;
    if (!ret)
    {
        return -EINVAL;
    }

    return 0;
}

const smf_media_audio_bt_codec_cfg_t* smf_media_audio_bt_get_codec_info(void)
{
    smf_media_audio_bt_pipe_t* pipe = get_pipe(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_SOURCE_CTRL);
    return pipe->codec_cfg;
}

void smf_media_audio_bt_set_sco_param(int sample_rate, uint8_t role)
{
    smf_media_audio_bt_pipe_t* pipe = get_pipe(CONFIG_BLUETOOTH_AUDIO_TRANS_ID_HFP_CTRL);
    smf_media_audio_bt_codec_cfg_t *config = pipe->codec_cfg;
    if (!config)
    {
        config = (smf_media_audio_bt_codec_cfg_t *)malloc(sizeof(smf_media_audio_bt_codec_cfg_t));
        pipe->codec_cfg = config;
    }

    config->type = SMF_MEDIA_AUDIO_BT_SCO;
    config->codec_param.sco.sample_rate = sample_rate;
    config->codec_param.sco.role = role;

}

