/**
 * @file
 *
 * @brief       Implementation of task management
 *
 * @date        2019-09-02
 * @author      Pascal Romahn
 * @copyright   This program is free software: you can redistribute it and/or modify
 *              it under the terms of the GNU General Public License as published by
 *              the Free Software Foundation, either version 3 of the License, or
 *              (at your option) any later version.
 */

#include <stdio.h>
#include "task.h"
#include "os.h"
#include "alarm.h"

#define TASK_COUNT 10

void test_tentry(void)
{
  struct atask_s *task = (struct atask_s *)this_task();
  EnableAllInterrupts();
  EnableAllInterrupts();
  EnableAllInterrupts();
  EnableAllInterrupts();
  printf("%s: task entry \n", task->name);
  DisableAllInterrupts();
  DisableAllInterrupts();
  DisableAllInterrupts();
  DisableAllInterrupts();
}

OS_CONFIG_TASK_DEF(alarm, 20480, SCHED_PRIORITY_DEFAULT, true, false, false, 1, LoopAlarm);
OS_CONFIG_TASK_DEF(task0, 20480, SCHED_PRIORITY_DEFAULT, true, false, false, 1, test_tentry);
OS_CONFIG_TASK_DEF(task1, 20480, SCHED_PRIORITY_DEFAULT, true, false, false, 1, test_tentry);
OS_CONFIG_TASK_DEF(task2, 20480, SCHED_PRIORITY_DEFAULT, true, false, false, 1, test_tentry);
OS_CONFIG_TASK_DEF(task3, 20480, SCHED_PRIORITY_DEFAULT, true, false, false, 1, test_tentry);
OS_CONFIG_TASK_DEF(task4, 20480, SCHED_PRIORITY_DEFAULT, true, false, false, 1, test_tentry);
OS_CONFIG_TASK_DEF(task5, 20480, SCHED_PRIORITY_DEFAULT, true, false, false, 1, test_tentry);
OS_CONFIG_TASK_DEF(task6, 20480, SCHED_PRIORITY_DEFAULT, true, false, false, 1, test_tentry);
OS_CONFIG_TASK_DEF(task7, 20480, SCHED_PRIORITY_DEFAULT, true, false, false, 1, test_tentry);
OS_CONFIG_TASK_DEF(task8, 20480, SCHED_PRIORITY_DEFAULT, true, false, false, 1, test_tentry);

struct atask_s* atask[TASK_COUNT] =
{
  &g_alarm,
  &g_task0,
  &g_task1,
  &g_task2,
  &g_task3,
  &g_task4,
  &g_task5,
  &g_task6,
  &g_task7,
  &g_task8,
};

int main(int argc, char *argv[])
{
  StartOS(OSDEFAULTAPPMODE);
  ActivateTask(1);
  ActivateTask(3);
  ActivateTask(5);
  ActivateTask(7);
  ActivateTask(8);
  return 0;
}
