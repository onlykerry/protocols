/***********************************************************************/
/*                                                                     */
/*   Module:  _vclean.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2004.0                                                   */
/*   Purpose: Implements vclean()                                      */
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
/*      vclean: Reclaim free space on a mounted volume                 */
/*                                                                     */
/*       Input: path = volume name                                     */
/*                                                                     */
/*     Returns: -1 if error, 1 if un-reclaimed space remains, else 0   */
/*                                                                     */
/***********************************************************************/
int vclean(const char *path)
{
  int rv;
  FileSys *fsys;
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
    /* If the volume is not mounted, break to return status.           */
    /*-----------------------------------------------------------------*/
    if (fsys == NULL)
    {
      set_errno(ENOENT);
      rv = -1;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* If volume found, call file system specific VSTAT routine.       */
    /*-----------------------------------------------------------------*/
    if (!strcmp(fsys->name, path))
    {
      rv = (int)fsys->ioctl(NULL, VCLEAN, fsys->volume);
      break;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to upper file system and return status.  */
  /*-------------------------------------------------------------------*/
  semPost(FileSysSem);
  return rv;
}

