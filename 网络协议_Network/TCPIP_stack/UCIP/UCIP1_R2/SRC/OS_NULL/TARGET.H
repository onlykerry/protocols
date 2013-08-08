/*****************************************************************************
* target.h - 
*
* Copyright (c) 2001 by Cognizant Pty Ltd.
*
* The authors hereby grant permission to use, copy, modify, distribute,
* and license this software and its documentation for any purpose, provided
* that existing copyright notices are retained in all copies and that this
* notice and the following disclaimer are included verbatim in any 
* distributions. No written agreement, license, or royalty fee is required
* for any of the authorized uses.
*
* THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS *AS IS* AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
* IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
******************************************************************************
* REVISION HISTORY (please don't use tabs!)
*
*(yyyy-mm-dd)
* 2001-06-01 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Original file.
*
*****************************************************************************
*/
#ifndef _TARGET_H_
#define _TARGET_H_

#undef UBYTE
#undef BYTE
#undef UWORD
#undef WORD
#undef ULONG
#undef LONG

#define FALSE 0
#define TRUE  1

#define TICKSPERSEC 1024

///////////////////////////////////////////////////////////////////////////////
#pragma warning (push)
#pragma warning (disable: 4142) // benign redefinition of type
typedef unsigned char  UBYTE;        // Unsigned  8 bit quantity
typedef   signed char  BYTE;         // Signed    8 bit quantity
typedef unsigned short UWORD;        // Unsigned 16 bit quantity
typedef   signed short WORD;         // Signed   16 bit quantity
typedef unsigned long  ULONG;        // Unsigned 32 bit quantity
typedef   signed long  LONG;         // Signed   32 bit quantity
#pragma warning (pop)

typedef signed int     INT;
typedef unsigned int   UINT;
//typedef signed short   SHORT;        // Signed   16 bit quantity
//typedef unsigned short USHORT;       // Unsigned 16 bit quantity

#ifndef FAR
#define FAR
#endif
#define OS_FAR
#ifndef NEAR
#define NEAR
#endif
#include "os.h"


////////////////////////////////////////////////////////////////////////////////
// Common utility macros
//
#define hiword(x)		((unsigned short)((x) >> 16))
#define	loword(x)		((unsigned short)(x))
#define	hibyte(x)		(((x) >> 8) & 0xff)
#define	lobyte(x)		((x) & 0xff)
#define	hinibble(x)		(((x) >> 4) & 0xf)
#define	lonibble(x)		((x) & 0xf)
#define	dim(x)			(sizeof(x) / sizeof(x[0]))


/* 
 * Return the minimum and maximum of two values.  Not recommended for function
 * expressions.
 */
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* XXX These should be the function call equivalents. */
#define max(a,b)	(((a) > (b)) ? (a) : (b))
#define min(a,b)	(((a) < (b)) ? (a) : (b))

#endif

/* ---- end of target.h ----------------------------------------------------- */
