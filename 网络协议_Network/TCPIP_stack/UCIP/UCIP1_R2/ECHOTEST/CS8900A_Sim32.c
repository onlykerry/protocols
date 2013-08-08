////////////////////////////////////////////////////////////////////////////////
// CS8900A_Sim32.c
//
#include <windows.h>
#include "packet.h"
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

//#include "types.h"
#include "netconf.h"
#include "netaddrs.h"
#include "netbuf.h"
#include "netether.h"
#include "trace.h"

#include "netifdev.h"
#include "if_dev\cs89x\if_cs89d.h"  // Include CS8900A defintions
#include "if_dev\cs89x\if_cs89x.h"
#include "if_dev\cs89x\if_cs89r.h"

////////////////////////////////////////////////////////////////////////////////

short rx_packet_size = 0;
short rx_stat = 0;
short rx_buffer[5000];
short tx_packet_size = 0;
short tx_buffer[5000];

////////////////////////////////////////////////////////////////////////////////
extern Interface EthAdaptor;

void RxEvent_Sim32(USHORT* FramePtr, short length)
{
    memcpy(rx_buffer, FramePtr, length);
    rx_packet_size = length;
//    ProcessISQ();
//    RxEthEvent();
    EthAdaptor.interrupt(&EthAdaptor);
}

////////////////////////////////////////////////////////////////////////////////
extern LPADAPTER lpAdapter;
extern LPPACKET lpPacket;

void WritePacket(int Snaplen)
{
    LPPACKET lpPacket = 0;   // define a pointer to a PACKET structure

    if (Snaplen > sizeof(tx_buffer)) Snaplen = sizeof(tx_buffer);
    lpPacket = CreatePacket((char*)tx_buffer, Snaplen);
    PacketSetNumWrites(lpAdapter, 1);
    PacketSendPacket(lpAdapter, lpPacket, TRUE);
    ReleasePacket(lpPacket);
}



// values for variable ppAdress
/*
USHORT _portRxTxData;   // (CS8900Base+0x00)   //Receive/Transmit data (port 0)
USHORT _portRxTxData1;  // (CS8900Base+0x02)   //Receive/Transmit data (port 0)
USHORT _portTxCmd;      // (CS8900Base+0x04)   //Transmit Commnad
USHORT _portTxLength;   // (CS8900Base+0x06)   //Transmit Length
USHORT _portISQ;        // (CS8900Base+0x08)   //Interrupt status queue
USHORT _portPtr;        // (CS8900Base+0x0a)   //PacketPage pointer
USHORT _portData;       // (CS8900Base+0x0c)   //PacketPage data (port 0)
USHORT _portData1;      // (CS8900Base+0x0e)   //PacketPage data (port 1)
 */                  
/***********************************************************************************
 *
 * Function:    ReadPP(ppAdress)
 * Description: This function reads the Packet Page memory in the CS8900 at the
 *              given adress and returns the value.
 *
 **********************************************************************************/
USHORT ReadPP(USHORT ppAdress)
{
    USHORT data;
//    _portPtr = ppAdress;     // Write the pp Pointer

    if (ppAdress >= ppRxFrame) {
        int offset = ppAdress - ppRxFrame;
        offset = offset >> 1;
        if (offset >= 0 && offset <= sizeof(rx_buffer)) {
            data = rx_buffer[offset];
        } else {
            data = 0x55AA;
        }
        return data;
    }

    switch (ppAdress) {
    case ppBusSt:
        data = BUS_ST_RDY4TXNOW;
        break;
    case ppRxStat:
        data = rx_stat;
        break;
    case ppRxLength:
        data = rx_packet_size;
        break;
    default:
        data = 0;
        break;
    }
    return data;        // Get the data from the chip
}

/***********************************************************************************
 *
 * Function:    ReadPP8(USHORT ppAdress)
 * Description: This function allows reads to one single byte. This is needed when
 *              Reading the first two bytes of the RxFrame.
 *
 **********************************************************************************/
UCHAR ReadPP8(USHORT ppAdress)
{
//    _portPtr = ppAdress;      // Write the pp Pointer
//    return (UCHAR)_portData;  // Get the data from the chip
    return (UCHAR)ReadPP(ppAdress);
}

/***********************************************************************************
 *
 * Function:    WritePP(USHORT ppAdress, USHORT inData)
 * Description: This function simply writes the supplied data to the given adress
 *              in the Packet Page data base of the CS8900
 *
 **********************************************************************************/

// NOTE: This assumes ppAdress is 16-bit aligned always
//
void WritePP(USHORT ppAdress, USHORT inData)
{
//    _portPtr = ppAdress;    // Write the pp Pointer
//    _portData = inData;     // Write the data to the chip

    if (ppAdress >= ppTxFrame) {
        int offset = ppAdress - ppTxFrame;
/*
        TRACE("WritePP(ppTxData: %04X %c %c) offset: %u\n", inData, \
            isprint(HIBYTE(inData)) ? HIBYTE(inData) : ' ', \
            isprint(LOBYTE(inData)) ? LOBYTE(inData) : ' ', \
            offset);
 */
//        if (offset >= 0 && offset < (tx_packet_size-2)) {
//            tx_buffer[offset >> 1] = inData;
//        } else {
//            // Last byte so send packet
//            tx_buffer[offset >> 1] = inData;
//            WritePacket(offset);
//        }
        if (offset >= 0 && offset < (tx_packet_size)) {
            if (offset < sizeof(tx_buffer)) {
                tx_buffer[offset >> 1] = inData;
            } else {
                TRACE("WritePP(...) ATTEMPT TO WRITE BEYOND BUFFER\n");
            }
        }
        return;
    }
    switch (ppAdress) {
    case ppTxCmd:
        if (inData == TX_CMD_START_ALL) {
//            TRACE("WritePP(ppTxCmd: TX_CMD_START_ALL)\n");
//            tx_packet_cnt = 0;
        } else
        if (inData == TX_CMD_FORCE) {
//            TRACE("WritePP(ppTxCmd: TX_CMD_FORCE) tx_packet_size: %u\n", tx_packet_size);
            WritePacket(tx_packet_size);
        } else {
            TRACE("WritePP(ppTxCmd: UNKNOWN)\n");
        }

        break;
    case ppTxLength:
//        TRACE("WritePP(ppTxLength: %u)\n", inData);
        tx_packet_size = inData;
        break;
    default:
        TRACE("WritePP(default: %04X) - unknown PP address\n", ppAdress);
        break;
    }
}
/*
    TRACE("TXEthernetPacket(%p, %u)\n", PacketStart, PacketLength);
    {
        short i;
        char* data = (char*)PacketStart;
        for (i = 0; i < PacketLength; i++) {
            TRACE(" %02X : %c \n", data[i] & 0x00ff, isprint(data[i]) ? data[i] : ' ');
        }
    }
 */

/***********************************************************************************
 *
 * Function:    VerifyCS8900(void)
 * Description: Insert your own description of this function
 *
 **********************************************************************************/
USHORT VerifyCS8900(void)
{
    USHORT data = 0;
/*
    if ((*(USHORT*)_portPtr != 0x3000) && (ReadPP(ppEISA) != 0x630e)) { // Check CS8900
    } else {
        data = ReadPP(ppProdID) >> 8;   // Get the data
    }
 */
    return data;
}




//        ISQSem |= 0x04; // Generate an Rx Event
////////////////////////////////////////////////////////////////////////////////
