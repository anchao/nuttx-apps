/**
 * @file
 *
 * @brief       Implementation of alarm management
 *
 * @date        2020-06-15
 * @author      Pascal Romahn
 * @copyright   This program is free software: you can redistribute it and/or modify
 *              it under the terms of the GNU General Public License as published by
 *              the Free Software Foundation, either version 3 of the License, or
 *              (at your option) any later version.
 */

#include "os.h"
#include "alarm.h"
#include "task.h"

static const AlarmBaseType g_systemTimerBase =
{
    /* .MaxAllowedValue      = */ (1073741823uL),
    /* .MinCycle             = */ (1uL),
    /* .TicksPerBase         = */ (1uL),
};

/**
 * @brief   Alarm action type
 */
enum alarmActionType_e {
    ALARM_ACTION_TASK = 0,  /**< Alarm activates task on expiration */
    ALARM_ACTION_EVENT,     /**< Alarm sets event on expiration */
    ALARM_ACTION_CALLBACK,  /**< Alarm calls callback on expiration */
    ALARM_ACTION_COUNTER    /**< Alarm increments counter on expiration */
};

static struct aalarm_s aalarm[ALARM_COUNT];

/************************************************************************/
/* STATIC FUNCTIONS                                                     */
/************************************************************************/
/**
 * @brief   Handle alarm expiration
 *
 * Handle an alarm expiration and perform the configured action. The alarm will be stopped
 * and restarted if configured as a cyclic alarm.
 *
 * @param   alarmID     Alarm to handle
 */

/************************************************************************/
/* EXTERN FUNCTIONS                                                     */
/************************************************************************/
StatusType GetAlarmBase(AlarmType alarmID, AlarmBaseRefType info)
{
  memcpy(info, &g_systemTimerBase, sizeof(AlarmBaseType));
  return E_OK;
}

StatusType GetAlarm(AlarmType alarmID, TickRefType tick)
{
  if (alarm_available(&g_alarm_work[alarmID]))
    {
      return E_OS_NOFUNC;
    }

  *tick = alarm_timeleft(&g_alarm_work[alarmID]);

  return E_OK;
}

StatusType SetRelAlarm(AlarmType alarmID, TickType increment, TickType cycle)
{
  if (!alarm_available(&g_alarm_work[alarmID]))
    {
      return E_OS_STATE;
    }

  alarm_queue(HPWORK, &g_alarm_work[alarmID], addrenv_destroy, addrenv, 0);
  return E_OK;
}

StatusType SetAbsAlarm(AlarmType alarmID, TickType start, TickType cycle)
{
  return E_OK;
}

StatusType CancelAlarm(AlarmType alarmID)
{
  return E_OK;
}
