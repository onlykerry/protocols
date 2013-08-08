/** 
    @file os.h

    Operating system functions used by uCIP.

    Use this as a starting point to adapt uCIP 
    to your own operating system.
*/                                                                                    

#ifndef _OS_H_
#define _OS_H_

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

#ifndef UINT
#define UINT	unsigned int
#endif
#ifndef ULONG
#define ULONG	unsigned long int
#endif

//#define OS_ENTER_CRITICAL()		/* placeholder */
//#define OS_EXIT_CRITICAL()		/* placeholder */                    

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

//int main();
//void panic(char * msg);

/*-------- time management --------*/
int clk_stat();
//void msleep(ULONG time);
//LONG diffTime(ULONG time);
//ULONG mtime();
//long diffJTime(ULONG time);
//ULONG jiffyTime();


#endif /* _OS_H_ */
