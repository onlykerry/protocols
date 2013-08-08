/***********************************************************************/
/*                                                                     */
/*   Module:  _lseek.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements lseek()                                       */
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
/*       lseek: Reposition read/write file offset                      */
/*                                                                     */
/*      Inputs: fid = descriptor of file to be repositioned            */
/*              offset = file offset                                   */
/*              whence = offset reference position                     */
/*                                                                     */
/*     Returns: The new offset on success, -1 on failure               */
/*                                                                     */
/***********************************************************************/
off_t lseek(int fid, off_t offset, int whence)
{
  off_t rc;
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
  if (Files[fid].flags & FCB_DIR)
  {
    set_errno(EISDIR);
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
    set_errno(EBADF);
    semPost(FileSysSem);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  file->acquire(file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Call file system specific FSEEK routine.                          */
  /*-------------------------------------------------------------------*/
  rc = (off_t)file->ioctl(file, FSEEK, (long)offset, whence);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to file systems and return result.       */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ | F_WRITE);
  semPost(FileSysSem);
  return rc;
}

