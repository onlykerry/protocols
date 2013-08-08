////////////////////////////////////////////////////////////////////////////////
// hardware.c :
//
#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <process.h>

#include "hardware.h"
#include "packet.h"
#include "main.h"
#include "netaddrs.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////////////////////////////////////////////////////////////////////
// network simulation data

LPADAPTER lpAdapter = 0; // define a pointer to an ADAPTER structure
LPPACKET lpPacket = 0;   // define a pointer to a PACKET structure

////////////////////////////////////////////////////////////////////////////////

void RxEvent_Sim32(USHORT* FramePtr, short length);

////////////////////////////////////////////////////////////////////////////////

int pk_bc_cnt;
int pk_ia_cnt;
int pk_un_cnt;


// parse packet for the broadcast or our virtual ethernet address
void ParsePacket(char* pChar, short caplen, short datalen)
{
    EthernetAdress* pEA = (EthernetAdress*)pChar;

//    TRACE("MAC %02X:%02X:%02X:%02X:%02X:%02X\n", pEA->mac1, pEA->mac2, pEA->mac3, pEA->mac4, pEA->mac5, pEA->mac6);
    if (CompareEA(&bcMAC, (EthernetAdress*)pChar)) {
        pk_bc_cnt++;
//        TRACE("Packet length : %ld - BROADCAST\n", datalen);
        RxEvent_Sim32((USHORT*)pChar, datalen);
    } else if (CompareEA(&myMAC, (EthernetAdress*)pChar)) {
        pk_ia_cnt++;
//        TRACE("Packet length : %ld - DIRECTED\n", datalen);
        RxEvent_Sim32((USHORT*)pChar, datalen);
    } else {
        pk_un_cnt++;
//        TRACE("Packet length : %ld - UNKNOWN\n", datalen);
//        PrintPacket(pChar, caplen, datalen);
    }
}

////////////////////////////////////////////////////////////////////////////////

void EthernetSimulationProc(void* dummy)
{
//    char buffer[256000];    // buffer to hold the data coming from the driver
    char buffer[8096];    // buffer to hold the data coming from the driver
    int AdapterCount = 0;

    TRACE("EthernetSimulationProc Started\n");


    AdapterCount = FindAdaptors();
    TRACE("HostServiceTask: FindAdaptors() returned: %u\n", AdapterCount);
    if (!AdapterCount) {
        TRACE("HostServiceTask aborting - no packet adaptor found.\n");
    }

    lpAdapter = OpenAdaptor(SelectAdaptor(1));
    TRACE("HostServiceTask: OpenAdaptor(1) returned: %p\n", lpAdapter);

    if (lpAdapter == NULL) {
        TRACE("EthernetSimulationProc failed to open adapter - aborting\n");
        return;
    }

    lpPacket = CreatePacket(buffer, sizeof(buffer));
    if (lpPacket == NULL) {
        TRACE("EthernetSimulationProc failed to create packet - aborting\n");
        return;
    }

    TRACE("HostServiceTask: CreatePacket(...) returned: %p\n", lpPacket);

    do { // capture the packets
        if (PacketReceivePacket(lpAdapter, lpPacket, TRUE) == FALSE) {
            TRACE("\nERROR: PacketReceivePacket failed\n\n");
            break;
        }
//        PrintPackets(lpPacket, 5);
//        EnumPackets(lpPacket, &PrintPacket);
        EnumPackets(lpPacket, &ParsePacket);
//        Sleep(50);
    } while (repeat);

    CloseAdaptor(lpAdapter);
    ReleasePacket(lpPacket);
    TRACE("EthernetSimulationProc Exiting\n");
}

////////////////////////////////////////////////////////////////////////////////

short StartHardwareSim(void)
{
    _beginthread(EthernetSimulationProc, 0, NULL);
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
