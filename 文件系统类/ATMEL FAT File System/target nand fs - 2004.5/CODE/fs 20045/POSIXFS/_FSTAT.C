/***********************************************************************/
/*                                                                     */
/*   Module:  _fstat.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements fstat()                                       */
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
/*       fstat: Obtain information about the named open file           */
/*                                                                     */
/*      Inputs: fid = file descriptor                                  */
/*              buf = pointer to where information is to be stored     */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int fstat(int fid, struct stat *buf)
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
  /* Ensure buffer parameter is valid.                                 */
  /*-------------------------------------------------------------------*/
  if (buf == NULL)
  {
    Files[fid].errcode = EFAULT;
    set_errno(EFAULT);
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
  /* Call file system specific FSTAT routine.                          */
  /*-------------------------------------------------------------------*/
  rc = (int)Files[fid].ioctl(&Files[fid], FSTAT, buf);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to file systems and return result.       */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ | F_WRITE);
  semPost(FileSysSem);
  return rc;
}

