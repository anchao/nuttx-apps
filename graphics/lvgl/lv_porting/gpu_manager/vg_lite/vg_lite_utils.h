/**
 * @file vg_lite_utils.h
 *
 */

#ifndef VG_LITE_UTILS_H
#define VG_LITE_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../lv_gpu_types.h"
#include "../lv_gpu_utils.h"

#ifdef CONFIG_LV_GPU_USE_VG_LITE

#include <vg_lite.h>

/*********************
 *      DEFINES
 *********************/

#define VG_LITE_IS_ERROR(err) (err > 0)

#define VG_LITE_CHECK_ERROR(func)                                  \
    do {                                                           \
        error = func;                                              \
        if (VG_LITE_IS_ERROR(error)) {                             \
            LV_GPU_LOG_ERROR("Execute '" #func "' error(%d): %s",  \
                             (int)error, vg_lite_get_error_type_string(error)); \
            goto error_handler;                                    \
        }                                                          \
    } while (0)

#define VG_LITE_WAIT_FINISH()                  \
    do {                                       \
        LV_GPU_PERF_WAIT_START();              \
        VG_LITE_CHECK_ERROR(vg_lite_finish()); \
        LV_GPU_PERF_WAIT_STOP();               \
    } while (0)

#define VG_LITE_ALIGN(number, align_bytes) \
    (((number) + ((align_bytes)-1)) & ~((align_bytes)-1))

#define VG_LITE_IS_ALIGNED(num, align) (((uint32_t)(num) & ((align)-1)) == 0)

#define VG_LITE_DUMP_BUFFER_INFO(buffer)                                                      \
    do {                                                                                      \
        LV_GPU_LOG_TRACE("name: %s", #buffer);                                                \
        LV_GPU_LOG_TRACE("memory: %p", (buffer)->memory);                                     \
        LV_GPU_LOG_TRACE("address: 0x%08x", (int)(buffer)->address);                          \
        LV_GPU_LOG_TRACE("size: W%d x H%d", (int)((buffer)->width), (int)((buffer)->height)); \
        LV_GPU_LOG_TRACE("stride: %d", (int)((buffer)->stride));                              \
        LV_GPU_LOG_TRACE("format: %d (%s)",                                                   \
                         (int)((buffer)->format),                                                          \
                         vg_lite_get_buffer_format_string((buffer)->format));                              \
    } while (0)

#define VG_LITE_DUMP_MATRIX_INFO(matrix)                                  \
    do {                                                                  \
        LV_GPU_LOG_TRACE("name: %s", #matrix);                            \
        for (int i = 0; i < 3; i++) {                                     \
            LV_GPU_LOG_TRACE("| %0.2f, %0.2f, %0.2f |",                   \
                             (matrix)->m[i][0], (matrix)->m[i][1], (matrix)->m[i][2]); \
        }                                                                 \
    } while (0)

#define VG_LITE_IMAGE_MAGIC_NUM 0x7615600D /* VGISGOOD:1981112333 */

#define VG_LITE_IS_INDEX_FMT(fmt) ((fmt) >= VG_LITE_INDEX_1 && (fmt) <= VG_LITE_INDEX_8)

#if LV_COLOR_DEPTH == 32
#define VG_LITE_NATIVE_COLOR_FMT VG_LITE_BGRA8888
#elif LV_COLOR_DEPTH == 24
#define VG_LITE_NATIVE_COLOR_FMT VG_LITE_BGR888
#elif LV_COLOR_DEPTH == 16
#define VG_LITE_NATIVE_COLOR_FMT VG_LITE_BGR565
#else
#error "Unsupport LV_COLOR_DEPTH"
#endif

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    int pathcount;
    /* A transformation matrix to be applied to ALL paths within this file.
     * note that gradient matrices should NOT be multiplied with this. */
    vg_lite_matrix_t transform;
    void * evo_path_dsc;
} vg_lite_evo_info_t;

typedef struct {
    uint32_t magic;
    union {
        vg_lite_buffer_t vg_buf;
        vg_lite_evo_info_t evo_info;
    };
    uint32_t recolor;
} __attribute__((aligned(64))) vg_lite_img_header_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/* Print info */

void vg_lite_dump_info(void);

const char * vg_lite_get_error_type_string(vg_lite_error_t error);

const char * vg_lite_get_buffer_format_string(vg_lite_buffer_format_t format);

const char * vg_lite_get_filter_string(vg_lite_filter_t filter);

const char * vg_lite_get_blend_string(vg_lite_blend_t blend);

const char * vg_lite_get_global_alpha_string(vg_lite_global_alpha_t global_alpha);

const char * vg_lite_get_fill_rule_string(vg_lite_fill_t fill_rule);

const char * vg_lite_get_image_mode_string(vg_lite_buffer_image_mode_t image_mode);

const char * vg_lite_get_vlc_op_string(uint8_t vlc_op);

int vg_lite_get_vlc_op_arg_len(uint8_t vlc_op);

void vg_lite_dump_path_info(const vg_lite_path_t * path);

/* Converter */

void vg_lite_get_format_bytes(
    vg_lite_buffer_format_t format,
    uint32_t * mul,
    uint32_t * div,
    uint32_t * bytes_align);

void vg_lite_buffer_init(vg_lite_buffer_t * buffer);

bool vg_lite_custom_buffer_init(
    vg_lite_buffer_t * buffer,
    void * ptr,
    int32_t width,
    int32_t height,
    vg_lite_buffer_format_t format);

void vg_lite_img_trasnfrom_to_matrix(vg_lite_matrix_t * matrix, const lv_draw_img_dsc_t * dsc,
                                     const lv_area_t * coords);

vg_lite_buffer_format_t vg_lite_img_cf_to_vg_fmt(lv_img_cf_t cf);

vg_lite_color_t vg_lite_lv_color_to_vg_color(lv_color_t color, lv_opa_t opa);

bool vg_lite_gpu_buf_to_vg_buf(vg_lite_buffer_t * vg_buf, const lv_gpu_dest_buf_t * gpu_buf);

vg_lite_buffer_t * vg_lite_img_data_to_vg_buf(void * img_data);

bool vg_lite_create_vg_buf_from_img_data(
    vg_lite_buffer_t * vg_buf,
    const uint8_t * img_data,
    const lv_img_header_t * header,
    uint8_t * buf_p,
    lv_color32_t recolor,
    bool preprocessed);

uint32_t vg_lite_img_buf_get_align_width(lv_img_cf_t cf);

uint32_t vg_lite_img_buf_get_buf_size(lv_coord_t w, lv_coord_t h, lv_img_cf_t cf);

void * vg_lite_img_alloc(lv_coord_t w, lv_coord_t h, lv_img_cf_t cf, uint32_t * data_size);

void vg_lite_img_free(void * img);

void * vg_lite_img_file_alloc(const char * filename, uint32_t * size, uint32_t offset);

lv_img_dsc_t * vg_lite_img_dsc_create(lv_coord_t w, lv_coord_t h, lv_img_cf_t cf);

void vg_lite_img_dsc_del(lv_img_dsc_t * img_dsc);

void vg_lite_img_dsc_update_header(lv_img_dsc_t * img_dsc);

uint32_t vg_lite_get_palette_size(vg_lite_buffer_format_t format);

vg_lite_blend_t vg_lite_lv_blend_mode_to_vg_blend_mode(lv_blend_mode_t mode);

/**********************
 *      MACROS
 **********************/

#endif /* CONFIG_LV_GPU_USE_VG_LITE */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*VG_LITE_UTILS_H*/
