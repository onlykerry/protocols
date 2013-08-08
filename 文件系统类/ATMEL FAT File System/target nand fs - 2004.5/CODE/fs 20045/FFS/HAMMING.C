/***********************************************************************/
/*                                                                     */
/*   Module:  hamming.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2002.4                                                   */
/*   Purpose: Implement Hamming 1 bit encoding and decoding            */
/*                                                                     */
/*---------------------------------------------------------------------*/
/*                                                                     */
/*                 Copyright 2004, Blunk Microsystems.                 */
/*                        ALL RIGHTS RESERVED                          */
/*                                                                     */
/*   Licensees have the non-exclusive right to use, modify, or extract */
/*   this computer program for software development at a single site.  */
/*   This program may be resold or disseminated in executable format   */
/*   only. The source code may not be redistributed or resold.         */
/*                                                                     */
/***********************************************************************/
#include "flashfsp.h"

#if INC_NAND_FS && NUM_FFS_VOLS

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
#define CODE_ERROR      1
#define CORRECTABLE     2

#define MASK1      0x11  /* 1st interleaving 0001 0001 (bits 0 and 4) */
#define MASK2      0x22  /* 2nd interleaving 0010 0010 (bits 1 and 5) */
#define MASK3      0x44  /* 3rd interleaving 0100 0100 (bits 2 and 6) */
#define MASK4      0x88  /* 4th interleaving 1000 1000 (bits 3 and 7) */

/***********************************************************************/
/* Type Definitions                                                    */
/***********************************************************************/
typedef union
{
  ui8  byte[4];
  ui16 half[2];
  ui32 lword;
} Container;

/***********************************************************************/
/* Global Variable Declarations                                        */
/***********************************************************************/
const ui8 NibXor[] =
{
  0x0^0x0, 0x0^0x1, 0x0^0x2, 0x0^0x3, 0x0^0x4, 0x0^0x5, 0x0^0x6, 0x0^0x7,
  0x0^0x8, 0x0^0x9, 0x0^0xA, 0x0^0xB, 0x0^0xC, 0x0^0xD, 0x0^0xE, 0x0^0xF,
  0x1^0x0, 0x1^0x1, 0x1^0x2, 0x1^0x3, 0x1^0x4, 0x1^0x5, 0x1^0x6, 0x1^0x7,
  0x1^0x8, 0x1^0x9, 0x1^0xA, 0x1^0xB, 0x1^0xC, 0x1^0xD, 0x1^0xE, 0x1^0xF,
  0x2^0x0, 0x2^0x1, 0x2^0x2, 0x2^0x3, 0x2^0x4, 0x2^0x5, 0x2^0x6, 0x2^0x7,
  0x2^0x8, 0x2^0x9, 0x2^0xA, 0x2^0xB, 0x2^0xC, 0x2^0xD, 0x2^0xE, 0x2^0xF,
  0x3^0x0, 0x3^0x1, 0x3^0x2, 0x3^0x3, 0x3^0x4, 0x3^0x5, 0x3^0x6, 0x3^0x7,
  0x3^0x8, 0x3^0x9, 0x3^0xA, 0x3^0xB, 0x3^0xC, 0x3^0xD, 0x3^0xE, 0x3^0xF,
  0x4^0x0, 0x4^0x1, 0x4^0x2, 0x4^0x3, 0x4^0x4, 0x4^0x5, 0x4^0x6, 0x4^0x7,
  0x4^0x8, 0x4^0x9, 0x4^0xA, 0x4^0xB, 0x4^0xC, 0x4^0xD, 0x4^0xE, 0x4^0xF,
  0x5^0x0, 0x5^0x1, 0x5^0x2, 0x5^0x3, 0x5^0x4, 0x5^0x5, 0x5^0x6, 0x5^0x7,
  0x5^0x8, 0x5^0x9, 0x5^0xA, 0x5^0xB, 0x5^0xC, 0x5^0xD, 0x5^0xE, 0x5^0xF,
  0x6^0x0, 0x6^0x1, 0x6^0x2, 0x6^0x3, 0x6^0x4, 0x6^0x5, 0x6^0x6, 0x6^0x7,
  0x6^0x8, 0x6^0x9, 0x6^0xA, 0x6^0xB, 0x6^0xC, 0x6^0xD, 0x6^0xE, 0x6^0xF,
  0x7^0x0, 0x7^0x1, 0x7^0x2, 0x7^0x3, 0x7^0x4, 0x7^0x5, 0x7^0x6, 0x7^0x7,
  0x7^0x8, 0x7^0x9, 0x7^0xA, 0x7^0xB, 0x7^0xC, 0x7^0xD, 0x7^0xE, 0x7^0xF,
  0x8^0x0, 0x8^0x1, 0x8^0x2, 0x8^0x3, 0x8^0x4, 0x8^0x5, 0x8^0x6, 0x8^0x7,
  0x8^0x8, 0x8^0x9, 0x8^0xA, 0x8^0xB, 0x8^0xC, 0x8^0xD, 0x8^0xE, 0x8^0xF,
  0x9^0x0, 0x9^0x1, 0x9^0x2, 0x9^0x3, 0x9^0x4, 0x9^0x5, 0x9^0x6, 0x9^0x7,
  0x9^0x8, 0x9^0x9, 0x9^0xA, 0x9^0xB, 0x9^0xC, 0x9^0xD, 0x9^0xE, 0x9^0xF,
  0xA^0x0, 0xA^0x1, 0xA^0x2, 0xA^0x3, 0xA^0x4, 0xA^0x5, 0xA^0x6, 0xA^0x7,
  0xA^0x8, 0xA^0x9, 0xA^0xA, 0xA^0xB, 0xA^0xC, 0xA^0xD, 0xA^0xE, 0xA^0xF,
  0xB^0x0, 0xB^0x1, 0xB^0x2, 0xB^0x3, 0xB^0x4, 0xB^0x5, 0xB^0x6, 0xB^0x7,
  0xB^0x8, 0xB^0x9, 0xB^0xA, 0xB^0xB, 0xB^0xC, 0xB^0xD, 0xB^0xE, 0xB^0xF,
  0xC^0x0, 0xC^0x1, 0xC^0x2, 0xC^0x3, 0xC^0x4, 0xC^0x5, 0xC^0x6, 0xC^0x7,
  0xC^0x8, 0xC^0x9, 0xC^0xA, 0xC^0xB, 0xC^0xC, 0xC^0xD, 0xC^0xE, 0xC^0xF,
  0xD^0x0, 0xD^0x1, 0xD^0x2, 0xD^0x3, 0xD^0x4, 0xD^0x5, 0xD^0x6, 0xD^0x7,
  0xD^0x8, 0xD^0x9, 0xD^0xA, 0xD^0xB, 0xD^0xC, 0xD^0xD, 0xD^0xE, 0xD^0xF,
  0xE^0x0, 0xE^0x1, 0xE^0x2, 0xE^0x3, 0xE^0x4, 0xE^0x5, 0xE^0x6, 0xE^0x7,
  0xE^0x8, 0xE^0x9, 0xE^0xA, 0xE^0xB, 0xE^0xC, 0xE^0xD, 0xE^0xE, 0xE^0xF,
  0xF^0x0, 0xF^0x1, 0xF^0x2, 0xF^0x3, 0xF^0x4, 0xF^0x5, 0xF^0x6, 0xF^0x7,
  0xF^0x8, 0xF^0x9, 0xF^0xA, 0xF^0xB, 0xF^0xC, 0xF^0xD, 0xF^0xE, 0xF^0xF,
};

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*   EncodeEcc: Generate ECC data for specified 512 bytes of user data */
/*                                                                     */
/*       Input: data = pointer to 512 bytes of user data               */
/*                                                                     */
/*      Output: ecc[10] = 10 byte array of ECC data                    */
/*                                                                     */
/***********************************************************************/
void EncodeEcc(const ui32 *data, ui8 *ecc)
{
  ui32 lword;
  Container every, every1[2], every2[2], every4[2], every8[2];
  Container every16[2], every32[2], every64[2], tmp;
  int i;

  /*-------------------------------------------------------------------*/
  /* Initially clear the checksums.                                    */
  /*-------------------------------------------------------------------*/
  every1[0].lword = every1[1].lword = every2[0].lword = every2[1].lword =
  every4[0].lword = every4[1].lword = every8[0].lword = every8[1].lword =
  every16[0].lword = every16[1].lword = 0; every32[0].lword =
  every32[1].lword = every64[0].lword = every64[1].lword = 0;

  /*-------------------------------------------------------------------*/
  /* Build the 32-bit checksums.                                       */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < 128; ++i)
  {
    lword = data[i];

    /*-----------------------------------------------------------------*/
    /* Update 7 checksums each loop.                                   */
    /*-----------------------------------------------------------------*/
    every1[i & 1].lword ^= lword;
    every2[(i & 2) >> 1].lword ^= lword;
    every4[(i & 4) >> 2].lword ^= lword;
    every8[(i & 8) >> 3].lword ^= lword;
    every16[(i & 16) >> 4].lword ^= lword;
    every32[(i & 32) >> 5].lword ^= lword;
    every64[(i & 64) >> 6].lword ^= lword;
  }
  every.lword = every1[0].lword ^ every1[1].lword;

  /*-------------------------------------------------------------------*/
  /* Calculate the "every other bit" checksums.                        */
  /*-------------------------------------------------------------------*/
  tmp.half[0] = (ui16)(every.half[0] ^ every.half[1]);
  ecc[0] = (ui8)(tmp.byte[0] ^ tmp.byte[1]);
  /*
  ** ecc[0] contains
  ** bit 0: checksum of all even bits of 1st interleave plane
  ** bit 1: checksum of all even bits of 2nd interleave plane
  ** bit 2: checksum of all even bits of 3rd interleave plane
  ** bit 3: checksum of all even bits of 4th interleave plane
  ** bit 4: checksum of all odd bits of 1st interleave plane
  ** bit 5: checksum of all odd bits of 2nd interleave plane
  ** bit 6: checksum of all odd bits of 3rd interleave plane
  ** bit 7: checksum of all odd bits of 4th interleave plane
  */

  /*-------------------------------------------------------------------*/
  /* Calculate the "every 2 bits" checksums.                           */
  /*-------------------------------------------------------------------*/
  tmp.half[0] = (ui16)(every.half[0] ^ every.half[1]);
  ecc[1] = (ui8)(NibXor[tmp.byte[0]] | (NibXor[tmp.byte[1]] << 4));
  /*
  ** ecc[1] contains
  ** bit 0: checksum of all even 2-bit groups of 1st interleave plane
  ** bit 1: checksum of all even 2-bit groups of 2nd interleave plane
  ** bit 2: checksum of all even 2-bit groups of 3rd interleave plane
  ** bit 3: checksum of all even 2-bit groups of 4th interleave plane
  ** bit 4: checksum of all odd 2-bit groups of 1st interleave plane
  ** bit 5: checksum of all odd 2-bit groups of 2nd interleave plane
  ** bit 6: checksum of all odd 2-bit groups of 3rd interleave plane
  ** bit 7: checksum of all odd 2-bit groups of 4th interleave plane
  */

  /*-------------------------------------------------------------------*/
  /* Calculate the "every 4 bits" checksums (2 byte groupings).        */
  /*-------------------------------------------------------------------*/
  tmp.byte[0] = (ui8)(every.byte[0] ^ every.byte[1]);
  tmp.byte[1] = (ui8)(every.byte[2] ^ every.byte[3]);
  ecc[2] = (ui8)(NibXor[tmp.byte[0]] | (NibXor[tmp.byte[1]] << 4));
  /*
  ** ecc[2] contains
  ** bit 0: checksum of all even 4-bit groups of 1st interleave plane
  ** bit 1: checksum of all even 4-bit groups of 2nd interleave plane
  ** bit 2: checksum of all even 4-bit groups of 3rd interleave plane
  ** bit 3: checksum of all even 4-bit groups of 4th interleave plane
  ** bit 4: checksum of all odd 4-bit groups of 1st interleave plane
  ** bit 5: checksum of all odd 4-bit groups of 2nd interleave plane
  ** bit 6: checksum of all odd 4-bit groups of 3rd interleave plane
  ** bit 7: checksum of all odd 4-bit groups of 4th interleave plane
  */

  /*-------------------------------------------------------------------*/
  /* Calculate the "every 8 bits" checksums (4 byte groupings).        */
  /*-------------------------------------------------------------------*/
  tmp.half[0] = (ui8)(every1[0].half[0] ^ every1[0].half[1]);
  tmp.byte[0] ^= tmp.byte[1];
  tmp.half[1] = (ui16)(every1[1].half[0] ^ every1[1].half[1]);
  tmp.byte[2] ^= tmp.byte[3];
  ecc[3] = (ui8)(NibXor[tmp.byte[0]] | (NibXor[tmp.byte[2]] << 4));
  /*
  ** ecc[3] contains
  ** bit 0: checksum of all even 8-bit groups of 1st interleave plane
  ** bit 1: checksum of all even 8-bit groups of 2nd interleave plane
  ** bit 2: checksum of all even 8-bit groups of 3rd interleave plane
  ** bit 3: checksum of all even 8-bit groups of 4th interleave plane
  ** bit 4: checksum of all odd 8-bit groups of 1st interleave plane
  ** bit 5: checksum of all odd 8-bit groups of 2nd interleave plane
  ** bit 6: checksum of all odd 8-bit groups of 3rd interleave plane
  ** bit 7: checksum of all odd 8-bit groups of 4th interleave plane
  */

  /*-------------------------------------------------------------------*/
  /* Calculate the "every 16 bits" checksums (every 2 longs).          */
  /*-------------------------------------------------------------------*/
  tmp.half[0] = (ui16)(every2[0].half[0] ^ every2[0].half[1]);
  tmp.byte[0] ^= tmp.byte[1];
  tmp.half[1] = (ui16)(every2[1].half[0] ^ every2[1].half[1]);
  tmp.byte[2] ^= tmp.byte[3];
  ecc[4] = (ui8)(NibXor[tmp.byte[0]] | (NibXor[tmp.byte[2]] << 4));
  /*
  ** ecc[4] contains
  ** bit 0: checksum of all even 16-bit groups of 1st interleave plane
  ** bit 1: checksum of all even 16-bit groups of 2nd interleave plane
  ** bit 2: checksum of all even 16-bit groups of 3rd interleave plane
  ** bit 3: checksum of all even 16-bit groups of 4th interleave plane
  ** bit 4: checksum of all odd 16-bit groups of 1st interleave plane
  ** bit 5: checksum of all odd 16-bit groups of 2nd interleave plane
  ** bit 6: checksum of all odd 16-bit groups of 3rd interleave plane
  ** bit 7: checksum of all odd 16-bit groups of 4th interleave plane
  */

  /*-------------------------------------------------------------------*/
  /* Calculate the "every 32 bits" checksums (every 4 longs).          */
  /*-------------------------------------------------------------------*/
  tmp.half[0] = (ui16)(every4[0].half[0] ^ every4[0].half[1]);
  tmp.byte[0] ^= tmp.byte[1];
  tmp.half[1] = (ui16)(every4[1].half[0] ^ every4[1].half[1]);
  tmp.byte[2] ^= tmp.byte[3];
  ecc[5] = (ui8)(NibXor[tmp.byte[0]] | (NibXor[tmp.byte[2]] << 4));
  /*
  ** ecc[5] contains
  ** bit 0: checksum of all even 32-bit groups of 1st interleave plane
  ** bit 1: checksum of all even 32-bit groups of 2nd interleave plane
  ** bit 2: checksum of all even 32-bit groups of 3rd interleave plane
  ** bit 3: checksum of all even 32-bit groups of 4th interleave plane
  ** bit 4: checksum of all odd 32-bit groups of 1st interleave plane
  ** bit 5: checksum of all odd 32-bit groups of 2nd interleave plane
  ** bit 6: checksum of all odd 32-bit groups of 3rd interleave plane
  ** bit 7: checksum of all odd 32-bit groups of 4th interleave plane
  */

  /*-------------------------------------------------------------------*/
  /* Calculate the "every 64 bits" checksums (every 8 longs).          */
  /*-------------------------------------------------------------------*/
  tmp.half[0] = (ui16)(every8[0].half[0] ^ every8[0].half[1]);
  tmp.byte[0] ^= tmp.byte[1];
  tmp.half[1] = (ui16)(every8[1].half[0] ^ every8[1].half[1]);
  tmp.byte[2] ^= tmp.byte[3];
  ecc[6] = (ui8)(NibXor[tmp.byte[0]] | (NibXor[tmp.byte[2]] << 4));
  /*
  ** ecc[6] contains
  ** bit 0: checksum of all even 32-bit groups of 1st interleave plane
  ** bit 1: checksum of all even 32-bit groups of 2nd interleave plane
  ** bit 2: checksum of all even 32-bit groups of 3rd interleave plane
  ** bit 3: checksum of all even 32-bit groups of 4th interleave plane
  ** bit 4: checksum of all odd 32-bit groups of 1st interleave plane
  ** bit 5: checksum of all odd 32-bit groups of 2nd interleave plane
  ** bit 6: checksum of all odd 32-bit groups of 3rd interleave plane
  ** bit 7: checksum of all odd 32-bit groups of 4th interleave plane
  */

  /*-------------------------------------------------------------------*/
  /* Calculate the "every 128 bits" checksums (every 16 longs).        */
  /*-------------------------------------------------------------------*/
  tmp.half[0] = (ui16)(every16[0].half[0] ^ every16[0].half[1]);
  tmp.byte[0] ^= tmp.byte[1];
  tmp.half[1] = (ui16)(every16[1].half[0] ^ every16[1].half[1]);
  tmp.byte[2] ^= tmp.byte[3];
  ecc[7] = (ui8)(NibXor[tmp.byte[0]] | (NibXor[tmp.byte[2]] << 4));
  /*
  ** ecc[7] contains
  ** bit 0: checksum of all even 128-bit groups of 1st interleave plane
  ** bit 1: checksum of all even 128-bit groups of 2nd interleave plane
  ** bit 2: checksum of all even 128-bit groups of 3rd interleave plane
  ** bit 3: checksum of all even 128-bit groups of 4th interleave plane
  ** bit 4: checksum of all odd 128-bit groups of 1st interleave plane
  ** bit 5: checksum of all odd 128-bit groups of 2nd interleave plane
  ** bit 6: checksum of all odd 128-bit groups of 3rd interleave plane
  ** bit 7: checksum of all odd 128-bit groups of 4th interleave plane
  */

  /*-------------------------------------------------------------------*/
  /* Calculate the "every 256 bits" checksums (every 32 longs).        */
  /*-------------------------------------------------------------------*/
  tmp.half[0] = (ui16)(every32[0].half[0] ^ every32[0].half[1]);
  tmp.byte[0] ^= tmp.byte[1];
  tmp.half[1] = (ui16)(every32[1].half[0] ^ every32[1].half[1]);
  tmp.byte[2] ^= tmp.byte[3];
  ecc[8] = (ui8)(NibXor[tmp.byte[0]] | (NibXor[tmp.byte[2]] << 4));
  /*
  ** ecc[8] contains
  ** bit 0: checksum of all even 256-bit groups of 1st interleave plane
  ** bit 1: checksum of all even 256-bit groups of 2nd interleave plane
  ** bit 2: checksum of all even 256-bit groups of 3rd interleave plane
  ** bit 3: checksum of all even 256-bit groups of 4th interleave plane
  ** bit 4: checksum of all odd 256-bit groups of 1st interleave plane
  ** bit 5: checksum of all odd 256-bit groups of 2nd interleave plane
  ** bit 6: checksum of all odd 256-bit groups of 3rd interleave plane
  ** bit 7: checksum of all odd 256-bit groups of 4th interleave plane
  */

  /*-------------------------------------------------------------------*/
  /* Calculate the "every 512 bits" checksums (every 64 longs).        */
  /*-------------------------------------------------------------------*/
  tmp.half[0] = (ui16)(every64[0].half[0] ^ every64[0].half[1]);
  tmp.byte[0] ^= tmp.byte[1];
  tmp.half[1] = (ui16)(every64[1].half[0] ^ every64[1].half[1]);
  tmp.byte[2] ^= tmp.byte[3];
  ecc[9] = (ui8)(NibXor[tmp.byte[0]] | (NibXor[tmp.byte[2]] << 4));
  /*
  ** ecc[9] contains
  ** bit 0: checksum of all even 512-bit groups of 1st interleave plane
  ** bit 1: checksum of all even 512-bit groups of 2nd interleave plane
  ** bit 2: checksum of all even 512-bit groups of 3rd interleave plane
  ** bit 3: checksum of all even 512-bit groups of 4th interleave plane
  ** bit 4: checksum of all odd 512-bit groups of 1st interleave plane
  ** bit 5: checksum of all odd 512-bit groups of 2nd interleave plane
  ** bit 6: checksum of all odd 512-bit groups of 3rd interleave plane
  ** bit 7: checksum of all odd 512-bit groups of 4th interleave plane
  */
}

/***********************************************************************/
/*   DecodeEcc: Correct 512 bytes of user data using specified ECC     */
/*                                                                     */
/*      Inputs: data = pointer to 512 bytes of user data               */
/*              ecc = original ECC encoding performed on data          */
/*                                                                     */
/*     Returns: 0 if decoding successful, else -1                      */
/*                                                                     */
/***********************************************************************/
int DecodeEcc(ui32 *data, const ui8 *ecc)
{
  int byte0 = 0, byte1 = 0, byte2 = 0, byte3 = 0, i;
  int bit0 = 0, bit1 = 0, bit2 = 0, bit3 = 0;
  int err0 = 0, err1 = 0, err2 = 0, err3 = 0;
  ui8 curr[10], diff, tmp_byte;

  /*-------------------------------------------------------------------*/
  /* Get the current parity for the data.                              */
  /*-------------------------------------------------------------------*/
  EncodeEcc(data, curr);

  /*-------------------------------------------------------------------*/
  /* Get the error byte and bit for all 4 interleavings.               */
  /*-------------------------------------------------------------------*/
  for (i = 9; i >= 0; --i)
  {
    /*-----------------------------------------------------------------*/
    /* Get the difference between the original and current parity.     */
    /*-----------------------------------------------------------------*/
    diff = (ui8)(ecc[i] ^ curr[i]);

    /*-----------------------------------------------------------------*/
    /* Do first interleaving.                                          */
    /*-----------------------------------------------------------------*/
    tmp_byte = (ui8)(diff & MASK1);

    /*-----------------------------------------------------------------*/
    /* If both bits, 0 and 4, are 1, uncorrectable error.              */
    /*-----------------------------------------------------------------*/
    if (tmp_byte == MASK1)
      return -1;

    /*-----------------------------------------------------------------*/
    /* Else if one of the bits is 1, correctable or code error.        */
    /*-----------------------------------------------------------------*/
    else if (tmp_byte)
    {
      /*---------------------------------------------------------------*/
      /* If no error recorded, set error to either correctable or code.*/
      /*---------------------------------------------------------------*/
      if (err0 == 0)
      {
        if (i == 9)
          err0 = CORRECTABLE;
        else
          err0 = CODE_ERROR;
      }

      /*---------------------------------------------------------------*/
      /* If bit 4 is set, odd, set error byte or bit number.           */
      /*---------------------------------------------------------------*/
      if (tmp_byte > 0x8)
      {
        if (i >= 3)
          byte0 |= (1 << (i - 3));
        else
          bit0 |= (1 << i);
      }
    }

    /*-----------------------------------------------------------------*/
    /* Else if correctable error recorded, switch to code_error.       */
    /*-----------------------------------------------------------------*/
    else if (err0 == CORRECTABLE)
      err0 = CODE_ERROR;

    /*-----------------------------------------------------------------*/
    /* Do second interleaving.                                         */
    /*-----------------------------------------------------------------*/
    tmp_byte = (ui8)(diff & MASK2);

    /*-----------------------------------------------------------------*/
    /* If both bits, 1 and 5, are 1, uncorrectable error.              */
    /*-----------------------------------------------------------------*/
    if (tmp_byte == MASK2)
      return -1;

    /*-----------------------------------------------------------------*/
    /* Else if one of the bits is 1, correctable or code error.        */
    /*-----------------------------------------------------------------*/
    else if (tmp_byte)
    {
      /*---------------------------------------------------------------*/
      /* If no error recorded, set error to either correctable or code.*/
      /*---------------------------------------------------------------*/
      if (err1 == 0)
      {
        if (i == 9)
          err1 = CORRECTABLE;
        else
          err1 = CODE_ERROR;
      }

      /*---------------------------------------------------------------*/
      /* If bit 5 is set, odd, set error byte or bit number.           */
      /*---------------------------------------------------------------*/
      if (tmp_byte > 0x8)
      {
        if (i >= 3)
          byte1 |= (1 << (i - 3));
        else
          bit1 |= (1 << i);
      }
    }

    /*-----------------------------------------------------------------*/
    /* Else if correctable error recorded, switch to code_error.       */
    /*-----------------------------------------------------------------*/
    else if (err1 == CORRECTABLE)
      err1 = CODE_ERROR;

    /*-----------------------------------------------------------------*/
    /* Do third interleaving.                                          */
    /*-----------------------------------------------------------------*/
    tmp_byte = (ui8)(diff & MASK3);

    /*-----------------------------------------------------------------*/
    /* If both bits, 2 and 6, are 1, uncorrectable error.              */
    /*-----------------------------------------------------------------*/
    if (tmp_byte == MASK3)
      return -1;

    /*-----------------------------------------------------------------*/
    /* Else if one of the bits is 1, correctable or code error.        */
    /*-----------------------------------------------------------------*/
    else if (tmp_byte)
    {
      /*---------------------------------------------------------------*/
      /* If no error recorded, set error to either correctable or code.*/
      /*---------------------------------------------------------------*/
      if (err2 == 0)
      {
        if (i == 9)
          err2 = CORRECTABLE;
        else
          err2 = CODE_ERROR;
      }

      /*---------------------------------------------------------------*/
      /* If bit 6 is set, odd, set error byte number.                  */
      /*---------------------------------------------------------------*/
      if (tmp_byte > 0x8)
      {
        if (i >= 3)
          byte2 |= (1 << (i - 3));
        else
          bit2 |= (1 << i);
      }
    }

    /*-----------------------------------------------------------------*/
    /* Else if correctable error recorded, switch to code_error.       */
    /*-----------------------------------------------------------------*/
    else if (err2 == CORRECTABLE)
      err2 = CODE_ERROR;

    /*-----------------------------------------------------------------*/
    /* Do fourth interleaving.                                         */
    /*-----------------------------------------------------------------*/
    tmp_byte = (ui8)(diff & MASK4);

    /*-----------------------------------------------------------------*/
    /* If both bits, 3 and 7, are 1, uncorrectable error.              */
    /*-----------------------------------------------------------------*/
    if (tmp_byte == MASK4)
      return -1;

    /*-----------------------------------------------------------------*/
    /* Else if one of the bits is 1, correctable or code error.        */
    /*-----------------------------------------------------------------*/
    else if (tmp_byte)
    {
      /*---------------------------------------------------------------*/
      /* If no error recorded, set error to either correctable or code.*/
      /*---------------------------------------------------------------*/
      if (err3 == 0)
      {
        if (i == 9)
          err3 = CORRECTABLE;
        else
          err3 = CODE_ERROR;
      }

      /*---------------------------------------------------------------*/
      /* If bit 7 is set, odd, set error byte or bit number.           */
      /*---------------------------------------------------------------*/
      if (tmp_byte > 0x8)
      {
        if (i >= 3)
          byte3 |= (1 << (i - 3));
        else
          bit3 |= (1 << i);
      }
    }

    /*-----------------------------------------------------------------*/
    /* Else if correctable error recorded, switch to code_error.       */
    /*-----------------------------------------------------------------*/
    else if (err3 == CORRECTABLE)
      err3 = CODE_ERROR;
  }

  /*-------------------------------------------------------------------*/
  /* Correct all correctable errors.                                   */
  /*-------------------------------------------------------------------*/
  if (err0 == CORRECTABLE)
    ((ui8 *)data)[4 * byte0 + bit0 / 2] ^= 1 << (4 * (bit0 % 2));
  if (err1 == CORRECTABLE)
    ((ui8 *)data)[4 * byte1 + bit1 / 2] ^= 1 << (4 * (bit1 % 2) + 1);
  if (err2 == CORRECTABLE)
    ((ui8 *)data)[4 * byte2 + bit2 / 2] ^= 1 << (4 * (bit2 % 2) + 2);
  if (err3 == CORRECTABLE)
    ((ui8 *)data)[4 * byte3 + bit3 / 2] ^= 1 << (4 * (bit3 % 2) + 3);
  return 0;
}
#endif /* INC_NAND_FS && NUM_FFS_VOLS */

