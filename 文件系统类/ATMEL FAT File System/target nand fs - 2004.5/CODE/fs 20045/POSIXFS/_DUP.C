/***********************************************************************/
/*                                                                     */
/*   Module:  _dup.c                                                   */
/*   Release: 2004.5                                                   */
/*   Version: 2000.5                                                   */
/*   Purpose: Implements dup()                                         */
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
/*         dup: Duplicate an open file descriptor                      */
/*                                                                     */
/*       Input: fid = file descriptor to duplicate                     */
/*                                                                     */
/*     Returns: Descriptor refering to same file as fid or -1 if error */
/*                                                                     */
/***********************************************************************/
int dup(int fid)
{
  return fcntl(fid, F_DUPFD, 0);
}

