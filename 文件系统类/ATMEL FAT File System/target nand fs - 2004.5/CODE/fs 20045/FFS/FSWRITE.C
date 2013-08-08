/***********************************************************************/
/*                                                                     */
/*   Module:  fswrite.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2004.5                                                   */
/*   Purpose: Implements the function for writing bytes to a file      */
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

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*  FlashWrite: Write len number of bytes to file                      */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              buf    = buffer that holds the bytes to be written     */
/*              len    = number of bytes to be written                 */
/*                                                                     */
/*     Returns: Number of bytes successfully written, -1 on error      */
/*                                                                     */
/***********************************************************************/
int FlashWrite(FILE *stream, const ui8 *buf, ui32 len)
{
  int  skip_read, adjust_size, status, check_recycles = FALSE;
  ui32 remaining = len, num_sectors, i, space_left, sect_num;
  ui8  *sector, dirty_flag;
  FFIL_T *link_ptr = &((FFSEnt *)stream->handle)->entry.file;

#if QUOTA_ENABLED
  int adjust_quota = TRUE;
#endif /* QUOTA_ENABLED */

  /*-------------------------------------------------------------------*/
  /* If the file mode does not permit writing, return error.           */
  /*-------------------------------------------------------------------*/
  if ((link_ptr->comm->open_mode & (F_APPEND | F_WRITE)) == 0)
  {
    set_errno(EACCES);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* If file is in append mode, writes are always at the end.          */
  /*-------------------------------------------------------------------*/
  if (link_ptr->comm->open_mode & F_APPEND)
    FlashPoint2End(&stream->curr_ptr, link_ptr->comm);

  /*-------------------------------------------------------------------*/
  /* Else if a seek past end was performed, validate it.               */
  /*-------------------------------------------------------------------*/
  else if (stream->curr_ptr.sector == FDRTY_SECT &&
           FlashSeekPastEnd((i32)(stream->curr_ptr.sect_off *
                            Flash->sect_sz + stream->curr_ptr.offset -
                            link_ptr->comm->size), stream, ADJUST_FCBs))
    return -1;

  /*-------------------------------------------------------------------*/
  /* If curr_ptr is valid, fill out either len bytes or what's left of */
  /* the current sector, whichever is smaller.                         */
  /*-------------------------------------------------------------------*/
  if (stream->curr_ptr.sector != (ui16)-1)
  {
    /*-----------------------------------------------------------------*/
    /* NAND/MLC creatn files should skip cache reads.                  */
    /*-----------------------------------------------------------------*/
    if (Flash->type != FFS_NOR && (link_ptr->comm->mode & S_CREATN))
      skip_read = TRUE;
    else
      skip_read = FALSE;

    /*-----------------------------------------------------------------*/
    /* If stream does not have a cache entry associated with it, get   */
    /* the sector from the cache.                                      */
    /*-----------------------------------------------------------------*/
    if (stream->cached == NULL)
    {
      status = GetSector(&Flash->cache, (int)stream->curr_ptr.sector,
                         skip_read, link_ptr->comm, &stream->cached);
      if (status != GET_OK)
      {
        if (stream->cached)
          FreeSector(&stream->cached, &Flash->cache);
        return -1;
      }
    }
    PfAssert(stream->cached->file_ptr == link_ptr->comm);

    /*-----------------------------------------------------------------*/
    /* For creatn() files, NOR always uses dirty new sectors, NAND uses*/
    /* dirty new if first time writing to sector, else uses dirty old. */
    /*-----------------------------------------------------------------*/
    if (link_ptr->comm->mode & S_CREATN)
    {
      /*---------------------------------------------------------------*/
      /* For NAND/MLC check it's truly empty because can't write over. */
      /*---------------------------------------------------------------*/
      if (Flash->type != FFS_NOR &&
          ((CacheEntry*)stream->cached)->dirty == CLEAN &&
          !Flash->empty_sect(stream->curr_ptr.sector))
      {
        --Flash->free_sects;
        ((CacheEntry *)stream->cached)->dirty = DIRTY_OLD;
        Flash->cache.dirty_old = TRUE;
        stream->flags |= FCB_MOD;

        /*-------------------------------------------------------------*/
        /* Check for recycles when writing over old data.              */
        /*-------------------------------------------------------------*/
        check_recycles = TRUE;
      }

      /*---------------------------------------------------------------*/
      /* Else creatn() files should not use new sectors.               */
      /*---------------------------------------------------------------*/
      else
      {
        ((CacheEntry *)stream->cached)->dirty = DIRTY_NEW;
        Flash->cache.dirty_new = TRUE;
      }
    }

    /*-----------------------------------------------------------------*/
    /* For normal files, if FlashWrprWr() uses new sector, keep track  */
    /* of it and set sector to dirty old because we're writing over it.*/
    /* Set the stream modified flag if we're using a free sector.      */
    /*-----------------------------------------------------------------*/
    else if (((CacheEntry *)stream->cached)->dirty == CLEAN)
    {
      --Flash->free_sects;
      ((CacheEntry *)stream->cached)->dirty = DIRTY_OLD;
      Flash->cache.dirty_old = TRUE;
      stream->flags |= FCB_MOD;

      /*---------------------------------------------------------------*/
      /* Check for recycles when writing over old data.                */
      /*---------------------------------------------------------------*/
      check_recycles = TRUE;
    }

    /*-----------------------------------------------------------------*/
    /* Get the pointer to the sector data.                             */
    /*-----------------------------------------------------------------*/
    sector = ((CacheEntry *)stream->cached)->sector;

    /*-----------------------------------------------------------------*/
    /* Figure out how much space is left in current sector.            */
    /*-----------------------------------------------------------------*/
    space_left = (ui32)(Flash->sect_sz - stream->curr_ptr.offset);

    /*-----------------------------------------------------------------*/
    /* If what has to be written fits in this sector, write it and     */
    /* set remaining to 0 so that we're done.                          */
    /*-----------------------------------------------------------------*/
    if (space_left >= remaining)
    {
      /*---------------------------------------------------------------*/
      /* Use memcpy to copy from buf to sector remaining bytes.        */
      /*---------------------------------------------------------------*/
      memcpy(&sector[stream->curr_ptr.offset], buf, remaining);

      /*---------------------------------------------------------------*/
      /* Adjust current offset.                                        */
      /*---------------------------------------------------------------*/
      stream->curr_ptr.offset += (ui16)remaining;

      /*---------------------------------------------------------------*/
      /* Adjust remaining to 0 because we're done writing everything.  */
      /*---------------------------------------------------------------*/
      remaining = 0;

      /*---------------------------------------------------------------*/
      /* If one past last needs to be updated, update it.              */
      /*---------------------------------------------------------------*/
      if (link_ptr->comm->one_past_last.sector ==
                                               stream->curr_ptr.sector &&
          link_ptr->comm->one_past_last.offset < stream->curr_ptr.offset)
      {
        /*-------------------------------------------------------------*/
        /* Adjust file size.                                           */
        /*-------------------------------------------------------------*/
        link_ptr->comm->size += (stream->curr_ptr.offset -
                                 link_ptr->comm->one_past_last.offset);
        link_ptr->comm->one_past_last.offset =
                                           (ui16)stream->curr_ptr.offset;
      }

      /*---------------------------------------------------------------*/
      /* If recycles need to be checked, free sector.                  */
      /*---------------------------------------------------------------*/
      if (check_recycles && FreeSector(&stream->cached, &Flash->cache))
        return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Else the current sector has not enough room for all the bytes   */
    /* that need to be written. Write enough to fill this sector,      */
    /* adjust remaining and later continue writing sectors.            */
    /*-----------------------------------------------------------------*/
    else
    {
      /*---------------------------------------------------------------*/
      /* Use memcpy to copy from buf to sector space_left bytes.       */
      /*---------------------------------------------------------------*/
      memcpy(&sector[stream->curr_ptr.offset], buf, space_left);

      /*---------------------------------------------------------------*/
      /* Adjust number of bytes left to be written and buf pointer.    */
      /*---------------------------------------------------------------*/
      remaining -= space_left;
      buf += space_left;

      /*---------------------------------------------------------------*/
      /* Adjust current offset.                                        */
      /*---------------------------------------------------------------*/
      stream->curr_ptr.offset = 0;

      /*---------------------------------------------------------------*/
      /* If this is the last sector, adjust both current sector number */
      /* and one past last sector number.                              */
      /*---------------------------------------------------------------*/
      if (stream->curr_ptr.sector == link_ptr->comm->last_sect)
      {
        /*-------------------------------------------------------------*/
        /* Adjust file size.                                           */
        /*-------------------------------------------------------------*/
        if (link_ptr->comm->one_past_last.sector != (ui16)-1)
          link_ptr->comm->size += (Flash->sect_sz -
                                   link_ptr->comm->one_past_last.offset);
        stream->curr_ptr.sector = (ui16)-1;
        link_ptr->comm->one_past_last.sector = (ui16)-1;
        link_ptr->comm->one_past_last.offset = 0;
      }

      /*---------------------------------------------------------------*/
      /* Else set current sector number to the next sector.            */
      /*---------------------------------------------------------------*/
      else
      {
        stream->curr_ptr.sector =
                           Flash->sect_tbl[stream->curr_ptr.sector].next;
        ++stream->curr_ptr.sect_off;
      }

      /*---------------------------------------------------------------*/
      /* Free current sector because we're moving to new one. Also     */
      /* need to check for recycles at this point.                     */
      /*---------------------------------------------------------------*/
      if (FreeSector(&stream->cached, &Flash->cache))
        return -1;
      check_recycles = TRUE;
    }

    /*-----------------------------------------------------------------*/
    /* Check for recycles if we need to.                               */
    /*-----------------------------------------------------------------*/
    if (check_recycles && FlashRecChck(SKIP_CHECK_FULL) ==RECYCLE_FAILED)
      return (int)(len - remaining);
  }

  /*-------------------------------------------------------------------*/
  /* Figure out how many new sectors are needed to write what's left.  */
  /*-------------------------------------------------------------------*/
  num_sectors = (remaining + Flash->sect_sz - 1) / Flash->sect_sz;

  /*-------------------------------------------------------------------*/
  /* Loop through the number of sectors, either getting existing ones  */
  /* or creating new ones.                                             */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < num_sectors; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* The cache entry should always be NULL because we're doing whole */
    /* sector writes.                                                  */
    /*-----------------------------------------------------------------*/
    PfAssert(stream->cached == NULL);

    /*-----------------------------------------------------------------*/
    /* If current pointer is at the end of the file, get new sector.   */
    /*-----------------------------------------------------------------*/
    if (stream->curr_ptr.sector == (ui16)-1)
    {
      /*---------------------------------------------------------------*/
      /* If file was created by creatn(), cannot expand it.            */
      /*---------------------------------------------------------------*/
      if (link_ptr->comm->mode & S_CREATN)
      {
        set_errno(ENOSPC);
        return (int)(len - remaining);
      }

#if QUOTA_ENABLED
      /*---------------------------------------------------------------*/
      /* If quotas enabled, check if new sector(s) can be added to file*/
      /*---------------------------------------------------------------*/
      if (Flash->quota_enabled && adjust_quota)
      {
        if (link_ptr->parent_dir->entry.dir.free < (num_sectors - i) *
            Flash->sect_sz)
        {
          set_errno(ENOMEM);
          return (int)(len - remaining);
        }
      }
#endif /* QUOTA_ENABLED */

      /*---------------------------------------------------------------*/
      /* Set free sector to point where we can write to it.            */
      /*---------------------------------------------------------------*/
      if (FlashFrSect(link_ptr->comm))
        return (int)(len - remaining);

      /*---------------------------------------------------------------*/
      /* Set the dirty flag to dirty new because it's a new sector.    */
      /* Also, set the stream modified flag.                           */
      /*---------------------------------------------------------------*/
      dirty_flag = DIRTY_NEW;
      Flash->cache.dirty_new = TRUE;
      stream->flags |= FCB_MOD;

      /*---------------------------------------------------------------*/
      /* Reserve a free sector.                                        */
      /*---------------------------------------------------------------*/
      sect_num = Flash->free_sect;
      if (Flash->sect_tbl[sect_num].next == FLAST_SECT)
      {
        PfAssert(FALSE); /*lint !e506, !e774*/
        return -1;
      }
      Flash->free_sect = Flash->sect_tbl[sect_num].next;
      --Flash->free_sects;
      PfAssert(Flash->sect_tbl[sect_num].prev == FFREE_SECT);

      /*---------------------------------------------------------------*/
      /* This sector will be used, so mark both the total and block    */
      /* used counts.                                                  */
      /*---------------------------------------------------------------*/
      ++Flash->used_sects;
      ++Flash->blocks[sect_num / Flash->block_sects].used_sects;

      /*---------------------------------------------------------------*/
      /* Set the free sector to LAST because this sector will be last  */
      /* sector in the list of sectors belonging to the file.          */
      /*---------------------------------------------------------------*/
      Flash->sect_tbl[sect_num].next = FLAST_SECT;

      /*---------------------------------------------------------------*/
      /* If the file is empty, set first sector to sect_num, else set  */
      /* the current last to point to new last.                        */
      /*---------------------------------------------------------------*/
      if (link_ptr->comm->frst_sect == FFREE_SECT)
      {
        link_ptr->comm->frst_sect = (ui16)sect_num;
        Flash->sect_tbl[sect_num].prev = FLAST_SECT;
        FlashUpdateFCBs(stream, link_ptr->comm);
      }
      else
      {
        /*-------------------------------------------------------------*/
        /* Set the entry in the sector table for the last sector.      */
        /*-------------------------------------------------------------*/
        Flash->sect_tbl[link_ptr->comm->last_sect].next = (ui16)sect_num;
        Flash->sect_tbl[sect_num].prev = link_ptr->comm->last_sect;

        /*-------------------------------------------------------------*/
        /* Increment current number of sectors here and not outside    */
        /* the if-else because we only increment for non-zero files,   */
        /* that is for files for which we're not creating the first    */
        /* sector. The reason being that sect_off = 0 identifies both  */
        /* an empty file and a file with 1 sector.                     */
        /*-------------------------------------------------------------*/
        ++stream->curr_ptr.sect_off;
      }

      /*---------------------------------------------------------------*/
      /* Update last sector pointer.                                   */
      /*---------------------------------------------------------------*/
      link_ptr->comm->last_sect = (ui16)sect_num;

      /*---------------------------------------------------------------*/
      /* Update current pointer and one past last.                     */
      /*---------------------------------------------------------------*/
      link_ptr->comm->one_past_last.sector = (ui16)sect_num;
      link_ptr->comm->one_past_last.offset = 0;
      stream->curr_ptr.sector = (ui16)sect_num;
      stream->curr_ptr.offset = 0;

      /*---------------------------------------------------------------*/
      /* Set the add size factor to 1 so that file size increases.     */
      /*---------------------------------------------------------------*/
      adjust_size = TRUE;

      /*---------------------------------------------------------------*/
      /* Skip reading because it's a new sector.                       */
      /*---------------------------------------------------------------*/
      skip_read = TRUE;

#if QUOTA_ENABLED
      /*---------------------------------------------------------------*/
      /* If quotas enabled, adjust quota values due to new sector(s).  */
      /*---------------------------------------------------------------*/
      if (Flash->quota_enabled && adjust_quota)
      {
        FFSEnt *ent, *root = &Flash->files_tbl->tbl[0];

        /*-------------------------------------------------------------*/
        /* Update used from parent to root.                            */
        /*-------------------------------------------------------------*/
        for (ent = link_ptr->parent_dir; ent;
             ent = ent->entry.dir.parent_dir)
        {
          PfAssert(ent->type == FDIREN);
          ent->entry.dir.used += (num_sectors - i) * Flash->sect_sz;
        }

        /*-------------------------------------------------------------*/
        /* Update free below.                                          */
        /*-------------------------------------------------------------*/
        FlashFreeBelow(root);

        /*-------------------------------------------------------------*/
        /* Recompute free at each node.                                */
        /*-------------------------------------------------------------*/
        FlashFree(root, root->entry.dir.max_q - FlashVirtUsed(root));

        /*-------------------------------------------------------------*/
        /* Set flag to skip adjusting since we've done it here.        */
        /*-------------------------------------------------------------*/
        adjust_quota = FALSE;
      }
#endif /* QUOTA_ENABLED */
    }

    /*-----------------------------------------------------------------*/
    /* Else we're writing over old stuff, get the next sector.         */
    /*-----------------------------------------------------------------*/
    else
    {
      sect_num = (ui32)stream->curr_ptr.sector;

      /*---------------------------------------------------------------*/
      /* Set the dirty flag to dirty old because it's an old sector,   */
      /* unless it's a creatn() file, in which case it's as new. Also, */
      /* for dirty old sectors, set stream flag to modified.           */
      /*---------------------------------------------------------------*/
      if (link_ptr->comm->mode & S_CREATN)
      {
        /*-------------------------------------------------------------*/
        /* For NAND/MLC ensure sector hasn't previously been written.  */
        /*-------------------------------------------------------------*/
        if (Flash->type != FFS_NOR && !Flash->empty_sect(sect_num))
        {
          stream->flags |= FCB_MOD;
          dirty_flag = DIRTY_OLD;
          Flash->cache.dirty_old = TRUE;
        }
        else
        {
          dirty_flag = DIRTY_NEW;
          Flash->cache.dirty_new = TRUE;
        }
      }
      else
      {
        stream->flags |= FCB_MOD;
        dirty_flag = DIRTY_OLD;
        Flash->cache.dirty_old = TRUE;
      }

      /*---------------------------------------------------------------*/
      /* Set "adjust size" flag only if we go past the end of file.    */
      /*---------------------------------------------------------------*/
      adjust_size = (remaining > link_ptr->comm->one_past_last.offset) &&
       (link_ptr->comm->one_past_last.sector == stream->curr_ptr.sector);

      /*---------------------------------------------------------------*/
      /* Set "skip read" flag if able to skip reading sector into cache*/
      /*---------------------------------------------------------------*/
      skip_read = (remaining >= Flash->sect_sz);
    }

    /*-----------------------------------------------------------------*/
    /* NAND/MLC creatn files should skip cache reads.                  */
    /*-----------------------------------------------------------------*/
    if (Flash->type != FFS_NOR && (link_ptr->comm->mode & S_CREATN))
      skip_read = TRUE;

    /*-----------------------------------------------------------------*/
    /* Get pointer to the sector.                                      */
    /*-----------------------------------------------------------------*/
    status = GetSector(&Flash->cache, (int)sect_num, skip_read,
                       link_ptr->comm, &stream->cached);
    if (status != GET_OK)
    {
      if (stream->cached && FreeSector(&stream->cached, &Flash->cache))
        return -1;
      return (int)(len - remaining);
    }
    PfAssert(stream->cached->file_ptr == link_ptr->comm);

    /*-----------------------------------------------------------------*/
    /* If FlashWrprWr() will use a new sector, keep track of it and    */
    /* set the dirty flag for this sector.                             */
    /*-----------------------------------------------------------------*/
    if (((CacheEntry *)stream->cached)->dirty == CLEAN)
    {
      if (dirty_flag == DIRTY_OLD)
        --Flash->free_sects;
      ((CacheEntry *)stream->cached)->dirty = dirty_flag;
    }

    /*-----------------------------------------------------------------*/
    /* Get the pointer to the sector data.                             */
    /*-----------------------------------------------------------------*/
    sector = ((CacheEntry *)stream->cached)->sector;

    /*-----------------------------------------------------------------*/
    /* If what's left fits into this sector, it's the last sector to   */
    /* be written (maybe only partially).                              */
    /*-----------------------------------------------------------------*/
    if (remaining <= Flash->sect_sz)
    {
      /*---------------------------------------------------------------*/
      /* Use memcpy to copy everything that's left into current sect.  */
      /*---------------------------------------------------------------*/
      memcpy(sector, buf, remaining);

      /*---------------------------------------------------------------*/
      /* If we've written a whole sector, current pointer will be -1,  */
      /* else it will have the right sector number and offset.         */
      /*---------------------------------------------------------------*/
      if (remaining == Flash->sect_sz)
      {
        if (stream->curr_ptr.sector == link_ptr->comm->last_sect)
          stream->curr_ptr.sector = (ui16)-1;
        else
        {
          stream->curr_ptr.sector = Flash->sect_tbl[sect_num].next;
          ++stream->curr_ptr.sect_off;
        }
        stream->curr_ptr.offset = 0;

        /*-------------------------------------------------------------*/
        /* If one past last needs to be modified, do it.               */
        /*-------------------------------------------------------------*/
        if (sect_num == link_ptr->comm->last_sect)
        {
          /*-----------------------------------------------------------*/
          /* Adjust file size.                                         */
          /*-----------------------------------------------------------*/
          link_ptr->comm->size += adjust_size * (Flash->sect_sz -
                                  link_ptr->comm->one_past_last.offset);
          link_ptr->comm->one_past_last.offset = 0;
          link_ptr->comm->one_past_last.sector = (ui16)-1;
        }

        /*-------------------------------------------------------------*/
        /* Free current sector because we've filled it.                */
        /*-------------------------------------------------------------*/
        if (FreeSector(&stream->cached, &Flash->cache))
          return -1;
      }
      else
      {
        stream->curr_ptr.offset = (ui16)remaining;

        /*-------------------------------------------------------------*/
        /* If one past last needs to be modified, modify it.           */
        /*-------------------------------------------------------------*/
        if (stream->curr_ptr.sector ==
                                  link_ptr->comm->one_past_last.sector &&
            stream->curr_ptr.offset >
                                    link_ptr->comm->one_past_last.offset)
        {
          /*-----------------------------------------------------------*/
          /* Adjust file size.                                         */
          /*-----------------------------------------------------------*/
          link_ptr->comm->size += (stream->curr_ptr.offset -
                                   link_ptr->comm->one_past_last.offset);
          link_ptr->comm->one_past_last.offset =
                                           (ui16)stream->curr_ptr.offset;
        }
      }
    }

    /*-----------------------------------------------------------------*/
    /* Else have to write the whole sector.                            */
    /*-----------------------------------------------------------------*/
    else
    {
      /*---------------------------------------------------------------*/
      /* Use memcpy to write the whole sector.                         */
      /*---------------------------------------------------------------*/
      memcpy(sector, buf, Flash->sect_sz);

      /*---------------------------------------------------------------*/
      /* Update remaining and buf.                                     */
      /*---------------------------------------------------------------*/
      remaining -= Flash->sect_sz;
      buf = (void *)((ui32)buf + Flash->sect_sz);

      /*---------------------------------------------------------------*/
      /* If we're at the end of the file, from now on get new sectors. */
      /*---------------------------------------------------------------*/
      if (sect_num == link_ptr->comm->last_sect)
      {
        /*-------------------------------------------------------------*/
        /* Adjust file size.                                           */
        /*-------------------------------------------------------------*/
        link_ptr->comm->size += adjust_size * (Flash->sect_sz -
                                link_ptr->comm->one_past_last.offset);
        stream->curr_ptr.sector = (ui16)-1;
        stream->curr_ptr.offset = 0;
        link_ptr->comm->one_past_last.sector = (ui16)-1;
        link_ptr->comm->one_past_last.offset = 0;
      }

      /*---------------------------------------------------------------*/
      /* Else go to the next sector in the file.                       */
      /*---------------------------------------------------------------*/
      else
      {
        stream->curr_ptr.sector = Flash->sect_tbl[sect_num].next;
        stream->curr_ptr.offset = 0;
        ++stream->curr_ptr.sect_off;
      }

      /*---------------------------------------------------------------*/
      /* Free current sector because it's full.                        */
      /*---------------------------------------------------------------*/
      if (FreeSector(&stream->cached, &Flash->cache))
        return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Check for recycles when writing over old data.                  */
    /*-----------------------------------------------------------------*/
    if (dirty_flag == DIRTY_OLD &&
        FlashRecChck(SKIP_CHECK_FULL) == RECYCLE_FAILED)
      return (int)(len - remaining);
  }

  /*-------------------------------------------------------------------*/
  /* Set access and modification times and return length written.      */
  /*-------------------------------------------------------------------*/
  link_ptr->comm->mod_time = link_ptr->comm->ac_time = OsSecCount;
  return (int)len;
}
#endif /* NUM_FFS_VOLS */

