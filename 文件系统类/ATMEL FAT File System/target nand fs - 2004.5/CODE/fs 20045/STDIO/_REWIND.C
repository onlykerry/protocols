/***********************************************************************/
/*                                                                     */
/*   Module:  _rewind.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2002.1                                                   */
/*   Purpose: Implements the rewind function for stdio.h               */
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
/*      rewind: Set the file position to beginning of file             */
/*                                                                     */
/*       Input: file = pointer to file control block                   */
/*                                                                     */
/***********************************************************************/
void rewind(FILE *file)
{
  int rc;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* Check for valid stream.                                           */
  /*-------------------------------------------------------------------*/
  if (InvalidStream(file) || (file->flags & FCB_DIR))
  {
    set_errno(EBADF);
    return;
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
    return;
  }

  /*-------------------------------------------------------------------*/
  /* Call file system specific FSEEK routine to seek to beginning.     */
  /*-------------------------------------------------------------------*/
  rc = (int)file->ioctl(file, FSEEK, 0, SEEK_SET);

  /*-------------------------------------------------------------------*/
  /* If success, then clear ungetc() and file errno. Else set error.   */
  /*-------------------------------------------------------------------*/
  if (rc == 0)
    file->hold_char = file->errcode = 0;
  else
    file->errcode = get_errno();

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to stream.                               */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ | F_WRITE);
}

