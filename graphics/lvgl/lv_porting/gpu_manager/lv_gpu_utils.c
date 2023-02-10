/**
 * @file lv_gpu_utils.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_gpu_utils.h"
#include <stdlib.h>
#include <time.h>

/*********************
 *      DEFINES
 *********************/

/* The color format field in lv_img_header_t only occupies 5 bits,
 * and the value range is 0~31, So borrow RESERVED space. */

#define LV_GPU_CUSTOM_IMG_CF_BEGIN LV_IMG_CF_RESERVED_15
#define LV_GPU_CUSTOM_IMG_CF_END LV_IMG_CF_USER_ENCODED_1

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

uint32_t lv_gpu_tick_get_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t tick = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    return tick;
}

uint32_t lv_gpu_tick_elaps_us(uint32_t prev_tick)
{
    uint32_t act_time = lv_gpu_tick_get_us();
    if(act_time >= prev_tick) {
        prev_tick = act_time - prev_tick;
    }
    else {
        prev_tick = UINT32_MAX - prev_tick + 1;
        prev_tick += act_time;
    }
    return prev_tick;
}

float lv_gpu_fast_inv_sqrt(float number)
{
    long i;
    float x2, y;
    const float threehalfs = 1.5F;

    x2 = number * 0.5F;
    y = number;
    i = *(long *)&y; /* evil floating point bit level hacking */
    i = 0x5f3759df - (i >> 1); /* what the fuck? */
    y = *(float *)&i;
    y = y * (threehalfs - (x2 * y * y)); /* 1st iteration */

    return y;
}

lv_img_cf_t lv_gpu_get_disp_refr_cf(void)
{
    lv_disp_t * disp_refr = _lv_refr_get_disp_refreshing();

    if(disp_refr && disp_refr->driver->screen_transp) {
        return LV_IMG_CF_TRUE_COLOR_ALPHA;
    }

    return LV_IMG_CF_TRUE_COLOR;
}

void lv_gpu_dump_draw_ctx_info(const lv_draw_ctx_t * draw_ctx)
{
    LV_GPU_LOG_INFO("buf: %p, buf_area: (%d, %d) %d x %d, clip_area: (%d, %d) %d x %d",
                    draw_ctx->buf,
                    (int)draw_ctx->buf_area->x1, (int)draw_ctx->buf_area->y1,
                    (int)lv_area_get_width(draw_ctx->buf_area), (int)lv_area_get_height(draw_ctx->buf_area),
                    (int)draw_ctx->clip_area->x1, (int)draw_ctx->clip_area->y1,
                    (int)lv_area_get_width(draw_ctx->clip_area), (int)lv_area_get_height(draw_ctx->clip_area));
}

void lv_gpu_dump_draw_rect_dsc_info(const lv_draw_rect_dsc_t * dsc)
{
    LV_GPU_LOG_INFO("radius = %d", (int)dsc->radius);
    LV_GPU_LOG_INFO("blend_mode = %d", dsc->blend_mode);

    /*Background*/
    LV_GPU_DUMP_OPA(dsc->bg_opa);
    LV_GPU_DUMP_COLOR(&dsc->bg_color); /**< First element of a gradient is a color, so it maps well here*/
    // lv_grad_dsc_t bg_grad;

    /*Background img*/
    LV_GPU_LOG_INFO("bg_img_src = %p", dsc->bg_img_src);
    LV_GPU_LOG_INFO("bg_img_symbol_font = %p", dsc->bg_img_symbol_font);
    LV_GPU_DUMP_COLOR(&dsc->bg_img_recolor);
    LV_GPU_DUMP_COLOR(&dsc->bg_img_recolor);
    LV_GPU_DUMP_OPA(dsc->bg_img_opa);
    LV_GPU_DUMP_OPA(dsc->bg_img_recolor_opa);
    LV_GPU_LOG_INFO("bg_img_tiled = %d", dsc->bg_img_tiled);

    /*Border*/
    LV_GPU_DUMP_COLOR(&dsc->border_color);
    LV_GPU_LOG_INFO("border_width = %d", dsc->border_width);
    LV_GPU_DUMP_OPA(dsc->border_opa);
    LV_GPU_LOG_INFO("border_post = %d", dsc->border_post); /*There is a border it will be drawn later.*/
    LV_GPU_LOG_INFO("border_side = %d", dsc->border_side);

    /*Outline*/
    LV_GPU_DUMP_COLOR(&dsc->outline_color);
    LV_GPU_LOG_INFO("outline_width = %d", dsc->outline_width);
    LV_GPU_LOG_INFO("outline_pad = %d", dsc->outline_pad);
    LV_GPU_DUMP_OPA(dsc->outline_opa);

    /*Shadow*/
    LV_GPU_DUMP_COLOR(&dsc->shadow_color);
    LV_GPU_LOG_INFO("shadow_width = %d", dsc->shadow_width);
    LV_GPU_LOG_INFO("shadow_ofs_x = %d", dsc->shadow_ofs_x);
    LV_GPU_LOG_INFO("shadow_ofs_y = %d", dsc->shadow_ofs_y);
    LV_GPU_LOG_INFO("shadow_spread = %d", dsc->shadow_spread);
    LV_GPU_DUMP_OPA(dsc->shadow_opa);
}

lv_img_cf_t lv_gpu_register_custom_img_cf(void)
{
    static int cur_cf = LV_GPU_CUSTOM_IMG_CF_BEGIN;
    lv_img_cf_t img_cf = LV_IMG_CF_UNKNOWN;

    if(cur_cf <= LV_GPU_CUSTOM_IMG_CF_END) {
        img_cf = (lv_img_cf_t)cur_cf;
        LV_GPU_LOG_INFO("Register custom img cf: %d", cur_cf);
        cur_cf++;
    }
    else {
        LV_GPU_LOG_ERROR("No more custom img cf");
    }

    return img_cf;
}

void * lv_gpu_malloc(size_t size)
{
    void * mem = malloc(size);
    LV_GPU_LOG_TRACE("mem = %p, size = %d", mem, (int)size);
    return mem;
}

void * lv_gpu_aligned_alloc(size_t align, size_t size)
{
    void * mem = aligned_alloc(align, size);
    LV_GPU_LOG_TRACE("mem = %p, align = %d, size = %d", mem, (int)align, (int)size);
    return mem;
}

void * lv_gpu_realloc(void * oldmem, size_t newsize)
{
    void * mem = realloc(oldmem, newsize);
    LV_GPU_LOG_TRACE("mem = %p, oldmem = %p, newsize = %d", mem, oldmem, (int)newsize);
    return mem;
}

void lv_gpu_free(void * mem)
{
    LV_GPU_LOG_TRACE("mem = %p", mem);
    free(mem);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
