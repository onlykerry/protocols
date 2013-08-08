#ifndef _NETUDP_H_
#define _NETUDP_H_

/**
 * UDP header based on RFC768
 */
typedef struct udphdr{
	u_int16_t	srcPort;		/* source port */
	u_int16_t	dstPort;		/* destination port */
	u_int16_t	length;
	u_int16_t   checksum;
} UDPHdr;

/** Default time-to-live for UDP datagrams */
#define UDPTTL 0x3d

// Application Level functions
extern int udpOpen(void);
extern int udpClose(int ud);
extern int udpConnect(u_int ud, const struct sockaddr_in *remoteAddr, u_char tos);
extern int udpListen(u_int ud, int backLog);
extern int udpBind(u_int ud, struct sockaddr_in *peerAddr);
extern int udpRead(u_int ud, void *buf, long len);
extern int udpWrite(u_int ud, const void *buf, long len);
extern long udpRecvFrom(int ud, void  *buf, long len, struct sockaddr_in *from);
extern long udpSendTo(int ud, const void  *buf, long len, const struct sockaddr_in *to);

// Internal functions, used from within uC/IP
extern void udpInit(void);
extern void udpInput(NBuf *inBuf, u_int ipHeadLen);

#endif /* _NETUDP_H_ */
