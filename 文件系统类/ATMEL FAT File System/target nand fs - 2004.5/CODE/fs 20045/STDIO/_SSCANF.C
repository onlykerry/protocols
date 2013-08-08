/***********************************************************************/
/*                                                                     */
/*   Module:  _sscanf.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2003.5                                                   */
/*   Purpose: Implements the sscanf function for stdio.h               */
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
/*      sscanf: Read from an input string                              */
/*                                                                     */
/*      Inputs: s = input string (from which chars are read in)        */
/*              format = string control the read                       */
/*                                                                     */
/*     Returns: number of input assignments made or EOF if error       */
/*                                                                     */
/***********************************************************************/
int sscanf(const char *s, const char *format, ...)
{
  va_list list_of_args;
  int count;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* Check for valid input pointer.                                    */
  /*-------------------------------------------------------------------*/
  if (s == NULL)
  {
    set_errno(EFAULT);
    return EOF;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Pass all the sscanf() parameters to _xscanf().                    */
  /*-------------------------------------------------------------------*/
  va_start(list_of_args, format);
  count = ProcessScanf(NULL, format, list_of_args, s);
  va_end(list_of_args);

  /*-------------------------------------------------------------------*/
  /* Return either EOF or number of input assignments made.            */
  /*-------------------------------------------------------------------*/
  return count;
}

