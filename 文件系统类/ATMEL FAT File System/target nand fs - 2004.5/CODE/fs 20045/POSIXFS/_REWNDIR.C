/***********************************************************************/
/*                                                                     */
/*   Module:  _rewndir.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements rewinddir()                                   */
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
/*   rewinddir: Reset the position associated with the dir stream      */
/*                                                                     */
/*       Input: dir = pointer returned by opendir()                    */
/*                                                                     */
/***********************************************************************/
void rewinddir(DIR *dir)
{
#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* If the directory handle is invalid, return.                       */
  /*-------------------------------------------------------------------*/
  if (dir < &Files[0] || dir >= &Files[DIROPEN_MAX] ||
      !(dir->flags & FCB_DIR))
  {
    set_errno(EBADF);
    return;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Get exclusive access to upper file system.                        */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Return if directory is closed.                                    */
  /*-------------------------------------------------------------------*/
  if (dir->ioctl == NULL)
  {
    set_errno(EBADF);
    semPost(FileSysSem);
    return;
  }

  /*-------------------------------------------------------------------*/
  /* Reset the handle position.                                        */
  /*-------------------------------------------------------------------*/
  dir->pos = NULL;

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to upper file system.                    */
  /*-------------------------------------------------------------------*/
  semPost(FileSysSem);
}

