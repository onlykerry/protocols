/*****************************************************************************
* netos.h
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
* 2001-06-08 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Original file.
*
******************************************************************************
*/
#ifndef NETOS_H
#define NETOS_H


//------------------------------------------------------------------------------
// removed from avconfig.h

//#define OS_PRIO_SELF  1
#define PRI_TIMER     2
#define PRI_MAIN      3
#define PRI_PPP0      4
#define PRI_ECHO      5
#define PRI_MON0      6
#define PRI_MON1      7
#define PRI_ETH0      8

// MODULE    TASK       PARAMETER  STACK SIZE                       PRIORITY LEVEL/HANDLE
// netppp.c  pppMain,   (void*)pd, pc->pppStack + STACK_SIZE,       (UBYTE)(PRI_PPP0 + pd));
// nettcp.c  tcpEcho,   NULL,      tcpEchoStack + STACK_SIZE,       PRI_ECHO);
// startup.c mainTask,  NULL,      mainTaskStack + STACK_MAIN_SIZE, PRI_MAIN);
// timer.c   timerTask, NULL,      timerStack + TIMER_STACK_SIZE,   PRI_TIMER);
// debug.c   monitorMain0, (void *)0, monitorControl[0].monitorStack + STACK_SIZE, PRI_MON0);
// debug.c   monitorMain1, (void *)1, monitorControl[1].monitorStack + STACK_SIZE, PRI_MON1);


//------------------------------------------------------------------------------
// removed from avos.h

/**************************/
/*** PUBLIC DEFINITIONS ***/
/**************************/
/*
 *  OS Jiffy clock constants.  We use a 125 Jiffy per second
 *  (15.625ms) tick rate.  Since we use milliseconds extensively, it's
 *  best to keep the result of 1000/HZ as base-2 to make most run-time 
 *  calculations simple shift operations.
 */
//#define FCLK      7500000UL               /*  clock frequency */
#define HZ          125U                    /*  ticks per second */
//#define TICKSPERSEC HZ  // now defined in target.h includes
#define MSPERTICK   (1000/HZ)               /*  milliseconds per tick */
#define MSPERJIFFY  (1000/HZ)               /*  Milliseconds per Jiffy */
//#define JIFFYPERSEC   (HZ)                    /*  Jiffies per second */
//#define   FDIVCLK     (FCLK/128)              /*  timer frequency */
//#define MSCNTDOWN ((INT)(FDIVCLK/1000))   /*  count down value for one millisecond */
//#define HZCNTDOWN ((INT)(FDIVCLK/HZ))     /*  count down value for one tick */


void ucosInit(void (*shutdown)(void));
//u_short buttonStatus(void); // added by robert
//u_short buttonNoStatus(void); // added by robert
//u_short prompt(int button, int timeout, void* unknown);

#define YESNOBUTTON   7
#define NOBUTTON      0
#define tUndef 0xff                    // added by robert to make build


//------------------------------------------------------------------------------

/*
 * Sleep for n seconds;
 */
void sleep(u_short n);

/*
 * Sleep ms milliseconds.  Note that this only has a (close to) 1 Jiffy 
 *  resolution.
 * Note: Since there may me less than a ms left before the next clock
 *  tick, 1 tick is added to ensure we delay at least ms time.
 */
void msleep(u_short ms); // now defined in target.h includes

/*
 * Return the milliseconds since power up.  We base this on the number of 
 *  Jiffies that have passed plus the remaining time on the Jiffy timer.
 */
ULONG mtime(void);

/*
 * Return the difference between t and the current system elapsed
 *  time in milliseconds.
 */
LONG diffTime(ULONG t); // now defined in target.h includes

/*
 * Return the time in Jiffy timer ticks since power up.
 */
ULONG jiffyTime(void);

/*
 * Return the difference between t and the current system elapsed
 *  time in Jiffy timer ticks.  Positive values indicate that t
 *  is in the future, negative that t is in the past.
 */
LONG diffJTime(ULONG t);

/*
 * Halt the system.  Used by shutdown() which is in startup since 
 *  shutting down the system is in a way the reverse of starting
 *  up the system.  Note that this disables task switching but
 *  does not disable interrupt handling (thus the debugger will
 *  still function).
 */
void HALT(void);

/* Display a panic message and HALT the system. */
void panic(char* msg);

/*
 * Display the interrupt service routine number and the address at the time
 *  of interrupt and halt.  This is normally called for an unexpected interrupt.
 */
void isrDisplay(int isrnum, int ps, int pc);

int clk_stat();

void delay(int milliseconds);


#endif /* NETOS_H */
