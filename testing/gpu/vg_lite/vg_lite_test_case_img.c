/****************************************************************************
 * apps/testing/gpu/vg_lite/vg_lite_test_case_img.c
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

enum img_transform_mode_e
{
  IMG_TRANSFORM_NONE = 0,
  IMG_TRANSFORM_OFFSET,
  IMG_TRANSFORM_ROTATE,
  IMG_TRANSFORM_SCALE,
  IMG_TRANSFORM_MAX,
};

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
 * Name: vg_lite_test_image_transform
 ****************************************************************************/

static vg_lite_error_t vg_lite_test_image_transform(
  FAR struct gpu_test_context_s *ctx,
  vg_lite_blend_t blend,
  vg_lite_filter_t filter,
  enum img_transform_mode_e tf_mode,
  int rotation)
{
  char tf_str[64];

  vg_lite_error_t error = VG_LITE_SUCCESS;

  vg_lite_matrix_t matrix;
  vg_lite_identity(&matrix);
  switch (tf_mode)
  {
    case IMG_TRANSFORM_NONE:
      strncpy(tf_str, "None", sizeof(tf_str) - 1);
      break;
    case IMG_TRANSFORM_OFFSET:
          vg_lite_translate(64, 64, &matrix);
          strncpy(tf_str, "Offset (X64 Y64)", sizeof(tf_str) - 1);
      break;
    case IMG_TRANSFORM_ROTATE:
          matrix = vg_lite_init_matrix(0, 0, rotation, 1.0,
                                       IMG_WIDTH / 2, IMG_HEIGHT / 2);
          snprintf(tf_str, sizeof(tf_str), "Rotate (%d')", rotation);
      break;
    case IMG_TRANSFORM_SCALE:
          vg_lite_scale(1.7, 1.7, &matrix);
          strncpy(tf_str, "Scale (X1.7 Y1.7)", sizeof(tf_str) - 1);
      break;
    default:
      break;
  }

  GPU_LOG_INFO("Transform: %s", tf_str);
  GPU_LOG_INFO("Blend: 0x%x (%s)", blend, vg_lite_get_blend_string(blend));
  GPU_LOG_INFO("Filter: 0x%x (%s)", filter,
               vg_lite_get_filter_string(filter));

  GPU_PERF_PREPARE_START();
  VG_LITE_CHECK_ERROR(vg_lite_blit(
    VG_LITE_DEST_BUF,
    VG_LITE_SRC_BUF,
    &matrix,
    blend,
    0,
    filter));
  GPU_PERF_PREPARE_STOP();

  GPU_PERF_RENDER_START();
  VG_LITE_CHECK_ERROR(vg_lite_finish());
  GPU_PERF_RENDER_STOP();

error_handler:

  char instructions_str[256];
  snprintf(instructions_str, sizeof(instructions_str), "Transform: %s %s %s",
            tf_str,
            vg_lite_get_blend_string(blend),
            vg_lite_get_filter_string(filter));

  vg_lite_test_report_write(
    ctx,
    "image",
    instructions_str,
    VG_LITE_IS_ERROR(error) ? vg_lite_get_error_type_string(error) : NULL,
    error == VG_LITE_SUCCESS);
  VG_LITE_FB_UPDATE();
  return error;
}

/****************************************************************************
 * Name: vg_lite_test_image_transform
 ****************************************************************************/

static vg_lite_error_t vg_lite_test_image_combination(
  FAR struct gpu_test_context_s *ctx)
{
  vg_lite_error_t error = VG_LITE_SUCCESS;
  static const vg_lite_blend_t blend_arr[] =
  {
    VG_LITE_BLEND_NONE,
    VG_LITE_BLEND_SRC_OVER,
  };

  static const vg_lite_filter_t filter_arr[] =
  {
    VG_LITE_FILTER_POINT,
    VG_LITE_FILTER_LINEAR,
    VG_LITE_FILTER_BI_LINEAR,
  };

  static const int rotation[] =
    {
      0,
      45,
      89,
      90,
      180,
    };

  for (int b = 0; b < GPU_ARRAY_SIZE(blend_arr); b++)
    {
      for (int f = 0; f < GPU_ARRAY_SIZE(filter_arr); f++)
        {
          for (int tf = IMG_TRANSFORM_NONE; tf < IMG_TRANSFORM_MAX; tf++)
            {
              if (tf == IMG_TRANSFORM_ROTATE)
                {
                  for (int r = 0; r < GPU_ARRAY_SIZE(rotation); r++)
                    {
                      VG_LITE_CHECK_ERROR(vg_lite_test_image_transform(
                        ctx,
                        blend_arr[b], filter_arr[f], tf, rotation[r]));
                    }
                }
              else
                {
                  VG_LITE_CHECK_ERROR(vg_lite_test_image_transform(
                      ctx,
                      blend_arr[b], filter_arr[f], tf, 0));
                }
            }
        }
    }

error_handler:
  return error;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: vg_lite_test_image_bgra8888
 ****************************************************************************/

vg_lite_error_t vg_lite_test_image_bgra8888(
  FAR struct gpu_test_context_s *ctx)
{
  vg_lite_error_t error = VG_LITE_SUCCESS;
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

  VG_LITE_CHECK_ERROR(
    vg_lite_test_image_combination(ctx));

error_handler:
  vg_lite_delete_image(image);

  return error;
}

/****************************************************************************
 * Name: vg_lite_test_image_bgra8888
 ****************************************************************************/

vg_lite_error_t vg_lite_test_image_indexed8(
  FAR struct gpu_test_context_s *ctx)
{
  vg_lite_error_t error = VG_LITE_SUCCESS;
  vg_lite_buffer_t *image = VG_LITE_SRC_BUF;
  VG_LITE_CHECK_ERROR(vg_lite_create_image(
    image, IMG_WIDTH, IMG_HEIGHT, VG_LITE_INDEX_8));

  VG_LITE_CHECK_ERROR(
    vg_lite_test_image_combination(ctx));

error_handler:
  vg_lite_delete_image(image);

  return error;
}

/****************************************************************************
 * Name: vg_lite_test_image_bgra5658
 ****************************************************************************/

vg_lite_error_t vg_lite_test_image_bgra5658(
  FAR struct gpu_test_context_s *ctx)
{
  vg_lite_error_t error = VG_LITE_SUCCESS;

  /* Prepare image */

  vg_lite_buffer_t *image = VG_LITE_SRC_BUF;
  VG_LITE_CHECK_ERROR(vg_lite_create_image(
    image, IMG_WIDTH, IMG_HEIGHT, VG_LITE_BGRA5658));

  /* Generate image */

  for (int y = 0; y < image->height; y++)
    {
      uint8_t alpha = y * 0xff / image->height;
      gpu_color16_alpha_t *dst = image->memory + y * image->stride;

      for (int x = 0; x < image->width; x++)
        {
          dst->alpha = alpha;
          dst->color.ch.red = 31;
          dst->color.ch.red = dst->color.ch.red * alpha / 0xff;
          dst->color.ch.green = dst->color.ch.green * alpha / 0xff;
          dst->color.ch.blue = dst->color.ch.blue * alpha / 0xff;
          dst++;
        }
    }

  VG_LITE_CHECK_ERROR(
    vg_lite_test_image_combination(ctx));

error_handler:
  vg_lite_delete_image(image);

  return error;
}
