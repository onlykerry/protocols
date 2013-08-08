/*****************************************************************************
* netsocka.c - Network Sockets Assistant source file
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
#include "netconf.h"
#include "netsocka.h"
#include "InetAddr.h"
#include "net.h"

#include <stdio.h>
#include "netdebug.h"

////////////////////////////////////////////////////////////////////////////////

SOCKET OpenSocket(char* addrs, u_short port, unsigned long timeout)
{
    SOCKET s;
    struct sockaddr_in sin; 

    s = socket(AF_INET, SOCK_DGRAM, 0); 
    if (s < 0) {
        TRACE("OpenSocket(...) socket(...) failed, rc=%d\n", TaskGetLastError()); 
        return -1;
    }
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    sin.sin_len = sizeof(struct sockaddr_in);
    sin.sin_addr.s_addr = inet_addr(addrs);
    sin.sin_family = AF_INET; 
    sin.sin_port = htons(port); 
    if (connect(s, (struct sockaddr*)&sin, sizeof(sin)) == SOCKET_ERROR) { 
        TRACE("OpenSocket(...) connect(...) failed, rc=%d\n", TaskGetLastError()); 
        closesocket(s);
        return -1;
    } 
    return s;
}

////////////////////////////////////////////////////////////////////////////////
