/***********************************************************************/
/*                                                                     */
/*   Module:  _utime.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2002.1                                                   */
/*   Purpose: Implements utime()                                       */
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
/*       utime: Set file access and modification times                 */
/*                                                                     */
/*      Inputs: path  = pointer to the file to update                  */
/*              times = pointer to structure with the new times        */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int utime(const char *path, const struct utimbuf *times)
{
  void *ptr;
  int rc = -1;
  DIR dir;
#if !_PATH_NO_TRUNC
  char trunc_path[PATH_MAX + 1];
#endif

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
  /* Acquire exclusive access to file system.                          */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Ensure that path is valid.                                        */
  /*-------------------------------------------------------------------*/
  ptr = FSearch(&dir, &path, PARENT_DIR);
  if (ptr == NULL)
    goto end;

  /*-------------------------------------------------------------------*/
  /* If path too long, return error if no truncation, else truncate.   */
  /*-------------------------------------------------------------------*/
  if (strlen(path) > PATH_MAX)
  {
#if _PATH_NO_TRUNC
    set_errno(ENAMETOOLONG);
    goto end;
#else
    strncpy(trunc_path, path, PATH_MAX);
    trunc_path[PATH_MAX] = '\0';
    path = trunc_path;
#endif
  }

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  dir.acquire(&dir, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Call file system specific UTIME routine.                          */
  /*-------------------------------------------------------------------*/
  rc = (int)dir.ioctl(&dir, UTIME, path, times, ptr);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to file systems and return result.       */
  /*-------------------------------------------------------------------*/
  dir.release(&dir, F_READ | F_WRITE);
end:
  semPost(FileSysSem);
  return rc;
}

