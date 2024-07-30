/****************************************************************************
 * apps/examples/hello/hello_main.c
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

#include <nuttx/config.h>
#include <stdio.h>

#include <nuttx/event.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * hello_main
 ****************************************************************************/
FAR void *client_thread2(pthread_addr_t pvarg)
{
  event_t *event = pvarg;
  int ret;
  printf("%s: %d, in\n", __func__, __LINE__);

  ret = nxevent_wait(event, 0xFF);
  printf("%s: %d, ret: %d\n", __func__, __LINE__, ret);

  return NULL;
}
FAR void *client_thread(pthread_addr_t pvarg)
{
  event_t *event = pvarg;
  int ret;
  printf("%s: %d, in\n", __func__, __LINE__);

  ret = nxevent_wait(event, 0xF);
  printf("%s: %d, ret: %d\n", __func__, __LINE__, ret);
  ret = nxevent_wait(event, 0xF);
  printf("%s: %d, ret: %d\n", __func__, __LINE__, ret);
  ret = nxevent_wait(event, 0xF);
  printf("%s: %d, ret: %d\n", __func__, __LINE__, ret);
  ret = nxevent_wait(event, 0);
  printf("%s: %d, wait 0, ret: %d\n", __func__, __LINE__, ret);
  ret = nxevent_wait(event, 0xF);
  printf("%s: %d, wait 0xF, ret: %d\n", __func__, __LINE__, ret);
  ret = nxevent_wait(event, 0x1F);
  printf("%s: %d, wait 0xF, ret: %d\n", __func__, __LINE__, ret);

  return NULL;
}
static event_t event = NXEVENT_INITIALIZER(0xf);

int main(int argc, FAR char *argv[])
{
  struct sched_param sparam;
  pthread_attr_t attr;
  pthread_t tid;
  int rv;

  nxevent_init(&event, 0);

  printf("%s: %d, init event: 0\n", __func__, __LINE__);

  pthread_attr_init(&attr);

  sparam.sched_priority = 99;
  pthread_attr_setschedparam(&attr, &sparam);

  rv = pthread_create(&tid, &attr, client_thread, &event);

  sleep(1);

  nxevent_post(&event, 0xF);
  printf("%s: %d, post event: 0xf, rv: %d\n", __func__, __LINE__, rv);

  sleep(1);

  nxevent_post(&event, 0xF);
  printf("%s: %d, post event: 0xf\n", __func__, __LINE__);

  sleep(1);

  nxevent_post(&event, 0x1);
  printf("%s: %d, post event: 0x1\n", __func__, __LINE__);
  nxevent_post(&event, 0x2);
  printf("%s: %d, post event: 0x2\n", __func__, __LINE__);
  nxevent_post(&event, 0x4);
  printf("%s: %d, post event: 0x4\n", __func__, __LINE__);
  nxevent_post(&event, 0x8);
  printf("%s: %d, post event: 0x8\n", __func__, __LINE__);

  sleep(1);

  nxevent_post(&event, 0x8);
  printf("%s: %d, post event: 0x8\n", __func__, __LINE__);

  sleep(1);

  nxevent_post(&event, 0);
  printf("%s: %d, post event: 0x0\n", __func__, __LINE__);

  sleep(1);
  nxevent_post(&event, 0xFF);
  printf("%s: %d, post event: 0x0\n", __func__, __LINE__);

  sleep(1);
  printf("Hello, World!!\n");

  return 0;
}
