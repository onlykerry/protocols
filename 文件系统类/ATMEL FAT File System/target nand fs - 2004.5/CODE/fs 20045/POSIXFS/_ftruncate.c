/***********************************************************************/
/*                                                                     */
/*   Module:  _ftruncate.c                                             */
/*   Release: 2004.5                                                   */
/*   Version: 2002.3                                                   */
/*   Purpose: Implements ftruncate()                                   */
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
#include "posixfsp.h"

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*   ftruncate: Truncate the file size to the specified value          */
/*                                                                     */
/*      Inputs: fid = file descriptor                                  */
/*              length = desired file size                             */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int ftruncate(int fid, off_t length)
{
  int rc;
  FILE *file;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* If the file descriptor is invalid, return error.                  */
  /*-------------------------------------------------------------------*/
  if (fid < 0 || fid >= FOPEN_MAX)
  {
    set_errno(EBADF);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Ensure length parameter is valid.                                 */
  /*-------------------------------------------------------------------*/
  if (length < 0)
  {
    Files[fid].errcode = EINVAL;
    set_errno(EINVAL);
    return -1;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Get exclusive access to upper file system.                        */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Return error if file is closed.                                   */
  /*-------------------------------------------------------------------*/
  file = &Files[fid];
  if (file->ioctl == NULL)
  {
    semPost(FileSysSem);
    set_errno(EBADF);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  file->acquire(file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Call file system specific FTRUNCATE routine.                      */
  /*-------------------------------------------------------------------*/
  rc = (int)Files[fid].ioctl(&Files[fid], FTRUNCATE, length);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to file systems and return result.       */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ | F_WRITE);
  semPost(FileSysSem);
  return rc;
}

