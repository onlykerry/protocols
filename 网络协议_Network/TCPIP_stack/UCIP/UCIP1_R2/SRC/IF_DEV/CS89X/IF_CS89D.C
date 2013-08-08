/*****************************************************************************
* if_cs89d.c - Network Interface Driver for Cirrus CS8900A chipset source file
*
* Copyright (c) 2001 by Cognizant Pty Ltd.
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
* 2001-05-12 Robert Dickenson <odin@pnc.com.au>, Cognizant Pty Ltd.
*            Original file.
*
*****************************************************************************
*/
#include "..\..\netconf.h"
#include "..\..\netbuf.h"
#include "if_cs89d.h"
#include <stdio.h>
#include "..\..\netdebug.h"


#ifndef WIN32 

/***********************************************************************************
 *
 * Function:    ReadPP(ppAdress)
 * Description: This function reads the Packet Page memory in the CS8900 at the
 *              given adress and returns the value.
 *
 **********************************************************************************/
u_short ReadPP(u_short ppAdress)
{
    *(u_short*)portPtr = ppAdress;     // Write the pp Pointer
    return (*(u_short*)portData);      // Get the data from the chip
}

/***********************************************************************************
 *
 * Function:    ReadPP8(u_short ppAdress)
 * Description: This function allows reads to one single byte. This is needed when
 *              Reading the first two bytes of the RxFrame.
 *
 **********************************************************************************/
u_char ReadPP8(u_short ppAdress)
{
    *(u_short*)portPtr = ppAdress;    // Write the pp Pointer
    return(*(u_char*)portData);       // Get the data from the chip
}

/***********************************************************************************
 *
 * Function:    WritePP(u_short ppAdress, u_short inData)
 * Description: This function simply writes the supplied data to the given adress
 *              in the Packet Page data base of the CS8900
 *
 **********************************************************************************/
void WritePP(u_short ppAdress, u_short inData)
{
    *(u_short*)portPtr = ppAdress;    // Write the pp Pointer
    *(u_short*)portData = inData;     // Write the data to the chip
}

/***********************************************************************************
 *
 * Function:    VerifyCS8900(void)
 * Description: Insert your own description of this function
 *
 **********************************************************************************/
u_short VerifyCS8900(void)
{
//    char out[32];
    u_short data = 0;

    if ((*(u_short*)portPtr != 0x3000) && (ReadPP(ppEISA) != 0x630e)) { // Check CS8900
//        SendString((char*)"Error - CS8900 chip not valid !!!\x0a\x0d");
    } else {
        data = ReadPP(ppProdID) >> 8;   // Get the data
//        sprintf(out, "CS8900 OK, Revision: %02x\x0a\x0d\x0a\x0d", data);
//        SendString(out);
    }
    return data;
}

#endif // WIN32

////////////////////////////////////////////////////////////////////////////////
/* 
 * pppMPutRaw - append given character to end of given nBuf.
 * If nBuf is full, append another.
 * Return the current nBuf.
 */
/*
static NBuf* NBufPut(u_char c, NBuf* nb)
{
	NBuf* tb = nb;
	
	// Make sure there is room for the character and an escape code.
	// Sure we don't quite fill the buffer if the character doesn't
	// get escaped but is one character worth complicating this?
	// Note: We assume no packet header.
	if (nb && (&nb->body[NBUFSZ] - (nb->data + nb->len)) < 1) {
		nGET(tb);
		if (tb) {
			nb->nextBuf = tb;
			tb->len = 0;
		}
		nb = tb;
	}
	if (nb) {
		*(nb->data + nb->len++) = c;
	}
	return tb;
}
 */
NBuf* NBufPutW(u_short w, NBuf* nb)
{
	NBuf* tb = nb;
	
	/* Make sure there is room for the character and an escape code.
	 * Sure we don't quite fill the buffer if the character doesn't
	 * get escaped but is one character worth complicating this? */
	/* Note: We assume no packet header. */
	if (nb && (&nb->body[NBUFSZ] - (nb->data + nb->len)) < 2) {
//        TRACE("NBufPutW(%04X, %p) - APPENDING ANOTHER NBUF TO CHAIN !!!!!!!!!!!!\n", w, nb);
        OS_ENTER_CRITICAL();
		nGET(tb);
        OS_EXIT_CRITICAL();
		if (tb) {
			nb->nextBuf = tb;
			tb->len = 0;
		}
		nb = tb;
	}
	if (nb) {
		*(u_short*)(nb->data + nb->len) = w;
        nb->len += 2;
	}
	return tb;
}

/*
// warning: looks very dangerous, could release NBuf in nPullup
static u_short NBufGetW(u_short offset, NBuf* inBuf)
{
    u_short data = 0;
    u_short* pData;

	short i = 2;

//	if (inBuf->len < i && (inBuf = nPullup(inBuf, i)) == NULL)  {
	if (inBuf->len < i || (inBuf = nPullup(inBuf, i)) == NULL)  {
		return 0;
	}
	pData = nBUFTOPTR(inBuf, u_short*);
    data = *pData;
    return data;
}
 */

////////////////////////////////////////////////////////////////////////////////

void WriteNBufPP(u_short TxLocPtr, NBuf* inBuf)
{
    NBuf* nb = inBuf;
    u_short* sPtr;
    short extra = 0;
	short n;
    u_char realign = 0;

//    TRACE("WriteNBufPP(%04X, %p)\n", TxLocPtr, nb);

//    nb->nextBuf = NULL; // TODO: debug why this is req.

	while (nb) {
		sPtr = nBUFTOPTR(nb, u_short*);
		n = nb->len;

//        TRACE("WriteNBufPP(%04X, %p) n = %u\n", TxLocPtr, nb, n);
/*
        if (realign && n) {
            u_char* bPtr = nBUFTOPTR(nb, u_char*);
            extra |= ((*bPtr++) & 0x00ff);
            WritePP(TxLocPtr, extra);
            TxLocPtr += 2;
            n -= 1;
            sPtr = bPtr;
            realign = 0;
        }
 */
        n = max(n, 200);
        while (n > 1) {
            WritePP(TxLocPtr, *sPtr++);          // Send packet data words
            TxLocPtr += 2;
            n -= 2;
        }
/*
        if (n > 0) {
            extra = *sPtr++;
            extra <<= 8;
            realign = 1;
        }
 */
        nb = nb->nextBuf;
	}
    if (realign) {
        WritePP(TxLocPtr, extra);
    }
}


////////////////////////////////////////////////////////////////////////////////
