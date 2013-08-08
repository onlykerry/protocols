/***********************************************************************/
/*                                                                     */
/*   Module:  _clearerr.c                                              */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements the clearerr function for stdio.h             */
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
#include "stdiop.h"

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*    clearerr: Clear the specified stream's error indicator           */
/*                                                                     */
/*       Input: file = pointer to file control block                   */
/*                                                                     */
/***********************************************************************/
void clearerr(FILE *file)
{
#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* Check for valid stream.                                           */
  /*-------------------------------------------------------------------*/
  if (InvalidStream(file))
  {
    set_errno(EBADF);
    return;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to file.                                 */
  /*-------------------------------------------------------------------*/
  file->acquire(file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Return error if file is closed.                                   */
  /*-------------------------------------------------------------------*/
  if (file->ioctl == NULL)
  {
    set_errno(EBADF);
    file->release(file, F_READ | F_WRITE);
    return;
  }

  /*-------------------------------------------------------------------*/
  /* Clear file's error indicator.                                     */
  /*-------------------------------------------------------------------*/
  file->errcode = 0;

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to lower file system and return.         */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ | F_WRITE);
}

