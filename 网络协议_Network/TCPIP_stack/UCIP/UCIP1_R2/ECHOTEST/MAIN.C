////////////////////////////////////////////////////////////////////////////////
// main.c :
//
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <stdio.h>
#include <process.h>
#include <conio.h>
#include "main.h"
#include "netconf.h"
#include "netbuf.h"
#include "net.h"
#include "netaddrs.h"
#include "netether.h"
#include "netarp.h"
#include "netsock.h"
#include "netip.h"
#include "netifdev.h"
#include "neteth.h"
#include "InetAddr.h"
#include "if_dev\cs89x\if_cs89x.h"

//#include "trace.h"
#include "hardware.h"
#include "udpecho.h"
#include "tcpecho.h"

////////////////////////////////////////////////////////////////////////////////

int repeat = 1;
Interface EthAdaptor;

////////////////////////////////////////////////////////////////////////////////

void StartupNet(void)
{
    etherSetup setup;
    setup.arpExpire = 600;
    memcpy(&setup.hardwareAddr, &myMAC, sizeof(setup.hardwareAddr));
    memcpy(&setup.localAddr, &myIP, sizeof(setup.localAddr));
    setup.localAddr = ntohl(inet_addr("192.168.0.10"));
    setup.subnetMask = ntohl(inet_addr("255.255.255.0"));
    setup.gatewayAddr = ntohl(inet_addr("192.168.0.1"));

    printf("Initialising Network Subsystem...\n");
//    etherInit();             // call etherInit() before netInit()
//    etherConfig(&setup);
    nBufInit();
    netInit();
    CS89X_DriverEntry(&EthAdaptor);
//void ipSetDefault(u_int32_t localHost, u_int32_t IPAddr, IfType ifType, int ifID)
    ipSetDefault(htonl(setup.localAddr), 0, IFT_ETH, 0);
    etherInit();
    ethInit(&EthAdaptor);
    etherConfig(&setup);
    arpInit();
    socketInit();
}


////////////////////////////////////////////////////////////////////////////////

int Startup(void)
{
    StartupNet();  // EtherLib library startup

    OSInit();
    printf("Starting Network Tasks...\n");
    OSTaskCreate(UdpEchoTask, NULL, NULL, 1);
    OSTaskCreate(TcpEchoTask, NULL, NULL, 2);

    printf("Starting System Tasks...\n");
//    OSStart();
    return 1;
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	printf("uC/IP Stack test sample\n");
	if (!StartHardwareSim()) {
		printf("ERROR: Failed to start all hardware simulation threads\n");
		return FALSE;
	}
	if (!Startup())	{
		printf("ERROR: Failed to start all tasks\n");
		return FALSE;
	}
    printf("running...\n");
    while (!kbhit()) {
        Sleep(250);
    }
    printf("signaling shutdown...\n");
    repeat = 0;
    Sleep(2500);
    printf("done.\n");
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
