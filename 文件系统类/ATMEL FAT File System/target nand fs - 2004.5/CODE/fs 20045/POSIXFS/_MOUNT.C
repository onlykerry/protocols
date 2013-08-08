/***********************************************************************/
/*                                                                     */
/*   Module:  _mount.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2004.0                                                   */
/*   Purpose: Implements mount() function for the file systems         */
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
/*       mount: Mount the named file system                            */
/*                                                                     */
/*       Input: path = name of file system to be mounted               */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int mount(const char *path)
{
  Module fp;
  FileSys *fsys;
  int i, rv = -1;
  char *name;
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
  /* Get exclusive access to file system.                              */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* If there is a name duplication, return error.                     */
  /*-------------------------------------------------------------------*/
  for (fsys = MountedList.head; fsys; fsys = fsys->next)
    if (!strcmp(fsys->name, path))
    {
      set_errno(EEXIST);
      goto end;
    }

  /*-------------------------------------------------------------------*/
  /* Go through the module list and have the corresponding module      */
  /* perform the actual mount. The module should be able to recognize  */
  /* the name of the device to be mounted.                             */
  /*-------------------------------------------------------------------*/
  for (i = 0;; ++i)
  {
    fp = ModuleList[i];

    /*-----------------------------------------------------------------*/
    /* If device is not found, return error.                           */
    /*-----------------------------------------------------------------*/
    if (!fp)
    {
      set_errno(ENOENT);
      goto end;
    }

    /*-----------------------------------------------------------------*/
    /* If module finds the device, stop with error if mount fails,     */
    /* else stop successfully.                                         */
    /*-----------------------------------------------------------------*/
    fsys = fp(kMount, path, &name, FALSE);
    if (fsys == (FileSys *)-1)
      goto end;
    else if (fsys)
      break;
  }

  /*-------------------------------------------------------------------*/
  /* Add device to list of mounted devices.                            */
  /*-------------------------------------------------------------------*/
  if (MountedList.head)
  {
    fsys->prev = MountedList.tail;
    MountedList.tail->next = fsys;
  }
  else
  {
    fsys->prev = NULL;
    MountedList.head = fsys;
  }
  fsys->next = NULL;
  MountedList.tail = fsys;
  rv = 0;

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to upper file system and return.         */
  /*-------------------------------------------------------------------*/
end:
  semPost(FileSysSem);
  return rv;
}

