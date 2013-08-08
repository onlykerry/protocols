/*
***************************************************************************************************
*                                            uC/OS                                                *
*                    Microcomputer Real-Time Multitasking Operating System                        *
*                    (c) Copyright 1992, Jean J. Labrosse, Plantation, FL                         *
*                                                                                                 *
*                                                                                                 *
* File: OS.H                                                                                    *
***************************************************************************************************
*/
#ifndef _OS_H_
#define _OS_H_

/*
***************************************************************************************************
*                                      uC/OS Configuration                                        *
***************************************************************************************************
*/
#define uCOS            0x80    /* interrupt vector assigned to uCOS */
#define OS_MAX_TASKS      20    /* Maximum number of tasks in your application */
#define OS_MAX_EVENTS     20    /* Maximum number of event control blocks in your application */
#define OS_MAX_QS          5    /* Maximum number of queue control blocks in your application */
#define OS_IDLE_TASK_STK_SIZE 1024 /* Idle task stack size (BYTEs) */

#undef BOOLEAN
#undef UBYTE
#undef BYTE
#undef UWORD
#undef WORD
#undef ULONG
#undef LONG
typedef unsigned char  BOOLEAN;
typedef unsigned char  UBYTE;        /* Unsigned  8 bit quantity */
//typedef   signed char  BYTE;         /* Signed    8 bit quantity */
typedef unsigned short UWORD;        /* Unsigned 16 bit quantity */
//typedef   signed short WORD;         /* Signed   16 bit quantity */
#define WORD signed short            /* Signed   16 bit quantity */
typedef unsigned long  ULONG;        /* Unsigned 32 bit quantity */
typedef   signed long  LONG;         /* Signed   32 bit quantity */

/*
***************************************************************************************************
*                                      uC/OS Error Codes                                          *
***************************************************************************************************
*/
#define OS_NO_ERR                0       /* ERROR CODES */
#define OS_TIMEOUT              10
#define OS_MBOX_FULL            20
#define OS_Q_FULL               30
#define OS_PRIO_EXIST           40
#define OS_PRIO_ERR             41
#define OS_SEM_ERR              50
#define OS_SEM_OVF              51
#define OS_TASK_DEL_ERR         60
#define OS_TASK_DEL_IDLE        61
#define OS_NO_MORE_TCB          70

/*
***************************************************************************************************
*                                      EVENT CONTROL BLOCK                                        *
***************************************************************************************************
*/
typedef struct os_event {
  UBYTE OSEventGrp;             /* Group corresponding to tasks waiting for event to occur */
  UBYTE OSEventTbl[8];          /* List of tasks waiting for event to occur */
  WORD  OSEventCnt;             /* Count of used when event is a semaphore */
  void* OSEventPtr;             /* Pointer to message or queue structure */
#ifdef WIN32
  unsigned long OSInterlock;
#endif
} OS_EVENT;


/*
***************************************************************************************************
*                                      uC/OS GLOBAL VARIABLES                                     *
***************************************************************************************************
*/
extern UWORD   OSCtxSwCtr;      /* Counter of number of context switches */
extern ULONG   OSIdleCtr;       /* Idle counter */
extern BOOLEAN OSRunning;       /* Flag indicating that kernel is running */

/*
***************************************************************************************************
*                                      uC/OS FUNCTION PROTOTYPES                                  *
***************************************************************************************************
*/
void      OSInit(void);
void      OSStart(void);
void      OSStartHighRdy(void);
void      OSSched(void);
void      OSSchedLock(void);
void      OSSchedUnlock(void);
UBYTE     OSTCBInit(UBYTE p, void* stk);

//static void FAR  OSTaskIdle(void *data);
//UBYTE     OSTaskCreate(void (FAR *task)(void *pd), void *pdata, void *pstk, UBYTE prio);

UBYTE     OSTaskCreate(void (*task)(void*), void* pdata, void* pstk, UBYTE prio);

UBYTE     OSTaskDel(UBYTE p);
UBYTE     OSTaskChangePrio(UBYTE oldp, UBYTE newp);

UBYTE     OSTaskSuspend(UBYTE prio);
UBYTE     OSTaskResume(UBYTE prio);

void      OSIntEnter(void);
void      OSIntExit(void);

void      OSIntCtxSw(void);
//void FAR  OSCtxSw(void);
void      OSCtxSw(void);

void      OSTimeDly(UWORD ticks);
void      OSTimeTick(void);
void      OSTimeSet(ULONG ticks);
ULONG     OSTimeGet(void);

OS_EVENT* OSSemCreate(WORD value);
UBYTE OSSemPost(OS_EVENT* pevent);
void OSSemPend(OS_EVENT* pevent, UWORD timeout, UBYTE* err);


#endif // _OS_H_
