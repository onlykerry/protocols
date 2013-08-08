/***********************************************************************/
/*                                                                     */
/*   Module:  targetos.h                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2004.1                                                   */
/*   Purpose: Centralizes definitions used throughout                  */
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
#ifndef TARGETOS_H
#define TARGETOS_H

#ifdef __cplusplus
extern "C"
{
#endif

/***********************************************************************/
/* TargetOS Configuration                                              */
/***********************************************************************/
/*
** Error message strings are included if TRUE
*/
#define OS_INC_ERR_STRINGS      TRUE

/*
** Parameter errors are detected if TRUE
*/
#define OS_PARM_CHECK   TRUE    /* TRUE or FALSE */

/***********************************************************************/
/* General Definitions                                                 */
/***********************************************************************/
#define i8      char
#define i16     short
#define i32     int

#define ui8     unsigned char
#define ui16    unsigned short
#define ui32    unsigned int

#define vi8     volatile char
#define vi16    volatile short
#define vi32    volatile int

#define vui8    volatile unsigned char
#define vui16   volatile unsigned short
#define vui32   volatile unsigned int

#define uint    unsigned int
#define ssize_t int
#define size_t  unsigned int

#define TRUE    1
#define FALSE   0
#define ESC     0x1B

#define REG_8(addr)     (*(volatile ui8  *)(addr))
#define REG16(addr)     (*(volatile ui16 *)(addr))
#define REG32(addr)     (*(volatile ui32 *)(addr))

#define min(a, b)       (((a) < (b)) ? (a) : (b))
#define max(a, b)       (((a) > (b)) ? (a) : (b))

#define SEQ_GE(a, b)    ((i32)((a) - (b)) >= 0)
#define SEQ_GT(a, b)    ((i32)((a) - (b)) > 0)
#define SEQ_LE(a, b)    ((i32)((a) - (b)) <= 0)
#define SEQ_LT(a, b)    ((i32)((a) - (b)) < 0)

#define wr16_le(value, addr)                    \
  *((ui8 *)(addr) + 0) = (ui8)(value >> 0);     \
  *((ui8 *)(addr) + 1) = (ui8)(value >> 8);

#define wr24_le(value, addr)                    \
  *((ui8 *)(addr) + 0) = (ui8)(value >> 0);     \
  *((ui8 *)(addr) + 1) = (ui8)(value >> 8);     \
  *((ui8 *)(addr) + 2) = (ui8)(value >> 16);

#define wr32_le(value, addr)                    \
  *((ui8 *)(addr) + 0) = (ui8)(value >> 0);     \
  *((ui8 *)(addr) + 1) = (ui8)(value >> 8);     \
  *((ui8 *)(addr) + 2) = (ui8)(value >> 16);    \
  *((ui8 *)(addr) + 3) = (ui8)(value >> 24);

#define wr16_be(value, addr)                    \
  *((ui8 *)(addr) + 0) = (ui8)(value >> 8);     \
  *((ui8 *)(addr) + 1) = (ui8)(value >> 0);

#define wr24_be(value, addr)                    \
  *((ui8 *)(addr) + 1) = (ui8)(value >> 16);    \
  *((ui8 *)(addr) + 2) = (ui8)(value >> 8);     \
  *((ui8 *)(addr) + 3) = (ui8)(value >> 0);

#define wr32_be(value, addr)                    \
  *((ui8 *)(addr) + 0) = (ui8)(value >> 24);    \
  *((ui8 *)(addr) + 1) = (ui8)(value >> 16);    \
  *((ui8 *)(addr) + 2) = (ui8)(value >> 8);     \
  *((ui8 *)(addr) + 3) = (ui8)(value >> 0);

#define EOF       (-1)

#ifdef __cplusplus
}
#endif

#endif /* TARGETOS_H */

