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
#define LOG_TAG "sal_bes_gatts"

/****************************** header include ********************************/
#include <stdint.h> // For uint8_t, uint16_t, uint32_t types
#include <stdbool.h> // For bool type
#include "adapter_internel.h"
#include "sal_interface.h"
#include "sal_bes.h"
#include "bt_status.h"
#include "bluetooth.h"
#include "utils/log.h"

#include "sal_gatt_server_interface.h"

#include "api/bth_api_gatt_server.h"

/***************************** external declaration *****************************/

/***************************** macro defination *******************************/

/*****************************  type defination ********************************/

typedef struct
{
    int char_count;
    bth_gatt_db_element_t* service_db;
}gatts_service_info_t;

typedef struct
{
    int conn_id;
    bth_address_t addr;
}gatts_device_t;

typedef struct
{
    uint16_t  request_id;
    uint8_t* data;
    uint16_t  datalen;
}gatts_atts_data_t;

typedef struct
{
    int server_if;
    gatts_device_t device[CONFIG_BLUETOOTH_GATTS_MAX_CONNECTIONS];
    gatts_service_info_t service_info[GATT_MAX_SR_PROFILES];
    gatts_atts_data_t atts_data[GATT_MAX_SR_PROFILES];
}gatts_evn_t;

/*****************************  variable defination *****************************/
static gatts_evn_t sal_gatts_evn = {0};

/*****************************  function declaration ****************************/

static void bes_sal_gatts_set_device(uint16_t conn_id, bth_address_t* addr)
{
    for(int i = 0; i < CONFIG_BLUETOOTH_GATTS_MAX_CONNECTIONS; i++)
    {
        if(sal_gatts_evn.device[i].conn_id == 0 || sal_gatts_evn.device[i].conn_id == 0xffff)
        {
            BT_LOGE("[%s] device conn_id %d",__func__, conn_id);
            sal_gatts_evn.device[i].conn_id = conn_id;
            memcpy((uint8_t*)&sal_gatts_evn.device[i].addr, (uint8_t*)addr, BT_ADDR_LENGTH);
            break;
        }
    }
    return;
}

static void bes_sal_gatts_clear_device(uint16_t conn_id)
{
    for(int i = 0; i < CONFIG_BLUETOOTH_GATTS_MAX_CONNECTIONS; i++)
    {
        if(sal_gatts_evn.device[i].conn_id == conn_id)
        {
            BT_LOGE("[%s] device conn_id %d",__func__, conn_id);
            sal_gatts_evn.device[i].conn_id = 0;
            memset((uint8_t*)&sal_gatts_evn.device[i].addr, 0, BT_ADDR_LENGTH);
            break;
        }
    }
    return;
}

static bth_address_t* bes_sal_gatts_get_addr_by_conn_id(uint16_t conn_id)
{
    bth_address_t* addr = NULL;
    for(int i = 0; i < CONFIG_BLUETOOTH_GATTS_MAX_CONNECTIONS; i++)
    {
        if(sal_gatts_evn.device[i].conn_id == conn_id)
        {
            addr = &sal_gatts_evn.device[i].addr;
            break;
        }
    }
    return addr;
}

static uint16_t bes_sal_gatts_get_conn_id_by_addr(bth_address_t* addr)
{
    uint16_t conn_id = 0;
    for(int i = 0; i < CONFIG_BLUETOOTH_GATTS_MAX_CONNECTIONS; i++)
    {
        if(memcmp((uint8_t*)&sal_gatts_evn.device[i].addr, (uint8_t*)addr, BT_ADDR_LENGTH) == 0 )
        {
            conn_id = sal_gatts_evn.device[i].conn_id;
            break;
        }
    }
    return conn_id;
}

static uint16_t bes_sal_gatts_get_request_id_by_handle(uint16_t handle)
{
    uint16_t request_id = 0;
    for(int i = 0; i < GATT_MAX_SR_PROFILES; i++)
    {
        if(sal_gatts_evn.service_info[i].service_db)
        {
            for (int index = 0; index < sal_gatts_evn.service_info[i].char_count; index++)
            {
                if(sal_gatts_evn.service_info[i].service_db[index].attribute_handle == handle)
                {
                    request_id = sal_gatts_evn.service_info[i].service_db[index].id;
                    break;
                }
            }
        }
    }
    return request_id;
}

static uint16_t bes_sal_gatts_get_handle_by_request_id(uint16_t request_id)
{
    uint16_t handle = 0;
    for(int i = 0; i < GATT_MAX_SR_PROFILES; i++)
    {
        if(sal_gatts_evn.service_info[i].service_db)
        {
            for (int index = 0; index < sal_gatts_evn.service_info[i].char_count; index++)
            {
                if(sal_gatts_evn.service_info[i].service_db[index].id == request_id)
                {
                    handle = sal_gatts_evn.service_info[i].service_db[index].attribute_handle;
                    break;
                }
            }
        }
    }
    return handle;
}

static void bes_sal_gatts_reg_server_cb(int status, int server_if,
                                         const bth_uuid_t* app_uuid)
{
    BT_LOGE("[%s][%d] server_if %d",__func__, __LINE__, server_if);
    sal_gatts_evn.server_if = server_if;
}

static void bes_sal_gatts_conn_cb(int conn_id, int server_if, const bth_address_t* bda)
{
    bes_sal_gatts_set_device(conn_id, bda);
    if_gatts_on_connection_state_changed((bt_address_t*)bda,PROFILE_STATE_CONNECTED);
}

static void bes_sal_gatts_disconn_cb(int conn_id, int server_if, const bth_address_t* bda)
{
    bes_sal_gatts_clear_device(conn_id);
    if_gatts_on_connection_state_changed((bt_address_t*)bda,PROFILE_STATE_DISCONNECTED);
}

static void bes_sal_gatts_service_added_cb(int status, int server_if,
                                       const bth_gatt_db_element_t* service,
                                       int service_count)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
    for(int i = 0; i < GATT_MAX_SR_PROFILES; i++)
    {
        if(sal_gatts_evn.service_info[i].service_db && uuid_equal(&sal_gatts_evn.service_info[i].service_db->uuid, &service->uuid))
        {
            if(status != BT_STATUS_SUCCESS)
            {
                BT_LOGE("[%s][%d]",__func__, __LINE__);
                free(sal_gatts_evn.service_info[i].service_db);
                break;
            }
            for (int index = 0; index < sal_gatts_evn.service_info[i].char_count; index++)
            {
                sal_gatts_evn.service_info[i].service_db[index].attribute_handle = service[index].attribute_handle;
                BT_LOGE("[%s][handle] %d",__func__, sal_gatts_evn.service_info[i].service_db[index].attribute_handle);
                BT_LOGE("[%s][id] %d",__func__, sal_gatts_evn.service_info[i].service_db[index].id);
            }
        }
    }
}

static void bes_sal_gatts_service_stopped_cb(int status, int server_if,
                                         int srvc_handle)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
}

static void bes_sal_gatts_service_deleted_cb(int status, int server_if,
                                         int srvc_handle)
{
    for(int i = 0; i < GATT_MAX_SR_PROFILES; i++)
    {
        if(sal_gatts_evn.service_info[i].service_db && sal_gatts_evn.service_info[i].service_db->attribute_handle == srvc_handle)
        {
            free(sal_gatts_evn.service_info[i].service_db);
        }
    }
}

static void bes_sal_gatts_req_read_char_cb(int conn_id, int trans_id,
                                  const bth_address_t* bda, int attr_handle,
                                  int offset, bool is_long)
{
    uint16_t element_id = bes_sal_gatts_get_request_id_by_handle(attr_handle);
    if_gatts_on_received_element_read_request((bt_address_t*)bda, trans_id, element_id);
}

static void bes_sal_gatts_req_read_desc_cb(int conn_id, int trans_id,
                                  const bth_address_t* bda, int attr_handle,
                                  int offset, bool is_long)
{
    uint16_t element_id = bes_sal_gatts_get_request_id_by_handle(attr_handle);
    if_gatts_on_received_element_read_request((bt_address_t*)bda, trans_id, element_id);
}

static void bes_sal_gatts_req_write_char_cb(int conn_id, int trans_id,
                                   const bth_address_t* bda, int attr_handle,
                                   int offset, bool need_rsp, bool is_prep,
                                   const uint8_t* value, int length)
{
    uint16_t element_id = bes_sal_gatts_get_request_id_by_handle(attr_handle);
    if_gatts_on_received_element_write_request((bt_address_t*)bda,trans_id,element_id,value,offset,length);
}

static void bes_sal_gatts_req_write_desc_cb(int conn_id, int trans_id,
                                   const bth_address_t* bda, int attr_handle,
                                   int offset, bool need_rsp, bool is_prep,
                                   const uint8_t* value, int length)
{
    uint16_t element_id = bes_sal_gatts_get_request_id_by_handle(attr_handle);
    if_gatts_on_received_element_write_request((bt_address_t*)bda,trans_id,element_id,value,offset,length);
}

static void bes_sal_gatts_req_exec_write_cb(int conn_id, int trans_id,
                                        const bth_address_t* bda,
                                        int exec_write)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
}

static void bes_sal_gatts_rsp_confirm_cb(int status, int handle)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
}

static void bes_sal_gatts_ind_sent_cb(int conn_id, int status)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
}

static void bes_sal_gatts_congestion_cb(int conn_id, bool congested)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
}

static void bes_sal_gatts_mtu_changed_cb(int conn_id, int mtu)
{
    BT_LOGD("Updated MTU: mtu: %d bytes\n", mtu);
    bt_address_t* addr = (bt_address_t*)bes_sal_gatts_get_addr_by_conn_id(conn_id);
    if_gatts_on_mtu_changed(addr, mtu);
}

static void bes_sal_gatts_phy_updated_cb(int conn_id, uint8_t tx_phy,
                                     uint8_t rx_phy, uint8_t status)
{
    bt_address_t* addr = (bt_address_t*)bes_sal_gatts_get_addr_by_conn_id(conn_id);
    if_gatts_on_phy_updated(addr,tx_phy,rx_phy,status);
}

static void bes_sal_gatts_conn_updated_cb(int conn_id, uint16_t interval,
                                      uint16_t latency, uint16_t timeout,
                                      uint8_t status)
{
    bt_address_t* addr = (bt_address_t*)bes_sal_gatts_get_addr_by_conn_id(conn_id);
    if_gatts_on_connection_parameter_changed(addr,interval,latency,timeout);
}

static void bes_sal_gatts_subrate_chg_cb(int conn_id, uint16_t subrate_factor,
                                     uint16_t latency, uint16_t cont_num,
                                     uint16_t timeout, uint8_t status)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
}

static void bes_sal_gatts_read_phy_cb(const bth_address_t* addr, uint8_t tx_phy,
                                  uint8_t rx_phy, uint8_t status)
{
    if_gatts_on_phy_read((bt_address_t *)addr, tx_phy, rx_phy);
}

static void bes_sal_gatts_read_rssi_cb(int server, const bth_address_t *bda,
                                         int rssi, int status)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
}

gatts_server_callbacks_t bes_sal_gatts =
{
    .register_server_cb = bes_sal_gatts_reg_server_cb,
    .open_cb            = bes_sal_gatts_conn_cb,
    .close_cb           = bes_sal_gatts_disconn_cb,
    .service_added_cb   = bes_sal_gatts_service_added_cb,
    .service_stopped_cb = bes_sal_gatts_service_stopped_cb,
    .service_deleted_cb = bes_sal_gatts_service_deleted_cb,
    .req_read_char_cb   = bes_sal_gatts_req_read_char_cb,
    .req_read_desc_cb   = bes_sal_gatts_req_read_desc_cb,
    .req_write_char_cb  = bes_sal_gatts_req_write_char_cb,
    .req_write_desc_cb  = bes_sal_gatts_req_write_desc_cb,
    .req_exec_write_cb  = bes_sal_gatts_req_exec_write_cb,
    .rsp_confirm_cb     = bes_sal_gatts_rsp_confirm_cb,
    .ind_sent_cb        = bes_sal_gatts_ind_sent_cb,
    .congestion_cb      = bes_sal_gatts_congestion_cb,
    .mtu_changed_cb     = bes_sal_gatts_mtu_changed_cb,
    .phy_updated_cb     = bes_sal_gatts_phy_updated_cb,
    .conn_updated_cb    = bes_sal_gatts_conn_updated_cb,
    .subrate_chg_cb     = bes_sal_gatts_subrate_chg_cb,
    .read_phy_cb        = bes_sal_gatts_read_phy_cb,
    .read_rssi_cb       = bes_sal_gatts_read_rssi_cb,
};

bt_status_t bt_sal_gatt_server_enable(void)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_gatt_server_disable(void) {
    gatts_unregister_server(sal_gatts_evn.server_if);
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

uint8_t bt_sal_gatt_server_get_properties(uint16_t properties)
{
    uint8_t bes_properties = 0;
    if((properties & GATT_PROP_BROADCAST) == GATT_PROP_BROADCAST)
    {
        bes_properties |= BTH_PROPERTY_BROADCAST;
    }
    if((properties & GATT_PROP_READ) == GATT_PROP_READ)
    {
        bes_properties |= BTH_PROPERTY_READ;
    }
    if((properties & GATT_PROP_WRITE_NR) == GATT_PROP_WRITE_NR)
    {
        bes_properties |= BTH_PROPERTY_WRITE_NO_RESPONSE;
    }
    if((properties & GATT_PROP_WRITE) == GATT_PROP_WRITE)
    {
        bes_properties |= GATT_PROP_WRITE;
    }
    if((properties & GATT_PROP_NOTIFY) == GATT_PROP_NOTIFY)
    {
        bes_properties |= BTH_PROPERTY_NOTIFY;
    }
    if((properties & GATT_PROP_INDICATE) == GATT_PROP_INDICATE)
    {
        bes_properties |= BTH_PROPERTY_INDICATE;
    }
    if((properties & GATT_PROP_SIGNED_WRITE) == GATT_PROP_SIGNED_WRITE)
    {
        // bes_properties |= BTH_PROPERTY_SIGNED_WRITE;
    }
    if((properties & GATT_PROP_EXTENDED_PROPS) == GATT_PROP_EXTENDED_PROPS)
    {
        //bes_properties |= BTH_PROPERTY_EXTENDED_PROPS;
    }
    return bes_properties;
}

uint16_t bt_sal_gatt_server_get_permissions(uint16_t permissions)
{
    uint16_t bes_permissions = 0;
    if((permissions & GATT_PERM_READ) == GATT_PERM_READ)
    {
        bes_permissions |= BTH_ATT_RD_PERM;
        if((permissions & GATT_PERM_ENCRYPT_REQUIRED) == GATT_PERM_ENCRYPT_REQUIRED)
        {
            // if(permissions & GATT_PERM_MITM_REQUIRED == GATT_PERM_MITM_REQUIRED)
            // {
            // }
            // else
            {
                bes_permissions |= BTH_ATT_RD_ENC;
            }
        }
        if((permissions & GATT_PERM_MITM_REQUIRED) == GATT_PERM_MITM_REQUIRED)
        {
        }
    }
    if((permissions & GATT_PERM_WRITE) == GATT_PERM_WRITE)
    {
        bes_permissions |= BTH_ATT_WR_PERM;
        if((permissions & GATT_PERM_ENCRYPT_REQUIRED) == GATT_PERM_ENCRYPT_REQUIRED)
        {
            // if(permissions & GATT_PERM_MITM_REQUIRED == GATT_PERM_MITM_REQUIRED)
            // {
            // }
            // else
            {
                bes_permissions |= BTH_ATT_WR_ENC;
            }
        }
        else if((permissions & GATT_PERM_AUTHEN_REQUIRED) == GATT_PERM_AUTHEN_REQUIRED)
        {
            // if(permissions & GATT_PERM_MITM_REQUIRED == GATT_PERM_MITM_REQUIRED)
            // {
            // }
            // else
            {
                bes_permissions |= BTH_ATT_WR_ENC;
            }
        }
    }
    return bes_permissions;
}

bt_status_t bt_sal_gatt_server_add_elements(gatt_element_t* elements, uint16_t size) {
    size_t index;
    bth_gatt_db_element_t* services = (bth_gatt_db_element_t*) malloc(sizeof(bth_gatt_db_element_t) * size);
    memset((uint8_t*) services, 0, sizeof(bth_gatt_db_element_t) * size);
    bth_gatt_db_element_t* tmp = services;
    for (index = 0; index < size; index++) {
        switch (elements[index].type) {
        case GATT_PRIMARY_SERVICE:
        {
            tmp->uuid = uuid_from_128bit_LE((uint8_t*)&elements[index].uuid.val);
            tmp->properties = 0;
            tmp->extended_properties = 0;
            tmp->permissions = 0;
            tmp->type = BTH_GATT_DB_PRIMARY_SERVICE;
            tmp->id = elements[index].handle;
            tmp++;
        }
        break;
        case GATT_SECONDARY_SERVICE:
        {
            tmp->uuid = uuid_from_128bit_LE((uint8_t*)&elements[index].uuid.val);
            tmp->properties = 0;
            tmp->extended_properties = 0;
            tmp->permissions = 0;
            tmp->type = BTH_GATT_DB_SECONDARY_SERVICE;
            tmp->id = elements[index].handle;
            tmp++;
        }
        break;
        case GATT_CHARACTERISTIC:
        {
            tmp->uuid = uuid_from_128bit_LE((uint8_t*)&elements[index].uuid.val);
            tmp->type = BTH_GATT_DB_CHARACTERISTIC;
            tmp->properties = bt_sal_gatt_server_get_properties(elements[index].properties);
            tmp->extended_properties = 0;
            tmp->permissions = bt_sal_gatt_server_get_permissions(elements[index].permissions);
            tmp->id = elements[index].handle;
            tmp++;
        }
        break;
        case GATT_DESCRIPTOR:
        {
            tmp->uuid = uuid_from_128bit_LE((uint8_t*)&elements[index].uuid.val);
            tmp->type = BTH_GATT_DB_DESCRIPTOR;
            tmp->properties = 0;
            tmp->extended_properties = 0;
            tmp->permissions = bt_sal_gatt_server_get_permissions(elements[index].permissions);
            tmp->id = elements[index].handle;
            tmp++;
        }
        break;
        default:
            BT_LOGE("%s, type:%d not handle", __func__, elements[index].type);
            break;
        }
    }
    for(int i = 0; i < GATT_MAX_SR_PROFILES; i++)
    {
        if(sal_gatts_evn.service_info[i].service_db == NULL)
        {
            sal_gatts_evn.service_info[i].service_db = services;
            sal_gatts_evn.service_info[i].char_count = size;
            gatts_add_service(sal_gatts_evn.server_if, services, size);
            return BT_STATUS_SUCCESS;
        }
    }
    return BT_STATUS_FAIL;
}

bt_status_t bt_sal_gatt_server_remove_elements(gatt_element_t* elements, uint16_t size) {
    uint16_t handle;
    for (int i = 0; i < size; i++) {
        if (elements[i].handle) {
            BT_LOGD("%s, handle:%d", __func__, elements[i].handle);
            handle = bes_sal_gatts_get_handle_by_request_id(elements[i].handle);
            gatts_delete_service(sal_gatts_evn.server_if, handle);
        }
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_server_connect(bt_controller_id_t id, bt_address_t* addr, ble_addr_type_t addr_type) {
    bth_bt_status_t status;
    status = gatts_connect(sal_gatts_evn.server_if, (const bth_address_t*)addr, false, BTH_BT_TRANSPORT_LE);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_server_cancel_connection(bt_controller_id_t id, bt_address_t* addr) {
    bth_bt_status_t status;
    uint16_t conn_id = bes_sal_gatts_get_conn_id_by_addr((bth_address_t*)addr);
    status = gatts_disconnect(sal_gatts_evn.server_if, (const bth_address_t*)addr, conn_id);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_server_send_response(bt_controller_id_t id, bt_address_t* addr, uint32_t request_id, uint8_t* value, uint16_t length) {
    bth_bt_status_t status;
    bth_gatt_response_t response = {
        .value = value,
        .len = length,
    };
    uint16_t conn_id = bes_sal_gatts_get_conn_id_by_addr((bth_address_t*)addr);
    status = gatts_send_response(conn_id, request_id, 0, &response);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_server_set_attr_value(bt_controller_id_t id, bt_address_t* addr, uint32_t request_id, uint8_t* value, uint16_t length) {
    if(length > (GATT_MAX_MTU_SIZE - 3))
    {
        return BT_STATUS_FAIL;
    }
    for(int i=0; i<GATT_MAX_SR_PROFILES; i++)
    {
        if(sal_gatts_evn.atts_data[i].request_id == 0)
        {
            sal_gatts_evn.atts_data[i].request_id = request_id;
            sal_gatts_evn.atts_data[i].data = (uint8_t*)malloc(length);
            sal_gatts_evn.atts_data[i].datalen = length;
            return BT_STATUS_SUCCESS;
        }
    }
    return BT_STATUS_FAIL;
}

bt_status_t bt_sal_gatt_server_get_attr_value(bt_controller_id_t id, bt_address_t* addr, uint32_t request_id, uint8_t* value, uint16_t length) {
    for(int i=0; i<GATT_MAX_SR_PROFILES; i++)
    {
        if(sal_gatts_evn.atts_data[i].request_id == request_id)
        {
            if(length <= sal_gatts_evn.atts_data[i].datalen)
            {
                memcpy(value, sal_gatts_evn.atts_data[i].data, length);
                return BT_STATUS_SUCCESS;
            }
        }
    }
    return BT_STATUS_FAIL;
}

#if defined(CONFIG_BLUETOOTH_STACK_LE_ZBLUE)
bt_status_t bt_sal_gatt_server_send_notification(bt_controller_id_t id, bt_address_t* addr, bt_uuid_t element_id, uint8_t* value, uint16_t length) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_NOT_SUPPORTED; // Replace with actual success status code
}

bt_status_t bt_sal_gatt_server_send_indication(bt_controller_id_t id, bt_address_t* addr, bt_uuid_t element_id, uint8_t* value, uint16_t length) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_NOT_SUPPORTED; // Replace with actual success status code
}
#else
bt_status_t bt_sal_gatt_server_send_notification(bt_controller_id_t id, bt_address_t* addr, uint16_t element_id, uint8_t* value, uint16_t length) {
    bth_bt_status_t status;
    uint16_t conn_id = bes_sal_gatts_get_conn_id_by_addr((bth_address_t*)addr);
    uint16_t handle = bes_sal_gatts_get_handle_by_request_id(element_id);
    status = gatts_send_indication_or_notify(sal_gatts_evn.server_if, handle,
                                    conn_id, false,
                                    value, length);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_server_send_indication(bt_controller_id_t id, bt_address_t* addr, uint16_t element_id, uint8_t* value, uint16_t length) {
    bth_bt_status_t status;
    uint16_t conn_id = bes_sal_gatts_get_conn_id_by_addr((bth_address_t*)addr);
    uint16_t handle = bes_sal_gatts_get_handle_by_request_id(element_id);
    status = gatts_send_indication_or_notify(sal_gatts_evn.server_if, handle,
                                    conn_id, true,
                                    value, length);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}
#endif

bt_status_t bt_sal_gatt_server_read_phy(bt_controller_id_t id, bt_address_t* addr) {
    bth_bt_status_t status;
    if (addr == NULL)
    {
        BT_LOGE("address is null!");
        return BT_STATUS_FAIL;
    }
    status = gatts_read_phy((bth_address_t*)addr);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_server_set_phy(bt_controller_id_t id, bt_address_t* addr, ble_phy_type_t tx_phy, ble_phy_type_t rx_phy) {
    bth_bt_status_t status;
    status = gatts_set_preferred_phy((bth_address_t*)addr, tx_phy, rx_phy, 0);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

void bt_sal_gatt_server_connection_changed_callback(bt_address_t* addr, uint16_t connection_interval, uint16_t peripheral_latency,
                                                    uint16_t supervision_timeout) {
    gatts_conn_parameter_update((bth_address_t*)addr, connection_interval, connection_interval,peripheral_latency, supervision_timeout, 0,0);
}
