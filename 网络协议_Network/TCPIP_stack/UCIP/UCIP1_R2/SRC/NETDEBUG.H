/*****************************************************************************
* netdebug.h - System debugging utilities.
*
* portions Copyright (c) 1998 Global Election Systems Inc.
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
* 98-07-29 Guy Lancaster <lancasterg@acm.org>, Global Election Systems Inc.
*	Original.
*
*(yyyy-mm-dd)
* 2001-06-01 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Added header to existing file.
*
*****************************************************************************
*/
#ifndef NETDEBUG_H
#define NETDEBUG_H

/************************
*** PUBLIC DATA TYPES ***
************************/
/* Trace levels. */
typedef enum {
	LOG_CRITICAL = 0,
	LOG_ERR = 1,
	LOG_NOTICE = 2,
	LOG_WARNING = 3,
	LOG_INFO = 5,
	LOG_DETAIL = 6,
	LOG_DEBUG = 7
} LogCodes;


/* 
 * Trace module codes - used as indices into the traceLev array of trace 
 * levels. 
 * *** NOTE: There must be an entry in the maskModuleToken in debug.c for
 * *** each entry here.
 */
typedef enum {
	TL_UNDEF,					/* Undefined module. */
	TL_PPP,						/* PPP */
	TL_IP,						/* IP */
	TL_TCP,						/* TCP */
	TL_CHAT,					/* Modem dialer */
	TL_ECHO,					/* TCP echo service. */
	TL_FEEDER,					/* Accu-Feed */
	TL_SCAN,					/* Scanner control. */
    TL_ETH,                     /* Ethernet 802.2 */
	TL_MAX						/*** Max modules - leave at end ***/
} TraceModule;

#define LOGMSGLEN 80					/* Max length of a log message. */


/*****************************
*** PUBLIC DATA STRUCTURES ***
*****************************/
extern unsigned char debug;


/***********************
*** PUBLIC FUNCTIONS ***
***********************/
extern char *debugchr(char FAR *d, unsigned char c);
extern void SetWinAdv(void);

void debugInit(void);
void monStart(void);
void setTraceLevel(int level, TraceModule tMod);
int getTraceLevel(TraceModule tMod);


/*
 *	trace - a form of printf to send tracing information to stderr
 */
void trace(int level, const char FAR *format,...);

/*
 * logTrace - A form of printf that writes a line to the trace log if
 *	the trace level for the specified module is high enough.
 */
void logTrace(int level, TraceModule tMod,  const char FAR *format,...);

/*
 * Print the given lines from the trace log in readable form to the
 *	specified file stream.  The previous start and max lines are saved so
 *	that if they are passed as -1, the dump displays the next set of lines.
 */
//void traceDump(FILE *fptr, int startLine, int maxLines);
void traceDump(void *fptr, int startLine, int maxLines);

//#ifdef WIN32
#include ".\OS_WIN32\Trace.h"
//#endif

/*
 *	ntrace - a numeric trace function
 */
//void ntrace(int level, NumTraceCodes tCode, ULONG arg1, ULONG arg2);

#define UDPDEBUG(a) Trace##a

#define AUTHDEBUG(a) Trace1##a
#define ICMPDEBUG(a) Trace1##a
#define IPCPDEBUG(a) Trace1##a
#define UPAPDEBUG(a) Trace1##a
#define NBUFDEBUG(a) Trace1##a
#define LCPDEBUG(a) Trace1##a
#define FSMDEBUG(a) Trace1##a
#define DIAGMONTRACE(a) Trace1##a

#define IPDEBUG(a) Trace2##a
#define ETHDEBUG(a) Trace2##a
#define PPPDEBUG(a) Trace2##a
#define TCPDEBUG(a) Trace2##a
#define CHATTRACE(a) Trace2##a
#define ECHODEBUG(a) Trace2##a
#define TIMERDEBUG(a) Trace1##a


#endif // NETDEBUG_H
