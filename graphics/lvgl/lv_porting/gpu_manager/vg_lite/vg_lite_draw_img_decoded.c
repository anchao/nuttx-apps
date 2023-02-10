/**
 * @file vg_lite_draw_img_decoded.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "../lv_gpu_conv.h"
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

static void fill_rect_path(int16_t * path, const lv_area_t * area);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void vg_lite_draw_img_decoded(
    lv_draw_ctx_t * draw_ctx,
    const lv_draw_img_dsc_t * dsc,
    const lv_area_t * coords,
    const uint8_t * map_p,
    lv_img_cf_t color_format)
{
    lv_gpu_ctx_t * gpu = draw_ctx->user_data;
    lv_opa_t opa = dsc->opa;

    if(opa < LV_OPA_MIN) {
        gpu->draw_ok = true;
        return;
    }

    vg_lite_buffer_t * vg_buf = vg_lite_img_data_to_vg_buf((void *)map_p);

    uint16_t angle = dsc->angle;
    uint16_t zoom = dsc->zoom;
    lv_point_t pivot = dsc->pivot;
    lv_color32_t recolor = { .full = lv_color_to32(dsc->recolor) };
    LV_COLOR_SET_A32(recolor, dsc->recolor_opa);

    vg_lite_buffer_t src_vg_buf;
    bool transformed = (angle != 0) || (zoom != LV_IMG_ZOOM_NONE);
    const lv_area_t * disp_area = draw_ctx->buf_area;
    int32_t map_w = lv_area_get_width(coords);
    int32_t map_h = lv_area_get_height(coords);
    lv_area_t coords_rel;

    lv_area_copy(&coords_rel, coords);
    lv_area_move(&coords_rel, -disp_area->x1, -disp_area->y1);
    vg_lite_buffer_t dest_vg_buf;
    vg_lite_error_t error;

    if(!vg_lite_custom_buffer_init(
           &dest_vg_buf,
           draw_ctx->buf,
           lv_area_get_width(draw_ctx->buf_area),
           lv_area_get_height(draw_ctx->buf_area),
           VG_LITE_NATIVE_COLOR_FMT)) {
        LV_GPU_LOG_TRACE("dest_vg_buf init failed");
        return;
    }

    lv_area_t map_tf, draw_area;
    lv_area_copy(&draw_area, draw_ctx->clip_area);
    _lv_img_buf_get_transformed_area(&map_tf, map_w, map_h, angle, zoom, &pivot);
    lv_area_move(&map_tf, coords->x1, coords->y1);

    if(_lv_area_intersect(&draw_area, &draw_area, &map_tf) == false) {
        LV_GPU_LOG_TRACE("not in area");
        gpu->draw_ok = true;
        return;
    }

    bool indexed = false, alpha = false;
    bool allocated_src = false;
    bool preprocessed = false;
    bool masked = lv_draw_mask_is_any(&draw_area);
    lv_color32_t pre_recolor;
    uint32_t * palette = NULL;

    if(vg_buf) {
        LV_GPU_LOG_TRACE("vg_buf = %p", vg_buf);
        indexed = VG_LITE_IS_INDEX_FMT(vg_buf->format);
        alpha = (vg_buf->format == VG_LITE_A4) || (vg_buf->format == VG_LITE_A8);
        pre_recolor.full = ((vg_lite_img_header_t *)map_p)->recolor;
        palette = (uint32_t *)(map_p + sizeof(vg_lite_img_header_t) + vg_buf->stride * vg_buf->height);
        preprocessed = true;

        if(!indexed && !alpha && dsc->recolor_opa != LV_OPA_TRANSP && pre_recolor.ch.alpha == LV_OPA_TRANSP) {
            LV_GPU_LOG_WARN("allocating new vg_buf:(%d,%d)", (int)vg_buf->width, (int)vg_buf->height);
            lv_img_header_t header;
            header.w = vg_buf->width;
            header.h = vg_buf->height;
            header.cf = color_format;

            if(!vg_lite_create_vg_buf_from_img_data(&src_vg_buf, vg_buf->memory, &header, NULL, recolor, preprocessed)) {
                LV_GPU_LOG_ERROR("load failed");
                goto error_handler;
            }

            allocated_src = true;
            pre_recolor = recolor;
        }
        else {
            LV_GPU_LOG_TRACE("copy info vg_buf -> src_vg_buf");
            lv_memcpy(&src_vg_buf, vg_buf, sizeof(src_vg_buf));
        }
    }
    else {
        LV_GPU_LOG_WARN("allocating new vg_buf:(%d, %d)", (int)map_w, (int)map_h);
        lv_img_header_t header;
        header.w = map_w;
        header.h = map_h;
        header.cf = color_format;

        if(!vg_lite_create_vg_buf_from_img_data(
               &src_vg_buf,
               map_p,
               &header,
               NULL,
               recolor,
               preprocessed)) {
            LV_GPU_LOG_ERROR("load failed");
            goto error_handler;
        }

        allocated_src = true;
        pre_recolor.full = ((vg_lite_img_header_t *)map_p)->recolor;
    }

    vg_lite_matrix_t matrix;
    vg_lite_img_trasnfrom_to_matrix(&matrix, dsc, &coords_rel);
    uint32_t rect[4] = { 0, 0, map_w, map_h };
    lv_color32_t color;
    vg_lite_blend_t blend = vg_lite_lv_blend_mode_to_vg_blend_mode(dsc->blend_mode);

    LV_GPU_LOG_TRACE("blend: 0x%x (%s)", blend, vg_lite_get_blend_string(blend));

    if(opa >= LV_OPA_MAX) {
        color.full = 0x0;
        src_vg_buf.image_mode = VG_LITE_NORMAL_IMAGE_MODE;
    }
    else {
        color.full = opa;
        color.full |= color.full << 8;
        color.full |= color.full << 16;
        src_vg_buf.image_mode = VG_LITE_MULTIPLY_IMAGE_MODE;
    }

    LV_GPU_LOG_TRACE("src_vg_buf: color = 0x%08x, image_mode = 0x%x (%s)",
                     (int)color.full,
                     src_vg_buf.image_mode,
                     vg_lite_get_image_mode_string(src_vg_buf.image_mode));

    if(indexed || alpha) {
        LV_GPU_LOG_TRACE("index: %d, alpha: %d", indexed, alpha);

        uint16_t palette_size = vg_lite_get_palette_size(src_vg_buf.format);
        if(dsc->recolor_opa != LV_OPA_TRANSP && recolor.full != pre_recolor.full) {
            if(pre_recolor.ch.alpha != LV_OPA_TRANSP) {
                LV_GPU_LOG_ERROR("Recolor diff! get:%lx expected:%lx",
                                 recolor.full, pre_recolor.full);
            }

            uint32_t * palette_r = lv_mem_buf_get(palette_size * sizeof(lv_color32_t));
            LV_ASSERT_MALLOC(palette_r);
            if(palette_r == NULL) {
                goto error_handler;
            }

            LV_GPU_LOG_WARN("%lx recolor now", recolor.full);
            lv_gpu_conv_recolor_palette_dsc_t conv_dsc;
            lv_memset_00(&conv_dsc, sizeof(conv_dsc));
            conv_dsc.dst = (lv_color32_t *)palette_r;
            conv_dsc.src = alpha ? NULL : (lv_color32_t *)palette;
            conv_dsc.size = palette_size;
            conv_dsc.recolor = recolor.full;
            lv_gpu_conv_start(LV_GPU_CONV_TYPE_RECOLOR_PALETTE, &conv_dsc);
            vg_lite_set_CLUT(palette_size, palette_r);
            lv_mem_buf_release(palette_r);
        }
        else {
            vg_lite_set_CLUT(palette_size, palette);
        }
    }

    vg_lite_filter_t filter = transformed ? VG_LITE_FILTER_BI_LINEAR
                              : VG_LITE_FILTER_POINT;
    lv_area_move(&draw_area, -disp_area->x1, -disp_area->y1);
    lv_area_move(&map_tf, -disp_area->x1, -disp_area->y1);

    int16_t rect_path[] = {
        VLC_OP_MOVE, 0, 0,
        VLC_OP_LINE, 0, 0,
        VLC_OP_LINE, 0, 0,
        VLC_OP_LINE, 0, 0,
        VLC_OP_END
    };

    fill_rect_path(rect_path, &draw_area);
    vg_lite_path_t vpath;
    lv_memset_00(&vpath, sizeof(vg_lite_path_t));

    VG_LITE_CHECK_ERROR(
        vg_lite_init_path(
            &vpath,
            VG_LITE_S16,
            VG_LITE_HIGH,
            sizeof(rect_path),
            rect_path,
            draw_area.x1,
            draw_area.y1,
            draw_area.x2 + 1,
            draw_area.y2 + 1));

    masked = vg_lite_draw_mask_to_path(&vpath, &draw_area);

    if(!vpath.path) {
        LV_GPU_LOG_WARN("draw img unsupported mask found");
        return;
    }

    if(masked) {
        LV_GPU_LOG_TRACE("masked, have to use draw_pattern");
        /* masked, have to use draw_pattern */
        vpath.format = VG_LITE_FP32;
        goto draw_pattern;
    }
    else if(_lv_area_is_in(&map_tf, &draw_area, 0)) {
        LV_GPU_LOG_TRACE("no clipping, simply blit");
        /* No clipping, simply blit */
        VG_LITE_CHECK_ERROR(
            vg_lite_blit_rect(
                &dest_vg_buf,
                &src_vg_buf,
                rect,
                &matrix,
                blend,
                color.full,
                filter));
    }
    else if(!transformed && map_tf.x1 == draw_area.x1
            && map_tf.y1 == draw_area.y1) {
        LV_GPU_LOG_TRACE("clipped from left top");
        /* Clipped from left top, use good old blit_rect */
        rect[2] = lv_area_get_width(&draw_area);
        rect[3] = lv_area_get_height(&draw_area);
        VG_LITE_CHECK_ERROR(
            vg_lite_blit_rect(
                &dest_vg_buf,
                &src_vg_buf,
                rect,
                &matrix,
                blend,
                color.full,
                filter));
    }
    else {
draw_pattern:
        LV_GPU_LOG_TRACE("draw_pattern");
        vg_lite_matrix_t src_matrix;
        vg_lite_identity(&src_matrix);

        /* arbitrarily clipped, have to use draw_pattern */
        vg_lite_set_multiply_color(color.full);
        VG_LITE_CHECK_ERROR(vg_lite_draw_pattern(
                                &dest_vg_buf,
                                &vpath,
                                VG_LITE_FILL_EVEN_ODD,
                                &src_matrix,
                                &src_vg_buf,
                                &matrix,
                                blend,
                                VG_LITE_PATTERN_COLOR,
                                0,
                                filter));
    }

    VG_LITE_DUMP_BUFFER_INFO(&dest_vg_buf);
    VG_LITE_DUMP_BUFFER_INFO(&src_vg_buf);

    VG_LITE_CHECK_ERROR(vg_lite_flush());

    lv_area_move(&draw_area, disp_area->x1, disp_area->y1);

    if(masked) {
        vg_lite_draw_mask_clean_path(vpath.path);
    }

    gpu->draw_ok = true;

error_handler:
    if(allocated_src) {
        LV_GPU_LOG_INFO("freeing allocated vg_buf:(%d, %d)@%p",
                        (int)src_vg_buf.width,
                        (int)src_vg_buf.height,
                        src_vg_buf.memory);
        lv_gpu_free(src_vg_buf.memory);
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void fill_rect_path(int16_t * path, const lv_area_t * area)
{
    path[1] = path[4] = area->x1;
    path[7] = path[10] = area->x2 + 1;
    path[2] = path[11] = area->y1;
    path[5] = path[8] = area->y2 + 1;
}

#endif /* CONFIG_LV_GPU_USE_VG_LITE */
