/***********************************************************************/
/*                                                                     */
/*   Module:  _fopen.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2004.0                                                   */
/*   Purpose: Implements the fopen function for stdio.h                */
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
#include "stdiop.h"

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*       fopen: Open a named file                                      */
/*                                                                     */
/*      Inputs: filename = name of stream to open                      */
/*              mode = mode in which to open file                      */
/*                                                                     */
/*     Returns: handle of opened file if successful, or NULL           */
/*                                                                     */
/***********************************************************************/
FILE *fopen(const char *filename, const char *mode)
{
  void *ptr = NULL;
  FILE *file;
#if !_PATH_NO_TRUNC
  char trunc_path[PATH_MAX + 1];
#endif

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* If file name or mode is NULL, return NULL.                        */
  /*-------------------------------------------------------------------*/
  if (filename == NULL || mode == NULL)
  {
    set_errno(EINVAL);
    return NULL;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Acquire file system semaphore.                                    */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Find a free file control block and initialize it.                 */
  /*-------------------------------------------------------------------*/
  for (file = &Files[0]; file->ioctl; ++file)
  {
    /*-----------------------------------------------------------------*/
    /* If no FILE was found, set error number and go to error exit.    */
    /*-----------------------------------------------------------------*/
    if (file == &Files[FOPEN_MAX - 1])
    {
      set_errno(EMFILE);
      goto end;
    }
  }
  FsInitFCB(file, FCB_DIR);

  /*-------------------------------------------------------------------*/
  /* Verify stream path. Error exit if path is invalid.                */
  /*-------------------------------------------------------------------*/
  ptr = FSearch(file, &filename, PARENT_DIR);
  if (ptr == NULL)
  {
    file->ioctl = NULL;
    goto end;
  }

  /*-------------------------------------------------------------------*/
  /* If path too long, return error if no truncation, else truncate.   */
  /*-------------------------------------------------------------------*/
  if (strlen(filename) > PATH_MAX)
  {
#if _PATH_NO_TRUNC
    set_errno(ENAMETOOLONG);
    file->ioctl = NULL;
    goto end;
#else
    strncpy(trunc_path, filename, PATH_MAX);
    trunc_path[PATH_MAX] = '\0';
    filename = trunc_path;
#endif
  }

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  file->acquire(file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Call file system specific FOPEN routine.                          */
  /*-------------------------------------------------------------------*/
  ptr = file->ioctl(file, FOPEN, filename, mode, ptr);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Free control block if error, else set return value.               */
  /*-------------------------------------------------------------------*/
  if (ptr == NULL)
    file->ioctl = NULL;
  else
    ptr = file;

  /*-------------------------------------------------------------------*/
  /* Release semaphore and return result.                              */
  /*-------------------------------------------------------------------*/
end:
  semPost(FileSysSem);
  return ptr;
}

