////////////////////////////////////////////////////////////////////////////////
// Packet.c - packet driver helper functions source file.
//
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <time.h>
#include "packet.h"

////////////////////////////////////////////////////////////////////////////////

#define SIMULTANEOU_READS 10
#define MAX_ETHERNET_FRAME_SIZE 1514
#define Max_Num_Adapter 10

// Prototypes
char AdapterList[Max_Num_Adapter][1024];
int AdapterCount = 0;

////////////////////////////////////////////////////////////////////////////////

int FindAdaptors(void)
{
    int i;
    DWORD dwVersion;
    DWORD dwWindowsMajorVersion;
    //unicode strings (winnt)
    WCHAR AdapterName[512]; // string that contains a list of the network adapters
    WCHAR *temp, *temp1;
    //ascii strings (win95)
    char AdapterNamea[512]; // string that contains a list of the network adapters
    char *tempa, *temp1a;
    ULONG AdapterLength;
  
    memset(AdapterName, 0, sizeof(AdapterName));

    // obtain the name of the adapters installed on this machine
    AdapterLength = 512;
//    printf("Adapters installed:\n");
    i = 0;    
    // the data returned by PacketGetAdapterNames is different in Win95 and in WinNT.
    // We have to check the os on which we are running
    dwVersion = GetVersion();
    dwWindowsMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)));
    if (!(dwVersion >= 0x80000000 && dwWindowsMajorVersion >= 4)) {  // Windows NT
        PacketGetAdapterNames((char*)AdapterName, &AdapterLength);
        temp = AdapterName;
        temp1 = AdapterName;
        while ((*temp != '\0') || (*(temp-1) != '\0')) {
            if (*temp == '\0') {
                memcpy(AdapterList[i], temp1, (temp-temp1)*2);
                temp1 = temp+1;
                i++;
            }
            temp++;
        }
        AdapterCount = i;
//        for (i = 0; i < AdapterCount; i++) wprintf(L"\n%d- %s\n", i+1, AdapterList[i]);
//        printf("\n");
    } else {   //windows 95
        PacketGetAdapterNames(AdapterNamea, &AdapterLength);
        tempa = AdapterNamea;
        temp1a = AdapterNamea;
        while ((*tempa != '\0') || (*(tempa-1) != '\0')) {
            if (*tempa == '\0') {
                memcpy(AdapterList[i], temp1a, tempa-temp1a);
                temp1a = tempa+1;
                i++;
            }
            tempa++;
        }
        AdapterCount = i;
//        for (i = 0; i < AdapterCount; i++) printf("\n%d- %s\n", i+1, AdapterList[i]);
//        printf("\n");
    }
    return AdapterCount;
}

////////////////////////////////////////////////////////////////////////////////

char* SelectAdaptorPrompt(int AdapterCount)
{
    int i, which;

    printf("Adapters installed:\n");
    for (i = 0; i < AdapterCount; i++) {
//        printf("\n%d- %s\n", i+1, AdapterList[i]);
        wprintf(L"\n%d- %s\n", i+1, AdapterList[i]);
    }
    printf("\n");

    do {
        printf("Select the number of the adapter to open : ");
        scanf("%d", &which);
        if (which > AdapterCount) 
            printf("\nThe number must be smaller than %d", AdapterCount); 
    } while (which > AdapterCount);

    return AdapterList[which-1];
}

////////////////////////////////////////////////////////////////////////////////

char* SelectAdaptor(int adapter)
{
    if (!adapter || adapter > AdapterCount) {
        return NULL;
    }
    return AdapterList[adapter-1];
}

////////////////////////////////////////////////////////////////////////////////

LPADAPTER OpenAdaptor(char* name)
{
    //define a pointer to an ADAPTER structure
    DWORD dwErrorCode;
    LPADAPTER lpAdapter = 0;

    lpAdapter = PacketOpenAdapter(name);
//    wprintf(L"\n%s\n", name);
//    wprintf(L"\n%s\n", AdapterList[0]);
//    lpAdapter = PacketOpenAdapter(AdapterList[0]);

    if (!lpAdapter || (lpAdapter->hFile == INVALID_HANDLE_VALUE)) {
        dwErrorCode = GetLastError();
        printf("Unable to open the driver, Error Code : %lx\n", dwErrorCode); 
        return NULL;
    }   
    // set the network adapter in promiscuous mode
    PacketSetHwFilter(lpAdapter, NDIS_PACKET_TYPE_PROMISCUOUS);
    // set a 512K buffer in the driver
    PacketSetBuff(lpAdapter, 512000);
    return lpAdapter;
}

int CloseAdaptor(LPADAPTER lpAdapter)
{
    struct bpf_stat stat;
  
    // print the capture statistics
    PacketGetStats(lpAdapter, &stat);
    printf("\n\n%d packets received.\n%d Packets lost", stat.bs_recv, stat.bs_drop);
    // close the adapter and exit
    PacketCloseAdapter(lpAdapter);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

LPPACKET CreatePacket(char* buffer, int buflen)
{
    LPPACKET lpPacket = 0;

    // allocate and initialize a packet structure that will be used to
    // receive the packets.
    // Notice that the user buffer is only 256K to save memory.
    // For best capture performances a buffer of 512K
    // (i.e the same size of the kernel buffer) can be used.
    if ((lpPacket = PacketAllocatePacket()) == NULL) {
        printf("\nError:failed to allocate the LPPACKET structure.");
        return NULL;
    }
    PacketInitPacket(lpPacket, (char*)buffer, buflen);
    return lpPacket;
}

int ReleasePacket(LPPACKET lpPacket)
{
    PacketFreePacket(lpPacket);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

// this function prints the content of a block of packets received from the driver
void PrintPackets(LPPACKET lpPacket, ULONG maxlines)
{
    ULONG i, j;
    ULONG ulLines;
    ULONG ulen;
    char* pChar;
    char* pLine;
    char* base;
    u_int caplen, datalen;
    struct bpf_hdr* hdr;
   
    ULONG ulBytesReceived = lpPacket->ulBytesReceived;
    char* buf = (char*)lpPacket->Buffer;
    u_int off = 0;

    while (off < ulBytesReceived) {
        if (kbhit()) return;
        hdr = (struct bpf_hdr *)(buf+off);
        datalen = hdr->bh_datalen;
        caplen = hdr->bh_caplen;
        printf("Packet length : %ld\n", datalen);
        off += hdr->bh_hdrlen;
        ulLines = (caplen + 15) / 16;
        if (maxlines)
            if (ulLines > maxlines) 
                ulLines = maxlines;
        pChar = (char*)(buf+off);
        base = pChar;
        off = Packet_WORDALIGN(off+datalen);
        for (i = 0; i < ulLines; i++) {
            pLine = pChar;
            printf("%08lx : ", pChar-base);
            ulen = caplen;
            ulen = (ulen > 16) ? 16 : ulen;
            caplen -= ulen;
            for (j = 0; j < ulen; j++)
                printf("%02x ", *(BYTE*)pChar++);
            if (ulen < 16 )
                printf("%*s", (16 - ulen) * 3, " ");
            pChar = pLine;
            for (j = 0; j < ulen; j++, pChar++)
                printf("%c", isprint(*pChar) ? *pChar : '.');
            printf("\n");
        } 
        printf("\n");
    }
} 

////////////////////////////////////////////////////////////////////////////////

void PrintPacket(char* pChar, short caplen, short datalen)
{
    short i, j;
    char* base = pChar;
    short ulLines = (caplen + 15) / 16;

    printf("Packet length : %ld\n", datalen);
    if (ulLines > 5) ulLines = 5;
    for (i = 0; i < ulLines; i++) {
        char* pLine = pChar;
        short ulen = caplen;
        ulen = (ulen > 16) ? 16 : ulen;
        caplen -= ulen;

        printf("%08lx : ", pChar-base);
        for (j = 0; j < ulen; j++)
            printf("%02x ", *(BYTE*)pChar++);
        if (ulen < 16)
            printf("%*s", (16 - ulen) * 3, " ");
        pChar = pLine;
        for (j = 0; j < ulen; j++, pChar++)
            printf("%c", isprint(*pChar) ? *pChar : '.');
        printf("\n");
    } 
    printf("\n");
} 

////////////////////////////////////////////////////////////////////////////////

void EnumPackets(LPPACKET lpPacket, void (*Packet_Handler)(char*, short, short))
{
    char buffer[8096];
    char* pBuf = buffer;
    char* pChar;
//    char* base;
    ULONG ulBytesReceived = lpPacket->ulBytesReceived;
    char* buf = (char*)lpPacket->Buffer;
    u_int off = 0;

//    printf("EnumPackets - ulBytesReceived: %ld ", ulBytesReceived);
    while (off < ulBytesReceived) {
        struct bpf_hdr* hdr = (struct bpf_hdr *)(buf+off);
        u_int datalen = hdr->bh_datalen;
        u_int caplen = hdr->bh_caplen;
//        printf("    datalen %u, caplen %u", datalen, caplen);
        off += hdr->bh_hdrlen;
        pChar = (char*)(buf+off);
//        base = pChar;
        off = Packet_WORDALIGN(off+datalen);
//        Packet_Handler(pChar, caplen, datalen);
        memcpy(pBuf, pChar, datalen);
        pBuf += datalen;
    }
//    printf("\n");
    Packet_Handler(buffer, 0, (short)ulBytesReceived);
}

////////////////////////////////////////////////////////////////////////////////

/*
int pcap_read(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	int cc;
	int n = 0;
	register u_char *bp, *ep;

	cc = p->cc;
	if (p->cc == 0) {
	    // capture the packets
		if(PacketReceivePacket(p->adapter,p->Packet,TRUE)==FALSE){
			sprintf(p->errbuf, "read error: PacketReceivePacket failed");
			return (-1);
		}
		cc = p->Packet->ulBytesReceived;
		bp = p->buffer;
    } else {
        bp = p->bp;
    }
	// Loop through each packet.
#define bhp ((struct bpf_hdr *)bp)
	ep = bp + cc;
	while (bp < ep) {
		register int caplen, hdrlen;
		caplen = bhp->bh_caplen;
		hdrlen = bhp->bh_hdrlen;

		// XXX A bpf_hdr matches a pcap_pkthdr.
		(*callback)(user, (struct pcap_pkthdr*)bp, bp + hdrlen);
		bp += BPF_WORDALIGN(caplen + hdrlen);
		if (++n >= cnt && cnt > 0) {
			p->bp = bp;
			p->cc = ep - bp;
			return (n);
		}
	}
#undef bhp
	p->cc = 0;
	return (n);
}
 */
