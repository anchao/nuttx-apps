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
#ifndef __SAL_HFP_AG_INTERFACE_H__
#define __SAL_HFP_AG_INTERFACE_H__
#include <stdbool.h>
#include <stdint.h>
#include "bt_device.h"
#include "hfp_ag_service.h"

bt_status_t bt_sal_hfp_ag_init(uint32_t ag_feature, uint8_t max_connection);

bt_status_t bt_sal_hfp_ag_cleanup();

bt_status_t bt_sal_hfp_ag_connect(const bt_address_t *bd_addr);

bt_status_t bt_sal_hfp_ag_disconnect(const bt_address_t *bd_addr);

bt_status_t bt_sal_hfp_ag_connect_audio(const bt_address_t *bd_addr);

bt_status_t bt_sal_hfp_ag_disconnect_audio(const bt_address_t *bd_addr);

bt_status_t bt_sal_hfp_ag_manufacture_id_response(const bt_address_t *bd_addr, char* data, uint16_t len);

bt_status_t bt_sal_hfp_ag_model_id_response(const bt_address_t *bd_addr, char* data, uint16_t len);

bt_status_t bt_sal_hfp_ag_clcc_response(const bt_address_t *bd_addr, int idx, bool incoming,
                                        uint8_t call_state, uint8_t call_mode, bool multiparty,
                                        uint8_t call_addr_type, char* line_identification);

bt_status_t bt_sal_hfp_ag_send_at_cmd(const bt_address_t *bd_addr, char* str, uint16_t len);

bt_status_t bt_sal_hfp_ag_cind_response(const bt_address_t *bd_addr, hfp_ag_cind_resopnse_t *resp);

bt_status_t bt_sal_hfp_ag_error_response(const bt_address_t *bd_adr, uint8_t error);

bt_status_t bt_sal_hfp_ag_start_voice_recognition(const bt_address_t *bd_adr);

bt_status_t bt_sal_hfp_ag_stop_voice_recognition(const bt_address_t *bd_adr);

bt_status_t bt_sal_hfp_ag_phone_state_change(const bt_address_t *bd_addr, uint8_t num_active, uint8_t num_held,
                                             hfp_ag_call_state_t call_state, hfp_call_addrtype_t addr_type,
                                             char *str1, char* str2);

bt_status_t bt_sal_hfp_ag_notify_device_status_changed(const bt_address_t *bd_addr, uint32_t val1, uint32_t val2, uint32_t val3, uint32_t val4);

bt_status_t bt_sal_hfp_ag_set_inband_ring_enable(const bt_address_t *bd_addr, bool ring);

bt_status_t bt_sal_hfp_ag_dial_response(const bt_address_t *bd_addr, uint32_t val);

bt_status_t bt_sal_hfp_ag_cops_response(const bt_address_t *bd_addr, char* operator, uint16_t len);

bt_status_t bt_sal_hfp_ag_set_volume(const bt_address_t *bd_addr, hfp_volume_type_t type, uint8_t vol);

#endif