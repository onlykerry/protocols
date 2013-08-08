/***********************************************************************/
/*                                                                     */
/*   Module:  _getcwd.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2003.7                                                   */
/*   Purpose: Implements getcwd()                                      */
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
/*      getcwd: Get the current working directory                      */
/*                                                                     */
/*      Inputs: buf = pointer to a place to store the current dir      */
/*              size = size of the array pointed to by buf             */
/*                                                                     */
/*     Returns: buf on success and NULL on error                       */
/*                                                                     */
/***********************************************************************/
char *getcwd(char *buf, size_t size)
{
  char *rc;
  FsVolume *volume;
  ui32 dummy;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* If buffer size is invalid, return error.                          */
  /*-------------------------------------------------------------------*/
  if (buf && size == 0)
  {
    set_errno(EINVAL);
    return NULL;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Get exclusive access to upper file system.                        */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Call file system specific GETCWD routine.                         */
  /*-------------------------------------------------------------------*/
  FsReadCWD((ui32 *)&volume, &dummy);
  if (volume == NULL)
    rc = Files[ROOT_DIR_INDEX].ioctl(NULL, GETCWD, buf, size);
  else
    rc = volume->sys.ioctl(NULL, GETCWD, buf, size);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to upper file system and return result.  */
  /*-------------------------------------------------------------------*/
  semPost(FileSysSem);
  return rc;
}

