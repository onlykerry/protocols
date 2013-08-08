/*****************************************************************************
* if_cs89x.c - 
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
******************************************************************************
*/
#include "..\..\netconf.h"
#include "..\..\netbuf.h"
#include "..\..\netos.h"
#include "..\..\netifdev.h"

#include "if_cs89d.h"
#include "if_cs89x.h"
#include "if_cs89r.h"
#include <stdio.h>
#include "..\..\netdebug.h"


u_short ISQSem;     // CS8900 interrupt occured, ISQSem contains ISQ value


/***********************************************************************************
 *
 * Function:    InitCS8900A(void)
 * Description: This function initializes the CS8900 to work in the mode desired
 *              for this application. Also writes the MAC to the chip.
 *      Settings are:
 *          10BaseT
 *          Full Duplex
 *          Enable RxOK interrupt
 *
 **********************************************************************************/
static u_char InitCS8900A(void)
{
    WritePP(ppLineCtl, LINE_CTL_10BASET);   // Set to 10BaseT
    WritePP(ppTestCtl, TEST_CTL_FDX);       // Set to full duplex
    WritePP(ppRxCfg, RX_CFG_RX_OK_IE);      // Enable RX OK interrupt
    WritePP(ppRxCtl, RX_CTL_RX_OK_A | RX_CTL_IND_A | RX_CTL_BCAST_A);
    WritePP(ppTxCfg, TX_CFG_ALL_IE);        // Enable interrupts for all Tx events
    WritePP(ppIntNum, 0);                   // Use interrupt 0 signal
//
// Now we need to write the myMAC to the CS8900
// Since we are wrong endian we need to reverse the word entries
//
//    WritePP(ppIndAddr+0, (u_short)(myMAC.mac2<<8 | myMAC.mac1));
//    WritePP(ppIndAddr+2, (u_short)(myMAC.mac4<<8 | myMAC.mac3));
//    WritePP(ppIndAddr+4, (u_short)(myMAC.mac6<<8 | myMAC.mac5));

    WritePP(ppBusCtl, 0x8000);          // Enable interrupts
    WritePP(ppLineCtl, 0x00c0);         // Enable SerRxOn  and SerTxOn

// TODO     IIMC = 0x04;                // Enable INT0 interrupts from CS8900
// TODO     INTE0AD = 0x04;             // Set interupt priority level to 4
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// 
//
static u_char StopCS8900A(void)
{
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// 
//
static u_char DriverTxPacket(NBuf* pNBuf)
{
    u_short packet_len;
    u_short TxLocPtr;
    u_short padding_cnt = 0;

    // send complete frame from NBuf to device and transmit.
    packet_len = nChainLen(pNBuf);
//    TRACE("DriverTxPacket() nChainLen - %u\n", packet_len);
    if (packet_len < 60) {                        // Minimum length is 60 bytes
        padding_cnt = 60 - packet_len;
        packet_len = 60;
    }

    WritePP(ppTxCmd, TX_CMD_START_ALL);             // Start transmitting after all bytes are sent
    WritePP(ppTxLength, packet_len);                // Tell CS8900 how many bytes
    TxLocPtr = ppTxFrame;                           // Set up CS8900 Tx location pointer
    while (!(ReadPP(ppBusSt) & BUS_ST_RDY4TXNOW)) { // Wait for CS8900 to complete previous Tx
    }
    WriteNBufPP(TxLocPtr, pNBuf);
    WritePP(ppTxCmd, TX_CMD_FORCE);              // Force send of packet - WIN32 ONLY ?
    return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Ethernet device interrupt support for frame reception.
//
NBuf* RxEthEvent(void)
{
	NBuf* headNB = NULL;
    u_short rx_status = ReadPP(ppRxStat);
    u_short packet_len = ReadPP(ppRxLength);

    OS_ENTER_CRITICAL();
	nGET(headNB);           // Grab an input buffer.
    OS_EXIT_CRITICAL();

//    TRACE("RxEthEvent() packet_len: %u\n", packet_len);

    if (headNB != NULL) {
        u_short data;
        u_short i;
        NBuf* tailNB = headNB;
        u_short wordcnt = packet_len >> 1;
        headNB->len = 0;
        for (i = 0; i < wordcnt; i++) {        // Get all data
            data = ReadPP((u_short)(ppRxFrame+(i*2))); // Get the data
//            TRACE(" %04X, ", data & 0xffff);
            tailNB = NBufPutW(data, tailNB);
            if (tailNB == NULL) {
                TRACE("RxEthEvent() error building nBuf chain ! - nFreeChain(%p)\n", headNB);
                nFreeChain(headNB);
                return (headNB = NULL);
            }
        }
/*
    {
        short i;
        char* data = nBUFTOPTR(headNB, char*);
        for (i = 0; i < packet_len; i++) {
            TRACE(" %02X : %c \n", data[i] & 0x00ff, isprint(data[i]) ? data[i] : ' ');
        }
    }
 */
//        TRACE("RxEthEvent() - posting new NBuf %p\n", headNB);
//        OSQPost(pEthRxQ, headNB);
    } else {
        TRACE("RxEthEvent() - failed to nGET()\n");
    }
    return headNB;
}


/***********************************************************************************
 *
 * Function:    __regbank(1) Intcs8900a(void)
 * Description: This is the entry point for interrupts from the CS8900
 *
 **********************************************************************************/
static void Intcs8900a(void)
{
    ISQSem = *(u_short*)portISQ;         // Interrupt occured so read first value in que
}

/***********************************************************************************
 *
 * Function:    ProcessISQ(void)
 * Description: Here we process all the interrupts held on the que ISQ.
 *              ISQSem contains the first que entry, it was read in the interrupt
 *              routine in order to reset the interrupt pin.
 *
 **********************************************************************************/
#ifdef QUEUE_TASKS
void ProcessISQ(Interface* pInterface)
{
	NBuf* headNB = NULL;
    ISQSem = 0x04; // TODO: debugging

//    do {
        switch (ISQSem & REGMASK) {
        case 0x04:          // Detected an Rx Event
            headNB = RxEthEvent();      // Go handle it
            if (headNB != NULL)
                OSQPost(pInterface->pRxQ, headNB);
            break;
        case 0x08:          // Detected a Tx Event
            break;
        case 0x0c:          // Detected a Buff event
            break;
        case 0x10:          // Detected a RxMISS event
            break;
        case 0x12:          // Detected a TXCOL event
            break;
        }                   // Those are all events generated by the CS8900
//    } while ((ISQSem = *(u_short*)portISQ) != 0x0000);
}

#else
void ProcessISQ(Interface* pInterface)
{
    ISQSem = 0x04; // TODO: debugging
//    do {
        switch (ISQSem & REGMASK) {
        case 0x04:          // Detected an Rx Event
            pInterface->rxEventCnt++;
            OSSemPost(pInterface->pSemIF);
            break;
        case 0x08:          // Detected a Tx Event
            pInterface->txEventCnt++;
            OSSemPost(pInterface->pSemIF);
            break;
        case 0x0c:          // Detected a Buff event
            break;
        case 0x10:          // Detected a RxMISS event
            break;
        case 0x12:          // Detected a TXCOL event
            break;
        }                   // Those are all events generated by the CS8900
//    } while ((ISQSem = *(u_short*)portISQ) != 0x0000);
}
#endif

////////////////////////////////////////////////////////////////////////////////
/*
1.  Declare/create new device structure
2.  Call DriverEntry(&device) to initialise func pointers
3.  
 */

static u_char statistics(if_statistics* pStats)
{
    return FALSE;
}

static u_char dummy_func(void)
{
    return TRUE;
}


u_char CS89X_DriverEntry(Interface* pInterface)
{
/*
  OS_EVENT* pEthTxQ;
  OS_EVENT* pEthRxQ;
 */
    pInterface->start = InitCS8900A;
    pInterface->stop = StopCS8900A;
    pInterface->receive = RxEthEvent;
    pInterface->receive_ready = dummy_func;
    pInterface->transmit = DriverTxPacket;
    pInterface->transmit_ready = dummy_func;
    pInterface->statistics = statistics;
    pInterface->interrupt = ProcessISQ;

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
