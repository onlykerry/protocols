/***********************************************************************/
/*                                                                     */
/*   Module:  _fputc.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements the fputc function for stdio.h                */
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
/*       fputc: Write character to stream                              */
/*                                                                     */
/*      Inputs: ch = character to write                                */
/*              file = pointer to file control block                   */
/*                                                                     */
/*     Returns: ch if successful, EOF otherwise                        */
/*                                                                     */
/***********************************************************************/
int fputc(int ch, FILE *file)
{
  int count;
  ui8 c = (ui8)ch;

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
  /* Acquire exclusive access to stream.                               */
  /*-------------------------------------------------------------------*/
  file->acquire(file, F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Return error if file is closed.                                   */
  /*-------------------------------------------------------------------*/
  if (file->ioctl == NULL)
  {
    set_errno(EBADF);
    file->release(file, F_WRITE);
    return EOF;
  }

  /*-------------------------------------------------------------------*/
  /* Write character to stream.                                        */
  /*-------------------------------------------------------------------*/
  count = file->write(file, &c, 1);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to stream and return result.             */
  /*-------------------------------------------------------------------*/
  file->release(file, F_WRITE);
  return (count == 1) ? ch : EOF;
}

