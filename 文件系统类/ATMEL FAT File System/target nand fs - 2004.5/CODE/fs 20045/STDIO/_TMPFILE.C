/***********************************************************************/
/*                                                                     */
/*   Module:  _tmpfile.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2003.0                                                   */
/*   Purpose: Implements the tmpfile function for stdio.h              */
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
/*     tmpfile: Create a temporary file                                */
/*                                                                     */
/*     Returns: temporary file handle or NULL                          */
/*                                                                     */
/***********************************************************************/
FILE *tmpfile(void)
{
  FileSys *vol;
  FILE *rv = NULL;
  char *name;

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to file system.                          */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Obtain a name for the file.                                       */
  /*-------------------------------------------------------------------*/
  name = TmpName(NULL);

  /*-------------------------------------------------------------------*/
  /* Temporary files are created in RAM. See if RAM volume is mounted. */
  /*-------------------------------------------------------------------*/
  for (vol = MountedList.head;; vol = vol->next)
  {
    /*-----------------------------------------------------------------*/
    /* If RAM volume is not mounted, return error.                     */
    /*-----------------------------------------------------------------*/
    if (vol == NULL)
    {
      set_errno(EINVAL);
      goto end;
    }

    /*-----------------------------------------------------------------*/
    /* If RAM file system is mounted, stop.                            */
    /*-----------------------------------------------------------------*/
    rv = vol->ioctl(NULL, TMPFILE, vol->volume, name);
    if (rv != NULL)
      break;
  }

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to file system and return.               */
  /*-------------------------------------------------------------------*/
end:
  semPost(FileSysSem);
  if (rv == (void *)-1)
    return NULL;
  return rv;
}

