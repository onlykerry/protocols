/***********************************************************************/
/*                                                                     */
/*   Module:  stdiop.h                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2003.5                                                   */
/*   Purpose: Private stdio library declarations                       */
/*            for performing input and output                          */
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
#ifndef _STDIOP_H
#define _STDIOP_H

#include "../posix.h"
#include "../include/libc/ctype.h"
#include "../include/libc/errno.h"
#include "../include/libc/stdio.h"
#include "../include/libc/string.h"
#include "../include/fsprivate.h"

/***********************************************************************/
/* Macro Definitions                                                   */
/***********************************************************************/
#define PLUS_SIGN      1
#define SPACE_SIGN     2
#define DEFAULT_VALUE  0
#define l_VALUE        2
#define L_VALUE        4
#define h_VALUE        8
#define DEFAULT_FLAG   0
#define ZERO_FLAG      2
#define MINUS_FLAG     4
#define PLUS_FLAG      8
#define NUM_FLAG      16
#define SPACE_FLAG    32
#define P_SPEC        -1
#define S_SPEC        -2
#define DI_SPEC       -3
#define UO_SPEC       -4
#define X_SPEC        -8
#define SIGN_MASK     0x80000000
#define EXPONENT_MASK 0x7F800000
#define MANTISSA_MASK 0x007FFFFF
#define _PLUS          1
#define _MINUS         0

/*
** Check for stream validity
*/
#define InvalidStream(stream) (!stream || (stream < &Files[0]) || \
                               (stream > &Files[FOPEN_MAX - 1]))

/***********************************************************************/
/* Function prototypes                                                 */
/***********************************************************************/
int   ProcessPrintf(FILE *stream, const char *format, va_list args);
int   ProcessScanf(FILE *stream, const char *format, va_list args,
                   const char *s);
int   StringWrite(FILE *stream, const ui8 *buf, ui32 len);
int   StringWriteN(FILE *stream, const ui8 *buf, ui32 len);
char *TmpName(char *s);
#endif /* _STDIOP_H */

