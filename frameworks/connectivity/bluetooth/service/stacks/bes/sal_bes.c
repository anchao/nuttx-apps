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
#define BT_SAL_ASYNC_DATA_MAX    512

/***************************** type defination ********************************/
typedef struct
{
    char* call_func;
    bool ready;
    uint32_t len;
    uint8_t data[BT_SAL_ASYNC_DATA_MAX];
} bes_async_task_t;

typedef struct 
{
    const bt_vhal_interface   *vhal;
    bth_bt_state_t             state;
    pthread_mutex_t            lock;
    pthread_cond_t             cond;

    bes_async_task_t           async_task;
} bes_bt_sal_env_t;

/***************************** variable defination *****************************/
static  bes_bt_sal_env_t  bt_env = {0};

/***************************** function declaration ****************************/
static void bes_bt_sal_state_changed_cb(bth_bt_state_t state)
{
    bes_bt_sal_env_t *env = &bt_env;
    uint8_t stack_state = 0;

    env->state = state;
    BT_LOGD("adapter State %d", state);
    if (state == BTH_BT_STATE_IDLE)
    {
        ASYNC_CALL_SET_DATA(bes_bt_sal_state_changed_cb, &state, sizeof(bth_bt_state_t));
        return;
    }

#ifdef CONFIG_BLUETOOTH_BLE_SUPPORT
    stack_state = state == BTH_BT_STATE_ON ? BLE_STACK_STATE_ON : BLE_STACK_STATE_OFF;
#else
    stack_state = state == BTH_BT_STATE_ON ? BT_BREDR_STACK_STATE_ON : BT_BREDR_STACK_STATE_OFF;
#endif

    adapter_on_adapter_state_changed(stack_state);
}

void bes_bt_sal_properties_cb(bth_bt_status_t status,
                                 int num_properties,
                                 const bth_bt_property_t* properties)
{
    BT_LOGD("status  = %d properties number = %d", status, num_properties);
    for (int i = 0; i < num_properties; i++, properties++)
    {
        BT_LOGD("property type = 0x%x value len %d", properties->type, properties->len);
        switch (properties->type) {
            case BTH_PROPERTY_BDADDR:
            {
                uint8_t *addr = properties->val;
                BT_LOGD("adddress %02X:XX:XX:XX:%02X:%02X", addr[0], addr[4], addr[5]);
                ASYNC_CALL_SET_DATA(bt_sal_get_address, addr, properties->len);
            } break;
            case BTH_PROPERTY_BLEADDR:
            {
                ASYNC_CALL_SET_DATA(bt_sal_le_get_address, properties->val, properties->len);
            } break;
            case BTH_PROPERTY_BDNAME:
            {
                ASYNC_CALL_SET_DATA(bt_sal_get_name, properties->val, properties->len);
            } break;
            case BTH_PROPERTY_CLASS_OF_DEVICE:
            {
                ASYNC_CALL_SET_DATA(bt_sal_get_device_class, properties->val, properties->len);
            } break;
            case BTH_PROPERTY_LOCAL_IO_CAPS:
            {
                ASYNC_CALL_SET_DATA(bt_sal_get_io_capability, properties->val, properties->len);
            } break;
            case BTH_PROPERTY_ADAPTER_SCAN_MODE:
            {
                ASYNC_CALL_SET_DATA(bt_sal_get_scan_mode, properties->val, properties->len);
            } break;
            case BTH_PROPERTY_ADAPTER_DISCOVERABLE_TIMEOUT:
            {
                ASYNC_CALL_SET_DATA(bt_sal_start_discovery, properties->val, properties->len);
            } break;
            case BTH_PROPERTY_ADAPTER_BONDED_DEVICES:
            {
                ASYNC_CALL_SET_DATA(bt_sal_get_bonded_devices, properties->val, properties->len);
            } break;
            case BTH_PROPERTY_APPEARANCE:
            {
                ASYNC_CALL_SET_DATA(bt_sal_le_get_appearance, properties->val, properties->len);
            } break;
            default:
                break;
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

static void bes_bt_sal_ssp_request_cb(const bth_address_t* bd_addr, int transport_link_type,
                                      const bth_bt_bdname_t* bd_name, uint32_t cod,
                                      bth_bt_ssp_variant_t pairing_variant,
                                      uint32_t pass_key)
{
    uint8_t transport_type = BT_TRANSPORT_BREDR;
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

    if (transport_link_type == BTH_BT_TRANSPORT_LE)
    {
        transport_type = BT_TRANSPORT_BLE;
    }
    adapter_on_ssp_request((bt_address_t*)bd_addr, transport_type,
        cod, ssp_type, pass_key, (char *)bd_name->name);
}

static void bes_bt_sal_bond_state_changed_cb(const bth_address_t* bd_addr, bth_bt_status_t status,
                                             bth_bt_bond_state_t state, int transport_link_type, int fail_reason)
{
    uint8_t transport_type = BT_TRANSPORT_BREDR;
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

    if (transport_link_type == BTH_BT_TRANSPORT_LE)
    {
        transport_type = BT_TRANSPORT_BLE;
    }
    adapter_on_bond_state_changed((bt_address_t*)bd_addr, sal_state, transport_type, sal_status, false);
    adapter_on_encryption_state_changed((bt_address_t*)bd_addr, true, transport_type);
}

static void bes_bt_sal_acl_state_changed_cb(const bth_address_t* bd_addr, uint8_t remote_bd_addr_type,
                                            bth_bt_status_t status,
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

    if (transport_link_type == BTH_BT_TRANSPORT_LE)
    {
        acl_info.transport = BT_TRANSPORT_BLE;
    }
    else
    {
        acl_info.transport = BT_TRANSPORT_BREDR;
    }
    acl_info.addr_type = remote_bd_addr_type;
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

static void bes_bt_sal_key_missing_cb(const bth_address_t* bd_addr)
{
    BT_LOGD("link key missing");
    adapter_on_link_key_removed((bt_address_t *)bd_addr, BT_STATUS_SUCCESS);
}

static void bes_bt_acl_conn_req_callback(const bth_address_t *bd_addr, uint32_t cod, bth_transport_t transport)
{
    adapter_on_connect_request((bt_address_t *)bd_addr, cod);
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
    bes_bt_sal_env_t *env = &bt_env;
    bth_init_params_t params;
    int ret;

    if (env->vhal != NULL)
    {
        BT_LOGD("[%d]: already init", __LINE__);
        return BT_STATUS_SUCCESS;
    }

    env->vhal = vhal;

    bt_sal_lock_init();
    bt_sal_cond_init();

    ASYNC_CALL_PREPARE(bes_bt_sal_state_changed_cb);

    ret = bluetooth_init(&bes_bt_sal_callbacks, &params);
    if (ret != BTH_STATUS_SUCCESS)
    {
        BT_LOGE("[%d]: ret=%d",__LINE__, ret);
        ASYNC_CALL_SET_DATA(bes_bt_sal_state_changed_cb, &ret, sizeof(ret));
    }

    ASYNC_CALL_GET_DATA(NULL, 0);

    return ret == BTH_STATUS_SUCCESS ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
}

void bt_sal_cleanup(void)
{
    bes_bt_sal_env_t *env = &bt_env;

    if(env->state == BTH_BT_STATE_IDLE)
    {
        BT_LOGD("[%d]: cur_state=%d", __LINE__, env->state);
        return;
    }

    ASYNC_CALL_PREPARE(bes_bt_sal_state_changed_cb);

    bluetooth_cleanup();

    ASYNC_CALL_GET_DATA(NULL, 0);

    bt_sal_lock_deinit();
    bt_sal_cond_deinit();
}

bt_status_t bt_sal_enable(bt_controller_id_t id)
{
    bes_bt_sal_env_t *env = &bt_env;

    if(env->state == BTH_BT_STATE_ON)
    {
        BT_LOGD("[%s][%d]: already enable cur_state=%d", __FUNCTION__, __LINE__, env->state);
        adapter_on_adapter_state_changed(BT_BREDR_STACK_STATE_ON);
        return BT_STATUS_SUCCESS;
    }

#ifndef CONFIG_BLUETOOTH_BLE_SUPPORT
    bth_bt_status_t ret;
    ret = bluetooth_enable();
    return ret == BTH_STATUS_SUCCESS ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
#endif

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_disable(bt_controller_id_t id)
{
    bt_status_t ret = BT_STATUS_SUCCESS;

#ifdef CONFIG_BLUETOOTH_BLE_SUPPORT
    if (bt_env.state == BTH_BT_STATE_ON)
    {
        BT_LOGD("[%s][%d]: cur_state=%d", __FUNCTION__, __LINE__, bt_env.state);
        adapter_on_adapter_state_changed(BT_BREDR_STACK_STATE_OFF);
        return ret;
    }
#else
    ret = bluetooth_disable() == BTH_STATUS_SUCCESS ? BT_STATUS_SUCCESS : BT_STATUS_FAIL;
#endif

    return ret;
}

bool bt_sal_is_enabled(bt_controller_id_t id)
{
    UNUSED(id);
    if(bt_env.state == BTH_BT_STATE_ON)
    {
        return true;
    }

    return false;
}

bt_status_t bt_sal_lock_init()
{
    int ret;

    ret = pthread_mutex_init(&bt_env.lock, NULL);
    if (ret)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_lock_deinit()
{
    int ret;

    ret = pthread_mutex_destroy(&bt_env.lock);
    if (ret)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_lock()
{
    int ret;

    ret = pthread_mutex_lock(&bt_env.lock);
    if (ret)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_unlock()
{
    int ret;

    ret = pthread_mutex_unlock(&bt_env.lock);
    if (ret)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_cond_init()
{
    int ret;

    ret = pthread_cond_init(&bt_env.cond, NULL);
    if (ret)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

bt_status_t bt_sal_cond_deinit()
{
    int ret;

    ret = pthread_cond_destroy(&bt_env.cond);
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

        ret = pthread_cond_timewait(&bt_env.cond, &bt_env.lock, &end_tm);
        */
       ret = -1;
    }
    else
    {
        ret = pthread_cond_wait(&bt_env.cond, &bt_env.lock);
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
        ret = pthread_cond_broadcast(&bt_env.cond);
    }
    else
    {
        ret = pthread_cond_signal(&bt_env.cond);
    }

    if (ret)
    {
        return BT_STATUS_FAIL;
    }

    return BT_STATUS_SUCCESS;
}

void bt_sal_async_call_prepare(char *func_name)
{
    bes_async_task_t *task = &bt_env.async_task;

    task->call_func = func_name;
    memset(task->data, 0, task->len);
    task->len = 0;
    task->ready = false;
}

void bt_sal_async_call_get_data(void** buf, uint32_t len)
{
    bes_async_task_t *task = &bt_env.async_task;
    while(!task->ready)
    {
        bt_sal_cond_wait(0);
    }

    if(len && len != task->len)
    {
        BT_LOGW("except length not matched %lud %lud", len, task->len);
        return;
    }

    if (buf != NULL)
    {
        *buf = task->data;
    }
    task->call_func = NULL;
}

void bt_sal_async_call_set_data(char* func_name, uint8_t* data, uint32_t len)
{
    bes_async_task_t *task = &bt_env.async_task;
    if (func_name == NULL || task->call_func == NULL)
    {
        return;
    }

    bt_sal_lock();
    if (strcmp(task->call_func, func_name) != 0)
    {
        BT_LOGW("ignore %s, %s", task->call_func, func_name);
        bt_sal_unlock();
        return;
    }

    if (len > BT_SAL_ASYNC_DATA_MAX)
    {
        BT_LOGE("over BT_SAL_ASYNC_DATA_MAX %s", func_name);
        goto __set_data_done__;
    }
    if (data != NULL && len)
    {
        memcpy(task->data, data, len);
        task->len = len;
    }

__set_data_done__:
    task->ready = true;
    bt_sal_unlock();
    bt_sal_cond_signal(0);
}

void bt_sal_get_stack_info(bt_stack_info_t* info)
{
    snprintf(info->name, 32, "bes");
    info->stack_ver_major = 1;
    info->stack_ver_minor = 1;
    info->sal_ver = 1;
}
