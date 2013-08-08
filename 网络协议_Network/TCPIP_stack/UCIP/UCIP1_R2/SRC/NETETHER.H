/*****************************************************************************
* netether.h - Ethernet interface functions header file
*
* portions Copyright (c) 2001 by Partner Voxtream A/S.
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
*(dd-mm-yyyy)
* 01-05-2001 Mads Christiansen <mc@voxtream.com>, Partner Voxtream.
*            Original file.
* 12-05-2001 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Merged with version by Mads, moved arp additions to netarp.h
* 2001-06-01 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Modified to make OS independant, back like original by Mads
*
*****************************************************************************/

#ifndef NETETHER_H
#define NETETHER_H

#define ETH_HEADER_LENGTH   14

// Ethernet protocol types
#define ETHERTYPE_IP   0x0800
#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_RARP 0x8035

// Ethernet header
typedef struct
{
  u_char  dst[6];
  u_char  src[6];
  u_short protocol;
} etherHdr;


typedef struct {
    EthernetAdress DestAdr;
    EthernetAdress SourceAdr;
    u_short FrameType;
} EthHdr;


// Setup type for ethernet
typedef struct
{
  u_long   subnetMask;          // Subnet mask in host order
  u_long   gatewayAddr;         // Gateway address in host order
  u_long   localAddr;
  u_long   networkAddr;
  u_char   hardwareAddr[6];     // MAC address of ethernet card
  u_long   arpExpire;           // Expire time for ARP entries
} etherSetup;


/*
 * etherConfig
 *
 * Use this to setup the ARP protocol and etherNNNN interface.
 * This is NOT the best implementation but will do for now.
 */
void etherConfig(etherSetup* setup);


/*
 * etherMTU
 *
 * Returns Maximum Transmission Unit for this ethernet interface. 
 * This is a VERY simple impl. It just returns 1500 (bytes).
 * Actually depending on the ethernet driver (IEEE 802.3 or (DIX) Ethernet) 
 * this should return the correct MTU.
 */
int etherMTU(void);


/*
 * etherInput
 *
 * All incoming ethernet packets should be sent here.
 */
void etherInput(NBuf* inBuf);


/*
 * etherOutput
 *
 * All outgoing IP packets should be sent here (no ethernet header!).
 */
void etherOutput(NBuf* outBuf);


/*
 * etherInit
 *
 * Use this initialize the ARP protocol and etherNNNN interface.
 * REMEMBER TO HAVE CALLED etherConfig AND set localHost BEFORE CALLING THIS FUNCTION!
 */
void etherInit(void);


// the following added by robert TODO: cleanup.

// Prototypes
void etherSend(NBuf* pNBuf);
void etherLock(void);
void etherRelease(void);


/* Error codes. */
#define ETHERR_PARAM -1             // Invalid parameter.
#define ETHERR_OPEN -2              // Unable to open ETH session.
#define ETHERR_DEVICE -3            // Invalid I/O device for ETH.
#define ETHERR_ALLOC -4             // Unable to allocate resources.
#define ETHERR_USER -5              // User interrupt.
#define ETHERR_CONNECT -6           // Connection lost.
#define ETHERR_AUTHFAIL -7          // Failed authentication challenge.
#define ETHERR_PROTOCOL -8          // Failed to meet protocol.

/*
 * Statistics.
 */
typedef struct {
    DiagStat headLine;              // Head line for display.
    DiagStat eth_ibytes;            // bytes received
    DiagStat eth_ipackets;          // packets received
    DiagStat eth_ierrors;           // receive errors
    DiagStat eth_derrors;           // dispatch errors
    DiagStat eth_obytes;            // bytes sent
    DiagStat eth_opackets;          // packets sent
    DiagStat eth_oerrors;           // transmit errors
    u_long  nbufError;              // # of times we tried to get a new nBuf but failed to do so
} ETHStats;

#define ETHibytes   eth_ibytes.val      // bytes received
#define ETHipackets eth_ipackets.val    // packets received
#define ETHierrors  eth_ierrors.val     // receive errors
#define ETHderrors  eth_derrors.val     // dispatch errors
#define ETHobytes   eth_obytes.val      // bytes sent
#define ETHopackets eth_opackets.val    // packets sent
#define ETHoerrors  eth_oerrors.val     // transmit errors



#endif // NETETHER_H

