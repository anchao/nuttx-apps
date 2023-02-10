/**
 * @file lv_jpeg_turbo.h
 *
 */

#ifndef LV_JPEG_TURBO_H
#define LV_JPEG_TURBO_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include <lvgl/lvgl.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Register the JPEG decoder functions in LVGL
 */
void lv_jpeg_turbo_custom_init(lv_img_decoder_t * dec);

static inline void lv_jpeg_turbo_init(void)
{
    lv_jpeg_turbo_custom_init(lv_img_decoder_create());
}

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_JPEG_TURBO_H*/