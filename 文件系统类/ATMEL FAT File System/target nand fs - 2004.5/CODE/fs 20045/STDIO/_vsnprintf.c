/***********************************************************************/
/*                                                                     */
/*   Module:  _vsnprintf.c                                             */
/*   Release: 2004.5                                                   */
/*   Version: 2003.5                                                   */
/*   Purpose: Implements vsnprintf()                                   */
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
/*    vsprintf: Print to a string up to n characters                   */
/*                                                                     */
/*      Inputs: s = pointer to string                                  */
/*              n = maximum number of characters to be printed         */
/*              format = string to be printed                          */
/*              ap = list of arguments                                 */
/*                                                                     */
/*     Returns: number of characters that were printed                 */
/*                                                                     */
/***********************************************************************/
int vsnprintf(char *s, size_t n, const char *format, va_list ap)
{
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
  /* Get parameters from vsprintf() and pass them to _process.         */
  /*-------------------------------------------------------------------*/
  ProcessPrintf(&stream, format, ap);

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

