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

#ifndef __SAL_PAN_INTERFACE_H__
#define __SAL_PAN_INTERFACE_H__

#include <stdint.h>
#include <stdio.h>

#include "bt_pan.h"
#include "pan_service.h"

/**
 * @brief Initialize PAN service adaptation layer
 *
 * @param max_connections Maximum number of connections
 * @param role PAN role
 * @return bt_status_t Operation status
 */
bt_status_t bt_sal_pan_init(int max_connections, pan_role_t role);

/**
 * @brief Clean up PAN service adaptation layer resources
 *
 * @return bt_status_t Operation status
 */
bt_status_t bt_sal_pan_cleanup(void);

/**
 * @brief Connect to remote PAN device
 *
 * @param addr Remote device address
 * @param dst_role Destination role
 * @param src_role Source role
 * @return bt_status_t Operation status
 */
bt_status_t bt_sal_pan_connect(bt_address_t* addr, uint8_t dst_role, uint8_t src_role);

/**
 * @brief Disconnect from remote PAN device
 *
 * @param addr Remote device address
 * @return bt_status_t Operation status
 */
bt_status_t bt_sal_pan_disconnect(bt_address_t* addr);

/**
 * @brief Send data over PAN
 *
 * @param addr Target device address
 * @param protocol Protocol type
 * @param dst_addr Destination MAC address
 * @param src_addr Source MAC address
 * @param data Data buffer
 * @param length Data length
 * @return bt_status_t Operation status
 */
bt_status_t bt_sal_pan_write(bt_address_t* addr, uint16_t protocol,
                            uint8_t* dst_addr, uint8_t* src_addr,
                            uint8_t* data, uint16_t length);

#endif // __SAL_PAN_INTERFACE_H__