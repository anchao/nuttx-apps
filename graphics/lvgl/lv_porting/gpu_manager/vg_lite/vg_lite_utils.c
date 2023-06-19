/**
 * @file vg_lite_utils.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "vg_lite_internal.h"
#include "vg_lite_decoder.h"

#ifdef CONFIG_LV_GPU_USE_VG_LITE

#include "../lv_gpu_conv.h"
#include <string.h>

/*********************
 *      DEFINES
 *********************/

#define ENUM_TO_STRING_DEF(e) \
    case (e):                 \
    return #e

#define VG_ENUM_TO_STRING_CASE_DEF(e) \
    case (VG_LITE_##e):               \
    return #e

#define VLC_OP_ARG_LEN_DEF(OP, LEN) \
    case VLC_OP_##OP:               \
    return (LEN)

#define VLC_GET_OP_CODE(ptr) (*((uint8_t*)((float*)(ptr))))

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

void vg_lite_dump_info(void)
{
    char name[64];
    uint32_t chip_id;
    uint32_t chip_rev;
    uint32_t cid;
    vg_lite_get_product_info(name, &chip_id, &chip_rev);
    vg_lite_get_register(0x30, &cid);
    LV_GPU_LOG_INFO("Product Info: %s"
                    " | Chip ID: 0x%" PRIx32
                    " | Revision: 0x%" PRIx32
                    " | CID: 0x%" PRIx32,
                    name, chip_id, chip_rev, cid);

    vg_lite_info_t info;
    vg_lite_get_info(&info);
    LV_GPU_LOG_INFO("VGLite API version: 0x%" PRIx32, info.api_version);
    LV_GPU_LOG_INFO("VGLite API header version: 0x%" PRIx32, info.header_version);
    LV_GPU_LOG_INFO("VGLite release version: 0x%" PRIx32, info.release_version);

    for(int feature = 0; feature < gcFEATURE_COUNT; feature++) {
        uint32_t ret = vg_lite_query_feature((vg_lite_feature_t)feature);
        LV_UNUSED(ret);
        LV_GPU_LOG_INFO("Feature: %d - %s",
                        feature,
                        ret ? "YES" : "NO");
    }

    uint32_t mem_avail;
    vg_lite_mem_avail(&mem_avail);
    LV_GPU_LOG_INFO("Memory Avaliable: %" PRId32 " Bytes", mem_avail);
}

const char * vg_lite_get_error_type_string(vg_lite_error_t error)
{
    switch(error) {
            VG_ENUM_TO_STRING_CASE_DEF(SUCCESS);
            VG_ENUM_TO_STRING_CASE_DEF(INVALID_ARGUMENT);
            VG_ENUM_TO_STRING_CASE_DEF(OUT_OF_MEMORY);
            VG_ENUM_TO_STRING_CASE_DEF(NO_CONTEXT);
            VG_ENUM_TO_STRING_CASE_DEF(TIMEOUT);
            VG_ENUM_TO_STRING_CASE_DEF(OUT_OF_RESOURCES);
            VG_ENUM_TO_STRING_CASE_DEF(GENERIC_IO);
            VG_ENUM_TO_STRING_CASE_DEF(NOT_SUPPORT);
            VG_ENUM_TO_STRING_CASE_DEF(ALREADY_EXISTS);
            VG_ENUM_TO_STRING_CASE_DEF(NOT_ALIGNED);
            VG_ENUM_TO_STRING_CASE_DEF(FLEXA_TIME_OUT);
            VG_ENUM_TO_STRING_CASE_DEF(FLEXA_HANDSHAKE_FAIL);
        default:
            break;
    }
    return "UNKNOW_ERROR";
}

const char * vg_lite_get_buffer_format_string(vg_lite_buffer_format_t format)
{
    switch(format) {
            VG_ENUM_TO_STRING_CASE_DEF(RGBA8888);
            VG_ENUM_TO_STRING_CASE_DEF(BGRA8888);
            VG_ENUM_TO_STRING_CASE_DEF(RGBX8888);
            VG_ENUM_TO_STRING_CASE_DEF(BGRX8888);
            VG_ENUM_TO_STRING_CASE_DEF(RGB565);
            VG_ENUM_TO_STRING_CASE_DEF(BGR565);
            VG_ENUM_TO_STRING_CASE_DEF(RGBA4444);
            VG_ENUM_TO_STRING_CASE_DEF(BGRA4444);
            VG_ENUM_TO_STRING_CASE_DEF(BGRA5551);
            VG_ENUM_TO_STRING_CASE_DEF(A4);
            VG_ENUM_TO_STRING_CASE_DEF(A8);
            VG_ENUM_TO_STRING_CASE_DEF(L8);
            VG_ENUM_TO_STRING_CASE_DEF(YUYV);
            VG_ENUM_TO_STRING_CASE_DEF(YUY2);
            VG_ENUM_TO_STRING_CASE_DEF(NV12);
            VG_ENUM_TO_STRING_CASE_DEF(ANV12);
            VG_ENUM_TO_STRING_CASE_DEF(AYUY2);
            VG_ENUM_TO_STRING_CASE_DEF(YV12);
            VG_ENUM_TO_STRING_CASE_DEF(YV24);
            VG_ENUM_TO_STRING_CASE_DEF(YV16);
            VG_ENUM_TO_STRING_CASE_DEF(NV16);
            VG_ENUM_TO_STRING_CASE_DEF(YUY2_TILED);
            VG_ENUM_TO_STRING_CASE_DEF(NV12_TILED);
            VG_ENUM_TO_STRING_CASE_DEF(ANV12_TILED);
            VG_ENUM_TO_STRING_CASE_DEF(AYUY2_TILED);
            VG_ENUM_TO_STRING_CASE_DEF(INDEX_1);
            VG_ENUM_TO_STRING_CASE_DEF(INDEX_2);
            VG_ENUM_TO_STRING_CASE_DEF(INDEX_4);
            VG_ENUM_TO_STRING_CASE_DEF(INDEX_8);
            VG_ENUM_TO_STRING_CASE_DEF(RGBA2222);
            VG_ENUM_TO_STRING_CASE_DEF(BGRA2222);
            VG_ENUM_TO_STRING_CASE_DEF(ABGR2222);
            VG_ENUM_TO_STRING_CASE_DEF(ARGB2222);
            VG_ENUM_TO_STRING_CASE_DEF(ABGR4444);
            VG_ENUM_TO_STRING_CASE_DEF(ARGB4444);
            VG_ENUM_TO_STRING_CASE_DEF(ABGR8888);
            VG_ENUM_TO_STRING_CASE_DEF(ARGB8888);
            VG_ENUM_TO_STRING_CASE_DEF(ABGR1555);
            VG_ENUM_TO_STRING_CASE_DEF(RGBA5551);
            VG_ENUM_TO_STRING_CASE_DEF(ARGB1555);
            VG_ENUM_TO_STRING_CASE_DEF(XBGR8888);
            VG_ENUM_TO_STRING_CASE_DEF(XRGB8888);
            VG_ENUM_TO_STRING_CASE_DEF(RGBA8888_ETC2_EAC);
            VG_ENUM_TO_STRING_CASE_DEF(RGB888);
            VG_ENUM_TO_STRING_CASE_DEF(BGR888);
            VG_ENUM_TO_STRING_CASE_DEF(ABGR8565);
            VG_ENUM_TO_STRING_CASE_DEF(BGRA5658);
            VG_ENUM_TO_STRING_CASE_DEF(ARGB8565);
            VG_ENUM_TO_STRING_CASE_DEF(RGBA5658);
        default:
            break;
    }
    return "UNKNOW_BUFFER_FORMAT";
}

const char * vg_lite_get_filter_string(vg_lite_filter_t filter)
{
    switch(filter) {
            VG_ENUM_TO_STRING_CASE_DEF(FILTER_POINT);
            VG_ENUM_TO_STRING_CASE_DEF(FILTER_LINEAR);
            VG_ENUM_TO_STRING_CASE_DEF(FILTER_BI_LINEAR);
        default:
            break;
    }
    return "UNKNOW_FILTER";
}

const char * vg_lite_get_blend_string(vg_lite_blend_t blend)
{
    switch(blend) {
            VG_ENUM_TO_STRING_CASE_DEF(BLEND_NONE); /*! S, i.e. no blending. */
            VG_ENUM_TO_STRING_CASE_DEF(BLEND_SRC_OVER); /*! S + (1 - Sa) * D */
            VG_ENUM_TO_STRING_CASE_DEF(BLEND_DST_OVER); /*! (1 - Da) * S + D */
            VG_ENUM_TO_STRING_CASE_DEF(BLEND_SRC_IN); /*! Da * S */
            VG_ENUM_TO_STRING_CASE_DEF(BLEND_DST_IN); /*! Sa * D */
            VG_ENUM_TO_STRING_CASE_DEF(BLEND_SCREEN); /*! S + D - S * D */
            VG_ENUM_TO_STRING_CASE_DEF(BLEND_MULTIPLY); /*! S * (1 - Da) + D * (1 - Sa) + S * D */
            VG_ENUM_TO_STRING_CASE_DEF(BLEND_ADDITIVE); /*! S + D */
            VG_ENUM_TO_STRING_CASE_DEF(BLEND_SUBTRACT); /*! D * (1 - S) */
#ifdef CONFIG_LV_GPU_VG_LITE_LVGL_BLEND_SUPPORT
            VG_ENUM_TO_STRING_CASE_DEF(BLEND_SUBTRACT_LVGL); /*! D - S */
            VG_ENUM_TO_STRING_CASE_DEF(BLEND_NORMAL_LVGL); /*! S * Sa + (1 - Sa) * D  */
            VG_ENUM_TO_STRING_CASE_DEF(BLEND_ADDITIVE_LVGL); /*! (S + D) * Sa + D * (1 - Sa) */
            VG_ENUM_TO_STRING_CASE_DEF(BLEND_MULTIPLY_LVGL); /*! (S * D) * Sa + D * (1 - Sa) */
#endif
        default:
            break;
    }
    return "UNKNOW_BLEND";
}

const char * vg_lite_get_global_alpha_string(vg_lite_global_alpha_t global_alpha)
{
    switch(global_alpha) {
            VG_ENUM_TO_STRING_CASE_DEF(NORMAL);
            VG_ENUM_TO_STRING_CASE_DEF(GLOBAL);
            VG_ENUM_TO_STRING_CASE_DEF(SCALED);
        default:
            break;
    }
    return "UNKNOW_GLOBAL_ALPHA";
}

const char * vg_lite_get_fill_rule_string(vg_lite_fill_t fill_rule)
{
    switch(fill_rule) {
            VG_ENUM_TO_STRING_CASE_DEF(FILL_NON_ZERO);
            VG_ENUM_TO_STRING_CASE_DEF(FILL_EVEN_ODD);
        default:
            break;
    }
    return "UNKNOW_FILL";
}

const char * vg_lite_get_image_mode_string(vg_lite_buffer_image_mode_t image_mode)
{
    switch(image_mode) {
            VG_ENUM_TO_STRING_CASE_DEF(NORMAL_IMAGE_MODE);
            VG_ENUM_TO_STRING_CASE_DEF(NONE_IMAGE_MODE);
            VG_ENUM_TO_STRING_CASE_DEF(MULTIPLY_IMAGE_MODE);
        default:
            break;
    }
    return "UNKNOW_IMAGE_MODE";
}

const char * vg_lite_get_vlc_op_string(uint8_t vlc_op)
{
    switch(vlc_op) {
            ENUM_TO_STRING_DEF(VLC_OP_END);
            ENUM_TO_STRING_DEF(VLC_OP_CLOSE);
            ENUM_TO_STRING_DEF(VLC_OP_MOVE);
            ENUM_TO_STRING_DEF(VLC_OP_MOVE_REL);
            ENUM_TO_STRING_DEF(VLC_OP_LINE);
            ENUM_TO_STRING_DEF(VLC_OP_LINE_REL);
            ENUM_TO_STRING_DEF(VLC_OP_QUAD);
            ENUM_TO_STRING_DEF(VLC_OP_QUAD_REL);
            ENUM_TO_STRING_DEF(VLC_OP_CUBIC);
            ENUM_TO_STRING_DEF(VLC_OP_CUBIC_REL);

            ENUM_TO_STRING_DEF(VLC_OP_SCCWARC);
            ENUM_TO_STRING_DEF(VLC_OP_SCCWARC_REL);
            ENUM_TO_STRING_DEF(VLC_OP_SCWARC);
            ENUM_TO_STRING_DEF(VLC_OP_SCWARC_REL);
            ENUM_TO_STRING_DEF(VLC_OP_LCCWARC);
            ENUM_TO_STRING_DEF(VLC_OP_LCCWARC_REL);
            ENUM_TO_STRING_DEF(VLC_OP_LCWARC);
            ENUM_TO_STRING_DEF(VLC_OP_LCWARC_REL);
        default:
            break;
    }
    return "UNKNOW_VLC_OP";
}

int vg_lite_get_vlc_op_arg_len(uint8_t vlc_op)
{
    switch(vlc_op) {
            VLC_OP_ARG_LEN_DEF(END, 0);
            VLC_OP_ARG_LEN_DEF(CLOSE, 0);
            VLC_OP_ARG_LEN_DEF(MOVE, 2);
            VLC_OP_ARG_LEN_DEF(MOVE_REL, 2);
            VLC_OP_ARG_LEN_DEF(LINE, 2);
            VLC_OP_ARG_LEN_DEF(LINE_REL, 2);
            VLC_OP_ARG_LEN_DEF(QUAD, 4);
            VLC_OP_ARG_LEN_DEF(QUAD_REL, 4);
            VLC_OP_ARG_LEN_DEF(CUBIC, 6);
            VLC_OP_ARG_LEN_DEF(CUBIC_REL, 6);
            VLC_OP_ARG_LEN_DEF(SCCWARC, 5);
            VLC_OP_ARG_LEN_DEF(SCCWARC_REL, 5);
            VLC_OP_ARG_LEN_DEF(SCWARC, 5);
            VLC_OP_ARG_LEN_DEF(SCWARC_REL, 5);
            VLC_OP_ARG_LEN_DEF(LCCWARC, 5);
            VLC_OP_ARG_LEN_DEF(LCCWARC_REL, 5);
            VLC_OP_ARG_LEN_DEF(LCWARC, 5);
            VLC_OP_ARG_LEN_DEF(LCWARC_REL, 5);
        default:
            break;
    }
    LV_GPU_LOG_ERROR("UNKNOW_VLC_OP: 0x%x", vlc_op);
    return 0;
}

void vg_lite_dump_path_info(const vg_lite_path_t * path)
{
    size_t len = path->path_length / sizeof(float);

    LV_GPU_LOG_INFO("address: %p", path->path);
    LV_GPU_LOG_INFO("length: %d", (int)len);
    LV_GPU_LOG_INFO("bonding box: (%0.2f, %0.2f) - (%0.2f, %0.2f)",
                    path->bounding_box[0], path->bounding_box[1],
                    path->bounding_box[2], path->bounding_box[3]);

    if(len == 0) {
        LV_GPU_LOG_WARN("path is empty");
        return;
    }

    volatile float * cur = path->path;
    volatile float * end = cur + len;

    while(cur < end) {
        /* get op code */
        uint8_t op_code = VLC_GET_OP_CODE(cur);
        LV_GPU_LOG_INFO("%d (%s) ->", op_code, vg_lite_get_vlc_op_string(op_code));

        /* get arguments length */
        int arg_len = vg_lite_get_vlc_op_arg_len(op_code);

        /* skip op code */
        cur++;

        /* print arguments */
        while(arg_len--) {
            LV_GPU_LOG_INFO("%0.2f", *cur);
            cur++;
        }
    }

    uint8_t end_op_code = VLC_GET_OP_CODE(end - 1);
    if(end_op_code != VLC_OP_END) {
        LV_GPU_LOG_ERROR("%d (%s) -> is NOT VLC_OP_END", end_op_code, vg_lite_get_vlc_op_string(end_op_code));
    }
}

void vg_lite_get_format_bytes(
    vg_lite_buffer_format_t format,
    uint32_t * mul,
    uint32_t * div,
    uint32_t * bytes_align)
{
    /* Get the bpp information of a color format. */
    *mul = *div = 1;
    *bytes_align = 4;
    switch(format) {
        case VG_LITE_L8:
        case VG_LITE_A8:
        case VG_LITE_RGBA8888_ETC2_EAC:
            break;
        case VG_LITE_A4:
            *div = 2;
            break;
        case VG_LITE_ABGR1555:
        case VG_LITE_ARGB1555:
        case VG_LITE_BGRA5551:
        case VG_LITE_RGBA5551:
        case VG_LITE_RGBA4444:
        case VG_LITE_BGRA4444:
        case VG_LITE_ABGR4444:
        case VG_LITE_ARGB4444:
        case VG_LITE_RGB565:
        case VG_LITE_BGR565:
        case VG_LITE_YUYV:
        case VG_LITE_YUY2:
        case VG_LITE_YUY2_TILED:
        /* AYUY2 buffer memory = YUY2 + alpha. */
        case VG_LITE_AYUY2:
        case VG_LITE_AYUY2_TILED:
            *mul = 2;
            break;
        case VG_LITE_RGBA8888:
        case VG_LITE_BGRA8888:
        case VG_LITE_ABGR8888:
        case VG_LITE_ARGB8888:
        case VG_LITE_RGBX8888:
        case VG_LITE_BGRX8888:
        case VG_LITE_XBGR8888:
        case VG_LITE_XRGB8888:
            *mul = 4;
            break;
        case VG_LITE_NV12:
        case VG_LITE_NV12_TILED:
            *mul = 3;
            break;
        case VG_LITE_ANV12:
        case VG_LITE_ANV12_TILED:
            *mul = 4;
            break;
        case VG_LITE_INDEX_1:
            *div = 8;
            *bytes_align = 8;
            break;
        case VG_LITE_INDEX_2:
            *div = 4;
            *bytes_align = 8;
            break;
        case VG_LITE_INDEX_4:
            *div = 2;
            *bytes_align = 8;
            break;
        case VG_LITE_INDEX_8:
            *bytes_align = 1;
            break;
        case VG_LITE_RGBA2222:
        case VG_LITE_BGRA2222:
        case VG_LITE_ABGR2222:
        case VG_LITE_ARGB2222:
            *mul = 1;
            break;
        case VG_LITE_RGB888:
        case VG_LITE_BGR888:
        case VG_LITE_ABGR8565:
        case VG_LITE_BGRA5658:
        case VG_LITE_ARGB8565:
        case VG_LITE_RGBA5658:
            *mul = 3;
            break;
        default:
            break;
    }
}

void vg_lite_buffer_init(vg_lite_buffer_t * buffer)
{
    memset(buffer, 0, sizeof(vg_lite_buffer_t));
}

bool vg_lite_custom_buffer_init(
    vg_lite_buffer_t * buffer,
    void * ptr,
    int32_t width,
    int32_t height,
    vg_lite_buffer_format_t format)
{
    uint32_t mul;
    uint32_t div;
    uint32_t align;
    LV_ASSERT_NULL(buffer);
    LV_ASSERT_NULL(ptr);

    vg_lite_buffer_init(buffer);

    if(!VG_LITE_IS_ALIGNED(width, VG_LITE_IMG_SRC_PX_ALIGN)) {
        LV_GPU_LOG_ERROR("buffer width(%d) not aligned to %dpx",
                         (int)width, VG_LITE_IMG_SRC_PX_ALIGN);
        return false;
    }

    buffer->format = format;
    if(format == VG_LITE_RGBA8888_ETC2_EAC) {
        buffer->tiled = VG_LITE_TILED;
    }
    else {
        buffer->tiled = VG_LITE_LINEAR;
    }
    buffer->image_mode = VG_LITE_NORMAL_IMAGE_MODE;
    buffer->transparency_mode = VG_LITE_IMAGE_OPAQUE;
    buffer->width = width;
    buffer->height = height;
    vg_lite_get_format_bytes(buffer->format, &mul, &div, &align);
    buffer->stride = VG_LITE_ALIGN((buffer->width * mul / div), align);
    buffer->memory = ptr;
    buffer->address = (uint32_t)(uintptr_t)ptr;

    return true;
}

void vg_lite_img_trasnfrom_to_matrix(vg_lite_matrix_t * matrix, const lv_draw_img_dsc_t * dsc, const lv_area_t * coords)
{
    LV_ASSERT_NULL(matrix);
    LV_ASSERT_NULL(dsc);
    LV_ASSERT_NULL(coords);

    vg_lite_identity(matrix);

    int16_t angle = dsc->angle;
    uint16_t zoom = dsc->zoom;
    lv_point_t pivot = dsc->pivot;

    vg_lite_translate(coords->x1, coords->y1, matrix);

    if(angle != 0 || zoom != LV_IMG_ZOOM_NONE) {
        vg_lite_translate(pivot.x, pivot.y, matrix);

        if(zoom != LV_IMG_ZOOM_NONE) {
            vg_lite_float_t scale = (vg_lite_float_t)zoom / LV_IMG_ZOOM_NONE;
            vg_lite_scale(scale, scale, matrix);
        }

        if(angle != 0) {
            vg_lite_rotate(angle * 0.1f, matrix);
        }

        vg_lite_translate(-pivot.x, -pivot.y, matrix);
    }
}

vg_lite_buffer_format_t vg_lite_img_cf_to_vg_fmt(lv_img_cf_t cf)
{
    vg_lite_buffer_format_t vg_fmt = VG_LITE_RGBA8888;
    switch(cf) {
        case LV_IMG_CF_TRUE_COLOR:
#if LV_COLOR_DEPTH == 32
            vg_fmt = VG_LITE_BGRX8888;
#elif LV_COLOR_DEPTH == 24
            vg_fmt = VG_LITE_BGR888;
#elif LV_COLOR_DEPTH == 16
            vg_fmt = VG_LITE_BGR565;
#elif LV_COLOR_DEPTH == 8
            /* NOT SUPPORT */
#elif LV_COLOR_DEPTH == 1
            /* NOT SUPPORT */
#endif
            break;
        case LV_IMG_CF_TRUE_COLOR_ALPHA:
#if LV_COLOR_DEPTH == 32
            vg_fmt = VG_LITE_BGRA8888;
#elif LV_COLOR_DEPTH == 24
            vg_fmt = VG_LITE_BGRA8888;
#elif LV_COLOR_DEPTH == 16
            vg_fmt = VG_LITE_BGRA5658;
#elif LV_COLOR_DEPTH == 8
            /* NOT SUPPORT */
#elif LV_COLOR_DEPTH == 1
            /* NOT SUPPORT */
#endif
            break;
        case LV_IMG_CF_INDEXED_1BIT:
            vg_fmt = VG_LITE_INDEX_1;
            break;
        case LV_IMG_CF_INDEXED_2BIT:
            vg_fmt = VG_LITE_INDEX_2;
            break;
        case LV_IMG_CF_INDEXED_4BIT:
            vg_fmt = VG_LITE_INDEX_4;
            break;
        case LV_IMG_CF_INDEXED_8BIT:
            vg_fmt = VG_LITE_INDEX_8;
            break;
        case LV_IMG_CF_ALPHA_4BIT:
            vg_fmt = VG_LITE_A4;
            break;
        case LV_IMG_CF_ALPHA_8BIT:
            vg_fmt = VG_LITE_A8;
            break;
        case LV_IMG_CF_RGB888:
            vg_fmt = VG_LITE_RGB888;
            break;
        case LV_IMG_CF_RGBA8888:
            vg_fmt = VG_LITE_RGBA8888;
            break;
        case LV_IMG_CF_RGBX8888:
            vg_fmt = VG_LITE_RGBX8888;
            break;
        case LV_IMG_CF_RGB565:
            vg_fmt = VG_LITE_RGB565;
            break;
        case LV_IMG_CF_RGBA5658:
            vg_fmt = VG_LITE_RGBA5658;
            break;
        case VG_LITE_ETC2_COLOR_FORMAT:
            vg_fmt = VG_LITE_RGBA8888_ETC2_EAC;
            break;
        case VG_LITE_YUV_COLOR_FORMAT:
            vg_fmt = VG_LITE_NV12;
            break;
        case VG_LITE_DEC_COLOR_FORMAT:
            vg_fmt = VG_LITE_RGBA8888;
            break;
        // case LV_IMG_CF_RGB565A8:
        //     vg_fmt = VG_LITE_RGB565A8;
        //     break;
        default:
            LV_GPU_LOG_WARN("unsupport color format: %d", cf);
            break;
    }

    return vg_fmt;
}

vg_lite_color_t vg_lite_lv_color_to_vg_color(lv_color_t color, lv_opa_t opa)
{
    lv_color32_t c32;
    c32.full = lv_color_to32(color);
    if(opa < LV_OPA_MAX) {
        c32.ch.red = LV_UDIV255(c32.ch.red * opa);
        c32.ch.green = LV_UDIV255(c32.ch.green * opa);
        c32.ch.blue = LV_UDIV255(c32.ch.blue * opa);
    }
    return (uint32_t)opa << 24 | (uint32_t)c32.ch.blue << 16 | (uint32_t)c32.ch.green << 8 | c32.ch.red;
}

bool vg_lite_gpu_buf_to_vg_buf(vg_lite_buffer_t * vg_buf, const lv_gpu_dest_buf_t * gpu_buf)
{
    bool retval = vg_lite_custom_buffer_init(
                      vg_buf,
                      gpu_buf->buf,
                      lv_area_get_width(gpu_buf->buf_area),
                      lv_area_get_height(gpu_buf->buf_area),
                      vg_lite_img_cf_to_vg_fmt(gpu_buf->cf));
    VG_LITE_DUMP_BUFFER_INFO(vg_buf);
    return retval;
}

vg_lite_buffer_t * vg_lite_img_data_to_vg_buf(void * img_data)
{
    vg_lite_img_header_t * header = img_data;

    if(header->magic == VG_LITE_IMAGE_MAGIC_NUM) {
        LV_GPU_LOG_TRACE("has magic num");
        VG_LITE_DUMP_BUFFER_INFO(&header->vg_buf);
        return &header->vg_buf;
    }

    LV_GPU_LOG_WARN("No magic number");
    return NULL;
}

bool vg_lite_create_vg_buf_from_img_data(
    vg_lite_buffer_t * vg_buf,
    const uint8_t * img_data,
    const lv_img_header_t * header,
    uint8_t * buf_p,
    lv_color32_t recolor,
    bool preprocessed)
{
    LV_ASSERT_NULL(vg_buf);
    LV_ASSERT_NULL(header);

    vg_lite_buffer_t vgbuf;

    void * mem = buf_p;
    uint32_t vgbuf_w = VG_LITE_ALIGN(header->w, VG_LITE_IMG_SRC_PX_ALIGN);
    uint32_t vgbuf_stride = vgbuf_w * sizeof(lv_color_t);
    vg_lite_buffer_format_t vgbuf_format = VG_LITE_NATIVE_COLOR_FMT;
    uint32_t map_stride = header->w * sizeof(lv_color_t);

#if LV_COLOR_DEPTH == 16
    if(header->cf == LV_IMG_CF_TRUE_COLOR_ALPHA
       || header->cf == LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED) {
        vgbuf_stride = vgbuf_w * sizeof(lv_color32_t);
        vgbuf_format = VG_LITE_BGRA8888;
        if(header->cf == LV_IMG_CF_TRUE_COLOR_ALPHA) {
            map_stride = header->w * LV_IMG_PX_SIZE_ALPHA_BYTE;
        }
    }

    if(preprocessed) {
        map_stride = vgbuf_stride;
    }
#endif

    uint32_t vgbuf_size = header->h * vgbuf_stride;

    if(mem == NULL) {
        LV_GPU_LOG_TRACE("mem is NULL, alloc...");
        mem = lv_gpu_aligned_alloc(VG_LITE_IMG_SRC_ADDR_ALIGN, vgbuf_size);
        LV_ASSERT_MALLOC(mem);
    }

    if(mem == NULL) {
        LV_GPU_LOG_WARN("Insufficient memory for GPU 16px aligned image cache");
        return LV_RES_INV;
    }

    vg_lite_custom_buffer_init(&vgbuf, mem, vgbuf_w, header->h, vgbuf_format);
    VG_LITE_DUMP_BUFFER_INFO(&vgbuf);
    uint8_t * px_buf = vgbuf.memory;
    const uint8_t * px_map = img_data;

    lv_gpu_conv_x_to_vgimg_dsc_t conv_dsc;
    lv_gpu_conv_type_t conv_type = _LV_GPU_CONV_TYPE_LAST;
    lv_memset_00(&conv_dsc, sizeof(conv_dsc));
    conv_dsc.base.px_buf = px_buf;
    conv_dsc.base.buf_stride = vgbuf_stride;
    conv_dsc.base.px_map = px_map;
    conv_dsc.base.map_stride = map_stride;
    conv_dsc.base.header = header;
    conv_dsc.recolor = recolor;

    if(preprocessed) {
        /* must be recolored */
        if(header->cf == LV_IMG_CF_TRUE_COLOR) {
#if LV_COLOR_DEPTH == 16
            conv_type = LV_GPU_CONV_TYPE_BGR565_TO_VGIMG;
#elif LV_COLOR_DEPTH == 32
            conv_type = LV_GPU_CONV_TYPE_BGR888_TO_VGIMG;
#endif /* LV_COLOR_DEPTH */
        }
        else {
            /* has alpha, already converted to argb8888 */
            conv_type = LV_GPU_CONV_TYPE_BGRA8888_TO_VGIMG;
            conv_dsc.preprocessed = true;
        }
    }
    else {
        /* regular decode */
        if(header->cf == LV_IMG_CF_TRUE_COLOR_ALPHA) {
#if LV_COLOR_DEPTH == 16
            lv_gpu_conv_bgra5658_to_bgra8888_dsc_t dsc;
            lv_memset_00(&dsc, sizeof(dsc));
            dsc.base.px_buf = px_buf;
            dsc.base.buf_stride = vgbuf_stride;
            dsc.base.px_map = px_map;
            dsc.base.map_stride = map_stride;
            dsc.base.header = header;
            dsc.recolor = recolor;
            lv_gpu_conv_start(LV_GPU_CONV_TYPE_BGRA5658_TO_BGRA8888, &dsc);
#elif LV_COLOR_DEPTH == 32
            conv_type = LV_GPU_CONV_TYPE_BGRA8888_TO_VGIMG;
#endif /* LV_COLOR_DEPTH */
        }
        else {   /* LV_IMG_CF_TRUE_COLOR || LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED */
            uint32_t ckey = 0;
            if(header->cf == LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED) {
                ckey = LV_COLOR_CHROMA_KEY.full;
            }

#if LV_COLOR_DEPTH == 16
            conv_type = LV_GPU_CONV_TYPE_BGR565_TO_VGIMG;
#elif LV_COLOR_DEPTH == 32
            conv_type = LV_GPU_CONV_TYPE_BGR888_TO_VGIMG;
#endif /* LV_COLOR_DEPTH */
            conv_dsc.ckey = ckey;
        }
    }

#ifdef CONFIG_LV_GPU_VG_LITE_IMG_SRC_SOFT_PREMULTIPLIED
    lv_gpu_conv_res_t res = lv_gpu_conv_start(conv_type, &conv_dsc);
    if(res != LV_GPU_CONV_RES_OK) {
        LV_GPU_LOG_WARN("convert failed: %d", res);
        return false;
    }
#else
    uint32_t line_size = lv_img_buf_get_img_size(vgbuf_w, 1, header->cf);
    LV_GPU_LOG_TRACE("line_size = %d", (int)line_size);
    for(lv_coord_t y = 0; y < header->h; y++) {
        lv_memcpy(px_buf, px_map, line_size);
        px_map += map_stride;
        px_buf += vgbuf_stride;
    }
#endif

    /*Save vglite buffer info*/
    lv_memcpy_small(vg_buf, &vgbuf, sizeof(vgbuf));
    return true;
}

uint32_t vg_lite_img_buf_get_align_width(lv_img_cf_t cf)
{
    uint32_t align;
    switch(cf) {
        case LV_IMG_CF_INDEXED_1BIT:
            align = 64;
            break;
        case LV_IMG_CF_INDEXED_2BIT:
            align = 32;
            break;
        default:
            align = VG_LITE_IMG_SRC_PX_ALIGN;
            break;
    }
    return align;
}

uint32_t vg_lite_img_buf_get_buf_size(lv_coord_t w, lv_coord_t h, lv_img_cf_t cf)
{
    w = VG_LITE_ALIGN(w, vg_lite_img_buf_get_align_width(cf));
    uint8_t px_size = cf == LV_IMG_CF_TRUE_COLOR_ALPHA ? 32 : lv_img_cf_get_px_size(cf);
    bool indexed = cf >= LV_IMG_CF_INDEXED_1BIT && cf <= LV_IMG_CF_INDEXED_8BIT;
    bool alpha = cf >= LV_IMG_CF_ALPHA_1BIT && cf <= LV_IMG_CF_ALPHA_8BIT;
    uint32_t palette_size = indexed || alpha ? 1 << px_size : 0;
    uint32_t data_size = w * h * px_size >> 3;
    return sizeof(vg_lite_img_header_t) + data_size + palette_size * sizeof(uint32_t);
}

void * vg_lite_img_alloc(lv_coord_t w, lv_coord_t h, lv_img_cf_t cf, uint32_t * data_size)
{
    /*Get image data size*/
    uint32_t buf_size = vg_lite_img_buf_get_buf_size(w, h, cf);
    if(buf_size == 0) {
        return NULL;
    }

    /*Allocate raw buffer*/
    void * data = lv_gpu_aligned_alloc(VG_LITE_IMG_SRC_ADDR_ALIGN, buf_size);
    LV_ASSERT_MALLOC(data);
    if(data == NULL) {
        LV_GPU_LOG_ERROR("malloc failed for data");
        return NULL;
    }

    if(data_size) {
        *data_size = buf_size;
    }

    return data;
}

void vg_lite_img_free(void * img)
{
    LV_ASSERT_NULL(img);
    lv_gpu_free(img);
}

void * vg_lite_img_file_alloc(const char * filename, uint32_t * size, uint32_t offset)
{
    lv_fs_file_t f;
    uint32_t data_size;
    uint32_t rn;
    lv_fs_res_t res;
    void * data = NULL;

    res = lv_fs_open(&f, filename, LV_FS_MODE_RD);
    if(res != LV_FS_RES_OK) {
        LV_LOG_WARN("can't open %s", filename);
        return NULL;
    }

    res = lv_fs_seek(&f, 0, LV_FS_SEEK_END);
    if(res != LV_FS_RES_OK) {
        goto failed;
    }

    res = lv_fs_tell(&f, &data_size);
    if(res != LV_FS_RES_OK) {
        goto failed;
    }

    res = lv_fs_seek(&f, offset, LV_FS_SEEK_SET);
    if(res != LV_FS_RES_OK) {
        goto failed;
    }

    data_size -= offset;

    /*Allocate raw buffer*/
    data = lv_gpu_aligned_alloc(VG_LITE_IMG_SRC_ADDR_ALIGN, sizeof(vg_lite_img_header_t) + data_size);
    LV_ASSERT_MALLOC(data);
    if(data == NULL) {
        LV_GPU_LOG_ERROR("malloc failed for data");
        goto failed;
    }

    res = lv_fs_read(&f, data + sizeof(vg_lite_img_header_t), data_size, &rn);

    if(res == LV_FS_RES_OK && rn == data_size) {
        if(size != NULL) {
            *size = rn;
        }
    }
    else {
        LV_LOG_WARN("read file failed");
        lv_gpu_free(data);
        data = NULL;
    }

failed:
    lv_fs_close(&f);

    return data;
}


lv_img_dsc_t * vg_lite_img_dsc_create(lv_coord_t w, lv_coord_t h, lv_img_cf_t cf)
{
    /*Allocate image descriptor*/
    lv_img_dsc_t * dsc = lv_gpu_malloc(sizeof(lv_img_dsc_t));
    LV_ASSERT_MALLOC(dsc);
    lv_memset_00(dsc, sizeof(lv_img_dsc_t));

    /*Allocate raw buffer*/
    dsc->data = vg_lite_img_alloc(w, h, cf, &dsc->data_size);
    if(dsc->data == NULL) {
        lv_gpu_free(dsc);
        return NULL;
    }

    ((vg_lite_img_header_t *)dsc->data)->magic = 0;

    /*Fill in header*/
    dsc->header.always_zero = 0;
    dsc->header.w = w;
    dsc->header.h = h;
    dsc->header.cf = cf;
    return dsc;
}

void vg_lite_img_dsc_del(lv_img_dsc_t * img_dsc)
{
    LV_ASSERT_NULL(img_dsc);
    if(img_dsc->data) {
        vg_lite_img_free((void *)img_dsc->data);
        img_dsc->data = NULL;
    }
    lv_gpu_free(img_dsc);
}

void vg_lite_img_dsc_update_header(lv_img_dsc_t * img_dsc)
{
    LV_ASSERT_NULL(img_dsc);
    vg_lite_img_header_t * header = (vg_lite_img_header_t *)img_dsc->data;
    LV_ASSERT_NULL(header);

    if(header->magic == VG_LITE_IMAGE_MAGIC_NUM) {
        LV_GPU_LOG_TRACE("has magic num");
        header->vg_buf.address = (uint32_t)img_dsc->data + sizeof(vg_lite_img_header_t);
        header->vg_buf.memory = (void *)header->vg_buf.address;
    }
    else {
        header->magic = VG_LITE_IMAGE_MAGIC_NUM;
        lv_coord_t w = VG_LITE_ALIGN(img_dsc->header.w, VG_LITE_IMG_SRC_PX_ALIGN);
        vg_lite_custom_buffer_init(
            &header->vg_buf,
            (void *)(img_dsc->data + sizeof(vg_lite_img_header_t)),
            w,
            img_dsc->header.h,
            vg_lite_img_cf_to_vg_fmt(img_dsc->header.cf));
        LV_GPU_LOG_TRACE("update magic num");
    }
    VG_LITE_DUMP_BUFFER_INFO(&header->vg_buf);
}

uint32_t vg_lite_get_palette_size(vg_lite_buffer_format_t format)
{
    uint32_t size = 0;
    switch(format) {
        case VG_LITE_INDEX_1:
            size = 1 << 1;
            break;
        case VG_LITE_INDEX_2:
            size = 1 << 2;
            break;
        case VG_LITE_INDEX_4:
        case VG_LITE_A4:
            size = 1 << 4;
            break;
        case VG_LITE_INDEX_8:
        case VG_LITE_A8:
            size = 1 << 8;
            break;
        default:
            LV_GPU_LOG_WARN("unsupport format: %d(%s)",
                            format,
                            vg_lite_get_buffer_format_string(format));
            break;
    }
    return size;
}

vg_lite_blend_t vg_lite_lv_blend_mode_to_vg_blend_mode(lv_blend_mode_t mode)
{
    vg_lite_blend_t blend;
    switch(mode) {
#ifdef CONFIG_LV_GPU_VG_LITE_LVGL_BLEND_SUPPORT
        case LV_BLEND_MODE_NORMAL: /**< Simply mix according to the opacity value*/
            blend = VG_LITE_BLEND_NORMAL_LVGL;
            break;
        case LV_BLEND_MODE_ADDITIVE: /**< Add the respective color channels*/
            blend = VG_LITE_BLEND_ADDITIVE_LVGL;
            break;
        case LV_BLEND_MODE_SUBTRACTIVE: /**< Subtract the foreground from the background*/
            blend = VG_LITE_BLEND_SUBTRACT_LVGL;
            break;
        case LV_BLEND_MODE_MULTIPLY: /**< Multiply the foreground and background*/
            blend = VG_LITE_BLEND_MULTIPLY_LVGL;
            break;
#else
        case LV_BLEND_MODE_NORMAL: /**< Simply mix according to the opacity value*/
            blend = VG_LITE_BLEND_SRC_OVER;
            break;
        case LV_BLEND_MODE_ADDITIVE: /**< Add the respective color channels*/
            blend = VG_LITE_BLEND_ADDITIVE;
            break;
        case LV_BLEND_MODE_SUBTRACTIVE: /**< Subtract the foreground from the background*/
            blend = VG_LITE_BLEND_SUBTRACT;
            break;
        case LV_BLEND_MODE_MULTIPLY: /**< Multiply the foreground and background*/
            blend = VG_LITE_BLEND_MULTIPLY;
            break;
#endif
        default:
            LV_GPU_LOG_WARN("Unsupport blend mode: %d", mode);
            blend = VG_LITE_BLEND_NONE;
            break;
    }
    return blend;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /* CONFIG_LV_GPU_USE_VG_LITE */
