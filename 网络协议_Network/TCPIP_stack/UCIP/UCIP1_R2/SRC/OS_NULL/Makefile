#
#	MAKEFILE for uC/IP 1.0
#
#	This makes the objects only.
#

# Choose the network interface driver.
IF_SRC = ../if_dev/ne2k

# Choose the operating system.
OS = OS_NULL
OS_SRC = .

# Specify the path to netxxx source files.
UCIP_SRC = ..

# Specify the path to write the target.
OUTPUT = .


#CFLAGS = -I$(OS_SRC)
CFLAGS = -I.. -I../OS_NULL -DTARGET=OS_NULL
CC = gcc -mconsole

NET_OBJS = \
       $(UCIP_SRC)/InetAddr.o \
       $(UCIP_SRC)/net.o \
       $(UCIP_SRC)/netaddrs.o \
       $(UCIP_SRC)/netarp.o \
       $(UCIP_SRC)/netauth.o \
       $(UCIP_SRC)/netbuf.o \
       $(UCIP_SRC)/netchap.o \
       $(UCIP_SRC)/netchat.o \
       $(UCIP_SRC)/netchpms.o \
       $(UCIP_SRC)/netdebug.o \
       $(UCIP_SRC)/netether.o \
       $(UCIP_SRC)/neteth.o \
       $(UCIP_SRC)/netfsm.o \
       $(UCIP_SRC)/neticmp.o \
       $(UCIP_SRC)/netip.o \
       $(UCIP_SRC)/netipcp.o \
       $(UCIP_SRC)/netlcp.o \
       $(UCIP_SRC)/netmagic.o \
       $(UCIP_SRC)/netmd5.o \
       $(UCIP_SRC)/netpap.o \
       $(UCIP_SRC)/netppp.o \
       $(UCIP_SRC)/netrand.o \
       $(UCIP_SRC)/netsock.o \
       $(UCIP_SRC)/netsocka.o \
       $(UCIP_SRC)/nettcp.o \
       $(UCIP_SRC)/nettimer.o \
       $(UCIP_SRC)/netudp.o \
       $(UCIP_SRC)/netvj.o \
       $(IF_SRC)/if_ne2kd.c \
       $(IF_SRC)/if_os.c \
       $(OS_SRC)/os.c \
       $(OS_SRC)/main.c \
       $(OS_SRC)/trace.c


all:	$(NET_OBJS)
	$(CC) $(NET_OBJS) -o $(OUTPUT)/os_null



# Cleanup
clean:
	del $(UCIP_SRC)\*.o
	del $(IF_SRC)\*.o
	del $(OS_SRC)\*.o

