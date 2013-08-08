/*****************************************************************************
* if_ne2k.c - NE2000 specific functions (or simply NE2000 driver)
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
*(yyyy-mm-dd)
* 2001-03-01 Mads Christiansen <mads@mogi.dk>, Partner Voxtream.
*            Original file.
*
*****************************************************************************/
#include "..\..\netconf.h"
#include "..\..\netbuf.h"
#include "if_ne2kr.h"
#include "if_ne2k.h"
#include "if_os.h"

// ***** INTERNAL TYPE DEFINES
typedef struct 
{
  u_char Status;
  u_char NextPage;
  u_short Length; 
} BufferHeader;

typedef union
{
  u_short Word;
  u_char  Uchar[2];
} Word;

// ***** PROTOTYPES
static int ReadBuffer(BufferHeader *, u_char *, u_short);


// ***** DEFINES
// Interrupt Mask Register Setup
// Packet received, packet sent, receive error, transmit error, buffer overflow
//     No interrupt for counter overflow!
#define IMR IMR_PRXE | IMR_PTXE | IMR_RXEE | IMR_TXEE | IMR_OVWE

// Minimum packet size for the ethernet (this is without the trailing CRC)
#define MIN_PACKET_SIZE 60

// ***** LOCAL VARIABLES
// Specific NIC info (first 6 bytes are cards MAC address)
static u_char CardInfo[16];

// Driver statistics
static Ne2kStatistics Statistics;

// Next packet buffer pointer, used by Ne2kReceive
static u_char NextPacket;



int Ne2kInitialize(u_char *address)
{
  u_char ReadData;
  UINT32 Test0;
  UINT32 Test1;
  int Count;

  // ***** Reset statistics
  memset(&Statistics, 0, sizeof(Statistics));

  // ***** Stop NIC
  OUTPORTB(NIC_CR, CR_STOP | CR_NO_DMA | CR_PAGE0);
  // Do a long wait to let the NIC finish receive/sending
  // THIS IS MANDATORY!
  LONGPAUSE;

  // ***** Detect NIC
  // Write 0x55 to 'Boundary Pointer Register' on page 0
  OUTPORTB(PG0W_BNRY, 0x55);
  PAUSE;

  // Write 0xaa to 'Physical Address Pointer Register2' on page 1
  OUTPORTB(NIC_CR, CR_STOP | CR_NO_DMA | CR_PAGE1);
  PAUSE;
  OUTPORTB(PG1W_PAR2, 0xaa);
  PAUSE;

  // Read 'Boundary Pointer Register' on page 0
  OUTPORTB(NIC_CR, CR_STOP | CR_NO_DMA | CR_PAGE0);
  PAUSE;
  Test0 = INPORTB(PG0R_BNRY);
  PAUSE;

  // Read 'Physical Address Pointer Register2' on page 1
  OUTPORTB(NIC_CR, CR_STOP | CR_NO_DMA | CR_PAGE1);
  PAUSE;
  Test1 = INPORTB(PG1R_PAR2);
  PAUSE;

  // ***** IF NIC is not found THEN RETURN FALSE
  if ((Test0 != 0x55) || (Test1 != 0xaa)) return FALSE;

  // ***** Read NIC's MAC address
  // We want to read MAC address, select transfer mode (word), no loopback, FIFO 4 words
  OUTPORTB(NIC_CR, CR_PAGE0 | CR_NO_DMA | CR_STOP );
  PAUSE;
  OUTPORTB(PG0W_DCR, DCR_FT1 | DCR_LS | DCR_WTS); 
  PAUSE;

  // Setup Remote Byte Count Register
  // We need 16 bytes (value must be doubled)
  OUTPORTB(PG0W_RBCR0, 0x20);
  PAUSE;
  OUTPORTB(PG0W_RBCR1, 0x00);
  PAUSE;
  // Setup Remote Start Address Register
  OUTPORTB(PG0W_RSAR0, 0x00);
  PAUSE;
  OUTPORTB(PG0W_RSAR1, 0x00);
  PAUSE;
  // DMA Remote Read and Start NIC
  OUTPORTB(NIC_CR, CR_PAGE0 | CR_DMA_READ | CR_START);
  PAUSE;

  // Read 16 bytes of data (first 6 is our MAC address), the rest is currently not used
  for (Count = 0; Count < 16; Count++)
    CardInfo[Count] = INPORTB(NIC_DATAPORT);

  // ***** Reset NIC
  // Stop NS 8390 CHIP, somewhere it states that issuing a read to
  // NIC address + 1fh (NIC_RESET) will issue a reset on the NIC! SO THIS IS DONE!
  ReadData = INPORTB(NIC_RESET);
  // Do a long wait for the 8390 to reset.
  // THIS IS MANDATORY!
  LONGPAUSE;
  OUTPORTB(NIC_RESET, ReadData); // THIS IS DONE IN A PACKET DRIVER ?
  PAUSE;

  // ***********************************************************************************
  // ***** The following initialization procedure is taken from the datasheet 
  // ***** DP8390D/NS32490D NIC Network Interface Controller (July 1995) from National 
  // ***** Semiconductor. 
  // ***** 1. Stop NIC (again...)
  OUTPORTB(NIC_CR, CR_PAGE0 | CR_STOP | CR_NO_DMA );
  // Don't do a longpause, the NIC should already be stopped
  PAUSE;

  // ***** 2. Initialize Data Configuration Register (DCR) to normal operation,
  //          word wide transfer, 4 words FIFO threshold
  OUTPORTB(PG0W_DCR, DCR_FT1 | DCR_LS | DCR_WTS);
  PAUSE;

  // ***** 3. Clear Remote Byte Count Registers (RBCR0, RBCR1)
  OUTPORTB(PG0W_RBCR0, 0x00);
  PAUSE;
  OUTPORTB(PG0W_RBCR1, 0x00);
  PAUSE;

  // ***** 4. Initialize Receive Configuration Register (RCR) to accept broadcast packets and
  //          packets addressed to this NIC (MAC address).
  // NOTICE THAT SOME NE2000 CLONES HAVE ACCEPT BROADCAST AND ACCEPT MULTICAST BITS HARDWIRED TOGETHER!
  // SO IF YOU SET ONE YOU ALSO SET THE OTHER!
  OUTPORTB(PG0W_RCR, RCR_AB | RCR_AM);
  PAUSE;

  // ***** 5. Place the NIC in Loopback Mode 1, internal loopback (Transmit Configuration Register).
  OUTPORTB(PG0W_TCR, TCR_LB0);
  PAUSE;

  // ***** 6. Initialize Page Start Register, Boundary Pointer & Page Stop Register
  OUTPORTB(PG0W_PSTART, RSTART_PG);
  PAUSE;
  OUTPORTB(PG0W_BNRY, RSTART_PG);
  PAUSE;
  OUTPORTB(PG0W_PSTOP, RSTOP_PG);
  PAUSE;
  
  // ***** 7. Clear Interrupt Status Register (ISR) by writing 0FFh to it..
  OUTPORTB(PG0W_ISR, 0xFF);
  PAUSE;

  // ***** 8. Initialize IMR (Interrupt Mask Register) to accept:
  OUTPORTB(PG0W_IMR, IMR);
  PAUSE;

  // ***** 9. Initialize Physical Address Registers (PAR0-PAR5) (MAC Address)
  // Select PAGE 1
  OUTPORTB(NIC_CR, CR_PAGE1 | CR_NO_DMA | CR_STOP);
  PAUSE;

  // Setup MAC address
  for (Count = 0; Count < 6; Count++)
  {
    OUTPORTB(PG1W_PAR0 + Count, CardInfo[Count]);
    PAUSE;
  }

  // ***** Initialize Multicast Address Registers to 00h (MAR0-MAR7) (don't accept multicast packets)
  for (Count = 0; Count < 8; Count++)
  {
    OUTPORTB(PG1W_MAR0 + Count, 0x00);
    PAUSE;
  }

  // ***** Initialize CURRent pointer to Boundary Pointer + 1
  OUTPORTB(PG1W_CURR, RSTART_PG+1);
  PAUSE;
  NextPacket = RSTART_PG + 1;

  // ***** 10. Start NIC
  OUTPORTB(NIC_CR, CR_PAGE0 | CR_NO_DMA | CR_START);
  PAUSE;

  // ***** 11. Initialize the Transmit Configuration Register for normal operation (out of loopback mode)
  OUTPORTB(PG0W_TCR, 0x00);
  PAUSE;

	// ***** Copy the 6 bytes long MAC address
	if (address) memcpy(address, CardInfo, 6);

  // ***** RETURN TRUE
  return TRUE;
}



void Ne2kStop(void)
{
  // ***** Stop NIC
  OUTPORTB(NIC_CR, CR_STOP | CR_NO_DMA | CR_PAGE0);
  PAUSE;

  // ***** Disable interrupts from NIC
  OUTPORTB(PG0W_IMR, 0x00);
  PAUSE;
  
  // Clear any generated interrupts
  OUTPORTB(PG0W_ISR, 0xff);
  PAUSE;
}



void Ne2kProcessInterrupts(void)
{
  // ***** Disable netcard IRQ 
  DISABLE_NE2K_IRQ;

  // ***** Disable interrupts from NIC (IMR = 0)
  OUTPORTB(PG0W_IMR, 0x00);
  PAUSE;

  // ***** WHILE (ISR > 0)
  while (INPORTB(PG0R_ISR) & 0x3F)
  {
    PAUSE;

    // ***** ALL INTERRUPTS MUST BE CLEARED 
    // ***** (except for OVW, which is cleared when calling Ne2kReceive)
    
    // ***** IF overwrite warning interrupt THEN
    if (INPORTB(PG0R_ISR) & ISR_OVW) 
    {
      PAUSE;
      // ***** CALL Ne2kReceiveEvent() 
      Ne2kReceiveEvent();
    }
    else PAUSE;

    // ***** IF packet received interrupt THEN 
    if (INPORTB(PG0R_ISR) & ISR_PRX)
    {
      PAUSE;
      // ***** clear packet received interrupt status bit
      OUTPORTB(PG0W_ISR, ISR_PRX);
      PAUSE;
      // ***** CALL Ne2kReceiveEvent()
      Ne2kReceiveEvent();
    } 
    else PAUSE;

    // ***** IF packet transmitted interrupt THEN 
    if (INPORTB(PG0R_ISR) & ISR_PTX)
    {
      PAUSE;
      // ***** clear packet transmitted interrupt status bit
      OUTPORTB(PG0W_ISR, ISR_PTX);
      PAUSE;
      // ***** CALL Ne2kTransmitEvent()
      Ne2kTransmitEvent();
    } 
    else PAUSE;

    // ***** IF receive error interrupt THEN 
    if (INPORTB(PG0R_ISR) & ISR_RXE)
    {
      PAUSE;
      // ***** clear receive error interrupt status bit
      OUTPORTB(PG0W_ISR, ISR_RXE);
      PAUSE;
      // ***** update receive error statistics
      Statistics.ReceiveErrors++;
    } 
    else PAUSE;

    // ***** IF transmit error interrupt THEN 
    if (INPORTB(PG0R_ISR) & ISR_TXE)
    {
      PAUSE;
      // ***** clear transmit error interrupt status bit
      OUTPORTB(PG0W_ISR, ISR_TXE);
      PAUSE;
      // ***** update transmit error statistics
      Statistics.TransmitErrors++;
      // ***** CALL Ne2kTransmitEvent()
      Ne2kTransmitEvent();
    } 
    else PAUSE;

    /* NETWORK TALLY COUNTERS ARE NOT USED, THEY COULD BE USED FOR MORE PRECISE STATISTICS
    // ***** IF counter overflow interrupt THEN 
    if (INPORTB(PG0R_ISR) & ISR_CNT)
    {
      PAUSE;
      // ***** clear counter overflow interrupt status bit
      OUTPORTB(PG0W_ISR, ISR_CNT);
      PAUSE;
      // ***** empty tally counters
      INPORTB(PG0R_CNTR0);
      PAUSE;
      INPORTB(PG0R_CNTR2);
      PAUSE;
      INPORTB(PG0R_CNTR3);
      PAUSE;
    } 
    else PAUSE;
    */
  }

  // Enabling interrupts again, don't change this order or you might loose interrupts!
  DISABLE_INTERRUPTS;
  ENABLE_NE2K_IRQ;
  SIGNAL_EOI;
  ENABLE_INTERRUPTS;

  // ***** Enable interrupts from NIC (set IMR)
  OUTPORTB(PG0W_IMR, IMR); // If new or pending interrupts from NIC they should be generated here
  PAUSE;
}


int Ne2kReceiveReady(void)
{
  BufferHeader Header;
  u_char Imr;

  // ***** Remember NIC IMR and disable interrupt from NIC (ATOMIC OPERATION!)
  DISABLE_INTERRUPTS;
  // Select PAGE 2
  OUTPORTB(NIC_CR, CR_PAGE2 | CR_NO_DMA | CR_START);
  PAUSE;
  // Read IMR register
  Imr = INPORTB(PG2R_IMR);
  PAUSE;
  // Select PAGE 0 again
  OUTPORTB(NIC_CR, CR_PAGE0 | CR_NO_DMA | CR_START);
  PAUSE;
  // Disable interrupts from NIC
  OUTPORTB(PG0W_IMR, 0x00);
  PAUSE;
  ENABLE_INTERRUPTS;
  
  // ***** Read the NIC packet header which is 4 bytes 
  // ***** IF ReadBuffer(header, NULL, 0) THEN
  if (ReadBuffer(&Header, NULL, 0))
  {
    // ***** Restore NIC IMR
    OUTPORTB(PG0W_IMR, Imr);
    PAUSE;

    // ***** RETURN packet length field in header - 4 (we don't want to count buffer header)
    return Header.Length - 4; 
  }

  // ***** Restore NIC IMR
  OUTPORTB(PG0W_IMR, Imr);
  PAUSE;

  // ***** No packets in buffer
  // ***** RETURN FALSE
  return FALSE;
}


int Ne2kReceive(u_char *packet, u_short length)
{
  int Success, Resend, TxpBit;
  u_short PacketLength;
  u_char Imr;
  BufferHeader Header;

  // Sanity check parameters
  if (packet)
    PacketLength = length;
  else
    PacketLength = 0;

  // ***** IF length > 1514 THEN RETURN FALSE
  if (PacketLength > 1514) return FALSE;

  // ***** Remember NIC IMR and disable interrupt from NIC (ATOMIC OPERATION!)
  DISABLE_INTERRUPTS;
  // Select PAGE 2
  OUTPORTB(NIC_CR, CR_PAGE2 | CR_NO_DMA | CR_START);
  PAUSE;
  // Read IMR register
  Imr = INPORTB(PG2R_IMR);
  PAUSE;
  // Select PAGE 0 again
  OUTPORTB(NIC_CR, CR_PAGE0 | CR_NO_DMA | CR_START);
  PAUSE;
  // Disable interrupts from NIC
  OUTPORTB(PG0W_IMR, 0x00);
  PAUSE;
  ENABLE_INTERRUPTS;

  // ***** Success = FALSE;
  Success = FALSE;
  Resend = FALSE;

  // ***** IF NIC buffer overwrite warning THEN
  if (INPORTB(PG0R_ISR) & ISR_OVW) 
  {
    PAUSE;

    // ***** Update statistics
    Statistics.OverrunErrors++;

    // ***********************************************************************************
    // ***** The following buffer ring overflow procedure is taken from the datasheet 
    // ***** DP8390D/NS32490D NIC Network Interface Controller (July 1995) from National 
    // ***** Semiconductor. This procedure is mandatory!

    // ***** 1. Read and store the value of the TXP bit (from command register)
    TxpBit = (INPORTB(NIC_CR) & CR_TXP); 
    PAUSE;

    // ***** 2. Issue a stop command
    OUTPORTB(NIC_CR, CR_STOP | CR_NO_DMA | CR_PAGE0);
    PAUSE;

    // ***** 3. Wait for at least 1.6 ms
    LONGPAUSE;

    // ***** 4. Clear NIC's Remote Byte Count registers (RBCR0 and RBCR1)
    OUTPORTB(PG0W_RBCR0, 0x00);
    PAUSE;
    OUTPORTB(PG0W_RBCR1, 0x00);
    PAUSE;

    // ***** 5. Read the stored value of the TXP bit from step 1 (determine if we stopped the NIC 
    // *****    when it was transmitting).
    // ***** IF TXP bit = 1 THEN
    if (TxpBit)
      // ***** IF PTX = 1 OR TXE = 1 THEN 
      if (INPORTB(PG0R_ISR) & (ISR_PTX | ISR_TXE))
      {
        PAUSE;
        // ***** Resend = FALSE
        Resend = FALSE;
      }
      else
      {
        PAUSE;
        // ***** Resend = TRUE
        Resend = TRUE;
      }
    else
      // ***** Resend = FALSE
      Resend = FALSE;

    // ***** 6. Place the NIC in mode 1 (internal loopback)
    OUTPORTB(PG0W_TCR, TCR_LB0);
    PAUSE;

    // ***** 7. Start the NIC
    OUTPORTB(NIC_CR, CR_START | CR_NO_DMA | CR_PAGE0);
    PAUSE;
  } 
  else PAUSE;

  // ***** 8. Remove one or more packets from the NIC
  // ***** IF (packet = NULL) OR (length = 0) THEN - Just remove one packet from the receive buffer ring
  if (!PacketLength) 
  {
    // ***** IF ReadBuffer(header, NULL, 0) THEN
    if (ReadBuffer(&Header, NULL, 0))
    {
      // ***** Remove packet!
      // ***** NextPacket = Header.NextPage;
      NextPacket = Header.NextPage;

      // ***** Initialize Boundary (read) Pointer to the value of NextPacket - 1
      // ***** IF Boundary Pointer < RSTART_PG THEN BNDRY = RSTOP_PG - 1
      if ( (NextPacket - 1) < RSTART_PG )
          OUTPORTB(PG0W_BNRY, RSTOP_PG - 1);
      else
          OUTPORTB(PG0W_BNRY, NextPacket - 1);
      PAUSE;

      // ***** Success = TRUE
      Success = TRUE;

      // ***** Update statistics
      Statistics.BytesReceived += Header.Length - 4; 
      Statistics.PacketsReceived++;
    }
  }
  else
  {
    // ***** IF ReadBuffer(header, packet, length) THEN
    if (ReadBuffer(&Header, packet, PacketLength))
    {
      // ***** Remove packet!
      // ***** NextPacket = Header.NextPage;
      NextPacket = Header.NextPage;

      // ***** Initialize Boundary (read) Pointer to the value of NextPacket - 1
      // ***** IF Boundary Pointer < RSTART_PG THEN BNDRY = RSTOP_PG - 1
      if ( (NextPacket - 1) < RSTART_PG )
          OUTPORTB(PG0W_BNRY, RSTOP_PG - 1);
      else
          OUTPORTB(PG0W_BNRY, NextPacket - 1);
      PAUSE;

      // ***** Success = TRUE
      Success = TRUE;

      // ***** Update statistics
      Statistics.BytesReceived += Header.Length - 4; 
      Statistics.PacketsReceived++;
    }
  }

  // ***** IF NIC buffer overwrite warning THEN
  if (INPORTB(PG0R_ISR) & ISR_OVW) 
  {
    PAUSE;

    // ***** 9. Reset the overwrite warning bit in the Interrupt Status Register.
    OUTPORTB(PG0W_ISR, ISR_OVW);
    PAUSE;

    // ***** 10. Take the NIC out of loopback mode (that means normal operation)
    OUTPORTB(PG0W_TCR, 0x00);
    PAUSE;

    // ***** 11. IF Resend = 1 THEN reissue a transmit
    if (Resend) 
    {
      // Reissue transmit
      OUTPORTB(NIC_CR, CR_START | CR_NO_DMA | CR_TXP);
      PAUSE;
    }
  }
  else PAUSE;

  // ***** Restore NIC IMR
  OUTPORTB(PG0W_IMR, Imr);
  PAUSE;

  // ***** RETURN Success
  return Success;
}



int Ne2kTransmitReady(void)
{
  // ***** IF transmitting THEN RETURN FALSE
  if (INPORTB(NIC_CR) & CR_TXP) return FALSE;
      
  // ***** RETURN TRUE
  return TRUE;
}


int Ne2kTransmit(const u_char *packet, u_short length)
{
  int Timeout, Count;
  u_char Imr;
  u_char CrdaLow, CrdaHigh;

  // ***** IF transmitting THEN RETURN FALSE
  if (!Ne2kTransmitReady()) return FALSE;

  // ***** IF (length < 14) OR (length > 1514) THEN RETURN FALSE
  if ((length < 14) || (length > 1514)) return FALSE;

  // ***** Remember NIC IMR and disable interrupt from NIC (ATOMIC OPERATION!)
  DISABLE_INTERRUPTS;
  // Select PAGE 2
  OUTPORTB(NIC_CR, CR_PAGE2 | CR_NO_DMA | CR_START);
  PAUSE;
  // Read IMR register
  Imr = INPORTB(PG2R_IMR);
  PAUSE;
  // Select PAGE 0 again
  OUTPORTB(NIC_CR, CR_PAGE0 | CR_NO_DMA | CR_START);
  PAUSE;
  // Disable interrupts from NIC
  OUTPORTB(PG0W_IMR, 0x00);
  PAUSE;
  ENABLE_INTERRUPTS;

  // ***** Clear REMOTE DMA COMPLETE bit in ISR
  OUTPORTB(PG0W_ISR, ISR_RDC);
  PAUSE;

  // ***********************************************************************************
  // ***** Due to two non synchronized state machines in the NIC, you should always do a 
  // ***** Remote Read (called a dummy read) before a Remote Write.
  // ***** This is stated in the datasheet DP8390D/NS32490D NIC Network Interface Controller
  // ***** (July 1995) from National  Semiconductor. 

  // ***** 1. Set Remote Byte Count to a value > 0 and Remote Start Address to unused RAM 
  //          (like transmit start page - 1) (REMEMBER THESE VALUES!)
  CrdaLow = 0;
  CrdaHigh = TSTART_PG - 1;  

  OUTPORTB(PG0W_RBCR0, 1);
  PAUSE;
  OUTPORTB(PG0W_RBCR1, 0);
  PAUSE;
  OUTPORTB(PG0W_RSAR0, CrdaLow);
  PAUSE;
  OUTPORTB(PG0W_RSAR1, CrdaHigh);
  PAUSE;

  // ***** 2. Issue the "dummy" Remote Read command 
  OUTPORTB(NIC_CR, CR_START | CR_DMA_READ);
  PAUSE;

  // ***** 3. Read the current remote DMA address (CRDA) (both bytes)
  // ***** 4. Compare to previous CRDA value, if different go to step 6
  // ***** 5. Delay and go to step 3.
  // timeout < 100 === leave us with appr. 200us timeout (which is a lot)
  // (this timeout is not impl. in NS' datasheet, but it's better to be on the safe side)
  for (Timeout = 0; Timeout < 50; Timeout++)  
  {
    if (CrdaLow != INPORTB(PG0R_CRDA0)) break;
    PAUSE;
    if (CrdaHigh != INPORTB(PG0R_CRDA1)) break;
    PAUSE;
  }

  // ***** 6. Setup for the Remote Write command
  // *****     Stop REMOTE DMA 
  OUTPORTB(NIC_CR, CR_START | CR_NO_DMA);
  PAUSE;

  // *****     Initialize Transmit Byte Count Registers to packet length and MIN. MIN_PACKET_SIZE bytes 
  if (length < MIN_PACKET_SIZE)
  { 
    // Padding required
    OUTPORTB(PG0W_TBCR0, MIN_PACKET_SIZE);
    PAUSE;
    OUTPORTB(PG0W_TBCR1, 0);
    PAUSE;
  }
  else
  { 
    // No padding required
    OUTPORTB(PG0W_TBCR0, (length & 0xFF));
    PAUSE;
    OUTPORTB(PG0W_TBCR1, ((length >> 8) & 0xFF) );
    PAUSE;
  }

  // ***** Update statistics (before making length an even value!)
  Statistics.BytesTransmitted += length; 
  Statistics.PacketsTransmitted++; 

  // *****     We use word transfer mode, so make length an even value
  length &= 0xfffe;
  length++;

  // *****     Initialize Remote Byte Count Register (DMA) to packet length 
  OUTPORTB(PG0W_RBCR0, (length & 0xFF));
  PAUSE;
  OUTPORTB(PG0W_RBCR1, ((length >> 8) & 0xFF));
  PAUSE;
  
  // *****     Initialize  Remote Start Address Register to address of NIC transmit buffer  
  OUTPORTB(PG0W_RSAR0, 0);
  PAUSE;
  OUTPORTB(PG0W_RSAR1, TSTART_PG);
  PAUSE;

  // ***** 7. Issue the Remote DMA WRITE 
  // *****     Start DMA Write
  OUTPORTB(NIC_CR, CR_START | CR_DMA_WRITE);
  PAUSE;

  // *****     Write data to NIC
  for (Count = 0; Count < length; Count += 2) OUTPORTW(NIC_DATAPORT, packet[Count] | (packet[Count+1] << 8));

  // ***** Wait for Remote DMA to complete (should be instantly, but we have to wait anyway)
  // timeout < 200 === leave us with appr. 200us timeout (which is a lot)
  // (this timeout is not impl. in NS' datasheet, but it's better to be on the safe side)
  for (Timeout = 0; Timeout < 200; Timeout++)
  {
    if (INPORTB(PG0R_ISR) & ISR_RDC) break;
    PAUSE;
  }

  // ***** Clear REMOTE DMA COMPLETE bit in ISR
  OUTPORTB(PG0W_ISR, ISR_RDC);
  PAUSE;

  // ***** Initialize Transmit Page Start Register
  OUTPORTB(PG0W_TPSR, TSTART_PG);
  PAUSE;

  // ***** Start transmission
  OUTPORTB(NIC_CR, CR_START | CR_NO_DMA | CR_TXP);
  PAUSE;

  // ***** Restore NIC IMR
  OUTPORTB(PG0W_IMR, Imr);
  PAUSE;

  // ***** RETURN TRUE
  return TRUE;
}



void Ne2kGetStatistics(Ne2kStatistics *statistics)
{
  DISABLE_INTERRUPTS;

  // Copy statistics to caller
  memcpy(statistics, &Statistics, sizeof(Statistics));

  ENABLE_INTERRUPTS;
}


int ReadBuffer(BufferHeader *header, u_char *packet, u_short length)
{
  u_char Save_curr;
  Word Word;
  int OddLength;
  int Count = 0;

  // ***** IF packet pointer = NULL THEN length = 0
  if (!packet) length = 0;

  // ***** Clear REMOTE DMA COMPLETE bit in ISR
  OUTPORTB(PG0W_ISR, ISR_RDC);
  PAUSE;

  // ***** Make length an even value
  OddLength = length & 0x0001;
  length++;
  length &= 0xfffe;

  // ***** Read CURR
  // Read Current Page of rcv ring
  // Go to page 1
  OUTPORTB(NIC_CR, CR_START | CR_NO_DMA | CR_PAGE1);
  PAUSE;

  // Read CURR
  Save_curr = INPORTB(PG1R_CURR);
  PAUSE;

  // Go to page 0
  OUTPORTB(NIC_CR, CR_START | CR_NO_DMA | CR_PAGE0);
  PAUSE;

  // ***** LOOP WHILE CURR <> NextPacket  - While we have a packet in buffer
  while (Save_curr != NextPacket)
  {
    // Setup Remote Byte Counts and Remote Start Address to read length + 4 bytes for buffer header 
    OUTPORTB(PG0W_RBCR0, (length + 4) & 0xFF);
    PAUSE;
    OUTPORTB(PG0W_RBCR1, ((length + 4) >> 8) & 0xFF);
    PAUSE;
    OUTPORTB(PG0W_RSAR0, 0);
    PAUSE;
    OUTPORTB(PG0W_RSAR1, NextPacket);
    PAUSE;

    // ***** Issue the Remote Read command
    OUTPORTB(NIC_CR, CR_START | CR_DMA_READ);
    PAUSE;

    // ***** Read buffer header (4 bytes)
    Word.Word = INPORTW(NIC_DATAPORT);
    header->Status = Word.Uchar[0];
    header->NextPage = Word.Uchar[1];
    header->Length = INPORTW(NIC_DATAPORT);

    if (length)
    {
      for (Count = 0; Count < (length - 2); Count+=2)
      {
        Word.Word = INPORTW(NIC_DATAPORT);
        packet[Count] = Word.Uchar[0];
        packet[Count+1] = Word.Uchar[1];
        // The line below might very well be a bit faster...
        //   *((u_short *)(packet+Count)) = INPORTW(NIC_DATAPORT);
      }
      Word.Word = INPORTW(NIC_DATAPORT);
    }
    
    // ***** Stop REMOTE DMA
    OUTPORTB(NIC_CR, CR_START | CR_NO_DMA);
    PAUSE;

    if (length) 
    {
      packet[Count] = Word.Uchar[0];
      if (!OddLength) packet[Count+1] = Word.Uchar[1];
    }

    // Clear REMOTE DMA COMPLETE bit in ISR
    OUTPORTB(PG0W_ISR, ISR_RDC);
    PAUSE;

    // ***** IF next page pointer is out of receive buffer range THEN
    //       We have a serious problem
    if ( (header->NextPage < RSTART_PG) || (header->NextPage > RSTOP_PG) )
    {
      // ***** Initialize NextPacket pointer to last known good value of the Current (write) Pointer.
      // This means that we drop a lot of packets, but beats winding up in the weeds!
      NextPacket = Save_curr;

      // ***** Initialize Boundary (read) Pointer to the value of NextPacket - 1
      // ***** IF Boundary Pointer < RSTART_PG THEN BNDRY = PSTOP - 1
      if ( (NextPacket - 1) < RSTART_PG )
          OUTPORTB(PG0W_BNRY, RSTOP_PG - 1);
      else
          OUTPORTB(PG0W_BNRY, NextPacket - 1);
      PAUSE;

      // ***** This will drop a lot of packets! Count this error!
      Statistics.NextPageErrors++;
    }   
    else
    {
      // ***** Packets should always be OK, but you never know
      // (the NIC is configured to only store valid packets)
      // ***** IF status field is OK (valid packet) THEN RETURN length
      if (header->Status & RSR_PRX) 
      {
        return header->Length - 4;
      }
      else
      {
        // ***** Discard packet in receive buffer 
        // ***** Set NextPacket to next page pointer
        NextPacket = header->NextPage;

        // ***** Initialize Boundary (read) Pointer to the value of NextPacket - 1
        // ***** IF Boundary Pointer < RSTART_PG THEN BNDRY = RSTOP - 1
        // (drops invalid packet)
        if ( (NextPacket - 1) < RSTART_PG )
            OUTPORTB(PG0W_BNRY, RSTOP_PG - 1);
        else
            OUTPORTB(PG0W_BNRY, NextPacket - 1);
        PAUSE;
      }
    }
  }

  // ***** No valid packets in buffer, RETURN FALSE
  return FALSE;
}

