#
#	MAKEFILE for uC/IP 1.0
#
#	This makes the objects only.
#

# Choose the operating system.
IF = ./if_dev/ne2k
OS = ./OS_WIN32

CFLAGS = -I$(OS)
CC = gcc

NET_OBJS = \
       InetAddr.o \
       net.o \
       netaddrs.o \
       netarp.o \
       netauth.o \
       netbuf.o \
       netchap.o \
       netchat.o \
       netchpms.o \
       netdebug.o \
       netether.o \
       netfsm.o \
       nethelp.o \
       neticmp.o \
       netip.o \
       netipcp.o \
       netlcp.o \
       netmagic.o \
       netmd5.o \
       netpap.o \
       netppp.o \
       netrand.o \
       netsock.o \
       netsocka.o \
       nettcp.o \
       nettimer.o \
       netudp.o \
       netvj.o \
       $(IF)/if_ne2kd.c \
       $(IF)/if_os.c \
       $(OS)/ucos.c \
       $(OS)/ucos32.c \
       $(OS)/trace.c


all:	$(NET_OBJS)
	$(CC) $(NET_OBJS) -o $(OS)/ucip



# Cleanup
clean:
	del *.o

