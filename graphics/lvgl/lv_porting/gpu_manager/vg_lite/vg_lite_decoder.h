/**
 * @file vg_lite_decoder.h
 *
 */

#ifndef VG_LITE_DECODER_H
#define VG_LITE_DECODER_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "vg_lite_internal.h"

#ifdef CONFIG_LV_GPU_USE_VG_LITE

/*********************
 *      DEFINES
 *********************/

#define VG_LITE_ETC2_COLOR_FORMAT   LV_IMG_CF_RESERVED_15
#define VG_LITE_DEC_COLOR_FORMAT    LV_IMG_CF_RESERVED_16
#define VG_LITE_YUV_COLOR_FORMAT    LV_IMG_CF_RESERVED_17

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void vg_lite_decoder_init(vg_lite_draw_ctx_t * draw_ctx);
void vg_lite_decoder_uninit(vg_lite_draw_ctx_t * draw_ctx);
void vg_lite_decoder_raw_init(lv_img_decoder_t * dec);
void vg_lite_decoder_evo_init(lv_img_decoder_t * dec);
void vg_lite_decoder_etc2_init(lv_img_decoder_t * dec);

/**********************
 *      MACROS
 **********************/

#endif /* CONFIG_LV_GPU_USE_VG_LITE */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*VG_LITE_DECODER_H*/
