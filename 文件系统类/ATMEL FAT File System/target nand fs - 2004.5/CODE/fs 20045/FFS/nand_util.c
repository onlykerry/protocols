/***********************************************************************/
/*                                                                     */
/*   Module:  nand_util.c                                              */
/*   Release: 2004.5                                                   */
/*   Version: 2004.1                                                   */
/*   Purpose: Specific NAND code for the flash file system             */
/*                                                                     */
/*---------------------------------------------------------------------*/
/*                                                                     */
/*               Copyright 2004, Blunk Microsystems                    */
/*                      ALL RIGHTS RESERVED                            */
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
#define CRC32_START     0xFFFFFFFF  /* starting CRC bit string */
#define CRC32_FINAL     0xDEBB20E3  /* summed over data and CRC */

#define RWR_CTRL_OK     5

/***********************************************************************/
/* Macro Definitions                                                   */
/***********************************************************************/
#define CRC_UPDATE(crc, c)  ((crc >> 8) ^ FfsCrcTab[(ui8)(crc ^ c)])

/***********************************************************************/
/* Constant Data Definitions                                           */
/***********************************************************************/
/*
** CRC Lookup Table
*/
static const ui32 FfsCrcTab[256] =
{
  0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
  0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
  0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
  0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
  0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
  0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
  0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
  0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
  0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
  0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
  0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
  0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
  0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
  0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
  0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
  0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
  0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
  0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
  0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
  0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
  0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
  0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
  0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
  0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
  0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
  0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
  0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
  0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
  0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
  0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
  0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
  0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
  0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
  0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
  0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
  0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
  0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
  0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
  0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
  0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
  0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
  0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
  0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
  0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
  0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
  0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
  0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
  0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
  0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
  0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
  0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
  0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
  0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
  0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
  0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
  0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
  0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
  0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
  0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
  0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
  0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
  0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
  0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
  0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

const ui8 NumberOnes[] =
{
/*0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
};

#if FFS_DEBUG
int printf(const char *, ...);
#endif

/***********************************************************************/
/* Local Function Definitions                                          */
/***********************************************************************/

/***********************************************************************/
/*   ctrl_page: Check the page's type, read from its extra bytes       */
/*                                                                     */
/*       Input: type = page type                                       */
/*                                                                     */
/*     Returns: TRUE if page holds control information, else FALSE     */
/*                                                                     */
/***********************************************************************/
static int ctrl_page(ui32 addr)
{
  int ones = 0;
  ui32 type;

  /*-------------------------------------------------------------------*/
  /* Read the page's type code.                                        */
  /*-------------------------------------------------------------------*/
  type = Flash->driver.nand.read_type(addr, Flash->vol);

  /*-------------------------------------------------------------------*/
  /* Do a quick check for an error-free data sector type code.         */
  /*-------------------------------------------------------------------*/
  if (type == 0xFFFFFFFF || type == 0x00000000)
    return FALSE;

  /*-------------------------------------------------------------------*/
  /* Count number of 1 valued bits in type code.                       */
  /*-------------------------------------------------------------------*/
  while (type)
  {
    ones += NumberOnes[type & 0xF];
    type >>= 4;
  }

  /*-------------------------------------------------------------------*/
  /* If between 8 and 24 bits are 1, it's a control sector.            */
  /*-------------------------------------------------------------------*/
  return (8 < ones) && (ones < 24);
}

/***********************************************************************/
/* is_ctrl_sect: Check if a sector contains control information and    */
/*               update the control CRC calculation                    */
/*                                                                     */
/*     Inputs: sect = sector to check                                  */
/*             crc  = current CRC value                                */
/*                                                                     */
/*    Returns: TRUE if sector has control info, FALSE otherwise        */
/*                                                                     */
/***********************************************************************/
static ui8 is_ctrl_sect(ui32 sect, ui32 *crc)
{
  int i;
  ui8 *buf = Flash->tmp_sect;
  ui32 addr = Flash->mem_base + Flash->sect_sz * sect;
  ui32 wear = Flash->blocks[sect / Flash->block_sects].wear_count;

  /*-------------------------------------------------------------------*/
  /* Read in a page at a time.                                         */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < Flash->pages_per_sect; ++i, addr += Flash->page_size)
  {
    /*-----------------------------------------------------------------*/
    /* Check that the page contains control information.               */
    /*-----------------------------------------------------------------*/
    if (ctrl_page(addr) == FALSE)
      return FALSE;

    /*-----------------------------------------------------------------*/
    /* Read the page data.                                             */
    /*-----------------------------------------------------------------*/
    if (Flash->driver.nand.read_page(buf, addr, wear, Flash->vol))
      return FALSE;
    buf += Flash->page_size;
  }

  /*-------------------------------------------------------------------*/
  /* Perform CRC over it.                                              */
  /*-------------------------------------------------------------------*/
  buf = Flash->tmp_sect;
  for (i = 0; i < Flash->sect_sz; ++i)
    *crc = CRC_UPDATE(*crc, buf[i]);

  return TRUE;
}

/***********************************************************************/
/*   next_ctrl: Find next valid ctrl in volume, if any is present      */
/*                                                                     */
/*      Inputs: *start_sect = first sector to search                   */
/*                                                                     */
/*      Output: *seq_num = sequence number if valid ctrl info found    */
/*                                                                     */
/*     Returns: Starting sector number if control info found, else -1  */
/*             if no more valid ctrls in flash                         */
/*                                                                     */
/***********************************************************************/
static int next_ctrl(ui32 *start_sect, ui32 *seq_num)
{
  ui32 crc, num_sects, i, ctrl_sect;
  int r_val;

  /*-------------------------------------------------------------------*/
  /* Look through the sectors for ctrl info sectors.                   */
  /*-------------------------------------------------------------------*/
  for (; *start_sect < Flash->num_sects; *start_sect += 1)
  {
    /*-----------------------------------------------------------------*/
    /* Initialize the control information CRC calculation.             */
    /*-----------------------------------------------------------------*/
    crc = CRC32_START;

    /*-----------------------------------------------------------------*/
    /* If this sector is a control sector, check the ctrl validity.    */
    /*-----------------------------------------------------------------*/
    if (is_ctrl_sect(*start_sect, &crc))
    {
      /*---------------------------------------------------------------*/
      /* Get the control information size, in number of sectors.       */
      /*---------------------------------------------------------------*/
      num_sects = ((ui32 *)Flash->tmp_sect)[0];
#if FFS_SWAP_ENDIAN
      num_sects = ((num_sects & 0xFF) << 24) |
                  ((num_sects & 0xFF00) << 8) |
                  ((num_sects & 0xFF0000) >> 8) |
                  ((num_sects & 0xFF000000) >> 24);
#endif
      num_sects = (num_sects + Flash->sect_sz - 1) / Flash->sect_sz;

      /*---------------------------------------------------------------*/
      /* Check if the control information size is reasonable.          */
      /*---------------------------------------------------------------*/
      if (num_sects > 0 &&
          num_sects <= Flash->block_sects * Flash->max_set_blocks)
      {
        /*-------------------------------------------------------------*/
        /* Read the control sequence number.                           */
        /*-------------------------------------------------------------*/
        *seq_num = ((ui32 *)Flash->tmp_sect)[1];
#if FFS_SWAP_ENDIAN
        *seq_num = ((*seq_num & 0xFF) << 24) |
                   ((*seq_num & 0xFF00) << 8) |
                   ((*seq_num & 0xFF0000) >> 8) |
                   ((*seq_num & 0xFF000000) >> 24);
#endif

        /*-------------------------------------------------------------*/
        /* Read remaining sectors, updating the CRC and checking type. */
        /*-------------------------------------------------------------*/
        for (i = 1, ctrl_sect = *start_sect;; ++i)
        {
          /*-----------------------------------------------------------*/
          /* Check if we have read enough sectors.                     */
          /*-----------------------------------------------------------*/
          if (i == num_sects)
          {
            if (crc == CRC32_FINAL)
            {
              r_val = (int)*start_sect;
              *start_sect += num_sects;
              return r_val;
            }
            break;
          }

          /*-----------------------------------------------------------*/
          /* If current ctrl sect is at the end of a block, next ctrl  */
          /* sector is in next ctrl block.                             */
          /*-----------------------------------------------------------*/
          if ((ctrl_sect + 1) % Flash->block_sects == 0)
          {
            /*---------------------------------------------------------*/
            /* If next ctrl block has valid value, use it, else it's   */
            /* an invalid ctrl sector.                                 */
            /*---------------------------------------------------------*/
            ui8 hi_byte = Flash->tmp_sect[Flash->sect_sz - 2];
            ui8 lo_byte = Flash->tmp_sect[Flash->sect_sz - 1];
            ui16 next_ctrl_block = (ui16)((hi_byte << 8) | lo_byte);
            if (next_ctrl_block >= Flash->num_blocks)
              break;
            ctrl_sect = next_ctrl_block * Flash->block_sects +
                        Flash->hdr_sects;
          }
          else
            ++ctrl_sect;

          if (!is_ctrl_sect(ctrl_sect, &crc))
            break;
        }
      }
    }
  }
  return -1;
}

/***********************************************************************/
/* invalidate_ctrl_sects_upto: Invalidate all control sectors from     */
/*              beginning upto bad sector                              */
/*                                                                     */
/*       Input: bad_sect = sector to stop at                           */
/*                                                                     */
/*     Returns: First sector in next control block past bad one,       */
/*              FLAST_SECT if no such block                            */
/*                                                                     */
/***********************************************************************/
static ui32 invalidate_ctrl_sects_upto(ui32 bad_sect)
{
  ui32 sect, bad_block = bad_sect / Flash->block_sects;

  /*-------------------------------------------------------------------*/
  /* Mark all sectors from begining to bad sector as dirty.            */
  /*-------------------------------------------------------------------*/
  sect = Flash->frst_ctrl_sect;
  for (;;)
  {
    Flash->sect_tbl[sect].prev = FDRTY_SECT;
    Flash->blocks[sect / Flash->block_sects].ctrl_block = FALSE;

    /*-----------------------------------------------------------------*/
    /* If we've reached the bad sector, stop.                          */
    /*-----------------------------------------------------------------*/
    if (sect == bad_sect)
      break;

    sect = Flash->sect_tbl[sect].next;
    PfAssert(sect != FLAST_SECT);
  }

  /*-------------------------------------------------------------------*/
  /* Get first control sector in next block past bad one.              */
  /*-------------------------------------------------------------------*/
  while (sect != FLAST_SECT && sect / Flash->block_sects == bad_block)
    sect = Flash->sect_tbl[sect].next;

  return sect;
}

/***********************************************************************/
/* update_erase_set: Look for a block in erasable set, and if found,   */
/*              take it out and replace it with another one            */
/*                                                                     */
/*       Input: block = bad block to take out of erasable set          */
/*                                                                     */
/***********************************************************************/
static void update_erase_set(ui32 block)
{
  int s, best_block = -1, f_best_block = -1, f, b, i;

  /*-------------------------------------------------------------------*/
  /* Look for block in erasable set.                                   */
  /*-------------------------------------------------------------------*/
  for (s = 0; s < Flash->set_blocks; ++s)
  {
    /*-----------------------------------------------------------------*/
    /* If block in erasable set, take it out.                          */
    /*-----------------------------------------------------------------*/
    if (Flash->erase_set[s] == block)
    {
      for (b = 0; b < Flash->num_blocks; ++b)
        if (!Flash->blocks[b].bad_block && !Flash->blocks[b].ctrl_block
            && Flash->sect_tbl[(b + 1) * Flash->block_sects - 1].prev !=
               FFREE_SECT)
        {
          /*-----------------------------------------------------------*/
          /* Check if block is in erasable set already.                */
          /*-----------------------------------------------------------*/
          for (i = 0; i < Flash->set_blocks; ++i)
            if (Flash->erase_set[i] == b)
              break;

          /*-----------------------------------------------------------*/
          /* If block not in erase set see if it is a good candidate.  */
          /* When computing the selector function, ignore wear count.  */
          /*-----------------------------------------------------------*/
          if (i == Flash->set_blocks)
          {
            f = (int)(16 * (Flash->block_sects -
                      Flash->blocks[b].used_sects) +
                      (Flash->high_wear - Flash->blocks[b].wear_count));
            if (best_block == -1 || f_best_block < f)
            {
              best_block = b;
              f_best_block = f;
            }
          }
        }

      /*---------------------------------------------------------------*/
      /* If a replacement block found, use it.                         */
      /*---------------------------------------------------------------*/
      if (best_block != -1)
        Flash->erase_set[s] = best_block;

      /*---------------------------------------------------------------*/
      /* Else the flash contains only free/bad blocks. Remove bad block*/
      /* from erase set and replace it with the block containing the   */
      /* first free sector.                                            */
      /*---------------------------------------------------------------*/
      else
        Flash->erase_set[s] = (int)(Flash->free_sect/Flash->block_sects);
    }
  }
}

/***********************************************************************/
/* mark_block_bad: On a write sector failure, invalidate whole block   */
/*                                                                     */
/*       Input: block = block to be marked bad                         */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int mark_block_bad(ui32 block)
{
  ui32 sect, i;

  /*-------------------------------------------------------------------*/
  /* Mark block as bad.                                                */
  /*-------------------------------------------------------------------*/
  Flash->blocks[block].bad_block = TRUE;
  ++Flash->bad_blocks;
  Flash->blocks[block].ctrl_block = FALSE;

  /*-------------------------------------------------------------------*/
  /* Remove any free sectors from bad block and mark all non-data      */
  /* sectors as invalid.                                               */
  /*-------------------------------------------------------------------*/
  sect = block * Flash->block_sects;
  for (i = 0; i++ < Flash->block_sects; ++sect)
  {
    if (Flash->sect_tbl[sect].prev == FFREE_SECT)
    {
      if (Flash->free_sect != Flash->last_free_sect)
      {
        --Flash->free_sects;
        Flash->free_sect = Flash->sect_tbl[sect].next;
        Flash->sect_tbl[sect].prev = FNVLD_SECT;
      }
      else
      {
        PfAssert(FALSE); /*lint !e506, !e774*/
        return -1;
      }
    }
    else if (Flash->sect_tbl[sect].prev == FDRTY_SECT)
      Flash->sect_tbl[sect].prev = FNVLD_SECT;
  }

  /*-------------------------------------------------------------------*/
  /* Update erasable set if bad block is in there.                     */
  /*-------------------------------------------------------------------*/
  update_erase_set(block);

  /*-------------------------------------------------------------------*/
  /* If bad blocks exceeded maximum, error.                            */
  /*-------------------------------------------------------------------*/
  if (Flash->bad_blocks > Flash->max_bad_blocks)
  {
    set_errno(EIO);
    return -1;
  }
  return 0;
}

/***********************************************************************/
/* free_ctrl_blocks_from: Free all control blocks from current sector  */
/*                                                                     */
/*       Input: sect = sector to start at                              */
/*                                                                     */
/***********************************************************************/
static void free_ctrl_blocks_from(ui32 sect)
{
  ui32 next_sect, one_past_last_ctrl_sect = FLAST_SECT;

  while (sect != FLAST_SECT)
  {
    next_sect = Flash->sect_tbl[sect].next;

    /*-----------------------------------------------------------------*/
    /* Mark sector as free.                                            */
    /*-----------------------------------------------------------------*/
    Flash->sect_tbl[sect].prev = FFREE_SECT;
    ++Flash->free_sects;
    Flash->blocks[sect / Flash->block_sects].ctrl_block = FALSE;

    /*-----------------------------------------------------------------*/
    /* If at end of block, add whole block to free list.               */
    /*-----------------------------------------------------------------*/
    if (sect % Flash->block_sects == Flash->block_sects - 1)
    {
      Flash->sect_tbl[Flash->last_free_sect].next =
                                   (ui16)(sect - Flash->block_sects + 1);
      Flash->sect_tbl[sect].next = FLAST_SECT;
      Flash->last_free_sect = sect;
      one_past_last_ctrl_sect = FLAST_SECT;
    }
    else
    {
      Flash->sect_tbl[sect].next = (ui16)(sect + 1);
      one_past_last_ctrl_sect = sect + 1;
    }
    sect = next_sect;
  }

  /*-------------------------------------------------------------------*/
  /* If there are sectors left in last control block, mark them free.  */
  /*-------------------------------------------------------------------*/
  if (one_past_last_ctrl_sect != FLAST_SECT)
    for (sect = one_past_last_ctrl_sect;; ++sect)
    {
      /*---------------------------------------------------------------*/
      /* Mark sector as free.                                          */
      /*---------------------------------------------------------------*/
      Flash->sect_tbl[sect].prev = FFREE_SECT;
      ++Flash->free_sects;

      /*---------------------------------------------------------------*/
      /* If at end of block, add whole block to free list.             */
      /*---------------------------------------------------------------*/
      if (sect % Flash->block_sects == Flash->block_sects - 1)
      {
        Flash->sect_tbl[Flash->last_free_sect].next =
                                   (ui16)(sect - Flash->block_sects + 1);
        Flash->sect_tbl[sect].next = FLAST_SECT;
        Flash->last_free_sect = sect;
        return;
      }
      else
        Flash->sect_tbl[sect].next = (ui16)(sect + 1);
    }
}

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/* FsNandReadSect: Read a sector from the volume                       */
/*                                                                     */
/*      Inputs: buffer = buffer to place sector data into              */
/*              sect_num = sector to be read                           */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int FsNandReadSect(void *buffer, ui32 sect_num)
{
  int i, rc = 0;
  ui32 addr = Flash->mem_base + Flash->sect_sz * sect_num;
  ui32 wear = Flash->blocks[sect_num / Flash->block_sects].wear_count;

  /*-------------------------------------------------------------------*/
  /* Read in a page at a time.                                         */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < Flash->pages_per_sect; ++i, addr += Flash->page_size)
  {
    rc |= Flash->driver.nand.read_page(buffer, addr, wear, Flash->vol);
    buffer = (void *)((ui32)buffer + Flash->page_size);
  }

  return rc;
}

/***********************************************************************/
/* FsNandWriteType: Attempt to invalidate control sectors              */
/*                                                                     */
/*      Inputs: sect_num = control sector to be invalidated            */
/*              type = sector type to be written for this sector       */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure, RWR_CTRL_OK if control    */
/*              was rewritten succesfully                              */
/*                                                                     */
/***********************************************************************/
int FsNandWriteType(ui32 sect_num, ui32 type)
{
  ui32 addr = Flash->mem_base + sect_num * Flash->sect_sz, bad_block;
  ui32 frst_ctrl_sect, j, i;
  FlashGlob *vol;
  int status;

  /*-------------------------------------------------------------------*/
  /* Write sector type a page at a time.                               */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < Flash->pages_per_sect; ++i, addr += Flash->page_size)
  {
    /*-----------------------------------------------------------------*/
    /* Release exclusive access to the flash file system.              */
    /*-----------------------------------------------------------------*/
    vol = Flash;
    semPost(FlashSem);

    /*-----------------------------------------------------------------*/
    /* Write the type.                                                 */
    /*-----------------------------------------------------------------*/
    status = vol->driver.nand.write_type(addr, type, vol->vol);

    /*-----------------------------------------------------------------*/
    /* Acquire exclusive access to the flash file system.              */
    /*-----------------------------------------------------------------*/
    semPend(FlashSem, WAIT_FOREVER);
    Flash = vol;

    /*-----------------------------------------------------------------*/
    /* If chip error, mark whole block as bad.                         */
    /*-----------------------------------------------------------------*/
    if (status == -1)
    {
      bad_block = sect_num / Flash->block_sects;

      /*---------------------------------------------------------------*/
      /* If part of current control info in this block, invalidate it. */
      /*---------------------------------------------------------------*/
      if (Flash->frst_ctrl_sect / Flash->block_sects == bad_block)
      {
        frst_ctrl_sect = Flash->frst_ctrl_sect;

        /*-------------------------------------------------------------*/
        /* Invalidate all the control sectors.                         */
        /*-------------------------------------------------------------*/
        if (frst_ctrl_sect != (ui32)-1)
        {
          /*-----------------------------------------------------------*/
          /* Invalidate a block at a time.                             */
          /*-----------------------------------------------------------*/
          for (j = frst_ctrl_sect; j != FLAST_SECT;)
          {
            /*---------------------------------------------------------*/
            /* Invalidate control block.                               */
            /*---------------------------------------------------------*/
            do
            {
              Flash->sect_tbl[i = j].prev = FDRTY_SECT;
              j = Flash->sect_tbl[j].next;
            } while (j != FLAST_SECT && (i + 1) % Flash->block_sects);

            /*---------------------------------------------------------*/
            /* Unmark control block flag.                              */
            /*---------------------------------------------------------*/
            Flash->blocks[i / Flash->block_sects].ctrl_block = FALSE;
          }
        }

        /*-------------------------------------------------------------*/
        /* If free control area in bad block, invalidate it.           */
        /*-------------------------------------------------------------*/
        if (Flash->free_ctrl_sect / Flash->block_sects == bad_block)
          Flash->free_ctrl_sect = (ui32)-1;
      }

      /*---------------------------------------------------------------*/
      /* Mark block as bad.                                            */
      /*---------------------------------------------------------------*/
      return mark_block_bad(sect_num);
    }
  }
  return 0;
}

/***********************************************************************/
/* FsNandWriteSect: Write a given sector to flash by calling driver    */
/*              write routine                                          */
/*                                                                     */
/*      Inputs: buffer = pointer to buffer containing sector data      */
/*              sect_num = the sector number                           */
/*              sect_type = sector type (data or control)              */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure, RWR_CTRL_OK if control    */
/*              was rewritten succesfully                              */
/*                                                                     */
/***********************************************************************/
int FsNandWriteSect(void *buffer, ui32 sect_num, ui32 sect_type)
{
  int  i, status;
  ui32 addr = Flash->mem_base + sect_num * Flash->sect_sz;
  ui32 wear = Flash->blocks[sect_num / Flash->block_sects].wear_count;
  ui32 next_sect_num, bad_block = sect_num / Flash->block_sects;
  void *orig_buffer = buffer;
  FlashGlob *vol;

  /*-------------------------------------------------------------------*/
  /* Write sector to flash, a page at a time.                          */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < Flash->pages_per_sect; ++i, addr += Flash->page_size)
  {
    /*-----------------------------------------------------------------*/
    /* Release exclusive access to the flash file system.              */
    /*-----------------------------------------------------------------*/
    vol = Flash;
    semPost(FlashSem);

    /*-----------------------------------------------------------------*/
    /* Write page.                                                     */
    /*-----------------------------------------------------------------*/
    status = vol->driver.nand.write_page(buffer, addr, (ui32)sect_type,
                                         wear, vol->vol);

    /*-----------------------------------------------------------------*/
    /* Acquire exclusive access to the flash file system.              */
    /*-----------------------------------------------------------------*/
    semPend(FlashSem, WAIT_FOREVER);
    Flash = vol;

    /*-----------------------------------------------------------------*/
    /* If write error, either program disturb or bad block.            */
    /*-----------------------------------------------------------------*/
    if (status)
    {
      /*---------------------------------------------------------------*/
      /* Data sector.                                                  */
      /*---------------------------------------------------------------*/
      if (sect_type == DATA_SECT)
      {
        /*-------------------------------------------------------------*/
        /* If bad, block, mark it.                                     */
        /*-------------------------------------------------------------*/
        if (status == -1 && mark_block_bad(bad_block))
          return -1;

        /*-------------------------------------------------------------*/
        /* Relocate bad sector.                                        */
        /*-------------------------------------------------------------*/
        if (FlashRelocSect(sect_num, Flash->free_sect, orig_buffer))
          return -1;

        /*-------------------------------------------------------------*/
        /* If bad block, invalidate sector that was just relocated.    */
        /*-------------------------------------------------------------*/
        if (status == -1)
          Flash->sect_tbl[sect_num].prev = FNVLD_SECT;

        return 0;
      }

      /*---------------------------------------------------------------*/
      /* Control sector.                                               */
      /*---------------------------------------------------------------*/
      else
      {
        /*-------------------------------------------------------------*/
        /* All control sectors up to and including current one become  */
        /* dirty.                                                      */
        /*-------------------------------------------------------------*/
        next_sect_num = invalidate_ctrl_sects_upto(sect_num);

        /*-------------------------------------------------------------*/
        /* If bad block, control sectors in current block become dirty.*/
        /*-------------------------------------------------------------*/
        if (status == -1)
        {
          /*-----------------------------------------------------------*/
          /* Mark all sectors in current block as dirty.               */
          /*-----------------------------------------------------------*/
          sect_num = bad_block * Flash->block_sects;
          for (i = 0; i < Flash->block_sects; ++i, ++sect_num)
            Flash->sect_tbl[sect_num].prev = FDRTY_SECT;

          /*-----------------------------------------------------------*/
          /* Mark block bad.                                           */
          /*-----------------------------------------------------------*/
          if (mark_block_bad(bad_block))
            return -1;

          /*-----------------------------------------------------------*/
          /* Also, invalidate first free control sector.               */
          /*-----------------------------------------------------------*/
          Flash->free_ctrl_sect = (ui32)-1;
        }

        /*-------------------------------------------------------------*/
        /* Else, invalidate all sectors in current block and the free  */
        /* control sector.                                             */
        /*-------------------------------------------------------------*/
        else
        {
            ++sect_num;
            Flash->free_ctrl_sect = (ui32)-1;
            for (; sect_num % Flash->block_sects; ++sect_num)
              Flash->sect_tbl[sect_num].prev = FDRTY_SECT;
        }

        /*-------------------------------------------------------------*/
        /* All control blocks past current one become free.            */
        /*-------------------------------------------------------------*/
        free_ctrl_blocks_from(next_sect_num);

        /*-------------------------------------------------------------*/
        /* No more valid control information left.                     */
        /*-------------------------------------------------------------*/
        Flash->frst_ctrl_sect = Flash->last_ctrl_sect = (ui32)-1;

        /*-------------------------------------------------------------*/
        /* Rewrite the control information.                            */
        /*-------------------------------------------------------------*/
        if (FlashWrCtrl(SKIP_ADJUST_ERASE))
          return -1;
        else
          return RWR_CTRL_OK;
      }
    }
    buffer = (ui8 *)buffer + Flash->page_size;
  }
  return 0;
}

/***********************************************************************/
/* FsNandWriteCtrl: Write a chunk of control memory to NAND            */
/*                                                                     */
/*      Inputs: head = pointer to the beginning of chunk in RAM        */
/*              length = number of bytes the chunk has                 */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure, RWR_CTRL_OK in case the   */
/*              whole ctrl was rewritten because of bad block          */
/*                                                                     */
/***********************************************************************/
int FsNandWriteCtrl(const void *head, uint length)
{
  int i = 0, j, rc;
  ui32 next_block;
  static ui32 crc;
  static int offset;
  static ui32 tot_sects;

  /*-------------------------------------------------------------------*/
  /* Check if this call is just to initialize the static variables.    */
  /*-------------------------------------------------------------------*/
  if (head == Flash)
  {
    tot_sects = Flash->ctrl_sects;
    crc = CRC32_START;
    offset = 0;
    return 0;
  }

  /*-------------------------------------------------------------------*/
  /* Copy data from head to buffer, and as soon as 1 sector in the     */
  /* buffer gets filled, write it to flash.                            */
  /*-------------------------------------------------------------------*/
  while (i < length)
  {
    /*-----------------------------------------------------------------*/
    /* If a full sector worth of ctrl data has been written to buffer, */
    /* send it to flash.                                               */
    /*-----------------------------------------------------------------*/
    PfAssert(offset <= Flash->sect_sz);
    if (offset == Flash->sect_sz)
    {
      /*---------------------------------------------------------------*/
      /* Write the TmpBuf.data to flash.                               */
      /*---------------------------------------------------------------*/
      rc = FlashWriteCtrlSect();
      if (rc)
        return rc;

      /*---------------------------------------------------------------*/
      /* Update the state variables.                                   */
      /*---------------------------------------------------------------*/
      Flash->ctrl_sect = Flash->sect_tbl[Flash->ctrl_sect].next;
      --tot_sects;
      offset = 0;
    }

    /*-----------------------------------------------------------------*/
    /* If this is the last sector in a block and we are down to the    */
    /* last two bytes, write the next ctrl block.                      */
    /*-----------------------------------------------------------------*/
    if (offset == Flash->sect_sz - 2 &&
        (Flash->ctrl_sect + 1) % Flash->block_sects == 0)
    {
      next_block = Flash->sect_tbl[Flash->ctrl_sect].next /
                   Flash->block_sects;
      Flash->tmp_sect[offset++] = (ui8)((next_block & 0xFF00) >> 8);
      Flash->tmp_sect[offset++] = (ui8)(next_block & 0xFF);
      crc = CRC_UPDATE(crc, Flash->tmp_sect[offset - 2]);
      crc = CRC_UPDATE(crc, Flash->tmp_sect[offset - 1]);
    }
    else
    {
      /*---------------------------------------------------------------*/
      /* Copy one byte at a time from head into buffer.                */
      /*---------------------------------------------------------------*/
      Flash->tmp_sect[offset++] = ((ui8 *)head)[i];

      /*---------------------------------------------------------------*/
      /* Update the CRC with the new ctrl info byte.                   */
      /*---------------------------------------------------------------*/
      crc = CRC_UPDATE(crc, ((ui8 *)head)[i++]);
    }
  }

  /*-------------------------------------------------------------------*/
  /* If it's last write, add the CRC at the end of the ctrl info.      */
  /*-------------------------------------------------------------------*/
  if (length == 0)
  {
    /*-----------------------------------------------------------------*/
    /* Fill out the rest of ctrl sectors with 0xFF, except for last    */
    /* ctrl sector.                                                    */
    /*-----------------------------------------------------------------*/
    PfAssert(tot_sects > 0);
    for (; tot_sects > 1; --tot_sects)
    {
      for (j = offset; j < Flash->sect_sz - 2; ++j)
      {
        Flash->tmp_sect[j] = 0xFF;
        crc = CRC_UPDATE(crc, 0xFF);
      }

      /*---------------------------------------------------------------*/
      /* If on block boundary, save next block in last two bytes, else */
      /* use 0xFF.                                                     */
      /*---------------------------------------------------------------*/
      if ((Flash->ctrl_sect + 1) % Flash->block_sects == 0)
      {
        next_block = Flash->sect_tbl[Flash->ctrl_sect].next /
                     Flash->block_sects;
        Flash->tmp_sect[j++] = (ui8)((next_block & 0xFF00) >> 8);
        Flash->tmp_sect[j++] = (ui8)(next_block & 0xFF);
        crc = CRC_UPDATE(crc, Flash->tmp_sect[j - 2]);
        crc = CRC_UPDATE(crc, Flash->tmp_sect[j - 1]);
      }
      else
        while (j < Flash->sect_sz)
        {
          Flash->tmp_sect[j++] = 0xFF;
          crc = CRC_UPDATE(crc, 0xFF);
        }
      PfAssert(j == Flash->sect_sz);
      rc = FlashWriteCtrlSect();
      if (rc)
        return rc;
      offset = 0;
      Flash->ctrl_sect = Flash->sect_tbl[Flash->ctrl_sect].next;
    }

    /*-----------------------------------------------------------------*/
    /* Pad the rest of the last sector with 0xFFs, except for CRC.     */
    /*-----------------------------------------------------------------*/
    PfAssert(offset < Flash->sect_sz - 4);
    for (i = offset; i < Flash->sect_sz - 4; ++i)
    {
      Flash->tmp_sect[i] = 0xFF;
      crc = CRC_UPDATE(crc, 0xFF);
    }

    /*-----------------------------------------------------------------*/
    /* Write the CRC.                                                  */
    /*-----------------------------------------------------------------*/
    for (j = 0, crc = ~crc; j < 4; ++j)
      Flash->tmp_sect[i + j] = (ui8)(crc >> (8 * j));

    /*-----------------------------------------------------------------*/
    /* Write the buffered sector to flash.                             */
    /*-----------------------------------------------------------------*/
    rc = FlashWriteCtrlSect();
    if (rc)
      return rc;
  }

  return 0;
}

/***********************************************************************/
/* FsNandEraseBlockWrapper: Wrapper function for the NAND driver erase */
/*              block                                                  */
/*                                                                     */
/*       Input: addr = address within block to be erased               */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int FsNandEraseBlockWrapper(ui32 addr)
{
  int status;
  FlashGlob *vol;

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to the flash file system.                */
  /*-------------------------------------------------------------------*/
  vol = Flash;
  semPost(FlashSem);

  /*-------------------------------------------------------------------*/
  /* Erase block.                                                      */
  /*-------------------------------------------------------------------*/
  status = vol->driver.nand.erase_block(addr, vol->vol);

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to the flash file system.                */
  /*-------------------------------------------------------------------*/
  semPend(FlashSem, WAIT_FOREVER);
  Flash = vol;

  /*-------------------------------------------------------------------*/
  /* If the block erase fails, it's an invalid block.                  */
  /*-------------------------------------------------------------------*/
  if (status)
  {
    ui32 i, sect, first_invalid, last_invalid = (ui32)-1;
    ui32 block = (addr - Flash->mem_base) / Flash->block_size;
#if FFS_DEBUG
printf("NAND: erase block %u failure!\n", block);
#endif

    /*-----------------------------------------------------------------*/
    /* Increment the total number of bad blocks.                       */
    /*-----------------------------------------------------------------*/
    ++Flash->bad_blocks;

    /*-----------------------------------------------------------------*/
    /* Mark block as bad.                                              */
    /*-----------------------------------------------------------------*/
    PfAssert(!Flash->blocks[block].ctrl_block);
    Flash->blocks[block].bad_block = TRUE;

    /*-----------------------------------------------------------------*/
    /* Mark all sectors in block as invalid.                           */
    /*-----------------------------------------------------------------*/
    sect = block * Flash->block_sects;
    if (Flash->sect_tbl[sect].prev == FFREE_SECT)
    {
      first_invalid = sect;
      last_invalid = first_invalid + Flash->block_sects - 1;
      Flash->free_sects -= Flash->block_sects;
      PfAssert(Flash->sect_tbl[last_invalid].prev == FFREE_SECT);
    }
    else
      first_invalid = (ui32)-1;

    for (i = 0; i < Flash->block_sects; ++i, ++sect)
      Flash->sect_tbl[sect].prev = FNVLD_SECT;

    if (first_invalid != (ui32)-1)
    {
      /*---------------------------------------------------------------*/
      /* Find the sector in free list that points to first invalid     */
      /* sector (if there is one, it is at end of a block).            */
      /*---------------------------------------------------------------*/
      sect = Flash->block_sects - 1;
      for (block = 0; block < Flash->num_blocks; ++block)
      {
        if (!Flash->blocks[block].bad_block)
        {
          if (Flash->sect_tbl[sect].prev == FFREE_SECT &&
              Flash->sect_tbl[sect].next == first_invalid)
          {
            Flash->sect_tbl[sect].next =
                                      Flash->sect_tbl[last_invalid].next;
            if (Flash->sect_tbl[sect].next == FLAST_SECT)
              Flash->last_free_sect = sect;
            break;
          }
        }
        sect += Flash->block_sects;
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* If number of bad blocks has exceeded maximum, error.              */
  /*-------------------------------------------------------------------*/
  if (Flash->bad_blocks > Flash->max_bad_blocks)
    return -1;

  return 0;
}

/***********************************************************************/
/* FsNandUnformatToFormat: NAND specific code to prepare unformatted   */
/*              volume for format                                      */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
int FsNandUnformatToFormat(void)
{
  int sect, block, i;

  Flash->bad_blocks = 0;
  Flash->free_sect = (ui32)-1;
  Flash->free_sects = 0;
  Flash->last_free_sect = (ui32)-1;

  /*-------------------------------------------------------------------*/
  /* Look for bad vs. free blocks.                                     */
  /*-------------------------------------------------------------------*/
  for (block = 0; block < Flash->num_blocks; ++block)
  {
    Flash->blocks[block].used_sects = 0;
    Flash->blocks[block].ctrl_block = FALSE;
    Flash->blocks[block].wear_count = 0;

    sect = (int)(block * Flash->block_sects);
    for (i = 0; i < Flash->block_sects; ++i, ++sect)
    {
      /*---------------------------------------------------------------*/
      /* If a sector is non-empty at this stage, its block is bad.     */
      /*---------------------------------------------------------------*/
      if (Flash->max_bad_blocks && !FsNandEmptySect(sect))
      {
        Flash->blocks[block].bad_block = TRUE;
        ++Flash->bad_blocks;

        /*-------------------------------------------------------------*/
        /* Mark all sectors in block as invalid.                       */
        /*-------------------------------------------------------------*/
        sect = (int)(block * Flash->block_sects);
        for (i = 0; i < Flash->block_sects; ++i, ++sect)
          Flash->sect_tbl[sect].prev = FNVLD_SECT;
        sect = -1;
        break;
      }

      /*---------------------------------------------------------------*/
      /* Mark sector as free.                                          */
      /*---------------------------------------------------------------*/
      Flash->sect_tbl[sect].prev = FFREE_SECT;
      Flash->sect_tbl[sect].next = (ui16)(sect + 1);
    }

    /*-----------------------------------------------------------------*/
    /* If block is free, tie it in with the free list.                 */
    /*-----------------------------------------------------------------*/
    if (sect != -1)
    {
      Flash->free_sects += Flash->block_sects;
      if (Flash->free_sect == (ui32)-1)
        Flash->free_sect = block * Flash->block_sects;
      else
        Flash->sect_tbl[Flash->last_free_sect].next =
                                      (ui16)(block * Flash->block_sects);
      Flash->last_free_sect = (ui32)(sect - 1);
      Flash->sect_tbl[Flash->last_free_sect].next = FLAST_SECT;
      Flash->blocks[block].bad_block = FALSE;
    }
  }
  return 0;
}

/***********************************************************************/
/* FsNandAddVol: Add NAND volume                                       */
/*                                                                     */
/*       Input: driver = pointer to the driver control block           */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
int FsNandAddVol(const FfsVol *driver)
{
  return FlashAddVol(driver, FFS_NAND);
}

/***********************************************************************/
/* FsNandFindLastCtrl: Look for most recent control information        */
/*                                                                     */
/*     Returns: First sector of most recent control information or -1  */
/*              if no control information was found                    */
/*                                                                     */
/***********************************************************************/
int FsNandFindLastCtrl(void)
{
  int  beg_ctrl_sect = -1, ctrl_start;
  ui32 high_seq_num = 0, seq_num, sect;

  /*-------------------------------------------------------------------*/
  /* Scan the whole flash for valid ctrl info. Remember the one with   */
  /* latest sequence number.                                           */
  /*-------------------------------------------------------------------*/
  for (sect = 0;;)
  {
    /*-----------------------------------------------------------------*/
    /* Find the next valid copy of control information in flash.       */
    /*-----------------------------------------------------------------*/
    ctrl_start = next_ctrl(&sect, &seq_num);

    /*-----------------------------------------------------------------*/
    /* If no more valid ctr info found, break.                         */
    /*-----------------------------------------------------------------*/
    if (ctrl_start == -1)
      break;

    /*-----------------------------------------------------------------*/
    /* If this is first valid ctrl info, or it's more recent than the  */
    /* saved one, remember it as the most recent one.                  */
    /*-----------------------------------------------------------------*/
    if ((beg_ctrl_sect == -1) || SEQ_GT(seq_num, high_seq_num))
    {
      beg_ctrl_sect = ctrl_start;
      high_seq_num = seq_num;
#if FFS_DEBUG
      printf("high_seq_num = 0x%08X\n", high_seq_num);
#endif
    }
  }

  /*-------------------------------------------------------------------*/
  /* Return beginning ctrl sect or -1 if ctrl info not present.        */
  /*-------------------------------------------------------------------*/
  return beg_ctrl_sect;
}

/***********************************************************************/
/* FsNandEmptySect: Check if a flash sector is empty                   */
/*                                                                     */
/*       Input: sector = sector to check                               */
/*                                                                     */
/*     Returns: TRUE if empty, FALSE otherwise                         */
/*                                                                     */
/***********************************************************************/
int FsNandEmptySect(ui32 sector)
{
  ui32 i, addr = Flash->mem_base + Flash->sect_sz * sector;

  /*-------------------------------------------------------------------*/
  /* Loop to check each page in the sector.                            */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < Flash->pages_per_sect; ++i, addr += Flash->page_size)
  {
    /*-----------------------------------------------------------------*/
    /* If read fails, sector is considered not empty.                  */
    /*-----------------------------------------------------------------*/
    if (!Flash->driver.nand.page_erased(addr, Flash->vol))
      return FALSE;
  }
  return TRUE;
}
#endif /* INC_NAND_FS && NUM_FFS_VOLS */

