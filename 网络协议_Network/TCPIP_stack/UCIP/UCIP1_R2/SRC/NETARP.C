/*****************************************************************************
* netarp.c - ARP protocol and cache
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
*(yyyy-mm-dd)
* 2001-05-12 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Original file. Merged with version by Mads in netether.
*
******************************************************************************
* NOTES (PLEASE READ THIS!)
*
* This is the first release of an ARP impl. for uC/IP.
* In its present form it only supports one gateway.
* It only supports one ethernet interface.
* The way etherConfig() works is NOT the best solution. 
*
* Choose number of ARP cache entries and size of ARP cache table in netether.h
* arpExpire should be set to a value like 300s with etherConfig().
* Call arpCleanup() at least every arpExpire seconds.
* Call etherConfig() AFTER netInit() AND AFTER ipSetDefault().
* 
* You must impl. an etherSend(NBuf *) function!!!
*  
* The time(NULL) is the ANSI time function which returns time elapsed in seconds.
* I have made a small implementation of the time function, which is located
* in os.c. It returns time elapsed since first call to time(NULL). This is
* ofcourse only to be used, if the ANSI time() function is not supported by the OS/COMPILER.
*
* All the arp functions is defined static, which I found to be the best solution.
* This design looks similar to the arp impl. found in earlier versions of BSD. 
*
* Since I'm using uC/IP in a single proces, I'm not sure if the mutex semaphore
* works properly or is impl. correctly. Please verify this! This was added very quickly!
*
* Currently there is no support for static IP adresses, but the code is prepared for it.
* Actually you should just add a function which calls arpAlloc and change the
* state of the entry to ARP_FIXED.
*
* NOTICE, if the ARP cache is full, the oldest entry will be freed upon next arpResolve of
* a new IP. If this is not done we have the possibility of not beeing able to
* communicate with any hosts that are not located in the ARP cache.
* To avoid removing the oldest before it really has expired (BAD PERFORMANCE)
* you should be sure to have the worst case of ARP entries available
* (adjust ARP_ENTRIES in netether.h).
*
* Good luck.
* Mads Christiansen (mads@mogi.dk or mc@voxtream.com).
*/
#include "netconf.h"
#include "netaddrs.h"
#include "netbuf.h"
#include "netether.h"
#include "netarp.h"
#include "netsock.h"

#include <string.h>
#ifdef HAVE_ANSI_TIME
#include <time.h>
#endif
#include <stdio.h>
#include "netdebug.h"

#pragma warning (push)
#pragma warning (disable: 4018) // signed/unsigned mismatch


// External setup variables
extern etherSetup mySetup;

// Public variables (this is made public for debugging purposes)
arpStatistics arpStats;

// Internal prototypes
static arpEntry *arpLookup(u_long ip, int create);
static arpEntry *arpAlloc(u_long ip);
static void arpRemove(u_long index);

// ARP cache tables
static arpEntry arpEntries[ARP_ENTRIES];
static arpEntry *arpTable[ARP_TABLE_SIZE];


void arpInit(void)
{
    // Get a lock to the ethernet/arp variables
    etherLock();
    // Cleaning tables ;-)
    memset(arpEntries, 0, sizeof(arpEntries));
    memset(arpTable, 0, sizeof(arpTable));
    // Reset statistics
    memset(&arpStats, 0, sizeof(arpStats));
    etherRelease();
    // Do a Gratuitous ARP
//    arpRequest(htonl(mySetup.localAddr));
}


/* 
 * Go through arpTable and remove any entries that have expired
 * (more than arpExpire time old).
 */
void arpCleanup(void)
{
  u_long index;
  arpEntry **entry;

  etherLock();
  // **** FOR every index in ARP cache table
  for (index = 0; index < ARP_TABLE_SIZE; index++) {
    // Get a pointer to first arpEntry pointer in chain
    entry = &arpTable[index];

    // **** Traverse entries in chain
    while (*entry) {
      // Which state is the found entry in ?
      switch ((*entry)->state) {
        case ARP_RESOLVED:  // **** CASE ARP_RESOLVED
        case ARP_PENDING:   // **** CASE ARP_PENDING
          // Is this entry expired ?
          if ((*entry)->expire < time(NULL)) {
            // Update entries allocated stats
            arpStats.alloc--;
            // Yes it is, so remove it
            (*entry)->state = ARP_EXPIRED;
            // If we have an NBuf chain waiting to be sent free it
            // (unlikely when state is resolved)
            if ((*entry)->packet) nFreeChain((*entry)->packet);
            // Update chain
            *entry = (*entry)->next;
          }
          else
            // **** Go to next entry in chain
            entry = &((*entry)->next);
          break;
        case ARP_FIXED:     // **** CASE ARP_FIXED (pure cosmetic)
        default: 
          // **** Go to next entry in chain
          entry = &((*entry)->next);
      }
    }
  }
  etherRelease();
}


// *************************** INTERNAL FUCTIONS
// ***************** ARP PROTOCOL IMPLEMENTATION
void arpInput(NBuf* pNBuf)
{
    arpPacket* arpPtr;
    arpEntry* entry;

    TRACE("arpInput(%p) chainLen = %u\n", pNBuf, pNBuf->chainLen);
    ASSERT(pNBuf);
    arpPtr = nBUFTOPTR(pNBuf, arpPacket*);

    // Validate ARP packet
//    if ( (pNBuf->chainLen < sizeof(arpPacket) ) ||
    if ( // quickfix by Robert - above test caused ping to fail when introduced ???
         (arpPtr->hwType != htons(1)) ||
         (arpPtr->prType != htons(ETHERTYPE_IP)) ||
         (arpPtr->hwLength != sizeof(mySetup.hardwareAddr)) ||
         (arpPtr->prLength != sizeof(mySetup.localAddr)) ||
         ((arpPtr->operation != htons(ARP_REQUEST)) &&
          (arpPtr->operation != htons(ARP_REPLY))) ) {
      // Update invalid ARP packets received statistics
      arpStats.invalidARPs++;
      nFreeChain(pNBuf);
      return;
    }

    // If Hardware address of sender eq. myHardwareAddr discard packet
    if (!memcmp(arpPtr->ether.src, mySetup.hardwareAddr, sizeof(mySetup.hardwareAddr))) {
//      TRACE("arpInput(%p) - not our IP [%u.%u.%u.%u], dropping\n", pNBuf, pArpPacket->TargetIP.ip1, pArpPacket->TargetIP.ip2, pArpPacket->TargetIP.ip3, pArpPacket->TargetIP.ip4);
      nFreeChain(pNBuf);
      return;
    }

    // If sender IP eq. myIpAddr we have an IP conflict! Log and discard packet.
    if (arpPtr->senderIp == htonl(mySetup.localAddr)) {
      // We have an IP conflict!
      // Update ip conflict statistics
      arpStats.ipConflicts++;
      nFreeChain(pNBuf);
      return;
    }

    // Find senders IP address in ARP cache (with create == TRUE if target IP is our IP)
    entry = arpLookup(htonl(arpPtr->senderIp), (arpPtr->targetIp == htonl(mySetup.localAddr)));
    if (entry) {
    // Update senders HW address in ARP cache
      memcpy(entry->hardware, arpPtr->senderHw, sizeof(entry->hardware));
      // Set ARP host entry to resolved
      entry->state = ARP_RESOLVED;
      // Set new expire time
      entry->expire = time(NULL) + mySetup.arpExpire;
      // **** IF output packet is queued for this host entry THEN
      if (entry->packet) {
      // **** Send it
        etherOutput(entry->packet);
        entry->packet = NULL;
      }
    }

    // If this is not an ARP request discard packet and return
    // If this ARP request is not targeted for us discard packet and return
    if ( (arpPtr->operation != htons(ARP_REQUEST)) ||
         (arpPtr->targetIp  != htonl(mySetup.localAddr)) ) {
      nFreeChain(pNBuf);
      return;
    }

    // We have an ARP request so build an ARP reply with packet and call etherSend
    // ARP
    arpPtr->targetIp = arpPtr->senderIp;
    memcpy(arpPtr->targetHw, arpPtr->senderHw, sizeof(arpPtr->targetHw));
    arpPtr->senderIp = htonl(mySetup.localAddr);
    memcpy(arpPtr->senderHw, mySetup.hardwareAddr, sizeof(arpPtr->senderHw));
    arpPtr->operation = htons(ARP_REPLY);
    // Ether
    memcpy(arpPtr->ether.dst, arpPtr->ether.src, sizeof(arpPtr->ether.dst));
    memcpy(arpPtr->ether.src, mySetup.hardwareAddr, sizeof(arpPtr->ether.src));
  // **** Send ARP request
    etherSend(pNBuf);
}


////////////////////////////////////////////////////////////////////////////////
// Allocate a new ARP cache entry for given IP.
//
static arpEntry *arpAlloc(u_long ip)
{
  u_long traverse;
  u_long index;
  u_long oldest = -1;
  u_long oldestAge = -1;

  // **** FOR every ARP entry (also those that are free) 
  for (traverse = 0; traverse < ARP_ENTRIES; traverse++) {
    // **** IF ARP entry is free THEN
    if (arpEntries[traverse].state == ARP_FREE) {
      // We have a free arp entry, clear it and break;
      memset(&arpEntries[traverse], 0, sizeof(arpEntry));
      // **** BREAK for loop
      break;
    }
  }

  // **** IF we didn't find a free arp entry THEN
  if (traverse >= ARP_ENTRIES) {
    // Update ARP entry needed but not found stat
    arpStats.allocError++;

    // We didn't find any free ARP entry, so we will try to find the oldest one
    // This search for the oldest one is done here, and not in the first for loop 
    // since I wanted to keep the normal succesfull lookup as fast as possible.
    
    // **** Find oldest entry
    for (traverse = 0; traverse < ARP_ENTRIES; traverse++) {
      switch (arpEntries[traverse].state) {
        case ARP_PENDING:
        case ARP_RESOLVED:
          if (arpEntries[traverse].expire < oldestAge) {
            oldestAge = arpEntries[traverse].expire;
            oldest = traverse;
          }
          break;
      }
    }

    // **** IF we didn't find any oldest THEN RETURN NULL
    if (oldest == -1) return NULL;

    // Set traverse to oldest
    traverse = oldest;

    // ***** Remove oldest from ARP cache table
    arpRemove(traverse);

    // We have a "free" arp entry, clear it
    memset(&arpEntries[traverse], 0, sizeof(arpEntry));

    // Update ARP allocated statistics
    arpStats.alloc--;
  }

  // Update ARP allocated statistics
  arpStats.alloc++;
  // Update max. ARP allocated statistics
  if (arpStats.alloc > arpStats.maxAlloc) 
    arpStats.maxAlloc = arpStats.alloc;

  // **** Fill in new ARP entry
  // Fill in new infos...
  arpEntries[traverse].ip = ip;
  // Allow an ARP request to be sent now
  arpEntries[traverse].expire = time(NULL) + mySetup.arpExpire;
  // Since this is a new entry, we assume this ip is to be resolved
  arpEntries[traverse].state = ARP_PENDING;
  // Just in case, no packet queued
  arpEntries[traverse].packet = NULL;
  // Get index
  index = arpEntries[traverse].ip & (ARP_TABLE_SIZE - 1);
  // **** Insert new entry in ARP cache table
  arpEntries[traverse].next = arpTable[index];
  arpTable[index] = &arpEntries[traverse];

//  arpEntries[traverse].next = arpTable[arpEntries[traverse].ip & (ARP_TABLE_SIZE - 1)];
//  arpTable[arpEntries[traverse].ip & (ARP_TABLE_SIZE - 1)] = &arpEntries[traverse];

  TRACE("uCIP: ARP Alloc ADDED NEW IP %d.%d.%d.%d\n", 
      (ip >> 24) & 0xFF,
      (ip >> 16) & 0xFF,
      (ip >> 8)  & 0xFF,
      (ip)       & 0xFF );

  // **** RETURN ARP cache entry allocated
  return &arpEntries[traverse];
}


////////////////////////////////////////////////////////////////////////////////
// Search the ARP cache for given IP, create if not found and specified.
//
static arpEntry *arpLookup(u_long ip, int create)
{
  arpEntry *entry;

  // **** Get first arpEntry
  entry = arpTable[ip & (ARP_TABLE_SIZE - 1)];
  
  // Traverse chain
  while (entry) {
    // Did we find the ip we wanted
    if (entry->ip == ip)
      // Yes, return it
      return entry;

    // **** Go to next entry in chain
    entry = entry->next;
  }

  // We didn't find the ip
  // **** IF we should create a new entry THEN
  if (create)
    // **** RETURN new entry created
    return arpAlloc(ip);

  // **** RETURN no entry found or allocated
  return NULL;
}


int arpRequest(u_long ip)
{
  NBuf* pNBuf;
  int len;
  arpPacket arp;

  // **** Build arp request packet
  // Fill ethernet header
  memset(arp.ether.dst, 0xFF, sizeof(arp.ether.dst));
  memcpy(arp.ether.src, mySetup.hardwareAddr, sizeof(arp.ether.src));
  arp.ether.protocol = htons(ETHERTYPE_ARP);

  // Fill arp header
  arp.hwType    = htons(1);            // HW TYPE IS ETHER
  arp.hwLength  = 6;                   // 6 bytes long hardware address
  arp.prType    = htons(ETHERTYPE_IP); // TYPE IS IP
  arp.prLength  = 4;                   // 4 bytes IP adresses
  arp.operation = htons(ARP_REQUEST);  // ARP request
  arp.senderIp  = mySetup.localAddr;
  memcpy(arp.senderHw, mySetup.hardwareAddr, sizeof(arp.senderHw));
  arp.targetIp  = htonl(ip);

  // Create NBuf with ARP request
  nGET(pNBuf);
  // If not packet is allocated return FALSE
  if (!pNBuf) {
    // Update stat
    arpStats.nbufError++;
    return FALSE;
  }

  // Append prepared ARP Request
  nAPPEND(pNBuf, (void*)&arp, sizeof(arp), len);
  if (len != sizeof(arp)) {
    nFree(pNBuf);
    return FALSE;
  }

  // Send ARP request
  etherSend(pNBuf);

  // **** RETURN ARP request packet sent
  return TRUE;
}


////////////////////////////////////////////////////////////////////////////////
//
int arpResolve(u_long ip, u_char *hardware, NBuf* pNBuf)
{
  arpEntry *entry;

  // Should this packet be sent to the gateway ?
  if ((ip & mySetup.subnetMask) != mySetup.networkAddr) {
    // Yes, we have a destination outside our network
    // Since we only (currently) support one gateway send it there...

    // **** IF we have a gateway defined THEN
    // Do we have a gateway defined ?
    if (mySetup.gatewayAddr)
      // **** Find entry for gateway
      entry = arpLookup(mySetup.gatewayAddr, TRUE);
    else
      // **** No gateway is defined, indicate no entry found for gateway
      entry = NULL;
  } else {
    // **** ELSE Find entry for host on our network
    entry = arpLookup(ip, TRUE);
  }

  if (!entry) {
    // We didn't get an entry!
    if (pNBuf) nFreeChain(pNBuf);
    return FALSE;
  }

//  TRACE("uCIP: ARP Resolve LOOKUP IP OK %d.%d.%d.%d\n", 
//      ((entry)->ip >> 24) & 0xFF, ((entry)->ip >> 16) & 0xFF,
//      ((entry)->ip >> 8)  & 0xFF, ((entry)->ip)       & 0xFF);

  // Did we get a valid hardware address
  if ((entry->state == ARP_RESOLVED) || (entry->state == ARP_FIXED)) {
//    TRACE("uCIP: ARP Resolve IP IS RESOLVED OR FIXED\n"); 

    // Yes! Copy ethernet address
    memcpy(hardware, entry->hardware, sizeof(entry->hardware));
    // Update expire time
    entry->expire = time(NULL) + mySetup.arpExpire;
    return TRUE;
  }

  // No hardware address found, is IP being resolved ?
  if (entry->state == ARP_PENDING) {
    TRACE("uCIP: ARP Resolve IP IS PENDING\n"); 
    // Is it OK to send a new ARP request ?
    if ( entry->expire - mySetup.arpExpire <= time(NULL) ) {
      // Update expire time
      // Allow a new request to be sent after 1 second
      entry->expire = time(NULL) + mySetup.arpExpire + 1;

      TRACE("uCIP: ARP Resolve REQUEST FOR IP %d.%d.%d.%d\n", 
      ((entry)->ip >> 24) & 0xFF,
      ((entry)->ip >> 16) & 0xFF,
      ((entry)->ip >> 8)  & 0xFF,
      ((entry)->ip)       & 0xFF);

      // **** Send ARP request
      arpRequest(entry->ip);
    }

    // If a new packet is to be sent
    if (pNBuf) {
      // Free old queued packet (if any)
      if (entry->packet) nFreeChain(entry->packet);

      // Queue new packet
      entry->packet = pNBuf;
    }
    return FALSE;
  }

  // This is wrong should never get here
  // Cleanup...
  if (pNBuf) nFreeChain(pNBuf);

  // **** RETURN none found (FALSE)
  return FALSE;
}


////////////////////////////////////////////////////////////////////////////////
//
static void arpRemove(u_long index)
{
  arpEntry *entry, *lastEntry = NULL;

  // **** Get first arpEntry
  entry = arpTable[arpEntries[index].ip & (ARP_TABLE_SIZE - 1)];

  // **** IF this is the index we want removed THEN
  if (&arpEntries[index] == entry)
  {
    // **** Remove entry from chain
    arpTable[arpEntries[index].ip & (ARP_TABLE_SIZE - 1)] =
      arpTable[arpEntries[index].ip & (ARP_TABLE_SIZE - 1)]->next;
    entry = NULL;
  }
  else
  {
    // **** Go to next entry
    lastEntry = entry;
    entry = entry->next;
  }

  // **** WHILE entries in chain
  while (entry)
  {
  // **** IF this is the index we want removed THEN
    if (&arpEntries[index] == entry)
    {
      // **** Remove entry from chain
      lastEntry->next = lastEntry->next->next; 
      // **** RETURN
      return;
    }
    else
    {
      // **** Go to next entry
      lastEntry = entry;
      entry = entry->next; 
    }
  }
}

#pragma warning (pop)
////////////////////////////////////////////////////////////////////////////////
