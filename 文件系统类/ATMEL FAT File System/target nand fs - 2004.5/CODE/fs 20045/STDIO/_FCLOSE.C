/***********************************************************************/
/*                                                                     */
/*   Module:  _fclose.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2003.4                                                   */
/*   Purpose: Implements the fclose function for stdio.h               */
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
/*      fclose: Close an open stream                                   */
/*                                                                     */
/*       Input: file = pointer to file control block                   */
/*                                                                     */
/*     Returns: zero on success, EOF on failure                        */
/*                                                                     */
/***********************************************************************/
int fclose(FILE *file)
{
  int rc = 0;

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
  /* Get exclusive access to upper file system.                        */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Return error if file is closed.                                   */
  /*-------------------------------------------------------------------*/
  if (file->ioctl == NULL)
  {
    set_errno(EBADF);
    semPost(FileSysSem);
    return EOF;
  }

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  file->acquire(file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Close the selected file.                                          */
  /*-------------------------------------------------------------------*/
  if (file->ioctl(file, FCLOSE))
    rc = EOF;

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  file->release(file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Re-initialize this file control block.                            */
  /*-------------------------------------------------------------------*/
  FsInitFCB(file, FCB_FILE);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to upper file system and return result.  */
  /*-------------------------------------------------------------------*/
  semPost(FileSysSem);
  return rc;
}

