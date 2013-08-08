/*****************************************************************************
 *  2.00a Oct 31/97 GLL Add mPut and mGet routines to device interfaces.
 */
/*****************************************************************************
* devio.h  - Input/Output Device Interface Header File.
*
* Copyright (c) 1992 Global Election Systems Inc.
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
* REVISION HISTORY
*
* 97-10-31 Guy Lancaster <glanca@gesn.com>, Global Election Systems Inc.
*   Added mPut() and mGet() calls to handle network packet I/O.
*****************************************************************************/

#ifndef DEVIO_H
#define DEVIO_H

/* NOTE: depends on netbuf.h. */



/*  Definition of public constants
 */

/*  Open access flags */
#define O_RDONLY   1       /* Open for reading only */
#define O_WRONLY   2       /* Open for writing only */
#define O_RDWR     3       /* Open for reading and writing */
#define O_NONBLOCK 4       /* Do not block on I/O */
#define O_EXLOCK   8       /* Lock others from access */
#define O_CREAT    16      /* Create file if it doesn't exist */
#define O_TRUNC    32      /* If file exists, truncate to zero length */


/* Device error codes for errno. */
#define EACCES  1           /* The requested permissions are denied. */
#define ENXIO   2           /* The named device does not exist. */
#define ELOCK   3           /* The named device is locked or in use and cannot be locked. */
#define EBADF   4           /* Invalid device descriptor. */
#define ENOTTY  5           /* Specified device does not take I/O parameters */
#define EUSER   6           /* Operation interrupted by user action (NO button pressed) */
#define ENOTSUP 7           /* Function not supported */

/*  I/O control requests */
#define GETFL   1           /* Return the current I/O control flags. */
#define SETFL   2           /* Set the I/O control flags. */
#define ADDFL   3           /* Bitwise set the I/O control flags. */
#define SUBFL   4           /* Bitwise clear the I/O control flags. */ 
#define SETBAUD 6           /* Set only the baud rate bits */
#define IOFLUSH 7           /* flush input and output queues */
#define GETFRAME 8          /* Get framing character. */
#define SETFRAME 9          /* Set framing character. */

/*  I/O port settings */
#define B38400  0x00        /* Serial port baud rates. */
#define B19200  0x01
#define B9600   0x02
#define B4800   0x03
#define B2400   0x04
#define B1200   0x05
#define B600    0x06
#define B300    0x07
#define BAUD    0X07

#define CS8 0x08            /* Use 8 bit data frames (7 otherwise). */
#define PAREVN  0x10        /* Use even parity (if enabled). */
#define PARENB  0x20        /* Enable parity. */

#define IOXOFF  0x40        /* Enable XOFF flow control output. */
#define IONLCR  0x80        /* Map NL to CR-NL on output. */
#define LOCAL   0x100       /* Flag to indicate local mode */



/* Definition of public data types.
 */
typedef struct DevDef_s {
    short (*open)(u_short minorNum);
    short (*close)(u_short minorNum);
    short (*read)(u_short minorNum, void *buf, u_short n);
    short (*write)(u_short minorNum, const void *buf, u_short n);
    
    short (*nGet)(u_short minorNum, NBuf **nbm, u_short timeout);
    short (*nPut)(u_short minorNum, NBuf *nb);
    
    short (*ioctl)(u_short minorNum, short request, void *arg);
} DevDef;


/*  Definition of public variables
 */
extern short errno;                   /* File device error number */


/*  Definition of public functions
 */
const char *nameForDevice(int fd);                  /* Get name for file device ID. */
int open(const char *name, const int access, ... ); /* Open fid on name */
int close(int fid);                                 /* Close a file */
int read(int fid, void *s, int n);                  /* read n char from device */
int write(int fid, const void *s, int n);           /* write n char to device */
int nPut(int fid, NBuf *nb);                        /* Put an nBuf packet to device. */
int nGet(int fid, NBuf **nb, int timeout);          /* Get an nBuf packet from device. */
int isatty(int fid);                                /* Tests if a tty device */
int ioctl(int fid, int cmd, void *arg);             /* Input output control */


#endif // DEVIO_H
////////////////////////////////////////////////////////////////////////////////
