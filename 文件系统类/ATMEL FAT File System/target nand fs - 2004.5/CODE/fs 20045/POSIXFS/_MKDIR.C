/***********************************************************************/
/*                                                                     */
/*   Module:  _mkdir.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2004.1                                                   */
/*   Purpose: Implements mkdir()                                       */
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
/*       mkdir: Create a new directory                                 */
/*                                                                     */
/*      Inputs: path = pointer to directory path                       */
/*              mode = directory permission bits                       */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int mkdir(const char *path, mode_t mode)
{
  void *parent;
  int r_value = -1;
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
  /* Get exclusive access to the file system.                          */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Ensure that path is valid.                                        */
  /*-------------------------------------------------------------------*/
  parent = FSearch(&dir, &path, PARENT_DIR);
  if (parent == NULL)
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
  /* Call file system specific MKDIR routine.                          */
  /*-------------------------------------------------------------------*/
  r_value = (int)dir.ioctl(&dir, MKDIR, path, mode, parent, 0, 0);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to file systems and return result.       */
  /*-------------------------------------------------------------------*/
  dir.release(&dir, F_READ | F_WRITE);
end:
  semPost(FileSysSem);
  return r_value;
}

