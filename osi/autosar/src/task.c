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

int TaskHook(int argc, FAR char *argv[])
{
  struct atask_s *task = (struct atask_s *)this_task();

  if (task->auto_start && task->nactivations == 0)
    ActivateTask(task->taskid);

  while (1)
    {
      nxsem_wait_uninterruptible(&task->act_sem);
      task->entry();
    }

  return 0;
}

void InitTask(void *tasks, int ntask)
{
  struct atask_s **tlist = (struct atask_s **)tasks;
  struct atask_s *task;
  int ret;
  int i;

  for (i = 0; i < ntask; i++)
    {
      task = tlist[i];

      task->taskid = i;

      ret = nxtask_init(&task->tcb, task->name, task->priority,
          (void *)task->stack, task->stack_size,
          TaskHook, NULL, NULL, NULL);
      if (ret < OK)
        return;

      nxtask_activate(&task->tcb.cmn);
    }
}

extern StatusType ActivateTask(TaskType TaskID)
{
  struct atask_s *task = atask[TaskID];
  nxsem_post(&task->act_sem);
  return E_OK;
}

extern StatusType ChainTask(TaskType TaskID)
{
  struct atask_s *task = atask[TaskID];
  nxsem_post(&task->act_sem);
  return E_OK;
}

extern StatusType TerminateTask(void)
{
  struct atask_s *task = (struct atask_s *)this_task();
  nxsem_reset(&task->act_sem, 0);
  return E_OK;
}

extern StatusType Schedule(void)
{
  sched_yield();
  return E_OK;
}

extern StatusType GetTaskID(TaskRefType TaskID)
{
  struct atask_s *task = (struct atask_s *)this_task();
  *TaskID = task->taskid;
  return E_OK;
}

extern StatusType GetTaskState(TaskType TaskID, TaskStateRefType State)
{
  struct atask_s *task = atask[TaskID];

  switch (task->tcb.cmn.task_state)
  {
    case TSTATE_TASK_PENDING:
    case TSTATE_TASK_READYTORUN:
      *State = PRE_READY;
      break;
    case TSTATE_TASK_RUNNING:
      *State = RUNNING;
      break;
    case TSTATE_WAIT_SEM:
    case TSTATE_WAIT_SIG:
#if !defined(CONFIG_DISABLE_MQUEUE) || !defined(CONFIG_DISABLE_MQUEUE_SYSV)
    case TSTATE_WAIT_MQNOTEMPTY:
    case TSTATE_WAIT_MQNOTFULL:
#endif
#ifdef CONFIG_LEGACY_PAGING
    case TSTATE_WAIT_PAGEFILL:
#endif
#ifdef CONFIG_SIG_SIGSTOP_ACTION
    case TSTATE_TASK_STOPPED:
#endif
      *State = WAITING;
      break;
    case TSTATE_TASK_INVALID:
    default:
      *State = SUSPENDED;
      break;
  }

  return E_OK;
}
