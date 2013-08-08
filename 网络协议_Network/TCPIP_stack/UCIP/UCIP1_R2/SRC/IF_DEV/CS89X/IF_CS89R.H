/*****************************************************************************
* if_cs89r.h - 
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
#ifndef _IF_CS89R_H_
#define _IF_CS89R_H_


//
// Crystal CS8900 PacketPage equates
//
#define CS8900Base       0x7f00

#define portRxTxData   (CS8900Base+0x00)   //Receive/Transmit data (port 0)
#define portRxTxData1  (CS8900Base+0x02)   //Receive/Transmit data (port 0)
#define portTxCmd      (CS8900Base+0x04)   //Transmit Commnad
#define portTxLength   (CS8900Base+0x06)   //Transmit Length
#define portISQ        (CS8900Base+0x08)   //Interrupt status queue
#define portPtr        (CS8900Base+0x0a)   //PacketPage pointer
#define portData       (CS8900Base+0x0c)   //PacketPage data (port 0)
#define portData1      (CS8900Base+0x0e)   //PacketPage data (port 1)
//
// CS8900 PacketPage Offsets 
//
#define ppEISA             (u_short)0x0000          //EISA Registration number of CS8900
#define ppProdID           (u_short)0x0002          //Product ID Number
#define ppIOBase           (u_short)0x0020          //I/O Base Address
#define ppIntNum           (u_short)0x0022          //Interrupt number (0,1,2, or 3)
#define ppMemBase          (u_short)0x002C          //Memory Base address register (20 bit)
#define ppRxCfg            (u_short)0x0102          //Receiver Configuration
#define ppRxCtl            (u_short)0x0104          //Receiver Control
#define ppTxCfg            (u_short)0x0106          //Transmit Configuration
#define ppBufCfg           (u_short)0x010A          //Buffer Configuration
#define ppLineCtl          (u_short)0x0112          //Line Control
#define ppSelfCtl          (u_short)0x0114          //Self Control
#define ppBusCtl           (u_short)0x0116          //Bus Control
#define ppTestCtl          (u_short)0x0118          //Test Control
#define ppISQ              (u_short)0x0120          //Interrupt status queue
#define ppRxEvt            (u_short)0x0124          //Receiver Event
#define ppTxEvt            (u_short)0x0128          //Transmitter Event
#define ppBufEvt           (u_short)0x012C          //Buffer Event
#define ppRxMiss           (u_short)0x0130          //Receiver Miss Counter
#define ppTxCol            (u_short)0x0132          //Transmit Collision Counter
#define ppLineSt           (u_short)0x0134          //Line Status
#define ppSelfSt           (u_short)0x0136          //Self Status
#define ppBusSt            (u_short)0x0138          //Bus Status
#define ppTxCmd            (u_short)0x0144          //Transmit Command Request
#define ppTxLength         (u_short)0x0146          //Transmit Length
#define ppIndAddr          (u_short)0x0158          //Individual Address (IA)
#define ppRxStat           (u_short)0x0400          //Receive Status
#define ppRxLength         (u_short)0x0402          //Receive Length
#define ppRxFrame          (u_short)0x0404          //Receive Frame Location
#define ppTxFrame          (u_short)0x0A00          //Transmit Frame Location
//
// Register Numbers 
//
#define REG_NUM_MASK            0x003F
#define REG_NUM_RX_EVENT        0x0004
#define REG_NUM_TX_EVENT        0x0008
#define REG_NUM_BUF_EVENT       0x000C
#define REG_NUM_RX_MISS         0x0010
#define REG_NUM_TX_COL          0x0012
//
// Self Control Register
//
#define SELF_CTL_RESET          0x0040
#define SELF_CTL_HC1E           0x2000
#define SELF_CTL_HCB1           0x8000
//
// Self Status Register 
//
#define SELF_ST_INIT_DONE       0x0080
#define SELF_ST_SI_BUSY         0x0100
#define SELF_ST_EEP_PRES        0x0200
#define SELF_ST_EEP_OK          0x0400
#define SELF_ST_EL_PRES         0x0800
//
// Bus Control Register 
//
#define BUS_CTL_USE_SA          0x0200
#define BUS_CTL_MEM_MODE        0x0400
#define BUS_CTL_IOCHRDY         0x1000
#define BUS_CTL_INT_ENBL        0x8000
//
// Bus Status Register 
//
#define BUS_ST_TX_BID_ERR       0x0080
#define BUS_ST_RDY4TXNOW        0x0100
//
// Line Control Register 
//
#define LINE_CTL_RX_ON          0x0040
#define LINE_CTL_TX_ON          0x0080
#define LINE_CTL_AUI_ONLY       0x0100
#define LINE_CTL_10BASET        0x0000
//
// Test Control Register 
//
#define TEST_CTL_DIS_LT         0x0080
#define TEST_CTL_ENDEC_LP       0x0200
#define TEST_CTL_AUI_LOOP       0x0400
#define TEST_CTL_DIS_BKOFF      0x0800
#define TEST_CTL_FDX            0x4000
//
// Receiver Configuration Register 
//
#define RX_CFG_SKIP             0x0040
#define RX_CFG_RX_OK_IE         0x0100
#define RX_CFG_CRC_ERR_IE       0x1000
#define RX_CFG_RUNT_IE          0x2000
#define RX_CFG_X_DATA_IE        0x4000
//
// Receiver Event Register 
//
#define RX_EVENT_RX_OK          0x0100
#define RX_EVENT_IND_ADDR       0x0400
#define RX_EVENT_BCAST          0x0800
#define RX_EVENT_CRC_ERR        0x1000
#define RX_EVENT_RUNT           0x2000
#define RX_EVENT_X_DATA         0x4000
//
//Receiver Control Register 
//
#define RX_CTL_RX_OK_A          0x0100
#define RX_CTL_MCAST_A          0x0200
#define RX_CTL_IND_A            0x0400
#define RX_CTL_BCAST_A          0x0800
#define RX_CTL_CRC_ERR_A        0x1000
#define RX_CTL_RUNT_A           0x2000
#define RX_CTL_X_DATA_A         0x4000
//
//Transmit Configuration Register 
//
#define TX_CFG_LOSS_CRS_IE      0x0040
#define TX_CFG_SQE_ERR_IE       0x0080
#define TX_CFG_TX_OK_IE         0x0100
#define TX_CFG_OUT_WIN_IE       0x0200
#define TX_CFG_JABBER_IE        0x0400
#define TX_CFG_16_COLL_IE       0x8000
#define TX_CFG_ALL_IE           0x8FC0
//
//Transmit Event Register 
//
#define TX_EVENT_TX_OK          0x0100
#define TX_EVENT_OUT_WIN        0x0200
#define TX_EVENT_JABBER         0x0400
#define TX_EVENT_16_COLL        0x1000
//
// Transmit Command Register
//
#define TX_CMD_START_5          0x0000
#define TX_CMD_START_381        0x0080
#define TX_CMD_START_1021       0x0040
#define TX_CMD_START_ALL        0x00C0
#define TX_CMD_FORCE            0x0100
#define TX_CMD_ONE_COLL         0x0200
#define TX_CMD_NO_CRC           0x1000
#define TX_CMD_NO_PAD           0x2000
//
//Buffer Configuration Register 
//
#define BUF_CFG_SW_INT          0x0040
#define BUF_CFG_RDY4TX_IE       0x0100
#define BUF_CFG_TX_UNDR_IE      0x0200
//
// Other stuff for the CS8900
//
#define REGMASK         0x3f


#endif // _IF_CS89R_H_
////////////////////////////////////////////////////////////////////////////////
