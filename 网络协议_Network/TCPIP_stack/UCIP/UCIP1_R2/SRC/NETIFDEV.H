/*****************************************************************************
* netifdev.h - NETwork InterFace Device Header File.
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
#ifndef _NETIFDEV_H_
#define _NETIFDEV_H_

/* NOTE: depends on netbuf.h. */


////////////////////////////////////////////////////////////////////////////////

typedef struct {
  u_long PacketsReceived;    
  u_long PacketsTransmitted;    
  u_long BytesReceived;
  u_long BytesTransmitted;
  u_long ReceiveErrors;
  u_long TransmitErrors;
  u_long NextPageErrors;
  u_long OverrunErrors;
} if_statistics;

////////////////////////////////////////////////////////////////////////////////

/* Interface control structure */
struct iface {
    struct iface *next; /* Linked list pointer */
    char *name;     /* Ascii string with interface name */
    int32 addr;     /* IP address */
    int32 broadcast;    /* Broadcast address */
    int32 netmask;      /* Network mask */
//    uint16 mtu;       /* Maximum transmission unit size */

//  int  (*Initialize)(u_char *address);
//    u_char (*nbuf_in)(NBuf* pNBuf);
//    u_char (*driver_entry)(struct t_Interface*);
//    OS_EVENT* pTxQ;
//    OS_EVENT* pRxQ;
    u_char rxEventCnt;
    u_char txEventCnt;
    void* pSemIF;
    void* pTxQ;
    void* pRxQ;
    // the device driver must provide a function for all of the following:
    u_char (*start)(void);
    u_char (*stop)(void);
    NBuf* (*receive)(void);
//    u_char (*receive)(void);
    u_char (*receive_ready)(void);
    u_char (*transmit)(NBuf* pNBuf);
    u_char (*transmit_ready)(void);
    u_char (*statistics)(if_statistics*);
    void (*interrupt)(struct iface*);
};


typedef struct iface Interface;
/*
typedef struct t_Interface {

//  int  (*Initialize)(u_char *address);

//    u_char (*nbuf_in)(NBuf* pNBuf);
//    u_char (*driver_entry)(struct t_Interface*);

//    OS_EVENT* pTxQ;
//    OS_EVENT* pRxQ;
    u_char rxEventCnt;
    u_char txEventCnt;
    void* pSemIF;


    void* pTxQ;
    void* pRxQ;

    // the device driver must provide a function for all of the following:
    u_char (*start)(void);
    u_char (*stop)(void);
    NBuf* (*receive)(void);
//    u_char (*receive)(void);
    u_char (*receive_ready)(void);
    u_char (*transmit)(NBuf* pNBuf);
    u_char (*transmit_ready)(void);
    u_char (*statistics)(if_statistics*);
    void (*interrupt)(struct t_Interface*);

} Interface;
 */

#endif /* _NETIFDEV_H_ */
