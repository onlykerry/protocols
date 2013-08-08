/*****************************************************************************
* neteth.c - Ethernet interface functions
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
* 2001-06-05 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Original file. Split UCOS task & queue handling out of netether.c
*
******************************************************************************
*/
#include "netconf.h"
#include "net.h"
#include "netbuf.h"
#include "netip.h"
#include "netiphdr.h"
#include "netaddrs.h"
#include "netether.h"
#include "netifdev.h"
#include "neteth.h"
#include <string.h>
#include "netarp.h"
#include "netos.h"
#include <stdio.h>
#include "netdebug.h"


#pragma warning (push)
#pragma warning (disable: 4761) // integral size mismatch in argument; conversion supplied


////////////////////////////////////////////////////////////////////////////////
extern int repeat;

//OS_EVENT* pEthTxQ;
Interface* pDefaultInterface;

// Setup variables
//etherSetup mySetup;

#if STATS_SUPPORT > 0
ETHStats ethStats;              /* Statistics. */
#endif

#define TXQLEN 8
NBuf* TxNBufQ[TXQLEN];
UBYTE TxNBufHead;
UBYTE TxNBufTail;


////////////////////////////////////////////////////////////////////////////////
// Send a packet on the given connection.
// Return 0 on success, an error code on failure.
int ethOutput(int pd, u_short protocol, NBuf* nb)
{
//    Interface* pInterface = (Interface*)pd;
    etherOutput(nb);
    return 0;
}


////////////////////////////////////////////////////////////////////////////////
//
void etherSend(NBuf* pNBuf)
{
//    TRACE("etherSend(%p) - posting NBuf\n", pNBuf);
    etherLock();
    TxNBufQ[TxNBufHead] = pNBuf;
    TxNBufHead++;
    if (TxNBufHead >= TXQLEN) TxNBufHead = 0;
    pDefaultInterface->txEventCnt++;
    etherRelease();
    OSSemPost(pDefaultInterface->pSemIF);
}


////////////////////////////////////////////////////////////////////////////////
//
void EthTask(void* param)
{
    UBYTE err;
    NBuf* pNBuf = NULL;
    UWORD timeout = 500;
    Interface* pInterface = (Interface*)param;

    TRACE("EthTask Started\n");
    do {
        OSSemPend(pInterface->pSemIF, timeout, &err);
        if (err == OS_NO_ERR) {
            if (pInterface->rxEventCnt) {
//                TRACE("EthTask Rx Event Detected\n");
                pNBuf = pInterface->receive();
                if (pNBuf != NULL) etherInput(pNBuf);
                pInterface->rxEventCnt--;
            }
            if (pInterface->txEventCnt) {
//                TRACE("EthTask Tx Event Detected\n");
                if (pInterface->transmit_ready()) {
                    etherLock();
                    if (TxNBufTail != TxNBufHead) {
                        pNBuf = TxNBufQ[TxNBufTail];
                        TxNBufQ[TxNBufTail] = NULL;
                        TxNBufTail++;
                        if (TxNBufTail >= TXQLEN) TxNBufTail = 0;
                        etherRelease();
                        if (pNBuf) {
                            pInterface->transmit(pNBuf);
                            nFreeChain(pNBuf);
                        } else {
                            TRACE("EthTask Tx Event ERROR: null pointer\n");
                        }
                    } else {
                        etherRelease();
                    }
                    pInterface->txEventCnt--;
                }
            }
        }
    } while (repeat);
    TRACE("EthTask Exiting\n");
}

////////////////////////////////////////////////////////////////////////////////
//
void ethInit(Interface* pInterface)
{
    pDefaultInterface = pInterface;
    pInterface->pSemIF = OSSemCreate(0x0034);
    OSTaskCreate(EthTask, pInterface, NULL, 12);
}

#pragma warning (pop)
////////////////////////////////////////////////////////////////////////////////
