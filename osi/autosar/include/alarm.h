/**
 * @file
 *
 * @brief       Alarm management
 *
 * @date        2020-06-15
 * @author      Pascal Romahn
 * @copyright   This program is free software: you can redistribute it and/or modify
 *              it under the terms of the GNU General Public License as published by
 *              the Free Software Foundation, either version 3 of the License, or
 *              (at your option) any later version.
 */


#ifndef ALARM_H_
#define ALARM_H_

#include "types.h"
#include <nuttx/wqueue.h>

#define OS_CONFIG_ALARM_DEF(NAME, STACKSIZE, PRIORITY, AUTOSTART, \
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

OS_CONFIG_ALARM_DEF(Alarm1, true,  0b01,    ALARM_ACTION_EVENT,      T7,    1,    1)

#define OS_CONFIG_ALARM_DEF(NAME, AUTOSTART, EVENT, TYPE, ACTION, EXPIRATION, CYCLE) \
volatile struct alarm_s g_##NAME = { \
    .event          = EVENT, \
    .type           = TYPE, \
    .action.action  = (void *)ACTION, \
    .auto_start     = AUTOSTART, \
    .expiration     = EXPIRATION, \
    .cycle          = CYCLE \
};

static struct aalarm_s g_alarm =
{
  .event    = EVENT, \
  .type     = TYPE,
  .action   = 
};

struct aalarm_s
{
  struct alarm_s                alarm;
  TickType                      cycle;
  TickType                      increment;
  const enum alarmActionType_e  type;    /**< Type of the alarm */
  const EventMaskType           event;             /**< Event to set if type is #ALARM_ACTION_EVENT */
  union {
    const void* action;                  /**< Untyped value to be used during configuration */
    const TaskType task;                 /**< Task to activate if type is #ALARM_ACTION_TASK or to set event for if type is #ALARM_ACTION_EVENT */
    const pAlarmCallback callback;       /**< Callback to execute if type is #ALARM_ACTION_CALLBACK */
  } action;
};

/**
 * @brief   Data type of counter values
 */
typedef clock_t TickType;

/**
 * @brief   Reference to counter values
 *
 * References #TickType
 */
typedef TickType* TickRefType;

struct AlarmBase {
    TickType maxallowedvalue;
    TickType mincycle;
    TickType tickperbase;
};

/*! The data type of alarm base parameters */
typedef struct AlarmBase AlarmBaseType;

/*! The data type of alarm base parameters reference*/
typedef struct AlarmBase* AlarmBaseRefType;

/*! This data type represents a alarm identifier.
 */
typedef uint16_t AlarmType;


/**
 * @brief   Get alarm base
 *
 * This service will get a copy of the counter configuration used as the alarm base.
 *
 * @param   alarmID                 Alarm which base should be read
 * @param   info                    Pointer to copy base info into
 *
 * @return  E_OK                    No error \n
 *          E_OS_ID                 alarmID is invalid \n
 *          E_OS_PARAM_POINTER      Pointer parameter is invalid
 */
extern StatusType GetAlarmBase(AlarmType alarmID, AlarmBaseRefType info);

/**
 * @brief   Get alarm
 *
 * Get relative value in ticks before alarm expires.
 *
 * @note    The value of tick is not defined if the alarm is not in use.
 *
 * @param   alarmID                 Alarm which remaining ticks should be read
 * @param   tick                    Pointer to write remaining ticks into
 *
 * @return  E_OK                    No error \n
 *          E_OS_ID                 alarmID is invalid \n
 *          E_OS_NOFUNC             Alarm is not used \n
 *          E_OS_PARAM_POINT        Pointer parameter is invalid
 */
extern StatusType GetAlarm(AlarmType alarmID, TickRefType tick);

/**
 * @brief   Set relative alarm
 *
 * Set a relative alarm. The alarm will expire in increment ticks. If cycle does not equal zero
 * the alarm will be restarted to expire in cycle ticks after it expires.
 *
 * @param   alarmID                 Alarm to set
 * @param   increment               Relative value in ticks (must be between one and maxallowedvalue of the base)
 * @param   cycle                   Cycle value for cyclic alarm (must be between mincycle and maxallowedvalue of
 *                                  the base)
 *
 * @return  E_OK                    No error \n
 *          E_OS_ID                 alarmID is invalid \n
 *          E_OS_STATE              Alarm is already in use \n
 *          E_OS_VALUE              Values of increment or cycle are outside of the admissible range
 */
extern StatusType SetRelAlarm(AlarmType alarmID, TickType increment, TickType cycle);

/**
 * @brief   Set absolute alarm
 *
 * Set an absolute alarm. The alarm will expire once the base counter reaches the start value. If cycle
 * does not equal zero the alarm will be restarted to expire in cycle ticks after it expires.
 *
 * @param   alarmID                 Alarm to set
 * @param   start                   Absolute value in ticks (must be between zero and maxallowedvalue of the base)
 * @param   cycle                   Cycle value for cyclic alarm (must be between mincycle and maxallowedvalue of
 *                                  the base)
 *
 * @return  E_OK                    No error \n
 *          E_OS_ID                 alarmID is invalid \n
 *          E_OS_STATE              Alarm is already in use \n
 *          E_OS_VALUE              Values of increment or cycle are outside of the admissible range
 */
extern StatusType SetAbsAlarm(AlarmType alarmID, TickType start, TickType cycle);

/**
 * @brief   Cancel alarm
 *
 * @param   alarmID                 Alarm to cancel
 *
 * @return  E_OK                    No error \n
 *          E_OS_ID                 alarmID is invalid \n
 *          E_OS_NOFUNC             Alarm is not used
 */
extern StatusType CancelAlarm(AlarmType alarmID);

void LoopAlarm(void);

#endif /* ALARM_H_ */
