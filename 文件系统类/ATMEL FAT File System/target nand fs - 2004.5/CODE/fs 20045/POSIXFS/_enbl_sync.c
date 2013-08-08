/***********************************************************************/
/*                                                                     */
/*   Module:  _enbl_sync.c                                             */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements enable_sync()                                 */
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
/* enable_sync: Enable user independent syncs on volume                */
/*                                                                     */
/*       Input: path = volume name                                     */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int enable_sync(const char *path)
{
  FileSys *fsys;
  int rv = -1;
#if !_PATH_NO_TRUNC
  char trunc_path[PATH_MAX + 1];
#endif

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* Check that both path and buf are valid parameters.                */
  /*-------------------------------------------------------------------*/
  if (path == NULL)
  {
    set_errno(EFAULT);
    return -1;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* If path too long, return error if no truncation, else truncate.   */
  /*-------------------------------------------------------------------*/
  if (strlen(path) > PATH_MAX)
  {
#if _PATH_NO_TRUNC
    set_errno(ENAMETOOLONG);
    return -1;
#else
    strncpy(trunc_path, path, PATH_MAX);
    trunc_path[PATH_MAX] = '\0';
    path = trunc_path;
#endif
  }

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to upper file system.                    */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Check that 'path' is the name of a mounted volume.                */
  /*-------------------------------------------------------------------*/
  for (fsys = MountedList.head;; fsys = fsys->next)
  {
    /*-----------------------------------------------------------------*/
    /* If the volume is not mounted, return error.                     */
    /*-----------------------------------------------------------------*/
    if (fsys == NULL)
    {
      set_errno(ENOENT);
      goto end;
    }

    /*-----------------------------------------------------------------*/
    /* If the volume was found, stop looking.                          */
    /*-----------------------------------------------------------------*/
    if (!strcmp(fsys->name, path))
      break;
  }

  /*-------------------------------------------------------------------*/
  /* Call file system specific ENABLE_SYNC routine.                    */
  /*-------------------------------------------------------------------*/
  rv = (int)fsys->ioctl(NULL, ENABLE_SYNC, fsys->volume);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to upper file system and return.         */
  /*-------------------------------------------------------------------*/
end:
  semPost(FileSysSem);
  return rv;
}

