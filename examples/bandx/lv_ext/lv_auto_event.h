/*
 * Copyright (C) 2020 Xiaomi Corporation
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
 */
#ifndef __LV_AUTO_EVENT_H
#define __LV_AUTO_EVENT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl/lvgl.h"

    typedef struct
    {
        lv_obj_t **obj_p;
        lv_event_t event;
        uint32_t delay;
        void *user_data;
    } lv_auto_event_data_t;

    typedef struct
    {
        lv_task_t *task;
        lv_auto_event_data_t *auto_event_data;
        uint32_t len;
        uint32_t run_index;
    } lv_auto_event_t;

    lv_auto_event_t *lv_auto_event_create(lv_auto_event_data_t *auto_event_data, uint32_t len);
    void lv_auto_event_del(lv_auto_event_t *ae);

#ifdef __cplusplus
}
#endif

#endif
