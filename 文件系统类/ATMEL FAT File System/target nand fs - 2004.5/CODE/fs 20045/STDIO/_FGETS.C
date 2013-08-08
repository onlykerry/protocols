/***********************************************************************/
/*                                                                     */
/*   Module:  _fgets.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements the fgets function for stdio.h                */
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
/*       fgets: Read chars from stream and store them in s             */
/*                                                                     */
/*      Inputs: buf = pointer to buffer to place string                */
/*              n = maximum number of chars to read                    */
/*              file = stream to read from                             */
/*                                                                     */
/*     Returns: initial input if any, or NULL if error                 */
/*                                                                     */
/***********************************************************************/
char *fgets(char *buf, int n, FILE *file)
{
  char *s = buf;
  ui8 c;

  /*-------------------------------------------------------------------*/
  /* Check for edge case.                                              */
  /*-------------------------------------------------------------------*/
  if (n < 2) return NULL;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* Check for valid stream.                                           */
  /*-------------------------------------------------------------------*/
  if (InvalidStream(file))
  {
    set_errno(EBADF);
    return NULL;
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
    return NULL;
  }

  /*-------------------------------------------------------------------*/
  /* Check for character that was previously pushed back.              */
  /*-------------------------------------------------------------------*/
  c = (ui8)file->hold_char;
  if (c)
  {
    *s++ = (char)c;
    file->hold_char = 0;
    --n;
  }

  /*-------------------------------------------------------------------*/
  /* Read until '\n', n == 1, or EOF.                                  */
  /*-------------------------------------------------------------------*/
  for (; (c != '\n') && (n > 1); --n)
  {
    if (file->read(file, (ui8 *)&c, 1) != 1)
      break;
    else
      *s++ = (char)c;
  }

  /*-------------------------------------------------------------------*/
  /* Release exclusive read access to stream.                          */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ);

  /*-------------------------------------------------------------------*/
  /* If characters read, terminate string and return original pointer. */
  /*-------------------------------------------------------------------*/
  if (s != buf)
  {
    *s = '\0';
    return buf;
  }

  /*-------------------------------------------------------------------*/
  /* Else return NULL.                                                 */
  /*-------------------------------------------------------------------*/
  else
    return NULL;
}

