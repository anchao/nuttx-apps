/****************************************************************************
 * apps/testing/gpu/gpu_log.h
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

#ifndef __APPS_TESTING_GPU_GPU_LOG_H
#define __APPS_TESTING_GPU_GPU_LOG_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define GPU_LOG_INFO(format, ...) \
  gpu_log_printf(GPU_LOG_LEVEL_INFO, \
  __func__, \
  format, \
  ##__VA_ARGS__)
#define GPU_LOG_WARN(format, ...) \
  gpu_log_printf(GPU_LOG_LEVEL_WARN, \
  __func__, \
  format, \
  ##__VA_ARGS__)
#define GPU_LOG_ERROR(format, ...) \
  gpu_log_printf(GPU_LOG_LEVEL_ERROR, \
  __func__, \
  format, \
  ##__VA_ARGS__)

/****************************************************************************
 * Public Types
 ****************************************************************************/

enum gpu_log_level_type_e
{
  GPU_LOG_LEVEL_INFO,
  GPU_LOG_LEVEL_WARN,
  GPU_LOG_LEVEL_ERROR,
  _GPU_LOG_LEVEL_LAST
};

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
 * Name: vglite_log_printf
 ****************************************************************************/

void gpu_log_printf(enum gpu_log_level_type_e level,
                    FAR const char *func,
                    FAR const char *fmt,
                    ...);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __APPS_TESTING_GPU_GPU_LOG_H */
