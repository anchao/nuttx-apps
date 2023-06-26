/****************************************************************************
 * apps/graphics/lvgl/lv_porting/lv_dcache_interface.c
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

#include <lvgl/lvgl.h>
#include <nuttx/cache.h>
#include "lv_dcache_interface.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: clean_dcache
 ****************************************************************************/

static void clean_dcache(FAR lv_disp_drv_t * disp_drv)
{
  LV_UNUSED(disp_drv);
  up_clean_dcache_all();
}

/****************************************************************************
 * Name: clean_dcache_by_addr
 ****************************************************************************/

static void clean_dcache_by_addr(FAR lv_disp_drv_t * disp_drv,
                                 lv_uintptr_t start,
                                 lv_uintptr_t end)
{
  LV_UNUSED(disp_drv);
  up_clean_dcache(start, end);
}

/****************************************************************************
 * Name: flush_dcache
 ****************************************************************************/

static void flush_dcache(FAR lv_disp_drv_t * disp_drv)
{
  LV_UNUSED(disp_drv);
  up_flush_dcache_all();
}

/****************************************************************************
 * Name: flush_dcache_by_addr
 ****************************************************************************/

static void flush_dcache_by_addr(FAR lv_disp_drv_t * disp_drv,
                                 lv_uintptr_t start,
                                 lv_uintptr_t end)
{
  LV_UNUSED(disp_drv);
  up_flush_dcache(start, end);
}

/****************************************************************************
 * Name: invalidate_dcache
 ****************************************************************************/

static void invalidate_dcache(FAR lv_disp_drv_t * disp_drv)
{
  LV_UNUSED(disp_drv);
  up_invalidate_dcache_all();
}

/****************************************************************************
 * Name: invalidate_dcache_by_addr
 ****************************************************************************/

static void invalidate_dcache_by_addr(FAR lv_disp_drv_t * disp_drv,
                                      lv_uintptr_t start,
                                      lv_uintptr_t end)
{
  LV_UNUSED(disp_drv);
  up_invalidate_dcache(start, end);
}

/****************************************************************************
 * Name: lv_dcache_interface_init
 *
 * Description:
 *   Cache interface initialization.
 *
 ****************************************************************************/

void lv_dcache_interface_init(void)
{
  FAR lv_disp_t *disp;

  disp = lv_disp_get_default();

  if (disp == NULL)
    {
      LV_LOG_WARN("No display registered");
      return;
    }

  disp->driver->clean_dcache_cb = clean_dcache;
  disp->driver->clean_dcache_by_addr_cb = clean_dcache_by_addr;
  disp->driver->flush_dcache_cb = flush_dcache;
  disp->driver->flush_dcache_by_addr_cb = flush_dcache_by_addr;
  disp->driver->invalidate_dcache_cb = invalidate_dcache;
  disp->driver->invalidate_dcache_by_addr_cb = invalidate_dcache_by_addr;
}
