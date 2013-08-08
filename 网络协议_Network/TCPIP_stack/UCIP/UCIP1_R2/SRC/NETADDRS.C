/*****************************************************************************
* netaddrs.c
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

#include "netconf.h"
//#define HOME
#include "netaddrs.h"



EthernetAdress bcMAC   = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};   // BROADCAST MAC
EthernetAdress nullMAC = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
EthernetAdress myMAC   = {0x55, 0x55, 0x55, 0x12, 0x34, 0x56};
InternetAdress myIP    = {192,  168,  0,    10};


/***********************************************************************************
 *
 * Function:    CompareIP(InternetAdress* ip1, InternetAdress* ip2)
 * Description: Compare two IP adresses and return true or false
 *
 **********************************************************************************/
u_char CompareIP(FAR InternetAdress* ip1, FAR InternetAdress* ip2)
{
    if ((ip1->ip1 == ip2->ip1) && (ip1->ip2 == ip2->ip2) && 
        (ip1->ip3 == ip2->ip3) && (ip1->ip4 == ip2->ip4))
        return(1);
    else
        return(0);
}

/***********************************************************************************
 *
 * Function:    CompareEA(EthernetAdress* ea1, EthernetAdress* ea2)
 * Description: Compare two Ethernet Adresses (MAC) and return true if equal.
 *
 **********************************************************************************/
u_char CompareEA(FAR EthernetAdress* ea1, FAR EthernetAdress* ea2)
{
    if ((ea1->mac1 == ea2->mac1) && (ea1->mac2 == ea2->mac2) &&
        (ea1->mac3 == ea2->mac3) && (ea1->mac4 == ea2->mac4) &&
        (ea1->mac5 == ea2->mac5) && (ea1->mac6 == ea2->mac6))
        return(1);
    else
        return(0);
}

/***********************************************************************************
 *
 * Function:    CopyEA(EthernetAdress* SourcePtr, EthernetAdress* DestPtr)
 * Description: This function simply copies the Ethernet adress from the source
 *              pointer to the destination pointer.
 *
 **********************************************************************************/
void CopyEA(FAR EthernetAdress* SourcePtr, FAR EthernetAdress* DestPtr)
{
    DestPtr->mac1 = SourcePtr->mac1;
    DestPtr->mac2 = SourcePtr->mac2;
    DestPtr->mac3 = SourcePtr->mac3;
    DestPtr->mac4 = SourcePtr->mac4;
    DestPtr->mac5 = SourcePtr->mac5;
    DestPtr->mac6 = SourcePtr->mac6;
}

/***********************************************************************************
 *
 * Function:    CopyIP(InternetAdress *SourcePtr, InternetAdress *DestPtr)
 * Description: This function copies the internet adress from the source pointer to
 *              the destination pointer.
 *
 **********************************************************************************/
void CopyIP(FAR InternetAdress* SourcePtr, FAR InternetAdress* DestPtr)
{
    DestPtr->ip1 = SourcePtr->ip1;
    DestPtr->ip2 = SourcePtr->ip2;
    DestPtr->ip3 = SourcePtr->ip3;
    DestPtr->ip4 = SourcePtr->ip4;
}

////////////////////////////////////////////////////////////////////////////////
