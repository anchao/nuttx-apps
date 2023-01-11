/****************************************************************************
 * apps/testing/gpu/gpu_recorder.c
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

#include "gpu_recorder.h"
#include "gpu_test.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct gpu_recorder_s
{
  FAR struct gpu_test_context_s *ctx;
  int fd;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: gpu_recorder_create
 ****************************************************************************/

FAR struct gpu_recorder_s *gpu_recorder_create(
    FAR struct gpu_test_context_s *ctx,
    const char *name)
{
  FAR struct gpu_recorder_s *recorder;
  char path[PATH_MAX];
  char filename[64];
  gpu_get_localtime_str(filename, sizeof(filename));

  snprintf(path, sizeof(path), "%s/report_%s_%s.csv",
           ctx->param.output_dir, name, filename);

  int fd = open(path, O_CREAT | O_WRONLY | O_CLOEXEC, 0666);

  if (fd < 0)
    {
      GPU_LOG_ERROR("open %s failed", path);
      return NULL;
    }

  recorder = calloc(1, sizeof(struct gpu_recorder_s));
  GPU_ASSERT_NULL(recorder);
  recorder->fd = fd;
  recorder->ctx = ctx;
  GPU_LOG_WARN("recorder file: %s created, fd = %d", path, fd);
  return recorder;
}

/****************************************************************************
 * Name: gpu_recorder_delete
 ****************************************************************************/

void gpu_recorder_delete(FAR struct gpu_recorder_s *recorder)
{
  GPU_ASSERT_NULL(recorder);
  close(recorder->fd);
  free(recorder);
  GPU_LOG_WARN("recorder deleted");
}

/****************************************************************************
 * Name: gpu_recorder_write_string
 ****************************************************************************/

int gpu_recorder_write_string(FAR struct gpu_recorder_s *recorder,
                              FAR const char *str)
{
  GPU_ASSERT_NULL(recorder);
  GPU_ASSERT_NULL(str);
  size_t len = strlen(str);
  size_t written = 0;

  while (written < len)
    {
      ssize_t ret = write(recorder->fd, str + written, len - written);

      if (ret < 0)
        {
          GPU_LOG_ERROR("write failed: %d", errno);
          return ERROR;
        }

      written += ret;
    }

  return OK;
}
