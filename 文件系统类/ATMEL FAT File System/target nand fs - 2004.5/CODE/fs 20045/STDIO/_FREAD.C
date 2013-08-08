/***********************************************************************/
/*                                                                     */
/*   Module:  _fread.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2004.1                                                   */
/*   Purpose: Implements the fread function for stdio.h                */
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
/*       fread: Read an array from file                                */
/*                                                                     */
/*      Inputs: ptr = pointer to array to read into                    */
/*              size = size of one array member                        */
/*              nmemb = number of array members to read                */
/*              file = pointer to file to read from                    */
/*                                                                     */
/*     Returns: number of elements successfully read                   */
/*                                                                     */
/***********************************************************************/
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *file)
{
  int rdcnt = 0, hchar = 0;
  size_t request = nmemb * size;

  /*-------------------------------------------------------------------*/
  /* If the element count or size is zero, return zero.                */
  /*-------------------------------------------------------------------*/
  if (request == 0 || size == 0)
    return 0;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* Check for valid stream.                                           */
  /*-------------------------------------------------------------------*/
  if (InvalidStream(file))
  {
    set_errno(EBADF);
    return 0;
  }

  /*-------------------------------------------------------------------*/
  /* Ensure buffer pointer is valid.                                   */
  /*-------------------------------------------------------------------*/
  if (ptr == NULL)
  {
    file->errcode = EINVAL;
    set_errno(EINVAL);
    return 0;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive read access to stream.                          */
  /*-------------------------------------------------------------------*/
  file->acquire(file, F_READ);

  /*-------------------------------------------------------------------*/
  /* Return error if file is closed.                                   */
  /*-------------------------------------------------------------------*/
  if (file->ioctl == NULL)
  {
    set_errno(EBADF);
    file->release(file, F_READ);
    return 0;
  }

  /*-------------------------------------------------------------------*/
  /* If it is available, read pushed-back character first.             */
  /*-------------------------------------------------------------------*/
  if (file->hold_char)
  {
    ui8 *cp = ptr;

    *cp = (ui8)file->hold_char;
    ptr = cp + 1;
    file->hold_char = 0;
    hchar = 1;
    --request;
  }

  /*-------------------------------------------------------------------*/
  /* Check if there are more characters to read.                       */
  /*-------------------------------------------------------------------*/
  if (request)
  {
    /*-----------------------------------------------------------------*/
    /* Pass read request to file system or device driver.              */
    /*-----------------------------------------------------------------*/
    rdcnt = file->read(file, ptr, request);

    /*-----------------------------------------------------------------*/
    /* Read error is disregarded iff pushed-back character was read.   */
    /*-----------------------------------------------------------------*/
    if (rdcnt == -1)
    {
      rdcnt = 0;
      if (hchar == 0)
        file->errcode = get_errno();
    }
  }

  /*-------------------------------------------------------------------*/
  /* Release exclusive read access to stream.                          */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ);

  /*-------------------------------------------------------------------*/
  /* Return number of elements successfully read or EOF.               */
  /*-------------------------------------------------------------------*/
  return (rdcnt + hchar) / size;
}

