/****************************************************************************
 * apps/testing/gpu/vg_lite/vg_lite_test_case_path.c
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

#include "vg_lite_test_case.h"
#include "resource/tiger_paths.h"
#include "resource/glphy_paths.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: vg_lite_test_path_rect
 ****************************************************************************/

vg_lite_error_t vg_lite_test_path_rect(
  FAR struct gpu_test_context_s *ctx)
{
  vg_lite_error_t error = VG_LITE_SUCCESS;
  vg_lite_matrix_t matrix;
  int32_t path_data[VG_LITE_RECT_PATH_LEN_MAX];
  struct vg_lite_area_s area =
    {
      0, 0, 240 - 1, 320 - 1
    };

  vg_lite_path_t path;

  GPU_PERF_PREPARE_START();
  vg_lite_identity(&matrix);
  int path_len = vg_lite_fill_round_rect_path(path_data, &area, 0);

  vg_lite_init_path(
    &path,
    VG_LITE_S32,
    VG_LITE_HIGH,
    path_len * sizeof(int32_t),
    path_data,
    0, 0,
    VG_LITE_FB_WIDTH, VG_LITE_FB_HEIGHT);

  VG_LITE_CHECK_ERROR(vg_lite_draw(
    VG_LITE_DEST_BUF,
    &path,
    VG_LITE_FILL_EVEN_ODD,
    &matrix,
    VG_LITE_BLEND_SRC_OVER,
    0xff0000ff));
  GPU_PERF_PREPARE_STOP();

  GPU_PERF_RENDER_START();
  VG_LITE_CHECK_ERROR(vg_lite_finish());
  GPU_PERF_RENDER_STOP();

error_handler:
  return error;
}

/****************************************************************************
 * Name: vg_lite_test_path_round_rect
 ****************************************************************************/

vg_lite_error_t vg_lite_test_path_round_rect(
  FAR struct gpu_test_context_s *ctx)
{
  vg_lite_error_t error = VG_LITE_SUCCESS;
  vg_lite_matrix_t matrix;
  int32_t path_data[VG_LITE_RECT_PATH_LEN_MAX];

  struct vg_lite_area_s area =
    {
      0, 0, 240 - 1, 320 - 1
    };

  vg_lite_path_t path;

  GPU_PERF_PREPARE_START();
  vg_lite_identity(&matrix);
  int path_len = vg_lite_fill_round_rect_path(path_data, &area, 20);

  vg_lite_init_path(
    &path,
    VG_LITE_S32,
    VG_LITE_HIGH,
    path_len * sizeof(int32_t),
    path_data,
    0, 0,
    VG_LITE_FB_WIDTH, VG_LITE_FB_HEIGHT);

  VG_LITE_CHECK_ERROR(vg_lite_draw(
    VG_LITE_DEST_BUF,
    &path,
    VG_LITE_FILL_EVEN_ODD,
    &matrix,
    VG_LITE_BLEND_SRC_OVER,
    0xff0000ff));
  GPU_PERF_PREPARE_STOP();

  GPU_PERF_RENDER_START();
  VG_LITE_CHECK_ERROR(vg_lite_finish());
  GPU_PERF_RENDER_STOP();

error_handler:
  return error;
}

/****************************************************************************
 * Name: vg_lite_test_path_tiger
 ****************************************************************************/

vg_lite_error_t vg_lite_test_path_tiger(
  FAR struct gpu_test_context_s *ctx)
{
  vg_lite_error_t error = VG_LITE_SUCCESS;
  vg_lite_matrix_t matrix;
  int i;

  GPU_PERF_PREPARE_START();
  vg_lite_identity(&matrix);
  vg_lite_scale(4, 4, &matrix);

  for (i = 0; i < tiger_path_count; i++)
    {
      VG_LITE_CHECK_ERROR(vg_lite_draw(
        VG_LITE_DEST_BUF,
        &tiger_path[i],
        VG_LITE_FILL_EVEN_ODD,
        &matrix,
        VG_LITE_BLEND_SRC_OVER,
        tiger_color_data[i]));
    }

  GPU_PERF_PREPARE_STOP();

  GPU_PERF_RENDER_START();
  VG_LITE_CHECK_ERROR(vg_lite_finish());
  GPU_PERF_RENDER_STOP();

error_handler:
  return error;
}
