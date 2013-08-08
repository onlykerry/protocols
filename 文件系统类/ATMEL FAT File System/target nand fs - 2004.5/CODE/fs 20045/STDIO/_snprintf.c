/***********************************************************************/
/*                                                                     */
/*   Module:  _snprintf.c                                              */
/*   Release: 2004.5                                                   */
/*   Version: 2003.5                                                   */
/*   Purpose: Implements the snprintf function for stdio.h             */
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
/*    snprintf: Print to a string up to n characters                   */
/*                                                                     */
/*      Inputs: s = pointer to string                                  */
/*              n = maximum number of characters to be printed         */
/*              format = string to be printed                          */
/*                                                                     */
/*     Returns: number of characters that were printed                 */
/*                                                                     */
/***********************************************************************/
int snprintf(char *s, size_t n, const char *format, ...)
{
  va_list list_of_args;
  FILE stream;
  int pos;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* Check for valid output pointer.                                   */
  /*-------------------------------------------------------------------*/
  if (s == NULL)
  {
    set_errno(EFAULT);
    return -1;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Set the stream for string writes.                                 */
  /*-------------------------------------------------------------------*/
  stream.write = StringWriteN;
  stream.pos = (void *)0;
  stream.handle = s;
  stream.volume = (void *)(n - 1);

  /*-------------------------------------------------------------------*/
  /* Pass all the sprintf() parameters to _process().                  */
  /*-------------------------------------------------------------------*/
  va_start(list_of_args, format);
  ProcessPrintf(&stream, format, list_of_args);
  va_end(list_of_args);

  /*-------------------------------------------------------------------*/
  /* Mark the end of the string.                                       */
  /*-------------------------------------------------------------------*/
  pos = (int)stream.pos;
  if (pos > 0)
    s[pos] = '\0';

  /*-------------------------------------------------------------------*/
  /* Return number of characters written to string.                    */
  /*-------------------------------------------------------------------*/
  return pos;
}

