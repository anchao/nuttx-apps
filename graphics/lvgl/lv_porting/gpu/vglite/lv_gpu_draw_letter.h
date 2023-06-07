/****************************************************************************
 * apps/graphics/lvgl/lv_porting/gpu/lv_gpu_draw_letter.h
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef __LV_GPU_DRAW_LETTER_H__
#define __LV_GPU_DRAW_LETTER_H__

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <lvgl/lvgl.h>
#include <nuttx/config.h>

/****************************************************************************
 * Public Prototypes
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C" {
#else
#define EXTERN extern
#endif

void lv_draw_letter_gpu_init(lv_draw_ctx_t* draw_ctx);

lv_res_t lv_draw_letter_gpu(
    lv_draw_ctx_t* draw_ctx,
    const lv_draw_label_dsc_t* dsc,
    const lv_point_t* pos_p,
    uint32_t letter);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __LV_GPU_DRAW_LETTER_H__ */
