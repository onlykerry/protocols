/***********************************************************************/
/*                                                                     */
/*   Module:  _unmount.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2004.0                                                   */
/*   Purpose: Implements unmount function for the file systems         */
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
/*     unmount: Unmount the named volume                               */
/*                                                                     */
/*       Input: path = name of volume to be unmounted                  */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int unmount(const char *path)
{
  FileSys *fsys;
  int rv = -1;
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
  /* Strip leading '\' or '/'.                                         */
  /*-------------------------------------------------------------------*/
  if (*path == '/' || *path == '\\')
    ++path;

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
  /* Acquire exclusive access to file system.                          */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Walk through list of mounted volumes looking for named volume.    */
  /*-------------------------------------------------------------------*/
  for (fsys = MountedList.head;; fsys = fsys->next)
  {
    /*-----------------------------------------------------------------*/
    /* If the volume was not found, return error.                      */
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
  /* Call file system specific unmount function.                       */
  /*-------------------------------------------------------------------*/
  rv = (int)fsys->ioctl(NULL, UNMOUNT, fsys->volume, TRUE);

  /*-------------------------------------------------------------------*/
  /* Take the volume off the mounted list.                             */
  /*-------------------------------------------------------------------*/
  if (fsys->prev)
    fsys->prev->next = fsys->next;
  else
    MountedList.head = fsys->next;

  if (fsys->next)
    fsys->next->prev = fsys->prev;
  else
    MountedList.tail = fsys->prev;
  fsys->prev = fsys->next = NULL;

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to upper file system and return.         */
  /*-------------------------------------------------------------------*/
end:
  semPost(FileSysSem);
  return rv;
}

