/***********************************************************************/
/*                                                                     */
/*   Module:  _sprintf.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2003.5                                                   */
/*   Purpose: Implements the sprintf function for stdio.h              */
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
/*     sprintf: Print to a string                                      */
/*                                                                     */
/*      Inputs: s = pointer to string                                  */
/*              format = string to be printed                          */
/*                                                                     */
/*     Returns: number of characters that were printed or -1 if error  */
/*                                                                     */
/***********************************************************************/
int sprintf(char *s, const char *format, ...)
{
  va_list list_of_args;
  int count;
  FILE stream;

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
  stream.write = StringWrite;
  stream.pos = (void *)0;
  stream.handle = s;

  /*-------------------------------------------------------------------*/
  /* Pass all the sprintf() parameters to _process().                  */
  /*-------------------------------------------------------------------*/
  va_start(list_of_args, format);
  count = ProcessPrintf(&stream, format, list_of_args);
  va_end(list_of_args);

  /*-------------------------------------------------------------------*/
  /* If successful, mark the end of the string.                        */
  /*-------------------------------------------------------------------*/
  if (count > 0)
    s[count] = '\0';

  /*-------------------------------------------------------------------*/
  /* Return count of characters written to string or -1 if error.      */
  /*-------------------------------------------------------------------*/
  return count;
}

