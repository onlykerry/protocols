/***********************************************************************/
/*                                                                     */
/*   Module:  _ferror.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements the ferror function for stdio.h               */
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
/*      ferror: Check file's error indicator                           */
/*                                                                     */
/*       Input: file = pointer to file control block                   */
/*                                                                     */
/*     Returns: non-zero value if the error indicator is set           */
/*                                                                     */
/***********************************************************************/
int ferror(FILE *file)
{
  int rv;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* Check for valid stream.                                           */
  /*-------------------------------------------------------------------*/
  if (InvalidStream(file))
  {
    set_errno(EBADF);
    return -1;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to stream.                               */
  /*-------------------------------------------------------------------*/
  file->acquire(file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Return error if file is closed.                                   */
  /*-------------------------------------------------------------------*/
  if (file->ioctl == NULL)
  {
    set_errno(EBADF);
    file->release(file, F_READ | F_WRITE);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Read the stream's error indicator.                                */
  /*-------------------------------------------------------------------*/
  rv = file->errcode;

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to stream and return result.             */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ | F_WRITE);
  return rv;
}

