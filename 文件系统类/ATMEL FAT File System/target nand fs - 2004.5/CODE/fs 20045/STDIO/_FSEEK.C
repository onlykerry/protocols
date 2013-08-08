/***********************************************************************/
/*                                                                     */
/*   Module:  _fseek.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2002.1                                                   */
/*   Purpose: Implements fseek()                                       */
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
/*       fseek: Set file position indicator                            */
/*                                                                     */
/*      Inputs: stream = pointer to file to be positioned              */
/*              offset = file offset                                   */
/*              whence = reference position for offset                 */
/*                                                                     */
/*     Returns: zero on success and -1 on failure                      */
/*                                                                     */
/***********************************************************************/
int fseek(FILE *file, long offset, int whence)
{
  int rc;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* Check for valid stream.                                           */
  /*-------------------------------------------------------------------*/
  if (InvalidStream(file) || (file->flags & FCB_DIR))
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
  /* Call file system specific FSEEK routine.                          */
  /*-------------------------------------------------------------------*/
  rc = (int)file->ioctl(file, FSEEK, offset, whence);

  /*-------------------------------------------------------------------*/
  /* If seek succeeded, then clear effects of ungetc(), else error.    */
  /*-------------------------------------------------------------------*/
  if (rc != -1)
  {
    file->hold_char = '\0';
    rc = 0;
  }
  else
    file->errcode = get_errno();

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to stream and return result.             */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ | F_WRITE);
  return rc;
}

