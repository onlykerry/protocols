/*****************************************************************************
* nethelp.c
*
* portions Copyright (c) 1997 by Global Election Systems Inc.
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
* Guy Lancaster <lancasterg@acm.org>, Global Election Systems Inc.
*   97-02-12  Modified from startup.c.
*
*(yyyy-mm-dd)
* 2001-04-05 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Collection of code from various modules, build support.
*
*****************************************************************************
*/
#include <stdio.h>      // Need sprintf()
#include <string.h>

#include "netconf.h"
#include "netbuf.h"
#include "nettimer.h"
#include "netos.h"



#define INTR_TU2 0x0000  // added by robert to make build
#define MSPANIC "panic"  // added by robert to make build
#define LCDBUFSZ 2000    // added by robert to make build
#define MSCALFORSERV 0x55AA // added by robert to make build


short MD1;                 // added by robert to make build
short TMIC2;               // added by robert to make build
short TMC1;                // added by robert to make build
short TM1;                 // added by robert to make build
unsigned long OSTime;      // added by robert to make build

u_short buttonStatus(void) { return 0; }
u_short buttonNoStatus(void) { return 0; }
u_short prompt(int button, int timeout, void* unknown) { return 0; }

/* Input output control */
int ioctl(int fid, int cmd, void *arg) { return 0; }

/* Put an nBuf packet to device. */
int nPut(int fid, NBuf *nb) { return 0; }

/* Get an nBuf packet from device. */
int nGet(int fid, NBuf **nb, int timeout) { return 0; }

/* Get name for file device ID. */
const char *nameForDevice(int fd)
{
    return "unknown";
}



/*****************************/
/*** LOCAL DATA STRUCTURES ***/
/*****************************/
static void (*avShutdown)(void) = NULL;


/**********************************/
/*** LOCAL FUNCTION DEFINITIONS ***/
/**********************************/
/*
 * Start the Accu-Vote's OS clock timer.  This is known as a 'Jiffy'
 *  timer and the clock ticks are known as 'Jiffies'.
 */
static void startJiffy(void)
{
    OS_ENTER_CRITICAL();
//    setvect(INTR_TU2, tickISR); // rd
//    MD1 = HZCNTDOWN;
//    TMIC2 &= ~0xC0;     /* what does this mean: sould be set? */
//    TMC1 = 0xC0;        /* start timer at FCLK/128 */
    OS_EXIT_CRITICAL();
}


/***********************************/
/*** PUBLIC FUNCTION DEFINITIONS ***/
/***********************************/
void ucosInit(void (*shutdown)(void))
{
    avShutdown = shutdown;
    startJiffy();
}


/*
 * Sleep for n seconds;
 */
void sleep(u_short n)
{
    OSTimeDly((UWORD)(n * HZ));
}

/*
 * Sleep ms milliseconds.  Note that this only has a (close to) 1 Jiffy 
 *  resolution.  Use msleep[01] if you need better than this.
 * Note: Since there may me less than a ms left before the next clock
 *  tick, 1 tick is added to ensure we delay at least ms time.
 */
void msleep(u_short ms)
{
    OSTimeDly((UWORD)((ms / MSPERTICK) + 1));
}

/*
 * Return the milliseconds since power up.  We base this on the number of 
 *  Jiffies that have passed plus the remaining time on the Jiffy timer.
 */
ULONG mtime(void)
{
//    u_short count;
//    ULONG time;

    OS_ENTER_CRITICAL();
//    count = TM1;
    /* Get the current OSTime and check. If an interrupt is pending then add 1
        to the OSTime */
//    time = OSTime + ((TMIC2 & 0x80) ? 1UL : 0UL);
    OS_EXIT_CRITICAL();
//    return (time * MSPERTICK) + (HZCNTDOWN - (count/(FDIVCLK/1000)));
    return OSTime;
}

/*
 * Return the difference between t and the current system elapsed
 *  time in milliseconds.
 */
LONG diffTime(ULONG t)
{
    return t - mtime();
}


/*
 * Return the time in Jiffy timer ticks since power up.
 */
ULONG jiffyTime(void)
{
    ULONG time;
    
    OS_ENTER_CRITICAL();
    
    /* Get the current OSTime and if an interrupt is pending then add 1. */
    time = OSTime + ((TMIC2 & 0x80) ? 1UL : 0UL);
    OS_EXIT_CRITICAL();
    
    return time;
}

/*
 * Return the difference between t and the current system elapsed
 *  time in Jiffy timer ticks.  Positive values indicate that t
 *  is in the future, negative that t is in the past.
 */
LONG diffJTime(ULONG t)
{
    return t - jiffyTime();
}


/*
 * Halt the system.  If a shutdown function is registered, it is invoked
 *  to turn off devices before we lock up.
 * Note that this disables task switching but does not disable interrupt
 *  handling (thus the debugger will still function).
 */
void HALT(void)
{
    OSSchedLock();          // Disable task switching.
    if (avShutdown)
        (*avShutdown)();    // Shut down devices.
    for (;;)                // And loop here forever.
        ;
    /* Not reached */
}

/* Display a panic message and HALT the system. */
void panic(char *msg)
{
    char errStr[50];
    
    OSSchedLock();
//    _asm STI;
    
    sprintf(errStr, MSPANIC, msg);
//    lcdRawWrite(errStr); // rd
    HALT();
    /* Not reached */
}


/*
 * Display the interrupt service routine number and the address at the time
 *  of interrupt and halt.  This is normally called for an unexpected interrupt.
 */
void isrDisplay(int isrnum, int ps, int pc)
{
    char buffer[LCDBUFSZ + 1];
    
    OSSchedLock();
    sprintf(buffer, "ISR%-2d: %4.4X:%4.4X\n%s", isrnum, ps, pc, MSCALFORSERV);
//    lcdRawWrite(buffer); // rd
    HALT();
    /* not reached */   
}



/*
 * Interrupt handler for the Accu-Vote's OS clock.  Currently this services
 *  the timer service, the uC/OS time tick, and the pseudo interrupts.
 */
//static void interrupt tickISR(void)
static void tickISR(void)
{
    OSIntEnter();
    timerCheck();
    OSTimeTick();
//    fint(); // rd
    OSIntExit();
}

