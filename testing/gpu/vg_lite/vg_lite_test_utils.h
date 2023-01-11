/****************************************************************************
 * apps/testing/gpu/vg_lite/vg_lite_test_utils.h
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

#ifndef __APPS_TESTING_GPU_VG_LITE_VG_LITE_TEST_UTILS_H
#define __APPS_TESTING_GPU_VG_LITE_VG_LITE_TEST_UTILS_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <vg_lite.h>
#include "../gpu_test.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VG_LITE_IS_ERROR(err) (err > 0)

#define VG_LITE_CHECK_ERROR(func) \
do { \
    error = func; \
    if (VG_LITE_IS_ERROR(error)) \
      { \
        GPU_LOG_ERROR("Execute '" #func "' error(%d): %s", \
                      (int)error, vg_lite_get_error_type_string(error)); \
        goto error_handler; \
      } \
} while (0)

#define VG_LITE_ALIGN(number, align_bytes)    \
        (((number) + ((align_bytes) - 1)) & ~((align_bytes) - 1))

#define VG_LITE_CTX      ((FAR struct vg_lite_test_ctx_s*)((ctx)->user_data))
#define VG_LITE_SRC_BUF  (&(VG_LITE_CTX->src_buffer))
#define VG_LITE_DEST_BUF (&(VG_LITE_CTX->dest_buffer))

#define VG_LITE_FB_WIDTH  (VG_LITE_DEST_BUF->width)
#define VG_LITE_FB_HEIGHT (VG_LITE_DEST_BUF->height)

#define VG_LITE_USE_DIRECT_MODE 1

#if VG_LITE_USE_DIRECT_MODE
#define VG_LITE_FB_UPDATE() gpu_fb_update(ctx)
#else
#define VG_LITE_FB_UPDATE() vg_lite_draw_fb_center(ctx)
#endif

/* 3(MOVE) + 3(LINE) * 3 + 7(CUBIC) * 4 + 1(CLOSE/END) */
#define VG_LITE_RECT_PATH_LEN_MAX 41

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct vg_lite_test_ctx_s
{
  FAR vg_lite_buffer_t src_buffer;
  FAR vg_lite_buffer_t dest_buffer;
};

struct vg_lite_area_s
{
  int x1;
  int y1;
  int x2;
  int y2;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Name: vg_lite_get_error_type_string
 ****************************************************************************/

FAR const char *vg_lite_get_error_type_string(vg_lite_error_t error);

/****************************************************************************
 * Name: vg_lite_get_buffer_format_string
 ****************************************************************************/

FAR const char *vg_lite_get_buffer_format_string(
  vg_lite_buffer_format_t format);

/****************************************************************************
 * Name: vg_lite_get_filter_string
 ****************************************************************************/

FAR const char *vg_lite_get_filter_string(vg_lite_filter_t filter);

/****************************************************************************
 * Name: vg_lite_get_blend_string
 ****************************************************************************/

FAR const char *vg_lite_get_blend_string(vg_lite_blend_t blend);

/****************************************************************************
 * Name: vg_lite_get_global_alpha_string
 ****************************************************************************/

FAR const char *vg_lite_get_global_alpha_string(
  vg_lite_global_alpha_t global_alpha);

/****************************************************************************
 * Name: vg_lite_get_feature_string
 ****************************************************************************/

FAR const char *vg_lite_get_feature_string(vg_lite_feature_t feature);

/****************************************************************************
 * Name: vg_lite_dump_buffer_info
 ****************************************************************************/

void vg_lite_dump_buffer_info(FAR const char *name,
                              FAR const vg_lite_buffer_t *buffer);

/****************************************************************************
 * Name: vg_lite_draw_fb
 ****************************************************************************/

void vg_lite_draw_fb(FAR struct gpu_test_context_s *ctx,
                     FAR vg_lite_buffer_t *buffer,
                     int x_pos, int y_pos);

/****************************************************************************
 * Name: vg_lite_draw_fb_center
 ****************************************************************************/

void vg_lite_draw_fb_center(FAR struct gpu_test_context_s *ctx,
                            FAR vg_lite_buffer_t *buffer);

/****************************************************************************
 * Name: vg_lite_buffer_init
 ****************************************************************************/

void vg_lite_buffer_init(FAR vg_lite_buffer_t *buffer);

/****************************************************************************
 * Name: vg_lite_color_premult
 ****************************************************************************/

vg_lite_error_t vg_lite_color_premult(FAR vg_lite_buffer_t *buffer);

/****************************************************************************
 * Name: vg_lite_create_image
 ****************************************************************************/

vg_lite_error_t vg_lite_create_image(FAR vg_lite_buffer_t *image,
                                     int width,
                                     int height,
                                     vg_lite_buffer_format_t fmt);

/****************************************************************************
 * Name: vg_lite_delete_image
 ****************************************************************************/

void vg_lite_delete_image(FAR vg_lite_buffer_t *image);

/****************************************************************************
 * Name: vg_lite_get_format_bytes
 ****************************************************************************/

void vg_lite_get_format_bytes(vg_lite_buffer_format_t format,
                              FAR uint32_t *mul,
                              FAR uint32_t *div,
                              FAR uint32_t *bytes_align);

/****************************************************************************
 * Name: vg_lite_init_custon_buffer
 ****************************************************************************/

vg_lite_error_t vg_lite_init_custom_buffer(
  FAR vg_lite_buffer_t *buffer,
  FAR void *ptr,
  int32_t width,
  int32_t height,
  vg_lite_buffer_format_t format);

/****************************************************************************
 * Name: vg_lite_init_matrix
 ****************************************************************************/

vg_lite_matrix_t vg_lite_init_matrix(float x, float y,
                                     float angle, float scale,
                                     int pivot_x, int pivot_y);

/****************************************************************************
 * Name: vg_lite_get_coord_format_size
 ****************************************************************************/

size_t vg_lite_get_coord_format_size(vg_lite_format_t format);

/****************************************************************************
 * Name: vg_lite_fill_round_rect_path
 ****************************************************************************/

int vg_lite_fill_round_rect_path(
  FAR int32_t *path,
  FAR const struct vg_lite_area_s *rect,
  uint16_t radius);

/****************************************************************************
 * Name: vg_lite_test_report_start
 ****************************************************************************/

void vg_lite_test_report_start(FAR struct gpu_test_context_s *ctx);

/****************************************************************************
 * Name: vg_lite_test_report_finish
 ****************************************************************************/

void vg_lite_test_report_finish(FAR struct gpu_test_context_s *ctx);

/****************************************************************************
 * Name: vg_lite_test_report_write
 ****************************************************************************/

void vg_lite_test_report_write(FAR struct gpu_test_context_s *ctx,
                               const char *testcase_str,
                               const char *instructions_str,
                               const char *remark_str,
                               bool is_passed);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_TESTING_GPU_VG_LITE_VG_LITE_TEST_UTILS_H */
