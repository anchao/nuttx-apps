/**
 * @file lv_gpu_conv.h
 *
 */

#ifndef LV_GPU_CONV_H
#define LV_GPU_CONV_H

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

typedef enum {
    LV_GPU_CONV_TYPE_PRE_MULTIPLY,
    LV_GPU_CONV_TYPE_RECOLOR_PALETTE,
    LV_GPU_CONV_TYPE_BGRA5658_TO_BGRA8888,
    LV_GPU_CONV_TYPE_INDEXED8_TO_BGRA8888,
    LV_GPU_CONV_TYPE_BGR565_TO_VGIMG,
    LV_GPU_CONV_TYPE_BGR888_TO_VGIMG,
    LV_GPU_CONV_TYPE_BGRA8888_TO_VGIMG,
    LV_GPU_CONV_TYPE_GAUSSIAN_BLUR,
    LV_GPU_CONV_TYPE_GAUSSIAN_HOR_BLUR,
    LV_GPU_CONV_TYPE_GAUSSIAN_VER_BLUR,
    _LV_GPU_CONV_TYPE_LAST,
} lv_gpu_conv_type_t;

typedef enum {
    LV_GPU_CONV_RES_OK,
    LV_GPU_CONV_RES_NO_IMPLEMENTATION,
    LV_GPU_CONV_RES_PARAM_ERROR,
    LV_GPU_CONV_RES_ADDRESS_UNALIGN,
    LV_GPU_CONV_RES_OUT_OF_MEMORY,
    LV_GPU_CONV_RES_UNKNOWN
} lv_gpu_conv_res_t;

typedef lv_gpu_conv_res_t (*lv_gpu_conv_callback_t)(void * dsc);

/* LV_GPU_CONV_TYPE_PRE_MULTIPLY */

typedef struct {
    lv_color32_t * dst;
    const lv_color32_t * src;
    uint32_t count;
} lv_gpu_conv_pre_multiply_dsc_t;

/* LV_GPU_CONV_TYPE_RECOLOR_PALETTE */
typedef struct {
    lv_color32_t * dst;
    const lv_color32_t * src;
    uint16_t size;
    uint32_t recolor;
} lv_gpu_conv_recolor_palette_dsc_t;

typedef struct {
    uint8_t * px_buf;
    uint32_t buf_stride;
    const uint8_t * px_map;
    uint32_t map_stride;
    const lv_img_header_t * header;
} lv_gpu_conv_base_dsc_t;

/* LV_GPU_CONV_TYPE_BGRA5658_TO_BGRA8888 */
typedef struct {
    lv_gpu_conv_base_dsc_t base;
    lv_color32_t recolor;
} lv_gpu_conv_bgra5658_to_bgra8888_dsc_t;

/* LV_GPU_CONV_TYPE_INDEXED8_TO_BGRA8888 */
typedef struct {
    lv_gpu_conv_base_dsc_t base;
    const uint32_t * palette;
} lv_gpu_conv_indexed8_to_bgra8888_dsc_t;

typedef struct {
    lv_gpu_conv_base_dsc_t base;
    lv_color32_t recolor;
    union {
        uint32_t ckey;
        bool preprocessed;
    };
} lv_gpu_conv_x_to_vgimg_dsc_t;

/* LV_GPU_CONV_TYPE_BGR565_TO_VGIMG */
typedef lv_gpu_conv_x_to_vgimg_dsc_t lv_gpu_conv_bgr565_to_vgimg_dsc_t;

/* LV_GPU_CONV_TYPE_BGR888_TO_VGIMG */
typedef lv_gpu_conv_x_to_vgimg_dsc_t lv_gpu_conv_bgr888_to_vgimg_dsc_t;

/* LV_GPU_CONV_TYPE_BGRA8888_TO_VGIMG */
typedef lv_gpu_conv_x_to_vgimg_dsc_t lv_gpu_conv_bgra8888_to_vgimg_dsc_t;

/* LV_GPU_CONV_TYPE_GAUSSIAN_BLUR */

typedef struct {
    lv_color_t * dst;
    uint32_t dst_stride;
    lv_color_t * src;
    uint32_t src_stride;
    const lv_area_t * blur_area;
    int r;
} lv_gpu_conv_gaussian_blur_dsc_t;

/* LV_GPU_CONV_TYPE_GAUSSIAN_HOR/VER_BLUR */

typedef struct {
    lv_color_t * dst;
    const lv_color_t * src;
    lv_coord_t dst_stride;
    lv_coord_t src_stride;
    lv_coord_t w;
    lv_coord_t h;
    lv_coord_t r;
    lv_color_t * tmp;
} lv_gpu_conv_gaussian_dir_blur_dsc_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lv_gpu_conv_init(void);

void lv_gpu_conv_deinit(void);

void lv_gpu_conv_set_callback(lv_gpu_conv_type_t type, lv_gpu_conv_callback_t callback);

lv_gpu_conv_res_t lv_gpu_conv_start(lv_gpu_conv_type_t type, void * dsc);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_GPU_CONV_H*/
