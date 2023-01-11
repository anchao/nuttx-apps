/****************************************************************************
 * apps/testing/gpu/vg_lite/vg_lite_test_case_misc.c
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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define IMG_WIDTH  (ctx->param.img_width)
#define IMG_HEIGHT (ctx->param.img_height)

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
 * Name: vg_lite_test_fill
 ****************************************************************************/

vg_lite_error_t vg_lite_test_fill(FAR struct gpu_test_context_s *ctx)
{
  vg_lite_error_t error = VG_LITE_SUCCESS;

  /* Clear the buffer with blue. */

  GPU_PERF_PREPARE_START();
  VG_LITE_CHECK_ERROR(vg_lite_clear(VG_LITE_DEST_BUF, NULL, 0xffff0000));
  GPU_PERF_PREPARE_STOP();

  GPU_PERF_RENDER_START();
  VG_LITE_CHECK_ERROR(vg_lite_finish());
  GPU_PERF_RENDER_STOP();

error_handler:
  return error;
}

/****************************************************************************
 * Name: vg_lite_test_blit
 ****************************************************************************/

vg_lite_error_t vg_lite_test_blit(FAR struct gpu_test_context_s *ctx)
{
  vg_lite_error_t error = VG_LITE_SUCCESS;
  vg_lite_buffer_t *image = VG_LITE_SRC_BUF;
  gpu_color32_t *dest;
  VG_LITE_CHECK_ERROR(vg_lite_create_image(
    image, IMG_WIDTH, IMG_HEIGHT, VG_LITE_BGRA8888));

  /* Draw image */

  for (int y = 0; y < image->height; y++)
    {
      dest = image->memory + y * image->stride;

      for (int x = 0; x < image->width; x++)
        {
          dest->ch.alpha = y & 0xff;
          dest->ch.red = x & 0xff;
          dest->ch.green = (x << 1) & 0xff;
          dest->ch.blue = (x << 2) & 0xff;
          dest++;
        }
    }

  GPU_PERF_PREPARE_START();
  VG_LITE_CHECK_ERROR(vg_lite_blit(
    VG_LITE_DEST_BUF,
    image,
    NULL,
    VG_LITE_BLEND_NONE,
    0,
    VG_LITE_FILTER_POINT));
  GPU_PERF_PREPARE_STOP();

  GPU_PERF_RENDER_START();
  VG_LITE_CHECK_ERROR(vg_lite_finish());
  GPU_PERF_RENDER_STOP();

error_handler:
  vg_lite_delete_image(image);

  return error;
}

/****************************************************************************
 * Name: vg_lite_test_global_alpha
 ****************************************************************************/

vg_lite_error_t vg_lite_test_global_alpha(FAR struct gpu_test_context_s *ctx)
{
  vg_lite_error_t error = VG_LITE_SUCCESS;
  vg_lite_matrix_t matrix;

  const vg_lite_global_alpha_t global_alpha_array[] =
  {
    VG_LITE_NORMAL,
    VG_LITE_GLOBAL,
    VG_LITE_SCALED
  };

  vg_lite_buffer_t *image = VG_LITE_SRC_BUF;
  VG_LITE_CHECK_ERROR(vg_lite_create_image(
    image, IMG_WIDTH, IMG_HEIGHT, VG_LITE_BGRA8888));

  /* Draw image */

  for (int y = 0; y < image->height; y++)
    {
      uint8_t alpha = y * 0xff / image->height;
      gpu_color32_t *dst = image->memory + y * image->stride;

      for (int x = 0; x < image->width; x++)
        {
          dst->ch.alpha = alpha;
          dst->ch.red = 0xff;
          dst->ch.red = dst->ch.red * alpha / 0xff;
          dst->ch.green = dst->ch.green * alpha / 0xff;
          dst->ch.blue = dst->ch.blue * alpha / 0xff;
          dst++;
        }
    }

  vg_lite_identity(&matrix);

  for (int i = 0 ; i < GPU_ARRAY_SIZE(global_alpha_array); i++)
    {
      vg_lite_global_alpha_t global_alpha = global_alpha_array[i];
      GPU_LOG_INFO("global_alpha: %d (%s)", global_alpha,
                   vg_lite_get_global_alpha_string(global_alpha));
      GPU_PERF_PREPARE_START();
      VG_LITE_CHECK_ERROR(
        vg_lite_set_image_global_alpha(global_alpha, 0x55));
      VG_LITE_CHECK_ERROR(
        vg_lite_set_dest_global_alpha(global_alpha, 0x55));

      VG_LITE_CHECK_ERROR(vg_lite_blit(
        VG_LITE_DEST_BUF,
        image,
        &matrix,
        VG_LITE_BLEND_SRC_OVER,
        0,
        VG_LITE_FILTER_POINT));
      GPU_PERF_PREPARE_STOP();

      GPU_PERF_RENDER_START();
      VG_LITE_CHECK_ERROR(vg_lite_finish());
      GPU_PERF_RENDER_STOP();

      VG_LITE_FB_UPDATE();
    }

error_handler:
  vg_lite_delete_image(image);

  return error;
}
