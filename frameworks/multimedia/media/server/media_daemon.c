/****************************************************************************
 * frameworks/media/server/media_daemon.c
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

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "media_common.h"
#include "media_server.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MAX_POLLFDS CONFIG_MEDIA_SERVER_MAX_POLLFDS
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef struct MediaPriv {
    int idx[MAX_POLLFDS];
    struct pollfd fds[MAX_POLLFDS];
    void* ctx[MAX_POLLFDS];
} MediaPriv;

typedef void* (*media_create)(void* param);
typedef int (*media_get_pollfds)(void* handle, struct pollfd* fds,
    void** cookies, int count);
typedef int (*media_poll_available)(void* handle, struct pollfd* fds,
    void* cookies);
typedef int (*media_run_once)(void* handle);
typedef int (*media_destroy)(void* handle);

typedef struct MediaPoll {
    const char* name;
    void* handle;
    void* param;
    media_create create;
    media_get_pollfds get;
    media_poll_available available;
    media_run_once run_once;
    media_destroy destroy;
} MediaPoll;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static MediaPoll g_media[] = {
#ifdef CONFIG_MEDIA_FOCUS
    {
        "media_focus",
        NULL,
        CONFIG_MEDIA_SERVER_CONFIG_PATH "media_focus.conf",
        media_focus_create,
        NULL,
        NULL,
        NULL,
        media_focus_destroy,
    },
#endif
#ifdef CONFIG_LIB_FFMPEG
    {
        "media_graph",
        NULL,
        CONFIG_MEDIA_SERVER_CONFIG_PATH "graph.conf",
        media_graph_create,
        media_graph_get_pollfds,
        media_graph_poll_available,
        media_graph_run_once,
        media_graph_destroy,
    },
    {
        "media_session",
        NULL,
        NULL,
        media_session_create,
        NULL,
        NULL,
        NULL,
        media_session_destroy,
    },
#endif
#ifdef SMF_MEDIA
    {
        "media_session",
        NULL,
        NULL,
        media_session_create,
        NULL,
        NULL,
        NULL,
        media_session_destroy,
    },
#endif
#ifdef CONFIG_LIB_PFW
    {
        "media_policy",
        NULL,
        (const char*[]) {
            CONFIG_MEDIA_SERVER_CONFIG_PATH "criteria.txt",
            CONFIG_MEDIA_SERVER_CONFIG_PATH "settings.pfw" },
        media_policy_create,
        NULL,
        NULL,
        NULL,
        media_policy_destroy,
    },
#endif
    {
        "media_server",
        NULL,
        media_stub_onreceive,
        media_server_create,
        media_server_get_pollfds,
        media_server_poll_available,
        NULL,
        media_server_destroy,
    },
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void* media_get_handle(const char* name)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(g_media); i++) {
        if (!strcmp(name, g_media[i].name))
            return g_media[i].handle;
    }

    return NULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void* media_get_focus(void)
{
    return media_get_handle("media_focus");
}

void* media_get_graph(void)
{
    return media_get_handle("media_graph");
}

void* media_get_policy(void)
{
    return media_get_handle("media_policy");
}

void* media_get_session(void)
{
    return media_get_handle("media_session");
}

void* media_get_server(void)
{
    return media_get_handle("media_server");
}

int main(int argc, char* argv[])
{
    MediaPriv* priv;
    int ret, n, i;
    priv = malloc(sizeof(MediaPriv));
    if (!priv)
        return -ENOMEM;

    for (i = 0; i < ARRAY_SIZE(g_media); i++) {
        g_media[i].handle = g_media[i].create(g_media[i].param);
        if (!g_media[i].handle) {
            free(priv);
            MEDIA_ERR("%s create failed\n", g_media[i].name);
            return -EINVAL;
        }
    }

    while (1) {
        for (n = i = 0; i < ARRAY_SIZE(g_media); i++) {
            if (!g_media[i].get)
                continue;

            ret = g_media[i].get(g_media[i].handle, &priv->fds[n],
                &priv->ctx[n], MAX_POLLFDS - n);
            if (ret < 0) {
                MEDIA_ERR("%s get_pollfds failed %d\n", g_media[i].name, ret);
                continue;
            }

            while (ret--)
                priv->idx[n++] = i;
        }

        assert(n > 0 && n < MAX_POLLFDS);

        poll(priv->fds, n, -1);

        for (i = 0; i < n; i++) {
            if (!priv->fds[i].revents)
                continue;

            ret = g_media[priv->idx[i]].available(g_media[priv->idx[i]].handle,
                &priv->fds[i], priv->ctx[i]);
            if (ret < 0 && ret != -EAGAIN && ret != -EPIPE)
                MEDIA_ERR("%s poll_available failed %d\n",
                    g_media[priv->idx[i]].name, ret);
        }

        for (i = 0; i < ARRAY_SIZE(g_media); i++) {
            if (!g_media[i].run_once)
                continue;

            ret = g_media[i].run_once(g_media[i].handle);
            if (ret < 0)
                MEDIA_ERR("%s run_once failed %d\n", g_media[i].name, ret);
        }
    }

    for (i = 0; i < ARRAY_SIZE(g_media); i++)
        g_media[i].destroy(g_media[i].handle);

    free(priv);
    return 0;
}
