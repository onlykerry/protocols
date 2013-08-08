/***********************************************************************/
/*                                                                     */
/*   Module:  kernel.h                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2004.0                                                   */
/*   Purpose: Kernel prototypes and constants                          */
/*                                                                     */
/*---------------------------------------------------------------------*/
/*                                                                     */
/*               Copyright 2004, Blunk Microsystems                    */
/*                      ALL RIGHTS RESERVED                            */
/*                                                                     */
/*   Licensees have the non-exclusive right to use, modify, or extract */
/*   this computer program for software development at a single site.  */
/*   This program may be resold or disseminated in executable format   */
/*   only. The source code may not be redistributed or resold.         */
/*                                                                     */
/***********************************************************************/
#ifndef _KERNEL_H /* Don't include this file more than once */
#define _KERNEL_H

#ifdef __cplusplus
extern "C"
{
#endif

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
/*
** Flags for Semaphore/Queue Mode for Task Queueing
*/
#define OS_FIFO         0

/*
** Flags for Wait-Option
*/
#define WAIT_FOREVER    ((ui32)-1)

/*
** Task Stack Size Codes
*/
#define STACK_2KB       71

/***********************************************************************/
/* Type Declarations                                                   */
/***********************************************************************/
typedef struct tcb *TASK; /* Task Control Block */
typedef struct scb *SEM;     /* Semaphore Control Block */

/***********************************************************************/
/* Variable Declarations                                               */
/***********************************************************************/
extern ui32 OsSecCount;

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
/*
** Task Related Routines
*/
TASK taskCreate(const char name[8], ui8 stack_size, ui8 priority,
                void (*funcp)(ui32 msg), ui32 msg, ui32 flags);
void taskSleep(ui32 wait_opt);
void taskWake(TASK tid);

/*
** Semaphore Related Routines
*/
SEM  semCreate(char name[8], int init_count, int mode);
void semPost(SEM sem);
int  semPend(SEM sem, ui32 wait_opt);
void semDelete(SEM *sem);

/*
** Interrupt Related Routines
*/
int  isrCreate(uint vector, void (*func_ptr)(void));
void isrMask(void);
void isrUnmask(void);
int  isrMasked(void);

#ifdef __cplusplus
}
#endif

#endif /* _KERNEL_H */

