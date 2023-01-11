/****************************************************************************
 * apps/testing/gpu/vg_lite/vg_lite_test.h
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

#ifndef __APPS_TESTING_GPU_VG_LITE_VG_LITE_TEST_H__
#define __APPS_TESTING_GPU_VG_LITE_VG_LITE_TEST_H__

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stddef.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct gpu_test_context_s;

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
 * Name: vg_lite_test
 ****************************************************************************/

int vg_lite_test(FAR struct gpu_test_context_s *ctx);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __APPS_TESTING_GPU_VG_LITE_VG_LITE_TEST_H__ */
