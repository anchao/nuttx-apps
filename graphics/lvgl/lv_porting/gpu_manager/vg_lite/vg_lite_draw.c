/**
 * @file vg_lite_draw.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "vg_lite_internal.h"

#ifdef CONFIG_LV_GPU_USE_VG_LITE

#include "../lv_gpu_manager.h"
#include "vg_lite_decoder.h"
#include "vg_lite_draw.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void vg_lite_draw_ctx_init(lv_disp_drv_t * disp_drv, lv_draw_ctx_t * draw_ctx);
static void vg_lite_draw_ctx_deinit(lv_disp_drv_t * disp_drv, lv_draw_ctx_t * draw_ctx);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

struct _lv_gpu_ctx_t * vg_lite_draw_ctx_create(void)
{
    lv_gpu_ctx_t * ctx = lv_mem_alloc(sizeof(lv_gpu_ctx_t));
    LV_ASSERT_MALLOC(ctx);
    lv_memset_00(ctx, sizeof(lv_gpu_ctx_t));
    ctx->name = "VG_Lite";
    ctx->draw_ctx_init = vg_lite_draw_ctx_init;
    ctx->draw_ctx_deinit = vg_lite_draw_ctx_deinit;
    ctx->draw_ctx_size = sizeof(vg_lite_draw_ctx_t);
    return ctx;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void vg_lite_draw_ctx_init(lv_disp_drv_t * disp_drv, lv_draw_ctx_t * draw_ctx)
{
#ifdef CONFIG_LV_GPU_VG_LITE_CUSTOM_INIT
    /* from 'vendor/bes/framework/services/platform/drivers/graphic/gpu/gpu_port.c' */
    extern void gpu_init(void);
    gpu_init();
    LV_GPU_LOG_TRACE("init GPU func = %p", gpu_init);
#endif

    vg_lite_draw_ctx_t * ctx = (vg_lite_draw_ctx_t *)draw_ctx;

#ifdef CONFIG_LV_GPU_VG_LITE_ARC
    ctx->base.draw_arc = vg_lite_draw_arc;
#endif

#ifdef CONFIG_LV_GPU_VG_LITE_RECT
    ctx->base.draw_rect = vg_lite_draw_rect;
#endif

#ifdef CONFIG_LV_GPU_VG_LITE_BG
    ctx->base.draw_bg = vg_lite_draw_bg;
#endif

#ifdef CONFIG_LV_GPU_VG_LITE_IMG_DECODED
    ctx->base.draw_img_decoded = vg_lite_draw_img_decoded;
#endif

#ifdef CONFIG_LV_GPU_VG_LITE_LINE
    ctx->base.draw_line = vg_lite_draw_line;
#endif

#ifdef CONFIG_LV_GPU_VG_LITE_POLYGON
    ctx->base.draw_polygon = vg_lite_draw_polygon;
#endif

    ctx->base.wait_for_finish = vg_lite_wait_for_finish;

    ctx->base.layer_init = vg_lite_draw_layer_init;
    ctx->base.layer_adjust = vg_lite_draw_layer_adjust;
    ctx->base.layer_blend = vg_lite_draw_layer_blend;
    ctx->base.layer_destroy = vg_lite_draw_layer_destroy;
    ctx->base.layer_instance_size = sizeof(vg_lite_draw_layer_ctx_t);

    vg_lite_decoder_init(ctx);
    vg_lite_dump_info();
}

static void vg_lite_draw_ctx_deinit(lv_disp_drv_t * disp_drv, lv_draw_ctx_t * draw_ctx)
{
    vg_lite_draw_ctx_t * ctx = (vg_lite_draw_ctx_t *)draw_ctx;
    vg_lite_decoder_uninit(ctx);
}

#endif /* CONFIG_LV_GPU_USE_VG_LITE */
