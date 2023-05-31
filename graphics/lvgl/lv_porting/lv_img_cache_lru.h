/****************************************************************************
 * apps/graphics/lvgl/lv_porting/lv_img_cache_lru.h
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

#ifndef __LV_IMG_CACHE_LRU_H__
#define __LV_IMG_CACHE_LRU_H__

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stddef.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Public Function Protypes
 ****************************************************************************/

/****************************************************************************
 * Name: lv_img_cache_lru_init
 ****************************************************************************/

void lv_img_cache_lru_init(uint16_t entry_size, uint32_t mem_size);

/****************************************************************************
 * Name: lv_img_cache_malloc
 ****************************************************************************/

FAR void *lv_img_cache_malloc(size_t size);

/****************************************************************************
 * Name: lv_img_cache_aligned_alloc
 ****************************************************************************/

FAR void *lv_img_cache_aligned_alloc(size_t align, size_t size);

/****************************************************************************
 * Name: lv_img_cache_realloc
 ****************************************************************************/

FAR void *lv_img_cache_realloc(FAR void *data_p, size_t new_size);

/****************************************************************************
 * Name: lv_img_cache_free
 ****************************************************************************/

void lv_img_cache_free(FAR void *ptr);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __LV_IMG_CACHE_LRU_H__ */
