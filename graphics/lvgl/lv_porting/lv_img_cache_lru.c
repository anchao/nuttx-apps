/****************************************************************************
 * apps/graphics/lvgl/lv_porting/lv_img_cache_lru.c
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

#include "lv_img_cache_lru.h"
#include "lvgl/lvgl.h"
#include <nuttx/mm/mm.h>
#include <malloc.h>
#include <stdlib.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define IMG_CACHE_HEAP_NAME "img_cache"
#define IMG_CACHE_DONT_ALIGN 0

/****************************************************************************
 * Private Type Declarations
 ****************************************************************************/

struct cache_manager_s
  {
    lv_ll_t              cache_ll;
    uint16_t             entry_used;
    uint16_t             entry_size;
    size_t               mem_size;
    size_t               heap_size;
    void                 *heap_mem;
    FAR struct mm_heap_s *heap;
  };

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static FAR _lv_img_cache_entry_t *img_cache_open(FAR const void *src,
                                                 lv_color32_t color,
                                                 int32_t frame_id);

static bool img_cache_set_size(uint16_t entry_size, size_t mem_size);

static void img_cache_set_entry_size(uint16_t new_entry_size);

static void img_cache_invalidate_src(FAR const void *src);

static bool remove_cache_tail_entry(void);

static FAR void *img_cache_alloc(size_t align, size_t size,
                                 bool re, FAR void *ptr);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct cache_manager_s cache_manager =
{
  0
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lv_img_cache_lru_init
 *
 * Description:
 *   Initialize image cache manager.
 *
 * Input Parameters:
 *   entry_size - The size of each cache entry.
 *   mem_size   - The size of the memory used by the cache manager.
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void lv_img_cache_lru_init(uint16_t entry_size, uint32_t mem_size)
{
  lv_img_cache_manager_t manager;

  if (!img_cache_set_size(entry_size, mem_size))
    {
      LV_LOG_WARN("img cache init failed");
      return;
    }

  /* Before replacing the image cache manager,
   * you should ensure that all caches are cleared to prevent memory leaks.
   */

  lv_img_cache_invalidate_src(NULL);

  /* Initialize image cache manager. */

  lv_img_cache_manager_init(&manager);
  manager.open_cb           = img_cache_open;
  manager.set_size_cb       = img_cache_set_entry_size;
  manager.invalidate_src_cb = img_cache_invalidate_src;

  /* Apply image cache manager to LVGL. */

  lv_img_cache_manager_apply(&manager);
}

/****************************************************************************
 * Name: lv_img_cache_malloc
 ****************************************************************************/

FAR void *lv_img_cache_malloc(size_t size)
{
  return img_cache_alloc(IMG_CACHE_DONT_ALIGN, size, false, NULL);
}

/****************************************************************************
 * Name: lv_img_cache_aligned_alloc
 ****************************************************************************/

FAR void *lv_img_cache_aligned_alloc(size_t align, size_t size)
{
  return img_cache_alloc(align, size, false, NULL);
}

/****************************************************************************
 * Name: lv_img_cache_realloc
 ****************************************************************************/

FAR void *lv_img_cache_realloc(FAR void *data_p, size_t new_size)
{
  return img_cache_alloc(IMG_CACHE_DONT_ALIGN, new_size, true, data_p);
}

/****************************************************************************
 * Name: lv_img_cache_free
 ****************************************************************************/

void lv_img_cache_free(FAR void *ptr)
{
  mm_free(cache_manager.heap, ptr);
}

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: img_cache_match
 ****************************************************************************/

static bool img_cache_match(FAR const void *src1, FAR const void *src2)
{
  lv_img_src_t src_type = lv_img_src_get_type(src1);

  if (src_type == LV_IMG_SRC_VARIABLE)
    {
      return src1 == src2;
    }

  if (src_type != LV_IMG_SRC_FILE)
    {
      return false;
    }

  if (lv_img_src_get_type(src2) != LV_IMG_SRC_FILE)
    {
      return false;
    }

  return strcmp(src1, src2) == 0;
}

/****************************************************************************
 * Name: insert_cache_entry
 ****************************************************************************/

static FAR _lv_img_cache_entry_t *insert_cache_entry(
  FAR const _lv_img_cache_entry_t *entry)
{
  _lv_img_cache_entry_t *head = _lv_ll_ins_head(&cache_manager.cache_ll);

  LV_ASSERT_MALLOC(head);
  LV_ASSERT_NULL(entry);

  lv_memcpy(head, entry, sizeof(_lv_img_cache_entry_t));
  cache_manager.entry_used++;

  LV_LOG_INFO("cache entry inserted, entry_used: %d",
              cache_manager.entry_used);

  return head;
}

/****************************************************************************
 * Name: remove_cache_entry
 ****************************************************************************/

static void remove_cache_entry(FAR _lv_img_cache_entry_t *entry)
{
  LV_ASSERT_NULL(entry);

  lv_img_decoder_close(&entry->dec_dsc);
  _lv_ll_remove(&cache_manager.cache_ll, entry);
  lv_mem_free(entry);
  cache_manager.entry_used--;

  LV_LOG_INFO("cache entry removed, entry_used: %d",
              cache_manager.entry_used);
}

/****************************************************************************
 * Name: remove_cache_tail_entry
 ****************************************************************************/

static bool remove_cache_tail_entry(void)
{
  FAR _lv_img_cache_entry_t *tail;

  tail = _lv_ll_get_tail(&cache_manager.cache_ll);

  if (tail)
    {
      remove_cache_entry(tail);
      return true;
    }

  LV_LOG_WARN("Image cache is empty.");

  return false;
}

/****************************************************************************
 * Name: img_cache_open
 ****************************************************************************/

static FAR _lv_img_cache_entry_t *img_cache_open(FAR const void *src,
                                                 lv_color32_t color,
                                                 int32_t frame_id)
{
  FAR _lv_img_cache_entry_t *cache;
  _lv_img_cache_entry_t     cache_entry;
  lv_res_t                  open_res;

  lv_memset_00(&cache_entry, sizeof(_lv_img_cache_entry_t));

  if (cache_manager.entry_size == 0)
    {
      LV_LOG_WARN("cache size is 0");
      return NULL;
    }

  /* Try to find cache entry */

  _LV_LL_READ(&cache_manager.cache_ll, cache)
    {
      if (memcmp(&color, &cache->dec_dsc.color, sizeof(color)) == 0
          && frame_id == cache->dec_dsc.frame_id
          && img_cache_match(src, cache->dec_dsc.src))
        {
          LV_LOG_TRACE("cache hit");
          FAR _lv_img_cache_entry_t *head;
          head = _lv_ll_get_head(&cache_manager.cache_ll);
          _lv_ll_move_before(&cache_manager.cache_ll, cache, head);
          return cache;
        }
    }

  open_res = lv_img_decoder_open(
    &cache_entry.dec_dsc, src, color, frame_id
  );

  if (open_res != LV_RES_OK)
    {
      LV_LOG_WARN("image draw cannot open the image resource");
      return NULL;
    }

  /* If the number of cache entries is insufficient, look for the entry
   * that can be reused
   * */

  if (cache_manager.entry_used >= cache_manager.entry_size)
    {
      LV_LOG_INFO("cache entry is full, close tail entry");
      remove_cache_tail_entry();
    }

  return insert_cache_entry(&cache_entry);
}

/****************************************************************************
 * Name: set_image_cache
 ****************************************************************************/

static bool img_cache_set_size(uint16_t entry_size, size_t mem_size)
{
  struct mallinfo info;
  FAR void        *mem;

  if (entry_size == 0 || mem_size == 0)
    {
      LV_LOG_WARN("cache size is 0");
      return false;
    }

  if (cache_manager.heap != NULL)
    {
      LV_LOG_WARN("heap has been initialized");
      return false;
    }

  _lv_ll_init(&cache_manager.cache_ll, sizeof(_lv_img_cache_entry_t));

  cache_manager.entry_size = entry_size;
  cache_manager.mem_size   = mem_size;

  mem = malloc(mem_size);
  LV_ASSERT_MALLOC(mem);

  if (mem == NULL)
    {
      LV_LOG_ERROR("malloc %zu failed", mem_size);
      return false;
    }

  cache_manager.heap_mem = mem;
  cache_manager.heap     = mm_initialize(
    IMG_CACHE_HEAP_NAME,
    mem,
    mem_size
  );

  info = mm_mallinfo(cache_manager.heap);
  cache_manager.heap_size = info.arena;

  LV_LOG_WARN("heap info:");
  LV_LOG_WARN("  heap: %p", cache_manager.heap);
  LV_LOG_WARN("  mem: %p", mem);
  LV_LOG_WARN("  mem_size: %zu", mem_size);
  LV_LOG_WARN("  arena: %d", info.arena);
  LV_LOG_WARN("  ordblks: %d", info.ordblks);
  LV_LOG_WARN("  aordblks: %d", info.aordblks);
  LV_LOG_WARN("  mxordblk: %d", info.mxordblk);
  LV_LOG_WARN("  uordblks: %d", info.uordblks);
  LV_LOG_WARN("  fordblks: %d", info.fordblks);
  return true;
}

/****************************************************************************
 * Name: img_cache_set_entry_size
 ****************************************************************************/

static void img_cache_set_entry_size(uint16_t new_entry_size)
{
  if (new_entry_size == cache_manager.entry_size)
    {
      return;
    }

  if (new_entry_size > cache_manager.entry_size)
    {
      cache_manager.entry_size = new_entry_size;
      return;
    }

  while (cache_manager.entry_used > new_entry_size)
    {
      remove_cache_tail_entry();
    }

  cache_manager.entry_size = new_entry_size;
}

/****************************************************************************
 * Name: img_cache_invalidate_src
 ****************************************************************************/

static void img_cache_invalidate_src(FAR const void *src)
{
  FAR _lv_img_cache_entry_t *cache;
  cache = _lv_ll_get_head(&cache_manager.cache_ll);

  while (cache != NULL)
    {
      FAR _lv_img_cache_entry_t *cache_next;
      cache_next = _lv_ll_get_next(&cache_manager.cache_ll, cache);

      if (src == NULL || img_cache_match(src, cache->dec_dsc.src))
        {
          remove_cache_entry(cache);
        }

      cache = cache_next;
    }
}

/****************************************************************************
 * Name: img_cache_alloc
 ****************************************************************************/

static FAR void *img_cache_alloc(size_t align, size_t size, bool re, FAR
                                 void *ptr)
{
  LV_LOG_INFO("img_cache_alloc (align: %zu, size: %zu, re: %d, ptr: %p)",
              align, size, (int)re, ptr);

  if (size > cache_manager.heap_size)
    {
      LV_LOG_WARN("Image cache memory is not enough for size %zu",
                  size);
      return NULL;
    }

  while (1)
    {
      FAR void *mem;

      if (re)
        {
          mem = mm_realloc(cache_manager.heap, ptr, size);
        }
      else
        {
          if (align != IMG_CACHE_DONT_ALIGN)
            {
              mem = mm_memalign(cache_manager.heap, align, size);
            }
          else
            {
              mem = mm_malloc(cache_manager.heap, size);
            }
        }

      if (mem)
        {
          LV_LOG_INFO("Image malloc addr: %p success", mem);
          return mem;
        }

      LV_LOG_INFO(
        "Image malloc size: %zu failed, "
        "try to free the memory of the tail node.",
        size);

      /* If the memory is not enough, try to free the memory of the
       * tail node.
       * */

      if (!remove_cache_tail_entry())
        {
          break;
        }
    }

  LV_LOG_WARN("Image cache memory is not enough for size %zu",
              size);
  return NULL;
}
