/***********************************************************************/
/*                                                                     */
/*   Module:  _fscanf.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2003.5                                                   */
/*   Purpose: Implements the fscanf function for stdio.h               */
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
/*      fscanf: Read formatted input from stream                       */
/*                                                                     */
/*      Inputs: file = pointer to file control block                   */
/*              format = string control the read                       */
/*                                                                     */
/*     Returns: number of input assignments made or EOF if error       */
/*                                                                     */
/***********************************************************************/
int fscanf(FILE *file, const char *format, ...)
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
    return EOF;
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
    return EOF;
  }

  /*-------------------------------------------------------------------*/
  /* Get the fscanf() parameters and pass them to _xscanf().           */
  /*-------------------------------------------------------------------*/
  va_start(list_of_args, format);
  count = ProcessScanf(file, format, list_of_args, NULL);
  va_end(list_of_args);

  /*-------------------------------------------------------------------*/
  /* Release exclusive read access to stream.                          */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ);

  /*-------------------------------------------------------------------*/
  /* Return either EOF or number of input assignments made.            */
  /*-------------------------------------------------------------------*/
  return count;
}

