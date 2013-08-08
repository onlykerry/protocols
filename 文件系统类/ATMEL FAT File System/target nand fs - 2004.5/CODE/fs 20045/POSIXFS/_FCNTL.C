/***********************************************************************/
/*                                                                     */
/*   Module:  _fcntl.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2002.3                                                   */
/*   Purpose: Implements fcntl()                                       */
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
#include <stdarg.h>

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*       fcntl: Manipulate open file identifier                        */
/*                                                                     */
/*      Inputs: fid = file identifier                                  */
/*              cmd = command                                          */
/*              ... = additional command specific arguments            */
/*                                                                     */
/*     Returns: -1 on failure                                          */
/*                                                                     */
/***********************************************************************/
int fcntl(int fid, int cmd, ...)
{
  va_list ap;
  FILE *file;

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* If the file descriptor is invalid, return error.                  */
  /*-------------------------------------------------------------------*/
  if (fid < 0 || fid >= FOPEN_MAX)
  {
    set_errno(EBADF);
    return -1;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Get exclusive access to upper file system.                        */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Return error if file is closed.                                   */
  /*-------------------------------------------------------------------*/
  file = &Files[fid];
  if (file->ioctl == NULL)
  {
    semPost(FileSysSem);
    set_errno(EBADF);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Based on the command, execute the right instructions.             */
  /*-------------------------------------------------------------------*/
  switch (cmd)
  {
    case F_DUPFD:
    {
      /*---------------------------------------------------------------*/
      /* Use the va_arg mechanism to get the argument.                 */
      /*---------------------------------------------------------------*/
      va_start(ap, cmd);
      fid = va_arg(ap, int);
      va_end(ap);

#if OS_PARM_CHECK
      /*---------------------------------------------------------------*/
      /* If the file descriptor is invalid, return error.              */
      /*---------------------------------------------------------------*/
      if (fid < 0 || fid >= FOPEN_MAX)
      {
        semPost(FileSysSem);
        set_errno(EINVAL);
        return -1;
      }
#endif
      /*---------------------------------------------------------------*/
      /* Look for free file control block identifier >= fid.           */
      /*---------------------------------------------------------------*/
      for (;;)
      {
        FILE *file2 = &Files[fid];

        /*-------------------------------------------------------------*/
        /* Check if file control block is free.                        */
        /*-------------------------------------------------------------*/
        if (file2->ioctl == NULL)
        {
          /*-----------------------------------------------------------*/
          /* Copy previous file control block to new one.              */
          /*-----------------------------------------------------------*/
          *file2 = *file;

          /*-----------------------------------------------------------*/
          /* Acquire exclusive access to lower file system.            */
          /*-----------------------------------------------------------*/
          file2->acquire(file2, F_READ | F_WRITE);

          /*-----------------------------------------------------------*/
          /* Call file system specific DUP routine.                    */
          /*-----------------------------------------------------------*/
          file2->ioctl(file2, DUP);

          /*-----------------------------------------------------------*/
          /* Release exclusive access to lower file system.            */
          /*-----------------------------------------------------------*/
          file2->release(file2, F_READ | F_WRITE);
          break;
        }

        /*-------------------------------------------------------------*/
        /* If none are free, set errno and break.                      */
        /*-------------------------------------------------------------*/
        if (++fid >= FOPEN_MAX)
        {
          set_errno(EMFILE);
          fid = -1;
          break;
        }
      }

      /*---------------------------------------------------------------*/
      /* Release access to upper file system and return result.        */
      /*---------------------------------------------------------------*/
      semPost(FileSysSem);
      return fid;
    }

    case F_SETFL:
    {
      int oflag, r_val;

      /*---------------------------------------------------------------*/
      /* Acquire exclusive access to lower file system.                */
      /*---------------------------------------------------------------*/
      file->acquire(file, F_READ | F_WRITE);

      /*---------------------------------------------------------------*/
      /* Use the va_arg mechanism to get the argument.                 */
      /*---------------------------------------------------------------*/
      va_start(ap, cmd);
      oflag = va_arg(ap, int);
      va_end(ap);

      /*---------------------------------------------------------------*/
      /* Call specific ioctl to set flag.                              */
      /*---------------------------------------------------------------*/
      r_val = (int)file->ioctl(file, SET_FL, oflag);

      /*---------------------------------------------------------------*/
      /* Release exclusive access to lower file system.                */
      /*---------------------------------------------------------------*/
      file->release(file, F_READ | F_WRITE);

      /*---------------------------------------------------------------*/
      /* Release access to upper file system and return result.        */
      /*---------------------------------------------------------------*/
      semPost(FileSysSem);
      return r_val;
    }

    case F_GETFL:
    {
      int r_val;

      /*---------------------------------------------------------------*/
      /* Acquire exclusive access to lower file system.                */
      /*---------------------------------------------------------------*/
      file->acquire(file, F_READ);

      /*---------------------------------------------------------------*/
      /* Call specific ioctl to get flag.                              */
      /*---------------------------------------------------------------*/
      r_val = (int)file->ioctl(file, GET_FL);

      /*---------------------------------------------------------------*/
      /* Release exclusive access to lower file system.                */
      /*---------------------------------------------------------------*/
      file->release(file, F_READ);

      /*---------------------------------------------------------------*/
      /* Release access to upper file system and return result.        */
      /*---------------------------------------------------------------*/
      semPost(FileSysSem);
      return r_val;
    }
  }

  /*-------------------------------------------------------------------*/
  /* An unsupported command was requested.                             */
  /*-------------------------------------------------------------------*/
  set_errno(EINVAL);
  semPost(FileSysSem);
  return -1;
}

