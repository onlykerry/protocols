/***********************************************************************/
/*                                                                     */
/*   Module:  _isatty.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements isatty()                                      */
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
#include "posixfsp.h"

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*      isatty: Determine if file descriptor is associated with a TTY  */
/*                                                                     */
/*       Input: fid = file descriptor to test                          */
/*                                                                     */
/*     Returns: 1 if fildes refers to a terminal and 0 if it does not  */
/*                                                                     */
/***********************************************************************/
int isatty(int fid)
{
  /*-------------------------------------------------------------------*/
  /* Check the flags entry for this file control block to find out.    */
  /*-------------------------------------------------------------------*/
  if (fid >= 0 && fid < FOPEN_MAX && (Files[fid].flags & FCB_TTY))
    return 1;
  else
  {
    set_errno(ENOTTY);
    return 0;
  }
}

