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
 * @brief Bluetooth PAN adapter.
 *
 ****************************************************************************/
#define LOG_TAG "sal_bes_pan"

/****************************** header include ********************************/
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "adapter_internel.h"
#include "bt_status.h"
#include "bluetooth.h"
#include "bt_utils.h"
#include "bt_device.h"
#include "utils/log.h"
#include "sal_pan_interface.h"
#include "sal_bes.h"

#include "api/bth_api_pan.h"

typedef struct {
    bool in_use;
    bt_address_t remote_addr;
    pan_role_t local_role;
    pan_role_t remote_role;
    profile_connection_state_t state;
} sal_pan_dev_map_t;

typedef struct {
    bool initialized;
    int max_connections;
    pan_role_t current_role;
    int active_connections;
    sal_pan_dev_map_t dev_map[BT_DEVICE_NUM];
    bt_address_t pending_addr;
    uint8_t pending_src_role;
    uint8_t pending_dst_role;
    bool is_pending;
} sal_pan_ctx_t;

static sal_pan_ctx_t g_sal_pan_ctx = {0};

/**
 * Convert bt_address_t to bth_address_t
 */
static void convert_bt_addr_to_bth(const bt_address_t* src, bth_address_t* dst)
{
    if (src && dst)
    {
        memcpy(dst->address, src->addr, BT_ADDRESS_LEN);
    }
}

/**
 * Convert bth_address_t to bt_address_t
 */
static void convert_bth_addr_to_bt(const bth_address_t* src, bt_address_t* dst)
{
    if (src && dst)
    {
        memcpy(dst->addr, src->address, BT_ADDRESS_LEN);
    }
}

/**
 * Find device mapping by bt_address_t
 */
static sal_pan_dev_map_t* find_dev_map_by_addr(const bt_address_t* addr)
{
    if (!addr) return NULL;

    for (int i = 0; i < BT_DEVICE_NUM; i++)
    {
        if (g_sal_pan_ctx.dev_map[i].in_use &&
            memcmp(g_sal_pan_ctx.dev_map[i].remote_addr.addr, addr->addr, BT_ADDRESS_LEN) == 0)
        {
            return &g_sal_pan_ctx.dev_map[i];
        }
    }
    return NULL;
}

/**
 * Find free device mapping slot
 */
static sal_pan_dev_map_t* get_free_dev_map(void)
{
    for (int i = 0; i < BT_DEVICE_NUM; i++)
    {
        if (!g_sal_pan_ctx.dev_map[i].in_use)
        {
            return &g_sal_pan_ctx.dev_map[i];
        }
    }
    return NULL;
}

/**
 * Find device mapping by device_id
 */
static sal_pan_dev_map_t* find_dev_map_by_id(uint8_t device_id)
{
    if (device_id < BT_DEVICE_NUM)
    {
        return &g_sal_pan_ctx.dev_map[device_id];
    }
    return NULL;
}

/**
 * Map BTH status to SAL status
 */
static bt_status_t map_bth_status_to_sal(bth_bt_status_t bth_status)
{
    switch (bth_status)
    {
        case BTH_STATUS_SUCCESS: return BT_STATUS_SUCCESS;
        case BTH_STATUS_NOT_READY: return BT_STATUS_NOT_READY;
        case BTH_STATUS_PARM_INVALID: return BT_STATUS_PARM_INVALID;
        case BTH_STATUS_NOMEM: return BT_STATUS_NOMEM;
        case BTH_STATUS_DONE: return BT_STATUS_DONE;
        case BTH_STATUS_FAIL:
        default: return BT_STATUS_FAIL;
    }
}

/**
 * Internal callback to handle BTH PAN events
 */
static void sal_pan_internal_callback(uint8_t device_id, struct btif_pan_channel_t *pan_ctl,
                                      const struct pan_callback_parm_t *param)
{
    if (!param)
    {
        BT_LOGE("NULL event parameter");
        return;
    }

    BT_LOGD("PAN callback: device_id=%d, event=%d, state=%d, error=%d",
          device_id, param->pan_event, param->pan_state, param->error_code);

    sal_pan_dev_map_t* dev_map = find_dev_map_by_id(device_id);

    switch (param->pan_event)
    {
        case PAN_EVENT_CHANNEL_OPENED:
        {
            bool success = (param->error_code == PAN_SUCCESS);
            if (success)
            {
                // Handle successful connection
                if (g_sal_pan_ctx.is_pending && device_id < BT_DEVICE_NUM)
                {
                    // New connection - use pending info
                    sal_pan_dev_map_t* map = get_free_dev_map();
                    if (map)
                    {
                        memcpy(&map->remote_addr, &g_sal_pan_ctx.pending_addr, sizeof(bt_address_t));
                        map->local_role = (pan_role_t)g_sal_pan_ctx.pending_src_role;
                        map->remote_role = (pan_role_t)g_sal_pan_ctx.pending_dst_role;
                        map->state = PROFILE_STATE_CONNECTED;
                        map->in_use = true;

                        g_sal_pan_ctx.active_connections++;
                        dev_map = map;
                    }

                    // Reset pending state
                    g_sal_pan_ctx.is_pending = false;
                    memset(&g_sal_pan_ctx.pending_addr, 0, sizeof(bt_address_t));
                }
                else if (dev_map)
                {
                    // Existing connection updated
                    dev_map->state = PROFILE_STATE_CONNECTED;
                }

                if (dev_map)
                {
                    pan_on_connection_state_changed(&dev_map->remote_addr,
                                                   dev_map->remote_role,
                                                   dev_map->local_role,
                                                   PROFILE_STATE_CONNECTED);
                }
            }
            else
            {
                // Connection failed
                if (g_sal_pan_ctx.is_pending)
                {
                    pan_on_connection_state_changed(&g_sal_pan_ctx.pending_addr,
                                                   (pan_role_t)g_sal_pan_ctx.pending_dst_role,
                                                   (pan_role_t)g_sal_pan_ctx.pending_src_role,
                                                   PROFILE_STATE_DISCONNECTED);
                    g_sal_pan_ctx.is_pending = false;
                }
                BT_LOGE("Connection failed for device %d: %d", device_id, param->error_code);
            }
            break;
        }

        case PAN_EVENT_CHANNEL_CLOSED:
        {
            if (dev_map)
            {
                pan_on_connection_state_changed(&dev_map->remote_addr,
                                               dev_map->remote_role,
                                               dev_map->local_role,
                                               PROFILE_STATE_DISCONNECTED);

                // Clean up mapping
                dev_map->in_use = false;
                dev_map->state = PROFILE_STATE_DISCONNECTED;
                if (g_sal_pan_ctx.active_connections > 0)
                {
                    g_sal_pan_ctx.active_connections--;
                }
            }
            break;
        }

        case PAN_EVENT_ETHERNET_DATA_IND:
        {
            if (!dev_map || !param->data || param->length == 0)
            {
                BT_LOGE("Invalid data event for device %d", device_id);
                return;
            }

            // Get local address for source MAC
            bt_address_t local_addr;
            adapter_get_address(&local_addr);

            // Forward data to upper layer
            pan_on_data_received(&dev_map->remote_addr,
                                param->protocol,
                                (uint8_t*)dev_map->remote_addr.addr,
                                (uint8_t*)local_addr.addr,
                                param->data,
                                param->length);
            break;
        }

        default:
            BT_LOGW("Unhandled PAN event: %d", param->pan_event);
            break;
    }
}

bt_status_t bt_sal_pan_init(int max_connections, pan_role_t role)
{
    if (max_connections <= 0 || max_connections > BT_DEVICE_NUM)
    {
        BT_LOGE("Invalid max_connections: %d", max_connections);
        return BT_STATUS_PARM_INVALID;
    }

    if (role != PAN_ROLE_PANU || role != PAN_ROLE_NAP)
    {
        BT_LOGE("Invalid PAN role: %d", role);
        return BT_STATUS_PARM_INVALID;
    }

    if (g_sal_pan_ctx.initialized)
    {
        BT_LOGW("Already initialized");
        return BT_STATUS_SUCCESS;
    }

    BT_LOGI("Initializing: max_conn=%d, role=%d", max_connections, role);

    bth_pan_init();
    bth_pan_set_callback(sal_pan_internal_callback);

    // Initialize context
    g_sal_pan_ctx.initialized = true;
    g_sal_pan_ctx.max_connections = max_connections;
    g_sal_pan_ctx.current_role = role;
    g_sal_pan_ctx.active_connections = 0;
    g_sal_pan_ctx.is_pending = false;

    for (int i = 0; i < BT_DEVICE_NUM; i++)
    {
        g_sal_pan_ctx.dev_map[i].in_use = false;
        g_sal_pan_ctx.dev_map[i].state = PROFILE_STATE_DISCONNECTED;
        memset(&g_sal_pan_ctx.dev_map[i].remote_addr, 0, sizeof(bt_address_t));
    }

    BT_LOGI("Initialization completed");
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_pan_cleanup(void)
{
    if (!g_sal_pan_ctx.initialized)
    {
        BT_LOGW("Not initialized");
        return BT_STATUS_SUCCESS;
    }

    BT_LOGI("Cleaning up");

    // Disconnect all active connections
    bth_pan_disconnect_all();
    bth_pan_cleanup();

    // Reset context
    memset(&g_sal_pan_ctx, 0, sizeof(sal_pan_ctx_t));

    BT_LOGI("Cleanup completed");
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_pan_connect(const bt_address_t* addr, uint8_t dst_role, uint8_t src_role)
{
    if (!g_sal_pan_ctx.initialized)
    {
        BT_LOGE("Not initialized");
        return BT_STATUS_NOT_READY;
    }

    if (!addr)
    {
        BT_LOGE("Invalid address");
        return BT_STATUS_PARM_INVALID;
    }

    if (g_sal_pan_ctx.active_connections >= g_sal_pan_ctx.max_connections)
    {
        BT_LOGE("Max connections reached: %d", g_sal_pan_ctx.max_connections);
        return BT_STATUS_NOMEM;
    }

    if (g_sal_pan_ctx.is_pending)
    {
        BT_LOGE("Connection pending");
        return BT_STATUS_BUSY;
    }

    if (find_dev_map_by_addr(addr))
    {
        BT_LOGW("Device already connected: " BT_ADDR_STR, BT_ADDR_ARG(addr));
        return BT_STATUS_DONE;
    }

    bth_address_t bth_addr;
    convert_bt_addr_to_bth(addr, &bth_addr);

    bth_bt_status_t bth_status = bth_pan_connect(&bth_addr, (int)src_role, (int)dst_role);
    if (bth_status != BTH_STATUS_SUCCESS)
    {
        BT_LOGE("Connect failed: %d", bth_status);
        return map_bth_status_to_sal(bth_status);
    }

    // Store pending connection info
    memcpy(&g_sal_pan_ctx.pending_addr, addr, sizeof(bt_address_t));
    g_sal_pan_ctx.pending_src_role = src_role;
    g_sal_pan_ctx.pending_dst_role = dst_role;
    g_sal_pan_ctx.is_pending = true;

    BT_LOGI("Connect initiated: " BT_ADDR_STR " (src_role=%d, dst_role=%d)",
            BT_ADDR_ARG(addr), src_role, dst_role);
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_pan_disconnect(const bt_address_t* addr)
{
    if (!g_sal_pan_ctx.initialized)
    {
        BT_LOGE("Not initialized");
        return BT_STATUS_NOT_READY;
    }

    if (!addr)
    {
        BT_LOGE("Invalid address");
        return BT_STATUS_PARM_INVALID;
    }

    sal_pan_dev_map_t* dev_map = find_dev_map_by_addr(addr);
    if (!dev_map)
    {
        BT_LOGE("Device not found: " BT_ADDR_STR, BT_ADDR_ARG(addr));
        return BT_STATUS_FAIL;
    }

    bth_address_t bth_addr;
    convert_bt_addr_to_bth(addr, &bth_addr);

    bth_bt_status_t bth_status = bth_pan_disconnect(&bth_addr);
    if (bth_status != BTH_STATUS_SUCCESS)
    {
        BT_LOGE("Disconnect failed: %d", bth_status);
        return map_bth_status_to_sal(bth_status);
    }

    BT_LOGI("Disconnect initiated: " BT_ADDR_STR, BT_ADDR_ARG(addr));
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_pan_write(const bt_address_t* addr, uint16_t protocol,
                             const uint8_t* dst_addr, const uint8_t* src_addr,
                             const uint8_t* data, uint16_t length)
{
    if (!g_sal_pan_ctx.initialized)
    {
        BT_LOGE("Not initialized");
        return BT_STATUS_NOT_READY;
    }

    if (!addr || !data || length == 0)
    {
        BT_LOGE("Invalid parameters");
        return BT_STATUS_PARM_INVALID;
    }

    sal_pan_dev_map_t* dev_map = find_dev_map_by_addr(addr);
    if (!dev_map || dev_map->state != PROFILE_STATE_CONNECTED)
    {
        BT_LOGE("Device not connected: " BT_ADDR_STR, BT_ADDR_ARG(addr));
        return BT_STATUS_FAIL;
    }

    bth_address_t bth_addr;
    convert_bt_addr_to_bth(addr, &bth_addr);

    bth_bt_status_t bth_status = bth_pan_send_data(&bth_addr, (pan_protocol_type)protocol, data, length);
    if (bth_status != BTH_STATUS_SUCCESS)
    {
        BT_LOGE("Send failed: %d", bth_status);
        return map_bth_status_to_sal(bth_status);
    }

    BT_LOGD("Data sent: " BT_ADDR_STR " (proto=0x%04X, len=%d)",
          BT_ADDR_ARG(addr), protocol, length);
    return BT_STATUS_SUCCESS;
}