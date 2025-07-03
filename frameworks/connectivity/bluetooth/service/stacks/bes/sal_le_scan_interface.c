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
#define LOG_TAG "sal_bes_scan"

/****************************** header include ********************************/
#include <stdint.h>
#include "adapter_internel.h"
#include "sal_interface.h"
#include "scan_manager.h"
#include "sal_bes.h"
#include "bt_status.h"
#include "bluetooth.h"
#include "utils/log.h"

#include "sal_le_scan_interface.h"

#include "api/bth_api_ble_scanner.h"

/***************************** external declaration *****************************/

/***************************** macro defination *******************************/
#define SAL_LE_SCAN_GET_BIT(data, bit)        (data&(1<<bit))

/*****************************  type defination ********************************/
typedef struct
{
    int scanner_id;
} bes_bt_sal_le_scan_env_t;

/*****************************  variable defination *****************************/
static bes_bt_sal_le_scan_env_t bes_bt_sal_le_scan_env = {0};

/*****************************  function declaration ****************************/
static void bes_sal_scan_reg_cb(const bth_uuid_t* app_uuid,
                                                  uint8_t scanner_id, uint8_t status)
{
    if (status == SCANNING_SUCCESS)
    {
        bes_bt_sal_le_scan_env.scanner_id = scanner_id;
    }
}

static void bes_sal_scan_set_param_cmp_cb(uint8_t scanner_id,
                                                              uint8_t status)
{
}

static void bes_sal_scan_scan_result_cb(uint16_t event_type, uint8_t addr_type,
                                           bth_address_t bda, uint8_t primary_phy,
                                           uint8_t secondary_phy, uint8_t advertising_sid,
                                           int8_t tx_power, int8_t rssi,
                                           uint16_t periodic_adv_int,
                                           common_data_t adv_data)
{
    ble_scan_result_t result_info;

    memcpy(result_info.addr.addr, bda.address, sizeof(result_info.addr.addr));
    result_info.dev_type  = BT_DEVICE_DEVTYPE_BLE;
    result_info.rssi      = rssi;
    result_info.addr_type = addr_type;
    result_info.length    = adv_data.size;

    if (SAL_LE_SCAN_GET_BIT(event_type, EVENT_TYPE_CONNECTABLE_BIT) &&
        SAL_LE_SCAN_GET_BIT(event_type, EVENT_TYPE_SCANNABLE_BIT))
    {
        result_info.adv_type = BT_LE_ADV_IND;
    }
    else if (SAL_LE_SCAN_GET_BIT(event_type, EVENT_TYPE_CONNECTABLE_BIT) &&
             SAL_LE_SCAN_GET_BIT(event_type, EVENT_TYPE_DIRECTED_BIT))
    {
        result_info.adv_type = BT_LE_ADV_DIRECT_IND;
    }
    else if (SAL_LE_SCAN_GET_BIT(event_type, EVENT_TYPE_SCANNABLE_BIT))
    {
        result_info.adv_type = BT_LE_ADV_SCAN_IND;
    }
    else
    {
        result_info.adv_type = BT_LE_ADV_NONCONN_IND;
    }

    if (SAL_LE_SCAN_GET_BIT(event_type, EVENT_TYPE_LEGACY_BIT))
    {
        result_info.adv_type += BT_LE_LEGACY_ADV_IND;
    }
    else
    {
        result_info.adv_type += BT_LE_EXT_ADV_IND;
    }

    scan_on_result_data_update(&result_info, (char*)adv_data.data);
}

static void bes_sal_scan_track_adv_found_lost_cb(
                bth_scan_advertising_track_info_t advertising_track_info)
{
}

static void bes_sal_scan_pa_sync_started_cb(int reg_id, uint8_t status,
                                                     uint16_t sync_handle, uint8_t advertising_sid,
                                                     uint8_t address_type, bth_address_t address,
                                                     uint8_t phy, uint16_t interval)
{
}

static void bes_sal_can_pa_sync_report_cb(uint16_t sync_handle, int8_t tx_power,
                                                    int8_t rssi, uint8_t status,
                                                    common_data_t* data)
{
}

static void bes_sal_scan_pa_sync_lost_cb(uint16_t sync_handle)
{
}

static void bes_sal_scan_pa_sync_transferred_cb(int pa_source, uint8_t status,
                                                         bth_address_t address)
{
}

static void bes_sal_scan_big_info_report_cb(uint16_t sync_handle, bool encrypted)
{
}

bth_scan_scanning_callbacks_t bes_sal_scan =
{
    .on_scanner_registered             = bes_sal_scan_reg_cb,
    .on_set_scanner_parameter_complete = bes_sal_scan_set_param_cmp_cb,
    .on_scan_result                    = bes_sal_scan_scan_result_cb,
    .on_track_adv_found_lost           = bes_sal_scan_track_adv_found_lost_cb,
    .on_periodic_sync_started          = bes_sal_scan_pa_sync_started_cb,
    .on_periodic_sync_report           = bes_sal_can_pa_sync_report_cb,
    .on_periodic_sync_lost             = bes_sal_scan_pa_sync_lost_cb,
    .on_periodic_sync_transferred      = bes_sal_scan_pa_sync_transferred_cb,
    .on_big_info_report                = bes_sal_scan_big_info_report_cb,
};

bt_status_t bt_sal_le_set_scan_parameters(bt_controller_id_t id, ble_scan_params_t* params)
{
    if (!bes_bt_sal_le_scan_env.scanner_id)
    {
        return BT_STATUS_FAIL;
    }

    bth_scan_set_scan_parameters(bes_bt_sal_le_scan_env.scanner_id,
        params->scan_type, params->scan_interval, params->scan_window);
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_le_start_scan(bt_controller_id_t id)
{
    bth_scan_start(true);

    scan_on_state_changed(1);
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_le_stop_scan(bt_controller_id_t id)
{
    bth_scan_start(false);

    scan_on_state_changed(0);
    return BT_STATUS_SUCCESS;
}