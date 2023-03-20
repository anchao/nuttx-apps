/****************************************************************************
 * apps/interpreters/wamr/wamr_custom_init.c
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/param.h>
#include <sys/types.h>

#include "wasm_export.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void
va_list_string2native(wasm_exec_env_t exec_env, const char *format,
                      va_list ap)
{
  wasm_module_inst_t module_inst = get_module_inst(exec_env);
  char *pos = *((char **)&ap);

  if (pos == NULL)
    {
      return;
    }

  int long_ctr = 0;
  int might = 0;

  while (*format)
    {
      if (!might)
        {
          if (*format == '%')
            {
              might = 1;
              long_ctr = 0;
            }
        }
      else
        {
          switch (*format)
            {
              case '.':
              case '+':
              case '-':
              case ' ':
              case '#':
              case '0':
              case '1':
              case '2':
              case '3':
              case '4':
              case '5':
              case '6':
              case '7':
              case '8':
              case '9':
                  goto still_might;

              case 't':
              case 'z':
                  long_ctr = 1;
                  goto still_might;

              case 'j':
                  long_ctr = 2;
                  goto still_might;

              case 'l':
                  long_ctr++;
              case 'h':
                  goto still_might;

              case 'o':
              case 'd':
              case 'i':
              case 'u':
              case 'p':
              case 'x':
              case 'X':
              case 'c':
                {
                  if (long_ctr < 2)
                    {
                      pos += sizeof(int32_t);
                    }
                  else
                    {
                      pos += sizeof(int64_t);
                    }
                  break;
                }

              case 'e':
              case 'E':
              case 'g':
              case 'G':
              case 'f':
              case 'F':
                {
                  pos += sizeof(double);
                  break;
                }

              case 's':
                {
                  *(uint32_t *)pos =
                      (uintptr_t)addr_app_to_native(*(uintptr_t *)pos);
                  pos += sizeof(uintptr_t);
                  break;
                }

              default:
                  break;
          }

        might = 0;
      }

  still_might:
      ++format;
  }
}

static void
scanf_begin(wasm_module_inst_t module_inst, va_list ap)
{
  uintptr_t *apv = *(uintptr_t **)&ap;
  if (apv == NULL)
    {
      return;
    }
  while (*apv != 0)
    {
      *apv = (uintptr_t)addr_app_to_native(*apv);
      apv++;
    }
}

static void
scanf_end(wasm_module_inst_t module_inst, va_list ap)
{
  uintptr_t *apv = *(uintptr_t **)&ap;
  if (apv == NULL)
    {
      return;
    }
  while (*apv != 0)
    {
      *apv = (uintptr_t)addr_native_to_app((void *)*apv);
      apv++;
    }
}

static pthread_mutex_t g_compare_mutex = PTHREAD_MUTEX_INITIALIZER;
static wasm_exec_env_t g_compare_env;
static void           *g_compare_func;

static int
compare_proxy(const void *a, const void *b)
{
  wasm_module_inst_t module_inst = get_module_inst(g_compare_env);
  uint32_t argv[2];

  argv[0] = addr_native_to_app((void *)a);
  argv[1] = addr_native_to_app((void *)b);

  return wasm_runtime_call_indirect(g_compare_env,
           (uint32_t)g_compare_func, 2, argv) ? argv[0] : 0;
}

#ifndef GLUE_FUNCTION_qsort
#define GLUE_FUNCTION_qsort
void glue_qsort(wasm_exec_env_t env, uintptr_t parm1, uintptr_t parm2,
                uintptr_t parm3, uintptr_t parm4)
{
  wasm_module_inst_t module_inst = get_module_inst(env);
  pthread_mutex_lock(&g_compare_mutex);
  g_compare_env = env;
  g_compare_func = parm4;
  qsort((FAR void *)addr_app_to_native(parm1), (size_t)parm2,
        (size_t)parm3, compare_proxy);
  pthread_mutex_unlock(&g_compare_mutex);
}

#endif /* GLUE_FUNCTION_qsort */

#ifndef GLUE_FUNCTION_bsearch
#define GLUE_FUNCTION_bsearch
uintptr_t glue_bsearch(wasm_exec_env_t env, uintptr_t parm1, uintptr_t parm2,
                       uintptr_t parm3, uintptr_t parm4, uintptr_t parm5)
{
  wasm_module_inst_t module_inst = get_module_inst(env);
  uintptr_t ret;

  pthread_mutex_lock(&g_compare_mutex);
  g_compare_env = env;
  g_compare_func = parm5;
  ret = bsearch((FAR const void *)addr_app_to_native(parm1),
                (FAR const void *)addr_app_to_native(parm2),
                (size_t)parm3, (size_t)parm4, compare_proxy);
  pthread_mutex_unlock(&g_compare_mutex);
  return ret;
}

#endif /* GLUE_FUNCTION_bsearch */

static void glue_msghdr_begin(wasm_module_inst_t module_inst,
                              FAR struct msghdr *hdr)
{
  int i;

  hdr->msg_iov = addr_app_to_native((uintptr_t)hdr->msg_iov);

  for (i = 0; i < hdr->msg_iovlen; i++)
    {
      hdr->msg_iov[i].iov_base =
        addr_app_to_native((uintptr_t)hdr->msg_iov[i].iov_base);
    }

  if (hdr->msg_name != NULL && hdr->msg_namelen > 0)
    {
      hdr->msg_name = addr_app_to_native(hdr->msg_name);
    }

  if (hdr->msg_control != NULL && hdr->msg_controllen > 0)
    {
      hdr->msg_control = addr_app_to_native((uintptr_t)hdr->msg_control);
    }
}

static void glue_msghdr_end(wasm_module_inst_t module_inst,
                            FAR struct msghdr *hdr)
{
  int i;

  for (i = 0; i < hdr->msg_iovlen; i++)
    {
      hdr->msg_iov[i].iov_base =
        addr_native_to_app((uintptr_t)hdr->msg_iov[i].iov_base);
    }

  hdr->msg_iov = addr_native_to_app((uintptr_t)hdr->msg_iov);

  if (hdr->msg_name != NULL && hdr->msg_namelen > 0)
    {
      hdr->msg_name = addr_native_to_app(hdr->msg_name);
    }

  if (hdr->msg_control != NULL && hdr->msg_controllen > 0)
    {
      hdr->msg_control = addr_native_to_app((uintptr_t)hdr->msg_control);
    }
}

#ifndef GLUE_FUNCTION_sendmsg
#define GLUE_FUNCTION_sendmsg
uintptr_t glue_sendmsg(wasm_exec_env_t env, uintptr_t parm1,
                       uintptr_t parm2, uintptr_t parm3)
{
  wasm_module_inst_t module_inst = get_module_inst(env);
  FAR struct msghdr *hdr =
    (FAR struct msghdr *)addr_app_to_native((uintptr_t)parm2);

  glue_msghdr_begin(module_inst, hdr);
  uintptr_t ret = sendmsg((int)parm1,
                          (FAR struct msghdr *)addr_app_to_native(parm2),
                          (int)parm3);
  glue_msghdr_end(module_inst, hdr);

  return ret;
}

#endif /* GLUE_FUNCTION_sendmsg */

#ifndef GLUE_FUNCTION_recvmsg
#define GLUE_FUNCTION_recvmsg
uintptr_t glue_recvmsg(wasm_exec_env_t env, uintptr_t parm1,
                       uintptr_t parm2, uintptr_t parm3)
{
  wasm_module_inst_t module_inst = get_module_inst(env);
  FAR struct msghdr *hdr =
    (FAR struct msghdr *)addr_app_to_native((uintptr_t)parm2);

  glue_msghdr_begin(module_inst, hdr);
  uintptr_t ret = recvmsg((int)parm1,
                          (FAR struct msghdr *)addr_app_to_native(parm2),
                          (int)parm3);
  glue_msghdr_end(module_inst, hdr);

  return ret;
}

#endif /* GLUE_FUNCTION_recvmsg */

#ifndef GLUE_FUNCTION_strsep
#define GLUE_FUNCTION_strsep
uintptr_t glue_strsep(wasm_exec_env_t env, uintptr_t parm1, uintptr_t parm2)
{
  wasm_module_inst_t module_inst = get_module_inst(env);
  FAR char **stringp = parm1;

  if (*stringp != NULL)
    {
      *stringp = addr_app_to_native(*stringp);
    }

  return addr_native_to_app((uintptr_t)strsep(
    (FAR char **)addr_app_to_native(parm1),
    (FAR const char *)addr_app_to_native(parm2)));
}

#endif /* GLUE_FUNCTION_strsep */

#ifndef GLUE_FUNCTION_scandir
#define GLUE_FUNCTION_scandir
uintptr_t glue_scandir(wasm_exec_env_t env, uintptr_t parm1, uintptr_t parm2,
                       uintptr_t parm3, uintptr_t parm4)
{
  wasm_module_inst_t module_inst = get_module_inst(env);

  return scandir((FAR const char *)addr_app_to_native(parm1),
                 (FAR struct dirent ***)addr_app_to_native(parm2),
                 (FAR void *)addr_app_to_native(parm3), alphasort);
}

#endif /* GLUE_FUNCTION_scandir */

#ifndef GLUE_FUNCTION_daemon
#define GLUE_FUNCTION_daemon
uintptr_t glue_daemon(wasm_exec_env_t env, uintptr_t parm1, uintptr_t parm2)
{
  return 0;
}

#endif /* GLUE_FUNCTION_daemon */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "libc_glue.c"
#include "libm_glue.c"
#include "syscall_glue.c"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

bool
wamr_custom_init(RuntimeInitArgs *init_args)
{
  bool ret = wasm_runtime_full_init(init_args);

  /* Add extra init hook here */

  ret = wasm_native_register_natives("env", g_syscall_native_symbols,
                                      nitems(g_syscall_native_symbols));
  if (ret == true)
    {
      ret = wasm_native_register_natives("env", g_libc_native_symbols,
                                          nitems(g_libc_native_symbols));
      if (ret == true)
        {
          ret = wasm_native_register_natives("env", g_libm_native_symbols,
                                              nitems(g_libm_native_symbols));
        }
    }

  return ret;
}
