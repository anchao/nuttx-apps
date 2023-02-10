/**
 * @file lv_gpu_utils.h
 *
 */

#ifndef LV_GPU_UTILS_H
#define LV_GPU_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include <assert.h>
#include <inttypes.h>
#include <lvgl/lvgl.h>
#include <nuttx/config.h>
#include <stdint.h>

/*********************
 *      DEFINES
 *********************/

#define LV_GPU_ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define LV_GPU_STATIC_ASSERT(expr) static_assert(expr)

#ifdef CONFIG_LV_GPU_MANAGER_USE_LOG
#define LV_GPU_LOG_TRACE LV_LOG_INFO
#define LV_GPU_LOG_INFO LV_LOG_INFO
#define LV_GPU_LOG_WARN LV_LOG_WARN
#define LV_GPU_LOG_ERROR LV_LOG_ERROR
#define LV_GPU_LOG_USER LV_LOG_USER
#else
#define LV_GPU_LOG_TRACE(...)
#define LV_GPU_LOG_INFO(...)
#define LV_GPU_LOG_WARN(...)
#define LV_GPU_LOG_ERROR(...)
#define LV_GPU_LOG_USER(...)
#endif /* CONFIG_LV_GPU_MANAGER_USE_LOG */

#ifdef CONFIG_LV_GPU_MANAGER_USE_PERF
#define LV_GPU_PERF_START() uint32_t gpu_perf_start = lv_gpu_tick_get_us()
#define LV_GPU_PERF_STOP()                            \
    do {                                              \
        LV_GPU_LOG_INFO("[PERF] total %" PRIu32 "us", \
                        lv_gpu_tick_elaps_us(gpu_perf_start));    \
    } while (0)

#define LV_GPU_PERF_WAIT_START() uint32_t gpu_perf_wait_start = lv_gpu_tick_get_us()
#define LV_GPU_PERF_WAIT_STOP()                                            \
    do {                                                                   \
        LV_GPU_LOG_INFO("[PERF] total %" PRIu32 "us / wait %" PRIu32 "us", \
                        lv_gpu_tick_elaps_us(gpu_perf_start),                          \
                        lv_gpu_tick_elaps_us(gpu_perf_wait_start));                    \
    } while (0)
#else
#define LV_GPU_PERF_START()
#define LV_GPU_PERF_STOP()
#define LV_GPU_PERF_WAIT_START()
#define LV_GPU_PERF_WAIT_STOP()
#endif /* CONFIG_LV_GPU_MANAGER_USE_PERF */

#define LV_GPU_DUMP_COLOR(c)                                      \
    do {                                                          \
        LV_GPU_LOG_INFO(#c ": %" LV_PRIx32 " (R%d G%d B%d)",      \
                        (c)->full, (c)->ch.red, (c)->ch.blue, (c)->ch.green); \
    } while (0)

#define LV_GPU_DUMP_OPA(opa)                       \
    do {                                           \
        LV_GPU_LOG_INFO(#opa " = LV_OPA_%d", opa); \
    } while (0)

#define LV_GPU_DUMP_DEST_BUFFER_INFO(dest_buf)                      \
    do {                                                            \
        LV_GPU_LOG_TRACE("name: %s", #dest_buf);                    \
        LV_GPU_LOG_TRACE("buf: %p", (dest_buf)->buf);               \
        LV_GPU_LOG_TRACE("buf_area: (%d, %d, %d, %d)",              \
                         (dest_buf)->buf_area->x1, (dest_buf)->buf_area->y1,     \
                         (dest_buf)->buf_area->x2, (dest_buf)->buf_area->y2);    \
        LV_GPU_LOG_TRACE("clip_area: (%d, %d, %d, %d)",             \
                         (dest_buf)->clip_area->x1, (dest_buf)->clip_area->y1,   \
                         (dest_buf)->clip_area->x2, (dest_buf)->clip_area->y2); \
        LV_GPU_LOG_TRACE("cf: %d", (dest_buf)->cf);                 \
    } while (0)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

uint32_t lv_gpu_tick_get_us(void);

uint32_t lv_gpu_tick_elaps_us(uint32_t prev_tick);

void lv_gpu_dump_draw_ctx_info(const lv_draw_ctx_t * draw_ctx);

void lv_gpu_dump_draw_rect_dsc_info(const lv_draw_rect_dsc_t * dsc);

float lv_gpu_fast_inv_sqrt(float number);

lv_img_cf_t lv_gpu_get_disp_refr_cf(void);

lv_img_cf_t lv_gpu_register_custom_img_cf(void);

void * lv_gpu_malloc(size_t size);

void * lv_gpu_aligned_alloc(size_t align, size_t size);

void * lv_gpu_realloc(void * oldmem, size_t newsize);

void lv_gpu_free(void * mem);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_GPU_UTILS_H*/
