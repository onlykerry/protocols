/***********************************************************************/
/*                                                                     */
/*   Module:  _readdir.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements readdir()                                     */
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
/*     readdir: Return pointer to next directory entry                 */
/*                                                                     */
/*       Input: dir = pointer returned by opendir()                    */
/*                                                                     */
/*     Returns: A struct dirent pointer, or NULL if error occurred     */
/*                                                                     */
/***********************************************************************/
struct dirent *readdir(DIR *dir)
{
  struct dirent *rv;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* If the file descriptor is invalid, return error.                  */
  /*-------------------------------------------------------------------*/
  if (dir < &Files[0] || dir >= &Files[DIROPEN_MAX] ||
      !(dir->flags & FCB_DIR))
  {
    set_errno(EBADF);
    return NULL;
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
    return NULL;
  }

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  dir->acquire(dir, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Call file system specific READDIR routine.                        */
  /*-------------------------------------------------------------------*/
  rv = dir->ioctl(dir, READDIR);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to file systems and return result.       */
  /*-------------------------------------------------------------------*/
  dir->release(dir, F_READ | F_WRITE);
  semPost(FileSysSem);
  return rv;
}

