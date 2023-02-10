/**
 * @file sw_conv_imp.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "sw_conv_imp.h"
#include "../lv_gpu_conv.h"
#include "../lv_gpu_utils.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static lv_gpu_conv_res_t convert_pre_multiply(void * dsc);
static lv_gpu_conv_res_t convert_recolor_palette(void * dsc);
static lv_gpu_conv_res_t convert_bgra5658_to_bgra8888(void * dsc);
static lv_gpu_conv_res_t convert_indexed8_to_bgra8888(void * dsc);
static lv_gpu_conv_res_t convert_bgr565_to_vgimg(void * dsc);
static lv_gpu_conv_res_t convert_bgr888_to_vgimg(void * dsc);
static lv_gpu_conv_res_t convert_bgra8888_to_vgimg(void * dsc);
static lv_gpu_conv_res_t convert_gaussian_blur(void * dsc);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void sw_conv_init(void)
{
    lv_gpu_conv_set_callback(LV_GPU_CONV_TYPE_PRE_MULTIPLY, convert_pre_multiply);
    lv_gpu_conv_set_callback(LV_GPU_CONV_TYPE_RECOLOR_PALETTE, convert_recolor_palette);
    lv_gpu_conv_set_callback(LV_GPU_CONV_TYPE_BGRA5658_TO_BGRA8888, convert_bgra5658_to_bgra8888);
    lv_gpu_conv_set_callback(LV_GPU_CONV_TYPE_INDEXED8_TO_BGRA8888, convert_indexed8_to_bgra8888);
    lv_gpu_conv_set_callback(LV_GPU_CONV_TYPE_BGR565_TO_VGIMG, convert_bgr565_to_vgimg);
    lv_gpu_conv_set_callback(LV_GPU_CONV_TYPE_BGR888_TO_VGIMG, convert_bgr888_to_vgimg);
    lv_gpu_conv_set_callback(LV_GPU_CONV_TYPE_BGRA8888_TO_VGIMG, convert_bgra8888_to_vgimg);
    lv_gpu_conv_set_callback(LV_GPU_CONV_TYPE_GAUSSIAN_BLUR, convert_gaussian_blur);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_gpu_conv_res_t convert_pre_multiply(void * dsc)
{
    lv_gpu_conv_pre_multiply_dsc_t * pre_multiply_dsc = dsc;

    lv_color32_t * dst = pre_multiply_dsc->dst;
    const lv_color32_t * src = pre_multiply_dsc->src;
    uint32_t count = pre_multiply_dsc->count;

    while(count--) {
        dst->ch.red = LV_UDIV255(src->ch.red * src->ch.alpha);
        dst->ch.green = LV_UDIV255(src->ch.green * src->ch.alpha);
        dst->ch.blue = LV_UDIV255(src->ch.blue * src->ch.alpha);
        dst->ch.alpha = src->ch.alpha;
        dst++;
        src++;
    }

    return LV_GPU_CONV_RES_OK;
}

static lv_gpu_conv_res_t convert_recolor_palette(void * dsc)
{
    lv_gpu_conv_recolor_palette_dsc_t * recolor_palette_dsc = dsc;

    lv_color32_t * dst = recolor_palette_dsc->dst;
    const lv_color32_t * src = recolor_palette_dsc->src;
    uint16_t size = recolor_palette_dsc->size;
    uint32_t recolor = recolor_palette_dsc->recolor;

    LV_ASSERT_NULL(dst);

    lv_gpu_conv_pre_multiply_dsc_t pre_multiply_dsc;
    lv_memset_00(&pre_multiply_dsc, sizeof(pre_multiply_dsc));

    lv_opa_t opa = recolor >> 24;
    if(opa == LV_OPA_TRANSP) {
        if(!src) {
            LV_GPU_LOG_WARN("alpha recolor unspecified, default to black");
            recolor = 0xFFFFFFFF;
        }
        else {
            pre_multiply_dsc.dst = dst;
            pre_multiply_dsc.src = src;
            pre_multiply_dsc.count = size;
            return lv_gpu_conv_start(LV_GPU_CONV_TYPE_PRE_MULTIPLY, &pre_multiply_dsc);
        }
    }

    uint16_t recolor_premult[3] = {
        (recolor >> 16 & 0xFF) * opa,
        (recolor >> 8 & 0xFF) * opa,
        (recolor & 0xFF) * opa
    };

    for(int i = 0; i < size; i++) {
        if(src != NULL) {
            lv_opa_t mix = 255 - opa;
            /* index recolor */
            if(src[i].ch.alpha == 0) {
                dst[i].full = 0;
            }
            else {
                LV_COLOR_SET_R32(dst[i],
                                 LV_UDIV255(recolor_premult[0] + LV_COLOR_GET_R32(src[i]) * mix));
                LV_COLOR_SET_G32(dst[i],
                                 LV_UDIV255(recolor_premult[1] + LV_COLOR_GET_G32(src[i]) * mix));
                LV_COLOR_SET_B32(dst[i],
                                 LV_UDIV255(recolor_premult[2] + LV_COLOR_GET_B32(src[i]) * mix));
                LV_COLOR_SET_A32(dst[i], src[i].ch.alpha);
            }
        }
        else {
            /* fill alpha palette */
            uint32_t opa_i = (size == 256) ? i : i * 0x11;
            LV_COLOR_SET_R32(dst[i], LV_UDIV255(recolor_premult[0]));
            LV_COLOR_SET_G32(dst[i], LV_UDIV255(recolor_premult[1]));
            LV_COLOR_SET_B32(dst[i], LV_UDIV255(recolor_premult[2]));
            LV_COLOR_SET_A32(dst[i], opa_i);
        }
    }

    pre_multiply_dsc.dst = dst;
    pre_multiply_dsc.src = dst;
    pre_multiply_dsc.count = size;
    return lv_gpu_conv_start(LV_GPU_CONV_TYPE_PRE_MULTIPLY, &pre_multiply_dsc);
}

static lv_gpu_conv_res_t convert_bgra5658_to_bgra8888(void * dsc)
{
    lv_gpu_conv_bgra5658_to_bgra8888_dsc_t * conv_dsc = dsc;

    uint8_t * px_buf = conv_dsc->base.px_buf;
    uint32_t buf_stride = conv_dsc->base.buf_stride;
    const uint8_t * px_map = conv_dsc->base.px_map;
    uint32_t map_stride = conv_dsc->base.map_stride;
    const lv_img_header_t * header = conv_dsc->base.header;
    lv_color32_t recolor = conv_dsc->recolor;

    LV_UNUSED(map_stride);

    lv_opa_t opa = recolor.ch.alpha;
    lv_opa_t mix = 255 - opa;

    uint32_t recolor_premult[3];

    if(opa != LV_OPA_TRANSP) {
        recolor_premult[0] = recolor.ch.red * opa;
        recolor_premult[1] = recolor.ch.green * opa;
        recolor_premult[2] = recolor.ch.blue * opa;
    }

    for(int_fast16_t i = 0; i < header->h; i++) {
        for(int_fast16_t j = 0; j < header->w; j++) {
            lv_color32_t * c32 = (lv_color32_t *)px_buf + j;
            lv_color16_t * c16 = (lv_color16_t *)px_map;
            c32->ch.alpha = px_map[LV_IMG_PX_SIZE_ALPHA_BYTE - 1];
            c32->ch.red = (c16->ch.red * 263 + 7) * c32->ch.alpha >> 13;
            c32->ch.green = (c16->ch.green * 259 + 3) * c32->ch.alpha >> 14;
            c32->ch.blue = (c16->ch.blue * 263 + 7) * c32->ch.alpha >> 13;
            if(opa != LV_OPA_TRANSP) {
                c32->ch.red = LV_UDIV255(c32->ch.red * mix + recolor_premult[0]);
                c32->ch.green = LV_UDIV255(c32->ch.green * mix + recolor_premult[1]);
                c32->ch.blue = LV_UDIV255(c32->ch.blue * mix + recolor_premult[2]);
            }
            px_map += LV_IMG_PX_SIZE_ALPHA_BYTE;
        }
        lv_memset_00(px_buf + (header->w << 2), buf_stride - (header->w << 2));
        px_buf += buf_stride;
    }

    return LV_GPU_CONV_RES_OK;
}

static lv_gpu_conv_res_t convert_indexed8_to_bgra8888(void * dsc)
{
    lv_gpu_conv_indexed8_to_bgra8888_dsc_t * conv_dsc = dsc;

    uint8_t * px_buf = conv_dsc->base.px_buf;
    uint32_t buf_stride = conv_dsc->base.buf_stride;
    const uint8_t * px_map = conv_dsc->base.px_map;
    uint32_t map_stride = conv_dsc->base.map_stride;
    const lv_img_header_t * header = conv_dsc->base.header;
    const uint32_t * palette = conv_dsc->palette;

    for(int i = 0; i < header->h; i++) {
        uint32_t * dst = (uint32_t *)px_buf;
        for(int j = 0; j < header->w; j++) {
            dst[j] = palette[px_map[j]];
        }
        lv_memset_00(px_buf + (map_stride << 2), buf_stride - (map_stride << 2));
        px_map += map_stride;
        px_buf += buf_stride;
    }

    return LV_GPU_CONV_RES_OK;
}

static lv_gpu_conv_res_t convert_bgr565_to_vgimg(void * dsc)
{
    lv_gpu_conv_bgr565_to_vgimg_dsc_t * conv_dsc = dsc;

    uint8_t * px_buf = conv_dsc->base.px_buf;
    uint32_t buf_stride = conv_dsc->base.buf_stride;
    const uint8_t * px_map = conv_dsc->base.px_map;
    uint32_t map_stride = conv_dsc->base.map_stride;
    const lv_img_header_t * header = conv_dsc->base.header;
    lv_color32_t recolor = conv_dsc->recolor;
    uint32_t ckey = conv_dsc->ckey;

    lv_opa_t opa = recolor.ch.alpha;
    lv_opa_t mix = 255 - opa;
    uint32_t recolor_premult[3] = {
        recolor.ch.red * opa,
        recolor.ch.green * opa,
        recolor.ch.blue * opa
    };

    if(ckey == 0) {
        if(opa == LV_OPA_TRANSP) {
            if(map_stride == buf_stride) {
                lv_memcpy(px_buf, px_map, buf_stride * header->h);
            }
            else {
                for(int_fast16_t i = 0; i < header->h; i++) {
                    lv_memcpy(px_buf, px_map, map_stride);
                    lv_memset_00(px_buf + map_stride, buf_stride - map_stride);
                    px_map += map_stride;
                    px_buf += buf_stride;
                }
            }
        }
        else {
            for(int_fast16_t i = 0; i < header->h; i++) {
                const lv_color_t * src = (const lv_color_t *)px_map;
                lv_color_t * dst = (lv_color_t *)px_buf;
                for(int_fast16_t j = 0; j < header->w; j++) {
                    dst[j].ch.red = LV_UDIV255(src[j].ch.red * mix + (recolor_premult[0] >> 3));
                    dst[j].ch.green = LV_UDIV255(src[j].ch.green * mix + (recolor_premult[1] >> 2));
                    dst[j].ch.blue = LV_UDIV255(src[j].ch.blue * mix + (recolor_premult[2] >> 3));
                }
                uint32_t bytes_done = header->w * sizeof(lv_color_t);
                lv_memset_00(px_buf + bytes_done, buf_stride - bytes_done);
                px_map += map_stride;
                px_buf += buf_stride;
            }
        }

        return LV_GPU_CONV_RES_OK;
    }

    if(opa == LV_OPA_TRANSP) {
        /* chroma keyed, target is argb32 */
        for(int_fast16_t i = 0; i < header->h; i++) {
            const lv_color_t * src = (const lv_color_t *)px_map;
            lv_color32_t * dst = (lv_color32_t *)px_buf;
            for(int_fast16_t j = 0; j < header->w; j++) {
                dst[j].full = (src[j].full == ckey) ? 0 : lv_color_to32(src[j]);
            }
            uint32_t bytes_done = header->w * sizeof(lv_color32_t);
            lv_memset_00(px_buf + bytes_done, buf_stride - bytes_done);
            px_map += map_stride;
            px_buf += buf_stride;
        }
        return LV_GPU_CONV_RES_OK;
    }

    for(int_fast16_t i = 0; i < header->h; i++) {
        const lv_color_t * src = (const lv_color_t *)px_map;
        lv_color32_t * dst = (lv_color32_t *)px_buf;
        for(int_fast16_t j = 0; j < header->w; j++) {
            if(src[j].full == ckey) {
                dst[j].full = 0;
            }
            else {
                dst[j].ch.alpha = 0xFF;
                dst[j].ch.red = LV_UDIV255(((src[j].ch.red * 263 + 7) >> 5) * mix + recolor_premult[0]);
                dst[j].ch.green = LV_UDIV255(((src[j].ch.green * 259 + 3) >> 6) * mix + recolor_premult[1]);
                dst[j].ch.blue = LV_UDIV255(((src[j].ch.blue * 263 + 7) >> 5) * mix + recolor_premult[2]);
            }
        }
        uint32_t bytes_done = header->w * sizeof(lv_color32_t);
        lv_memset_00(px_buf + bytes_done, buf_stride - bytes_done);
        px_map += map_stride;
        px_buf += buf_stride;
    }

    return LV_GPU_CONV_RES_OK;
}

static lv_gpu_conv_res_t convert_bgr888_to_vgimg(void * dsc)
{
    lv_gpu_conv_bgr888_to_vgimg_dsc_t * conv_dsc = dsc;

    uint8_t * px_buf = conv_dsc->base.px_buf;
    uint32_t buf_stride = conv_dsc->base.buf_stride;
    const uint8_t * px_map = conv_dsc->base.px_map;
    uint32_t map_stride = conv_dsc->base.map_stride;
    const lv_img_header_t * header = conv_dsc->base.header;
    lv_color32_t recolor = conv_dsc->recolor;
    uint32_t ckey = conv_dsc->ckey;

    lv_opa_t opa = recolor.ch.alpha;
    lv_opa_t mix = 255 - opa;

    uint32_t recolor_premult[3] = {
        recolor.ch.red * opa,
        recolor.ch.green * opa,
        recolor.ch.blue * opa
    };

    for(int_fast16_t i = 0; i < header->h; i++) {
        const lv_color32_t * src = (const lv_color32_t *)px_map;
        lv_color32_t * dst = (lv_color32_t *)px_buf;
        for(int_fast16_t j = 0; j < header->w; j++) {
            if(src[j].full == ckey) {
                dst[j].full = 0;
            }
            else {
                dst[j].ch.alpha = 0xFF;
                dst[j].ch.red = LV_UDIV255(src[j].ch.red * mix + recolor_premult[0]);
                dst[j].ch.green = LV_UDIV255(src[j].ch.green * mix + recolor_premult[1]);
                dst[j].ch.blue = LV_UDIV255(src[j].ch.blue * mix + recolor_premult[2]);
            }
        }
        lv_memset_00(px_buf + map_stride, buf_stride - map_stride);
        px_map += map_stride;
        px_buf += buf_stride;
    }

    return LV_GPU_CONV_RES_OK;
}

static lv_gpu_conv_res_t convert_bgra8888_to_vgimg(void * dsc)
{
    lv_gpu_conv_bgra8888_to_vgimg_dsc_t * conv_dsc = dsc;

    uint8_t * px_buf = conv_dsc->base.px_buf;
    uint32_t buf_stride = conv_dsc->base.buf_stride;
    const uint8_t * px_map = conv_dsc->base.px_map;
    uint32_t map_stride = conv_dsc->base.map_stride;
    const lv_img_header_t * header = conv_dsc->base.header;
    lv_color32_t recolor = conv_dsc->recolor;
    bool preprocessed = conv_dsc->preprocessed;

    lv_opa_t opa = recolor.ch.alpha;
    lv_opa_t mix = 255 - opa;
    uint32_t recolor_premult[3];

    if(opa != LV_OPA_TRANSP) {
        recolor_premult[0] = recolor.ch.red * opa;
        recolor_premult[1] = recolor.ch.green * opa;
        recolor_premult[2] = recolor.ch.blue * opa;
    }

    if(!preprocessed) {
        if(opa != LV_OPA_TRANSP) {
            for(int_fast16_t i = 0; i < header->h; i++) {
                const lv_color32_t * src = (const lv_color32_t *)px_map;
                lv_color32_t * dst = (lv_color32_t *)px_buf;
                for(int_fast16_t j = 0; j < header->w; j++) {
                    if(src[j].ch.alpha == 0) {
                        dst[j].full = 0;
                        continue;
                    }
                    dst[j].ch.alpha = src[j].ch.alpha;
                    dst[j].ch.red = LV_UDIV255(src[j].ch.red * mix + recolor_premult[0]);
                    dst[j].ch.green = LV_UDIV255(src[j].ch.green * mix + recolor_premult[1]);
                    dst[j].ch.blue = LV_UDIV255(src[j].ch.blue * mix + recolor_premult[2]);
                    if(src[j].ch.alpha != 0xFF) {
                        dst[j].ch.red = LV_UDIV255(dst[j].ch.red * src[j].ch.alpha);
                        dst[j].ch.green = LV_UDIV255(dst[j].ch.green * src[j].ch.alpha);
                        dst[j].ch.blue = LV_UDIV255(dst[j].ch.blue * src[j].ch.alpha);
                    }
                }
                lv_memset_00(px_buf + map_stride, buf_stride - map_stride);
                px_map += map_stride;
                px_buf += buf_stride;
            }
        }
        else {
            for(int_fast16_t i = 0; i < header->h; i++) {
                const lv_color32_t * src = (const lv_color32_t *)px_map;
                lv_color32_t * dst = (lv_color32_t *)px_buf;
                for(int_fast16_t j = 0; j < header->w; j++) {
                    if(src[j].ch.alpha == 0) {
                        dst[j].full = 0;
                    }
                    else if(src[j].ch.alpha < 0xFF) {
                        dst[j].ch.alpha = src[j].ch.alpha;
                        dst[j].ch.red = LV_UDIV255(src[j].ch.red * src[j].ch.alpha);
                        dst[j].ch.green = LV_UDIV255(src[j].ch.green * src[j].ch.alpha);
                        dst[j].ch.blue = LV_UDIV255(src[j].ch.blue * src[j].ch.alpha);
                    }
                    else {
                        dst[j] = src[j];
                    }
                }
                lv_memset_00(px_buf + map_stride, buf_stride - map_stride);
                px_map += map_stride;
                px_buf += buf_stride;
            }
        }

        return LV_GPU_CONV_RES_OK;
    }

    if(opa != LV_OPA_TRANSP) {
        /* have been pre-multiplied */
        for(int_fast16_t i = 0; i < header->h; i++) {
            const lv_color32_t * src = (const lv_color32_t *)px_map;
            lv_color32_t * dst = (lv_color32_t *)px_buf;
            for(int_fast16_t j = 0; j < header->w; j++) {
                if(src[j].ch.alpha == 0) {
                    dst[j].full = 0;
                    continue;
                }
                dst[j].ch.alpha = src[j].ch.alpha;
                if(src[j].ch.alpha < 0xFF) {
                    dst[j].ch.red = LV_UDIV255(src[j].ch.red * mix + LV_UDIV255(recolor_premult[0]) * src[j].ch.alpha);
                    dst[j].ch.green = LV_UDIV255(src[j].ch.green * mix + LV_UDIV255(recolor_premult[1]) * src[j].ch.alpha);
                    dst[j].ch.blue = LV_UDIV255(src[j].ch.blue * mix + LV_UDIV255(recolor_premult[2]) * src[j].ch.alpha);
                }
                else {
                    dst[j].ch.red = LV_UDIV255(src[j].ch.red * mix + recolor_premult[0]);
                    dst[j].ch.green = LV_UDIV255(src[j].ch.green * mix + recolor_premult[1]);
                    dst[j].ch.blue = LV_UDIV255(src[j].ch.blue * mix + recolor_premult[2]);
                }
            }
            lv_memset_00(px_buf + map_stride, buf_stride - map_stride);
            px_map += map_stride;
            px_buf += buf_stride;
        }
        return LV_GPU_CONV_RES_OK;
    }

    if(map_stride != buf_stride) {
        for(int_fast16_t i = 0; i < header->h; i++) {
            lv_memcpy(px_buf, px_map, header->w << 2);
            lv_memset_00(px_buf + map_stride, buf_stride - map_stride);
            px_map += map_stride;
            px_buf += buf_stride;
        }

        return LV_GPU_CONV_RES_OK;
    }

    lv_memcpy(px_buf, px_map, header->h * map_stride);

    return LV_GPU_CONV_RES_OK;
}

static lv_gpu_conv_res_t convert_gaussian_blur(void * dsc)
{
    lv_gpu_conv_gaussian_blur_dsc_t * conv_dsc = dsc;

    lv_color_t * dst = conv_dsc->dst;
    uint32_t dst_stride = conv_dsc->dst_stride;
    lv_color_t * src = conv_dsc->src;
    uint32_t src_stride = conv_dsc->src_stride;
    const lv_area_t * blur_area = conv_dsc->blur_area;
    int r = conv_dsc->r;

    lv_coord_t blur_w = lv_area_get_width(blur_area);
    lv_coord_t blur_h = lv_area_get_height(blur_area);
    int d = r << 1 | 1;

    if(d > blur_w || d > blur_h) {
        r = (LV_MIN(blur_w, blur_h) - 1) >> 1;
    }

    if(r <= 0) {
        return LV_GPU_CONV_RES_OK;
    }

    src += blur_area->y1 * src_stride + blur_area->x1;
    if(!dst) {
        dst = src;
        dst_stride = src_stride;
    }

    lv_color_t * tmp = lv_mem_buf_get(sizeof(lv_color_t) * (blur_w + 2));
    LV_ASSERT_MALLOC(tmp);
    if(tmp == NULL) {
        return LV_GPU_CONV_RES_OUT_OF_MEMORY;
    }

    /* box_blur_3 */
    lv_gpu_conv_gaussian_dir_blur_dsc_t blur_dsc;
    lv_memset_00(&blur_dsc, sizeof(blur_dsc));
    blur_dsc.dst = dst;
    blur_dsc.src = src;
    blur_dsc.dst_stride = dst_stride;
    blur_dsc.src_stride = src_stride;
    blur_dsc.w = blur_w;
    blur_dsc.h = blur_h;
    blur_dsc.r = r;
    blur_dsc.tmp = tmp;

    lv_gpu_conv_start(LV_GPU_CONV_TYPE_GAUSSIAN_HOR_BLUR, &blur_dsc);
    blur_dsc.src = dst;
    lv_gpu_conv_start(LV_GPU_CONV_TYPE_GAUSSIAN_HOR_BLUR, &blur_dsc);
    lv_gpu_conv_start(LV_GPU_CONV_TYPE_GAUSSIAN_HOR_BLUR, &blur_dsc);
    lv_gpu_conv_start(LV_GPU_CONV_TYPE_GAUSSIAN_VER_BLUR, &blur_dsc);
    lv_gpu_conv_start(LV_GPU_CONV_TYPE_GAUSSIAN_VER_BLUR, &blur_dsc);
    lv_gpu_conv_start(LV_GPU_CONV_TYPE_GAUSSIAN_VER_BLUR, &blur_dsc);

    lv_mem_buf_release(tmp);
    return LV_GPU_CONV_RES_OK;
}
