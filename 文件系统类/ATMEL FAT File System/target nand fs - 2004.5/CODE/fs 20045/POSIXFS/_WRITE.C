/***********************************************************************/
/*                                                                     */
/*   Module:  _write.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2003.0                                                   */
/*   Purpose: Implements write()                                       */
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
/*       write: Write to a file                                        */
/*                                                                     */
/*      Inputs: fid = descriptor of file open for writing              */
/*              buf = pointer to data to be written                    */
/*              nbytes = number of bytes to write                      */
/*                                                                     */
/*     Returns: Number of bytes written or -1 to indicate an error     */
/*                                                                     */
/***********************************************************************/
int write(int fid, const void *buf, unsigned int nbytes)
{
  int written;
  FILE *file;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* Ensure file descriptor is valid.                                  */
  /*-------------------------------------------------------------------*/
  if (fid < 0 || fid >= FOPEN_MAX)
  {
    set_errno(EBADF);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Return error if buffer pointer is invalid.                        */
  /*-------------------------------------------------------------------*/
  if (buf == NULL)
  {
    Files[fid].errcode = EFAULT;
    set_errno(EFAULT);
    return -1;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive write access to stream.                         */
  /*-------------------------------------------------------------------*/
  file = &Files[fid];
  file->acquire(file, F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Return error if file is closed.                                   */
  /*-------------------------------------------------------------------*/
  if (file->ioctl == NULL)
  {
    set_errno(EBADF);
    file->release(file, F_WRITE);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Call file system specific write routine.                          */
  /*-------------------------------------------------------------------*/
  written = file->write(file, buf, nbytes);

  /*-------------------------------------------------------------------*/
  /* Set file's errno if less than requested number was written.       */
  /*-------------------------------------------------------------------*/
  if (written < (int)nbytes)
  {
    file->errcode = get_errno();
    if (written == 0)
      written = -1;
  }

  /*-------------------------------------------------------------------*/
  /* Release exclusive write access to stream.                         */
  /*-------------------------------------------------------------------*/
  file->release(file, F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Return -1 or actual number of bytes read.                         */
  /*-------------------------------------------------------------------*/
  return written;
}

