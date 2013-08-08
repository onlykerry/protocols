/***********************************************************************/
/*                                                                     */
/*   Module:  _fflush.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements the fflush function for stdio.h               */
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
/*      fflush: Flush either a specified stream or all open streams    */
/*                                                                     */
/*       Input: file = pointer to file control block or NULL           */
/*                                                                     */
/*     Returns: zero on success, EOF on failure                        */
/*                                                                     */
/***********************************************************************/
int fflush(FILE *file)
{
  int rv = 0;

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to upper file system.                    */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* If file is not NULL, flush just the specified file.               */
  /*-------------------------------------------------------------------*/
  if (file)
  {
#if OS_PARM_CHECK
    /*-----------------------------------------------------------------*/
    /* Return error if file handle is invalid.                         */
    /*-----------------------------------------------------------------*/
    if (InvalidStream(file))
    {
      set_errno(EBADF);
      semPost(FileSysSem);
      return EOF;
    }
#endif

    /*-----------------------------------------------------------------*/
    /* Return error if file is closed.                                 */
    /*-----------------------------------------------------------------*/
    if (file->ioctl == NULL)
    {
      set_errno(EBADF);
      semPost(FileSysSem);
      return EOF;
    }

    /*-----------------------------------------------------------------*/
    /* Acquire exclusive access to stream.                             */
    /*-----------------------------------------------------------------*/
    file->acquire(file, F_WRITE);

    /*-----------------------------------------------------------------*/
    /* Call file system specific FFLUSH routine.                       */
    /*-----------------------------------------------------------------*/
    rv = (int)file->ioctl(file, FFLUSH);

    /*-----------------------------------------------------------------*/
    /* Release exclusive access to stream.                             */
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
          rv = EOF;

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

