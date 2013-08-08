/***********************************************************************/
/*                                                                     */
/*   Module:  _vfprintf.c                                              */
/*   Release: 2004.5                                                   */
/*   Version: 2003.5                                                   */
/*   Purpose: Implements vfprintf()                                    */
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
/*    vfprintf: Write to an open stream                                */
/*                                                                     */
/*      Inputs: file = pointer to file control block                   */
/*              format = string to be printed                          */
/*              ap = list of arguments                                 */
/*                                                                     */
/*     Returns: number of characters written or -1 if error            */
/*                                                                     */
/***********************************************************************/
int vfprintf(FILE *file, const char *format, va_list ap)
{
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
  /* Pass all the vprintf() parameters to _process().                  */
  /*-------------------------------------------------------------------*/
  count = ProcessPrintf(file, format, ap);

  /*-------------------------------------------------------------------*/
  /* Release exclusive write access to stream.                         */
  /*-------------------------------------------------------------------*/
  file->release(file, F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Return the number of characters written or -1 if error occurred.  */
  /*-------------------------------------------------------------------*/
  return count;
}

