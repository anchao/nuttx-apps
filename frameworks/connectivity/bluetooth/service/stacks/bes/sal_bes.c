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
#define LOG_TAG "sal_bes"

#include <pthread.h>
#include "adapter_internel.h"
#include "sal_interface.h"
#include "sal_bes.h"
#include "bt_status.h"
#include "bluetooth.h"
#include "utils/log.h"

#include "api/bth_api_bluetooth.h"

/***************************** external declaration *****************************/

/***************************** macro defination *******************************/
#define BT_SAL_ASYNC_DATA_MAX    256
#define BT_SAL_ASYNC_STATE_WAIT  0XFF

/***************************** type defination ********************************/
typedef struct 
{
    bth_bt_state_t      state;
    pthread_mutex_t     lock;
    pthread_cond_t      cond;

    uint8_t async_result;
    int async_data_len;
    uint8_t async_data[BT_SAL_ASYNC_DATA_MAX];
} bes_bt_sal_env_t;

/***************************** variable defination *****************************/
static  bes_bt_sal_env_t  bes_bt_sal_env = {0};

/***************************** function declaration ****************************/
static void bes_bt_sal_state_changed_cb(bth_bt_state_t state)
{
    bes_bt_sal_env.state = state;

    bt_sal_set_async_info(NULL, 0, BT_STATUS_SUCCESS);
    BT_LOGD("Adapter State %d", state);
}

void bes_bt_sal_properties_cb(bth_bt_status_t status,
                                 int num_properties,
                                 bth_bt_property_t* properties)
{
    BT_LOGD("status  = %d properties number = %d", status, num_properties);
    for (int i = 0; i < num_properties; i++, properties++)
    {
        BT_LOGD("property type = 0x%x", properties->type);
        if (status == BTH_STATUS_SUCCESS)
        {
             bt_sal_set_async_info(properties->val, properties->len, BT_STATUS_SUCCESS);
        }
        else
        {
             bt_sal_set_async_info(properties->val, properties->len, BT_STATUS_FAIL);
        }
    }
}

static void bes_bt_sal_remote_device_properties_cb(const bth_address_t* bd_addr, bth_bt_status_t status,
                                                   int num_properties,
                                                   const bth_bt_property_t* properties)
{
    BT_LOGD("status  = %d properties number = %d", status, num_properties);

    for (int i = 0; i < num_properties; i++, properties++)
    {
        BT_LOGD("property type = %x", properties->type);
        bes_bt_sal_env.async_result = BT_STATUS_FAIL;
        switch (properties->type)
        {
            case BTH_PROPERTY_BDNAME:
            {
                if (status != BTH_STATUS_SUCCESS)
                {
                    BT_LOGD("state = %x", status);
                    break;
                }

                BT_LOGD("remote name = %s", (char*) properties->val);
                adapter_on_remote_name_recieved((bt_address_t*)bd_addr, (char *)properties->val);
            } break;
            case BTH_PROPERTY_BDADDR:
            {
                BT_LOGD("remote address = " STRMAC, MAC2STR((uint8_t*)properties->val));
            } break;
            case BTH_PROPERTY_UUIDS:
            {
                uint8_t uuid_num = properties->len / sizeof(bth_uuid_t);
                bth_uuid_t* from = (bth_uuid_t*) properties->val;
                bt_uuid_t* to = malloc(sizeof(bt_uuid_t) * uuid_num);
                if (!to)
                {
                    break;
                }

                for (uint8_t n = 0; n < uuid_num; n++, to++, from++)
                {
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
                        memcpy(to->val.u128, from->data, sizeof(to->val.u128));
                    }
                }
                adapter_on_service_search_done((bt_address_t*)bd_addr, to - uuid_num, uuid_num);
                free(to - uuid_num);
            }
            case BTH_PROPERTY_REMOTE_RSSI:
            {
                BT_LOGD("state = %d, rssi=%d", status, *(uint8_t*)properties->val);
            } break;
            case BTH_PROPERTY_BONDED_LINK_KEY:
            {
                link_key_property_t *prop_val = (link_key_property_t*) properties[0].val;
                BT_LOGD("update link key type = %d", prop_val->type);
                adapter_on_link_key_update(TO_BT_ADDRESS(bd_addr), prop_val->key.data, prop_val->type);
                break;
            }
            default: break;
        }
    }
}

static void bes_bt_sal_device_found_cb(int num_properties,
                                       const bth_bt_property_t* properties)
{
    bt_discovery_result_t result = {0};
    for (int i=0; i< num_properties; ++i)
    {
        if ((properties[i].type == BTH_PROPERTY_BDNAME) && (properties[i].len))
        {
            memcpy(result.name, properties[i].val,
                (properties[i].len > sizeof(result.name) ? sizeof(result.name) : properties[i].len));
        }
        else if ((properties[i].type == BTH_PROPERTY_BDADDR) && (properties[i].len == sizeof(result.addr)))
        {
            memcpy(&result.addr, properties[i].val, sizeof(result.addr));
        }
        else if ((properties[i].type == BTH_PROPERTY_CLASS_OF_DEVICE) && (properties[i].len == sizeof(uint32_t)))
        {
            result.cod = *((uint32_t*)properties[i].val);
        }
    }
    adapter_on_device_found(&result);
}

static void bes_bt_sal_discovery_state_changed_cb(bth_bt_discovery_state_t state)
{
    bt_discovery_state_t sal_state;

    if (state == BTH_BT_DISCOVERY_STOPPED)
    {
        sal_state = BT_DISCOVERY_STOPPED;
    }
    else if (state == BTH_BT_DISCOVERY_STARTED)
    {
        sal_state = BT_DISCOVERY_STARTED;
    }
    else
    {
        BT_LOGE("[%d]: %d", __LINE__, state);
        return;
    }

    adapter_on_discovery_state_changed(sal_state);
}

static void bes_bt_sal_pin_request_cb(const bth_address_t* bd_addr,
                                      const bth_bt_bdname_t* bd_name, uint32_t cod,
                                      bool min_16_digit)
{
    adapter_on_pin_request((bt_address_t*)bd_addr, cod, min_16_digit, (char *)bd_name->name);
}

static void bes_bt_sal_ssp_request_cb(const bth_address_t* bd_addr,
                                      bth_bt_bdname_t* bd_name, uint32_t cod,
                                      bth_bt_ssp_variant_t pairing_variant,
                                      uint32_t pass_key)
{
    bt_pair_type_t ssp_type;

    BT_LOGD("name = " STRMAC " cod = %lu pairing_variant = %d pass_key = %lu", MAC2STR(bd_addr->address),
          cod, pairing_variant, pass_key);
    switch (pairing_variant)
    {
        case BTH_BT_SSP_VARIANT_PASSKEY_CONFIRMATION:
            ssp_type = PAIR_TYPE_PASSKEY_CONFIRMATION;
            break;
        case BTH_BT_SSP_VARIANT_PASSKEY_ENTRY:
            ssp_type = PAIR_TYPE_PASSKEY_ENTRY;
            break;
        case BTH_BT_SSP_VARIANT_CONSENT:
            ssp_type = PAIR_TYPE_CONSENT;
            break;
        case BTH_BT_SSP_VARIANT_PASSKEY_NOTIFICATION:
            ssp_type = PAIR_TYPE_PASSKEY_NOTIFICATION;
            break;
        default:
            return;
    }
    adapter_on_ssp_request((bt_address_t*)bd_addr, BT_TRANSPORT_BREDR,
        cod, ssp_type, pass_key, (char *)bd_name->name);
}

static void bes_bt_sal_bond_state_changed_cb(const bth_address_t* bd_addr, bth_bt_status_t status,
                                             bth_bt_bond_state_t state, int fail_reason)
{
    bt_status_t  sal_status;
    bond_state_t sal_state;
    BT_LOGD("status = %d state = %d ", status, state);

    switch (state)
    {
        case BTH_BT_BOND_STATE_NONE:
            sal_state = BOND_STATE_NONE;
            break;
        case BTH_BT_BOND_STATE_BONDING:
            sal_state = BOND_STATE_BONDING;
            break;
        case BTH_BT_BOND_STATE_BONDED:
            sal_state = BOND_STATE_BONDED;
            break;
        default:
            BT_LOGE("[%d]: %d", __LINE__, state);
            return;
    }

    if(status == BTH_STATUS_SUCCESS)
    {
        sal_status = BT_STATUS_SUCCESS;
    }
    else
    {
        BT_LOGE("[%d]: %d", __LINE__, status);
        sal_status = BT_STATUS_FAIL;
    }

    adapter_on_bond_state_changed((bt_address_t*)bd_addr, sal_state, BT_TRANSPORT_BREDR, sal_status, false);
    adapter_on_encryption_state_changed((bt_address_t*)bd_addr, true, BT_TRANSPORT_BREDR);
}

static void bes_bt_sal_acl_state_changed_cb(const bth_address_t* bd_addr, bth_bt_status_t status,
                                            bth_bt_acl_state_t state, int transport_link_type,
                                            bth_bt_hci_error_code_t hci_reason,
                                            bth_bt_conn_direction_t direction, uint16_t acl_handle)
{
    acl_state_param_t acl_info = {0};

    if(state == BTH_BT_ACL_STATE_CONNECTED)
    {
        acl_info.connection_state = CONNECTION_STATE_CONNECTED;
    }
    else if(state == BTH_BT_ACL_STATE_DISCONNECTED)
    {
        acl_info.connection_state = CONNECTION_STATE_DISCONNECTED;
    }
    else
    {
        return;
    }

    acl_info.transport = BT_TRANSPORT_BREDR;
    acl_info.status = (bt_status_t)status;
    acl_info.hci_reason_code = hci_reason;
    memcpy(acl_info.addr.addr, bd_addr->address, sizeof(acl_info.addr.addr));
    adapter_on_connection_state_changed(&acl_info);
}

static void bes_bt_sal_dut_mode_recv_cb(uint16_t opcode, uint8_t* buf,
                                           uint8_t len)
{
}

static void bes_bt_sal_le_test_mode_cb(bth_bt_status_t status, uint16_t num_packets)
{
    
}

static void bes_bt_sal_energy_info_cb(bth_bt_activity_energy_info* energy_info,
                                         bth_bt_uid_traffic_t* uid_data)
{
}

static void bes_bt_sal_link_quality_report_cb(
    uint64_t timestamp, int report_id, int rssi, int snr,
    int retransmission_count, int packets_not_receive_count,
    int negative_acknowledgement_count)
{

}

static void bes_bt_sal_generate_local_oob_data_cb(bth_transport_t transport,
                                                     bth_bt_oob_data_t oob_data)
{
}

static void bes_bt_sal_switch_buffer_size_cb(bool is_low_latency_buffer_size)
{
}

static void bes_bt_sal_switch_codec_callback(bool is_low_latency_buffer_size)
{

}

static void bes_bt_sal_le_rand_cb(uint64_t random)
{

}

static void bes_bt_sal_key_missing_cb(const bth_address_t bd_addr)
{
}

static void bes_bt_acl_conn_req_callback(const bth_address_t bd_addr, uint32_t cod, bth_transport_t transport)
{
    adapter_on_connect_request((bt_address_t *)&bd_addr, cod);
}


static bth_bt_callbacks_t bes_bt_sal_callbacks =
{
    .adapter_state_changed_cb    = bes_bt_sal_state_changed_cb,
    .adapter_properties_cb       = bes_bt_sal_properties_cb,
    .remote_device_properties_cb = bes_bt_sal_remote_device_properties_cb,
    .device_found_cb             = bes_bt_sal_device_found_cb,
    .discovery_state_changed_cb  = bes_bt_sal_discovery_state_changed_cb,
    .pin_request_cb              = bes_bt_sal_pin_request_cb,
    .ssp_request_cb              = bes_bt_sal_ssp_request_cb,
    .bond_state_changed_cb       = bes_bt_sal_bond_state_changed_cb,
    .address_consolidate_cb      = NULL,
    .le_address_associate_cb     = NULL,
    .acl_state_changed_cb        = bes_bt_sal_acl_state_changed_cb,
    .dut_mode_recv_cb            = bes_bt_sal_dut_mode_recv_cb,
    .le_test_mode_cb             = bes_bt_sal_le_test_mode_cb,
    .energy_info_cb              = bes_bt_sal_energy_info_cb,
    .link_quality_report_cb      = bes_bt_sal_link_quality_report_cb,
    .generate_local_oob_data_cb  = bes_bt_sal_generate_local_oob_data_cb,
    .switch_buffer_size_cb       = bes_bt_sal_switch_buffer_size_cb,
    .switch_codec_cb             = bes_bt_sal_switch_codec_callback,
    .le_rand_cb                  = bes_bt_sal_le_rand_cb,
    .key_missing_cb              = bes_bt_sal_key_missing_cb,
    .acl_req_cb                  = bes_bt_acl_conn_req_callback,
};

bt_status_t bt_sal_init(const bt_vhal_interface* vhal)
{
    int ret;
    bt_status_t sal_ret;
    bth_init_params_t params;
    if(bes_bt_sal_env.state != BTH_BT_STATE_IDLE)
    {
        BT_LOGD("[%d]: cur_state=%d", __LINE__, bes_bt_sal_env.state);
        return BT_STATUS_SUCCESS;
    }

    bt_sal_lock_init();
    bt_sal_cond_init();

    if (BT_STATUS_SUCCESS != bt_sal_set_async_send_check())
    {
        return BT_STATUS_BUSY;
    }

    ret = bluetooth_init(&bes_bt_sal_callbacks, &params);
    if (ret != BTH_STATUS_SUCCESS)
    {
        BT_LOGE("[%d]: ret=%d",__LINE__, ret);
        bt_sal_set_async_info(NULL, 0, BT_STATUS_FAIL);
    }

    sal_ret = bt_sal_get_async_info(NULL,0);

    return sal_ret;
}

void bt_sal_cleanup(void)
{
    if(bes_bt_sal_env.state == BTH_BT_STATE_IDLE)
    {
        BT_LOGD("[%d]: cur_state=%d", __LINE__, bes_bt_sal_env.state);
        return;
    }

    if (BT_STATUS_SUCCESS != bt_sal_set_async_send_check())
    {
        return;
    }

    bluetooth_cleanup();

    bt_sal_get_async_info(NULL,0);

    bt_sal_lock_deinit();
    bt_sal_cond_deinit();
}

bt_status_t bt_enable()
{
    int ret;
    if (BT_STATUS_SUCCESS != bt_sal_set_async_send_check())
    {
        return BT_STATUS_BUSY;
    }

    ret = bluetooth_enable();
    if (ret != BTH_STATUS_SUCCESS)
    {
        BT_LOGE("[%s][%d]: ret=%d", __FUNCTION__, __LINE__, ret);
        bt_sal_set_async_info(NULL, 0, BT_STATUS_FAIL);
    }

    ret = bt_sal_get_async_info(NULL, 0);
    return ret;

}

bt_status_t bt_sal_enable(bt_controller_id_t id)
{

    bt_status_t ret;

    if(bes_bt_sal_env.state == BTH_BT_STATE_ON)
    {
        BT_LOGD("[%s][%d]: already enable cur_state=%d", __FUNCTION__, __LINE__, bes_bt_sal_env.state);
        adapter_on_adapter_state_changed(BT_BREDR_STACK_STATE_ON);
        return BT_STATUS_SUCCESS;
    }

    ret = bt_enable();

    if (ret == BT_STATUS_SUCCESS)
    {
        adapter_on_adapter_state_changed(BT_BREDR_STACK_STATE_ON);
    }
    return ret;
}

bt_status_t bt_sal_disable(bt_controller_id_t id)
{
    int ret;
    bt_status_t sal_ret;

    if(bes_bt_sal_env.state == BTH_BT_STATE_OFF)
    {
        BT_LOGD("[%s][%d]: cur_state=%d", __FUNCTION__, __LINE__, bes_bt_sal_env.state);
        adapter_on_adapter_state_changed(BT_BREDR_STACK_STATE_OFF);
        return BT_STATUS_SUCCESS;
    }

    if (BT_STATUS_SUCCESS != bt_sal_set_async_send_check())
    {
        return BT_STATUS_BUSY;
    }

    ret = bluetooth_disable();
    if (ret != BTH_STATUS_SUCCESS)
    {
        BT_LOGE("[%s][%d]: ret=%d", __FUNCTION__, __LINE__, ret);
        bt_sal_set_async_info(NULL, 0, BT_STATUS_FAIL);
    }

    sal_ret = bt_sal_get_async_info(NULL,0);
    if (sal_ret == BT_STATUS_SUCCESS)
    {
        adapter_on_adapter_state_changed(BT_BREDR_STACK_STATE_OFF);
    }

    return sal_ret;
}

bool bt_sal_is_enabled(bt_controller_id_t id)
{
    UNUSED(id);
    if(bes_bt_sal_env.state == BTH_BT_STATE_ON)
    {
        return true;
    }

    return false;
}

bt_status_t bt_sal_lock_init()
{
    int ret;

    ret = pthread_mutex_init(&bes_bt_sal_env.lock, NULL);
    if (ret)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_lock_deinit()
{
    int ret;

    ret = pthread_mutex_destroy(&bes_bt_sal_env.lock);
    if (ret)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_lock()
{
    int ret;

    ret = pthread_mutex_lock(&bes_bt_sal_env.lock);
    if (ret)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_unlock()
{
    int ret;

    ret = pthread_mutex_unlock(&bes_bt_sal_env.lock);
    if (ret)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_cond_init()
{
    int ret;

    ret = pthread_cond_init(&bes_bt_sal_env.cond, NULL);
    if (ret)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_cond_deinit()
{
    int ret;

    ret = pthread_cond_destroy(&bes_bt_sal_env.cond);
    if (ret)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_cond_wait(int timeout_ms)
{
    int ret;

    if (timeout_ms)
    {
        /*
        struct timespec start_tm;
        struct timespec end_tm;

        clock_gettime(CLOCK_REALTIME, &start_tm);
        end_tm = ns_to_tm(tm_to_ns(start_tm) + timeout_ms*1000000);

        ret = pthread_cond_timewait(&bes_bt_sal_env.cond, &bes_bt_sal_env.lock, &end_tm);
        */
       ret = -1;
    }
    else
    {
        ret = pthread_cond_wait(&bes_bt_sal_env.cond, &bes_bt_sal_env.lock);
    }

    if (ret)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_cond_signal(bool broadcast)
{
    int ret;

    if (broadcast)
    {
        ret = pthread_cond_broadcast(&bes_bt_sal_env.cond);
    }
    else
    {
        ret = pthread_cond_signal(&bes_bt_sal_env.cond);
    }

    if (ret)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_set_async_send_check(void)
{
    bt_status_t ret;

    bt_sal_lock();
    if (bes_bt_sal_env.async_result != BT_SAL_ASYNC_STATE_WAIT)
    {
        bes_bt_sal_env.async_result = BT_SAL_ASYNC_STATE_WAIT;
        ret=  BT_STATUS_SUCCESS;
    }
    else
    {
        ret=  BT_STATUS_BUSY;
    }
    bt_sal_unlock();

    return ret;
}

bt_status_t bt_sal_set_async_info(uint8_t* data, int data_len, bt_status_t async_result)
{
    bt_status_t ret = BT_STATUS_SUCCESS;

    bt_sal_lock();

    if (data_len > BT_SAL_ASYNC_DATA_MAX)
    {
        BT_LOGE("[%s][%d]: %d, %d", __FUNCTION__, __LINE__, data_len, BT_SAL_ASYNC_DATA_MAX);
        ret = BT_STATUS_FAIL;
        goto RETURN_RESULT;
    }

    if (bes_bt_sal_env.async_data_len || (bes_bt_sal_env.async_result != BT_SAL_ASYNC_STATE_WAIT))
    {
        BT_LOGE("lost async data, %d， %d!!!",
            bes_bt_sal_env.async_data_len, bes_bt_sal_env.async_result);
    }

    bes_bt_sal_env.async_result = async_result;
    if (data && data_len)
    {
        bes_bt_sal_env.async_data_len = data_len;
        memcpy(bes_bt_sal_env.async_data, data, data_len);
    }

RETURN_RESULT:
    bt_sal_unlock();
    bt_sal_cond_signal(0);

    return ret;
}

bt_status_t bt_sal_get_async_info(uint8_t** data, int* data_len)
{
    bt_status_t ret;

    bt_sal_lock();
    while (bes_bt_sal_env.async_result == BT_SAL_ASYNC_STATE_WAIT)
    {
        bt_sal_cond_wait(0);
    }

    if (data && data_len)
    {
        if (bes_bt_sal_env.async_data_len)
        {
            *data     = bes_bt_sal_env.async_data;
            *data_len = bes_bt_sal_env.async_data_len;
        }
        else
        {
            *data     = NULL;
            *data_len = 0;
        }
    }
    ret = bes_bt_sal_env.async_result;

    // Clean async info
    bes_bt_sal_env.async_data_len = 0;
    bes_bt_sal_env.async_result   = BT_STATUS_SUCCESS;
    bt_sal_unlock();

    return ret;
}

void bt_sal_get_stack_info(bt_stack_info_t* info)
{
    snprintf(info->name, 32, "bes");
    info->stack_ver_major = 1;
    info->stack_ver_minor = 1;
    info->sal_ver = 1;
}
