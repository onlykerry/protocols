/*****************************************************************************
* trace.c - System debug diagnostic trace macros.
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
#include <stdio.h> 
#include <stdarg.h>
#include "..\netconf.h"
#include "..\netdebug.h"

#ifdef _DEBUG



void _DebugBreak(void)
{
}

void Trace(FAR char* lpszFormat, ...)
{
    va_list args;
    int nBuf;
//    char szBuffer[512];
    va_start(args, lpszFormat);
    printf(lpszFormat, args);
//    nBuf = _vsnprintf(szBuffer, sizeof(szBuffer), lpszFormat, args);
//    OutputDebugString(szBuffer);
    // was there an error? was the expanded string too long?
//    ASSERT(nBuf >= 0);
    va_end(args);
}

void Trace1(int code, char* lpszFormat, ...)
{
}

void Trace2(int code1, TraceModule module, char* lpszFormat, ...)
{
}

void Assert(FAR void* assert, FAR char* file, int line, FAR void* msg)
{
    if (msg == NULL) {
        printf("ASSERT -- %s occured on line %u of file %s.\n",
               assert, line, file);
    } else {
        printf("ASSERT -- %s occured on line %u of file %s: Message = %s.\n",
               assert, line, file, msg);
    }
}


#else

//inline void Trace(FAR char* lpszFormat, ...) { };
void Trace(FAR char* lpszFormat, ...) { };
void Trace1(int code, char* lpszFormat, ...) { };
void Trace2(int code1, TraceModule module, char* lpszFormat, ...) { };
void Assert(void* assert, char* file, int line, void* msg) { };

#endif //_DEBUG
/////////////////////////////////////////////////////////////////////////////
