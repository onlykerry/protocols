/*****************************************************************************
* target.h - 
*
* Copyright (c) 2001 by Cognizant Pty Ltd.
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
* 2001-06-01 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Original file.
*
*****************************************************************************
*/
#ifndef _TARGET_H_
#define _TARGET_H_

#undef UBYTE
#undef BYTE
#undef UWORD
#undef WORD
#undef ULONG
#undef LONG

///////////////////////////////////////////////////////////////////////////////

#define FALSE 0
#define TRUE  1

void os_enter_critical(void);
void os_exit_critical(void);
void os_task_sw(void);
#define OS_ENTER_CRITICAL()  os_enter_critical()
#define OS_EXIT_CRITICAL()   os_exit_critical()
#define OS_TASK_SW()         os_task_sw()


#pragma warning (push)
#pragma warning (disable: 4142) // benign redefinition of type
typedef unsigned char  UBYTE;        // Unsigned  8 bit quantity
typedef   signed char  BYTE;         // Signed    8 bit quantity
typedef unsigned short UWORD;        // Unsigned 16 bit quantity
typedef   signed short WORD;         // Signed   16 bit quantity
typedef unsigned long  ULONG;        // Unsigned 32 bit quantity
typedef   signed long  LONG;         // Signed   32 bit quantity
#pragma warning (pop)


typedef signed int     INT;
typedef unsigned int   UINT;
//typedef signed short   SHORT;                     /* Signed   16 bit quantity                           */
//typedef unsigned short USHORT;                    /* Unsigned 16 bit quantity                           */
#ifndef FAR
#define FAR
#endif
#define OS_FAR
#ifndef NEAR
#define NEAR
#endif
#include "types.h"
//#include "ucos.h"
#include "time.h"

///////////////////////////////////////////////////////////////////////////////
/*####### LOCAL CONFIGURATION ################*/
#define AVOS	0	/** @todo see netchat.c and FIX */

#define HAVE_ANSI_TIME		1
#define HAVE_ANSI_STDIO		1
#define FORCE_16BIT_INT		AVOS

#ifdef __IAR_SYSTEMS_ICC
#define COMPILER_BROKEN_BITFIELD	/* does not support char bitfields ?? */
#endif


/*################## uC/OS ###################*/
#define OS_NO_ERR                 0
#define OS_TIMEOUT               10

#ifndef UINT
#define UINT	unsigned int
#endif
#ifndef ULONG
#define ULONG	unsigned long int
#endif

ULONG	OSTimeGet();

/*------ semaphores ------*/
#define	OS_EVENT long		    /* placeholder */

extern OS_EVENT* OSSemCreate(UWORD value);
extern UWORD     OSSemAccept(OS_EVENT *pevent);
extern UBYTE     OSSemPost(OS_EVENT* pevent);
extern void      OSSemPend(OS_EVENT* pevent, UWORD timeout, UBYTE* err);
extern UBYTE     OSTaskCreate(void (OS_FAR *task)(void *pd), void *pdata, void *pstk, UBYTE prio);


/*################## AVOS ###################*/
//#define TICKSPERSEC 1
//#define MSPERTICK 1

int main();
void panic(char * msg);

/*-------- time management --------*/
int clk_stat();
//void msleep(ULONG time);
//LONG diffTime(ULONG time);
//ULONG mtime();
//long diffJTime(ULONG time);
//ULONG jiffyTime();


/*-------- ANSI time.h ------------*/
#if HAVE_ANSI_TIME==0
#define _TIME_H_
struct tm
{ 
	int	tm_sec;
	int	tm_min;
	int	tm_hour;
	int	tm_mday;
	int	tm_mon;
	int	tm_year;
	int	tm_wday;
	int	tm_yday;
	int	tm_isdst;
	int tm_gmtoff;
	char *tm_zone;
};

ULONG time(ULONG *); 		/* placeholder for standard ANSI C time */
#endif

int gettime(struct tm * time);	/* placeholder for standard ANSI C gettime */


/*-------- random number generation --------*/
void magicInit();
ULONG magic();
void avRandomize();		/* maps to avChurnRand which depends on MD5_SUPPORT */


/*-------- device I/O --------*/
#if HAVE_ANSI_STDIO==0
#define FILE		void	/* placeholder */		
#endif

#define	FILE_DESC	int		/* placeholder */

///////////////////////////////////////////////////////////////////////////////

#define B0 0x01
#define B1 0x02
#define B2 0x04
#define B3 0x08
#define B4 0x10
#define B5 0x20
#define B6 0x40
#define B7 0x80

#define TICKSPERSEC 1024


#endif

/* ---- end of target.h ----------------------------------------------------- */
