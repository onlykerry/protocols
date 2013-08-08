/*****************************************************************************
* netsockd.h - Network Sockets Definitions header file
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
#ifndef _NETSOCKD_H_
#define _NETSOCKD_H_

#include "netaddrs.h"
#include "netbuf.h"


#define tcp_MaxData 32              // maximum bytes to buffer on output

typedef int (*procref)();

/*
typedef struct _tcp_socket
{
    struct _tcp_socket *next;
    short           state;          // connection state
    procref         dataHandler;    // called with incoming data
//    eth_HwAddress   hisethaddr;     // ethernet address of peer
//    in_HwAddress    hisaddr;        // internet address of peer
//    word            myport, hisport;// tcp ports for this connection
//    longword        acknum, seqnum; // data ack'd and sequence num
    int             timeout;        // timeout, in milliseconds
//    BOOL            unhappy;        // flag, indicates retransmitting segt's
//    word            flags;          // tcp flags word for last packet sent
    short           dataSize;       // number of bytes of data to send
//    byte            data[tcp_MaxData]; // data to send
} tcp_Socket;
 */

typedef struct _socket
{
    struct _udp_socket *next;
    short           state;          // connection state
    procref         dataHandler;    // called with incoming data
    EthernetAdress  hisEthAddr;     // ethernet address of peer
    InternetAdress  hisIPAddr;      // internet address of peer
    short           myPort, hisPort;// tcp ports for this connection
    int             timeout;        // timeout, in milliseconds
    short           dataSize;       // number of bytes of data to send
    unsigned char   data[tcp_MaxData]; // data to send
    NBuf* pRxData;
    NBuf* pTxData;
} Socket;


typedef struct _tcp_socket
{
    struct _tcp_socket *next;
    short           state;          // connection state
    procref         dataHandler;    // called with incoming data
    EthernetAdress  hisEthAddr;     // ethernet address of peer
    InternetAdress  hisIPAddr;      // internet address of peer
    short           myPort, hisPort;// tcp ports for this connection
    unsigned long   acknum, seqnum; // data ack'd and sequence num
    int             timeout;        // timeout, in milliseconds
    unsigned char   unhappy;        // flag, indicates retransmitting segt's
    short           flags;          // tcp flags word for last packet sent
    short           dataSize;       // number of bytes of data to send
    unsigned char   data[tcp_MaxData]; // data to send
    long af;            // copied parameters from socket(...) call
    short type;         // copied parameters from socket(...) call
    short protocol;     // copied parameters from socket(...) call
} tcp_Socket;


#endif // _NETSOCKD_H_
////////////////////////////////////////////////////////////////////////////////


