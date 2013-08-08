/*****************************************************************************
* netbootp.h - 
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
*            Original file.
*
******************************************************************************
*/
#ifndef _NETBOOTP_H_
#define _NETBOOTP_H_


/*************************************************/
/* Center for Information Technology Integration */
/*           The University of Michigan          */
/*                    Ann Arbor                  */
/*                                               */
/* Dedicated to the public domain.               */
/* Send questions to info@citi.umich.edu         */
/*                                               */
/* BOOTP is documented in RFC 951 and RFC 1048   */
/*************************************************/

#ifndef BOOTREQUEST
/*
#ifndef _MBUF_H
#include "mbuf.h"
#endif

#ifndef _SOCKET_H
#include "socket.h"
#endif

#ifndef _IP_H
#include "ip.h"
#endif
 */
typedef unsigned char uint8;
typedef unsigned short uint16;

struct bootp {
    uint8   op;             /* packet opcode type */
    uint8   htype;          /* hardware addr type */
    uint8   hlen;           /* hardware addr length */
    uint8   hops;           /* gateway hops */
    int32   xid;            /* transaction ID */
    uint16  secs;           /* seconds since boot began */
    uint16  unused;
    struct in_addr  ciaddr; /* client IP address */
    struct in_addr  yiaddr; /* 'your' IP address */
    struct in_addr  siaddr; /* server IP address */
    struct in_addr  giaddr; /* gateway IP address */
    uint8   chaddr[16];     /* client hardware address */
    char    sname[64];      /* server host name */
    char    file[128];      /* boot file name */
    uint8   vend[64];       /* vendor-specific area */
};

/*
 * UDP port numbers, server and client.
 */
#define IPPORT_BOOTPS   67
#define IPPORT_BOOTPC   68

#define BOOTREQUEST     1
#define BOOTREPLY       2

#define BOOTP_PAD       0
#define BOOTP_SUBNET    1
#define BOOTP_GATEWAY   3
#define BOOTP_DNS       6
#define BOOTP_HOSTNAME  12
#define BOOTP_END       0xff

/*
 * "vendor" data permitted for Stanford boot clients.
 */
struct vend {
    uint8  v_magic[4];      /* magic number */
    unsigned long  v_flags; /* flags/opcodes, etc. */
    uint8  v_unused[56];    /* currently unused */
};

#define VM_STANFORD     "STAN"  /* v_magic for Stanford */

/* v_flags values */
#define VF_PCBOOT       1       /* an IBMPC or Mac wants environment info */
#define VF_HELP         2       /* help me, I'm not registered */

extern int WantBootp;
extern char bp_ascii[];

void bootp_print_packet(struct bootp *bp);
int bootp_validPacket(struct ip *ip,struct mbuf *bp);

#endif



#endif /* _NETBOOTP_H_ */
