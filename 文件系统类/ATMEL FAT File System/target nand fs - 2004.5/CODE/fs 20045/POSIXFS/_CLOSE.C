/***********************************************************************/
/*                                                                     */
/*   Module:  _close.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2003.4                                                   */
/*   Purpose: Implements close()                                       */
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
/*       close: Close an open file                                     */
/*                                                                     */
/*       Input: fid = descriptor of file to close                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int close(int fid)
{
  int rc = 0;
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
  /* Close the selected file.                                          */
  /*-------------------------------------------------------------------*/
  if (file->ioctl(file, FCLOSE))
    rc = EOF;

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Re-initialize this file control block.                            */
  /*-------------------------------------------------------------------*/
  FsInitFCB(file, FCB_FILE);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to upper file system and return result.  */
  /*-------------------------------------------------------------------*/
  semPost(FileSysSem);
  return rc;
}

