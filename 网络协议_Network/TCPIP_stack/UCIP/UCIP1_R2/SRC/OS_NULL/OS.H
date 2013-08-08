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

#define OS_ENTER_CRITICAL()		/* placeholder */
#define OS_EXIT_CRITICAL()		/* placeholder */                    

ULONG	OSTimeGet();

/*------ semaphores ------*/
#define	OS_EVENT long		    /* placeholder */
//extern OS_EVENT* OSSemCreate(int initval);
//extern void      OSSemPend(OS_EVENT * sem, UINT timeout);
//extern void      OSSemPost(OS_EVENT * sem);

extern OS_EVENT* OSSemCreate(UWORD value);
extern UWORD     OSSemAccept(OS_EVENT *pevent);
extern UBYTE     OSSemPost(OS_EVENT* pevent);
extern void      OSSemPend(OS_EVENT* pevent, UWORD timeout, UBYTE* err);
extern UBYTE     OSTaskCreate(void (OS_FAR *task)(void *pd), void *pdata, void *pstk, UBYTE prio);


/*################## AVOS ###################*/

int main();
void panic(char * msg);

/*-------- time management --------*/


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

ULONG time(ULONG *); 		// placeholder for standard ANSI C time
#endif


#if HAVE_ANSI_TIME==1

#include <time.h>
int gettime(struct tm * time);	// placeholder for standard ANSI C gettime

#endif


/*-------- random number generation --------*/
void magicInit();
ULONG magic();
void avRandomize();		/* maps to avChurnRand which depends on MD5_SUPPORT */


/*-------- device I/O --------*/
#if HAVE_ANSI_STDIO==0
#define FILE		void	/* placeholder */		
#endif

#define	FILE_DESC	int		/* placeholder */

//char * nameForDevice(FILE_DESC fd);
//void nPut(FILE_DESC fd, void /*NBuf*/ * buffer);
//void nGet(FILE_DESC fd, void /*NBuf*/ ** buffer, UINT killdelay);	 /* !!! check prototype */                   

#endif /* _OS_H_ */
