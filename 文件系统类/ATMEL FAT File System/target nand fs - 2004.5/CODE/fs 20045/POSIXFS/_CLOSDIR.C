/***********************************************************************/
/*                                                                     */
/*   Module:  _closdir.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2003.0                                                   */
/*   Purpose: Implements closedir()                                    */
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
/*    closedir: Close the specified directory                          */
/*                                                                     */
/*       Input: dir = pointer returned by opendir()                    */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int closedir(DIR *dir)
{
  int rc;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* If the file descriptor is invalid, return error.                  */
  /*-------------------------------------------------------------------*/
  if (dir < &Files[0] || dir >= &Files[DIROPEN_MAX] ||
      ((dir->flags & FCB_DIR) != FCB_DIR))
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
  if (dir->ioctl == NULL)
  {
    set_errno(EBADF);
    semPost(FileSysSem);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  dir->acquire(dir, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Call file system specific CLOSEDIR routine.                       */
  /*-------------------------------------------------------------------*/
  rc = (int)dir->ioctl(dir, CLOSEDIR);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  dir->release(dir, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Re-initialize this file control block.                            */
  /*-------------------------------------------------------------------*/
  FsInitFCB(dir, FCB_FILE);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to upper file system and return result.  */
  /*-------------------------------------------------------------------*/
  semPost(FileSysSem);
  return rc;
}

