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
    ctx->draw_ctx_init = lv_draw_sw_init_ctx;
    ctx->draw_ctx_deinit = lv_draw_sw_deinit_ctx;
    ctx->draw_ctx_size = sizeof(lv_draw_sw_ctx_t);
    return ctx;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
