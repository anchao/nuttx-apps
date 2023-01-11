/****************************************************************************
 * apps/testing/gpu/gpu_screenshot.c
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

#include "gpu_screenshot.h"
#include "gpu_utils.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef CONFIG_LIB_PNG
#include <png.h>
#define SCREENSHOT_EXT ".png"
#else
#define SCREENSHOT_EXT ".raw"
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifdef CONFIG_LIB_PNG

/****************************************************************************
 * Name: save_img_file
 ****************************************************************************/

static int save_img_file(FAR struct gpu_test_context_s *ctx,
                         FAR const char *path)
{
  png_image image;
  int retval;

  /* Construct the PNG image structure. */

  memset(&image, 0, sizeof(image));

  image.version = PNG_IMAGE_VERSION;
  image.width   = ctx->xres;
  image.height  = ctx->yres;
  image.format  = PNG_FORMAT_BGRA;

  /* Write the PNG image. */

  retval = png_image_write_to_file(&image, path, 0, ctx->fbmem,
                                   ctx->stride, NULL);

  return retval;
}
#else

/****************************************************************************
 * Name: save_img_file
 ****************************************************************************/

static int save_img_file(FAR struct gpu_test_context_s *ctx,
                         FAR const char *path)
{
  size_t len = ctx->stride * ctx->yres;
  size_t written = 0;
  int retval = OK;
  int fd;

  fd = open(path, O_CREAT | O_WRONLY | O_CLOEXEC, 0666);
  if (fd < 0)
    {
      GPU_LOG_ERROR("Failed to open file %s", path);
      return ERROR;
    }

  while (written < len)
    {
      ssize_t ret = write(fd, ctx->fbmem + written, len - written);

      if (ret < 0)
        {
          GPU_LOG_ERROR("write failed: %d", errno);
          retval = ERROR;
          break;
        }

      written += ret;
    }

  close(fd);

  return OK;
}

#endif /* CONFIG_LIB_PNG */

/****************************************************************************
 * Name: gpu_screenshot
 ****************************************************************************/

int gpu_screenshot(FAR struct gpu_test_context_s *ctx, FAR const char *name)
{
  GPU_ASSERT_NULL(ctx);
  GPU_ASSERT_NULL(name);

  int retval;
  char path[PATH_MAX];
  char time_str[64];

  if (!ctx->param.screenshot_en)
    {
      return OK;
    }

  GPU_LOG_INFO("Taking screenshot of '%s' ...", name);

  gpu_get_localtime_str(time_str, sizeof(time_str));
  snprintf(path, sizeof(path), "%s/screenshot_%s_%s." SCREENSHOT_EXT,
           ctx->param.output_dir,
           name, time_str);

  retval = save_img_file(ctx, path);

  if (retval > 0)
    {
      GPU_LOG_INFO("Screenshot saved to %s", path);
    }
  else
    {
      GPU_LOG_ERROR("Failed to save screenshot: %d", retval);
    }

  return retval;
}
