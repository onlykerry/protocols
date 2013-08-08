/***********************************************************************/
/*                                                                     */
/*   Module:  _tmpnam.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements the tmpnam function for stdio.h               */
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
/*     TmpName: Create a unique filename                               */
/*                                                                     */
/*       Input: name = NULL or buffer where the name is to be stored   */
/*                                                                     */
/*     Returns: Pointer to new filename                                */
/*        Note: If a unique name can not be generated, the name        */
/*              created most recently is repeated.                     */
/*                                                                     */
/***********************************************************************/
char *TmpName(char *name)
{
  int i;
  FileSys *fsys;
  static int count;
  static char tmp_name[L_tmpnam];

  /*-------------------------------------------------------------------*/
  /* If s is NULL, build filename in tmp_name.                         */
  /*-------------------------------------------------------------------*/
  if (name == NULL)
    name = tmp_name;

  /*-------------------------------------------------------------------*/
  /* Try up to TMP_MAX number of times to build a unique name.         */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < TMP_MAX; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* Form the name as 'tempXXXX', where XXXX is the count.           */
    /*-----------------------------------------------------------------*/
    sprintf(name, "temp%04u", count);

    /*-----------------------------------------------------------------*/
    /* Increment counter used to generate unique names.                */
    /*-----------------------------------------------------------------*/
    if (++count == TMP_MAX)
      count = 0;

    /*-----------------------------------------------------------------*/
    /* In every installed file system, check if this name exists.      */
    /*-----------------------------------------------------------------*/
    for (fsys = MountedList.head;; fsys = fsys->next)
    {
      /*---------------------------------------------------------------*/
      /* If every file system has been searched, accept this name.     */
      /*---------------------------------------------------------------*/
      if (fsys == NULL)
        return name;

      /*---------------------------------------------------------------*/
      /* If a match is found, break to build another name.             */
      /*---------------------------------------------------------------*/
      if (fsys->ioctl(NULL, TMPNAM, name))
        break;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Failed to find unique name, return pointer to last name tried.    */
  /*-------------------------------------------------------------------*/
  return name;
}

/***********************************************************************/
/*      tmpnam: Create a unique filename                               */
/*                                                                     */
/*       Input: name = NULL or buffer where the name is to be stored   */
/*                                                                     */
/*     Returns: Pointer to new filename                                */
/*        Note: If a unique name can not be generated, the name        */
/*              created most recently is repeated.                     */
/*                                                                     */
/***********************************************************************/
char *tmpnam(char *name)
{
  char *s;

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to file system.                          */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Call TmpName() to get a unique file name.                         */
  /*-------------------------------------------------------------------*/
  s = TmpName(name);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to file system and return.               */
  /*-------------------------------------------------------------------*/
  semPost(FileSysSem);
  return s;
}

