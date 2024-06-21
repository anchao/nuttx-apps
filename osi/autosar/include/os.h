/**
 * @file
 *
 * @brief       Operating system control
 *
 * @date        2019-09-02
 * @author      Pascal Romahn
 * @copyright   This program is free software: you can redistribute it and/or modify
 *              it under the terms of the GNU General Public License as published by
 *              the Free Software Foundation, either version 3 of the License, or
 *              (at your option) any later version.
 */


#ifndef OS_H_
#define OS_H_

#include "types.h"

#include <stdbool.h>

/**
 * @brief   Start operating system
 *
 * This service starts the operating system in the specified application mode. The default
 * application mode is #OSDEFAULTAPPMODE.
 *
 * @note    Calls to this functions are only allowed outside of the operating system. This call does not
 *          return.
 *
 * @note    This implementation only supports the default mode.
 *
 * @param   mode        Application mode
 */
extern void StartOS(AppModeType mode);

/**
 * @brief   Shutdown operating system
 *
 * This service shuts down the operating system and does not return. If a ShutdownHook is configured
 * it will be called before the system is terminated.
 *
 * @param   error       Error occurred
 */
extern void ShutdownOS(StatusType error);

/**
 * @brief   Enable all interrupts
 *
 * This service enables all interrupts. It is the counterpart to DisableAllInterrupts().
 *
 * Nesting is not supported by this call.
 */
extern void EnableAllInterrupts(void);

/**
 * @brief   Disable all interrupts
 *
 * This service disables all interrupts. It is the counterpart to EnableAllInterrupts().
 *
 * Nesting is not supported by this call.
 */
extern void DisableAllInterrupts(void);

/**
 * @brief   Resume all interrupts
 *
 * This service resumes all interrupts disabled by the previous call to SuspendAllInterrupts().
 *
 * Nesting is supported by this call.
 */
extern void ResumeAllInterrupts(void);

/**
 * @brief   Suspend all interrupts
 *
 * This service suspends all interrupts. It is the counterpart to ResumeAllInterrupts().
 *
 * Nesting is supported by this call
 */
extern void SuspendAllInterrupts(void);

/**
 * @brief   Resume OS interrupts
 *
 * This service resumes OS interrupts disabled by the privious call to SuspendOSInterrupts().
 *
 * Nesting is supported by this call.
 *
 * @note    Because of hardware limitations there is no difference between this function and
 *          ResumeAllInterrupts(). However the correct counterpart must be used.
 *
 * @warning Nesting is only supported up to 8 levels!
 */
extern void ResumeOSInterrupts(void);

/**
 * @brief   Suspend OS interrupts
 *
 * This service suspends all OS interrupts. It is the counterpart to ResumeOSInterrupts().
 *
 * Nesting is supported by this call.
 *
 * @note    Because of hardware limitation there is no difference between this function and
 *          SuspendAllInterrupts(). However the correct counterpart must be used.
 *
 * @warning Nesting is only supported up to 8 levels!
 */
extern void SuspendOSInterrupts(void);

#endif /* OS_H_ */
