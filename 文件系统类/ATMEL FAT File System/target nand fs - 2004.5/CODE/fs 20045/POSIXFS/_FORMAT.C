/***********************************************************************/
/*                                                                     */
/*   Module:  _format.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2004.0                                                   */
/*   Purpose: Implements format()                                      */
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
/*      format: Format a file system volume                            */
/*                                                                     */
/*       Input: name = name of volume to be formated                   */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int format(const char *name)
{
  int r_value;
  Module fp;
  int i;
#if !_PATH_NO_TRUNC
  char trunc_name[PATH_MAX + 1];
#endif

#if OS_PARM_CHECK
  /*-------------------------------------------------------------------*/
  /* If name is NULL, return -1.                                       */
  /*-------------------------------------------------------------------*/
  if (name == NULL)
  {
    set_errno(EFAULT);
    return -1;
  }
#endif

  /*-------------------------------------------------------------------*/
  /* Strip leading '\' or '/'.                                         */
  /*-------------------------------------------------------------------*/
  if (*name == '/' || *name == '\\')
    ++name;

  /*-------------------------------------------------------------------*/
  /* If name too long, return error if no truncation, else truncate.   */
  /*-------------------------------------------------------------------*/
  if (strlen(name) > PATH_MAX)
  {
#if _PATH_NO_TRUNC
    set_errno(ENAMETOOLONG);
    return -1;
#else
    strncpy(trunc_name, name, PATH_MAX);
    trunc_name[PATH_MAX] = '\0';
    name = trunc_name;
#endif
  }

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to file system.                          */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Search module list for matching volume name.                      */
  /*-------------------------------------------------------------------*/
  for (i = 0;; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* Select module to search.                                        */
    /*-----------------------------------------------------------------*/
    fp = ModuleList[i];

    /*-----------------------------------------------------------------*/
    /* If end of module list reached, set errno and return -1.         */
    /*-----------------------------------------------------------------*/
    if (fp == NULL)
    {
      set_errno(ENOENT);
      semPost(FileSysSem);
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* If volume found, break. No errors if return value is 1.         */
    /*-----------------------------------------------------------------*/
    r_value = (int)fp(kFormat, name);
    if (r_value)
    {
      if (r_value == 1)
        r_value = 0;
      break;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to file system and return.               */
  /*-------------------------------------------------------------------*/
  semPost(FileSysSem);
  return r_value;
}

