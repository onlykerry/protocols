////////////////////////////////////////////////////////////////////////////////
// udpecho.c :
//
#include <stdio.h>
#include "netconf.h"
#include "net.h"
#include "netbuf.h"
#include "netudp.h"

#include "udpecho.h"
#include "main.h"


////////////////////////////////////////////////////////////////////////////////
/*
extern int udpOpen(void);
extern int udpClose(int ud);
extern int udpConnect(u_int ud, const struct sockaddr_in *remoteAddr, u_char tos);
extern int udpListen(u_int ud, int backLog);
extern int udpBind(u_int ud, struct sockaddr_in *peerAddr);
extern int udpRead(u_int ud, void *buf, long len);
extern int udpWrite(u_int ud, const void *buf, long len);
extern long udpRecvFrom(int ud, void  *buf, long len, struct sockaddr_in *from);
extern long udpSendTo(int ud, const void  *buf, long len, const struct sockaddr_in *to);
 */

void UdpEchoTask(void* dummy)
{
    int bytes;
    int socket;
    struct sockaddr_in sockAddr;
    char buffer[512];

    TRACE("UdpEchoTask Started\n");

    sockAddr.ipAddr = INADDR_ANY;
    sockAddr.sin_port = htons(7);
    sockAddr.sin_family = AF_INET;

    socket = udpOpen();
    udpBind(socket, &sockAddr);
    do {
        bytes = udpRead(socket, &buffer, sizeof(buffer));
        if (bytes > 0) {
            TRACE("UdpEchoTask Received Data From Socket !\n");
            udpWrite(socket, buffer, bytes);
        }
        delay(50);

    } while (repeat);

    udpClose(socket);

    TRACE("UdpEchoTask Exiting\n");
}

////////////////////////////////////////////////////////////////////////////////
