/***********************************************************************/
/*                                                                     */
/*   Module:  _opendir.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements opendir()                                     */
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
/*     opendir: Open a directory stream                                */
/*                                                                     */
/*       Input: dirname = pointer to the name of directory to read     */
/*                                                                     */
/*     Returns: Pointer to directory, or NULL if an error occurred     */
/*                                                                     */
/***********************************************************************/
DIR *opendir(const char *dirname)
{
  DIR *dir;
  void *ptr;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* If path is NULL, return NULL.                                     */
  /*-------------------------------------------------------------------*/
  if (dirname == NULL)
  {
    set_errno(ENOTDIR);
    return NULL;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to upper file system.                    */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Find a free file control block and initialize it.                 */
  /*-------------------------------------------------------------------*/
  for (dir = &Files[0]; dir->ioctl; ++dir)
  {
    /*-----------------------------------------------------------------*/
    /* If no DIR was found, return error.                              */
    /*-----------------------------------------------------------------*/
    if (dir == &Files[DIROPEN_MAX - 1])
    {
      set_errno(ENOMEM);
      semPost(FileSysSem);
      return NULL;
    }
  }
  FsInitFCB(dir, FCB_DIR);

  /*-------------------------------------------------------------------*/
  /* Ensure that path is valid and assign ioctl callback function.     */
  /*-------------------------------------------------------------------*/
  ptr = FSearch(dir, &dirname, ACTUAL_DIR);
  if (ptr == NULL)
  {
    dir->ioctl = NULL;
    semPost(FileSysSem);
    return NULL;
  }

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  dir->acquire(dir, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Call file system specific OPENDIR routine.                        */
  /*-------------------------------------------------------------------*/
  ptr = dir->ioctl(dir, OPENDIR, ptr);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  dir->release(dir, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Release file control block if an error occurred.                  */
  /*-------------------------------------------------------------------*/
  if (ptr == NULL)
    dir->ioctl = NULL;

  /*-------------------------------------------------------------------*/
  /* Release upper file system access and return result.               */
  /*-------------------------------------------------------------------*/
  semPost(FileSysSem);
  return ptr;
}

