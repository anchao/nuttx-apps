/****************************************************************************
 * apps/testing/gpu/gpu_main.c
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

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include <nuttx/video/fb.h>
#include "gpu_test.h"

/****************************************************************************
 * Preprocessor Definitions
 ****************************************************************************/

#define GPU_FB_PATH "/dev/fb0"

#define GPU_PREFIX "GPU: "

#define OPTARG_TO_VALUE(value, type, base)                             \
  do                                                                   \
  {                                                                    \
    FAR char *ptr;                                                     \
    value = (type)strtoul(optarg, &ptr, base);                         \
    if (*ptr != '\0')                                                  \
      {                                                                \
        printf(GPU_PREFIX "Parameter error: -%c %s\n", ch, optarg); \
        show_usage(argv[0], EXIT_FAILURE);                             \
      }                                                                \
  } while (0)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct fb_state_s
{
  int fd;
  struct fb_videoinfo_s vinfo;
  struct fb_planeinfo_s pinfo;
  FAR void *fbmem;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: show_usage
 ****************************************************************************/

static void show_usage(FAR const char *progname, int exitcode)
{
  printf("\nUsage: %s"
         " -o <string> -m <string> -i <string> -s\n",
         progname);
  printf("\nWhere:\n");
  printf("  -o <string> GPU report file output path.\n");
  printf("  -m <string> Test mode: default; random; stress.\n");
  printf("  -i <string> Test image size(px): "
         "<decimal-value width>x<decimal-value height>\n");
  printf("  -s Enable screenshot\n");

  exit(exitcode);
}

/****************************************************************************
 * Name: gpu_test_string_to_mode
 ****************************************************************************/

static enum gpu_test_mode_e gpu_test_string_to_mode(FAR const char *str)
{
  if (strcmp(str, "default") == 0)
    {
      return GPU_TEST_MODE_DEFAULT;
    }
  else if (strcmp(str, "random") == 0)
    {
      return GPU_TEST_MODE_RANDOM;
    }
  else if (strcmp(str, "stress") == 0)
    {
      return GPU_TEST_MODE_STRESS;
    }

  printf(GPU_PREFIX "Unknown mode: %s\n", str);

  return GPU_TEST_MODE_DEFAULT;
}

/****************************************************************************
 * Name: parse_commandline
 ****************************************************************************/

static void parse_commandline(int argc, FAR char **argv,
                              FAR struct gpu_test_param_s *param)
{
  int ch;
  int converted;

  /* set default param */

  memset(param, 0, sizeof(struct gpu_test_param_s));
  param->output_dir = "/data/gpu";
  param->mode = GPU_TEST_MODE_DEFAULT;
  param->img_width = 128;
  param->img_height = 128;

  while ((ch = getopt(argc, argv, "ho:m:i:s")) != ERROR)
    {
      switch (ch)
        {
          case 'o':
            param->output_dir = optarg;
            break;

          case 'm':
            param->mode = gpu_test_string_to_mode(optarg);
            break;

          case 'i':
          {
            int width;
            int height;
            converted = sscanf(optarg, "%dx%d", &width, &height);
            if (converted == 2 && width >= 0 && height >= 0)
              {
                param->img_width = width;
                param->img_height = height;
              }
            else
              {
                printf(GPU_PREFIX ": Error image size: %s\n", optarg);
                show_usage(argv[0], EXIT_FAILURE);
              }
            break;
          }

          case 's':
            param->screenshot_en = true;
            break;

          case '?':
            printf(GPU_PREFIX ": Unknown option: %c\n", optopt);
          case 'h':
            show_usage(argv[0], EXIT_FAILURE);
            break;
        }
    }

  printf(GPU_PREFIX "Output DIR: %s\n", param->output_dir);
  printf(GPU_PREFIX "Test mode: %d\n", param->mode);
  printf(GPU_PREFIX "Image size: %dx%d\n",
         param->img_width,
         param->img_height);
  printf(GPU_PREFIX "Screenshot: %s\n",
         param->screenshot_en ? "enable" : "disable");
}

/****************************************************************************
 * Name: gpu_fb_init
 ****************************************************************************/

static int gpu_fb_init(FAR struct fb_state_s *state, FAR const char *path)
{
  int ret;

  /* Open the framebuffer driver */

  state->fd = open(path, O_RDWR);
  if (state->fd < 0)
    {
      int errcode = errno;
      fprintf(stderr, "ERROR: Failed to open %s: %d\n", path, errcode);
      return ERROR;
    }

  /* Get the characteristics of the framebuffer */

  ret = ioctl(state->fd, FBIOGET_VIDEOINFO,
              (unsigned long)((uintptr_t)&state->vinfo));
  if (ret < 0)
    {
      int errcode = errno;
      fprintf(stderr, "ERROR: ioctl(FBIOGET_VIDEOINFO) failed: %d\n",
              errcode);
      close(state->fd);
      return ERROR;
    }

  printf("VideoInfo:\n");
  printf("      fmt: %u\n", state->vinfo.fmt);
  printf("     xres: %u\n", state->vinfo.xres);
  printf("     yres: %u\n", state->vinfo.yres);
  printf("  nplanes: %u\n", state->vinfo.nplanes);

  ret = ioctl(state->fd, FBIOGET_PLANEINFO,
              (unsigned long)((uintptr_t)&state->pinfo));
  if (ret < 0)
    {
      int errcode = errno;
      fprintf(stderr, "ERROR: ioctl(FBIOGET_PLANEINFO) failed: %d\n",
              errcode);
      close(state->fd);
      return ERROR;
    }

  printf("PlaneInfo (plane 0):\n");
  printf("    fbmem: %p\n", state->pinfo.fbmem);
  printf("    fblen: %lu\n", (unsigned long)state->pinfo.fblen);
  printf("   stride: %u\n", state->pinfo.stride);
  printf("  display: %u\n", state->pinfo.display);
  printf("      bpp: %u\n", state->pinfo.bpp);

  /* Only these pixel depths are supported.  viinfo.fmt is ignored, only
   * certain color formats are supported.
   */

  if (state->pinfo.bpp != 32)
    {
      fprintf(stderr, "ERROR: bpp=%u not supported\n", state->pinfo.bpp);
      close(state->fd);
      return ERROR;
    }

  /* mmap() the framebuffer.
   *
   * NOTE: In the FLAT build the frame buffer address returned by the
   * FBIOGET_PLANEINFO IOCTL command will be the same as the framebuffer
   * address.  mmap(), however, is the preferred way to get the framebuffer
   * address because in the KERNEL build, it will perform the necessary
   * address mapping to make the memory accessible to the application.
   */

  state->fbmem = mmap(NULL, state->pinfo.fblen, PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_FILE, state->fd, 0);
  if (state->fbmem == MAP_FAILED)
    {
      int errcode = errno;
      fprintf(stderr, "ERROR: ioctl(FBIOGET_PLANEINFO) failed: %d\n",
              errcode);
      close(state->fd);
      return ERROR;
    }

  printf(GPU_PREFIX "Mapped FB: %p\n", state->fbmem);
  return OK;
}

/****************************************************************************
 * Name: gpu_fb_deinit
 ****************************************************************************/

static void gpu_fb_deinit(FAR struct fb_state_s *state)
{
  munmap(state->fbmem, state->pinfo.fblen);
  close(state->fd);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: gpu_fb_update
 ****************************************************************************/

void gpu_fb_update(FAR struct gpu_test_context_s *ctx)
{
  FAR struct fb_state_s *state = ctx->state;
  int ret;

#ifdef CONFIG_FB_UPDATE
  struct fb_area_s area;
#endif

  ret = ioctl(state->fd, FBIOPAN_DISPLAY,
              (unsigned long)((uintptr_t)&(state->pinfo)));

  if (ret < 0)
    {
      fprintf(stderr, "ERROR: ioctl(FBIOPAN_DISPLAY) failed: %d\n",
              errno);
      return;
    }

#ifdef CONFIG_FB_UPDATE
  area.x = 0;
  area.y = 0;
  area.w = ctx->xres;
  area.h = ctx->yres;
  ret = ioctl(state->fd, FBIO_UPDATE, (unsigned long)((uintptr_t)&area));

  if (ret < 0)
    {
      fprintf(stderr, "ERROR: ioctl(FBIO_UPDATE) failed: %d\n",
              errno);
      return;
    }
#endif
}

/****************************************************************************
 * Name: gpu_main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  FAR const char *path = GPU_FB_PATH;
  struct fb_state_s state;
  struct gpu_test_context_s ctx;
  int ret;

  memset(&ctx, 0, sizeof(ctx));
  parse_commandline(argc, argv, &ctx.param);

  gpu_dir_create(ctx.param.output_dir);

  ret = gpu_fb_init(&state, path);

  if (ret != OK)
    {
      return EXIT_FAILURE;
    }

  ctx.state = &state;
  ctx.fbmem = state.fbmem;
  ctx.bpp = state.pinfo.bpp;
  ctx.stride = state.pinfo.stride;
  ctx.xres = state.vinfo.xres;
  ctx.yres = state.vinfo.yres;

  gpu_test_run(&ctx);

  gpu_fb_deinit(&state);
  printf(GPU_PREFIX "Test finished\n");
  return EXIT_SUCCESS;
}
