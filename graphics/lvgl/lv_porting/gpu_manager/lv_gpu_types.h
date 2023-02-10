/**
 * @file lv_gpu_types.h
 *
 */

#ifndef LV_GPU_TYPES_H
#define LV_GPU_TYPES_H

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

typedef struct _lv_gpu_ctx_t {
    lv_draw_ctx_t * draw_ctx;
    lv_draw_ctx_t * main_draw_ctx;

    size_t draw_ctx_size;

    void (*draw_ctx_init)(lv_disp_drv_t * disp_drv, lv_draw_ctx_t * draw_ctx);
    void (*draw_ctx_deinit)(lv_disp_drv_t * disp_drv, lv_draw_ctx_t * draw_ctx);

    const char * name;
    bool draw_ok;

    struct _lv_gpu_ctx_t * fallback;
} lv_gpu_ctx_t;

typedef struct {
    void * buf;
    lv_area_t * buf_area;
    lv_area_t * clip_area;
    lv_img_cf_t cf;
} lv_gpu_dest_buf_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_GPU_TYPES_H*/
