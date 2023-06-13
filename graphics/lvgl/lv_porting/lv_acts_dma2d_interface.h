/****************************************************************************
 * apps/graphics/lvgl/lv_porting/lv_acts_dma2d_interface.h
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

#ifndef __LV_ACTS_DMA2D_INTERFACE_H__
#define __LV_ACTS_DMA2D_INTERFACE_H__

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <lvgl/lvgl.h>
#include <lvgl/src/draw/sw/lv_draw_sw.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if defined(CONFIG_LV_USE_ACTS_DMA2D_INTERFACE)

/****************************************************************************
 * Macros
 ****************************************************************************/

/****************************************************************************
 * Type Definitions
 ****************************************************************************/

typedef lv_draw_sw_ctx_t lv_acts_dma2d_draw_ctx_t;

/****************************************************************************
 * Public Data
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Name: lv_acts_dma2d_interface_init
 *
 * Description:
 *   Actions DMA2D interface initialization.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 * @return LV_RES_OK on success; LV_RES_INV on failure. (always succeed)
 *
 ****************************************************************************/

lv_res_t lv_acts_dma2d_interface_init(void);

/****************************************************************************
 * Name: lv_acts_dma2d_draw_ctx_init
 *
 * Description:
 *   Actions DMA2D draw context init callback. (Do not call directly)
 *
 * Input Parameters:
 * @param drv lvgl display driver
 * @param draw_ctx lvgl draw context struct
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void lv_acts_dma2d_draw_ctx_init(lv_disp_drv_t * drv,
                                 lv_draw_ctx_t * draw_ctx);

/****************************************************************************
 * Name: lv_acts_dma2d_draw_ctx_deinit
 *
 * Description:
 *   Actions DMA2D draw context deinit callback. (Do not call directly)
 *
 * Input Parameters:
 * @param drv lvgl display driver
 * @param draw_ctx lvgl draw context struct
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void lv_acts_dma2d_draw_ctx_deinit(lv_disp_drv_t * drv,
                                   lv_draw_ctx_t * draw_ctx);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* CONFIG_LV_USE_ACTS_DMA2D_INTERFACE */
#endif /* __LV_ACTS_DMA2D_INTERFACE_H__ */
