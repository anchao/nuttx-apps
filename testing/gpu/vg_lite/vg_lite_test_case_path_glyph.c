/****************************************************************************
 * apps/testing/gpu/vg_lite/vg_lite_test_case_path_glyph.c
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
 * Name: vg_lite_test_path_glyph
 ****************************************************************************/

vg_lite_error_t vg_lite_test_path_glyph(
  FAR struct gpu_test_context_s *ctx)
{
  vg_lite_error_t error = VG_LITE_SUCCESS;
  vg_lite_matrix_t matrix;

  vg_lite_path_t path;
  vg_lite_init_path(
    &path,
    VG_LITE_S16,
    VG_LITE_HIGH,
    sizeof(glphy_u9f8d_path_data),
    (FAR void *)glphy_u9f8d_path_data, 0, 0, 4000, 4000);

  vg_lite_identity(&matrix);
  vg_lite_scale(0.05, 0.05, &matrix);

  VG_LITE_CHECK_ERROR(vg_lite_draw(
    VG_LITE_DEST_BUF,
    &path,
    VG_LITE_FILL_EVEN_ODD,
    &matrix,
    VG_LITE_BLEND_SRC_OVER,
    0xff0a59f7));

  GPU_PERF_RENDER_START();
  VG_LITE_CHECK_ERROR(vg_lite_finish());
  GPU_PERF_RENDER_STOP();

error_handler:
  return error;
}
