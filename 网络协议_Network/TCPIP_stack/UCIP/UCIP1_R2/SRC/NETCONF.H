/*****************************************************************************
* netconf.h
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
* 2001-06-01 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Added header to existing file.
*
******************************************************************************
*/
#ifndef NETCONF_H
#define NETCONF_H

#include "nettypes.h"

#ifdef OS_WIN32
#include ".\OS_WIN32\target.h"
#endif

#ifndef IX86P
#if TARGET==OS_NULL
#include ".\OS_NULL\target.h"
#endif
#endif

#ifdef IX86P
#include ".\OS_X86P\target.h"
#endif




//#define OS_DEPENDENT
//#define HAVE_ANSI_TIME // now defined in target.h includes
#pragma warning (disable: 4068) // unknown pragma


/* Configuration. */
#define NUM_PPP 1           /* Max PPP sessions. */
#define MAXPPPHDR 5         /* Max bytes of a PPP header with a flag. */
//#define LOCALHOST "localhost"
#define LOCALHOST "192.168.0.10"       // mod by robert for test build

/* Define the processor byte ordering for the network protocols. */
#define LITTLE_ENDIAN 1         /* Bytes swapped Intel style */
#define BIG_ENDIAN    2         /* Greatest to least significant ala Motorola */
#define BYTE_ORDER LITTLE_ENDIAN
//#define BYTE_ORDER BIG_ENDIAN


#ifndef DEBUG_SUPPORT
/* Select modules to enable.  Ideally these would be set in the makefile but
 * we're limited by the command line length so you need to modify the settings
 * in this file.
 */
#define DEBUGMONPORT     0      /* Set > 0 for diag mon on modem port. */
#define DEBUG_SUPPORT    0      /* Set > 0 for debug monitor. */
#define STATS_SUPPORT    1      /* Set > 0 for network statistics. */
#define PAP_SUPPORT      1      /* Set > 0 for PAP. */
#define CHAP_SUPPORT     0      /* Set > 0 for CHAP. */
#define MSCHAP_SUPPORT   0      /* Set > 0 for MSCHAP (NOT FUNCTIONAL!) */
#define CBCP_SUPPORT     0      /* Set > 0 for CBCP (NOT FUNCTIONAL!) */
#define CCP_SUPPORT      0      /* Set > 0 for CCP (NOT FUNCTIONAL!) */
#define VJ_SUPPORT       1      /* Set > 0 for VJ header compression. */
#define ECHO_SUPPORT     0      /* Set > 0 for TCP echo service. */
#define MD5_SUPPORT      0      /* Set > 0 for MD5 (see also CHAP) */
#define UDP_SUPPORT      1      /* Set > 0 for UDP stack enable */
#define PPP_SUPPORT      0      /* Set > 0 for PPP */
#define ETHER_SUPPORT    1      /* Set > 0 for ETHER */
#define ONETASK_SUPPORT  0      /* Set > 0 for running uC/IP in a single task like DOS 
                                   This will enable callback functionality for TCP sockets,
                                   you should no longer use semaphores.
                                   OSSemCreate() must always return !0   !!!!      
                                 */
#endif


#define OURADDR      0xAC100371 /* Local IP address - 0 to negotiate (172.16.3.113)*/
#define PEERADDR     0x00000000 /* Default peer IP address. */
#define LOOPADDR     0x7F000001 /* Loopback address (127.0.0.1) */ 
#define IPTTLDEFAULT 64         /* Default IP time-to-live. */

#define MAXWORDLEN      1024    /* max length of word in file (incl null) */
#define MAXARGS         1       /* max # args to a command */
#define MAXNAMELEN      256     /* max length of hostname or name for auth */
#define MAXSECRETLEN    256     /* max length of password or secret */
#define IFNAMSIZ        16      /* Length of an interface name field. */

#define MAXIFHDR    MAXPPPHDR   /* Largest link level header. */

/*
 * Process stack sizes.
 */
#define OSMINSTACK  256                 /* Minimum stack size to support interrupt handlers. */
#define NETSTACK    OSMINSTACK + 512    /* Network goes deep. */

/*
 * Packet sizes
 *
 * Note - lcp shouldn't be allowed to negotiate stuff outside these
 *    limits.  See lcp.h in the pppd directory.
 * (XXX - these constants should simply be shared by lcp.c instead
 *    of living in lcp.h)
 */
#define PPP_MTU     512     /* Default MTU (size of Info field) */
#ifdef XXX
#define PPP_MAXMTU  65535 - (PPP_HDRLEN + PPP_FCSLEN)
#else
#define PPP_MAXMTU  512 /* Largest MTU we allow */
#endif
#define PPP_MINMTU  64
#define PPP_MRU     512     /* default MRU = max length of info field */
#define PPP_MAXMRU  512     /* Largest MRU we allow */
#define PPP_MINMRU  128

#define PPP_ADDRESS(p)  (((u_char *)(p))[0])
#define PPP_CONTROL(p)  (((u_char *)(p))[1])
#define PPP_PROTOCOL(p) ((((u_char *)(p))[2] << 8) + ((u_char *)(p))[3])


/*
 * Operating system internals.
 */


#if STATS_SUPPORT > 0
#define STATS(cmd) cmd
#else
#define STATS(cmd)
#endif

/*
 * Diagnostic statistics record structure.
 * This structure is designed to allow direct addressing of the statistics
 * counter (to minimize run time overhead) while also allowing automated 
 * display of the table.
 * This record should be put in a structure composed of only these records
 * with the last record having a null statistic name.  Then the statistics
 * can be printed automatically by treating the structure as an array.
 */
typedef struct {
	char	*fmtStr;	/* printf format string to display value. */
	u_long	val;		/* The statistics value. */
} DiagStat;



#endif // NETCONF_H
