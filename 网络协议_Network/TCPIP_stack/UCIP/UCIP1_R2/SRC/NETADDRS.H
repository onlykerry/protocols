/*****************************************************************************
* netaddrs.h
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
* Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
* 2001-04-05  initial IP and MAC address support routines.
*
*****************************************************************************/
#ifndef _NETADDRS_H_
#define _NETADDRS_H_


//
// Structures used to manage Ethernet communication
//
typedef struct {
    unsigned char mac1;
    unsigned char mac2;
    unsigned char mac3;
    unsigned char mac4;
    unsigned char mac5;
    unsigned char mac6;
} EthernetAdress;

//
// IPV4 simple address structure
//
typedef struct {
    unsigned char ip1;
    unsigned char ip2;
    unsigned char ip3;
    unsigned char ip4;
} InternetAdress;


u_char CompareIP(FAR InternetAdress* ip1, FAR InternetAdress* ip2);
void CopyEA(FAR EthernetAdress* SourcePtr, FAR EthernetAdress* DestPtr);
void CopyIP(FAR InternetAdress* SourcePtr, FAR InternetAdress* DestPtr);
u_char CompareEA(FAR EthernetAdress* ea1, FAR EthernetAdress* ea2);


extern EthernetAdress bcMAC;    // BROADCAST MAC
extern EthernetAdress nullMAC;  // EMPTY MAC address
extern EthernetAdress myMAC;    // MAC of this device
extern InternetAdress myIP;     // IP of this device


#endif // _NETADDRS_H_
////////////////////////////////////////////////////////////////////////////////
