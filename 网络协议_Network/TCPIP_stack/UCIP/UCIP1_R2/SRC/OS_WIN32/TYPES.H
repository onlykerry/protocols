/*****************************************************************************
* types.h - System TYPE definitions header file
*
* portions Copyright (c) 2001 by Cognizant Pty Ltd.
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
* 2001-05-12 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Original file.
*
*****************************************************************************
*/
#ifndef _TYPES_H_
#define _TYPES_H_


//#include "typedefs.h"


short TaskGetLastError(void);


#ifndef LOBYTE
#define LOBYTE(w)   ((unsigned char)(w))
#endif

#ifndef HIBYTE
#define HIBYTE(w)   ((unsigned char)((unsigned short)(w) >> 8))
#endif

#ifndef NULL
#define NULL    ((void*)0)
#endif


#define BOOLEAN unsigned char
#define ULONG   unsigned long
//#define USHORT  unsigned short
#define UCHAR   unsigned char
#define WORD    short
#define UWORD   unsigned short
#define CHAR    char
#define BYTE    unsigned char
#define UBYTE   unsigned char
//#define INT     short
//#define UINT    unsigned short


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


#endif // _TYPES_H_
////////////////////////////////////////////////////////////////////////////////
