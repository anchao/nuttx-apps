/****************************************************************************
 * frameworks/media/client/media_uv_graph.c
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

#include <errno.h>
#include <inttypes.h>
#include <media_defs.h>
#include <media_focus.h>
#include <media_player.h>
#include <media_policy.h>
#include <media_recorder.h>
#include <media_utils.h>
#include <stdio.h>
#include <sys/queue.h>
#include <uv.h>

#include "media_common.h"
#include "media_metadata.h"
#include "media_uv.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef LIST_ENTRY(MediaListenPriv) MediaListenEntry;
typedef LIST_HEAD(MediaListenList, MediaListenPriv) MediaListenList;

#define MEDIA_STREAM_FIELDS                       \
    int id;                                       \
    char* name; /* Stream type or source type. */ \
    void* proxy;                                  \
    void* cookie;                                 \
    media_uv_callback on_open;                    \
    media_uv_callback on_close;                   \
    media_event_callback on_event;                \
    uv_loop_t* loop;                              \
    /* Fields for buffer mode. */                 \
    uv_pipe_t* pipe;                              \
    MediaListenList listeners;                    \
    media_uv_object_callback on_connection;       \
    /* Fields for auto focus. */                  \
    MediaFocusPriv* focus;                        \
    /* Fields for query metadata. */              \
    MediaQueryPriv* query;

typedef struct MediaFocusPriv {
    void* stream;
    void* handle; /* Focus handle. */
    media_uv_callback on_play;
    void* on_play_cookie;
} MediaFocusPriv;

typedef struct MediaQueryPriv {
    void* player;
    int expected;
    void* cookie;
    media_uv_object_callback on_query;
    media_metadata_t diff;
} MediaQueryPriv;

typedef struct MediaStreamPriv {
    MEDIA_STREAM_FIELDS
} MediaStreamPriv;

typedef struct MediaPlayerPriv {
    MEDIA_STREAM_FIELDS
    media_metadata_t data;
} MediaPlayerPriv;

typedef struct MediaRecorderPriv {
    MEDIA_STREAM_FIELDS
} MediaRecorderPriv;

typedef struct MediaListenPriv {
    MediaListenEntry entry;
    MediaStreamPriv* priv;
    uv_pipe_t pipe;
} MediaListenPriv;

typedef struct MediaTakePicPriv {
    MediaRecorderPriv* handle;
    media_uv_callback on_complete;
    void* cookie;
    int err_code;
} MediaTakePicPriv;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* Functions to open/close proxy. */

static void media_uv_stream_release(MediaStreamPriv* priv);
static void media_uv_stream_disconnect_cb(void* cookie, int ret);
static void media_uv_stream_close_cb(void* cookie, int ret);
static void media_uv_stream_open_cb(void* cookie, int ret);
static void media_uv_stream_connect_cb(void* cookie, int ret);

/* Functions to send/receive parcel. */

static void media_uv_stream_event_cb(void* cookie,
    void* cookie0, void* cookie1, media_parcel* parcel);
static void media_uv_stream_receive_cb(void* cookie,
    void* cookie0, void* cookie1, media_parcel* parcel);
static void media_uv_stream_receive_int_cb(void* cookie,
    void* cookie0, void* cookie1, media_parcel* parcel);
static void media_uv_stream_receive_unsigned_cb(void* cookie,
    void* cookie0, void* cookie1, media_parcel* parcel);
static int media_uv_stream_send(void* stream, const char* target,
    const char* cmd, const char* arg, int res_len,
    media_uv_parcel_callback parser, void* cb, void* cookie);

/* Functions to create data pipe for buffer mode. */

static void media_uv_stream_close_pipe_cb(uv_handle_t* handle);
static void media_uv_stream_close_pipe(MediaStreamPriv* priv);
static void media_uv_stream_listen_close_cb(uv_handle_t* handle);
static void media_uv_stream_listen_remove(MediaListenPriv* pipe);
static MediaListenPriv* media_uv_stream_listen_add(MediaStreamPriv* priv);
static void media_uv_stream_listen_clear(MediaStreamPriv* priv,
    MediaListenPriv* availble);
static void media_uv_stream_listen_connection_cb(uv_stream_t* stream, int ret);
static int media_uv_stream_listen_init(MediaStreamPriv* priv, const char* addr);

/* Functions for focus suggestions. */

static void media_uv_stream_abandon_focus_cb(void* cookie, int ret);
static void media_uv_stream_abandon_focus(MediaStreamPriv* stream);
static int media_uv_stream_request_focus(MediaStreamPriv* stream,
    const char* scenario, media_focus_callback on_suggestion,
    media_uv_callback on_play, void* cookie);
static void media_uv_player_suggest_cb(int suggest, void* cookie);
static void media_uv_recorder_suggest_cb(int suggest, void* cookie);
static int media_uv_stream_pause(MediaStreamPriv* priv, void* cb, void* cookie);

/* Functions to query metadata. */

static void media_uv_player_query_complete(void* cookie);
static void media_uv_player_query_duration_cb(void* cookie, int ret, unsigned value);
static void media_uv_player_query_position_cb(void* cookie, int ret, unsigned value);
static void media_uv_player_query_state_cb(void* cookie, int ret, int value);
static void media_uv_player_query_volume_cb(void* cookie, int ret, int value);

/****************************************************************************
 * Common Functions
 ****************************************************************************/

static void media_uv_stream_release(MediaStreamPriv* priv)
{
    if (priv && !priv->proxy && LIST_EMPTY(&priv->listeners) && (!priv->query || !priv->query->expected)) {
        if (priv->on_close)
            priv->on_close(priv->cookie, 0);

        free(priv->query);
        free(priv);
    }
}

static void media_uv_stream_disconnect_cb(void* cookie, int ret)
{
    MediaStreamPriv* priv = cookie;

    priv->proxy = NULL; /* Mark proxy is finalized. */
    media_uv_stream_release(priv);
}

static void media_uv_stream_close_cb(void* cookie, int ret)
{
    MediaStreamPriv* priv = cookie;

    MEDIA_INFO("%s:%p ret:%d\n", priv->name, priv, ret);
    media_uv_disconnect(priv->proxy, media_uv_stream_disconnect_cb);
}

static void media_uv_stream_open_cb(void* cookie, int ret)
{
    MediaStreamPriv* priv = cookie;

    if (ret < 0)
        media_uv_reconnect(priv->proxy);
    else {
        MEDIA_INFO("open:%s result:%d handle:%p\n", priv->name, ret, priv);
        if (priv->on_open)
            priv->on_open(priv->cookie, ret);
    }
}

static void media_uv_stream_connect_cb(void* cookie, int ret)
{
    MediaStreamPriv* priv = cookie;

    if (ret >= 0)
        ret = media_uv_stream_send(priv, NULL, "open", priv->name, 0,
            media_uv_stream_receive_cb, media_uv_stream_open_cb, priv);

    if (ret < 0 && priv->on_open)
        priv->on_open(priv->cookie, ret);
}

static void media_uv_stream_event_cb(void* cookie, void* cookie0, void* cookie1,
    media_parcel* parcel)
{
    int32_t event = MEDIA_EVENT_NOP, result = -ECANCELED;
    MediaStreamPriv* priv = cookie;
    const char* response = NULL;

    if (parcel) {
        media_parcel_read_scanf(parcel, "%i%i%s", &event, &result, &response);
        MEDIA_INFO("%s:%p %s(%" PRId32 ") ret:%" PRId32,
            priv->name, priv, media_event_get_name(event), event, result);
        if (event == MEDIA_EVENT_COMPLETED)
            media_uv_stream_abandon_focus(priv);
    }

    if (priv->focus && result == -EPERM)
        return; /* Ignore the invalid state changes. */

    priv->on_event(priv->cookie, event, result, response);
}

static void media_uv_stream_receive_cb(void* cookie,
    void* cookie0, void* cookie1, media_parcel* parcel)
{
    media_uv_callback cb = cookie0;
    int32_t result = -ECANCELED;

    if (parcel)
        media_parcel_read_scanf(parcel, "%i%s", &result, NULL);

    cb(cookie1, result);
}

static void media_uv_stream_receive_int_cb(void* cookie,
    void* cookie0, void* cookie1, media_parcel* parcel)
{
    media_uv_int_callback cb = cookie0;
    const char* response = NULL;
    int32_t result = -ECANCELED;
    int value = 0;

    if (parcel) {
        media_parcel_read_scanf(parcel, "%i%s", &result, &response);
        if (response)
            value = strtol(response, NULL, 0);
    }

    cb(cookie1, result, value);
}

static void media_uv_stream_receive_unsigned_cb(void* cookie,
    void* cookie0, void* cookie1, media_parcel* parcel)
{
    media_uv_unsigned_callback cb = cookie0;
    const char* response = NULL;
    int32_t result = -ECANCELED;
    unsigned value = 0;

    if (parcel) {
        media_parcel_read_scanf(parcel, "%i%s", &result, &response);
        if (response)
            value = strtoul(response, NULL, 0);
    }

    cb(cookie1, result, value);
}

static void media_uv_stream_receive_volume_cb(void* cookie,
    void* cookie0, void* cookie1, media_parcel* parcel)
{
    media_uv_float_callback cb = cookie0;
    const char* response = NULL;
    int32_t result = -ECANCELED;
    float volume = 0;

    if (parcel) {
        media_parcel_read_scanf(parcel, "%i%s", &result, &response);
        if (response)
            sscanf(response, "vol:%f", &volume);
    }

    cb(cookie1, result, volume);
}

static void media_uv_stream_receive_string_cb(void* cookie,
    void* cookie0, void* cookie1, media_parcel* parcel)
{
    media_uv_string_callback cb = cookie0;
    const char* response = NULL;
    int32_t result = -ECANCELED;

    if (parcel)
        media_parcel_read_scanf(parcel, "%i%s", &result, &response);

    cb(cookie1, result, response);
}

static int media_uv_stream_send(void* stream, const char* target,
    const char* cmd, const char* arg, int res_len,
    media_uv_parcel_callback parser, void* cb, void* cookie)
{
    MediaStreamPriv* priv = stream;
    media_parcel parcel;
    int ret;

    media_parcel_init(&parcel);
    ret = media_parcel_append_printf(&parcel, "%i%s%s%s%i",
        priv->id, target, cmd, arg, res_len);
    if (ret < 0)
        return ret;

    MEDIA_INFO("%s %p %s %s %s",
        priv->name, priv->proxy, target ? target : "_", cmd, arg ? arg : "_");

    ret = media_uv_send(priv->proxy, parser, cb, cookie, &parcel);
    media_parcel_deinit(&parcel);
    return ret;
}

/**
 * @brief Close callback for listener pipe.
 */
static void media_uv_stream_listen_close_cb(uv_handle_t* handle)
{
    MediaListenPriv* listener = uv_handle_get_data(handle);
    MediaStreamPriv* priv = listener->priv;

    LIST_REMOVE(listener, entry);
    free(listener);
    media_uv_stream_release(priv);
}

/**
 * @brief Remove a listener from list.
 */
static void media_uv_stream_listen_remove(MediaListenPriv* listener)
{
    MEDIA_DEBUG("listener:%p\n", listener);

    if (!uv_is_closing((uv_handle_t*)&listener->pipe))
        uv_close((uv_handle_t*)&listener->pipe, media_uv_stream_listen_close_cb);
}

/**
 * @brief Add a listener to list.
 */
static MediaListenPriv* media_uv_stream_listen_add(MediaStreamPriv* priv)
{
    MediaListenPriv* listener;

    listener = zalloc(sizeof(MediaListenPriv));
    if (!listener)
        return NULL;

    if (uv_pipe_init(priv->loop, &listener->pipe, 0) < 0) {
        free(listener);
        return NULL;
    }

    MEDIA_DEBUG("stream:%p listener:%p\n", priv, listener);
    LIST_INSERT_HEAD(&priv->listeners, listener, entry);
    listener->priv = priv;
    uv_handle_set_data((uv_handle_t*)&listener->pipe, listener);
    return listener;
}

/**
 * @brief Clear unavailable listeners.
 */
static void media_uv_stream_listen_clear(MediaStreamPriv* priv,
    MediaListenPriv* availble)
{
    MediaListenPriv *listener, *tmp;

    LIST_FOREACH_SAFE(listener, &priv->listeners, entry, tmp)
    {
        if (listener != availble)
            media_uv_stream_listen_remove(listener);
    }
}

static void media_uv_stream_close_pipe_cb(uv_handle_t* handle)
{
    free(handle);
}

static void media_uv_stream_close_pipe(MediaStreamPriv* priv)
{
    if (priv->pipe) {
        uv_close((uv_handle_t*)priv->pipe, media_uv_stream_close_pipe_cb);
        priv->pipe = NULL;
    }

    if (priv->on_connection) {
        priv->on_connection(priv->cookie, -ECANCELED, NULL);
        priv->on_connection = NULL;
    }
}

/**
 * @brief Connection available to accept a data pipe.
 */
static void media_uv_stream_listen_connection_cb(uv_stream_t* stream, int ret)
{
    MediaListenPriv* listener = uv_handle_get_data((uv_handle_t*)stream);
    MediaStreamPriv* priv = listener->priv;

    if (ret < 0 || !priv || priv->pipe) { /* Only one listener meets error. */
        media_uv_stream_listen_remove(listener);
        return;
    }

    priv->pipe = zalloc(sizeof(uv_pipe_t));
    if (!priv->pipe) {
        media_uv_stream_listen_clear(priv, NULL);
        return;
    }

    ret = uv_pipe_init(priv->loop, priv->pipe, 0);
    if (ret < 0) {
        free(priv->pipe);
        priv->pipe = NULL;
        media_uv_stream_listen_clear(priv, NULL);
        return;
    }

    ret = uv_accept(stream, (uv_stream_t*)priv->pipe);
    if (ret < 0) {
        media_uv_stream_close_pipe(priv);
        media_uv_stream_listen_clear(priv, NULL);
        return;
    }

    MEDIA_DEBUG("listener:%p accept:%p\n", listener, priv->pipe);
    media_uv_stream_listen_clear(priv, listener); /* Clear redundant listeners. */
    if (!priv->on_connection) {
        media_uv_stream_close_pipe(priv);
        return;
    }

    priv->on_connection(priv->cookie, 0, priv->pipe);
    priv->on_connection = NULL;
}

/**
 * @brief Create listeners for data pipe.
 */
static int media_uv_stream_listen_init(MediaStreamPriv* priv, const char* addr)
{
    MediaListenPriv* listener;
    char *cpu, *saveptr;
    int ret = -ENOENT;
    char tmp[128];

    if (!LIST_EMPTY(&priv->listeners))
        return 0; /* Only create listeners once. */

    LIST_INIT(&priv->listeners);
    strlcpy(tmp, media_get_cpuname(), sizeof(tmp));
    for (cpu = strtok_r(tmp, " ,;|", &saveptr); cpu;
         cpu = strtok_r(NULL, " ,;|", &saveptr)) {
        listener = media_uv_stream_listen_add(priv);
        if (!listener)
            return -ENOMEM;

        if (!strcmp(cpu, CONFIG_RPMSG_LOCAL_CPUNAME)) {
            ret = uv_pipe_bind(&listener->pipe, addr);
        } else {
#ifdef CONFIG_NET_RPMSG
            ret = uv_pipe_rpmsg_bind(&listener->pipe, addr, cpu);
#else
            ret = -ENOSYS;
            media_uv_stream_listen_remove(listener);
            continue;
#endif
        }

        if (ret < 0)
            goto err;

        ret = uv_listen((uv_stream_t*)&listener->pipe, 1,
            media_uv_stream_listen_connection_cb);
        if (ret < 0)
            goto err;
    }

    return LIST_EMPTY(&priv->listeners) ? ret : 0;

err:
    media_uv_stream_listen_clear(priv, NULL);
    return ret;
}

static void media_uv_stream_abandon_focus_cb(void* cookie, int ret)
{
    free(cookie);
}

static void media_uv_stream_abandon_focus(MediaStreamPriv* stream)
{
    MediaFocusPriv* priv;
    int ret;

    if (!stream)
        return;

    priv = stream->focus;
    if (priv) {
        ret = media_uv_focus_abandon(priv->handle, media_uv_stream_abandon_focus_cb);
        if (ret >= 0) {
            /* Detach focus and stream at once. */
            priv->stream = NULL;
            stream->focus = NULL;
        }
    }
}

static int media_uv_stream_request_focus(MediaStreamPriv* stream,
    const char* scenario, media_focus_callback on_suggestion,
    media_uv_callback on_play, void* cookie)
{
    MediaFocusPriv* priv;

    priv = zalloc(sizeof(MediaFocusPriv));
    if (!priv)
        return -ENOMEM;

    priv->stream = stream;
    priv->on_play = on_play;
    priv->on_play_cookie = cookie;
    priv->handle = media_uv_focus_request(stream->loop, scenario,
        on_suggestion, priv);
    if (!priv->handle) {
        free(priv);
        return -ENOMEM;
    }

    stream->focus = priv;
    return 0;
}

static int media_uv_stream_pause(MediaStreamPriv* priv, void* cb, void* cookie)
{
    return media_uv_stream_send(priv, NULL, "pause", NULL, 0,
        media_uv_stream_receive_cb, cb, cookie);
}

/****************************************************************************
 * Player Functions
 ****************************************************************************/

static void media_uv_player_query_complete(void* cookie)
{
    MediaQueryPriv* ctx = cookie;
    MediaPlayerPriv* priv = ctx->player;
    int flags = 0;

    if (!ctx->expected) {
        flags = ctx->diff.flags;
        media_metadata_update(&priv->data, &ctx->diff);
        ctx->on_query(ctx->cookie, flags, &priv->data);
    }

    media_uv_stream_release((MediaStreamPriv*)priv);
}

static void media_uv_player_query_state_cb(void* cookie, int ret, int value)
{
    MediaQueryPriv* ctx = cookie;

    ctx->expected &= ~MEDIA_METAFLAG_STATE;

    if (ret >= 0) {
        ctx->diff.state = value;
        ctx->diff.flags |= MEDIA_METAFLAG_STATE;
    }

    media_uv_player_query_complete(ctx);
}

static void media_uv_player_query_volume_cb(void* cookie, int ret, int value)
{
    MediaQueryPriv* ctx = cookie;

    ctx->expected &= ~MEDIA_METAFLAG_VOLUME;

    if (ret >= 0) {
        ctx->diff.volume = value;
        ctx->diff.flags |= MEDIA_METAFLAG_VOLUME;
    }

    media_uv_player_query_complete(ctx);
}

static void media_uv_player_query_position_cb(void* cookie, int ret, unsigned value)
{
    MediaQueryPriv* ctx = cookie;

    ctx->expected &= ~MEDIA_METAFLAG_POSITION;

    if (ret >= 0) {
        ctx->diff.position = value;
        ctx->diff.flags |= MEDIA_METAFLAG_POSITION;
    }

    media_uv_player_query_complete(ctx);
}

static void media_uv_player_query_duration_cb(void* cookie, int ret, unsigned value)
{
    MediaQueryPriv* ctx = cookie;

    ctx->expected &= ~MEDIA_METAFLAG_DURATION;

    if (ret >= 0) {
        ctx->diff.duration = value;
        ctx->diff.flags |= MEDIA_METAFLAG_DURATION;
    }

    media_uv_player_query_complete(ctx);
}

static void media_uv_player_suggest_cb(int suggest, void* cookie)
{
    MediaFocusPriv* priv = cookie;
    MediaPlayerPriv* player = priv->stream;
    bool suggest_active = false;

    if (!player) {
        MEDIA_INFO("suggest:%d canceled\n", suggest);
        goto out;
    }

    MEDIA_INFO("%s:%p suggest:%d\n", player->name, player, suggest);

    switch (suggest) {
    case MEDIA_FOCUS_PLAY:
        suggest_active = true;
        media_uv_player_set_volume(player, 1.0, NULL, NULL);
        media_uv_player_start(player, priv->on_play, priv->on_play_cookie);
        break;

    case MEDIA_FOCUS_STOP:
        suggest_active = false;
        media_uv_player_stop(player, NULL, NULL);
        break;

    case MEDIA_FOCUS_PAUSE:
        suggest_active = false;
        media_uv_stream_pause(priv->stream, NULL, NULL);
        break;

    case MEDIA_FOCUS_PLAY_BUT_SILENT:
        suggest_active = true;
        media_uv_player_set_volume(player, 0.0, NULL, NULL);
        media_uv_player_start(player, NULL, NULL);
        break;

    case MEDIA_FOCUS_PLAY_WITH_DUCK:
        suggest_active = true;
        media_uv_player_set_volume(player, 0.1, NULL, NULL);
        media_uv_player_start(player, NULL, NULL);
        break;

    case MEDIA_FOCUS_PLAY_WITH_KEEP:
        break;
    }

out:
    if (priv->on_play) {
        if (!suggest_active) /* Notify user if focus request failed. */
            priv->on_play(priv->on_play_cookie, -EPERM);

        priv->on_play = NULL;
        priv->on_play_cookie = NULL;
    }
}

void* media_uv_player_open(void* loop, const char* stream,
    media_uv_callback on_open, void* cookie)
{
    MediaPlayerPriv* priv;

    if (stream && stream[0] != '\0') {
        priv = zalloc(sizeof(MediaPlayerPriv) + strlen(stream) + 1);
        if (!priv)
            return NULL;

        priv->name = (char*)(priv + 1);
        strcpy(priv->name, stream);
    } else {
        priv = zalloc(sizeof(MediaPlayerPriv));
        if (!priv)
            return NULL;
    }

    priv->loop = loop;
    priv->cookie = cookie;
    priv->on_open = on_open;
    priv->id = MEDIA_ID_PLAYER;
    priv->proxy = media_uv_connect(loop, media_get_cpuname(),
        media_uv_stream_connect_cb, priv);
    if (!priv->proxy) {
        media_uv_stream_disconnect_cb(priv, 0);
        return NULL;
    }

    return priv;
}

int media_uv_player_close(void* handle, int pending, media_uv_callback on_close)
{
    MediaPlayerPriv* priv = handle;
    char tmp[32];
    int ret;

    if (!priv)
        return -EINVAL;

    priv->on_close = on_close;
    snprintf(tmp, sizeof(tmp), "%d", pending);
    ret = media_uv_stream_send(priv, NULL, "close", tmp, 0,
        media_uv_stream_receive_cb, media_uv_stream_close_cb, priv);

    media_uv_stream_close_pipe(handle);
    media_uv_stream_listen_clear(handle, NULL);
    media_uv_stream_abandon_focus(handle);

    if (ret < 0)
        media_uv_stream_close_cb(priv, ret);

    return ret;
}

int media_uv_player_listen(void* handle, media_event_callback on_event)
{
    MediaPlayerPriv* priv = handle;
    int ret;

    if (!priv || !on_event)
        return -EINVAL;

    priv->on_event = on_event;
    ret = media_uv_stream_send(priv, NULL, "set_event", NULL, 0, NULL, NULL, NULL);
    if (ret < 0)
        return ret;

    return media_uv_listen(priv->proxy, NULL, media_uv_stream_event_cb);
}

int media_uv_player_prepare(void* handle, const char* url, const char* options,
    media_uv_object_callback on_connection, media_uv_callback on_prepare, void* cookie)
{
    MediaPlayerPriv* priv = handle;
    const char* cpu = NULL;
    char addr[32];
    int ret = 0;

    if (!handle)
        return -EINVAL;

    if (!url || !url[0]) {
        if (!on_connection)
            return -EINVAL; /* Need to provide data pipe to caller. */

        if (priv->on_connection)
            return -EPERM; /* Double prepare is not permitted. */

        snprintf(addr, sizeof(addr), MEDIA_GRAPH_SOCKADDR_NAME, handle);
        ret = media_uv_stream_listen_init(handle, addr);
        if (ret < 0)
            return ret;

        priv->on_connection = on_connection;
        cpu = CONFIG_RPMSG_LOCAL_CPUNAME;
        url = addr;
    }

    if (options && options[0] != '\0')
        ret = media_uv_stream_send(handle, NULL, "set_options", options, 0, NULL, NULL, NULL);

    if (ret >= 0)
        ret = media_uv_stream_send(handle, cpu, "prepare", url, 0,
            media_uv_stream_receive_cb, on_prepare, cookie);

    return ret;
}

int media_uv_player_reset(void* handle, media_uv_callback cb, void* cookie)
{
    if (!handle)
        return -EINVAL;

    media_uv_stream_close_pipe(handle);
    media_uv_stream_abandon_focus(handle);

    return media_uv_stream_send(handle, NULL, "reset", NULL, 0,
        media_uv_stream_receive_cb, cb, cookie);
}

int media_uv_player_start_auto(void* handle, const char* scenario,
    media_uv_callback cb, void* cookie)
{
    MediaPlayerPriv* priv = handle;

    if (!priv || !scenario || !scenario[0])
        return -EINVAL;

    if (priv->focus) {
        MEDIA_WARN("%s:%p force start\n", priv->name, priv);
        return media_uv_player_start(priv, cb, cookie);
    }

    return media_uv_stream_request_focus(handle, scenario,
        media_uv_player_suggest_cb, cb, cookie);
}

int media_uv_player_start(void* handle, media_uv_callback cb, void* cookie)
{
    if (!handle)
        return -EINVAL;

    return media_uv_stream_send(handle, NULL, "start", NULL, 0,
        media_uv_stream_receive_cb, cb, cookie);
}

int media_uv_player_pause(void* handle, media_uv_callback cb, void* cookie)
{
    MediaPlayerPriv* priv = handle;

    if (!priv)
        return -EINVAL;

    media_uv_stream_abandon_focus(handle);

    return media_uv_stream_pause(handle, cb, cookie);
}

int media_uv_player_stop(void* handle, media_uv_callback cb, void* cookie)
{
    MediaPlayerPriv* priv = handle;

    if (!priv)
        return -EINVAL;

    media_uv_stream_close_pipe(handle);
    media_uv_stream_abandon_focus(handle);

    return media_uv_stream_send(handle, NULL, "stop", NULL, 0,
        media_uv_stream_receive_cb, cb, cookie);
}

int media_uv_player_set_volume(void* handle, float volume,
    media_uv_callback cb, void* cookie)
{
    char tmp[32];

    if (!handle)
        return -EINVAL;

    snprintf(tmp, sizeof(tmp), "%f", volume);
    return media_uv_stream_send(handle, NULL, "volume", tmp, 0,
        media_uv_stream_receive_cb, cb, cookie);
}

int media_uv_player_get_volume(void* handle, media_uv_float_callback cb, void* cookie)
{
    if (!handle)
        return -EINVAL;

    return media_uv_stream_send(handle, NULL, "get_volume", NULL, 32,
        media_uv_stream_receive_volume_cb, cb, cookie);
}

int media_uv_player_get_playing(void* handle, media_uv_int_callback cb, void* cookie)
{
    if (!handle)
        return -EINVAL;

    return media_uv_stream_send(handle, NULL, "get_playing", NULL, 32,
        media_uv_stream_receive_int_cb, cb, cookie);
}

int media_uv_player_get_position(void* handle,
    media_uv_unsigned_callback cb, void* cookie)
{
    if (!handle)
        return -EINVAL;

    return media_uv_stream_send(handle, NULL, "get_position", NULL, 32,
        media_uv_stream_receive_unsigned_cb, cb, cookie);
}

int media_uv_player_get_duration(void* handle,
    media_uv_unsigned_callback cb, void* cookie)
{
    if (!handle)
        return -EINVAL;

    return media_uv_stream_send(handle, NULL, "get_duration", NULL, 32,
        media_uv_stream_receive_unsigned_cb, cb, cookie);
}

int media_uv_player_get_latency(void* handle,
    media_uv_unsigned_callback cb, void* cookie)
{
    if (!handle)
        return -EINVAL;

    return media_uv_stream_send(handle, NULL, "get_latency", NULL, 32,
        media_uv_stream_receive_unsigned_cb, cb, cookie);
}

int media_uv_player_set_looping(void* handle, int loop,
    media_uv_callback cb, void* cookie)
{
    char tmp[32];

    if (!handle)
        return -EINVAL;

    snprintf(tmp, sizeof(tmp), "%d", loop);
    return media_uv_stream_send(handle, NULL, "set_loop", tmp, 0,
        media_uv_stream_receive_cb, cb, cookie);
}

int media_uv_player_seek(void* handle, unsigned int msec,
    media_uv_callback cb, void* cookie)
{
    char tmp[32];

    if (!handle)
        return -EINVAL;

    snprintf(tmp, sizeof(tmp), "%u", msec);
    return media_uv_stream_send(handle, NULL, "seek", tmp, 0,
        media_uv_stream_receive_cb, cb, cookie);
}

int media_uv_player_set_property(void* handle, const char* target, const char* key,
    const char* value, media_uv_callback cb, void* cookie)
{
    if (!handle)
        return -EINVAL;

    return media_uv_stream_send(handle, target, key, value, 0,
        media_uv_stream_receive_cb, cb, cookie);
}

int media_uv_player_get_property(void* handle, const char* target, const char* key,
    media_uv_string_callback cb, void* cookie)
{
    if (!handle)
        return -EINVAL;

    return media_uv_stream_send(handle, target, key, NULL, 32,
        media_uv_stream_receive_string_cb, cb, cookie);
}

int media_uv_player_query(void* handle, media_uv_object_callback cb, void* cookie)
{
    MediaPlayerPriv* priv = handle;

    if (!priv || !cb)
        return -EINVAL;

    if (!priv->query) {
        priv->query = malloc(sizeof(MediaQueryPriv));
        if (!priv->query)
            return -ENOMEM;
    }

    memset(priv->query, 0, sizeof(MediaQueryPriv));
    priv->query->on_query = cb;
    priv->query->player = priv;
    priv->query->cookie = cookie;

    if (media_uv_player_get_playing(handle, media_uv_player_query_state_cb, priv->query) >= 0)
        priv->query->expected |= MEDIA_METAFLAG_STATE;

    if (media_uv_policy_get_stream_volume(priv->loop, priv->name,
            media_uv_player_query_volume_cb, priv->query)
        >= 0)
        priv->query->expected |= MEDIA_METAFLAG_VOLUME;

    if (media_uv_player_get_duration(handle, media_uv_player_query_duration_cb, priv->query) >= 0)
        priv->query->expected |= MEDIA_METAFLAG_DURATION;

    if (media_uv_player_get_position(handle, media_uv_player_query_position_cb, priv->query) >= 0)
        priv->query->expected |= MEDIA_METAFLAG_POSITION;

    if (!priv->query->expected) {
        return -EINVAL;
    }

    return 0;
}
/****************************************************************************
 * Recorder Functions
 ****************************************************************************/

static void media_uv_recorder_suggest_cb(int suggest, void* cookie)
{
    MediaFocusPriv* priv = cookie;
    MediaRecorderPriv* recorder = priv->stream;
    bool suggest_active = false;

    if (!recorder) {
        MEDIA_INFO("suggest:%d canceled\n", suggest);
        goto out;
    }

    MEDIA_INFO("%s:%p suggest:%d\n", recorder->name, recorder, suggest);

    switch (suggest) {
    case MEDIA_FOCUS_PLAY:
    case MEDIA_FOCUS_PLAY_BUT_SILENT:
    case MEDIA_FOCUS_PLAY_WITH_DUCK:
        suggest_active = true;
        media_uv_recorder_start(recorder, priv->on_play, priv->on_play_cookie);
        break;

    case MEDIA_FOCUS_STOP:
        suggest_active = false;
        media_uv_recorder_stop(recorder, NULL, NULL);
        break;

    case MEDIA_FOCUS_PAUSE:
        suggest_active = false;
        media_uv_stream_pause(priv->stream, NULL, NULL);
        break;

    case MEDIA_FOCUS_PLAY_WITH_KEEP:
        break;
    }

out:
    if (priv->on_play) {
        if (!suggest_active) /* Notify user if focus request failed. */
            priv->on_play(priv->on_play_cookie, -EPERM);

        priv->on_play = NULL;
        priv->on_play_cookie = NULL;
    }
}

void* media_uv_recorder_open(void* loop, const char* source,
    media_uv_callback on_open, void* cookie)
{
    MediaRecorderPriv* priv;

    if (source && source[0] != '\0') {
        priv = zalloc(sizeof(MediaStreamPriv) + strlen(source) + 1);
        if (!priv)
            return NULL;

        priv->name = (char*)(priv + 1);
        strcpy(priv->name, source);
    } else {
        priv = zalloc(sizeof(MediaStreamPriv));
        if (!priv)
            return NULL;
    }

    priv->loop = loop;
    priv->cookie = cookie;
    priv->on_open = on_open;
    priv->id = MEDIA_ID_RECORDER;
    priv->proxy = media_uv_connect(loop, media_get_cpuname(),
        media_uv_stream_connect_cb, priv);
    if (!priv->proxy) {
        media_uv_stream_disconnect_cb(priv, 0);
        return NULL;
    }

    return priv;
}

int media_uv_recorder_close(void* handle, media_uv_callback on_close)
{
    MediaRecorderPriv* priv = handle;
    int ret;

    if (!priv)
        return -EINVAL;

    priv->on_close = on_close;
    ret = media_uv_stream_send(priv, NULL, "close", "0", 0,
        media_uv_stream_receive_cb, media_uv_stream_close_cb, priv);

    media_uv_stream_close_pipe(handle);
    media_uv_stream_listen_clear(handle, NULL);
    media_uv_stream_abandon_focus(handle);

    if (ret < 0)
        media_uv_stream_close_cb(priv, ret);

    return ret;
}

int media_uv_recorder_listen(void* handle, media_event_callback on_event)
{
    MediaRecorderPriv* priv = handle;
    int ret;

    if (!priv || !on_event)
        return -EINVAL;

    priv->on_event = on_event;
    ret = media_uv_stream_send(priv, NULL, "set_event", NULL, 0, NULL, NULL, NULL);
    if (ret < 0)
        return ret;

    return media_uv_listen(priv->proxy, NULL, media_uv_stream_event_cb);
}

int media_uv_recorder_prepare(void* handle, const char* url, const char* options,
    media_uv_object_callback on_connection, media_uv_callback on_prepare, void* cookie)
{
    MediaRecorderPriv* priv = handle;
    const char* cpu = NULL;
    char addr[32];
    int ret = 0;

    if (!handle)
        return -EINVAL;

    if (!url || !url[0]) {
        if (!on_connection)
            return -EINVAL; /* Need to provide data pipe to caller. */

        if (priv->on_connection)
            return -EPERM; /* Double prepare is not permitted. */

        snprintf(addr, sizeof(addr), MEDIA_GRAPH_SOCKADDR_NAME, handle);
        ret = media_uv_stream_listen_init(handle, addr);
        if (ret < 0)
            return ret;

        priv->on_connection = on_connection;
        cpu = CONFIG_RPMSG_LOCAL_CPUNAME;
        url = addr;
    }

    if (options && options[0] != '\0')
        ret = media_uv_stream_send(handle, NULL, "set_options", options, 0, NULL, NULL, NULL);

    if (ret >= 0)
        ret = media_uv_stream_send(handle, cpu, "prepare", url, 0,
            media_uv_stream_receive_cb, on_prepare, cookie);

    return ret;
}

int media_uv_recorder_start_auto(void* handle, const char* scenario,
    media_uv_callback cb, void* cookie)
{
    MediaRecorderPriv* priv = handle;

    if (!priv || !scenario || !scenario[0])
        return -EINVAL;

    if (priv->focus) {
        MEDIA_WARN("%s:%p force start\n", priv->name, priv);
        return media_uv_recorder_start(priv, cb, cookie);
    }

    return media_uv_stream_request_focus(handle, scenario,
        media_uv_recorder_suggest_cb, cb, cookie);
}

int media_uv_recorder_start(void* handle, media_uv_callback cb, void* cookie)
{
    if (!handle)
        return -EINVAL;

    return media_uv_stream_send(handle, NULL, "start", NULL, 0,
        media_uv_stream_receive_cb, cb, cookie);
}

int media_uv_recorder_pause(void* handle, media_uv_callback cb, void* cookie)
{
    MediaRecorderPriv* priv = handle;

    if (!priv)
        return -EINVAL;

    media_uv_stream_abandon_focus(handle);

    return media_uv_stream_pause(handle, cb, cookie);
}

int media_uv_recorder_stop(void* handle, media_uv_callback cb, void* cookie)
{
    MediaRecorderPriv* priv = handle;

    if (!priv)
        return -EINVAL;

    media_uv_stream_close_pipe(handle);
    media_uv_stream_abandon_focus(handle);

    return media_uv_stream_send(handle, NULL, "stop", NULL, 0,
        media_uv_stream_receive_cb, cb, cookie);
}

int media_uv_recorder_set_property(void* handle, const char* target, const char* key,
    const char* value, media_uv_callback cb, void* cookie)
{
    if (!handle)
        return -EINVAL;

    return media_uv_stream_send(handle, target, key, value, 0,
        media_uv_stream_receive_cb, cb, cookie);
}

int media_uv_recorder_get_property(void* handle, const char* target, const char* key,
    media_uv_string_callback cb, void* cookie)
{
    if (!handle)
        return -EINVAL;

    return media_uv_stream_send(handle, target, key, NULL, 32,
        media_uv_stream_receive_string_cb, cb, cookie);
}

int media_uv_recorder_reset(void* handle, media_uv_callback cb, void* cookie)
{
    if (!handle)
        return -EINVAL;

    media_uv_stream_close_pipe(handle);
    media_uv_stream_abandon_focus(handle);

    return media_uv_stream_send(handle, NULL, "reset", NULL, 0,
        media_uv_stream_receive_cb, cb, cookie);
}

static void media_uv_recorder_take_picture_close_cb(void* cookie, int ret)
{
    MediaTakePicPriv* priv = cookie;

    if (priv->on_complete)
        priv->on_complete(priv->cookie, priv->err_code);

    free(priv);
}

static int media_uv_recorder_take_picture_close(void* handle, bool send_close)
{
    MediaRecorderPriv* priv = handle;
    int ret = 0;

    if (!priv)
        return -EINVAL;

    priv->on_close = media_uv_recorder_take_picture_close_cb;

    if (send_close)
        ret = media_uv_stream_send(priv, NULL, "close", "1", 0, NULL, NULL, NULL);

    if (ret >= 0)
        ret = media_uv_disconnect(priv->proxy, media_uv_stream_disconnect_cb);

    return ret;
}

static void media_uv_recorder_take_picture_on_event(void* cookie, int event, int ret,
    const char* extra)
{
    MediaTakePicPriv* priv = cookie;

    MEDIA_INFO("%s, take pic, event %d, ret %d, line %d\n",
        __func__, event, ret, __LINE__);

    if (ret < 0 || event == MEDIA_EVENT_STOPPED) {
        if (!priv->err_code) {
            priv->err_code = ret;
            media_uv_recorder_take_picture_close(priv->handle, true);
        }
    }
}

static void media_uv_recorder_take_picture_on_open(void* cookie, int ret)
{
    MediaTakePicPriv* priv = cookie;
    if (ret < 0 && !priv->err_code) {
        priv->err_code = ret;
        media_uv_recorder_take_picture_close(priv->handle, false);
    }
}

int media_uv_recorder_take_picture(void* loop, char* params, char* filename,
    size_t number, media_uv_callback on_complete, void* cookie)
{
    MediaTakePicPriv* priv;
    char option[32];
    int ret;

    if (!number || number > INT_MAX)
        return -EINVAL;

    priv = zalloc(sizeof(MediaTakePicPriv));
    if (!priv)
        return -ENOMEM;

    priv->on_complete = on_complete;
    priv->cookie = cookie;

    priv->handle = media_uv_recorder_open(loop, params,
        media_uv_recorder_take_picture_on_open, priv);
    if (!priv->handle) {
        free(priv);
        return -EINVAL;
    }

    ret = media_uv_recorder_listen(priv->handle, media_uv_recorder_take_picture_on_event);
    if (ret < 0)
        goto err;

    snprintf(option, sizeof(option), "total_number=%zu", number);
    ret = media_uv_recorder_prepare(priv->handle, filename, option, NULL, NULL, NULL);
    if (ret < 0)
        goto err;

    ret = media_uv_recorder_start(priv->handle, NULL, NULL);
    if (ret < 0)
        goto err;

    return ret;

err:
    priv->err_code = ret;
    media_uv_recorder_take_picture_close(priv->handle, false);
    return 0;
}
