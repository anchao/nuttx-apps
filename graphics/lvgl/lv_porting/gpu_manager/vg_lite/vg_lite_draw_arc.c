/**
 * @file vg_lite_draw_arc.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "vg_lite_internal.h"

#ifdef CONFIG_LV_GPU_USE_VG_LITE

#include "vg_lite_utils_path.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static bool vg_lite_draw_arc_float(vg_lite_draw_ctx_t * draw_ctx, const lv_draw_arc_dsc_t * dsc,
                                   const lv_point_t * center,
                                   uint16_t radius, uint16_t start_angle, uint16_t end_angle);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void vg_lite_draw_arc(lv_draw_ctx_t * draw_ctx, const lv_draw_arc_dsc_t * dsc, const lv_point_t * center,
                      uint16_t radius, uint16_t start_angle, uint16_t end_angle)
{
    lv_gpu_ctx_t * gpu = draw_ctx->user_data;
    if(dsc->opa <= LV_OPA_MIN || dsc->width == 0 || start_angle == end_angle) {
        gpu->draw_ok = true;
        return;
    }
    gpu->draw_ok = vg_lite_draw_arc_float((vg_lite_draw_ctx_t *)draw_ctx, dsc, center, radius, start_angle, end_angle);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static bool vg_lite_draw_arc_float(
    vg_lite_draw_ctx_t * draw_ctx,
    const lv_draw_arc_dsc_t * dsc,
    const lv_point_t * center,
    uint16_t radius,
    uint16_t start_angle,
    uint16_t end_angle)
{
    lv_area_t coords;
    lv_coord_t radius16 = (lv_coord_t)radius;
    coords.x1 = center->x - radius16;
    coords.y1 = center->y - radius16;
    /* -1 because the center already belongs to the left/bottom part */
    coords.x2 = center->x + radius16 - 1;
    coords.y2 = center->y + radius16 - 1;

    lv_area_t clip_area;
    if(_lv_area_intersect(&clip_area, &coords, draw_ctx->base.clip_area) == false) {
        return true;
    }

    vg_lite_arc_dsc_t arc_dsc;
    lv_memcpy_small(&arc_dsc.dsc, dsc, sizeof(lv_draw_arc_dsc_t));
    arc_dsc.radius = radius;

    while(start_angle > 360.0f) {
        start_angle -= 360.0f;
    }
    while(start_angle < -VG_LITE_ANGLE_RES) {
        start_angle += 360.0f;
    }

    while(end_angle > 360.0f) {
        end_angle -= 360.0f;
    }
    while(end_angle < start_angle - VG_LITE_ANGLE_RES) {
        end_angle += 360.0f;
    }

    arc_dsc.start_angle = start_angle;
    arc_dsc.end_angle = end_angle;
    uint16_t path_length = vg_lite_fill_path(draw_ctx->arc_path, VG_LITE_ARC_PATH, center, &arc_dsc);
    path_length *= sizeof(float);

    vg_lite_curve_fill_dsc_t fill_dsc;
    vg_lite_curve_fill_dsc_init(&fill_dsc);
    fill_dsc.color = dsc->color;
    fill_dsc.opa = dsc->opa;
    fill_dsc.fill_rule = LV_ABS(end_angle - start_angle) < VG_LITE_ANGLE_RES ? VG_LITE_FILL_EVEN_ODD
                         : VG_LITE_FILL_NON_ZERO;

    lv_gpu_dest_buf_t gpu_buf = {
        .buf = draw_ctx->base.buf,
        .buf_area = draw_ctx->base.buf_area,
        .clip_area = &clip_area,
        .cf = LV_IMG_CF_TRUE_COLOR
    };
    LV_GPU_DUMP_DEST_BUFFER_INFO(&gpu_buf);

    vg_lite_img_dsc_t vg_img_dsc;
    lv_memset_00(&vg_img_dsc, sizeof(vg_img_dsc));

    lv_img_dsc_t img_dsc;
    lv_memset_00(&img_dsc, sizeof(img_dsc));

    lv_draw_img_dsc_t draw_dsc;
    lv_draw_img_dsc_init(&draw_dsc);
    draw_dsc.angle = 0;
    draw_dsc.zoom = LV_IMG_ZOOM_NONE;
    draw_dsc.recolor_opa = 0;

    vg_img_dsc.area = &coords;
    vg_img_dsc.draw_dsc = &draw_dsc;
    _lv_img_cache_entry_t * cache_entry = NULL;

    if(dsc->img_src) {
        lv_color32_t color = (lv_color32_t)lv_color_to32(dsc->color);
        LV_COLOR_SET_A32(color, 0);
        cache_entry = _lv_img_cache_open(dsc->img_src, color, 0);
    }

    if(cache_entry) {
        img_dsc.header = cache_entry->dec_dsc.header;
        img_dsc.data = cache_entry->dec_dsc.img_data;
        fill_dsc.type |= CURVE_FILL_IMAGE;
        fill_dsc.img_dsc = &vg_img_dsc;
        vg_img_dsc.img_dsc = &img_dsc;
    }
    else {
        fill_dsc.type |= CURVE_FILL_COLOR;
    }

    return vg_lite_draw_path(&gpu_buf, draw_ctx->arc_path, path_length, &fill_dsc);
}

#endif /* CONFIG_LV_GPU_USE_VG_LITE */
