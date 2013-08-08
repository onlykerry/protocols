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

///////////////////////////////////////////////////////////////////////////////

typedef signed int     INT;
typedef unsigned int   UINT;
//typedef signed short   SHORT;        // Signed   16 bit quantity
//typedef unsigned short USHORT;       // Unsigned 16 bit quantity

#ifndef FAR
#define FAR
#endif
#ifndef NEAR
#define NEAR
#endif

#include "ix86p.h"
#include "ucos.h"
//#define HAVE_ANSI_TIME

/*-------- ANSI time.h ------------*/
/*
#if HAVE_ANSI_TIME==0
#define _TIME_H_
struct tm
{ 
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
    int tm_gmtoff;
    char *tm_zone;
};

int gettime(struct tm * time);  // placeholder for standard ANSI C gettime
ULONG time(ULONG *);            // placeholder for standard ANSI C time
int clk_stat();
#endif
 */

////////////////////////////////////////////////////////////////////////////////
// Common utility macros
//
#define hiword(x)       ((unsigned short)((x) >> 16))
#define loword(x)       ((unsigned short)(x))
#define hibyte(x)       (((x) >> 8) & 0xff)
#define lobyte(x)       ((x) & 0xff)
#define hinibble(x)     (((x) >> 4) & 0xf)
#define lonibble(x)     ((x) & 0xf)
#define dim(x)          (sizeof(x) / sizeof(x[0]))


/* 
 * Return the minimum and maximum of two values.  Not recommended for function
 * expressions.
 */
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* XXX These should be the function call equivalents. */
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#define min(a,b)    (((a) < (b)) ? (a) : (b))


#define B0 0x01
#define B1 0x02
#define B2 0x04
#define B3 0x08
#define B4 0x10
#define B5 0x20
#define B6 0x40
#define B7 0x80

#define TICKSPERSEC 18

#endif /* _TARGET_H_ */

/* ---- end of target.h ----------------------------------------------------- */
