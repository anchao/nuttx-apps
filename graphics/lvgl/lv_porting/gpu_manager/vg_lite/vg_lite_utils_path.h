/**
 * @file vg_lite_utils_path.h
 *
 */

#ifndef VG_LITE_UTILS_PATH_H
#define VG_LITE_UTILS_PATH_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../lv_gpu_types.h"

#ifdef CONFIG_LV_GPU_USE_VG_LITE

#include <lvgl/lvgl.h>
#include <vg_lite.h>

/*********************
 *      DEFINES
 *********************/

#define VG_LITE_ANGLE_RES 0.01f

/**********************
 *      TYPEDEFS
 **********************/

typedef enum {
    CURVE_NOP = 0,
    CURVE_END = 1, /* optional, must be preceded by CURVE_CLOSE */
    CURVE_LINE = 3, /* draw a line to next point */
    CURVE_CLOSE = 4, /* close the curve after this point */
    CURVE_QUAD = 5, /* the next two points will be control points */
    CURVE_CUBIC = 7, /* the next three points will be control points */
    CURVE_ARC_90 = 8, /* draw a right angle arc. the next point is center */
    CURVE_ARC_ACUTE = 9 /* draw an acute angle arc. obtuse arc should be splitted */
} vg_lite_curve_op_t;

typedef enum {
    CURVE_FILL_COLOR,
    CURVE_FILL_IMAGE,
    CURVE_FILL_LINEAR_GRADIENT,
    CURVE_FILL_RADIAL_GRADIENT
} vg_lite_curve_fill_type_t;

typedef struct {
    const lv_img_dsc_t * img_dsc;
    const lv_draw_img_dsc_t * draw_dsc;
    const lv_area_t * area;
} vg_lite_img_dsc_t;

typedef struct {
    vg_lite_curve_fill_type_t type;
    vg_lite_fill_t fill_rule;
    lv_color_t color;
    lv_opa_t opa;
    const vg_lite_img_dsc_t * img_dsc;
    const lv_grad_dsc_t * grad_dsc;
    const lv_area_t * grad_area;
} vg_lite_curve_fill_dsc_t;

typedef struct {
    float x;
    float y;
} vg_lite_fpoint_t;

typedef struct {
    const lv_point_t * points;
    const vg_lite_fpoint_t * fpoints;
    uint32_t num;
    const vg_lite_curve_op_t * op;
    const vg_lite_curve_fill_dsc_t * fill_dsc;
} vg_lite_curve_dsc_t;

typedef enum {
    VG_LITE_LINE_PATH,
    VG_LITE_RECT_PATH,
    VG_LITE_ARC_PATH,
    VG_LITE_POLYGON_PATH,
    VG_LITE_CIRCLE_PATH,
    VG_LITE_POINT_PATH,
} vg_lite_shape_path_type_t;

typedef struct {
    lv_coord_t num;
} vg_lite_polygon_dsc_t;

typedef struct {
    lv_draw_arc_dsc_t dsc;
    float radius;
    float start_angle;
    float end_angle;
} vg_lite_arc_dsc_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void vg_lite_curve_fill_dsc_init(vg_lite_curve_fill_dsc_t * fill_dsc);

bool vg_lite_draw_curve(lv_gpu_dest_buf_t * dest, const vg_lite_curve_dsc_t * curve_dsc);

bool vg_lite_draw_path(
    lv_gpu_dest_buf_t * dest,
    float * path_data,
    uint32_t path_data_size,
    const vg_lite_curve_fill_dsc_t * fill_dsc);

bool vg_lite_draw_mask_to_path(vg_lite_path_t * vg_path, const lv_area_t * area);
void vg_lite_draw_mask_clean_path(vg_lite_path_t * vg_path);

uint16_t vg_lite_fill_path(
    float * path_data,
    vg_lite_shape_path_type_t type,
    const lv_point_t * points,
    const void * dsc);

uint32_t vg_lite_calculate_path_length(vg_lite_shape_path_type_t type, const void * dsc);

void vg_lite_curve_fill_dsc_dump_info(const vg_lite_curve_fill_dsc_t * dsc);

/**********************
 *      MACROS
 **********************/

#endif /* CONFIG_LV_GPU_USE_VG_LITE */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*VG_LITE_UTILS_PATH_H*/
