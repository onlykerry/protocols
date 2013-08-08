////////////////////////////////////////////////////////////////////////////////
// tcpecho.c :
//
#include <stdio.h>
#include "netconf.h"
#include "net.h"
#include "netbuf.h"
#include "nettcp.h"

#include "tcpecho.h"
#include "main.h"


////////////////////////////////////////////////////////////////////////////////
/*
extern int tcpOpen(void);
extern int tcpClose(int ud);
extern int tcpConnect(u_int ud, const struct sockaddr_in *remoteAddr, u_char tos);
extern int tcpListen(u_int ud, int backLog);
extern int tcpBind(u_int ud, struct sockaddr_in *peerAddr);
extern int tcpRead(u_int ud, void *buf, long len);
extern int tcpWrite(u_int ud, const void *buf, long len);
extern long tcpRecvFrom(int ud, void  *buf, long len, struct sockaddr_in *from);
extern long tcpSendTo(int ud, const void  *buf, long len, const struct sockaddr_in *to);
 */

void TcpEchoTask(void* dummy)
{
    int bytes;
    int socket;
    struct sockaddr_in sockAddr;
    char buffer[512];

    TRACE("TcpEchoTask Started\n");

    sockAddr.ipAddr = INADDR_ANY;
    sockAddr.sin_port = htons(8);
    sockAddr.sin_family = AF_INET;

    socket = tcpOpen();
    tcpBind(socket, &sockAddr);
    do {
        bytes = tcpRead(socket, &buffer, sizeof(buffer));
        if (bytes > 0) {
            TRACE("TcpEchoTask Received Data From Socket !\n");
            tcpWrite(socket, buffer, bytes);
        }
        delay(50);

    } while (repeat);

    tcpClose(socket);

    TRACE("TcpEchoTask Exiting\n");
}

////////////////////////////////////////////////////////////////////////////////
