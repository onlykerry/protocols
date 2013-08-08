/***********************************************************************/
/*                                                                     */
/*   Module:  _rmdir.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements rmdir()                                       */
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
/*       rmdir: If directory named by path is empty, it is removed     */
/*                                                                     */
/*       Input: path = pointer to the directory to remove              */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int rmdir(const char *path)
{
  void *ptr;
  int rv = -1;
  DIR dir;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* If path is NULL, return -1.                                       */
  /*-------------------------------------------------------------------*/
  if (path == NULL)
  {
    set_errno(EFAULT);
    return -1;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Initialize temporary file control block.                          */
  /*-------------------------------------------------------------------*/
  FsInitFCB(&dir, FCB_DIR);

  /*-------------------------------------------------------------------*/
  /* Get exclusive access to the file system.                          */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Ensure that path is valid.                                        */
  /*-------------------------------------------------------------------*/
  ptr = FSearch(&dir, &path, ACTUAL_DIR);
  if (ptr == NULL)
    goto end;

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  dir.acquire(&dir, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Call file system specific RMDIR routine.                          */
  /*-------------------------------------------------------------------*/
  rv = (int)dir.ioctl(&dir, RMDIR, ptr);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to file systems and return result.       */
  /*-------------------------------------------------------------------*/
  dir.release(&dir, F_READ | F_WRITE);
end:
  semPost(FileSysSem);
  return rv;
}

