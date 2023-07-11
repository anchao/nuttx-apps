/****************************************************************************
 * apps/graphics/lvgl/lv_porting/lv_fbdev_interface.c
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
#include <nuttx/video/fb.h>
#include <nuttx/video/rgbcolors.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <lv_porting/lv_porting.h>
#include "gpu_manager/lv_gpu_manager.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define FBDEV_FPS_COUNTER_SAMPLEING_TIME 300 /* ms */

/****************************************************************************
 * Private Type Declarations
 ****************************************************************************/

struct fbdev_obj_s
{
  lv_disp_draw_buf_t disp_draw_buf;
  lv_disp_drv_t disp_drv;
  FAR lv_disp_t *disp;
  FAR void *last_buffer;
  FAR void *act_buffer;
  lv_area_t inv_areas[LV_INV_BUF_SIZE];
  uint16_t inv_areas_len;
  lv_area_t final_area;

  int fd;
  FAR void *fbmem;
  uint32_t fbmem2_yoffset;
  struct fb_videoinfo_s vinfo;
  struct fb_planeinfo_s pinfo;

  bool double_buffer;
#if defined(CONFIG_LV_FBDEV_FPS_COUNTER)
  int refr_count;
  int render_count;
  uint32_t refr_total_time;
  uint32_t refr_start_time;
#endif
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: buf_rotate_copy
 ****************************************************************************/

#if defined(CONFIG_FB_UPDATE)
static void fbdev_update_area(FAR struct fbdev_obj_s *fbdev_obj)
{
  struct fb_area_s fb_area;
  int ret;
  int yoffset = fbdev_obj->act_buffer == fbdev_obj->fbmem ?
                0 : fbdev_obj->fbmem2_yoffset;

  fb_area.x = fbdev_obj->final_area.x1;
  fb_area.y = fbdev_obj->final_area.y1 + yoffset;
  fb_area.w = lv_area_get_width(&fbdev_obj->final_area);
  fb_area.h = lv_area_get_height(&fbdev_obj->final_area);

  LV_LOG_TRACE("area: (%d, %d) W%d x H%d",
               fb_area.x, fb_area.y, fb_area.w, fb_area.h);

  ret = ioctl(fbdev_obj->fd, FBIO_UPDATE,
              (unsigned long)((uintptr_t)&fb_area));

  if (ret < 0)
    {
      LV_LOG_ERROR("ioctl(FBIO_UPDATE) failed: %d", errno);
    }

  LV_LOG_TRACE("finished");
}
#endif /* CONFIG_FB_UPDATE */

/****************************************************************************
 * Name: fbdev_switch_buffer
 ****************************************************************************/

static void fbdev_update_inv_areas(FAR struct fbdev_obj_s *fbdev_obj)
{
  FAR lv_disp_t *disp_refr = fbdev_obj->disp;
  uint16_t inv_index;

  /* check inv_areas_len, it must == 0 */

  if (fbdev_obj->inv_areas_len != 0)
    {
      fbdev_obj->inv_areas_len = 0;
    }

  /* Save dirty area table for next synchronizationn */

  bool area_joined = false;

  for (inv_index = 0; inv_index < disp_refr->inv_p; inv_index++)
    {
      if (disp_refr->inv_area_joined[inv_index] == 0)
        {
          FAR const lv_area_t *area_p = &disp_refr->inv_areas[inv_index];
          fbdev_obj->inv_areas[fbdev_obj->inv_areas_len] = *area_p;
          fbdev_obj->inv_areas_len++;

          /* Join to final_area */

          if (!area_joined)
            {
              /* copy first area */

              fbdev_obj->final_area = *area_p;
              area_joined = true;
            }
          else
            {
              _lv_area_join(&fbdev_obj->final_area,
                            &fbdev_obj->final_area,
                            area_p);
            }
        }
    }
}

/****************************************************************************
 * Name: fbdev_switch_buffer
 ****************************************************************************/

static void fbdev_switch_buffer(FAR struct fbdev_obj_s *fbdev_obj)
{
  int ret;

  /* Save the buffer address for the next synchronization */

  fbdev_obj->last_buffer = fbdev_obj->act_buffer;

  LV_LOG_TRACE("Commit buffer = %p", fbdev_obj->act_buffer);

  if (fbdev_obj->act_buffer == fbdev_obj->fbmem)
    {
      fbdev_obj->pinfo.yoffset = 0;
      fbdev_obj->act_buffer = fbdev_obj->fbmem
        + fbdev_obj->fbmem2_yoffset * fbdev_obj->pinfo.stride;
    }
  else
    {
      fbdev_obj->pinfo.yoffset = fbdev_obj->fbmem2_yoffset;
      fbdev_obj->act_buffer = fbdev_obj->fbmem;
    }

  LV_LOG_TRACE("Commit yoffset = %" PRIu32, fbdev_obj->pinfo.yoffset);

  /* Commit buffer to fb driver */

  ret = ioctl(fbdev_obj->fd, FBIOPAN_DISPLAY,
              (unsigned long)((uintptr_t)&(fbdev_obj->pinfo)));

  if (ret < 0)
    {
      LV_LOG_ERROR("ioctl(FBIOPAN_DISPLAY) failed: %d", errno);
    }

  LV_LOG_TRACE("finished");
}

#if defined(CONFIG_LV_FBDEV_ENABLE_WAITFORVSYNC)

/****************************************************************************
 * Name: fbdev_disp_vsync_refr
 ****************************************************************************/

static void fbdev_disp_vsync_refr(FAR lv_timer_t *timer)
{
  int ret;
  FAR struct fbdev_obj_s *fbdev_obj = timer->user_data;

  LV_LOG_TRACE("Check vsync...");

  ret = ioctl(fbdev_obj->fd, FBIO_WAITFORVSYNC, NULL);

  if (ret != OK)
    {
      LV_LOG_TRACE("No vsync signal detect");
      return;
    }

  LV_LOG_TRACE("Refresh start");

  _lv_disp_refr_timer(NULL);
}

#endif /* CONFIG_LV_FBDEV_ENABLE_WAITFORVSYNC */

/****************************************************************************
 * Name: fbdev_check_inv_area_covered
 ****************************************************************************/

static bool fbdev_check_inv_area_covered(FAR lv_disp_t *disp_refr,
                                         FAR const lv_area_t *area_p)
{
  int i;

  for (i = 0; i < disp_refr->inv_p; i++)
    {
      FAR const lv_area_t *cur_area;

      /* Skip joined area */

      if (disp_refr->inv_area_joined[i])
        {
          continue;
        }

      cur_area = &disp_refr->inv_areas[i];

      /* Check cur_area is coverd area_p  */

      if (_lv_area_is_in(area_p, cur_area, 0))
        {
          return true;
        }
    }

  return false;
}

/****************************************************************************
 * Name: fbdev_clear_notify
 ****************************************************************************/

static void fbdev_clear_notify(FAR struct fbdev_obj_s *fbdev_obj)
{
  int ret;
  ret = ioctl(fbdev_obj->fd, FBIO_CLEARNOTIFY, NULL);

  if (ret < 0)
    {
      LV_LOG_ERROR("ERROR: ioctl(FBIO_CLEARNOTIFY) failed: %d", errno);
    }
}

/****************************************************************************
 * Name: fbdev_refr_start
 ****************************************************************************/

static void fbdev_refr_start(FAR lv_disp_drv_t *disp_drv)
{
  FAR struct fbdev_obj_s *fbdev_obj = disp_drv->user_data;
  fbdev_clear_notify(fbdev_obj);
#if defined(CONFIG_LV_FBDEV_FPS_COUNTER)
  fbdev_obj->refr_total_time += lv_tick_elaps(fbdev_obj->refr_start_time);
  fbdev_obj->refr_start_time = lv_tick_get();
  fbdev_obj->refr_count++;
#endif
}

/****************************************************************************
 * Name: fbdev_render_start
 ****************************************************************************/

static void fbdev_render_start(FAR lv_disp_drv_t *disp_drv)
{
  FAR struct fbdev_obj_s *fbdev_obj = disp_drv->user_data;
  FAR lv_disp_t *disp_refr;
  FAR lv_draw_ctx_t *draw_ctx;
  lv_coord_t hor_res;
  int i;

  /* No need sync buffer when inv_areas_len == 0 */

  if (fbdev_obj->inv_areas_len == 0)
    {
      LV_LOG_TRACE("No sync area");
      return;
    }

  LV_LOG_TRACE("Start sync %d areas...", fbdev_obj->inv_areas_len);

  disp_refr = _lv_refr_get_disp_refreshing();
  draw_ctx = disp_drv->draw_ctx;
  hor_res = disp_drv->hor_res;

  for (i = 0; i < fbdev_obj->inv_areas_len; i++)
    {
      FAR const lv_area_t *last_area = &fbdev_obj->inv_areas[i];

      LV_LOG_TRACE("Check area[%d]: (%d, %d) W%d x H%d",
        i,
        (int)last_area->x1, (int)last_area->y1,
        (int)lv_area_get_width(last_area),
        (int)lv_area_get_height(last_area));

      if (fbdev_check_inv_area_covered(disp_refr, last_area))
        {
          LV_LOG_TRACE("Skipped");
          continue;
        }

      /* Sync the inv area of ​​the previous frame */

      draw_ctx->buffer_copy(
        draw_ctx,
        fbdev_obj->act_buffer, hor_res, last_area,
        fbdev_obj->last_buffer, hor_res, last_area);

      LV_LOG_TRACE("Copied");
    }

  fbdev_obj->inv_areas_len = 0;
}

/****************************************************************************
 * Name: fbdev_flush_finish
 ****************************************************************************/

static void fbdev_flush_finish(FAR lv_disp_drv_t *disp_drv,
                               FAR const lv_area_t *area_p,
                               FAR lv_color_t *color_p)
{
  FAR struct fbdev_obj_s *fbdev_obj = disp_drv->user_data;

  /* Skip the non-last flush */

  if (!lv_disp_flush_is_last(disp_drv))
    {
      lv_disp_flush_ready(disp_drv);
      return;
    }

  /* Update inv areas list */

  fbdev_update_inv_areas(fbdev_obj);

#if defined(CONFIG_FB_UPDATE)
  /* Report the flush area to the driver */

  fbdev_update_area(fbdev_obj);
#endif

  if (fbdev_obj->double_buffer)
    {
      /* Notify the driver to switch buffers */

      fbdev_switch_buffer(fbdev_obj);
    }

  /* Clear writable flag */

  fbdev_clear_notify(fbdev_obj);

  /* Tell the flushing is ready */

  lv_disp_flush_ready(disp_drv);

#if defined(CONFIG_LV_FBDEV_FPS_COUNTER)
  fbdev_obj->render_count++;
#endif
}

/****************************************************************************
 * Name: fbdev_get_pinfo
 ****************************************************************************/

static int fbdev_get_pinfo(int fd, FAR struct fb_planeinfo_s *pinfo)
{
  int ret = ioctl(fd, FBIOGET_PLANEINFO,
                  (unsigned long)((uintptr_t)pinfo));
  if (ret < 0)
    {
      LV_LOG_ERROR("ERROR: ioctl(FBIOGET_PLANEINFO) failed: %d", errno);
      return ret;
    }

  LV_LOG_INFO("PlaneInfo (plane %d):", pinfo->display);
  LV_LOG_INFO("    fbmem: %p", pinfo->fbmem);
  LV_LOG_INFO("    fblen: %lu", (unsigned long)pinfo->fblen);
  LV_LOG_INFO("   stride: %u", pinfo->stride);
  LV_LOG_INFO("  display: %u", pinfo->display);
  LV_LOG_INFO("      bpp: %u", pinfo->bpp);

  /* Only these pixel depths are supported.  viinfo.fmt is ignored, only
   * certain color formats are supported.
   */

  if (pinfo->bpp != 32 && pinfo->bpp != 16 &&
      pinfo->bpp != 8  && pinfo->bpp != 1)
    {
      LV_LOG_ERROR("bpp = %u not supported", pinfo->bpp);
      return -1;
    }

  return 0;
}

/****************************************************************************
 * Name: fbdev_try_init_fbmem2
 ****************************************************************************/

static int fbdev_try_init_fbmem2(FAR struct fbdev_obj_s *state)
{
  uintptr_t buf_offset;
  struct fb_planeinfo_s pinfo;

  memset(&pinfo, 0, sizeof(pinfo));

  /* Get display[1] planeinfo */

  pinfo.display = state->pinfo.display + 1;

  if (fbdev_get_pinfo(state->fd, &pinfo) < 0)
    {
      return -1;
    }

  /* check displey and match bpp */

  if (!(pinfo.display != state->pinfo.display
     && pinfo.bpp == state->pinfo.bpp))
    {
      LV_LOG_WARN("fbmem2 is incorrect");
      return -1;
    }

  /* Check the buffer address offset,
   * It needs to be divisible by pinfo.stride
   */

  buf_offset = pinfo.fbmem - state->fbmem;

  if ((buf_offset % state->pinfo.stride) != 0)
    {
      LV_LOG_ERROR("The buf_offset(%" PRIuPTR ") is incorrect,"
                   " it needs to be divisible"
                   " by pinfo.stride(%d)",
                   buf_offset, state->pinfo.stride);
      return -1;
    }

  /* Enable double buffer mode */

  state->double_buffer = true;
  state->fbmem2_yoffset = buf_offset / state->pinfo.stride;

  LV_LOG_INFO("Use non-consecutive fbmem2 = %p, yoffset = %" PRIu32,
              pinfo.fbmem, state->fbmem2_yoffset);

  return 0;
}

#if defined(CONFIG_LV_FBDEV_FPS_COUNTER)

/****************************************************************************
 * Name: fbdev_print_fps_timer
 ****************************************************************************/

static void fbdev_print_fps_timer(FAR lv_timer_t *timer)
{
  FAR struct fbdev_obj_s *fbdev_obj = timer->user_data;
  uint32_t fps = 0;

  if (fbdev_obj->refr_total_time)
    {
      fps = 1000 * fbdev_obj->refr_count / fbdev_obj->refr_total_time;
    }

  LV_LOG_USER("FPS: %" PRIu32 " (refr: %d, render: %d)",
    fps,
    fbdev_obj->refr_count,
    fbdev_obj->render_count);
  fbdev_obj->refr_total_time = 0;
  fbdev_obj->refr_count = 0;
  fbdev_obj->render_count = 0;
}

#endif /* CONFIG_LV_FBDEV_FPS_COUNTER */

/****************************************************************************
 * Name: fbdev_init
 ****************************************************************************/

static FAR lv_disp_t *fbdev_init(FAR struct fbdev_obj_s *state)
{
  FAR struct fbdev_obj_s *fbdev_obj = malloc(sizeof(struct fbdev_obj_s));
  FAR lv_disp_drv_t *disp_drv;
  int fb_xres = state->vinfo.xres;
  int fb_yres = state->vinfo.yres;
  size_t fb_size = fb_xres * fb_yres;
  FAR lv_color_t *buf1 = NULL;
  FAR lv_color_t *buf2 = NULL;

  if (fbdev_obj == NULL)
    {
      LV_LOG_ERROR("fbdev_obj_s malloc failed");
      return NULL;
    }

  *fbdev_obj = *state;
  disp_drv = &(fbdev_obj->disp_drv);

  lv_disp_drv_init(disp_drv);
  disp_drv->draw_buf = &(fbdev_obj->disp_draw_buf);
  disp_drv->screen_transp = false;
  disp_drv->user_data = fbdev_obj;
  disp_drv->hor_res = fb_xres;
  disp_drv->ver_res = fb_yres;
  disp_drv->refr_start_cb = fbdev_refr_start;
  disp_drv->flush_cb = fbdev_flush_finish;
  disp_drv->direct_mode = true;
  buf1 = fbdev_obj->fbmem;

#if defined(CONFIG_LV_USE_GPU_MANAGER)
  lv_gpu_manager_init(disp_drv);
#elif defined(CONFIG_LV_USE_GPU_INTERFACE)
  disp_drv->draw_ctx_init = lv_gpu_draw_ctx_init;
  disp_drv->draw_ctx_size = sizeof(gpu_draw_ctx_t);
#endif

  if (fbdev_obj->double_buffer)
    {
      LV_LOG_INFO("Double buffer mode");
      buf2 = fbdev_obj->fbmem
        + fbdev_obj->fbmem2_yoffset * fbdev_obj->pinfo.stride;
      disp_drv->render_start_cb = fbdev_render_start;
    }
  else
    {
      LV_LOG_INFO("Single buffer mode");
    }

  lv_disp_draw_buf_init(&(fbdev_obj->disp_draw_buf), buf1, buf2, fb_size);
  fbdev_obj->act_buffer = fbdev_obj->fbmem;
  fbdev_obj->disp = lv_disp_drv_register(&(fbdev_obj->disp_drv));

#if defined(CONFIG_LV_FBDEV_ENABLE_WAITFORVSYNC)
  /* If double buffer and vsync is supported, use active refresh method */

  if (fbdev_obj->disp_drv.direct_mode)
    {
      FAR lv_timer_t *refr_timer = _lv_disp_get_refr_timer(fbdev_obj->disp);
      lv_timer_del(refr_timer);
      fbdev_obj->disp->refr_timer = NULL;
      lv_timer_create(fbdev_disp_vsync_refr, 1, fbdev_obj);
    }
#endif

#if defined(CONFIG_LV_FBDEV_FPS_COUNTER)
  lv_timer_create(fbdev_print_fps_timer,
                  FBDEV_FPS_COUNTER_SAMPLEING_TIME,
                  fbdev_obj);
#endif

  return fbdev_obj->disp;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lv_fbdev_interface_init
 *
 * Description:
 *   Framebuffer device interface initialization.
 *
 * Input Parameters:
 *   dev_path - Framebuffer device path, set to NULL to use the default path.
 *
 * Returned Value:
 *   lv_disp object address on success; NULL on failure.
 *
 ****************************************************************************/

FAR lv_disp_t *lv_fbdev_interface_init(FAR const char *dev_path,
                                       int line_buf)
{
  FAR const char *device_path = dev_path;
  struct fbdev_obj_s state;
  int ret;
  FAR lv_disp_t *disp;

  memset(&state, 0, sizeof(state));

  if (device_path == NULL)
    {
      device_path = CONFIG_LV_FBDEV_INTERFACE_DEFAULT_DEVICEPATH;
    }

  LV_LOG_INFO("fbdev %s opening", device_path);

  state.fd = open(device_path, O_RDWR);
  if (state.fd < 0)
    {
      LV_LOG_ERROR("fbdev %s open failed: %d", device_path, errno);
      return NULL;
    }

  /* Get the characteristics of the framebuffer */

  ret = ioctl(state.fd, FBIOGET_VIDEOINFO,
              (unsigned long)((uintptr_t)&state.vinfo));
  if (ret < 0)
    {
      LV_LOG_ERROR("ioctl(FBIOGET_VIDEOINFO) failed: %d", errno);
      close(state.fd);
      return NULL;
    }

  LV_LOG_INFO("VideoInfo:");
  LV_LOG_INFO("      fmt: %u", state.vinfo.fmt);
  LV_LOG_INFO("     xres: %u", state.vinfo.xres);
  LV_LOG_INFO("     yres: %u", state.vinfo.yres);
  LV_LOG_INFO("  nplanes: %u", state.vinfo.nplanes);

  ret = fbdev_get_pinfo(state.fd, &state.pinfo);

  if (ret < 0)
    {
      close(state.fd);
      return NULL;
    }

  /* Check color depth match */

  if (state.pinfo.bpp != LV_COLOR_DEPTH)
    {
      LV_LOG_ERROR("fbdev bpp = %d, LV_COLOR_DEPTH = %d, "
                   "color depth does not match.",
                   state.pinfo.bpp, LV_COLOR_DEPTH);
      close(state.fd);
      return NULL;
    }

  state.double_buffer = (state.pinfo.yres_virtual == (state.vinfo.yres * 2));

  /* mmap() the framebuffer.
   *
   * NOTE: In the FLAT build the frame buffer address returned by the
   * FBIOGET_PLANEINFO IOCTL command will be the same as the framebuffer
   * address.  mmap(), however, is the preferred way to get the framebuffer
   * address because in the KERNEL build, it will perform the necessary
   * address mapping to make the memory accessible to the application.
   */

  state.fbmem = mmap(NULL, state.pinfo.fblen, PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_FILE, state.fd, 0);
  if (state.fbmem == MAP_FAILED)
    {
      LV_LOG_ERROR("ioctl(FBIOGET_PLANEINFO) failed: %d", errno);
      close(state.fd);
      return NULL;
    }

  LV_LOG_INFO("Mapped FB: %p", state.fbmem);

  if (state.double_buffer)
    {
      state.fbmem2_yoffset = state.vinfo.yres;
    }
  else
    {
      fbdev_try_init_fbmem2(&state);
    }

  disp = fbdev_init(&state);

  if (!disp)
    {
      munmap(state.fbmem, state.pinfo.fblen);
      close(state.fd);
      return NULL;
    }

  return disp;
}
