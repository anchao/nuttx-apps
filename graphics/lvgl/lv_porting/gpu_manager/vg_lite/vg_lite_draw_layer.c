/**
 * @file vg_lite_draw_layer.c
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

lv_draw_layer_ctx_t * vg_lite_draw_layer_init(
    lv_draw_ctx_t * draw_ctx,
    lv_draw_layer_ctx_t * layer_ctx,
    lv_draw_layer_flags_t flags)
{
    lv_gpu_ctx_t * gpu = draw_ctx->user_data;
    vg_lite_draw_layer_ctx_t * vg_layer_ctx = (vg_lite_draw_layer_ctx_t *)layer_ctx;
    uint32_t px_size = (flags & LV_DRAW_LAYER_FLAG_HAS_ALPHA) ? LV_IMG_PX_SIZE_ALPHA_BYTE : sizeof(lv_color_t);
    lv_coord_t w = lv_area_get_width(&vg_layer_ctx->base_draw.area_full);

    if(!VG_LITE_IS_ALIGNED(w, VG_LITE_IMG_SRC_PX_ALIGN)) {
        flags |= LV_DRAW_LAYER_FLAG_HAS_ALPHA;
        w = VG_LITE_ALIGN(w, VG_LITE_IMG_SRC_PX_ALIGN);
        LV_GPU_LOG_TRACE("align width = %d", (int)w);
    }

    vg_layer_ctx->area_aligned = vg_layer_ctx->base_draw.area_full;
    vg_layer_ctx->area_aligned.x2 = vg_layer_ctx->area_aligned.x1 + w - 1;

    if(flags & LV_DRAW_LAYER_FLAG_CAN_SUBDIVIDE) {
        LV_GPU_LOG_TRACE("SUBDIVIDE layer");
        vg_layer_ctx->buf_size_bytes = LV_LAYER_SIMPLE_BUF_SIZE;

        uint32_t full_size = lv_area_get_size(&vg_layer_ctx->area_aligned) * px_size;

        if(vg_layer_ctx->buf_size_bytes > full_size) {
            vg_layer_ctx->buf_size_bytes = full_size;
        }

        vg_layer_ctx->base_draw.buf = lv_gpu_aligned_alloc(VG_LITE_IMG_SRC_ADDR_ALIGN, vg_layer_ctx->buf_size_bytes);

        if(vg_layer_ctx->base_draw.buf == NULL) {
            LV_GPU_LOG_WARN("Cannot allocate %" LV_PRIu32 " bytes for layer buffer."
                            " Allocating %" LV_PRIu32 " bytes instead. (Reduced performance)",
                            (uint32_t)vg_layer_ctx->buf_size_bytes, (uint32_t)LV_LAYER_SIMPLE_FALLBACK_BUF_SIZE * px_size);

            vg_layer_ctx->buf_size_bytes = LV_LAYER_SIMPLE_FALLBACK_BUF_SIZE;
            vg_layer_ctx->base_draw.buf = lv_gpu_aligned_alloc(VG_LITE_IMG_SRC_ADDR_ALIGN, vg_layer_ctx->buf_size_bytes);

            if(vg_layer_ctx->base_draw.buf == NULL) {
                LV_GPU_LOG_WARN("Allocate failed");
                return NULL;
            }
        }

        vg_layer_ctx->base_draw.area_act = vg_layer_ctx->base_draw.area_full;
        vg_layer_ctx->base_draw.area_act.y2 = vg_layer_ctx->base_draw.area_full.y1;
        vg_layer_ctx->base_draw.max_row_with_alpha = vg_layer_ctx->buf_size_bytes / w / LV_IMG_PX_SIZE_ALPHA_BYTE;
        vg_layer_ctx->base_draw.max_row_with_no_alpha = vg_layer_ctx->buf_size_bytes / w / sizeof(lv_color_t);
    }
    else {
        LV_GPU_LOG_TRACE("FULL layer");
        vg_layer_ctx->base_draw.area_act = vg_layer_ctx->base_draw.area_full;
        vg_layer_ctx->buf_size_bytes = lv_area_get_size(&vg_layer_ctx->area_aligned) * px_size;
        vg_layer_ctx->base_draw.buf = lv_gpu_aligned_alloc(VG_LITE_IMG_SRC_ADDR_ALIGN, vg_layer_ctx->buf_size_bytes);
        vg_layer_ctx->has_alpha = flags & LV_DRAW_LAYER_FLAG_HAS_ALPHA ? 1 : 0;

        if(vg_layer_ctx->base_draw.buf == NULL) {
            LV_GPU_LOG_WARN("Allocate failed");
            return NULL;
        }

        lv_memset_00(vg_layer_ctx->base_draw.buf, vg_layer_ctx->buf_size_bytes);

        draw_ctx->buf = vg_layer_ctx->base_draw.buf;
        draw_ctx->buf_area = &vg_layer_ctx->area_aligned;
        draw_ctx->clip_area = &vg_layer_ctx->base_draw.area_act;

        lv_disp_t * disp_refr = _lv_refr_get_disp_refreshing();
        disp_refr->driver->screen_transp = flags & LV_DRAW_LAYER_FLAG_HAS_ALPHA ? 1 : 0;
    }

    gpu->draw_ok = true;

    return layer_ctx;
}

void vg_lite_draw_layer_adjust(
    lv_draw_ctx_t * draw_ctx,
    lv_draw_layer_ctx_t * layer_ctx,
    lv_draw_layer_flags_t flags)
{
    lv_gpu_ctx_t * gpu = draw_ctx->user_data;
    vg_lite_draw_layer_ctx_t * vg_layer_ctx = (vg_lite_draw_layer_ctx_t *)layer_ctx;
    lv_disp_t * disp_refr = _lv_refr_get_disp_refreshing();
    if(flags & LV_DRAW_LAYER_FLAG_HAS_ALPHA) {
        LV_GPU_LOG_TRACE("has alpha");
        lv_memset_00(layer_ctx->buf, vg_layer_ctx->buf_size_bytes);
        vg_layer_ctx->has_alpha = 1;
        disp_refr->driver->screen_transp = 1;
    }
    else {
        LV_GPU_LOG_TRACE("no alpha");
        vg_layer_ctx->has_alpha = 0;
        disp_refr->driver->screen_transp = 0;
    }

    draw_ctx->buf = layer_ctx->buf;
    draw_ctx->buf_area = &vg_layer_ctx->area_aligned;
    draw_ctx->clip_area = &layer_ctx->area_act;
    draw_ctx->buf_area->y1 = draw_ctx->clip_area->y1;
    draw_ctx->buf_area->y2 = draw_ctx->clip_area->y2;

    gpu->draw_ok = true;
}

void vg_lite_draw_layer_blend(
    lv_draw_ctx_t * draw_ctx,
    lv_draw_layer_ctx_t * layer_ctx,
    const lv_draw_img_dsc_t * draw_dsc)
{
    lv_gpu_ctx_t * gpu = draw_ctx->user_data;
    vg_lite_draw_layer_ctx_t * vg_layer_ctx = (vg_lite_draw_layer_ctx_t *)layer_ctx;

    lv_img_dsc_t img;
    img.data = draw_ctx->buf;
    img.header.always_zero = 0;
    img.header.w = lv_area_get_width(draw_ctx->buf_area);
    img.header.h = lv_area_get_height(draw_ctx->buf_area);
    img.header.cf = vg_layer_ctx->has_alpha ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR;

    /*Restore the original draw_ctx*/
    draw_ctx->buf = layer_ctx->original.buf;
    draw_ctx->buf_area = layer_ctx->original.buf_area;
    draw_ctx->clip_area = layer_ctx->original.clip_area;
    lv_disp_t * disp_refr = _lv_refr_get_disp_refreshing();
    disp_refr->driver->screen_transp = layer_ctx->original.screen_transp;

    /*Blend the layer*/
    // lv_draw_img(gpu->main_draw_ctx, draw_dsc, &vg_layer_ctx->area_aligned, &img);
    // lv_draw_wait_for_finish(gpu->main_draw_ctx);
    lv_draw_img(draw_ctx, draw_dsc, &vg_layer_ctx->area_aligned, &img);
    lv_draw_wait_for_finish(draw_ctx);
    lv_img_cache_invalidate_src(&img);

    LV_GPU_LOG_TRACE("finish");
    gpu->draw_ok = true;
}

void vg_lite_draw_layer_destroy(lv_draw_ctx_t * draw_ctx, lv_draw_layer_ctx_t * layer_ctx)
{
    lv_gpu_ctx_t * gpu = draw_ctx->user_data;
    LV_UNUSED(draw_ctx);
    lv_gpu_free(layer_ctx->buf);
    LV_GPU_LOG_TRACE("finish");
    gpu->draw_ok = true;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /* CONFIG_LV_GPU_USE_VG_LITE */
