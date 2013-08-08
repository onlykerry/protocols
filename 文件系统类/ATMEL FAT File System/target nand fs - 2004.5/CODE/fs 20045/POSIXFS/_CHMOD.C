/***********************************************************************/
/*                                                                     */
/*   Module:  _chmod.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements chmod()                                       */
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
/*       chmod: Change file mode                                       */
/*                                                                     */
/*      Inputs: path = pointer to pathname of file to modify           */
/*              mode = new permission bits, S_ISUID and S_ISGID        */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int chmod(const char *path, mode_t mode)
{
  int rc = -1;
  FILE file;
  void *dir;
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
  FsInitFCB(&file, FCB_DIR);

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to upper file system.                    */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Ensure that path is valid.                                        */
  /*-------------------------------------------------------------------*/
  dir = FSearch(&file, &path, PARENT_DIR);
  if (dir == NULL)
    goto chmod_err;

  /*-------------------------------------------------------------------*/
  /* If path too long, error if no truncation allowed, else truncate.  */
  /*-------------------------------------------------------------------*/
  if (strlen(path) > PATH_MAX)
  {
#if _PATH_NO_TRUNC
    set_errno(ENAMETOOLONG);
    goto chmod_err;
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
  /* Call file system specific ioctl to change file mode.              */
  /*-------------------------------------------------------------------*/
  rc = (int)file.ioctl(&file, CHMOD, path, dir, mode);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  file.release(&file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to upper file system and return result.  */
  /*-------------------------------------------------------------------*/
chmod_err:
  semPost(FileSysSem);
  return rc;
}

