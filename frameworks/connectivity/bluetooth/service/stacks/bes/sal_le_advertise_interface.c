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
#define LOG_TAG "sal_bes_adv"

/****************************** header include ********************************/
#include <stdint.h>
#include "adapter_internel.h"
#include "sal_interface.h"
#include "sal_bes.h"
#include "bt_status.h"
#include "bluetooth.h"
#include "utils/log.h"

#include "sal_le_advertise_interface.h"

#include "api/bth_api_ble_advertiser.h"

/***************************** external declaration *****************************/

/***************************** macro defination *******************************/

/*****************************  type defination ********************************/
typedef enum
{
    BES_SAL_PHY_UNKONW = 0,
    BES_SAL_PHY_1M     = 1,
    BES_SAL_PHY_2M     = 2,
    BES_SAL_PHY_CODEC  = 3,
    BES_SAL_PHY_MAX,
} BES_SAL_PHY_TYPE_E;

/*****************************  variable defination *****************************/

/*****************************  function declaration ****************************/

static void bes_sal_adv_set_started_cb(int reg_id, uint8_t advertiser_id, int8_t tx_power, uint8_t status)
{
}

static void bes_sal_adv_enabled_cb(uint8_t advertiser_id, bool enable, uint8_t status)
{
}

static void bes_sal_adv_data_set_cb(uint8_t advertiser_id, uint8_t status)
{

}

static void bes_sal_scan_response_data_set_cb(uint8_t advertiser_id, uint8_t status)
{
}

static void bes_sal_adv_parameters_updated_cb(uint8_t advertiser_id, int8_t tx_power, uint8_t status)
{
}

static void bes_sal_pa_parameters_updated_cb(uint8_t advertiser_id, uint8_t status)
{
}

static void bes_sal_pa_data_set_cb(uint8_t advertiser_id, uint8_t status)
{
}

static void bes_sal_pa_enabled_cb(uint8_t advertiser_id, bool enable, uint8_t status)
{
}

static void bes_sal_own_address_read_cb(uint8_t advertiser_id, uint8_t address_type, bth_address_t address)
{
}

bth_adv_advertising_callbacks_t bes_sal_adv =
{
    .adv_set_started        = bes_sal_adv_set_started_cb,
    .adv_enabled            = bes_sal_adv_enabled_cb,
    .adv_data_set           = bes_sal_adv_data_set_cb,
    .scan_response_data_set = bes_sal_scan_response_data_set_cb,
    .adv_parameters_updated = bes_sal_adv_parameters_updated_cb,
    .pa_parameters_updated  = bes_sal_pa_parameters_updated_cb,
    .pa_data_set            = bes_sal_pa_data_set_cb,
    .pa_enabled             = bes_sal_pa_enabled_cb,
    .own_address_read       = bes_sal_own_address_read_cb,
};

bt_status_t bt_sal_le_start_adv(bt_controller_id_t id, uint8_t adv_id, ble_adv_params_t* params, 
                                uint8_t* adv_data, uint16_t adv_len, 
                                uint8_t* scan_rsp_data, uint16_t scan_rsp_len)
{
    bth_adv_parameters bes_params = {0};
    common_data_t bes_adv_data = {0};
    common_data_t bes_rsp_data = {0};

    switch (params->adv_type)
    {
        case BT_LE_ADV_IND:
        case BT_LE_EXT_ADV_IND:
            bes_params.advertising_event_properties = ADVERTISING_CONNECTABLE|ADVERTISING_SCANABLE;
            break;
        case BT_LE_ADV_DIRECT_IND:
        case BT_LE_EXT_ADV_DIRECT_IND:
            bes_params.advertising_event_properties = ADVERTISING_CONNECTABLE|ADVERTISING_DIRECTED;
            break;
        case BT_LE_ADV_SCAN_IND:
        case BT_LE_EXT_ADV_SCAN_IND:
            bes_params.advertising_event_properties = ADVERTISING_SCANABLE;
            break;
        case BT_LE_ADV_NONCONN_IND:
        case BT_LE_EXT_ADV_NONCONN_IND:
            bes_params.advertising_event_properties = 0;
            break;
        case BT_LE_SCAN_RSP:
        case BT_LE_EXT_SCAN_RSP:
            bes_params.advertising_event_properties = ADVERTISING_SCANABLE;
            break;
        case BT_LE_LEGACY_ADV_IND:
            bes_params.advertising_event_properties = ADVERTISING_USE_LEGACY_PDUS|ADVERTISING_CONNECTABLE|ADVERTISING_SCANABLE;
            break;
        case BT_LE_LEGACY_ADV_DIRECT_IND:
            bes_params.advertising_event_properties = ADVERTISING_USE_LEGACY_PDUS|ADVERTISING_CONNECTABLE|ADVERTISING_DIRECTED;
            break;
        case BT_LE_LEGACY_ADV_SCAN_IND:
            bes_params.advertising_event_properties = ADVERTISING_USE_LEGACY_PDUS|ADVERTISING_SCANABLE;
            break;
        case BT_LE_LEGACY_ADV_NONCONN_IND:
            bes_params.advertising_event_properties = ADVERTISING_USE_LEGACY_PDUS;
            break;
        case BT_LE_LEGACY_SCAN_RSP:
            bes_params.advertising_event_properties = ADVERTISING_USE_LEGACY_PDUS|ADVERTISING_SCANABLE;
            break;
        default:
            BT_LOGE("[%d]: %d", __LINE__, params->adv_type);
            return BT_STATUS_PARM_INVALID;
    }

    bes_params.min_interval = params->interval;
    bes_params.max_interval = params->interval;
    bes_params.channel_map  = params->channel_map;
    bes_params.tx_power     = params->tx_power;
    bes_params.primary_advertising_phy   = BES_SAL_PHY_1M;
    bes_params.secondary_advertising_phy = BES_SAL_PHY_1M;
    bes_params.scan_request_notification_enable = false;
    bes_params.own_address_type = params->own_addr_type;

    bes_adv_data.data = adv_data;
    bes_adv_data.size = adv_len;
    bes_rsp_data.data = scan_rsp_data;
    bes_rsp_data.size = scan_rsp_len;

    bth_adv_start_advertising_set(adv_id, &bes_params, &bes_adv_data, &bes_rsp_data,
            NULL, NULL, params->duration, 0);

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_le_stop_adv(bt_controller_id_t id, uint8_t adv_id)
{
    bth_adv_stop_advertising_set(adv_id);

    return BT_STATUS_SUCCESS;
}

