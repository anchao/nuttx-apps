/**
 * @file mve_draw.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "mve_draw.h"

#ifdef CONFIG_LV_GPU_USE_ARM_MVE

#include "../lv_gpu_manager.h"
#include "../lv_gpu_types.h"
#include "mve_internal.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void mve_draw_ctx_init(lv_disp_drv_t * disp_drv, lv_draw_ctx_t * draw_ctx);
static void mve_draw_ctx_deinit(lv_disp_drv_t * disp_drv, lv_draw_ctx_t * draw_ctx);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

struct _lv_gpu_ctx_t * mve_draw_ctx_create(void)
{
    lv_gpu_ctx_t * ctx = lv_mem_alloc(sizeof(lv_gpu_ctx_t));
    LV_ASSERT_MALLOC(ctx);
    lv_memset_00(ctx, sizeof(lv_gpu_ctx_t));
    ctx->name = "MVE";
    ctx->draw_ctx_init = mve_draw_ctx_init;
    ctx->draw_ctx_deinit = mve_draw_ctx_deinit;
    ctx->draw_ctx_size = sizeof(mve_draw_ctx_t);
    return ctx;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void mve_draw_ctx_init(lv_disp_drv_t * disp_drv, lv_draw_ctx_t * draw_ctx)
{
    mve_draw_ctx_t * ctx = (mve_draw_ctx_t *)draw_ctx;

    lv_draw_sw_init_ctx(disp_drv, draw_ctx);
    ctx->blend = mve_draw_blend;
}

static void mve_draw_ctx_deinit(lv_disp_drv_t * disp_drv, lv_draw_ctx_t * draw_ctx)
{
}

#endif /* CONFIG_LV_GPU_USE_ARM_MVE */
