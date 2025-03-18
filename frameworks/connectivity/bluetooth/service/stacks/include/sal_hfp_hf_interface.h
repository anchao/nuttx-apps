/****************************************************************************
 *  Copyright (C) 2024 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/
#ifndef __SAL_HFP_HF_INTERFACE_H__
#define __SAL_HFP_HF_INTERFACE_H__
#include "bt_device.h"
#include <stdint.h>

bt_status_t bt_sal_hfp_hf_init(uint32_t features, uint8_t max_connection);
bt_status_t bt_sal_hfp_hf_cleanup();
bt_status_t bt_sal_hfp_hf_connect(const bt_address_t *addr);
bt_status_t bt_sal_hfp_hf_disconnect(const bt_address_t *addr);
bt_status_t bt_sal_hfp_hf_connect_audio(const bt_address_t *addr);
bt_status_t bt_sal_hfp_hf_disconnect_audio(const bt_address_t *addr);
bt_status_t bt_sal_hfp_hf_answer_call(const bt_address_t *addr);
bt_status_t bt_sal_hfp_hf_call_control(const bt_address_t *addr, uint8_t ctrl, uint8_t idx);
bt_status_t bt_sal_hfp_hf_reject_call(const bt_address_t *addr);
bt_status_t bt_sal_hfp_hf_hangup_call(const bt_address_t *addr);
bt_status_t bt_sal_hfp_hf_get_current_calls(const bt_address_t *addr);
bt_status_t bt_sal_hfp_hf_send_at_cmd(const bt_address_t *addr, char* str, uint16_t len);
bt_status_t bt_sal_hfp_hf_send_battery_level(const bt_address_t *addr, uint8_t level);
bt_status_t bt_sal_hfp_hf_send_dtmf(const bt_address_t *addr, char value);
bt_status_t bt_sal_hfp_hf_set_volume(const bt_address_t *addr, uint8_t type, uint8_t volume);
bt_status_t bt_sal_hfp_hf_start_voice_recognition(const bt_address_t *addr);
bt_status_t bt_sal_hfp_hf_stop_voice_recognition(const bt_address_t *addr);
bt_status_t bt_sal_hfp_hf_dial_number(const bt_address_t *addr, char* number);
bt_status_t bt_sal_hfp_hf_dial_memory(const bt_address_t *addr, int memory);

#endif