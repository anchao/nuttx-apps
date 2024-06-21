/****************************************************************************
 * sched/wqueue/wqueue.h
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this alarm for additional information regarding copyright ownership.  The
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
 ****************************************************************************/

#ifndef __SCHED_WQUEUE_WQUEUE_H
#define __SCHED_WQUEUE_WQUEUE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <semaphore.h>
#include <sys/types.h>
#include <stdbool.h>

#include <nuttx/clock.h>
#include <nuttx/queue.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Type Definitions
 ****************************************************************************/

/* Defines the alarm callback */

typedef CODE void (*alarmer_t)(FAR void *arg);

/* Defines one entry in the alarm queue.  The user only needs this structure
 * in order to declare instances of the alarm structure.  Handling of all
 * fields is performed by the alarm APIs
 */

struct alarm_s
{
  union
  {
    struct
    {
      struct dq_entry_s dq; /* Implements a double linked list */
      clock_t qtime;        /* Time alarm queued */
    } s;
    struct wdog_s timer;    /* Delay expiry timer */
  } u;
  alarmer_t  alarmer;       /* Work callback */
  FAR void *arg;            /* Callback argument */
};

/* This represents one alarmer */

struct alarmer_s
{
  pid_t               pid;       /* The task ID of the alarmer thread */
  FAR struct alarm_s *alarm;     /* The alarm structure */
  sem_t               wait;      /* Sync waiting for alarmer done */
};

/* This structure defines the state of one kernel-mode alarm queue */

struct alarm_queue_s
{
  struct dq_queue_s q;         /* The queue of pending alarm */
  sem_t             sem;       /* The counting semaphore of the wqueue */
  struct alarmer_s alarmer;    /* Describes a alarmer thread */
};

#endif /* __SCHED_WQUEUE_WQUEUE_H */
