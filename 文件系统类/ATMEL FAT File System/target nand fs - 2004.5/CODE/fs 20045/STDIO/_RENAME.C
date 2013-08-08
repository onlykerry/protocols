/***********************************************************************/
/*                                                                     */
/*   Module:  _rename.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2005.0                                                   */
/*   Purpose: Implements the rename function for stdio.h               */
/*                                                                     */
/*---------------------------------------------------------------------*/
/*                                                                     */
/*               Copyright 2005, Blunk Microsystems                    */
/*                      ALL RIGHTS RESERVED                            */
/*                                                                     */
/*   Licensees have the non-exclusive right to use, modify, or extract */
/*   this computer program for software development at a single site.  */
/*   This program may be resold or disseminated in executable format   */
/*   only. The source code may not be redistributed or resold.         */
/*                                                                     */
/***********************************************************************/
#include "stdiop.h"

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*      rename: Rename a file                                          */
/*                                                                     */
/*       Input: old_loc = pointer to path name of an existing file     */
/*              new_loc = pointer to a new path name for the file      */
/*                                                                     */
/*     Returns: zero on success and -1 on failure                      */
/*                                                                     */
/***********************************************************************/
int rename(const char *old_loc, const char *new_loc)
{
  void *old_dir, *new_dir;
  FILE tmp_old, tmp_new;
  int rc = -1, is_dir = FALSE;
  struct stat stat_buf;
#if !_PATH_NO_TRUNC
  char trunc_old_loc[PATH_MAX + 1];
  char trunc_new_loc[PATH_MAX + 1];
#endif

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* If either of the filenames is NULL, return -1.                    */
  /*-------------------------------------------------------------------*/
  if ((old_loc == NULL) || (new_loc == NULL))
  {
    set_errno(EFAULT);
    return -1;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Figure out whether we're dealing with files or directories.       */
  /*-------------------------------------------------------------------*/
  if (stat(old_loc, &stat_buf))
    return -1;
  else if (S_ISDIR(stat_buf.st_mode))
    is_dir = TRUE;

  /*-------------------------------------------------------------------*/
  /* If rename in place, return successs.                              */
  /*-------------------------------------------------------------------*/
  if (!strcmp(old_loc, new_loc))
    return 0;

  /*-------------------------------------------------------------------*/
  /* Set the tmp variables.                                            */
  /*-------------------------------------------------------------------*/
  tmp_old.handle = tmp_old.volume = NULL;
  tmp_new.handle = tmp_new.volume = NULL;

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to all file systems.                     */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Check that old path is valid.                                     */
  /*-------------------------------------------------------------------*/
  tmp_old.handle = tmp_old.volume = NULL;
  old_dir = FSearch(&tmp_old, &old_loc, PARENT_DIR);
  if (old_dir == NULL)
    goto rename_err;

  /*-------------------------------------------------------------------*/
  /* Check that new path is valid.                                     */
  /*-------------------------------------------------------------------*/
  new_dir = FSearch(&tmp_new, &new_loc, PARENT_DIR);
  if (new_dir == NULL)
    goto rename_err;

  /*-------------------------------------------------------------------*/
  /* If paths belong to different volumes, go to error exit.           */
  /*-------------------------------------------------------------------*/
  if (tmp_new.volume != tmp_old.volume)
  {
    set_errno(EXDEV);
    goto rename_err;
  }

  /*-------------------------------------------------------------------*/
  /* If no truncation, error exit if either path name is too long.     */
  /*-------------------------------------------------------------------*/
  if (strlen(old_loc) > PATH_MAX || strlen(new_loc) > PATH_MAX)
  {
#if _PATH_NO_TRUNC
    set_errno(ENAMETOOLONG);
    goto rename_err;
#else
    if (strlen(old_loc) > PATH_MAX)
    {
      strncpy(trunc_old_loc, old_loc, PATH_MAX);
      trunc_old_loc[PATH_MAX] = '\0';
      old_loc = trunc_old_loc;
    }
    if (strlen(new_loc) > PATH_MAX)
    {
      strncpy(trunc_new_loc, new_loc, PATH_MAX);
      trunc_new_loc[PATH_MAX] = '\0';
      new_loc = trunc_new_loc;
    }
#endif
  }

  /*-------------------------------------------------------------------*/
  /* Make rename request. If it fails, return error.                   */
  /*-------------------------------------------------------------------*/
  tmp_new.acquire(&tmp_new, F_READ | F_WRITE);
  rc = (int)tmp_new.ioctl(&tmp_new, RENAME, old_loc, new_loc, old_dir,
                          new_dir, is_dir);
  tmp_new.release(&tmp_new, F_READ | F_WRITE);

rename_err:
  /*-------------------------------------------------------------------*/
  /* Release exclusive access to all file systems.                     */
  /*-------------------------------------------------------------------*/
  semPost(FileSysSem);
  return rc;
}

