/****************************************************************************
 * apps/testing/gpu/gpu_utils.h
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

#ifndef __APPS_TESTING_GPU_GPU_UTILS_H__
#define __APPS_TESTING_GPU_GPU_UTILS_H__

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "gpu_log.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define GPU_ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define GPU_ASSERT(expr) DEBUGASSERT(expr)
#define GPU_ASSERT_NULL(ptr) GPU_ASSERT(ptr != NULL)

#define GPU_PERF_RESET()          memset(&ctx->perf, 0, sizeof(ctx->perf))
#define GPU_PERF_PREPARE_START()  ctx->perf.prepare = gpu_tick_get()
#define GPU_PERF_PREPARE_STOP()   ctx->perf.prepare = gpu_tick_elaps_us(ctx->perf.prepare)
#define GPU_PERF_RENDER_START()   ctx->perf.render = gpu_tick_get()
#define GPU_PERF_RENDER_STOP()    ctx->perf.render = gpu_tick_elaps_us(ctx->perf.render)

/****************************************************************************
 * Public Types
 ****************************************************************************/

enum gpu_test_mode_e
{
  GPU_TEST_MODE_DEFAULT = 0,
  GPU_TEST_MODE_RANDOM,
  GPU_TEST_MODE_STRESS,
};

struct gpu_test_param_s
{
  FAR const char *output_dir;
  enum gpu_test_mode_e mode;
  int img_width;
  int img_height;
  bool screenshot_en;
};

struct gpu_test_context_s
{
  FAR void *state;
  FAR void *fbmem;
  FAR void *recorder;
  FAR void *user_data;
  int bpp;
  int stride;
  int xres;
  int yres;
  struct gpu_test_param_s param;
  struct
  {
    uint32_t prepare;
    uint32_t render;
  } perf;
};

#pragma pack(1)

typedef union gpu_color32_u
{
    struct
    {
        uint8_t blue;
        uint8_t green;
        uint8_t red;
        uint8_t alpha;
    } ch;
    uint32_t full;
} gpu_color32_t;

typedef union gpu_color24_u
{
    struct
    {
        uint8_t blue;
        uint8_t green;
        uint8_t red;
    } ch;
    uint8_t full[3];
} gpu_color24_t;

typedef union gpu_color16_u
{
    struct
    {
        uint16_t blue : 5;
        uint16_t green : 6;
        uint16_t red : 5;
    } ch;
    uint16_t full;
} gpu_color16_t;

typedef struct gpu_color16_alpha_s
{
  gpu_color16_t color;
  uint8_t alpha;
} gpu_color16_alpha_t;

#pragma pack()

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
 * Name: gpu_sw_color32_mix
 ****************************************************************************/

gpu_color32_t gpu_sw_color32_mix(gpu_color32_t c1,
                                 gpu_color32_t c2,
                                 uint8_t mix);

/****************************************************************************
 * Name: gpu_sw_fill_screen
 ****************************************************************************/

void gpu_sw_fill_screen(FAR struct gpu_test_context_s *ctx,
                        gpu_color32_t color);

/****************************************************************************
 * Name: gpu_sw_bgra5658_to_bgra8888
 ****************************************************************************/

void gpu_sw_bgra5658_to_bgra8888(FAR gpu_color32_t *dest,
                                 FAR const gpu_color16_alpha_t *src,
                                 size_t size);

/****************************************************************************
 * Name: gpu_tick_init
 ****************************************************************************/

void gpu_tick_init(void);

/****************************************************************************
 * Name: gpu_tick_get
 ****************************************************************************/

uint32_t gpu_tick_get(void);

/****************************************************************************
 * Name: gpu_tick_elaps
 ****************************************************************************/

uint32_t gpu_tick_elaps(uint32_t prev_tick);

/****************************************************************************
 * Name: gpu_tick_elaps_us
 ****************************************************************************/

uint32_t gpu_tick_elaps_us(uint32_t prev_tick);

/****************************************************************************
 * Name: gpu_delay
 ****************************************************************************/

void gpu_delay(uint32_t ms);

/****************************************************************************
 * Name: gpu_get_localtime_str
 ****************************************************************************/

void gpu_get_localtime_str(FAR char *str_buf, size_t buf_size);

/****************************************************************************
 * Name: gpu_dir_create
 ****************************************************************************/

bool gpu_dir_create(FAR const char *dir_path);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __APPS_TESTING_GPU_GPU_UTILS_H__ */
