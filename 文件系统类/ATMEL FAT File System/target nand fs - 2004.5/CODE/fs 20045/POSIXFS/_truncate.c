/***********************************************************************/
/*                                                                     */
/*   Module:  _truncate.c                                              */
/*   Release: 2004.5                                                   */
/*   Version: 2002.3                                                   */
/*   Purpose: Implements truncate()                                    */
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
/*    truncate: Truncate the file size to the specified value          */
/*                                                                     */
/*      Inputs: path = pointer to the file to truncate                 */
/*              length = desired file size                             */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int truncate(const char *path, off_t length)
{
  void *dir;
  int rc = -1;
  FILE file;
#if !_PATH_NO_TRUNC
  char trunc_path[PATH_MAX + 1];
#endif

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* Check that path is a valid parameter.                             */
  /*-------------------------------------------------------------------*/
  if (path == NULL)
  {
    set_errno(EFAULT);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Ensure length parameter is valid.                                 */
  /*-------------------------------------------------------------------*/
  if (length < 0)
  {
    set_errno(EINVAL);
    return -1;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Initialize temporary file control block.                          */
  /*-------------------------------------------------------------------*/
  FsInitFCB(&file, FCB_FILE);

  /*-------------------------------------------------------------------*/
  /* Get exclusive access to the file system.                          */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Ensure that path is valid.                                        */
  /*-------------------------------------------------------------------*/
  dir = FSearch(&file, &path, PARENT_DIR);
  if (dir == NULL)
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
  file.acquire(&file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Call file system specific TRUNCATE routine.                       */
  /*-------------------------------------------------------------------*/
  rc = (int)file.ioctl(&file, TRUNCATE, path, length, dir);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to file systems and return result.       */
  /*-------------------------------------------------------------------*/
  file.release(&file, F_READ | F_WRITE);
end:
  semPost(FileSysSem);
  return rc;
}

