/*****************************************************************************
* netsock.h - Network Sockets header file
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
#ifndef _NETSOCK_H_
#define _NETSOCK_H_

#include "netaddrs.h"
#include "netbuf.h"
#include "net.h"
#include "netsockd.h"


/////////////////////////////////////////////////////////////////////////////
#ifndef SOCKET_ERROR

#define SOCKET_ERROR -1
#define SOCK_STREAM   1  // Provides sequenced, reliable, two-way, connection-based byte streams with an OOB data transmission mechanism. Uses TCP for the Internet address family. 
#define SOCK_DGRAM    2  // Supports datagrams, which are connectionless, unreliable buffers of a fixed (typically small) maximum length. Uses UDP for the Internet address family.  


#define SOL_SOCKET    4
#define SO_RCVTIMEO   5

typedef short SOCKET;


void socketInit(void);
void SocketArpUpdate(FAR InternetAdress* pIP, FAR EthernetAdress* pEth);


SOCKET socket(long af, short type, short protocol);
short setsockopt(SOCKET s, short level, short optname, const char FAR *optval, short optlen);

//short connect(SOCKET s, const struct sockaddr FAR *name, short namelen);
short connect(SOCKET s, FAR const NetAddr* name, short namelen);

short send(SOCKET s, const char FAR *buf, short len, short flags);
short recv(SOCKET s, char FAR *buf, short len, short flags);
short closesocket(SOCKET s);


/*
int recvfrom(SOCKET s, char FAR * buf, int len, int flags, struct sockaddr FAR *from, int FAR * fromlen);
int sendto(SOCKET s, const char FAR * buf, int len, int flags, const struct sockaddr FAR *to, int tolen);

SOCKET accept(SOCKET s, struct sockaddr FAR *addr, int FAR *addrlen);
int bind(SOCKET s, const struct sockaddr FAR *addr, int namelen);
int listen(SOCKET s, int backlog);

int ioctlsocket(SOCKET s, long cmd, u_long FAR *argp);
int select(int nfds, fd_set FAR *readfds, fd_set FAR *writefds, fd_set FAR *exceptfds, const struct timeval FAR *timeout);
int shutdown(SOCKET s, int how);


int getpeername(SOCKET s, struct sockaddr FAR *name, int FAR * namelen);
int getsockname(SOCKET s, struct sockaddr FAR *name, int FAR * namelen);
int getsockopt(SOCKET s, int level, int optname, char FAR * optval, int FAR *optlen);
u_long htonl(u_long hostlong);
u_short htons(u_short hostshort);
unsigned long inet_addr(const char FAR * cp);
char FAR * inet_ntoa(struct in_addr in);
u_long ntohl(u_long netlong);
u_short ntohs(u_short netshort);


// Database function prototypes
struct hostent FAR * gethostbyaddr(const char FAR * addr, int len, int type);
struct hostent FAR * gethostbyname(const char FAR * name);
int gethostname(char FAR * name, int namelen);
struct servent FAR * getservbyport(int port, const char FAR * proto);
struct servent FAR * getservbyname(const char FAR * name, const char FAR * proto);
struct protoent FAR * getprotobynumber(int proto);
struct protoent FAR * getprotobyname(const char FAR * name);
 */

#endif

#endif // _NETSOCK_H_
////////////////////////////////////////////////////////////////////////////////
