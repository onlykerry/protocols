/***********************************************************************/
/*                                                                     */
/*   Module:  _pathcnf.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2000.5                                                   */
/*   Purpose: Implements pathconf()                                    */
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
/*    pathconf: Get configuration variable for a path                  */
/*                                                                     */
/*      Inputs: path = pointer to pathname of file                     */
/*              name = symbolic constant                               */
/*                                                                     */
/*     Returns: Variable value on success, -1 if no limit or failure   */
/*                                                                     */
/***********************************************************************/
long pathconf(const char *path, int name)
{
  int fid;
  long rv;

  /*-------------------------------------------------------------------*/
  /* Open the file in read only mode. If it fails, return error.       */
  /*-------------------------------------------------------------------*/
  fid = open(path, O_RDONLY, 0);
  if (fid == -1)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Call fpathconf on the open file to find the necessary info.       */
  /*-------------------------------------------------------------------*/
  rv = fpathconf(fid, name);
  close(fid);
  return rv;
}

