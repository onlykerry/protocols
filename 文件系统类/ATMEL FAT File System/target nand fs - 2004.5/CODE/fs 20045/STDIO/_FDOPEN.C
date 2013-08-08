/***********************************************************************/
/*                                                                     */
/*   Module:  _fdopen.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2001.3                                                   */
/*   Purpose: Implements fdopen()                                      */
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
/*      fdopen: Associate a stream with a file descriptor              */
/*                                                                     */
/*      Inputs: fid = file descriptor                                  */
/*              type = ignored                                         */
/*                                                                     */
/*     Returns: File handle on success, NULL on error                  */
/*                                                                     */
/***********************************************************************/
FILE *fdopen(int fid, const char *type)
{
  /*-------------------------------------------------------------------*/
  /* If it's a valid file descriptor, return FILE*, else return NULL.  */
  /*-------------------------------------------------------------------*/
  if ((fid >= 0) && (fid < FOPEN_MAX) && Files[fid].ioctl)
    return &Files[fid];
  else
    return NULL;
}

