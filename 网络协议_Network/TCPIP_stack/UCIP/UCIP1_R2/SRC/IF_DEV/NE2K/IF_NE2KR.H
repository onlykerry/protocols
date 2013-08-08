/*****************************************************************************
* if_ne2kr.h - NE2000 specific (register) defines
*
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
* 01-03-2001 Mads Christiansen <mc@voxtream.com>, Partner Voxtream.
*	           Original file.
*
*****************************************************************************/

#ifndef IF_NE2KR_H
#define IF_NE2KR_H

/* NE2000 INTERNAL REGISTERS DEFINITION  */
#define NIC_CR           0x340                /* Command Register (I/O ADDRESS OF CARD) */

/* NE2000 INTERNAL REGISTERS PAGE 0 READ */
#define PG0R_CLDA0       (NIC_CR + 0x01)      /* Current Local DMA Address 0 */
#define PG0R_CLDA1       (NIC_CR + 0x02)      /* Current Local DMA Address 1 */
#define PG0R_BNRY        (NIC_CR + 0x03)      /* Boundary Pointer */
#define PG0R_TSR         (NIC_CR + 0x04)      /* Transmit Status Register */
#define PG0R_NCR         (NIC_CR + 0x05)      /* Number of Collisions Register */
#define PG0R_FIFO        (NIC_CR + 0x06)      /* FIFO */
#define PG0R_ISR         (NIC_CR + 0x07)      /* Interrupt Status Register */
#define PG0R_CRDA0       (NIC_CR + 0x08)      /* Current Remote DMA Address 0 */
#define PG0R_CRDA1       (NIC_CR + 0x09)      /* Current Remote DMA Address 1 */
#define PG0R_RSR         (NIC_CR + 0x0C)      /* Receive Status Register */
#define PG0R_CNTR0       (NIC_CR + 0x0D)      /* Tally Counter 0 (Frame Alignment Errors) */
#define PG0R_CNTR1       (NIC_CR + 0x0E)      /* Tally Counter 1 (CRC Errors) */
#define PG0R_CNTR2       (NIC_CR + 0x0F)      /* Tally Counter 2 (Missed Packet Errors) */

/* NE2000 INTERNAL REGISTERS PAGE 0 WRITE */
#define PG0W_PSTART      (NIC_CR + 0x01)      /*  */
#define PG0W_PSTOP       (NIC_CR + 0x02)      /*  */
#define PG0W_BNRY        (NIC_CR + 0x03)      /*  */
#define PG0W_TPSR        (NIC_CR + 0x04)      /*  */
#define PG0W_TBCR0       (NIC_CR + 0x05)      /*  */
#define PG0W_TBCR1       (NIC_CR + 0x06)      /*  */
#define PG0W_ISR         (NIC_CR + 0x07)      /*  */
#define PG0W_RSAR0       (NIC_CR + 0x08)      /*  */
#define PG0W_RSAR1       (NIC_CR + 0x09)      /*  */
#define PG0W_RBCR0       (NIC_CR + 0x0A)      /*  */
#define PG0W_RBCR1       (NIC_CR + 0x0B)      /*  */
#define PG0W_RCR         (NIC_CR + 0x0C)      /*  */
#define PG0W_TCR         (NIC_CR + 0x0D)      /*  */
#define PG0W_DCR         (NIC_CR + 0x0E)      /*  */
#define PG0W_IMR         (NIC_CR + 0x0F)

/* NE2000 INTERNAL REGISTERS PAGE 1 READ */
#define PG1R_PAR0        (NIC_CR + 0x01)      /*  */
#define PG1R_PAR1        (NIC_CR + 0x02)      /*  */
#define PG1R_PAR2        (NIC_CR + 0x03)      /*  */
#define PG1R_PAR3        (NIC_CR + 0x04)      /*  */
#define PG1R_PAR4        (NIC_CR + 0x05)      /*  */
#define PG1R_PAR5        (NIC_CR + 0x06)      /*  */
#define PG1R_CURR        (NIC_CR + 0x07)      /*  */
#define PG1R_MAR0        (NIC_CR + 0x08)      /*  */
#define PG1R_MAR1        (NIC_CR + 0x09)      /*  */
#define PG1R_MAR2        (NIC_CR + 0x0A)      /*  */
#define PG1R_MAR3        (NIC_CR + 0x0B)      /*  */
#define PG1R_MAR4        (NIC_CR + 0x0C)      /*  */
#define PG1R_MAR5        (NIC_CR + 0x0D)      /*  */
#define PG1R_MAR6        (NIC_CR + 0x0E)      /*  */
#define PG1R_MAR7        (NIC_CR + 0x0F)      /*  */
      
/* NE2000 INTERNAL REGISTERS PAGE 1 WRITE */
#define PG1W_PAR0        (NIC_CR + 0x01)      /*  */
#define PG1W_PAR1        (NIC_CR + 0x02)      /*  */
#define PG1W_PAR2        (NIC_CR + 0x03)      /*  */
#define PG1W_PAR3        (NIC_CR + 0x04)      /*  */
#define PG1W_PAR4        (NIC_CR + 0x05)      /*  */
#define PG1W_PAR5        (NIC_CR + 0x06)      /*  */
#define PG1W_CURR        (NIC_CR + 0x07)      /*  */
#define PG1W_MAR0        (NIC_CR + 0x08)      /*  */
#define PG1W_MAR1        (NIC_CR + 0x09)      /*  */
#define PG1W_MAR2        (NIC_CR + 0x0A)      /*  */
#define PG1W_MAR3        (NIC_CR + 0x0B)      /*  */
#define PG1W_MAR4        (NIC_CR + 0x0C)      /*  */
#define PG1W_MAR5        (NIC_CR + 0x0D)      /*  */
#define PG1W_MAR6        (NIC_CR + 0x0E)      /*  */
#define PG1W_MAR7        (NIC_CR + 0x0F)      /*  */
      
/* NE2000 INTERNAL REGISTERS PAGE 2 READ */
#define PG2R_PSTART      (NIC_CR + 0x01)      /*  */
#define PG2R_PSTOP       (NIC_CR + 0x02)      /*  */
#define PG2R_RNPP        (NIC_CR + 0x03)      /*  */
#define PG2R_TPSR        (NIC_CR + 0x04)      /*  */
#define PG2R_LNPP        (NIC_CR + 0x05)      /*  */
#define PG2R_ACUPPER     (NIC_CR + 0x06)      /*  */
#define PG2R_ACLOWER     (NIC_CR + 0x07)      /*  */
#define PG2R_RCR         (NIC_CR + 0x0C)      /*  */
#define PG2R_TCR         (NIC_CR + 0x0D)      /*  */
#define PG2R_DCR         (NIC_CR + 0x0E)      /*  */
#define PG2R_IMR         (NIC_CR + 0x0F)      /*  */

/* NE2000 INTERNAL REGISTERS PAGE 2 WRITE */
#define PG2W_CLDA0       (NIC_CR + 0x01)      /*  */
#define PG2W_CLDA1       (NIC_CR + 0x02)      /*  */
#define PG2W_RNPP        (NIC_CR + 0x03)      /*  */
#define PG2W_LNPP        (NIC_CR + 0x05)      /*  */
#define PG2W_ACUPPER     (NIC_CR + 0x06)      /*  */
#define PG2W_ACLOWER     (NIC_CR + 0x07)      /*  */

/* SPECIAL NE2000 REGISTERS */
#define NIC_RESET        (NIC_CR + 0x1f)      /* Issue a read for reset */
#define NIC_DATAPORT     (NIC_CR + 0x10)      /* Data port */

/* SHARED MEMORY MANAGEMENT PARAMETERS */
#define TSTART_PG           0x40	    // First page of TX buffer (room for one ethernet packet)
#define RSTART_PG           0x46	    // Starting page of RX ring
#define RSTOP_PG            0x80	    // Last page +1 of RX ring

/* NE2000 COMMAND REGISTER CONSTANTS */
#define CR_STOP             0x01        // reset the card
#define CR_START            0x02        // start the card
#define CR_TXP              0x04        // begin transmission (or transmission in progess)

#define CR_PS0              0x40        // low bit of page number
#define CR_PS1              0x80        // high bit of page number
#define CR_PAGE0            0x00        // select page 0
#define CR_PAGE1            CR_PS0      // select page 1
#define CR_PAGE2            CR_PS1      // select page 2

#define CR_DMA_READ         0x08        // DMA Remote Read  (NIC->PC)
#define CR_DMA_WRITE        0x10        // DMA Remote Write (PC->NIC)
#define CR_DMA_SEND         0x18        // Transmit
#define CR_NO_DMA           0x20        // Stop Remote DMA (Abort)



//
// Constants for the NIC_XMIT_STATUS register.
//
// Indicate the result of a packet transmission.
//

#define TSR_XMIT_OK     (UCHAR)0x01        // transmit with no errors
#define TSR_COLLISION   (UCHAR)0x04        // collided at least once
#define TSR_ABORTED     (UCHAR)0x08        // too many collisions
#define TSR_NO_CARRIER  (UCHAR)0x10        // carrier lost
#define TSR_NO_CDH      (UCHAR)0x40        // no collision detect heartbeat


//
// Constants for the NIC_INTR_STATUS register.
//
// Indicate the cause of an interrupt.
//
#define ISR_PRX         (UCHAR)0x01        // packet received with no errors
#define ISR_PTX         (UCHAR)0x02        // packet transmitted with no errors
#define ISR_RXE         (UCHAR)0x04        // error on packet reception
#define ISR_TXE         (UCHAR)0x08        // error on packet transmission
#define ISR_OVW         (UCHAR)0x10        // receive buffer overflow
#define ISR_CNT         (UCHAR)0x20        // MSB set on tally counter
#define ISR_RDC         (UCHAR)0x40        // Remote DMA Complete
#define ISR_RST         (UCHAR)0x80        // (not an interrupt) card is reset


//
// Constants for the NIC_RCV_CONFIG register.
//
// Configure what type of packets are received.
//
#define RCR_SEP         (UCHAR)0x01        // SAVE ERRORED PACKETS (INVALID PACKETS)
#define RCR_AR          (UCHAR)0x02        // ACCEPT RUNT PACKETS (ACCEPTS PACKET SHORTER THAN 64 BYTES)
#define RCR_AB          (UCHAR)0x04        // ACCEPT BROADCAST
#define RCR_AM          (UCHAR)0x08        // ACCEPT MULTICAST
#define RCR_PRO         (UCHAR)0x10        // PROMISCUOUS PHYSICAL (ALL PACKETS REGARDLESS OF PHYSICAL ADDRESS)
#define RCR_MON         (UCHAR)0x20        // MONITOR MODE (PACKETS ARE VERIFIED BUT NOT BUFFERED)

//
// Constants for the NIC_RCV_STATUS register.
//
// Indicate the status of a received packet.
//
// These are also used to interpret the status byte in the
// packet header of a received packet.
//

#define RSR_PRX         (UCHAR)0x01        // Packet received intact
#define RSR_CRC_ERROR   (UCHAR)0x02        // packet received with CRC error
#define RSR_MULTICAST   (UCHAR)0x20        // packet received was multicast
#define RSR_DISABLED    (UCHAR)0x40        // received is disabled
#define RSR_DEFERRING   (UCHAR)0x80        // receiver is deferring


//
// Constants for the NIC_XMIT_CONFIG register.
//
// Configures how packets are transmitted.
//
#define TCR_CRC         (UCHAR)0x01        // INHIBIT CRC (0 = CRC appended, 1 = inhibit CRC)
#define TCR_LB0         (UCHAR)0x02        // ENCODED LOOPBACK CONTROL (0 = normal operation,  1 = internal loopback)
#define TCR_LB1         (UCHAR)0x04        // ENCODED LOOPBACK CONTROL (2 = external loopback, 3 = external loopback)
#define TCR_ATD         (UCHAR)0x08        // AUTO TRANSMIT DISABLE
#define TCR_OFST        (UCHAR)0x10        // COLLISION OFFSET ENABLE


//
// Constants for the NIC_DATA_CONFIG register.
//
// Set data transfer sizes.
//
#define DCR_WTS			    (UCHAR)0x01        // WORD TRANSFER SELECT (0 = byte wide DMA transfer, 1 = word)
#define DCR_BOS         (UCHAR)0x02        // BYTE ORDER SELECT (0 = intel, 1 = motorola 680x0)
#define DCR_LAS         (UCHAR)0x04        // LONG ADDRESS SELECT (1 = 32 bit, 0 = Dual 16 bit mode)
#define DCR_LS          (UCHAR)0x08        // LOOPBACK SELECT (0 = loopback, 1 = normal operation)
#define DCR_AR          (UCHAR)0x10        // AUTO-INITIALIZE REMOTE (0 = send command not executed, 1 = executed)
#define DCR_FT0         (UCHAR)0x20        // FIFO THRESHOLD SELECT BIT 0
#define DCR_FT1         (UCHAR)0x40        // FIFO THRESHOLD SELECT BIT 1


//
// Constants for the NIC_INTR_MASK register.
//
// Configure which ISR settings actually cause interrupts.
//
#define IMR_PRXE        (UCHAR)0x01        // PACKET RECEIVED INTERRUPT ENABLE
#define IMR_PTXE        (UCHAR)0x02        // PACKET TRANSMITTED INTERUPT ENABLE
#define IMR_RXEE        (UCHAR)0x04        // RECEIVE ERROR INTERRUPT ENABLE
#define IMR_TXEE        (UCHAR)0x08        // TRANSMIT ERROR INTERRUPT ENABLE
#define IMR_OVWE        (UCHAR)0x10        // OVERWRITE WARNING INTERRUPT ENABLE
#define IMR_CNTE        (UCHAR)0x20        // COUNTER OVERFLOW INTERRUPT ENABLE
#define IMR_RDCE        (UCHAR)0x40        // DMA COMPLETE INTERRUPT ENABLE



// UMC/DAVICOM 9008 SPECIFIC REGISTERS (NOT USED BY NE2000 DRIVER)
#define PG0R_CRA        (NIC_CR + 0x0A)    // CONFIGURATION REGISTER A
#define PG0W_CRA        (NIC_CR + 0x0A)    // CONFIGURATION REGISTER A (MUST DO A READ BEFORE WRITE!!!)
#define PG0R_CRB        (NIC_CR + 0x0B)    // CONFIGURATION REGISTER B
#define PG0W_CRB        (NIC_CR + 0x0A)    // CONFIGURATION REGISTER B (MUST DO A READ BEFORE WRITE!!!)
#define PG2R_CRC        (NIC_CR + 0x0B)    // CONFIGURATION REGISTER C
#define PG2W_CRC        (NIC_CR + 0x0B)    // CONFIGURATION REGISTER C
#define PG3R_CRD        (NIC_CR + 0x07)    // CONFIGURATION REGISTER D
#define PG3W_CRD        (NIC_CR + 0x07)    // CONFIGURATION REGISTER D

#define CRA_IRQ3        (UCHAR)0x00
#define CRA_IRQ4        (UCHAR)0x10
#define CRA_IRQ5        (UCHAR)0x20
#define CRA_IRQ9        (UCHAR)0x30
#define CRA_IRQ10       (UCHAR)0x40
#define CRA_IRQ11       (UCHAR)0x50
#define CRA_IRQ12       (UCHAR)0x60
#define CRA_IRQ15       (UCHAR)0x70


#endif
