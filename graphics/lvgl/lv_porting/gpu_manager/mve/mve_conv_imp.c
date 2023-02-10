/**
 * @file mve_conv_imp.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include <nuttx/config.h>

#ifdef CONFIG_ARM_HAVE_MVE

#include "../lv_gpu_conv.h"
#include "../lv_gpu_utils.h"
#include "mve_conv_imp.h"
#include <arm_mve.h>

/*********************
 *      DEFINES
 *********************/

#define ALIGN_UP(num, align) (((num) + ((align)-1)) & ~((align)-1))

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
static lv_gpu_conv_res_t convert_bgr888_to_vgimg(void * dsc);
static lv_gpu_conv_res_t convert_bgra8888_to_vgimg(void * dsc);

#if (LV_COLOR_DEPTH == 32)
    static lv_gpu_conv_res_t convert_gaussian_hor_blur(void * dsc);
    static lv_gpu_conv_res_t convert_gaussian_ver_blur(void * dsc);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void mve_conv_init(void)
{
    lv_gpu_conv_set_callback(LV_GPU_CONV_TYPE_PRE_MULTIPLY, convert_pre_multiply);
    lv_gpu_conv_set_callback(LV_GPU_CONV_TYPE_RECOLOR_PALETTE, convert_recolor_palette);
    lv_gpu_conv_set_callback(LV_GPU_CONV_TYPE_BGRA5658_TO_BGRA8888, convert_bgra5658_to_bgra8888);
    lv_gpu_conv_set_callback(LV_GPU_CONV_TYPE_INDEXED8_TO_BGRA8888, convert_indexed8_to_bgra8888);
    lv_gpu_conv_set_callback(LV_GPU_CONV_TYPE_BGR888_TO_VGIMG, convert_bgr888_to_vgimg);
    lv_gpu_conv_set_callback(LV_GPU_CONV_TYPE_BGRA8888_TO_VGIMG, convert_bgra8888_to_vgimg);
#if (LV_COLOR_DEPTH == 32)
    lv_gpu_conv_set_callback(LV_GPU_CONV_TYPE_GAUSSIAN_HOR_BLUR, convert_gaussian_hor_blur);
    lv_gpu_conv_set_callback(LV_GPU_CONV_TYPE_GAUSSIAN_VER_BLUR, convert_gaussian_ver_blur);
#endif
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

    __asm volatile(
        "   .p2align 2                                                  \n"
        "   wlstp.8                 lr, %[loopCnt], 1f                  \n"
        "   2:                                                          \n"
        "   vldrb.8                 q0, [%[pSource]], #16               \n"
        "   vsri.32                 q1, q0, #8                          \n"
        "   vsri.32                 q1, q0, #16                         \n"
        "   vsri.32                 q1, q0, #24                         \n"
        /* pre-multiply alpha to all channels */
        "   vrmulh.u8               q0, q0, q1                          \n"
        "   vsli.32                 q0, q1, #24                         \n"
        "   vstrb.8                 q0, [%[pTarget]], #16               \n"
        "   letp                    lr, 2b                              \n"
        "   1:                                                          \n"
        : [pSource] "+r"(src), [pTarget] "+r"(dst)
        : [loopCnt] "r"(count << 2)
        : "q0", "q1", "lr", "memory");

    return LV_GPU_CONV_RES_OK;
}

static lv_gpu_conv_res_t convert_recolor_palette(void * dsc)
{
    lv_gpu_conv_recolor_palette_dsc_t * recolor_palette_dsc = dsc;

    lv_color32_t * dst = recolor_palette_dsc->dst;
    const lv_color32_t * src = recolor_palette_dsc->src;
    uint16_t size = recolor_palette_dsc->size;
    uint32_t recolor = recolor_palette_dsc->recolor;

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

    int32_t blkCnt = size << 2;
    uint32_t * pwTarget = (uint32_t *)dst;
    uint32_t * phwSource = (uint32_t *)src;
    if(src != NULL) {
        __asm volatile(
            "   .p2align 2                                                  \n"
            "   vdup.32                 q0, %[pRecolor]                     \n"
            "   vdup.8                  q1, %[opa]                          \n"
            "   vrmulh.u8               q0, q0, q1                          \n"
            "   vmvn                    q1, q1                              \n"
            "   wlstp.8                 lr, %[loopCnt], 1f                  \n"
            "   2:                                                          \n"
            "   vldrb.8                 q2, [%[pSource]], #16               \n"
            "   vsri.32                 q3, q2, #8                          \n"
            "   vsri.32                 q3, q2, #16                         \n"
            "   vsri.32                 q3, q2, #24                         \n"
            "   vrmulh.u8               q2, q2, q1                          \n"
            "   vadd.i8                 q2, q2, q0                          \n"
            /* pre-multiply */
            "   vrmulh.u8               q2, q2, q3                          \n"
            "   vsli.32                 q2, q3, #24                         \n"
            "   vstrb.8                 q2, [%[pTarget]], #16               \n"
            "   letp                    lr, 2b                              \n"
            "   1:                                                          \n"
            : [pSource] "+r"(phwSource), [pTarget] "+r"(pwTarget),
            [pRecolor] "+r"(recolor)
            : [loopCnt] "r"(blkCnt), [opa] "r"(opa)
            : "q0", "q1", "q2", "q3", "lr", "memory");
    }
    else {
        __asm volatile(
            "   .p2align 2                                                  \n"
            "   vdup.32                 q0, %[pSource]                      \n"
            "   vdup.8                  q1, %[opa]                          \n"
            "   vrmulh.u8               q0, q0, q1                          \n"
            "   mov                     r0, #0                              \n"
            "   vidup.u32               q1, r0, #1                          \n"
            "   cmp                     %[size], #16                        \n"
            "   itte                    eq                                  \n"
            "   moveq                   r1, #0x11                           \n"
            "   moveq                   r0, #0x44                           \n"
            "   movne                   r1, #0x1                            \n"
            "   vsli.32                 q1, q1, #8                          \n"
            "   vsli.32                 q1, q1, #16                         \n"
            "   vmul.i8                 q1, q1, r1                          \n"
            "   1:                                                          \n"
            "   wlstp.8                 lr, %[loopCnt], 3f                  \n"
            "   2:                                                          \n"
            "   vrmulh.u8               q2, q0, q1                          \n"
            "   vsli.32                 q2, q1, #24                         \n"
            "   vstrb.8                 q2, [%[pTarget]], #16               \n"
            "   vadd.i8                 q1, q1, r0                          \n"
            "   letp                    lr, 2b                              \n"
            "   3:                                                          \n"
            : [pSource] "+r"(recolor), [pTarget] "+r"(pwTarget)
            : [loopCnt] "r"(blkCnt), [opa] "r"(opa), [size] "r"(size)
            : "r0", "r1", "q0", "q1", "q2", "lr", "memory");
    }

    return LV_GPU_CONV_RES_OK;
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

    lv_opa_t opa = recolor.ch.alpha;
    lv_opa_t mix = 255 - opa;

    uint32_t ww = ALIGN_UP(header->w, 4);
    int32_t h = header->h;
    int32_t map_offset = map_stride - ww * 3;
    int32_t buf_offset = buf_stride - ww * 4;
    lv_color32_t recolor_premult = {
        .ch.alpha = opa,
        .ch.red = LV_UDIV255(opa * recolor.ch.red),
        .ch.green = LV_UDIV255(opa * recolor.ch.green),
        .ch.blue = LV_UDIV255(opa * recolor.ch.blue)
    };
    __asm volatile(
        "   .p2align 2                                                  \n"
        "   movs                    r0, #24                             \n"
        "   vddup.u32               q0, r0, #8                          \n" /* q0 = [24 16 8 0] */
        "   movs                    r0, #0                              \n"
        "   vidup.u32               q4, r0, #8                          \n" /* q4 = [0 8 16 24] */
        "   movs                    r0, #0xFF                           \n"
        "   movs                    r1, #0xF8                           \n"
        "   vdup.32                 q5, r0                              \n"
        "   vdup.32                 q6, r1                              \n"
        "   movs                    r0, #0xFC                           \n"
        "   vshl.u32                q5, q5, q0                          \n" /* q5 = [0xFF000000 0xFF0000 0xFF00 0xFF] */
        "   vdup.32                 q7, r0                              \n"
        "   vshl.u32                q6, q6, q0                          \n" /* q6 = [0xF8000000 0xF80000 0xF800 0xF8] */
        "   vshl.u32                q7, q7, q0                          \n" /* q7 = [0xFC000000 0xFC0000 0xFC00 0xFC] */
        "   movs                    r1, %[opa]                          \n"
        "   1:                                                          \n"
        "   wlstp.8                 lr, %[w], 4f                        \n"
        "   2:                                                          \n"
        "   vldrb.8                 q0, [%[pSource]], #12               \n" /* q0 = |****|AQPA|QPAQ|PAQP| */
        "   vshlc                   q0, r0, #8                          \n" /* q0 = |***A|QPAQ|PAQP|AQP0| */
        "   vand                    q2, q0, q5                          \n" /* q2 = |000A|00A0|0A00|A000| */
        "   vshl.u32                q1, q2, q4                          \n" /* q1 = |A000|A000|A000|A000| */
        "   vshlc                   q0, r0, #8                          \n" /* q0 = |**AQ|PAQP|AQPA|QP**| */
        "   vand                    q2, q0, q6                          \n" /* q2 = |000r|00r0|0r00|r000| */
        "   vshl.u32                q3, q2, q4                          \n" /* q3 = |r000|r000|r000|r000| */
        "   vsri.32                 q1, q3, #8                          \n" /* q1 = |Ar00|Ar00|Ar00|Ar00| */
        "   vsri.32                 q1, q3, #13                         \n" /* q1 = |AR*0|AR*0|AR*0|AR*0| */
        "   vshlc                   q0, r0, #5                          \n" /* Similar operation on G channel */
        "   vand                    q2, q0, q7                          \n" /* q2 = |000g|00g0|0g00|g000| */
        "   vshl.u32                q3, q2, q4                          \n" /* q3 = |g000|g000|g000|g000| */
        "   vsri.32                 q1, q3, #16                         \n" /* q1 = |ARg0|ARg0|ARg0|ARg0| */
        "   vsri.32                 q1, q3, #22                         \n" /* q1 = |ARG*|ARG*|ARG*|ARG*| */
        "   vshlc                   q0, r0, #6                          \n" /* Similar operation on B channel */
        "   vand                    q2, q0, q6                          \n" /* q2 = |000b|00b0|0b00|b000| */
        "   vshl.u32                q3, q2, q4                          \n" /* q3 = |b000|b000|b000|b000| */
        "   vsri.32                 q1, q3, #24                         \n" /* q1 = |ARGb|ARGb|ARGb|ARGb| */
        "   vsri.32                 q1, q3, #29                         \n" /* q1 = |ARGB|ARGB|ARGB|ARGB| */
        "   vsri.32                 q3, q1, #8                          \n"
        "   vsri.32                 q3, q1, #16                         \n"
        "   vsri.32                 q3, q1, #24                         \n" /* q3 = |0AAA|0AAA|0AAA|0AAA| */
        "   vrmulh.u8               q2, q1, q3                          \n" /* pre-multiply alpha to all channels */
        "   vsli.32                 q2, q3, #24                         \n" /* q2 = |AR'G'B'|AR'G'B'|AR'G'B'|AR'G'B'| */
        "   cbz                     r1, 3f                              \n" /* recolor */
        "   vdup.32                 q0, %[recolor]                      \n"
        "   vdup.8                  q1, %[mix]                          \n"
        "   vrmulh.u8               q2, q2, q1                          \n"
        "   vadd.i8                 q2, q2, q0                          \n"
        "   3:                                                          \n"
        "   vstrb.8                 q2, [%[pTarget]], #16               \n"
        "   letp                    lr, 2b                              \n"
        "   4:                                                          \n"
        "   vstrb.8                 q2, [%[pTarget], #-16]              \n"
        "   wlstp.8                 lr, %[dst_offset], 6f               \n"
        "   5:                                                          \n"
        "   vstrb.8                 q2, [%[pTarget]], #16               \n"
        "   letp                    lr, 5b                              \n"
        "   6:                                                          \n"
        "   adds                    %[pSource], %[src_offset]           \n"
        "   subs                    %[h], #1                            \n"
        "   bne                     1b                                  \n"

        : [pSource] "+r"(px_map), [pTarget] "+r"(px_buf), [h] "+r"(h)
        : [w] "r"(header->w << 2), [recolor] "r"(recolor_premult),
        [opa] "r"(opa), [mix] "r"(mix), [src_offset] "r"(map_offset),
        [dst_offset] "r"(buf_offset)
        : "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "r0", "r1",
        "lr", "memory");

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

    uint32_t h = header->h;
    int32_t map_offset = map_stride - header->w;
    int32_t buf_offset = buf_stride - (header->w << 2);
    if(map_offset || buf_offset) {
        __asm volatile(
            "   .p2align 2                                                  \n"
            "   vmov.i32                q2, #0                              \n"
            "   1:                                                          \n"
            "   wlstp.32                lr, %[w], 3f                        \n"
            "   2:                                                          \n"
            "   vldrb.u32               q0, [%[pSource]], #4                \n"
            "   vldrw.u32               q1, [%[palette], q0, uxtw #2]       \n"
            "   vstrb.8                 q1, [%[pTarget]], #16               \n"
            "   letp                    lr, 2b                              \n"
            "   3:                                                          \n"
            "   vstrb.8                 q1, [%[pTarget], #-16]              \n"
            "   wlstp.8                 lr, %[dst_offset], 5f               \n"
            "   4:                                                          \n"
            "   vstrb.8                 q2, [%[pTarget]], #16               \n"
            "   letp                    lr, 4b                              \n"
            "   5:                                                          \n"
            "   adds                    %[pSource], %[src_offset]           \n"
            "   subs                    %[h], #1                            \n"
            "   bne                     1b                                  \n"
            : [pSource] "+r"(px_map), [pTarget] "+r"(px_buf), [h] "+r"(h)
            : [w] "r"(header->w), [src_offset] "r"(map_offset),
            [dst_offset] "r"(buf_offset), [palette] "r"(palette)
            : "q0", "q1", "q2", "lr", "memory");
    }
    else {
        __asm volatile(
            "   .p2align 2                                                  \n"
            "   wlstp.32                lr, %[size], 2f                     \n"
            "   1:                                                          \n"
            "   vldrb.u32               q0, [%[pSource]], #4                \n"
            "   vldrw.u32               q1, [%[palette], q0, uxtw #2]       \n"
            "   vstrb.8                 q1, [%[pTarget]], #16               \n"
            "   letp                    lr, 1b                              \n"
            "   2:                                                          \n"
            : [pSource] "+r"(px_map), [pTarget] "+r"(px_buf)
            : [size] "r"(header->w * header->h), [palette] "r"(palette)
            : "q0", "q1", "lr", "memory");
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

    // lv_opa_t opa = recolor.ch.alpha;
    // lv_opa_t mix = 255 - opa;

    uint32_t ww = ALIGN_UP(header->w, 4) << 2;
    uint32_t h = header->h;
    int32_t map_offset = map_stride - ww;
    int32_t buf_offset = buf_stride - ww;
    if(recolor.ch.alpha != LV_OPA_TRANSP) {
        if(ckey != 0) {
            /* recolor && chroma keyed */
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   movs                    r0, %[recolor], lsr #24             \n"
                "   vdup.32                 q0, %[recolor]                      \n"
                "   vdup.8                  q1, r0                              \n"
                "   vrmulh.u8               q0, q0, q1                          \n" /* q0 = recolor_premult */
                "   vmvn                    q1, q1                              \n"
                "   vmov.i32                q3, #0                              \n"
                "   1:                                                          \n"
                "   wlstp.8                 lr, %[w], 3f                        \n"
                "   2:                                                          \n"
                "   vldrb.8                 q2, [%[pSource]], #16               \n"
                "   vorr.i32                q2, #0xFF000000                     \n"
                "   vcmp.i32                eq, q2, %[ckey]                     \n"
                "   vrmulh.u8               q2, q2, q1                          \n"
                "   vadd.i8                 q2, q2, q0                          \n"
                "   vorr.i32                q2, #0xFF000000                     \n" /* set alpha to 0xFF */
                "   vpst                                                        \n"
                "   vmovt.i32               q2, #0                              \n"
                "   vstrb.8                 q2, [%[pTarget]], #16               \n"
                "   letp                    lr, 2b                              \n"
                "   3:                                                          \n"
                "   vstrb.8                 q2, [%[pTarget], #-16]              \n"
                "   wlstp.8                 lr, %[dst_offset], 5f               \n"
                "   4:                                                          \n"
                "   vstrb.8                 q3, [%[pTarget]], #16               \n"
                "   letp                    lr, 4b                              \n"
                "   5:                                                          \n"
                "   adds                    %[pSource], %[src_offset]           \n"
                "   subs                    %[h], #1                            \n"
                "   bne                     1b                                  \n"
                : [pSource] "+r"(px_map), [pTarget] "+r"(px_buf), [h] "+r"(h)
                : [w] "r"(header->w << 2), [recolor] "r"(recolor), [ckey] "r"(ckey),
                [src_offset] "r"(map_offset), [dst_offset] "r"(buf_offset)
                : "q0", "q1", "q2", "q3", "r0", "lr", "memory");
        }
        else {
            /* recolor, no chroma key */
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   movs                    r0, %[recolor], lsr #24             \n"
                "   vdup.32                 q0, %[recolor]                      \n"
                "   vdup.8                  q1, r0                              \n"
                "   vrmulh.u8               q0, q0, q1                          \n" /* q0 = recolor_premult */
                "   vmvn                    q1, q1                              \n" /* q1 = ~recolor_opa */
                "   vmov.i32                q3, #0                              \n"
                "   1:                                                          \n"
                "   wlstp.8                 lr, %[w], 3f                        \n"
                "   2:                                                          \n"
                "   vldrb.8                 q2, [%[pSource]], #16               \n"
                "   vrmulh.u8               q2, q2, q1                          \n"
                "   vadd.i8                 q2, q2, q0                          \n"
                "   vorr.i32                q2, #0xFF000000                     \n" /* set alpha to 0xFF */
                "   vstrb.8                 q2, [%[pTarget]], #16               \n"
                "   letp                    lr, 2b                              \n"
                "   3:                                                          \n"
                "   vstrb.8                 q2, [%[pTarget], #-16]              \n"
                "   wlstp.8                 lr, %[dst_offset], 5f               \n"
                "   4:                                                          \n"
                "   vstrb.8                 q3, [%[pTarget]], #16               \n"
                "   letp                    lr, 4b                              \n"
                "   5:                                                          \n"
                "   adds                    %[pSource], %[src_offset]           \n"
                "   subs                    %[h], #1                            \n"
                "   bne                     1b                                  \n"
                : [pSource] "+r"(px_map), [pTarget] "+r"(px_buf), [h] "+r"(h)
                : [w] "r"(header->w << 2), [recolor] "r"(recolor),
                [src_offset] "r"(map_offset), [dst_offset] "r"(buf_offset)
                : "q0", "q1", "q2", "q3", "r0", "lr", "memory");
        }
    }
    else if(ckey != 0) {
        /* no recolor, chroma keyed */
        __asm volatile(
            "   .p2align 2                                                  \n"
            "   vmov.i32                q1, #0                              \n"
            "   1:                                                          \n"
            "   wlstp.8                 lr, %[w], 3f                        \n"
            "   2:                                                          \n"
            "   vldrb.8                 q0, [%[pSource]], #16               \n"
            "   vorr.i32                q0, #0xFF000000                     \n"
            "   vcmp.i32                eq, q0, %[ckey]                     \n"
            "   vpst                                                        \n"
            "   vmovt.i32               q0, #0                              \n"
            "   vstrb.8                 q0, [%[pTarget]], #16               \n"
            "   letp                    lr, 2b                              \n"
            "   3:                                                          \n"
            "   vstrb.8                 q0, [%[pTarget], #-16]              \n"
            "   wlstp.8                 lr, %[dst_offset], 5f               \n"
            "   4:                                                          \n"
            "   vstrb.8                 q1, [%[pTarget]], #16               \n"
            "   letp                    lr, 4b                              \n"
            "   5:                                                          \n"
            "   adds                    %[pSource], %[src_offset]           \n"
            "   subs                    %[h], #1                            \n"
            "   bne                     1b                                  \n"
            : [pSource] "+r"(px_map), [pTarget] "+r"(px_buf), [h] "+r"(h)
            : [w] "r"(header->w << 2), [ckey] "r"(ckey),
            [src_offset] "r"(map_offset), [dst_offset] "r"(buf_offset)
            : "q0", "q1", "lr", "memory");
    }
    else if(map_offset || buf_offset) {
        /* no recolor, no chroma key */
        __asm volatile(
            "   .p2align 2                                                  \n"
            "   vmov.i32                q1, #0                              \n"
            "   1:                                                          \n"
            "   wlstp.8                 lr, %[w], 3f                        \n"
            "   2:                                                          \n"
            "   vldrb.8                 q0, [%[pSource]], #16               \n"
            "   vorr.i32                q0, #0xFF000000                     \n" /* set alpha to 0xFF */
            "   vstrb.8                 q0, [%[pTarget]], #16               \n"
            "   letp                    lr, 2b                              \n"
            "   3:                                                          \n"
            "   vstrb.8                 q0, [%[pTarget], #-16]              \n"
            "   wlstp.8                 lr, %[dst_offset], 5f               \n"
            "   4:                                                          \n"
            "   vstrb.8                 q1, [%[pTarget]], #16               \n"
            "   letp                    lr, 4b                              \n"
            "   5:                                                          \n"
            "   adds                    %[pSource], %[src_offset]           \n"
            "   subs                    %[h], #1                            \n"
            "   bne                     1b                                  \n"
            : [pSource] "+r"(px_map), [pTarget] "+r"(px_buf), [h] "+r"(h)
            : [w] "r"(header->w << 2), [src_offset] "r"(map_offset),
            [dst_offset] "r"(buf_offset)
            : "q0", "q1", "lr", "memory");
    }
    else {
        /* map_stride and buf_stride matches width, just copy 'em all */
        __asm volatile(
            "   .p2align 2                                                  \n"
            "   wlstp.8                 lr, %[size], 2f                     \n"
            "   1:                                                          \n"
            "   vldrb.8                 q0, [%[pSource]], #16               \n"
            "   vorr.i32                q0, #0xFF000000                     \n" /* set alpha to 0xFF */
            "   vstrb.8                 q0, [%[pTarget]], #16               \n"
            "   letp                    lr, 1b                              \n"
            "   2:                                                          \n"
            : [pSource] "+r"(px_map), [pTarget] "+r"(px_buf)
            : [size] "r"(ww * header->h)
            : "q0", "lr", "memory");
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

    uint32_t ww = ALIGN_UP(header->w, 4) << 2;
    uint32_t h = header->h;
    int32_t map_offset = map_stride - ww;
    int32_t buf_offset = buf_stride - ww;
    if(recolor.ch.alpha != LV_OPA_TRANSP) {
        if(!preprocessed) {
            /* recolor && need premult */
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   movs                    r0, %[recolor], lsr #24             \n"
                "   vdup.32                 q0, %[recolor]                      \n"
                "   vdup.8                  q1, r0                              \n"
                "   vrmulh.u8               q0, q0, q1                          \n" /* q0 = recolor_premult */
                "   vmvn                    q1, q1                              \n" /* q1 = ~recolor_opa */
                "   1:                                                          \n"
                "   wlstp.8                 lr, %[w], 3f                        \n"
                "   2:                                                          \n"
                "   vldrb.8                 q2, [%[pSource]], #16               \n"
                "   vsri.32                 q3, q2, #8                          \n"
                "   vsri.32                 q3, q2, #16                         \n"
                "   vsri.32                 q3, q2, #24                         \n"
                "   vrmulh.u8               q2, q2, q1                          \n"
                "   vadd.i8                 q2, q2, q0                          \n"
                "   vrmulh.u8               q2, q2, q3                          \n"
                "   vsli.32                 q2, q3, #24                         \n"
                "   vstrb.8                 q2, [%[pTarget]], #16               \n"
                "   letp                    lr, 2b                              \n"
                "   3:                                                          \n"
                "   vstrb.8                 q2, [%[pTarget], #-16]              \n"
                "   vmov.i32                q3, #0                              \n"
                "   wlstp.8                 lr, %[dst_offset], 5f               \n"
                "   4:                                                          \n"
                "   vstrb.8                 q3, [%[pTarget]], #16               \n"
                "   letp                    lr, 4b                              \n"
                "   5:                                                          \n"
                "   adds                    %[pSource], %[src_offset]           \n"
                "   subs                    %[h], #1                            \n"
                "   bne                     1b                                  \n"
                : [pSource] "+r"(px_map), [pTarget] "+r"(px_buf), [h] "+r"(h)
                : [w] "r"(header->w << 2), [recolor] "r"(recolor),
                [src_offset] "r"(map_offset), [dst_offset] "r"(buf_offset)
                : "q0", "q1", "q2", "q3", "r0", "lr", "memory");
        }
        else {
            /* recolor, need not premult */
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   movs                    r0, %[recolor], lsr #24             \n"
                "   vdup.32                 q0, %[recolor]                      \n"
                "   vdup.8                  q1, r0                              \n"
                "   vrmulh.u8               q0, q0, q1                          \n" /* q0 = recolor_premult */
                "   vmvn                    q1, q1                              \n" /* q1 = ~recolor_opa */
                "   1:                                                          \n"
                "   wlstp.8                 lr, %[w], 3f                        \n"
                "   2:                                                          \n"
                "   vldrb.8                 q2, [%[pSource]], #16               \n"
                "   vsri.32                 q3, q2, #8                          \n"
                "   vsri.32                 q3, q2, #16                         \n"
                "   vsri.32                 q3, q2, #24                         \n"
                "   vrmulh.u8               q2, q2, q1                          \n"
                "   vrmulh.u8               q4, q0, q3                          \n"
                "   vadd.i8                 q2, q2, q4                          \n"
                "   vsli.32                 q2, q3, #24                         \n"
                "   vstrb.8                 q2, [%[pTarget]], #16               \n"
                "   letp                    lr, 2b                              \n"
                "   3:                                                          \n"
                "   vstrb.8                 q2, [%[pTarget], #-16]              \n"
                "   vmov.i32                q3, #0                              \n"
                "   wlstp.8                 lr, %[dst_offset], 5f               \n"
                "   4:                                                          \n"
                "   vstrb.8                 q3, [%[pTarget]], #16               \n"
                "   letp                    lr, 4b                              \n"
                "   5:                                                          \n"
                "   adds                    %[pSource], %[src_offset]           \n"
                "   subs                    %[h], #1                            \n"
                "   bne                     1b                                  \n"
                : [pSource] "+r"(px_map), [pTarget] "+r"(px_buf), [h] "+r"(h)
                : [w] "r"(header->w << 2), [recolor] "r"(recolor),
                [src_offset] "r"(map_offset), [dst_offset] "r"(buf_offset)
                : "q0", "q1", "q2", "q3", "q4", "r0", "lr", "memory");
        }
    }
    else if(!preprocessed) {
        /* no recolor, need premult */
        __asm volatile(
            "   .p2align 2                                                  \n"
            "   vmov.i32                q2, #0                              \n"
            "   1:                                                          \n"
            "   wlstp.8                 lr, %[w], 3f                        \n"
            "   2:                                                          \n"
            "   vldrb.8                 q0, [%[pSource]], #16               \n"
            "   vsri.32                 q1, q0, #8                          \n"
            "   vsri.32                 q1, q0, #16                         \n"
            "   vsri.32                 q1, q0, #24                         \n"
            "   vrmulh.u8               q0, q0, q1                          \n"
            "   vsli.32                 q0, q1, #24                         \n"
            "   vstrb.8                 q0, [%[pTarget]], #16               \n"
            "   letp                    lr, 2b                              \n"
            "   3:                                                          \n"
            "   vstrb.8                 q0, [%[pTarget], #-16]              \n"
            "   wlstp.8                 lr, %[dst_offset], 5f               \n"
            "   4:                                                          \n"
            "   vstrb.8                 q2, [%[pTarget]], #16               \n"
            "   letp                    lr, 4b                              \n"
            "   5:                                                          \n"
            "   adds                    %[pSource], %[src_offset]           \n"
            "   subs                    %[h], #1                            \n"
            "   bne                     1b                                  \n"
            : [pSource] "+r"(px_map), [pTarget] "+r"(px_buf), [h] "+r"(h)
            : [w] "r"(header->w << 2), [src_offset] "r"(map_offset),
            [dst_offset] "r"(buf_offset)
            : "q0", "q1", "q2", "lr", "memory");
    }
    else if(map_offset || buf_offset) {
        /* no recolor, need not premult */
        __asm volatile(
            "   .p2align 2                                                  \n"
            "   vmov.i32                q1, #0                              \n"
            "   1:                                                          \n"
            "   wlstp.8                 lr, %[w], 3f                        \n"
            "   2:                                                          \n"
            "   vldrb.8                 q0, [%[pSource]], #16               \n"
            "   vstrb.8                 q0, [%[pTarget]], #16               \n"
            "   letp                    lr, 2b                              \n"
            "   3:                                                          \n"
            "   vstrb.8                 q0, [%[pTarget], #-16]              \n"
            "   wlstp.8                 lr, %[dst_offset], 5f               \n"
            "   4:                                                          \n"
            "   vstrb.8                 q1, [%[pTarget]], #16               \n"
            "   letp                    lr, 4b                              \n"
            "   5:                                                          \n"
            "   adds                    %[pSource], %[src_offset]           \n"
            "   subs                    %[h], #1                            \n"
            "   bne                     1b                                  \n"
            : [pSource] "+r"(px_map), [pTarget] "+r"(px_buf), [h] "+r"(h)
            : [w] "r"(header->w << 2), [src_offset] "r"(map_offset),
            [dst_offset] "r"(buf_offset)
            : "q0", "q1", "lr", "memory");
    }
    else {
        /* map_stride and buf_stride matches width, just copy 'em all */
        __asm volatile(
            "   .p2align 2                                                  \n"
            "   wlstp.8                 lr, %[size], 2f                     \n"
            "   1:                                                          \n"
            "   vldrb.8                 q0, [%[pSource]], #16               \n"
            "   vstrb.8                 q0, [%[pTarget]], #16               \n"
            "   letp                    lr, 1b                              \n"
            "   2:                                                          \n"
            : [pSource] "+r"(px_map), [pTarget] "+r"(px_buf)
            : [size] "r"(ww * h)
            : "q0", "lr", "memory");
    }

    return LV_GPU_CONV_RES_OK;
}

#if (LV_COLOR_DEPTH == 32)

static lv_gpu_conv_res_t convert_gaussian_hor_blur(void * dsc)
{
    lv_gpu_conv_gaussian_dir_blur_dsc_t * conv_dsc = dsc;

    lv_color_t * dst = conv_dsc->dst;
    const lv_color_t * src = conv_dsc->src;
    lv_coord_t dst_stride = conv_dsc->dst_stride;
    lv_coord_t src_stride = conv_dsc->src_stride;
    lv_coord_t w = conv_dsc->w;
    lv_coord_t h = conv_dsc->h;
    lv_coord_t r = conv_dsc->r;
    lv_color_t * tmp = conv_dsc->tmp;

    enum COLOR_ID {
        B,
        G,
        R,
        A
    };

    for(lv_coord_t i = 0; i < h; i++) {
        lv_color_t fv, lv;
        uint32_t val[4], len = r + r + 1;
        uint32_t src_res_px = src_stride;
        uint32_t dst_res_px = dst_stride;
        uint32_t res_w = w;
        val[A] = src[0].ch.alpha * (r + 1);
        val[R] = src[0].ch.red * (r + 1);
        val[G] = src[0].ch.green * (r + 1);
        val[B] = src[0].ch.blue * (r + 1);
        tmp[0].full = tmp[1].full = src[0].full;

        if(r == 1 && w >= 16) {
            uint32_t * phwSource = (uint32_t *)src;
            uint32_t * pwTarget = (uint32_t *)dst;
            int32_t blkCnt = w >> 4;
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   vldrw.32                q6, [%[pSrc]], #4                   \n"
                "   mov                     r4, #0x55                           \n"
                "   vdup.8                  q5, r4                              \n"
                "   vrmulh.u8               q6, q6, q5                          \n"
                "   vmov.u8                 r0, q6[0]                           \n"
                "   mov                     r4, #0xFF                           \n"
                "   and                     r1, r4, r0, lsr #8                  \n"
                "   and                     r2, r4, r0, lsr #16                 \n"
                "   and                     r3, r4, r0, lsr #24                 \n"
                "   and                     r0, r4, r0                          \n"
                "   orr                     r0, r0, r0, lsl #8                  \n"
                "   orr                     r1, r1, r1, lsl #8                  \n"
                "   orr                     r2, r2, r2, lsl #8                  \n"
                "   orr                     r3, r3, r3, lsl #8                  \n"
                "   wls                     lr, %[loopCnt], 1f                  \n"
                "   2:                                                          \n"
                "   vld40.8                 {q0, q1, q2, q3}, [%[pSrc]]         \n"
                "   vld41.8                 {q0, q1, q2, q3}, [%[pSrc]]         \n"
                "   vld42.8                 {q0, q1, q2, q3}, [%[pSrc]]         \n"
                "   vld43.8                 {q0, q1, q2, q3}, [%[pSrc]]!        \n"
                "   mov                     r4, #0x55                           \n"
                "   vdup.8                  q7, r4                              \n"
                "   vrmulh.u8               q0, q0, q7                          \n"
                "   vrmulh.u8               q1, q1, q7                          \n"
                "   vrmulh.u8               q2, q2, q7                          \n"
                "   vrmulh.u8               q3, q3, q7                          \n"
                "   vmov                    q4, q0                              \n"
                "   vmov.u16                r4, q0[7]                           \n"
                "   orr                     r4, r4, r0, lsl #16                 \n"
                "   mov                     r0, r0, lsr #8                      \n"
                "   vshlc                   q0, r0, #8                          \n"
                "   vadd.u8                 q4, q4, q0                          \n"
                "   mov                     r0, r4, lsr #16                     \n"
                "   vshlc                   q0, r0, #8                          \n"
                "   vadd.u8                 q4, q4, q0                          \n"
                "   mov                     r0, r4                              \n"
                "   vmov                    q5, q1                              \n"
                "   vmov.u16                r4, q1[7]                           \n"
                "   orr                     r4, r4, r1, lsl #16                 \n"
                "   mov                     r1, r1, lsr #8                      \n"
                "   vshlc                   q1, r1, #8                          \n"
                "   vadd.u8                 q5, q5, q1                          \n"
                "   mov                     r1, r4, lsr #16                     \n"
                "   vshlc                   q1, r1, #8                          \n"
                "   vadd.u8                 q5, q5, q1                          \n"
                "   mov                     r1, r4                              \n"
                "   vmov                    q6, q2                              \n"
                "   vmov.u16                r4, q2[7]                           \n"
                "   orr                     r4, r4, r2, lsl #16                 \n"
                "   mov                     r2, r2, lsr #8                      \n"
                "   vshlc                   q2, r2, #8                          \n"
                "   vadd.u8                 q6, q6, q2                          \n"
                "   mov                     r2, r4, lsr #16                     \n"
                "   vshlc                   q2, r2, #8                          \n"
                "   vadd.u8                 q6, q6, q2                          \n"
                "   mov                     r2, r4                              \n"
                "   vmov                    q7, q3                              \n"
                "   vmov.u16                r4, q3[7]                           \n"
                "   orr                     r4, r4, r3, lsl #16                 \n"
                "   mov                     r3, r3, lsr #8                      \n"
                "   vshlc                   q3, r3, #8                          \n"
                "   vadd.u8                 q7, q7, q3                          \n"
                "   mov                     r3, r4, lsr #16                     \n"
                "   vshlc                   q3, r3, #8                          \n"
                "   vadd.u8                 q7, q7, q3                          \n"
                "   mov                     r3, r4                              \n"
                "   vst40.8                 {q4, q5, q6, q7}, [%[pDst]]         \n"
                "   vst41.8                 {q4, q5, q6, q7}, [%[pDst]]         \n"
                "   vst42.8                 {q4, q5, q6, q7}, [%[pDst]]         \n"
                "   vst43.8                 {q4, q5, q6, q7}, [%[pDst]]!        \n"
                "   le                      lr, 2b                              \n"
                "   1:                                                          \n"
                "   and                     r0, r0, #0xFF                       \n"
                "   and                     r1, r1, #0xFF                       \n"
                "   and                     r2, r2, #0xFF                       \n"
                "   and                     r3, r3, #0xFF                       \n"
                "   str                     r0, [%[val]]                        \n"
                "   str                     r1, [%[val], #4]                    \n"
                "   str                     r2, [%[val], #8]                    \n"
                "   str                     r3, [%[val], #12]                   \n"
                : [pSrc] "+r"(phwSource), [pDst] "+r"(pwTarget)
                : [loopCnt] "r"(blkCnt), [val] "r"(val)
                : "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "r0", "r1", "r2",
                "r3", "r4", "lr", "memory");
            blkCnt <<= 4;
            src += blkCnt;
            dst += blkCnt;
            src_res_px -= blkCnt;
            dst_res_px -= blkCnt;
            res_w -= blkCnt;
            if(res_w) {
                tmp[0].full = 0;
                tmp[1].ch.blue = val[B] *= 3;
                tmp[1].ch.green = val[G] *= 3;
                tmp[1].ch.red = val[R] *= 3;
                tmp[1].ch.alpha = val[A] *= 3;
            }
            else {
                lv_color_t * last = dst - 1;
                last->ch.blue += val[B] - src->ch.blue / 3;
                last->ch.green += val[G] - src->ch.green / 3;
                last->ch.red += val[R] - src->ch.red / 3;
                last->ch.alpha += val[A] - src->ch.alpha / 3;
                src += src_res_px;
                dst += dst_res_px;
                continue;
            }
        }

        for(lv_coord_t j = 0; j < r; j++) {
            val[B] += src[j].ch.blue;
            val[G] += src[j].ch.green;
            val[R] += src[j].ch.red;
            val[A] += src[j].ch.alpha;
        }
        for(lv_coord_t j = 0; j < res_w; j++) {
            fv.full = j > r - 2 ? tmp[j - r + 1].full : tmp[0].full;
            lv.full = j < res_w - r ? src[j + r].full : src[res_w - 1].full;
            val[B] += lv.ch.blue - fv.ch.blue;
            val[G] += lv.ch.green - fv.ch.green;
            val[R] += lv.ch.red - fv.ch.red;
            val[A] += lv.ch.alpha - fv.ch.alpha;
            tmp[j + 2] = src[j];
            dst[j].ch.blue = val[B] / len;
            dst[j].ch.green = val[G] / len;
            dst[j].ch.red = val[R] / len;
            dst[j].ch.alpha = val[A] / len;
        }
        src += src_res_px;
        dst += dst_res_px;
    }

    return LV_GPU_CONV_RES_OK;
}

static lv_gpu_conv_res_t convert_gaussian_ver_blur(void * dsc)
{
    lv_gpu_conv_gaussian_dir_blur_dsc_t * conv_dsc = dsc;

    lv_color_t * dst = conv_dsc->dst;
    const lv_color_t * src = conv_dsc->src;
    lv_coord_t dst_stride = conv_dsc->dst_stride;
    lv_coord_t src_stride = conv_dsc->src_stride;
    lv_coord_t w = conv_dsc->w;
    lv_coord_t h = conv_dsc->h;
    lv_coord_t r = conv_dsc->r;
    lv_color_t * tmp = conv_dsc->tmp;

    enum COLOR_ID {
        B,
        G,
        R,
        A
    };

    if(r == 1 && w >= 16) {
        lv_coord_t w16 = w >> 4;
        for(lv_coord_t i = 0; i < w16; i++) {
            uint32_t * phwSource = (uint32_t *)src;
            uint32_t * pwTarget = (uint32_t *)dst;
            int32_t blkCnt = h - 1;
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   vld40.8                 {q0, q1, q2, q3}, [%[pSrc]]         \n"
                "   vld41.8                 {q0, q1, q2, q3}, [%[pSrc]]         \n"
                "   vld42.8                 {q0, q1, q2, q3}, [%[pSrc]]         \n"
                "   vld43.8                 {q0, q1, q2, q3}, [%[pSrc]]         \n"
                "   mov                     r0, #0x55                           \n"
                "   vdup.8                  q6, r0                              \n"
                "   vrmulh.u8               q0, q0, q6                          \n"
                "   vrmulh.u8               q1, q1, q6                          \n"
                "   vrmulh.u8               q2, q2, q6                          \n"
                "   vrmulh.u8               q3, q3, q6                          \n"
                "   mov                     r0, %[tmp]                          \n"
                "   vstrw.32                q0, [r0], #16                       \n"
                "   vstrw.32                q1, [r0], #16                       \n"
                "   vstrw.32                q2, [r0], #16                       \n"
                "   vstrw.32                q3, [r0], #16                       \n"
                "   vstrw.32                q0, [r0], #16                       \n"
                "   vstrw.32                q1, [r0], #16                       \n"
                "   vstrw.32                q2, [r0], #16                       \n"
                "   vstrw.32                q3, [r0], #16                       \n"
                "   mov                     r1, %[tmp]                          \n"
                "   mov                     r2, r0                              \n"
                "   add                     %[pSrc], %[pSrc], %[sS], lsl #2     \n"
                "   wls                     lr, %[loopCnt], 1f                  \n"
                "   2:                                                          \n"
                "   cmp                     r0, r2                              \n"
                "   it                      eq                                  \n"
                "   moveq                   r0, %[tmp]                          \n"
                "   mov                     r1, %[tmp]                          \n"
                "   vld40.8                 {q0, q1, q2, q3}, [%[pSrc]]         \n"
                "   vld41.8                 {q0, q1, q2, q3}, [%[pSrc]]         \n"
                "   vld42.8                 {q0, q1, q2, q3}, [%[pSrc]]         \n"
                "   vld43.8                 {q0, q1, q2, q3}, [%[pSrc]]         \n"
                "   vrmulh.u8               q4, q0, q6                          \n"
                "   vldrw.32                q5, [r1, #64]                       \n"
                "   vadd.u8                 q0, q4, q5                          \n"
                "   vldrw.32                q5, [r1], #16                       \n"
                "   vadd.u8                 q0, q0, q5                          \n"
                "   vstrw.32                q4, [r0], #16                       \n"
                "   vrmulh.u8               q4, q1, q6                          \n"
                "   vldrw.32                q5, [r1, #64]                       \n"
                "   vadd.u8                 q1, q4, q5                          \n"
                "   vldrw.32                q5, [r1], #16                       \n"
                "   vadd.u8                 q1, q1, q5                          \n"
                "   vstrw.32                q4, [r0], #16                       \n"
                "   vrmulh.u8               q4, q2, q6                          \n"
                "   vldrw.32                q5, [r1, #64]                       \n"
                "   vadd.u8                 q2, q4, q5                          \n"
                "   vldrw.32                q5, [r1], #16                       \n"
                "   vadd.u8                 q2, q2, q5                          \n"
                "   vstrw.32                q4, [r0], #16                       \n"
                "   vrmulh.u8               q4, q3, q6                          \n"
                "   vldrw.32                q5, [r1, #64]                       \n"
                "   vadd.u8                 q3, q4, q5                          \n"
                "   vldrw.32                q5, [r1], #16                       \n"
                "   vadd.u8                 q3, q3, q5                          \n"
                "   vstrw.32                q4, [r0], #16                       \n"
                "   vst40.8                 {q0, q1, q2, q3}, [%[pDst]]         \n"
                "   vst41.8                 {q0, q1, q2, q3}, [%[pDst]]         \n"
                "   vst42.8                 {q0, q1, q2, q3}, [%[pDst]]         \n"
                "   vst43.8                 {q0, q1, q2, q3}, [%[pDst]]         \n"
                "   add                     %[pSrc], %[pSrc], %[sS], lsl #2     \n"
                "   add                     %[pDst], %[pDst], %[dS], lsl #2     \n"
                "   le                      lr, 2b                              \n"
                "   1:                                                          \n"
                "   sub                     r0, r0, #64                         \n"
                "   mov                     r1, %[tmp]                          \n"
                "   vldrw.32                q4, [r0], #16                       \n"
                "   vldrw.32                q0, [r1, #64]                       \n"
                "   vadd.u8                 q0, q0, q4                          \n"
                "   vldrw.32                q4, [r1], #16                       \n"
                "   vadd.u8                 q0, q0, q4                          \n"
                "   vldrw.32                q4, [r0], #16                       \n"
                "   vldrw.32                q1, [r1, #64]                       \n"
                "   vadd.u8                 q1, q1, q4                          \n"
                "   vldrw.32                q4, [r1], #16                       \n"
                "   vadd.u8                 q1, q1, q4                          \n"
                "   vldrw.32                q4, [r0], #16                       \n"
                "   vldrw.32                q2, [r1, #64]                       \n"
                "   vadd.u8                 q2, q2, q4                          \n"
                "   vldrw.32                q4, [r1], #16                       \n"
                "   vadd.u8                 q2, q2, q4                          \n"
                "   vldrw.32                q4, [r0], #16                       \n"
                "   vldrw.32                q3, [r1, #64]                       \n"
                "   vadd.u8                 q3, q3, q4                          \n"
                "   vldrw.32                q4, [r1], #16                       \n"
                "   vadd.u8                 q3, q3, q4                          \n"
                "   vst40.8                 {q0, q1, q2, q3}, [%[pDst]]         \n"
                "   vst41.8                 {q0, q1, q2, q3}, [%[pDst]]         \n"
                "   vst42.8                 {q0, q1, q2, q3}, [%[pDst]]         \n"
                "   vst43.8                 {q0, q1, q2, q3}, [%[pDst]]         \n"
                : [pSrc] "+r"(phwSource), [pDst] "+r"(pwTarget)
                : [loopCnt] "r"(blkCnt), [sS] "r"(src_stride), [dS] "r"(dst_stride),
                [tmp] "r"(tmp)
                : "q0", "q1", "q2", "q3", "q4", "q5", "q6", "r0", "r1", "r2", "lr",
                "memory");
            src += 16;
            dst += 16;
        }
        w -= w16 << 4;
    }

    for(lv_coord_t i = 0; i < w; i++) {
        lv_color_t fv, lv;
        uint32_t val[4] = { 0, 0, 0, 0 }, len = r + r + 1;
        uint32_t sj = i;
        val[B] = (r + 1) * src[sj].ch.blue;
        val[G] = (r + 1) * src[sj].ch.green;
        val[R] = (r + 1) * src[sj].ch.red;
        val[A] = (r + 1) * src[sj].ch.alpha;
        for(lv_coord_t j = 0; j < r; j++) {
            val[B] += src[sj].ch.blue;
            val[G] += src[sj].ch.green;
            val[R] += src[sj].ch.red;
            val[A] += src[sj].ch.alpha;
            sj += src_stride;
        }
        sj = i;
        uint32_t tj = i;
        tmp[0] = src[i];
        for(lv_coord_t j = 0; j < h; j++) {
            fv.full = j > r ? tmp[j - r - 1].full : tmp[0].full;
            lv.full = j < h - r ? src[sj + r * src_stride].full
                      : src[i + (h - 1) * src_stride].full;
            val[B] += lv.ch.blue - fv.ch.blue;
            val[G] += lv.ch.green - fv.ch.green;
            val[R] += lv.ch.red - fv.ch.red;
            val[A] += lv.ch.alpha - fv.ch.alpha;
            tmp[j] = src[sj];
            dst[tj].ch.blue = val[B] / len;
            dst[tj].ch.green = val[G] / len;
            dst[tj].ch.red = val[R] / len;
            dst[tj].ch.alpha = val[A] / len;
            sj += src_stride;
            tj += dst_stride;
        }
    }

    return LV_GPU_CONV_RES_OK;
}

#endif /* LV_COLOR_DEPTH == 32 */

#endif /* CONFIG_ARM_HAVE_MVE */
