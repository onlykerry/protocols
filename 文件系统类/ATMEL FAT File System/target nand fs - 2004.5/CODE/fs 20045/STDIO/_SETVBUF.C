/***********************************************************************/
/*                                                                     */
/*   Module:  _setvbuf.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements the setvbuf function for stdio.h              */
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
/*     setvbuf: Set the buffering mode for stream                      */
/*                                                                     */
/*      Inputs: file = pointer to file control block                   */
/*              buf  = stream buffer                                   */
/*              mode = what kind of buffering                          */
/*              size = size of buffer                                  */
/*                                                                     */
/*     Returns: zero on success, -1 on failure                         */
/*                                                                     */
/***********************************************************************/
int setvbuf(FILE *file, char *buf, int mode, size_t size)
{
  int rc;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* Check for valid stream.                                           */
  /*-------------------------------------------------------------------*/
  if (InvalidStream(file))
  {
    set_errno(EBADF);
    return -1;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to stream.                               */
  /*-------------------------------------------------------------------*/
  file->acquire(file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Return error if stream is closed.                                 */
  /*-------------------------------------------------------------------*/
  if (file->ioctl == NULL)
  {
    set_errno(EBADF);
    file->release(file, F_READ | F_WRITE);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Call file system specific SETVBUF routine.                        */
  /*-------------------------------------------------------------------*/
  rc = (int)file->ioctl(file, SETVBUF, buf, mode, size);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to stream and return result.             */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ | F_WRITE);
  return rc;
}

