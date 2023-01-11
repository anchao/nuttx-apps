/****************************************************************************
 * apps/testing/gpu/gpu_recorder.h
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

#ifndef __APPS_TESTING_GPU_RECORDER_H
#define __APPS_TESTING_GPU_RECORDER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stddef.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct gpu_test_context_s;
struct gpu_recorder_s;

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
 * Name: gpu_recoder_create
 ****************************************************************************/

FAR struct gpu_recorder_s *gpu_recorder_create(
    FAR struct gpu_test_context_s *ctx,
    const char *name);

/****************************************************************************
 * Name: gpu_recoder_delete
 ****************************************************************************/

void gpu_recorder_delete(FAR struct gpu_recorder_s *recorder);

/****************************************************************************
 * Name: gpu_recorder_write_string
 ****************************************************************************/

int gpu_recorder_write_string(FAR struct gpu_recorder_s *recorder,
                              FAR const char *str);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __APPS_TESTING_GPU_VGLITE_H */
