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
#include "advertising.h"
#include "sal_bes.h"
#include "bt_status.h"
#include "bluetooth.h"
#include "utils/log.h"

#include "sal_le_advertise_interface.h"

#include "api/bth_api_ble_advertiser.h"

/***************************** external declaration *****************************/

/***************************** macro defination *******************************/
#define LE_ADV_INVALID_ID 0xFF
#define LE_ADV_ID_MAP_MAX 10
/*****************************  type defination ********************************/
typedef enum
{
    BES_SAL_PHY_UNKONW = 0,
    BES_SAL_PHY_1M     = 1,
    BES_SAL_PHY_2M     = 2,
    BES_SAL_PHY_CODEC  = 3,
    BES_SAL_PHY_MAX,
} BES_SAL_PHY_TYPE_E;

typedef enum
{
    SAL_ADV_CHANNEL_UNKNOWN = 0x00,
    SAL_ADV_CHANNEL_37 = 0x01,
    SAL_ADV_CHANNEL_38 = 0x02,
    SAL_ADV_CHANNEL_39 = 0x04,
} sal_adv_channel_t;

typedef struct
{
    bool in_use;
    uint8_t reg_id;
    uint8_t adv_id;
} sal_adv_id_map_t;

static sal_adv_id_map_t g_id_map[10] = {0};
/*****************************  variable defination *****************************/

/*****************************  function declaration ****************************/
static sal_adv_id_map_t* get_id_map_by_reg_id(uint8_t reg_id)
{
    sal_adv_id_map_t* map = NULL;
    for(uint8_t i = 0; i < LE_ADV_ID_MAP_MAX; i++)
    {
        map = &g_id_map[i];
        if (map->in_use && map->reg_id == reg_id)
        {
            return map;
        }
    }
    return NULL;
}

static sal_adv_id_map_t* get_id_map_by_adv_id(uint8_t adv_id)
{
    sal_adv_id_map_t* map = NULL;
    for(uint8_t i = 0; i < LE_ADV_ID_MAP_MAX; i++)
    {
        map = &g_id_map[i];
        if (map->in_use && map->adv_id == adv_id)
        {
            return map;
        }
    }
    return NULL;
}

static void add_id_map(uint8_t reg_id, uint8_t adv_id)
{
    sal_adv_id_map_t* map = get_id_map_by_reg_id(adv_id);
    if (map != NULL)
    {
        return;
    }
    for (uint8_t i = 0; i < LE_ADV_ID_MAP_MAX; i++)
    {
        map = &g_id_map[i];
        if (!map->in_use)
        {
            map->reg_id = reg_id;
            map->adv_id = adv_id;
            map->in_use = true;
            return;
        }
    }
    BT_LOGE("No resource!");
}

static void remove_id_map(uint8_t reg_id)
{
    sal_adv_id_map_t* map = get_id_map_by_reg_id(reg_id);
    if (map == NULL)
    {
        // BT_LOGE("Can't find reg id %d", reg_id);
        return;
    }
    map->in_use = false;
}

static void bes_sal_adv_set_started_cb(int reg_id, uint8_t advertiser_id, int8_t tx_power, uint8_t status)
{
    sal_adv_id_map_t* map = NULL;
    if (status == ADV_SUCCESS)
    {
        map = get_id_map_by_reg_id(reg_id);
        map->adv_id = advertiser_id;
        advertising_on_state_changed(reg_id, LE_ADVERTISING_STARTED);
    }

}

static void bes_sal_adv_enabled_cb(uint8_t advertiser_id, bool enable, uint8_t status)
{
    sal_adv_id_map_t* map = get_id_map_by_adv_id(advertiser_id);
    if (map == NULL || map->adv_id == LE_ADV_INVALID_ID)
    {
        BT_LOGE("Can't find reg id for adv id %d", advertiser_id);
        return;
    }

    if (enable && (status == ADV_SUCCESS))
    {
        advertising_on_state_changed(map->reg_id, LE_ADVERTISING_STARTED);
    }
    else
    {
        advertising_on_state_changed(map->reg_id, LE_ADVERTISING_STOPPED);
        remove_id_map(map->reg_id);
    }
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
            bes_params.adv_event_type = ADVERTISING_CONNECTABLE;
            break;
        case BT_LE_ADV_DIRECT_IND:
        case BT_LE_EXT_ADV_DIRECT_IND:
            bes_params.adv_event_type = ADVERTISING_DIRECTED|ADVERTISING_CONNECTABLE;;
            break;
        case BT_LE_ADV_SCAN_IND:
        case BT_LE_EXT_ADV_SCAN_IND:
            bes_params.adv_event_type = ADVERTISING_SCANABLE;
            break;
        case BT_LE_ADV_NONCONN_IND:
        case BT_LE_EXT_ADV_NONCONN_IND:
            bes_params.adv_event_type = 0;
            break;
        case BT_LE_SCAN_RSP:
        case BT_LE_EXT_SCAN_RSP:
            bes_params.adv_event_type = ADVERTISING_SCANABLE;
            break;
        case BT_LE_LEGACY_ADV_IND:
            bes_params.adv_event_type = ADVERTISING_USE_LEGACY_PDUS|ADVERTISING_CONNECTABLE|ADVERTISING_SCANABLE;
            break;
        case BT_LE_LEGACY_ADV_DIRECT_IND:
            bes_params.adv_event_type = ADVERTISING_USE_LEGACY_PDUS|ADVERTISING_DIRECTED|ADVERTISING_CONNECTABLE;
            break;
        case BT_LE_LEGACY_ADV_SCAN_IND:
            bes_params.adv_event_type = ADVERTISING_USE_LEGACY_PDUS|ADVERTISING_SCANABLE;
        case BT_LE_LEGACY_ADV_NONCONN_IND:
        case BT_LE_LEGACY_SCAN_RSP:
            bes_params.adv_event_type = ADVERTISING_USE_LEGACY_PDUS;
            break;
        default:
            BT_LOGE("[%d]: %d", __LINE__, params->adv_type);
            return BT_STATUS_PARM_INVALID;
    }

    if(params->channel_map == BT_LE_ADV_CHANNEL_DEFAULT)
    {
        params->channel_map = SAL_ADV_CHANNEL_37|SAL_ADV_CHANNEL_38|SAL_ADV_CHANNEL_39;
    }
    else if(params->channel_map == BT_LE_ADV_CHANNEL_37_ONLY)
    {
        params->channel_map = SAL_ADV_CHANNEL_37;
    }
    else if(params->channel_map == BT_LE_ADV_CHANNEL_38_ONLY)
    {
        params->channel_map = SAL_ADV_CHANNEL_38;
    }
    else if(params->channel_map == BT_LE_ADV_CHANNEL_39_ONLY)
    {
        params->channel_map = SAL_ADV_CHANNEL_39;
    }
    else
    {
        params->channel_map = SAL_ADV_CHANNEL_UNKNOWN;
    }
    bes_params.min_interval = params->interval;
    bes_params.max_interval = params->interval;
    bes_params.channel_map  = params->channel_map;
    bes_params.tx_power     = params->tx_power;
    bes_params.primary_adv_phy   = BES_SAL_PHY_1M;
    bes_params.secondary_adv_phy = BES_SAL_PHY_1M;
    bes_params.scan_req_notif_enable = false;
    bes_params.own_address_type = params->own_addr_type;
    bes_params.peer_addr_type = params->peer_addr_type;
    memcpy(bes_params.peer_addr.address, params->peer_addr.addr, BT_ADDRESS_LEN);

    bes_adv_data.data = adv_data;
    bes_adv_data.size = adv_len;
    bes_rsp_data.data = scan_rsp_data;
    bes_rsp_data.size = scan_rsp_len;

    add_id_map(adv_id, LE_ADV_INVALID_ID);
    bth_adv_start_advertising_set(adv_id, &bes_params, &bes_adv_data, &bes_rsp_data,
            NULL, NULL, params->duration, 0);
    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_le_stop_adv(bt_controller_id_t id, uint8_t adv_id)
{
    sal_adv_id_map_t* map = get_id_map_by_reg_id(adv_id);
    if (map == NULL || map->adv_id == LE_ADV_INVALID_ID)
    {
        BT_LOGE("Can't find reg id %d", adv_id);
        return BT_STATUS_FAIL;
    }
    bth_adv_stop_advertising_set(map->adv_id);

    return BT_STATUS_SUCCESS;
}

