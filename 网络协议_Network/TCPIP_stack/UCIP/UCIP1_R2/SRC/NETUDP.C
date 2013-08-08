/**
 * UDP Protocol Support for uC/IP.
 *
 * Copyright (c) 2001 by Intelligent Systems Software (UK) Ltd.  All rights reserved.
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
 ******************************************************************************
 * REVISION HISTORY
 *
 * 02-02-2001 Craig Graham <c_graham@hinge.mistral.co.uk>,Intelligent Systems Software (UK) Ltd
 *  First Version. Not based on anything really except RFC 768 (and Guy Lancaster's TCP
 *      protocol implementation as a basis for the checksum stuff).
 *(yyyy-mm-dd)
 * 2001-06-27 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
 *      Added support for static array of UDP_QUEUE* removing dependancy on 
 *      malloc and free for all the usual reasons.
 ******************************************************************************
 * NOTES
 *  This probably isn't very good, but it does work (at least well enough to support DNS lookups,
 *  which is what I wanted it for).
 *
 */

#include "netconf.h"
#include <stdlib.h>
#include <string.h>
#include "net.h"
#include "nettimer.h"
#include "netbuf.h"
#include "netrand.h"
#include "netip.h"
#include "netiphdr.h"
#include "netudp.h"
#include "neticmp.h"

#include <stdio.h>
#include "netdebug.h"
#include "netos.h"

#pragma warning (push)
#pragma warning (disable: 4761) // integral size mismatch in argument; conversion supplied
#pragma warning (disable: 4244) // 'function' : conversion from 'short ' to 'unsigned char ', possible loss of data
#pragma warning (disable: 4018) // signed/unsigned mismatch
////////////////////////////////////////////////////////////////////////////////

/** Maximum number of udp connections at one time */
#define MAXUDP          4
#define FUDP_OPEN       1
#define FUDP_CONNECTED  2
#define FUDP_LISTEN     4
#define MAXUDPQUEUES    20

/*
#ifdef DEBUG_UDP
#define UDPDEBUG(A) printf A
#else
#define UDPDEBUG(A)
#endif
 */

/** Dump an nBuf chain */
#define DUMPCHAIN(A) {\
        NBuf* p;\
        int i; \
        for (p = (A); p; p = p->nextBuf) {\
            for (i = 0; i < p->len; i++) {\
                if ((i & 3)==0)\
                    UDPDEBUG(("\n[%d]:",i));\
                    UDPDEBUG(("%x,",p->data[i]));\
            }\
        }\
        UDPDEBUG(("\n"));\
    }

/**
 * Queue of incoming datagrams.
 * This maintains a list of nBuf chains, along with the source address and port they came from
 * to allow reponses to be sent to multiple peers from one unconnected socket
 */
typedef struct _udp_queue {
    /** Pointer to next chain */
    struct _udp_queue* next;
    /** Source address for this chain (machine byte order) */
    struct in_addr srcAddr;
    /** Source port for this chain (machine byte order) */
    u_int16_t srcPort;
    /** pointer to the head of the actual nBuf chain */
    NBuf* nBuf;
} UDP_QUEUE;

/**
 * UDP port control block
 */
typedef struct {
    unsigned long flags;
    /** incoming data semaphore (value is number of UDP_QUEUE's from head) */
    OS_EVENT* sem;
    /** accept address (network byte order) */
    struct in_addr acceptFromAddr;
    /** send to address (network byte order) */
    struct in_addr theirAddr;
    /** sendto/recvfrom port (network byte order) */
    u_int16_t theirPort;
    /** port number on local machine (network byte order) */
    u_int16_t ourPort;
    /** type of service */
    u_int16_t tos;
    /** Head of the queue of incoming datagrams */
    UDP_QUEUE* head;
    /** Last incoming datagram in the queue */
    UDP_QUEUE* tail;
} UDPCB;

static UDPCB udps[MAXUDP];
static UDP_QUEUE udpqs[MAXUDPQUEUES];
static UDP_QUEUE* udp_free_list;

/** Port number auto-assigned by udpConnect if you've not already bound the socket when you connect it */
static int randPort = 0x0507;

UDP_QUEUE* alloc_udp_q(void)
{
//    return (UDP_QUEUE*)malloc(sizeof(UDP_QUEUE));
    UDP_QUEUE* q = NULL;
    if (udp_free_list != NULL) {
        q = udp_free_list;
        udp_free_list = udp_free_list->next;
    }
    return q;
}

void free_udp_q(UDP_QUEUE* q)
{
//    free(q);
    q->srcPort = 0;
    q->next = udp_free_list;
    udp_free_list = q;
}

void udpInit(void)
{
    int i;
    UDPDEBUG(("udpInit()\n"));
    for (i = 0; i < MAXUDP; i++) {
        udps[i].flags = 0;
        udps[i].sem = OSSemCreate(0);
    }
    udp_free_list = NULL;
    for (i = MAXUDPQUEUES; i--; ) {
        free_udp_q(&udpqs[i]);
    }
}


int udpOpen(void)
{
    int i;
    UDPDEBUG(("udpOpen()\n"));
    OS_ENTER_CRITICAL();
    for (i = 0; (i < MAXUDP) && (udps[i].flags & FUDP_OPEN); i++);
    if (i == MAXUDP) {
        OS_EXIT_CRITICAL();
        return -1;
    }
    udps[i].flags |= FUDP_OPEN;
    udps[i].ourPort = udps[i].theirPort = 0;
    udps[i].theirAddr.s_addr = 0xffffffff;
    udps[i].acceptFromAddr.s_addr = 0xffffffff;   /* Default to not accepting any address stuff */
    udps[i].head = udps[i].tail = NULL;
    OS_EXIT_CRITICAL();
    return i;
}

int udpClose(int ud)
{
    if (!(udps[ud].flags & FUDP_OPEN)) return -1;
    udps[ud].flags = 0;
    return 0;
}

int udpConnect(u_int ud, const struct sockaddr_in *remoteAddr, u_char tos)
{
    if (!(udps[ud].flags & FUDP_OPEN)) return -1;
    udps[ud].acceptFromAddr = udps[ud].theirAddr = remoteAddr->sin_addr;
    udps[ud].theirPort = htons(remoteAddr->sin_port);
    udps[ud].tos = tos;
    if (udps[ud].ourPort == 0)
        udps[ud].ourPort = htons(randPort++);
    udps[ud].flags |= FUDP_CONNECTED;
    return 0;
}

int udpListen(u_int ud, int backLog)
{
    if (!(udps[ud].flags & FUDP_OPEN)) return -1;
    udps[ud].flags |= FUDP_LISTEN;
    return 0;
}

int udpBind(u_int ud, struct sockaddr_in* peerAddr)
{
    UDPDEBUG(("udpBind(%d,%lx)\n",ud,peerAddr));
    if (!(udps[ud].flags & FUDP_OPEN)) return -1;
    udps[ud].acceptFromAddr = peerAddr->sin_addr;
    // TODO: work out whether we should do the htons or the client ???
    udps[ud].ourPort = peerAddr->sin_port;
//    udps[ud].ourPort = htons(peerAddr->sin_port);
    return 0;
}

int udpRead(u_int ud, void* buf, long len)
{
    unsigned char* d;
    int rtn;
    NBuf* b;
    UDP_QUEUE* q;
    struct in_addr fromAddr;
    UBYTE err;

    if (!(udps[ud].flags & FUDP_OPEN)) return -1;
    d = (unsigned char*)buf;
    rtn = 0;
    OSSemPend(udps[ud].sem, 0, &err);
    if (udps[ud].head == NULL) {
        return -1;
    }
    fromAddr = udps[ud].theirAddr = udps[ud].head->srcAddr;
    udps[ud].theirPort = udps[ud].head->srcPort;
    while ((udps[ud].head) && (len) && (fromAddr.s_addr == udps[ud].head->srcAddr.s_addr)) {
        b = udps[ud].head->nBuf;
        while ((len) && (b)) {
            while ((len) && (b->data != &b->body[b->len])) {
                *d++ = *b->data++;
                len--;
                rtn++;
            }
            if (b->data == &b->body[b->len]) {
                OS_ENTER_CRITICAL();
                b = udps[ud].head->nBuf = nFree(b);
                OS_EXIT_CRITICAL();
            }
        }
        if (b == NULL) {
            q = udps[ud].head;
            OS_ENTER_CRITICAL();
            udps[ud].head = q->next;
            OS_EXIT_CRITICAL();
            free_udp_q(q);
            if (udps[ud].head)
                OSSemPend(udps[ud].sem, 0, &err);
        } else {
            OSSemPost(udps[ud].sem);
        }
    }
    if (udps[ud].head == NULL)
        udps[ud].tail = NULL;
    return rtn;
}

int udpWrite(u_int ud, const void* buf, long len)
{
    NBuf* outHead;
    NBuf* outTail;
    IPHdr* ipHdr;
    UDPHdr* udpHdr;
    long rtn = len;
    long segLen;
    long packetLen;
    const unsigned char* d;
    u_int mtu;

    UDPDEBUG(("udpWrite()\n"));
    if (!(udps[ud].flags & FUDP_OPEN)) return -1;
    mtu = ipMTU(htonl(udps[ud].theirAddr.s_addr));
    if (!mtu) {
        UDPDEBUG(("no route to host\n"));
        return -1;
    }
    while (len) {
        outHead = NULL;
        do {
            nGET(outHead);
        } while (outHead == NULL);
        outTail = outHead;
        packetLen = MIN(mtu, sizeof(IPHdr) + sizeof(UDPHdr) + len);

        /* build IP header */
        ipHdr = nBUFTOPTR(outTail, IPHdr*);
        ipHdr->ip_v = 4;
        ipHdr->ip_hl = sizeof(IPHdr) / 4;
        ipHdr->ip_tos = udps[ud].tos;
        ipHdr->ip_len = packetLen;
        ipHdr->ip_id = IPNEWID();
        ipHdr->ip_off = 0;
        ipHdr->ip_ttl = 0;
        ipHdr->ip_p = IPPROTO_UDP;
        ipHdr->ip_sum = htons(ipHdr->ip_len - sizeof(IPHdr));

        ipHdr->ip_src.s_addr = htonl(localHost);
//        ipHdr->ip_dst.s_addr = htonl(udps[ud].theirAddr.s_addr);
        ipHdr->ip_dst.s_addr = udps[ud].theirAddr.s_addr;

        outHead->chainLen = packetLen;
        packetLen -= sizeof(IPHdr);

        /* Build the UDP header */
        udpHdr = (UDPHdr*)(ipHdr + 1);
        udpHdr->srcPort = udps[ud].ourPort;
        udpHdr->dstPort = udps[ud].theirPort;
        udpHdr->length = packetLen;
        udpHdr->checksum = 0;
        outTail->data = (unsigned char*)(udpHdr+1);
        outTail->len = sizeof(IPHdr) + sizeof(UDPHdr);
        packetLen -= sizeof(UDPHdr);
        len -= packetLen;


        UDPDEBUG(("-src=%s port %ld dest=%s port=%ld\n", \
            ip_ntoa(/*htonl*/(ipHdr->ip_src.s_addr)),   \
            htons(udpHdr->srcPort),                     \
            ip_ntoa2(/*htonl*/(ipHdr->ip_dst.s_addr)),  \
            htons(udpHdr->dstPort)                      \
            ));

        /* copy data to nBuf chain */
        d = (unsigned char*)buf;
        while (packetLen) {
            segLen = MIN(NBUFSZ - outTail->len, packetLen);
            memcpy(outTail->data, d, segLen);
            outTail->len += segLen;
            packetLen -= segLen;
            if (packetLen) {
                do {
                    nGET(outTail->nextBuf);
                } while (!outTail->nextBuf);
                outTail = outTail->nextBuf;
                outTail->nextBuf = NULL;
                outTail->len = 0;
            }
        }
        outHead->data = outHead->body;
        udpHdr->checksum = inChkSum(outHead, outHead->chainLen - 8, 8);
        ipHdr->ip_ttl = UDPTTL;
      //  DUMPCHAIN(outHead);
        /* Pass the datagram to IP and we're done. */
        ipRawOut(outHead);
    }
    return rtn;
}

long udpRecvFrom(int ud, void* buf, long len, struct sockaddr_in* from)
{
    long rtn;

    UDPDEBUG(("udpRecvFrom()\n"));
    if (!(udps[ud].flags & FUDP_OPEN)) return -1;
    rtn = udpRead(ud, buf, len);
    if (rtn < 0) return rtn;
    from->sin_family = AF_INET;
    from->sin_addr.s_addr = htonl(udps[ud].theirAddr.s_addr);
    from->sin_port = htons(udps[ud].theirPort);
    return rtn;
}

long udpSendTo(int ud, const void* buf, long len, const struct sockaddr_in* to)
{
    long rtn;

    UDPDEBUG(("udpSendTo()\n"));
    if (!(udps[ud].flags & FUDP_OPEN)) return -1;
    udps[ud].theirAddr.s_addr = htonl(to->sin_addr.s_addr);
    udps[ud].theirPort = htons(to->sin_port);
    rtn = udpWrite(ud, buf, len);
    return rtn;
}

static UDPCB* udpResolveIncomingUDPCB(u_long srcAddr, u_int16_t port)
{
    int i;
    for (i = 0; (i < MAXUDP); i++) {
        if ((udps[i].flags & FUDP_OPEN) && 
            (udps[i].ourPort == port) &&
           ((udps[i].acceptFromAddr.s_addr == 0) ||
            (udps[i].acceptFromAddr.s_addr == srcAddr)))
            return &udps[i];
    }
    return NULL;
}


////////////////////////////////////////////////////////////////////////////////

u_int16 CalcChkSum(u_int16* addr, u_int16 count, u_int16 initial)
{
    register unsigned long sum;

    sum = initial;
    while (count > 1) {
        sum += *addr++;
        count -= 2;
    }
    // Add left over byte if any
    if (count > 0) {
        sum += *(FAR UCHAR*)addr;
    }
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return (u_int16)(~sum);
}

typedef struct {
	struct	in_addr ih_src;
	struct	in_addr ih_dst;
	u_char Zero;
	u_char Protocol;
	u_int16 Length;
} PseudoHdr;


u_int16 ToInt(u_int16 Indata)
{
    u_int16 swapped = (LOBYTE(Indata) << 8) | (HIBYTE(Indata));
    return swapped;
}

u_int16 CalcPseudoHdr(FAR IPHdr* IPPtr, u_int16 Length)
{
    PseudoHdr   PsHdr;
    memcpy(&PsHdr.ih_src, &IPPtr->ip_src, sizeof(struct	in_addr));
    memcpy(&PsHdr.ih_dst, &IPPtr->ip_dst, sizeof(struct	in_addr));
    PsHdr.Zero = 0;
    PsHdr.Protocol = IPPtr->ip_p;
    PsHdr.Length = Length;
    return ~CalcChkSum((u_int16*)&PsHdr, 12, 0);  // Return the checksum of the pseudo header
}

void RxUdp(NBuf* pNBuf)
{
    u_int16 UDPLength;
    u_int16 UDPChkSum;
    u_int16 ChkSum;
    UDPHdr* pUDPHdr = (UDPHdr*)(nBUFTOPTR(pNBuf, char*) + sizeof(IPHdr));
    IPHdr* pIPHdr = (IPHdr*)(nBUFTOPTR(pNBuf, char*));
    
    UDPLength = ToInt(pUDPHdr->length);
    UDPChkSum = pUDPHdr->checksum;
    pUDPHdr->checksum = 0;

    // First we evaluate the checksum to see if it is OK.
    ChkSum = CalcPseudoHdr(pIPHdr, pUDPHdr->length);
    ChkSum = CalcChkSum((FAR u_int16*)pUDPHdr, UDPLength, ChkSum);
    if (ChkSum != UDPChkSum) {
        TRACE("RxUdp(%p) FAIL", pNBuf);
    } else {
        TRACE("RxUdp(%p) PASS", pNBuf);
    }
    TRACE("  UDPChkSum: %04X ChkSum:%04X\n", UDPChkSum, ChkSum);
    pUDPHdr->checksum = UDPChkSum;
}

////////////////////////////////////////////////////////////////////////////////

void udpInput(NBuf* inBuf, u_int ipHeadLen)
{
    IPHdr* ipHdr;               /* Ptr to IP header in output buffer. */
    UDPHdr* udpHdr;             /* Ptr to UDP header in output buffer. */
    UDPCB* udpCB;
    u_int16_t recv_chkSum,calc_chkSum;

    ipHdr = nBUFTOPTR(inBuf, IPHdr*);
    UDPDEBUG(("udpInput()\n"));
  //  DUMPCHAIN(inBuf);
    if (inBuf == NULL) {
        UDPDEBUG(("udpInput: Null input dropped\n"));
        return;
    }

    /*
     * Strip off the IP options.  The UDP checksum includes fields from the
     * IP header but without the options.
     */
    if (ipHeadLen > sizeof(IPHdr)) {
        inBuf = ipOptStrip(inBuf, ipHeadLen);
        ipHeadLen = sizeof(IPHdr);
    }

    /*
     * Get IP and UDP header together in first nBuf.
     */
    if (inBuf->len < sizeof(UDPHdr) + sizeof(IPHdr)) {
        if ((inBuf = nPullup(inBuf, sizeof(UDPHdr) + ipHeadLen)) == 0) {
            UDPDEBUG(("udpInput: Runt packet dropped\n"));
            return;
        }
    }
    ipHdr = nBUFTOPTR(inBuf, IPHdr*);

    /*
     * Note: We use ipHeadLen below just in case we kept an option with
     *  the IP header.
     */
    udpHdr = (UDPHdr*)((char*)ipHdr + ipHeadLen);
    UDPDEBUG(("src=%s port %ld dest=%s port=%ld\n", \
        ip_ntoa(/*htonl*/(ipHdr->ip_src.s_addr)),   \
        htons(udpHdr->srcPort),                     \
        ip_ntoa2(/*htonl*/(ipHdr->ip_dst.s_addr)),  \
        htons(udpHdr->dstPort)                      \
        ));
    UDPDEBUG(("length=%d\n",htons(udpHdr->length)));
//    UDPDEBUG(("checksum=%04X\n", htons(udpHdr->checksum)));
//    UDPDEBUG(("nbuf_chkSum=%04X\n", udpHdr->checksum));

    /* Save the recieved checksum for validating the packet */
//    recv_chkSum = htons(udpHdr->checksum);
    recv_chkSum = udpHdr->checksum;
    udpHdr->checksum = 0;

    /*
     * Prepare the header for the TCP checksum.  The TCP checksum is
     * computed on a pseudo IP header as well as the TCP header and
     * the data segment.  The pseudo IP header includes the length
     * (not including the length of the IP header), protocol, source
     * address and destination address fields.  We prepare this by
     * clearing the TTL field and loading the length in the IP checksum
     * field.
     */
    ipHdr->ip_ttl = 0;
//    ipHdr->ip_sum = htons(ipHdr->ip_len - sizeof(IPHdr));
//    ipHdr->ip_sum = htons(ipHdr->ip_len) - sizeof(IPHdr);
    ipHdr->ip_sum = ipHdr->ip_len;

    /* Calculate the checksum on the packet */
//    calc_chkSum = inChkSum(inBuf, inBuf->chainLen - 8, 8);
//    calc_chkSum = inChkSum(inBuf, inBuf->len - 8, 8);
    calc_chkSum = inChkSum(inBuf, ToInt(udpHdr->length) + 12, 8);

//    UDPDEBUG(("inChkSum  =%04X\n", inChkSum(inBuf, ToInt(udpHdr->length), sizeof(IPHdr))));
//    UDPDEBUG(("CalcChkSum=%04X\n", CalcChkSum((u_int16*)udpHdr, ToInt(udpHdr->length), 0)));

    UDPDEBUG((" HeaderSum =%04X\n", inChkSum(inBuf, 12, 8)));
    UDPDEBUG(("~CalcPseudo=%04X\n", ~CalcPseudoHdr(ipHdr, udpHdr->length) & 0xFFFF));

//    UDPDEBUG(("udpHdr->length=%04X\n",udpHdr->length));
//    UDPDEBUG(("calc_chkSum=%04X\n",calc_chkSum));
//    UDPDEBUG(("inBuf->len=%04X\n",inBuf->len));

    if (recv_chkSum != calc_chkSum) {
        // checksum failed, junk the packet - as UDP isn't a connected protocol,
        //  we don't need to inform the sender, we can simply throw the packet away
        UDPDEBUG(("chkSum FAILED - recv_chkSum=%04X, calc_chkSum=%04X\n", recv_chkSum, calc_chkSum));

        udpHdr->checksum = recv_chkSum;
        RxUdp(inBuf);

        nFreeChain(inBuf);
        return;
    }


    /* Try to find an open udp port to receive the incoming packet */
    udpCB = udpResolveIncomingUDPCB(ipHdr->ip_src.s_addr, udpHdr->dstPort);
    if (udpCB == NULL) {
        /* if port not open, or port is not listening for connections from that address reflect the packet */
        UDPDEBUG(("udpInput: port %ld not open, sending icmp_error\n", udpHdr->dstPort));
        icmp_error(inBuf, ICMP_UNREACH,ICMP_UNREACH_PORT, 0);
        return;
    }

    /* Add NBuf chain to the incoming data queue for the port */
    inBuf->data = (unsigned char*)(udpHdr + 1);
    if (udpCB->tail) {
        udpCB->tail->next = alloc_udp_q();
        if (udpCB->tail->next == NULL) {
            goto UDP_ALLOC_FAILED;
        }
        udpCB->tail = udpCB->tail->next;
    } else {
        udpCB->head = udpCB->tail = alloc_udp_q();
        if (udpCB->tail == NULL) {
            goto UDP_ALLOC_FAILED;
        }
    }
    udpCB->tail->next = NULL;
    udpCB->tail->srcAddr = ipHdr->ip_src;
    udpCB->tail->srcPort = udpHdr->srcPort;
    udpCB->tail->nBuf = inBuf;

    /* finally, post a semaphore to wake up anyone waiting for data on this UDP port */
    OSSemPost(udpCB->sem);
    return;

UDP_ALLOC_FAILED:
    UDPDEBUG(("udpInput: port %ld, out of udp queue buffers\n", udpHdr->dstPort));
    return;
}


#pragma warning (pop)
////////////////////////////////////////////////////////////////////////////////
