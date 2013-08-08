/***********************************************************************/
/*                                                                     */
/*   Module:  _open.c                                                  */
/*   Release: 2004.5                                                   */
/*   Version: 2004.0                                                   */
/*   Purpose: Implements open()                                        */
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
#include <stdarg.h>
#include "posixfsp.h"

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*        open: Open a file                                            */
/*                                                                     */
/*      Inputs: path  = pointer to path of the file to open            */
/*              oflag = symbolic flags                                 */
/*                                                                     */
/*     Returns: File descriptor on success, -1 on error                */
/*                                                                     */
/***********************************************************************/
int open(const char *path, int oflag, ...)
{
  int rv = -1;
  void *dir;
  va_list ap;
  mode_t mode = 0;
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
  FsInitFCB(file, FCB_DIR);

  /*-------------------------------------------------------------------*/
  /* Ensure path is valid.                                             */
  /*-------------------------------------------------------------------*/
  dir = FSearch(file, &path, PARENT_DIR);
  if (dir == NULL)
    goto end;

  /*-------------------------------------------------------------------*/
  /* Get file mode if creating file.                                   */
  /*-------------------------------------------------------------------*/
  if (oflag & O_CREAT)
  {
    va_start(ap, oflag);
    mode = (mode_t)va_arg(ap, int);
    va_end(ap);
  }

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
  /* Call file system specific OPEN routine.                           */
  /*-------------------------------------------------------------------*/
  rv = (int)file->ioctl(file, OPEN, path, oflag, mode, dir);

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

