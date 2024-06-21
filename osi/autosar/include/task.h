/**
 * @file
 *
 * @brief       Task management
 *
 * @date        2019-09-02
 * @author      Pascal Romahn
 * @copyright   This program is free software: you can redistribute it and/or modify
 *              it under the terms of the GNU General Public License as published by
 *              the Free Software Foundation, either version 3 of the License, or
 *              (at your option) any later version.
 */


#ifndef TASK_H_
#define TASK_H_

#include <nuttx/sched.h>
#include <sched/sched.h>
#include <nuttx/nuttx.h>

#include "types.h"

#define TASK_COUNT 10
extern struct atask_s* atask[TASK_COUNT];

typedef CODE void (*tentry_t)(void);

#define OS_CONFIG_TASK_DEF(NAME, STACKSIZE, PRIORITY, AUTOSTART, \
                           EXTENDED, PREEMPTIVE, NUM_ACTIVATIONS, \
                           TASKENTRY) \
static uintptr_t TASK##NAME##_STACK[STACKSIZE / sizeof(uintptr_t)]; \
static struct atask_s g_##NAME = \
{ \
    .name         = #NAME, \
    .stack        = TASK##NAME##_STACK, \
    .stack_size   = STACKSIZE, \
    .priority     = PRIORITY, \
    .auto_start   = AUTOSTART, \
    .extended     = EXTENDED, \
    .preemptive   = PREEMPTIVE, \
    .nactivations = NUM_ACTIVATIONS, \
    .act_sem      = SEM_INITIALIZER(NUM_ACTIVATIONS), \
    .entry        = TASKENTRY, \
};

/**
 * @brief   Data structure for task
 */
struct atask_s
{
    struct task_tcb_s tcb;
    const char     *name;
    const void     *stack;          /**< Pointer to stack base */
    const uint16_t stack_size;      /**< Size of stack in bytes */
    const uint8_t  priority;        /**< Static priority of the task */
    const uint8_t  auto_start;      /**< Whether or not to start the task during startup */
    const bool     extended;        /**< Type of task */
    const bool     preemptive;      /**< Scheduling-type of task */
    uint8_t        nactivations;    /**< Current number of activations */
    tentry_t       entry;           /**< Pointer to task function */
    sem_t          act_sem;
    irqstate_t     irqflags;
    int16_t        irqcount;
    TaskType       taskid;
};

void InitTask(void *tasks, int ntask);

/**
 * @brief   Activate a task
 *
 * The task is transferred from the suspended state into the ready state.
 *
 * @note    ActivateTask will not immediately change the state of the task in case of multiple activation requests.
 *          If the task is not suspended, the activation will only be recorded and performed later.
 *
 * @param   TaskID                  ID of the task to be activated
 *
 * @return  E_OK                    No error \n
 *          E_OS_LIMIT              Too many activations of the task \n
 *          E_OS_ID                 TaskID is invalid
 */
StatusType ActivateTask(TaskType TaskID);

/**
 * @brief   Chain task
 *
 * The current is transferred from the running state into the suspended state. The specified task will be transferred
 * into the ready state. The specified task may be identical to the current task.
 *
 * @param   TaskID                  ID of the task to be chained
 *
 * @return  E_OK                    No error \n
 *          E_OS_LIMIT              Too many activations of the task \n
 *          E_OS_ID                 TaskID is invalid \n
 *          E_OS_RESOURCE           Task still occupies resources \n
 *          E_OS_CALLLEVEL          Call at interrupt level
 */
StatusType ChainTask(TaskType TaskID);

/**
 * @brief   Terminate active task
 *
 * The calling task is transferred from the running state into the suspended state.
 *
 * @return  E_OS_RESOURCE           Task still occupies resources \n
 *          E_OS_CALLLEVEL          Call at interrupt level
 */
StatusType TerminateTask(void);

/**
 * @brief   Reschedule current task
 *
 * If a higher priority task is ready it will be executed. Otherwise the calling task is continued. This allows a
 * processor assignment to other tasks with lower or equal priority than the ceiling priority of the current task.
 *
 * This service has no influence on preemptive tasks with no internal resource.
 *
 * @return  E_OK                    No error \n
 *          E_OS_RESOURCE           Task still occupies resources \n
 *          E_OS_CALLLEVEL          Call at interrupt level
 *
 */
StatusType Schedule(void);

/**
 * @brief   Return the ID of the task currently running
 *
 * @param   TaskID                  Reference of the task currently running. INVALID_TASK if no task is in
 *                                  running state.
 *
 * @return  E_OK                    No error \n
 *          E_OS_PARAM_POINTER      Pointer parameter is invalid
 */
StatusType GetTaskID(TaskRefType TaskID);

/**
 * @brief   Return the state of a task
 *
 * @param   TaskID                  ID of the task to return the state for
 * @param   State                   Reference of the specified tasks state
 *
 * @return  E_OK                    No error \n
 *          E_OS_ID                 TaskID is invalid
 */
StatusType GetTaskState(TaskType TaskID, TaskStateRefType State);

#endif /* TASK_H_ */
