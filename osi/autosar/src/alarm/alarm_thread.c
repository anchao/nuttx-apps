/****************************************************************************
 * sched/wqueue/kalarm_thread.c
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
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <unistd.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/kthread.h>
#include <nuttx/semaphore.h>

#include "alarm.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* The state of the kernel mode, high priority alarm queue(s). */

struct alarm_queue_s g_hpalarm =
{
  {NULL, NULL},
  SEM_INITIALIZER(0),
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: alarm_thread
 *
 * Description:
 *   These are the alarmer threads that perform the actions placed on the
 *   high priority alarm queue.
 *
 *   These, along with the lower priority alarmer thread(s) are the kernel
 *   mode alarm queues (also built in the flat build).
 *
 *   All kernel mode alarmer threads are started by the OS during normal
 *   bring up.  This entry point is referenced by OS internally and should
 *   not be accessed by application logic.
 *
 * Input Parameters:
 *   argc, argv
 *
 * Returned Value:
 *   Does not return
 *
 ****************************************************************************/

void LoopAlarm(void)
{
  FAR struct alarm_queue_s *wqueue;
  FAR struct alarmer_s *palarmer;
  FAR struct alarm_s *alarm;
  alarmer_t alarmer;
  irqstate_t flags;
  FAR void *arg;
  int semcount;

  /* Get the handle from argv */

  wqueue  = &g_hpalarm; 
  palarmer = &wqueue->alarmer;

  flags = enter_critical_section();

  nxsem_init(&g_hpalarm.alarmer.wait, 0, 0);
  g_hpalarm.alarmer.pid = getpid();

  /* Loop forever */

  for (; ; )
    {
      /* And check each entry in the alarm queue.  Since we have disabled
       * interrupts we know:  (1) we will not be suspended unless we do
       * so ourselves, and (2) there will be no changes to the alarm queue
       */

      /* Remove the ready-to-execute alarm from the list */

      while ((alarm = (FAR struct alarm_s *)dq_remfirst(&wqueue->q)) != NULL)
        {
          if (alarm->alarmer == NULL)
            {
              continue;
            }

          /* Extract the alarm description from the entry (in case the alarm
           * instance will be re-used after it has been de-queued).
           */

          alarmer = alarm->alarmer;

          /* Extract the alarm argument (before re-enabling interrupts) */

          arg = alarm->arg;

          /* Mark the alarm as no longer being queued */

          alarm->alarmer = NULL;

          /* Mark the thread busy */

          palarmer->alarm = alarm;

          /* Do the alarm.  Re-enable interrupts while the alarm is being
           * performed... we don't have any idea how long this will take!
           */

          leave_critical_section(flags);
          alarmer(arg);
          flags = enter_critical_section();

          /* Mark the thread un-busy */

          palarmer->alarm = NULL;

          /* Check if someone is waiting, if so, wakeup it */

          nxsem_get_value(&palarmer->wait, &semcount);
          while (semcount++ < 0)
            {
              nxsem_post(&palarmer->wait);
            }
        }

      /* Then process queued alarm.  alarm_process will not return until: (1)
       * there is no further alarm in the alarm queue, and (2) semaphore is
       * posted.
       */

      nxsem_wait_uninterruptible(&wqueue->sem);
    }

  leave_critical_section(flags);
}

/****************************************************************************
 * Name: alarm_qcancel
 *
 * Description:
 *   Cancel previously queued alarm.  This removes alarm from the alarm queue.
 *   After alarm has been cancelled, it may be requeued by calling
 *   alarm_queue() again.
 *
 * Input Parameters:
 *   wqueue  - The alarm queue to use.  Must be HPWORK or LPWORK
 *   nthread - The number of threads in the alarm queue
 *             > 0 unsynchronous cancel
 *             < 0 synchronous cancel
 *   alarm    - The previously queued alarm structure to cancel
 *
 * Returned Value:
 *   Zero (OK) on success, a negated errno on failure.  This error may be
 *   reported:
 *
 *   -ENOENT - There is no such alarm queued.
 *   -EINVAL - An invalid alarm queue was specified
 *
 ****************************************************************************/

static int alarm_qcancel(FAR struct alarm_queue_s *wqueue, bool sync,
                        FAR struct alarm_s *alarm)
{
  irqstate_t flags;
  int ret = -ENOENT;

  DEBUGASSERT(alarm != NULL);

  /* Cancelling the alarm is simply a matter of removing the alarm structure
   * from the alarm queue.  This must be done with interrupts disabled because
   * new alarm is typically added to the alarm queue from interrupt handlers.
   */

  flags = enter_critical_section();
  if (alarm->alarmer != NULL)
    {
      /* Remove the entry from the alarm queue and make sure that it is
       * marked as available (i.e., the alarmer field is nullified).
       */

      if (WDOG_ISACTIVE(&alarm->u.timer))
        {
          wd_cancel(&alarm->u.timer);
        }
      else
        {
          dq_rem((FAR dq_entry_t *)alarm, &wqueue->q);
        }

      alarm->alarmer = NULL;
      ret = OK;
    }
  else if (sync)
    {
      if (wqueue->alarmer.alarm == alarm &&
          wqueue->alarmer.pid != nxsched_gettid())
        {
          nxsem_wait_uninterruptible(&wqueue->alarmer.wait);
          ret = OK;
        }
    }

  leave_critical_section(flags);
  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: alarm_cancel
 *
 * Description:
 *   Cancel previously queued user-mode alarm.  This removes alarm from the
 *   user mode alarm queue.  After alarm has been cancelled, it may be
 *   requeued by calling alarm_queue() again.
 *
 * Input Parameters:
 *   qid    - The alarm queue ID (must be HPWORK or LPWORK)
 *   alarm   - The previously queued alarm structure to cancel
 *
 * Returned Value:
 *   Zero (OK) on success, a negated errno on failure.  This error may be
 *   reported:
 *
 *   -ENOENT - There is no such alarm queued.
 *   -EINVAL - An invalid alarm queue was specified
 *
 ****************************************************************************/

int alarm_cancel(int qid, FAR struct alarm_s *alarm)
{
  /* Cancel high priority alarm */

  return alarm_qcancel((FAR struct alarm_queue_s *)&g_hpalarm, false, alarm);
}

/****************************************************************************
 * Name: alarm_cancel_sync
 *
 * Description:
 *   Blocked cancel previously queued user-mode alarm.  This removes alarm
 *   from the user mode alarm queue.  After alarm has been cancelled, it may
 *   be requeued by calling alarm_queue() again.
 *
 * Input Parameters:
 *   qid    - The alarm queue ID (must be HPWORK or LPWORK)
 *   alarm   - The previously queued alarm structure to cancel
 *
 * Returned Value:
 *   Zero (OK) on success, a negated errno on failure.  This error may be
 *   reported:
 *
 *   -ENOENT - There is no such alarm queued.
 *   -EINVAL - An invalid alarm queue was specified
 *
 ****************************************************************************/

int alarm_cancel_sync(int qid, FAR struct alarm_s *alarm)
{
  /* Cancel high priority alarm */

  return alarm_qcancel((FAR struct alarm_queue_s *)&g_hpalarm, true, alarm);
}

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define queue_alarm(wqueue, alarm) \
  do \
    { \
      int sem_count; \
      dq_addlast((FAR dq_entry_t *)(alarm), &(wqueue).q); \
      nxsem_get_value(&(wqueue).sem, &sem_count); \
      if (sem_count < 0) /* There are threads waiting for sem. */ \
        { \
          nxsem_post(&(wqueue).sem); \
        } \
    } \
  while (0)

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: hp_alarm_timer_expiry
 ****************************************************************************/

static void hp_alarm_timer_expiry(wdparm_t arg)
{
  irqstate_t flags = enter_critical_section();
  queue_alarm(g_hpalarm, arg);
  leave_critical_section(flags);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int alarm_queue(int qid, FAR struct alarm_s *alarm, alarmer_t alarmer,
               FAR void *arg, clock_t delay)
{
  irqstate_t flags;
  int ret = OK;

  /* Interrupts are disabled so that this logic can be called from with
   * task logic or from interrupt handling logic.
   */

  flags = enter_critical_section();

  /* Remove the entry from the timer and alarm queue. */

  if (alarm->alarmer != NULL)
    {
      alarm_cancel(qid, alarm);
    }

  /* Initialize the alarm structure. */

  alarm->alarmer = alarmer;           /* Work callback. non-NULL means queued */
  alarm->arg = arg;                 /* Callback argument */

  /* Queue high priority alarm */

  if (!delay)
    {
      queue_alarm(g_hpalarm, alarm);
    }
  else
    {
      wd_start(&alarm->u.timer, delay, hp_alarm_timer_expiry,
               (wdparm_t)alarm);
    }

  leave_critical_section(flags);

  return ret;
}
