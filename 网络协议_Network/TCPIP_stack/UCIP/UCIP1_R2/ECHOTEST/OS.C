/*****************************************************************************
* os.c - os extensions for running under WIN32
*
* portions Copyright (c) 2001 by Cognizant Pty Ltd.
*
* The authors hereby grant permission to use, copy, modify, distribute,
* and license this software and its documentation for any purpose, provided
* that existing copyright notices are retained in all copies and that this
* notice and the following disclaimer are included verbatim in any 
* distributions. No written agreement, license, or royalty fee is required
* for any of the authorized uses.
*
* THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS *AS IS* AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
* IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
******************************************************************************
* REVISION HISTORY (please don't use tabs!)
*
*(yyyy-mm-dd)
* 2001-05-12 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Original file.
*
*****************************************************************************
*/
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <stdio.h>
#include <process.h>

#include "target.h"
#include "os.h"
#include "trace.h"

////////////////////////////////////////////////////////////////////////////////
//VOID InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
//VOID EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
//VOID LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
CRITICAL_SECTION UCOS_CS;


ULONG    OSIdleCtr;         /* Counter used in OSTaskIdle() */
static  UBYTE      OSLockNesting;       /* Multitasking lock nesting level */

////////////////////////////////////////////////////////////////////////////////
// UCOS simulated functions

void os_enter_critical(void)
{
    EnterCriticalSection(&UCOS_CS);
}

void os_exit_critical(void)
{
    LeaveCriticalSection(&UCOS_CS);
}

void os_task_sw(void)
{
}

////////////////////////////////////////////////////////////////////////////////
/*
HANDLE OpenMutex(
  DWORD dwDesiredAccess,  // access
  BOOL bInheritHandle,    // inheritance option
  LPCTSTR lpName          // object name
);
 */

OS_EVENT* OSSemCreate(UWORD value)
{
    HANDLE handle;
    
//    handle = CreateMutex(NULL, FALSE, NULL);
    handle = CreateEvent(NULL, FALSE, FALSE, NULL);

    TRACE("OSSemCreate(%u)\n", value);
    return handle;
}

void OSSemPend(OS_EVENT* pevent, UWORD timeout, UBYTE* err)
{
    DWORD retval = WaitForSingleObject(pevent, timeout);
    if (err != NULL) {
        if (retval == WAIT_OBJECT_0) {
            *err = OS_NO_ERR;
        } else {
            *err = OS_TIMEOUT;
        }
    }
//    return (retval == WAIT_TIMEOUT ? OS_TIMEOUT : OS_NO_ERR);
//WAIT_ABANDONED The specified object is a mutex object that was not released by the thread that owned the mutex object before the owning thread terminated. Ownership of the mutex object is granted to the calling thread, and the mutex is set to nonsignaled. 
//WAIT_OBJECT_0 The state of the specified object is signaled. 
//WAIT_TIMEOUT The time-out interval elapsed, and the object's state is nonsignaled.

}

UBYTE OSSemPost(OS_EVENT* pevent)
{
//    return (ReleaseMutex(pevent) ? OS_NO_ERR : OS_TIMEOUT);
    return (SetEvent(pevent) ? OS_NO_ERR : OS_TIMEOUT);
}


/*
#define MAX_OS_EVENTS 50
OS_EVENT OSEventList[MAX_OS_EVENTS];

OS_EVENT* OSSemCreate(WORD value)
{
    OS_EVENT* pevent = NULL;

    TRACE("OSSemCreate(%u)\n", value);
    if (value < MAX_OS_EVENTS) {
        pevent = &OSEventList[value];
        pevent->OSInterlock = 0L;
    }
    return pevent;
}

void OSSemPend(OS_EVENT* pevent, UWORD timeout, UBYTE* err)
{
}

UBYTE OSSemPost(OS_EVENT* pevent)
{
    return OS_NO_ERR;
}
 */


////////////////////////////////////////////////////////////////////////////////

void OSInit(void)
{
//VOID InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
    InitializeCriticalSection(&UCOS_CS);
}

////////////////////////////////////////////////////////////////////////////////

short TaskGetLastError(void)
{
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

UBYTE OSTaskCreate(void (FAR* task)(void* pd), void* pdata, void* pstk, UBYTE p)
{
    _beginthread(task, 0, pdata);
    return OS_NO_ERR;
}

void OSTickISR(void)
{
}

UBYTE OSTaskSuspend(UBYTE prio)
{
    return 0;
}

UBYTE OSTaskResume(UBYTE prio)
{
    return 0;
}

void OSTimeDly(UWORD ticks)
{
}

ULONG OSTimeGet(void)
{
    return 0;
}

void OSSchedLock(void)
{
  OSLockNesting++;                      /* Increment lock nesting level */
}

void OSStart(void)
{
}

////////////////////////////////////////////////////////////////////////////////

void delay(int milliseconds)
{
    Sleep(milliseconds);
}

void panic(char * msg)
{
    printf(msg);
    exit(1);
}


/*-------- time management --------*/
int clk_stat()
{
    return 0;
}

// Time diff in ms, between system time in ms and time in ms
LONG diffTime(ULONG time)
{
    return 0;
}

void msleep(ULONG time)
{}

// Get system time in ms
ULONG mtime()
{
    return 0;
}

// Time diff in jiffys, between system time in jiffys and time in jiffys
// !!!NOTICE!!!
// If time is newer than system time this functions returns a value > 0
// If time is older than system time this functions returns a value < 0 (NEGATIVE VALUE)
long diffJTime(ULONG time)
{
    return 0;
}

// Get system time in jiffys, which is a system clock tick 
ULONG jiffyTime()
{
    return 0;
}

int gettime(struct tm * time)   /* standard ANSI C */
{
    return 0;
}

