/**
 * @file os_port_embos.c
 * @brief RTOS abstraction layer (Segger embOS)
 *
 * @section License
 *
 * Copyright (C) 2010-2018 Oryx Embedded SARL. All rights reserved.
 *
 * This file is part of CycloneTCP Eval.
 *
 * This software is provided in source form for a short-term evaluation only. The
 * evaluation license expires 90 days after the date you first download the software.
 *
 * If you plan to use this software in a commercial product, you are required to
 * purchase a commercial license from Oryx Embedded SARL.
 *
 * After the 90-day evaluation period, you agree to either purchase a commercial
 * license or delete all copies of this software. If you wish to extend the
 * evaluation period, you must contact sales@oryx-embedded.com.
 *
 * This evaluation software is provided "as is" without warranty of any kind.
 * Technical support is available as an option during the evaluation period.
 *
 * @author Oryx Embedded SARL (www.oryx-embedded.com)
 * @version 1.8.2
 **/

//Switch to the appropriate trace level
#define TRACE_LEVEL TRACE_LEVEL_OFF

//Dependencies
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "os_port.h"
#include "os_port_embos.h"
#include "debug.h"

//Forward declaration of functions
void osIdleTaskHook(void);

//Variables
static OS_TASK *tcbTable[OS_PORT_MAX_TASKS];
static void *stkTable[OS_PORT_MAX_TASKS];


/**
 * @brief Kernel initialization
 **/

void osInitKernel(void)
{
   //Initialize tables
   memset(tcbTable, 0, sizeof(tcbTable));
   memset(stkTable, 0, sizeof(stkTable));

   //Disable interrupts
   OS_IncDI();
   //Kernel initialization
   OS_InitKern();
   //Hardware initialization
   OS_InitHW();
}


/**
 * @brief Start kernel
 **/

void osStartKernel(void)
{
   //Start the scheduler
   OS_Start();
}


/**
 * @brief Create a static task
 * @param[out] task Pointer to the task structure
 * @param[in] name A name identifying the task
 * @param[in] taskCode Pointer to the task entry function
 * @param[in] param A pointer to a variable to be passed to the task
 * @param[in] stack Pointer to the stack
 * @param[in] stackSize The initial size of the stack, in words
 * @param[in] priority The priority at which the task should run
 * @return The function returns TRUE if the task was successfully
 *   created. Otherwise, FALSE is returned
 **/

bool_t osCreateStaticTask(OsTask *task, const char_t *name, OsTaskCode taskCode,
   void *param, void *stack, size_t stackSize, int_t priority)
{
   //Create a new task
   OS_CreateTaskEx(task, name, priority, taskCode,
      stack, stackSize * sizeof(uint_t), 1, param);

   //The task was successfully created
   return TRUE;
}


/**
 * @brief Create a new task
 * @param[in] name A name identifying the task
 * @param[in] taskCode Pointer to the task entry function
 * @param[in] param A pointer to a variable to be passed to the task
 * @param[in] stackSize The initial size of the stack, in words
 * @param[in] priority The priority at which the task should run
 * @return If the function succeeds, the return value is a pointer to the
 *   new task. If the function fails, the return value is NULL
 **/

OsTask *osCreateTask(const char_t *name, OsTaskCode taskCode,
   void *param, size_t stackSize, int_t priority)
{
   uint_t i;
   OS_TASK *task;
   void *stack;

   //Enter critical section
   osSuspendAllTasks();

   //Loop through TCB table
   for(i = 0; i < OS_PORT_MAX_TASKS; i++)
   {
      //Check whether the current entry is free
      if(tcbTable[i] == NULL)
         break;
   }

   //Any entry available in the table?
   if(i < OS_PORT_MAX_TASKS)
   {
      //Allocate a memory block to hold the task's control block
      task = osAllocMem(sizeof(OS_TASK));

      //Successful memory allocation?
      if(task != NULL)
      {
         //Allocate a memory block to hold the task's stack
         stack = osAllocMem(stackSize * sizeof(uint_t));

         //Successful memory allocation?
         if(stack != NULL)
         {
            //Create a new task
            OS_CreateTaskEx(task, name, priority, taskCode,
               stack, stackSize * sizeof(uint_t), 1, param);

            //Save TCB base address
            tcbTable[i] = task;
            //Save stack base address
            stkTable[i] = stack;
         }
         else
         {
            osFreeMem(task);
            //Memory allocation failed
            task = NULL;
         }
      }
   }
   else
   {
      //Memory allocation failed
      task = NULL;
   }

   //Leave critical section
   osResumeAllTasks();

   //Return task pointer
   return task;
}


/**
 * @brief Delete a task
 * @param[in] task Pointer to the task to be deleted
 **/

void osDeleteTask(OsTask *task)
{
   //Delete the specified task
   OS_TerminateTask(task);
}


/**
 * @brief Delay routine
 * @param[in] delay Amount of time for which the calling task should block
 **/

void osDelayTask(systime_t delay)
{
   //Delay the task for the specified duration
   OS_Delay(OS_MS_TO_SYSTICKS(delay));
}


/**
 * @brief Yield control to the next task
 **/

void osSwitchTask(void)
{
   //Not implemented
}


/**
 * @brief Suspend scheduler activity
 **/

void osSuspendAllTasks(void)
{
   //Make sure the operating system is running
   if(OS_IsRunning())
   {
      //Suspend scheduler activity
      OS_SuspendAllTasks();
   }
}


/**
 * @brief Resume scheduler activity
 **/

void osResumeAllTasks(void)
{
   //Make sure the operating system is running
   if(OS_IsRunning())
   {
      //Resume scheduler activity
      OS_ResumeAllSuspendedTasks();
   }
}


/**
 * @brief Create an event object
 * @param[in] event Pointer to the event object
 * @return The function returns TRUE if the event object was successfully
 *   created. Otherwise, FALSE is returned
 **/

bool_t osCreateEvent(OsEvent *event)
{
   //Create an event object
   OS_EVENT_Create(event);

   //The event object was successfully created
   return TRUE;
}


/**
 * @brief Delete an event object
 * @param[in] event Pointer to the event object
 **/

void osDeleteEvent(OsEvent *event)
{
   //Make sure the operating system is running
   if(OS_IsRunning())
   {
      //Properly dispose the event object
      OS_EVENT_Delete(event);
   }
}


/**
 * @brief Set the specified event object to the signaled state
 * @param[in] event Pointer to the event object
 **/

void osSetEvent(OsEvent *event)
{
   //Set the specified event to the signaled state
   OS_EVENT_Set(event);
}


/**
 * @brief Set the specified event object to the nonsignaled state
 * @param[in] event Pointer to the event object
 **/

void osResetEvent(OsEvent *event)
{
   //Force the specified event to the nonsignaled state
   OS_EVENT_Reset(event);
}


/**
 * @brief Wait until the specified event is in the signaled state
 * @param[in] event Pointer to the event object
 * @param[in] timeout Timeout interval
 * @return The function returns TRUE if the state of the specified object is
 *   signaled. FALSE is returned if the timeout interval elapsed
 **/

bool_t osWaitForEvent(OsEvent *event, systime_t timeout)
{
   bool_t ret;

   //Wait until the specified event is in the signaled
   //state or the timeout interval elapses
   if(timeout == 0)
   {
      //Non-blocking call
      ret = OS_EVENT_Get(event);
   }
   else if(timeout == INFINITE_DELAY)
   {
      //Infinite timeout period
      OS_EVENT_Wait(event);
      ret = TRUE;
   }
   else
   {
      //Wait until the specified event becomes set
      ret = !OS_EVENT_WaitTimed(event, OS_MS_TO_SYSTICKS(timeout));
   }

   //The return value specifies whether the event is set
   return ret;
}


/**
 * @brief Set an event object to the signaled state from an interrupt service routine
 * @param[in] event Pointer to the event object
 * @return TRUE if setting the event to signaled state caused a task to unblock
 *   and the unblocked task has a priority higher than the currently running task
 **/

bool_t osSetEventFromIsr(OsEvent *event)
{
   //Set the specified event to the signaled state
   OS_EVENT_Set(event);

   //The return value is not relevant
   return FALSE;
}


/**
 * @brief Create a semaphore object
 * @param[in] semaphore Pointer to the semaphore object
 * @param[in] count The maximum count for the semaphore object. This value
 *   must be greater than zero
 * @return The function returns TRUE if the semaphore was successfully
 *   created. Otherwise, FALSE is returned
 **/

bool_t osCreateSemaphore(OsSemaphore *semaphore, uint_t count)
{
   //Create a semaphore
   OS_CreateCSema(semaphore, count);

   //The event object was successfully created
   return TRUE;
}


/**
 * @brief Delete a semaphore object
 * @param[in] semaphore Pointer to the semaphore object
 **/

void osDeleteSemaphore(OsSemaphore *semaphore)
{
   //Make sure the operating system is running
   if(OS_IsRunning())
   {
      //Properly dispose the specified semaphore
      OS_DeleteCSema(semaphore);
   }
}


/**
 * @brief Wait for the specified semaphore to be available
 * @param[in] semaphore Pointer to the semaphore object
 * @param[in] timeout Timeout interval
 * @return The function returns TRUE if the semaphore is available. FALSE is
 *   returned if the timeout interval elapsed
 **/

bool_t osWaitForSemaphore(OsSemaphore *semaphore, systime_t timeout)
{
   bool_t ret;

   //Wait until the semaphore is available or the timeout interval elapses
   if(timeout == 0)
   {
      //Non-blocking call
      ret = OS_CSemaRequest(semaphore);
   }
   else if(timeout == INFINITE_DELAY)
   {
      //Infinite timeout period
      OS_WaitCSema(semaphore);
      ret = TRUE;
   }
   else
   {
      //Wait until the specified semaphore becomes available
      ret = OS_WaitCSemaTimed(semaphore, OS_MS_TO_SYSTICKS(timeout));
   }

   //The return value specifies whether the semaphore is available
   return ret;
}


/**
 * @brief Release the specified semaphore object
 * @param[in] semaphore Pointer to the semaphore object
 **/

void osReleaseSemaphore(OsSemaphore *semaphore)
{
   //Release the semaphore
   OS_SignalCSema(semaphore);
}


/**
 * @brief Create a mutex object
 * @param[in] mutex Pointer to the mutex object
 * @return The function returns TRUE if the mutex was successfully
 *   created. Otherwise, FALSE is returned
 **/

bool_t osCreateMutex(OsMutex *mutex)
{
   //Create a mutex
   OS_CreateRSema(mutex);

   //The mutex was successfully created
   return TRUE;
}


/**
 * @brief Delete a mutex object
 * @param[in] mutex Pointer to the mutex object
 **/

void osDeleteMutex(OsMutex *mutex)
{
   //Make sure the operating system is running
   if(OS_IsRunning())
   {
      //Properly dispose the specified mutex
      OS_DeleteRSema(mutex);
   }
}


/**
 * @brief Acquire ownership of the specified mutex object
 * @param[in] mutex Pointer to the mutex object
 **/

void osAcquireMutex(OsMutex *mutex)
{
   //Obtain ownership of the mutex object
   OS_Use(mutex);
}


/**
 * @brief Release ownership of the specified mutex object
 * @param[in] mutex Pointer to the mutex object
 **/

void osReleaseMutex(OsMutex *mutex)
{
   //Release ownership of the mutex object
   OS_Unuse(mutex);
}


/**
 * @brief Retrieve system time
 * @return Number of milliseconds elapsed since the system was last started
 **/

systime_t osGetSystemTime(void)
{
   systime_t time;

   //Get current tick count
   time = OS_GetTime32();

   //Convert system ticks to milliseconds
   return OS_SYSTICKS_TO_MS(time);
}


/**
 * @brief Allocate a memory block
 * @param[in] size Bytes to allocate
 * @return A pointer to the allocated memory block or NULL if
 *   there is insufficient memory available
 **/

void *osAllocMem(size_t size)
{
   void *p;

   //Allocate a memory block
   p = OS_malloc(size);

   //Debug message
   TRACE_DEBUG("Allocating %" PRIuSIZE " bytes at 0x%08" PRIXPTR "\r\n", size, (uintptr_t) p);

   //Return a pointer to the newly allocated memory block
   return p;
}


/**
 * @brief Release a previously allocated memory block
 * @param[in] p Previously allocated memory block to be freed
 **/

void osFreeMem(void *p)
{
   //Make sure the pointer is valid
   if(p != NULL)
   {
      //Debug message
      TRACE_DEBUG("Freeing memory at 0x%08" PRIXPTR "\r\n", (uintptr_t) p);

      //Free memory block
      OS_free(p);
   }
}
