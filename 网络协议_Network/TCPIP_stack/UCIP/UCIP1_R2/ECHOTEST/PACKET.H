////////////////////////////////////////////////////////////////////////////////
// packet.c - packet driver helper functions header file.
//
#ifndef __PACKET_H__
#define __PACKET_H__

#include "packet32.h"

#ifdef __cplusplus
extern "C" {
#endif

int FindAdaptors(void);
char* SelectAdaptor(int adapter);
char* SelectAdaptorPrompt(int AdapterCount);
LPADAPTER OpenAdaptor(char* name);

int CloseAdaptor(LPADAPTER lpAdapter);
LPPACKET CreatePacket(char* buffer, int buflen);
int ReleasePacket(LPPACKET lpPacket);

void PrintPackets(LPPACKET lpPacket, ULONG maxlines);
void PrintPacket(char* pChar, short caplen, short datalen);
void EnumPackets(LPPACKET lpPacket, void (*Packet_Handler)(char*, short, short));

#ifdef __cplusplus
}
#endif

#endif // __PACKET_H__
////////////////////////////////////////////////////////////////////////////////
