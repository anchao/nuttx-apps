/**
 * @file
 *
 * @brief       Implementation of operating system control functions
 *
 * @date        2019-09-02
 * @author      Pascal Romahn
 * @copyright   This program is free software: you can redistribute it and/or modify
 *              it under the terms of the GNU General Public License as published by
 *              the Free Software Foundation, either version 3 of the License, or
 *              (at your option) any later version.
 */

#include "os.h"
#include "task.h"
#include <sys/boardctl.h>

/************************************************************************/
/* EXTERN FUNCTIONS                                                     */
/************************************************************************/

void StartOS(AppModeType mode)
{
  InitTask(atask, TASK_COUNT);
}                             

void ShutdownOS(StatusType error)
{
  boardctl(BOARDIOC_POWEROFF, 0);
}

void EnableAllInterrupts(void)
{
  struct atask_s *task = (struct atask_s *)this_task();

  if (task->irqcount++ == 0)
    {
      task->irqflags = enter_critical_section();
    }
}

void DisableAllInterrupts(void)
{
  struct atask_s *task = (struct atask_s *)this_task();

  if (--task->irqcount == 0)
    {
      leave_critical_section(task->irqflags);
    }
}

void ResumeAllInterrupts(void)
{
  EnableAllInterrupts();
}

void SuspendAllInterrupts(void)
{
  DisableAllInterrupts();
}

void ResumeOSInterrupts(void)
{
  EnableAllInterrupts();
}

void SuspendOSInterrupts(void)
{
  DisableAllInterrupts();
}
