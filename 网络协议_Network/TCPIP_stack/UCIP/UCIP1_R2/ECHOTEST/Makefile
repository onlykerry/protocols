#
#	MAKEFILE for uC/IP echotest program
#       Created by Robert Dickenson <odin@pnc.com.au> 25-5-2001
#

# Choose the operating system.
OS = ../src/OS_NULL
INC = ../src
INCLUDE = ../../../Packet/INCLUDE

CFLAGS = -I$(OS) -I$(INC) -I$(INCLUDE)
CC = gcc

NET_OBJS = \
       main.o \
       hardware.o \
       packet.o \
       udpecho.o \
       CS8900A_Sim32.o \
\
       ../src/InetAddr.o \
       ../src/net.o \
       ../src/netaddrs.o \
       ../src/netarp.o \
       ../src/netauth.o \
       ../src/netbuf.o \
       ../src/netchap.o \
       ../src/netchat.o \
       ../src/netchpms.o \
       ../src/netdebug.o \
       ../src/netether.o \
       ../src/netfsm.o \
       ../src/nethelp.o \
       ../src/neticmp.o \
       ../src/netip.o \
       ../src/netipcp.o \
       ../src/netlcp.o \
       ../src/netmagic.o \
       ../src/netmd5.o \
       ../src/netpap.o \
       ../src/netppp.o \
       ../src/netrand.o \
       ../src/netsock.o \
       ../src/netsocka.o \
       ../src/nettcp.o \
       ../src/nettimer.o \
       ../src/netudp.o \
       ../src/netvj.o \
       ../src/trace.o \
       ../src/if_ne2kd.c \
       ../src/if_os.c \
       ../src/UCOS.o \
       ../src/UCOS32.o


all:	$(NET_OBJS)
	$(CC) $(NET_OBJS) -Wl,../../../Packet/LIB/Packet.lib -o ./echotest



# Cleanup
clean:
	del *.o

