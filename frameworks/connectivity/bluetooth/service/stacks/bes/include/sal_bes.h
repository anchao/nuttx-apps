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
#ifndef __SAL_BES_H_
#define __SAL_BES_H_

/****************************** header include ********************************/
#include "bt_addr.h"
#include "bt_status.h"

/***************************** external declaration *****************************/

/***************************** macro defination *******************************/
#define AVDTP_RTP_HEADER_LEN 12
#define STREAM_DATA_RESERVED AVDTP_RTP_HEADER_LEN

#define BT_SAL_ADDR_IVALED(addr) ((addr[0] || addr[1] ||addr[2] ||addr[3] ||addr[4] || addr[5]))

#define TO_BTH_ADDRESS(addr) (bth_address_t *)(addr)
#define TO_BT_ADDRESS(addr) (bt_address_t *)(addr)

#define CHECK_BES_STACK_RETURN(call)    \
    do                                  \
    {                                   \
        bth_bt_status_t ret = call;     \
        if (ret != BTH_STATUS_SUCCESS)  \
        {                               \
            return BT_STATUS_FAIL;      \
        }                               \
    } while(0);                         \

/*****************************  type defination ********************************/

/*****************************  function declaration ****************************/
bt_status_t bt_sal_lock_init();

bt_status_t bt_sal_lock_deinit();

bt_status_t bt_sal_lock();

bt_status_t bt_sal_unlock();

bt_status_t bt_sal_cond_init();

bt_status_t bt_sal_cond_deinit();

bt_status_t bt_sal_cond_wait(int timeout_ms);

bt_status_t bt_sal_cond_signal(bool broadcast);

bt_status_t bt_sal_set_async_send_check();

bt_status_t bt_sal_set_async_info(uint8_t* data, int data_len, bt_status_t async_result);

bt_status_t bt_sal_get_async_info(uint8_t** data, int* data_len);

bt_status_t bt_enable();

#endif
