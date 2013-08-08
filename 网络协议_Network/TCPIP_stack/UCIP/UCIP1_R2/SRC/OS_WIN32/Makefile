#
#	MAKEFILE for uC/IP 1.0
#
#	This makes the objects only.
#

# Choose the operating system.
IF = ./../if_dev/ne2k
OS = ./OS_WIN32
UCIP = ./..

CFLAGS = -I$(OS)
CC = gcc

NET_OBJS = \
       $(UCIP)/InetAddr.o \
       $(UCIP)/net.o \
       $(UCIP)/netaddrs.o \
       $(UCIP)/netarp.o \
       $(UCIP)/netauth.o \
       $(UCIP)/netbuf.o \
       $(UCIP)/netchap.o \
       $(UCIP)/netchat.o \
       $(UCIP)/netchpms.o \
       $(UCIP)/netdebug.o \
       $(UCIP)/netether.o \
       $(UCIP)/netfsm.o \
       $(UCIP)/nethelp.o \
       $(UCIP)/neticmp.o \
       $(UCIP)/netip.o \
       $(UCIP)/netipcp.o \
       $(UCIP)/netlcp.o \
       $(UCIP)/netmagic.o \
       $(UCIP)/netmd5.o \
       $(UCIP)/netpap.o \
       $(UCIP)/netppp.o \
       $(UCIP)/netrand.o \
       $(UCIP)/netsock.o \
       $(UCIP)/netsocka.o \
       $(UCIP)/nettcp.o \
       $(UCIP)/nettimer.o \
       $(UCIP)/netudp.o \
       $(UCIP)/netvj.o \
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

