/****************************************************************************
 * apps/testing/gpu/gpu_log.c
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

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include "gpu_log.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: gpu_log_printf
 ****************************************************************************/

void gpu_log_printf(enum gpu_log_level_type_e level,
                    FAR const char *func,
                    FAR const char *fmt,
                    ...)
{
  struct va_format vaf;
  va_list ap;

  static const int priority[_GPU_LOG_LEVEL_LAST] =
  {
    LOG_INFO, LOG_WARNING, LOG_ERR
  };

  va_start(ap, fmt);
  vaf.fmt = fmt;
  vaf.va  = &ap;
  syslog(priority[level], "[GPU] %s: %pV\n", func, &vaf);
  va_end(ap);
}
