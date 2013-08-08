/**	
 *  @file main.c
 *
 *  Sample main function for compiling NULL OS test set used by uCIP.
 *    
 *  Use this as a starting point to adapt uCIP 
 *  to your own operating system.
 */
//#include "os.h"
#include "..\netconf.h"
#include "..\netbuf.h"
#include "..\net.h"
#include "..\netaddrs.h"
#include "..\netether.h"
#include "..\netarp.h"
#include "..\netsock.h"

#include "..\netifdev.h"
#include "..\if_dev\cs89x\if_cs89x.h"
#include "..\neteth.h"

#include <string.h>
#include <stdio.h>
#include "..\netdebug.h"

int repeat = 1;
Interface EthAdaptor;


void StartupNet(void)
{
    etherSetup setup;
    setup.arpExpire = 600;
    memcpy(&setup.hardwareAddr, &myMAC, sizeof(setup.hardwareAddr));
    memcpy(&setup.localAddr, &myIP, sizeof(setup.localAddr));
    setup.localAddr = ntohl(inet_addr("192.168.0.10"));
    setup.subnetMask = ntohl(inet_addr("255.255.255.0"));
    setup.gatewayAddr = ntohl(inet_addr("192.168.0.1"));

//    etherInit();             // call etherInit() before netInit()
//    etherConfig(&setup);
    nBufInit();
    netInit();
//    CS89X_DriverEntry(&EthAdaptor);
//void ipSetDefault(u_int32_t localHost, u_int32_t IPAddr, IfType ifType, int ifID)
    ipSetDefault(htonl(setup.localAddr), 0, IFT_ETH, 0);
    etherInit();
    ethInit(&EthAdaptor);
    etherConfig(&setup);
    arpInit();
    socketInit();
}


int	main()
{
    printf("################## uC/OS ###################\n");
    StartupNet();
    printf("done.\n");
}

