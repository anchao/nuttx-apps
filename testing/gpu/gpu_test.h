/****************************************************************************
 * apps/testing/gpu/gpu_test.h
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

#ifndef __APPS_TESTING_GPU_TEST_H
#define __APPS_TESTING_GPU_TEST_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <assert.h>
#include "gpu_log.h"
#include "gpu_utils.h"
#include "gpu_recorder.h"
#include "gpu_screenshot.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Types
 ****************************************************************************/

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
 * Name: gpu_test_run
 ****************************************************************************/

int gpu_test_run(FAR struct gpu_test_context_s *ctx);

/****************************************************************************
 * Name: gpu_fb_update
 ****************************************************************************/

void gpu_fb_update(FAR struct gpu_test_context_s *ctx);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __APPS_TESTING_GPU_TEST_H */
