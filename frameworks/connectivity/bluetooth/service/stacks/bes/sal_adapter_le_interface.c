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
#define LOG_TAG "sal_bes_le"

/****************************** header include ********************************/
#include <stdbool.h> // For bool type
#include "adapter_internel.h"
#include "sal_interface.h"
#include "sal_bes.h"
#include "bt_status.h"
#include "bluetooth.h"
#include "utils/log.h"
#include "utils.h"

#include "sal_adapter_le_interface.h"

#include "api/bth_api_gatt.h"

/***************************** external declaration *****************************/
extern gattc_client_callbacks_t bes_sal_gattc;
extern gatts_server_callbacks_t bes_sal_gatts;
extern bth_adv_advertising_callbacks_t bes_sal_adv;
extern bth_scan_scanning_callbacks_t bes_sal_scan;

/***************************** macro defination *******************************/

/*****************************  type defination ********************************/

/*****************************  variable defination *****************************/
static bth_gatt_callbacks_t bes_gatt_cb = {0};

/*****************************  function declaration ****************************/
static void bt_sal_le_get_random_bytes(unsigned char *buffer, size_t size)
{
    static uint8_t count = 0;
    if (!buffer || size == 0) return;

    srand((unsigned) get_timestamp_msec());

    for (size_t i = 0; i < size; ++i) {
        buffer[i] = (unsigned char)(rand() % 256 + count);
    }
    count++;
}

bth_uuid_t bt_sal_le_uuid_get_random()
{
    bth_uuid_t uuid;
    bt_sal_le_get_random_bytes(uuid.data, sizeof(uuid.data));
    uuid.type = BTH_UUID_TYPE_128;
    return uuid;
}

bt_status_t bt_sal_le_init(const bt_vhal_interface* vhal)
{
    bt_status_t ret;
    bth_uuid_t gatts_uuid = bt_sal_le_uuid_get_random();
    bth_uuid_t gattc_uuid = bt_sal_le_uuid_get_random();
    bth_uuid_t scanner_uuid = bt_sal_le_uuid_get_random();

    ret = bt_sal_init(vhal);
    if (ret != BT_STATUS_SUCCESS)
    {
        BT_LOGE("[%d]: ret=%d",__LINE__, ret);
        return ret;
    }

    bes_gatt_cb.advertiser = &bes_sal_adv;
    bes_gatt_cb.scanner    = &bes_sal_scan;
    bes_gatt_cb.client     = &bes_sal_gattc;
    bes_gatt_cb.server     = &bes_sal_gatts;
    bes_gatt_cb.size       = sizeof(bes_gatt_cb);
    bth_gatt_init(&bes_gatt_cb);

    bth_gatts_register_server(&gatts_uuid, false);
    bth_gattc_register_client(&gattc_uuid, false);
    bth_scan_register_scanner(&scanner_uuid);

    return ret;
}

void bt_sal_le_cleanup(void)
{
    bt_sal_cleanup();
}

bt_status_t bt_sal_le_enable(bt_controller_id_t id)
{
    int ret;

    ret = bluetooth_enable();

    return ret == BTH_STATUS_SUCCESS ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}

bt_status_t bt_sal_le_disable(bt_controller_id_t id)
{
    bth_bt_status_t ret;

    ret = bluetooth_disable();

    return ret == BTH_STATUS_SUCCESS ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}

bt_status_t bt_sal_le_set_io_capability(bt_controller_id_t id, bt_io_capability_t cap)
{
    uint8_t io_cap = BTH_IO_CAP_UNKNOWN;
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

    ASYNC_CALL_PREPARE(bt_sal_get_io_capability);

    param.type = BTH_PROPERTY_LOCAL_IO_CAPS;
    param.len  = sizeof(io_cap);
    param.val  = &io_cap;
    bluetooth_set_adapter_property(&param);

    ASYNC_CALL_GET_DATA(NULL, 0);

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_le_set_static_identity(bt_controller_id_t id, bt_address_t* addr)
{
    bth_ble_address_t le_addr = {0};
    bth_bt_property_t param = {0};

    le_addr.type = BTH_ADDR_TYPE_RND_IA;
    memcpy(&le_addr.addr, addr, sizeof(le_addr.addr));

    param.type = BTH_PROPERTY_BLEADDR;
    param.len  = sizeof(le_addr);
    param.val  = &le_addr;
    bluetooth_set_adapter_property(&param);

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_le_set_public_identity(bt_controller_id_t id, bt_address_t* addr)
{
    bth_ble_address_t le_addr = {0};
    bth_bt_property_t param = {0};

    le_addr.type = BTH_ADDR_TYPE_PUB_IA;
    memcpy(&le_addr.addr, addr, sizeof(le_addr.addr));

    param.type = BTH_PROPERTY_BLEADDR;
    param.len  = sizeof(le_addr);
    param.val  = &le_addr;
    bluetooth_set_adapter_property(&param);

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_le_set_address(bt_controller_id_t id, bt_address_t* addr)
{
    bth_ble_address_t le_addr = {0};
    bth_bt_property_t param = {0};

    le_addr.type = BTH_ADDR_TYPE_RANDOM;
    memcpy(&le_addr.addr, addr, sizeof(le_addr.addr));

    param.type = BTH_PROPERTY_BLEADDR;
    param.len  = sizeof(le_addr);
    param.val  = &le_addr;
    bluetooth_set_adapter_property(&param);

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_le_get_address(bt_controller_id_t id, bt_address_t* addr)
{
    bth_ble_address_t* le_addr = NULL;

    if (!addr)
    {
        return BT_STATUS_PARM_INVALID;
    }

    ASYNC_CALL_PREPARE(bt_sal_le_get_address);

    bluetooth_get_adapter_property(BTH_PROPERTY_BLEADDR);

    ASYNC_CALL_GET_DATA((void**)&le_addr, sizeof(bth_ble_address_t));

    if (le_addr)
    {
        memcpy(addr, &(le_addr->addr), sizeof(bt_address_t));
        return BT_STATUS_SUCCESS;
    }

    return BT_STATUS_FAIL;
}

bt_status_t bt_sal_le_set_bonded_devices(bt_controller_id_t id, remote_device_le_properties_t* props, uint16_t prop_cnt)
{
    if (props == NULL)
    {
        return BT_STATUS_FAIL;
    }
    for (int i = 0; i < prop_cnt; i ++)
    {
        CHECK_BES_STACK_RETURN(bluetooth_le_add_bond_device((bth_address_t*)props->addr.addr, props->addr_type, props->smp_key, sizeof(props->smp_key)));
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_le_get_bonded_devices(bt_controller_id_t id, remote_device_le_properties_t* props, uint16_t* prop_cnt)
{
    ASYNC_CALL_PREPARE(bt_sal_get_bonded_devices);

    bluetooth_get_adapter_property(BTH_PROPERTY_ADAPTER_BONDED_DEVICES);

    ASYNC_CALL_GET_DATA(NULL, 0);

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_le_remove_bond(bt_controller_id_t id, bt_address_t* addr)
{
    int ret;

    ret = bluetooth_le_remove_bond_device((bth_address_t*)addr);

    return (ret == BTH_STATUS_SUCCESS)? BT_STATUS_SUCCESS:BT_STATUS_FAIL;
}

bt_status_t bt_sal_le_connect(bt_controller_id_t id, bt_address_t* addr, ble_addr_type_t type, ble_connect_params_t* params)
{
    int ret;
    bth_le_conn_params_t conn_param = {0};

    conn_param.bd_addr = (bth_address_t*)addr;
    conn_param.addr_type = type;
    conn_param.phys      = params->init_phy;

    if (params->filter_policy == BT_LE_CONNECT_FILTER_POLICY_ADDR)
    {
        conn_param.is_direct = true;
    }
    else
    {
        conn_param.is_direct = false;
    }

    ret = bluetooth_le_connect(&conn_param);

    return (ret == BTH_STATUS_SUCCESS)? BT_STATUS_SUCCESS:BT_STATUS_FAIL;
}

bt_status_t bt_sal_le_disconnect(bt_controller_id_t id, bt_address_t* addr)
{
    int ret;

    ret = bluetooth_le_disconnect((bth_address_t*)addr);

    return (ret == BTH_STATUS_SUCCESS)? BT_STATUS_SUCCESS:BT_STATUS_FAIL;
}

bt_status_t bt_sal_le_create_bond(bt_controller_id_t id, bt_address_t* addr, ble_addr_type_t type)
{
    int ret;

    ret = bluetooth_le_create_bond((bth_address_t*)addr, type);

    return (ret == BTH_STATUS_SUCCESS)? BT_STATUS_SUCCESS:BT_STATUS_FAIL;
}

bt_status_t bt_sal_le_smp_reply(bt_controller_id_t id, bt_address_t* addr, bool accept, bt_pair_type_t type, uint32_t passkey)
{
    int ret;

    ret = bluetooth_ssp_reply((bth_address_t*)addr, (bth_bt_ssp_variant_t)type, accept, passkey);

    return (ret == BTH_STATUS_SUCCESS)? BT_STATUS_SUCCESS:BT_STATUS_FAIL;
}

bt_status_t bt_sal_le_set_legacy_tk(bt_controller_id_t id, bt_address_t* addr, bt_128key_t tk_val)
{
    return BT_STATUS_UNSUPPORTED;
}

bt_status_t bt_sal_le_enable_key_derivation(bt_controller_id_t id, bool brkey_to_lekey, bool lekey_to_brkey)
{
    return BT_STATUS_UNSUPPORTED;
}

bt_status_t bt_sal_le_set_remote_oob_data(bt_controller_id_t id, bt_address_t* addr, bt_128key_t c_val, bt_128key_t r_val)
{
    int ret;
    bth_bt_oob_data_t oob_data192 = {0};
    bth_bt_oob_data_t oob_data256 = {0};

    memcpy(oob_data192.address, addr, sizeof(bt_address_t));
    memcpy(oob_data256.address, addr, sizeof(bt_address_t));
    memcpy(oob_data192.c, c_val, sizeof(oob_data192.c));
    memcpy(oob_data256.c, c_val, sizeof(oob_data256.c));
    memcpy(oob_data192.r, r_val, sizeof(oob_data192.r));
    memcpy(oob_data256.r, r_val, sizeof(oob_data256.r));
    ret = bluetooth_create_bond_out_of_band((bth_address_t*)addr, BTH_BT_TRANSPORT_LE, &oob_data192, &oob_data256);

    return (ret == BTH_STATUS_SUCCESS)? BT_STATUS_SUCCESS:BT_STATUS_FAIL;
}

bt_status_t bt_sal_le_get_local_oob_data(bt_controller_id_t id, bt_address_t* addr)
{
    return BT_STATUS_UNSUPPORTED;
}

bt_status_t bt_sal_le_add_white_list(bt_controller_id_t id, bt_address_t* addr, ble_addr_type_t addr_type)
{
    bth_ble_address_t bd_addr;

    bd_addr.type = addr_type;
    memcpy(&bd_addr.addr, addr, sizeof(bd_addr.addr));
    bluetooth_le_filter_list_add(&bd_addr);

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_le_remove_white_list(bt_controller_id_t id, bt_address_t* addr, ble_addr_type_t addr_type)
{
    bth_ble_address_t bd_addr;

    bd_addr.type = addr_type;
    memcpy(&bd_addr.addr, addr, sizeof(bd_addr.addr));
    bluetooth_le_filter_list_remove(&bd_addr);

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_le_set_phy(bt_controller_id_t id, bt_address_t* addr, ble_phy_type_t tx_phy, ble_phy_type_t rx_phy)
{
    int ret;
    uint8_t tx_bits = 0;
    uint8_t rx_bits = 0;

    tx_bits  = 1 << tx_phy;
    rx_bits  = 1 << rx_phy;
    ret = bluetooth_le_conn_set_phy((bth_address_t*)addr, tx_bits, rx_bits);

    return (ret == BTH_STATUS_SUCCESS)? BT_STATUS_SUCCESS:BT_STATUS_FAIL;
}

bt_status_t bt_sal_le_set_appearance(bt_controller_id_t id, uint16_t appearance)
{
    bth_bt_property_t param = {0};

    param.type = BTH_PROPERTY_APPEARANCE;
    param.len  = sizeof(appearance);
    param.val  = &appearance;
    bluetooth_set_adapter_property(&param);

    return BT_STATUS_SUCCESS;
}

uint16_t bt_sal_le_get_appearance(bt_controller_id_t id)
{
    uint16_t* appearance = NULL;

    ASYNC_CALL_PREPARE(bt_sal_le_get_appearance);

    bluetooth_get_adapter_property(BTH_PROPERTY_APPEARANCE);

    ASYNC_CALL_GET_DATA(&appearance, sizeof(uint16_t));

    return *appearance;
}

struct bt_conn* get_le_conn_from_addr(bt_address_t* addr) {
    // Empty implementation, return NULL or default connection
    return NULL;
}

bt_status_t get_le_addr_from_conn(struct bt_conn* conn, bt_address_t* addr) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_UNSUPPORTED; // Replace with actual success status code
}

#if defined(CONFIG_BT_USER_PHY_UPDATE)
ble_phy_type_t le_phy_convert_from_stack(uint8_t mode) {
    // Empty implementation, return default PHY type
    return BLE_PHY_TYPE_1M; // Replace with actual default PHY type
}

uint8_t le_phy_convert_from_service(ble_phy_type_t mode) {
    // Empty implementation, return default PHY mode
    return 0; // Replace with actual default PHY mode
}
#endif
