/***********************************************************************/
/*                                                                     */
/*   Module:  _fprintf.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2003.5                                                   */
/*   Purpose: Implements fprintf()                                     */
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
/*     fprintf: Write to an open stream                                */
/*                                                                     */
/*      Inputs: file = pointer to file control block                   */
/*              format = string to be printed                          */
/*                                                                     */
/*     Returns: number of characters written or -1 if error            */
/*                                                                     */
/***********************************************************************/
int fprintf(FILE *file, const char *format, ...)
{
  va_list list_of_args;
  int count;

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
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Get all the parameters for fprintf and pass them to _process.     */
  /*-------------------------------------------------------------------*/
  va_start(list_of_args, format);
  count = ProcessPrintf(file, format, list_of_args);
  va_end(list_of_args);

  /*-------------------------------------------------------------------*/
  /* Release exclusive stream write access and return chars written.   */
  /*-------------------------------------------------------------------*/
  file->release(file, F_WRITE);
  return count;
}

