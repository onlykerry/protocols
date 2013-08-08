/***********************************************************************/
/*                                                                     */
/*   Module:  _link.c                                                  */
/*   Release: 2004.5                                                   */
/*   Version: 2004.0                                                   */
/*   Purpose: Implements link()                                        */
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
/*        link: Create a file link and increment the link count        */
/*                                                                     */
/*      Inputs: existing = pointer to pathname of existing file        */
/*              new_link = pointer to additional link to same file     */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int link(const char *existing, const char *new_link)
{
  void *ext_parent, *new_parent;
  int r_value = -1;
  DIR dir_ext, dir_new;
#if !_PATH_NO_TRUNC
  char trunc_existing[PATH_MAX + 1];
  char trunc_new_link[PATH_MAX + 1];
#endif

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* If either of the paths is NULL, return -1.                        */
  /*-------------------------------------------------------------------*/
  if (existing == NULL || new_link == NULL)
  {
    set_errno(EFAULT);
    return -1;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Initialize temporary file control blocks.                         */
  /*-------------------------------------------------------------------*/
  FsInitFCB(&dir_ext, FCB_DIR);
  FsInitFCB(&dir_new, FCB_DIR);

  /*-------------------------------------------------------------------*/
  /* Get exclusive access to upper file system.                        */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Ensure existing path is valid.                                    */
  /*-------------------------------------------------------------------*/
  ext_parent = FSearch(&dir_ext, &existing, PARENT_DIR);
  if (ext_parent == NULL)
    goto end;

  /*-------------------------------------------------------------------*/
  /* Ensure new path is valid.                                         */
  /*-------------------------------------------------------------------*/
  new_parent = FSearch(&dir_new, &new_link, PARENT_DIR);
  if (new_parent == NULL)
    goto end;

  /*-------------------------------------------------------------------*/
  /* If paths belong to different file systems, return error.          */
  /*-------------------------------------------------------------------*/
  if (dir_ext.ioctl != dir_new.ioctl)
  {
    set_errno(EXDEV);
    goto end;
  }

  /*-------------------------------------------------------------------*/
  /* If either path is too long, then either return error or truncate. */
  /*-------------------------------------------------------------------*/
  if (strlen(existing) > PATH_MAX || strlen(new_link) > PATH_MAX)
  {
#if _PATH_NO_TRUNC
    set_errno(ENAMETOOLONG);
    goto end;
#else
    if (strlen(existing) > PATH_MAX)
    {
      strncpy(trunc_existing, existing, PATH_MAX);
      trunc_existing[PATH_MAX] = '\0';
      existing = trunc_existing;
    }
    if (strlen(new_link) > PATH_MAX)
    {
      strncpy(trunc_new_link, new_link, PATH_MAX);
      trunc_new_link[PATH_MAX] = '\0';
      new_link = trunc_new_link;
    }
#endif
  }

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  dir_ext.acquire(&dir_ext, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Call file system specific LINK routine.                           */
  /*-------------------------------------------------------------------*/
  r_value = (int)dir_new.ioctl(&dir_new, LINK, existing, new_link,
                               ext_parent, new_parent);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to file systems and return result.       */
  /*-------------------------------------------------------------------*/
  dir_ext.release(&dir_ext, F_READ | F_WRITE);
end:
  semPost(FileSysSem);
  return r_value;
}

