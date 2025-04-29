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

    gatts_register_server(&gatts_uuid, false);
    gattc_register_client(&gattc_uuid, false);
    bth_scan_register_scanner(&scanner_uuid);

    return ret;
}

void bt_sal_le_cleanup(void)
{
    bt_sal_cleanup();
}

bt_status_t bt_sal_le_enable(bt_controller_id_t id)
{
    bt_status_t ret;

    ret = bt_enable();
    if (ret != BT_STATUS_SUCCESS)
    {
        BT_LOGE("[%d]: ret=%d", __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    adapter_on_adapter_state_changed(BLE_STACK_STATE_ON);

    return ret; // Replace with actual success status code
}

bt_status_t bt_sal_le_disable(bt_controller_id_t id)
{
    bt_status_t ret;

    ret = bt_sal_disable(id);
    if (ret != BT_STATUS_SUCCESS)
    {
        BT_LOGE("[%d]: ret=%d", __LINE__, ret);
        return BT_STATUS_FAIL;
    }

    adapter_on_adapter_state_changed(BLE_STACK_STATE_OFF);

    return ret; // Replace with actual success status code
}

bt_status_t bt_sal_le_set_io_capability(bt_controller_id_t id, bt_io_capability_t cap)
{
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

bt_status_t bt_sal_le_set_static_identity(bt_controller_id_t id, bt_address_t* addr)
{
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_set_public_identity(bt_controller_id_t id, bt_address_t* addr)
{
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_set_address(bt_controller_id_t id, bt_address_t* addr)
{
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_get_address(bt_controller_id_t id) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_set_bonded_devices(bt_controller_id_t id, remote_device_le_properties_t* props, uint16_t prop_cnt) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_get_bonded_devices(bt_controller_id_t id, remote_device_le_properties_t* props, uint16_t* prop_cnt) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_connect(bt_controller_id_t id, bt_address_t* addr, ble_addr_type_t type, ble_connect_params_t* params) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_disconnect(bt_controller_id_t id, bt_address_t* addr) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_create_bond(bt_controller_id_t id, bt_address_t* addr, ble_addr_type_t type) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_remove_bond(bt_controller_id_t id, bt_address_t* addr) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_smp_reply(bt_controller_id_t id, bt_address_t* addr, bool accept, bt_pair_type_t type, uint32_t passkey) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_set_legacy_tk(bt_controller_id_t id, bt_address_t* addr, bt_128key_t tk_val) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_set_remote_oob_data(bt_controller_id_t id, bt_address_t* addr, bt_128key_t c_val, bt_128key_t r_val) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_get_local_oob_data(bt_controller_id_t id, bt_address_t* addr) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_add_white_list(bt_controller_id_t id, bt_address_t* addr, ble_addr_type_t addr_type) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_remove_white_list(bt_controller_id_t id, bt_address_t* addr, ble_addr_type_t addr_type) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_set_phy(bt_controller_id_t id, bt_address_t* addr, ble_phy_type_t tx_phy, ble_phy_type_t rx_phy) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

bt_status_t bt_sal_le_set_appearance(bt_controller_id_t id, uint16_t appearance) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

uint16_t bt_sal_le_get_appearance(bt_controller_id_t id) {
    // Empty implementation, return default appearance
    return 0; // Replace with actual default appearance
}

bt_status_t bt_sal_le_enable_key_derivation(bt_controller_id_t id, bool brkey_to_lekey, bool lekey_to_brkey) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
}

struct bt_conn* get_le_conn_from_addr(bt_address_t* addr) {
    // Empty implementation, return NULL or default connection
    return NULL;
}

bt_status_t get_le_addr_from_conn(struct bt_conn* conn, bt_address_t* addr) {
    // Empty implementation, return success or appropriate status code
    return BT_STATUS_SUCCESS; // Replace with actual success status code
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
