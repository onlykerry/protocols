/***********************************************************************/
/*                                                                     */
/*   Module:  fsinit.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2004.8                                                   */
/*   Purpose: Implements the initialization functions for the flash    */
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
#include <stdarg.h>
#include "flashfsp.h"

#if NUM_FFS_VOLS

/***********************************************************************/
/* Symbol Defintions                                                   */
/***********************************************************************/
#define FFS_MAX_SIZE   0xFFFB

/***********************************************************************/
/* Global Variable Declarations                                        */
/***********************************************************************/
FlashGlob  FlashGlobals[NUM_FFS_VOLS];
SEM        FlashSem;
FlashGlob *Flash;

/***********************************************************************/
/* Local Function Definitions                                          */
/***********************************************************************/

/***********************************************************************/
/* assign_volume_mem: Allocate memory for internal structures          */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int assign_volume_mem(void)
{
  int sect_sz;

  /*-------------------------------------------------------------------*/
  /* Allocate memory for the sectors table.                            */
  /*-------------------------------------------------------------------*/
  Flash->sect_tbl = malloc(sizeof(Sectors) * Flash->num_sects);
  if (Flash->sect_tbl == NULL)
  {
    set_errno(ENOSPC);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Allocate memory for the used counters table.                      */
  /*-------------------------------------------------------------------*/
  Flash->blocks = malloc(sizeof(FFSBlock) * Flash->num_blocks);
  if (Flash->blocks == NULL)
  {
    set_errno(ENOSPC);
    free(Flash->sect_tbl);
    Flash->sect_tbl = NULL;
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Allocate memory for the set table.                                */
  /*-------------------------------------------------------------------*/
  Flash->erase_set = malloc(sizeof(int) * Flash->max_set_blocks);
  if (Flash->erase_set == NULL)
  {
    free(Flash->sect_tbl);
    Flash->sect_tbl = NULL;
    free(Flash->blocks);
    Flash->blocks = NULL;
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Set the cache.                                                    */
  /*-------------------------------------------------------------------*/
  sect_sz = Flash->sect_sz;
  PfAssert(sect_sz % 2 == 0);
  if (!InitCache(&Flash->cache, FOPEN_MAX, FlashWrprWr,
                 Flash->read_sector, sect_sz, 1, CACHE_UI32_ALLIGN))
  {
    free(Flash->sect_tbl);
    Flash->sect_tbl = NULL;
    free(Flash->blocks);
    Flash->blocks = NULL;
    free(Flash->erase_set);
    Flash->erase_set = NULL;
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Assign pointer to extra sector in cache for use by the internals  */
  /* of the file system.                                               */
  /*-------------------------------------------------------------------*/
  Flash->tmp_sect = (ui8 *)(Flash->cache.pool[FOPEN_MAX - 1].sector +
                            Flash->sect_sz);

  return 0;
}

/***********************************************************************/
/* init_volume: Prepare unformatted volume for format                  */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int init_volume(void)
{
  int i;
  FFSEnt *v_ent;

  /*-------------------------------------------------------------------*/
  /* Assign volume memory.                                             */
  /*-------------------------------------------------------------------*/
  if (assign_volume_mem())
    return -1;

  /*-------------------------------------------------------------------*/
  /* Set ctrl info pointer to invalid and the seq num to 0.            */
  /*-------------------------------------------------------------------*/
  Flash->frst_ctrl_sect = Flash->last_ctrl_sect = (ui32)-1;
  Flash->seq_num = 0;

  /*-------------------------------------------------------------------*/
  /* Set the file number generator.                                    */
  /*-------------------------------------------------------------------*/
  Flash->fileno_gen = 1;

  /*-------------------------------------------------------------------*/
  /* Set the high wear count.                                          */
  /*-------------------------------------------------------------------*/
  Flash->high_wear = 0;

  /*-------------------------------------------------------------------*/
  /* Allocate the first table, the files table.                        */
  /*-------------------------------------------------------------------*/
  Flash->files_tbl = FlashGetTbl();
  if (!Flash->files_tbl)
  {
    free(Flash->sect_tbl);
    Flash->sect_tbl = NULL;
    free(Flash->blocks);
    Flash->blocks = NULL;
    free(Flash->erase_set);
    Flash->erase_set = NULL;
    DestroyCache(&Flash->cache);
    return -1;
  }
  Flash->files_tbl->free = FNUM_ENT - 2;

  /*-------------------------------------------------------------------*/
  /* Set all entries in files table to empty except for the first two. */
  /*-------------------------------------------------------------------*/
  for (i = 2; i < FNUM_ENT; ++i)
    Flash->files_tbl->tbl[i].type = FEMPTY;

  /*-------------------------------------------------------------------*/
  /* Get pointer to the second entry in the files table.               */
  /*-------------------------------------------------------------------*/
  v_ent = &Flash->files_tbl->tbl[1];

  /*-------------------------------------------------------------------*/
  /* Set second entry in files table to be the file struct associated  */
  /* with the root directory.                                          */
  /*-------------------------------------------------------------------*/
  v_ent->type = FCOMMN;
  v_ent->entry.comm.open_mode = 0;
  v_ent->entry.comm.links = 1;
  v_ent->entry.comm.open_links = 0;
  v_ent->entry.comm.attrib = 0;
  v_ent->entry.comm.fileno = 0;
  v_ent->entry.comm.frst_sect = (ui16)-1;
  v_ent->entry.comm.last_sect = (ui16)-1;
  v_ent->entry.comm.mod_time = v_ent->entry.comm.ac_time = OsSecCount;
  v_ent->entry.comm.one_past_last.offset = 0;
  v_ent->entry.comm.one_past_last.sector = (ui16)-1;
  v_ent->entry.comm.addr = v_ent;
  v_ent->entry.comm.size = 0;

  /*-------------------------------------------------------------------*/
  /* Get pointer to the first entry in the files table.                */
  /*-------------------------------------------------------------------*/
  v_ent = &Flash->files_tbl->tbl[0];

  /*-------------------------------------------------------------------*/
  /* Set the first entry in files table to be root dir.                */
  /*-------------------------------------------------------------------*/
  v_ent->type = FDIREN;
  v_ent->entry.dir.cwds = 0;
  v_ent->entry.dir.next = NULL;
  v_ent->entry.dir.prev = NULL;
  v_ent->entry.dir.comm = &(Flash->files_tbl->tbl[1].entry.comm);
  v_ent->entry.dir.parent_dir = NULL;
  v_ent->entry.dir.first = NULL;
  strcpy(v_ent->entry.dir.name, Flash->sys.name);

  /*-------------------------------------------------------------------*/
  /* Set the permissions for the root directory.                       */
  /*-------------------------------------------------------------------*/
  SetPerm(v_ent->entry.dir.comm, S_IROTH | S_IWOTH | S_IXOTH);

  /*-------------------------------------------------------------------*/
  /* Set the other variables for this volume.                          */
  /*-------------------------------------------------------------------*/
  Flash->total_free = FNUM_ENT - 2;
  Flash->total = FNUM_ENT;
  Flash->ctrl_size = Flash->ctrl_sects = 0;

  /*-------------------------------------------------------------------*/
  /* Update file tables size.                                          */
  /*-------------------------------------------------------------------*/
  Flash->tbls_size = (OFDIR_SZ + OFCOM_SZ + 1 +
                      strlen(Flash->files_tbl->tbl[0].entry.dir.name));

#if QUOTA_ENABLED
  /*-------------------------------------------------------------------*/
  /* Set the quotas value for the root directory if quota enabled.     */
  /*-------------------------------------------------------------------*/
  if (Flash->quota_enabled)
  {
    /*-----------------------------------------------------------------*/
    /* max_q is set based on the type of file system. The value is     */
    /* based on the total number of sectors minus the space that has   */
    /* to be free for recycles and worst encoding of sector table.     */
    /*-----------------------------------------------------------------*/
    if (Flash->type == FFS_NAND)
      v_ent->entry.dir.max_q = Flash->num_sects -
        Flash->block_sects * (Flash->max_bad_blocks + 2 +
                              (Flash->max_set_blocks + 1) / 2) -
        ((2 * (1 + CONSEC_DISTURBS) * (Flash->num_sects -
          (Flash->max_bad_blocks + 2) * Flash->block_sects) +
          Flash->sect_sz - 1) / Flash->sect_sz);
    else
      v_ent->entry.dir.max_q = Flash->num_sects -
        Flash->block_sects * (2 + (Flash->max_set_blocks + 1) / 2) -
        ((2 * (Flash->num_sects - 2 * Flash->block_sects) +
         Flash->sect_sz - 1) / Flash->sect_sz);
    v_ent->entry.dir.max_q -= EXTRA_FREE / Flash->sect_sz;
    v_ent->entry.dir.max_q *= Flash->sect_sz;

    /*-----------------------------------------------------------------*/
    /* Set the rest of the quota values.                               */
    /*-----------------------------------------------------------------*/
    v_ent->entry.dir.min_q = 0;
    v_ent->entry.dir.free_below = 0;
    v_ent->entry.dir.res_below = 0;
    v_ent->entry.dir.used = strlen(v_ent->entry.dir.name) + 1 + OFDIR_SZ
                          + OFDIR_QUOTA_SZ + OFCOM_SZ;
    v_ent->entry.dir.free = v_ent->entry.dir.max_q -
                            v_ent->entry.dir.used;
    Flash->tbls_size += OFDIR_QUOTA_SZ;
  }
#endif /* QUOTA_ENABLED */

  return 0;
}

/***********************************************************************/
/* prepare_mounted_for_format: Prepare a mounted volume for format     */
/*                                                                     */
/*       Input: vol = pointer to flash volume                          */
/*                                                                     */
/*     Returns: 0 on success, 1 on error                               */
/*                                                                     */
/***********************************************************************/
static int prepare_mounted_for_format(FlashGlob *vol)
{
  FFSEnts *temp_tbl, *prev_tbl;
  ui32     i, sect, block, dummy;
  FFSEnt *curr_dir;
  int sect_sz;

  /*-------------------------------------------------------------------*/
  /* Step through all files, invalidating those associated with volume.*/
  /*-------------------------------------------------------------------*/
  for (i = 0; i < FOPEN_MAX; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* If a FILE is associated with this volume, clear it.             */
    /*-----------------------------------------------------------------*/
    if (Files[i].volume == vol)
    {
      Files[i].ioctl = NULL;
      Files[i].volume = NULL;
      Files[i].handle = Files[i].cached = NULL;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to volume and flash file system.         */
  /*-------------------------------------------------------------------*/
  semPend(vol->sem, WAIT_FOREVER);
  semPend(FlashSem, WAIT_FOREVER);
  Flash = vol;

  /*-------------------------------------------------------------------*/
  /* For mounted volumes clear out all the internal structures. Start  */
  /* with the cache.                                                   */
  /*-------------------------------------------------------------------*/
  sect_sz = (int)Flash->sect_sz;
  ReinitCache(&Flash->cache, sect_sz);

  /*-------------------------------------------------------------------*/
  /* Clear it's first files table, and if there are any left free them.*/
  /*-------------------------------------------------------------------*/
  FsReadCWD(&dummy, (void *)&curr_dir);
  for (i = 2; i < FNUM_ENT; ++i)
  {
    Flash->files_tbl->tbl[i].type = FEMPTY;

    /*-----------------------------------------------------------------*/
    /* If the current directory is this entry, set it to first entry   */
    /* in the table, the root directory.                               */
    /*-----------------------------------------------------------------*/
    if (curr_dir == &Flash->files_tbl->tbl[i])
    {
      ++Flash->files_tbl->tbl[0].entry.dir.cwds;
      FsSaveCWD(dummy, (ui32)&Flash->files_tbl->tbl[0]);
    }
  }
  Flash->files_tbl->free = FNUM_ENT - 2;

  /*-------------------------------------------------------------------*/
  /* Set the root dir's first entry to NULL.                           */
  /*-------------------------------------------------------------------*/
  Flash->files_tbl->tbl[0].entry.dir.first = NULL;

  /*-------------------------------------------------------------------*/
  /* Free all tables except for the first one.                         */
  /*-------------------------------------------------------------------*/
  for (temp_tbl = Flash->files_tbl->next_tbl; temp_tbl; )
  {
    /*-----------------------------------------------------------------*/
    /* If the current directory is in this table set it to first entry */
    /* for this volume, the root directory.                            */
    /*-----------------------------------------------------------------*/
    if (curr_dir >= &temp_tbl->tbl[0] &&
        curr_dir < &temp_tbl->tbl[FNUM_ENT])
    {
      ++Flash->files_tbl->tbl[0].entry.dir.cwds;
      FsSaveCWD(dummy, (ui32)&Flash->files_tbl->tbl[0]);
    }

    prev_tbl = temp_tbl;
    temp_tbl = temp_tbl->next_tbl;
    free(prev_tbl);
  }
  Flash->files_tbl->next_tbl = NULL;

  /*-------------------------------------------------------------------*/
  /* Update file tables size.                                          */
  /*-------------------------------------------------------------------*/
  Flash->tbls_size = (OFDIR_SZ + OFCOM_SZ + 1 +
                      strlen(Flash->files_tbl->tbl[0].entry.dir.name));
#if QUOTA_ENABLED
  if (Flash->quota_enabled)
    Flash->tbls_size += OFDIR_QUOTA_SZ;
#endif /* QUOTA_ENABLED */

  /*-------------------------------------------------------------------*/
  /* Set sect_tbl so that all valid blocks contain free sects.         */
  /*-------------------------------------------------------------------*/
  for (block = 0; block < Flash->num_blocks; ++block)
  {
    /*-----------------------------------------------------------------*/
    /* If valid block, mark all sectors as free.                       */
    /*-----------------------------------------------------------------*/
    if (!Flash->blocks[block].bad_block)
    {
      /*---------------------------------------------------------------*/
      /* Get first sector in block and link it with last sector in     */
      /* previous block, if such sector exists.                        */
      /*---------------------------------------------------------------*/
      sect = block * Flash->block_sects + Flash->hdr_sects;

      /*---------------------------------------------------------------*/
      /* Since all data sects in block will be free, clear used count. */
      /*---------------------------------------------------------------*/
      Flash->blocks[block].used_sects = 0;
      Flash->blocks[block].ctrl_block = FALSE;
      for (i = Flash->hdr_sects; i < Flash->block_sects; ++i, ++sect)
      {
        /*-------------------------------------------------------------*/
        /* If sector not free, mark it as dirty.                       */
        /*-------------------------------------------------------------*/
        if (Flash->sect_tbl[sect].prev != FFREE_SECT)
          Flash->sect_tbl[sect].prev = FDRTY_SECT;
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* There are no more used or ctrl sectors.                           */
  /*-------------------------------------------------------------------*/
  Flash->used_sects = 0;
  Flash->frst_ctrl_sect = Flash->last_ctrl_sect = (ui32)-1;
  Flash->free_ctrl_sect = (ui32)-1;
  Flash->ctrl_sects = Flash->ctrl_size = 0;

  /*-------------------------------------------------------------------*/
  /* Set all other variables related to volume.                        */
  /*-------------------------------------------------------------------*/
  Flash->total_free = FNUM_ENT - 2;
  Flash->total = FNUM_ENT;

#if QUOTA_ENABLED
  /*-------------------------------------------------------------------*/
  /* Reset root quota values if volume uses quotas.                    */
  /*-------------------------------------------------------------------*/
  if (Flash->quota_enabled)
  {
    FDIR_T *dir_t = &Flash->files_tbl->tbl[0].entry.dir;
    dir_t->min_q = 0;
    dir_t->free_below = 0;
    dir_t->res_below = 0;
    dir_t->used = strlen(dir_t->name) + 1 + OFDIR_SZ + OFDIR_QUOTA_SZ +
                  OFCOM_SZ;
    dir_t->free = dir_t->max_q - dir_t->used;
  }
#endif /* QUOTA_ENABLED */

  /*-------------------------------------------------------------------*/
  /* Do a sync now to save the state of the system.                    */
  /*-------------------------------------------------------------------*/
  if (FlashSync(DO_SYNC) != NULL)
  {
    semPost(FlashSem);
    semPost(vol->sem);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Erase all sectors that need erasing.                              */
  /*-------------------------------------------------------------------*/
  if (FlashClean())
  {
    semPost(FlashSem);
    semPost(vol->sem);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Release access to flash file system and this volume.              */
  /*-------------------------------------------------------------------*/
  semPost(FlashSem);
  semPost(vol->sem);
  return 0;
}

/***********************************************************************/
/* next_free_block: Chooses the smallest wear count block              */
/*                                                                     */
/*     Returns: Smallest block, or -1 if no such block exists          */
/*                                                                     */
/***********************************************************************/
static int next_free_block(void)
{
  int block, nxt_free_blck = -1;

  /*-------------------------------------------------------------------*/
  /* Look through all blocks for one with smallest wear count.         */
  /*-------------------------------------------------------------------*/
  for (block = 0; block < Flash->num_blocks; ++block)
    if (!(Flash->blocks[block].bad_block ||
          Flash->blocks[block].ctrl_block))
    {
      if (nxt_free_blck == -1 || Flash->blocks[block].wear_count <
          Flash->blocks[nxt_free_blck].wear_count)
        nxt_free_blck = block;
    }

  /*-------------------------------------------------------------------*/
  /* If such a block found, temporarily mark its ctrl_block so it's    */
  /* not counted next time around.                                     */
  /*-------------------------------------------------------------------*/
  if (nxt_free_blck != -1)
    Flash->blocks[nxt_free_blck].ctrl_block = TRUE;

  return nxt_free_blck;
}

/***********************************************************************/
/* set_first_free: Readjust free list and clean blocks that held old   */
/*              ctrl info                                              */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int set_first_free(void)
{
  int old_1st_ctrl_block = -1, old_2nd_ctrl_block = -1;
  int frst_free_block = -1, block, status;
  ui32 addr, hdr, sect, i;
  FlashGlob *vol;

  /*-------------------------------------------------------------------*/
  /* Find the block with lowest wear count that has no ctrl info.      */
  /*-------------------------------------------------------------------*/
  for (block = 0; block < Flash->num_blocks; ++block)
    if (!(Flash->blocks[block].bad_block ||
          Flash->blocks[block].ctrl_block))
    {
      if (frst_free_block == -1 || Flash->blocks[block].wear_count <
          Flash->blocks[frst_free_block].wear_count)
        frst_free_block = block;
    }

  /*-------------------------------------------------------------------*/
  /* Mark all dirty/ctrl sectors as free.                              */
  /*-------------------------------------------------------------------*/
  for (block = 0; block < Flash->num_blocks; ++block)
  {
    if (!Flash->blocks[block].bad_block)
    {
      /*---------------------------------------------------------------*/
      /* If this is a control block, remember it.                      */
      /*---------------------------------------------------------------*/
      if (Flash->blocks[block].ctrl_block)
      {
        Flash->blocks[block].ctrl_block = FALSE;
        if (old_1st_ctrl_block == -1)
          old_1st_ctrl_block = block;
        else
        {
          PfAssert(old_2nd_ctrl_block == -1);
          old_2nd_ctrl_block = block;
        }
      }

      /*---------------------------------------------------------------*/
      /* Bump up the wear count.                                       */
      /*---------------------------------------------------------------*/
      Flash->blocks[block].wear_count += 1;
      if (Flash->high_wear < Flash->blocks[block].wear_count)
        Flash->high_wear = Flash->blocks[block].wear_count;

      /*---------------------------------------------------------------*/
      /* Make sure all sectors are free.                               */
      /*---------------------------------------------------------------*/
      sect = block * Flash->block_sects + Flash->hdr_sects;
      for (i = Flash->hdr_sects; i < Flash->block_sects; ++i, ++sect)
      {
        if (Flash->sect_tbl[sect].prev != FFREE_SECT)
        {
          Flash->sect_tbl[sect].prev = FFREE_SECT;
          ++Flash->free_sects;
        }
        Flash->sect_tbl[sect].next = (ui16)(sect + 1);
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* There are no more ctrl sectors.                                   */
  /*-------------------------------------------------------------------*/
  Flash->frst_ctrl_sect = Flash->last_ctrl_sect = (ui32)-1;
  Flash->ctrl_sects = Flash->ctrl_size = 0;
  Flash->free_ctrl_sect = (ui32)-1;

  /*-------------------------------------------------------------------*/
  /* Readjust free sector list. When rearranging free list, place      */
  /* blocks in ascending order of wear count and set the next erasable */
  /* set.                                                              */
  /*-------------------------------------------------------------------*/
  Flash->free_sect = frst_free_block * Flash->block_sects +
                     Flash->hdr_sects;
  Flash->last_free_sect = (frst_free_block + 1) * Flash->block_sects - 1;
  Flash->sect_tbl[Flash->last_free_sect].next = FLAST_SECT;
  Flash->blocks[frst_free_block].ctrl_block = TRUE;
  for (i = 1;; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* Choose next block to append to free list based on wear count.   */
    /*-----------------------------------------------------------------*/
    block = next_free_block();
    if (block == -1)
      break;

    /*-----------------------------------------------------------------*/
    /* Append next block to free list.                                 */
    /*-----------------------------------------------------------------*/
    Flash->sect_tbl[Flash->last_free_sect].next =
                   (ui16)(block * Flash->block_sects + Flash->hdr_sects);
    Flash->last_free_sect = (block + 1) * Flash->block_sects - 1;
    Flash->sect_tbl[Flash->last_free_sect].next = FLAST_SECT;
  }

  /*-------------------------------------------------------------------*/
  /* Reset the erase set.                                              */
  /*-------------------------------------------------------------------*/
  Flash->set_blocks = 1;
  for (i = 0; i < Flash->max_set_blocks; ++i)
    Flash->erase_set[i] = -1;

  /*-------------------------------------------------------------------*/
  /* Unmark the flag marking that was necessary for next_free_block(). */
  /*-------------------------------------------------------------------*/
  for (block = 0; block < Flash->num_blocks; ++block)
    if (!Flash->blocks[block].bad_block)
      Flash->blocks[block].ctrl_block = FALSE;

  /*-------------------------------------------------------------------*/
  /* Save the state of the system.                                     */
  /*-------------------------------------------------------------------*/
  strcpy(Flash->files_tbl->tbl[0].entry.dir.name, Flash->sys.name);
  if (FlashSync(DO_SYNC) != NULL)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Clean old ctrl blocks.                                            */
  /*-------------------------------------------------------------------*/
  if (old_1st_ctrl_block != -1)
  {
    PfAssert(!Flash->blocks[old_1st_ctrl_block].bad_block &&
             Flash->blocks[old_1st_ctrl_block].used_sects == 0);

    /*-----------------------------------------------------------------*/
    /* Erase 1st old control block.                                    */
    /*-----------------------------------------------------------------*/
    addr = Flash->mem_base + old_1st_ctrl_block * Flash->block_size;
    if (Flash->erase_block_wrapper(addr))
    {
      set_errno(EIO);
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Write signature bytes.                                          */
    /*-----------------------------------------------------------------*/
    if (Flash->type == FFS_NOR)
    {
      hdr = addr + Flash->hdr_sects * Flash->sect_sz - RES_BYTES;

      /*---------------------------------------------------------------*/
      /* Release exclusive access to the flash file system.            */
      /*---------------------------------------------------------------*/
      vol = Flash;
      semPost(FlashSem);

      /*---------------------------------------------------------------*/
      /* Call flash driver write_byte() routine.                       */
      /*---------------------------------------------------------------*/
      status = vol->driver.nor.write_byte(hdr++, NOR_BYTE1, vol->vol) ||
               vol->driver.nor.write_byte(hdr++, NOR_BYTE2, vol->vol) ||
               vol->driver.nor.write_byte(hdr++, NOR_BYTE3, vol->vol) ||
               vol->driver.nor.write_byte(hdr++, NOR_BYTE4, vol->vol);

      /*---------------------------------------------------------------*/
      /* Acquire exclusive access to the flash file system.            */
      /*---------------------------------------------------------------*/
      semPend(FlashSem, WAIT_FOREVER);
      Flash = vol;
      if (status)
      {
        set_errno(EIO);
        return -1;
      }
    }
  }
  if (old_2nd_ctrl_block != -1)
  {
    PfAssert(!Flash->blocks[old_2nd_ctrl_block].bad_block &&
             Flash->blocks[old_2nd_ctrl_block].used_sects == 0);

    /*-----------------------------------------------------------------*/
    /* Erase 2nd old control block.                                    */
    /*-----------------------------------------------------------------*/
    addr = Flash->mem_base + old_2nd_ctrl_block * Flash->block_size;
    if (Flash->erase_block_wrapper(addr))
    {
      set_errno(EIO);
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Write signature bytes.                                          */
    /*-----------------------------------------------------------------*/
    if (Flash->type == FFS_NOR)
    {
      hdr = addr + Flash->hdr_sects * Flash->sect_sz - RES_BYTES;

      /*---------------------------------------------------------------*/
      /* Release exclusive access to the flash file system.            */
      /*---------------------------------------------------------------*/
      vol = Flash;
      semPost(FlashSem);

      /*---------------------------------------------------------------*/
      /* Call flash driver write_byte() routine.                       */
      /*---------------------------------------------------------------*/
      status = vol->driver.nor.write_byte(hdr++, NOR_BYTE1, vol->vol) ||
               vol->driver.nor.write_byte(hdr++, NOR_BYTE2, vol->vol) ||
               vol->driver.nor.write_byte(hdr++, NOR_BYTE3, vol->vol) ||
               vol->driver.nor.write_byte(hdr++, NOR_BYTE4, vol->vol);

      /*---------------------------------------------------------------*/
      /* Acquire exclusive access to the flash file system.            */
      /*---------------------------------------------------------------*/
      semPend(FlashSem, WAIT_FOREVER);
      Flash = vol;
      if (status)
      {
        set_errno(EIO);
        return -1;
      }
    }
  }

  FlashChooseEraseSet(USE_WEAR);
  return 0;
}

#if FFS_DEBUG
/***********************************************************************/
/*   find_file: Find file name based on comm entry                     */
/*                                                                     */
/*      Inputs: vol = pointer to flash volume                          */
/*              comm = pointer to comm entry                           */
/*                                                                     */
/*     Returns: file name, "NO FILE" if no such file                   */
/*                                                                     */
/***********************************************************************/
static char *find_file(FlashGlob *vol, FFSEnt *comm)
{
  FFSEnts *tbl;
  FFSEnt *entry;
  ui32 i;

  for (tbl = vol->files_tbl; tbl; tbl = tbl->next_tbl)
  {
    entry = &tbl->tbl[0];
    for (i = 0; i < FNUM_ENT; ++i, ++entry)
      if (entry->type == FFILEN)
      {
        if ((void *)entry->entry.file.comm == comm)
          return entry->entry.file.name;
      }
  }
  return "NO FILE";
}
#endif

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

#if FFS_DEBUG
/***********************************************************************/
/* FlashCheckSectTbl: Check the sectors table for consistency          */
/*                                                                     */
/*       Input: vol = pointer to file system volume                    */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
int FlashCheckSectTbl(FlashGlob *vol)
{
  ui32 i, j, size, bitmap_sz = (vol->num_sects + 7) / 8, num_used = 0;
  ui8 *bitmap1 = calloc(bitmap_sz, sizeof(ui8));
  ui8 *bitmap2 = calloc(bitmap_sz, sizeof(ui8));
  FFSEnts *tbl;
  FFSEnt *entry;
  int r_val = -1;

  if (bitmap1 == NULL || bitmap2 == NULL)
  {
    set_errno(ENOMEM);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Check the used sectors for each file.                             */
  /*-------------------------------------------------------------------*/
  for (tbl = vol->files_tbl; tbl; tbl = tbl->next_tbl)
  {
    entry = &tbl->tbl[0];
    for (i = 0; i < FNUM_ENT; ++i, ++entry)
      if (entry->type == FCOMMN && entry->entry.comm.size)
      {
        size = 0;
        for (j = entry->entry.comm.frst_sect; j != FLAST_SECT;
             j = vol->sect_tbl[j].next)
        {
          /*-----------------------------------------------------------*/
          /* Check sector is used.                                     */
          /*-----------------------------------------------------------*/
          if (!used_sect(j))
          {
            printf("File = %s, sector = %u not used\n",
                   find_file(vol, entry), j);
            goto FlashCheckSectTbl_exit;
          }
          ++num_used;

          /*-----------------------------------------------------------*/
          /* Check for loops in sector chain.                          */
          /*-----------------------------------------------------------*/
          if (bitmap1[j / 8] & (1 << (j % 8)))
          {
            printf("File = %s has loop in sector chain\n",
                   find_file(vol, entry));
            goto FlashCheckSectTbl_exit;
          }

          /*-----------------------------------------------------------*/
          /* Check sector does not belong to a different file.         */
          /*-----------------------------------------------------------*/
          if (bitmap2[j / 8] & (1 << (j % 8)))
          {
            printf("File = %s has other file in sector chain\n",
                   find_file(vol, entry));
            goto FlashCheckSectTbl_exit;
          }

          /*-----------------------------------------------------------*/
          /* Mark bitmaps.                                             */
          /*-----------------------------------------------------------*/
          bitmap1[j / 8] |= (1 << (j % 8));
          bitmap2[j / 8] |= (1 << (j % 8));

          /*-----------------------------------------------------------*/
          /* Check next and prev pointers.                             */
          /*-----------------------------------------------------------*/
          if (vol->sect_tbl[j].prev == FLAST_SECT)
          {
            if (j != entry->entry.comm.frst_sect)
            {
              printf("File = %s has inconsistent first sector\n",
                     find_file(vol, entry));
              goto FlashCheckSectTbl_exit;
            }
          }
          else
          {
            if (vol->sect_tbl[vol->sect_tbl[j].prev].next != j)
            {
              printf("File = %s inconsistent chain:\n",
                     find_file(vol, entry));
              printf("   * sect[%u].prev = %u, sect[%u].next = %u\n",
                     j, vol->sect_tbl[j].prev, vol->sect_tbl[j].prev,
                     vol->sect_tbl[vol->sect_tbl[j].prev].next);
              goto FlashCheckSectTbl_exit;
            }
          }
          if (vol->sect_tbl[j].next == FLAST_SECT)
          {
            if (j != entry->entry.comm.last_sect)
            {
              printf("File = %s has inconsistent last sector\n",
                     find_file(vol, entry));
              goto FlashCheckSectTbl_exit;
            }
            if (entry->entry.comm.one_past_last.sector == 0xFFFF)
              size += vol->sect_sz;
            else
              size += entry->entry.comm.one_past_last.offset;
          }
          else
          {
            if (vol->sect_tbl[vol->sect_tbl[j].next].prev != j)
            {
              printf("File = %s inconsistent chain:\n",
                     find_file(vol, entry));
              printf("   * sect[%u].next = %u, sect[%u].prev = %u\n",
                     j, vol->sect_tbl[j].next, vol->sect_tbl[j].next,
                     vol->sect_tbl[vol->sect_tbl[j].next].prev);
              goto FlashCheckSectTbl_exit;
            }
            size += vol->sect_sz;
          }
        }
        memset(bitmap1, 0, bitmap_sz);

        /*-------------------------------------------------------------*/
        /* Check file size is correct.                                 */
        /*-------------------------------------------------------------*/
        if (size != entry->entry.comm.size)
        {
          printf("File = %s: thought size = %u, actual size = %u\n",
                 find_file(vol, entry), entry->entry.comm.size, size);
          goto FlashCheckSectTbl_exit;
        }
      }
  }

  /*-------------------------------------------------------------------*/
  /* Check total number of used sectors.                               */
  /*-------------------------------------------------------------------*/
  if (num_used != vol->used_sects)
  {
    printf("Vol total thought used = %u, actual used = %u\n",
           vol->used_sects, num_used);
    goto FlashCheckSectTbl_exit;
  }

  /*-------------------------------------------------------------------*/
  /* Check blocks num used against total used.                         */
  /*-------------------------------------------------------------------*/
  for (num_used = 0, i = 0; i < vol->num_blocks; ++i)
    if (!vol->blocks[i].ctrl_block)
      num_used += vol->blocks[i].used_sects;
  if (num_used != vol->used_sects)
  {
    printf("Vol total blocks used = %u, actual used = %u\n",
           vol->used_sects, num_used);
    goto FlashCheckSectTbl_exit;
  }

  /*-------------------------------------------------------------------*/
  /* Check the control sectors.                                        */
  /*-------------------------------------------------------------------*/
  for (num_used = 0, i = vol->frst_ctrl_sect; i != FLAST_SECT;
       i = vol->sect_tbl[i].next, ++num_used)
  {
    /*-----------------------------------------------------------------*/
    /* Check sector is used.                                           */
    /*-----------------------------------------------------------------*/
    if (!used_sect(i))
    {
      printf("Contorl sector = %u not used\n", i);
      goto FlashCheckSectTbl_exit;
    }

    /*-----------------------------------------------------------------*/
    /* Check for loops in sector chain.                                */
    /*-----------------------------------------------------------------*/
    if (bitmap1[i / 8] & (1 << (i % 8)))
    {
      printf("Contro info has loop in sector chain\n");
      goto FlashCheckSectTbl_exit;
    }

    /*-----------------------------------------------------------------*/
    /* Check sector does not belong to a different file.               */
    /*-----------------------------------------------------------------*/
    if (bitmap2[i / 8] & (1 << (i % 8)))
    {
      printf("File = %s has loop file data in sector chain\n");
      goto FlashCheckSectTbl_exit;
    }

    /*-----------------------------------------------------------------*/
    /* Mark bitmap.                                                    */
    /*-----------------------------------------------------------------*/
    bitmap1[i / 8] |= (1 << (i % 8));

    /*-----------------------------------------------------------------*/
    /* Check next and prev pointers.                                   */
    /*-----------------------------------------------------------------*/
    if (vol->sect_tbl[i].prev == FLAST_SECT)
    {
      if (i != vol->frst_ctrl_sect)
      {
        printf("Control info has inconsistent first sector\n");
        goto FlashCheckSectTbl_exit;
      }
    }
    else
    {
      if (vol->sect_tbl[vol->sect_tbl[i].prev].next != i)
      {
        printf("Control info inconsistent chain:\n");
        printf("   * sect[%u].prev = %u, sect[%u].next = %u\n",
               i, vol->sect_tbl[i].prev, vol->sect_tbl[i].prev,
               vol->sect_tbl[vol->sect_tbl[i].prev].next);
        goto FlashCheckSectTbl_exit;
      }
    }
    if (vol->sect_tbl[i].next == FLAST_SECT)
    {
      if (i != vol->last_ctrl_sect)
      {
        printf("Control info has inconsistent last sector\n");
        goto FlashCheckSectTbl_exit;
      }
    }
    else
    {
      if (vol->sect_tbl[vol->sect_tbl[i].next].prev != i)
      {
        printf("Control info inconsistent chain:\n");
        printf("   * sect[%u].next = %u, sect[%u].prev = %u\n",
               i, vol->sect_tbl[i].next, vol->sect_tbl[i].next,
               vol->sect_tbl[vol->sect_tbl[i].next].prev);
        goto FlashCheckSectTbl_exit;
      }
    }
  }

  /*------------------------------------------------------------------*/
  /* Check total number of control sectors.                           */
  /*------------------------------------------------------------------*/
  if (num_used != vol->ctrl_sects)
  {
    printf("Vol total ctrl sects thought = %u, actual = %u\n",
           vol->ctrl_sects, num_used);
  }
  memset(bitmap2, 0, bitmap_sz);

  /*-------------------------------------------------------------------*/
  /* Check the free sectors.                                           */
  /*-------------------------------------------------------------------*/
  for (j = vol->free_sect, size = 0;; j = vol->sect_tbl[j].next, ++size)
  {
    /*-----------------------------------------------------------------*/
    /* Check sector is free.                                           */
    /*-----------------------------------------------------------------*/
    if (vol->sect_tbl[j].prev != FFREE_SECT)
    {
      printf("Sector = %u in free sectors not free!\n", j);
      goto FlashCheckSectTbl_exit;
    }

    /*-----------------------------------------------------------------*/
    /* Check for loops in free list.                                   */
    /*-----------------------------------------------------------------*/
    if (bitmap2[j / 8] & (1 << (j % 8)))
    {
      printf("Loop in free sectors list!\n");
      goto FlashCheckSectTbl_exit;
    }
    bitmap2[j / 8] |= (1 << (j % 8));

    /*-----------------------------------------------------------------*/
    /* Check last sector.                                              */
    /*-----------------------------------------------------------------*/
    if (vol->sect_tbl[j].next == FLAST_SECT)
    {
      if (j != vol->last_free_sect)
      {
        printf("Free list thought last = %u, actual last = %u\n",
               vol->last_free_sect, j);
        goto FlashCheckSectTbl_exit;
      }
      ++size;
      break;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Successfully passed check.                                        */
  /*-------------------------------------------------------------------*/
  r_val = 0;

FlashCheckSectTbl_exit:
  free(bitmap1);
  free(bitmap2);
  return r_val;
}
#endif /* FFS_DEBUG */

/***********************************************************************/
/* FlashGetTbl: Create a new table to be added to the list of tables   */
/*                                                                     */
/*     Returns: pointer to the newly created table                     */
/*                                                                     */
/***********************************************************************/
FFSEnts *FlashGetTbl(void)
{
  FFSEnts *r_value = calloc(1, sizeof(FFSEnts));
  int i;

  if (r_value)
  {
    r_value->next_tbl = r_value->prev_tbl = NULL;
    r_value->free = FNUM_ENT - 1;

    /*-----------------------------------------------------------------*/
    /* Empty out all the entries in the table.                         */
    /*-----------------------------------------------------------------*/
    for (i = 0; i < FNUM_ENT; ++i)
      r_value->tbl[i].type = FEMPTY;

    /*-----------------------------------------------------------------*/
    /* Add size of table entries to the total entries.                 */
    /*-----------------------------------------------------------------*/
    Flash->total += FNUM_ENT;
  }
  else
    set_errno(ENOMEM);

  return r_value;
}

/***********************************************************************/
/*  FlashClean: Erase a flash volume                                   */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int FlashClean(void)
{
  ui32 block, hdr, i, j, sect, addr = Flash->mem_base;
  int rc;
  FlashGlob *vol;

  /*-------------------------------------------------------------------*/
  /* Loop over every block in the device.                              */
  /*-------------------------------------------------------------------*/
  block = 0;
  for (; block < Flash->num_blocks; ++block, addr += Flash->block_size)
  {
    /*-----------------------------------------------------------------*/
    /* Skip control blocks and bad blocks.                             */
    /*-----------------------------------------------------------------*/
    if (Flash->blocks[block].ctrl_block ||
        (Flash->type == FFS_NAND && Flash->blocks[block].bad_block))
      continue;

    /*-----------------------------------------------------------------*/
    /* Loop over every sector in this block.                           */
    /*-----------------------------------------------------------------*/
    sect = block * Flash->block_sects;
    for (i = 0;; ++i)
    {
      /*---------------------------------------------------------------*/
      /* Check if every sector has been verified empty.                */
      /*---------------------------------------------------------------*/
      if (i == Flash->block_sects)
      {
        /*-------------------------------------------------------------*/
        /* If NOR device, write the signature bytes.                   */
        /*-------------------------------------------------------------*/
        if (Flash->type == FFS_NOR)
        {
          hdr = addr + Flash->hdr_sects * Flash->sect_sz - RES_BYTES;

          /*-----------------------------------------------------------*/
          /* Release exclusive access to the flash file system.        */
          /*-----------------------------------------------------------*/
          vol = Flash;
          semPost(FlashSem);

          /*-----------------------------------------------------------*/
          /* Call flash driver write_byte() routine.                   */
          /*-----------------------------------------------------------*/
          rc = vol->driver.nor.write_byte(hdr++, NOR_BYTE1, vol->vol) ||
               vol->driver.nor.write_byte(hdr++, NOR_BYTE2, vol->vol) ||
               vol->driver.nor.write_byte(hdr++, NOR_BYTE3, vol->vol) ||
               vol->driver.nor.write_byte(hdr,   NOR_BYTE4, vol->vol);

          /*-----------------------------------------------------------*/
          /* Acquire exclusive access to the flash file system.        */
          /*-----------------------------------------------------------*/
          semPend(FlashSem, WAIT_FOREVER);
          Flash = vol;

          /*-----------------------------------------------------------*/
          /* Return error if any write_byte() failed.                  */
          /*-----------------------------------------------------------*/
          if (rc)
          {
            set_errno(EIO);
            return -1;
          }
        }

        /*-------------------------------------------------------------*/
        /* Break to continue to next block.                            */
        /*-------------------------------------------------------------*/
        break;
      }

      /*---------------------------------------------------------------*/
      /* For NAND/MLC check that all sectors are free.                 */
      /*---------------------------------------------------------------*/
      if (Flash->type != FFS_NOR)
      {
        if (Flash->empty_sect(sect + i) == FALSE)
        {
          if (Flash->erase_block_wrapper(addr))
          {
            set_errno(EIO);
            return -1;
          }
          break;
        }
      }

      /*---------------------------------------------------------------*/
      /* For NOR, check non-header sectors are empty and header ones   */
      /* contain at most the signature bytes.                          */
      /*---------------------------------------------------------------*/
      else if (Flash->empty_sect(sect + i) == FALSE)
      {
        /*-------------------------------------------------------------*/
        /* If non-header sector not empty, erase block.                */
        /*-------------------------------------------------------------*/
        if (i != Flash->hdr_sects - 1)
        {
          if (Flash->erase_block_wrapper(addr))
          {
            set_errno(EIO);
            return -1;
          }
          break;
        }

        /*-------------------------------------------------------------*/
        /* Check header sector (last one) contains only signature.     */
        /*-------------------------------------------------------------*/
        if (Flash->read_sector(Flash->tmp_sect, sect + i))
        {
          set_errno(EIO);
          return -1;
        }

        /*-------------------------------------------------------------*/
        /* Check that non-sinature bytes are all 0xFF.                 */
        /*-------------------------------------------------------------*/
        for (j = 0; j < Flash->sect_sz - RES_BYTES; ++j)
          if (Flash->tmp_sect[j] != 0xFF)
          {
            if (Flash->erase_block_wrapper(addr))
            {
              set_errno(EIO);
              return -1;
            }
            break;
          }
        if (j < Flash->sect_sz - RES_BYTES)
          break;

        /*-------------------------------------------------------------*/
        /* Check the signature bytes.                                  */
        /*-------------------------------------------------------------*/
        if (Flash->tmp_sect[j] != NOR_BYTE1 ||
            Flash->tmp_sect[j + 1] != NOR_BYTE2 ||
            Flash->tmp_sect[j + 2] != NOR_BYTE3 ||
            Flash->tmp_sect[j + 3] != NOR_BYTE4)
        {
          if (Flash->erase_block_wrapper(addr))
          {
            set_errno(EIO);
            return -1;
          }
          break;
        }
      }
    }
  }
  return 0;
}

/***********************************************************************/
/*   FfsModule: Flash file system interface to software object manager */
/*                                                                     */
/*       Input: req = module request code                              */
/*              ... = additional parameters specific to request        */
/*                                                                     */
/***********************************************************************/
void *FfsModule(int req, ...)
{
  void *r_value = NULL;
  va_list ap;
  char *path;
  ui32 i, j;
  FlashGlob *vol;

  switch (req)
  {
    case kInitMod:
    {
      char sem_name[9];

      /*---------------------------------------------------------------*/
      /* Initialize the FAT semaphores.                                */
      /*---------------------------------------------------------------*/
      FlashSem = semCreate("FLASH_SEM", 1, OS_FIFO);
      if (FlashSem == NULL)
        return (void *)-1;
      for (i = 0; i < NUM_FFS_VOLS; ++i)
      {
        sprintf(sem_name, "FFS_%04d", i);
        FlashGlobals[i].sem = semCreate(sem_name, 1, OS_FIFO);
        if (FlashGlobals[i].sem == NULL)
        {
          while (i--)
            semDelete(&FlashGlobals[i].sem);
          semDelete(&FlashSem);
          return (void *)-1;
        }
      }
      return r_value;
    }

    /*-----------------------------------------------------------------*/
    /* Handle the unformat function call.                              */
    /*-----------------------------------------------------------------*/
    case kUnformat:
    {
      int do_unmount = FALSE;
      ui32 block, addr;

      /*---------------------------------------------------------------*/
      /* Use the va_arg mechanism to fetch the path for unformat.      */
      /*---------------------------------------------------------------*/
      va_start(ap, req);
      path = va_arg(ap, char *);
      va_end(ap);

      /*---------------------------------------------------------------*/
      /* Go through all the existing unmounted volumes and if any has  */
      /* the given name, unformat it.                                  */
      /*---------------------------------------------------------------*/
      for (i = 0; i < NUM_FFS_VOLS; ++i)
      {
        /*-------------------------------------------------------------*/
        /* Set pointer to the current entry in the array.              */
        /*-------------------------------------------------------------*/
        vol = &FlashGlobals[i];

        /*-------------------------------------------------------------*/
        /* If this entry contains a valid volume and names match, then */
        /* unformat the volume.                                        */
        /*-------------------------------------------------------------*/
        if (vol->num_sects && !strcmp(path, vol->sys.name))
        {
          /*-----------------------------------------------------------*/
          /* Acquire exclusive access to volume and flash file system. */
          /*-----------------------------------------------------------*/
          semPend(vol->sem, WAIT_FOREVER);
          semPend(FlashSem, WAIT_FOREVER);
          Flash = vol;

          /*-----------------------------------------------------------*/
          /* If volume is mounted, remember to unmount, else do a semi */
          /* mount (i.e. read control up to bad block information.     */
          /*-----------------------------------------------------------*/
          if (Flash->sys.next || Flash->sys.prev ||
              &Flash->sys == MountedList.head)
            do_unmount = TRUE;
          else
          {
            /*---------------------------------------------------------*/
            /* Assign memory to hold bad block info.                   */
            /*---------------------------------------------------------*/
            Flash->blocks = malloc(Flash->num_blocks * sizeof(FFSBlock));
            if (Flash->blocks == NULL)
            {
              semPost(FlashSem);
              semPost(vol->sem);
              return (void *)-1;
            }

            /*---------------------------------------------------------*/
            /* Attempt to do a semi mount based on control from flash. */
            /*---------------------------------------------------------*/
            if (FlashSemiMount())
            {
              free(Flash->blocks);
              Flash->blocks = NULL;
              semPost(FlashSem);
              semPost(vol->sem);
              return (void *)1;
            }
          }

          /*-----------------------------------------------------------*/
          /* Release exclusive access to the flash file system.        */
          /*-----------------------------------------------------------*/
          semPost(FlashSem);

          /*-----------------------------------------------------------*/
          /* Walk the blocks list erasing all non-bad ones.            */
          /*-----------------------------------------------------------*/
          for (block = 0; block < vol->num_blocks; ++block)
            if (!vol->blocks[block].bad_block)
            {
              addr = vol->mem_base + block * vol->block_size;
              if (vol->type == FFS_NAND)
                (void)vol->driver.nand.erase_block(addr, vol->vol);
              else
                (void)vol->driver.nor.erase_block(addr, vol->vol);
            }

          /*-----------------------------------------------------------*/
          /* Release exclusive access to volume.                       */
          /*-----------------------------------------------------------*/
          semPost(vol->sem);

          /*-----------------------------------------------------------*/
          /* Do an unmount if necessary.                               */
          /*-----------------------------------------------------------*/
          if (do_unmount)
            FlashIoctl(NULL, UNMOUNT, vol->sys.volume, FALSE);
          else
          {
            free(vol->blocks);
            vol->blocks = NULL;
          }
          return (void *)1;
        }
      }
      break;
    }

    /*-----------------------------------------------------------------*/
    /* Handle the format function call.                                */
    /*-----------------------------------------------------------------*/
    case kFormat:
    {
      char *name;
      FileSys *fsys;
      int unmount_volume = FALSE;
      int fresh_format = FALSE;

      /*---------------------------------------------------------------*/
      /* Use the va_arg mechanism to fetch the path for format.        */
      /*---------------------------------------------------------------*/
      va_start(ap, req);
      path = va_arg(ap, char *);
      va_end(ap);

      /*---------------------------------------------------------------*/
      /* Go through all the existing volumes (mounted and not mounted) */
      /* and if any has the given name, format it.                     */
      /*---------------------------------------------------------------*/
      for (i = 0; i < NUM_FFS_VOLS; ++i)
      {
        /*-------------------------------------------------------------*/
        /* Set pointer to the current entry in the array.              */
        /*-------------------------------------------------------------*/
        vol = &FlashGlobals[i];

        /*-------------------------------------------------------------*/
        /* If this entry contains a valid volume and the names match,  */
        /* format the volume.                                          */
        /*-------------------------------------------------------------*/
        if (vol->num_sects && !strcmp(path, vol->sys.name))
        {
          /*-----------------------------------------------------------*/
          /* If the volume is mounted, prepare it for erase operation. */
          /*-----------------------------------------------------------*/
          if (vol->sys.next || vol->sys.prev ||
              (&vol->sys == MountedList.head))
          {
            /*---------------------------------------------------------*/
            /* If sector freeing fails, return error.                  */
            /*---------------------------------------------------------*/
            if (prepare_mounted_for_format(vol))
              return (void *)-1;
          }

          /*-----------------------------------------------------------*/
          /* Else (volume not mounted) we have to mount the volume.    */
          /*-----------------------------------------------------------*/
          else
          {
            /*---------------------------------------------------------*/
            /* Try mounting the volume if possible.                    */
            /*---------------------------------------------------------*/
            fsys = FfsModule(kMount, path,  /*lint !e416 !e415 */
                             &name, TRUE);
            if (fsys != (FileSys *)-1)
            {
              /*-------------------------------------------------------*/
              /* If sector freeing fails, unmount with error.          */
              /*-------------------------------------------------------*/
              if (prepare_mounted_for_format(vol))
              {
                FlashIoctl(NULL, UNMOUNT, fsys->volume, FALSE);
                return (void *)-1;
              }
              unmount_volume = TRUE;
            }

            /*---------------------------------------------------------*/
            /* Else set up unformatted volume for mounting.            */
            /*---------------------------------------------------------*/
            else
            {
              /*-------------------------------------------------------*/
              /* Acquire access to volume and flash file system.       */
              /*-------------------------------------------------------*/
              semPend(vol->sem, WAIT_FOREVER);
              semPend(FlashSem, WAIT_FOREVER);
              Flash = vol;

              /*-------------------------------------------------------*/
              /* Set up volume as if empty.                            */
              /*-------------------------------------------------------*/
              if (init_volume())
              {
                semPost(FlashSem);
                semPost(vol->sem);
                return (void *)-1;
              }

              /*-------------------------------------------------------*/
              /* Call volume specific code.                            */
              /*-------------------------------------------------------*/
              if (Flash->unformat_to_format())
              {
                semPost(FlashSem);
                semPost(vol->sem);
                return (void *)-1;
              }
              fresh_format = TRUE;

              /*-------------------------------------------------------*/
              /* Release access to flash file system and this volume.  */
              /*-------------------------------------------------------*/
              semPost(FlashSem);
              semPost(vol->sem);
            }
          }

          /*-----------------------------------------------------------*/
          /* Acquire exclusive access to volume and flash file system. */
          /*-----------------------------------------------------------*/
          semPend(vol->sem, WAIT_FOREVER);
          semPend(FlashSem, WAIT_FOREVER);
          Flash = vol;

          /*-----------------------------------------------------------*/
          /* Make sure first free sector points to a non-ctrl block.   */
          /*-----------------------------------------------------------*/
          if (set_first_free())
          {
            semPost(FlashSem);
            semPost(vol->sem);
            if (unmount_volume)
              FlashIoctl(NULL, UNMOUNT,
                         fsys->volume, FALSE); /*lint !e644 */
            return (void *)-1;
          }

          /*-----------------------------------------------------------*/
          /* If fresh volume, deallocate memory.                       */
          /*-----------------------------------------------------------*/
          if (fresh_format)
          {
            free(Flash->sect_tbl);
            Flash->sect_tbl = NULL;
            free(Flash->blocks);
            Flash->blocks = NULL;
            free(Flash->files_tbl);
            Flash->files_tbl = NULL;
            free(Flash->erase_set);
            Flash->erase_set = NULL;
            DestroyCache(&Flash->cache);
          }

          /*-----------------------------------------------------------*/
          /* Release access to flash file system and this volume.      */
          /*-----------------------------------------------------------*/
          semPost(FlashSem);
          semPost(vol->sem);

          /*-----------------------------------------------------------*/
          /* If we need to unmount the volume, do so.                  */
          /*-----------------------------------------------------------*/
          if (unmount_volume)
          {
            if (FlashIoctl(NULL, UNMOUNT, fsys->volume, TRUE) != NULL)
              return (void *)-1;
          }
          return (void *)1;
        }
      }
      break;
    }

    case kVolName:
    {
      /*---------------------------------------------------------------*/
      /* Use the va_arg mechanism to fetch the name to look for.       */
      /*---------------------------------------------------------------*/
      va_start(ap, req);
      path = va_arg(ap, char *);
      va_end(ap);

      /*---------------------------------------------------------------*/
      /* Look for name duplication.                                    */
      /*---------------------------------------------------------------*/
      for (i = 0; i < NUM_FFS_VOLS; ++i)
        if (FlashGlobals[i].num_sects &&
            !strcmp(path, FlashGlobals[i].sys.name))
        {
          set_errno(EEXIST);
          r_value = (void *)-1;
          break;
        }
      break;
    }

    /*-----------------------------------------------------------------*/
    /* Handle the mount function call.                                 */
    /*-----------------------------------------------------------------*/
    case kMount:
    {
      char *volume, **name_out;
      int status, partial_read;

      /*---------------------------------------------------------------*/
      /* Use va_arg mechanism to fetch volume name and output pointer. */
      /*---------------------------------------------------------------*/
      va_start(ap, req);
      volume = va_arg(ap, char *);
      name_out = va_arg(ap, char **);
      partial_read = va_arg(ap, int);
      va_end(ap);

      /*---------------------------------------------------------------*/
      /* Walk through the valid entries in FlashGlobals and see if any */
      /* matches the volume name.                                      */
      /*---------------------------------------------------------------*/
      for (i = 0; i < NUM_FFS_VOLS; ++i)
      {
        /*-------------------------------------------------------------*/
        /* Set vol to the current entry in the array.                  */
        /*-------------------------------------------------------------*/
        vol = &FlashGlobals[i];

        /*-------------------------------------------------------------*/
        /* Ignore empty entries.                                       */
        /*-------------------------------------------------------------*/
        if (vol->num_sects == 0)
          continue;

        /*-------------------------------------------------------------*/
        /* Acquire exclusive access to volume and flash file system.   */
        /*-------------------------------------------------------------*/
        semPend(vol->sem, WAIT_FOREVER);
        semPend(FlashSem, WAIT_FOREVER);

        /*-------------------------------------------------------------*/
        /* If matching entry is found (either name matches volume name */
        /* or volume name is NULL from mount_all()), do the mount.     */
        /*-------------------------------------------------------------*/
        if (vol->sys.prev == NULL && vol->sys.next == NULL &&
            &vol->sys != MountedList.head &&
            (volume == NULL || !strcmp(vol->sys.name, volume)))
        {
          Flash = vol;
          r_value = (void *)-1;
          *name_out = Flash->sys.name;

          /*-----------------------------------------------------------*/
          /* Matching entry must not be mounted if volume is non-NULL. */
          /*-----------------------------------------------------------*/
          if (volume)
          {
            PfAssert(Flash->sys.prev == NULL && Flash->sys.next == NULL);
          }

          /*-----------------------------------------------------------*/
          /* Allocate memory for the volume.                           */
          /*-----------------------------------------------------------*/
          PfAssert(Flash->sect_tbl == NULL);
          PfAssert(Flash->blocks == NULL);
          if (assign_volume_mem())
          {
            semPost(FlashSem);
            semPost(vol->sem);
            break;
          }

          /*-----------------------------------------------------------*/
          /* Initialize global control structure.                      */
          /*-----------------------------------------------------------*/
          for (j = 0, Flash->used_sects = 0; j < Flash->num_blocks; ++j)
            Flash->blocks[j].used_sects = 0;
          Flash->ctrl_size = Flash->ctrl_sects = Flash->opendirs = 0;
          Flash->set_blocks = 1;
          Flash->do_sync = TRUE;
          Flash->sync_deferred = FALSE;

          /*-----------------------------------------------------------*/
          /* Read the flash.                                           */
          /*-----------------------------------------------------------*/
          status = FlashRdCtrl(partial_read);
#if INC_OLD_NOR_FS
          if (status == FREAD_ERR && Flash->type == FFS_NOR)
            status = FsNorOldRdCtrl();
#endif
          if (status == FREAD_ERR)
          {
            free(Flash->sect_tbl);
            Flash->sect_tbl = NULL;
            free(Flash->blocks);
            Flash->blocks = NULL;
            free(Flash->erase_set);
            Flash->erase_set = NULL;
            DestroyCache(&Flash->cache);
            semPost(FlashSem);
            semPost(vol->sem);
            break;
          }

          /*-----------------------------------------------------------*/
          /* Assign root directory name.                               */
          /*-----------------------------------------------------------*/
          strcpy(Flash->files_tbl->tbl[0].entry.dir.name,
                 Flash->sys.name);

          /*-----------------------------------------------------------*/
          /* Before returning, set r_value correctly.                  */
          /*-----------------------------------------------------------*/
          r_value = &Flash->sys;

          /*-----------------------------------------------------------*/
          /* Set the wr_ctrl flag.                                     */
          /*-----------------------------------------------------------*/
          Flash->wr_ctrl = TRUE;

          /*-----------------------------------------------------------*/
          /* Release access to flash file system and this volume.      */
          /*-----------------------------------------------------------*/
          semPost(FlashSem);
          semPost(vol->sem);
          break;
        }

        /*-------------------------------------------------------------*/
        /* Release access to flash file system and this volume.        */
        /*-------------------------------------------------------------*/
        semPost(FlashSem);
        semPost(vol->sem);
      }
      break;
    }

    default:
      break;
  }
  return r_value;
}

/***********************************************************************/
/*   FfsDelVol: Delete an existing volume (added before)               */
/*                                                                     */
/*       Input: name = name of added volume                            */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
int FfsDelVol(const char *name)
{
  int i;
  FlashGlob *vol;

  /*-------------------------------------------------------------------*/
  /* Unmount volume. If already unmounted, this won't do anything.     */
  /*-------------------------------------------------------------------*/
  unmount(name);

  /*-------------------------------------------------------------------*/
  /* Look for the volume in the FlashGlobals array.                    */
  /*-------------------------------------------------------------------*/
  for (i = 0;; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* If the volume was not found, return error.                      */
    /*-----------------------------------------------------------------*/
    if (i == NUM_FFS_VOLS)
    {
      set_errno(ENOENT);
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Acquire exclusive access to volume.                             */
    /*-----------------------------------------------------------------*/
    vol = &FlashGlobals[i];
    semPend(vol->sem, WAIT_FOREVER);

    /*-----------------------------------------------------------------*/
    /* If volume names match and volume is installed, break.           */
    /*-----------------------------------------------------------------*/
    if (vol->num_sects && !strcmp(name, vol->sys.name))
      break;

    /*-----------------------------------------------------------------*/
    /* Release exclusive access to volume.                             */
    /*-----------------------------------------------------------------*/
    semPost(vol->sem);
  }

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to the flash file system.                */
  /*-------------------------------------------------------------------*/
  semPend(FlashSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Clear out all the driver specific variables.                      */
  /*-------------------------------------------------------------------*/
  if (vol->type == FFS_NOR)
  {
    vol->driver.nor.write_byte = NULL;
    vol->driver.nor.write_page = vol->driver.nor.read_page = NULL;
    vol->driver.nor.erase_block = NULL;
  }
  else
  {
    vol->driver.nand.write_page = NULL;
    vol->driver.nand.page_erased = NULL;
    vol->driver.nand.read_type = NULL;
    vol->driver.nand.read_page = NULL;
    vol->driver.nand.erase_block = NULL;
  }
  vol->ctrl_size = vol->block_size = vol->num_sects = 0;
  vol->mem_base = 0;

  /*-------------------------------------------------------------------*/
  /* Decrement the fixed volume count.                                 */
  /*-------------------------------------------------------------------*/
  --CurrFixedVols;

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to flash file system and this volume.    */
  /*-------------------------------------------------------------------*/
  semPost(FlashSem);
  semPost(vol->sem);
  return 0;
}

/***********************************************************************/
/* FlashAddVol: Add a new flash volume                                 */
/*                                                                     */
/*      Inputs: driver = pointer to the driver control block           */
/*              type = type of volume (FFS_NOR or FFS_NAND)            */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
int FlashAddVol(const FfsVol *driver, int type)
{
  int i, r_value = -1;
  FlashGlob *vol;
  Module fp;

  /*-------------------------------------------------------------------*/
  /* Check for invalid name, too few blocks, bad page, or block size.  */
  /*-------------------------------------------------------------------*/
  if (InvalidName(driver->name, FALSE) || (driver->num_blocks < 4) ||
      (driver->block_size % driver->page_size))
  {
    set_errno(EINVAL);
    return r_value;
  }

  /*-------------------------------------------------------------------*/
  /* For NAND, check max_bad_blocks.                                   */
  /*-------------------------------------------------------------------*/
  if (type == FFS_NAND && driver->num_blocks <= driver->max_bad_blocks)
  {
    set_errno(EINVAL);
    return r_value;
  }

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to upper layer and flash file system.    */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);
  semPend(FlashSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Ensure there is no name duplication. Look through all modules.    */
  /*-------------------------------------------------------------------*/
  for (i = 0; ; ++i)
  {
    fp = ModuleList[i];
    if (fp == NULL)
      break;

    if (fp(kVolName, driver->name))
    {
      set_errno(EEXIST);
      goto end;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Look for an empty entry in the FlashGlobals array to place new    */
  /* volume controls.                                                  */
  /*-------------------------------------------------------------------*/
  for (i = 0;; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* If no space, set errno and go to exit.                          */
    /*-----------------------------------------------------------------*/
    if (i == NUM_FFS_VOLS)
    {
      set_errno(ENOMEM);
      goto end;
    }

    if (FlashGlobals[i].num_sects == 0)
      break;
  }

  /*-------------------------------------------------------------------*/
  /* Check we haven't reached max allowed fixed volumes.               */
  /*-------------------------------------------------------------------*/
  if (CurrFixedVols >= MAX_FIXED_VOLS)
  {
    set_errno(ENOMEM);
    goto end;
  }

  /*-------------------------------------------------------------------*/
  /* Get pointer to volume entry in the FlashGlobals array.            */
  /*-------------------------------------------------------------------*/
  vol = &FlashGlobals[i];

  /*-------------------------------------------------------------------*/
  /* Block size, number of blocks, beginning of flash, page size, and  */
  /* driver's vol pointer are driver-specific values.                  */
  /*-------------------------------------------------------------------*/
  vol->block_size = driver->block_size;
  vol->num_blocks = driver->num_blocks;
  vol->mem_base = driver->mem_base;
  vol->page_size = driver->page_size;
  vol->vol = driver->vol;

  /*-------------------------------------------------------------------*/
  /* Max erase set is 1/10 of total number of blocks.                  */
  /*-------------------------------------------------------------------*/
  vol->max_set_blocks = vol->num_blocks / 10;
  if (vol->max_set_blocks == 0)
    vol->max_set_blocks = 1;

  /*-------------------------------------------------------------------*/
  /* Set the sys node so it can later be added to the mounted list.    */
  /*-------------------------------------------------------------------*/
  vol->sys.volume = vol;
  vol->sys.ioctl = FlashIoctl;
  if (strlen(driver->name) >= FILENAME_MAX)
  {
    set_errno(ENAMETOOLONG);
    goto end;
  }
  strcpy(vol->sys.name, driver->name);

  /*-------------------------------------------------------------------*/
  /* Figure out the number of sectors and their size.                  */
  /*-------------------------------------------------------------------*/
  vol->num_sects = vol->block_size / vol->page_size * vol->num_blocks;
  vol->sect_sz = vol->page_size;
  while (vol->num_sects > FFS_MAX_SIZE)
  {
    vol->num_sects /= 2;
    vol->sect_sz *= 2;
  }
  vol->pages_per_sect = vol->sect_sz / vol->page_size;
  vol->block_sects = vol->block_size / vol->sect_sz;

  /*-------------------------------------------------------------------*/
  /* Set the erase set size.                                           */
  /*-------------------------------------------------------------------*/
  vol->set_blocks = (FLASH_SET_LIMIT + vol->block_sects - 1) /
                    vol->block_sects;

  /*-------------------------------------------------------------------*/
  /* Set the vol type, free_sects_used & comm_ptr (used to figure out  */
  /* when to recompute size of control info when syncs disabled) and   */
  /* the volume ID (used for FSUID).                                   */
  /*-------------------------------------------------------------------*/
  vol->type = (ui8)type;
  vol->free_sects_used = 0;
  vol->comm_ptr = NULL;
  vol->vid = CurrFixedVols++;

#if QUOTA_ENABLED
  /*-------------------------------------------------------------------*/
  /* Set the quota enabled flag.                                       */
  /*-------------------------------------------------------------------*/
  if (driver->flag & FFS_QUOTA_ENABLED)
    vol->quota_enabled = TRUE;
  else
    vol->quota_enabled = FALSE;
#endif /* QUOTA_ENABLED */

  /*-------------------------------------------------------------------*/
  /* Based on the type of volume, set specific info.                   */
  /*-------------------------------------------------------------------*/
  if (type == FFS_NOR)
  {
#if INC_NOR_FS
    /*-----------------------------------------------------------------*/
    /* Set the driver functions.                                       */
    /*-----------------------------------------------------------------*/
    vol->driver.nor.write_byte = driver->driver.nor.write_byte;
    vol->driver.nor.write_page = driver->driver.nor.write_page;
    vol->driver.nor.page_erased = driver->driver.nor.page_erased;
    vol->driver.nor.read_page = driver->driver.nor.read_page;
    vol->driver.nor.erase_block = driver->driver.nor.erase_block;

    /*-----------------------------------------------------------------*/
    /* Set memory dependent function handles.                          */
    /*-----------------------------------------------------------------*/
    vol->write_ctrl = FsNorWriteCtrl;
    vol->write_sector = FsNorWriteSect;
    vol->read_sector = FsNorReadSect;
    vol->erase_block_wrapper = FsNorEraseBlockWrapper;
    vol->unformat_to_format = FsNorUnformatToFormat;
    vol->find_last_ctrl = FsNorFindLastCtrl;
    vol->empty_sect = FsNorEmptySect;

    /*-----------------------------------------------------------------*/
    /* Set number of header sectors and bad blocks.                    */
    /*-----------------------------------------------------------------*/
    vol->hdr_sects = (vol->block_sects + vol->sect_sz - 1) /vol->sect_sz;
    vol->max_bad_blocks = 0;
#endif
  }
  else if (type == FFS_NAND)
  {
#if INC_NAND_FS
    /*-----------------------------------------------------------------*/
    /* Set the driver functions.                                       */
    /*-----------------------------------------------------------------*/
    vol->driver.nand.write_page = driver->driver.nand.write_page;
    vol->driver.nand.write_type = driver->driver.nand.write_type;
    vol->driver.nand.page_erased = driver->driver.nand.page_erased;
    vol->driver.nand.read_type = driver->driver.nand.read_type;
    vol->driver.nand.read_page = driver->driver.nand.read_page;
    vol->driver.nand.erase_block = driver->driver.nand.erase_block;

    /*-----------------------------------------------------------------*/
    /* Set memory dependent function handles.                          */
    /*-----------------------------------------------------------------*/
    vol->write_ctrl = FsNandWriteCtrl;
    vol->write_sector = FsNandWriteSect;
    vol->read_sector = FsNandReadSect;
    vol->erase_block_wrapper = FsNandEraseBlockWrapper;
    vol->unformat_to_format = FsNandUnformatToFormat;
    vol->find_last_ctrl = FsNandFindLastCtrl;
    vol->empty_sect = FsNandEmptySect;

    /*-----------------------------------------------------------------*/
    /* Set number of header sectors and bad blocks.                    */
    /*-----------------------------------------------------------------*/
    vol->hdr_sects = 0;
    vol->max_bad_blocks = driver->max_bad_blocks;
#endif
  }
  else
  {
#if INC_MLC_FS
    /*-----------------------------------------------------------------*/
    /* Set the driver functions.                                       */
    /*-----------------------------------------------------------------*/
    vol->driver.mlc.write_byte = driver->driver.mlc.write_byte;
    vol->driver.mlc.write_page = driver->driver.mlc.write_page;
    vol->driver.mlc.page_erased = driver->driver.mlc.page_erased;
    vol->driver.mlc.read_page = driver->driver.mlc.read_page;
    vol->driver.mlc.erase_block = driver->driver.mlc.erase_block;

    /*-----------------------------------------------------------------*/
    /* Set memory dependent function handles.                          */
    /*-----------------------------------------------------------------*/
    vol->write_ctrl = FsMlcWriteCtrl;
    vol->write_sector = FsMlcWriteSect;
    vol->read_sector = FsMlcReadSect;
    vol->erase_block_wrapper = FsMlcEraseBlockWrapper;
    vol->unformat_to_format = FsMlcUnformatToFormat;
    vol->find_last_ctrl = FsMlcFindLastCtrl;
    vol->empty_sect = FsMlcEmptySect;

    /*-----------------------------------------------------------------*/
    /* Set number of header sectors and bad blocks.                    */
    /*-----------------------------------------------------------------*/
    vol->hdr_sects = 0;
    vol->max_bad_blocks = 0;
#endif
  }

  /*-------------------------------------------------------------------*/
  /* Set the return value to success.                                  */
  /*-------------------------------------------------------------------*/
  r_value = 0;

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to upper layer and flash file system.    */
  /*-------------------------------------------------------------------*/
end:
  semPost(FileSysSem);
  semPost(FlashSem);
  return r_value;
}
#endif /* NUM_FFS_VOLS */

