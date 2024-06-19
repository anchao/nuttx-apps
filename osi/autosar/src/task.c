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

#include "task.h"
#include <stdio.h>

typedef CODE void (*tentry_t)(void);

/**
 * @brief   Data structure for task
 */
struct atask_s
{
    struct task_tcb_s tcb;
    const char     *name;
    const void     *stack;     /**< Pointer to stack base */
    const uint16_t stack_size; /**< Size of stack in bytes */
    const uint8_t  priority;   /**< Static priority of the task */
    const uint8_t  auto_start; /**< Whether or not to start the task during startup */
    const bool     extended;   /**< Type of task */
    const bool     preemptive; /**< Scheduling-type of task */
    uint8_t        nactive;    /**< Current number of activations */
    tentry_t       entry;      /**< Pointer to task function */
    TaskType       taskid;
};

static uintptr_t g_task_stack[204800 / sizeof(uintptr_t)];

void test_tentry(void)
{
    printf("1111 \n");
    sleep(100);
}

int default_main_hook(int argc, FAR char *argv[])
{
    struct atask_s *task = (struct atask_s *)this_task();

    task->entry();

    return 0;
}

static struct atask_s g_task =
{
    .name = "task",
    .stack = (const void *)g_task_stack,
    .stack_size = (uint16_t)sizeof(g_task_stack),
    .priority   = SCHED_PRIORITY_DEFAULT,
    .auto_start = true,
    .extended   = false,
    .preemptive = false,
    .nactive    = 1,
    .entry      = test_tentry,
};

extern StatusType ActivateTask(TaskType TaskID)
{
    int ret;

    g_task.taskid = TaskID;

    ret = nxtask_init(&g_task.tcb, "test", g_task.priority,
                      (void *)g_task.stack, g_task.stack_size,
                      default_main_hook, NULL, NULL, NULL);
    if (ret < OK)
        return ret;

    nxtask_activate(&g_task.tcb.cmn);

    return E_OK;
}

extern StatusType ChainTask(TaskType TaskID)
{
    return E_OK;
}

extern StatusType TerminateTask(void)
{
    exit(0);
    return E_OK;
}

extern StatusType Schedule(void)
{
    return E_OK;
}

extern StatusType GetTaskID(TaskRefType TaskID)
{
    return E_OK;
}

extern StatusType GetTaskState(TaskType TaskID, TaskStateRefType State)
{
    return E_OK;
}

int main(int argc, char *argv[])
{
    ActivateTask(1);
    return 0;
}
