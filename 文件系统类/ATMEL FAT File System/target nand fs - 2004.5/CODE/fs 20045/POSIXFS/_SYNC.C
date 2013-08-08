/***********************************************************************/
/*                                                                     */
/*   Module:  _sync.c                                                  */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements the sync function for posix.h                 */
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
/*        sync: Flush either a specified stream or all open streams    */
/*                                                                     */
/*       Input: fid = valid file identifer or -1                       */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int sync(int fid)
{
  int rv = 0;
  FILE *file;

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to upper file system.                    */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* If fid is not -1, flush just the specified file.                  */
  /*-------------------------------------------------------------------*/
  if (fid != -1)
  {
    /*-----------------------------------------------------------------*/
    /* Return error if file descriptor is invalid.                     */
    /*-----------------------------------------------------------------*/
#if OS_PARM_CHECK
    if (fid < 0 || fid >= FOPEN_MAX)
    {
      set_errno(EBADF);
      semPost(FileSysSem);
      return -1;
    }
#endif
    file = &Files[fid];

    /*-----------------------------------------------------------------*/
    /* Return error if file is closed.                                 */
    /*-----------------------------------------------------------------*/
    if (file->ioctl == NULL)
    {
      set_errno(EBADF);
      semPost(FileSysSem);
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Acquire exclusive write access to lower file system.            */
    /*-----------------------------------------------------------------*/
    file->acquire(file, F_WRITE);

    /*-----------------------------------------------------------------*/
    /* Call file system specific FFLUSH routine.                       */
    /*-----------------------------------------------------------------*/
    rv = (int)file->ioctl(file, FFLUSH);

    /*-----------------------------------------------------------------*/
    /* Release exclusive write access to lower file system.            */
    /*-----------------------------------------------------------------*/
    file->release(file, F_WRITE);
  }

  /*-------------------------------------------------------------------*/
  /* Else flush all open files.                                        */
  /*-------------------------------------------------------------------*/
  else
  {
    /*-----------------------------------------------------------------*/
    /* Loop over every file.                                           */
    /*-----------------------------------------------------------------*/
    for (file = &Files[0]; file < &Files[FOPEN_MAX]; ++file)
    {
      /*---------------------------------------------------------------*/
      /* Check if file is open.                                        */
      /*---------------------------------------------------------------*/
      if (file->ioctl)
      {
        /*-------------------------------------------------------------*/
        /* Acquire exclusive write access to lower file system.        */
        /*-------------------------------------------------------------*/
        file->acquire(file, F_WRITE);

        /*-------------------------------------------------------------*/
        /* If any flush fails, set return value to -1.                 */
        /*-------------------------------------------------------------*/
        if (file->ioctl(file, FFLUSH))
          rv = -1;

        /*-------------------------------------------------------------*/
        /* Release exclusive write access to lower file system.        */
        /*-------------------------------------------------------------*/
        file->release(file, F_WRITE);
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to upper file system and return result.  */
  /*-------------------------------------------------------------------*/
  semPost(FileSysSem);
  return rv;
}

