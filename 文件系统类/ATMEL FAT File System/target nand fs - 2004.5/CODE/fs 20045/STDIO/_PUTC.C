/***********************************************************************/
/*                                                                     */
/*   Module:  _putc.c                                                  */
/*   Release: 2004.5                                                   */
/*   Version: 99.0                                                     */
/*   Purpose: Implements putc()                                        */
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
/*        putc: Read one char from specified stream                    */
/*                                                                     */
/*      Inputs: c = character to be placed on the stream               */
/*              stream = pointer to file control block                 */
/*                                                                     */
/*     Returns: character written to stream or -1 if error             */
/*                                                                     */
/***********************************************************************/
int (putc)(int c, FILE *stream)
{
  return fputc(c, stream);
}

