/***********************************************************************/
/*                                                                     */
/*   Module:  _setbuf.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements the setbuf function for stdio.h               */
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
/*      setbuf: Set the buffering mode for stream                      */
/*                                                                     */
/*      Inputs: file = pinter to file control block                    */
/*              buf = stream buffer                                    */
/*                                                                     */
/***********************************************************************/
void setbuf(FILE *file, char *buf)
{
  setvbuf(file, buf, _IOFBF, BUFSIZ);
}

