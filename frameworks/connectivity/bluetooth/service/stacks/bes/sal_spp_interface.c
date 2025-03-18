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
 * @brief xxx.
 *
 ****************************************************************************/
#define LOG_TAG "sal_bes_av"

/****************************** header include ********************************/
#include <stdint.h> // For uint8_t type
#include "bt_status.h"
#include "bluetooth.h"
#include "utils/log.h"

#include "sal_spp_interface.h"
#include "bt_utils.h"
#include "sal_bes.h"

#include "api/bth_api_spp.h"
/***************************** external declaration *****************************/

/***************************** macro defination *******************************/
#define STACK_SVR_PORT(scn) (((scn << 1) & 0x3E) + 1)
#define STACK_CONN_PORT(scn, conn_id, accept) \
    ((conn_id << 6) + (accept ? STACK_SVR_PORT(scn) : ((scn << 1) & 0x3E)))
#define SERVICE_SCN(port) ((port & 0x3E) >> 1)
#define SERVICE_CONN_ID(conn_port) (conn_port >> 6)
/*****************************  type defination ********************************/
typedef struct {
    struct list_node node;
    uint8_t* data;
    int len;
} spp_tx_data_t;

/*****************************  variable defination *****************************/

/*****************************  function declaration ****************************/

static void bes_sal_spp_conn_state_changed_cb(const bth_address_t *bd_addr, uint16_t scn, uint8_t state)
{
    if(state == true)
    {
        spp_on_connection_state_changed(bd_addr, STACK_SVR_PORT(scn), PROFILE_STATE_CONNECTED);
    }
    else
    {
        spp_on_connection_state_changed(bd_addr, STACK_SVR_PORT(scn), PROFILE_STATE_DISCONNECTED);
    }
}

static void bes_sal_spp_data_sent_cb(uint16_t scn, uint8_t *data, uint16_t data_len)
{
    spp_on_data_sent(STACK_SVR_PORT(scn), data, data_len, data_len);
}

static void bes_sal_spp_data_receive_callback(const bth_address_t *bd_addr, uint16_t scn, uint8_t *data, uint16_t data_len)
{
    spp_on_data_received(bd_addr, STACK_SVR_PORT(scn), data, data_len);
}

static void bes_sal_spp_connect_req_callback(const bth_address_t *bd_addr, uint16_t port)
{
    spp_on_server_recieve_connect_request(bd_addr, port);
}

static void bes_sal_spp_update_mfs_callback(uint16_t port, uint16_t len)
{
    spp_on_connection_mfs_update(port, len);
}

static bth_spp_callback bt_sal_spp_callbacks = {
    .conn_state_changed_cb = bes_sal_spp_conn_state_changed_cb,
    .data_sent_cb = bes_sal_spp_data_sent_cb,
    .data_receive_cb = bes_sal_spp_data_receive_callback,
    .conn_req_cb = bes_sal_spp_connect_req_callback,
    .update_mfs_cb=bes_sal_spp_update_mfs_callback,
};

bt_status_t bt_sal_spp_init()
{
    int status = BT_STATUS_FAIL;
    status = bth_spp_init(&bt_sal_spp_callbacks);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_spp_cleanup()
{
    int status = BT_STATUS_FAIL;
    status = bth_spp_cleanup();
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_spp_server_start(uint16_t scn, bt_uuid_t *uuid, uint8_t max_connection)
{
    bth_uuid_t bth_uuid;
    if(uuid->type == BT_UUID16_TYPE)
    {
        bth_uuid = uuid_from_16bit(uuid->val.u16);
    }
    else if(uuid->type == BT_UUID32_TYPE)
    {
        bth_uuid = uuid_from_32bit(uuid->val.u32);
    }
    else
    {
        bth_uuid = uuid_from_128bit_LE(uuid->val.u128);
    }
    bth_spp_server_start(scn, &bth_uuid, max_connection);
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_spp_server_stop(uint16_t scn)
{
    bth_bt_status_t status;
    status = bth_spp_server_stop(scn);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_spp_connect(const bt_address_t *addr, uint16_t conn_port, bt_uuid_t *uuid)
{
    bth_bt_status_t status;
    bth_uuid_t bth_uuid;
    if(uuid->type == BT_UUID16_TYPE)
    {
        bth_uuid = uuid_from_16bit(uuid->val.u16);
    }
    else if(uuid->type == BT_UUID32_TYPE)
    {
        bth_uuid = uuid_from_32bit(uuid->val.u32);
    }
    else
    {
        bth_uuid = uuid_from_128bit_LE(uuid->val.u128);
    }
    status = bth_spp_connect((bth_address_t *)addr, conn_port, &bth_uuid);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_spp_disconnect(uint16_t conn_port)
{
    bth_bt_status_t status;
    status = bth_spp_disconnect(conn_port);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_spp_data_received_response(uint16_t conn_port, uint8_t *buf)
{
    bth_spp_data_received_response(conn_port, *buf);
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_spp_write(uint16_t conn_port, uint8_t* buf, uint16_t size)
{
    int status;
    status = bth_spp_write(conn_port, buf, size);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_spp_connect_request_reply(const bt_address_t *addr, uint16_t port, bool accept)
{
    bth_spp_connect_request_reply((bth_address_t *)addr, port, accept);
    return BT_STATUS_SUCCESS;
}

