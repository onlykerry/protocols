/*****************************************************************************
* netrand.c - Random number generator program file.
*
* Copyright (c) 1998 by Global Election Systems Inc.
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
* 98-06-03 Guy Lancaster <lancasterg@acm.org>, Global Election Systems Inc.
*   Extracted from avos.
*****************************************************************************/

#include "netconf.h"
#include <string.h>
#include "net.h"
#include "netmd5.h"
#include "netrand.h"

#include <stdio.h>
#include "netdebug.h"

#include <stdlib.h>
#include "netos.h"

#if MD5_SUPPORT>0   /* this module depends on MD5 */
#define RANDPOOLSZ 16   /* Bytes stored in the pool of randomness. */

/*****************************/
/*** LOCAL DATA STRUCTURES ***/
/*****************************/
static char randPool[RANDPOOLSZ];   /* Pool of randomness. */
static long randCount = 0;      /* Pseudo-random incrementer */


/***********************************/
/*** PUBLIC FUNCTION DEFINITIONS ***/
/***********************************/
/*
 * Initialize the random number generator.
 *
 * Since this is to be called on power up, we don't have much
 *  system randomess to work with.  Here all we use is the
 *  real-time clock.  We'll accumulate more randomness as soon
 *  as things start happening.
 */
void avRandomInit()
{
    avChurnRand(NULL, 0);
}

/*
 * Churn the randomness pool on a random event.  Call this early and often
 *  on random and semi-random system events to build randomness in time for
 *  usage.  For randomly timed events, pass a null pointer and a zero length
 *  and this will use the system timer and other sources to add randomness.
 *  If new random data is available, pass a pointer to that and it will be
 *  included.
 *
 * Ref: Applied Cryptography 2nd Ed. by Bruce Schneier p. 427
 */
void avChurnRand(char *randData, UINT randLen)
{
    MD5_CTX md5;

//  trace(LOG_INFO, "churnRand: %u@%P", randLen, randData);
    MD5Init(&md5);
    MD5Update(&md5, (UCHAR *)randPool, sizeof(randPool));
    if (randData)
        MD5Update(&md5, (UCHAR *)randData, randLen);
    else {
        struct {
            // INCLUDE fields for any system sources of randomness
        } sysData;

        // Load sysData fields here.
        ;

        MD5Update(&md5, (UCHAR *)&sysData, sizeof(sysData));
    }
    MD5Final((UCHAR *)randPool, &md5);
//  trace(LOG_INFO, "churnRand: -> 0");
}

/*
 * Use the random pool to generate random data.  This degrades to pseudo
 *  random when used faster than randomness is supplied using churnRand().
 * Note: It's important that there be sufficient randomness in randPool
 *  before this is called for otherwise the range of the result may be
 *  narrow enough to make a search feasible.
 *
 * Ref: Applied Cryptography 2nd Ed. by Bruce Schneier p. 427
 *
 * XXX Why does he not just call churnRand() for each block?  Probably
 *  so that you don't ever publish the seed which could possibly help
 *  predict future values.
 * XXX Why don't we preserve md5 between blocks and just update it with
 *  randCount each time?  Probably there is a weakness but I wish that
 *  it was documented.
 */
void avGenRand(char *buf, UINT bufLen)
{
    MD5_CTX md5;
    UCHAR tmp[16];
    UINT n;

    while (bufLen > 0) {
        n = MIN(bufLen, RANDPOOLSZ);
        MD5Init(&md5);
        MD5Update(&md5, (UCHAR *)randPool, sizeof(randPool));
        MD5Update(&md5, (UCHAR *)&randCount, sizeof(randCount));
        MD5Final(tmp, &md5);
        randCount++;
        memcpy(buf, tmp, n);
        buf += n;
        bufLen -= n;
    }
}

/*
 * Return a new random number.
 */
ULONG avRandom()
{
    ULONG newRand;

    avGenRand((char *)&newRand, sizeof(newRand));

    return newRand;
}

#else /* MD5_SUPPORT */


char clockBuf[16];      // added by robert to make build
//extern int TM1;                // added by robert to make build
void readClk(void) { }  // added by robert to make build



/*****************************/
/*** LOCAL DATA STRUCTURES ***/
/*****************************/
static int  avRandomized = 0;       // Set when truely randomized.
static ULONG avRandomSeed = 0;      // Seed used for random number generation.


/***********************************/
/*** PUBLIC FUNCTION DEFINITIONS ***/
/***********************************/
/*
 * Initialize the random number generator.
 *
 * Here we attempt to compute a random number seed but even if
 * it isn't random, we'll randomize it later.
 *
 * The current method uses the fields from the real time clock,
 * the idle process counter, the millisecond counter, and the
 * hardware timer tick counter.  When this is invoked
 * in startup(), then the idle counter and timer values may
 * repeat after each boot and the real time clock may not be
 * operational.  Thus we call it again on the first random
 * event.
 */
void avRandomInit()
{
    /* Get a pointer into the last 4 bytes of clockBuf. */
    ULONG *lptr1 = (ULONG *)((char *)&clockBuf[3]);

    /*
     * Initialize our seed using the real-time clock, the idle
     * counter, the millisecond timer, and the hardware timer
     * tick counter.  The real-time clock and the hardware
     * tick counter are the best sources of randomness but
     * since the tick counter is only 16 bit (and truncated
     * at that), the idle counter and millisecond timer
     * (which may be small values) are added to help
     * randomize the lower 16 bits of the seed.
     */
    readClk();
//    avRandomSeed += *(ULONG *)clockBuf + *lptr1 + OSIdleCtr
//             + mtime() + ((ULONG)TM1 << 16) + TM1;
        
    /* Initialize the Borland random number generator. */
    srand((unsigned)avRandomSeed);
}

/*
 * Randomize our random seed value.  Here we use the fact that
 * this function is called at *truely random* times by the polling
 * and network functions.  Here we only get 16 bits of new random
 * value but we use the previous value to randomize the other 16
 * bits.
 */
void avRandomize(void)
{
    if (!avRandomized) {
        avRandomized = !0;
        avRandomInit();
        /* The initialization function also updates the seed. */
    } else {
//        avRandomSeed += (avRandomSeed << 16) + TM1;
    }
}

/*
 * Return a new random number.
 * Here we use the Borland rand() function to supply a pseudo random
 * number which we make truely random by combining it with our own
 * seed which is randomized by truely random events. 
 * Thus the numbers will be truely random unless there have been no
 * operator or network events in which case it will be pseudo random
 * seeded by the real time clock.
 */
ULONG avRandom()
{
    return ((((ULONG)rand() << 16) + rand()) + avRandomSeed);
}



#endif /* MD5_SUPPORT */

