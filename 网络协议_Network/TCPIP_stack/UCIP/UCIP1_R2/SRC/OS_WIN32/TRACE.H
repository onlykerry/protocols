/*****************************************************************************
* trace.h - System debug diagnostic trace macros.
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
#ifndef __TRACE_H__
#define __TRACE_H__

#include "..\netdebug.h"

#ifdef _DEBUG

//=============================================================================
//  BreakPoint() macro.
//=============================================================================

#ifdef _X86_
#define BreakPoint()        _asm { int 3h }
#else
#define BreakPoint()        _DebugBreak()
#endif

//=============================================================================
//  MACRO: ASSERT()
//=============================================================================

#define ASSERT(exp)                                 \
{                                                   \
    if ( !(exp) )                                   \
    {                                               \
        Assert(#exp, __FILE__, __LINE__, NULL);   \
        BreakPoint();                               \
    }                                               \
}                                                   \

#define ASSERTMSG(exp, msg)                         \
{                                                   \
    if ( !(exp) )                                   \
    {                                               \
        Assert(#exp, __FILE__, __LINE__, msg);    \
        BreakPoint();                               \
    }                                               \
}                                                   \

//=============================================================================
//  MACRO: TRACE()
//=============================================================================

void _DebugBreak(void);
// void Assert(FAR void* assert, FAR char* file, int line, FAR void* msg);
// void Trace(FAR char* lpszFormat, ...);
void Assert(void* assert, char* file, int line, void* msg);
void Trace(char* lpszFormat, ...);
void Trace1(int code, char* lpszFormat, ...);
void Trace2(int code1, TraceModule module, char* lpszFormat, ...);

#define TRACE  Trace
//#define trace  Trace1

#else   // _DEBUG

#define ASSERT(exp)
#define ASSERTMSG(exp, msg)


void Assert(FAR void* assert, FAR char* file, int line, FAR void* msg);
void Trace(FAR char* lpszFormat, ...);
void Trace1(int code, char* lpszFormat, ...);
void Trace2(int code1, TraceModule module, char* lpszFormat, ...);

#define TRACE 1 ? (void)0 : Trace


#endif // !_DEBUG


#endif // __TRACE_H__
/////////////////////////////////////////////////////////////////////////////





