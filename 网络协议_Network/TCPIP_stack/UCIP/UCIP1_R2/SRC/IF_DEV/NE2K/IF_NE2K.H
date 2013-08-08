/*****************************************************************************
* if_ne2k.h - NE2000 specific functions (or simply NE2000 driver)
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

#ifndef IF_NE2K_H
#define IF_NE2K_H

#include "if_os.h"

typedef struct {
  UINT32 PacketsReceived;    
  UINT32 PacketsTransmitted;    
  UINT32 BytesReceived;
  UINT32 BytesTransmitted;
  UINT32 ReceiveErrors;
  UINT32 TransmitErrors;
  UINT32 NextPageErrors;
  UINT32 OverrunErrors;
} Ne2kStatistics;


extern int  Ne2kInitialize(u_char *address);
extern void Ne2kStop(void);
extern void Ne2kProcessInterrupts(void);

extern int  Ne2kReceiveReady(void);
extern int  Ne2kReceive(u_char *packet, u_short length);

extern int  Ne2kTransmitReady(void);
extern int  Ne2kTransmit(const u_char *packet, u_short length);

extern void Ne2kGetStatistics(Ne2kStatistics *pStat);


#endif