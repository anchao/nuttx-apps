/****************************************************************************
 * apps/graphics/lvgl/lv_porting/gpu_manager/mve/mve_draw_blend.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "mve_internal.h"

#ifdef CONFIG_LV_GPU_USE_ARM_MVE

#include "arm_mve.h"

/****************************************************************************
 * Preprocessor Definitions
 ****************************************************************************/

/****************************************************************************
 * Macros
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void fill_normal(lv_color_t * dst,
                        const lv_area_t * draw_area, lv_coord_t dst_stride, lv_color_t color,
                        lv_opa_t opa, const lv_opa_t * mask, lv_coord_t mask_stride)
{
    lv_coord_t w = lv_area_get_width(draw_area);
    lv_coord_t h = lv_area_get_height(draw_area);
    if(mask) {
        for(lv_coord_t i = 0; i < h; i++) {
            uint32_t * pwTarget = (uint32_t *)dst;
            uint32_t blkCnt = w;
            uint8_t R = color.ch.red;
            uint8_t G = color.ch.green;
            uint8_t B = color.ch.blue;
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   movs                    r1, %[mask]                         \n"
                "   bics                    r0, %[loopCnt], #0xF                \n"
                "   wlstp.8                 lr, r0, 1f                          \n"
                "   2:                                                          \n"
                "   vld40.8                 {q0, q1, q2, q3}, [%[pDst]]         \n"
                "   vld41.8                 {q0, q1, q2, q3}, [%[pDst]]         \n"
                "   vld42.8                 {q0, q1, q2, q3}, [%[pDst]]         \n"
                "   vld43.8                 {q0, q1, q2, q3}, [%[pDst]]         \n"
                "   vldrb.8                 q4, [r1], #16                       \n"
                "   vdup.8                  q5, %[opa]                          \n"
                "   vrmulh.u8               q4, q4, q5                          \n"
                "   vmvn.i32                q3, #0                              \n"
                "   vmvn                    q5, q4                              \n"
                "   vrmulh.u8               q0, q0, q5                          \n"
                "   vrmulh.u8               q1, q1, q5                          \n"
                "   vrmulh.u8               q2, q2, q5                          \n"
                "   vdup.8                  q5, %[B]                            \n"
                "   vrmulh.u8               q5, q4, q5                          \n"
                "   vadd.i8                 q0, q0, q5                          \n"
                "   vdup.8                  q5, %[G]                            \n"
                "   vrmulh.u8               q5, q4, q5                          \n"
                "   vadd.i8                 q1, q1, q5                          \n"
                "   vdup.8                  q5, %[R]                            \n"
                "   vrmulh.u8               q5, q4, q5                          \n"
                "   vadd.i8                 q2, q2, q5                          \n"
                "   vst40.8                 {q0, q1, q2, q3}, [%[pDst]]         \n"
                "   vst41.8                 {q0, q1, q2, q3}, [%[pDst]]         \n"
                "   vst42.8                 {q0, q1, q2, q3}, [%[pDst]]         \n"
                "   vst43.8                 {q0, q1, q2, q3}, [%[pDst]]!        \n"
                "   letp                    lr, 2b                              \n"
                "   1:                                                          \n"
                "   ands                    r0, %[loopCnt], #0xF                \n"
                "   movs                    r0, r0, lsl #2                      \n"
                "   vdup.8                  q2, %[opa]                          \n"
                "   vdup.32                 q3, %[color]                        \n"
                "   wlstp.8                 lr, r0, 3f                          \n"
                "   4:                                                          \n"
                "   vldrb.8                 q0, [%[pDst]]                       \n"
                "   vldrb.u32               q1, [r1], #4                        \n"
                "   vsli.32                 q1, q1, #8                          \n"
                "   vsli.32                 q1, q1, #16                         \n"
                "   vrmulh.u8               q1, q1, q2                          \n"
                "   vmvn                    q4, q1                              \n"
                "   vrmulh.u8               q0, q0, q4                          \n"
                "   vrmulh.u8               q5, q3, q1                          \n"
                "   vadd.i8                 q0, q0, q5                          \n"
                "   vstrb.8                 q0, [%[pDst]], #16                  \n"
                "   letp                    lr, 4b                              \n"
                "   3:                                                          \n"
                : [pDst] "+r"(pwTarget)
                : [R] "r"(R), [G] "r"(G), [B] "r"(B), [opa] "r"(opa),
                [loopCnt] "r"(blkCnt), [color] "r"(color.full), [mask] "r"(mask)
                : "q0", "q1", "q2", "q3", "q4", "q5", "r0", "r1", "lr", "memory");
            dst += dst_stride;
            mask += mask_stride;
        }
    }
    else if(opa != LV_OPA_COVER) {
        if(dst_stride != w) {
            for(lv_coord_t i = 0; i < h; i++) {
                uint32_t * pwTarget = (uint32_t *)dst;
                register unsigned blkCnt __asm("lr") = w << 2;
                __asm volatile(
                    "   .p2align 2                                                  \n"
                    "   vdup.32                 q0, %[color]                        \n"
                    "   vdup.8                  q1, %[opa]                          \n"
                    "   vrmulh.u8               q0, q0, q1                          \n"
                    "   vmvn                    q1, q1                              \n"
                    "   wlstp.8                 lr, %[loopCnt], 1f                  \n"
                    "   2:                                                          \n"
                    "   vldrb.8                 q2, [%[pDst]]                       \n"
                    "   vrmulh.u8               q2, q2, q1                          \n"
                    "   vadd.i8                 q2, q0, q2                          \n"
                    "   vstrb.8                 q2, [%[pDst]], #16                  \n"
                    "   letp                    lr, 2b                              \n"
                    "   1:                                                          \n"
                    : [pDst] "+r"(pwTarget), [loopCnt] "+r"(blkCnt)
                    : [color] "r"(color), [opa] "r"(opa)
                    : "q0", "q1", "q2", "memory");
                dst += dst_stride;
            }
        }
        else {
            register unsigned blkCnt __asm("lr") = w * h << 2;
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   vdup.32                 q0, %[color]                        \n"
                "   vdup.8                  q1, %[opa]                          \n"
                "   vrmulh.u8               q0, q0, q1                          \n"
                "   vmvn                    q1, q1                              \n"
                "   wlstp.8                 lr, %[loopCnt], 1f                  \n"
                "   2:                                                          \n"
                "   vldrb.8                 q2, [%[pDst]]                       \n"
                "   vrmulh.u8               q2, q2, q1                          \n"
                "   vadd.i8                 q2, q0, q2                          \n"
                "   vstrb.8                 q2, [%[pDst]], #16                  \n"
                "   letp                    lr, 2b                              \n"
                "   1:                                                          \n"
                : [pDst] "+r"(dst), [loopCnt] "+r"(blkCnt)
                : [color] "r"(color), [opa] "r"(opa)
                : "q0", "q1", "q2", "memory");
        }
    }
    else if(dst_stride != w) {
        for(lv_coord_t i = 0; i < h; i++) {
            uint32_t * pwTarget = (uint32_t *)dst;
            register unsigned blkCnt __asm("lr") = w << 2;
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   vdup.32                 q0, %[color]                        \n"
                "   wlstp.8                 lr, %[loopCnt], 1f                  \n"
                "   2:                                                          \n"
                "   vstrb.8                 q0, [%[pDst]], #16                  \n"
                "   letp                    lr, 2b                              \n"
                "   1:                                                          \n"
                : [pDst] "+r"(pwTarget), [loopCnt] "+r"(blkCnt)
                : [color] "r"(color)
                : "q0", "memory");
            dst += dst_stride;
        }
    }
    else {
        register unsigned blkCnt __asm("lr") = w * h << 2;
        __asm volatile(
            "   .p2align 2                                                  \n"
            "   vdup.32                 q0, %[color]                        \n"
            "   wlstp.8                 lr, %[loopCnt], 1f                  \n"
            "   2:                                                          \n"
            "   vstrb.8                 q0, [%[pDst]], #16                  \n"
            "   letp                    lr, 2b                              \n"
            "   1:                                                          \n"
            : [pDst] "+r"(dst), [loopCnt] "+r"(blkCnt)
            : [color] "r"(color)
            : "q0", "memory");
    }
}

static void map_normal(lv_color_t * dst,
                       const lv_area_t * draw_area, lv_coord_t dst_stride, const lv_color_t * src,
                       lv_coord_t src_stride, lv_opa_t opa, const lv_opa_t * mask,
                       lv_coord_t mask_stride)
{
    lv_coord_t w = lv_area_get_width(draw_area);
    lv_coord_t h = lv_area_get_height(draw_area);
    if(mask) {
        for(lv_coord_t i = 0; i < h; i++) {
            uint32_t * phwSource = (uint32_t *)src;
            uint32_t * pwTarget = (uint32_t *)dst;
            uint32_t blkCnt = w;
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   movs                    r1, %[mask]                         \n"
                "   bics                    r0, %[loopCnt], #0xF                \n"
                "   wlstp.8                 lr, r0, 1f                          \n"
                "   2:                                                          \n"
                "   vld40.8                 {q0, q1, q2, q3}, [%[pSrc]]         \n"
                "   vld41.8                 {q0, q1, q2, q3}, [%[pSrc]]         \n"
                "   vld42.8                 {q0, q1, q2, q3}, [%[pSrc]]         \n"
                "   vld43.8                 {q0, q1, q2, q3}, [%[pSrc]]!        \n"
                "   vld40.8                 {q4, q5, q6, q7}, [%[pDst]]         \n"
                "   vld41.8                 {q4, q5, q6, q7}, [%[pDst]]         \n"
                "   vld42.8                 {q4, q5, q6, q7}, [%[pDst]]         \n"
                "   vld43.8                 {q4, q5, q6, q7}, [%[pDst]]         \n"
                "   vldrb.8                 q3, [r1], #16                       \n"
                "   vdup.8                  q7, %[opa]                          \n"
                "   vrmulh.u8               q3, q3, q7                          \n"
                "   vrmulh.u8               q0, q0, q3                          \n"
                "   vrmulh.u8               q1, q1, q3                          \n"
                "   vrmulh.u8               q2, q2, q3                          \n"
                "   vmvn.i32                q7, #0                              \n"
                "   vmvn                    q3, q3                              \n"
                "   vrmulh.u8               q4, q4, q3                          \n"
                "   vrmulh.u8               q5, q5, q3                          \n"
                "   vrmulh.u8               q6, q6, q3                          \n"
                "   vadd.i8                 q4, q0, q4                          \n"
                "   vadd.i8                 q5, q1, q5                          \n"
                "   vadd.i8                 q6, q2, q6                          \n"
                "   vst40.8                 {q4, q5, q6, q7}, [%[pDst]]         \n"
                "   vst41.8                 {q4, q5, q6, q7}, [%[pDst]]         \n"
                "   vst42.8                 {q4, q5, q6, q7}, [%[pDst]]         \n"
                "   vst43.8                 {q4, q5, q6, q7}, [%[pDst]]!        \n"
                "   letp                    lr, 2b                              \n"
                "   1:                                                          \n"
                "   ands                    r0, %[loopCnt], #0xF                \n"
                "   movs                    r0, r0, lsl #2                      \n"
                "   vdup.8                  q2, %[opa]                          \n"
                "   wlstp.8                 lr, r0, 3f                          \n"
                "   4:                                                          \n"
                "   vldrb.8                 q0, [%[pDst]]                       \n"
                "   vldrb.u32               q1, [r1], #4                        \n"
                "   vldrw.32                q3, [%[pSrc]], #16                  \n"
                "   vsli.32                 q1, q1, #8                          \n"
                "   vsli.32                 q1, q1, #16                         \n"
                "   vrmulh.u8               q1, q1, q2                          \n"
                "   vmvn                    q4, q1                              \n"
                "   vrmulh.u8               q0, q0, q4                          \n"
                "   vrmulh.u8               q3, q3, q1                          \n"
                "   vadd.i8                 q0, q0, q3                          \n"
                "   vstrb.8                 q0, [%[pDst]], #16                  \n"
                "   letp                    lr, 4b                              \n"
                "   3:                                                          \n"
                : [pSrc] "+r"(phwSource), [pDst] "+r"(pwTarget)
                : [opa] "r"(opa), [loopCnt] "r"(blkCnt), [mask] "r"(mask)
                : "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "r0", "r1", "lr",
                "memory");
            src += src_stride;
            dst += dst_stride;
            mask += mask_stride;
        }
    }
    else if(opa != LV_OPA_COVER) {
        for(lv_coord_t i = 0; i < h; i++) {
            uint32_t * phwSource = (uint32_t *)src;
            uint32_t * pwTarget = (uint32_t *)dst;
            register unsigned blkCnt __asm("lr") = w << 2;
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   vdup.8                  q0, %[opa]                          \n"
                "   vmvn                    q1, q0                              \n"
                "   wlstp.8                 lr, %[loopCnt], 1f                  \n"
                "   2:                                                          \n"
                "   vldrb.8                 q2, [%[pSrc]], #16                  \n"
                "   vrmulh.u8               q2, q2, q0                          \n"
                "   vldrb.8                 q3, [%[pDst]]                       \n"
                "   vrmulh.u8               q3, q3, q1                          \n"
                "   vadd.i8                 q3, q2, q3                          \n"
                "   vstrb.8                 q3, [%[pDst]], #16                  \n"
                "   letp                    lr, 2b                              \n"
                "   1:                                                          \n"
                : [pSrc] "+r"(phwSource), [pDst] "+r"(pwTarget),
                [loopCnt] "+r"(blkCnt)
                : [opa] "r"(opa)
                : "q0", "q1", "q2", "q3", "memory");
            src += src_stride;
            dst += dst_stride;
        }
    }
    else {
        for(lv_coord_t i = 0; i < h; i++) {
            uint32_t * phwSource = (uint32_t *)src;
            uint32_t * pwTarget = (uint32_t *)dst;
            register unsigned blkCnt __asm("lr") = w << 2;
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   wlstp.8                 lr, %[loopCnt], 1f                  \n"
                "   2:                                                          \n"
                "   vldrb.8                 q0, [%[pSrc]], #16                  \n"
                "   vstrb.8                 q0, [%[pDst]], #16                  \n"
                "   letp                    lr, 2b                              \n"
                "   1:                                                          \n"
                : [pSrc] "+r"(phwSource), [pDst] "+r"(pwTarget),
                [loopCnt] "+r"(blkCnt)
                :
                : "q0", "memory");
            src += src_stride;
            dst += dst_stride;
        }
    }
}

static void fill_transp(lv_color_t * dst,
                        const lv_area_t * draw_area, lv_coord_t dst_stride, lv_color_t color,
                        lv_opa_t opa, const lv_opa_t * mask, lv_coord_t mask_stride)
{
    lv_coord_t w = lv_area_get_width(draw_area);
    lv_coord_t h = lv_area_get_height(draw_area);
    if(mask) {
        for(lv_coord_t i = 0; i < h; i++) {
            uint32_t * pwTarget = (uint32_t *)dst;
            register unsigned blkCnt __asm("lr") = w << 2;
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   mov                     r3, %[mask]                         \n"
                "   mov                     r0, #0xFE01                         \n"
                "   wlstp.8                 lr, %[loopCnt], 1f                  \n"
                "   2:                                                          \n"
                "   vldrb.8                 q2, [%[pDst]]                       \n"
                "   vldrb.u32               q3, [r3], #4                        \n"
                "   vshr.u32                q1, q2, #24                         \n"
                "   vdup.32                 q0, %[opa]                          \n"
                "   vrmulh.u8               q3, q0, q3                          \n"
                "   vmvn                    q0, q3                              \n"
                "   vrmulh.u8               q1, q1, q0                          \n"
                "   vadd.i32                q1, q1, q3                          \n"
                "   vmov                    r1, r2, q1[2], q1[0]                \n"
                "   cmp                     r1, #0                              \n"
                "   it                      ne                                  \n"
                "   udivne                  r1, r0, r1                          \n"
                "   cmp                     r2, #0                              \n"
                "   it                      ne                                  \n"
                "   udivne                  r2, r0, r2                          \n"
                "   vmov                    q0[2], q0[0], r1, r2                \n"
                "   vmov                    r1, r2, q1[3], q1[1]                \n"
                "   cmp                     r1, #0                              \n"
                "   it                      ne                                  \n"
                "   udivne                  r1, r0, r1                          \n"
                "   cmp                     r2, #0                              \n"
                "   it                      ne                                  \n"
                "   udivne                  r2, r0, r2                          \n"
                "   vmov                    q0[3], q0[1], r1, r2                \n"
                "   vmul.i32                q3, q0, q3                          \n"
                "   vsri.16                 q3, q3, #8                          \n"
                "   vsli.32                 q3, q3, #16                         \n"
                "   vdup.32                 q4, %[color]                        \n"
                "   vrmulh.u8               q4, q4, q3                          \n"
                "   vmvn                    q3, q3                              \n"
                "   vrmulh.u8               q2, q2, q3                          \n"
                "   vadd.i8                 q2, q4, q2                          \n"
                "   vsli.32                 q2, q1, #24                         \n"
                "   vstrb.8                 q2, [%[pDst]], #16                  \n"
                "   letp                    lr, 2b                              \n"
                "   1:                                                          \n"
                : [pDst] "+r"(pwTarget), [loopCnt] "+r"(blkCnt)
                : [color] "r"(color), [opa] "r"(opa), [mask] "r"(mask)
                : "q0", "q1", "q2", "q3", "q4", "r0", "r1", "r2", "r3", "memory");
            dst += dst_stride;
            mask += mask_stride;
        }
    }
    else if(opa != LV_OPA_COVER) {
        if(dst_stride != w) {
            for(lv_coord_t i = 0; i < h; i++) {
                uint32_t * pwTarget = (uint32_t *)dst;
                register unsigned blkCnt __asm("lr") = w << 2;
                __asm volatile(
                    "   .p2align 2                                                  \n"
                    "   vdup.8                  q0, %[opa]                          \n"
                    "   mov                     r0, #0xFF                           \n"
                    "   mul                     r0, r0, %[opa]                      \n"
                    "   vmvn                    q0, q0                              \n"
                    "   wlstp.8                 lr, %[loopCnt], 1f                  \n"
                    "   2:                                                          \n"
                    "   vldrb.8                 q2, [%[pDst]]                       \n"
                    "   vshr.u32                q1, q2, #24                         \n"
                    "   vrmulh.u8               q1, q1, q0                          \n"
                    "   vadd.i32                q1, q1, %[opa]                      \n"
                    "   vmov                    r1, r2, q1[2], q1[0]                \n"
                    "   cmp                     r1, #0                              \n"
                    "   it                      ne                                  \n"
                    "   udivne                  r1, r0, r1                          \n"
                    "   cmp                     r2, #0                              \n"
                    "   it                      ne                                  \n"
                    "   udivne                  r2, r0, r2                          \n"
                    "   vmov                    q3[2], q3[0], r1, r2                \n"
                    "   vmov                    r1, r2, q1[3], q1[1]                \n"
                    "   cmp                     r1, #0                              \n"
                    "   it                      ne                                  \n"
                    "   udivne                  r1, r0, r1                          \n"
                    "   cmp                     r2, #0                              \n"
                    "   it                      ne                                  \n"
                    "   udivne                  r2, r0, r2                          \n"
                    "   vmov                    q3[3], q3[1], r1, r2                \n"
                    "   vsri.16                 q3, q3, #8                          \n"
                    "   vsli.32                 q3, q3, #16                         \n"
                    "   vdup.32                 q4, %[color]                        \n"
                    "   vrmulh.u8               q4, q4, q3                          \n"
                    "   vmvn                    q3, q3                              \n"
                    "   vrmulh.u8               q2, q2, q3                          \n"
                    "   vadd.i8                 q2, q4, q2                          \n"
                    "   vsli.32                 q2, q1, #24                         \n"
                    "   vstrb.8                 q2, [%[pDst]], #16                  \n"
                    "   letp                    lr, 2b                              \n"
                    "   1:                                                          \n"
                    : [pDst] "+r"(pwTarget), [loopCnt] "+r"(blkCnt)
                    : [color] "r"(color), [opa] "r"(opa)
                    : "q0", "q1", "q2", "q3", "q4", "r0", "r1", "r2", "memory");
                dst += dst_stride;
            }
        }
        else {
            register unsigned blkCnt __asm("lr") = w * h << 2;
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   vdup.32                 q0, %[color]                        \n"
                "   vdup.8                  q1, %[opa]                          \n"
                "   vrmulh.u8               q0, q0, q1                          \n"
                "   vmvn                    q1, q1                              \n"
                "   wlstp.8                 lr, %[loopCnt], 1f                  \n"
                "   2:                                                          \n"
                "   vldrb.8                 q2, [%[pDst]]                       \n"
                "   vrmulh.u8               q2, q2, q1                          \n"
                "   vadd.i8                 q2, q0, q2                          \n"
                "   vstrb.8                 q2, [%[pDst]], #16                  \n"
                "   letp                    lr, 2b                              \n"
                "   1:                                                          \n"
                : [pDst] "+r"(dst), [loopCnt] "+r"(blkCnt)
                : [color] "r"(color), [opa] "r"(opa)
                : "q0", "q1", "q2", "memory");
        }
    }
    else if(dst_stride != w) {
        for(lv_coord_t i = 0; i < h; i++) {
            uint32_t * pwTarget = (uint32_t *)dst;
            register unsigned blkCnt __asm("lr") = w << 2;
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   vdup.32                 q0, %[color]                        \n"
                "   wlstp.8                 lr, %[loopCnt], 1f                  \n"
                "   2:                                                          \n"
                "   vstrb.8                 q0, [%[pDst]], #16                  \n"
                "   letp                    lr, 2b                              \n"
                "   1:                                                          \n"
                : [pDst] "+r"(pwTarget), [loopCnt] "+r"(blkCnt)
                : [color] "r"(color)
                : "q0", "memory");
            dst += dst_stride;
        }
    }
    else {
        register unsigned blkCnt __asm("lr") = w * h << 2;
        __asm volatile(
            "   .p2align 2                                                  \n"
            "   vdup.32                 q0, %[color]                        \n"
            "   wlstp.8                 lr, %[loopCnt], 1f                  \n"
            "   2:                                                          \n"
            "   vstrb.8                 q0, [%[pDst]], #16                  \n"
            "   letp                    lr, 2b                              \n"
            "   1:                                                          \n"
            : [pDst] "+r"(dst), [loopCnt] "+r"(blkCnt)
            : [color] "r"(color)
            : "q0", "memory");
    }
}

static void map_transp(lv_color_t * dst,
                       const lv_area_t * draw_area, lv_coord_t dst_stride, const lv_color_t * src,
                       lv_coord_t src_stride, lv_opa_t opa, const lv_opa_t * mask,
                       lv_coord_t mask_stride)
{
    lv_coord_t w = lv_area_get_width(draw_area);
    lv_coord_t h = lv_area_get_height(draw_area);
    if(mask) {
        for(lv_coord_t i = 0; i < h; i++) {
            uint32_t * phwSource = (uint32_t *)src;
            uint32_t * pwTarget = (uint32_t *)dst;
            register unsigned blkCnt __asm("lr") = w << 2;
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   mov                     r3, %[mask]                         \n"
                "   mov                     r0, #0xFE01                         \n"
                "   wlstp.8                 lr, %[loopCnt], 1f                  \n"
                "   2:                                                          \n"
                "   vldrb.8                 q2, [%[pDst]]                       \n"
                "   vldrb.u32               q3, [r3], #4                        \n"
                "   vldrb.8                 q4, [%[pSrc]], #16                  \n"
                "   vshr.u32                q0, q4, #24                         \n"
                "   vdup.32                 q1, %[opa]                          \n"
                "   vrmulh.u8               q0, q0, q1                          \n"
                "   vshr.u32                q1, q2, #24                         \n"
                "   vrmulh.u8               q3, q0, q3                          \n"
                "   vmvn                    q0, q3                              \n"
                "   vrmulh.u8               q1, q1, q0                          \n"
                "   vadd.i32                q1, q1, q3                          \n"
                "   vmov                    r1, r2, q1[2], q1[0]                \n"
                "   cmp                     r1, #0                              \n"
                "   it                      ne                                  \n"
                "   udivne                  r1, r0, r1                          \n"
                "   cmp                     r2, #0                              \n"
                "   it                      ne                                  \n"
                "   udivne                  r2, r0, r2                          \n"
                "   vmov                    q0[2], q0[0], r1, r2                \n"
                "   vmov                    r1, r2, q1[3], q1[1]                \n"
                "   cmp                     r1, #0                              \n"
                "   it                      ne                                  \n"
                "   udivne                  r1, r0, r1                          \n"
                "   cmp                     r2, #0                              \n"
                "   it                      ne                                  \n"
                "   udivne                  r2, r0, r2                          \n"
                "   vmov                    q0[3], q0[1], r1, r2                \n"
                "   vmul.i32                q3, q0, q3                          \n"
                "   vsri.16                 q3, q3, #8                          \n"
                "   vsli.32                 q3, q3, #16                         \n"
                "   vrmulh.u8               q4, q4, q3                          \n"
                "   vmvn                    q3, q3                              \n"
                "   vrmulh.u8               q2, q2, q3                          \n"
                "   vadd.i8                 q2, q4, q2                          \n"
                "   vsli.32                 q2, q1, #24                         \n"
                "   vstrb.8                 q2, [%[pDst]], #16                  \n"
                "   letp                    lr, 2b                              \n"
                "   1:                                                          \n"
                : [pSrc] "+r"(phwSource), [pDst] "+r"(pwTarget), [loopCnt] "+r"(blkCnt)
                : [opa] "r"(opa), [mask] "r"(mask)
                : "q0", "q1", "q2", "q3", "q4", "r0", "r1", "r2", "r3", "memory");
            src += src_stride;
            dst += dst_stride;
            mask += mask_stride;
        }
    }
    else if(opa != LV_OPA_COVER) {
        for(lv_coord_t i = 0; i < h; i++) {
            uint32_t * phwSource = (uint32_t *)src;
            uint32_t * pwTarget = (uint32_t *)dst;
            register unsigned blkCnt __asm("lr") = w << 2;
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   mov                     r0, #0xFE01                         \n"
                "   wlstp.8                 lr, %[loopCnt], 1f                  \n"
                "   2:                                                          \n"
                "   vldrb.8                 q2, [%[pDst]]                       \n"
                "   vldrb.8                 q4, [%[pSrc]], #16                  \n"
                "   vshr.u32                q3, q4, #24                         \n"
                "   vdup.32                 q1, %[opa]                          \n"
                "   vrmulh.u8               q3, q3, q1                          \n"
                "   vmvn                    q0, q3                              \n"
                "   vshr.u32                q1, q2, #24                         \n"
                "   vrmulh.u8               q1, q1, q0                          \n"
                "   vadd.i32                q1, q1, q3                          \n"
                "   vmov                    r1, r2, q1[2], q1[0]                \n"
                "   cmp                     r1, #0                              \n"
                "   it                      ne                                  \n"
                "   udivne                  r1, r0, r1                          \n"
                "   cmp                     r2, #0                              \n"
                "   it                      ne                                  \n"
                "   udivne                  r2, r0, r2                          \n"
                "   vmov                    q3[2], q3[0], r1, r2                \n"
                "   vmov                    r1, r2, q1[3], q1[1]                \n"
                "   cmp                     r1, #0                              \n"
                "   it                      ne                                  \n"
                "   udivne                  r1, r0, r1                          \n"
                "   cmp                     r2, #0                              \n"
                "   it                      ne                                  \n"
                "   udivne                  r2, r0, r2                          \n"
                "   vmov                    q3[3], q3[1], r1, r2                \n"
                "   vsri.16                 q3, q3, #8                          \n"
                "   vsli.32                 q3, q3, #16                         \n"
                "   vrmulh.u8               q4, q4, q3                          \n"
                "   vmvn                    q3, q3                              \n"
                "   vrmulh.u8               q2, q2, q3                          \n"
                "   vadd.i8                 q2, q4, q2                          \n"
                "   vsli.32                 q2, q1, #24                         \n"
                "   vstrb.8                 q2, [%[pDst]], #16                  \n"
                "   letp                    lr, 2b                              \n"
                "   1:                                                          \n"
                : [pSrc] "+r"(phwSource), [pDst] "+r"(pwTarget), [loopCnt] "+r"(blkCnt)
                : [opa] "r"(opa)
                : "q0", "q1", "q2", "q3", "q4", "r0", "r1", "r2", "memory");
            src += src_stride;
            dst += dst_stride;
        }
    }
    else {
        for(lv_coord_t i = 0; i < h; i++) {
            uint32_t * phwSource = (uint32_t *)src;
            uint32_t * pwTarget = (uint32_t *)dst;
            register unsigned blkCnt __asm("lr") = w << 2;
            __asm volatile(
                "   .p2align 2                                                  \n"
                "   wlstp.8                 lr, %[loopCnt], 1f                  \n"
                "   2:                                                          \n"
                "   vldrb.8                 q0, [%[pSrc]], #16                  \n"
                "   vstrb.8                 q0, [%[pDst]], #16                  \n"
                "   letp                    lr, 2b                              \n"
                "   1:                                                          \n"
                : [pSrc] "+r"(phwSource), [pDst] "+r"(pwTarget),
                [loopCnt] "+r"(blkCnt)
                :
                : "q0", "memory");
            src += src_stride;
            dst += dst_stride;
        }
    }
}

/****************************************************************************
 * Global Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mve_draw_blend
 *
 * Description:
 *   Blend an area to a display buffer.
 *
 * Input Parameters:
 * @param draw_ctx draw context (refer to LVGL 8.2 changelog)
 * @param dsc blend descriptor
 *
 * Returned Value:
 * @return LV_RES_OK on success, LV_RES_INV on failure.
 *
 ****************************************************************************/
void mve_draw_blend(lv_draw_ctx_t * draw_ctx, const lv_draw_sw_blend_dsc_t * dsc)
{
    lv_gpu_ctx_t * gpu = draw_ctx->user_data;

    /* Wait for gpu manager draw ctx to finish drawing */
    lv_draw_wait_for_finish(gpu->main_draw_ctx);

    if(dsc->blend_mode != LV_BLEND_MODE_NORMAL) {
        LV_GPU_LOG_WARN("only normal blend mode acceleration supported at the moment");
        return;
    }

    lv_disp_t * disp = _lv_refr_get_disp_refreshing();
    if(disp->driver->set_px_cb) {
        LV_GPU_LOG_WARN("drawing with set_px_cb can't be accelerated");
        return;
    }

    if(dsc->mask_buf && dsc->mask_res == LV_DRAW_MASK_RES_TRANSP) {
        gpu->draw_ok = true;
        return;
    }

    lv_area_t draw_area;
    const lv_area_t * disp_area = draw_ctx->buf_area;
    if(!_lv_area_intersect(&draw_area, dsc->blend_area, draw_ctx->clip_area)) {
        gpu->draw_ok = true;
        return;
    }

    lv_coord_t src_stride = 0;
    lv_coord_t dst_stride = lv_area_get_width(disp_area);
    lv_coord_t mask_stride = 0;

    lv_color_t * dst = draw_ctx->buf;
    dst += dst_stride * (draw_area.y1 - disp_area->y1) + draw_area.x1 - disp_area->x1;
    const lv_color_t * src = dsc->src_buf;
    if(src) {
        src_stride = lv_area_get_width(dsc->blend_area);
        src += src_stride * (draw_area.y1 - dsc->blend_area->y1) + draw_area.x1 - dsc->blend_area->x1;
    }
    const lv_opa_t * mask = dsc->mask_res == LV_DRAW_MASK_RES_FULL_COVER ? NULL : dsc->mask_buf;
    if(mask) {
        mask_stride = lv_area_get_width(dsc->mask_area);
        mask += mask_stride * (draw_area.y1 - dsc->mask_area->y1) + draw_area.x1 - dsc->mask_area->x1;
    }
    if(disp->driver->screen_transp == 0) {
        if(src == NULL) {
            fill_normal(dst, &draw_area, dst_stride, dsc->color, dsc->opa, mask,
                        mask_stride);
        }
        else {
            map_normal(dst, &draw_area, dst_stride, src, src_stride, dsc->opa, mask,
                       mask_stride);
        }
    }
    else {
        if(src == NULL) {
            fill_transp(dst, &draw_area, dst_stride, dsc->color, dsc->opa, mask,
                        mask_stride);
        }
        else {
            map_transp(dst, &draw_area, dst_stride, src, src_stride, dsc->opa, mask,
                       mask_stride);
        }
    }

    gpu->draw_ok = true;
}

#endif /* CONFIG_LV_GPU_USE_ARM_MVE */
