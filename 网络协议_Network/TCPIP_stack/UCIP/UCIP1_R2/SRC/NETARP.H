/*****************************************************************************
* netarp.h - ARP protocol and cache
*
* portions Copyright (c) 2001 by Cognizant Pty Ltd.
* portions Copyright (c) 2001 by Partner Voxtream A/S.
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
*(dd-mm-yyyy)
* 12-05-2001 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Original file. Merged with version by Mads in netether.
*
******************************************************************************
*/
#ifndef NETARP_H
#define NETARP_H

#define ARP_ENTRIES 50                // ARP cache size
#define ARP_TABLE_SIZE (2 << (6-1))   // ARP cache lookup table
                                      // Room for 2^6 = 64 entries in first hit.
                                      // THIS MUST BE AN 2^n NUMBER!!!!!
// ARP operation defines
#define ARP_REQUEST    1
#define ARP_REPLY      2
#define RARP_REQUEST   3
#define RARP_REPLY     4

#define ARP_ETHER      1

// ARP packet with ethernet header (see RFC 826 - ARP)
typedef struct
{
  etherHdr ether;
  u_short hwType;
  u_short prType;
  u_char  hwLength;
  u_char  prLength;
  u_short operation;
  u_char  senderHw[6];
  u_long  senderIp;
  u_char  targetHw[6];
  u_long  targetIp;
} arpPacket;


// ARP states for entries
typedef enum
{
  ARP_FREE = 0,
  ARP_EXPIRED = ARP_FREE,
  ARP_PENDING,
  ARP_RESOLVED,
  ARP_FIXED
} arpState;


// ARP entry
typedef struct arpEntryTag
{
  u_long   ip;
  u_char   hardware[6];
  u_long   expire;            
  arpState state;
  NBuf     *packet;
  struct arpEntryTag *next;
} arpEntry;


// ARP statistics (simple impl.)
typedef struct {
    u_long allocError;	 // # of entries that could not be allocated 
    u_long alloc;		 // # of entries in arpTable 
    u_long maxAlloc;	 // Max. # of entries ever in arpTable 
    u_long nbufError;	 // # of times we tried to get a new nBuf but failed to do so 
    u_long ipConflicts;  // Number of IP conflicts detected
    u_long invalidARPs;  // Number of invalid ARP packets received
} arpStatistics;


/*
 * arpCleanup
 *
 * Goes through the ARP cache table to remove any expired entries.
 * This function should be called at least arpExpire time (se etherSetup).
 */
void arpCleanup(void); // Call this every 5 minutes


// Prototypes
int arpResolve(u_long ip, u_char* hardware, NBuf* pNBuf);
int arpRequest(u_long ip);
void arpInput(NBuf* pNBuf);
void arpInit(void);


/*
///////////////////////////////////////////////////////////////////////////////
//  Additional stuff by Robert. TODO: remove
//
typedef struct {
	EthernetAdress DestAdr;
	EthernetAdress SourceAdr;
	u_short FrameType;

	u_short HardType;
	u_short ProtType;
	u_char HardSize;
	u_char ProtSize;
	u_short Op;
	EthernetAdress SenderAdr;
	InternetAdress SenderIP;
	EthernetAdress TargetAdr;
	InternetAdress TargetIP;
} ArpPacket;


//#define ARP_HEADER_LENGTH  sizeof(ArpPacket); // 
//#define ARP_HEADER_LENGTH  42 

void RxArp(NBuf* pNBuf); // TO GO
short ArpRequest(FAR InternetAdress* pReqIP);
short AddArpCache(FAR InternetAdress* pIP, FAR EthernetAdress* pEth);
short ArpLookup(FAR InternetAdress* pReqIP, FAR EthernetAdress* pReqEth);
 */

#endif // NETARP_H

