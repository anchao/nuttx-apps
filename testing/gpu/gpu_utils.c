/****************************************************************************
 * apps/testing/gpu/gpu_utils.c
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

#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "gpu_test.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define CPU_CORE_CLOCK_MHZ 200

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: gpu_sw_color32_mix
 ****************************************************************************/

gpu_color32_t gpu_sw_color32_mix(gpu_color32_t c1,
                                 gpu_color32_t c2,
                                 uint8_t mix)
{
  gpu_color32_t ret;
  ret.ch.red = ((uint16_t)c1.ch.red * mix
                        + c2.ch.red * (255 - mix)) / 255;
  ret.ch.green = ((uint16_t)c1.ch.green * mix
                          + c2.ch.green * (255 - mix)) / 255;
  ret.ch.blue = ((uint16_t)c1.ch.blue * mix
                         + c2.ch.blue * (255 - mix)) / 255;
  ret.ch.alpha = 0xff;
  return ret;
}

/****************************************************************************
 * Name: gpu_sw_fill_screen
 ****************************************************************************/

void gpu_sw_fill_screen(FAR struct gpu_test_context_s *ctx,
                        gpu_color32_t color)
{
  GPU_ASSERT_NULL(ctx);
  FAR gpu_color32_t *dest = ctx->fbmem;
  size_t size = ctx->xres * ctx->yres;

  while (size--)
    {
      *dest++ = color;
    }
}

/****************************************************************************
 * Name: gpu_bgra5658_to_bgra8888
 ****************************************************************************/

void gpu_sw_bgra5658_to_bgra8888(FAR gpu_color32_t *dest,
                                 FAR const gpu_color16_alpha_t *src,
                                 size_t size)
{
  GPU_ASSERT_NULL(dest);
  GPU_ASSERT_NULL(src);

  while (size--)
    {
      dest->ch.red = src->color.ch.red << 3;
      dest->ch.green = src->color.ch.green << 2;
      dest->ch.blue = src->color.ch.blue << 3;
      dest++;
      src++;
    }
}

/****************************************************************************
 * Name: gpu_tick_init
 ****************************************************************************/

void gpu_tick_init(void)
{
  uint32_t freq = CPU_CORE_CLOCK_MHZ * 1000000;
  up_perf_init((FAR void *)freq);
}

/****************************************************************************
 * Name: gpu_tick_get
 ****************************************************************************/

uint32_t gpu_tick_get(void)
{
  return up_perf_gettime();
}

/****************************************************************************
 * Name: gpu_tick_elaps
 ****************************************************************************/

uint32_t gpu_tick_elaps(uint32_t prev_tick)
{
  uint32_t act_time = gpu_tick_get();

  if (act_time >= prev_tick)
    {
      prev_tick = act_time - prev_tick;
    }
  else
    {
      prev_tick = UINT32_MAX - prev_tick + 1;
      prev_tick += act_time;
    }

  return prev_tick;
}

/****************************************************************************
 * Name: gpu_tick_elaps_us
 ****************************************************************************/

uint32_t gpu_tick_elaps_us(uint32_t prev_tick)
{
  return gpu_tick_elaps(prev_tick) / (float)CPU_CORE_CLOCK_MHZ;
}

/****************************************************************************
 * Name: gpu_delay
 ****************************************************************************/

void gpu_delay(uint32_t ms)
{
  usleep(ms * 1000);
}

/****************************************************************************
 * Name: gpu_get_localtime_str
 ****************************************************************************/

void gpu_get_localtime_str(FAR char *str_buf, size_t buf_size)
{
  time_t rawtime;
  FAR struct tm *timeinfo;

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  snprintf(str_buf, buf_size, "%04d%02d%02d_%02d%02d%02d",
           1900 + timeinfo->tm_year,
           timeinfo->tm_mon, timeinfo->tm_mday, timeinfo->tm_hour,
           timeinfo->tm_min, timeinfo->tm_sec);
}

/****************************************************************************
 * Name: gpu_dir_create
 ****************************************************************************/

bool gpu_dir_create(FAR const char *dir_path)
{
  if (access(dir_path, F_OK) == OK)
    {
      GPU_LOG_INFO("directory: %s already exists", dir_path);
      return true;
    }

  GPU_LOG_WARN("can't access directory: %s, creating...", dir_path);

  if (mkdir(dir_path, 0777) == OK)
    {
      GPU_LOG_INFO("OK");
      return true;
    }

  GPU_LOG_ERROR("create directory: %s failed", dir_path);

  return false;
}
