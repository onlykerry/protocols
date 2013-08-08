/***********************************************************************/
/*                                                                     */
/*   Module:  _fileno.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements fileno()                                      */
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
/*      fileno: Map a stream pointer to a file descriptor              */
/*                                                                     */
/*       Input: file = stream pointer                                  */
/*                                                                     */
/*     Returns: File descriptor or -1 on error                         */
/*                                                                     */
/***********************************************************************/
int fileno(FILE *file)
{
  /*-------------------------------------------------------------------*/
  /* The file number is the index into Files[].                        */
  /*-------------------------------------------------------------------*/
  if (file && (file >= &Files[0]) && (file < &Files[FOPEN_MAX]) &&
      file->ioctl)
    return file - &Files[0];
  else
  {
    set_errno(EBADF);
    return -1;
  }
}

