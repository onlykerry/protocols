/***********************************************************************/
/*                                                                     */
/*   Module:  _sortdir.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2000.0                                                   */
/*   Purpose: Implements sortdir()                                     */
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
/*     sortdir: Sort a directory stream                                */
/*                                                                     */
/*      Inputs: path = pointer to the name of directory to sort        */
/*              cmp = sorting function pointer                         */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
int sortdir(const char *path, int (*cmp) (const DirEntry *e1,
                                          const DirEntry *e2))
{
  DIR dir;
  int r_value = -1;
  void *ptr;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* If path is NULL, return error.                                    */
  /*-------------------------------------------------------------------*/
  if (path == NULL)
  {
    set_errno(ENOTDIR);
    return -1;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Initialize temporary file control block.                          */
  /*-------------------------------------------------------------------*/
  FsInitFCB(&dir, FCB_DIR);

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to upper file system.                    */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Ensure that path is valid.                                        */
  /*-------------------------------------------------------------------*/
  ptr = FSearch(&dir, &path, ACTUAL_DIR);
  if (ptr == NULL)
    goto end;

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to lower file system.                    */
  /*-------------------------------------------------------------------*/
  dir.acquire(&dir, F_READ | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* Call file system specific SORTDIR routine.                        */
  /*-------------------------------------------------------------------*/
  r_value = (int)dir.ioctl(&dir, SORTDIR, ptr, cmp);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to file systems and return result.       */
  /*-------------------------------------------------------------------*/
  dir.release(&dir, F_READ | F_WRITE);
end:
  semPost(FileSysSem);
  return r_value;
}

