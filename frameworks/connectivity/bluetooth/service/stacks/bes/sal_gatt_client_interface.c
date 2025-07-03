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
#define LOG_TAG "sal_bes_gattc"

/****************************** header include ********************************/
#include <stdint.h> // For uint8_t, uint16_t, uint32_t types
#include <stdbool.h> // For bool type
#include "adapter_internel.h"
#include "sal_interface.h"
#include "sal_bes.h"
#include "bt_status.h"
#include "bluetooth.h"
#include "utils/log.h"

#include "sal_gatt_client_interface.h"

#include "api/bth_api_gatt_client.h"

/***************************** external declaration *****************************/

/***************************** macro defination *******************************/
#define CONFIG_GATT_CLIENT_ELEMENT_MAX 20

/*****************************  type defination ********************************/

typedef struct
{
    int conn_id;
    bth_address_t addr;
}gattc_device_t;

typedef struct
{
    int client_if;
    gattc_device_t device[CONFIG_BLUETOOTH_GATTC_MAX_CONNECTIONS];
}gattc_evn_t;
/*****************************  variable defination *****************************/
static gattc_evn_t sal_gattc_evn = {0};

/*****************************  function declaration ****************************/

static void bes_sal_gattc_set_device(uint16_t conn_id, const bth_address_t* addr)
{
    for(int i = 0; i < CONFIG_BLUETOOTH_GATTC_MAX_CONNECTIONS; i++)
    {
        if(sal_gattc_evn.device[i].conn_id == 0 || sal_gattc_evn.device[i].conn_id == 0xffff)
        {
            BT_LOGE("[%s] device conn_id %d",__func__, conn_id);
            sal_gattc_evn.device[i].conn_id = conn_id;
            memcpy((uint8_t*)&sal_gattc_evn.device[i].addr, addr, BT_ADDR_LENGTH);
            break;
        }
    }
    return;
}

static void bes_sal_gattc_clear_device(uint16_t conn_id)
{
    for(int i = 0; i < CONFIG_BLUETOOTH_GATTC_MAX_CONNECTIONS; i++)
    {
        if(sal_gattc_evn.device[i].conn_id == conn_id)
        {
            BT_LOGE("[%s] device conn_id %d",__func__, conn_id);
            sal_gattc_evn.device[i].conn_id = 0;
            memset((uint8_t*)&sal_gattc_evn.device[i].addr, 0, BT_ADDR_LENGTH);
            break;
        }
    }
    return;
}

static bth_address_t* bes_sal_gattc_get_addr_by_conn_id(uint16_t conn_id)
{
    bth_address_t* addr = NULL;
    for(int i = 0; i < CONFIG_BLUETOOTH_GATTC_MAX_CONNECTIONS; i++)
    {
        if(sal_gattc_evn.device[i].conn_id == conn_id)
        {
            addr = &sal_gattc_evn.device[i].addr;
            break;
        }
    }
    return addr;
}

static uint16_t bes_sal_gattc_get_conn_id_by_addr(bth_address_t* addr)
{
    uint16_t conn_id = 0;
    for(int i = 0; i < CONFIG_BLUETOOTH_GATTC_MAX_CONNECTIONS; i++)
    {
        if(memcmp(&sal_gattc_evn.device[i].addr, addr, BT_ADDR_LENGTH) == 0 )
        {
            conn_id = sal_gattc_evn.device[i].conn_id;
            break;
        }
    }
    return conn_id;
}

static void bes_sal_gattc_conversion_element(const bth_gatt_db_element_t* db_element, gatt_element_t* element)
{
    element->handle = db_element->attribute_handle;
    memcpy((uint8_t*)element->uuid.val.u128 , (uint8_t*)db_element->uuid.data, UUID_LEN);
    element->uuid.type = db_element->uuid.type;
    element->type = db_element->type;
    element->properties = db_element->properties;
    element->permissions = db_element->permissions;
}

static void bes_sal_gattc_register_client_cb(int status, int client_if,
                                               const bth_uuid_t *app_uuid)
{
    BT_LOGE("[%s][%d] client_if %d",__func__, __LINE__, client_if);
    sal_gattc_evn.client_if = client_if;
}

static void bes_sal_gattc_connect_cb(int conn_id, int status, int client_if,
                                       const bth_address_t *bda)
{
    bes_sal_gattc_set_device(conn_id, bda);
    if_gattc_on_connection_state_changed((bt_address_t*)bda,PROFILE_STATE_CONNECTED);
}

static void bes_sal_gattc_disconnect_cb(int conn_id, int status, int client_if,
                                          const bth_address_t *bda)
{
    bes_sal_gattc_clear_device(conn_id);
    if_gattc_on_connection_state_changed((bt_address_t*)bda,PROFILE_STATE_DISCONNECTED);
}

static void bes_sal_gattc_search_cmpl_cb(int conn_id, int status)
{
    bt_address_t* addr = (bt_address_t*)bes_sal_gattc_get_addr_by_conn_id(conn_id);
    bth_gattc_get_gatt_db(conn_id);
    if_gattc_on_discover_completed(addr, status);
}

static void bes_sal_gattc_reg_notifi_cb(int conn_id, int registered,
                                          int status, uint16_t handle)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
}

static void bes_sal_gattc_notify_cb(int conn_id, const bth_gatt_notify_params_t* p_data)
{
    bt_address_t* addr = (bt_address_t*)bes_sal_gattc_get_addr_by_conn_id(conn_id);
    if_gattc_on_element_changed(addr, p_data->handle, (uint8_t*)p_data->value, p_data->len);
}

static void bes_sal_gattc_read_char_cb(int conn_id, int status,
                                         const bth_gatt_read_params_t* p_data)
{
    bt_address_t* addr = (bt_address_t*)bes_sal_gattc_get_addr_by_conn_id(conn_id);
    if_gattc_on_element_read(addr, p_data->handle, (uint8_t*)p_data->value, p_data->len, p_data->status);
}

static void bes_sal_gattc_write_char_cb(int conn_id, int status,
                                          uint16_t handle, uint16_t len,
                                          const uint8_t* value)
{
    bt_address_t* addr = (bt_address_t*)bes_sal_gattc_get_addr_by_conn_id(conn_id);
    if_gattc_on_element_written(addr, handle, status);
}

static void bes_sal_gattc_exec_write_cb(int conn_id, int status)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
}

static void bes_sal_gattc_read_desc_cb(int conn_id, int status,
                                         const bth_gatt_read_params_t* p_data)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
}

static void bes_sal_gattc_write_desc_cb(int conn_id, int status,
                                          uint16_t handle, uint16_t len,
                                          const uint8_t* value)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
}

static void bes_sal_gattc_read_rssi_cb(int client_if, const bth_address_t *bda,
                                         int rssi, int status)
{
    if_gattc_on_rssi_read((bt_address_t *)bda, rssi, status);
}

static void bes_sal_gattc_config_mtu_cb(int conn_id, int status, int mtu)
{
    bt_address_t* addr = (bt_address_t*)bes_sal_gattc_get_addr_by_conn_id(conn_id);
    if_gattc_on_mtu_changed(addr, mtu, status);
}

static void bes_sal_gattc_congestion_cb(int conn_id, bool congested)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
}

static void bes_sal_gattc_get_gatt_db_cb(int conn_id, const bth_gatt_db_element_t* db,
                                           int count)
{
    bt_address_t* addr = (bt_address_t *)bes_sal_gattc_get_addr_by_conn_id(conn_id);
    gatt_element_t* buf = (gatt_element_t*) malloc(count * sizeof(gatt_element_t));
    for(int i = 0; i < count; i++)
    {
        bes_sal_gattc_conversion_element(db + i,buf + i);
    }
    if_gattc_on_service_discovered(addr, buf, count);
    free(buf);
}

static void bes_sal_gattc_services_removed_cb(int conn_id, uint16_t start_handle,
                                                uint16_t end_handle)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
}

static void bes_sal_gattc_services_added_cb(int conn_id, const bth_gatt_db_element_t* added,
                                              int added_count)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
}

static void bes_sal_gattc_phy_updated_cb(int conn_id, uint8_t tx_phy,
                                           uint8_t rx_phy, uint8_t status)
{
    bt_address_t* addr = (bt_address_t *)bes_sal_gattc_get_addr_by_conn_id(conn_id);
    if_gattc_on_phy_updated(addr, tx_phy, rx_phy, status);
}

static void bes_sal_gattc_conn_updated_cb(int conn_id, uint16_t interval,
                                            uint16_t latency, uint16_t timeout,
                                            uint8_t status)
{
    bt_address_t* addr = (bt_address_t *)bes_sal_gattc_get_addr_by_conn_id(conn_id);
    if_gattc_on_connection_parameter_updated(addr, interval, latency, timeout, status);
}

static void bes_sal_gattc_service_chg_cb(int conn_id)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
}

static void bes_sal_gattc_subrate_chg_cb(int conn_id, uint16_t subrate_factor,
                                           uint16_t latency, uint16_t cont_num,
                                           uint16_t timeout, uint8_t status)
{
    BT_LOGE("[%s][%d]",__func__, __LINE__);
}

static void bes_sal_gattc_read_phy_cb(const bth_address_t* addr, uint8_t tx_phy, uint8_t rx_phy, uint8_t status)
{
    if_gattc_on_phy_read((bt_address_t *)addr, tx_phy, rx_phy);
}

gattc_client_callbacks_t bes_sal_gattc =
{
    .register_client_cb = bes_sal_gattc_register_client_cb,
    .open_cb            = bes_sal_gattc_connect_cb,
    .close_cb           = bes_sal_gattc_disconnect_cb,
    .search_cmpl_cb     = bes_sal_gattc_search_cmpl_cb,
    .reg_notifi_cb      = bes_sal_gattc_reg_notifi_cb,
    .notify_cb          = bes_sal_gattc_notify_cb,
    .read_char_cb       = bes_sal_gattc_read_char_cb,
    .write_char_cb      = bes_sal_gattc_write_char_cb,
    .read_desc_cb       = bes_sal_gattc_read_desc_cb,
    .write_desc_cb      = bes_sal_gattc_write_desc_cb,
    .exec_write_cb      = bes_sal_gattc_exec_write_cb,
    .read_rssi_cb       = bes_sal_gattc_read_rssi_cb,
    .config_mtu_cb      = bes_sal_gattc_config_mtu_cb,
    .congestion_cb      = bes_sal_gattc_congestion_cb,
    .get_gatt_db_cb     = bes_sal_gattc_get_gatt_db_cb,
    .services_rm_cb     = bes_sal_gattc_services_removed_cb,
    .services_add_cb    = bes_sal_gattc_services_added_cb,
    .phy_updated_cb     = bes_sal_gattc_phy_updated_cb,
    .conn_updated_cb    = bes_sal_gattc_conn_updated_cb,
    .service_chg_cb     = bes_sal_gattc_service_chg_cb,
    .subrate_chg_cb     = bes_sal_gattc_subrate_chg_cb,
    .read_phy_cb        = bes_sal_gattc_read_phy_cb,
};

bt_status_t bt_sal_gatt_client_connect(bt_controller_id_t id, bt_address_t* addr, ble_addr_type_t addr_type) {
    bth_bt_status_t status;
    status = bth_gattc_connect(sal_gattc_evn.client_if, (const bth_address_t*)addr, addr_type, false, BTH_BT_TRANSPORT_LE,BT_LE_2M_PHY,false);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_disconnect(bt_controller_id_t id, bt_address_t* addr) {
    bth_bt_status_t status;
    uint16_t conn_id = bes_sal_gattc_get_conn_id_by_addr((bth_address_t *)addr);
    status = bth_gattc_disconnect(sal_gattc_evn.client_if, (const bth_address_t*)addr, conn_id);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_discover_all_services(bt_controller_id_t id, bt_address_t* addr) {
    bth_bt_status_t status;
    uint16_t conn_id = bes_sal_gattc_get_conn_id_by_addr((bth_address_t *)addr);
    status = bth_gattc_search_service(conn_id, NULL);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_discover_service_by_uuid(bt_controller_id_t id, bt_address_t* addr, bt_uuid_t* uuid) {
    uint16_t conn_id = bes_sal_gattc_get_conn_id_by_addr((bth_address_t *)addr);
    bth_gattc_discover_service_by_uuid(conn_id, uuid_is_empty((bth_uuid_t*)uuid) ? NULL : (bth_uuid_t*)uuid);
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_read_element(bt_controller_id_t id, bt_address_t* addr, uint16_t element_id) {
    uint16_t conn_id = bes_sal_gattc_get_conn_id_by_addr((bth_address_t*)addr);
    bth_gattc_read_characteristic(conn_id, element_id, BLE_AUTHENTICATION_NO_NONE);
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_write_element(bt_controller_id_t id, bt_address_t* addr, uint16_t element_id, uint8_t* value, uint16_t length, gatt_write_type_t write_type) {
    bth_bt_status_t status;
    int type = GATTC_WRITE_TYPE_NO_RESPONSE;
    if(GATT_WRITE_TYPE_NO_RSP == write_type)
    {
        type= GATTC_WRITE_TYPE_NO_RESPONSE;
    }
    else
    {
        type= GATTC_WRITE_TYPE_DEFAULT;
    }
    uint16_t conn_id = bes_sal_gattc_get_conn_id_by_addr((bth_address_t*)addr);
    status = bth_gattc_write_characteristic(conn_id, element_id,
                               type, BLE_AUTHENTICATION_NO_NONE,
                               (uint8_t*) value, (int) length);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_register_notifications(bt_controller_id_t id, bt_address_t* addr, uint16_t element_id, uint16_t properties, bool enable) {
    bth_bt_status_t status;
    if (addr == NULL)
    {
        BT_LOGE("address is null!");
        return BT_STATUS_FAIL;
    }
    if(enable)
    {
        status = bth_gattc_register_for_notification(sal_gattc_evn.client_if, (bth_address_t*)addr, element_id);
    }
    else
    {
        status = bth_gattc_deregister_for_notification(sal_gattc_evn.client_if, (bth_address_t*)addr, element_id);
    }
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_send_mtu_req(bt_controller_id_t id, bt_address_t* addr, uint32_t mtu) {
    bth_bt_status_t status;
    uint16_t conn_id = bes_sal_gattc_get_conn_id_by_addr((bth_address_t*)addr);
    status = bth_gattc_configure_mtu(conn_id, mtu);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_update_connection_parameter(bt_controller_id_t id, bt_address_t* addr, uint32_t min_interval, uint32_t max_interval, uint32_t latency,
                                                           uint32_t timeout, uint32_t min_connection_event_length, uint32_t max_connection_event_length) {
    bth_bt_status_t status;
    status = bth_gattc_conn_parameter_update((bth_address_t*)addr, min_interval, max_interval, latency, timeout, min_connection_event_length, max_connection_event_length);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_read_remote_rssi(bt_controller_id_t id, bt_address_t* addr) {
    bth_bt_status_t status;
    status = bth_gattc_read_remote_rssi(sal_gattc_evn.client_if, (bth_address_t*)addr);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_read_phy(bt_controller_id_t id, bt_address_t* addr) {
    bth_bt_status_t status;
    status = bth_gattc_read_phy((bth_address_t*)addr);  // Assuming callback is NULL here.
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_set_phy(bt_controller_id_t id, bt_address_t* addr, ble_phy_type_t tx_phy, ble_phy_type_t rx_phy) {
    bth_bt_status_t status;
    status = bth_gattc_set_preferred_phy((bth_address_t*)addr, tx_phy, rx_phy, 0);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

void bt_sal_gatt_client_connection_updated_callback(bt_controller_id_t id, bt_address_t* addr, uint16_t connection_interval, uint16_t peripheral_latency,
                                                    uint16_t supervision_timeout, bt_status_t status) {
    bth_gattc_conn_parameter_update((bth_address_t*)addr, connection_interval, connection_interval,peripheral_latency, supervision_timeout, 0,8);
}