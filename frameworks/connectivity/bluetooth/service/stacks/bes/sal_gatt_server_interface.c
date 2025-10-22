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
#include "sal_bes.h"
#include "bt_status.h"
#include "bluetooth.h"
#include "utils/log.h"
#include "bt_list.h"

#include "sal_gatt_server_interface.h"
#include "api/bth_api_gatt_server.h"

#define GATTS_MAX_PACKET 10


/***************************** external declaration *****************************/

/***************************** macro defination *******************************/

/*****************************  type defination ********************************/
#define GATTS_MAX_SEND_PACKET 10

typedef struct
{
    bool in_use;
    int conn_id;
    int packet_sent;
    bth_address_t addr;
} gatts_device_t;

typedef struct
{
    size_t                  count;
    bth_gatt_attr_t      elements[0];
} gatts_sal_service_t;

typedef struct
{
    bool     in_use;
    uint16_t element_id;
    uint16_t handle;
    int      trans_id;
} gatts_op_t;

typedef struct
{
    bool            init;
    int             server_if;
    gatts_device_t  device[CONFIG_BLUETOOTH_GATTS_MAX_CONNECTIONS];
    bt_list_t*      services; //gatts_sal_service_t
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
} gatts_env_t;

/*****************************  variable defination *****************************/
static gatts_env_t sal_gatts_env = {0};

/*****************************  function declaration ****************************/

static const char* attr_type_to_short_string(bth_gatt_attribute_type_t  type)
{
    switch(type)
    {
        case BTH_GATT_DB_PRIMARY_SERVICE: return "PRIM";
        case BTH_GATT_DB_SECONDARY_SERVICE: return "SECO";
        case BTH_GATT_DB_INCLUDED_SERVICE: return "INCL";
        case BTH_GATT_DB_CHARACTERISTIC: return "CHAR";
        case BTH_GATT_DB_DESCRIPTOR: return "DESC";
        default: return "UNKN";
    }
}

static void gattc_dump_db(bth_gatt_attr_t* buffer, uint16_t size)
{
    bth_gatt_attr_t* el = buffer;
    char dump_str[100];
    char uuid_str[UUID_STR_LEN];

    if (buffer == NULL)
    {
        return;
    }

    for(uint16_t i = 0; i < size; i++, el++)
    {
        uuid_to_string(&el->uuid, uuid_str, UUID_STR_LEN);
        snprintf(dump_str, sizeof(dump_str), "[id:%04x][handle:%04X][%s][%s]", el->id,
                 el->attribute_handle, attr_type_to_short_string(el->type), uuid_str);
        if (el->type == BTH_GATT_DB_PRIMARY_SERVICE)
        {
            LOG_D(">>%s", dump_str);
        }
        else if (el->type == BTH_GATT_DB_SECONDARY_SERVICE || el->type == BTH_GATT_DB_INCLUDED_SERVICE)
        {
            LOG_D(">>  %s", dump_str);
        }
        else if (el->type == BTH_GATT_DB_CHARACTERISTIC)
        {
            LOG_D(">>    %s", dump_str);
        }
        else if (el->type == BTH_GATT_DB_DESCRIPTOR)
        {
            LOG_D(">>       %s", dump_str);
        }
    }
}

static void free_service(void* data)
{
    if (data != NULL)
    {
        free(data);
    }
}

static gatts_env_t* get_gatts_env()
{
    gatts_env_t* gatts = &sal_gatts_env;
    return gatts;
}

static bool find_service_by_id(void* data, void* context)
{
    gatts_sal_service_t* serivce = data;
    uint16_t id = *(uint16_t*) context;
    return serivce->elements[0].id == id;
}

static bool find_service_by_handle(void* data, void* context)
{
    gatts_sal_service_t* serivce = data;
    uint16_t handle = *(uint16_t*) context;
    return serivce->elements[0].attribute_handle == handle;
}

static bth_gatt_attr_t* gatts_get_service_element_by_id(gatts_sal_service_t* service, uint16_t id)
{
    bth_gatt_attr_t* element = NULL;
    if (service == NULL)
    {
        LOG_E("Service is NULL");
        return NULL;
    }
    for (size_t i = 0; i < service->count; i++)
    {
        element = service->elements + i;
        if (element->id == id)
        {
            return element;
        }
    }
    LOG_E("Can't find service element for id %d", id);
    return NULL;
}

static void gatts_add_device(uint16_t conn_id, const bth_address_t* addr)
{
    gatts_env_t* gatts = get_gatts_env();
    gatts_device_t* device = NULL;
    int not_used = 0xFFFF;

    for(int i = 0; i < CONFIG_BLUETOOTH_GATTS_MAX_CONNECTIONS; i++)
    {
        device = gatts->device + i;
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
        device = gatts->device + not_used;
        device->conn_id = conn_id;
        device->addr = *addr;
        device->in_use = true;
    }
    else
    {
        LOG_E("Reach max connections");
    }
}

static void gatts_remove_device(uint16_t conn_id)
{
    gatts_env_t* gatts = get_gatts_env();
    gatts_device_t* device = NULL;
    for(int i = 0; i < CONFIG_BLUETOOTH_GATTS_MAX_CONNECTIONS; i++)
    {
        device = gatts->device + i;
        if (device->in_use && device->conn_id == conn_id)
        {
            device->in_use = false;
            return;
        }
    }
    LOG_E("Can't find device for conn id %d", conn_id);
    return;
}

static gatts_device_t* gatts_get_device(bth_address_t* addr, uint16_t conn_id)
{
    gatts_env_t* gatts = get_gatts_env();
    gatts_device_t* device = NULL;
    for(int i = 0; i < CONFIG_BLUETOOTH_GATTS_MAX_CONNECTIONS; i++)
    {
        device = &gatts->device[i];
        if (device->in_use)
        {
            if (device->in_use &&
                ((addr != NULL && is_bt_address_equal(addr, &device->addr)) ||
                 device->conn_id == conn_id))
            {
                return device;
            }
        }
    }
    return NULL;
}

static bth_address_t* gatts_get_addr_by_conn_id(uint16_t conn_id)
{
    gatts_env_t* gatts = get_gatts_env();
    gatts_device_t* device = NULL;
    for(int i = 0; i < CONFIG_BLUETOOTH_GATTS_MAX_CONNECTIONS; i++)
    {
        device = gatts->device + i;
        if (device->in_use && device->conn_id == conn_id)
        {
            return &device->addr;
        }
    }
    LOG_E("Can't find device for conn id %d", conn_id);
    return NULL;
}

static uint16_t gatts_get_conn_id_by_addr(bth_address_t* addr)
{
    gatts_env_t* gatts = get_gatts_env();
    gatts_device_t* device = NULL;
    for(int i = 0; i < CONFIG_BLUETOOTH_GATTS_MAX_CONNECTIONS; i++)
    {
        device = gatts->device + i;
        if(device->in_use && is_bt_address_equal(addr, &device->addr))
        {
            return device->conn_id;
        }
    }
    LOG_E("Can't find device for addr " STRMAC, MAC2STR(addr->address));
    return 0;
}

static uint16_t gatts_get_element_id_by_handle(uint16_t handle)
{
    gatts_env_t* gatts = get_gatts_env();
    gatts_sal_service_t* service = NULL;
    bt_list_t* list = gatts->services;
    bt_list_node_t* node = NULL;
    for (node = bt_list_head(list); node != NULL; node = bt_list_next(list, node))
    {
        service = bt_list_node(node);
        for (size_t i = 0; i < service->count; i++)
        {
            if (service->elements[i].attribute_handle == handle)
            {
                return service->elements[i].id;
            }
        }
    }
    LOG_D("Can't find element id for handle %d", handle);
    return 0;
}

static uint16_t gatts_get_handle_by_element_id(uint16_t id)
{
    gatts_env_t* gatts = get_gatts_env();
    gatts_sal_service_t* service = NULL;
    bt_list_t* list = gatts->services;
    bt_list_node_t* node = NULL;
    for (node = bt_list_head(list); node != NULL; node = bt_list_next(list, node))
    {
        service = bt_list_node(node);
        for (size_t i = 0; i < service->count; i++)
        {
            if (service->elements[i].id == id)
            {
                return service->elements[i].attribute_handle;
            }
        }
    }
    return 0;
}

static bt_status_t gatts_send_ind_or_ntf(int server_if, int attribute_handle, bt_address_t* addr, bool confirm, const uint8_t* value, int length)
{
    gatts_device_t* device = gatts_get_device((bth_address_t*)addr, 0);
    bth_bt_status_t status;
    if (device == NULL)
    {
        LOG_E("Device not connected!");
        return BT_STATUS_FAIL;
    }
    status = bth_gatts_send_indication_or_notify(server_if, attribute_handle,
                                    device->conn_id, confirm,
                                    value, length);
    if(status != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }
    return BT_STATUS_SUCCESS;
}


static void bes_sal_gatts_reg_server_cb(int status, int server_if,
                                         const bth_uuid_t* app_uuid)
{
    gatts_env_t* gatts = get_gatts_env();
    UNUSED(app_uuid);

    if (status == BTH_STATUS_SUCCESS)
    {
        LOG_E("Server if %d", server_if);
        gatts->server_if = server_if;
        return;
    }
    else
    {
        LOG_E("Register service failed!");
    }
}

static void bes_sal_gatts_conn_cb(int conn_id, int server_if, const bth_address_t* bda)
{
    gatts_env_t* gatts = get_gatts_env();
    LOG_D("device connected id %d server if %d-%d address " STRMAC, conn_id, server_if, gatts->server_if, MAC2STR(bda->address));
    if (server_if == gatts->server_if)
    {
        gatts_add_device(conn_id, bda);
        if_gatts_on_connection_state_changed((bt_address_t*)bda, PROFILE_STATE_CONNECTED);
    }
}

static void bes_sal_gatts_disconnected_cb(int conn_id, int server_if, const bth_address_t* bda)
{
    gatts_env_t* gatts = get_gatts_env();
    LOG_D("device disconnected id %d server if %d-%d address " STRMAC, conn_id, server_if, gatts->server_if, MAC2STR(bda->address));

    if (server_if == gatts->server_if)
    {
        gatts_remove_device(conn_id);
        if_gatts_on_connection_state_changed((bt_address_t*)bda, PROFILE_STATE_DISCONNECTED);
    }
}

static void bes_sal_gatts_service_added_cb(int status, int server_if,
                                       const bth_gatt_attr_t* elements,
                                       int service_count)
{
    gatts_env_t* gatt_env = get_gatts_env();
    gatts_sal_service_t* service = NULL;
    bth_gatt_attr_t* element = NULL;
    uint16_t element_id = 0;

    if (status != BTH_GATT_SUCCESS)
    {
        LOG_E("Add service failed status %d", status);
        return;
    }
    if (server_if != gatt_env->server_if)
    {
        LOG_E("This interface not registerd for %d", server_if);
        return;
    }
    if (elements == NULL || service_count == 0)
    {
        LOG_E("No valid elements!");
        if_gatts_on_elements_added(GATT_STATUS_FAILURE, 0, 0);
        return;
    }

    element_id = elements[0].id;
    service = bt_list_find(gatt_env->services, find_service_by_id, &element_id);
    if (service == NULL)
    {
        LOG_E("Can't find service for id %d", element_id);
        return;
    }

    for (int i = 0; i < service_count; i++)
    {
        element = gatts_get_service_element_by_id(service, elements[i].id);
        if (element == NULL)
        {
            continue;
        }
        element->attribute_handle = elements[i].attribute_handle;
    }
    gattc_dump_db(service->elements, service->count);
    if_gatts_on_elements_added(GATT_STATUS_SUCCESS, element_id, service_count);
}

static void bes_sal_gatts_service_stopped_cb(int status, int server_if,
                                             int srvc_handle)
{
    UNUSED(status);
    UNUSED(server_if);
    UNUSED(srvc_handle);
    LOG_E("server if %d status %d service handle %d", server_if, status, srvc_handle);
}

static void bes_sal_gatts_service_deleted_cb(int status, int server_if, int srvc_handle)
{
    gatts_env_t* gatts = get_gatts_env();
    uint16_t element_id = 0;
    uint16_t size = 0;
    gatts_sal_service_t* service = bt_list_find(gatts->services, find_service_by_handle, &srvc_handle);

    if (gatts->server_if != server_if)
    {
        LOG_E("server if not mached!");
        return;
    }

    if (service != NULL)
    {
        element_id = service->elements[0].id;
        size = service->count;
        bt_list_remove(gatts->services, service);
    }
    else
    {
        LOG_E("Can't find service for handle %d", srvc_handle);
    }
    LOG_E("ele id %d handle %d", element_id, srvc_handle);
    if_gatts_on_elements_removed(to_gatt_status(status), element_id, size);
}

static void bes_sal_gatts_req_read_char_cb(int conn_id, int trans_id,
                                  const bth_address_t* bda, int attr_handle,
                                  int offset, bool is_long)
{
    uint16_t element_id = gatts_get_element_id_by_handle(attr_handle);
    UNUSED(conn_id);
    UNUSED(attr_handle);
    UNUSED(offset);
    UNUSED(is_long);

    if_gatts_on_received_element_read_request((bt_address_t*)bda, trans_id, element_id);
}

static void bes_sal_gatts_req_read_desc_cb(int conn_id, int trans_id,
                                  const bth_address_t* bda, int attr_handle,
                                  int offset, bool is_long)
{
    uint16_t element_id = gatts_get_element_id_by_handle(attr_handle);
    UNUSED(conn_id);
    UNUSED(offset);
    UNUSED(is_long);

    if_gatts_on_received_element_read_request((bt_address_t*)bda, trans_id, element_id);
}

static void bes_sal_gatts_req_write_char_cb(int conn_id, int trans_id,
                                   const bth_address_t* bda, int attr_handle,
                                   int offset, bool need_rsp, bool is_prep,
                                   const uint8_t* value, int length)
{
    uint16_t element_id = gatts_get_element_id_by_handle(attr_handle);
    UNUSED(conn_id);
    UNUSED(need_rsp);
    UNUSED(is_prep);

    if_gatts_on_received_element_write_request((bt_address_t*)bda,trans_id, element_id,(uint8_t*)value, offset, length);
}

static void bes_sal_gatts_req_write_desc_cb(int conn_id, int trans_id,
                                   const bth_address_t* bda, int attr_handle,
                                   int offset, bool need_rsp, bool is_prep,
                                   const uint8_t* value, int length)
{
    uint16_t element_id = gatts_get_element_id_by_handle(attr_handle);
    UNUSED(conn_id);
    UNUSED(need_rsp);
    UNUSED(is_prep);

    if_gatts_on_received_element_write_request((bt_address_t*)bda, trans_id, element_id, (uint8_t*)value, offset, length);
}

static void bes_sal_gatts_req_exec_write_cb(int conn_id, int trans_id,
                                        const bth_address_t* bda,
                                        int exec_write)
{
    LOG_D("");
    UNUSED(conn_id);
    UNUSED(trans_id);
    UNUSED(bda);
    UNUSED(exec_write);
}

static void bes_sal_gatts_rsp_confirm_cb(int status, int handle)
{
    LOG_D("");
    UNUSED(status);
    UNUSED(handle);
}

static void bes_sal_gatts_ind_sent_cb(int conn_id, int handle, int status)
{
#if 0
    LOG_D("");
    bth_address_t* addr = gatts_get_addr_by_conn_id(conn_id);
    uint16_t el_id = gatts_get_element_id_by_handle(handle);
    if (addr == NULL)
    {
        LOG_E("Can't find connected device for %d", conn_id);
        return;
    }
    if_gatts_on_notification_sent(TO_BT_ADDRESS(addr), el_id, to_gatt_status(status));
#endif
    gatts_env_t* gatts = get_gatts_env();
    gatts_device_t* device = gatts_get_device(NULL, conn_id);
    UNUSED(handle);
    UNUSED(status);
    if (device == NULL)
    {
        LOG_E("Device not connected!");
        return;
    }
}

static void bes_sal_gatts_congestion_cb(int conn_id, bool congested)
{
    LOG_D("");
    UNUSED(conn_id);
    UNUSED(congested);
}

static void bes_sal_gatts_mtu_changed_cb(int conn_id, int mtu)
{
    LOG_D("Updated MTU: mtu: %d bytes\n", mtu);
    bt_address_t* addr = (bt_address_t*)gatts_get_addr_by_conn_id(conn_id);
    if (addr == NULL)
    {
        LOG_E("Can't find connected device for %d", conn_id);
        return;
    }
    if_gatts_on_mtu_changed(addr, mtu);
}

static void bes_sal_gatts_phy_updated_cb(int conn_id, uint8_t tx_phy,
                                     uint8_t rx_phy, uint8_t status)
{
    bt_address_t* addr = (bt_address_t*)gatts_get_addr_by_conn_id(conn_id);
    if (addr == NULL)
    {
        LOG_E("Can't find connected device for %d", conn_id);
        return;
    }
    if_gatts_on_phy_updated(addr, tx_phy, rx_phy, status);
}

static void bes_sal_gatts_conn_updated_cb(int conn_id, uint16_t interval,
                                      uint16_t latency, uint16_t timeout,
                                      uint8_t status)
{

    bt_address_t* addr = (bt_address_t*)gatts_get_addr_by_conn_id(conn_id);
    UNUSED(status);
    if (addr == NULL)
    {
        LOG_E("Can't find connected device for %d", conn_id);
        return;
    }
    if_gatts_on_connection_parameter_changed(addr, interval, latency, timeout);
}

static void bes_sal_gatts_subrate_chg_cb(int conn_id, uint16_t subrate_factor,
                                     uint16_t latency, uint16_t cont_num,
                                     uint16_t timeout, uint8_t status)
{
    LOG_D("");
    UNUSED(conn_id);
    UNUSED(subrate_factor);
    UNUSED(latency);
    UNUSED(cont_num);
    UNUSED(timeout);
    UNUSED(status);
}

static void bes_sal_gatts_read_phy_cb(const bth_address_t* addr, uint8_t tx_phy,
                                  uint8_t rx_phy, uint8_t status)
{
    UNUSED(status);
    if_gatts_on_phy_read((bt_address_t *)addr, tx_phy, rx_phy);
}

static void bes_sal_gatts_read_rssi_cb(int server, const bth_address_t *bda,
                                         int rssi, int status)
{
    LOG_D("");
    UNUSED(server);
    UNUSED(bda);
    UNUSED(rssi);
    UNUSED(status);
}

gatts_server_callbacks_t bes_sal_gatts =
{
    .register_server_cb = bes_sal_gatts_reg_server_cb,
    .open_cb            = bes_sal_gatts_conn_cb,
    .close_cb           = bes_sal_gatts_disconnected_cb,
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
    LOG_D("");
    gatts_env_t* gatts = get_gatts_env();
    bth_uuid_t uuid = uuid_get_random();

    pthread_mutex_init(&gatts->mutex, NULL);
    pthread_cond_init(&gatts->cond, NULL);

    memset(gatts->device, 0, sizeof(gatts->device));
    gatts->services = bt_list_new(free_service);

    bth_gatts_register_server(&uuid, false);

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_server_disable(void)
{
    gatts_env_t* gatts = get_gatts_env();

    bt_list_free(gatts->services);
    pthread_mutex_destroy(&gatts->mutex);
    pthread_cond_destroy(&gatts->cond);

    bth_gatts_unregister_server(gatts->server_if);

    return BT_STATUS_SUCCESS;
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

bt_status_t bt_sal_gatt_server_add_elements(gatt_element_t* elements, uint16_t size)
{
    gatts_env_t* gatts = get_gatts_env();
    bth_bt_status_t status;
    gatts_sal_service_t* service = NULL;
    bth_gatt_attr_t* to = NULL;
    gatt_element_t* from = NULL;

    if (bt_list_length(gatts->services) + 1 > GATT_MAX_SR_PROFILES)
    {
        LOG_E("reach max registered service %d", GATT_MAX_SR_PROFILES);
        return BT_STATUS_FAIL;
    }

    service = malloc(sizeof(gatts_sal_service_t) + sizeof(bth_gatt_attr_t) * size);
    if (service == NULL)
    {
        LOG_E("No resource!");
        return BT_STATUS_FAIL;
    }

    memset((uint8_t*) service, 0, sizeof(bth_gatt_attr_t) * size);

    to = service->elements;
    from = elements;

    for (uint16_t i = 0; i < size; i++, from++, to++)
    {
        switch (from->type) {
        case GATT_PRIMARY_SERVICE:
        {
            to->id = from->handle;
            to->uuid = uuid_from_128bit_LE((uint8_t*)&from->uuid.val);
            to->properties = 0;
            to->extended_properties = 0;
            to->permissions = 0;
            to->type = BTH_GATT_DB_PRIMARY_SERVICE;
            break;
        }
        case GATT_SECONDARY_SERVICE:
        {
            to->id = from->handle;
            to->uuid = uuid_from_128bit_LE((uint8_t*)&from->uuid.val);
            to->properties = 0;
            to->extended_properties = 0;
            to->permissions = 0;
            to->type = BTH_GATT_DB_SECONDARY_SERVICE;
            break;
        }
        case GATT_CHARACTERISTIC:
        {
            to->id = from->handle;
            to->uuid = uuid_from_128bit_LE((uint8_t*)&from->uuid.val);
            to->type = BTH_GATT_DB_CHARACTERISTIC;
            to->properties = bt_sal_gatt_server_get_properties(from->properties);
            to->extended_properties = 0;
            to->permissions = bt_sal_gatt_server_get_permissions(from->permissions);
            break;
        }

        case GATT_DESCRIPTOR:
        {
            to->id = from->handle;
            to->uuid = uuid_from_128bit_LE((uint8_t*)&from->uuid.val);
            to->type = BTH_GATT_DB_DESCRIPTOR;
            to->properties = 0;
            to->extended_properties = 0;
            to->permissions = bt_sal_gatt_server_get_permissions(from->permissions);
            break;
        }

        default:
            LOG_E("%s, type:%d not handle", __func__, from->type);
            break;
        }
    }

    status = bth_gatts_add_service(gatts->server_if, service->elements, size);
    LOG_D("status %d", status);
    gattc_dump_db(service->elements, service->count);
    if (status == BTH_STATUS_SUCCESS)
    {
        service->count = size;
        bt_list_add_tail(gatts->services, service);
        gattc_dump_db(service->elements, service->count);
        return BT_STATUS_SUCCESS;
    }
    free(service);
    return BT_STATUS_FAIL;
}

bt_status_t bt_sal_gatt_server_remove_elements(gatt_element_t* elements, uint16_t size)
{
    gatts_env_t* gatts = get_gatts_env();
    gatts_sal_service_t* service = bt_list_find(gatts->services, find_service_by_id, &elements[0].handle);
    UNUSED(size);

    LOG_D("id %d service handle %d", elements[0].handle, service->elements[0].attribute_handle);

    CHECK_BES_STACK_RETURN(bth_gatts_delete_service(gatts->server_if, service->elements[0].attribute_handle));
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_server_connect(bt_controller_id_t id, bt_address_t* addr, ble_addr_type_t addr_type)
{
    gatts_env_t* gatts = get_gatts_env();
    int transport = addr_type == BT_GATT_OVER_BR_EDR ? BTH_BT_TRANSPORT_BR_EDR : BTH_BT_TRANSPORT_LE;
    UNUSED(id);
    UNUSED(addr_type);
    CHECK_BES_STACK_RETURN(bth_gatts_connect(gatts->server_if, (const bth_address_t*)addr, false, transport))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_server_cancel_connection(bt_controller_id_t id, bt_address_t* addr)
{
    gatts_env_t* gatts = get_gatts_env();
    uint16_t conn_id = gatts_get_conn_id_by_addr((bth_address_t*)addr);
    UNUSED(id);

    CHECK_BES_STACK_RETURN(bth_gatts_disconnect(gatts->server_if, (const bth_address_t*)addr, conn_id))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_server_send_response(bt_controller_id_t id, bt_address_t* addr, uint32_t request_id, uint8_t* value, uint16_t length)
{
    bth_gatt_response_t response = {0};
    uint16_t conn_id = gatts_get_conn_id_by_addr((bth_address_t*)addr);
    UNUSED(id);

    response.value = value;
    response.len = length;

    CHECK_BES_STACK_RETURN(bth_gatts_send_response(conn_id, request_id, BTH_GATT_SUCCESS, &response))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_server_send_notification(bt_controller_id_t id, bt_address_t* addr, uint16_t element_id, uint8_t* value, uint16_t length)
{
    gatts_env_t* gatts = get_gatts_env();
    uint16_t handle = gatts_get_handle_by_element_id(element_id);
    UNUSED(id);
    return gatts_send_ind_or_ntf(gatts->server_if, handle, addr, false,
                                 value, length);
}

bt_status_t bt_sal_gatt_server_send_indication(bt_controller_id_t id, bt_address_t* addr, uint16_t element_id, uint8_t* value, uint16_t length)
{
    gatts_env_t* gatts = get_gatts_env();
    uint16_t handle = gatts_get_handle_by_element_id(element_id);
    UNUSED(id);

    return gatts_send_ind_or_ntf(gatts->server_if, handle, addr, true,
                                 value, length);
}

bt_status_t bt_sal_gatt_server_read_phy(bt_controller_id_t id, bt_address_t* addr)
{
    UNUSED(id);
    if (addr == NULL)
    {
        LOG_E("address is null!");
        return BT_STATUS_FAIL;
    }
    CHECK_BES_STACK_RETURN(bth_gatts_read_phy((bth_address_t*)addr))
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_gatt_server_set_phy(bt_controller_id_t id, bt_address_t* addr, ble_phy_type_t tx_phy, ble_phy_type_t rx_phy)
{
    UNUSED(id);
    CHECK_BES_STACK_RETURN(bth_gatts_set_preferred_phy((bth_address_t*)addr, tx_phy, rx_phy, 0))
    return BT_STATUS_SUCCESS;
}

void bt_sal_gatt_server_connection_changed_callback(bt_address_t* addr, uint16_t connection_interval, uint16_t peripheral_latency,
                                                    uint16_t supervision_timeout)
{
    bth_gatts_conn_parameter_update((bth_address_t*)addr, connection_interval,
                           connection_interval, peripheral_latency,
                           supervision_timeout, 0, 0);
}
