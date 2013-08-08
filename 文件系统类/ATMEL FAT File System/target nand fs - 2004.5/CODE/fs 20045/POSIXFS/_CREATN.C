/***********************************************************************/
/*                                                                     */
/*   Module:  _creatn.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2004.0                                                   */
/*   Purpose: Implements creatn()                                      */
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
/*      creatn: Create new file of given size filled with 0xFF bytes   */
/*                                                                     */
/*      Inputs: path = pointer to path of file to be created           */
/*              mode = permission bits for the new file                */
/*              size = file size in bytes (rounded up to multiples of  */
/*                     the sector size)                                */
/*                                                                     */
/*        Note: Writes to a file created by this call always happen in */
/*              place.                                                 */
/*                                                                     */
/*     Returns: File descriptor, -1 on failure                         */
/*                                                                     */
/***********************************************************************/
int creatn(const char *path, mode_t mode, size_t size)
{
  int rv = -1;
  void *dir;
  FILE *file;
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
  /* Acquire exclusive access to upper file system.                    */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Find a free file control block and initialize it.                 */
  /*-------------------------------------------------------------------*/
  for (file = &Files[0]; file->ioctl; ++file)
  {
    /*-----------------------------------------------------------------*/
    /* If none are free, return error.                                 */
    /*-----------------------------------------------------------------*/
    if (file == &Files[FOPEN_MAX - 1])
    {
      set_errno(EMFILE);
      semPost(FileSysSem);
      return -1;
    }
  }
  FsInitFCB(file, FCB_FILE);

  /*-------------------------------------------------------------------*/
  /* Ensure path is valid.                                             */
  /*-------------------------------------------------------------------*/
  dir = FSearch(file, &path, PARENT_DIR);
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
  file->acquire(file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Call file system specific CREATN routine.                         */
  /*-------------------------------------------------------------------*/
  rv = (int)file->ioctl(file, CREATN, path, mode, size, dir);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Free control block if error, else set return value.               */
  /*-------------------------------------------------------------------*/
end:
  if (rv == -1)
    file->ioctl = NULL;
  else
    rv = file - &Files[0];

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to upper file system and return result.  */
  /*-------------------------------------------------------------------*/
  semPost(FileSysSem);
  return rv;
}

