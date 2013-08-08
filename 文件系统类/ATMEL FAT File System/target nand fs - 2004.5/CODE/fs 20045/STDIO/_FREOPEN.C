/***********************************************************************/
/*                                                                     */
/*   Module:  _freopen.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2002.1                                                   */
/*   Purpose: Implements the freopen function for stdio.h              */
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
/*     freopen: Close the open file associated with the specified file */
/*              handle and use the file handle to open the named file  */
/*                                                                     */
/*      Inputs: filename = name of stream to open                      */
/*              mode = mode in which to open file                      */
/*              file = ptr to file control block to close and re-use   */
/*                                                                     */
/*     Returns: handle of opened file if successful, or NULL           */
/*                                                                     */
/***********************************************************************/
FILE *freopen(const char *filename, const char *mode, FILE *file)
{
  void *dir;
  FILE *rv = NULL;
#if !_PATH_NO_TRUNC
  char trunc_filename[PATH_MAX + 1];
#endif

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* If file handle is invalid, return NULL.                           */
  /*-------------------------------------------------------------------*/
  if (InvalidStream(file))
  {
    set_errno(EBADF);
    return NULL;
  }

  /*-------------------------------------------------------------------*/
  /* If file name is NULL, return NULL.                                */
  /*-------------------------------------------------------------------*/
  if (filename == NULL)
  {
    file->errcode = EINVAL;
    set_errno(EINVAL);
    return NULL;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Get exclusive access to upper file system.                        */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Flush and close the file, acquiring and releasing volume access.  */
  /*-------------------------------------------------------------------*/
  file->acquire(file, F_READ | F_WRITE);
  file->ioctl(file, FFLUSH);
  file->ioctl(file, FCLOSE);
  file->release(file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Verify path of new file. Error exit if path is invalid.           */
  /*-------------------------------------------------------------------*/
  dir = FSearch(file, &filename, PARENT_DIR);
  if (dir == NULL)
    goto end;

  /*-------------------------------------------------------------------*/
  /* If path too long, return error if no truncation, else truncate.   */
  /*-------------------------------------------------------------------*/
  if (strlen(filename) > PATH_MAX)
  {
#if _PATH_NO_TRUNC
    set_errno(ENAMETOOLONG);
    goto end;
#else
    strncpy(trunc_filename, filename, PATH_MAX);
    trunc_filename[PATH_MAX] = '\0';
    filename = trunc_filename;
#endif
  }

  /*-------------------------------------------------------------------*/
  /* Open the file, acquiring and releasing volume access.             */
  /*-------------------------------------------------------------------*/
  file->acquire(file, F_READ | F_WRITE);
  rv = file->ioctl(file, FOPEN, filename, mode, dir);
  file->release(file, F_READ | F_WRITE);

end:
  /*-------------------------------------------------------------------*/
  /* Free control block if error. Release access to upper file system. */
  /*-------------------------------------------------------------------*/
  if (rv == NULL)
    file->ioctl = NULL;
  semPost(FileSysSem);
  return rv;
}

