/**
 * @file sw_draw.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "sw_draw.h"
#include "../lv_gpu_manager.h"
#include "../lv_gpu_types.h"
#include <lvgl/lvgl.h>
#include <lvgl/src/draw/sw/lv_draw_sw.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void sw_draw_ctx_init(lv_disp_drv_t * disp_drv, lv_draw_ctx_t * draw_ctx);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

struct _lv_gpu_ctx_t * sw_draw_ctx_create(void)
{
    lv_gpu_ctx_t * ctx = lv_mem_alloc(sizeof(lv_gpu_ctx_t));
    LV_ASSERT_MALLOC(ctx);
    lv_memset_00(ctx, sizeof(lv_gpu_ctx_t));
    ctx->name = "SW";
    ctx->draw_ctx_init = sw_draw_ctx_init;
    ctx->draw_ctx_deinit = lv_draw_sw_deinit_ctx;
    ctx->draw_ctx_size = sizeof(lv_draw_sw_ctx_t);
    return ctx;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void sw_draw_blend(lv_draw_ctx_t * draw_ctx, const lv_draw_sw_blend_dsc_t * dsc)
{
    lv_gpu_ctx_t * gpu = draw_ctx->user_data;

    /* Wait for gpu manager draw ctx to finish drawing */
    lv_draw_wait_for_finish(gpu->main_draw_ctx);

    lv_draw_sw_blend_basic(draw_ctx, dsc);
}

static void sw_draw_ctx_init(lv_disp_drv_t * disp_drv, lv_draw_ctx_t * draw_ctx)
{
    lv_draw_sw_ctx_t * draw_sw_ctx = (lv_draw_sw_ctx_t *) draw_ctx;
    lv_draw_sw_init_ctx(disp_drv, draw_ctx);
    draw_sw_ctx->blend = sw_draw_blend;
}
