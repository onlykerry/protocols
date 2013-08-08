/***********************************************************************/
/*                                                                     */
/*   Module:  _dup2.c                                                  */
/*   Release: 2004.5                                                   */
/*   Version: 2002.1                                                   */
/*   Purpose: Implements dup2()                                        */
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
/*        dup2: If file specified by fid2 is open, close it. Duplicate */
/*              file specified by fid using fid2's control block.      */
/*                                                                     */
/*      Inputs: fid = identifier of open file to duplicate             */
/*              fid2 = file control block to (re)use                   */
/*                                                                     */
/*     Returns: fid2 if successful, -1 if error                        */
/*                                                                     */
/***********************************************************************/
int dup2(int fid, int fid2)
{
  FILE *file, *file2;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* Return error if either identifier is invalid.                     */
  /*-------------------------------------------------------------------*/
  if (fid < 0 || fid2 < 0 || fid >= FOPEN_MAX || fid2 >= FOPEN_MAX)
  {
    set_errno(EBADF);
    return -1;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to upper file system.                    */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Return error if first file is closed.                             */
  /*-------------------------------------------------------------------*/
  file = &Files[fid];
  if (file->ioctl == NULL)
  {
    semPost(FileSysSem);
    set_errno(EBADF);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* If identifiers match, just return the first identifier.           */
  /*-------------------------------------------------------------------*/
  if (fid == fid2)
  {
    semPost(FileSysSem);
    return fid;
  }

  /*-------------------------------------------------------------------*/
  /* If second file is open, flush and close it.                       */
  /*-------------------------------------------------------------------*/
  file2 = &Files[fid2];
  if (file2->ioctl)
  {
    file2->acquire(file2, F_READ | F_WRITE);
    file2->ioctl(file2, FFLUSH);
    file2->ioctl(file2, FCLOSE);
    file2->ioctl = NULL;
    file2->release(file2, F_READ | F_WRITE);
  }

  /*-------------------------------------------------------------------*/
  /* Duplicate first file and copy its file control block.             */
  /*-------------------------------------------------------------------*/
  file->acquire(file, F_READ | F_WRITE);
  file->ioctl(file, DUP);
  *file2 = *file;
  file->release(file, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to upper file system and return result.  */
  /*-------------------------------------------------------------------*/
  semPost(FileSysSem);
  return fid2;
}

