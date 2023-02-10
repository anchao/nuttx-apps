/**
 * @file vglite_internal.h
 *
 */

#ifndef VGLITE_INTERNAL_H
#define VGLITE_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "vg_lite_utils.h"

#ifdef CONFIG_LV_GPU_USE_VG_LITE

/*********************
 *      DEFINES
 *********************/

#define VG_LITE_MAX_PATH_LENGTH 32512 /* when GPU CMD_BUF_LEN == 32k */
#define VG_LITE_RECT_PATH_LEN 13 /* 3(MOVE) + 3(LINE) * 3 + 1(CLOSE/END) */
#define VG_LITE_POINT_PATH_LEN 41 /* 3(MOVE) + 3(LINE) * 3 + 7(CUBIC) * 4 + 1(CLOSE/END) */
#define VG_LITE_POINT_PATH_SIZE (VG_LITE_POINT_PATH_LEN * sizeof(float))
#define VG_LITE_LINE_PATH_SIZE 52 /* (3(LINE) * 4 + 1(CLOSE/END)) * sizeof(float) */
#define VG_LITE_LINE_PATH_ROUND_DELTA 44 /* (7(CUBIC) * 2 - 3(LINE)) * sizeof(float) */
#define VG_LITE_POLYGON_PATH_SIZE(n) (((n)*3 + 1) * sizeof(float))
#define VG_LITE_ARC_MAX_PATH_LEN 89

#ifndef VLC_GRADIENT_BUFFER_WIDTH
#define VLC_GRADIENT_BUFFER_WIDTH 256
#endif

#if VGLITE_HEADER_VERSION > 6
#define vg_lite_set_multiply_color(color) LV_UNUSED(color)
#endif

#define VG_LITE_IMG_SRC_PX_ALIGN CONFIG_LV_GPU_VG_LITE_IMG_SRC_PX_ALIGN
#define VG_LITE_IMG_SRC_ADDR_ALIGN 64

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    lv_draw_ctx_t base;
    lv_ll_t decoder_ll;
    lv_img_decoder_t * raw_decoder;
    lv_img_decoder_t * evo_decoder;
    lv_img_decoder_t * etc2_decoder;
    float rect_path[VG_LITE_POINT_PATH_SIZE * 2];
    float arc_path[VG_LITE_ARC_MAX_PATH_LEN];
    uint32_t polygon_grad_mem[VLC_GRADIENT_BUFFER_WIDTH];
} vg_lite_draw_ctx_t;

typedef struct {
    lv_draw_layer_ctx_t base_draw;
    uint32_t buf_size_bytes : 31;
    uint32_t has_alpha : 1;
    lv_area_t area_aligned;
} vg_lite_draw_layer_ctx_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void vg_lite_draw_rect(
    lv_draw_ctx_t * draw_ctx,
    const lv_draw_rect_dsc_t * dsc,
    const lv_area_t * coords);

void vg_lite_draw_bg(
    lv_draw_ctx_t * draw_ctx,
    const lv_draw_rect_dsc_t * dsc,
    const lv_area_t * coords);

void vg_lite_draw_arc(
    lv_draw_ctx_t * draw_ctx,
    const lv_draw_arc_dsc_t * dsc,
    const lv_point_t * center,
    uint16_t radius,
    uint16_t start_angle,
    uint16_t end_angle);

void vg_lite_draw_img_decoded(
    lv_draw_ctx_t * draw_ctx,
    const lv_draw_img_dsc_t * dsc,
    const lv_area_t * coords,
    const uint8_t * map_p,
    lv_img_cf_t color_format);

void vg_lite_draw_polygon(
    lv_draw_ctx_t * draw_ctx,
    const lv_draw_rect_dsc_t * draw_dsc,
    const lv_point_t * points,
    uint16_t point_cnt);

void vg_lite_draw_line(
    lv_draw_ctx_t * draw_ctx,
    const lv_draw_line_dsc_t * dsc,
    const lv_point_t * point1,
    const lv_point_t * point2);

void vg_lite_wait_for_finish(lv_draw_ctx_t * draw_ctx);

lv_draw_layer_ctx_t * vg_lite_draw_layer_init(
    lv_draw_ctx_t * draw_ctx,
    lv_draw_layer_ctx_t * layer_ctx,
    lv_draw_layer_flags_t flags);

void vg_lite_draw_layer_adjust(
    lv_draw_ctx_t * draw_ctx,
    lv_draw_layer_ctx_t * layer_ctx,
    lv_draw_layer_flags_t flags);

void vg_lite_draw_layer_blend(
    lv_draw_ctx_t * draw_ctx,
    lv_draw_layer_ctx_t * layer_ctx,
    const lv_draw_img_dsc_t * draw_dsc);

void vg_lite_draw_layer_destroy(lv_draw_ctx_t * draw_ctx, lv_draw_layer_ctx_t * layer_ctx);

/**********************
 *      MACROS
 **********************/

#endif /* CONFIG_LV_GPU_USE_VG_LITE */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*VGLITE_INTERNAL_H*/
