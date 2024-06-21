/**
 * @file
 *
 * @brief       Type definitions
 *
 * @date        2019-09-10
 * @author      Pascal Romahn
 * @copyright   This program is free software: you can redistribute it and/or modify
 *              it under the terms of the GNU General Public License as published by
 *              the Free Software Foundation, either version 3 of the License, or
 *              (at your option) any later version.
 */

#ifndef TYPES_H_
#define TYPES_H_

#include <stdint.h>
#include <sys/types.h>

/**
 * @brief   Type for status
 */
typedef enum StatusType_e {
    E_OK = 0,
    E_OS_ACCESS,
    E_OS_CALLEVEL,
    E_OS_ID,
    E_OS_LIMIT,
    E_OS_NOFUNC,
    E_OS_RESOURCE,
    E_OS_STATE,
    E_OS_VALUE,
    E_OS_SERVICEID,
    E_OS_ILLEGAL_ADDRESS,
    E_OS_MISSINGEND,
    E_OS_DISABLEDINT,
    E_OS_STACKFAULT,
    E_OS_PARAM_POINTER,
    E_OS_PROTECTION_MEMORY,
    E_OS_PROTECTION_TIME,
    E_OS_PROTECTION_ARRIVAL,
    E_OS_PROTECTION_LOCKED,
    E_OS_PROTECTION_EXCEPTION,
    E_OS_SPINLOCK,
    E_OS_INTERFERENCE_DEADLOCK,
    E_OS_NESTING_DEADLOCK,
    E_OS_CORE,
} StatusType;

/*! This data type represents a task identifier.
 */
typedef uint16_t TaskType;

/*! This reference type represents a task identifier.
 */
typedef TaskType *TaskRefType;

/**
 * @brief   Task state
 */
typedef enum OsTaskState_e {
    SUSPENDED = 0,  /**< The task is suspended and will not be scheduled */
    PRE_READY,      /**< The task is ready but its stack is uninitialized */
    READY,          /**< The task is ready to be scheduled */
    RUNNING,        /**< The task is currently running */
    WAITING         /**< The task is waiting for an event */
} TaskStateType;

/**
 * @brief   Type for task state reference
 *
 * Reference a #TaskStateType.
 */
typedef TaskStateType* TaskStateRefType;

/**
 * @brief   Type for application mode
 */
typedef enum applicationMode_e {
    OSDEFAULTAPPMODE
} AppModeType;

#endif /* TYPES_H_ */
