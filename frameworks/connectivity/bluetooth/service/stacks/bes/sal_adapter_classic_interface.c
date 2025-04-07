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

/****************************** header include ********************************/
#define LOG_TAG "sal_bes_bredr"

#include <stdbool.h> // For bool type
#include "adapter_internel.h"
#include "sal_interface.h"
#include "sal_bes.h"
#include "bt_status.h"
#include "bluetooth.h"
#include "utils/log.h"

#include "sal_adapter_classic_interface.h"

#include "api/bth_api_bluetooth.h"

/***************************** external declaration *****************************/

/***************************** macro defination *******************************/

/***************************** type defination ********************************/

typedef struct 
{
    bool scan_bondable;
    bool auto_accept_conn;
    bt_vhal_interface* vhal_cb;
} bt_sal_classic_env_t;

/***************************** variable defination *****************************/
bt_sal_classic_env_t  bt_sal_classic_env = {0};

/***************************** function declaration ****************************/
const char* bt_sal_get_name(bt_controller_id_t id)
{
    UNUSED(id);
    char* name = NULL;
    int   name_len = 0;

    if (BT_STATUS_SUCCESS != bt_sal_set_async_send_check())
    {
        return NULL;
    }

    bluetooth_get_adapter_property(BTH_PROPERTY_BDNAME);

    bt_sal_get_async_info((uint8_t **)&name, &name_len);

    return name;
}

bt_status_t bt_sal_set_name(bt_controller_id_t id, char* name)
{
    UNUSED(id);
    bt_status_t ret = BT_STATUS_SUCCESS;
    bth_bt_property_t param = {0};

    if (BT_STATUS_SUCCESS != bt_sal_set_async_send_check())
    {
        return BT_STATUS_BUSY;
    }

    param.type = BTH_PROPERTY_BDNAME;
    param.len  = strlen(name);
    param.val  = (uint8_t *)name;
    bluetooth_set_adapter_property(&param);

    ret = bt_sal_get_async_info(NULL,0);

    return ret;
}

bt_status_t bt_sal_get_address(bt_controller_id_t id, bt_address_t* addr)
{
    UNUSED(id);
    uint8_t* get_addr = NULL;
    int get_addr_len = 0;
    int ret = BT_STATUS_SUCCESS;

    if (BT_STATUS_SUCCESS != bt_sal_set_async_send_check())
    {
        return BT_STATUS_BUSY;
    }

    bluetooth_get_adapter_property(BTH_PROPERTY_BDADDR);
    ret = bt_sal_get_async_info(&get_addr, &get_addr_len);
    if(get_addr && get_addr_len && (ret == BT_STATUS_SUCCESS))
    {
        memcpy(addr, get_addr, sizeof(bt_address_t));
    }

    return ret;
}

bt_status_t bt_sal_set_io_capability(bt_controller_id_t id, bt_io_capability_t cap)
{
    UNUSED(id);
    uint8_t io_cap = BTH_IO_CAP_UNKNOWN;
    bt_status_t ret = BT_STATUS_SUCCESS;
    bth_bt_property_t param = {0};

    switch(cap)
    {
        case BT_IO_CAPABILITY_DISPLAYONLY:
            io_cap = BTH_IO_CAP_OUT;
            break;
        case BT_IO_CAPABILITY_DISPLAYYESNO:
            io_cap = BTH_IO_CAP_IO;
            break;
        case BT_IO_CAPABILITY_KEYBOARDONLY:
            io_cap = BTH_IO_CAP_IN;
            break;
        case BT_IO_CAPABILITY_NOINPUTNOOUTPUT:
            io_cap = BTH_IO_CAP_NONE;
            break;
        case BT_IO_CAPABILITY_KEYBOARDDISPLAY:
            io_cap = BTH_IO_CAP_KBDISP;
            break;
        default:
            BT_LOGE("cap  = %d", cap);
            return BT_STATUS_PARM_INVALID;
    }

    if (BT_STATUS_SUCCESS != bt_sal_set_async_send_check())
    {
        return BT_STATUS_BUSY;
    }

    param.type = BTH_PROPERTY_LOCAL_IO_CAPS;
    param.len  = sizeof(io_cap);
    param.val  = &io_cap;
    bluetooth_set_adapter_property(&param);

    ret = bt_sal_get_async_info(NULL,0);

    return ret;
}

bt_io_capability_t bt_sal_get_io_capability(bt_controller_id_t id)
{
    UNUSED(id);
    uint8_t* get_param = NULL;
    int get_param_len = 0;
    int ret = BT_STATUS_SUCCESS;
    bt_io_capability_t io_cap = BT_IO_CAPABILITY_UNKNOW;

    if (BT_STATUS_SUCCESS != bt_sal_set_async_send_check())
    {
        return BT_IO_CAPABILITY_UNKNOW;
    }

    bluetooth_get_adapter_property(BTH_PROPERTY_BDADDR);
    ret = bt_sal_get_async_info(&get_param, &get_param_len);
    if (!(get_param && get_param_len && (ret == BT_STATUS_SUCCESS)))
    {
        return io_cap;
    }

    switch (*get_param)
    {
        case BTH_IO_CAP_OUT:
            io_cap = BT_IO_CAPABILITY_DISPLAYONLY;
            break;
        case BTH_IO_CAP_IO:
            io_cap = BT_IO_CAPABILITY_DISPLAYYESNO;
            break;
        case BTH_IO_CAP_IN:
            io_cap = BT_IO_CAPABILITY_KEYBOARDONLY;
            break;
        case BTH_IO_CAP_NONE:
            io_cap = BT_IO_CAPABILITY_NOINPUTNOOUTPUT;
            break;
        case BTH_IO_CAP_KBDISP:
            io_cap = BT_IO_CAPABILITY_KEYBOARDDISPLAY;
            break;
        default:
            io_cap = BT_IO_CAPABILITY_UNKNOW;
    }

    return io_cap;
}

bt_status_t bt_sal_set_device_class(bt_controller_id_t id, uint32_t cod)
{
    UNUSED(id);
    bt_status_t ret = BT_STATUS_SUCCESS;
    bth_bt_property_t param = {0};

    if (BT_STATUS_SUCCESS != bt_sal_set_async_send_check())
    {
        return BT_STATUS_BUSY;
    }

    param.type = BTH_PROPERTY_CLASS_OF_DEVICE;
    param.len  = sizeof(cod);
    param.val  = (uint8_t *)&cod;
    bluetooth_set_adapter_property(&param);

    ret = bt_sal_get_async_info(NULL,0);

    return ret;
}

uint32_t bt_sal_get_device_class(bt_controller_id_t id)
{
    UNUSED(id);
    uint8_t* get_param = NULL;
    uint32_t cod = 0;
    int get_param_len = 0;
    int ret = BT_STATUS_SUCCESS;

    if (BT_STATUS_SUCCESS != bt_sal_set_async_send_check())
    {
        return BT_STATUS_BUSY;
    }

    bluetooth_get_adapter_property(BTH_PROPERTY_CLASS_OF_DEVICE);
    ret = bt_sal_get_async_info(&get_param, &get_param_len);
    if (get_param && get_param_len && (get_param_len == sizeof(cod)) &&
        (ret == BT_STATUS_SUCCESS))
    {
        cod = *((uint32_t *)get_param);
    }

    return cod;
}

bool bt_sal_set_bondable(bt_controller_id_t id, bool bondable)
{
    UNUSED(id);
    BT_LOGD("[%s][%d]: bondable=%d", __FUNCTION__, __LINE__, bondable);
    bt_sal_classic_env.scan_bondable = bondable;
    return true;
}

bool bt_sal_get_bondable(bt_controller_id_t id)
{
    UNUSED(id);
    BT_LOGD("[%s][%d]: ", __FUNCTION__, __LINE__);
    return bt_sal_classic_env.scan_bondable;
}

bt_status_t bt_sal_set_scan_mode(bt_controller_id_t id, bt_scan_mode_t scan_mode, bool bondable)
{
    UNUSED(id);
    bth_bt_scan_mode_t set_scan_mode = BTH_SCAN_MODE_NONE;
    bt_status_t ret = BT_STATUS_SUCCESS;
    bth_bt_property_t param = {0};

    switch(scan_mode)
    {
        case BT_SCAN_MODE_NONE:
            set_scan_mode = BTH_SCAN_MODE_NONE;
            break;
        case BT_SCAN_MODE_CONNECTABLE:
            set_scan_mode = BTH_SCAN_MODE_CONNECTABLE;
            break;
        case BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE:
            set_scan_mode = BTH_SCAN_MODE_CONNECTABLE_DISCOVERABLE;
            break;
        default:
            BT_LOGE("cap  = %d", scan_mode);
            return BT_STATUS_PARM_INVALID;
    }

    if (BT_STATUS_SUCCESS != bt_sal_set_async_send_check())
    {
        return BT_STATUS_BUSY;
    }

    param.type = BTH_PROPERTY_ADAPTER_SCAN_MODE;
    param.len  = sizeof(set_scan_mode);
    param.val  = (uint8_t *)&set_scan_mode;
    bluetooth_set_adapter_property(&param);
    ret = bt_sal_get_async_info(NULL,0);
    if (ret != BT_STATUS_SUCCESS)
    {
        return ret;
    }

    if (bt_sal_set_bondable(id, bondable))
    {
        ret = BT_STATUS_SUCCESS;
    }
    else
    {
        ret = BT_STATUS_FAIL;
    }

    return ret;
}

bt_scan_mode_t bt_sal_get_scan_mode(bt_controller_id_t id)
{
    UNUSED(id);
    uint8_t* get_param = NULL;
    int get_param_len = 0;
    int ret = BT_STATUS_SUCCESS;
    uint8_t scan_mode = BTH_SCAN_MODE_NONE;

    if (BT_STATUS_SUCCESS != bt_sal_set_async_send_check())
    {
        return BT_SCAN_MODE_NONE;
    }

    bluetooth_get_adapter_property(BTH_PROPERTY_ADAPTER_SCAN_MODE);
    ret = bt_sal_get_async_info(&get_param, &get_param_len);
    if (!(get_param && get_param_len && (ret == BT_STATUS_SUCCESS)))
    {
        return scan_mode;
    }

    switch (*get_param)
    {
        case BTH_SCAN_MODE_NONE:
            scan_mode = BT_SCAN_MODE_NONE;
            break;
        case BTH_SCAN_MODE_CONNECTABLE:
            scan_mode = BT_SCAN_MODE_CONNECTABLE;
            break;
        case BTH_SCAN_MODE_CONNECTABLE_DISCOVERABLE:
        case BTH_SCAN_MODE_CONNECTABLE_LIMITED_DISCOVERABLE:
            scan_mode = BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE;
            break;
        default:
            BT_LOGE("scan_mode  = %d", scan_mode);
            scan_mode = BTH_SCAN_MODE_NONE;
    }

    return scan_mode;
}

/* Inquiry/page and inquiry/page scan */
bt_status_t bt_sal_start_discovery(bt_controller_id_t id, uint32_t timeout)
{
    UNUSED(id);
    int bes_ret = 0;
    bt_status_t ret = BT_STATUS_SUCCESS;
    bth_bt_property_t param = {0};

    if (BT_STATUS_SUCCESS != bt_sal_set_async_send_check())
    {
        return BT_STATUS_BUSY;
    }

    param.type = BTH_PROPERTY_ADAPTER_DISCOVERABLE_TIMEOUT;
    param.len  = sizeof(timeout);
    param.val  = (uint8_t*)&timeout;
    bluetooth_set_adapter_property(&param);
    ret = bt_sal_get_async_info(NULL,0);
    if (ret != BT_STATUS_SUCCESS)
    {
        return ret;
    }

    bes_ret = bluetooth_start_discovery();
    if (bes_ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_stop_discovery(bt_controller_id_t id)
{
    UNUSED(id);
    int bes_ret = 0;

    bes_ret = bluetooth_cancel_discovery();
    if (bes_ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_set_page_scan_parameters(bt_controller_id_t id, bt_scan_type_t type,
                                            uint16_t interval, uint16_t window)
{
    UNUSED(id);
    return BT_STATUS_UNSUPPORTED;
}

bt_status_t bt_sal_set_inquiry_scan_parameters(bt_controller_id_t id, bt_scan_type_t type,
                                               uint16_t interval, uint16_t window)
{
    return BT_STATUS_UNSUPPORTED;
}

/* Remote device RNR/connection/bond/properties */
bt_status_t bt_sal_get_remote_name(bt_controller_id_t id, bt_address_t* addr)
{
    UNUSED(id);
    int bes_ret = 0;

    bes_ret = bluetooth_get_remote_device_property((bth_address_t *)addr, BTH_PROPERTY_BDNAME);
    if (bes_ret != BTH_STATUS_SUCCESS)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_auto_accept_connection(bt_controller_id_t id, bool enable)
{
    UNUSED(id);
    bt_sal_classic_env.auto_accept_conn = enable;
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_sco_connection_reply(bt_controller_id_t id, bt_address_t* addr, bool accept)
{
    UNUSED(id);
    bluetooth_sco_req_reply((const bth_address_t*)addr, accept);
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_acl_connection_reply(bt_controller_id_t id, bt_address_t* addr, bool accept)
{
    UNUSED(id);
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_pair_reply(bt_controller_id_t id, bt_address_t* addr, uint8_t reason)
{

    UNUSED(id);
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_ssp_reply(bt_controller_id_t id, bt_address_t* addr,
                             bool accept, bt_pair_type_t type, uint32_t passkey)
{
    UNUSED(id);
    int ret;
    bth_bt_ssp_variant_t variant;

    switch (type)
    {
        case PAIR_TYPE_PASSKEY_CONFIRMATION:
            variant = BTH_BT_SSP_VARIANT_PASSKEY_CONFIRMATION;
            break;
        case PAIR_TYPE_PASSKEY_ENTRY:
            variant = BTH_BT_SSP_VARIANT_PASSKEY_ENTRY;
            break;
        case PAIR_TYPE_CONSENT :
            variant = BTH_BT_SSP_VARIANT_CONSENT;
            break;
        case PAIR_TYPE_PASSKEY_NOTIFICATION:
            variant = BTH_BT_SSP_VARIANT_PASSKEY_NOTIFICATION;
            break;
        default:
            BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, type);
            return BT_STATUS_PARM_INVALID;
    }

    ret = bluetooth_ssp_reply((bth_address_t*)addr, variant, accept, passkey);
    if (ret != BTH_STATUS_SUCCESS)
    {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_pin_reply(bt_controller_id_t id, bt_address_t* addr,
                             bool accept, char* pincode, int len)
{
    UNUSED(id);
    int ret;

    ret = bluetooth_pin_reply((bth_address_t*)addr, accept, len, (bth_bt_pin_code_t*)pincode);
    if (ret != BTH_STATUS_SUCCESS)
    {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

connection_state_t bt_sal_get_connection_state(bt_controller_id_t id, bt_address_t* addr)
{
    UNUSED(id);
    connection_state_t ret_state;

    if (bluetooth_get_connection_state((bth_address_t*)addr))
    {
        ret_state = CONNECTION_STATE_CONNECTED;
    }
    else
    {
        ret_state = CONNECTION_STATE_DISCONNECTED;
    }

    return ret_state;
}

uint16_t bt_sal_get_acl_connection_handle(bt_controller_id_t id, bt_address_t* addr, bt_transport_t transport)
{
    UNUSED(id);
    return 0; // Replace with actual default handle
}

uint16_t bt_sal_get_sco_connection_handle(bt_controller_id_t id, bt_address_t* addr)
{
    UNUSED(id);
    return 0;
}

bt_status_t bt_sal_connect(bt_controller_id_t id, bt_address_t* addr)
{
    UNUSED(id);
    int ret;

    ret = bluetooth_connect((bth_address_t*)addr);
    if (ret != BTH_STATUS_SUCCESS)
    {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_disconnect(bt_controller_id_t id, bt_address_t* addr, uint8_t reason)
{
    UNUSED(id);
    int ret;

    ret = bluetooth_disconnect((bth_address_t*)addr);
    if (ret != BTH_STATUS_SUCCESS)
    {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_create_bond(bt_controller_id_t id, bt_address_t* addr, bt_transport_t transport, bt_addr_type_t type)
{
    UNUSED(id);
    if (transport == BT_TRANSPORT_BLE)
    {
        bluetooth_create_bond_le((bth_address_t*)addr, type);
    }
    else
    {
        bluetooth_create_bond((bth_address_t*)addr, transport);
    }

    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_cancel_bond(bt_controller_id_t id, bt_address_t* addr, bt_transport_t transport)
{
    UNUSED(id);
    int ret;

    ret = bluetooth_cancel_bond((bth_address_t*)addr);
    if (ret != BTH_STATUS_SUCCESS)
    {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_remove_bond(bt_controller_id_t id, bt_address_t* addr, bt_transport_t transport)
{
    UNUSED(id);
    int ret;

    ret =bluetooth_remove_bond((bth_address_t*)addr);
    if (ret != BTH_STATUS_SUCCESS)
    {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_set_remote_oob_data(bt_controller_id_t id, bt_address_t* addr,
                                       bt_oob_data_t* p192_val, bt_oob_data_t* p256_val)
{
    UNUSED(id);
    int ret;

    ret = bluetooth_create_bond_out_of_band((bth_address_t*)addr, 1,
        (bth_bt_oob_data_t*)p192_val, (bth_bt_oob_data_t*)p256_val);
    if (ret != BTH_STATUS_SUCCESS)
    {
        BT_LOGE("[%s][%d]: %d", __FUNCTION__, __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_remove_remote_oob_data(bt_controller_id_t id, bt_address_t* addr)
{
    UNUSED(id);
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_get_local_oob_data(bt_controller_id_t id) {
    UNUSED(id);
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_get_remote_device_info(bt_controller_id_t id, bt_address_t* addr, remote_device_properties_t* properties)
{
    
    UNUSED(id);
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_set_bonded_devices(bt_controller_id_t id, remote_device_properties_t* props, int cnt)
{
    UNUSED(id);
    if (props == NULL)
    {
        return BT_STATUS_FAIL;
    }
    for (int i = 0; i < cnt; i ++)
    {
        CHECK_BES_STACK_RETURN(bluetooth_add_bond_device(TO_BTH_ADDRESS(&props->addr), *(link_key_t *)props->link_key,
                                                         props->link_key_type, 0));
    }
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_get_bonded_devices(bt_controller_id_t id, remote_device_properties_t* props, int* cnt)
{
    UNUSED(id);
    uint8_t* get_param = NULL;
    int get_param_len = 0;
    int ret = BT_STATUS_SUCCESS;
    if (BT_STATUS_SUCCESS != bt_sal_set_async_send_check())
    {
        return BT_STATUS_BUSY;
    }

    bluetooth_get_adapter_property(BTH_PROPERTY_ADAPTER_BONDED_DEVICES);
    ret = bt_sal_get_async_info(&get_param, &get_param_len);
    if (!(get_param && get_param_len && (ret == BT_STATUS_SUCCESS)))
    {
        return BT_STATUS_FAIL;
    }


    return BT_STATUS_UNSUPPORTED;
}

bt_status_t bt_sal_get_connected_devices(bt_controller_id_t id, remote_device_properties_t* props, int* cnt)
{
    UNUSED(id);
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

/* Service discovery */
bt_status_t bt_sal_start_service_discovery(bt_controller_id_t id, bt_address_t* addr, bt_uuid_t* uuid)
{
    UNUSED(id);
    bluetooth_get_remote_services((bth_address_t*)addr, BT_TRANSPORT_BREDR);
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_stop_service_discovery(bt_controller_id_t id, bt_address_t* addr)
{
    UNUSED(id);
    return BT_STATUS_UNSUPPORTED;
}

/* Link policy */
bt_status_t bt_sal_set_power_mode(bt_controller_id_t id, bt_address_t* addr, bt_pm_mode_t* mode) {
    UNUSED(id);
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_set_link_role(bt_controller_id_t id, bt_address_t* addr, bt_link_role_t role) {
    UNUSED(id);
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_set_link_policy(bt_controller_id_t id, bt_address_t* addr, bt_link_policy_t policy) {
    UNUSED(id);
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_set_afh_channel_classification(bt_controller_id_t id, uint16_t central_frequency,
                                                  uint16_t band_width, uint16_t number) {
    UNUSED(id);
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_set_afh_channel_classification_1(bt_controller_id_t id, uint8_t* map) {
    UNUSED(id);
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

/* VSC */
bt_status_t bt_sal_send_hci_command(bt_controller_id_t id, uint8_t ogf, uint16_t ocf, uint8_t length, uint8_t* buf,
                                    bt_hci_event_callback_t cb, void* context) {
    UNUSED(id);
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}
