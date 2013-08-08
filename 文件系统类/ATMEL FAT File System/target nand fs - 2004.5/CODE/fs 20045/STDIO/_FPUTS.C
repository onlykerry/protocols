/***********************************************************************/
/*                                                                     */
/*   Module:  _fputs.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements fputs()                                       */
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
/*       fputs: Write string to stream                                 */
/*                                                                     */
/*      Inputs: s = string to be written to stream                     */
/*              file = stream to read from                             */
/*                                                                     */
/*     Returns: non-negative value if successful, EOF otherwise        */
/*                                                                     */
/***********************************************************************/
int fputs(const char *s, FILE *file)
{
  ui32 want;
  int sent;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* Check for valid stream.                                           */
  /*-------------------------------------------------------------------*/
  if (InvalidStream(file))
  {
    set_errno(EBADF);
    return EOF;
  }

  /*-------------------------------------------------------------------*/
  /* Check for valid output pointer.                                   */
  /*-------------------------------------------------------------------*/
  if (s == NULL)
  {
    file->errcode = EINVAL;
    set_errno(EINVAL);
    return EOF;
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
    file->errcode = EBADF;
    set_errno(EBADF);
    file->release(file, F_WRITE);
    return EOF;
  }

  /*-------------------------------------------------------------------*/
  /* Determine strength length and request write of entire string.     */
  /*-------------------------------------------------------------------*/
  want = strlen(s);
  sent = file->write(file, (ui8 *)s, want);

  /*-------------------------------------------------------------------*/
  /* Release exclusive write access to stream.                         */
  /*-------------------------------------------------------------------*/
  file->release(file, F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Return EOF unless entire string was written.                      */
  /*-------------------------------------------------------------------*/
  return (want == sent) ? sent : EOF;
}

