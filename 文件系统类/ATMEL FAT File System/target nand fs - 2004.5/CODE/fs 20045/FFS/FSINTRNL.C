/***********************************************************************/
/*                                                                     */
/*   Module:  fsintrnl.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2004.15                                                  */
/*   Purpose: Implements the internal functions for the flash files    */
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

#if NUM_FFS_VOLS

#define TIMING FALSE
#if TIMING
#include "../../../include/libc/time.h"
clock_t TotWrCtrl = 0, PreWrCtrl = 0, TotRec = 0, RecWrCtrl = 0;
ui32 RecCount = 0, WrCtrlCount = 0;
#endif

#define DO_JMP_RECYCLE     FALSE
#if DO_JMP_RECYCLE
extern int EnableCount;
#endif

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
#define SINGLE_USED        1  /* Flags for sector values on flash */
#define MULTIPLE_USED      2
#define SINGLE_FREE        3
#define MULTIPLE_FREE      4
#define SINGLE_DIRTY       5
#define MULTIPLE_DIRTY     6
#define BLOCK_INVALID      7

#define MAX_WDELTA         15 /* Maximum allowed wear count diff */

#define FLASH_CTRL_UI32s   13 /* Num of ui32s present in ctrl info */

#define WRITE_CTRL         TRUE
#define SKIP_WRITE_CTRL    FALSE

#define SKIP_SECTS         TRUE
#define DONT_SKIP_SECTS    FALSE

#define FFS_FAILED_POWER_UP    1
#define FFS_NORMAL_POWER_UP    2
#define FFS_SLOW_POWER_UP      3

/***********************************************************************/
/* Macro Definitions                                                   */
/***********************************************************************/
#if FFS_SWAP_ENDIAN
  #define write_ctrl_ui32(v)                                            \
    v_ui32 = v;                                                         \
    v_ui32 = ((v_ui32 & 0xFF) << 24) | ((v_ui32 & 0xFF00) << 8) |       \
             ((v_ui32 & 0xFF0000) >> 8) | ((v_ui32 & 0xFF000000) >> 24);\
    status = Flash->write_ctrl(&v_ui32, sizeof(ui32));                  \
    if (status)                                                         \
      goto WrCtrl_exit;

  #define write_ctrl_ui16(v)                                            \
    v_ui16 = v;                                                         \
    v_ui16 = (ui16)(((v_ui16 & 0xFF) << 8) | ((v_ui16 & 0xFF00) >> 8)); \
    status  = Flash->write_ctrl(&v_ui16, sizeof(ui16))                  \
    if (status)                                                         \
      goto WrCtrl_exit;
#else
  #define write_ctrl_ui32(v)                           \
    v_ui32 = v;                                        \
    status = Flash->write_ctrl(&v_ui32, sizeof(ui32)); \
    if (status)                                        \
      goto WrCtrl_exit;

  #define write_ctrl_ui16(v)                           \
    v_ui16 = v;                                        \
    status = Flash->write_ctrl(&v_ui16, sizeof(ui16)); \
    if (status)                                        \
      goto WrCtrl_exit;
#endif /* FFS_SWAP_ENDIAN */

#if FFS_DEBUG
int printf(const char *, ...);
#endif

/***********************************************************************/
/* Local Function Definitions                                          */
/***********************************************************************/

/***********************************************************************/
/*   read_ctrl: Read a chunk of control memory from flash              */
/*                                                                     */
/*      Inputs: head = address to write to in RAM                      */
/*              length = number of bytes the chunk has                 */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int read_ctrl(void *head, uint length)
{
  static ui32 offset = 0;
  ui8 *dst = head;
  ui16 next_ctrl_block = 0xFFFF;

  /*-------------------------------------------------------------------*/
  /* If length is zero, reset offset and read first sector.            */
  /*-------------------------------------------------------------------*/
  if (length == 0)
  {
    offset = 0;
    if (Flash->read_sector(Flash->tmp_sect, Flash->ctrl_sect))
    {
      set_errno(EIO);
      return -1;
    }
    return 0;
  }

  /*-------------------------------------------------------------------*/
  /* Loop, reading one byte at a time.                                 */
  /*-------------------------------------------------------------------*/
  do
  {
    /*-----------------------------------------------------------------*/
    /* If needed, read new sector from flash.                          */
    /*-----------------------------------------------------------------*/
    if (offset == Flash->sect_sz)
    {
      /*---------------------------------------------------------------*/
      /* If at block boundary, update using NextCtrlBlock.             */
      /*---------------------------------------------------------------*/
      if ((++Flash->ctrl_sect % Flash->block_sects) == 0)
      {
        PfAssert(next_ctrl_block < Flash->num_blocks);
        Flash->ctrl_sect = next_ctrl_block * Flash->block_sects +
                           Flash->hdr_sects;
      }

      /*---------------------------------------------------------------*/
      /* Read whole sector in and reset the sector offset.             */
      /*---------------------------------------------------------------*/
      if (Flash->read_sector(Flash->tmp_sect, Flash->ctrl_sect))
      {
        set_errno(EIO);
        return -1;
      }
      offset = 0;
    }

    /*-----------------------------------------------------------------*/
    /* If this is last sector in block, and we are down to last two    */
    /* bytes, read in next ctrl block.                                 */
    /*-----------------------------------------------------------------*/
    if (offset == Flash->sect_sz - 2 &&
        (Flash->ctrl_sect + 1) % Flash->block_sects == 0)
    {
      next_ctrl_block  = (ui16)(Flash->tmp_sect[offset++] << 8);
      next_ctrl_block |= Flash->tmp_sect[offset++];
    }
    else
    {
      /*---------------------------------------------------------------*/
      /* Read one byte of control information and increment.           */
      /*---------------------------------------------------------------*/
      *dst++ = Flash->tmp_sect[offset++];
      --length;
    }
  }
  while (length);
  return 0;
}

/***********************************************************************/
/* read_ctrl_ui32: Read a ui32 from flash in big endian and convert to */
/*              machine endianess                                      */
/*                                                                     */
/*       Input: val = place to store the read ui32                     */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
static int read_ctrl_ui32(ui32 *val)
{
#if FFS_SWAP_ENDIAN
  ui32 tmp;
#endif

  if (read_ctrl(val, sizeof(ui32)))
    return -1;

#if FFS_SWAP_ENDIAN
  tmp = *val;
  *val = ((tmp & 0xFF) << 24) | ((tmp & 0xFF00) << 8) |
         ((tmp & 0xFF0000) >> 8) | ((tmp & 0xFF000000) >> 24);
#endif
  return 0;
}

/***********************************************************************/
/* read_ctrl_ui16: Read a ui16 from flash in big endian and convert to */
/*              machine endianess                                      */
/*                                                                     */
/*       Input: val = place to store the read ui16                     */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
static int read_ctrl_ui16(ui16 *val)
{
#if FFS_SWAP_ENDIAN
  ui16 tmp;
#endif

  if (read_ctrl(val, sizeof(ui16)))
    return -1;

#if FFS_SWAP_ENDIAN
  tmp = *val;
  *val = (ui16)(((tmp & 0xFF) << 8) | ((tmp & 0xFF00) >> 8));
#endif
  return 0;
}

/***********************************************************************/
/* readjust_set: Fill -1 slots in erase set with blocks                */
/*                                                                     */
/***********************************************************************/
static void readjust_set(void)
{
  ui32 i, j, sect;
  int  b, max_f, f_b, max_b;
  ui32 free_blck, last_in_free_block;

  /*-------------------------------------------------------------------*/
  /* First, shift all non -1 slots to the front.                       */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < Flash->set_blocks; ++i)
    if (Flash->erase_set[i] == -1)
    {
      for (j = i + 1; j < Flash->set_blocks; ++j)
        if (Flash->erase_set[j] != -1)
          break;
      if (j >= Flash->set_blocks)
        break;
      Flash->erase_set[i] = Flash->erase_set[j];
      Flash->erase_set[j] = -1;
    }
  for (j = 0; j < i; ++j)
    Flash->blocks[Flash->erase_set[j]].bad_block = TRUE;

  /*-------------------------------------------------------------------*/
  /* Now fill all -1 slots with new blocks.                            */
  /*-------------------------------------------------------------------*/
  for (; i < Flash->set_blocks; ++i)
  {
    for (b = 0, max_f = -1; b < Flash->num_blocks; ++b)
    {
      /*---------------------------------------------------------------*/
      /* If block is not bad, ctrl and has no free sects, candidate.   */
      /*---------------------------------------------------------------*/
      if (!Flash->blocks[b].bad_block && !Flash->blocks[b].ctrl_block &&
          Flash->sect_tbl[(b + 1) * Flash->block_sects - 1].prev !=
          FFREE_SECT)
      {
        /*-------------------------------------------------------------*/
        /* Sanity check.                                               */
        /*-------------------------------------------------------------*/
        PfAssert(Flash->high_wear >= Flash->blocks[b].wear_count);

        /*-------------------------------------------------------------*/
        /* Compute the selector.                                       */
        /*-------------------------------------------------------------*/
        f_b = (int)(16 * (Flash->block_sects -
                    Flash->blocks[b].used_sects) +
                    (Flash->high_wear - Flash->blocks[b].wear_count));
        if (Flash->high_wear >= Flash->blocks[b].wear_count + MAX_WDELTA)
          f_b += 65536;

        /*-------------------------------------------------------------*/
        /* If better candidate, remember it.                           */
        /*-------------------------------------------------------------*/
        if (max_f < f_b)
        {
          max_f = f_b;
          max_b = b;
        }
      }
    }

    /*-----------------------------------------------------------------*/
    /* If candidate block found, place it in set.                      */
    /*-----------------------------------------------------------------*/
    if (max_f != -1)
    {
      Flash->erase_set[i] = max_b; /*lint !e644 */

      /*---------------------------------------------------------------*/
      /* Temporarily mark block as bad so it's not selected next time  */
      /* around.                                                       */
      /*---------------------------------------------------------------*/
      Flash->blocks[max_b].bad_block = TRUE;
    }

    /*-----------------------------------------------------------------*/
    /* Else stop and start using free blocks.                          */
    /*-----------------------------------------------------------------*/
    else
      break;
  }

  /*-------------------------------------------------------------------*/
  /* If not enough non-free blocks to fill set, use free blocks.       */
  /*-------------------------------------------------------------------*/
  for (; i < Flash->set_blocks; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* Select the first free block and get the last sector in block.   */
    /*-----------------------------------------------------------------*/
    free_blck = Flash->free_sect / Flash->block_sects;
    last_in_free_block = (free_blck + 1) * Flash->block_sects - 1;
    PfAssert(Flash->sect_tbl[last_in_free_block].prev == FFREE_SECT);

    /*-----------------------------------------------------------------*/
    /* Put the block on the erase set.                                 */
    /*-----------------------------------------------------------------*/
    Flash->erase_set[i] = (int)free_blck;

    /*-----------------------------------------------------------------*/
    /* Advance free sector to next free block.                         */
    /*-----------------------------------------------------------------*/
    PfAssert(Flash->sect_tbl[last_in_free_block].next != FLAST_SECT);
    Flash->free_sect = Flash->sect_tbl[last_in_free_block].next;
    PfAssert(Flash->sect_tbl[Flash->free_sect].prev == FFREE_SECT);

    /*-----------------------------------------------------------------*/
    /* Mark all free sectors in erase set block to dirty.              */
    /*-----------------------------------------------------------------*/
    sect = free_blck * Flash->block_sects + Flash->hdr_sects;
    for (j = Flash->hdr_sects; j < Flash->block_sects; ++j, ++sect)
    {
      if (Flash->sect_tbl[sect].prev == FFREE_SECT)
      {
        Flash->sect_tbl[sect].prev = FDRTY_SECT;
        --Flash->free_sects;
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* Turn back to good all blocks in erasable set.                     */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < Flash->set_blocks; ++i)
    if (Flash->erase_set[i] != -1)
      Flash->blocks[Flash->erase_set[i]].bad_block = FALSE;
}

/***********************************************************************/
/* recycle_not_possible: Determine whether there are enough free       */
/*              sectors to recycle the erase set                       */
/*                                                                     */
/*     Returns: TRUE if not enough free, FALSE otherwise               */
/*                                                                     */
/***********************************************************************/
static int recycle_not_possible(void)
{
  ui32 block_sects, used, ctrl_blocks;

  FlashRecycleValues(&ctrl_blocks, &used, &block_sects);

  return ctrl_blocks * block_sects + used > Flash->free_sects;
}

/***********************************************************************/
/*   erase_set: Write signature bytes and erase erasable set as part   */
/*              of a recycle operation                                 */
/*                                                                     */
/*       Input: old_set_size = size of erasable set before adjustment  */
/*              due to write ctrl in recycle_finish()                  */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int erase_set(int old_set_size)
{
  int s;

  /*-------------------------------------------------------------------*/
  /* Erase all blocks in next erasable set.                            */
  /*-------------------------------------------------------------------*/
  for (s = 0; s < old_set_size; ++s)
    if (Flash->erase_set[s] != -1 &&
        FlashEraseBlock((ui32)Flash->erase_set[s]))
      return -1;

  return 0;
}

/***********************************************************************/
/* in_erase_set: Determine if a block belongs to the erase set         */
/*                                                                     */
/*       Input: block = block to look for                              */
/*                                                                     */
/*     Returns: TRUE if block in set, FALSE otherwise                  */
/*                                                                     */
/***********************************************************************/
static int in_erase_set(ui32 block)
{
  ui32 s;

  /*-------------------------------------------------------------------*/
  /* Skip checking during format.                                      */
  /*-------------------------------------------------------------------*/
  if (Flash->set_blocks == 1)
    return FALSE;
  for (s = 0; s < Flash->set_blocks; ++s)
    if (Flash->erase_set[s] == block)
      return TRUE;
  return FALSE;
}

/***********************************************************************/
/* choose_free_block: Choose free block to write control info          */
/*                                                                     */
/*       Input: action = flag to indicate if in middle of reycles      */
/*                                                                     */
/*     Returns: First data sector in block, -1 on error                */
/*                                                                     */
/***********************************************************************/
static ui32 choose_free_block(int action)
{
  ui32 i, curr_sect, last_b4_curr = (ui32)-1, last_in_lowest;

  /*-------------------------------------------------------------------*/
  /* If free sector on block boundary, consider the first block in free*/
  /* list, otherwise skip it.                                          */
  /*-------------------------------------------------------------------*/
  if (Flash->free_sect % Flash->block_sects == Flash->hdr_sects)
    curr_sect = Flash->free_sect;
  else
  {
    last_b4_curr = (Flash->free_sect / Flash->block_sects + 1) *
                   Flash->block_sects - 1;
    curr_sect = Flash->sect_tbl[last_b4_curr].next;
  }

  /*-------------------------------------------------------------------*/
  /* Selected free block shouldn't be in erase set.                    */
  /*-------------------------------------------------------------------*/
  PfAssert(!in_erase_set(curr_sect / Flash->block_sects));

  /*-------------------------------------------------------------------*/
  /* If no more free blocks, error.                                    */
  /*-------------------------------------------------------------------*/
  if (curr_sect == FLAST_SECT)
    return (ui32)-1;

  /*-------------------------------------------------------------------*/
  /* Figure out last sector in free block and check block is free.     */
  /*-------------------------------------------------------------------*/
  last_in_lowest = curr_sect + Flash->block_sects - Flash->hdr_sects - 1;
  PfAssert(Flash->sect_tbl[curr_sect].prev == FFREE_SECT &&
           Flash->sect_tbl[last_in_lowest].prev == FFREE_SECT);

  /*-------------------------------------------------------------------*/
  /* Take block out of free list.                                      */
  /*-------------------------------------------------------------------*/
  if (curr_sect == Flash->free_sect)
  {
    /*-----------------------------------------------------------------*/
    /* If no more free blocks, error.                                  */
    /*-----------------------------------------------------------------*/
    if (Flash->sect_tbl[last_in_lowest].next == FLAST_SECT)
      return (ui32)-1;
    Flash->free_sect = Flash->sect_tbl[last_in_lowest].next;
  }
  else
  {
    PfAssert(last_b4_curr != (ui32)-1);
    Flash->sect_tbl[last_b4_curr].next =
                                    Flash->sect_tbl[last_in_lowest].next;

    /*-----------------------------------------------------------------*/
    /* Update last free sector if selected block is last block in list.*/
    /*-----------------------------------------------------------------*/
    if (last_in_lowest == Flash->last_free_sect)
      Flash->last_free_sect = last_b4_curr;
  }

  /*-------------------------------------------------------------------*/
  /* Mark all of its sectors as dirty.                                 */
  /*-------------------------------------------------------------------*/
  for (i = curr_sect; i <= last_in_lowest; ++i)
  {
    --Flash->free_sects;
    PfAssert(Flash->sect_tbl[i].prev == FFREE_SECT);
    Flash->sect_tbl[i].prev = FDRTY_SECT;
  }

  /*-------------------------------------------------------------------*/
  /* Return first sector in selected block.                            */
  /*-------------------------------------------------------------------*/
  return curr_sect;
}

/***********************************************************************/
/* adjust_set_size: Adjust the erase set size based on control size    */
/*                                                                     */
/*      Inputs: adjust_erase = flag to adjust erase set                */
/*              check_recycle = flag to check recycles                 */
/*                                                                     */
/***********************************************************************/
static void adjust_set_size(int adjust_erase, int check_recycle)
{
  int s1, set_increase = FALSE, b, max_f, f_b, max_b = 0;
  ui32 old_size = Flash->set_blocks, s_optimal, s = 0;
  ui32 max_set_blocks = (Flash->max_set_blocks == 1) ? 1 :
                        Flash->max_set_blocks - 1;
  ui32 ctrl_sects, ctrl_inc;

  /*-------------------------------------------------------------------*/
  /* Compute the control sectors with control increment accounted.     */
  /*-------------------------------------------------------------------*/
  ctrl_inc = (Flash->num_sects - Flash->used_sects - Flash->set_blocks *
              Flash->block_sects - Flash->num_blocks * Flash->hdr_sects +
              Flash->block_sects - 1) / Flash->block_sects +
              Flash->used_sects;
  ctrl_sects = (Flash->ctrl_size + 2 * ctrl_inc + Flash->sect_sz - 1) /
               Flash->sect_sz;

  /*-------------------------------------------------------------------*/
  /* Reset the free_sects_used count.                                  */
  /*-------------------------------------------------------------------*/
  Flash->free_sects_used = 0;

  /*-------------------------------------------------------------------*/
  /* Determine the erase set size.                                     */
  /*-------------------------------------------------------------------*/
  for (Flash->set_blocks = 0; Flash->set_blocks < max_set_blocks; ++s)
  {
    ++Flash->set_blocks;

    /*-----------------------------------------------------------------*/
    /* If size goes up, mark new holder with invalid blocks.           */
    /*-----------------------------------------------------------------*/
    if (s >= old_size)
    {
      Flash->erase_set[s] = -1;
      set_increase = TRUE;
    }

    /*-----------------------------------------------------------------*/
    /* Determine if size is big enough.                                */
    /*-----------------------------------------------------------------*/
    if ((ctrl_sects <= Flash->block_sects * Flash->set_blocks /
                       FLASH_SET_LIMIT ||
         Flash->set_blocks >= Flash->num_blocks / 8) &&
        Flash->set_blocks >= Flash->max_set_blocks / 40)
      break;
  }

  /*-------------------------------------------------------------------*/
  /* Return if the set size did not increase.                          */
  /*-------------------------------------------------------------------*/
  if (set_increase == FALSE)
    return;

  /*-------------------------------------------------------------------*/
  /* If volume is full or adjust erase not set, don't increment.       */
  /*-------------------------------------------------------------------*/
  if ((Flash->files_tbl->tbl[1].entry.comm.one_past_last.offset ||
       !adjust_erase))
  {
    Flash->set_blocks = old_size;
    return;
  }

  /*-------------------------------------------------------------------*/
  /* Try to fill -1 slots with block numbers.                          */
  /*-------------------------------------------------------------------*/
  s_optimal = (Flash->set_blocks + 1) * 3 / 4;
  for (s = 0; s < Flash->set_blocks; ++s)
    if (!check_recycle || Flash->erase_set[s] == -1)
    {
      /*---------------------------------------------------------------*/
      /* Choose the best block among the non-free ones.                */
      /*---------------------------------------------------------------*/
      for (b = 0, max_f = -1; b < Flash->num_blocks; ++b)
      {
        /*-------------------------------------------------------------*/
        /* If block is not bad, ctrl and has no free sects, candidate  */
        /*-------------------------------------------------------------*/
        if (!Flash->blocks[b].bad_block &&
            !Flash->blocks[b].ctrl_block &&
            Flash->sect_tbl[(b + 1) * Flash->block_sects - 1].prev !=
            FFREE_SECT)
        {
          /*-----------------------------------------------------------*/
          /* Sanity check.                                             */
          /*-----------------------------------------------------------*/
          PfAssert(Flash->high_wear >= Flash->blocks[b].wear_count);

          /*-----------------------------------------------------------*/
          /* Check that block is not in erase set.                     */
          /*-----------------------------------------------------------*/
          for (s1 = 0; s1 < Flash->set_blocks; ++s1)
            if (Flash->erase_set[s1] == b)
              break;

          if (s1 == Flash->set_blocks)
          {
            /*---------------------------------------------------------*/
            /* Compute the selector.                                   */
            /*---------------------------------------------------------*/
            if (s < s_optimal)
              f_b = (int)(16 * (Flash->block_sects -
                          Flash->blocks[b].used_sects) +
                    (Flash->high_wear - Flash->blocks[b].wear_count));
            else
              f_b = (int)(Flash->high_wear -Flash->blocks[b].wear_count);

            /*---------------------------------------------------------*/
            /* If better candidate, remember it.                       */
            /*---------------------------------------------------------*/
            if (max_f < f_b)
            {
              max_f = f_b;
              max_b = b;
            }
          }
        }
      }

      /*---------------------------------------------------------------*/
      /* If no more replacement blocks, stop.                          */
      /*---------------------------------------------------------------*/
      if (max_f == -1)
        break;

      /*---------------------------------------------------------------*/
      /* Add block to erase set. If recycle not possible afterwards,   */
      /* take block out and stop.                                      */
      /*---------------------------------------------------------------*/
      Flash->erase_set[s] = max_b;
      if (check_recycle && recycle_not_possible())
      {
        Flash->erase_set[s] = -1;
        break;
      }
    }

  /*-------------------------------------------------------------------*/
  /* If there are still -1 entries in erase set, readjust set size.    */
  /*-------------------------------------------------------------------*/
  while (Flash->erase_set[Flash->set_blocks - 1] == -1 &&
         Flash->set_blocks > 1)
    --Flash->set_blocks;
  PfAssert(Flash->set_blocks >= old_size);
}

/***********************************************************************/
/* recycle_finish: Perform end of recycle - update sector table,       */
/*              write ctrl info, update sig bytes and wear count       */
/*                                                                     */
/*      Inputs: check_full = flag to check for volume full             */
/*              write_ctrl = flag to write ctrl                        */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
static int recycle_finish(int check_full, int write_ctrl)
{
  ui32 s, sect, i, old_set_size = Flash->set_blocks;

#if TIMING
clock_t sample = clock();
#endif

  /*-------------------------------------------------------------------*/
  /* Perform a sync now to save everything before erasing block if a   */
  /* sync is required. A sync is required on the last recycle in a     */
  /* contiguous set of recycles.                                       */
  /*-------------------------------------------------------------------*/
  if (write_ctrl && FlashWrCtrl(SKIP_ADJUST_ERASE))
    return -1;
#if TIMING
RecWrCtrl += (clock() - sample);
#endif

  /*-------------------------------------------------------------------*/
  /* Set all sectors in erase set back to free.                        */
  /*-------------------------------------------------------------------*/
  for (s = 0; s < Flash->set_blocks; ++s)
    if (Flash->erase_set[s] != -1)
    {
      PfAssert(!Flash->blocks[Flash->erase_set[s]].ctrl_block);
      sect = Flash->erase_set[s] * Flash->block_sects;
      Flash->sect_tbl[Flash->last_free_sect].next =
                                         (ui16)(sect + Flash->hdr_sects);
      for (i = 0; i < Flash->block_sects; ++i, ++sect)
      {
        if (Flash->sect_tbl[sect].prev == FDRTY_SECT)
        {
          ++Flash->free_sects;
          Flash->sect_tbl[sect].prev = FFREE_SECT;
          Flash->sect_tbl[sect].next = (ui16)(sect + 1);
        }
        else if (Flash->type == FFS_NOR)
        {
          PfAssert(Flash->sect_tbl[i].prev == FHDER_SECT);
        }
        else
        {
          PfAssert(FALSE); /*lint !e644 !e506 !e774 */
        }
      }
      Flash->last_free_sect = sect - 1;
      Flash->sect_tbl[Flash->last_free_sect].next = FLAST_SECT;
    }

  /*-------------------------------------------------------------------*/
  /* Do the last part of recycle: erase block, write signature bytes,  */
  /* and modify wear count, and check for volume full if needed.       */
  /*-------------------------------------------------------------------*/
  if (erase_set((int)old_set_size) || (check_full && !FlashRoom(0)))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Choose the next erase set.                                        */
  /*-------------------------------------------------------------------*/
  FlashChooseEraseSet(USE_WEAR);
  for (;;)
  {
    /*-----------------------------------------------------------------*/
    /* If recycles can proceed, we have a valid erase set.             */
    /*-----------------------------------------------------------------*/
    if (!recycle_not_possible())
      break;

    /*-----------------------------------------------------------------*/
    /* Attempt to select an erase set without looking at wear count.   */
    /*-----------------------------------------------------------------*/
    FlashChooseEraseSet(IGNORE_WEAR);

    /*-----------------------------------------------------------------*/
    /* If recycles can proceed, we have a valid erase set.             */
    /*-----------------------------------------------------------------*/
    if (Flash->set_blocks == 1 || !recycle_not_possible())
      break;

    /*-----------------------------------------------------------------*/
    /* Shrink the erase set by one block.                              */
    /*-----------------------------------------------------------------*/
    Flash->erase_set[--Flash->set_blocks] = -1;
  }

  /*-------------------------------------------------------------------*/
  /* Remember if control info was written.                             */
  /*-------------------------------------------------------------------*/
  Flash->wr_ctrl = write_ctrl;

#if TIMING
++RecCount;
#endif
  return 0;
}

/***********************************************************************/
/* mark_ctrl_info: Clear header bit to mark recent control information */
/*                                                                     */
/*       Input: sector = start of most recent control information      */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
static int mark_ctrl_info(uint sector)
{
  uint byte_off, sect_off;
  ui32 hdr_start, byte_addr;
  FlashGlob *vol;
  int status;

  /*-------------------------------------------------------------------*/
  /* The sector cannot be a header sector.                             */
  /*-------------------------------------------------------------------*/
  PfAssert((sector % Flash->block_sects) >= Flash->hdr_sects);

  /*-------------------------------------------------------------------*/
  /* Get pointer to the beginning of the header.                       */
  /*-------------------------------------------------------------------*/
  sect_off = sector % Flash->block_sects;
  hdr_start = sector - sect_off;

  /*-------------------------------------------------------------------*/
  /* Figure out how far from the beginning of the header we are.       */
  /*-------------------------------------------------------------------*/
  byte_off = sect_off / 8;
  PfAssert(byte_off < Flash->hdr_sects * Flash->sect_sz);

  /*-------------------------------------------------------------------*/
  /* Calculate address of byte to write.                               */
  /*-------------------------------------------------------------------*/
  byte_addr = Flash->mem_base + hdr_start * Flash->sect_sz + byte_off;

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to the flash file system.                */
  /*-------------------------------------------------------------------*/
  vol = Flash;
  semPost(FlashSem);

  /*-------------------------------------------------------------------*/
  /* Mark location of most recent control information.                 */
  /*-------------------------------------------------------------------*/
  status = vol->driver.nor.write_byte(byte_addr,
                                      (ui8)~(1 << (7 - sect_off % 8)),
                                      vol->vol);

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to the flash file system.                */
  /*-------------------------------------------------------------------*/
  semPend(FlashSem, WAIT_FOREVER);
  Flash = vol;

  if (status)
  {
    set_errno(EIO);
    return -1;
  }

  return 0;
}

/***********************************************************************/
/* get_next_used_sector: Look for the next non_dirty_sector            */
/*                                                                     */
/*      Inputs: sector = sector past which to find used sector         */
/*              sp = index in erase_set to current block               */
/*                                                                     */
/*     Returns: Pointer to the next sector, or -1 when done            */
/*                                                                     */
/***********************************************************************/
static uint get_next_used_sector(uint sector, int *sp)
{
  int s = *sp;

  /*-------------------------------------------------------------------*/
  /* Keep looping until either we go past maximum value or we find a   */
  /* used sector.                                                      */
  /*-------------------------------------------------------------------*/
  while (++sector)
  {
    /*-----------------------------------------------------------------*/
    /* If we've reached the end of a block in the erase set, move to   */
    /* next block.                                                     */
    /*-----------------------------------------------------------------*/
    if (sector == (Flash->erase_set[s] + 1) * Flash->block_sects)
    {
      /*---------------------------------------------------------------*/
      /* If no more blocks in erase set, no more used sectors.         */
      /*---------------------------------------------------------------*/
      if (++s >= Flash->set_blocks || Flash->erase_set[s] == -1)
        return (uint)-1;
      sector = Flash->erase_set[s] * Flash->block_sects;
    }

    /*-----------------------------------------------------------------*/
    /* If a used sector is found, stop and return it.                  */
    /*-----------------------------------------------------------------*/
    if (used_sect(sector))
      break;
  }
  *sp = s;
  return sector;
}

/***********************************************************************/
/*    to_offst: Transform a pointer into a ui32 offset so that it can  */
/*              be stored in flash                                     */
/*                                                                     */
/*       Input: entry = pointer to entry in the table                  */
/*                                                                     */
/*     Returns: offset equivalent of pointer                           */
/*                                                                     */
/***********************************************************************/
static ui32 to_offst(const void *entry)
{
  ui16 table_num = 1;
  FFSEnts *tbl;
  union
  {
    OffLoc entry_loc;
    ui32 result;
  } obj;

  /*-------------------------------------------------------------------*/
  /* For NULL pointers, return -1 as offset location.                  */
  /*-------------------------------------------------------------------*/
  if (entry == NULL)
    return (ui32)-1;

  /*-------------------------------------------------------------------*/
  /* Store the special value, REMOVED_LINK, as OFF_REMOVED_LINK.       */
  /*-------------------------------------------------------------------*/
  if (entry == (void *)REMOVED_LINK)
    return (OFF_REMOVED_LINK << 16) | OFF_REMOVED_LINK; /*lint !e648*/

  /*-------------------------------------------------------------------*/
  /* Figure out which table this entry belongs to.                     */
  /*-------------------------------------------------------------------*/
  for (tbl = Flash->files_tbl; tbl; tbl = tbl->next_tbl, ++table_num)
    if ((tbl->tbl + FNUM_ENT > (FFSEnt *)entry) &&
        (tbl->tbl <= (FFSEnt *)entry))
      break;

  /*-------------------------------------------------------------------*/
  /* The entry must belong to some table.                              */
  /*-------------------------------------------------------------------*/
  PfAssert(tbl);

  /*-------------------------------------------------------------------*/
  /* Figure out the offset within the table for this entry.            */
  /*-------------------------------------------------------------------*/
  obj.entry_loc.offset = (ui16)((FFSEnt *)entry - tbl->tbl);

  /*-------------------------------------------------------------------*/
  /* Return ui32 containing table number and entry offset.             */
  /*-------------------------------------------------------------------*/
  obj.entry_loc.sector = table_num;
  return obj.result;
}

/***********************************************************************/
/* count_ctrl_size: Count control info size (bytes and sectors)        */
/*                                                                     */
/*      Inputs: adjust_erase = flag to adjust erase set size           */
/*              check_recycle = flag to check recycles in adjust set   */
/*                                                                     */
/*     Returns: number of ctrl shorts to be checked against actual val */
/*                                                                     */
/***********************************************************************/
static ui32 count_ctrl_size(int adjust_erase, int check_recycle)
{
  ui32 table_size, i, ctrl_num_shorts, ctrl_size, ctrl_sects;
  ui32 free_ctrl_sects, incr;
  ui16 sector, last_in_segment, curr_sect, next_sect;

  /*-------------------------------------------------------------------*/
  /* Figure out how much space is needed to store the sectors table.   */
  /* Start with the free list.                                         */
  /*-------------------------------------------------------------------*/
  ctrl_num_shorts = 2;
  for (sector = Flash->free_sect;; sector = next_sect)
  {
    /*-----------------------------------------------------------------*/
    /* Determine current free block.                                   */
    /*-----------------------------------------------------------------*/
    i = sector / Flash->block_sects;

    /*-----------------------------------------------------------------*/
    /* Determine first sector in next free block.                      */
    /*-----------------------------------------------------------------*/
    next_sect = Flash->sect_tbl[(i + 1) * Flash->block_sects - 1].next;

    /*-----------------------------------------------------------------*/
    /* If no more blocks, stop and account for current segment.        */
    /*-----------------------------------------------------------------*/
    if (next_sect == FLAST_SECT)
    {
      ctrl_num_shorts += 2;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* Skip all bad blocks following current block.                    */
    /*-----------------------------------------------------------------*/
    for (++i; i < Flash->num_blocks && Flash->blocks[i].bad_block; ++i) ;

    /*-----------------------------------------------------------------*/
    /* If end of contiguous segment, account for it.                   */
    /*-----------------------------------------------------------------*/
    if (i != next_sect / Flash->block_sects)
      ctrl_num_shorts += 2;
  }

  /*-------------------------------------------------------------------*/
  /* Now count the space for the used sectors (per file).              */
  /*-------------------------------------------------------------------*/
  for (sector = 0; sector < Flash->num_sects;)
  {
    /*-----------------------------------------------------------------*/
    /* If sector is beginning of file, count whole file sector list.   */
    /*-----------------------------------------------------------------*/
    if (Flash->sect_tbl[sector].prev == FLAST_SECT && used_sect(sector))
    {
      /*---------------------------------------------------------------*/
      /* Count head of list.                                           */
      /*---------------------------------------------------------------*/
      ctrl_num_shorts += 1;

      /*---------------------------------------------------------------*/
      /* Walk the list a contiguous segment at a time.                 */
      /*---------------------------------------------------------------*/
      for (curr_sect = sector; curr_sect != FLAST_SECT;)
      {
        /*-------------------------------------------------------------*/
        /* Count current contiguous segment. For NOR skip headers.     */
        /*-------------------------------------------------------------*/
        if (Flash->type == FFS_NOR)
          for (last_in_segment = curr_sect;; last_in_segment = next_sect)
          {
            next_sect = last_in_segment + 1;
            if (next_sect < Flash->num_sects &&
                Flash->sect_tbl[next_sect].prev == FHDER_SECT)
              next_sect += Flash->hdr_sects;
            if (Flash->sect_tbl[last_in_segment].next != next_sect)
              break;
          }
        else
          for (last_in_segment = curr_sect;
            Flash->sect_tbl[last_in_segment].next == last_in_segment + 1;
            ++last_in_segment) ;

        /*-------------------------------------------------------------*/
        /* If more than 1 sect in segment, count label and last sect.  */
        /*-------------------------------------------------------------*/
        if (curr_sect != last_in_segment)
          ctrl_num_shorts += 2;

        /*-------------------------------------------------------------*/
        /* Count next pointer.                                         */
        /*-------------------------------------------------------------*/
        ctrl_num_shorts += 1;

        /*-------------------------------------------------------------*/
        /* Increment sector counter if first segment in file list.     */
        /*-------------------------------------------------------------*/
        if (curr_sect == sector)
          sector = last_in_segment + 1;

        /*-------------------------------------------------------------*/
        /* Move to new segment.                                        */
        /*-------------------------------------------------------------*/
        curr_sect = Flash->sect_tbl[last_in_segment].next;
      }
    }

    /*-----------------------------------------------------------------*/
    /* Else move to next sector to look at.                            */
    /*-----------------------------------------------------------------*/
    else
      ++sector;
  }

  /*-------------------------------------------------------------------*/
  /* Account for the last FLAST_SECT marking for end of table.         */
  /*-------------------------------------------------------------------*/
  ctrl_num_shorts += 1;

  /*-------------------------------------------------------------------*/
  /* Add space for the files tables.                                   */
  /*-------------------------------------------------------------------*/
  table_size = Flash->tbls_size + Flash->total * sizeof(ui8);

  /*-------------------------------------------------------------------*/
  /* The ctrl size consists of the size of the files tables + the fixed*/
  /* number of ui32s + size of sectors table (in shorts) + wear count  */
  /* offsets for every block (2 offsets -> byte) + CRC and bad blocks  */
  /* (NAND only) or fast mount byte (NOR only) or CRC(MLC only).       */
  /*-------------------------------------------------------------------*/
  ctrl_num_shorts += 10;
  ctrl_size = table_size + FLASH_CTRL_UI32s * sizeof(ui32) +
              sizeof(ui16) * ctrl_num_shorts + (Flash->num_blocks + 1) /
              2 * sizeof(ui8);
  if (Flash->type == FFS_NAND)
    ctrl_size += (Flash->num_blocks + 7) / 8 + 4;
  else
    ctrl_size += 1;

#if QUOTA_ENABLED
  /*-------------------------------------------------------------------*/
  /* Count the quota enabled flag.                                     */
  /*-------------------------------------------------------------------*/
  ctrl_size += 1;
#endif /* QUOTA_ENABLED */

  /*-------------------------------------------------------------------*/
  /* Add the size taken by writing the ctrl blocks.                    */
  /*-------------------------------------------------------------------*/
  ctrl_size += sizeof(ui16);
  ctrl_sects = (ctrl_size + Flash->sect_sz - 1) / Flash->sect_sz;
  free_ctrl_sects = (ui32)((Flash->free_ctrl_sect == (ui32)-1) ? 0 :
                           (Flash->block_sects -
                            Flash->free_ctrl_sect % Flash->block_sects));
  if (ctrl_sects > free_ctrl_sects)
  {
    incr = (ctrl_sects - free_ctrl_sects + Flash->block_sects - 1)
           / Flash->block_sects + 1;
    ctrl_size += 8 * incr;
    ctrl_num_shorts += 4 * incr;
  }

  /*-------------------------------------------------------------------*/
  /* Save update control information byte size and sector count.       */
  /*-------------------------------------------------------------------*/
  Flash->ctrl_size = ctrl_size;
  Flash->ctrl_sects = (Flash->ctrl_size + Flash->sect_sz - 1) /
                       Flash->sect_sz;

  /*-------------------------------------------------------------------*/
  /* Readjust the erase set size based on new ctrl info size.          */
  /*-------------------------------------------------------------------*/
  adjust_set_size(adjust_erase, check_recycle);

  /*-------------------------------------------------------------------*/
  /* Return the number of ui16's it takes to write sect_tbl.           */
  /*-------------------------------------------------------------------*/
  return ctrl_num_shorts;
}

/***********************************************************************/
/*  free_block: Attempt to free block and add it to free list          */
/*                                                                     */
/*      Inputs: block = block to be erased                             */
/*              last_dirty = last dirty sector in corrupt free list    */
/*              adjust_set = flag to indicate if erase set has changed */
/*              modified = flag to sync if state changes               */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
static int free_block(ui32 block, ui32 last_dirty, int *adjust_set,
                      int *modified)
{
  ui32 i, j, last_sect = (block + 1) * Flash->block_sects - 1;
  int do_erase, s, add_to_free_list = TRUE;

  /*-------------------------------------------------------------------*/
  /* Erase only good, non used, non control blocks.                    */
  /*-------------------------------------------------------------------*/
  if (!Flash->blocks[block].bad_block && !Flash->blocks[block].ctrl_block
      && Flash->blocks[block].used_sects == 0)
  {
    /*-----------------------------------------------------------------*/
    /* If last sector in block is free, only consider if it's beginning*/
    /* of free list and it has dirty sectors on it.                    */
    /*-----------------------------------------------------------------*/
    if (Flash->sect_tbl[last_sect].prev == FFREE_SECT)
    {
      /*---------------------------------------------------------------*/
      /* If first sector in block is free, skip block.                 */
      /*---------------------------------------------------------------*/
      if (Flash->sect_tbl[block * Flash->block_sects +
                          Flash->hdr_sects].prev == FFREE_SECT)
        return 0;

      /*---------------------------------------------------------------*/
      /* This block must be the beginning of the free list.            */
      /*---------------------------------------------------------------*/
      PfAssert(Flash->free_sect / Flash->block_sects == block);
      Flash->free_sect = block * Flash->block_sects + Flash->hdr_sects;
      add_to_free_list = FALSE;
    }

    /*-----------------------------------------------------------------*/
    /* First, free up all dirty sectors in block.                      */
    /*-----------------------------------------------------------------*/
    i = block * Flash->block_sects + Flash->hdr_sects;
    for (j = 0, do_erase = FALSE; j < Flash->block_sects; ++j)
      if (Flash->sect_tbl[i + j].prev == FDRTY_SECT)
      {
        ++Flash->free_sects;
        Flash->sect_tbl[i + j].prev = FFREE_SECT;
        Flash->sect_tbl[i + j].next = (ui16)(i + j + 1);
        do_erase = TRUE;
      }

    /*-----------------------------------------------------------------*/
    /* Erase block.                                                    */
    /*-----------------------------------------------------------------*/
    if (do_erase)
    {
      /*---------------------------------------------------------------*/
      /* If blocks from erase set are erased, readjust erase set.      */
      /*---------------------------------------------------------------*/
      for (s = 0; s < Flash->set_blocks; ++s)
        if (Flash->erase_set[s] == block)
          break;
      if (s < Flash->set_blocks)
      {
        Flash->erase_set[s] = -1;
        *adjust_set = TRUE;
      }

      *modified = TRUE;

      if (FlashEraseBlock(block))
        return -1;

      /*---------------------------------------------------------------*/
      /* If block is not bad, tie it to free list.                     */
      /*---------------------------------------------------------------*/
      if (!Flash->blocks[block].bad_block && add_to_free_list)
      {
        /*-------------------------------------------------------------*/
        /* Free sector needs to be updated if invalid.                 */
        /*-------------------------------------------------------------*/
        if (Flash->free_sect == (ui32)-1)
        {
          Flash->free_sect = i;
          Flash->last_free_sect = i + Flash->block_sects -
                                  Flash->hdr_sects - 1;
          Flash->sect_tbl[Flash->last_free_sect].next = FLAST_SECT;

          /*-----------------------------------------------------------*/
          /* Tie last dirty with new free so resume has contiguous list*/
          /*-----------------------------------------------------------*/
          if (last_dirty != (ui32)-1)
            Flash->sect_tbl[last_dirty].next = (ui16)Flash->free_sect;
        }

        /*-------------------------------------------------------------*/
        /* Add block to end of free list.                              */
        /*-------------------------------------------------------------*/
        else
        {
          PfAssert(Flash->free_sect / Flash->block_sects != block);
          PfAssert(Flash->sect_tbl[Flash->free_sect].prev == FFREE_SECT
                && Flash->sect_tbl[Flash->last_free_sect].prev ==
                   FFREE_SECT);
          Flash->sect_tbl[Flash->last_free_sect].next = (ui16)i;
          Flash->last_free_sect = i + Flash->block_sects -
                                  Flash->hdr_sects - 1;
          Flash->sect_tbl[Flash->last_free_sect].next = FLAST_SECT;
        }
      }
    }
  }
  return 0;
}

/***********************************************************************/
/* check_flash: Ensure that the 'free' sectors are actually free, the  */
/*              'dirty' are dirty, count the number of dirty sectors   */
/*              and set free_sect                                      */
/*                                                                     */
/*      Output: modified = flag to due a sync if stat changes          */
/*       Input: fast_mount_check = check NOR/MLC for fast mount        */
/*                                                                     */
/*     Returns: FFS_NORMAL_POWER_UP on a normal shut down,             */
/*              FFS_SLOW_POWER_UP on an abnormal shut down,            */
/*              FFS_FAILED_POWER_UP on error                           */
/*                                                                     */
/***********************************************************************/
static int check_flash(int *modified, int fast_mount_check)
{
  ui32 i, j, sect, min_free_block, block;
  ui32 free_sects = Flash->free_sects, last_dirty = (ui32)-1;
  int adjust_set = FALSE;

  PfAssert(Flash->sect_tbl[Flash->free_sect].prev == FFREE_SECT &&
           Flash->last_free_sect % Flash->block_sects ==
           Flash->block_sects - 1);

  /*-------------------------------------------------------------------*/
  /* For NAND/MLC assume it's fast mount and adjust later.             */
  /*-------------------------------------------------------------------*/
  Flash->fast_mount = (Flash->type != FFS_NOR);

  /*-------------------------------------------------------------------*/
  /* Count the used sectors.                                           */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < Flash->num_sects; ++i)
    if (used_sect(i))
    {
      ++Flash->used_sects;
      ++Flash->blocks[i / Flash->block_sects].used_sects;
    }

  /*-------------------------------------------------------------------*/
  /* Take the ctrl sectors out of the used sectors block counts.       */
  /*-------------------------------------------------------------------*/
  for (i = Flash->frst_ctrl_sect; i != FLAST_SECT;
       i = Flash->sect_tbl[i].next)
  {
    --Flash->blocks[i / Flash->block_sects].used_sects;
    --Flash->used_sects;
  }

  /*-------------------------------------------------------------------*/
  /* Read the fast mount flag for NOR.                                 */
  /*-------------------------------------------------------------------*/
  if (Flash->type == FFS_NOR)
  {
    /*-----------------------------------------------------------------*/
    /* Check the fast mount flag.                                      */
    /*-----------------------------------------------------------------*/
    if (fast_mount_check)
    {
      if (Flash->read_sector(Flash->tmp_sect, Flash->last_ctrl_sect))
      {
        set_errno(EIO);
        return FFS_FAILED_POWER_UP;
      }
      Flash->fast_mount = (Flash->tmp_sect[Flash->sect_sz - 1] ==
                           FAST_MOUNT);
    }

    /*-----------------------------------------------------------------*/
    /* Rebuild the erase set at the time of power off.                 */
    /*-----------------------------------------------------------------*/
    FlashCountAndChoose(SKIP_CHECK_RECYCLE);
  }

  /*-------------------------------------------------------------------*/
  /* Check for fast mount.                                             */
  /*-------------------------------------------------------------------*/
  if (Flash->type != FFS_NOR || !Flash->fast_mount)
  {
    /*-----------------------------------------------------------------*/
    /* A fast mount happens when all the sectors past frst_free_ctrl   */
    /* sector in same block are free, the first FOPEN_MAX sectors on   */
    /* free list are free(cache size) plus the next block worth of     */
    /* sectors is free (it could have been selected for control info   */
    /* and we got cut off).                                            */
    /*-----------------------------------------------------------------*/
    if (Flash->free_ctrl_sect != (ui32)-1)
      for (i = Flash->free_ctrl_sect; i % Flash->block_sects; ++i)
      {
        if (!Flash->empty_sect(i))
        {
          Flash->fast_mount = FALSE;
          Flash->free_ctrl_sect = (ui32)-1;
        }
        else if (Flash->free_ctrl_sect == (ui32)-1)
          Flash->free_ctrl_sect = i;
      }

    /*-----------------------------------------------------------------*/
    /* Now check all the free list, skipping sectors if fast mount.    */
    /*-----------------------------------------------------------------*/
    Flash->free_sects = 0;
    for (i = Flash->free_sect; i != FLAST_SECT;)
    {
      PfAssert(i < Flash->num_sects &&
               Flash->sect_tbl[i].prev == FFREE_SECT);

      /*---------------------------------------------------------------*/
      /* If known a priori or verified, count sector as free. First    */
      /* cache size worth of sectors plus next block worth of sectors  */
      /* are always checked.                                           */
      /*---------------------------------------------------------------*/
      if ((Flash->fast_mount &&
           Flash->free_sects > FOPEN_MAX + Flash->block_sects + 1) ||
          Flash->empty_sect(i))
      {
        ++Flash->free_sects;
        Flash->last_free_sect = i;
      }

      /*---------------------------------------------------------------*/
      /* Else mark sector as dirty.                                    */
      /*---------------------------------------------------------------*/
      else
      {
        Flash->fast_mount = FALSE;
        Flash->sect_tbl[i].prev = FDRTY_SECT;
        last_dirty = i;
      }

      /*---------------------------------------------------------------*/
      /* Move to next free sector.                                     */
      /*---------------------------------------------------------------*/
      i = Flash->sect_tbl[i].next;
    }
  }

  /*-------------------------------------------------------------------*/
  /* For fast mount NOR, set last free sector pointer.                 */
  /*-------------------------------------------------------------------*/
  else
    for (i = Flash->free_sect, Flash->free_sects = 0;;)
    {
      ++Flash->free_sects;
      if (Flash->sect_tbl[i].next == FLAST_SECT)
      {
        Flash->last_free_sect = i;
        break;
      }
      else
        i = Flash->sect_tbl[i].next;
    }

  /*-------------------------------------------------------------------*/
  /* If there were dirty sectors, invalidate all free sectors that     */
  /* preceeded the last dirty sector in list.                          */
  /*-------------------------------------------------------------------*/
  if (last_dirty != (ui32)-1)
  {
    for (i = Flash->free_sect; i != last_dirty;
         i = Flash->sect_tbl[i].next)
      if (Flash->sect_tbl[i].prev == FFREE_SECT)
      {
        --Flash->free_sects;
        Flash->sect_tbl[i].prev = FDRTY_SECT;
      }

    /*-----------------------------------------------------------------*/
    /* Also, set the free sector to be one past the last dirty sector. */
    /*-----------------------------------------------------------------*/
    if (Flash->sect_tbl[last_dirty].next == FLAST_SECT)
      Flash->free_sect = Flash->last_free_sect = (ui32)-1;
    else
      Flash->free_sect = Flash->sect_tbl[last_dirty].next;
  }

  /*-------------------------------------------------------------------*/
  /* Quick check passes if we have at least as many free sectors as    */
  /* the control info indicates. Return success in this case.          */
  /*-------------------------------------------------------------------*/
  if (Flash->free_sects >= free_sects)
  {
    /*-----------------------------------------------------------------*/
    /* Count control info and chose erase set based on that. If there  */
    /* are not enough free sectors to perform recycle, loop through    */
    /* all blocks erasing enough so recycles are possible.             */
    /*-----------------------------------------------------------------*/
    count_ctrl_size(ADJUST_ERASE, SKIP_CHECK_RECYCLE);
    FlashChooseEraseSet(USE_WEAR);
    for (block = 0; recycle_not_possible() && block < Flash->num_blocks;)
      if (free_block(block++, last_dirty, &adjust_set, modified))
        return FFS_FAILED_POWER_UP;

    /*-----------------------------------------------------------------*/
    /* Mark end of free list.                                          */
    /*-----------------------------------------------------------------*/
    Flash->sect_tbl[Flash->last_free_sect].next = FLAST_SECT;

    /*-----------------------------------------------------------------*/
    /* Readjust erase set if needed.                                   */
    /*-----------------------------------------------------------------*/
    if (adjust_set)
      readjust_set();

    /*-----------------------------------------------------------------*/
    /* Check that there are no links that need to be deleted.          */
    /*-----------------------------------------------------------------*/
    FlashCheckDelLinks();

#if FFS_DEBUG
{
  ui32 sct, fr_scts = 0;

  for (sct = Flash->free_sect; sct != FLAST_SECT;
       sct = Flash->sect_tbl[sct].next)
  {
    if (Flash->sect_tbl[sct].prev != FFREE_SECT ||
        !Flash->empty_sect(sct))
      printf("check_flash: SECT %u CORRUPT FREE SECT!!!!!\n", sct);
    else
      ++fr_scts;
  }
  printf("check_flash: 1st free = %lu; last free = %lu\n",
         Flash->free_sect, Flash->last_free_sect);
  printf("             actual free = %lu vs. thought free = %lu\n",
         fr_scts, Flash->free_sects);
  if (Flash->fast_mount)
    printf("    FAST_MOUNT!!!!!!!!\n");
  else
    printf("    SEMI FAST_MOUNT!!!!!!!!!!\n");
}
#endif

    return FFS_NORMAL_POWER_UP;
  }

  /*-------------------------------------------------------------------*/
  /* Walk through all blocks, checking if we can erase any.            */
  /*-------------------------------------------------------------------*/
  for (block = 0; block < Flash->num_blocks; ++block)
    if (free_block(block, last_dirty, &adjust_set, modified))
      return FFS_FAILED_POWER_UP;

  /*-------------------------------------------------------------------*/
  /* At this time, free sector should not be invalid.                  */
  /*-------------------------------------------------------------------*/
  PfAssert(Flash->free_sect != (ui32)-1);

  /*-------------------------------------------------------------------*/
  /* The min_free_block is the block that contains the beginning of the*/
  /* free list.                                                        */
  /*-------------------------------------------------------------------*/
  min_free_block = Flash->free_sect / Flash->block_sects;
  Flash->last_free_sect = (min_free_block+1) * Flash->block_sects- 1;
  Flash->sect_tbl[Flash->last_free_sect].next = FLAST_SECT;
  PfAssert(Flash->sect_tbl[Flash->last_free_sect].prev == FFREE_SECT);

  /*-------------------------------------------------------------------*/
  /* Link all the other blocks that contain free to list.              */
  /*-------------------------------------------------------------------*/
  for (block = 0; block < Flash->num_blocks; ++block)
  {
    if (block != min_free_block &&
        Flash->sect_tbl[(block + 1) * Flash->block_sects - 1].prev ==
        FFREE_SECT)
    {
      /*---------------------------------------------------------------*/
      /* Since it's not the minimum, this block should have its first  */
      /* non-header sector as free.                                    */
      /*---------------------------------------------------------------*/
      sect = block * Flash->block_sects + Flash->hdr_sects;
      PfAssert(Flash->sect_tbl[sect].prev == FFREE_SECT);

      /*---------------------------------------------------------------*/
      /* Link it in.                                                   */
      /*---------------------------------------------------------------*/
      Flash->sect_tbl[Flash->last_free_sect].next = (ui16)sect;
      Flash->last_free_sect = (block + 1) * Flash->block_sects - 1;
      Flash->sect_tbl[Flash->last_free_sect].next = FLAST_SECT;
      PfAssert(Flash->sect_tbl[Flash->last_free_sect].prev== FFREE_SECT);
    }
  }

  /*-------------------------------------------------------------------*/
  /* Check that there are no links that need to be deleted.            */
  /*-------------------------------------------------------------------*/
  FlashCheckDelLinks();

  /*-------------------------------------------------------------------*/
  /* Make sure that in no block are there free sectors before used or  */
  /* dirty ones as this should never happen. Also, check that number   */
  /* of free sectors is valid.                                         */
  /*-------------------------------------------------------------------*/
  Flash->free_sects = 0;
  for (i = 0; i < Flash->num_sects; i += Flash->block_sects)
  {
    /*-----------------------------------------------------------------*/
    /* Find the last non free sector in block.                         */
    /*-----------------------------------------------------------------*/
    for (j = Flash->block_sects; j > 0 ; --j)
      if (Flash->sect_tbl[i + j - 1].prev != FFREE_SECT &&
          Flash->sect_tbl[i + j - 1].prev != FHDER_SECT)
        break;
      else if (Flash->sect_tbl[i + j - 1].prev == FFREE_SECT)
        ++Flash->free_sects;

    /*-----------------------------------------------------------------*/
    /* If there is a non free sector in block, make sure all preceding */
    /* sectors in block are non free.                                  */
    /*-----------------------------------------------------------------*/
    if (j)
      for (sect = i + j - 1, j = i; j < sect; ++j)
        PfAssert(Flash->sect_tbl[j].prev != FFREE_SECT);
  }

  /*-------------------------------------------------------------------*/
  /* For NAND/MLC count control info and chose erase set based on that.*/
  /*-------------------------------------------------------------------*/
  if (Flash->type != FFS_NOR)
    FlashCountAndChoose(CHECK_RECYCLE);

#if INC_NOR_FS
  /*-------------------------------------------------------------------*/
  /* For NOR, adjust erase set only if blocks from it were erased.     */
  /*-------------------------------------------------------------------*/
  else if (adjust_set)
    readjust_set();
#endif

#if FFS_DEBUG
{
  ui32 sct, fr_scts = 0;

  for (sct = Flash->free_sect; sct != FLAST_SECT;
       sct = Flash->sect_tbl[sct].next)
  {
    if (Flash->sect_tbl[sct].prev != FFREE_SECT)
      printf("check_flash: CORRUPT FREE LIST!!!!!\n");
    else
      ++fr_scts;
  }
  printf("check_flash: SLOW_MOUNT - 1st free = %lu; last free = %lu\n",
         Flash->free_sect, Flash->last_free_sect);
  printf("             actual free = %lu vs. thought free = %lu\n",
         fr_scts, Flash->free_sects);
}
#endif
  return FFS_SLOW_POWER_UP;
}

#if INC_NOR_FS
/***********************************************************************/
/*      resume: Perform a start up (power up) recycle, checking and,   */
/*              if necessary, recovering from a previous unfinished    */
/*              recycle that might have been interrupted               */
/*                                                                     */
/*       Input: look_back_first = ptr where to start the look back     */
/*                                process for interrupted data         */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
static int resume(ui32 look_back_first)
{
  ui32 look_back_last, look_back_last_in_block, sector, look_back_sect;
  int *tmp_set, r_val = -1, best_matches, b, best_b = -1, s;
  ui32 *sector_data, tmp_s = 0, last_used_written = (ui32)-1;
  ui32 *tmp_sect = (ui32 *)Flash->tmp_sect, start_s, t, i, next_sector;
  ui32 used_sector = (ui32)-1, sect_sz = Flash->sect_sz / sizeof(ui32);

  /*-------------------------------------------------------------------*/
  /* Allocate the necessary memory for temp set and temp sector data.  */
  /*-------------------------------------------------------------------*/
  tmp_set = malloc(sizeof(int) * Flash->set_blocks);
  if (tmp_set == NULL)
    return -1;
  sector_data = malloc(Flash->sect_sz);
  if (sector_data == NULL)
  {
    free(tmp_set);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Get last sector in look back block.                               */
  /*-------------------------------------------------------------------*/
  look_back_last_in_block = (look_back_first / Flash->block_sects + 1) *
                            Flash->block_sects - 1;

  /*-------------------------------------------------------------------*/
  /* Find last sector that is not empty in the look back block.        */
  /*-------------------------------------------------------------------*/
  look_back_last = look_back_first;
  for (sector = look_back_first; sector <= look_back_last_in_block;
       ++sector)
    if (!Flash->empty_sect(sector))
      look_back_last = sector;

  /*-------------------------------------------------------------------*/
  /* Scan from first to last and match as many as possible with used   */
  /* sectors in other blocks. Those blocks will be part of the temp    */
  /* erase set.                                                        */
  /*-------------------------------------------------------------------*/
  for (sector = look_back_first; sector <= look_back_last;)
  {
    best_matches = 0;

    /*-----------------------------------------------------------------*/
    /* Try to match current sector with first used in a block, skipping*/
    /* blocks with free sectors, control blocks or blocks in temp      */
    /* erase set.                                                      */
    /*-----------------------------------------------------------------*/
    for (b = 0; b < Flash->num_blocks; ++b)
      if (Flash->sect_tbl[(b + 1) * Flash->block_sects - 1].prev !=
          FFREE_SECT && !Flash->blocks[b].ctrl_block)
      {
        for (s = 0; s < tmp_s; ++s)
          if (tmp_set[s] == b)
            break;

        /*-------------------------------------------------------------*/
        /* If block is not free, not in temp set, and not control find */
        /* out how many sectors match.                                 */
        /*-------------------------------------------------------------*/
        if (s == tmp_s)
        {
          int matches = 0, j;

          /*-----------------------------------------------------------*/
          /* Read current look back sector.                            */
          /*-----------------------------------------------------------*/
          look_back_sect = sector;
          if (Flash->read_sector(sector_data, look_back_sect))
            goto resume_exit;

          /*-----------------------------------------------------------*/
          /* Count the matches with sectors in current block.          */
          /*-----------------------------------------------------------*/
          used_sector = b * Flash->block_sects;
          for (i = 0; i < Flash->block_sects;
               ++i, ++used_sector)
            if (used_sect(used_sector))
            {
              /*-------------------------------------------------------*/
              /* Read in current used sector.                          */
              /*-------------------------------------------------------*/
              if (Flash->read_sector(Flash->tmp_sect, used_sector))
                goto resume_exit;

              /*-------------------------------------------------------*/
              /* Check for matches.                                    */
              /*-------------------------------------------------------*/
              for (j = 0; j < sect_sz; ++j)
                if (tmp_sect[j] & ~sector_data[j])
                  break;

              /*-------------------------------------------------------*/
              /* If sector matched count it, else stop.                */
              /*-------------------------------------------------------*/
              if (j == sect_sz)
              {
                ++matches;

                /*-----------------------------------------------------*/
                /* Move to next look back sector, if any left.         */
                /*-----------------------------------------------------*/
                if (++look_back_sect > look_back_last)
                  break;
                if (Flash->read_sector(sector_data, look_back_sect))
                  goto resume_exit;
              }
              else
              {
                matches = 0;
                break;
              }
            }

          /*-----------------------------------------------------------*/
          /* If better match than what we saw so far, remember it.     */
          /*-----------------------------------------------------------*/
          if (matches > best_matches)
          {
            best_matches = matches;
            best_b = b;
          }
        }
      }

    /*-----------------------------------------------------------------*/
    /* If block found that could be transferred, add it to temp erase  */
    /* set and do the transfer.                                        */
    /*-----------------------------------------------------------------*/
    if (best_matches)
    {
      tmp_set[tmp_s++] = best_b;

      /*---------------------------------------------------------------*/
      /* Transfer used sectors up to best matches.                     */
      /*---------------------------------------------------------------*/
      used_sector = best_b * Flash->block_sects;
      for (i = 0; i < best_matches; ++used_sector)
        if (used_sect(used_sector))
        {
          if (FlashRelocSect(used_sector, sector, NULL))
            goto resume_exit;
          last_used_written = used_sector;
          ++sector;
          ++i;
        }

      /*---------------------------------------------------------------*/
      /* If no more look back sectors, stop.                           */
      /*---------------------------------------------------------------*/
      if (sector > look_back_last || tmp_s == Flash->set_blocks)
        break;
    }

    /*-----------------------------------------------------------------*/
    /* Else, no matches. Move to next sector in the back.              */
    /*-----------------------------------------------------------------*/
    else
    {
      used_sector = (ui32)-1;
      ++sector;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Fill the rest of temp set with blocks from erase set.             */
  /*-------------------------------------------------------------------*/
  start_s = tmp_s;
  for (; tmp_s < Flash->set_blocks; ++tmp_s)
  {
    /*-----------------------------------------------------------------*/
    /* Choose a block from erase set that is not in temp set yet.      */
    /*-----------------------------------------------------------------*/
    for (s = 0; s < Flash->set_blocks; ++s)
    {
      for (t = 0; t < tmp_s; ++t)
        if (tmp_set[t] == Flash->erase_set[s])
          break;
      if (t == tmp_s)
        break;
    }

    /*-----------------------------------------------------------------*/
    /* There must be at least one block in erase set available.        */
    /*-----------------------------------------------------------------*/
    PfAssert(s < Flash->set_blocks);
    tmp_set[tmp_s] = Flash->erase_set[s];
  }

  /*-------------------------------------------------------------------*/
  /* Transfer the temp set to the erase set.                           */
  /*-------------------------------------------------------------------*/
  memcpy(Flash->erase_set, tmp_set, sizeof(int) * Flash->set_blocks);

  /*-------------------------------------------------------------------*/
  /* If no sector from which to continue transferring, start transfer  */
  /* from the beginning (start_s).                                     */
  /*-------------------------------------------------------------------*/
  if (last_used_written == (ui32)-1)
  {
    PfAssert(Flash->erase_set[0] != -1);
    s = 0;
    used_sector = get_next_used_sector(Flash->erase_set[s] *
                                       Flash->block_sects, &s);
  }

  /*-------------------------------------------------------------------*/
  /* Else it's the next sector past the last one transferred.          */
  /*-------------------------------------------------------------------*/
  else
  {
    s = (int)(start_s - 1);
    used_sector = get_next_used_sector(last_used_written, &s);
  }

  /*-------------------------------------------------------------------*/
  /* Keep transferring sectors as in a normal recycle set.             */
  /*-------------------------------------------------------------------*/
  while (used_sector != (ui32)-1)
  {
    /*-----------------------------------------------------------------*/
    /* Original sector cannot be dirty.                                */
    /*-----------------------------------------------------------------*/
    PfAssert(Flash->sect_tbl[used_sector].prev != FDRTY_SECT);

    /*-----------------------------------------------------------------*/
    /* If we have reached the free sectors switch to using them.       */
    /*-----------------------------------------------------------------*/
    if (sector == Flash->free_sect)
      sector = look_back_last_in_block + 1;

    /*-----------------------------------------------------------------*/
    /* Either we use sectors from back that are empty, or free sectors.*/
    /*-----------------------------------------------------------------*/
    if (sector > look_back_last && sector <= look_back_last_in_block)
    {
      PfAssert(Flash->sect_tbl[sector].prev == FDRTY_SECT);
      if (FlashRelocSect(used_sector, sector, NULL))
        goto resume_exit;
      ++sector;
    }
    else if (FlashRelocSect(used_sector, Flash->free_sect, NULL))
      goto resume_exit;

    /*-----------------------------------------------------------------*/
    /* Advance to next used sector in erase set.                       */
    /*-----------------------------------------------------------------*/
    used_sector = get_next_used_sector(used_sector, &s);
  }

  /*-------------------------------------------------------------------*/
  /* If no more sectors past last control sector in same block,        */
  /* perform end of a normal recycle.                                  */
  /*-------------------------------------------------------------------*/
  if ((Flash->last_ctrl_sect + 1) % Flash->block_sects == 0)
  {
    if (recycle_finish(SKIP_CHECK_FULL, TRUE))
      goto resume_exit;
  }

  /*-------------------------------------------------------------------*/
  /* Analyze the sectors past last contorl sector to see if we can     */
  /* continue with the write control.                                  */
  /*-------------------------------------------------------------------*/
  else
  {
    /*-----------------------------------------------------------------*/
    /* Pretend all sectors past last control sector in block are empty.*/
    /*-----------------------------------------------------------------*/
    ui32 old_set_size, new_free_ctrl_sect = Flash->free_ctrl_sect;
    Flash->free_ctrl_sect = Flash->last_ctrl_sect + 1;

    /*-----------------------------------------------------------------*/
    /* Switch to special write control function for resume.            */
    /*-----------------------------------------------------------------*/
    Flash->write_ctrl = FsNorCheckCtrl;

    /*-----------------------------------------------------------------*/
    /* If the control cannot be written over, perform a normal end of  */
    /* recycle using the standard write control function.              */
    /*-----------------------------------------------------------------*/
    old_set_size = Flash->set_blocks;
    if (FlashWrCtrl(SKIP_ADJUST_ERASE))
    {
      /*---------------------------------------------------------------*/
      /* First, set write control function back to standard one.       */
      /*---------------------------------------------------------------*/
      Flash->write_ctrl = FsNorWriteCtrl;

      /*---------------------------------------------------------------*/
      /* If write failed, it was because of mismatch with sector in    */
      /* block containing last control sector of valid control info.   */
      /* Mark back to dirty all sectors from that block, and to free   */
      /* all other sectors that might have been reserved by the failed */
      /* write above.                                                  */
      /*---------------------------------------------------------------*/
      for (sector = Flash->frst_ctrl_sect;
           sector != FLAST_SECT && used_sect(sector);
           sector = next_sector)
      {
        next_sector = Flash->sect_tbl[sector].next;
        Flash->blocks[sector / Flash->block_sects].ctrl_block = FALSE;

        /*-------------------------------------------------------------*/
        /* If sector in first block, it's dirty, else free.            */
        /*-------------------------------------------------------------*/
        if (sector / Flash->block_sects ==
            Flash->frst_ctrl_sect / Flash->block_sects)
          Flash->sect_tbl[sector].prev = FDRTY_SECT;
        else
        {
          Flash->sect_tbl[sector].prev = FFREE_SECT;
          Flash->sect_tbl[sector].next = FLAST_SECT;
          Flash->sect_tbl[Flash->last_free_sect].next = (ui16)sector;
          Flash->last_free_sect = sector;
          ++Flash->free_sects;

          /*-----------------------------------------------------------*/
          /* If last control sector not last in block, free rest.      */
          /*-----------------------------------------------------------*/
          if (next_sector == FLAST_SECT)
          {
            while (++sector % Flash->block_sects)
            {
              Flash->sect_tbl[sector].prev = FFREE_SECT;
              Flash->sect_tbl[sector].next = FLAST_SECT;
              Flash->sect_tbl[Flash->last_free_sect].next = (ui16)sector;
              Flash->last_free_sect = sector;
              ++Flash->free_sects;
            }
          }
        }
      }

      /*---------------------------------------------------------------*/
      /* Invalidate the incomplete control info so it does not affect  */
      /* the new write control.                                        */
      /*---------------------------------------------------------------*/
      Flash->frst_ctrl_sect = (ui32)-1;

      /*---------------------------------------------------------------*/
      /* Before finishing recycle, reset free control sector to valid  */
      /* value.                                                        */
      /*---------------------------------------------------------------*/
      Flash->free_ctrl_sect = new_free_ctrl_sect;
      if (Flash->free_ctrl_sect != (ui32)-1)
        Flash->blocks[Flash->free_ctrl_sect /
                      Flash->block_sects].ctrl_block = TRUE;
      PfAssert(Flash->sect_tbl[Flash->free_ctrl_sect].prev !=FFREE_SECT);
      if (recycle_finish(SKIP_CHECK_FULL, TRUE))
        goto resume_exit;
    }

    /*-----------------------------------------------------------------*/
    /* Else the write went through. Finish up the end of the recycle.  */
    /*-----------------------------------------------------------------*/
    else
    {
      /*---------------------------------------------------------------*/
      /* First, set write control function back to standard one.       */
      /*---------------------------------------------------------------*/
      Flash->write_ctrl = FsNorWriteCtrl;

      /*---------------------------------------------------------------*/
      /* Set all sectors in erase set back to free.                    */
      /*---------------------------------------------------------------*/
      for (s = 0; s < Flash->set_blocks; ++s)
        if (Flash->erase_set[s] != -1)
        {
          sector = Flash->erase_set[s] * Flash->block_sects;
          Flash->sect_tbl[Flash->last_free_sect].next =
                                       (ui16)(sector + Flash->hdr_sects);
          for (i = 0; i < Flash->block_sects; ++i, ++sector)
          {
            if (Flash->sect_tbl[sector].prev == FDRTY_SECT)
            {
              ++Flash->free_sects;
              Flash->sect_tbl[sector].prev = FFREE_SECT;
              Flash->sect_tbl[sector].next = (ui16)(sector + 1);
            }
            else
            {
              PfAssert(Flash->sect_tbl[sector].prev == FHDER_SECT);
            }
          }
          Flash->last_free_sect = sector - 1;
          Flash->sect_tbl[Flash->last_free_sect].next = FLAST_SECT;
        }

      /*---------------------------------------------------------------*/
      /* Do last part of recycle: erase block, write signature bytes,  */
      /* and modify wear count, and check for volume full if needed.   */
      /*---------------------------------------------------------------*/
      if (erase_set((int)old_set_size))
        goto resume_exit;

      /*---------------------------------------------------------------*/
      /* Choose the next erase set.                                    */
      /*---------------------------------------------------------------*/
      FlashChooseEraseSet(USE_WEAR);
      if (recycle_not_possible())
        FlashChooseEraseSet(IGNORE_WEAR);
    }
  }
  r_val = 0;
resume_exit:
  free(tmp_set);
  free(sector_data);
  return r_val;
}
#endif

/***********************************************************************/
/* recycle_set: Perform recycle on the erasable set                    */
/*                                                                     */
/*       Input: check_full = flag to check for volume full             */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
static int recycle_set(int check_full)
{
  ui32 i, sect = (ui32)-1;
  int write_ctrl = FALSE, s;

  /*-------------------------------------------------------------------*/
  /* Get the first used sector in erase set.                           */
  /*-------------------------------------------------------------------*/
  for (s = 0; s < Flash->set_blocks; ++s)
    if (Flash->erase_set[s] != -1)
    {
      sect = Flash->erase_set[s] * Flash->block_sects;
      for (i = 0; i < Flash->block_sects; ++i, ++sect)
      {
        /*-------------------------------------------------------------*/
        /* If a valid sector is found, stop.                           */
        /*-------------------------------------------------------------*/
        if (used_sect(sect))
        {
          write_ctrl = TRUE;
          break;
        }
      }
      if (write_ctrl)
        break;
    }

  /*-------------------------------------------------------------------*/
  /* If any sectors from erasable set have to be copied, copy them.    */
  /*-------------------------------------------------------------------*/
  if (write_ctrl)
  {
    /*-----------------------------------------------------------------*/
    /* Copy all used sectors off the erasable set.                     */
    /*-----------------------------------------------------------------*/
    while (sect != (ui32)-1)
    {
      /*---------------------------------------------------------------*/
      /* Original sector cannot be dirty.                              */
      /*---------------------------------------------------------------*/
      PfAssert((Flash->sect_tbl[sect].prev != FDRTY_SECT));

      /*---------------------------------------------------------------*/
      /* Ensure expected free sector is actually free.                 */
      /*---------------------------------------------------------------*/
      PfAssert(Flash->sect_tbl[Flash->free_sect].prev == FFREE_SECT);

      /*---------------------------------------------------------------*/
      /* Relocate used sector to free sector.                          */
      /*---------------------------------------------------------------*/
      if (FlashRelocSect(sect, Flash->free_sect, NULL))
        return -1;

      /*---------------------------------------------------------------*/
      /* Advance to next used sector in erase set.                     */
      /*---------------------------------------------------------------*/
      sect = get_next_used_sector(sect, &s);
    }
  }

  /*-------------------------------------------------------------------*/
  /* Perform the end of a recycle.                                     */
  /*-------------------------------------------------------------------*/
  if (recycle_finish(check_full, write_ctrl))
    return -1;

  return 0;
}

#if FFS_DEBUG
/***********************************************************************/
/* Debug Helper Function                                               */
/***********************************************************************/
void ValidateFiles(void);
void ValidateFiles(void)
{
  FFSEnts *tmp_table;
  ui32 sect, size, i, count, num_files = 0;
  ui8 *sectors;

  /*-------------------------------------------------------------------*/
  /* Check consistency of every file in the files tables.              */
  /*-------------------------------------------------------------------*/
  sectors = calloc(Flash->num_sects, 1);
  for (tmp_table = Flash->files_tbl; tmp_table;
       tmp_table = tmp_table->next_tbl)
    for (i = 0; i < FNUM_ENT; ++i)
    {
      if (tmp_table->tbl[i].type == FFILEN &&
          tmp_table->tbl[i].entry.file.comm->size != 0)
      {
        ++num_files;
        size = count = 0;
        memset(sectors, 0, Flash->num_sects);
        for (sect = tmp_table->tbl[i].entry.file.comm->frst_sect;
             Flash->sect_tbl[sect].next != FLAST_SECT;
             sect = Flash->sect_tbl[sect].next)
        {
          if (sectors[sect] < 5)
             sectors[sect] += 1;
          if (sect > Flash->num_sects)
            break;
          if (++count >= Flash->num_sects)
          {
            sect = 11110000;
            break;
          }
          size += Flash->sect_sz;
          if (sectors[sect] == 1)
          {
            if (sect == tmp_table->tbl[i].entry.file.comm->last_sect &&
                Flash->sect_tbl[sect].next != FLAST_SECT)
              printf("last_sect %u -> next = 0x%X\n", sect,
                     Flash->sect_tbl[sect].next);
            if (sect != tmp_table->tbl[i].entry.file.comm->frst_sect &&
                Flash->sect_tbl[sect].prev > Flash->num_sects)
              printf("sect %u -> prev = 0x%X\n", sect,
                     Flash->sect_tbl[sect].prev);
            if (Flash->sect_tbl[sect].prev < Flash->num_sects)
            {
              if (Flash->sect_tbl[Flash->sect_tbl[sect].prev].next !=
                  sect)
                printf("sect %u -> prev = %u -> next = %u\n", sect,
                      Flash->sect_tbl[sect].prev,
                       Flash->sect_tbl[Flash->sect_tbl[sect].prev].next);
            }
            if (Flash->sect_tbl[sect].next < Flash->num_sects)
            {
              if (Flash->sect_tbl[Flash->sect_tbl[sect].next].prev !=
                  sect)
                printf("sect %u -> next = %u -> prev = %u\n", sect,
                       Flash->sect_tbl[sect].next,
                       Flash->sect_tbl[Flash->sect_tbl[sect].next].prev);
            }
          }
        }
        if (tmp_table->tbl[i].entry.file.comm->one_past_last.sector !=
            (ui16)-1)
          size +=tmp_table->tbl[i].entry.file.comm->one_past_last.offset;
        else
          size += Flash->sect_sz;
        if (size != tmp_table->tbl[i].entry.file.comm->size)
        {
          printf("ValidateStream: CORRUPT FILE!\n");
          printf("file %s: frst = %lu, last = %lu,  size = %lu vs.:\n",
                 tmp_table->tbl[i].entry.file.name,
                 tmp_table->tbl[i].entry.file.comm->frst_sect,
                 tmp_table->tbl[i].entry.file.comm->last_sect,
                 tmp_table->tbl[i].entry.file.comm->size);
          printf("      actual last = %lu, actual size = %lu\n", sect,
                 size);
          if (sect == 11110000)
            printf("LOOP in sector list\n");
          else if (sect > Flash->num_sects)
            printf("INVALID SECTOR VALUE sect = 0x%X\n", sect);
          printf("------------------------------------------------\n");
        }
      }
    }
  printf("volume_check checked %u files\n", num_files);
}

/***********************************************************************/
/* Debug Helper Function                                               */
/***********************************************************************/
static int count_used(void)
{
  int r_val = 0, s;

  for (s = 0; s < Flash->set_blocks; ++s)
    if (Flash->erase_set[s] != -1)
      r_val += Flash->blocks[Flash->erase_set[s]].used_sects;
  return r_val;
}

/***********************************************************************/
/* Debug Helper Function                                               */
/***********************************************************************/
static int check_used(void)
{
  int sect, used = 0;

  for (sect = 0; sect < Flash->num_sects; ++sect)
    if (Flash->sect_tbl[sect].prev != FDRTY_SECT &&
        Flash->sect_tbl[sect].prev != FFREE_SECT &&
        Flash->sect_tbl[sect].prev != FHDER_SECT &&
        Flash->sect_tbl[sect].prev != FNVLD_SECT)
      ++used;

  if (used != Flash->used_sects + Flash->ctrl_sects)
  {
    printf("INCONSISTENT used: thought = %u vs. actual = %u\n",
           Flash->used_sects, used);
    return -1;
  }
  return 0;
}

/***********************************************************************/
/* Debug Helper Function                                               */
/***********************************************************************/
void ValidateStream(int fid);
void ValidateStream(int fid)
{
  FILE *stream = &Files[fid];
  int sect, size = 0;
  FFIL_T *link_ptr = &((FFSEnt *)stream->handle)->entry.file;
  struct stat buf;

  if (link_ptr->comm->size != 0)
  {
    for (sect = link_ptr->comm->frst_sect;
         Flash->sect_tbl[sect].next != FLAST_SECT;
         sect = Flash->sect_tbl[sect].next)
      size += Flash->sect_sz;
    if (link_ptr->comm->one_past_last.sector != (ui16)-1)
      size += link_ptr->comm->one_past_last.offset;
    else
      size += Flash->sect_sz;

    if (fstat(fid, &buf))
    {
      PfAssert(FALSE);
    }

    printf("file: frst = %u, last = %u,  size = %u vs.:\n",
           link_ptr->comm->frst_sect, link_ptr->comm->last_sect,
           link_ptr->comm->size);
    printf("      actual last = %u, actual size = %u\n",
           sect, size);
    if (size != link_ptr->comm->size ||
        sect != link_ptr->comm->last_sect)
      printf("ValidateStream: CORRUPT FILE!\n");
  }
  check_used();
}
#endif

/***********************************************************************/
/* read_entry_name: Read the name of a dir or link from flash          */
/*                                                                     */
/*       Input: str = string where to place the name                   */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
static int read_entry_name(char *str)
{
  int  i;
  char c;

  for (i = 0;; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* Read character from flash.                                      */
    /*-----------------------------------------------------------------*/
    if (read_ctrl(&c, sizeof(ui8)))
      return -1;
    str[i] = c;

    /*-----------------------------------------------------------------*/
    /* If we've reached the end of the string, stop.                   */
    /*-----------------------------------------------------------------*/
    if (c == 0)
      return 0;

    /*-----------------------------------------------------------------*/
    /* If we are at the limit but there still are characters to read,  */
    /* mark filename so that later, when we've read all of them we can */
    /* go back and truncate the long ones.                             */
    /*-----------------------------------------------------------------*/
    if (i == FILENAME_MAX)
    {
      str[i - 1] = '*';
      str[i] = '\0';
      return 0;
    }
  }
}

/***********************************************************************/
/* set_parent_dirs: Set the parent_dir pointer for files for older     */
/*              versions that don't have that as part of the control   */
/*                                                                     */
/***********************************************************************/
static void set_parent_dirs(void)
{
  FFSEnts *tbl;
  FFSEnt *entry, *f_entry;
  int i;

  for (tbl = Flash->files_tbl; tbl; tbl = tbl->next_tbl)
  {
    entry = &tbl->tbl[0];
    for (i = 0; i < FNUM_ENT; ++i, ++entry)
    {
      /*---------------------------------------------------------------*/
      /* Walk each directory's entry list updating the file entries.   */
      /*---------------------------------------------------------------*/
      if (entry->type == FDIREN)
        for (f_entry = entry->entry.dir.first; f_entry;
             f_entry = f_entry->entry.dir.next)
          if (f_entry->type == FFILEN)
            f_entry->entry.file.parent_dir = entry;
    }
  }
}

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/* FlashRecycle: Perform at least a block recycle and by the time it's */
/*              done it is guaranteed there is enough free sectors for */
/*              normal operation to resume.                            */
/*                                                                     */
/*       Input: check_full = flag to check for volume full             */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int FlashRecycle(int check_full)
{
  ui32 num_recycles = 1;

#if TIMING
clock_t sample = clock();
#endif
  /*-------------------------------------------------------------------*/
  /* Keep doing recycles until no more are needed.                     */
  /*-------------------------------------------------------------------*/
#if FFS_DEBUG
  int count = 0;
  printf("--------------------recycle begin-------------------------\n");
#endif
  for (;;)
  {
#if FFS_DEBUG
    if (!count++)
      printf("rcycl set w/ %u used, free = %u - free sects = ",
             count_used(), Flash->free_sects);
    else
      printf("  * rcycl set w/ %u used, free =%u - free sects = ",
             count_used(), Flash->free_sects);
#endif
#if DO_JMP_RECYCLE
    EnableCount = TRUE;
#endif
    /*-----------------------------------------------------------------*/
    /* Perform block recycle.                                          */
    /*-----------------------------------------------------------------*/
    if (recycle_set(check_full))
      break;
#if DO_JMP_RECYCLE
    EnableCount = FALSE;
#endif

#if FFS_DEBUG
    printf("%lu - nxt set USE_WEAR = %lu used\n", Flash->free_sects,
           count_used());
    printf("      -> frst_free = %lu, frst_ctrl = %lu,"
           " bad_blocks = %lu\n", Flash->free_sect,Flash->frst_ctrl_sect,
           Flash->bad_blocks);
    if (FlashRecNeeded() == FALSE)
      printf("----------------------recycle end---------------------\n");
#endif
    /*-----------------------------------------------------------------*/
    /* If finished, return.                                            */
    /*-----------------------------------------------------------------*/
    if (FlashRecNeeded() == FALSE)
    {
#if TIMING
TotRec += (clock() - sample);
#endif
      return 0;
    }

    /*-----------------------------------------------------------------*/
    /* If syncs are disabled, readjust the set size.                   */
    /*-----------------------------------------------------------------*/
    if (!Flash->do_sync && num_recycles)
      adjust_set_size(ADJUST_ERASE, CHECK_RECYCLE);

    /*-----------------------------------------------------------------*/
    /* If unfinished after full round of recycles, we're out of memory.*/
    /*-----------------------------------------------------------------*/
    PfAssert(num_recycles < 2*(Flash->num_blocks/Flash->set_blocks + 1));
    if (++num_recycles > 2 * (Flash->num_blocks / Flash->set_blocks + 1))
    {
      set_errno(ENOSPC);
      break;
    }
#if FFS_DEBUG
    if (recycle_not_possible())
      printf("        --> free sects = %u, nxt set IGNORE_WEAR = %u "
             "used\n", Flash->free_sects, count_used());
#endif
  }

  /*-------------------------------------------------------------------*/
  /* In case of an error in recycle, still choose next erasable block  */
  /* normally first and, if that requires a recycle, re-choose without */
  /* considering the wear count difference, before returning an error. */
  /*-------------------------------------------------------------------*/
  FlashChooseEraseSet(USE_WEAR);
  if (FlashRecNeeded())
    FlashChooseEraseSet(IGNORE_WEAR);
#if FFS_DEBUG
  printf("           --> free sects = %u, nxt set %u used\n",
         Flash->free_sects, count_used());
  printf("-------------recycle end ENOSPC---------------------------\n");
#endif
  return -1;
}

/***********************************************************************/
/* FlashTruncateNames: Go through all the dir and link entries in all  */
/*              tables and truncate the ones that need to be truncated */
/*                                                                     */
/***********************************************************************/
void FlashTruncateNames(void)
{
  FFSEnts *temp_table = Flash->files_tbl;
  FFSEnt *temp_entry;
  int i, j, k, m;
  char *temp_name, digit;

  do
  {
    /*-----------------------------------------------------------------*/
    /* Go through all the entries in the current table.                */
    /*-----------------------------------------------------------------*/
    for (i = 0; i < FNUM_ENT; ++i)
    {
      /*---------------------------------------------------------------*/
      /* If this entry is either a dir or link and its name has to be  */
      /* truncated, do it.                                             */
      /*---------------------------------------------------------------*/
      if ((temp_table->tbl[i].type == FDIREN ||
           temp_table->tbl[i].type == FFILEN) &&
          (temp_table->tbl[i].entry.dir.name[FILENAME_MAX - 1] == '*'))
      {
        /*-------------------------------------------------------------*/
        /* Truncate works by having the last five chars in the name be */
        /* _xxxx, where xxxx is a number. Start with xxxx be 0000 and  */
        /* look for name duplication.                                  */
        /*-------------------------------------------------------------*/
        temp_name = temp_table->tbl[i].entry.dir.name;

        /*-------------------------------------------------------------*/
        /* First place the '_'.                                        */
        /*-------------------------------------------------------------*/
        temp_name[FILENAME_MAX - 5] = '_';
        temp_name[FILENAME_MAX] = '\0';

        /*-------------------------------------------------------------*/
        /* Keep putting a number in (starting with 0), until a unique  */
        /* name is found.                                              */
        /*-------------------------------------------------------------*/
        for (j = 0;; ++j)
        {
          /*-----------------------------------------------------------*/
          /* Convert the number to "xxxx" and add it to the name.      */
          /*-----------------------------------------------------------*/
          for (k = 3, m = j; k >= 0; --k, m /= 10)
          {
            digit = (char)(m % 10 + '0');
            temp_name[FILENAME_MAX - 4 + k] = digit;
          }

          /*-----------------------------------------------------------*/
          /* Look through the list of entries in this entry's parent   */
          /* directory for name duplication. Start by going through    */
          /* the prev entries first.                                   */
          /*-----------------------------------------------------------*/
          for (temp_entry = temp_table->tbl[i].entry.dir.prev;
               temp_entry; temp_entry = temp_entry->entry.dir.prev)
          {
            /*---------------------------------------------------------*/
            /* If the name is the same, stop and start over.           */
            /*---------------------------------------------------------*/
            if (!strcmp(temp_entry->entry.dir.name, temp_name))
              break;
          }

          /*-----------------------------------------------------------*/
          /* If no name duplication was found in the prev entries,     */
          /* look also at the next entries.                            */
          /*-----------------------------------------------------------*/
          if (!temp_entry)
            for (temp_entry = temp_table->tbl[i].entry.dir.next;
                 temp_entry; temp_entry = temp_entry->entry.dir.next)
            {
              /*-------------------------------------------------------*/
              /* If the name is the same, stop and start over.         */
              /*-------------------------------------------------------*/
              if (strcmp(temp_entry->entry.dir.name, temp_name))
                break;
            }

          /*-----------------------------------------------------------*/
          /* If no name duplication found, keep name.                  */
          /*-----------------------------------------------------------*/
          if (!temp_entry)
            break;
        }
      }
    }

    /*-----------------------------------------------------------------*/
    /* Go to the next table.                                           */
    /*-----------------------------------------------------------------*/
    temp_table = temp_table->next_tbl;
  } while (temp_table);
}

/***********************************************************************/
/* FlashConvertToPtr: Transform an offset into a pointer so that it    */
/*              can be read from flash                                 */
/*                                                                     */
/*       Input: entry = offset value to be converted                   */
/*                                                                     */
/*     Returns: pointer equivalent of offset                           */
/*                                                                     */
/***********************************************************************/
FFSEnt *FlashConvertToPtr(ui32 entry)
{
  FFSEnts *tbl;
  ui16 table_num, table_offset;
  OffLoc *entry_loc = (void *)&entry;

  /*-------------------------------------------------------------------*/
  /* For NULL pointers, return NULL.                                   */
  /*-------------------------------------------------------------------*/
  if (entry == (ui32)-1)
    return NULL;

  /*-------------------------------------------------------------------*/
  /* Get the table number and offset.                                  */
  /*-------------------------------------------------------------------*/
  table_num = entry_loc->sector;
  table_offset = entry_loc->offset;

  /*-------------------------------------------------------------------*/
  /* For OFF_REMOVED_LINK, restore it as REMOVED_LINK.                 */
  /*-------------------------------------------------------------------*/
  if (table_num == OFF_REMOVED_LINK && table_offset == OFF_REMOVED_LINK)
    return (FFSEnt *)REMOVED_LINK;

  /*-------------------------------------------------------------------*/
  /* First figure out the table this entry belongs to.                 */
  /*-------------------------------------------------------------------*/
  for (tbl = Flash->files_tbl; tbl && table_num > 1; --table_num)
    tbl = tbl->next_tbl;

  /*-------------------------------------------------------------------*/
  /* The entry must belong to a table.                                 */
  /*-------------------------------------------------------------------*/
  PfAssert(tbl);

  /*-------------------------------------------------------------------*/
  /* Figure out the pointer value.                                     */
  /*-------------------------------------------------------------------*/
  return tbl->tbl + table_offset;
}

/***********************************************************************/
/* FlashCheckDelLinks: Check that there are no links any longer in use */
/*                                                                     */
/***********************************************************************/
void FlashCheckDelLinks(void)
{
  FFSEnts *tbl;
  FFIL_T  *link_ptr;
  int     i;

#if QUOTA_ENABLED
  ui32 used;
#endif /* QUOTA_ENABLED */

  /*-------------------------------------------------------------------*/
  /* Go through all tables, and in each table look at all links.       */
  /*-------------------------------------------------------------------*/
  for (tbl = Flash->files_tbl; tbl; tbl = tbl->next_tbl)
  {
    for (i = 0; i < FNUM_ENT; ++i)
    {
      /*---------------------------------------------------------------*/
      /* If entry is link and can be removed, do so.                   */
      /*---------------------------------------------------------------*/
      if (tbl->tbl[i].type == FFILEN &&
          tbl->tbl[i].entry.file.next == (FFSEnt *)REMOVED_LINK &&
          tbl->tbl[i].entry.file.prev == (FFSEnt *)REMOVED_LINK)
      {
        link_ptr = &(tbl->tbl[i].entry.file);
#if QUOTA_ENABLED
        used = 0;
#endif /* QUOTA_ENABLED */

        /*-------------------------------------------------------------*/
        /* If the number of links to this file is 0, need to remove    */
        /* the contents of the file.                                   */
        /*-------------------------------------------------------------*/
        if (link_ptr->comm->links == 0)
        {
          /*-----------------------------------------------------------*/
          /* Truncate file size to zero.                               */
          /*-----------------------------------------------------------*/
          FlashTruncZero(link_ptr, ADJUST_FCBs);

#if QUOTA_ENABLED
          /*-----------------------------------------------------------*/
          /* If volume has quotas, remember link used space.           */
          /*-----------------------------------------------------------*/
          if (Flash->quota_enabled)
            used += OFCOM_SZ;
#endif /* QUOTA_ENABLED */

          /*-----------------------------------------------------------*/
          /* Update file tables size.                                  */
          /*-----------------------------------------------------------*/
          Flash->tbls_size -= OFCOM_SZ;

          /*-----------------------------------------------------------*/
          /* Remove file from the files table and adjust free entries. */
          /*-----------------------------------------------------------*/
          link_ptr->comm->addr->type = FEMPTY;
          ++Flash->total_free;
          FlashIncrementTblEntries(link_ptr->comm->addr);
        }

        /*-------------------------------------------------------------*/
        /* Clear struct fields.                                        */
        /*-------------------------------------------------------------*/
        link_ptr->comm = NULL;
        link_ptr->next = link_ptr->prev = NULL;
        link_ptr->file = NULL;
        tbl->tbl[i].type = FEMPTY;

        /*-------------------------------------------------------------*/
        /* Update file tables size.                                    */
        /*-------------------------------------------------------------*/
        Flash->tbls_size -= (strlen(link_ptr->name) + 1 + OFFIL_SZ);

        /*-------------------------------------------------------------*/
        /* Increment number of free entries.                           */
        /*-------------------------------------------------------------*/
        ++Flash->total_free;
        FlashIncrementTblEntries(&(tbl->tbl[i]));

#if QUOTA_ENABLED
        /*-------------------------------------------------------------*/
        /* If quota enabled, update quota values due to file removal.  */
        /*-------------------------------------------------------------*/
        if (Flash->quota_enabled)
        {
          FFSEnt *ent, *root = &Flash->files_tbl->tbl[0];
          used += (strlen(link_ptr->name) + 1 + OFFIL_SZ);

          /*-----------------------------------------------------------*/
          /* Update used from parent to root.                          */
          /*-----------------------------------------------------------*/
          for (ent = link_ptr->parent_dir; ent;
               ent = ent->entry.dir.parent_dir)
          {
            PfAssert(ent->type == FDIREN && ent->entry.dir.used >= used);
            ent->entry.dir.used -= used;
          }

          /*-----------------------------------------------------------*/
          /* Update free below.                                        */
          /*-----------------------------------------------------------*/
          FlashFreeBelow(root);

          /*-----------------------------------------------------------*/
          /* Recompute free at each node.                              */
          /*-----------------------------------------------------------*/
          FlashFree(root, root->entry.dir.max_q - FlashVirtUsed(root));
        }
#endif /* QUOTA_ENABLED */

        /*-------------------------------------------------------------*/
        /* Do garbage collection if necessary.                         */
        /*-------------------------------------------------------------*/
        if (Flash->total_free >= (FNUM_ENT * 5 / 3))
          FlashGarbageCollect();
      }
    }
  }
}

/***********************************************************************/
/* FlashEraseBlock: Erase a block of memory                            */
/*                                                                     */
/*       Input: block = block to be erased                             */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
int FlashEraseBlock(ui32 block)
{
  ui32 addr = Flash->mem_base + block * Flash->block_size;

  /*-------------------------------------------------------------------*/
  /* Erase the block.                                                  */
  /*-------------------------------------------------------------------*/
  if (Flash->erase_block_wrapper(addr))
  {
    set_errno(EIO);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Bump up the wear count for this block.                            */
  /*-------------------------------------------------------------------*/
  Flash->blocks[block].wear_count += 1;
  if (Flash->high_wear < Flash->blocks[block].wear_count)
    Flash->high_wear = Flash->blocks[block].wear_count;

  return 0;
}

/***********************************************************************/
/* FlashCountAndChoose: Count control info and choose erase set        */
/*                                                                     */
/*       Input: check_recycle = flag to check recycles in adjust set   */
/*                                                                     */
/***********************************************************************/
void FlashCountAndChoose(int check_recycle)
{
  count_ctrl_size(ADJUST_ERASE, check_recycle);
  FlashChooseEraseSet(USE_WEAR);
  if (recycle_not_possible())
    FlashChooseEraseSet(IGNORE_WEAR);
}

/***********************************************************************/
/* FlashRecycleValues: Helper function to compute values for recycle   */
/*              needed, recycle not possible, and vstat functions      */
/*                                                                     */
/*     Outputs: ctrl_blks = number of control blocks needed            */
/*              used = number of used sectors in erase set             */
/*              blk_sects = number of sectors per block                */
/*                                                                     */
/***********************************************************************/
void FlashRecycleValues(ui32 *ctrl_blks, ui32 *used, ui32 *blk_sects)
{
  ui32 consec_disturbs = (Flash->type == FFS_NAND) ? CONSEC_DISTURBS : 0;
  ui32 num_ctrl, block_sects, ctrl_sects;
  ui32 used_sects = 0, s;

  /*-------------------------------------------------------------------*/
  /* Determine the number of sectors in an erasable block.             */
  /*-------------------------------------------------------------------*/
  *blk_sects = block_sects = ((Flash->type == FFS_NOR) ?
                              Flash->block_sects - Flash->hdr_sects :
                              Flash->block_sects);

  /*-------------------------------------------------------------------*/
  /* Figure out how many unused sectors left in last control block.    */
  /*-------------------------------------------------------------------*/
  if (Flash->free_ctrl_sect == (ui32)-1)
    num_ctrl = 0;
  else
    num_ctrl = Flash->block_sects - (Flash->free_ctrl_sect %
                                     Flash->block_sects);

  /*-------------------------------------------------------------------*/
  /* Figure out the control size in sectors.                           */
  /*-------------------------------------------------------------------*/
  ctrl_sects = Flash->ctrl_sects * (1 + consec_disturbs);

  /*-------------------------------------------------------------------*/
  /* Figure out number of extra sectors (without the last ctrl block)  */
  /* needed for new control information writes.                        */
  /*-------------------------------------------------------------------*/
  if (num_ctrl >= ctrl_sects)
  {
    /*-----------------------------------------------------------------*/
    /* If enough exist in last control, no more needed, else at least  */
    /* 1 block needed.                                                 */
    /*-----------------------------------------------------------------*/
    if (num_ctrl / Flash->ctrl_sects >= Flash->num_blocks -
        Flash->set_blocks)
      ctrl_sects = 0;
    else
      ctrl_sects = 1;
  }
  else
    ctrl_sects -= num_ctrl;

  /*-------------------------------------------------------------------*/
  /* Round number of extra control sectors to number of blocks.        */
  /*-------------------------------------------------------------------*/
  *ctrl_blks = (ctrl_sects + block_sects - 1) / block_sects;

  /*-------------------------------------------------------------------*/
  /* Count number of used sectors in erasable set.                     */
  /*-------------------------------------------------------------------*/
  PfAssert(Flash->set_blocks > 0);
  for (s = 0; s < Flash->set_blocks; ++s)
    if (Flash->erase_set[s] != -1)
      used_sects += Flash->blocks[Flash->erase_set[s]].used_sects;
  *used = used_sects + 2;
}

/***********************************************************************/
/* FlashRecNeeded: Figure out if a recycle is needed                   */
/*                                                                     */
/*     Returns: FALSE if no recycle needed, TRUE if recycle is needed  */
/*                                                                     */
/***********************************************************************/
int FlashRecNeeded(void)
{
  ui32 block_sects, used, ctrl_blocks;

  FlashRecycleValues(&ctrl_blocks, &used, &block_sects);

#if FFS_DEBUG
if (ctrl_blocks * block_sects + used >= Flash->free_sects)
{
  int fr_scts = 0, sct;
  printf("recycle_needed: ctrl_sects = %u, ctrl_blocks = %u data = %u "
         " free = %u\n", Flash->ctrl_sects, ctrl_blocks, used,
         Flash->free_sects);
  for (sct = (int)Flash->free_sect; sct != FLAST_SECT;
       sct = Flash->sect_tbl[sct].next)
  {
    if (Flash->sect_tbl[sct].prev != FFREE_SECT)
      printf("check_flash: CORRUPT FREE LIST!!!!!\n");
    else
      ++fr_scts;
  }
  printf("recycle_needed: 1st free = %u; last free = %u, set = %u\n",
         Flash->free_sect, Flash->last_free_sect, Flash->set_blocks);
  printf("  - actual = %lu thought free = %lu, used = %u\n",
         fr_scts, Flash->free_sects, Flash->used_sects);
}
#endif

  return (ctrl_blocks * block_sects + used >= Flash->free_sects ||
          Flash->free_sects < Flash->block_sects);
}

/***********************************************************************/
/* FlashInvalidSignature: Look for the signature bytes (if partial     */
/*              signature bytes found, complete them). If no signature */
/*              bytes found, return unformat error.                    */
/*                                                                     */
/*     Returns: 0 if bytes found or format, -1 otherwise               */
/*                                                                     */
/***********************************************************************/
int FlashInvalidSignature(void)
{
  ui8 *last_hdr_sect = Flash->tmp_sect, *sig_bytes;
  ui16 sig_val;
  ui32 hdr;
  int  i, invalid_block = -1, status;
  FlashGlob *vol;

  /*-------------------------------------------------------------------*/
  /* Go through all the blocks looking for signature bytes.            */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < Flash->num_blocks; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* Set the wear count and ctrl block flag for this block.          */
    /*-----------------------------------------------------------------*/
    Flash->blocks[i].wear_count = 0;
    Flash->blocks[i].ctrl_block = FALSE;

    /*-----------------------------------------------------------------*/
    /* Read in last sector of current header.                          */
    /*-----------------------------------------------------------------*/
    if (Flash->read_sector(last_hdr_sect, Flash->hdr_sects + i *
                                          Flash->block_sects - 1))
    {
      set_errno(EIO);
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* If the signature bytes are not there, either a recycle got cut  */
    /* off or volume is not valid.                                     */
    /*-----------------------------------------------------------------*/
    sig_bytes = &last_hdr_sect[Flash->sect_sz - RES_BYTES];
    sig_val = sig_bytes_val(sig_bytes[2], sig_bytes[3]);
    if (!(sig_bytes[0] == NOR_BYTE1 && sig_bytes[1] == NOR_BYTE2 &&
          sig_val >= INIT_SIG && sig_val <= CURR_NOR_SIG))
    {
      /*---------------------------------------------------------------*/
      /* If this is the first block without a signature, remember it,  */
      /* else the whole volume is invalid.                             */
      /*---------------------------------------------------------------*/
      if (invalid_block == -1)
        invalid_block = i;
      else
      {
        set_errno(ENXIO);
        return -1;
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* If only one invalid block, erase it and write in signature bytes. */
  /*-------------------------------------------------------------------*/
  if (invalid_block != -1)
  {
    /*-----------------------------------------------------------------*/
    /* Figure out beginning of block and erase it.                     */
    /*-----------------------------------------------------------------*/
    hdr = Flash->mem_base + invalid_block * Flash->block_sects *
                            Flash->sect_sz;
    if (Flash->erase_block_wrapper(hdr))
    {
      set_errno(EIO);
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Bump up the wear count for block and write signature bytes.     */
    /*-----------------------------------------------------------------*/
    hdr += Flash->hdr_sects * Flash->sect_sz - RES_BYTES;
    Flash->blocks[invalid_block].wear_count += 1;
    if (Flash->high_wear < Flash->blocks[invalid_block].wear_count)
      Flash->high_wear = Flash->blocks[invalid_block].wear_count;

    /*-----------------------------------------------------------------*/
    /* Release exclusive access to the flash file system.              */
    /*-----------------------------------------------------------------*/
    vol = Flash;
    semPost(FlashSem);

    /*-----------------------------------------------------------------*/
    /* Write signature bytes.                                          */
    /*-----------------------------------------------------------------*/
    status = vol->driver.nor.write_byte(hdr++, NOR_BYTE1, vol->vol) ||
             vol->driver.nor.write_byte(hdr++, NOR_BYTE2, vol->vol) ||
             vol->driver.nor.write_byte(hdr++, NOR_BYTE3, vol->vol) ||
             vol->driver.nor.write_byte(hdr, NOR_BYTE4, vol->vol);

    /*-----------------------------------------------------------------*/
    /* Acquire exclusive access to the flash file system.              */
    /*-----------------------------------------------------------------*/
    semPend(FlashSem, WAIT_FOREVER);
    Flash = vol;

    if (status)
      return -1;
  }
  return 0;
}

/***********************************************************************/
/* FlashChooseEraseSet: Choose next (optimal) erasable set for recycle */
/*                                                                     */
/*       Input: enforce_wear_limit = when set to TRUE considers the    */
/*              MAX_WDELTA in optimal function, otherwise it doesn't   */
/*                                                                     */
/*     Returns: Next block to be erased by a recycle                   */
/*                                                                     */
/***********************************************************************/
void FlashChooseEraseSet(int enforce_wear_limit)
{
  int s, b, max_f, f_b, max_b;
  ui32 i, sect, s_optimal, free_blck, last_in_free_block;

  /*-------------------------------------------------------------------*/
  /* Select the fraction of the erase set in which to place optimally  */
  /* chosen blocks, the rest will have worst blocks. Optimization is   */
  /* given by the number of used sectors in block and wear count.      */
  /*-------------------------------------------------------------------*/
  if (enforce_wear_limit)
    s_optimal = (Flash->set_blocks + 1) * 3 / 4;
  else
    s_optimal = Flash->set_blocks;

  /*-------------------------------------------------------------------*/
  /* Place blocks in set in order of increasing selector function.     */
  /*-------------------------------------------------------------------*/
  for (s = 0; s < Flash->set_blocks; ++s)
  {
    for (b = 0, max_f = -1; b < Flash->num_blocks; ++b)
    {
      /*---------------------------------------------------------------*/
      /* If block is not bad, ctrl and has no free sects, candidate.   */
      /*---------------------------------------------------------------*/
      if (!Flash->blocks[b].bad_block && !Flash->blocks[b].ctrl_block &&
          Flash->sect_tbl[(b + 1) * Flash->block_sects - 1].prev !=
          FFREE_SECT)
      {
        /*-------------------------------------------------------------*/
        /* Sanity check.                                               */
        /*-------------------------------------------------------------*/
        PfAssert(Flash->high_wear >= Flash->blocks[b].wear_count);

        /*-------------------------------------------------------------*/
        /* Compute the selector. First 3/4th optimal selector, rest    */
        /* worst selector used.                                        */
        /*-------------------------------------------------------------*/
        if (s < s_optimal)
        {
          f_b = (int)(16 * (Flash->block_sects -
                      Flash->blocks[b].used_sects) +
                      (Flash->high_wear - Flash->blocks[b].wear_count));
          if (enforce_wear_limit &&
              Flash->high_wear >= Flash->blocks[b].wear_count +
                                  MAX_WDELTA)
            f_b += 65536;
        }
        else
          f_b = (int)(Flash->high_wear - Flash->blocks[b].wear_count);

        /*-------------------------------------------------------------*/
        /* If better candidate, remember it.                           */
        /*-------------------------------------------------------------*/
        if (max_f < f_b)
        {
          max_f = f_b;
          max_b = b;
        }
      }
    }

    /*-----------------------------------------------------------------*/
    /* If candidate block found, place it in set.                      */
    /*-----------------------------------------------------------------*/
    if (max_f != -1)
    {
      Flash->erase_set[s] = max_b; /*lint !e644 */

      /*---------------------------------------------------------------*/
      /* Temporarily mark block as bad so it's not selected next time  */
      /* around.                                                       */
      /*---------------------------------------------------------------*/
      Flash->blocks[max_b].bad_block = TRUE;
    }

    /*-----------------------------------------------------------------*/
    /* Else stop and start using free blocks.                          */
    /*-----------------------------------------------------------------*/
    else
      break;
  }

  /*-------------------------------------------------------------------*/
  /* If not enough non-free blocks to fill set, use free blocks.       */
  /*-------------------------------------------------------------------*/
  for (; s < Flash->set_blocks; ++s)
  {
    /*-----------------------------------------------------------------*/
    /* Select the first free block and get the last sector in block.   */
    /*-----------------------------------------------------------------*/
    free_blck = Flash->free_sect / Flash->block_sects;
    last_in_free_block = (free_blck + 1) * Flash->block_sects - 1;
    PfAssert(Flash->sect_tbl[last_in_free_block].prev == FFREE_SECT);

    /*-----------------------------------------------------------------*/
    /* Put the block on the erase set.                                 */
    /*-----------------------------------------------------------------*/
    Flash->erase_set[s] = (int)free_blck;

    /*-----------------------------------------------------------------*/
    /* Advance free sector to next free block.                         */
    /*-----------------------------------------------------------------*/
    PfAssert(Flash->sect_tbl[last_in_free_block].next != FLAST_SECT);
    Flash->free_sect = Flash->sect_tbl[last_in_free_block].next;
    PfAssert(Flash->sect_tbl[Flash->free_sect].prev == FFREE_SECT);

    /*-----------------------------------------------------------------*/
    /* Mark all free sectors in erase set block to dirty.              */
    /*-----------------------------------------------------------------*/
    sect = free_blck * Flash->block_sects + Flash->hdr_sects;
    for (i = Flash->hdr_sects; i < Flash->block_sects; ++i, ++sect)
    {
      if (Flash->sect_tbl[sect].prev == FFREE_SECT)
      {
        Flash->sect_tbl[sect].prev = FDRTY_SECT;
        --Flash->free_sects;
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* Turn back to good all blocks in erasable set.                     */
  /*-------------------------------------------------------------------*/
  for (s = 0; s < Flash->set_blocks; ++s)
    Flash->blocks[Flash->erase_set[s]].bad_block = FALSE;

  /*-------------------------------------------------------------------*/
  /* Save enforce_wear_limit flag for use with FlashAdjustEraseSet().  */
  /*-------------------------------------------------------------------*/
  Flash->enforce_wear = (ui8)enforce_wear_limit;
}

/***********************************************************************/
/* FlashAdjustEraseSet: Check if erasable set needs updating           */
/*                                                                     */
/*       Input: candidate_block = possible replacement                 */
/*                                                                     */
/***********************************************************************/
void FlashAdjustEraseSet(ui32 candidate_block)
{
  ui32 f_c, f_s, s, s_optimal = (Flash->set_blocks + 1) * 3 / 4;

  /*-------------------------------------------------------------------*/
  /* If block does not contain free sectors and its ctrl block flag is */
  /* not set, compare it with erasable set blocks.                     */
  /*-------------------------------------------------------------------*/
  if (Flash->sect_tbl[(candidate_block + 1) *
                      Flash->block_sects - 1].prev != FFREE_SECT &&
      Flash->blocks[candidate_block].ctrl_block == FALSE &&
      Flash->blocks[candidate_block].bad_block == FALSE)
  {
    /*-----------------------------------------------------------------*/
    /* Check if block already in set.                                  */
    /*-----------------------------------------------------------------*/
    for (s = 0; s < Flash->set_blocks; ++s)
      if (Flash->erase_set[s] == candidate_block)
        return;

    /*-----------------------------------------------------------------*/
    /* Compute the selector for candidate block.                       */
    /*-----------------------------------------------------------------*/
    f_c = 16 * (Flash->block_sects -
                Flash->blocks[candidate_block].used_sects) +
          Flash->high_wear - Flash->blocks[candidate_block].wear_count;

    /*-----------------------------------------------------------------*/
    /* Compare selector with selectors of all blocks in erasable set.  */
    /*-----------------------------------------------------------------*/
    for (s = 0; s < Flash->set_blocks; ++s)
    {
      /*---------------------------------------------------------------*/
      /* Skip '-1' slots.                                              */
      /*---------------------------------------------------------------*/
      if (Flash->erase_set[s] == -1)
        continue;

      /*---------------------------------------------------------------*/
      /* Compute selector for current block in set.                    */
      /*---------------------------------------------------------------*/
      if (s < s_optimal)
      {
        f_s = 16 * (Flash->block_sects -
                    Flash->blocks[Flash->erase_set[s]].used_sects) +
                   (Flash->high_wear -
                    Flash->blocks[Flash->erase_set[s]].wear_count);

        /*-------------------------------------------------------------*/
        /* See if we need adjusting for max wear being reached.        */
        /*-------------------------------------------------------------*/
        if (Flash->enforce_wear)
        {
          if (Flash->high_wear >=
              Flash->blocks[Flash->erase_set[s]].wear_count + MAX_WDELTA)
          {
            f_s += 65536;
            if (Flash->high_wear >=
                Flash->blocks[candidate_block].wear_count + MAX_WDELTA)
              f_c += 65536;
          }
        }

        /*-------------------------------------------------------------*/
        /* If better than current block in set, replace it.            */
        /*-------------------------------------------------------------*/
        if (f_c > f_s)
        {
          Flash->erase_set[s] = (int)candidate_block;
          break;
        }
        else if (f_c >= 65536)
          f_c -= 65536;
      }
    }
  }
}

/***********************************************************************/
/* FlashRecChck: Perform recycles if they are needed                   */
/*                                                                     */
/*       Input: check_full = flag to check for volume full or not      */
/*                                                                     */
/*     Returns: RECYCLE_NOT_NEEDED, RECYCLE_FAILED, or RECYCLE_OK      */
/*                                                                     */
/***********************************************************************/
int FlashRecChck(int check_full)
{
  /*-------------------------------------------------------------------*/
  /* If a recycle is not needed, skip.                                 */
  /*-------------------------------------------------------------------*/
  if (FlashRecNeeded() == FALSE)
    return RECYCLE_NOT_NEEDED;

  /*-------------------------------------------------------------------*/
  /* Flush sectors from cache to flash.                                */
  /*-------------------------------------------------------------------*/
  if (FlushSectors(&Flash->cache) == -1)
    return RECYCLE_FAILED;

  /*-------------------------------------------------------------------*/
  /* Perform recycle.                                                  */
  /*-------------------------------------------------------------------*/
  if (FlashRecycle(check_full))
    return RECYCLE_FAILED;

  return RECYCLE_OK;
}

/***********************************************************************/
/*  FlashRoom: Check whether volume is full or not                     */
/*                                                                     */
/*       Input: create_entity_incr = extra space in ctrl information   */
/*                      due to the creation of new dir/file            */
/*                                                                     */
/*     Returns: Number of sectors till volume becomes full             */
/*                                                                     */
/***********************************************************************/
ui32 FlashRoom(ui32 create_entity_incr)
{
  ui32 avail = Flash->num_sects - Flash->max_bad_blocks *
                                  Flash->block_sects;
  ui32 left_s = EXTRA_FREE / Flash->sect_sz + 2, ctrl_inc;
  ui32 ctrl_sects, block_sects = (Flash->type == FFS_NAND) ?
                                 Flash->block_sects :
                                 Flash->block_sects - Flash->hdr_sects;

  /*-------------------------------------------------------------------*/
  /* Compute the control sectors with control increment accounted.     */
  /*-------------------------------------------------------------------*/
  ctrl_inc = (avail - Flash->used_sects - Flash->set_blocks *
              block_sects - Flash->num_blocks * Flash->hdr_sects +
              block_sects - 1) / block_sects + Flash->used_sects;
  ctrl_sects = (Flash->ctrl_size + create_entity_incr + 2 * ctrl_inc +
                Flash->sect_sz - 1) / Flash->sect_sz;

  /*-------------------------------------------------------------------*/
  /* Calculate required number of reserved sectors based on type.      */
  /*-------------------------------------------------------------------*/
  if (Flash->type == FFS_NOR)
  {
    /*-----------------------------------------------------------------*/
    /* Take into account erase set, number of used sectors, header     */
    /* sectors and 1/5th block sects (for fewer recycles).             */
    /*-----------------------------------------------------------------*/
    left_s += (((5 * Flash->set_blocks + 1) * block_sects) / 5 +
               Flash->used_sects + Flash->num_blocks * Flash->hdr_sects);

    /*-----------------------------------------------------------------*/
    /* Take into account 2 control writes (in number of blocks).       */
    /*-----------------------------------------------------------------*/
    left_s += 2 * block_sects * ((ctrl_sects + block_sects - 1) /
              block_sects);
  }
  else
  {
    /*-----------------------------------------------------------------*/
    /* Because NAND/MLC have no resume, reserve one extra block.       */
    /*-----------------------------------------------------------------*/
    left_s += Flash->block_sects;

    /*-----------------------------------------------------------------*/
    /* Take into account erase set and used sectors.                   */
    /*-----------------------------------------------------------------*/
    left_s += (Flash->set_blocks * block_sects + Flash->used_sects);

    /*-----------------------------------------------------------------*/
    /* Take into account 2 control writes (in number of blocks).       */
    /*-----------------------------------------------------------------*/
    if (Flash->type == FFS_NAND)
      left_s += 2 * block_sects * (((CONSEC_DISTURBS + 1) * ctrl_sects +
                                    block_sects - 1) / block_sects);
    else
      left_s += 2 * block_sects * ((ctrl_sects + block_sects - 1) /
                                   block_sects);
  }

  /*-------------------------------------------------------------------*/
  /* Compare number of used/reserved sectors to total available.       */
  /*-------------------------------------------------------------------*/
  if (left_s < avail)
    return avail - left_s;
  else
  {
#if FFS_DEBUG
printf("VlFull: set_blks = %u, ctrl_sects = %u, "
       "left_s = %u < avail = %u\n", Flash->set_blocks,
       Flash->ctrl_sects, left_s, avail);
#endif
    set_errno(ENOSPC);
    return 0L;
  }
}

/***********************************************************************/
/* FlashFrSect: Move free sect ptr if necessary to where sector can be */
/*              used without checking for recycles or num used         */
/*                                                                     */
/*       Input: comm_ptr = pointer to file that called function        */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int FlashFrSect(void *comm_ptr)
{
  int r_val;

  /*-------------------------------------------------------------------*/
  /* If number of bad blocks exceeds maximum, error.                   */
  /*-------------------------------------------------------------------*/
  if (Flash->bad_blocks > Flash->max_bad_blocks)
  {
    set_errno(EIO);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Check for move to new non-contiguous free block or new file.      */
  /*-------------------------------------------------------------------*/
  if (Flash->free_sect + 1 != Flash->sect_tbl[Flash->free_sect].next ||
      Flash->comm_ptr != comm_ptr)
  {
    /*-----------------------------------------------------------------*/
    /* Increment number of free sectors used since last recount and    */
    /* recount control info size if it could have grown in sectors.    */
    /*-----------------------------------------------------------------*/
    ++Flash->free_sects_used;
    if (3 * Flash->free_sects_used + Flash->ctrl_size % Flash->sect_sz
        >= Flash->sect_sz)
    {
      count_ctrl_size(ADJUST_ERASE, CHECK_RECYCLE);
    }
  }

  /*-------------------------------------------------------------------*/
  /* Remember volume comm pointer for above comparison on next entry.  */
  /*-------------------------------------------------------------------*/
  Flash->comm_ptr = comm_ptr;

  /*-------------------------------------------------------------------*/
  /* If volume is full, return error.                                  */
  /*-------------------------------------------------------------------*/
  if (!FlashRoom(0))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Check recycles.                                                   */
  /*-------------------------------------------------------------------*/
  r_val = FlashRecChck(CHECK_FULL);
  if (r_val == RECYCLE_FAILED)
    return -1;

  return 0;
}

/***********************************************************************/
/* FlashSemiMount: Attempt partial read of control from flash. Stop    */
/*              when bad block info is read.                           */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int FlashSemiMount(void)
{
  ui32 tmp_ui32;
  ui8 tmp_ui8, byte;
  int i, j;

  /*-------------------------------------------------------------------*/
  /* Look for the place in flash where the last control info was.      */
  /*-------------------------------------------------------------------*/
  Flash->ctrl_sect = (ui32)Flash->find_last_ctrl();
  if (Flash->ctrl_sect == (ui32)-1)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Reset the read_ctrl internals.                                    */
  /*-------------------------------------------------------------------*/
  if (read_ctrl(NULL, 0))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Read ctrl size.                                                   */
  /*-------------------------------------------------------------------*/
  if (read_ctrl(&tmp_ui32, sizeof(ui32)))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Read the sequence number.                                         */
  /*-------------------------------------------------------------------*/
  if (read_ctrl(&tmp_ui32, sizeof(ui32)))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Get the total number of flash entries, and the size of the sector */
  /* table in shorts, file number generator, first free sector and     */
  /* number of free sectors.                                           */
  /*-------------------------------------------------------------------*/
  if (read_ctrl(&tmp_ui32, sizeof(ui32)) ||
      read_ctrl(&tmp_ui32, sizeof(ui32)) ||
      read_ctrl(&tmp_ui32, sizeof(ui32)) ||
      read_ctrl(&tmp_ui32, sizeof(ui32)) ||
      read_ctrl(&tmp_ui32, sizeof(ui32)) ||
      read_ctrl(&tmp_ui32, sizeof(ui32)))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Read high wear and offsets for all blocks.                        */
  /*-------------------------------------------------------------------*/
  if (read_ctrl(&tmp_ui32, sizeof(ui32)))
    return -1;
  for (i = 0; i < Flash->num_blocks; i += 2)
    if (read_ctrl(&tmp_ui8, sizeof(ui8)))
      return -1;

  /*-------------------------------------------------------------------*/
  /* Read bad block flags for NAND only.                               */
  /*-------------------------------------------------------------------*/
  if (Flash->type == FFS_NAND)
    for (i = 0; i < Flash->num_blocks; i += 8)
    {
      if (read_ctrl(&byte, sizeof(ui8)))
        return -1;
      for (j = 0; j < 8; ++j)
        if (i + j < Flash->num_blocks)
        {
          if (byte & (1 << j))
          {
            Flash->blocks[i + j].bad_block = TRUE;
            ++Flash->bad_blocks;
          }
          else
            Flash->blocks[i + j].bad_block = FALSE;
        }
    }
  else
    for (i = 0; i < Flash->num_blocks; ++i)
      Flash->blocks[i].bad_block = FALSE;

  return 0;
}

/***********************************************************************/
/* FlashRdCtrl: Read all the control information from flash            */
/*                                                                     */
/*       Input: partial_read = when TRUE, partial read because called  */
/*                             from format                             */
/*                                                                     */
/*     Returns: FREAD_EMP if flash is empty, FREAD_VAL if valid,       */
/*              FREAD_ERR if error                                     */
/*                                                                     */
/***********************************************************************/
int FlashRdCtrl(int partial_read)
{
  ui8  two_offsets, byte;
  int  offset, status, modified = FALSE, fast_mount_check = FALSE;
  uint i, j;
  ui32 num_tables, num_free_entries, ctrl_num_shorts, sig_bytes;
  ui32 offset_ptr;
  ui16 sector, last_next, last, look_back_sector = (ui16)-1, first;
  ui16 sig_val;
  FFSEnts *tbl, *last_table;
  FFSEnt *v_ent;

#if DO_JMP_RECYCLE
  EnableCount = FALSE;
#endif
  /*-------------------------------------------------------------------*/
  /* Look for the signature bytes (NOR only).                          */
  /*-------------------------------------------------------------------*/
  if (Flash->type == FFS_NOR && FlashInvalidSignature())
    return FREAD_ERR;

  /*-------------------------------------------------------------------*/
  /* Look for the place in flash where the last control info was.      */
  /*-------------------------------------------------------------------*/
  Flash->ctrl_sect = (ui32)Flash->find_last_ctrl();
  if (Flash->ctrl_sect == (ui32)-1)
  {
    set_errno(ENXIO);
    return FREAD_ERR;
  }

  /*-------------------------------------------------------------------*/
  /* Set the first sector of the last valid control.                   */
  /*-------------------------------------------------------------------*/
  Flash->frst_ctrl_sect = Flash->ctrl_sect;

  /*-------------------------------------------------------------------*/
  /* Reset the read_ctrl internals.                                    */
  /*-------------------------------------------------------------------*/
  if (read_ctrl(NULL, 0))
    return FREAD_ERR;
  Flash->tbls_size = 0;

  /*-------------------------------------------------------------------*/
  /* Read ctrl size.                                                   */
  /*-------------------------------------------------------------------*/
  if (read_ctrl_ui32(&Flash->ctrl_size))
    return FREAD_ERR;

  /*-------------------------------------------------------------------*/
  /* Read the sequence number.                                         */
  /*-------------------------------------------------------------------*/
  if (read_ctrl_ui32(&Flash->seq_num))
    return FREAD_ERR;

  /*-------------------------------------------------------------------*/
  /* Get the total number of flash entries, and the size of the sector */
  /* table in shorts, file number generator, first free sector and     */
  /* number of free sectors.                                           */
  /*-------------------------------------------------------------------*/
  if (read_ctrl_ui32(&Flash->total) ||
      read_ctrl_ui32(&ctrl_num_shorts) ||
      read_ctrl_ui32(&Flash->fileno_gen) ||
      read_ctrl_ui32(&Flash->free_sect) ||
      read_ctrl_ui32(&Flash->last_free_sect) ||
      read_ctrl_ui32(&Flash->free_sects))
    return FREAD_ERR;

  /*-------------------------------------------------------------------*/
  /* Read high wear and offsets for all blocks.                        */
  /*-------------------------------------------------------------------*/
  if (read_ctrl_ui32(&Flash->high_wear))
    return FREAD_ERR;
  for (i = 0; i < Flash->num_blocks; i += 2)
  {
    if (read_ctrl(&two_offsets, sizeof(ui8)))
      return FREAD_ERR;

    /*-----------------------------------------------------------------*/
    /* Figure out the even block offset.                               */
    /*-----------------------------------------------------------------*/
    offset = (two_offsets & 0xF0) >> 4;
    offset &= 0xF;
    Flash->blocks[i].wear_count = Flash->high_wear - offset;
    Flash->blocks[i].ctrl_block = FALSE;

    /*-----------------------------------------------------------------*/
    /* Figure out the odd block offset.                                */
    /*-----------------------------------------------------------------*/
    if (i + 1 < Flash->num_blocks)
    {
      offset = two_offsets & 0xF;
      Flash->blocks[i + 1].wear_count = Flash->high_wear - offset;
      Flash->blocks[i + 1].ctrl_block = FALSE;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Set bad block flags and check signature bytes.                    */
  /*-------------------------------------------------------------------*/
  Flash->bad_blocks = 0;
  if (Flash->type == FFS_NAND)
  {
    for (i = 0; i < Flash->num_blocks; i += 8)
    {
      if (read_ctrl(&byte, sizeof(ui8)))
        return FREAD_ERR;
      for (j = 0; j < 8; ++j)
        if (i + j < Flash->num_blocks)
        {
          if (byte & (1 << j))
          {
            Flash->blocks[i + j].bad_block = TRUE;
            ++Flash->bad_blocks;
          }
          else
            Flash->blocks[i + j].bad_block = FALSE;
        }
    }

    /*-----------------------------------------------------------------*/
    /* Check the signature bytes for NAND.                             */
    /*-----------------------------------------------------------------*/
    if (read_ctrl_ui32(&sig_bytes))
      return FREAD_ERR;
    sig_val = sig_bytes_val((ui8)((sig_bytes & 0xFF00) >> 8),
                            (ui8)(sig_bytes & 0xFF));
    if ((sig_bytes & 0xFFFF0000) != (NAND_BYTE1 << 24 | NAND_BYTE2 << 16)
        || sig_val < INIT_SIG || sig_val > CURR_NAND_SIG)
    {
      set_errno(ENXIO);
      return FREAD_ERR;
    }
  }
  else
  {
    for (i = 0; i < Flash->num_blocks; ++i)
      Flash->blocks[i].bad_block = FALSE;

    /*-----------------------------------------------------------------*/
    /* Read the NOR/MLC signature bytes.                               */
    /*-----------------------------------------------------------------*/
    if (read_ctrl_ui32(&sig_bytes))
      return FREAD_ERR;
    sig_val = sig_bytes_val((ui8)((sig_bytes & 0xFF00) >> 8),
                            (ui8)(sig_bytes & 0xFF));

    /*-----------------------------------------------------------------*/
    /* Check the signature bytes for NOR.                              */
    /*-----------------------------------------------------------------*/
    if (Flash->type == FFS_NOR)
    {
      if ((sig_bytes & 0xFFFF0000) != (NOR_BYTE1 << 24 | NOR_BYTE2 << 16)
          || sig_val < INIT_SIG || sig_val > CURR_NOR_SIG)
      {
        set_errno(ENXIO);
        return FREAD_ERR;
      }
    }

    /*-----------------------------------------------------------------*/
    /* Check the signature bytes for MLC.                              */
    /*-----------------------------------------------------------------*/
    else
    {
      if ((sig_bytes & 0xFFFF0000) != (MLC_BYTE1 << 24 | MLC_BYTE2 << 16)
          || sig_val < INIT_SIG || sig_val > CURR_MLC_SIG)
      {
        set_errno(ENXIO);
        return FREAD_ERR;
      }
    }

    /*-----------------------------------------------------------------*/
    /* Check if fast mount is enabled.                                 */
    /*-----------------------------------------------------------------*/
    fast_mount_check = sig_val > INIT_SIG;
  }

  /*-------------------------------------------------------------------*/
  /* Read the sector where control information will be written next.   */
  /*-------------------------------------------------------------------*/
  if (read_ctrl_ui32(&Flash->free_ctrl_sect))
    return FREAD_ERR;

  PfAssert(Flash->bad_blocks <= Flash->max_bad_blocks + 1);

#if QUOTA_ENABLED
  /*-------------------------------------------------------------------*/
  /* Check version number for quota compatibility.                     */
  /*-------------------------------------------------------------------*/
  if (Flash->quota_enabled &&
      ((Flash->type == FFS_NAND && sig_val <= 10) ||
       (Flash->type == FFS_NOR && sig_val <= 11)))
    return FREAD_ERR;

  /*-------------------------------------------------------------------*/
  /* Read the quota enabled flag.                                      */
  /*-------------------------------------------------------------------*/
  if (read_ctrl(&byte, sizeof(ui8)))
    return FREAD_ERR;
  if (byte != Flash->quota_enabled)
    return FREAD_ERR;
#endif /* QUOTA_ENABLED */

  /*-------------------------------------------------------------------*/
  /* Figure out how many tables needed and allocate them.              */
  /*-------------------------------------------------------------------*/
  num_tables = Flash->total / FNUM_ENT;

  /*-------------------------------------------------------------------*/
  /* At this point in time, files_tbl should be null for vol.          */
  /*-------------------------------------------------------------------*/
  PfAssert(Flash->files_tbl == NULL);

  /*-------------------------------------------------------------------*/
  /* Allocate space for the first flash table.                         */
  /*-------------------------------------------------------------------*/
  Flash->files_tbl = calloc(1, sizeof(FFSEnts));
  if (Flash->files_tbl == NULL)
  {
    set_errno(ENOMEM);
    return FREAD_ERR;
  }
  Flash->files_tbl->prev_tbl = Flash->files_tbl->next_tbl = NULL;
  --num_tables;

  /*-------------------------------------------------------------------*/
  /* Allocate space for all the other flash tables.                    */
  /*-------------------------------------------------------------------*/
  for (i = 0, last_table = Flash->files_tbl; i < num_tables; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* Allocate flash table. If unable, set errno and goto error exit. */
    /*-----------------------------------------------------------------*/
    tbl = calloc(1, sizeof(FFSEnts));
    if (tbl == NULL)
    {
      set_errno(ENOMEM);
      goto FlashRdCtrl_error;
    }

    /*-----------------------------------------------------------------*/
    /* Add allocated table to the list of entries tables.              */
    /*-----------------------------------------------------------------*/
    tbl->next_tbl = NULL;
    last_table->next_tbl = tbl;
    tbl->prev_tbl = last_table;
    last_table = tbl;
  }

  /*-------------------------------------------------------------------*/
  /* Get the flash table information from FLASH.                       */
  /*-------------------------------------------------------------------*/
  Flash->total_free = 0;
  for (tbl = Flash->files_tbl; tbl; tbl = tbl->next_tbl)
  {
    for (i = 0, num_free_entries = 0; i < FNUM_ENT; ++i)
    {
      v_ent = &tbl->tbl[i];

      /*---------------------------------------------------------------*/
      /* Read the next entry type.                                     */
      /*---------------------------------------------------------------*/
      if (read_ctrl(&v_ent->type, sizeof(ui8)))
        goto FlashRdCtrl_error;

      /*---------------------------------------------------------------*/
      /* Based on the type of the entry, read its contents and copy    */
      /* all the info from off_ent to the current table entry,         */
      /* converting all offsets to pointers where necessary.           */
      /*---------------------------------------------------------------*/
      switch (v_ent->type)
      {
        /*-------------------------------------------------------------*/
        /* For an empty entry just increment the num of free entries.  */
        /*-------------------------------------------------------------*/
        case FEMPTY:
        {
          ++num_free_entries;
          break;
        }

        /*-------------------------------------------------------------*/
        /* For a directory entry need to convert all ptrs to offsets.  */
        /*-------------------------------------------------------------*/
        case FDIREN:
        {
          /*-----------------------------------------------------------*/
          /* Read all offset pointers and convert them.                */
          /*-----------------------------------------------------------*/
          if (read_ctrl_ui32(&offset_ptr))
            goto FlashRdCtrl_error;
          v_ent->entry.dir.next = FlashConvertToPtr(offset_ptr);
          if (read_ctrl_ui32(&offset_ptr))
            goto FlashRdCtrl_error;
          v_ent->entry.dir.prev = FlashConvertToPtr(offset_ptr);
          if (read_ctrl_ui32(&offset_ptr))
            goto FlashRdCtrl_error;
          v_ent->entry.dir.comm = (void *)FlashConvertToPtr(offset_ptr);
          if (read_ctrl_ui32(&offset_ptr))
            goto FlashRdCtrl_error;
          v_ent->entry.dir.parent_dir = FlashConvertToPtr(offset_ptr);
          if (read_ctrl_ui32(&offset_ptr))
            goto FlashRdCtrl_error;
          v_ent->entry.dir.first = FlashConvertToPtr(offset_ptr);

#if QUOTA_ENABLED
          /*-----------------------------------------------------------*/
          /* Read the quota information if volume uses it.             */
          /*-----------------------------------------------------------*/
          if (Flash->quota_enabled)
          {
            if (read_ctrl_ui32(&v_ent->entry.dir.max_q))
              goto FlashRdCtrl_error;
            if (read_ctrl_ui32(&v_ent->entry.dir.min_q))
              goto FlashRdCtrl_error;
            if (read_ctrl_ui32(&v_ent->entry.dir.used))
              goto FlashRdCtrl_error;
            if (read_ctrl_ui32(&v_ent->entry.dir.free))
              goto FlashRdCtrl_error;
            if (read_ctrl_ui32(&v_ent->entry.dir.free_below))
              goto FlashRdCtrl_error;
            if (read_ctrl_ui32(&v_ent->entry.dir.res_below))
              goto FlashRdCtrl_error;
            Flash->tbls_size += OFDIR_QUOTA_SZ;
          }
#endif /* QUOTA_ENABLED */

          /*-----------------------------------------------------------*/
          /* Read the name for the directory.                          */
          /*-----------------------------------------------------------*/
          if (read_entry_name(v_ent->entry.dir.name))
            goto FlashRdCtrl_error;

          /*-----------------------------------------------------------*/
          /* Directory is neither open nor current working directory.  */
          /*-----------------------------------------------------------*/
          v_ent->entry.dir.dir = NULL;
          v_ent->entry.dir.cwds = 0;

          /*-----------------------------------------------------------*/
          /* Update file tables size for control info.                 */
          /*-----------------------------------------------------------*/
          Flash->tbls_size +=
                          (OFDIR_SZ + strlen(v_ent->entry.dir.name) + 1);

          break;
        }

        /*-------------------------------------------------------------*/
        /* For a file entry no conversion necessary, just copy info.   */
        /*-------------------------------------------------------------*/
        case FCOMMN:
        {
          /*-----------------------------------------------------------*/
          /* Read one_past_last.                                       */
          /*-----------------------------------------------------------*/
          if (read_ctrl_ui16(&v_ent->entry.comm.one_past_last.sector))
            goto FlashRdCtrl_error;
          if (read_ctrl_ui16(&v_ent->entry.comm.one_past_last.offset))
            goto FlashRdCtrl_error;

          /*-----------------------------------------------------------*/
          /* Read the times.                                           */
          /*-----------------------------------------------------------*/
          if (read_ctrl_ui32(&v_ent->entry.comm.mod_time))
            goto FlashRdCtrl_error;
          if (read_ctrl_ui32(&v_ent->entry.comm.ac_time))
            goto FlashRdCtrl_error;

          /*-----------------------------------------------------------*/
          /* Read the file number.                                     */
          /*-----------------------------------------------------------*/
          if (read_ctrl_ui32(&v_ent->entry.comm.fileno))
            goto FlashRdCtrl_error;

          /*-----------------------------------------------------------*/
          /* Read the file size.                                       */
          /*-----------------------------------------------------------*/
          if (read_ctrl_ui32(&v_ent->entry.comm.size))
            goto FlashRdCtrl_error;

          /*-----------------------------------------------------------*/
          /* Read the first and last sectors.                          */
          /*-----------------------------------------------------------*/
          if (read_ctrl_ui16(&v_ent->entry.comm.frst_sect))
            goto FlashRdCtrl_error;
          if (read_ctrl_ui16(&v_ent->entry.comm.last_sect))
            goto FlashRdCtrl_error;

          /*-----------------------------------------------------------*/
          /* Read the user/group ID and creation mode.                 */
          /*-----------------------------------------------------------*/
          if (read_ctrl_ui16(&v_ent->entry.comm.user_id))
            goto FlashRdCtrl_error;
          if (read_ctrl_ui16(&v_ent->entry.comm.group_id))
            goto FlashRdCtrl_error;
          if (read_ctrl_ui16(&v_ent->entry.comm.mode))
            goto FlashRdCtrl_error;

          /*-----------------------------------------------------------*/
          /* Read the number of links.                                 */
          /*-----------------------------------------------------------*/
          if (read_ctrl(&v_ent->entry.comm.links, sizeof(ui8)))
            goto FlashRdCtrl_error;

          /*-----------------------------------------------------------*/
          /* If attributes present read them, else set to 0.           */
          /*-----------------------------------------------------------*/
          if ((Flash->type == FFS_NAND && sig_val >= 10) ||
              (Flash->type == FFS_NOR && sig_val >= 11))
          {
            if (read_ctrl_ui32(&v_ent->entry.comm.attrib))
              goto FlashRdCtrl_error;
          }
          else
            v_ent->entry.comm.attrib = 0;

          /*-----------------------------------------------------------*/
          /* All the files should be closed at startup.                */
          /*-----------------------------------------------------------*/
          v_ent->entry.comm.open_links = 0;

          /*-----------------------------------------------------------*/
          /* Set the address.                                          */
          /*-----------------------------------------------------------*/
          v_ent->entry.comm.addr = v_ent;

          /*-----------------------------------------------------------*/
          /* All files and dirs should be closed at startup.           */
          /*-----------------------------------------------------------*/
          v_ent->entry.comm.open_mode = 0;

          /*-----------------------------------------------------------*/
          /* Update file tables size for control info.                 */
          /*-----------------------------------------------------------*/
          Flash->tbls_size += OFCOM_SZ;

#if BACKWARD_COMPATIBLE
          if (read_ctrl(&byte, sizeof(ui8)))
            goto FlashRdCtrl_error;
#endif

          break;
        }

        /*-------------------------------------------------------------*/
        /* For a link entry need to convert all ptrs to offsets.       */
        /*-------------------------------------------------------------*/
        case FFILEN:
        {
          /*-----------------------------------------------------------*/
          /* Read all offset pointers and convert them.                */
          /*-----------------------------------------------------------*/
          if (read_ctrl_ui32(&offset_ptr))
            goto FlashRdCtrl_error;
          v_ent->entry.file.next = FlashConvertToPtr(offset_ptr);
          if (read_ctrl_ui32(&offset_ptr))
            goto FlashRdCtrl_error;
          v_ent->entry.file.prev = FlashConvertToPtr(offset_ptr);
          if (read_ctrl_ui32(&offset_ptr))
            goto FlashRdCtrl_error;
          v_ent->entry.file.comm = (void *)FlashConvertToPtr(offset_ptr);

          /*-----------------------------------------------------------*/
          /* If parent dir present read it.                            */
          /*-----------------------------------------------------------*/
          if ((Flash->type == FFS_NAND && sig_val >= 10) ||
              (Flash->type == FFS_NOR && sig_val >= 11))
          {
            if (read_ctrl_ui32(&offset_ptr))
              goto FlashRdCtrl_error;
            v_ent->entry.file.parent_dir = FlashConvertToPtr(offset_ptr);
          }

          /*-----------------------------------------------------------*/
          /* Read the name for the link.                               */
          /*-----------------------------------------------------------*/
          if (read_entry_name(v_ent->entry.file.name))
            goto FlashRdCtrl_error;

          /*-----------------------------------------------------------*/
          /* Set the file pointer.                                     */
          /*-----------------------------------------------------------*/
          v_ent->entry.file.file = NULL;

          /*-----------------------------------------------------------*/
          /* Update file tables size for control info.                 */
          /*-----------------------------------------------------------*/
          Flash->tbls_size +=
                         (OFFIL_SZ + strlen(v_ent->entry.file.name) + 1);

          break;
        }

        default:
        {
          /*-----------------------------------------------------------*/
          /* We shouldn't be here.                                     */
          /*-----------------------------------------------------------*/
          PfAssert(FALSE); /*lint !e506 !e774 */
          break;
        }
      }
    }
    tbl->free = (int)num_free_entries;
    Flash->total_free += num_free_entries;
  }

  /*-------------------------------------------------------------------*/
  /* After reading the whole table entries, go through the list of     */
  /* tables to truncate the link and dir names that need to be         */
  /* truncated.                                                        */
  /*-------------------------------------------------------------------*/
  FlashTruncateNames();

  /*-------------------------------------------------------------------*/
  /* If an older version without parent_dir for files, set them now.   */
  /*-------------------------------------------------------------------*/
  if ((Flash->type == FFS_NAND && sig_val < 10) ||
      (Flash->type == FFS_NOR && sig_val < 11))
    set_parent_dirs();

  /*-------------------------------------------------------------------*/
  /* Get the sector table. Start by first marking invalid sectors, and */
  /* everything else is dirty.                                         */
  /*-------------------------------------------------------------------*/
  for (sector = 0, i = 0; i < Flash->num_blocks; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* If block is bad, mark all its sectors as invalid, else dirty.   */
    /*-----------------------------------------------------------------*/
    if (Flash->blocks[i].bad_block)
      for (j = 0; j < Flash->block_sects; ++j, ++sector)
        Flash->sect_tbl[sector].prev = FNVLD_SECT;
    else
      for (j = 0; j < Flash->block_sects; ++j, ++sector)
      {
        /*-------------------------------------------------------------*/
        /* For NOR, the beginning of each block has header sectors.    */
        /*-------------------------------------------------------------*/
        if (Flash->type == FFS_NOR && j < Flash->hdr_sects)
          Flash->sect_tbl[sector].prev = FHDER_SECT;
        else
          Flash->sect_tbl[sector].prev = FDRTY_SECT;
      }
  }

  /*-------------------------------------------------------------------*/
  /* Read back the free list.                                          */
  /*-------------------------------------------------------------------*/
  for (sector = (ui16)Flash->free_sect;; sector = last_next)
  {
    /*-----------------------------------------------------------------*/
    /* Read last sector in segment and its next pointer.               */
    /*-----------------------------------------------------------------*/
    if (read_ctrl_ui16(&last) || read_ctrl_ui16(&last_next))
      goto FlashRdCtrl_error;
    ctrl_num_shorts -= 2;

    /*-----------------------------------------------------------------*/
    /* Mark every sector (skipping headers/invalid ones) as free.      */
    /*-----------------------------------------------------------------*/
    for (i = sector;; i = j)
    {
      /*---------------------------------------------------------------*/
      /* Mark sector as free.                                          */
      /*---------------------------------------------------------------*/
      Flash->sect_tbl[i].prev = FFREE_SECT;

      /*---------------------------------------------------------------*/
      /* If this is last sector in segment, stop.                      */
      /*---------------------------------------------------------------*/
      if (i == last)
      {
        Flash->sect_tbl[i].next = last_next;
        break;
      }

      /*---------------------------------------------------------------*/
      /* Go to next sector to mark as free.                            */
      /*---------------------------------------------------------------*/
      j = i + 1;
      while (Flash->sect_tbl[j].prev == FHDER_SECT)
        ++j;
      Flash->sect_tbl[i].next = (ui16)j;
    }

    /*-----------------------------------------------------------------*/
    /* If no more segments, stop.                                      */
    /*-----------------------------------------------------------------*/
    if (last_next == FLAST_SECT)
      break;
  }

  /*-------------------------------------------------------------------*/
  /* Read used sectors.                                                */
  /*-------------------------------------------------------------------*/
  for (;;)
  {
    /*-----------------------------------------------------------------*/
    /* Read head of used sector list.                                  */
    /*-----------------------------------------------------------------*/
    if (read_ctrl_ui16(&sector))
      goto FlashRdCtrl_error;
    --ctrl_num_shorts;
    if (sector == FLAST_SECT)
      break;

    /*-----------------------------------------------------------------*/
    /* Set previous for head.                                          */
    /*-----------------------------------------------------------------*/
    Flash->sect_tbl[sector].prev = FLAST_SECT;
    first = sector;

    /*-----------------------------------------------------------------*/
    /* Read back list, a segment at a time.                            */
    /*-----------------------------------------------------------------*/
    do
    {
      /*---------------------------------------------------------------*/
      /* Read in either label or next pointer.                         */
      /*---------------------------------------------------------------*/
      if (read_ctrl_ui16(&last_next))
        goto FlashRdCtrl_error;
      --ctrl_num_shorts;

      /*---------------------------------------------------------------*/
      /* If label, read in last sector and next pointer.               */
      /*---------------------------------------------------------------*/
      if (last_next == FCONT_SGMT)
      {
        if (read_ctrl_ui16(&last) || read_ctrl_ui16(&last_next))
          goto FlashRdCtrl_error;
        ctrl_num_shorts -= 2;

        /*-------------------------------------------------------------*/
        /* Restore all sectors in segment.                             */
        /*-------------------------------------------------------------*/
        for (i = first;; i = j)
        {
          /*-----------------------------------------------------------*/
          /* If we've reached last sector in segment, stop.            */
          /*-----------------------------------------------------------*/
          if (i == last)
          {
            Flash->sect_tbl[i].next = last_next;
            if (last_next != FLAST_SECT)
              Flash->sect_tbl[last_next].prev = (ui16)i;
            break;
          }

          /*-----------------------------------------------------------*/
          /* Find next used in segment.                                */
          /*-----------------------------------------------------------*/
          j = i + 1;
          if (Flash->type == FFS_NOR)
            while (Flash->sect_tbl[j].prev == FHDER_SECT)
              ++j;

          /*-----------------------------------------------------------*/
          /* Link current to next.                                     */
          /*-----------------------------------------------------------*/
          Flash->sect_tbl[i].next = (ui16)j;
          Flash->sect_tbl[j].prev = (ui16)i;
        }
      }

      /*---------------------------------------------------------------*/
      /* Else it's a single sector.                                    */
      /*---------------------------------------------------------------*/
      else
      {
        Flash->sect_tbl[first].next = last_next;
        if (last_next != FLAST_SECT)
          Flash->sect_tbl[last_next].prev = first;
      }
      first = last_next;
    } while (first != FLAST_SECT);
  }
#if FFS_DEBUG
printf("RdCtrl: last ctrl sect = %u, ctrl_num_shorts = %u\n",
       Flash->ctrl_sect, ctrl_num_shorts);
#endif

  /*-------------------------------------------------------------------*/
  /* Choose a valid block for the erase set (1 block in size here).    */
  /*-------------------------------------------------------------------*/
  FlashChooseEraseSet(USE_WEAR);

  /*-------------------------------------------------------------------*/
  /* Determine the size of the control information.                    */
  /*-------------------------------------------------------------------*/
  count_ctrl_size(SKIP_ADJUST_ERASE, SKIP_CHECK_RECYCLE);
#if FFS_DEBUG
printf("MOUNT: 1st_ctrl = %u, ctrls = %u, 1st_free = %u, frees = %u\n",
       Flash->frst_ctrl_sect, Flash->ctrl_sects, Flash->free_sect,
       Flash->free_sects);
#endif

  /*-------------------------------------------------------------------*/
  /* Set the ctrl block flag for the ctrl blocks.                      */
  /*-------------------------------------------------------------------*/
  for (i = 0, sector = (ui16)Flash->frst_ctrl_sect; sector != FLAST_SECT;
       sector = Flash->sect_tbl[sector].next)
  {
    Flash->blocks[sector / Flash->block_sects].ctrl_block = TRUE;
    Flash->last_ctrl_sect = sector;
    if (++i > 2 * Flash->ctrl_sects)
      goto FlashRdCtrl_error;
  }

#if INC_NOR_FS
  /*-------------------------------------------------------------------*/
  /* For NOR, remember where the free area starts for resume. If free  */
  /* area is at the beginning of a block, no need to do resume because */
  /* the whole block will be erased by check_flash().                  */
  /*-------------------------------------------------------------------*/
  if (Flash->type == FFS_NOR && Flash->free_sect % Flash->block_sects)
    look_back_sector = (ui16)Flash->free_sect;
#endif

  /*-------------------------------------------------------------------*/
  /* Check to make sure that indeed all the free sectors are actually  */
  /* free.                                                             */
  /*-------------------------------------------------------------------*/
  status = check_flash(&modified, fast_mount_check);
  if (status == FFS_FAILED_POWER_UP)
    goto FlashRdCtrl_error;

  /*-------------------------------------------------------------------*/
  /* If just a partial read, stop here.                                */
  /*-------------------------------------------------------------------*/
  if (partial_read)
    return FREAD_VAL;

  /*-------------------------------------------------------------------*/
  /* If a recycle is needed, do one.                                   */
  /*-------------------------------------------------------------------*/
  if (FlashRecNeeded())
  {
#if FFS_DEBUG
printf("FlashRdCtrl: recycles needed. Performing resume...\n");
#endif
#if INC_NOR_FS
    /*-----------------------------------------------------------------*/
    /* Start out by doing a resume first to check for power cut offs.  */
    /*-----------------------------------------------------------------*/
    if (Flash->type == FFS_NOR && look_back_sector != (ui16)-1 &&
        status == FFS_SLOW_POWER_UP)
    {
      /*---------------------------------------------------------------*/
      /* Perform a resume to restore an interrupted recycle.           */
      /*---------------------------------------------------------------*/
      if (resume(look_back_sector) && get_errno() != ENOSPC)
        goto FlashRdCtrl_error;
    }
#endif

    /*-----------------------------------------------------------------*/
    /* If a recycle is needed, do one.                                 */
    /*-----------------------------------------------------------------*/
    if (FlashRoom(0) && FlashRecNeeded())
    {
      /*---------------------------------------------------------------*/
      /* Make sure that the next erase block can be erased.            */
      /*---------------------------------------------------------------*/
      if (recycle_not_possible())
        FlashChooseEraseSet(IGNORE_WEAR);

#if FFS_DEBUG
printf("FlashRdCtrl: performing recycle...\n");
#endif
      /*---------------------------------------------------------------*/
      /* Perform recycle.                                              */
      /*---------------------------------------------------------------*/
      if (FlashRecycle(CHECK_FULL))
      {
        /*-------------------------------------------------------------*/
        /* Return error only if volume is not full, otherwise allow    */
        /* mount to go through.                                        */
        /*-------------------------------------------------------------*/
        if (get_errno() != ENOSPC)
          goto FlashRdCtrl_error;
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* Save state if needed due to check_flash().                        */
  /*-------------------------------------------------------------------*/
  else if (modified)
    FlashSync(DO_SYNC);

#if FFS_DEBUG
printf("FlashRdCtrl: SUCCESSFUL READ\n");
#endif
  return FREAD_VAL;

  /*-------------------------------------------------------------------*/
  /* Error return. Free all tables that have been allocated.           */
  /*-------------------------------------------------------------------*/
FlashRdCtrl_error:
  last_table = Flash->files_tbl;
  do
  {
    tbl = last_table->next_tbl;
    last_table->next_tbl = last_table->prev_tbl = NULL;
    free(last_table);
    last_table = tbl;
  } while (last_table);
  return FREAD_ERR;
} /*lint !e550 !e529 */

/***********************************************************************/
/*   FlashSync: Write dirty sectors plus control to flash              */
/*                                                                     */
/*       Input: do_sync = flag to do sync or postpone it               */
/*                                                                     */
/*     Returns: NULL on success, (void *)-1 on failure                 */
/*                                                                     */
/***********************************************************************/
void *FlashSync(int do_sync)
{
  int r_value;

  /*-------------------------------------------------------------------*/
  /* Check if a recycle is needed.                                     */
  /*-------------------------------------------------------------------*/
  r_value = FlashRecChck(SKIP_CHECK_FULL);

  /*-------------------------------------------------------------------*/
  /* If recyle performed, stop, else if it failed return error.        */
  /*-------------------------------------------------------------------*/
  if (r_value == RECYCLE_OK)
    return NULL;
  else if (r_value == RECYCLE_FAILED)
    return (void *)-1;

  /*-------------------------------------------------------------------*/
  /* If sync is disabled, postpone the sync and recount the ctrl size. */
  /*-------------------------------------------------------------------*/
  if (!do_sync)
  {
    Flash->sync_deferred = TRUE;
    count_ctrl_size(ADJUST_ERASE, CHECK_RECYCLE);
  }

  /*-------------------------------------------------------------------*/
  /* Else flush the data cache and write the control information.      */
  /*-------------------------------------------------------------------*/
  else
  {
    if (FlushSectors(&Flash->cache) == -1)
      return (void *)-1;
    if (FlashWrCtrl(ADJUST_ERASE))
      return (void *)-1;
  }

  /*-------------------------------------------------------------------*/
  /* Check if recycles are needed after flushing and writing ctrl.     */
  /*-------------------------------------------------------------------*/
  if (FlashRecNeeded())
  {
    if (FlashRecycle(SKIP_CHECK_FULL))
      return (void *)-1;
  }
  return NULL;
}

/***********************************************************************/
/* FlashWrprWr: Wrapper function for write_sector()                    */
/*                                                                     */
/*      Inputs: entry = cache entry for sector                         */
/*              update = when set, update cache with new sector        */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int FlashWrprWr(CacheEntry *entry, int update)
{
  ui32    i, new_sector, sect_num = (ui32)entry->sect_num;
  void   *head = entry->sector;
  FCOM_T *comm_ptr = entry->file_ptr;

  /*-------------------------------------------------------------------*/
  /* The sector can only be a data sector.                             */
  /*-------------------------------------------------------------------*/
  PfAssert(used_sect(sect_num));

  /*-------------------------------------------------------------------*/
  /* Reset the wr_ctrl flag.                                           */
  /*-------------------------------------------------------------------*/
  Flash->wr_ctrl = FALSE;

  /*-------------------------------------------------------------------*/
  /* If sect_num is a new dirty sector, write directly without getting */
  /* a new sector first.                                               */
  /*-------------------------------------------------------------------*/
  if (entry->dirty == DIRTY_NEW)
    return Flash->write_sector(head, (ui32)entry->sect_num, DATA_SECT);

  /*-------------------------------------------------------------------*/
  /* Look for a new sector in the flash where to place this sector.    */
  /*-------------------------------------------------------------------*/
  if (Flash->free_sect != Flash->last_free_sect)
  {
    new_sector = Flash->free_sect;
    Flash->free_sect = Flash->sect_tbl[Flash->free_sect].next;
  }
  else
  {
    PfAssert(FALSE); /*lint !e506, !e774*/
    return -1;
  }
  PfAssert(Flash->sect_tbl[Flash->free_sect].prev == FFREE_SECT);

  /*-------------------------------------------------------------------*/
  /* This sector will be used, so just swap the block counts from old  */
  /* and new sector.                                                   */
  /*-------------------------------------------------------------------*/
  --Flash->blocks[sect_num / Flash->block_sects].used_sects;
  ++Flash->blocks[new_sector / Flash->block_sects].used_sects;

  /*-------------------------------------------------------------------*/
  /* Set the new sector to the value of the old sector.                */
  /*-------------------------------------------------------------------*/
  Flash->sect_tbl[new_sector].next = Flash->sect_tbl[sect_num].next;
  Flash->sect_tbl[new_sector].prev = Flash->sect_tbl[sect_num].prev;

  /*-------------------------------------------------------------------*/
  /* If this sector is not first one, update the one pointing to it.   */
  /*-------------------------------------------------------------------*/
  if (Flash->sect_tbl[new_sector].prev != FLAST_SECT)
    Flash->sect_tbl[Flash->sect_tbl[new_sector].prev].next =
                                                        (ui16)new_sector;

  /*-------------------------------------------------------------------*/
  /* If this sector is not last one, update the one pointing to it.    */
  /*-------------------------------------------------------------------*/
  if (Flash->sect_tbl[new_sector].next != FLAST_SECT)
    Flash->sect_tbl[Flash->sect_tbl[new_sector].next].prev =
                                                       (ui16)new_sector;

  /*-------------------------------------------------------------------*/
  /* Update other ctrl info if needed: if first sector, last sector,   */
  /* one_past_last are connected to sect_num, change to new_sector.    */
  /*-------------------------------------------------------------------*/
  if (comm_ptr->frst_sect == sect_num)
    comm_ptr->frst_sect = (ui16)new_sector;
  if (comm_ptr->last_sect == sect_num)
    comm_ptr->last_sect = (ui16)new_sector;
  if (comm_ptr->one_past_last.sector == (int)sect_num)
    comm_ptr->one_past_last.sector = (ui16)new_sector;

  /*-------------------------------------------------------------------*/
  /* Also, go through all open flash files and the ones who have their */
  /* current/old pointers point to sect_num, change to new_sector.     */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < FOPEN_MAX; ++i)
  {
    if (Files[i].ioctl == FlashIoctl && Files[i].volume == Flash)
    {
      if (Files[i].curr_ptr.sector == sect_num)
        Files[i].curr_ptr.sector = (ui16)new_sector;
      if (Files[i].old_ptr.sector == sect_num)
        Files[i].old_ptr.sector = (ui16)new_sector;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Set the old sector to dirty in the sectors table.                 */
  /*-------------------------------------------------------------------*/
  Flash->sect_tbl[sect_num].prev = FDRTY_SECT;

  /*-------------------------------------------------------------------*/
  /* If the sector needs to be updated in the cache, do it.            */
  /*-------------------------------------------------------------------*/
  if (update)
    UpdateCache(entry, (int)new_sector, &Flash->cache);

  /*-------------------------------------------------------------------*/
  /* Write the sector to flash and return.                             */
  /*-------------------------------------------------------------------*/
  return Flash->write_sector(head, new_sector, DATA_SECT);
}

/***********************************************************************/
/* FlashTruncZero: Make a file zero length                             */
/*                                                                     */
/*      Inputs: entry = pointer to file in Flash->files_tbl            */
/*              adjust = flag to adjust or not the file control blocks */
/*                                                                     */
/***********************************************************************/
void FlashTruncZero(const FFIL_T *entry, int adjust)
{

  /*-------------------------------------------------------------------*/
  /* If the file has non-zero length, erase all the bytes.             */
  /*-------------------------------------------------------------------*/
  if (entry->comm->size)
  {
    ui32 i;

#if QUOTA_ENABLED
    ui32 used = 0;

    /*-----------------------------------------------------------------*/
    /* If quotas enabled, figure out how much space will be freed.     */
    /*-----------------------------------------------------------------*/
    if (Flash->quota_enabled)
      used = ((entry->comm->size + Flash->sect_sz - 1) / Flash->sect_sz)*
             Flash->sect_sz;
#endif /* QUOTA_ENABLED */

    /*-----------------------------------------------------------------*/
    /* Truncate the file by freeing all of its sectors.                */
    /*-----------------------------------------------------------------*/
    for (i = entry->comm->frst_sect; i != FLAST_SECT;
         i = Flash->sect_tbl[i].next)
    {
      /*---------------------------------------------------------------*/
      /* First remove sector from cache.                               */
      /*---------------------------------------------------------------*/
      PfAssert(used_sect(i));
      RemoveFromCache(&Flash->cache, (int)i, -1);

      /*---------------------------------------------------------------*/
      /* Adjust used_sects and blocks table.                           */
      /*---------------------------------------------------------------*/
      --Flash->used_sects;
      --Flash->blocks[i / Flash->block_sects].used_sects;

      /*---------------------------------------------------------------*/
      /* Mark sector as dirty.                                         */
      /*---------------------------------------------------------------*/
      Flash->sect_tbl[i].prev = FDRTY_SECT;

      /*---------------------------------------------------------------*/
      /* Adjust next set of blocks to be erased.                       */
      /*---------------------------------------------------------------*/
      if (Flash->sect_tbl[i].next / Flash->block_sects !=
          i / Flash->block_sects)
        FlashAdjustEraseSet(i / Flash->block_sects);
    }

    /*-----------------------------------------------------------------*/
    /* Set frst and last sector pointers to empty for the file.        */
    /*-----------------------------------------------------------------*/
    entry->comm->last_sect = FFREE_SECT;
    entry->comm->frst_sect = FFREE_SECT;

    /*-----------------------------------------------------------------*/
    /* Invalidate curr_ptr and one past last.                          */
    /*-----------------------------------------------------------------*/
    for (i = 0; i < FOPEN_MAX; ++i)
    {
      if (Files[i].ioctl == FlashIoctl &&
          ((FFSEnt *)Files[i].handle)->entry.file.comm == entry->comm)
      {
        if (adjust)
        {
          Files[i].curr_ptr.sector = (ui16)-1;
          Files[i].curr_ptr.sect_off = 0;
          Files[i].old_ptr.sector = (ui16)-1;
          Files[i].old_ptr.sect_off = 0;
          Files[i].cached = NULL;
        }
        else
          Files[i].curr_ptr.sector = Files[i].old_ptr.sector =FDRTY_SECT;
      }
    }
    entry->comm->one_past_last.sector = (ui16)-1;
    entry->comm->size = 0;

#if QUOTA_ENABLED
    /*-----------------------------------------------------------------*/
    /* If quotas enabled, adjust quota values because of truncation.   */
    /*-----------------------------------------------------------------*/
    if (Flash->quota_enabled)
    {
      FFSEnt *ent, *root = &Flash->files_tbl->tbl[0];
      PfAssert(used);

      /*---------------------------------------------------------------*/
      /* Update used from parent to root.                              */
      /*---------------------------------------------------------------*/
      for (ent = entry->parent_dir; ent; ent = ent->entry.dir.parent_dir)
      {
        PfAssert(ent->type == FDIREN && ent->entry.dir.used >= used);
        ent->entry.dir.used -= used;
      }

      /*---------------------------------------------------------------*/
      /* Update free below.                                            */
      /*---------------------------------------------------------------*/
      FlashFreeBelow(root);

      /*---------------------------------------------------------------*/
      /* Recompute free at each node.                                  */
      /*---------------------------------------------------------------*/
      FlashFree(root, root->entry.dir.max_q - FlashVirtUsed(root));
    }
#endif /* QUOTA_ENABLED */
  }
}

/***********************************************************************/
/* FlashRelocSect: Move a used sector from its place to the free area, */
/*              updating all the file control structure on the way     */
/*                                                                     */
/*      Inputs: curr_sect = current location of used sector            */
/*              new_sect = new location of used sector                 */
/*              buffer = if not NULL, contains curr_sect data          */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int FlashRelocSect(ui32 curr_sect, ui32 new_sect, const void *buffer)
{
  int found = FALSE;
  ui32 i;
  FFSEnts *temp_table;
  FFSEnt *entry;
  FILE *stream;

  PfAssert(used_sect(curr_sect));

  /*-------------------------------------------------------------------*/
  /* Read sector from old location only if buf is null, else use buf.  */
  /*-------------------------------------------------------------------*/
  if (buffer == NULL)
  {
    if (Flash->read_sector(Flash->tmp_sect, curr_sect))
    {
      set_errno(EIO);
      return -1;
    }
  }
  else if (Flash->tmp_sect != buffer)
    memcpy(Flash->tmp_sect, buffer, Flash->sect_sz);

  /*-------------------------------------------------------------------*/
  /* Since we're just moving a used sector, don't update total number  */
  /* of used, update rather only used block counts for the sectors.    */
  /*-------------------------------------------------------------------*/
  --Flash->blocks[curr_sect / Flash->block_sects].used_sects;
  ++Flash->blocks[new_sect / Flash->block_sects].used_sects;

  /*-------------------------------------------------------------------*/
  /* Increment the Flash->free_sect if needed.                         */
  /*-------------------------------------------------------------------*/
  if (Flash->free_sect == new_sect)
  {
    if (Flash->free_sect != Flash->last_free_sect)
    {
      Flash->free_sect = Flash->sect_tbl[Flash->free_sect].next;
      --Flash->free_sects;
    }
    else
    {
      PfAssert(FALSE); /*lint !e506, !e774*/
      return -1;
    }
    PfAssert(Flash->sect_tbl[Flash->free_sect].prev == FFREE_SECT);
  }

  /*-------------------------------------------------------------------*/
  /* Set prev and next for the new value of curr_sect.                 */
  /*-------------------------------------------------------------------*/
  Flash->sect_tbl[new_sect].prev = Flash->sect_tbl[curr_sect].prev;
  Flash->sect_tbl[new_sect].next = Flash->sect_tbl[curr_sect].next;
  if (Flash->sect_tbl[curr_sect].prev != FLAST_SECT)
    Flash->sect_tbl[Flash->sect_tbl[curr_sect].prev].next=(ui16)new_sect;
  if (Flash->sect_tbl[curr_sect].next != FLAST_SECT)
    Flash->sect_tbl[Flash->sect_tbl[curr_sect].next].prev=(ui16)new_sect;

  /*-------------------------------------------------------------------*/
  /* If curr_sect is either first or last, find and update file entry  */
  /* for it.                                                           */
  /*-------------------------------------------------------------------*/
  if (Flash->sect_tbl[curr_sect].prev == FLAST_SECT ||
      Flash->sect_tbl[curr_sect].next == FLAST_SECT)
  {
    /*-----------------------------------------------------------------*/
    /* Look for entry in all files tables.                             */
    /*-----------------------------------------------------------------*/
    found = FALSE;
    for (temp_table = Flash->files_tbl; temp_table && !found;
         temp_table = temp_table->next_tbl)
    {
      entry = &temp_table->tbl[0];
      for (i = FNUM_ENT; i && !found; --i, ++entry)
      {
        /*-------------------------------------------------------------*/
        /* If file entry in table, check if either first or last       */
        /* sectors for entry are equal to curr_sect.                   */
        /*-------------------------------------------------------------*/
        if (entry->type == FCOMMN)
        {
          /*-----------------------------------------------------------*/
          /* If it's first sector, set first sector in entry.          */
          /*-----------------------------------------------------------*/
          if (entry->entry.comm.frst_sect == curr_sect)
          {
            found = TRUE;
            entry->entry.comm.frst_sect = (ui16)new_sect;
          }

          /*-----------------------------------------------------------*/
          /* If it's last sector, set last and one_past_last.          */
          /*-----------------------------------------------------------*/
          if (entry->entry.comm.last_sect == curr_sect)
          {
            found = TRUE;
            entry->entry.comm.last_sect = (ui16)new_sect;

            /*---------------------------------------------------------*/
            /* Set one past last only if valid.                        */
            /*---------------------------------------------------------*/
            if (entry->entry.comm.one_past_last.sector != (ui16)-1)
              entry->entry.comm.one_past_last.sector = (ui16)new_sect;
          }
        }
      }
    }
    PfAssert(found);
  }

  /*-------------------------------------------------------------------*/
  /* Look if curr_ptr, old_ptr needs to be modified for open files.    */
  /*-------------------------------------------------------------------*/
  stream = &Files[0];
  for (i = FOPEN_MAX; i; --i, ++stream)
  {
    /*-----------------------------------------------------------------*/
    /* Check if file is open and belongs to the flash file system.     */
    /*-----------------------------------------------------------------*/
    if (stream->ioctl == FlashIoctl && stream->volume == Flash)
    {
      /*---------------------------------------------------------------*/
      /* If curr_ptr needs to be changed, change it.                   */
      /*---------------------------------------------------------------*/
      if (stream->curr_ptr.sector == (ui16)curr_sect)
      {
        stream->curr_ptr.sector = (ui16)new_sect;

        /*-------------------------------------------------------------*/
        /* If the cache entry is valid, null it.                       */
        /*-------------------------------------------------------------*/
        if (stream->cached)
        {
#if FFS_CACHE_WRITE_THROUGH
          stream->cached->dirty = CLEAN;
#endif
          FreeSector(&stream->cached, &Flash->cache);
        }
      }

      /*---------------------------------------------------------------*/
      /* If old_ptr needs to be changed, change it.                    */
      /*---------------------------------------------------------------*/
      if (stream->old_ptr.sector == (ui16)curr_sect)
        stream->old_ptr.sector = (ui16)new_sect;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Set curr_sect to dirty in sectors table.                          */
  /*-------------------------------------------------------------------*/
  Flash->sect_tbl[curr_sect].prev = FDRTY_SECT;

  /*-------------------------------------------------------------------*/
  /* If in cache, update with new sector.                              */
  /*-------------------------------------------------------------------*/
  UpdateCacheSectNum(&Flash->cache, (int)curr_sect, (int)new_sect);

  /*-------------------------------------------------------------------*/
  /* Copy sector into new location.                                    */
  /*-------------------------------------------------------------------*/
  if (Flash->write_sector(Flash->tmp_sect, new_sect, DATA_SECT))
    return -1;

  return 0;
}

/***********************************************************************/
/* FlashWrCtrl: Write all the control information to flash             */
/*                                                                     */
/*       Input: adjust_erase = flag to adjust erase set                */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int FlashWrCtrl(int adjust_erase)
{
  ui8  byte;
  ui16 sector, last_in_segment, curr_sect, next_sect;
  ui32 sig_bytes, offset;
  ui32 i, j, ctrl_num_shorts, prev_sect = (ui32)-1;
  ui32 candidate_block, frst_ctrl_sect = Flash->frst_ctrl_sect;
  uint temp_count;
  int  status;
  FFSEnts *tbl;
  FFSEnt *v_ent;
  ui32 v_ui32;
  ui16 v_ui16;

#if TIMING
clock_t sample = clock();
#endif

#if FFS_DEBUG
int control_sectors = 0;
#endif
  /*-------------------------------------------------------------------*/
  /* Set flag if volume is full.                                       */
  /*-------------------------------------------------------------------*/
  Flash->files_tbl->tbl[1].entry.comm.one_past_last.offset =
                                                     (ui16)!FlashRoom(0);

  /*-------------------------------------------------------------------*/
  /* If number of bad blocks exceeds maximum, error.                   */
  /*-------------------------------------------------------------------*/
  if (Flash->bad_blocks > Flash->max_bad_blocks)
  {
    set_errno(EIO);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* If it's not the first time ctrl info is written to FLASH, set the */
  /* sectors that contain the old info to dirty.                       */
  /*-------------------------------------------------------------------*/
  if (frst_ctrl_sect != (ui32)-1)
  {
#if INC_NOR_FS
    /*-----------------------------------------------------------------*/
    /* For NOR, mark slow mount in old control.                        */
    /*-----------------------------------------------------------------*/
    if (Flash->type == FFS_NOR && Flash->fast_mount)
    {
      /*---------------------------------------------------------------*/
      /* Release exclusive access to the flash file system.            */
      /*---------------------------------------------------------------*/
      FlashGlob *vol = Flash;
      semPost(FlashSem);

      /*---------------------------------------------------------------*/
      /* Mark slow mount.                                              */
      /*---------------------------------------------------------------*/
      status = vol->driver.nor.write_byte(vol->mem_base +
                   (vol->last_ctrl_sect + 1) * vol->sect_sz - 1,
                   SLOW_MOUNT, vol->vol);

      /*---------------------------------------------------------------*/
      /* Acquire exclusive access to the flash file system.            */
      /*---------------------------------------------------------------*/
      semPend(FlashSem, WAIT_FOREVER);
      Flash = vol;

      /*---------------------------------------------------------------*/
      /* If error writing the byte, return error.                      */
      /*---------------------------------------------------------------*/
      if (status)
      {
        set_errno(EIO);
        return -1;
      }
    }
#endif

    /*-----------------------------------------------------------------*/
    /* Invalidate a block at a time.                                   */
    /*-----------------------------------------------------------------*/
    for (j = frst_ctrl_sect; j != FLAST_SECT;)
    {
      /*---------------------------------------------------------------*/
      /* Invalidate control block.                                     */
      /*---------------------------------------------------------------*/
      do
      {
        Flash->sect_tbl[i = j].prev = FDRTY_SECT;
        j = Flash->sect_tbl[j].next;
      } while (j != FLAST_SECT && (i + 1) % Flash->block_sects);

      /*---------------------------------------------------------------*/
      /* Adjust next erase set.                                        */
      /*---------------------------------------------------------------*/
      candidate_block = i / Flash->block_sects;
      if (Flash->free_ctrl_sect / Flash->block_sects != candidate_block)
      {
        Flash->blocks[candidate_block].ctrl_block = FALSE;
        if (adjust_erase == ADJUST_ERASE)
          FlashAdjustEraseSet(candidate_block);
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* Recount the control information and sector array size.            */
  /*-------------------------------------------------------------------*/
  ctrl_num_shorts = count_ctrl_size(adjust_erase, CHECK_RECYCLE);

  /*-------------------------------------------------------------------*/
  /* If no place to write ctrl sectors, find room.                     */
  /*-------------------------------------------------------------------*/
  if (Flash->free_ctrl_sect == (ui32)-1)
  {
    /*-----------------------------------------------------------------*/
    /* Choose block with lowest wear count from free list.             */
    /*-----------------------------------------------------------------*/
    Flash->free_ctrl_sect = choose_free_block(adjust_erase);
    if (Flash->free_ctrl_sect == (ui32)-1)
    {
      set_errno(ENOSPC);
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Set the control block flag.                                     */
    /*-----------------------------------------------------------------*/
    Flash->blocks[Flash->free_ctrl_sect /
                  Flash->block_sects].ctrl_block = TRUE;
  }
  Flash->ctrl_sect = Flash->frst_ctrl_sect = Flash->free_ctrl_sect;
  Flash->sect_tbl[Flash->frst_ctrl_sect].prev = FLAST_SECT;

  /*-------------------------------------------------------------------*/
  /* Reserve enough sectors to write control information.              */
  /*-------------------------------------------------------------------*/
  for (j = 1; j <= Flash->ctrl_sects; ++j)
  {
    /*-----------------------------------------------------------------*/
    /* If no place to write control sectors, find room.                */
    /*-----------------------------------------------------------------*/
    if (Flash->free_ctrl_sect == (ui32)-1)
    {
      /*---------------------------------------------------------------*/
      /* Choose block with lowest wear count from free list.           */
      /*---------------------------------------------------------------*/
      Flash->free_ctrl_sect = choose_free_block(adjust_erase);
      if (Flash->free_ctrl_sect == (ui32)-1)
      {
        set_errno(ENOSPC);
        return -1;
      }

      /*---------------------------------------------------------------*/
      /* Set the control block flag.                                   */
      /*---------------------------------------------------------------*/
      Flash->blocks[Flash->free_ctrl_sect /
                    Flash->block_sects].ctrl_block = TRUE;
    }

    /*-----------------------------------------------------------------*/
    /* Set prev pointer for every sector but the first.                */
    /*-----------------------------------------------------------------*/
    if (Flash->free_ctrl_sect != Flash->frst_ctrl_sect)
      Flash->sect_tbl[Flash->free_ctrl_sect].prev = (ui16)prev_sect;

    /*-----------------------------------------------------------------*/
    /* If it's last sector, remember & set its next ptr to FLAST_SECT. */
    /*-----------------------------------------------------------------*/
    if (j == Flash->ctrl_sects)
    {
      Flash->last_ctrl_sect = Flash->free_ctrl_sect;
      Flash->sect_tbl[Flash->free_ctrl_sect].next = FLAST_SECT;
    }
    if (prev_sect != (ui32)-1)
      Flash->sect_tbl[prev_sect].next = (ui16)Flash->free_ctrl_sect;

    /*-----------------------------------------------------------------*/
    /* Advance to next free.                                           */
    /*-----------------------------------------------------------------*/
    prev_sect = Flash->free_ctrl_sect++;
    if (Flash->free_ctrl_sect % Flash->block_sects == 0)
      Flash->free_ctrl_sect = (ui32)-1;
  }

#if TIMING
PreWrCtrl += (clock() - sample);
#endif

#if FFS_DEBUG
{
  ui32 free_sects = 0, free_sect;

  for (free_sect = Flash->free_sect; free_sect != FLAST_SECT;
       free_sect = Flash->sect_tbl[free_sect].next, ++free_sects)
    if ((free_sect + 1) % Flash->block_sects == Flash->hdr_sects)
    {
      PfAssert(Flash->sect_tbl[free_sect].next == FLAST_SECT ||
               Flash->sect_tbl[free_sect].next % Flash->block_sects ==
               Flash->hdr_sects);
    }
  if (free_sects != Flash->free_sects)
    printf("WrCtrl: actual = %u | thought = %u FREE sectors mismatch\n",
           free_sects, Flash->free_sects);
  printf("WrCtrl: free_sects = %u | frst_free = %u, last_free = %u\n",
         Flash->free_sects, Flash->free_sect, Flash->last_free_sect);
  printf("WrCtrl:   = %u, CTRL_SECTOR = %u\n",
         Flash->seq_num + 1, Flash->ctrl_sect);
}
#endif
  /*-------------------------------------------------------------------*/
  /* Set fast mount flag so that NOR can mark slow mount on data writes*/
  /*-------------------------------------------------------------------*/
  Flash->fast_mount = TRUE;

  /*-------------------------------------------------------------------*/
  /* Initialize the control write function.                            */
  /*-------------------------------------------------------------------*/
  (void)Flash->write_ctrl(Flash, 0);

  /*-------------------------------------------------------------------*/
  /* Write the ctrl size.                                              */
  /*-------------------------------------------------------------------*/
  write_ctrl_ui32(Flash->ctrl_size)

  /*-------------------------------------------------------------------*/
  /* Store the sequence number.                                        */
  /*-------------------------------------------------------------------*/
  ++Flash->seq_num;
  write_ctrl_ui32(Flash->seq_num)

  /*-------------------------------------------------------------------*/
  /* Store the ctrl info into the flash.                               */
  /*-------------------------------------------------------------------*/
  write_ctrl_ui32(Flash->total)
  write_ctrl_ui32(ctrl_num_shorts)
  write_ctrl_ui32(Flash->fileno_gen)
  write_ctrl_ui32(Flash->free_sect)
  write_ctrl_ui32(Flash->last_free_sect)
  write_ctrl_ui32(Flash->free_sects)

  /*-------------------------------------------------------------------*/
  /* Write high wear count and offset count for every block.           */
  /*-------------------------------------------------------------------*/
  write_ctrl_ui32(Flash->high_wear)
  for (i = 0; i < Flash->num_blocks; i += 2)
  {
    /*-----------------------------------------------------------------*/
    /* Figure out even block offset.                                   */
    /*-----------------------------------------------------------------*/
    offset = Flash->high_wear - Flash->blocks[i].wear_count;
    PfAssert(Flash->high_wear >= Flash->blocks[i].wear_count);
    if (offset >= 16)
      offset = 15;
    byte = (ui8)(offset << 4); /*lint !e701 */

    /*-----------------------------------------------------------------*/
    /* Figure out odd block offset.                                    */
    /*-----------------------------------------------------------------*/
    if (i + 1 < Flash->num_blocks)
    {
      offset = Flash->high_wear - Flash->blocks[i + 1].wear_count;
      PfAssert(Flash->high_wear >= Flash->blocks[i + 1].wear_count);
      if (offset >= 16)
        offset = 15;
      byte |= (ui8)offset;
    }

    /*-----------------------------------------------------------------*/
    /* Write the combined offsets.                                     */
    /*-----------------------------------------------------------------*/
    status = Flash->write_ctrl(&byte, sizeof(ui8));
    if (status)
      goto WrCtrl_exit;
  }

  /*-------------------------------------------------------------------*/
  /* Write the bad block flags for NAND only, and set signature bytes. */
  /*-------------------------------------------------------------------*/
  if (Flash->type == FFS_NAND)
  {
    for (i = 0; i < Flash->num_blocks; i += 8)
    {
      byte = 0xFF;
      for (j = 0; j < 8; ++j)
        if (i + j < Flash->num_blocks && !Flash->blocks[i + j].bad_block)
          byte &= (0xFF ^ (1 << j));
      status = Flash->write_ctrl(&byte, sizeof(ui8));
      if (status)
        goto WrCtrl_exit;
    }

    /*-----------------------------------------------------------------*/
    /* Set NAND signature bytes.                                       */
    /*-----------------------------------------------------------------*/
    sig_bytes = NAND_BYTE1 << 24 | NAND_BYTE2 << 16 | NAND_BYTE3 << 8 |
                NAND_BYTE4;
  }
  else if (Flash->type == FFS_NOR)
  {
    /*-----------------------------------------------------------------*/
    /* Set NOR signature bytes.                                        */
    /*-----------------------------------------------------------------*/
    sig_bytes = NOR_BYTE1 << 24 | NOR_BYTE2 << 16 | NOR_BYTE3 << 8 |
                NOR_BYTE4;
  }
  else
  {
    /*-----------------------------------------------------------------*/
    /* Set MLC signature bytes.                                        */
    /*-----------------------------------------------------------------*/
    sig_bytes = MLC_BYTE1 << 24 | MLC_BYTE2 << 16 | MLC_BYTE3 << 8 |
                MLC_BYTE4;
  }

  /*-------------------------------------------------------------------*/
  /* Write the signature bytes.                                        */
  /*-------------------------------------------------------------------*/
  write_ctrl_ui32(sig_bytes)

  /*-------------------------------------------------------------------*/
  /* Write the sector where next control info will be written.         */
  /*-------------------------------------------------------------------*/
  write_ctrl_ui32(Flash->free_ctrl_sect)

#if QUOTA_ENABLED
  /*-------------------------------------------------------------------*/
  /* Write the quota enabled flag for volume.                          */
  /*-------------------------------------------------------------------*/
  status = Flash->write_ctrl(&Flash->quota_enabled, sizeof(ui8));
  if (status)
    goto WrCtrl_exit;
#endif /* QUOTA_ENABLED */

  /*-------------------------------------------------------------------*/
  /* Loop through the RAM file entry tables.                           */
  /*-------------------------------------------------------------------*/
  for (tbl = Flash->files_tbl; tbl; tbl = tbl->next_tbl)
    for (i = 0; i < FNUM_ENT; ++i)
    {
      v_ent = &tbl->tbl[i];

      /*---------------------------------------------------------------*/
      /* First write entry type.                                       */
      /*---------------------------------------------------------------*/
      status = Flash->write_ctrl(&v_ent->type, sizeof(ui8));
      if (status)
        goto WrCtrl_exit;

      /*---------------------------------------------------------------*/
      /* Then parse according to entry type.                           */
      /*---------------------------------------------------------------*/
      switch (tbl->tbl[i].type)
      {
        /*-------------------------------------------------------------*/
        /* Directory entry.                                            */
        /*-------------------------------------------------------------*/
        case FDIREN:
        {
          /*-----------------------------------------------------------*/
          /* Convert pointers to offsets before writing them to flash. */
          /*-----------------------------------------------------------*/
          write_ctrl_ui32(to_offst(v_ent->entry.dir.next))
          write_ctrl_ui32(to_offst(v_ent->entry.dir.prev))
          write_ctrl_ui32(to_offst(v_ent->entry.dir.comm))
          write_ctrl_ui32(to_offst(v_ent->entry.dir.parent_dir))
          write_ctrl_ui32(to_offst(v_ent->entry.dir.first))

#if QUOTA_ENABLED
          /*-----------------------------------------------------------*/
          /* Write quota information if volume uses it.                */
          /*-----------------------------------------------------------*/
          if (Flash->quota_enabled)
          {
            write_ctrl_ui32(v_ent->entry.dir.max_q)
            write_ctrl_ui32(v_ent->entry.dir.min_q)
            write_ctrl_ui32(v_ent->entry.dir.used)
            write_ctrl_ui32(v_ent->entry.dir.free)
            write_ctrl_ui32(v_ent->entry.dir.free_below)
            write_ctrl_ui32(v_ent->entry.dir.res_below)
          }
#endif /* QUOTA_ENABLED */

          /*-----------------------------------------------------------*/
          /* Write the directory name.                                 */
          /*-----------------------------------------------------------*/
          status = Flash->write_ctrl(v_ent->entry.dir.name,
                                     strlen(v_ent->entry.dir.name) + 1);
          if (status)
            goto WrCtrl_exit;

          break;
        }

        /*-------------------------------------------------------------*/
        /* Common link/directory entry.                                */
        /*-------------------------------------------------------------*/
        case FCOMMN:
        {
          /*-----------------------------------------------------------*/
          /* Write one past last.                                      */
          /*-----------------------------------------------------------*/
          write_ctrl_ui16(v_ent->entry.comm.one_past_last.sector)
          write_ctrl_ui16(v_ent->entry.comm.one_past_last.offset)

          /*-----------------------------------------------------------*/
          /* Write the modification and access times.                  */
          /*-----------------------------------------------------------*/
          write_ctrl_ui32(v_ent->entry.comm.mod_time)
          write_ctrl_ui32(v_ent->entry.comm.ac_time)

          /*-----------------------------------------------------------*/
          /* Write the fileno.                                         */
          /*-----------------------------------------------------------*/
          write_ctrl_ui32(v_ent->entry.comm.fileno)

          /*-----------------------------------------------------------*/
          /* Write the file size.                                      */
          /*-----------------------------------------------------------*/
          write_ctrl_ui32(v_ent->entry.comm.size)

          /*-----------------------------------------------------------*/
          /* Write first and last sectors.                             */
          /*-----------------------------------------------------------*/
          write_ctrl_ui16(v_ent->entry.comm.frst_sect)
          write_ctrl_ui16(v_ent->entry.comm.last_sect)

          /*-----------------------------------------------------------*/
          /* Write the user/group ID and file creation mode.           */
          /*-----------------------------------------------------------*/
          write_ctrl_ui16(v_ent->entry.comm.user_id)
          write_ctrl_ui16(v_ent->entry.comm.group_id)
          write_ctrl_ui16(v_ent->entry.comm.mode)

          /*-----------------------------------------------------------*/
          /* Write number of links.                                    */
          /*-----------------------------------------------------------*/
          status = Flash->write_ctrl(&v_ent->entry.comm.links,
                                     sizeof(ui8));
          if (status)
            goto WrCtrl_exit;

          /*-----------------------------------------------------------*/
          /* Write the attributes.                                     */
          /*-----------------------------------------------------------*/
          write_ctrl_ui32(v_ent->entry.comm.attrib)

#if BACKWARD_COMPATIBLE
          status = Flash->write_ctrl(&byte, sizeof(ui8)); /*lint !e772*/
          if (status)
            goto WrCtrl_exit;
#endif

          break;
        }

        /*-------------------------------------------------------------*/
        /* Link entry.                                                 */
        /*-------------------------------------------------------------*/
        case FFILEN:
        {
          /*-----------------------------------------------------------*/
          /* Every time control is written, we can unset all modified  */
          /* flags because a flush just happened (for open files).     */
          /*-----------------------------------------------------------*/
          if (v_ent->entry.file.file)
            v_ent->entry.file.file->flags &= ~FCB_MOD;

          /*-----------------------------------------------------------*/
          /* Convert pointers to offsets before writing them to flash. */
          /*-----------------------------------------------------------*/
          write_ctrl_ui32(to_offst(v_ent->entry.file.next))
          write_ctrl_ui32(to_offst(v_ent->entry.file.prev))
          write_ctrl_ui32(to_offst(v_ent->entry.file.comm))
          write_ctrl_ui32(to_offst(v_ent->entry.file.parent_dir))

          /*-----------------------------------------------------------*/
          /* Write the link name.                                      */
          /*-----------------------------------------------------------*/
          status = Flash->write_ctrl(v_ent->entry.file.name,
                                     strlen(v_ent->entry.file.name) + 1);
          if (status)
            goto WrCtrl_exit;

          break;
        }

        default:
          break;
      }
    }

  /*-------------------------------------------------------------------*/
  /* The sectors table is now set to write. Start with free sectors.   */
  /*-------------------------------------------------------------------*/
  temp_count = 0;
  for (sector = Flash->free_sect;; sector = next_sect)
  {
    /*-----------------------------------------------------------------*/
    /* Determine current free block.                                   */
    /*-----------------------------------------------------------------*/
    i = sector / Flash->block_sects;
    sector = (i + 1) * Flash->block_sects - 1;

    /*-----------------------------------------------------------------*/
    /* Determine first sector in next free block.                      */
    /*-----------------------------------------------------------------*/
    next_sect = Flash->sect_tbl[sector].next;

    /*-----------------------------------------------------------------*/
    /* If no more blocks, stop and write sig for current segment.      */
    /*-----------------------------------------------------------------*/
    if (next_sect == FLAST_SECT)
    {
      write_ctrl_ui16(sector)
      write_ctrl_ui16(next_sect)
      temp_count += 2;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* Skip all bad blocks following current block.                    */
    /*-----------------------------------------------------------------*/
    for (++i; i < Flash->num_blocks && Flash->blocks[i].bad_block; ++i) ;

    /*-----------------------------------------------------------------*/
    /* If end of contiguous segment, account for it.                   */
    /*-----------------------------------------------------------------*/
    if (i != next_sect / Flash->block_sects)
    {
      write_ctrl_ui16(sector)
      write_ctrl_ui16(next_sect)
      temp_count += 2;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Write the used sectors.                                           */
  /*-------------------------------------------------------------------*/
  for (sector = 0; sector < Flash->num_sects;)
  {
    /*-----------------------------------------------------------------*/
    /* If sector is beginning of file, count whole file sector list.   */
    /*-----------------------------------------------------------------*/
    if (Flash->sect_tbl[sector].prev == FLAST_SECT && used_sect(sector))
    {
      /*---------------------------------------------------------------*/
      /* Store the head of the list.                                   */
      /*---------------------------------------------------------------*/
      write_ctrl_ui16(sector)
      ++temp_count;

      /*---------------------------------------------------------------*/
      /* Walk the list a contiguous segment at a time.                 */
      /*---------------------------------------------------------------*/
      for (curr_sect = sector; curr_sect != FLAST_SECT;)
      {
        /*-------------------------------------------------------------*/
        /* Count current contiguous segment. For NOR skip headers.     */
        /*-------------------------------------------------------------*/
        if (Flash->type == FFS_NOR)
          for (last_in_segment = curr_sect;; last_in_segment = next_sect)
          {
            next_sect = last_in_segment + 1;
            if (next_sect < Flash->num_sects &&
                Flash->sect_tbl[next_sect].prev == FHDER_SECT)
              next_sect += Flash->hdr_sects;
            if (Flash->sect_tbl[last_in_segment].next != next_sect)
              break;
          }
        else
          for (last_in_segment = curr_sect;
            Flash->sect_tbl[last_in_segment].next == last_in_segment + 1;
            ++last_in_segment) ;

        /*-------------------------------------------------------------*/
        /* If segment contains more than 1 sector, write label to tell */
        /* it's multi sector segment and last sector in segment.       */
        /*-------------------------------------------------------------*/
        if (curr_sect != last_in_segment)
        {
          next_sect = FCONT_SGMT;
          write_ctrl_ui16(next_sect)
          write_ctrl_ui16(last_in_segment)
          temp_count += 2;
        }

        /*-------------------------------------------------------------*/
        /* Write next pointer.                                         */
        /*-------------------------------------------------------------*/
        next_sect = Flash->sect_tbl[last_in_segment].next;
        write_ctrl_ui16(next_sect)
        temp_count += 1;

        /*-------------------------------------------------------------*/
        /* Increment sector counter if first segment in file list.     */
        /*-------------------------------------------------------------*/
        if (curr_sect == sector)
          sector = last_in_segment + 1;

        /*-------------------------------------------------------------*/
        /* Move to new segment.                                        */
        /*-------------------------------------------------------------*/
        curr_sect = next_sect;
      }
    }

    /*-----------------------------------------------------------------*/
    /* Else move to next sector to look at.                            */
    /*-----------------------------------------------------------------*/
    else
      ++sector;
  }

  /*------------------------------------------------------------------*/
  /* Write the end of table marking (FLAST_SECT).                      */
  /*-------------------------------------------------------------------*/
  sector = FLAST_SECT;
  write_ctrl_ui16(sector)
  ++temp_count;
  (void)Flash->write_ctrl(NULL, 0);

  /*-------------------------------------------------------------------*/
  /* Make sure we haven't written more than we were supposed to.       */
  /*-------------------------------------------------------------------*/
  PfAssert(temp_count <= ctrl_num_shorts);

  /*-------------------------------------------------------------------*/
  /* If NOR, mark this control information in the block header.        */
  /*-------------------------------------------------------------------*/
  if (Flash->type == FFS_NOR)
  {
    if (mark_ctrl_info(Flash->frst_ctrl_sect))
      return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Else if previous control information was valid, invalidate it.    */
  /*-------------------------------------------------------------------*/
  else if (frst_ctrl_sect != (ui32)-1)
  {
#if INC_NAND_FS || INC_MLC_FS
    sector = (ui16)frst_ctrl_sect;
    do
    {
#if INC_NAND_FS
      if (Flash->type == FFS_NAND)
        status = FsNandWriteType(sector, OLD_CTRL_SECT);
#endif
      if (status)
        return status;
      sector = Flash->sect_tbl[sector].next;
    }
    while (sector != FLAST_SECT);
#endif
  }

  /*-------------------------------------------------------------------*/
  /* No more need to keep track of a sync.                             */
  /*-------------------------------------------------------------------*/
  Flash->sync_deferred = FALSE;

  /*-------------------------------------------------------------------*/
  /* Set the wr_ctrl flag.                                             */
  /*-------------------------------------------------------------------*/
  Flash->wr_ctrl = TRUE;

#if TIMING
TotWrCtrl += (clock() - sample);
++WrCtrlCount;
#endif

  return 0;

  /*-------------------------------------------------------------------*/
  /* Exit when control rewritten due to bad block / program disturb.   */
  /*-------------------------------------------------------------------*/
WrCtrl_exit:
  if (status == -1)
    return -1;
  return 0;
}

/***********************************************************************/
/* FlashWriteCtrlSect: Write a whole CtrlSector worth of control info  */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure, RWR_CTRL_OK (NAND only)   */
/*              in case whole ctrl was rewritten because of bad block  */
/*                                                                     */
/***********************************************************************/
int FlashWriteCtrlSect(void)
{
  return Flash->write_sector(Flash->tmp_sect, Flash->ctrl_sect,
                             CTRL_SECT);
}
#endif /* NUM_FFS_VOLS */

