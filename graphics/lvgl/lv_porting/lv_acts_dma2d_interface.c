/****************************************************************************
 * apps/graphics/lvgl/lv_porting/lv_acts_dma2d_interface.c
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

#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <nuttx/cache.h>
#include <arch/chip/acts_dma2d_dev.h>
#include "lv_acts_dma2d_interface.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Minimum area (in pixels) for DMA2D blit/fill processing. */

#ifdef CONFIG_LV_ACTS_DMA2D_SIZE_LIMIT
#  define LV_ACTS_DMA2D_SIZE_LIMIT CONFIG_LV_ACTS_DMA2D_SIZE_LIMIT
#else
#  define LV_ACTS_DMA2D_SIZE_LIMIT 32
#endif

#if LV_COLOR_DEPTH == 32
#  define LV_DMA2D_COLOR_FORMAT       DMA2D_PF_ARGB8888
#  define LV_DMA2D_COLOR_ALPHA_FORMAT DMA2D_PF_ARGB8888
#else
#  define LV_DMA2D_COLOR_FORMAT       DMA2D_PF_RGB565
#  define LV_DMA2D_COLOR_ALPHA_FORMAT DMA2D_PF_ARGB8565
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void _lv_draw_acts_dma2d_wait_for_finish(lv_draw_ctx_t * draw_ctx);
static void _lv_draw_acts_dma2d_draw_img_decoded(lv_draw_ctx_t * draw_ctx,
                const lv_draw_img_dsc_t * dsc, const lv_area_t * coords,
                const uint8_t * map_p, lv_img_cf_t cf);
static void _lv_draw_acts_dma2d_blend(lv_draw_ctx_t * draw_ctx,
                const lv_draw_sw_blend_dsc_t * dsc);

static uint32_t _lv_color_format_to_dma2d(lv_img_cf_t cf);

static lv_res_t _lv_acts_dma2d_fill(
        lv_color_t * dest, int16_t dest_stride,
        int16_t fill_w, int16_t fill_h, lv_color_t color, lv_opa_t opa);

static lv_res_t _lv_acts_dma2d_blit(
        lv_color_t * dest, int16_t dest_stride,
        const lv_img_dsc_t * src, int16_t copy_w, int16_t copy_h,
        lv_color_t color, lv_opa_t opa);

static void _lv_acts_dma2d_clean_img_src(const lv_img_dsc_t * src);
static void _lv_acts_dma2d_clean_drawbuf(lv_draw_ctx_t * draw_ctx);
static void _lv_acts_dma2d_flush_drawbuf(lv_draw_ctx_t * draw_ctx);

static lv_draw_layer_ctx_t * _lv_draw_acts_dma2d_layer_init(
        lv_draw_ctx_t * draw_ctx, lv_draw_layer_ctx_t * layer_ctx,
        lv_draw_layer_flags_t flags);
static void _lv_draw_acts_dma2d_layer_adjust(lv_draw_ctx_t * draw_ctx,
        lv_draw_layer_ctx_t * layer_ctx, lv_draw_layer_flags_t flags);
static void _lv_draw_acts_dma2d_layer_blend(lv_draw_ctx_t * draw_ctx,
        lv_draw_layer_ctx_t * layer_ctx, const lv_draw_img_dsc_t * draw_dsc);

/****************************************************************************
 * Private data
 ****************************************************************************/

static bool is_accl_dma2d = false;
static int dma2d_fd = -1;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * During rendering, LVGL might initializes new draw_ctxs and start drawing
 * into a separate buffer (called layer). If the content to be rendered has
 * "holes", e.g. rounded corner, LVGL temporarily sets the
 * disp_drv.screen_transp flag.
 * It means the renderers should draw into an ARGB buffer.
 * With 32 bit color depth it's not a big problem but with 16 bit color depth
 * the target pixel format is ARGB8565 which is not supported by the GPU.
 * In this case, the VG-Lite callbacks should fallback to SW rendering.
 */

static inline bool need_argb8565_support(lv_draw_ctx_t * draw_ctx)
{
#if LV_COLOR_DEPTH != 32 && LV_COLOR_SCREEN_TRANSP
  lv_disp_t * disp = _lv_refr_get_disp_refreshing();

  if (disp->driver->screen_transp == 1)
      return true;
#endif

  return false;
}

/****************************************************************************
 * Name: _lv_color_format_to_dma2d
 ****************************************************************************/

static uint32_t _lv_color_format_to_dma2d(lv_img_cf_t cf)
{
  switch (cf)
    {
      case LV_IMG_CF_TRUE_COLOR:
        return LV_DMA2D_COLOR_FORMAT;
      case LV_IMG_CF_TRUE_COLOR_ALPHA:
        return LV_DMA2D_COLOR_ALPHA_FORMAT;
      case LV_IMG_CF_ALPHA_8BIT:
        return DMA2D_PF_A8;
      case LV_IMG_CF_RGBA8888:
        return DMA2D_PF_ARGB8888;
      case LV_IMG_CF_RGB565:
        return DMA2D_PF_RGB565;
      case LV_IMG_CF_RGBX8888:
      default:
        return DMA2D_PF_UNKNOWN;
    }
}

/****************************************************************************
 * Name: _lv_acts_dma2d_clean_img_src
 ****************************************************************************/

static void _lv_acts_dma2d_clean_img_src(const lv_img_dsc_t * src)
{
  up_clean_dcache((uintptr_t)src->data, (uintptr_t)src->data + src->data_size);
}

/****************************************************************************
 * Name: _lv_acts_dma2d_clean_drawbuf
 ****************************************************************************/

static void _lv_acts_dma2d_clean_drawbuf(lv_draw_ctx_t * draw_ctx)
{
  uint32_t len = lv_area_get_size(draw_ctx->buf_area) * sizeof(lv_color_t);

  up_clean_dcache((uintptr_t)draw_ctx->buf, (uintptr_t)draw_ctx->buf + len);
}

/****************************************************************************
 * Name: _lv_acts_dma2d_flush_drawbuf
 ****************************************************************************/

static void _lv_acts_dma2d_flush_drawbuf(lv_draw_ctx_t * draw_ctx)
{
  uint32_t len = lv_area_get_size(draw_ctx->buf_area) * sizeof(lv_color_t);

  up_flush_dcache((uintptr_t)draw_ctx->buf, (uintptr_t)draw_ctx->buf + len);
}

/****************************************************************************
 * Name: _lv_draw_acts_dma2d_wait_for_finish
 ****************************************************************************/

static void _lv_draw_acts_dma2d_wait_for_finish(lv_draw_ctx_t * draw_ctx)
{
  if (is_accl_dma2d == true)
    {
      is_accl_dma2d = false;

      _lv_acts_dma2d_flush_drawbuf(draw_ctx);

      ioctl(dma2d_fd, DMA2DDEVIO_WAITFINISH, 0);
    }
}

/****************************************************************************
 * Name: _lv_draw_acts_dma2d_draw_img_decoded
 ****************************************************************************/

static void _lv_draw_acts_dma2d_draw_img_decoded(lv_draw_ctx_t * draw_ctx,
              const lv_draw_img_dsc_t * dsc, const lv_area_t * coords,
              const uint8_t * map_p, lv_img_cf_t cf)
{
  if (need_argb8565_support(draw_ctx) ||
      dsc->blend_mode != LV_BLEND_MODE_NORMAL ||
      dsc->recolor_opa != LV_OPA_TRANSP ||
      dsc->angle != 0 || dsc->zoom != LV_IMG_ZOOM_NONE ||
      lv_draw_mask_is_any(draw_ctx->clip_area))
    {
      lv_draw_sw_img_decoded(draw_ctx, dsc, coords, map_p, cf);
      return;
    }

  uint8_t px_bytes = lv_img_cf_get_px_size(cf) >> 3;

  lv_img_dsc_t img_dsc;
  img_dsc.header.cf = cf;
  img_dsc.header.w = lv_area_get_width(coords);
  img_dsc.header.h = lv_area_get_height(coords);
  img_dsc.data = map_p + px_bytes * ((draw_ctx->clip_area->y1 - coords->y1) *
          img_dsc.header.w + (draw_ctx->clip_area->x1 - coords->x1));
  img_dsc.data_size = img_dsc.header.w * img_dsc.header.h * px_bytes;

  int32_t dest_stride = lv_area_get_width(draw_ctx->buf_area);
  lv_color_t * dest_buf = (lv_color_t *)draw_ctx->buf +
      dest_stride * (draw_ctx->clip_area->y1 - draw_ctx->buf_area->y1) +
                (draw_ctx->clip_area->x1 - draw_ctx->buf_area->x1);

  if (is_accl_dma2d == false)
      _lv_acts_dma2d_clean_drawbuf(draw_ctx);

  lv_res_t res = _lv_acts_dma2d_blit(dest_buf, (int16_t)dest_stride,
          &img_dsc, lv_area_get_width(draw_ctx->clip_area),
          lv_area_get_height(draw_ctx->clip_area), dsc->recolor, dsc->opa);
  if (res == LV_RES_INV)
    {
      lv_draw_sw_img_decoded(draw_ctx, dsc, coords, map_p, cf);
    }
  else
    {
      is_accl_dma2d = true;
    }
}

/****************************************************************************
 * Name: _lv_draw_acts_dma2d_blend
 ****************************************************************************/

static void _lv_draw_acts_dma2d_blend(lv_draw_ctx_t * draw_ctx,
                                      const lv_draw_sw_blend_dsc_t * dsc)
{
  lv_img_dsc_t img_dsc;
  const lv_opa_t * mask;
  lv_res_t res = LV_RES_INV;

  if (dsc->opa <= (lv_opa_t)LV_OPA_MIN)
      return;

  if (need_argb8565_support(draw_ctx) ||
      dsc->blend_mode != LV_BLEND_MODE_NORMAL)
    {
      lv_draw_sw_blend_basic(draw_ctx, dsc);
      return;
    }

  if (dsc->mask_buf && dsc->mask_res == LV_DRAW_MASK_RES_TRANSP) return;
  else if (dsc->mask_res == LV_DRAW_MASK_RES_FULL_COVER) mask = NULL;
  else mask = dsc->mask_buf;

  if (mask != NULL && dsc->src_buf != NULL)
    {
      lv_draw_sw_blend_basic(draw_ctx, dsc);
      return;
    }

  lv_area_t blend_area;
  if (!_lv_area_intersect(&blend_area, dsc->blend_area, draw_ctx->clip_area))
      return;

  if (dsc->src_buf)
    {
      img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
      img_dsc.header.w = lv_area_get_width(dsc->blend_area);
      img_dsc.header.h = lv_area_get_height(&blend_area);
      img_dsc.data = (uint8_t *)dsc->src_buf + sizeof(lv_color_t) *
              (img_dsc.header.w * (blend_area.y1 - dsc->blend_area->y1) +
                  (blend_area.x1 - dsc->blend_area->x1));
      img_dsc.data_size = sizeof(lv_color_t) *
              img_dsc.header.w * img_dsc.header.h;
    }
  else if (mask)
    {
      img_dsc.header.cf = LV_IMG_CF_ALPHA_8BIT;
      img_dsc.header.w = lv_area_get_width(dsc->mask_area);
      img_dsc.header.h = lv_area_get_height(&blend_area);
      img_dsc.data = (uint8_t *)mask +
              img_dsc.header.w * (blend_area.y1 - dsc->mask_area->y1) +
                  (blend_area.x1 - dsc->mask_area->x1);
      img_dsc.data_size = img_dsc.header.w * img_dsc.header.h;
    }
  else
    {
      img_dsc.data = NULL;
    }

  int32_t dest_stride = lv_area_get_width(draw_ctx->buf_area);
  lv_color_t * dest_buf = (lv_color_t *)draw_ctx->buf +
          dest_stride * (blend_area.y1 - draw_ctx->buf_area->y1) +
                  (blend_area.x1 - draw_ctx->buf_area->x1);

  if (is_accl_dma2d == false)
      _lv_acts_dma2d_clean_drawbuf(draw_ctx);

  if (img_dsc.data)
    {
      res = _lv_acts_dma2d_blit(dest_buf, (int16_t)dest_stride,
                &img_dsc, lv_area_get_width(&blend_area),
                lv_area_get_height(&blend_area), dsc->color, dsc->opa);

      /* The mask buf is temporary, must sync at once */

      if (res == LV_RES_OK && mask != NULL)
          ioctl(dma2d_fd, DMA2DDEVIO_WAITFINISH, 0);
    }
  else
    {
      res = _lv_acts_dma2d_fill(dest_buf, (int16_t)dest_stride,
                lv_area_get_width(&blend_area),
                lv_area_get_height(&blend_area), dsc->color, dsc->opa);
    }

  if (res == LV_RES_INV)
    {
      lv_draw_sw_blend_basic(draw_ctx, dsc);
    }
  else
    {
      is_accl_dma2d = true;
    }
}

/****************************************************************************
 * Name: _lv_acts_dma2d_fill
 ****************************************************************************/

static lv_res_t _lv_acts_dma2d_fill(
          lv_color_t * dest, int16_t dest_stride,
          int16_t fill_w, int16_t fill_h, lv_color_t color, lv_opa_t opa)
{
  struct acts_dma2d_buffer_s dest_buf;
  int res = 0;

  if ((uint32_t)fill_w * fill_h < LV_ACTS_DMA2D_SIZE_LIMIT)
      return LV_RES_INV;

  dest_buf.memory = dest;
  dest_buf.width = fill_w;
  dest_buf.height = fill_h;
  dest_buf.stride = dest_stride * sizeof(lv_color_t);
  dest_buf.format = LV_DMA2D_COLOR_FORMAT;

  if (opa >= LV_OPA_MAX)
    {
      struct dma2ddev_fillcolor_s fillcolor =
      {
          .dest = &dest_buf,
          .argb = lv_color_to32(color),
      };

      res = ioctl(dma2d_fd, DMA2DDEVIO_FILLCOLOR, (unsigned long)&fillcolor);
    }
  else
    {
      struct acts_dma2d_overlay_s src_ovl =
      {
          .buffer = NULL,
          .color = ((uint32_t)opa << 24) | (lv_color_to32(color) & 0xffffff),
          .area = { .x = 0, .y = 0, .w = fill_w, .h = fill_h, },
      };

      struct acts_dma2d_overlay_s bg_ovl =
      {
          .buffer = &dest_buf,
          .area = src_ovl.area,
      };

      struct dma2ddev_blend_s blend =
      {
          .dest = &dest_buf,
          .fg = &src_ovl,
          .bg = &bg_ovl,
      };

      res = ioctl(dma2d_fd, DMA2DDEVIO_BLEND, (unsigned long)&blend);
    }

  return (res >= 0) ? LV_RES_OK : LV_RES_INV;
}

/****************************************************************************
 * Name: _lv_acts_dma2d_blit
 ****************************************************************************/

static lv_res_t _lv_acts_dma2d_blit(
        lv_color_t * dest, int16_t dest_stride,
        const lv_img_dsc_t * src, int16_t copy_w, int16_t copy_h,
        lv_color_t color, lv_opa_t opa)
{
  struct acts_dma2d_buffer_s dest_buf;
  struct acts_dma2d_buffer_s src_buf;
  struct acts_dma2d_overlay_s src_ovl;
  int res = 0;

  if ((uint32_t)copy_w * copy_h < LV_ACTS_DMA2D_SIZE_LIMIT)
      return LV_RES_INV;

  src_buf.format = _lv_color_format_to_dma2d(src->header.cf);
  if (src_buf.format == DMA2D_PF_UNKNOWN)
      return LV_RES_INV;

  src_buf.memory = (void *)src->data;
  src_buf.width = copy_w;
  src_buf.height = copy_h;
  src_buf.stride = src->header.w * lv_img_cf_get_px_size(src->header.cf) >> 3;

  src_ovl.buffer = &src_buf;
  src_ovl.color = ((uint32_t)opa << 24) | (lv_color_to32(color) & 0xffffff);
  src_ovl.area.x = 0;
  src_ovl.area.y = 0;
  src_ovl.area.w = copy_w;
  src_ovl.area.h = copy_h;

  dest_buf.memory = dest;
  dest_buf.width = copy_w;
  dest_buf.height = copy_h;
  dest_buf.stride = dest_stride * sizeof(lv_color_t);
  dest_buf.format = LV_DMA2D_COLOR_FORMAT;

  _lv_acts_dma2d_clean_img_src(src);

  if (opa < LV_OPA_MAX || src_buf.format != DMA2D_PF_RGB565)
    {
      struct acts_dma2d_overlay_s bg_ovl =
      {
          .buffer = &dest_buf,
          .area = src_ovl.area,
      };

      struct dma2ddev_blend_s blend =
      {
          .dest = &dest_buf,
          .fg = &src_ovl,
          .bg = &bg_ovl,
      };

      res = ioctl(dma2d_fd, DMA2DDEVIO_BLEND, (unsigned long)&blend);
    }
  else
    {
      struct dma2ddev_blit_s blit =
      {
          .dest = &dest_buf,
          .src = &src_ovl,
      };

      res = ioctl(dma2d_fd, DMA2DDEVIO_BLIT, (unsigned long)&blit);
    }

  return (res >= 0) ? LV_RES_OK : LV_RES_INV;
}

/****************************************************************************
 * Name: _lv_draw_acts_dma2d_layer_init
 ****************************************************************************/

static lv_draw_layer_ctx_t * _lv_draw_acts_dma2d_layer_init(
        lv_draw_ctx_t * draw_ctx, lv_draw_layer_ctx_t * layer_ctx,
        lv_draw_layer_flags_t flags)
{
  _lv_draw_acts_dma2d_wait_for_finish(draw_ctx);

  return lv_draw_sw_layer_create(draw_ctx, layer_ctx, flags);
}

/****************************************************************************
 * Name: _lv_draw_acts_dma2d_layer_adjust
 ****************************************************************************/

static void _lv_draw_acts_dma2d_layer_adjust(lv_draw_ctx_t * draw_ctx,
        lv_draw_layer_ctx_t * layer_ctx, lv_draw_layer_flags_t flags)
{
  _lv_draw_acts_dma2d_wait_for_finish(draw_ctx);

  lv_draw_sw_layer_adjust(draw_ctx, layer_ctx, flags);
}

/****************************************************************************
 * Name: _lv_draw_acts_dma2d_layer_blend
 ****************************************************************************/

static void _lv_draw_acts_dma2d_layer_blend(lv_draw_ctx_t * draw_ctx,
        lv_draw_layer_ctx_t * layer_ctx, const lv_draw_img_dsc_t * draw_dsc)
{
  _lv_draw_acts_dma2d_wait_for_finish(draw_ctx);

  lv_draw_sw_layer_blend(draw_ctx, layer_ctx, draw_dsc);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lv_acts_dma2d_interface_init
 *
 * Description:
 *   Actions DMA2D interface initialization.
 *
 ****************************************************************************/

lv_res_t lv_acts_dma2d_interface_init(void)
{
  dma2d_fd = open(CONFIG_LV_ACTS_DMA2D_INTERFACE_DEFAULT_DEVICEPATH, 0);
  return (dma2d_fd >= 0) ? LV_RES_OK : LV_RES_INV;
}

/****************************************************************************
 * Name: lv_acts_dma2d_draw_ctx_init
 *
 * Description:
 *   Actions DMA2D draw context init callback. (Do not call directly)
 *
 ****************************************************************************/

void lv_acts_dma2d_draw_ctx_init(lv_disp_drv_t * drv,
                                 lv_draw_ctx_t * draw_ctx)
{
  lv_draw_sw_init_ctx(drv, draw_ctx);

#if LV_COLOR_DEPTH == 32 || (LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 0)
  if (dma2d_fd >= 0)
    {
      draw_ctx->wait_for_finish = _lv_draw_acts_dma2d_wait_for_finish;
      draw_ctx->draw_img_decoded = _lv_draw_acts_dma2d_draw_img_decoded;
      ((lv_draw_sw_ctx_t *)draw_ctx)->blend = _lv_draw_acts_dma2d_blend;

      draw_ctx->layer_init = _lv_draw_acts_dma2d_layer_init;
      draw_ctx->layer_adjust = _lv_draw_acts_dma2d_layer_adjust;
      draw_ctx->layer_blend = _lv_draw_acts_dma2d_layer_blend;
      draw_ctx->layer_instance_size = sizeof(lv_draw_sw_layer_ctx_t);
    }
#endif
}

/****************************************************************************
 * Name: lv_acts_dma2d_draw_ctx_deinit
 *
 * Description:
 *   Actions DMA2D draw context deinit callback. (Do not call directly)
 *
 ****************************************************************************/

void lv_acts_dma2d_draw_ctx_deinit(lv_disp_drv_t * drv,
                                   lv_draw_ctx_t * draw_ctx)
{
  lv_draw_sw_deinit_ctx(drv, draw_ctx);
}
