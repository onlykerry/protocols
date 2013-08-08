/***********************************************************************/
/*                                                                     */
/*   Module:  _creat.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2000.5                                                   */
/*   Purpose: Implements creat()                                       */
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
/*       creat: Create a new file or rewrite an existing one           */
/*                                                                     */
/*      Inputs: path = pointer to path of file to be created           */
/*              mode = permission bits for the new file                */
/*                                                                     */
/*     Returns: File descriptor if successful, -1 on failure           */
/*                                                                     */
/***********************************************************************/
int creat(const char *path, mode_t mode)
{
  return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

