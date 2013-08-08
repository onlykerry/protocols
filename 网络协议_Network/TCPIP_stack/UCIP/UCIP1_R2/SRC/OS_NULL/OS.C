/** 
 *  @file os.c
 *
 *  Operating system functions used by uCIP.
 *    
 *  Use this as a starting point to adapt uCIP 
 *  to your own operating system.
 */
#include <stdio.h>
#include <process.h>
#include <string.h>

#include "..\netconf.h"
#include "..\netbuf.h"
#include "..\net.h"
//#include "..\netaddrs.h"
//#include "netdebug.h"

#include "os.h"


/*################## uC/OS ###################*/
ULONG OSTimeGet()
{}


/*------ semaphores ------*/
OS_EVENT* OSSemCreate(UWORD value)
{}

UWORD OSSemAccept(OS_EVENT *pevent)
{}

UBYTE OSSemPost(OS_EVENT* pevent)
{}

void OSSemPend(OS_EVENT* pevent, UWORD timeout, UBYTE* err)
{}

UBYTE OSTaskCreate(void (OS_FAR *task)(void *pd), void *pdata, void *pstk, UBYTE prio)
{}



/*################## AVOS ###################*/
//int main() {}


void panic(char * msg)  /* panic message */
{}


/*-------- time management --------*/
int clk_stat()
{}

// Time diff in ms, between system time in ms and time in ms
LONG diffTime(ULONG time)
{}

void msleep(ULONG time)
{}

// Get system time in ms
ULONG mtime()
{}

// Time diff in jiffys, between system time in jiffys and time in jiffys
// !!!NOTICE!!!
// If time is newer than system time this functions returns a value > 0
// If time is older than system time this functions returns a value < 0 (NEGATIVE VALUE)
long diffJTime(ULONG time)
{}

// Get system time in jiffys, which is a system clock tick 
ULONG jiffyTime()
{}

int gettime(struct tm * time)   /* standard ANSI C */
{}

#if HAVE_ANSI_TIME==0

// This function returns time in seconds since the first call.
// It's used but the ARP protocol in netether.c
// You can remove this if your OS/COMPILER already supports standard ANSI time functions
// or if you don't wish to use the ethernet feature of this stack.
// This functions uses mtime()!
ULONG time(ULONG *returnTime) 
{
  static ULONG timeStorage = 0;
  static ULONG lastTime = 0;
  ULONG newTime;
  ULONG timeDiff;

  if (timeStorage)
  {
    // Get new system time
    newTime = mtime();

    // Get time difference in seconds since last call
    if (newTime >= lastTime)
      timeDiff = (newTime - lastTime) / 1000;
    else
      timeDiff = (0 - lastTime + newTime) / 1000;

    // If time difference is more than one second since last called, update time
    if (timeDiff >= 1 )
    {
      timeStorage += timeDiff;
      lastTime    += timeDiff * 1000;
    }
  }
  else
  {
    lastTime = mtime();
    timeStorage = 1;
  }

  if (returnTime) *returnTime = timeStorage;
  return timeStorage;
}
#endif



/*-------- random number generation --------*/
//void magicInit() {}
//ULONG magic() {}
//void avRandomize() {}

void delay(int milliseconds) {}


/*-------- device I/O --------*/
#define FILE_DESC   int     /* the real one depends on your system */       
#define Nbuf void   /* the real one is defined in netbuf.h */

const char *nameForDevice(int fd) { return "unknown"; }

void nPut(FILE_DESC fd, NBuf * buffer)
{}

void nGet(FILE_DESC fd, NBuf ** buffer, UINT killdelay)  /* !!! check prototype */
{}
