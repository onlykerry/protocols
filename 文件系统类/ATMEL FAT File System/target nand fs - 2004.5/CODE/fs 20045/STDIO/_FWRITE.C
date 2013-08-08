/***********************************************************************/
/*                                                                     */
/*   Module:  _fwrite.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2003.0                                                   */
/*   Purpose: Implements fwrite()                                      */
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
/*      fwrite: Write an array to a stream                             */
/*                                                                     */
/*      Inputs: ptr = pointer to array to write                        */
/*              size = size of one array member                        */
/*              nmemb = number of array members to read                */
/*              file = pointer to file to write into                   */
/*                                                                     */
/*     Returns: number of elements successfully written or EOF         */
/*                                                                     */
/***********************************************************************/
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *file)
{
  int written;
  size_t request = nmemb * size;

  /*-------------------------------------------------------------------*/
  /* If the element count or size is zero, return zero.                */
  /*-------------------------------------------------------------------*/
  if (nmemb == 0 || size == 0)
    return 0;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* Check for valid stream.                                           */
  /*-------------------------------------------------------------------*/
  if (InvalidStream(file))
  {
    set_errno(EBADF);
    return (size_t)EOF;
  }

  /*-------------------------------------------------------------------*/
  /* Ensure buffer pointer is valid.                                   */
  /*-------------------------------------------------------------------*/
  if (ptr == NULL)
  {
    file->errcode = EINVAL;
    set_errno(EINVAL);
    return (size_t)EOF;
  }

#endif

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive write access to stream.                         */
  /*-------------------------------------------------------------------*/
  file->acquire(file, F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Return error if file is closed.                                   */
  /*-------------------------------------------------------------------*/
  if (file->ioctl == NULL)
  {
    set_errno(EBADF);
    file->release(file, F_WRITE);
    return (size_t)EOF;
  }

  /*-------------------------------------------------------------------*/
  /* Pass write request to file system or device driver.               */
  /*-------------------------------------------------------------------*/
  written = file->write(file, ptr, request);

  /*-------------------------------------------------------------------*/
  /* Set stream's errno if less than requested number was written.     */
  /*-------------------------------------------------------------------*/
  if (written < request)
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
  /* Return number of elements successfully written or EOF.            */
  /*-------------------------------------------------------------------*/
  if (written == -1)
    return (size_t)EOF;
  else
    return written / size;
}

