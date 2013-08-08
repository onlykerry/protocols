/***********************************************************************/
/*                                                                     */
/*   Module:  _fgetc.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements the fgetc function for stdio.h                */
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
/*       fgetc: Read one char from specified stream                    */
/*                                                                     */
/*       Input: file = pointer to file control block                   */
/*                                                                     */
/*     Returns: character read or EOF if error occurs                  */
/*                                                                     */
/***********************************************************************/
int fgetc(FILE *file)
{
  ui8 ch;
  int rv;

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
  /* Read either pushed-back character or new input character.         */
  /*-------------------------------------------------------------------*/
  if (file->hold_char)
  {
    rv = file->hold_char;
    file->hold_char = 0;
  }
  else
  {
    if (file->read(file, &ch, 1) != 1)
      rv = EOF;
    else
      rv = ch;
  }

  /*-------------------------------------------------------------------*/
  /* Release exclusive read access to stream and return result.        */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ);
  return rv;
}

