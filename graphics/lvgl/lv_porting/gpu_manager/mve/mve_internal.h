/**
 * @file mve_internal.h
 *
 */

#ifndef MVE_INTERNAL_H
#define MVE_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../lv_gpu_utils.h"

#ifdef CONFIG_LV_GPU_USE_ARM_MVE

#include "../lv_gpu_types.h"
#include <lvgl/lvgl.h>
#include <lvgl/src/draw/sw/lv_draw_sw.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef lv_draw_sw_ctx_t mve_draw_ctx_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void mve_draw_blend(lv_draw_ctx_t * draw_ctx, const lv_draw_sw_blend_dsc_t * dsc);

/**********************
 *      MACROS
 **********************/

#endif /* ifdef CONFIG_LV_GPU_USE_ARM_MVE */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*MVE_INTERNAL_H*/
