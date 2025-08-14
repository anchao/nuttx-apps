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
    bool in_use;
    int conn_id;
    bth_address_t addr;
} gattc_device_t;

typedef struct
{
    int client_if;
    gattc_device_t device[CONFIG_BLUETOOTH_GATTC_MAX_CONNECTIONS];
} gattc_env_t;
/*****************************  variable defination *****************************/
static gattc_env_t g_gattc = {0};

/*****************************  function declaration ****************************/
static gattc_env_t* get_gattc_env()
{
    return &g_gattc;
}

static void to_bt_uuid(const bth_uuid_t* from, bt_uuid_t* to)
{
    bth_uuid_t tmp128;
    if (from == NULL || to == NULL)
    {
        LOG_E("Invalid params!");
        return;
    }
    if (from->type == BTH_UUID_TYPE_16)
    {
        to->type = BT_UUID16_TYPE;
        to->val.u16 = as_16bit(from);
    }
    else if (from->type == BTH_UUID_TYPE_32)
    {
        to->type = BT_UUID32_TYPE;
        to->val.u32 = as_32bit(from);
    }
    else if (from->type == BTH_UUID_TYPE_128)
    {
        to->type = BT_UUID128_TYPE;
        tmp128 = uuid_to_128bit_LE(from);
        memcpy(to->val.u128, tmp128.data, UUID_LEN);
    }
    else
    {
        LOG_E("Unknown uuid type %d", from->type);
    }
}

static void to_bth_uuid()
{

}

static void gattc_add_device(int conn_id, const bth_address_t* addr)
{
    gattc_env_t* gattc = get_gattc_env();
    gattc_device_t* device = NULL;
    int not_used = 0xFFFF;

    for(int i = 0; i < CONFIG_BLUETOOTH_GATTS_MAX_CONNECTIONS; i++)
    {
        device = gattc->device + i;
        if (device->in_use && is_bt_address_equal(addr, &device->addr))
        {
            BT_LOGW("Already add this device " STRMAC, MAC2STR(device->addr.address));
            return;
        }
        else if (!device->in_use && not_used == 0XFFFF)
        {
            not_used = i;
        }
    }
    if (not_used != 0XFFFF)
    {
        device = &gattc->device[not_used];
        device->conn_id = conn_id;
        device->addr = *addr;
        device->in_use = true;
    }
    else
    {
        LOG_E("Reach max connections");
    }
    return;
}

static void gattc_remove_device(int conn_id)
{
    gattc_env_t* gattc = get_gattc_env();
    gattc_device_t* device = NULL;
    for(int i = 0; i < CONFIG_BLUETOOTH_GATTS_MAX_CONNECTIONS; i++)
    {
        device = gattc->device + i;
        if (device->in_use && device->conn_id == conn_id)
        {
            device->in_use = false;
            return;
        }
    }
    LOG_E("Can't find device for conn id %d", conn_id);
}

static bth_address_t* gattc_get_addr_by_conn_id(int conn_id)
{
    gattc_env_t* gattc = get_gattc_env();
    gattc_device_t* device = NULL;
    bth_address_t* addr = NULL;
    for(int i = 0; i < CONFIG_BLUETOOTH_GATTC_MAX_CONNECTIONS; i++)
    {
        device = &gattc->device[i];
        if(device->conn_id == conn_id)
        {
            return &device->addr;
        }
    }
    return addr;
}

static int gattc_get_conn_id_by_addr(bth_address_t* addr)
{
    gattc_env_t* gattc = get_gattc_env();
    gattc_device_t* device = NULL;
    for(int i = 0; i < CONFIG_BLUETOOTH_GATTC_MAX_CONNECTIONS; i++)
    {
        device = &gattc->device[i];
        if(device->in_use && is_bt_address_equal(addr, &device->addr))
        {
            return device->conn_id;
        }
    }
    return 0;
}

static void gattc_conversion_element(const bth_gatt_db_element_t* db_element, gatt_element_t* element, uint16_t size)
{
    for (uint16_t i = 0; i < size; i++, db_element++, element++)
    {
        element->handle = db_element->attribute_handle;
        element->type = (gatt_attr_type_t)db_element->type;
        element->properties = db_element->properties;
        element->permissions = db_element->permissions;
        to_bt_uuid(&db_element->uuid, &element->uuid);
    }

}

static void bes_sal_gattc_register_client_cb(int status, int client_if,
                                               const bth_uuid_t *app_uuid)
{
    gattc_env_t* gattc = get_gattc_env();
    UNUSED(app_uuid);

    LOG_D("client if %d status %d", client_if, status);
    if (status == BTH_STATUS_SUCCESS)
    {
        gattc->client_if = client_if;
    }
}

static void bes_sal_gattc_connect_cb(int conn_id, int status, int client_if,
                                       const bth_address_t *bda)
{
    gattc_env_t* gattc = get_gattc_env();
    if (gattc->client_if != client_if)
    {
        return;
    }

    if (status != BTH_STATUS_SUCCESS)
    {
        return;
    }
    gattc_add_device(conn_id, bda);
    if_gattc_on_connection_state_changed(TO_BT_ADDRESS(bda),PROFILE_STATE_CONNECTED);
}

static void bes_sal_gattc_disconnect_cb(int conn_id, int status, int client_if,
                                          const bth_address_t *bda)
{
    gattc_env_t* gattc = get_gattc_env();
    UNUSED(status);
    if (gattc->client_if != client_if)
    {
        return;
    }
    gattc_remove_device(conn_id);
    if_gattc_on_connection_state_changed(TO_BT_ADDRESS(bda), PROFILE_STATE_DISCONNECTED);
}

static void bes_sal_gattc_search_cmpl_cb(int conn_id, int status)
{
    bt_address_t* addr = (bt_address_t*)gattc_get_addr_by_conn_id(conn_id);
    if (addr == NULL)
    {
        LOG_E("Can't find device for conn id %d", conn_id);
        return;
    }

    if_gattc_on_discover_completed(addr, status);
    bth_gattc_get_gatt_db(conn_id);
}

static void bes_sal_gattc_reg_notifi_cb(int conn_id, bth_gatt_cccd_value_type_t type, int registered, int status, uint16_t handle)
{
    bt_address_t* addr = (bt_address_t*)gattc_get_addr_by_conn_id(conn_id);
    UNUSED(type);
    if (addr == NULL)
    {
        LOG_E("Can't find device for conn id %d", conn_id);
        return;
    }

    LOG_E("conn id %d registered %d status %d handle %d", conn_id, registered, status, handle);
    if_gattc_on_element_subscribed(addr, handle, to_gatt_status(status), registered);
}

static void bes_sal_gattc_notify_cb(int conn_id, const bth_gatt_notify_params_t* p_data)
{
    bt_address_t* addr = (bt_address_t*)gattc_get_addr_by_conn_id(conn_id);
    LOG_E("conn_id %d handle %d " STRMAC, conn_id, p_data->handle, MAC2STR(addr->addr));
    if_gattc_on_element_changed(addr, p_data->handle, (uint8_t*)p_data->value, p_data->len);
}

static void bes_sal_gattc_read_char_cb(int conn_id, int status,
                                         const bth_gatt_read_params_t* p_data)
{
    bt_address_t* addr = (bt_address_t*)gattc_get_addr_by_conn_id(conn_id);
    UNUSED(status);

    if_gattc_on_element_read(addr, p_data->handle, (uint8_t*)p_data->value, p_data->len, to_gatt_status(p_data->status));
}

static void bes_sal_gattc_write_char_cb(int conn_id, int status,
                                          uint16_t handle, uint16_t len,
                                          const uint8_t* value)
{
    bt_address_t* addr = (bt_address_t*)gattc_get_addr_by_conn_id(conn_id);
    UNUSED(len);
    UNUSED(value);

    if_gattc_on_element_written(addr, handle, status);
}

static void bes_sal_gattc_exec_write_cb(int conn_id, int status)
{
    LOG_D("conn id %d status %d", conn_id, status);
}

static void bes_sal_gattc_read_desc_cb(int conn_id, int status,
                                         const bth_gatt_read_params_t* p_data)
{
    bt_address_t* addr = (bt_address_t*)gattc_get_addr_by_conn_id(conn_id);
    LOG_D("conn id %d status %d", conn_id, status);
    if (addr == NULL)
    {
        LOG_E("Address is NULL");
        return;
    }

    if_gattc_on_element_read(addr, p_data->handle, p_data->value, p_data->len, to_gatt_status(p_data->status));
}

static void bes_sal_gattc_write_desc_cb(int conn_id, int status,
                                          uint16_t handle, uint16_t len,
                                          const uint8_t* value)
{
    LOG_D("conn id %d status %d handle %d len %d value %p", conn_id, status, handle, len, value);
}

static void bes_sal_gattc_read_rssi_cb(int client_if, const bth_address_t *bda,
                                         int rssi, int status)
{
    UNUSED(client_if);
    if_gattc_on_rssi_read((bt_address_t *)bda, rssi, status);
}

static void bes_sal_gattc_config_mtu_cb(int conn_id, int status, int mtu)
{
    bt_address_t* addr = (bt_address_t*)gattc_get_addr_by_conn_id(conn_id);
    if_gattc_on_mtu_changed(addr, mtu, status);
}

static void bes_sal_gattc_congestion_cb(int conn_id, bool congested)
{
    LOG_D("conn id %d congested %d", conn_id, congested);
}


static void bes_sal_gattc_get_gatt_db_cb(int conn_id, const bth_gatt_db_element_t* db,
                                           int count)
{
    bt_address_t* addr = (bt_address_t *)gattc_get_addr_by_conn_id(conn_id);
    uint16_t size = 0;
    gatt_element_t* buf = NULL;

    for(int i = 0; i < count; i++)
    {
        if (db[i].type == BTH_GATT_DB_PRIMARY_SERVICE)
        {
            int m = i + 1;
            for (; m < count; m++)
            {
                if (db[m].type == BTH_GATT_DB_PRIMARY_SERVICE)
                {
                    break;
                }
            }

            size = m - i;
            buf = malloc(size * sizeof(gatt_element_t));
            if (buf == NULL)
            {
                BT_LOGE("Malloc failed!");
                return;
            }
            gattc_conversion_element(db + i, buf, size);
            if_gattc_on_service_discovered(addr, buf, size);
            i = m - 1;
        }
    }
}

static void bes_sal_gattc_services_removed_cb(int conn_id, uint16_t start_handle,
                                                uint16_t end_handle)
{
    LOG_D("conn id %d start handle %d end handle %d", conn_id, start_handle, end_handle);
}

static void bes_sal_gattc_services_added_cb(int conn_id, const bth_gatt_db_element_t* added,
                                              int added_count)
{
    LOG_D("conn id %d", conn_id);
    UNUSED(added);
    UNUSED(added_count);
}

static void bes_sal_gattc_phy_updated_cb(int conn_id, uint8_t tx_phy,
                                           uint8_t rx_phy, uint8_t status)
{
    bt_address_t* addr = (bt_address_t *)gattc_get_addr_by_conn_id(conn_id);
    if_gattc_on_phy_updated(addr, tx_phy, rx_phy, status);
}

static void bes_sal_gattc_conn_updated_cb(int conn_id, uint16_t interval,
                                            uint16_t latency, uint16_t timeout,
                                            uint8_t status)
{
    bt_address_t* addr = (bt_address_t *)gattc_get_addr_by_conn_id(conn_id);
    if_gattc_on_connection_parameter_updated(addr, interval, latency, timeout, status);
}

static void bes_sal_gattc_service_chg_cb(int conn_id)
{
    LOG_D("conn id %d", conn_id);
}

static void bes_sal_gattc_subrate_chg_cb(int conn_id, uint16_t subrate_factor,
                                           uint16_t latency, uint16_t cont_num,
                                           uint16_t timeout, uint8_t status)
{
    LOG_D("conn id %d subrate %d latency %d cont_num %d timeout %d status %d",
          conn_id, subrate_factor, latency, cont_num, timeout, status);
}

static void bes_sal_gattc_read_phy_cb(const bth_address_t* addr, uint8_t tx_phy, uint8_t rx_phy, uint8_t status)
{
    UNUSED(status);
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

bt_status_t bt_sal_gatt_client_connect(bt_controller_id_t id, bt_address_t* addr, ble_addr_type_t addr_type)
{
    gattc_env_t* gattc = get_gattc_env();
    bth_bt_status_t status;
    UNUSED(id);
    status = bth_gattc_connect(gattc->client_if, (const bth_address_t*)addr, addr_type, false,
                               BTH_BT_TRANSPORT_LE, BT_LE_2M_PHY, false);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_disconnect(bt_controller_id_t id, bt_address_t* addr)
{
    gattc_env_t* gattc = get_gattc_env();
    uint16_t conn_id = gattc_get_conn_id_by_addr(TO_BTH_ADDRESS(addr));
    bth_bt_status_t status;
    UNUSED(id);
    status = bth_gattc_disconnect(gattc->client_if, TO_BTH_ADDRESS(addr), conn_id);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_discover_all_services(bt_controller_id_t id, bt_address_t* addr)
{
    bth_bt_status_t status;
    uint16_t conn_id = gattc_get_conn_id_by_addr(TO_BTH_ADDRESS(addr));
    UNUSED(id);
    status = bth_gattc_search_service(conn_id, NULL);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_discover_service_by_uuid(bt_controller_id_t id, bt_address_t* addr, bt_uuid_t* uuid)
{
    uint16_t conn_id = gattc_get_conn_id_by_addr((bth_address_t *)addr);
    bth_uuid_t tmp;
    UNUSED(id);
    if (uuid == NULL)
    {
        BT_LOGE("uuid is NULL");
        return BT_STATUS_FAIL;

    }
    if (uuid->type == BT_UUID16_TYPE)
    {
        tmp = uuid_from_16bit(uuid->val.u16);
    }else if (uuid->type == BTH_UUID_TYPE_32)
    {
        tmp = uuid_from_32bit(uuid->val.u16);
    }
    else
    {
        tmp = uuid_from_128bit_LE(uuid->val.u128);
    }
    bth_gattc_search_service(conn_id, &tmp);
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_read_element(bt_controller_id_t id, bt_address_t* addr, uint16_t element_id)
{
    uint16_t conn_id = gattc_get_conn_id_by_addr(TO_BTH_ADDRESS(addr));
    UNUSED(id);
    CHECK_BES_STACK_RETURN(bth_gattc_read_characteristic(conn_id, element_id, BLE_AUTHENTICATION_NO_NONE))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_write_element(bt_controller_id_t id, bt_address_t* addr, uint16_t element_id, uint8_t* value,
                                             uint16_t length, gatt_write_type_t write_type)
{
    bth_bt_status_t status;
    int type = GATTC_WRITE_TYPE_NO_RESPONSE;
    int conn_id = 0;
    UNUSED(id);
    if(GATT_WRITE_TYPE_NO_RSP == write_type)
    {
        type= GATTC_WRITE_TYPE_NO_RESPONSE;
    }
    else
    {
        type= GATTC_WRITE_TYPE_DEFAULT;
    }
    conn_id = gattc_get_conn_id_by_addr((bth_address_t*)addr);
    status = bth_gattc_write_characteristic(conn_id, element_id,
                               type, BLE_AUTHENTICATION_NO_NONE,
                               (uint8_t*) value, (int) length);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_register_notifications(bt_controller_id_t id, bt_address_t* addr, uint16_t element_id,
                                                      uint16_t properties, bool enable)
{
    gattc_env_t* gattc = get_gattc_env();
    bth_gatt_cccd_value_type_t type = GATTS_NOTIFICATION_ENABLE;
    bth_bt_status_t status;
    UNUSED(id);
    UNUSED(properties);
    if (addr == NULL)
    {
        BT_LOGE("address is null!");
        return BT_STATUS_FAIL;
    }
    if (properties == GATT_PROP_NOTIFY)
    {
        type = GATTS_NOTIFICATION_ENABLE;
    }
    else if (properties == GATT_PROP_INDICATE)
    {
        type = GATTS_INDICATION_ENABLE;
    }
    else
    {
        LOG_E("Unknown properties type %d", type);
        return BT_STATUS_FAIL;
    }
    if(enable)
    {
        status = bth_gattc_register_for_notification(gattc->client_if, (bth_address_t*)addr, type, element_id);
    }
    else
    {
        status = bth_gattc_deregister_for_notification(gattc->client_if, (bth_address_t*)addr, element_id);
    }
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_send_mtu_req(bt_controller_id_t id, bt_address_t* addr, uint32_t mtu)
{
    bth_bt_status_t status;
    int conn_id = gattc_get_conn_id_by_addr((bth_address_t*)addr);
    status = bth_gattc_configure_mtu(conn_id, mtu);
    UNUSED(id);

    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_update_connection_parameter(bt_controller_id_t id, bt_address_t* addr, uint32_t min_interval, uint32_t max_interval, uint32_t latency,
                                                           uint32_t timeout, uint32_t min_connection_event_length, uint32_t max_connection_event_length)
{
    bth_bt_status_t status;
    status = bth_gattc_conn_parameter_update(TO_BTH_ADDRESS(addr), min_interval, max_interval, latency, timeout, min_connection_event_length, max_connection_event_length);
    UNUSED(id);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_read_remote_rssi(bt_controller_id_t id, bt_address_t* addr) {
    bth_bt_status_t status;
    gattc_env_t* gattc = get_gattc_env();
    status = bth_gattc_read_remote_rssi(gattc->client_if, TO_BTH_ADDRESS(addr));
    UNUSED(id);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_read_phy(bt_controller_id_t id, bt_address_t* addr) {
    bth_bt_status_t status;
    status = bth_gattc_read_phy(TO_BTH_ADDRESS(addr));  // Assuming callback is NULL here.
    UNUSED(id);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_client_set_phy(bt_controller_id_t id, bt_address_t* addr, ble_phy_type_t tx_phy, ble_phy_type_t rx_phy) {
    bth_bt_status_t status;
    status = bth_gattc_set_preferred_phy((bth_address_t*)addr, tx_phy, rx_phy, 0);
    UNUSED(id);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}

void bt_sal_gatt_client_connection_updated_callback(bt_controller_id_t id, bt_address_t* addr, uint16_t connection_interval, uint16_t peripheral_latency,
                                                    uint16_t supervision_timeout, bt_status_t status)
{
    UNUSED(id);
    UNUSED(status);
    bth_gattc_conn_parameter_update((bth_address_t*)addr, connection_interval, connection_interval,peripheral_latency, supervision_timeout, 0,8);
}