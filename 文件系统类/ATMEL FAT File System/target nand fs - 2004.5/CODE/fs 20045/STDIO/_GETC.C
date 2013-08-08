/***********************************************************************/
/*                                                                     */
/*   Module:  _getc.c                                                  */
/*   Release: 2004.5                                                   */
/*   Version: 99.0                                                     */
/*   Purpose: Implements getc()                                        */
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
/*        getc: Read one char from specified stream                    */
/*                                                                     */
/*       Input: stream = pointer to file control block                 */
/*                                                                     */
/*     Returns: character read from specified stream                   */
/*                                                                     */
/***********************************************************************/
int (getc)(FILE *stream)
{
  return fgetc(stream);
}

