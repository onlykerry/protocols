/***********************************************************************/
/*                                                                     */
/*   Module:  _vsprintf.c                                              */
/*   Release: 2004.5                                                   */
/*   Version: 2003.5                                                   */
/*   Purpose: Implements vsprintf()                                    */
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
/*    vsprintf: Print to a string                                      */
/*                                                                     */
/*      Inputs: s = pointer to string                                  */
/*              format = string to be printed                          */
/*              ap = list of arguments                                 */
/*                                                                     */
/*     Returns: number of characters written or -1 if error            */
/*                                                                     */
/***********************************************************************/
int vsprintf(char *s, const char *format, va_list ap)
{
  FILE stream;
  int count;

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
  /* Get parameters from vsprintf() and pass them to _process.         */
  /*-------------------------------------------------------------------*/
  count = ProcessPrintf(&stream, format, ap);

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

