/**
 * @file lv_gpu_manager.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_gpu_manager.h"
#include "lv_gpu_conv.h"
#include "lv_gpu_types.h"
#include "lv_gpu_utils.h"

#include "mve/mve_draw.h"
#include "sw/sw_draw.h"
#include "vg_lite/vg_lite_draw.h"

/*********************
 *      DEFINES
 *********************/

#define GPU_MANAGER_COPY_DRAW_CTX_PARAM(dest, src) \
    do {                                           \
        dest->buf = src->buf;                      \
        dest->buf_area = src->buf_area;            \
        dest->clip_area = src->clip_area;          \
    } while (0)

#define GPU_MANAGER_FALLBACK_BEGIN(func)                                  \
    do {                                                                  \
        LV_GPU_PERF_START();                                              \
        lv_gpu_manager_t* manager = (lv_gpu_manager_t*)draw_ctx;          \
        lv_gpu_ctx_t* gpu = manager->head;                                \
        while (gpu) {                                                     \
            if (gpu->draw_ctx->func) {                                    \
                gpu->draw_ok = false;                                     \
                GPU_MANAGER_COPY_DRAW_CTX_PARAM(gpu->draw_ctx, draw_ctx); \
                LV_GPU_LOG_TRACE("GPU[%s] start", gpu->name);

#define GPU_MANAGER_FALLBACK_END \
    }                            \
    gpu = gpu->fallback;         \
    }                            \
    LV_GPU_PERF_STOP();          \
    }                            \
    while (0)                    \
        ;

#define GPU_MANAGER_FALLBACK_DRAW(func, ...)         \
    GPU_MANAGER_FALLBACK_BEGIN(func)                 \
    gpu->draw_ctx->func(gpu->draw_ctx, __VA_ARGS__); \
    if (gpu->draw_ok) {                              \
        LV_GPU_LOG_TRACE("OK");                      \
        break;                                       \
    }                                                \
    GPU_MANAGER_FALLBACK_END

/**********************
 *      TYPEDEFS
 **********************/
typedef struct _lv_gpu_manager_t {
    lv_draw_ctx_t draw_ctx;
    lv_gpu_ctx_t * head;
} lv_gpu_manager_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void gpu_manager_draw_ctx_init(lv_disp_drv_t * disp_drv, lv_draw_ctx_t * draw_ctx);
static void gpu_manager_draw_ctx_deinit(lv_disp_drv_t * disp_drv, lv_draw_ctx_t * draw_ctx);
static void gpu_manager_draw_rect(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords);
static void gpu_manager_draw_arc(lv_draw_ctx_t * draw_ctx, const lv_draw_arc_dsc_t * dsc, const lv_point_t * center,
                                 uint16_t radius, uint16_t start_angle, uint16_t end_angle);
static void gpu_manager_draw_img_decoded(lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * dsc,
                                         const lv_area_t * coords, const uint8_t * map_p, lv_img_cf_t color_format);
static void gpu_manager_draw_letter(lv_draw_ctx_t * draw_ctx, const lv_draw_label_dsc_t * dsc, const lv_point_t * pos_p,
                                    uint32_t letter);
static void gpu_manager_draw_line(lv_draw_ctx_t * draw_ctx, const lv_draw_line_dsc_t * dsc, const lv_point_t * point1,
                                  const lv_point_t * point2);
static void gpu_manager_draw_polygon(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * draw_dsc,
                                     const lv_point_t * points, uint16_t point_cnt);
static void gpu_manager_draw_transform(lv_draw_ctx_t * draw_ctx, const lv_area_t * dest_area, const void * src_buf,
                                       lv_coord_t src_w, lv_coord_t src_h, lv_coord_t src_stride,
                                       const lv_draw_img_dsc_t * draw_dsc, lv_img_cf_t cf, lv_color_t * cbuf, lv_opa_t * abuf);
static void gpu_manager_draw_bg(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * draw_dsc,
                                const lv_area_t * coords);
static void gpu_manager_wait_for_finish(lv_draw_ctx_t * draw_ctx);
static void gpu_manager_buffer_copy(lv_draw_ctx_t * draw_ctx, void * dest_buf, lv_coord_t dest_stride,
                                    const lv_area_t * dest_area,
                                    void * src_buf, lv_coord_t src_stride, const lv_area_t * src_area);
static lv_draw_layer_ctx_t * gpu_manager_layer_init(lv_draw_ctx_t * draw_ctx, lv_draw_layer_ctx_t * layer_ctx,
                                                    lv_draw_layer_flags_t flags);
static void gpu_manager_layer_adjust(lv_draw_ctx_t * draw_ctx, lv_draw_layer_ctx_t * layer_ctx,
                                     lv_draw_layer_flags_t flags);
static void gpu_manager_layer_blend(lv_draw_ctx_t * draw_ctx, lv_draw_layer_ctx_t * layer_ctx,
                                    const lv_draw_img_dsc_t * draw_dsc);
static void gpu_manager_layer_destroy(lv_draw_ctx_t * draw_ctx, lv_draw_layer_ctx_t * layer_ctx);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_gpu_manager_init(lv_disp_drv_t * disp_drv)
{
    LV_ASSERT_NULL(disp_drv);
    disp_drv->draw_ctx_init = gpu_manager_draw_ctx_init;
    disp_drv->draw_ctx_deinit = gpu_manager_draw_ctx_deinit;
    disp_drv->draw_ctx_size = sizeof(lv_gpu_manager_t);
}

void lv_gpu_manager_add_gpu(struct _lv_gpu_manager_t * manager, struct _lv_gpu_ctx_t * gpu)
{
    LV_ASSERT_NULL(manager);
    LV_ASSERT_NULL(gpu);
    LV_ASSERT_NULL(gpu->name);

    gpu->main_draw_ctx = &manager->draw_ctx;

    if(manager->head == NULL) {
        manager->head = gpu;
    }
    else {
        lv_gpu_ctx_t * gpu_p = manager->head;
        while(gpu_p->fallback) {
            gpu_p = gpu_p->fallback;
        }
        gpu_p->fallback = gpu;
    }

    LV_GPU_LOG_INFO("GPU[%s] add OK", gpu->name);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void gpu_manager_fallback_draw_ctx_init(lv_gpu_manager_t * manager)
{
#ifdef CONFIG_LV_GPU_USE_VG_LITE
    lv_gpu_manager_add_gpu(manager, vg_lite_draw_ctx_create());
#endif

#ifdef CONFIG_LV_GPU_USE_ARM_MVE
    lv_gpu_manager_add_gpu(manager, mve_draw_ctx_create());
#endif

    lv_gpu_manager_add_gpu(manager, sw_draw_ctx_create());
}

static void gpu_manager_draw_ctx_init(lv_disp_drv_t * disp_drv, lv_draw_ctx_t * draw_ctx)
{
    lv_gpu_manager_t * manager = (lv_gpu_manager_t *)draw_ctx;

    lv_memset_00(draw_ctx, sizeof(lv_gpu_manager_t));

    draw_ctx->draw_arc = gpu_manager_draw_arc;
    draw_ctx->draw_rect = gpu_manager_draw_rect;
    draw_ctx->draw_bg = gpu_manager_draw_bg;
    draw_ctx->draw_letter = gpu_manager_draw_letter;
    draw_ctx->draw_img_decoded = gpu_manager_draw_img_decoded;
    draw_ctx->draw_line = gpu_manager_draw_line;
    draw_ctx->draw_polygon = gpu_manager_draw_polygon;
    draw_ctx->draw_transform = gpu_manager_draw_transform;
    draw_ctx->wait_for_finish = gpu_manager_wait_for_finish;
    draw_ctx->buffer_copy = gpu_manager_buffer_copy;
    draw_ctx->layer_init = gpu_manager_layer_init;
    draw_ctx->layer_adjust = gpu_manager_layer_adjust;
    draw_ctx->layer_blend = gpu_manager_layer_blend;
    draw_ctx->layer_destroy = gpu_manager_layer_destroy;

    /* Set MIN layer_instance_size */
    draw_ctx->layer_instance_size = sizeof(lv_draw_layer_ctx_t);

    gpu_manager_fallback_draw_ctx_init(manager);
    lv_gpu_conv_init();

    lv_gpu_ctx_t * gpu = manager->head;

    while(gpu) {
        gpu->draw_ctx = lv_mem_alloc(gpu->draw_ctx_size);
        LV_ASSERT_MALLOC(gpu->draw_ctx);
        lv_memset_00(gpu->draw_ctx, gpu->draw_ctx_size);

        LV_GPU_LOG_INFO("GPU[%s] init draw_ctx = %p, draw_ctx_size = %zu",
                        gpu->name, gpu->draw_ctx, gpu->draw_ctx_size);

        gpu->draw_ctx_init(disp_drv, gpu->draw_ctx);
        gpu->draw_ctx->user_data = gpu;

        if(gpu->draw_ctx->layer_init && draw_ctx->layer_instance_size == 0) {
            LV_LOG_ERROR("GPU[%s] has layer, but layer_instance_size is 0", gpu->name);
        }

        /* Record MAX layer_instance_size */
        draw_ctx->layer_instance_size = LV_MAX(draw_ctx->layer_instance_size, gpu->draw_ctx->layer_instance_size);
        LV_LOG_INFO("layer_instance_size = %d", (int)draw_ctx->layer_instance_size);

        gpu = gpu->fallback;
    }
}

static void gpu_manager_draw_ctx_deinit(lv_disp_drv_t * disp_drv, lv_draw_ctx_t * draw_ctx)
{
    lv_gpu_manager_t * manager = (lv_gpu_manager_t *)draw_ctx;

    lv_gpu_ctx_t * gpu = manager->head;

    while(gpu) {
        lv_gpu_ctx_t * fallback = gpu->fallback;
        LV_GPU_LOG_INFO("GPU[%s] deinit", gpu->name);
        gpu->draw_ctx_deinit(disp_drv, gpu->draw_ctx);
        lv_mem_free(gpu->draw_ctx);
        lv_mem_free(gpu);
        gpu = fallback;
    }

    lv_gpu_conv_deinit();
}

static void gpu_manager_draw_rect(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords)
{
    GPU_MANAGER_FALLBACK_DRAW(draw_rect, dsc, coords);
}

static void gpu_manager_draw_arc(lv_draw_ctx_t * draw_ctx, const lv_draw_arc_dsc_t * dsc, const lv_point_t * center,
                                 uint16_t radius, uint16_t start_angle, uint16_t end_angle)
{
    GPU_MANAGER_FALLBACK_DRAW(draw_arc, dsc, center, radius, start_angle, end_angle);
}

static void gpu_manager_draw_img_decoded(lv_draw_ctx_t * draw_ctx, const lv_draw_img_dsc_t * dsc,
                                         const lv_area_t * coords, const uint8_t * map_p, lv_img_cf_t color_format)
{
    GPU_MANAGER_FALLBACK_DRAW(draw_img_decoded, dsc, coords, map_p, color_format);
}

static void gpu_manager_draw_letter(lv_draw_ctx_t * draw_ctx, const lv_draw_label_dsc_t * dsc, const lv_point_t * pos_p,
                                    uint32_t letter)
{
    GPU_MANAGER_FALLBACK_DRAW(draw_letter, dsc, pos_p, letter);
}

static void gpu_manager_draw_line(lv_draw_ctx_t * draw_ctx, const lv_draw_line_dsc_t * dsc, const lv_point_t * point1,
                                  const lv_point_t * point2)
{
    GPU_MANAGER_FALLBACK_DRAW(draw_line, dsc, point1, point2);
}

static void gpu_manager_draw_polygon(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * draw_dsc,
                                     const lv_point_t * points, uint16_t point_cnt)
{
    GPU_MANAGER_FALLBACK_DRAW(draw_polygon, draw_dsc, points, point_cnt);
}

static void gpu_manager_draw_transform(lv_draw_ctx_t * draw_ctx, const lv_area_t * dest_area, const void * src_buf,
                                       lv_coord_t src_w, lv_coord_t src_h, lv_coord_t src_stride,
                                       const lv_draw_img_dsc_t * draw_dsc, lv_img_cf_t cf, lv_color_t * cbuf, lv_opa_t * abuf)
{
    GPU_MANAGER_FALLBACK_DRAW(draw_transform, dest_area, src_buf, src_w, src_h, src_stride, draw_dsc, cf, cbuf, abuf);
}

static void gpu_manager_draw_bg(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * draw_dsc, const lv_area_t * coords)
{
    GPU_MANAGER_FALLBACK_DRAW(draw_bg, draw_dsc, coords);
}

static void gpu_manager_wait_for_finish(lv_draw_ctx_t * draw_ctx)
{
    GPU_MANAGER_FALLBACK_BEGIN(wait_for_finish)
    gpu->draw_ctx->wait_for_finish(gpu->draw_ctx);
    GPU_MANAGER_FALLBACK_END
}

static void gpu_manager_buffer_copy(lv_draw_ctx_t * draw_ctx, void * dest_buf, lv_coord_t dest_stride,
                                    const lv_area_t * dest_area,
                                    void * src_buf, lv_coord_t src_stride, const lv_area_t * src_area)
{
    GPU_MANAGER_FALLBACK_DRAW(buffer_copy, dest_buf, dest_stride, dest_area, src_buf, src_stride, src_area);
}

static lv_draw_layer_ctx_t * gpu_manager_layer_init(lv_draw_ctx_t * draw_ctx, lv_draw_layer_ctx_t * layer_ctx,
                                                    lv_draw_layer_flags_t flags)
{
    lv_draw_layer_ctx_t * ctx = NULL;
    GPU_MANAGER_FALLBACK_BEGIN(layer_init)
    ctx = gpu->draw_ctx->layer_init(gpu->draw_ctx, layer_ctx, flags);
    if(ctx) {
        GPU_MANAGER_COPY_DRAW_CTX_PARAM(draw_ctx, gpu->draw_ctx);
        LV_GPU_LOG_TRACE("draw_ctx->buf = %p", draw_ctx->buf);
        break;
    }
    GPU_MANAGER_FALLBACK_END
    return ctx;
}

static void gpu_manager_layer_adjust(lv_draw_ctx_t * draw_ctx, lv_draw_layer_ctx_t * layer_ctx,
                                     lv_draw_layer_flags_t flags)
{
    GPU_MANAGER_FALLBACK_BEGIN(layer_adjust)
    gpu->draw_ctx->layer_adjust(gpu->draw_ctx, layer_ctx, flags);
    if(gpu->draw_ok) {
        GPU_MANAGER_COPY_DRAW_CTX_PARAM(draw_ctx, gpu->draw_ctx);
        LV_GPU_LOG_TRACE("draw_ctx->buf = %p", draw_ctx->buf);
        break;
    }
    GPU_MANAGER_FALLBACK_END
}

static void gpu_manager_layer_blend(lv_draw_ctx_t * draw_ctx, lv_draw_layer_ctx_t * layer_ctx,
                                    const lv_draw_img_dsc_t * draw_dsc)
{
    GPU_MANAGER_FALLBACK_BEGIN(layer_blend)
    gpu->draw_ctx->layer_blend(gpu->draw_ctx, layer_ctx, draw_dsc);
    if(gpu->draw_ok) {
        GPU_MANAGER_COPY_DRAW_CTX_PARAM(draw_ctx, gpu->draw_ctx);
        LV_GPU_LOG_TRACE("draw_ctx->buf = %p", draw_ctx->buf);
        break;
    }
    GPU_MANAGER_FALLBACK_END
}

static void gpu_manager_layer_destroy(lv_draw_ctx_t * draw_ctx, lv_draw_layer_ctx_t * layer_ctx)
{
    GPU_MANAGER_FALLBACK_DRAW(layer_destroy, layer_ctx);
}
