/**
 * @file vg_lite_draw_polygon.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "vg_lite_internal.h"

#ifdef CONFIG_LV_GPU_USE_VG_LITE

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void vg_lite_draw_polygon(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * draw_dsc,
                          const lv_point_t * points, uint16_t point_cnt)
{
    lv_gpu_ctx_t * gpu = draw_ctx->user_data;
    vg_lite_error_t error;
    vg_lite_draw_ctx_t * vg_draw_ctx = (vg_lite_draw_ctx_t *)draw_ctx;

    /* Init destination vglite buffer */
    const lv_area_t * disp_area = draw_ctx->buf_area;
    lv_color_t * disp_buf = draw_ctx->buf;

    vg_lite_buffer_t dest_vg_buf;
    if(!vg_lite_custom_buffer_init(
           &dest_vg_buf,
           disp_buf,
           lv_area_get_width(disp_area),
           lv_area_get_height(disp_area),
           VG_LITE_NATIVE_COLOR_FMT)) {
        return;
    }
    VG_LITE_DUMP_BUFFER_INFO(&dest_vg_buf);

    /* Convert to vglite path */
    uint32_t path_size = (point_cnt * 3 + 1) * sizeof(int16_t);
    int16_t * poly_path = lv_mem_buf_get(path_size);
    LV_ASSERT_MALLOC(poly_path);
    if(!poly_path) {
        LV_GPU_LOG_WARN("out of memory");
        return;
    }

    lv_area_t poly_coords = { points->x, points->y, points->x, points->y };
    poly_path[0] = VLC_OP_MOVE;
    poly_path[1] = points->x;
    poly_path[2] = points->y;
    for(int_fast16_t i = 1; i < point_cnt; i++) {
        poly_path[i * 3] = VLC_OP_LINE;
        poly_path[i * 3 + 1] = points[i].x;
        poly_path[i * 3 + 2] = points[i].y;
        poly_coords.x1 = LV_MIN(poly_coords.x1, points[i].x);
        poly_coords.y1 = LV_MIN(poly_coords.y1, points[i].y);
        poly_coords.x2 = LV_MAX(poly_coords.x2, points[i].x);
        poly_coords.y2 = LV_MAX(poly_coords.y2, points[i].y);
    }

    poly_path[point_cnt * 3] = VLC_OP_END;

    /* If no intersection then draw complete */
    lv_area_t clip_area;
    if(!_lv_area_intersect(&clip_area, &poly_coords, draw_ctx->clip_area)) {
        goto error_handler;
    }

    /* Calculate some parameters */
    lv_coord_t poly_w = lv_area_get_width(&poly_coords);
    lv_coord_t poly_h = lv_area_get_height(&poly_coords);
    vg_lite_path_t vpath;
    lv_memset_00(&vpath, sizeof(vg_lite_path_t));
    vg_lite_init_path(
        &vpath, VG_LITE_S16, VG_LITE_HIGH, path_size,
        poly_path, clip_area.x1, clip_area.y1, clip_area.x2 + 1, clip_area.y2 + 1);

    vg_lite_matrix_t matrix;
    vg_lite_identity(&matrix);
    vg_lite_blend_t blend = vg_lite_lv_blend_mode_to_vg_blend_mode(draw_dsc->blend_mode);
    vg_lite_linear_gradient_t grad;

    LV_GPU_LOG_TRACE("blend: 0x%x (%s)", blend, vg_lite_get_blend_string(blend));

    if(draw_dsc->bg_grad.dir != LV_GRAD_DIR_NONE) {
        /* Linear gradient fill */
        vg_lite_custom_buffer_init(
            &grad.image,
            vg_draw_ctx->polygon_grad_mem,
            VLC_GRADIENT_BUFFER_WIDTH,
            1,
            VG_LITE_BGRA8888);
        VG_LITE_DUMP_BUFFER_INFO(&grad.image);

        grad.count = draw_dsc->bg_grad.stops_count;

        if(draw_dsc->bg_opa != LV_OPA_COVER) {
            for(int i = 0; i < grad.count; i++) {
                lv_color_t color = draw_dsc->bg_grad.stops[i].color;
                color = lv_color_mix(color, lv_color_black(), draw_dsc->bg_opa);
                LV_COLOR_SET_A(color, draw_dsc->bg_opa);
                grad.colors[i] = lv_color_to32(color);
                grad.stops[i] = draw_dsc->bg_grad.stops[i].frac;
            }
        }
        else {
            for(int i = 0; i < grad.count; i++) {
                lv_color_t color = draw_dsc->bg_grad.stops[i].color;
                grad.colors[i] = lv_color_to32(color);
                grad.stops[i] = draw_dsc->bg_grad.stops[i].frac;
            }
        }

        VG_LITE_CHECK_ERROR(vg_lite_update_grad(&grad));

        vg_lite_identity(&grad.matrix);
        vg_lite_translate(poly_coords.x1 - disp_area->x1, poly_coords.y1 - disp_area->y1, &grad.matrix);

        if(draw_dsc->bg_grad.dir == LV_GRAD_DIR_VER) {
            vg_lite_scale(1.0f, poly_h / 256.0f, &grad.matrix);
            vg_lite_rotate(90.0f, &grad.matrix);
        }
        else {
            vg_lite_scale(poly_w / 256.0f, 1.0f, &grad.matrix);
        }

        VG_LITE_CHECK_ERROR(vg_lite_draw_gradient(&dest_vg_buf, &vpath, VG_LITE_FILL_NON_ZERO, &matrix, &grad, blend));
    }
    else {
        /* Regular fill */
        vg_lite_color_t color = vg_lite_lv_color_to_vg_color(draw_dsc->bg_color, draw_dsc->bg_opa);
        VG_LITE_CHECK_ERROR(vg_lite_draw(&dest_vg_buf, &vpath, VG_LITE_FILL_NON_ZERO, &matrix, blend, color));
    }

    VG_LITE_CHECK_ERROR(vg_lite_finish());

    gpu->draw_ok = true;

error_handler:
    lv_mem_buf_release(poly_path);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /* CONFIG_LV_GPU_USE_VG_LITE */
