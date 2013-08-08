/***********************************************************************/
/*                                                                     */
/*   Module:  _read.c                                                  */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements read()                                        */
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
/*        read: Read from a file                                       */
/*                                                                     */
/*      Inputs: fid = file descriptor open for reading                 */
/*              buf = pointer to place where data should be read       */
/*              nbytes = maximum number of bytes to read               */
/*                                                                     */
/*     Returns: Number of bytes actually read or -1 on error           */
/*                                                                     */
/***********************************************************************/
int read(int fid, void *buf, unsigned int nbytes)
{
  int hchar = 0, read = 0;
  FILE *file;

  /*-------------------------------------------------------------------*/
  /* If number of requested bytes is zero, return zero.                */
  /*-------------------------------------------------------------------*/
  if (nbytes == 0)
    return 0;

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
  /* Acquire exclusive read access to file.                            */
  /*-------------------------------------------------------------------*/
  file = &Files[fid];
  file->acquire(file, F_READ);

  /*-------------------------------------------------------------------*/
  /* Return error if file is closed.                                   */
  /*-------------------------------------------------------------------*/
  if (file->ioctl == NULL)
  {
    set_errno(EBADF);
    file->release(file, F_READ);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* If available, read pushed-back character first.                   */
  /*-------------------------------------------------------------------*/
  if (file->hold_char)
  {
    ui8 *cp = buf;

    *cp = (ui8)file->hold_char;
    buf = cp + 1;
    file->hold_char = 0;
    hchar = 1;
    --nbytes;
  }

  /*-------------------------------------------------------------------*/
  /* Check if there are more characters to read.                       */
  /*-------------------------------------------------------------------*/
  if (nbytes)
  {
    /*-----------------------------------------------------------------*/
    /* Pass read request to file system or device driver.              */
    /*-----------------------------------------------------------------*/
    read = file->read(file, buf, nbytes);

    /*-----------------------------------------------------------------*/
    /* Read error is disregarded iff pushed-back character was read.   */
    /*-----------------------------------------------------------------*/
    if (read == -1)
    {
      if (hchar)
        read = 0;
      else
        file->errcode = get_errno();
    }
  }

  /*-------------------------------------------------------------------*/
  /* Release exclusive read access to file.                            */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ);

  /*-------------------------------------------------------------------*/
  /* Return number of bytes successfully read or -1.                   */
  /*-------------------------------------------------------------------*/
  return read + hchar;
}

