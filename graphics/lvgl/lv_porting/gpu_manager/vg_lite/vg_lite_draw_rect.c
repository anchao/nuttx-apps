/**
 * @file vg_lite_draw_rect.c
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

static bool draw_rect_base(vg_lite_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords);
static bool draw_bg_base(vg_lite_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void vg_lite_draw_rect(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords)
{
    lv_gpu_ctx_t * gpu = draw_ctx->user_data;
    gpu->draw_ok = draw_rect_base((vg_lite_draw_ctx_t *)draw_ctx, dsc, coords);
}

void vg_lite_draw_bg(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords)
{
    lv_gpu_ctx_t * gpu = draw_ctx->user_data;
    gpu->draw_ok = draw_bg_base((vg_lite_draw_ctx_t *)draw_ctx, dsc, coords);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static bool draw_shadow(vg_lite_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords)
{
    /* Do not draw when transparent */
    if(dsc->shadow_opa <= LV_OPA_MIN) {
        LV_GPU_LOG_TRACE("shadow_opa skip");
        return true;
    }

    /* Do not draw when width is 0 */
    if(dsc->shadow_width == 0) {
        LV_GPU_LOG_TRACE("shadow_width skip");
        return true;
    }

    if(dsc->shadow_width == 1 && dsc->shadow_spread <= 0 && dsc->shadow_ofs_x == 0 && dsc->shadow_ofs_y == 0) {
        LV_GPU_LOG_TRACE("shadow_spread, shadow_ofs skip");
        return true;
    }

    LV_GPU_LOG_INFO("draw shadow unsupported");
    return false;
}

static bool draw_bg(vg_lite_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords)
{
    LV_GPU_LOG_TRACE("start");

    /* Do not draw when transparent */
    if(dsc->bg_opa <= LV_OPA_MIN) {
        LV_GPU_LOG_TRACE("bg_opa skip");
        return true;
    }

    /* Check clip area */
    lv_area_t draw_area;
    if(!_lv_area_intersect(&draw_area, coords, draw_ctx->base.clip_area)) {
        LV_GPU_LOG_TRACE("draw_area skip");
        return true;
    }

    /* Prepare rect path data */
    uint16_t len = vg_lite_fill_path(draw_ctx->rect_path, VG_LITE_RECT_PATH, (const lv_point_t *)coords, dsc);
    LV_GPU_LOG_TRACE("path len = %d", len);

    /* Prepare curve fill dsc */
    vg_lite_curve_fill_dsc_t fill_dsc;
    vg_lite_curve_fill_dsc_init(&fill_dsc);
    fill_dsc.color = dsc->bg_color,
    fill_dsc.opa = dsc->bg_opa,
    fill_dsc.type = CURVE_FILL_COLOR;

    if(dsc->bg_grad.dir != LV_GRAD_DIR_NONE) {
        LV_GPU_LOG_TRACE("dsc->bg_grad.dir = %d", dsc->bg_grad.dir);
        fill_dsc.type = CURVE_FILL_LINEAR_GRADIENT;
        fill_dsc.grad_dsc = &dsc->bg_grad;
        fill_dsc.grad_area = coords;
    }

    /* Prepare draw buffer */
    lv_gpu_dest_buf_t gpu_buf = {
        .buf = draw_ctx->base.buf,
        .buf_area = draw_ctx->base.buf_area,
        .clip_area = &draw_area,
        .cf = LV_IMG_CF_TRUE_COLOR_ALPHA,
    };
    LV_GPU_DUMP_DEST_BUFFER_INFO(&gpu_buf);

    /* Prepare vg lite path */
    vg_lite_path_t vg_path = {
        .path = draw_ctx->rect_path,
        .path_length = len * sizeof(float)
    };

    /* generate mask path */
    bool masked = vg_lite_draw_mask_to_path(&vg_path, coords);

    if(!vg_path.path) {
        LV_GPU_LOG_INFO("vg_path.path is NULL");
        return LV_RES_INV;
    }

    /* start draw */
    bool retval = vg_lite_draw_path(
                      &gpu_buf,
                      vg_path.path,
                      vg_path.path_length,
                      &fill_dsc);

    /* clean mask */
    if(masked) {
        vg_lite_draw_mask_clean_path(&vg_path);
    }

    LV_GPU_LOG_TRACE("finish");

    return retval;
}

static void draw_bg_img(vg_lite_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords)
{
    lv_gpu_ctx_t * gpu = draw_ctx->base.user_data;

    /* Do not draw when transparent */
    if(dsc->bg_img_opa <= LV_OPA_MIN) {
        LV_GPU_LOG_TRACE("bg_img_opa skip");
        return;
    }

    /* Do not draw when src is NULL */
    if(dsc->bg_img_src == NULL) {
        LV_GPU_LOG_TRACE("bg_img_src skip");
        return;
    }

    /* check clip area */
    lv_area_t draw_area;
    if(!_lv_area_intersect(&draw_area, coords, draw_ctx->base.clip_area)) {
        LV_GPU_LOG_TRACE("draw_area skip");
        return;
    }

    /* check img src type */
    lv_img_src_t src_type = lv_img_src_get_type(dsc->bg_img_src);
    if(src_type == LV_IMG_SRC_SYMBOL) {
        LV_GPU_LOG_TRACE("src_type == LV_IMG_SRC_SYMBOL");
        lv_point_t size;
        lv_txt_get_size(&size, dsc->bg_img_src, dsc->bg_img_symbol_font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_area_t a;
        a.x1 = coords->x1 + lv_area_get_width(coords) / 2 - size.x / 2;
        a.x2 = a.x1 + size.x - 1;
        a.y1 = coords->y1 + lv_area_get_height(coords) / 2 - size.y / 2;
        a.y2 = a.y1 + size.y - 1;

        lv_draw_label_dsc_t label_draw_dsc;
        lv_draw_label_dsc_init(&label_draw_dsc);
        label_draw_dsc.font = dsc->bg_img_symbol_font;
        label_draw_dsc.color = dsc->bg_img_recolor;
        label_draw_dsc.opa = dsc->bg_img_opa;
        lv_draw_label(gpu->main_draw_ctx, &label_draw_dsc, &a, dsc->bg_img_src, NULL);
        return;
    }

    /* get img header info */
    lv_img_header_t header;
    lv_res_t res = lv_img_decoder_get_info(dsc->bg_img_src, &header);
    if(res != LV_RES_OK) {
        LV_GPU_LOG_WARN("Couldn't read the background image");
        return;
    }

    /* prepare img dsc */
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);
    img_dsc.blend_mode = dsc->blend_mode;
    img_dsc.recolor = dsc->bg_img_recolor;
    img_dsc.recolor_opa = dsc->bg_img_recolor_opa;
    img_dsc.opa = dsc->bg_img_opa;

    /* tile draw */
    if(dsc->bg_img_tiled) {
        LV_GPU_LOG_TRACE("bg_img_tiled");
        lv_area_t area;
        area.y1 = coords->y1;
        area.y2 = area.y1 + header.h - 1;

        for(; area.y1 <= coords->y2; area.y1 += header.h, area.y2 += header.h) {
            area.x1 = coords->x1;
            area.x2 = area.x1 + header.w - 1;
            for(; area.x1 <= coords->x2; area.x1 += header.w, area.x2 += header.w) {
                lv_draw_img(gpu->main_draw_ctx, &img_dsc, &area, dsc->bg_img_src);
            }
        }
        return;
    }

    /* single draw */
    LV_GPU_LOG_TRACE("bg img single");
    lv_area_t area;
    area.x1 = coords->x1 + lv_area_get_width(coords) / 2 - header.w / 2;
    area.y1 = coords->y1 + lv_area_get_height(coords) / 2 - header.h / 2;
    area.x2 = area.x1 + header.w - 1;
    area.y2 = area.y1 + header.h - 1;
    lv_draw_img(gpu->main_draw_ctx, &img_dsc, &area, dsc->bg_img_src);
}

static void draw_border(vg_lite_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords)
{
    /* Do not draw when transparent */
    if(dsc->border_opa <= LV_OPA_MIN) {
        LV_GPU_LOG_TRACE("border_opa skip");
        return;
    }

    /* Do not draw when width is 0 */
    if(dsc->border_width == 0) {
        LV_GPU_LOG_TRACE("border_width skip");
        return;
    }

    /* Do not draw when side is none */
    if(dsc->border_side == LV_BORDER_SIDE_NONE) {
        LV_GPU_LOG_TRACE("border_side skip");
        return;
    }

    /* Do not draw when later draw */
    if(dsc->border_post) {
        LV_GPU_LOG_TRACE("border_post skip");
        return;
    }

    /* check clip area */
    lv_area_t draw_area;
    if(!_lv_area_intersect(&draw_area, coords, draw_ctx->base.clip_area)) {
        LV_GPU_LOG_TRACE("draw_area skip");
        return;
    }

    lv_coord_t coords_w = lv_area_get_width(coords);
    lv_coord_t coords_h = lv_area_get_height(coords);
    lv_coord_t r_short = LV_MIN(coords_w, coords_h) >> 1;
    lv_coord_t rout = LV_MIN(dsc->radius, r_short);
    lv_coord_t rin = LV_MAX(0, rout - dsc->border_width);

    lv_area_t area_inner = {
        .x1 = coords->x1 + dsc->border_width,
        .y1 = coords->y1 + dsc->border_width,
        .x2 = coords->x2 - dsc->border_width,
        .y2 = coords->y2 - dsc->border_width
    };

    /* Prepare rect path */
    uint32_t len = vg_lite_fill_path(draw_ctx->rect_path, VG_LITE_RECT_PATH, (const lv_point_t *)coords, dsc);

    /* Prepare border path */
    lv_draw_rect_dsc_t inner_dsc = { .radius = rin };
    len += vg_lite_fill_path(draw_ctx->rect_path + len, VG_LITE_RECT_PATH, (const lv_point_t *)&area_inner, &inner_dsc);

    LV_GPU_LOG_TRACE("path len = %d", (int)len);

    vg_lite_curve_fill_dsc_t fill_dsc;
    vg_lite_curve_fill_dsc_init(&fill_dsc);
    fill_dsc.color = dsc->border_color;
    fill_dsc.opa = dsc->border_opa;
    fill_dsc.type = CURVE_FILL_COLOR;
    fill_dsc.fill_rule = VG_LITE_FILL_EVEN_ODD;

    lv_gpu_dest_buf_t gpu_buf = {
        .buf = draw_ctx->base.buf,
        .buf_area = draw_ctx->base.buf_area,
        .clip_area = &draw_area,
        .cf = LV_IMG_CF_TRUE_COLOR_ALPHA
    };
    LV_GPU_DUMP_DEST_BUFFER_INFO(&gpu_buf);

#ifdef CONFIG_LV_GPU_VG_LITE_CHECK_DRAW_MASK
    if(lv_draw_mask_is_any(coords)) {
        LV_GPU_LOG_WARN("Mask detected, output may be incorrect");
    }
#endif

    vg_lite_draw_path(&gpu_buf, draw_ctx->rect_path, len * sizeof(float), &fill_dsc);
}

static void draw_outline(vg_lite_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords)
{
    /* Do not draw when transparent */
    if(dsc->outline_opa <= LV_OPA_MIN) {
        LV_GPU_LOG_TRACE("outline_opa skip");
        return;
    }

    /* Do not draw when width is 0 */
    if(dsc->outline_width == 0) {
        LV_GPU_LOG_TRACE("outline_width skip");
        return;
    }

    /* Bring the outline closer to make sure there is no color bleeding with pad=0 */
    lv_coord_t pad = dsc->outline_pad - 1;

    lv_area_t area_inner = {
        .x1 = coords->x1 - pad,
        .y1 = coords->y1 - pad,
        .x2 = coords->x2 + pad,
        .y2 = coords->y2 + pad
    };

    lv_area_t area_outer = {
        .x1 = area_inner.x1 - dsc->outline_width,
        .y1 = area_inner.y1 - dsc->outline_width,
        .x2 = area_inner.x2 + dsc->outline_width,
        .y2 = area_inner.y2 + dsc->outline_width
    };

    lv_area_t draw_area;
    if(!_lv_area_intersect(&draw_area, &area_outer, draw_ctx->base.clip_area)) {
        LV_GPU_LOG_TRACE("draw_area skip");
        return;
    }

    int32_t inner_w = lv_area_get_width(&area_inner);
    int32_t inner_h = lv_area_get_height(&area_inner);
    lv_coord_t r_short = LV_MIN(inner_w, inner_h) >> 1;
    int32_t rin = LV_MIN(dsc->radius + pad, r_short);
    lv_draw_rect_dsc_t inner_dsc = { .radius = rin };

    uint32_t len = vg_lite_fill_path(draw_ctx->rect_path, VG_LITE_RECT_PATH, (const lv_point_t *)&area_inner, &inner_dsc);

    lv_coord_t rout = rin + dsc->outline_width;
    lv_draw_rect_dsc_t outer_dsc = { .radius = rout };
    len += vg_lite_fill_path(draw_ctx->rect_path + len, VG_LITE_RECT_PATH, (const lv_point_t *)&area_outer, &outer_dsc);

    LV_GPU_LOG_TRACE("path len = %d", (int)len);

    vg_lite_curve_fill_dsc_t fill_dsc;
    vg_lite_curve_fill_dsc_init(&fill_dsc);
    fill_dsc.color = dsc->outline_color;
    fill_dsc.opa = dsc->outline_opa;
    fill_dsc.type = CURVE_FILL_COLOR;
    fill_dsc.fill_rule = VG_LITE_FILL_EVEN_ODD;

    lv_gpu_dest_buf_t gpu_buf = {
        .buf = draw_ctx->base.buf,
        .buf_area = draw_ctx->base.buf_area,
        .clip_area = &draw_area,
        .cf = LV_IMG_CF_TRUE_COLOR_ALPHA
    };
    LV_GPU_DUMP_DEST_BUFFER_INFO(&gpu_buf);

#ifdef CONFIG_LV_GPU_VG_LITE_CHECK_DRAW_MASK
    if(lv_draw_mask_is_any(coords)) {
        LV_GPU_LOG_WARN("Mask detected, output may be incorrect");
    }
#endif

    vg_lite_draw_path(&gpu_buf, draw_ctx->rect_path, len * sizeof(float), &fill_dsc);
}

static bool draw_rect_base(vg_lite_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords)
{
#if CONFIG_LV_GPU_VG_LITE_DRAW_AREA_MIN_SIZE > 0
    if(lv_area_get_size(coords) <= CONFIG_LV_GPU_VG_LITE_DRAW_AREA_MIN_SIZE) {
        LV_GPU_LOG_TRACE("area size(%d) <= min size(%d)", (int)lv_area_get_size(coords),
                         CONFIG_LV_GPU_VG_LITE_DRAW_AREA_MIN_SIZE);
        return false;
    }
#endif

    if(draw_shadow(draw_ctx, dsc, coords) != LV_RES_OK) {
        return false;
    }

    if(dsc->border_opa > LV_OPA_MIN
       && dsc->border_width > 0 && dsc->border_side != LV_BORDER_SIDE_NONE
       && dsc->border_side != LV_BORDER_SIDE_FULL && !dsc->border_post) {
        LV_GPU_LOG_TRACE("unsupport border_side = %d", dsc->border_side);
        return false;
    }

    if(draw_bg(draw_ctx, dsc, coords) != LV_RES_OK) {
        return false;
    }

    draw_bg_img(draw_ctx, dsc, coords);

    draw_border(draw_ctx, dsc, coords);

    draw_outline(draw_ctx, dsc, coords);

    return true;
}

static bool draw_bg_base(vg_lite_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords)
{
#if CONFIG_LV_GPU_VG_LITE_DRAW_AREA_MIN_SIZE > 0
    if(lv_area_get_size(coords) <= CONFIG_LV_GPU_VG_LITE_DRAW_AREA_MIN_SIZE) {
        LV_GPU_LOG_TRACE("area size(%d) <= min size(%d)", (int)lv_area_get_size(coords),
                         CONFIG_LV_GPU_VG_LITE_DRAW_AREA_MIN_SIZE);
        return false;
    }
#endif

#if LV_COLOR_SCREEN_TRANSP && LV_COLOR_DEPTH == 32
    lv_disp_t * disp_refr = _lv_refr_get_disp_refreshing();
    if(disp_refr->driver->screen_transp) {
        LV_GPU_LOG_TRACE("screen_transp = 1, clear draw buf");
        lv_memset_00(draw_ctx->base.buf, lv_area_get_size(draw_ctx->base.buf_area) * sizeof(lv_color_t));
    }
#endif

    if(draw_bg(draw_ctx, dsc, coords) != LV_RES_OK) {
        return false;
    }

    draw_bg_img(draw_ctx, dsc, coords);

    return true;
}

#endif /* CONFIG_LV_GPU_USE_VG_LITE */
