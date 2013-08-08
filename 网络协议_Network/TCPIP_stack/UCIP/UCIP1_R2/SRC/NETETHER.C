/*****************************************************************************
* netether.c - Ethernet interface functions with ARP protocol
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
*(yyyy-mm-dd)
* 2001-05-18 Mads Christiansen <mads@mogi.dk>, Partner Voxtream 
*            Original file.
* 2001-05-12 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Merged with version by Mads, moved arp additions to netarp.c
* 2001-06-01 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Modified to make OS independant, back like original by Mads
*
*****************************************************************************
*/
#include "netconf.h"
#include "net.h"
#include "netbuf.h"
#include "netip.h"
#include "netiphdr.h"
#include "netaddrs.h"
#include "netether.h"

#include <string.h>
#include "netarp.h"
#include "netos.h"

#include <stdio.h>
#include "netdebug.h"


#pragma warning (push)
#pragma warning (disable: 4761) // integral size mismatch in argument; conversion supplied


////////////////////////////////////////////////////////////////////////////////
// Setup variables
etherSetup mySetup;

#if STATS_SUPPORT > 0
ETHStats ethStats;              /* Statistics. */
#endif

// Internal setup variables
static int initialized = FALSE;

////////////////////////////////////////////////////////////////////////////////
// Mutex for accessing ARP functions (and ARP tables)
static OS_EVENT *mutex;     


void etherLock(void)
{
    UBYTE err;
    OSSemPend(mutex, 0, &err);
}

void etherRelease(void)
{
    OSSemPost(mutex);
}

////////////////////////////////////////////////////////////////////////////////

void etherConfig(etherSetup* setup)
{
    // Create mutex semaphore
    if (!mutex) mutex = OSSemCreate(1);
    etherLock();
    // **** Copy new setup
    memcpy(&mySetup, setup, sizeof(mySetup));
    // **** Calculate my network address
    mySetup.networkAddr = mySetup.localAddr & mySetup.subnetMask;
    // **** Send a Gratuitous ARP
//    arpRequest(localHost);

    // **** Set initialized to true, we're ready to rx/tx data
    initialized = TRUE;
    etherRelease();
}



int etherMTU(void)
{
  // **** Return 1500 bytes MTU for ethernet
    return 1500;
}


void etherInput(NBuf* pNBuf)
{
//    ASSERT(pNBuf);
    if (!pNBuf) return;

    if (initialized) {
        etherHdr* etherHdrPtr;
        etherHdrPtr = nBUFTOPTR(pNBuf, etherHdr*);
        // Determine destination protocol of packet
        switch (ntohs(etherHdrPtr->protocol)) {
        case ETHERTYPE_IP: // IP protocol
            // Remove ethernet header
//            nTrim(NULL, &pNBuf, sizeof(etherHdr));

            nADVANCE(pNBuf, ETH_HEADER_LENGTH);

//            pNBuf->data += sizeof(etherHdr);
//            pNBuf->len  -= sizeof(etherHdr);
//            pNBuf->chainLen -= sizeof(etherHdr);
            // Deliver packet to the IP protocol
            if (pNBuf) ipInput(pNBuf, IFT_ETH, 0);
            break;
        case ETHERTYPE_ARP: // ARP protcol
            // Don't remove ethernet header!
            // Deliver packet to the ARP protocol
//            etherLock();
            arpInput(pNBuf);
//            etherRelease();
            break;
        default: // Unknown protocol
            // Free buffers
            nFreeChain(pNBuf);
        }
    } else {
        nFreeChain(pNBuf); // Free buffers
    }
}



////////////////////////////////////////////////////////////////////////////////
// parameter is a IP NBuf chain with NO ethernet header
//
void etherOutput(NBuf* pNBuf)
{
//    u_long ip;
    etherHdr ether;
    IPHdr* ipHdr;

//    ASSERT(pNBuf);
    if (!pNBuf) return;

    if (initialized) {
        ipHdr = nBUFTOPTR(pNBuf, IPHdr*);
        etherLock();

//        ip = ipHdr->ip_dst.s_addr;
//        NTOHL(ip);
//        if (arpResolve(ip, ether.dst, pNBuf)) {

        if (arpResolve(ntohl(ipHdr->ip_dst.s_addr), ether.dst, pNBuf)) {
            etherRelease();
            // Add my hardware (MAC) address and protocol type
            memcpy(ether.src, mySetup.hardwareAddr, sizeof(ether.src));
            ether.protocol = htons(ETHERTYPE_IP);

            // Prepend ethernet header to packet
            nPREPEND(pNBuf, &ether, sizeof(ether));

            // if outBuf == NULL packet could not be prepended is discarded
            if (pNBuf) {
                etherSend(pNBuf);
            } else {
                // Update nBuf allocate error
                ethStats.nbufError++;
                TRACE("etherOutput(...) - failed to nPREPEND()\n");
            }
        } else {
            etherRelease();
        }
    } else {
        nFreeChain(pNBuf); // Free buffers
    }
}


////////////////////////////////////////////////////////////////////////////////
//
void etherInit(void)
{
    // Create mutex semaphore
    if (!mutex) mutex = OSSemCreate(1);
    etherLock();

#if STATS_SUPPORT > 0
    /* Clear the statistics. */
    memset(&ethStats, 0, sizeof(ethStats));
    ethStats.headLine.fmtStr        = "\t\tETH STATISTICS\r\n";
    ethStats.eth_ibytes.fmtStr      = "\tBYTES IN    : %5lu\r\n";
    ethStats.eth_ipackets.fmtStr    = "\tPACKETS IN  : %5lu\r\n";
    ethStats.eth_ierrors.fmtStr     = "\tIN ERRORS   : %5lu\r\n";
    ethStats.eth_derrors.fmtStr     = "\tDISPATCH ERR: %5lu\r\n";
    ethStats.eth_obytes.fmtStr      = "\tBYTES OUT   : %5lu\r\n";
    ethStats.eth_opackets.fmtStr    = "\tPACKETS OUT : %5lu\r\n";
    ethStats.eth_oerrors.fmtStr     = "\tOUT ERRORS  : %5lu\r\n";
#endif
    etherRelease();
}


#pragma warning (pop)
////////////////////////////////////////////////////////////////////////////////
