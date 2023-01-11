/****************************************************************************
 * apps/testing/gpu/vg_lite/vg_lite_test_case.h
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

#ifndef __APPS_TESTING_GPU_VG_LITE_VG_LITE_CASE_H
#define __APPS_TESTING_GPU_VG_LITE_VG_LITE_CASE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "vg_lite_test_utils.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

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
 * Name: vg_lite_test_image_bgra8888
 ****************************************************************************/

vg_lite_error_t vg_lite_test_image_bgra8888(
  FAR struct gpu_test_context_s *ctx);

/****************************************************************************
 * Name: vg_lite_test_image_bgra5658
 ****************************************************************************/

vg_lite_error_t vg_lite_test_image_bgra5658(
  FAR struct gpu_test_context_s *ctx);

/****************************************************************************
 * Name: vg_lite_test_image_indexed8
 ****************************************************************************/

vg_lite_error_t vg_lite_test_image_indexed8(
  FAR struct gpu_test_context_s *ctx);

/****************************************************************************
 * Name: vg_lite_test_rect_path
 ****************************************************************************/

vg_lite_error_t vg_lite_test_path_rect(
  FAR struct gpu_test_context_s *ctx);

/****************************************************************************
 * Name: vg_lite_test_path_round_rect
 ****************************************************************************/

vg_lite_error_t vg_lite_test_path_round_rect(
  FAR struct gpu_test_context_s *ctx);

/****************************************************************************
 * Name: vg_lite_test_path_glyph
 ****************************************************************************/

vg_lite_error_t vg_lite_test_path_glyph(
  FAR struct gpu_test_context_s *ctx);

/****************************************************************************
 * Name: vg_lite_test_path_tiger
 ****************************************************************************/

vg_lite_error_t vg_lite_test_path_tiger(
  FAR struct gpu_test_context_s *ctx);

/****************************************************************************
 * Name: vg_lite_test_fill
 ****************************************************************************/

vg_lite_error_t vg_lite_test_fill(
  FAR struct gpu_test_context_s *ctx);

/****************************************************************************
 * Name: vg_lite_test_blit
 ****************************************************************************/

vg_lite_error_t vg_lite_test_blit(
  FAR struct gpu_test_context_s *ctx);

/****************************************************************************
 * Name: vg_lite_test_global_alpha
 ****************************************************************************/

vg_lite_error_t vg_lite_test_global_alpha(
  FAR struct gpu_test_context_s *ctx);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_TESTING_GPU_VG_LITE_VG_LITE_CASE_H */
