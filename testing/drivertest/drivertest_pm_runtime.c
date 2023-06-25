/****************************************************************************
 * apps/testing/drivertest/drivertest_pm_runtime.c
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

#include <nuttx/nuttx.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <cmocka.h>
#include <nuttx/power/pm_runtime.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TEST_PM_RUTIME_FAKE_DRIVER

/****************************************************************************
 * Private Functions Prototypes
 ****************************************************************************/

static int test_pm_runtime_suspend(FAR struct pm_runtime_dev_s *dev);
static int test_pm_runtime_resume(FAR struct pm_runtime_dev_s *dev);
static int test_pm_runtime_idle(FAR struct pm_runtime_dev_s *dev);
struct test_pm_runtime_dev_s
{
  struct pm_runtime_dev_s rd;
  int state;
};

enum
{
  TEST_PM_RUTIME_FAKE_SUSPEND = 0,
  TEST_PM_RUTIME_FAKE_RESUME,
  TEST_PM_RUTIME_FAKE_IDLE,
  TEST_PM_RUTIME_FAKE_UNKOWN,
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct pm_runtime_operations_s g_test_pm_runtime_ops =
{
  test_pm_runtime_suspend,
  test_pm_runtime_resume,
  test_pm_runtime_idle,
};

static struct test_pm_runtime_dev_s g_test_pm_runtine_dev;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int test_pm_runtime_fake_driver_init(void)
{
  g_test_pm_runtine_dev.rd.rpm_ops = &g_test_pm_runtime_ops;

  return pm_runtime_register(&g_test_pm_runtine_dev.rd);
}

static int test_pm_runtime_suspend(FAR struct pm_runtime_dev_s *dev)
{
  struct test_pm_runtime_dev_s *tdev =
                         container_of(dev, struct test_pm_runtime_dev_s, rd);
  tdev->state = TEST_PM_RUTIME_FAKE_SUSPEND;
  return 0;
}

static int test_pm_runtime_resume(FAR struct pm_runtime_dev_s *dev)
{
  struct test_pm_runtime_dev_s *tdev =
                         container_of(dev, struct test_pm_runtime_dev_s, rd);
  tdev->state = TEST_PM_RUTIME_FAKE_RESUME;
  return 0;
}

static int test_pm_runtime_idle(FAR struct pm_runtime_dev_s *dev)
{
  struct test_pm_runtime_dev_s *tdev =
                         container_of(dev, struct test_pm_runtime_dev_s, rd);
  tdev->state = TEST_PM_RUTIME_FAKE_IDLE;
  return 0;
}

static void test_pm_runtime(FAR void **state)
{
  int ret = 0;
  int cnt = 10;

  while (cnt--)
    {
      ret = pm_runtime_get(&g_test_pm_runtine_dev.rd);
      assert_int_equal(ret, -EACCES);
      ret = pm_runtime_get_sync(&g_test_pm_runtine_dev.rd);
      assert_int_equal(ret, -EACCES);
      ret = pm_runtime_put(&g_test_pm_runtine_dev.rd);
      assert_int_equal(ret, -EINVAL);
      ret = pm_runtime_put_autosuspend(&g_test_pm_runtine_dev.rd);
      assert_int_equal(ret, -EINVAL);
      ret = pm_runtime_put_sync(&g_test_pm_runtine_dev.rd);
      assert_int_equal(ret, -EINVAL);
      ret = pm_runtime_put_sync_suspend(&g_test_pm_runtine_dev.rd);
      assert_int_equal(ret, -EINVAL);
      ret = pm_runtime_put_sync_autosuspend(&g_test_pm_runtine_dev.rd);
      assert_int_equal(ret, -EINVAL);
      pm_runtime_enable(&g_test_pm_runtine_dev.rd);
      ret = pm_runtime_get(&g_test_pm_runtine_dev.rd);
      assert_int_equal(ret, 0);
      pm_runtime_barrier(&g_test_pm_runtine_dev.rd);
      assert_int_equal(g_test_pm_runtine_dev.state,
                       TEST_PM_RUTIME_FAKE_RESUME);
      ret = pm_runtime_put_sync(&g_test_pm_runtine_dev.rd);
      assert_int_equal(ret, 0);
      assert_int_equal(g_test_pm_runtine_dev.state,
                       TEST_PM_RUTIME_FAKE_SUSPEND);
      ret = pm_runtime_get_sync(&g_test_pm_runtine_dev.rd);
      assert_int_equal(ret, 0);
      assert_int_equal(g_test_pm_runtine_dev.state,
                       TEST_PM_RUTIME_FAKE_RESUME);
      pm_runtime_set_autosuspend_delay(&g_test_pm_runtine_dev.rd, 1000);
      pm_runtime_use_autosuspend(&g_test_pm_runtine_dev.rd, true);
      ret = pm_runtime_put_sync(&g_test_pm_runtine_dev.rd);
      assert_int_equal(ret, 0);
      assert_int_equal(g_test_pm_runtine_dev.state,
                       TEST_PM_RUTIME_FAKE_IDLE);
      sleep(3);
      assert_int_equal(g_test_pm_runtine_dev.state,
                       TEST_PM_RUTIME_FAKE_SUSPEND);
      ret = pm_runtime_get(&g_test_pm_runtine_dev.rd);
      assert_int_equal(ret, 0);
      sleep(3);
      assert_int_equal(g_test_pm_runtine_dev.state,
                       TEST_PM_RUTIME_FAKE_RESUME);
      ret = pm_runtime_put_sync_suspend(&g_test_pm_runtine_dev.rd);
      assert_int_equal(g_test_pm_runtine_dev.state,
                       TEST_PM_RUTIME_FAKE_SUSPEND);
      ret = pm_runtime_put(&g_test_pm_runtine_dev.rd);
      assert_int_equal(ret, -EINVAL);
      pm_runtime_use_autosuspend(&g_test_pm_runtine_dev.rd, false);
      ret = pm_runtime_get(&g_test_pm_runtine_dev.rd);
      assert_int_equal(ret, 0);
      sleep(2);
      assert_int_equal(g_test_pm_runtine_dev.state,
                       TEST_PM_RUTIME_FAKE_RESUME);
      ret = pm_runtime_put_sync(&g_test_pm_runtine_dev.rd);
      assert_int_equal(ret, 0);
      assert_int_equal(g_test_pm_runtine_dev.state,
                       TEST_PM_RUTIME_FAKE_SUSPEND);
      pm_runtime_disable(&g_test_pm_runtine_dev.rd);
    }

  return;
}

static int setup(FAR void **state)
{
  return test_pm_runtime_fake_driver_init();
}

static int teardown(FAR void **state)
{
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  const struct CMUnitTest tests[] =
    {
      cmocka_unit_test_prestate_setup_teardown(test_pm_runtime, setup,
                                               teardown, NULL),
    };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
