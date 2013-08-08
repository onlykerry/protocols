/***********************************************************************/
/*                                                                     */
/*   Module:  _ungetc.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2002.1                                                   */
/*   Purpose: Implements the ungetc function for stdio.h               */
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
/*      ungetc: Push up to one char back onto specified stream         */
/*                                                                     */
/*      Inputs: ch = character to push back                            */
/*              file = pointer to file control block                   */
/*                                                                     */
/*     Returns: ch or EOF if error occurred                            */
/*                                                                     */
/***********************************************************************/
int ungetc(int ch, FILE *file)
{
#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* Check for valid stream.                                           */
  /*-------------------------------------------------------------------*/
  if (InvalidStream(file) || (file->flags & FCB_DIR))
  {
    set_errno(EBADF);
    return EOF;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to stream.                               */
  /*-------------------------------------------------------------------*/
  file->acquire(file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Return error if file is closed.                                   */
  /*-------------------------------------------------------------------*/
  if (file->ioctl == NULL)
  {
    set_errno(EBADF);
    file->release(file, F_READ | F_WRITE);
    return EOF;
  }

  /*-------------------------------------------------------------------*/
  /* Save in control block if not EOF or full.                         */
  /*-------------------------------------------------------------------*/
  if ((ch == EOF) || file->hold_char)
    ch = EOF;
  else
    file->hold_char = ch;

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to stream and return character or EOF.   */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ | F_WRITE);
  return ch;
}

