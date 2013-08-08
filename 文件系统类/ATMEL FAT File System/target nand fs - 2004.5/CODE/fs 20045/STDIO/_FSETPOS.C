/***********************************************************************/
/*                                                                     */
/*   Module:  _fsetpos.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements the fsetpos function for stdio.h              */
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
/*     fsetpos: Set the file position for a stream                     */
/*                                                                     */
/*      Inputs: file = pointer to file control block                   */
/*              pos    = pointer to file position indicator            */
/*                                                                     */
/*     Returns: zero on success, nonzero on failure                    */
/*                                                                     */
/***********************************************************************/
int fsetpos(FILE *file, const fpos_t *pos)
{
  int rv;

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
  /* Call file system specific FSETPOS routine.                        */
  /*-------------------------------------------------------------------*/
  rv = (int)file->ioctl(file, FSETPOS, pos);

  /*-------------------------------------------------------------------*/
  /* If ioctl call succeeded, undo ungetc().                           */
  /*-------------------------------------------------------------------*/
  if (rv == 0)
    file->hold_char = '\0';

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to stream and return result.             */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ | F_WRITE);
  return rv;
}

