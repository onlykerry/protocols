/***********************************************************************/
/*                                                                     */
/*   Module:  _remove.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2001.0                                                   */
/*   Purpose: Implements the remove function for stdio.h               */
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
/* Function Prototypes                                                 */
/***********************************************************************/
int unlink(const char *path);

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*      remove: Remove a file from a directory                         */
/*                                                                     */
/*       Input: filename = pointer to filename to delete               */
/*                                                                     */
/*     Returns: zero on success and nonzero on failure                 */
/*                                                                     */
/***********************************************************************/
int remove(const char *filename)
{
  return unlink(filename);
}

