/***********************************************************************/
/*                                                                     */
/*   Module:  _fgetpos.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2002.1                                                   */
/*   Purpose: Implements the fgetpos function for stdio.h              */
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
/*     fgetpos: Get the file position indicator for the file           */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              pos    = pointer to file position indicator            */
/*                                                                     */
/*     Returns: zero on success, nonzero on failure                    */
/*                                                                     */
/***********************************************************************/
int fgetpos(FILE *file, fpos_t *pos)
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
  /* Call file system specific FGETPOS routine.                        */
  /*-------------------------------------------------------------------*/
  rc = (int)file->ioctl(file, FGETPOS, pos);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to stream and return result.             */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ | F_WRITE);
  return rc;
}

