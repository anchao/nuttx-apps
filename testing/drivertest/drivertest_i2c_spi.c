/****************************************************************************
 * apps/testing/drivertest/drivertest_i2c_spi.c
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
#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <poll.h>
#include <sensor/accel.h>
#include <nuttx/sensors/bmi160.h>

#include <cmocka.h>
#include <uORB/uORB.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define ACC_TIMEOUT      1000
#define READ_TIMES       100

/****************************************************************************
 * Private Type Declarations
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: read_from_device
 ****************************************************************************/

static void read_from_device(FAR void **state)
{
  FAR const struct orb_metadata *meta;
  struct sensor_accel accel_data;
  struct pollfd fds;
  uint64_t start_time;
  uint64_t end_time;
  int nb_msgs = 0;
  int fd;
  int ret;
  int i;

  meta = ORB_ID(sensor_accel_uncal);
  fd = orb_subscribe_multi(meta, 0);

  fds.fd = fd;
  fds.events = POLLIN;

  /* waiting for the sensor to initialize */

  usleep(50000);

  orb_copy(meta, fd, &accel_data);
  start_time = accel_data.timestamp;
  end_time = accel_data.timestamp;

  for (i = 0; i < READ_TIMES; i++)
    {
      /* wait for up to 1000ms for data */

      if (poll(&fds, 1, ACC_TIMEOUT) > 0)
        {
          if (fds.revents & POLLIN)
            {
              ret = orb_copy(meta, fd, &accel_data);

#ifdef CONFIG_DEBUG_UORB
              if (ret == OK && meta->o_cb != NULL)
                {
                  meta->o_cb(ORB_ID(sensor_accel_uncal), &accel_data);
                }
#endif

              end_time = accel_data.timestamp;
              nb_msgs++;
            }
        }
      else if (errno != EINTR)
        {
          snerr("Waited for %d milliseconds without a message. "
                        "Giving up. err:%d", ACC_TIMEOUT, errno);
          break;
        }
    }

  orb_unsubscribe(fd);

  assert_int_equal(nb_msgs, READ_TIMES);
  printf("mean:  %.2f ms\n", (float)(end_time - start_time) / READ_TIMES);
  printf("spend: %d ms\n", (int)(end_time - start_time));
  printf("recv/total: %d/%d \n", nb_msgs, READ_TIMES);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * drivertest_i2c_spi_main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  const struct CMUnitTest tests[] =
    {
      cmocka_unit_test(read_from_device),
    };

  return cmocka_run_group_tests(tests, NULL, NULL);
}

