/*****************************************************************************
* netsock.c - Network Sockets source file
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
* 2001-05-12 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Original file.
*
*****************************************************************************
*/
#include <string.h>
#include "netconf.h"
#include "netaddrs.h"
#include "netbuf.h"
#include "netether.h"
#include "netarp.h"
#include "netsock.h"
#include "netos.h"

#include <stdio.h>
#include "netdebug.h"

////////////////////////////////////////////////////////////////////////////////

#define MAX_SOCKET 16

tcp_Socket allSocks[MAX_SOCKET];


////////////////////////////////////////////////////////////////////////////////

void socketInit(void)
{
    short i;

    memset(&allSocks, 0, sizeof(allSocks));
    for (i = 0; i < MAX_SOCKET; i++) {
        allSocks[i].state = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////

SOCKET LookupSocket(short type, short protocol)
{
    short i;
    for (i = 0; i < MAX_SOCKET; i++) {
        if (allSocks[i].type == type) {
            if (allSocks[i].protocol == protocol) {
                if (allSocks[i].state != 0) {
                    return i;
                }
            }
        }
    }
    return -1;
}

////////////////////////////////////////////////////////////////////////////////

void SocketArpUpdate(FAR InternetAdress* pIP, FAR EthernetAdress* pEth)
{
    short i;
    TRACE("SocketArpUpdate(%p, %p) ****************************\n", pIP, pEth);
    for (i = 0; i < MAX_SOCKET; i++) {
        if (allSocks[i].state != 0) {
            if (CompareIP(pIP, &allSocks[i].hisIPAddr)) {
                TRACE("SocketArpUpdate(%p, %p) -  updating socket eth addrs\n", pIP, pEth);
                CopyEA(pEth, &allSocks[i].hisEthAddr);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
/*
SOCKET LookupSocketByEthAdr(EthernetAdress ethAdr)
{
    short i;
    for (i = 0; i < MAX_SOCKET; i++) {
        if (allSocks[i].type == type) {
            if (allSocks[i].protocol == protocol) {
                if (allSocks[i].state != 0) {
                    return i;
                }
            }
        }
    }
    return -1;
}
 */
////////////////////////////////////////////////////////////////////////////////

SOCKET socket(long af, short type, short protocol)
{
    short i;
    for (i = 0; i < MAX_SOCKET; i++) {
        if (allSocks[i].state == 0) {
            allSocks[i].protocol = protocol;
//            allSocks[i].protocol = PUDP;
            allSocks[i].type = type;
            allSocks[i].af = af;
            allSocks[i].state = 1;
            return i;
        }
    }
    return -1;
}

////////////////////////////////////////////////////////////////////////////////

short setsockopt(SOCKET s, short level, short optname, const char FAR *optval, short optlen)
{
    TRACE("setsockopt(...)\n");
    if (level == SOL_SOCKET) {
        switch (optname) {
        case SO_RCVTIMEO:
            if (optlen == 4)      allSocks[s].timeout = *(long*)optval;
            else if (optlen == 2) allSocks[s].timeout = *(short*)optval;
            else {
                TRACE("setsockopt() unsupported SO_RCVTIMEO parameter length: %u\n", optlen);
                goto abort;
            }
            return 0;
        default:
            TRACE("setsockopt(%u) unknown socket optname specified.\n", s);
            break;
        }
    } else {
        TRACE("setsockopt(%u) unknown socket level specified.\n", s);
    }
abort:
    return SOCKET_ERROR ;
}

////////////////////////////////////////////////////////////////////////////////

//    struct sockaddr_in sin; 
//    sin.sin_addr.s_addr = inet_addr(addrs);
//    sin.sin_family = AF_INET; 
//    sin.sin_port = htons(port); 
//    if (connect(s, (struct sockaddr*)&sin, sizeof(sin)) == SOCKET_ERROR) { 

u_short nextSourcePort = 978;

//short connect(SOCKET s, const struct sockaddr FAR *name, short namelen)
short connect(SOCKET s, FAR const NetAddr* name, short namelen)
{
    TRACE("connect(...)\n");
    if (s >= 0 && s < MAX_SOCKET) {
//        if (name->na_len == sizeof(sockaddr_in)) {
        if (name->na_len == sizeof(NetAddr)) {

//            FAR const NetAddr* sin = (NetAddr*)name;
            FAR struct sockaddr_in* sin = (FAR struct sockaddr_in*)name;

            allSocks[s].state = 1;
//            CopyIP(&sin->sin_addr, &allSocks[s].hisIPAddr);
            CopyIP(&sin->ipAddr, &allSocks[s].hisIPAddr);
            allSocks[s].hisPort = sin->sin_port;
            allSocks[s].myPort = nextSourcePort++;



//            ArpRequest(&allSocks[s].hisIPAddr);
//short ArpRequest(FAR InternetAdress* pReqIP)


            return 0;
        }
    }
    return SOCKET_ERROR;
}

////////////////////////////////////////////////////////////////////////////////


//short sock_print(short handle, char* buf)
short send(SOCKET s, const char FAR *buf, short len, short flags)
{
    short retval = 0;

    TRACE("send(%s)\n", buf);
    if (len < 0) len = strlen(buf);
    if (s >= 0 && s < MAX_SOCKET) {
//        switch (allSocks[s].protocol) {
        switch (allSocks[s].type) {
        case SOCK_DGRAM:
//            retval = TxUdp(&allSocks[s], buf, len);
            break;
        case SOCK_STREAM:
//            retval = TxTcp(s, buf, len);
            break;
        default:
            break;
        }
    }
    return retval;
}

////////////////////////////////////////////////////////////////////////////////

//short sock_read(short handle, void* buffer, unsigned short count)
short recv(SOCKET s, char FAR *buf, short len, short flags)
{
//    TRACE("recv(%u, ....) - BLOCKING TILL QUIT\n", s);

    delay(250); // simulate return from hard coded socket timeout option
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

short closesocket(SOCKET s)
{
    TRACE("closesocket()\n");
    if (s >= 0 && s < MAX_SOCKET) {
        allSocks[s].state = 0;
        return 0;
    }
    return SOCKET_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
