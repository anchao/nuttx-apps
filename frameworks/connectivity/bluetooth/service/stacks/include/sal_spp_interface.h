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
#ifndef __SAL_SPP_INTERFACE_H__
#define __SAL_SPP_INTERFACE_H__
#include <stdint.h>
#include "bt_device.h"
#include "bt_uuid.h"

bt_status_t bt_sal_spp_init();
bt_status_t bt_sal_spp_cleanup();
bt_status_t bt_sal_spp_server_start(uint16_t scn, bt_uuid_t *uuid, uint8_t max_connection);
bt_status_t bt_sal_spp_server_stop(uint16_t scn);
bt_status_t bt_sal_spp_connect(const bt_address_t *addr, uint16_t conn_port, bt_uuid_t *uuid);
bt_status_t bt_sal_spp_disconnect(uint16_t conn_port);
bt_status_t bt_sal_spp_data_received_response(uint16_t conn_port, uint8_t *buf);
bt_status_t bt_sal_spp_write(uint16_t conn_port, uint8_t* buf, uint16_t size);
bt_status_t bt_sal_spp_connect_request_reply(const bt_address_t *addr, uint16_t port, bool accept);

#endif