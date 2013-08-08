/***********************************************************************/
/*                                                                     */
/*   Module:  fsioctl.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2004.16                                                  */
/*   Purpose: Implements flash file system ioctl()                     */
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
#include <stddef.h>
#include "flashfsp.h"

#if NUM_FFS_VOLS

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
#define F_ONLY             0
#define F_AND_D            1

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
static int  check_name(const char *name);
static void *seek_within_file(FILE *stream, i32 offset, i32 curr_pos);
static void *seek_up(i32 offset, FILE *stream, fpos_t ptr);
static int find_file(const FDIR_T *dir, const char *path, int mode,
                     FFSEnt **entry, int search_flag);
static ui32 unique_fileno(void);

#if FFS_DEBUG
int printf(const char *, ...);
#endif

/***********************************************************************/
/* Local Function Definitions                                          */
/***********************************************************************/

/***********************************************************************/
/*   truncatef: Truncate the length of a file to desired length        */
/*                                                                     */
/*      Inputs: entry = pointer to file in Flash->files_tbl            */
/*              length = desired file length                           */
/*                                                                     */
/*     Returns: NULL on success, (void *)-1 otherwise                  */
/*                                                                     */
/***********************************************************************/
static void *truncatef(FFSEnt *entry, off_t length)
{
  ui16 sect, last_sect_offset;
  ui32  count, num_sects, i;
  FFIL_T *lnk = &entry->entry.file;

#if QUOTA_ENABLED
  ui32 rmv_sects = 0;
#endif /* QUOTA_ENABLED */

  /*-------------------------------------------------------------------*/
  /* Just return if the actual length equals the desired length.       */
  /*-------------------------------------------------------------------*/
  if (lnk->comm->size == length)
    return NULL;

  /*-------------------------------------------------------------------*/
  /* If length is 0, remove whole data content.                        */
  /*-------------------------------------------------------------------*/
  if (length == 0)
    FlashTruncZero(lnk, DONT_ADJUST_FCBs);

  /*-------------------------------------------------------------------*/
  /* Else if actual size smaller than length, append zeroes.           */
  /*-------------------------------------------------------------------*/
  else if (lnk->comm->size < length)
  {
    FILE fcb, *fcb_ptr;
    int append_set = FALSE;

    /*-----------------------------------------------------------------*/
    /* If file control block associated with file use it, else create  */
    /* one.                                                            */
    /*-----------------------------------------------------------------*/
    if (lnk->file != NULL)
      fcb_ptr = lnk->file;
    else
    {
      fcb_ptr = &fcb;
      FsInitFCB(fcb_ptr, FCB_FILE);
      fcb_ptr->handle = lnk;
      fcb_ptr->curr_ptr.sector = lnk->comm->frst_sect;
      fcb_ptr->curr_ptr.offset = 0;
      fcb_ptr->curr_ptr.sect_off = 0;
      fcb_ptr->old_ptr = fcb_ptr->curr_ptr;
      if (!(lnk->comm->open_mode & (F_APPEND | F_WRITE)))
      {
        lnk->comm->open_mode |= F_APPEND;
        append_set = TRUE;
      }
    }

    /*-----------------------------------------------------------------*/
    /* Increase file size by desired amount.                           */
    /*-----------------------------------------------------------------*/
    if (FlashSeekPastEnd((i32)(length - lnk->comm->size), fcb_ptr,
                         DONT_ADJUST_FCBs))
      return (void *)-1;

    /*-----------------------------------------------------------------*/
    /* If open mode set here, reset it.                                */
    /*-----------------------------------------------------------------*/
    if (append_set)
      lnk->comm->open_mode &= ~F_APPEND;
  }

  /*-------------------------------------------------------------------*/
  /* Else actual size is greater than desired size, truncate.          */
  /*-------------------------------------------------------------------*/
  else
  {
    /*-----------------------------------------------------------------*/
    /* Figure out how many sectors still in file and offset of last    */
    /* sector.                                                         */
    /*-----------------------------------------------------------------*/
    num_sects = (length + Flash->sect_sz - 1) / Flash->sect_sz;
    last_sect_offset = length - (num_sects - 1) * Flash->sect_sz;

    /*-----------------------------------------------------------------*/
    /* Go to last new sector.                                          */
    /*-----------------------------------------------------------------*/
    sect = lnk->comm->frst_sect;
    for (count = 1; count < num_sects; ++count)
      sect = Flash->sect_tbl[sect].next;

    /*-----------------------------------------------------------------*/
    /* Update file's last sector.                                      */
    /*-----------------------------------------------------------------*/
    lnk->comm->last_sect = sect;
    sect = Flash->sect_tbl[sect].next;

    /*-----------------------------------------------------------------*/
    /* Everything past it becomes dirty.                               */
    /*-----------------------------------------------------------------*/
    for (; sect != FLAST_SECT; sect = Flash->sect_tbl[sect].next)
    {
      /*---------------------------------------------------------------*/
      /* First remove sector from cache.                               */
      /*---------------------------------------------------------------*/
      RemoveFromCache(&Flash->cache, sect, -1);

      /*---------------------------------------------------------------*/
      /* Adjust used_sects and blocks table.                           */
      /*---------------------------------------------------------------*/
      --Flash->used_sects;
      --Flash->blocks[sect / Flash->block_sects].used_sects;

      /*---------------------------------------------------------------*/
      /* Mark sector as dirty.                                         */
      /*---------------------------------------------------------------*/
      Flash->sect_tbl[sect].prev = FDRTY_SECT;

      /*---------------------------------------------------------------*/
      /* Adjust next set of blocks to be erased.                       */
      /*---------------------------------------------------------------*/
      if (Flash->sect_tbl[sect].next / Flash->block_sects !=
          sect / Flash->block_sects)
        FlashAdjustEraseSet(sect / Flash->block_sects);

#if QUOTA_ENABLED
      /*---------------------------------------------------------------*/
      /* If quotas enabled, count the number of removed sectors.       */
      /*---------------------------------------------------------------*/
      if (Flash->quota_enabled)
        ++rmv_sects;
#endif /* QUOTA_ENABLED */
    }

    /*-----------------------------------------------------------------*/
    /* Update file's one past last pointer.                            */
    /*-----------------------------------------------------------------*/
    Flash->sect_tbl[lnk->comm->last_sect].next = FLAST_SECT;
    if (last_sect_offset == Flash->sect_sz)
    {
      lnk->comm->one_past_last.sector = (ui16)-1;
      lnk->comm->one_past_last.offset = 0;
    }
    else
    {
      lnk->comm->one_past_last.sector = lnk->comm->last_sect;
      lnk->comm->one_past_last.offset = last_sect_offset;
    }

    /*-----------------------------------------------------------------*/
    /* Update file's size.                                             */
    /*-----------------------------------------------------------------*/
    lnk->comm->size = (num_sects - 1) * Flash->sect_sz +
                       last_sect_offset;

    /*-----------------------------------------------------------------*/
    /* If there are any file descriptors for which curr_ptr/old_ptr    */
    /* need to be updated because of the truncation, update them.      */
    /*-----------------------------------------------------------------*/
    for (i = 0; i < FOPEN_MAX; ++i)
      if (Files[i].handle == lnk && Files[i].ioctl == FlashIoctl)
      {
        /*-------------------------------------------------------------*/
        /* If curr_ptr points passed the new EOF, update it.           */
        /*-------------------------------------------------------------*/
        if (Files[i].curr_ptr.sector != FDRTY_SECT &&
            (Files[i].curr_ptr.sect_off >= num_sects ||
             (Files[i].curr_ptr.sect_off == num_sects - 1 &&
              Files[i].curr_ptr.offset > last_sect_offset)))
        {
          Files[i].curr_ptr.sector = FDRTY_SECT;
          Files[i].cached = NULL;
        }

        /*-------------------------------------------------------------*/
        /* If old_ptr points passed the new EOF, update it.            */
        /*-------------------------------------------------------------*/
        if (Files[i].old_ptr.sector != FDRTY_SECT &&
            (Files[i].old_ptr.sect_off >= num_sects ||
             (Files[i].old_ptr.sect_off == num_sects - 1 &&
              Files[i].old_ptr.offset > last_sect_offset)))
          Files[i].old_ptr.sector = FDRTY_SECT;
      }
  }

  /*-------------------------------------------------------------------*/
  /* Update file access and modified times.                            */
  /*-------------------------------------------------------------------*/
  lnk->comm->ac_time = lnk->comm->mod_time = OsSecCount;

#if QUOTA_ENABLED
  /*-------------------------------------------------------------------*/
  /* If quotas enabled, adjust quota values due to file truncation.    */
  /*-------------------------------------------------------------------*/
  if (Flash->quota_enabled)
  {
    FFSEnt *ent, *root = &Flash->files_tbl->tbl[0];

    /*-----------------------------------------------------------------*/
    /* Update used from parent to root.                                */
    /*-----------------------------------------------------------------*/
    for (ent = lnk->parent_dir; ent; ent = ent->entry.dir.parent_dir)
    {
      PfAssert(ent->type == FDIREN &&
               ent->entry.dir.used >= rmv_sects * Flash->sect_sz);
      ent->entry.dir.used -= rmv_sects * Flash->sect_sz;
    }

    /*-----------------------------------------------------------------*/
    /* Update free below.                                              */
    /*-----------------------------------------------------------------*/
    FlashFreeBelow(root);

    /*-----------------------------------------------------------------*/
    /* Recompute free at each node.                                    */
    /*-----------------------------------------------------------------*/
    FlashFree(root, root->entry.dir.max_q - FlashVirtUsed(root));
  }
#endif /* QUOTA_ENABLED */

  return NULL;
}

/***********************************************************************/
/*  remove_dir: Helper function to remove dir                          */
/*                                                                     */
/*       Input: dir_ent = directory to be removed                      */
/*                                                                     */
/*     Returns: NULL on success, (void *)-1 on error                   */
/*                                                                     */
/***********************************************************************/
static void *remove_dir(FFSEnt *dir_ent)
{
  FDIR_T *dir = &dir_ent->entry.dir;
  uid_t uid;
  gid_t gid;
  uint i;

  /*-------------------------------------------------------------------*/
  /* If we're trying to delete the root directory, return error.       */
  /*-------------------------------------------------------------------*/
  if (dir_ent == &Flash->files_tbl->tbl[0])
  {
    set_errno(ENOENT);
    return (void *)-1;
  }

  /*-------------------------------------------------------------------*/
  /* Check that parent directory has write permissions.                */
  /*-------------------------------------------------------------------*/
  if (CheckPerm(dir->parent_dir->entry.dir.comm, F_WRITE))
    return (void *)-1;

  /*-------------------------------------------------------------------*/
  /* Check owner is trying to remove directory.                        */
  /*-------------------------------------------------------------------*/
  FsGetId(&uid, &gid);
  if (uid != dir->comm->user_id)
  {
    set_errno(EPERM);
    return (void *)-1;
  }

  /*-------------------------------------------------------------------*/
  /* If dir is the only link and is not empty, return error.           */
  /*-------------------------------------------------------------------*/
  if (dir->first && dir->comm->links == 1)
  {
    set_errno(EEXIST);
    return (void *)-1;
  }

  /*-------------------------------------------------------------------*/
  /* If directory is open or if it's the current working directory of  */
  /* any task, return error.                                           */
  /*-------------------------------------------------------------------*/
  if (dir->dir || dir->cwds)
  {
    set_errno(EBUSY);
    return (void *)-1;
  }

  /*-------------------------------------------------------------------*/
  /* If an open directory structure is using this entry, update it.    */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < DIROPEN_MAX; ++i)
    if (Files[i].ioctl == FlashIoctl && Files[i].pos == dir_ent)
      Files[i].pos = dir->prev;

  /*-------------------------------------------------------------------*/
  /* Remove the temp dir from the Flash->files_tbl.                    */
  /*-------------------------------------------------------------------*/
  if (dir->prev)
    dir->prev->entry.dir.next = dir->next;
  else
    dir->parent_dir->entry.dir.first = dir->next;
  if (dir->next)
    dir->next->entry.dir.prev = dir->prev;

#if QUOTA_ENABLED
  /*-------------------------------------------------------------------*/
  /* If volume has quota enabled, update quota values.                 */
  /*-------------------------------------------------------------------*/
  if (Flash->quota_enabled)
  {
    ui32 used;
    FFSEnt *ent, *root = &Flash->files_tbl->tbl[0];

    /*-----------------------------------------------------------------*/
    /* Directory should be empty and no links should point to it.      */
    /*-----------------------------------------------------------------*/
    PfAssert(dir->first == NULL && dir->comm->links == 1);

    /*-----------------------------------------------------------------*/
    /* Check free below, reserved below and used.                      */
    /*-----------------------------------------------------------------*/
    PfAssert(dir->res_below == 0 && dir->free_below == 0 &&
             dir->used == 0);

    /*-----------------------------------------------------------------*/
    /* Update used from parent to root.                                */
    /*-----------------------------------------------------------------*/
    used = strlen(dir->name) + 1 + OFDIR_SZ + OFDIR_QUOTA_SZ + OFCOM_SZ;
    for (ent = dir->parent_dir; ent; ent = ent->entry.dir.parent_dir)
    {
      PfAssert(ent->type == FDIREN && ent->entry.dir.used >= used);
      ent->entry.dir.used -= used;
    }

    /*-----------------------------------------------------------------*/
    /* Update reserved below from parent to root.                      */
    /*-----------------------------------------------------------------*/
    for (ent = dir->parent_dir;; ent = ent->entry.dir.parent_dir)
    {
      PfAssert(ent->entry.dir.res_below >= dir->min_q);
      ent->entry.dir.res_below -= dir->min_q;

      /*---------------------------------------------------------------*/
      /* Stop when we reach a directory with a reservation because     */
      /* the directories above this one don't have their reserved below*/
      /* affected by directory removal.                                */
      /*---------------------------------------------------------------*/
      if (ent->entry.dir.min_q != 0)
        break;
    }

    /*-----------------------------------------------------------------*/
    /* Update free below.                                              */
    /*-----------------------------------------------------------------*/
    FlashFreeBelow(root);

    /*-----------------------------------------------------------------*/
    /* Recompute free at each node.                                    */
    /*-----------------------------------------------------------------*/
    FlashFree(root, root->entry.dir.max_q - FlashVirtUsed(root));

    /*-----------------------------------------------------------------*/
    /* Reset the quota values.                                         */
    /*-----------------------------------------------------------------*/
    dir->min_q = dir->max_q = dir->free = 0;
  }
#endif /* QUOTA_ENABLED */

  /*-------------------------------------------------------------------*/
  /* If this was the only link remove the file entry as well.          */
  /*-------------------------------------------------------------------*/
  if (--dir->comm->links == 0)
  {
    dir->comm->addr->type = FEMPTY;
    ++Flash->total_free;
    FlashIncrementTblEntries(dir->comm->addr);
    Flash->tbls_size -= OFCOM_SZ;
  }

  /*-------------------------------------------------------------------*/
  /* Clear out the entry for the removed directory.                    */
  /*-------------------------------------------------------------------*/
  dir_ent->type = FEMPTY;
  dir->next = dir->prev = dir->first = NULL;

  /*-------------------------------------------------------------------*/
  /* Adjust the number of free file entries.                           */
  /*-------------------------------------------------------------------*/
  ++Flash->total_free;
  FlashIncrementTblEntries(dir_ent);
  Flash->tbls_size -= (OFDIR_SZ + strlen(dir->name) + 1);
#if QUOTA_ENABLED
  if (Flash->quota_enabled)
    Flash->tbls_size -= OFDIR_QUOTA_SZ;
#endif /* QUOTA_ENABLED */

  /*-------------------------------------------------------------------*/
  /* Do garbage collection if necessary.                               */
  /*-------------------------------------------------------------------*/
  if (Flash->total_free >= (FNUM_ENT * 5 / 3))
    FlashGarbageCollect();

  /*-------------------------------------------------------------------*/
  /* Do a sync to save everything before returning.                    */
  /*-------------------------------------------------------------------*/
  return FlashSync(Flash->do_sync);
}

/***********************************************************************/
/* remove_file: Helper function to remove file                         */
/*                                                                     */
/*      Inputs: dir_ent = parent directory for file to be removed      */
/*              fname = name of file to be removed                     */
/*                                                                     */
/*     Returns: NULL on success, (void *)-1 on error                   */
/*                                                                     */
/***********************************************************************/
static void *remove_file(FFSEnt *dir_ent, const char *fname)
{
  FFIL_T *link_ptr;
  FFSEnt *tmp_ent;
  uid_t  uid;
  gid_t  gid;
  uint   i;
  int    rmv_file;
  FDIR_T *dir = &dir_ent->entry.dir;

#if QUOTA_ENABLED
  ui32 used = 0;
#endif /* QUOTA_ENABLED */

  /*-------------------------------------------------------------------*/
  /* Look for the file in the directory.                               */
  /*-------------------------------------------------------------------*/
  if (find_file(dir, fname, F_EXECUTE | F_WRITE, &tmp_ent, F_ONLY))
    return (void *)-1;
  if (tmp_ent == NULL)
  {
    set_errno(ENOENT);
    return (void *)-1;
  }
  link_ptr = &tmp_ent->entry.file;

  /*-------------------------------------------------------------------*/
  /* Ensure that owner is trying to remove file.                       */
  /*-------------------------------------------------------------------*/
  FsGetId(&uid, &gid);
  if (uid != tmp_ent->entry.file.comm->user_id)
  {
    set_errno(EPERM);
    return (void *)-1;
  }

  /*-------------------------------------------------------------------*/
  /* All the dirty entries in the cache that belong to this file must  */
  /* be cleaned. Also, for dirty old ones, increment number of free    */
  /* sectors.                                                          */
  /*-------------------------------------------------------------------*/
  if (Flash->cache.dirty_old || Flash->cache.dirty_new)
  {
    /*-----------------------------------------------------------------*/
    /* All dirty entries belonging to file must be cleared.            */
    /*-----------------------------------------------------------------*/
    Flash->cache.dirty_old = Flash->cache.dirty_new = FALSE;
    for (i = 0; i < Flash->cache.pool_size; ++i)
    {
      /*---------------------------------------------------------------*/
      /* If sector dirty, either it belongs to file or set dirty flags.*/
      /*---------------------------------------------------------------*/
      if (Flash->cache.pool[i].dirty != CLEAN)
      {
        /*-------------------------------------------------------------*/
        /* If it belongs to the file, clear it. If dirty old, bump up  */
        /* free sectors count.                                         */
        /*-------------------------------------------------------------*/
        if (Flash->cache.pool[i].file_ptr == link_ptr->comm)
        {
          if (Flash->cache.pool[i].dirty == DIRTY_OLD)
            ++Flash->free_sects;
          Flash->cache.pool[i].dirty = CLEAN;
        }

        /*-------------------------------------------------------------*/
        /* Else, mark the appropriate flag.                            */
        /*-------------------------------------------------------------*/
        else if (Flash->cache.pool[i].dirty == DIRTY_NEW)
          Flash->cache.dirty_new = TRUE;
        else
          Flash->cache.dirty_old = TRUE;
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* Determine whether the file to be removed is open or not.          */
  /*-------------------------------------------------------------------*/
  rmv_file = (link_ptr->file == NULL);

  /*-------------------------------------------------------------------*/
  /* Decrement the number of links for the file.                       */
  /*-------------------------------------------------------------------*/
  --link_ptr->comm->links;

  /*-------------------------------------------------------------------*/
  /* If this was the only link to the file and there are no open file  */
  /* descriptors associated with link, clear contents of file.         */
  /*-------------------------------------------------------------------*/
  if (link_ptr->comm->links == 0 && rmv_file)
  {
    /*-----------------------------------------------------------------*/
    /* Truncate file size to zero.                                     */
    /*-----------------------------------------------------------------*/
    FlashTruncZero(link_ptr, ADJUST_FCBs);

#if QUOTA_ENABLED
    /*-----------------------------------------------------------------*/
    /* If volume has quotas, remember link used space.                 */
    /*-----------------------------------------------------------------*/
    if (Flash->quota_enabled)
      used += (strlen(link_ptr->name) + 1 + OFFIL_SZ);
#endif /* QUOTA_ENABLED */

    /*-----------------------------------------------------------------*/
    /* Remove file from the files table and adjust free entries.       */
    /*-----------------------------------------------------------------*/
    link_ptr->comm->addr->type = FEMPTY;
    ++Flash->total_free;
    FlashIncrementTblEntries(link_ptr->comm->addr);
    Flash->tbls_size -= OFCOM_SZ;
  }

  /*-------------------------------------------------------------------*/
  /* If an open dir structure has pointer to this entry, update it.    */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < DIROPEN_MAX; ++i)
    if (Files[i].ioctl == FlashIoctl && Files[i].pos == tmp_ent)
      Files[i].pos = link_ptr->prev;

  /*-------------------------------------------------------------------*/
  /* Remove the directory link, keeping directory structure.           */
  /*-------------------------------------------------------------------*/
  if (link_ptr->prev)
    link_ptr->prev->entry.dir.next = link_ptr->next;
  else
    dir->first = link_ptr->next;
  if (link_ptr->next)
    link_ptr->next->entry.dir.prev = link_ptr->prev;

  /*-------------------------------------------------------------------*/
  /* Check if the file is to be removed.                               */
  /*-------------------------------------------------------------------*/
  if (rmv_file)
  {
    /*-----------------------------------------------------------------*/
    /* Clear comm ptr, set next and prev to NULL, and mark type empty. */
    /*-----------------------------------------------------------------*/
    link_ptr->comm = NULL;
    link_ptr->next = link_ptr->prev = NULL;
    tmp_ent->type = FEMPTY;

    /*-----------------------------------------------------------------*/
    /* Increment number of free entries.                               */
    /*-----------------------------------------------------------------*/
    ++Flash->total_free;
    FlashIncrementTblEntries(tmp_ent);
    Flash->tbls_size -= (OFFIL_SZ + strlen(link_ptr->name) + 1);

#if QUOTA_ENABLED
    /*-----------------------------------------------------------------*/
    /* If quota enabled, update quota values due to file removal.      */
    /*-----------------------------------------------------------------*/
    if (Flash->quota_enabled)
    {
      FFSEnt *ent, *root = &Flash->files_tbl->tbl[0];
      used += OFCOM_SZ;

      /*---------------------------------------------------------------*/
      /* Update used from parent to root.                              */
      /*---------------------------------------------------------------*/
      for (ent = link_ptr->parent_dir; ent;
           ent = ent->entry.dir.parent_dir)
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

    /*-----------------------------------------------------------------*/
    /* Do garbage collection if necessary.                             */
    /*-----------------------------------------------------------------*/
    if (Flash->total_free >= (FNUM_ENT * 5 / 3))
      FlashGarbageCollect();
  }

  /*-------------------------------------------------------------------*/
  /* Else don't free the entry yet. Free it when the last open file    */
  /* descriptor for this link is closed. For now, just set next and    */
  /* prev to REMOVED_LINK.                                             */
  /*-------------------------------------------------------------------*/
  else
    link_ptr->next = link_ptr->prev = (FFSEnt *)REMOVED_LINK;

  /*-------------------------------------------------------------------*/
  /* Do a sync to save new state of file system.                       */
  /*-------------------------------------------------------------------*/
  return FlashSync(Flash->do_sync);
}

/***********************************************************************/
/*   find_file: Look for a file with name path in dir                  */
/*                                                                     */
/*      Inputs: dir = parent directory to look into                    */
/*              path = file name                                       */
/*              mode = permissions directory must have                 */
/*              entry = place to store found file                      */
/*              search_flag = look for file only/ file and dir         */
/*                                                                     */
/*     Returns: -1 if error in searching, 0 otherwise (entry will have */
/*              either pointer to found file or NULL otherwise)        */
/*                                                                     */
/***********************************************************************/
static int find_file(const FDIR_T *dir, const char *path, int mode,
                     FFSEnt **entry, int search_flag)
{
  /*-------------------------------------------------------------------*/
  /* If path ends in '/', return error (it's not a file), else look if */
  /* file exists.                                                      */
  /*-------------------------------------------------------------------*/
  if (path[strlen(path) - 1] == '/')
  {
    set_errno(EINVAL);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Check that parent directory has permissions.                      */
  /*-------------------------------------------------------------------*/
  if (CheckPerm(dir->comm, mode))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Look for file in its parent directory.                            */
  /*-------------------------------------------------------------------*/
  for (*entry = dir->first; *entry; *entry = (*entry)->entry.dir.next)
  {
    /*-----------------------------------------------------------------*/
    /* Check if entry's name matches our file's name.                  */
    /*-----------------------------------------------------------------*/
    if (!strcmp((*entry)->entry.dir.name, path))
    {
      /*---------------------------------------------------------------*/
      /* Error if looking for regular file and this is a directory.    */
      /*---------------------------------------------------------------*/
      if (search_flag == F_ONLY && (*entry)->type != FFILEN)
      {
        set_errno(EISDIR);
        return -1;
      }

      break;
    }
  }
  return 0;
}

/***********************************************************************/
/* find_new_place: Find a free entry from among the list of tables     */
/*                                                                     */
/*       Input: smallest = ptr to table to be removed                  */
/*                                                                     */
/*     Returns: pointer to found entry or NULL if no entry was found   */
/*                                                                     */
/***********************************************************************/
static FFSEnt *find_new_place(const FFSEnts *smallest)
{
  int i;
  FFSEnts *tbl;

  /*-------------------------------------------------------------------*/
  /* Loop over every file table.                                       */
  /*-------------------------------------------------------------------*/
  for (tbl = Flash->files_tbl; tbl; tbl = tbl->next_tbl)
  {
    /*-----------------------------------------------------------------*/
    /* Check if table is not being removed and has a free entry.       */
    /*-----------------------------------------------------------------*/
    if ((tbl != smallest) && tbl->free)
    {
      /*---------------------------------------------------------------*/
      /* Decrease free entry count and return pointer to free entry.   */
      /*---------------------------------------------------------------*/
      --tbl->free;
      for (i = 0;; ++i)
      {
        PfAssert(i < FNUM_ENT);
        if (tbl->tbl[i].type == FEMPTY)
          return &tbl->tbl[i];
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* No free entries were found.                                       */
  /*-------------------------------------------------------------------*/
  return NULL;
}

/***********************************************************************/
/* update_entries: Go through all non-empty entries, and for entry     */
/*                 pointing to old entry, update it to the new entry   */
/*                                                                     */
/*      Inputs: old_ent = pointer to old entry                         */
/*              new_ent = pointer to new entry                         */
/*                                                                     */
/***********************************************************************/
static void update_entries(const FFSEnt *old_ent, FFSEnt *new_ent)
{
  FFSEnts *curr_tbl = Flash->files_tbl;
  FFSEnt *curr_dir;
  FDIR_T *dir;
  FFIL_T *link_ptr;
  int     i;
  ui32   dummy;

  for (i = 0; curr_tbl; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* If entry is a directory, look at next, prev, parent, first, and */
    /* actual file to see if any is equal to the old entry.            */
    /*-----------------------------------------------------------------*/
    if (curr_tbl->tbl[i].type == FDIREN)
    {
      dir = &(curr_tbl->tbl[i].entry.dir);
      if (dir->next == old_ent)
        dir->next = new_ent;
      else if (dir->prev == old_ent)
        dir->prev = new_ent;
      else if (dir->parent_dir == old_ent)
        dir->parent_dir = new_ent;
      else if (dir->first == old_ent)
        dir->first = new_ent;
      else if (dir->comm->addr == old_ent)
      {
        new_ent->entry.comm.addr = new_ent;
        dir->comm = &(new_ent->entry.comm);
      }
    }

    /*-----------------------------------------------------------------*/
    /* Else if entry is link, look at next, prev, parent and the actual*/
    /* file to see if any is equal to the old entry.                   */
    /*-----------------------------------------------------------------*/
    else if (curr_tbl->tbl[i].type == FFILEN)
    {
      link_ptr = &(curr_tbl->tbl[i].entry.file);
      if (link_ptr->next == old_ent)
        link_ptr->next = new_ent;
      else if (link_ptr->prev == old_ent)
        link_ptr->prev = new_ent;
      else if (link_ptr->parent_dir == old_ent)
        link_ptr->parent_dir = new_ent;
      else if (link_ptr->comm->addr == old_ent)
      {
        new_ent->entry.comm.addr = new_ent;
        link_ptr->comm = &(new_ent->entry.comm);
      }
    }

    /*-----------------------------------------------------------------*/
    /* If we are at the end of the current table, go to the next one.  */
    /*-----------------------------------------------------------------*/
    if (i == (FNUM_ENT - 1))
    {
      i = -1;
      curr_tbl = curr_tbl->next_tbl;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Adjust all the open streams handles if necessary.                 */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < FOPEN_MAX; ++i)
    if (Files[i].handle == old_ent)
      Files[i].handle = new_ent;

  /*-------------------------------------------------------------------*/
  /* Adjust all file_ptrs in the cache entries that point to old.      */
  /*-------------------------------------------------------------------*/
  UpdateFilePtrs(&Flash->cache, old_ent, new_ent);

  /*-------------------------------------------------------------------*/
  /* Finally, if current directory is the same as old, update it.      */
  /*-------------------------------------------------------------------*/
  FsReadCWD(&dummy, (void *)&curr_dir);
  if (curr_dir == old_ent)
    FsSaveCWD((ui32)Flash, (ui32)new_ent);
}

/***********************************************************************/
/*   close_dir: Close a directory                                      */
/*                                                                     */
/*      Inputs: entry  = pointer to dir entry in files table           */
/*              stream = pointer to dir control block                  */
/*                                                                     */
/***********************************************************************/
static void close_dir(FFSEnt *entry, const FILE *stream)
{
  FDIR_T *dir_ptr = &entry->entry.dir;
  int i;

  /*-------------------------------------------------------------------*/
  /* If no more open links, reset.                                     */
  /*-------------------------------------------------------------------*/
  if (--dir_ptr->comm->open_links == 0)
  {
    dir_ptr->dir = NULL;
    return;
  }

  /*-------------------------------------------------------------------*/
  /* If the dir entry is set to stream on which close was performed,   */
  /* find another stream opened on the file to replace it.             */
  /*-------------------------------------------------------------------*/
  if (dir_ptr->dir == stream)
  {
    for (i = 0; i < DIROPEN_MAX; ++i)
      if (Files[i].handle == dir_ptr && stream != &Files[i])
      {
        PfAssert(Files[i].ioctl == FlashIoctl &&
                 Files[i].flags & FCB_DIR);
        dir_ptr->dir = &Files[i];
      }
  }
}

/***********************************************************************/
/*  close_file: Close a file                                           */
/*                                                                     */
/*      Inputs: entry  = pointer to file entry in files table          */
/*              stream = pointer to file control block                 */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int close_file(FFSEnt *entry, FILE *stream)
{
  FFIL_T *link_ptr = &entry->entry.file;
  int     i;

#if QUOTA_ENABLED
  ui32 used = 0;
#endif /* QUOTA_ENABLED */

  /*-------------------------------------------------------------------*/
  /* Free the current sector, if it's not free already.                */
  /*-------------------------------------------------------------------*/
  if (stream->cached && FreeSector(&stream->cached, &Flash->cache))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Free the file control block associated with this file.            */
  /*-------------------------------------------------------------------*/
  stream->read = NULL;
  stream->write = NULL;
  stream->ioctl = NULL;
  stream->volume = NULL;
  stream->curr_ptr.sector = (ui16)-1;
  stream->curr_ptr.offset = 0;
  stream->curr_ptr.sect_off = 0;

  /*-------------------------------------------------------------------*/
  /* If it is the last link open, reset the file mode and check for    */
  /* removal as well.                                                  */
  /*-------------------------------------------------------------------*/
  if (--link_ptr->comm->open_links == 0)
  {
    /*-----------------------------------------------------------------*/
    /* Clear the open_mode and the stream associated with file.        */
    /*-----------------------------------------------------------------*/
    link_ptr->comm->open_mode = 0;
    link_ptr->file = NULL;

    /*-----------------------------------------------------------------*/
    /* If the number of links to this file is 0, need to remove the    */
    /* contents of the file.                                           */
    /*-----------------------------------------------------------------*/
    if (link_ptr->comm->links == 0)
    {
      /*---------------------------------------------------------------*/
      /* A previous remove() must have been issued for this file.      */
      /*---------------------------------------------------------------*/
      PfAssert(link_ptr->next == (FFSEnt *)REMOVED_LINK &&
               link_ptr->prev == (FFSEnt *)REMOVED_LINK);

      /*---------------------------------------------------------------*/
      /* Truncate file size to zero.                                   */
      /*---------------------------------------------------------------*/
      FlashTruncZero(link_ptr, ADJUST_FCBs);

#if QUOTA_ENABLED
      /*---------------------------------------------------------------*/
      /* If volume has quotas, remember link used space.               */
      /*---------------------------------------------------------------*/
      if (Flash->quota_enabled)
        used += OFCOM_SZ;
#endif /* QUOTA_ENABLED */

      /*---------------------------------------------------------------*/
      /* Remove file from the files table and adjust free entries.     */
      /*---------------------------------------------------------------*/
      link_ptr->comm->addr->type = FEMPTY;
      ++Flash->total_free;
      FlashIncrementTblEntries(link_ptr->comm->addr);
      Flash->tbls_size -= OFCOM_SZ;
    }

    /*-----------------------------------------------------------------*/
    /* If entry has next and prev set to REMOVED_LINK, free it.        */
    /*-----------------------------------------------------------------*/
    if (link_ptr->next == (FFSEnt *)REMOVED_LINK &&
        link_ptr->prev == (FFSEnt *)REMOVED_LINK)
    {
      /*---------------------------------------------------------------*/
      /* Clear struct fields.                                          */
      /*---------------------------------------------------------------*/
      link_ptr->comm = NULL;
      link_ptr->next = link_ptr->prev = NULL;
      entry->type = FEMPTY;

      /*---------------------------------------------------------------*/
      /* Increment number of free entries.                             */
      /*---------------------------------------------------------------*/
      ++Flash->total_free;
      FlashIncrementTblEntries(entry);
      Flash->tbls_size -= (OFFIL_SZ + strlen(link_ptr->name) + 1);

#if QUOTA_ENABLED
      /*---------------------------------------------------------------*/
      /* If quota enabled, update quota values due to file removal.    */
      /*---------------------------------------------------------------*/
      if (Flash->quota_enabled)
      {
        FFSEnt *ent, *root = &Flash->files_tbl->tbl[0];
        used += (strlen(link_ptr->name) + 1 + OFFIL_SZ);

        /*-------------------------------------------------------------*/
        /* Update used from parent to root.                            */
        /*-------------------------------------------------------------*/
        for (ent = link_ptr->parent_dir; ent;
             ent = ent->entry.dir.parent_dir)
        {
          PfAssert(ent->type == FDIREN && ent->entry.dir.used >= used);
          ent->entry.dir.used -= used;
        }

        /*-------------------------------------------------------------*/
        /* Update free below.                                          */
        /*-------------------------------------------------------------*/
        FlashFreeBelow(root);

        /*-------------------------------------------------------------*/
        /* Recompute free at each node.                                */
        /*-------------------------------------------------------------*/
        FlashFree(root, root->entry.dir.max_q - FlashVirtUsed(root));
      }
#endif /* QUOTA_ENABLED */

      /*---------------------------------------------------------------*/
      /* Do garbage collection if necessary.                           */
      /*---------------------------------------------------------------*/
      if (Flash->total_free >= (FNUM_ENT * 5 / 3))
        FlashGarbageCollect();

      return 0;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Else, if we are closing a stream opened in write mode, clear the  */
  /* write option from the mode.                                       */
  /*-------------------------------------------------------------------*/
  else if ((link_ptr->file == stream) &&
           (link_ptr->comm->open_mode & (F_APPEND | F_WRITE)))
    link_ptr->comm->open_mode &= ~(F_APPEND | F_WRITE);

  /*-------------------------------------------------------------------*/
  /* If the file entry is set to stream on which close was performed,  */
  /* find another stream opened on the file to replace it.             */
  /*-------------------------------------------------------------------*/
  if (link_ptr->file == stream)
  {
    for (i = 0; i < FOPEN_MAX; ++i)
      if (Files[i].handle == link_ptr && stream != &Files[i])
      {
        PfAssert(Files[i].ioctl == FlashIoctl &&
                 Files[i].flags & FCB_FILE);
        link_ptr->file = &Files[i];
        break;
      }
    PfAssert(i < FOPEN_MAX);
  }

  return 0;
}

/***********************************************************************/
/*  statistics: Auxiliary function for FSTAT and STAT to fill out the  */
/*              struct stat contents                                   */
/*                                                                     */
/*      Inputs: buf = stat structure to be filled                      */
/*              entry = pointer to the entry for the stat              */
/*                                                                     */
/***********************************************************************/
static void statistics(struct stat *buf, FFSEnt *entry)
{
  FFIL_T *link_ptr = &entry->entry.file;

  /*-------------------------------------------------------------------*/
  /* Look at the mode for the file/dir. Start with the permissions.    */
  /*-------------------------------------------------------------------*/
  buf->st_mode = link_ptr->comm->mode;

  /*-------------------------------------------------------------------*/
  /* Set the bit corresponding to file/dir according to the entry.     */
  /*-------------------------------------------------------------------*/
  if (entry->type == FDIREN)
    buf->st_mode |= S_DIR;

  /*-------------------------------------------------------------------*/
  /* st_dev is always 0.                                               */
  /*-------------------------------------------------------------------*/
  buf->st_dev = 0;

  /*-------------------------------------------------------------------*/
  /* Get the uid and gid (0 if permissions not used).                  */
  /*-------------------------------------------------------------------*/
  buf->st_uid = link_ptr->comm->user_id;
  buf->st_gid = link_ptr->comm->group_id;

  /*-------------------------------------------------------------------*/
  /* Look at the serial number for the file/dir.                       */
  /*-------------------------------------------------------------------*/
  buf->st_ino = (ino_t)link_ptr->comm->fileno;

  /*-------------------------------------------------------------------*/
  /* Look at the number of links for the file/dir.                     */
  /*-------------------------------------------------------------------*/
  buf->st_nlink = (nlink_t)link_ptr->comm->links;

  /*-------------------------------------------------------------------*/
  /* Look at the size for the file. For a dir the size is 0.           */
  /*-------------------------------------------------------------------*/
  if (entry->type == FFILEN)
    buf->st_size = (off_t)link_ptr->comm->size;
  else
    buf->st_size = 0;

  /*-------------------------------------------------------------------*/
  /* Look at the access and modification times.                        */
  /*-------------------------------------------------------------------*/
  buf->st_atime = (time_t)link_ptr->comm->ac_time;
  buf->st_mtime = (time_t)link_ptr->comm->mod_time;
}

/***********************************************************************/
/* seek_within_file: Help function for FSEEK (steps through file)      */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              offset = new seek location                             */
/*              curr_pos = current position within file                */
/*                                                                     */
/*     Returns: NULL on success, (void*)-1 on error                    */
/*                                                                     */
/***********************************************************************/
static void *seek_within_file(FILE *stream, i32 offset, i32 curr_pos)
{
  i32 old_pos;
  fpos_t beg_ptr;
  void *vp;

  /*-------------------------------------------------------------------*/
  /* If stream has a cached entry, free it.                            */
  /*-------------------------------------------------------------------*/
  if (stream->cached && FreeSector(&stream->cached, &Flash->cache))
    return (void *)-1;

  /*-------------------------------------------------------------------*/
  /* Get valid position from old_ptr or set it to the end of the file. */
  /*-------------------------------------------------------------------*/
  if (stream->old_ptr.sector != (ui16)-1)
    old_pos = (i32)(stream->old_ptr.sect_off * Flash->sect_sz +
                    stream->old_ptr.offset);
  else
    old_pos = (i32)((FFSEnt *)stream->handle)->entry.file.comm->size;

  /*-------------------------------------------------------------------*/
  /* Choose among curr_pos, old_pos, or beginning of file, whichever   */
  /* is closer to where we want to get. The only time when beginning   */
  /* of file is better than the other two is when both curr and old    */
  /* position are beyond where we want to seek.                        */
  /*-------------------------------------------------------------------*/
  if (curr_pos > offset && old_pos > offset)
  {
    beg_ptr.sector =
                  ((FFSEnt *)stream->handle)->entry.file.comm->frst_sect;
    beg_ptr.offset = 0;
    beg_ptr.sect_off = 0;
    return seek_up(offset, stream, beg_ptr);
  }

  /*-------------------------------------------------------------------*/
  /* Else choose between old_pos and curr_pos and seek from there up   */
  /* to the right spot.                                                */
  /*-------------------------------------------------------------------*/
  else
  {
    /*-----------------------------------------------------------------*/
    /* If old_pos is past location, use curr_pos.                      */
    /*-----------------------------------------------------------------*/
    if (old_pos > offset)
    {
      /*---------------------------------------------------------------*/
      /* Adjust offset by subtracting curr_pos from it.                */
      /*---------------------------------------------------------------*/
      offset -= curr_pos;
      beg_ptr = stream->curr_ptr;
      vp = seek_up(offset, stream, stream->curr_ptr);
      stream->old_ptr = beg_ptr;
      return vp;
    }

    /*-----------------------------------------------------------------*/
    /* Else if curr_pos is past location, use old_pos.                 */
    /*-----------------------------------------------------------------*/
    else if (curr_pos > offset)
    {
      /*---------------------------------------------------------------*/
      /* Adjust offset by subtracting old_pos from it.                 */
      /*---------------------------------------------------------------*/
      offset -= old_pos;
      return seek_up(offset, stream, stream->old_ptr);
    }

    /*-----------------------------------------------------------------*/
    /* Else choose the one that's closest.                             */
    /*-----------------------------------------------------------------*/
    else
    {
      /*---------------------------------------------------------------*/
      /* Use whichever is bigger between curr_pos and old_pos.         */
      /*---------------------------------------------------------------*/
      if (old_pos > curr_pos)
      {
        /*-------------------------------------------------------------*/
        /* Adjust offset by subtracting old position from it.          */
        /*-------------------------------------------------------------*/
        offset -= old_pos;
        return seek_up(offset, stream, stream->old_ptr);
      }
      else
      {
        /*-------------------------------------------------------------*/
        /* Adjust offset by subtracting curr position from it.         */
        /*-------------------------------------------------------------*/
        offset -= curr_pos;
        beg_ptr = stream->curr_ptr;
        vp = seek_up(offset, stream, stream->curr_ptr);
        stream->old_ptr = beg_ptr;
        return vp;
      }
    }
  }
}

/***********************************************************************/
/* seek_past_end: Adjust file position past EOF                        */
/*                                                                     */
/*      Inputs: offset = length of fseek in bytes                      */
/*              stream = pointer to file control block (FILE)          */
/*                                                                     */
/*     Returns: NULL on success, (void *)-1 on error                   */
/*                                                                     */
/***********************************************************************/
static void *seek_past_end(i32 offset, FILE *stream)
{
  FFIL_T *link_ptr = &((FFSEnt *)stream->handle)->entry.file;
  FCOM_T *file = link_ptr->comm;

  /*-------------------------------------------------------------------*/
  /* Files created by creatn() cannot seek past end.                   */
  /*-------------------------------------------------------------------*/
  if (offset && (link_ptr->comm->mode & S_CREATN))
  {
    set_errno(EINVAL);
    return (void *)-1;
  }

  /*-------------------------------------------------------------------*/
  /* If stream has a cached entry, free it.                            */
  /*-------------------------------------------------------------------*/
  if (stream->cached && FreeSector(&stream->cached, &Flash->cache))
    return (void *)-1;

  /*-------------------------------------------------------------------*/
  /* If offset is zero, set current pointer to EOF.                    */
  /*-------------------------------------------------------------------*/
  if (offset == 0)
    FlashPoint2End(&stream->curr_ptr, file);

  /*-------------------------------------------------------------------*/
  /* Else set current pointer past the EOF.                            */
  /*-------------------------------------------------------------------*/
  else
  {
    /*-----------------------------------------------------------------*/
    /* Current sector will be invalid since we're past EOF.            */
    /*-----------------------------------------------------------------*/
    stream->curr_ptr.sector = FDRTY_SECT;

    /*-----------------------------------------------------------------*/
    /* Position is given by sect_off and offset.                       */
    /*-----------------------------------------------------------------*/
    stream->curr_ptr.sect_off = (file->size + offset) / Flash->sect_sz;
    stream->curr_ptr.offset = (ui16)((file->size + offset) %
                                     Flash->sect_sz);
  }

  return NULL;
}

/***********************************************************************/
/*     seek_up: Helper function for FSEEK                              */
/*                                                                     */
/*      Inputs: offset = length of fseek in bytes                      */
/*              stream = pointer to file control block (FILE)          */
/*              start_pos = start position (beginning, curr, or old)   */
/*                                                                     */
/*     Returns: NULL on success, (void *)-1 on error                   */
/*                                                                     */
/***********************************************************************/
static void *seek_up(i32 offset, FILE *stream, fpos_t start_pos)
{
  FCOM_T *file = ((FFSEnt *)stream->handle)->entry.file.comm;
  ui32 num_sects, i;

  /*-------------------------------------------------------------------*/
  /* If seek goes past start sector, subtract starting position from   */
  /* offset.                                                           */
  /*-------------------------------------------------------------------*/
  if (offset > (Flash->sect_sz - start_pos.offset))
    offset -= (Flash->sect_sz - start_pos.offset);

  /*-------------------------------------------------------------------*/
  /* Else adjust offset and return.                                    */
  /*-------------------------------------------------------------------*/
  else
  {
    start_pos.offset += (ui16)offset;
    stream->curr_ptr = start_pos;
    return NULL;
  }

  /*-------------------------------------------------------------------*/
  /* Move to the next sector in file.                                  */
  /*-------------------------------------------------------------------*/
  ++start_pos.sect_off;
  start_pos.sector = Flash->sect_tbl[start_pos.sector].next;

  /*-------------------------------------------------------------------*/
  /* Figure out how many sectors we have to go up.                     */
  /*-------------------------------------------------------------------*/
  num_sects = (ui32)offset / Flash->sect_sz;
  start_pos.offset = (ui16)((ui32)offset % Flash->sect_sz);

  /*-------------------------------------------------------------------*/
  /* Step through to the desired num_sects.                            */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < num_sects; ++i)
    start_pos.sector = Flash->sect_tbl[start_pos.sector].next;
  start_pos.sect_off += num_sects;

  /*-------------------------------------------------------------------*/
  /* If this was the last sector, we're at the end of the file, but    */
  /* not past it.                                                      */
  /*-------------------------------------------------------------------*/
  if (start_pos.sector == FLAST_SECT)
    FlashPoint2End(&start_pos, file);

  /*-------------------------------------------------------------------*/
  /* Update curr_ptr and return success.                               */
  /*-------------------------------------------------------------------*/
  stream->curr_ptr = start_pos;
  return NULL;
}

/***********************************************************************/
/*  check_name: Search through all entries for name                    */
/*                                                                     */
/*       Input: name = filename to look for                            */
/*                                                                     */
/*     Returns: 0 if the name was not found, 1 if it was               */
/*                                                                     */
/***********************************************************************/
static int check_name(const char *name)
{
  int i, j;
  FFSEnts *curr_tbl;

  /*-------------------------------------------------------------------*/
  /* Go through all mounted volumes.                                   */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < NUM_FFS_VOLS; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* If this volume is mounted, check all its entries for name dup.  */
    /*-----------------------------------------------------------------*/
    if (MountedList.head == &FlashGlobals[i].sys ||
        FlashGlobals[i].sys.prev || FlashGlobals[i].sys.next)
    {
      /*---------------------------------------------------------------*/
      /* Acquire exclusive access to the flash file system.            */
      /*---------------------------------------------------------------*/
      semPend(FlashSem, WAIT_FOREVER);

      /*---------------------------------------------------------------*/
      /* Go through each table in volume.                              */
      /*---------------------------------------------------------------*/
      for (curr_tbl = FlashGlobals[i].files_tbl; curr_tbl;
           curr_tbl = curr_tbl->next_tbl)
      {
        /*-------------------------------------------------------------*/
        /* Go through all entries in table.                            */
        /*-------------------------------------------------------------*/
        for (j = 0; j < FNUM_ENT; ++j)
        {
          /*-----------------------------------------------------------*/
          /* If entry is a file and names match, stop.                 */
          /*-----------------------------------------------------------*/
          if (curr_tbl->tbl[i].type == FFILEN &&
              !strcmp(curr_tbl->tbl[i].entry.dir.name, name))
          {
            semPost(FlashSem);
            return 1;
          }
        }
      }

      /*---------------------------------------------------------------*/
      /* Release exclusive access to the flash file system.            */
      /*---------------------------------------------------------------*/
      semPost(FlashSem);
    }
  }
  return 0;
}

/***********************************************************************/
/*  find_entry: Find an empty file table entry                         */
/*                                                                     */
/*      Output: *indexp = index of empty entry in table                */
/*                                                                     */
/*     Returns: NULL if no empty entry, or ptr to table with entry     */
/*                                                                     */
/***********************************************************************/
static FFSEnts *find_entry(ui32 *indexp)
{
  ui32 i;
  FFSEnts *curr_table, *prev_table;

  /*-------------------------------------------------------------------*/
  /* Look through all the tables for an empty entry.                   */
  /*-------------------------------------------------------------------*/
  curr_table = Flash->files_tbl;
  do
  {
    /*-----------------------------------------------------------------*/
    /* Check if table has an empty entry.                              */
    /*-----------------------------------------------------------------*/
    if (curr_table->free)
    {
      /*---------------------------------------------------------------*/
      /* Find index to the free entry.                                 */
      /*---------------------------------------------------------------*/
      for (i= 0; i < FNUM_ENT && curr_table->tbl[i].type != FEMPTY;++i) ;
      PfAssert(i < FNUM_ENT);

      /*---------------------------------------------------------------*/
      /* Adjust both the global free entry counts and this table's.    */
      /*---------------------------------------------------------------*/
      --Flash->total_free;
      --curr_table->free;

      /*---------------------------------------------------------------*/
      /* Mark not empty, output entry index, and return table pointer. */
      /*---------------------------------------------------------------*/
      curr_table->tbl[i].type = (ui8)EOF;
      *indexp = i;
      return curr_table;
    }

    /*-----------------------------------------------------------------*/
    /* Go to the next table.                                           */
    /*-----------------------------------------------------------------*/
    prev_table = curr_table;
    curr_table = curr_table->next_tbl;
  } while (curr_table);

  /*-------------------------------------------------------------------*/
  /* No empty entry was found, assign a new table.                     */
  /*-------------------------------------------------------------------*/
  curr_table = FlashGetTbl();

  /*-------------------------------------------------------------------*/
  /* If no space available to assign new table, return error.          */
  /*-------------------------------------------------------------------*/
  if (curr_table == NULL)
  {
    /*-----------------------------------------------------------------*/
    /* Set errno to no memory available.                               */
    /*-----------------------------------------------------------------*/
    set_errno(ENOMEM);
    return NULL;
  }

  /*-------------------------------------------------------------------*/
  /* Update the linked list of entry tables with the new table.        */
  /*-------------------------------------------------------------------*/
  prev_table->next_tbl = curr_table;
  curr_table->prev_tbl = prev_table;
  Flash->total_free += (FNUM_ENT - 1);

  /*-------------------------------------------------------------------*/
  /* Mark not empty, output entry index, and return table pointer.     */
  /*-------------------------------------------------------------------*/
  curr_table->tbl[0].type = (ui8)EOF;
  *indexp = 0;
  return curr_table;
}

/***********************************************************************/
/*    get_path: Get path of current working directory                  */
/*                                                                     */
/*      Inputs: buf = pointer to buffer or NULL                        */
/*              max_sz = size of buffer, if provided                   */
/*              curr_dir = current working directory handle            */
/*              f = function that called this one (getcwd, or getname) */
/*                                                                     */
/*     Returns: pointer to path buffer if successful, NULL on error    */
/*                                                                     */
/***********************************************************************/
static char *get_path(char *buf, size_t max_sz, FFSEnt *curr_dir, int f)
{
  size_t size;
  int depth, i;
  char *rval;
  FDIR_T *dir = &curr_dir->entry.dir;

  /*-------------------------------------------------------------------*/
  /* Count directories between CWD and root, and determine path size.  */
  /*-------------------------------------------------------------------*/
  size = strlen(dir->name) + 1;
  if (f == GETCWD)
    ++size;
  for (depth = 0; dir->parent_dir; ++depth)
  {
    dir = &dir->parent_dir->entry.dir;
    size += strlen(dir->name) + 1;
  }

  /*-------------------------------------------------------------------*/
  /* If a buffer has been provided, ensure it is big enough.           */
  /*-------------------------------------------------------------------*/
  if (buf)
  {
    if (max_sz < size + 1)
    {
      set_errno(ERANGE);
      return NULL;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Else allocate a buffer.                                           */
  /*-------------------------------------------------------------------*/
  else
  {
    buf = malloc(size + 1);
    if (buf == NULL)
    {
      set_errno(ENOMEM);
      return NULL;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Build path name of current working directory.                     */
  /*-------------------------------------------------------------------*/
  for (rval = buf; depth >= 0; --depth)
  {
    /*-----------------------------------------------------------------*/
    /* Set the dir pointer to the current directory.                   */
    /*-----------------------------------------------------------------*/
    dir = &curr_dir->entry.dir;

    /*-----------------------------------------------------------------*/
    /* Walk up to the "depth" level directory.                         */
    /*-----------------------------------------------------------------*/
    for (i = 0; i < depth; ++i)
      dir = &dir->parent_dir->entry.dir;

    /*-----------------------------------------------------------------*/
    /* Copy '/' and then the directory name to the buffer.             */
    /*-----------------------------------------------------------------*/
    *buf++ = '/';
    strcpy(buf, dir->name);
    buf += strlen(dir->name);
  }

  /*-------------------------------------------------------------------*/
  /* Add a last '/' if dir, and end buffer with '\0' before returning. */
  /*-------------------------------------------------------------------*/
  if (f == GETCWD)
    *buf++ = '/';
  *buf = '\0';
  return rval;
}

/***********************************************************************/
/*   set_fmode: Set access mode for a file in the Flash->files_tbl     */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              oflag = flag describing access mode                    */
/*                                                                     */
/***********************************************************************/
static void set_fmode(FILE *stream, int oflag)
{
  FFIL_T *entry = &(((FFSEnt *)stream->handle)->entry.file);

  /*-------------------------------------------------------------------*/
  /* If file is not empty, set curr_ptr to either beginning or end.    */
  /*-------------------------------------------------------------------*/
  if (entry->comm->frst_sect != FFREE_SECT)
  {
    /*-----------------------------------------------------------------*/
    /* In case of append mode, set curr_ptr to the end.                */
    /*-----------------------------------------------------------------*/
    if (oflag & O_APPEND)
      FlashPoint2End(&stream->curr_ptr, entry->comm);

    /*-----------------------------------------------------------------*/
    /* Else set curr_ptr to beginnig.                                  */
    /*-----------------------------------------------------------------*/
    else
    {
      stream->curr_ptr.sector = (ui16)entry->comm->frst_sect;
      stream->curr_ptr.offset = 0;
      stream->curr_ptr.sect_off = 0;
    }
  }
  else
  {
    stream->curr_ptr.sector = (ui16)-1;
    stream->curr_ptr.offset = 0;
    stream->curr_ptr.sect_off = 0;
  }

  /*-------------------------------------------------------------------*/
  /* When opening a file, set old_ptr to curr_ptr.                     */
  /*-------------------------------------------------------------------*/
  stream->old_ptr = stream->curr_ptr;

  /*-------------------------------------------------------------------*/
  /* Set file mode to either read, write or append or a combination.   */
  /*-------------------------------------------------------------------*/
  if (oflag & O_WRONLY)
  {
    if (oflag & O_APPEND)
      entry->comm->open_mode |= F_APPEND;
    else
      entry->comm->open_mode |= F_WRITE;
  }
  else if (oflag & O_RDWR)
  {
    if (oflag & O_APPEND)
      entry->comm->open_mode |= (F_APPEND | F_READ);
    else
      entry->comm->open_mode |= (F_WRITE | F_READ);
  }
  else
    entry->comm->open_mode |= F_READ;
}

/***********************************************************************/
/*     acquire: Acquire the flash file system and volume lock          */
/*                                                                     */
/*      Inputs: handle = pointer to file control block                 */
/*              code   = if more than 1 sem, select which one          */
/*                                                                     */
/***********************************************************************/
static void acquire(const FILE *file, int code)
{
  FlashGlob *vol = file->volume;

  semPend(vol->sem, WAIT_FOREVER);
  semPend(FlashSem, WAIT_FOREVER);
  Flash = vol;
}

/***********************************************************************/
/*     release: Release the flash file system and volume lock          */
/*                                                                     */
/*      Inputs: handle = pointer to file control block                 */
/*              code   = if more than 1 sem, select which one          */
/*                                                                     */
/***********************************************************************/
static void release(const FILE *file, int code)
{
  FlashGlob *vol = Flash;

  Flash = NULL;
  semPost(FlashSem);
  semPost(vol->sem);
}

/***********************************************************************/
/*    set_file: Initialize an entry's file control block               */
/*                                                                     */
/*      Inputs: file = pointer to file control block                   */
/*              entry = pointer to file entry                          */
/*              set_stream = if TRUE, set the stream pointer           */
/*                                                                     */
/*     Returns: file                                                   */
/*                                                                     */
/***********************************************************************/
static FILE *set_file(FILE *file, FFSEnt *entry, int set_stream)
{
  /*-------------------------------------------------------------------*/
  /* Set the pointer in the entry table to point to the file.          */
  /*-------------------------------------------------------------------*/
  if (set_stream)
    entry->entry.file.file = file;

  /*-------------------------------------------------------------------*/
  /* Set the file structure correctly.                                 */
  /*-------------------------------------------------------------------*/
  file->handle = entry;
  file->ioctl = FlashIoctl;
  file->acquire = acquire;
  file->release = release;
  file->write = FlashWrite;
  file->read = FlashRead;
  file->errcode = 0;
  file->hold_char = '\0';
  file->cached = NULL;
  file->flags = FCB_FILE;

  return file;
}

/***********************************************************************/
/*    open_dir: Open a directory                                       */
/*                                                                     */
/*      Inputs: dir = pointer to directory's file control block        */
/*              dir_ent = pointer to directory's file table entry      */
/*                                                                     */
/*     Returns: File control block pointer, NULL if error              */
/*                                                                     */
/***********************************************************************/
static void *open_dir(DIR *dir, FFSEnt *dir_ent)
{
  uid_t uid;
  gid_t gid;

  /*-------------------------------------------------------------------*/
  /* Check read permissions only if not owner is opening directory.    */
  /*-------------------------------------------------------------------*/
  FsGetId(&uid, &gid);
  if (uid != dir_ent->entry.dir.comm->user_id)
  {
    if (CheckPerm(dir_ent->entry.dir.comm, F_READ))
      return NULL;
  }

  /*-------------------------------------------------------------------*/
  /* Increment the number of open links for the directory.             */
  /*-------------------------------------------------------------------*/
  ++dir_ent->entry.dir.comm->open_links;

  /*-------------------------------------------------------------------*/
  /* Modify the access time for the directory.                         */
  /*-------------------------------------------------------------------*/
  dir_ent->entry.dir.comm->ac_time = OsSecCount;

  /*-------------------------------------------------------------------*/
  /* Initialize the file control structure and return success.         */
  /*-------------------------------------------------------------------*/
  dir_ent->entry.dir.dir = dir;
  dir->ioctl = FlashIoctl;
  dir->handle = dir_ent;
  dir->pos = NULL;
  return dir;
}

/***********************************************************************/
/*   open_file: Helper function for open(), fopen(), and creatn()      */
/*                                                                     */
/*      Inputs: filename = pointer to path of file to open             */
/*              oflag = mode of file to be opened                      */
/*              mode = permission mode for file                        */
/*              d = pointer to directory where file is to be openned   */
/*              fcbp = pointer to file control block                   */
/*              do_sync = flag to indicate if a sync is required       */
/*                                                                     */
/*     Returns: pointer to file control block or NULL if open failed   */
/*                                                                     */
/***********************************************************************/
static FILE *open_file(const char *filename, int oflag, mode_t mode,
                       FFSEnt *d, FILE *fcbp, int *do_sync)
{
  FFSEnt *entry, *v_ent;
  FFIL_T *link_ptr;
  FDIR_T *dir = &d->entry.dir;
  FILE   *r_value = NULL;
  ui32    i, j;
  FFSEnts *curr_table1, *curr_table2;
  int     permissions = 0, set_stream = TRUE;

  /*-------------------------------------------------------------------*/
  /* Initially assume synchronization won't be needed.                 */
  /*-------------------------------------------------------------------*/
  *do_sync = FALSE;

  /*-------------------------------------------------------------------*/
  /* If invalid name, return error.                                    */
  /*-------------------------------------------------------------------*/
  if (InvalidName(filename, FALSE))
    return NULL;

  /*-------------------------------------------------------------------*/
  /* Look for the file in the directory.                               */
  /*-------------------------------------------------------------------*/
  if (find_file(dir, filename, F_EXECUTE, &entry, F_AND_D))
    return NULL;

  /*-------------------------------------------------------------------*/
  /* If file exists, open it.                                          */
  /*-------------------------------------------------------------------*/
  if (entry)
  {
    /*-----------------------------------------------------------------*/
    /* If oflag is set to exclusive, return error.                     */
    /*-----------------------------------------------------------------*/
    if ((oflag & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL))
    {
      set_errno(EEXIST);
      return NULL;
    }

    /*-----------------------------------------------------------------*/
    /* If it's a directory, check mode is not RDWR or WRONLY.          */
    /*-----------------------------------------------------------------*/
    if (entry->type == FDIREN)
    {
      if (oflag & (O_RDWR | O_WRONLY))
      {
        set_errno(EISDIR);
        return NULL;
      }
      return open_dir(fcbp, entry);
    }

    /*-----------------------------------------------------------------*/
    /* If there are any open links, check for mode compatibility.      */
    /*-----------------------------------------------------------------*/
    link_ptr = &entry->entry.file;
    if (link_ptr->comm->open_links)
    {
      /*---------------------------------------------------------------*/
      /* If mode is write/append, new mode should be read only.        */
      /*---------------------------------------------------------------*/
      if (link_ptr->comm->open_mode & (F_APPEND | F_WRITE))
      {
        if (oflag & (O_RDWR | O_WRONLY))
        {
          set_errno(EACCES);
          return NULL;
        }
        set_stream = FALSE;
      }

      /*---------------------------------------------------------------*/
      /* If new mode is write/append, mode should be read only.        */
      /*---------------------------------------------------------------*/
      if ((oflag & (O_RDWR | O_WRONLY)) &&
          (link_ptr->comm->open_mode != F_READ))
      {
        set_errno(EACCES);
        return NULL;
      }
    }
    else
      link_ptr->comm->open_mode = 0;

    /*-----------------------------------------------------------------*/
    /* Check for permissions for the file.                             */
    /*-----------------------------------------------------------------*/
    if (oflag & O_WRONLY)
      permissions = F_WRITE;
    else if (oflag & O_RDWR)
      permissions = F_WRITE | F_READ;
    else
      permissions = F_READ;
    if (CheckPerm(link_ptr->comm, permissions))
      return NULL;

    /*-----------------------------------------------------------------*/
    /* If the file has no FILE*, associate one with it.                */
    /*-----------------------------------------------------------------*/
    if (link_ptr->file == NULL)
      link_ptr->file = fcbp;

    /*-----------------------------------------------------------------*/
    /* Initialize the file control block.                              */
    /*-----------------------------------------------------------------*/
    r_value = set_file(fcbp, entry, set_stream);

    /*-----------------------------------------------------------------*/
    /* Set the mode for the file and increase the number of links.     */
    /*-----------------------------------------------------------------*/
    set_fmode(fcbp, oflag);
    ++link_ptr->comm->open_links;

    /*-----------------------------------------------------------------*/
    /* If truncate flag is set, truncate file.                         */
    /*-----------------------------------------------------------------*/
    if ((oflag & O_TRUNC) &&
        (link_ptr->comm->open_mode & (F_APPEND | F_WRITE)))
    {
      FlashTruncZero(link_ptr, ADJUST_FCBs);
      *do_sync = TRUE;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Else create the file in the directory.                            */
  /*-------------------------------------------------------------------*/
  else
  {
    ui32 used;

    /*-----------------------------------------------------------------*/
    /* If file should not be created, return error.                    */
    /*-----------------------------------------------------------------*/
    if (!(oflag & O_CREAT))
    {
      set_errno(ENOENT);
      return NULL;
    }

    /*-----------------------------------------------------------------*/
    /* Check for write permissions for parent directory.               */
    /*-----------------------------------------------------------------*/
    if (CheckPerm(dir->comm, F_WRITE))
      return NULL;

    /*-----------------------------------------------------------------*/
    /* Figure out how much space this file will take.                  */
    /*-----------------------------------------------------------------*/
    used = strlen(filename) + 1 + OFFIL_SZ + OFCOM_SZ;

#if QUOTA_ENABLED
    /*-----------------------------------------------------------------*/
    /* If quota enabled, check if file creation can succeed.           */
    /*-----------------------------------------------------------------*/
    if (Flash->quota_enabled)
    {
      if (dir->free < used)
      {
        set_errno(ENOMEM);
        return NULL;
      }
    }
#endif /* QUOTA_ENABLED */

    /*-----------------------------------------------------------------*/
    /* If no room left in the volume, return error.                    */
    /*-----------------------------------------------------------------*/
    if (!FlashRoom(used))
      return NULL;

    /*-----------------------------------------------------------------*/
    /* If no more empty entries, return error.                         */
    /*-----------------------------------------------------------------*/
    curr_table1 = find_entry(&i);
    if (curr_table1 == NULL)
      return NULL;

    /*-----------------------------------------------------------------*/
    /* If no more empty entries, return error.                         */
    /*-----------------------------------------------------------------*/
    curr_table2 = find_entry(&j);
    if (curr_table2 == NULL)
    {
      curr_table1->tbl[i].type = FEMPTY;
      ++curr_table1->free;
      ++Flash->total_free;
      return NULL;
    }

    /*-----------------------------------------------------------------*/
    /* Set the file entry in FlashFiletbl.                             */
    /*-----------------------------------------------------------------*/
    v_ent = &curr_table2->tbl[j];
    v_ent->type = FCOMMN;
    v_ent->entry.comm.addr = v_ent;
    v_ent->entry.comm.frst_sect = FFREE_SECT;
    v_ent->entry.comm.last_sect = FFREE_SECT;
    v_ent->entry.comm.one_past_last.sector = (ui16)-1;
    v_ent->entry.comm.one_past_last.offset = 0;
    v_ent->entry.comm.size = 0;
    v_ent->entry.comm.links = 1;
    v_ent->entry.comm.attrib = 0;
    v_ent->entry.comm.open_links = 1;
    v_ent->entry.comm.open_mode = 0;
    v_ent->entry.comm.ac_time = v_ent->entry.comm.mod_time = OsSecCount;

    /*-----------------------------------------------------------------*/
    /* If fileno is passed 2^FID_LEN, have to check for unicity.       */
    /*-----------------------------------------------------------------*/
    if (check_filenos(Flash->fileno_gen))
      v_ent->entry.comm.fileno = unique_fileno();
    else
      v_ent->entry.comm.fileno = Flash->fileno_gen++;

    /*-----------------------------------------------------------------*/
    /* Set the link entry in FlashFileTable.                           */
    /*-----------------------------------------------------------------*/
    v_ent = &curr_table1->tbl[i];
    v_ent->type = FFILEN;
    v_ent->entry.file.comm = &curr_table2->tbl[j].entry.comm;
    v_ent->entry.file.parent_dir = d;
    strcpy(v_ent->entry.file.name, filename);

    /*-----------------------------------------------------------------*/
    /* Initialize the file control block.                              */
    /*-----------------------------------------------------------------*/
    r_value = set_file(fcbp, v_ent, TRUE);

    /*-----------------------------------------------------------------*/
    /* Set the mode for the entry in the files table.                  */
    /*-----------------------------------------------------------------*/
    set_fmode(fcbp, oflag);

    /*-----------------------------------------------------------------*/
    /* Set the permissions for the file.                               */
    /*-----------------------------------------------------------------*/
    SetPerm(v_ent->entry.file.comm, mode);

    /*-----------------------------------------------------------------*/
    /* Add new link to its parent directory.                           */
    /*-----------------------------------------------------------------*/
    entry = dir->first;
    dir->first = v_ent;
    v_ent->entry.file.next = entry;
    v_ent->entry.file.prev = NULL;
    if (entry)
      entry->entry.dir.prev = v_ent;

    /*-----------------------------------------------------------------*/
    /* Update file tables size.                                        */
    /*-----------------------------------------------------------------*/
    Flash->tbls_size += used;

    *do_sync = TRUE;

#if QUOTA_ENABLED
    /*-----------------------------------------------------------------*/
    /* If quotas enabled, set new quota values.                        */
    /*-----------------------------------------------------------------*/
    if (Flash->quota_enabled)
    {
      FFSEnt *root = &Flash->files_tbl->tbl[0];

      /*---------------------------------------------------------------*/
      /* Update used from parent to root.                              */
      /*---------------------------------------------------------------*/
      for (entry = d; entry; entry = entry->entry.dir.parent_dir)
      {
        PfAssert(entry->type == FDIREN);
        entry->entry.dir.used += used;
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
  return r_value;
}

/***********************************************************************/
/* create_link: Create link to a file/directory                        */
/*                                                                     */
/*      Inputs: old_name = name of original file/directory             */
/*              new_name = name of link                                */
/*              dir_old = parent directory for original                */
/*              dir_new = parent directory for link                    */
/*                                                                     */
/*     Returns: NULL on success, (void *)-1 on error                   */
/*                                                                     */
/***********************************************************************/
static void *create_link(char *old_name, char *new_name, FFSEnt *dir_old,
                         FFSEnt *dir_new)
{
  FFSEnt *old_ent, *new_ent;
  FFSEnts *curr_table;
  uid_t uid;
  gid_t gid;
  size_t end;
  ui32 i;

  /*-------------------------------------------------------------------*/
  /* If no room left in the volume, return error.                      */
  /*-------------------------------------------------------------------*/
  size_t entry_size = strlen(new_name) + 1 + OFFIL_SZ + OFCOM_SZ;
  if (!FlashRoom(entry_size))
    return (void *)-1;

  /*-------------------------------------------------------------------*/
  /* While there are terminating '/' characters, remove them.          */
  /*-------------------------------------------------------------------*/
  end = strlen(old_name) - 1;
  while (old_name[end] == '/')
    old_name[end--] = '\0';
  end = strlen(new_name) - 1;
  while (new_name[end] == '/')
    new_name[end--] = '\0';

  /*-------------------------------------------------------------------*/
  /* Check that both old and new dirs have execute access.             */
  /*-------------------------------------------------------------------*/
  if (CheckPerm(dir_old->entry.dir.comm, F_EXECUTE) ||
      CheckPerm(dir_new->entry.dir.comm, F_EXECUTE))
    return (void *)-1;

  /*-------------------------------------------------------------------*/
  /* If no old name is provided, use old directory, else look for old  */
  /* entry in dir_old.                                                 */
  /*-------------------------------------------------------------------*/
  if (old_name[0] == '\0')
    old_ent = dir_old;
  else
    for (old_ent = dir_old->entry.dir.first;;
         old_ent = old_ent->entry.dir.next)
    {
      /*---------------------------------------------------------------*/
      /* Return error if old file could not be found.                  */
      /*---------------------------------------------------------------*/
      if (old_ent == NULL)
      {
        set_errno(ENOENT);
        return (void *)-1;
      }

      /*---------------------------------------------------------------*/
      /* Break if old name is found.                                   */
      /*---------------------------------------------------------------*/
      if (!strcmp(old_ent->entry.dir.name, old_name))
        break;
    }

  /*-------------------------------------------------------------------*/
  /* Make sure owner of file is trying to link to file.                */
  /*-------------------------------------------------------------------*/
  FsGetId(&uid, &gid);
  if (uid != old_ent->entry.file.comm->user_id)
  {
    set_errno(EPERM);
    return (void *)-1;
  }

  /*-------------------------------------------------------------------*/
  /* Look for name duplication with the new link.                      */
  /*-------------------------------------------------------------------*/
  for (new_ent = dir_new->entry.dir.first; new_ent;
       new_ent = new_ent->entry.dir.next)
  {
    /*-----------------------------------------------------------------*/
    /* If a file exists with the name, error.                          */
    /*-----------------------------------------------------------------*/
    if (!strcmp(new_ent->entry.dir.name, new_name))
    {
      set_errno(EEXIST);
      return (void *)-1;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Check that the new directory has write permissions.               */
  /*-------------------------------------------------------------------*/
  if (CheckPerm(dir_new->entry.dir.comm, F_WRITE))
    return (void *)-1;

  /*-------------------------------------------------------------------*/
  /* If we're linking dirs, make sure the new one is not in a subdir   */
  /* of the old one.                                                   */
  /*-------------------------------------------------------------------*/
  for (new_ent = dir_new; new_ent != &Flash->files_tbl->tbl[0];
       new_ent = new_ent->entry.dir.parent_dir)
  {
    /*-----------------------------------------------------------------*/
    /* If new directory is subdirectory of old, return error.          */
    /*-----------------------------------------------------------------*/
    if (new_ent == old_ent)
    {
      set_errno(EINVAL);
      return (void *)-1;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Find a space in the files tables for the link.                    */
  /*-------------------------------------------------------------------*/
  curr_table = find_entry(&i);

  /*-------------------------------------------------------------------*/
  /* If no more free entries found, stop.                              */
  /*-------------------------------------------------------------------*/
  if (curr_table == NULL)
    return (void *)-1;

  /*-------------------------------------------------------------------*/
  /* Set the new link to point to the original file/dir.               */
  /*-------------------------------------------------------------------*/
  new_ent = &curr_table->tbl[i];
  new_ent->type = old_ent->type;
  new_ent->entry.file.comm = old_ent->entry.file.comm;

  /*-------------------------------------------------------------------*/
  /* Set the link name.                                                */
  /*-------------------------------------------------------------------*/
  strcpy(new_ent->entry.file.name, new_name);

  /*-------------------------------------------------------------------*/
  /* Initialize the file specific structure values.                    */
  /*-------------------------------------------------------------------*/
  if (old_ent->type == FFILEN)
  {
    new_ent->entry.file.file = NULL;
    new_ent->entry.file.parent_dir = dir_new;
    Flash->tbls_size += (OFFIL_SZ + strlen(new_ent->entry.file.name)+ 1);
  }

  /*-------------------------------------------------------------------*/
  /* Initialize the directory specific structure values.               */
  /*-------------------------------------------------------------------*/
  else
  {
    new_ent->entry.dir.parent_dir = dir_new;
    new_ent->entry.dir.first = old_ent->entry.dir.first;
    new_ent->entry.dir.dir = NULL;
    new_ent->entry.dir.cwds = 0;
    Flash->tbls_size += (OFDIR_SZ + strlen(new_ent->entry.dir.name) + 1);
#if QUOTA_ENABLED
    if (Flash->quota_enabled)
      Flash->tbls_size += OFDIR_QUOTA_SZ;
#endif /* QUOTA_ENABLED */
  }

  /*-------------------------------------------------------------------*/
  /* Increment the number of links for the original file/dir.          */
  /*-------------------------------------------------------------------*/
  ++old_ent->entry.file.comm->links;

  /*-------------------------------------------------------------------*/
  /* Add the new link to its parent directory.                         */
  /*-------------------------------------------------------------------*/
  old_ent = dir_new->entry.dir.first;
  dir_new->entry.dir.first = new_ent;
  new_ent->entry.dir.next = old_ent;
  new_ent->entry.dir.prev = NULL;
  if (old_ent)
    old_ent->entry.dir.prev = new_ent;

  /*-------------------------------------------------------------------*/
  /* Perform a sync before returning.                                  */
  /*-------------------------------------------------------------------*/
  return FlashSync(Flash->do_sync);
}

/***********************************************************************/
/* scan_for_fid: Search the volume for file/dir entry with fid         */
/*                                                                     */
/*      Inputs: vol = pointer to the volume                            */
/*              fid = file id to scan for                              */
/*                                                                     */
/*     Returns: pointer to entry, NULL if not found                    */
/*                                                                     */
/***********************************************************************/
static FFSEnt *scan_for_fid(const FlashGlob *vol, ui32 fid)
{
  FFSEnts *tbl;
  FFSEnt *entry;
  int i;

  /*-------------------------------------------------------------------*/
  /* Look through all the files tables.                                */
  /*-------------------------------------------------------------------*/
  for (tbl = vol->files_tbl; tbl; tbl = tbl->next_tbl)
  {
    entry = &tbl->tbl[0];
    for (i = 0; i < FNUM_ENT; ++i, ++entry)
      if ((entry->type == FDIREN || entry->type == FFILEN) &&
          ((entry->entry.file.comm->fileno & FSUID_MASK) == fid))
        return entry;
  }

  return NULL;
}

/***********************************************************************/
/* unique_fileno: Find unique fileno to assign to new file/dir         */
/*                                                                     */
/*     Returns: unique fileno                                          */
/*                                                                     */
/***********************************************************************/
static ui32 unique_fileno(void)
{
  /*-------------------------------------------------------------------*/
  /* Keep bumping up the fileno_gen until a unique one is found.       */
  /*-------------------------------------------------------------------*/
  while (scan_for_fid(Flash, Flash->fileno_gen))
    ++Flash->fileno_gen;

  return Flash->fileno_gen++;
}

/***********************************************************************/
/* lookup_fsuid: Search for file/dir based on file id number           */
/*                                                                     */
/*      Inputs: vol = pointer to the volume scanned                    */
/*              buf = buffer to store file/path name                   */
/*              size = size of buf when not NULL                       */
/*              lookup = flag to indicate file or path name            */
/*              fid = file id to scan for                              */
/*                                                                     */
/*     Returns: file/path name, -1 on error                            */
/*                                                                     */
/***********************************************************************/
static void *lookup_fsuid(const FlashGlob *vol, char *buf, size_t size,
                          int lookup, ui32 fid)
{
  FFSEnt *entry;
  char *tmp_buf = buf;

  /*-------------------------------------------------------------------*/
  /* Look for a file/dir entry with fsuid.                             */
  /*-------------------------------------------------------------------*/
  entry = scan_for_fid(vol, fid);

  /*-------------------------------------------------------------------*/
  /* If no such entry found, error.                                    */
  /*-------------------------------------------------------------------*/
  if (entry == NULL)
  {
    set_errno(ENOENT);
    return (void *)-1;
  }

  /*-------------------------------------------------------------------*/
  /* If looking for file name, return it.                              */
  /*-------------------------------------------------------------------*/
  if (lookup == FILE_NAME)
  {
    /*-----------------------------------------------------------------*/
    /* If buffer is supplied, check length is big enough.              */
    /*-----------------------------------------------------------------*/
    if (tmp_buf)
    {
      if (size < strlen(entry->entry.file.name) + 1)
      {
        set_errno(ERANGE);
        return (void *)-1;
      }
    }

    /*-----------------------------------------------------------------*/
    /* Else have to assign space here for the buffer.                  */
    /*-----------------------------------------------------------------*/
    else
    {
      tmp_buf = malloc(strlen(entry->entry.file.name) + 1);
      if (tmp_buf == NULL)
      {
        set_errno(ENOMEM);
        return (void *)-1;
      }
    }

    /*-----------------------------------------------------------------*/
    /* Copy filename into buffer and return it.                        */
    /*-----------------------------------------------------------------*/
    strcpy(tmp_buf, entry->entry.file.name);
    return tmp_buf;
  }

  /*-------------------------------------------------------------------*/
  /* Else looking for the full path name.                              */
  /*-------------------------------------------------------------------*/
  else if (lookup == PATH_NAME)
  {
    tmp_buf = get_path(tmp_buf, size, entry, GET_NAME);
    if (tmp_buf == NULL)
      return (void *)-1;
    return tmp_buf;
  }

  return (void *)-1;
}

#if QUOTA_ENABLED
/***********************************************************************/
/*    spare_at: Compute spare space at a directory                     */
/*                                                                     */
/*       Input: dir = directory for which to compute spare space       */
/*                                                                     */
/*     Returns: Spare space for directory                              */
/*                                                                     */
/***********************************************************************/
static ui32 spare_at(const FFSEnt *dir)
{
  PfAssert(dir->type == FDIREN);

  /*-------------------------------------------------------------------*/
  /* Subtract (if possible) used from reserved(min_q).                 */
  /*-------------------------------------------------------------------*/
  if (dir->entry.dir.min_q > dir->entry.dir.used)
    return dir->entry.dir.min_q - dir->entry.dir.used;
  return 0;
}

/***********************************************************************/
/*     free_at: Compute free space at a directory                      */
/*                                                                     */
/*       Input: dir = directory for which to compute free space        */
/*                                                                     */
/*     Returns: Free space for directory                               */
/*                                                                     */
/***********************************************************************/
static ui32 free_at(const FFSEnt *dir)
{
  ui32 spare;

  PfAssert(dir->type == FDIREN);

  /*-------------------------------------------------------------------*/
  /* First compute spare area for directory.                           */
  /*-------------------------------------------------------------------*/
  spare = spare_at(dir);

  /*-------------------------------------------------------------------*/
  /* Subtract (if possible) free_below from spare.                     */
  /*-------------------------------------------------------------------*/
  if (spare > dir->entry.dir.free_below)
    return spare - dir->entry.dir.free_below;
  return 0;
}
#endif /* QUOTA_ENABLED */

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

#if QUOTA_ENABLED
/***********************************************************************/
/* FlashFreeBelow: Recursively compute 'free_below' for all dirs       */
/*                                                                     */
/*       Input: ent = directory for which to update free below         */
/*                                                                     */
/*     Returns: Updated 'free_below' for directory                     */
/*                                                                     */
/***********************************************************************/
ui32 FlashFreeBelow(FFSEnt *ent)
{
  FFSEnt *dir;
  ui32 free_below = 0;

  PfAssert(ent->type == FDIREN);

  /*-------------------------------------------------------------------*/
  /* Call on the children dirs to update their free below and sum up   */
  /* reserved sectors from this dir that were reserved for the children*/
  /*-------------------------------------------------------------------*/
  for (dir = ent->entry.dir.first; dir; dir = dir->entry.dir.next)
    if (dir->type == FDIREN)
      free_below += FlashFreeBelow(dir);

  /*-------------------------------------------------------------------*/
  /* Set free below for this dir.                                      */
  /*-------------------------------------------------------------------*/
  ent->entry.dir.free_below = free_below;

  /*-------------------------------------------------------------------*/
  /* Return to the parent: free_below + max(0, reserved - used -       */
  /* free_below). This is the value that this directory contributes    */
  /* with for the parent dir's free below value.                       */
  /*-------------------------------------------------------------------*/
  return free_below + free_at(ent);
}

/***********************************************************************/
/* FlashVirtUsed: Compute virtual used amount for root recursively     */
/*                                                                     */
/*       Input: dir = current dir in recursion                         */
/*                                                                     */
/*     Returns: Contribution of this dir to root virtual used value    */
/*                                                                     */
/***********************************************************************/
ui32 FlashVirtUsed(const FFSEnt *dir)
{
  ui32 sum = 0;
  FFSEnt *child;

  /*-------------------------------------------------------------------*/
  /* If this is a normal file, return its control size contribution    */
  /* and its used data.                                                */
  /*-------------------------------------------------------------------*/
  if (dir->type == FFILEN)
    return strlen(dir->entry.file.name) + 1 + OFFIL_SZ + OFCOM_SZ +
           ((dir->entry.file.comm->size + Flash->sect_sz - 1) /
            Flash->sect_sz) * Flash->sect_sz;

  PfAssert(dir->type == FDIREN);

  /*-------------------------------------------------------------------*/
  /* If empty directory, return control size plus reservation.         */
  /*-------------------------------------------------------------------*/
  if (dir->entry.dir.first == NULL)
    return strlen(dir->entry.dir.name) + 1 + OFDIR_SZ + OFCOM_SZ +
           OFDIR_QUOTA_SZ + dir->entry.dir.min_q;

  /*-------------------------------------------------------------------*/
  /* Sum virtual used over every child.                                */
  /*-------------------------------------------------------------------*/
  for (child = dir->entry.dir.first; child; child =child->entry.dir.next)
  {
    /*-----------------------------------------------------------------*/
    /* If child is file, add its control + used space.                 */
    /*-----------------------------------------------------------------*/
    if (child->type == FFILEN)
    {
      sum += (strlen(child->entry.file.name) + 1 + OFFIL_SZ + OFCOM_SZ +
              ((child->entry.file.comm->size + Flash->sect_sz - 1) /
               Flash->sect_sz) * Flash->sect_sz);
    }

    /*-----------------------------------------------------------------*/
    /* Child is dir. Recursively add its virtual used contribution.    */
    /*-----------------------------------------------------------------*/
    else
    {
      PfAssert(child->type == FDIREN);
      sum += FlashVirtUsed(child);
    }
  }

  /*-------------------------------------------------------------------*/
  /* Take max of virtual used and dir's reservation (min_q).           */
  /*-------------------------------------------------------------------*/
  if (sum < dir->entry.dir.min_q)
    sum = dir->entry.dir.min_q;

  /*-------------------------------------------------------------------*/
  /* Add the control space for this directory.                         */
  /*-------------------------------------------------------------------*/
  sum += (strlen(dir->entry.dir.name) + 1 + OFDIR_SZ + OFCOM_SZ +
          OFDIR_QUOTA_SZ);

  /*-------------------------------------------------------------------*/
  /* Return computed virtual used.                                     */
  /*-------------------------------------------------------------------*/
  return sum;
}

/***********************************************************************/
/*   FlashFree: Recursively compute 'free' value for every dir.        */
/*                                                                     */
/*      Inputs: dir = current dir for which to comput free             */
/*              root_spare = spare space at root                       */
/*                                                                     */
/***********************************************************************/
void FlashFree(FFSEnt *dir, ui32 root_spare)
{
  ui32 spare, quota = 0;
  FFSEnt *p;

  PfAssert(dir->type == FDIREN);

  /*-------------------------------------------------------------------*/
  /* Compute spare area.                                               */
  /*-------------------------------------------------------------------*/
  spare = free_at(dir) + root_spare;

  /*-------------------------------------------------------------------*/
  /* For non-root dirs, take into account the free from parent(s).     */
  /*-------------------------------------------------------------------*/
  if (dir->entry.dir.parent_dir)
    for (p = dir->entry.dir.parent_dir; p->entry.dir.parent_dir;
         p = p->entry.dir.parent_dir)
    {
      PfAssert(p->type == FDIREN);

      /*---------------------------------------------------------------*/
      /* Each dir from parent to root contributes max(0, reserved -    */
      /* used - free_below).                                           */
      /*---------------------------------------------------------------*/
      spare += free_at(p);
    }

  /*-------------------------------------------------------------------*/
  /* Take quota minus used at first quota, searching upwards.          */
  /*-------------------------------------------------------------------*/
  for (p = dir; p; p = p->entry.dir.parent_dir)
    if (p->entry.dir.max_q)
    {
      PfAssert(p->entry.dir.max_q >= p->entry.dir.used);
      quota = p->entry.dir.max_q - p->entry.dir.used;
      break;
    }

  /*-------------------------------------------------------------------*/
  /* Take smaller of either spare or quota.                            */
  /*-------------------------------------------------------------------*/
  if (quota < spare)
    dir->entry.dir.free = quota;
  else
    dir->entry.dir.free = spare;

  /*-------------------------------------------------------------------*/
  /* Recursively calculate free for child nodes.                       */
  /*-------------------------------------------------------------------*/
  for (p = dir->entry.dir.first; p; p = p->entry.dir.next)
    if (p->type == FDIREN)
      FlashFree(p, root_spare);
}
#endif /* QUOTA_ENABLED */

/***********************************************************************/
/* FlashPoint2End: Point file entry's position indicator to file's end */
/*                                                                     */
/*      Inputs: pos_ptr = ptr to file control block position record    */
/*              file_entry = ptr to TargetRFS table file entry         */
/*                                                                     */
/***********************************************************************/
void FlashPoint2End(fpos_t *pos_ptr, const FCOM_T *file_entry)
{
  pos_ptr->sector = file_entry->last_sect;
  if (file_entry->one_past_last.sector == (ui16)-1)
  {
    if (file_entry->size)
    {
      pos_ptr->sect_off = (file_entry->size - 1) / Flash->sect_sz;
      pos_ptr->offset = (ui16)(file_entry->size - pos_ptr->sect_off *
                               Flash->sect_sz);
    }
    else
    {
      pos_ptr->offset = 0;
      pos_ptr->sect_off = 0;
    }
  }
  else
  {
    pos_ptr->offset = file_entry->one_past_last.offset;
    pos_ptr->sect_off = (file_entry->size - pos_ptr->offset) /
                        Flash->sect_sz;
  }
}

/***********************************************************************/
/* FlashSeekPastEnd: Extend a file size with 0 value bytes             */
/*                                                                     */
/*      Inputs: offset = length of fseek in bytes                      */
/*              stream = pointer to file control block (FILE)          */
/*              adjust = flag to adjust or not file control block      */
/*                                                                     */
/*     Returns: NULL on success, (void *)-1 on error                   */
/*                                                                     */
/**********************************************************************/
void *FlashSeekPastEnd(i32 offset, FILE *stream, int adjust)
{
  FCOM_T *comm = ((FFSEnt *)stream->handle)->entry.file.comm;
  ui32 num_sects, i;
  CacheEntry *cache_entry;
  ui8 *sector;
  int status;
#if QUOTA_ENABLED
  FFIL_T *link_ptr = &((FFSEnt *)stream->handle)->entry.file;
#endif /* QUOTA_ENABLED */

  /*-------------------------------------------------------------------*/
  /* If in read only mode, can't seek past file end unless offset is 0.*/
  /*-------------------------------------------------------------------*/
  if (!(comm->open_mode & (F_APPEND | F_WRITE)) && offset)
  {
    set_errno(EACCES);
    return (void *)-1;
  }

  /*-------------------------------------------------------------------*/
  /* If stream has a cached entry, free it.                            */
  /*-------------------------------------------------------------------*/
  if (stream->cached && FreeSector(&stream->cached, &Flash->cache))
    return (void *)-1;

  /*-------------------------------------------------------------------*/
  /* If last sector in file is not completely filled, subtract from    */
  /* offset the free part and adjust both offset and end of file.      */
  /*-------------------------------------------------------------------*/
  if (comm->one_past_last.sector != (ui16)-1)
  {
    /*-----------------------------------------------------------------*/
    /* Get pointer to sector.                                          */
    /*-----------------------------------------------------------------*/
    status = GetSector(&Flash->cache, (int)comm->one_past_last.sector,
                       FALSE,((FFSEnt *)stream->handle)->entry.file.comm,
                       &cache_entry);
    if (status != GET_OK)
      return (void *)-1;
    sector = cache_entry->sector;

    /*-----------------------------------------------------------------*/
    /* If FlashWrprWr() will use a new sector, keep track of it, and   */
    /* also set the sector to dirty old because we're writing over it. */
    /*-----------------------------------------------------------------*/
    if (cache_entry->dirty == CLEAN)
    {
      --Flash->free_sects;
      cache_entry->dirty = DIRTY_OLD;
      Flash->cache.dirty_old = TRUE;
    }

    /*-----------------------------------------------------------------*/
    /* If seek fits within this sector stop here.                      */
    /*-----------------------------------------------------------------*/
    if (comm->one_past_last.offset + offset <= Flash->sect_sz)
    {
      /*---------------------------------------------------------------*/
      /* Zero out the new part of the file.                            */
      /*---------------------------------------------------------------*/
      memset(&sector[comm->one_past_last.offset], 0, (size_t)offset);

      /*---------------------------------------------------------------*/
      /* Adjust offset for one past last and file size.                */
      /*---------------------------------------------------------------*/
      comm->one_past_last.offset += (ui16)offset;
      comm->size += (ui32)offset;

      /*---------------------------------------------------------------*/
      /* If we've filled the last sector completely, adjust EOF ptr.   */
      /*---------------------------------------------------------------*/
      if (comm->one_past_last.offset == Flash->sect_sz)
      {
        comm->one_past_last.sector = (ui16)-1;
        comm->one_past_last.offset = 0;
      }

      /*---------------------------------------------------------------*/
      /* Else if requested, set cached and adjust current pointer.     */
      /*---------------------------------------------------------------*/
      else if (adjust)
      {
        stream->cached = cache_entry;
        FlashPoint2End(&stream->curr_ptr, comm);
      }

      /*---------------------------------------------------------------*/
      /* Else free cached sector since current pointer is not set.     */
      /*---------------------------------------------------------------*/
      else if (FreeSector(&cache_entry, &Flash->cache))
        goto err_return;

      return NULL;
    }

    /*-----------------------------------------------------------------*/
    /* Else fill out last sector completely and look for other sects.  */
    /*-----------------------------------------------------------------*/
    else
    {
      /*---------------------------------------------------------------*/
      /* Zero out the new part of the file.                            */
      /*---------------------------------------------------------------*/
      memset(&sector[comm->one_past_last.offset], 0,
             Flash->sect_sz - comm->one_past_last.offset);

      /*---------------------------------------------------------------*/
      /* Adjust file size and one past last.                           */
      /*---------------------------------------------------------------*/
      comm->size += (Flash->sect_sz - comm->one_past_last.offset);
      offset -= (Flash->sect_sz - comm->one_past_last.offset);
      comm->one_past_last.offset = 0;
      comm->one_past_last.sector = (ui16)-1;
    }

    /*-----------------------------------------------------------------*/
    /* Free cached sector.                                             */
    /*-----------------------------------------------------------------*/
    if (FreeSector(&cache_entry, &Flash->cache))
      goto err_return;
  }

  /*-------------------------------------------------------------------*/
  /* Figure out how many new sectors we need.                          */
  /*-------------------------------------------------------------------*/
  num_sects = (ui32)(offset + Flash->sect_sz - 1) / Flash->sect_sz;
  comm->one_past_last.offset = (ui16)((ui32)offset % Flash->sect_sz);

#if QUOTA_ENABLED
  /*-------------------------------------------------------------------*/
  /* If quotas enabled, check if new sectors can be added to file.     */
  /*-------------------------------------------------------------------*/
  if (Flash->quota_enabled)
  {
    if (link_ptr->parent_dir->entry.dir.free < num_sects *Flash->sect_sz)
    {
      set_errno(ENOMEM);
      goto err_return;
    }
  }
#endif /* QUOTA_ENABLED */

  /*-------------------------------------------------------------------*/
  /* Try to assign num_sects worth of free sectors to the file.        */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < num_sects; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* Set the free sector to where we can write to it.                */
    /*-----------------------------------------------------------------*/
    if (FlashFrSect(comm))
      goto err_return;

    /*-----------------------------------------------------------------*/
    /* If file is empty, set first sector pointer.                     */
    /*-----------------------------------------------------------------*/
    if (!i && comm->frst_sect == FFREE_SECT)
    {
      comm->frst_sect = (ui16)Flash->free_sect;
      Flash->sect_tbl[Flash->free_sect].prev = FLAST_SECT;
      FlashUpdateFCBs(stream, comm);
    }

    /*-----------------------------------------------------------------*/
    /* Else set only last sector pointer.                              */
    /*-----------------------------------------------------------------*/
    else
    {
      /*---------------------------------------------------------------*/
      /* Make previous last sector point to new last sector.           */
      /*---------------------------------------------------------------*/
      Flash->sect_tbl[comm->last_sect].next = (ui16)Flash->free_sect;
      Flash->sect_tbl[Flash->free_sect].prev = comm->last_sect;
    }

    /*-----------------------------------------------------------------*/
    /* Set last sector in file and sectors table.                      */
    /*-----------------------------------------------------------------*/
    comm->last_sect = (ui16)Flash->free_sect;

    /*-----------------------------------------------------------------*/
    /* Update free sector and increment file size by Flash->sect_sz.   */
    /*-----------------------------------------------------------------*/
    if (Flash->free_sect != Flash->last_free_sect)
    {
      Flash->free_sect = Flash->sect_tbl[Flash->free_sect].next;
      --Flash->free_sects;
    }
    else
    {
      PfAssert(FALSE); /*lint !e506, !e774*/
      goto err_return;
    }
    if (i != num_sects - 1)
      comm->size += Flash->sect_sz;
    Flash->sect_tbl[comm->last_sect].next = FLAST_SECT;

    /*-----------------------------------------------------------------*/
    /* Update total number of used and block used count for last sect. */
    /*-----------------------------------------------------------------*/
    ++Flash->used_sects;
    ++Flash->blocks[comm->last_sect / Flash->block_sects].used_sects;

    /*-----------------------------------------------------------------*/
    /* Get pointer to sector.                                          */
    /*-----------------------------------------------------------------*/
    status = GetSector(&Flash->cache, comm->last_sect, TRUE,
                       ((FFSEnt *)stream->handle)->entry.file.comm,
                       &cache_entry);
    if (status != GET_OK)
      goto err_return;
    if (cache_entry->dirty == CLEAN)
    {
      cache_entry->dirty = DIRTY_NEW;
      Flash->cache.dirty_new = TRUE;
    }
    sector = cache_entry->sector;

    /*-----------------------------------------------------------------*/
    /* Zero out all sector if not last one and only offset if last.    */
    /*-----------------------------------------------------------------*/
    if (i == num_sects - 1)
    {
      memset(sector, 0, (size_t)offset);
      if (offset == Flash->sect_sz)
        comm->one_past_last.sector = (ui16)-1;
      else
        comm->one_past_last.sector = comm->last_sect;
    }
    else
    {
      memset(sector, 0, Flash->sect_sz);
      offset -= Flash->sect_sz;
    }

    /*-----------------------------------------------------------------*/
    /* If file control block points to last sector and is updatable,   */
    /* don't free the cached sector, else free it.                     */
    /*-----------------------------------------------------------------*/
    if (adjust && i == num_sects - 1 && comm->one_past_last.offset)
      stream->cached = cache_entry;
    else if (FreeSector(&cache_entry, &Flash->cache))
      goto err_return;
  }

  /*-------------------------------------------------------------------*/
  /* Increment file size by one_past_last.offset.                      */
  /*-------------------------------------------------------------------*/
  comm->size += offset;

  /*-------------------------------------------------------------------*/
  /* Adjust curr_ptr if file control block is updatable.               */
  /*-------------------------------------------------------------------*/
  if (adjust)
    FlashPoint2End(&stream->curr_ptr, comm);

#if QUOTA_ENABLED
  /*-------------------------------------------------------------------*/
  /* If quotas enabled, adjust quota values due to new sectors.        */
  /*-------------------------------------------------------------------*/
  if (Flash->quota_enabled)
  {
    FFSEnt *ent, *root = &Flash->files_tbl->tbl[0];

    /*-----------------------------------------------------------------*/
    /* Update used from parent to root.                                */
    /*-----------------------------------------------------------------*/
    for (ent = link_ptr->parent_dir; ent; ent =ent->entry.dir.parent_dir)
    {
      PfAssert(ent->type == FDIREN);
      ent->entry.dir.used += num_sects * Flash->sect_sz;
    }

    /*-----------------------------------------------------------------*/
    /* Update free below.                                              */
    /*-----------------------------------------------------------------*/
    FlashFreeBelow(root);

    /*-----------------------------------------------------------------*/
    /* Recompute free at each node.                                    */
    /*-----------------------------------------------------------------*/
    FlashFree(root, root->entry.dir.max_q - FlashVirtUsed(root));
  }
#endif /* QUOTA_ENABLED */

  return NULL;

err_return:
  comm->one_past_last.offset = 0;
  comm->one_past_last.sector = (ui16)-1;
  return (void *)-1;
}

/***********************************************************************/
/* FlashIncrementTblEntries: Increment the number of free entries      */
/*                                                                     */
/*       Input: entry = pointer to freed table entry                   */
/*                                                                     */
/***********************************************************************/
void FlashIncrementTblEntries(const FFSEnt *entry)
{
  FFSEnts *table;

  /*-------------------------------------------------------------------*/
  /* Look for table that contains the freed entry.                     */
  /*-------------------------------------------------------------------*/
  for (table = Flash->files_tbl; table; table = table->next_tbl)
  {
    /*-----------------------------------------------------------------*/
    /* If found table with entry, increment number of free entries.    */
    /*-----------------------------------------------------------------*/
    if (entry >= &table->tbl[0] && entry <= &table->tbl[FNUM_ENT - 1])
    {
      ++table->free;
      break;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Ensure that the table was found.                                  */
  /*-------------------------------------------------------------------*/
  PfAssert(table);
}

/***********************************************************************/
/* FlashGarbageCollect: Remove table with most free entries            */
/*                                                                     */
/***********************************************************************/
void FlashGarbageCollect(void)
{
  int i, max_free = 0;
  FFSEnt *entry;
  FFSEnts *table = NULL, *curr_tbl;

  /*-------------------------------------------------------------------*/
  /* Skip if any opendir() has unmatched closedir().                   */
  /*-------------------------------------------------------------------*/
  if (Flash->opendirs)
    return;

  /*-------------------------------------------------------------------*/
  /* Walk thru all tables except the first one, finding the one with   */
  /* most free entries.                                                */
  /*-------------------------------------------------------------------*/
  for (curr_tbl = Flash->files_tbl->next_tbl; curr_tbl;
       curr_tbl = curr_tbl->next_tbl)
  {
    /*-----------------------------------------------------------------*/
    /* Remember table with most free entries.                          */
    /*-----------------------------------------------------------------*/
    if (curr_tbl->free > max_free)
    {
      table = curr_tbl;
      max_free = curr_tbl->free;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Loop until all entries on this table are free.                    */
  /*-------------------------------------------------------------------*/
  while (table)
  {
    /*-----------------------------------------------------------------*/
    /* Check if table is now empty.                                    */
    /*-----------------------------------------------------------------*/
    if (table->free == FNUM_ENT)
    {
      PfAssert(Flash->files_tbl != table);

      /*---------------------------------------------------------------*/
      /* Remove and free table. Update counters, and then return.      */
      /*---------------------------------------------------------------*/
      if (table->prev_tbl)
        table->prev_tbl->next_tbl = table->next_tbl;
      if (table->next_tbl)
        table->next_tbl->prev_tbl = table->prev_tbl;
      table->next_tbl = table->prev_tbl = NULL;
      free(table);
      Flash->total_free -= FNUM_ENT;
      Flash->total -= FNUM_ENT;
      return;
    }

    /*-----------------------------------------------------------------*/
    /* Look for the next non-empty entry in the table to be moved.     */
    /*-----------------------------------------------------------------*/
    for (i = 0; i < FNUM_ENT; ++i)
    {
      if (table->tbl[i].type != FEMPTY)
      {
        /*-------------------------------------------------------------*/
        /* Find a place for the non-empty entry to be moved to.        */
        /*-------------------------------------------------------------*/
        entry = find_new_place(table);

        /*-------------------------------------------------------------*/
        /* If no more free entries, stop the garbage collection.       */
        /*-------------------------------------------------------------*/
        if (entry == NULL)
          return;

        /*-------------------------------------------------------------*/
        /* Move entry and update all pointers to it.                   */
        /*-------------------------------------------------------------*/
        entry->type = table->tbl[i].type;
        entry->entry = table->tbl[i].entry;
        table->tbl[i].type = FEMPTY;
        ++table->free;
        update_entries(&table->tbl[i], entry);
      }
    }
  }
}

/***********************************************************************/
/* FlashUpdateFCBs: Adjust all open file pointers when a file is not   */
/*              empty anylonger                                        */
/*                                                                     */
/*      Inputs: skip = FCB to skip                                     */
/*              handle = pointer to the file                           */
/*                                                                     */
/***********************************************************************/
void FlashUpdateFCBs(const FILE *skip, const FCOM_T *comm_ptr)
{
  FILE *stream;

  /*-------------------------------------------------------------------*/
  /* Loop through all files adjusting the ones that need to.           */
  /*-------------------------------------------------------------------*/
  for (stream = &Files[0]; stream < &Files[FOPEN_MAX]; ++stream)
  {
    /*-----------------------------------------------------------------*/
    /* Check if we need to skip.                                       */
    /*-----------------------------------------------------------------*/
    if (stream == skip)
      continue;

    /*-----------------------------------------------------------------*/
    /* Adjust file if we need to.                                      */
    /*-----------------------------------------------------------------*/
    if (stream->ioctl == FlashIoctl && stream->volume == Flash &&
        (stream->flags & FCB_FILE) &&
        ((FFIL_T *)stream->handle)->comm == comm_ptr)
    {
      stream->curr_ptr.sector = comm_ptr->frst_sect;
      stream->curr_ptr.offset = 0;
      stream->curr_ptr.sect_off = 0;
      stream->old_ptr = stream->curr_ptr;
    }
  }
}

/***********************************************************************/
/*  FlashIoctl: Perform multiple file functions for flash file system  */
/*                                                                     */
/*      Inputs: stream = holds either file or dir ctrl block ptr       */
/*              code = selects the function to be performed            */
/*                                                                     */
/*     Returns: The value of the function called                       */
/*                                                                     */
/***********************************************************************/
void *FlashIoctl(FILE *stream, int code, ...)
{
  void *vp = NULL;
  va_list ap;
  FFSEnts *curr_table1, *curr_table2;
  FFSEnt *dir_ent, *tmp_ent, *v_ent;
  FFIL_T *link_ptr;
  FDIR_T *dir;
  ui32 i, j, end;
  char *name;
  mode_t mode;
  uid_t uid;
  gid_t gid;
  FlashGlob *vol;
  int sync_f;

  /*-------------------------------------------------------------------*/
  /* Split actions into those with a FILE* arg and those without.      */
  /*-------------------------------------------------------------------*/
  if (stream == NULL)
  {
    switch (code)
    {
      /*---------------------------------------------------------------*/
      /* Handle chdir() current working directory count decrement.     */
      /*---------------------------------------------------------------*/
      case CHDIR_DEC:
        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        tmp_ent = va_arg(ap, FFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Decrement previous working directory count if any.          */
        /*-------------------------------------------------------------*/
        if (tmp_ent)
        {
          PfAssert(tmp_ent->entry.dir.cwds);
          --tmp_ent->entry.dir.cwds;
        }

        return NULL;

      /*---------------------------------------------------------------*/
      /* Handle getcwd() for the flash file system.                    */
      /*---------------------------------------------------------------*/
      case GETCWD:
      {
        char *buf;
        size_t size;
        FFSEnt *curr_dir;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        buf = va_arg(ap, char *);
        size = va_arg(ap, size_t);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Read current working directory.                             */
        /*-------------------------------------------------------------*/
        FsReadCWD((void *)&vol, (void *)&curr_dir);

        /*-------------------------------------------------------------*/
        /* Acquire exclusive access to volume and flash file system.   */
        /*-------------------------------------------------------------*/
        semPend(vol->sem, WAIT_FOREVER);
        semPend(FlashSem, WAIT_FOREVER);
        Flash = vol;

        /*-------------------------------------------------------------*/
        /* If there is a current working directory, get its name, else */
        /* set errno and return NULL.                                  */
        /*-------------------------------------------------------------*/
        if (curr_dir)
          buf = get_path(buf, size, curr_dir, GETCWD);
        else
        {
          set_errno(EINVAL);
          buf = NULL;
        }

        /*-------------------------------------------------------------*/
        /* Release access to flash file system and this volume.        */
        /*-------------------------------------------------------------*/
        semPost(FlashSem);
        semPost(vol->sem);
        return buf;
      }

      /*---------------------------------------------------------------*/
      /* Handle vclean() for the flash file system.                    */
      /*---------------------------------------------------------------*/
      case VCLEAN:
      {
        ui32 dirty_sects, ctrl_blocks;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        vol = (FlashGlob *)va_arg(ap, void *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Acquire exclusive access to volume and flash file system.   */
        /*-------------------------------------------------------------*/
        semPend(vol->sem, WAIT_FOREVER);
        semPend(FlashSem, WAIT_FOREVER);
        Flash = vol;

        /*-------------------------------------------------------------*/
        /* Figure out how many blocks contain control information.     */
        /*-------------------------------------------------------------*/
        ctrl_blocks = 0;
        if (Flash->frst_ctrl_sect != (ui32)-1)
        {
          i = Flash->sect_sz * (Flash->frst_ctrl_sect -
              (Flash->frst_ctrl_sect / Flash->block_sects) *
              Flash->block_sects);
          ctrl_blocks = (Flash->ctrl_size + i + Flash->block_size - 1) /
                        Flash->block_size;
        }

        /*-------------------------------------------------------------*/
        /* Figure out the number of dirty sectors in volume.           */
        /*-------------------------------------------------------------*/
        dirty_sects = Flash->num_sects - Flash->num_blocks *
                           Flash->hdr_sects - Flash->free_sects -
                           Flash->used_sects -
                           (Flash->bad_blocks + ctrl_blocks) *
                           Flash->block_sects;

        /*-------------------------------------------------------------*/
        /* Do a recycle when number of free sectors below the reserved */
        /* threshold, or when enough dirty sectors around, or when a   */
        /* recycle is needed.                                          */
        /*-------------------------------------------------------------*/
        if (Flash->free_sects < EXTRA_FREE / Flash->sect_sz ||
            dirty_sects > Flash->set_blocks * Flash->block_sects ||
            FlashRecNeeded())
        {
          /*-----------------------------------------------------------*/
          /* Flush sectors from cache to flash.                        */
          /*-----------------------------------------------------------*/
          if (FlushSectors(&Flash->cache) == -1)
          {
            semPost(FlashSem);
            semPost(vol->sem);
            return (void *)-1;
          }

          /*-----------------------------------------------------------*/
          /* Perform recycle.                                          */
          /*-----------------------------------------------------------*/
          if (FlashRecycle(CHECK_FULL))
          {
            semPost(FlashSem);
            semPost(vol->sem);
            return (void *)-1;
          }

          /*-----------------------------------------------------------*/
          /* Figure out how many blocks contain control information.   */
          /*-----------------------------------------------------------*/
          ctrl_blocks = 0;
          if (Flash->frst_ctrl_sect != (ui32)-1)
          {
            i = Flash->sect_sz * (Flash->frst_ctrl_sect -
                (Flash->frst_ctrl_sect / Flash->block_sects) *
                Flash->block_sects);
            ctrl_blocks = (Flash->ctrl_size + i + Flash->block_size - 1)
                           / Flash->block_size;
          }

          /*-----------------------------------------------------------*/
          /* Update the volume's dirty sector count.                   */
          /*-----------------------------------------------------------*/
          dirty_sects = Flash->num_sects - Flash->num_blocks *
                             Flash->hdr_sects - Flash->free_sects -
                             Flash->used_sects -
                             (Flash->bad_blocks + ctrl_blocks) *
                             Flash->block_sects;

          /*-----------------------------------------------------------*/
          /* Determine if another call to vclean() is needed.          */
          /*-----------------------------------------------------------*/
          vp = (void *)(Flash->free_sects < EXTRA_FREE / Flash->sect_sz
                || dirty_sects > Flash->set_blocks * Flash->block_sects);
        }

        /*-------------------------------------------------------------*/
        /* Release access to flash file system and this volume.        */
        /*-------------------------------------------------------------*/
        semPost(FlashSem);
        semPost(vol->sem);
        break;
      }

      /*---------------------------------------------------------------*/
      /* Handle vstat() for the flash file system.                     */
      /*---------------------------------------------------------------*/
      case VSTAT:
      {
        union vstat *buf;
        ui32 block_sects, used, ctrl_blocks, tot;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        vol = (FlashGlob *)va_arg(ap, void *);
        buf = va_arg(ap, union vstat *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Acquire exclusive access to volume and flash file system.   */
        /*-------------------------------------------------------------*/
        semPend(vol->sem, WAIT_FOREVER);
        semPend(FlashSem, WAIT_FOREVER);
        Flash = vol;

        /*-------------------------------------------------------------*/
        /* Write the type.                                             */
        /*-------------------------------------------------------------*/
        buf->ffs.vol_type = FFS_VOL;
        buf->ffs.flash_type = Flash->type;

        /*-------------------------------------------------------------*/
        /* Write in total num sects, highest wear count, used sects and*/
        /* bad blocks count.                                           */
        /*-------------------------------------------------------------*/
        buf->ffs.num_sects = Flash->num_sects -
                             Flash->num_blocks * Flash->hdr_sects -
                             Flash->max_bad_blocks * Flash->block_sects;
        buf->ffs.wear_count = Flash->blocks[0].wear_count;
        buf->ffs.used_sects = Flash->used_sects;
        buf->ffs.sect_size = Flash->sect_sz;
        buf->ffs.bad_blocks = Flash->bad_blocks;
        buf->ffs.max_bad_blocks = Flash->max_bad_blocks;

        /*-------------------------------------------------------------*/
        /* Figure out number of sectors that can be used before recycle*/
        /*-------------------------------------------------------------*/
        FlashRecycleValues(&ctrl_blocks, &used, &block_sects);
        tot = ctrl_blocks * block_sects + used;
        if (tot > Flash->free_sects)
          buf->ffs.sects_2recycle = 0;
        else
          buf->ffs.sects_2recycle = Flash->free_sects - tot;

        /*-------------------------------------------------------------*/
        /* Based on volume full equation, figure out available sects.  */
        /*-------------------------------------------------------------*/
        buf->ffs.avail_sects = FlashRoom(0);

        /*-------------------------------------------------------------*/
        /* Release access to flash file system and this volume.        */
        /*-------------------------------------------------------------*/
        semPost(FlashSem);
        semPost(vol->sem);

        /*-------------------------------------------------------------*/
        /* Return success.                                             */
        /*-------------------------------------------------------------*/
        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle tmpnam() for the flash file system.                    */
      /*---------------------------------------------------------------*/
      case TMPNAM:
        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get arguments.                  */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Check all entries for name duplication.                     */
        /*-------------------------------------------------------------*/
        return (void *)check_name(name);

      /*---------------------------------------------------------------*/
      /* Handle unmount() for the flash file system.                   */
      /*---------------------------------------------------------------*/
      case UNMOUNT:
      {
        FFSEnts *temp_table, *prev_table;
        int do_sync, modified = FALSE;
        FlashGlob *currvol;
        ui32 dummy;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get pointer to volume ctrls.    */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        vol = (FlashGlob *)va_arg(ap, void *);
        do_sync = va_arg(ap, int);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Acquire exclusive access to volume and flash file system.   */
        /*-------------------------------------------------------------*/
        semPend(vol->sem, WAIT_FOREVER);
        semPend(FlashSem, WAIT_FOREVER);
        Flash = vol;

        /*-------------------------------------------------------------*/
        /* Make sure that all FILE*s and DIR*s associated with this    */
        /* volume are freed.                                           */
        /*-------------------------------------------------------------*/
        for (stream = &Files[0]; stream < &Files[FOPEN_MAX]; ++stream)
        {
          /*-----------------------------------------------------------*/
          /* If a FILE/DIR is associated with this volume, clear it.   */
          /*-----------------------------------------------------------*/
          if (stream->ioctl == FlashIoctl && stream->volume == Flash)
          {
            if (stream->flags & FCB_FILE)
            {
              /*-------------------------------------------------------*/
              /* Modified files could lead to a sync.                  */
              /*-------------------------------------------------------*/
              if (stream->flags & FCB_MOD)
                modified = DO_SYNC;
              close_file(stream->handle, stream);
            }
            else
              close_dir(stream->handle, stream);
            FsInitFCB(stream, FCB_FILE);
          }
        }

        /*-------------------------------------------------------------*/
        /* Do a sync if disable syncs or no control write.             */
        /*-------------------------------------------------------------*/
        if ((!Flash->do_sync && Flash->sync_deferred) ||
            !Flash->wr_ctrl)
          modified = DO_SYNC;

        /*-------------------------------------------------------------*/
        /* Sync the volume.                                            */
        /*-------------------------------------------------------------*/
        if (do_sync)
          vp = FlashSync(modified);
  #if FFS_DEBUG
  printf("UNMOUNT: 1st_ctrl = %u, ctrls = %u, 1st_free = %u, "
         "frees = %u\n", Flash->frst_ctrl_sect, Flash->ctrl_sects,
         Flash->free_sect, Flash->free_sects);
  #endif

        /*-------------------------------------------------------------*/
        /* Free the memory allocated for sectors table.                */
        /*-------------------------------------------------------------*/
        free(Flash->sect_tbl);
        Flash->sect_tbl = NULL;

        /*-------------------------------------------------------------*/
        /* Free the memory allocated for the used block counts.        */
        /*-------------------------------------------------------------*/
        free(Flash->blocks);
        Flash->blocks = NULL;

        /*-------------------------------------------------------------*/
        /* Free the memory allocated for the erased set.               */
        /*-------------------------------------------------------------*/
        free(Flash->erase_set);
        Flash->erase_set = NULL;

        /*-------------------------------------------------------------*/
        /* Destroy the cache.                                          */
        /*-------------------------------------------------------------*/
        DestroyCache(&Flash->cache);

        /*-------------------------------------------------------------*/
        /* Free up all the files tables associated with volume.        */
        /*-------------------------------------------------------------*/
        for (temp_table = Flash->files_tbl; temp_table;)
        {
          prev_table = temp_table;
          temp_table = temp_table->next_tbl;
          free(prev_table);
        }
        Flash->files_tbl = NULL;

        /*-------------------------------------------------------------*/
        /* Clear out the other ctrl variables.                         */
        /*-------------------------------------------------------------*/
        Flash->total_free = Flash->total = Flash->free_sects = 0;
        Flash->free_sect = Flash->used_sects = 0;

        /*-------------------------------------------------------------*/
        /* If CWD is somewhere in this volume, set it to root.         */
        /*-------------------------------------------------------------*/
        FsReadCWD((void *)&currvol, &dummy);
        if (Flash == currvol)
          FsSaveCWD(0, 0);

        /*-------------------------------------------------------------*/
        /* Release access to flash file system and this volume.        */
        /*-------------------------------------------------------------*/
        semPost(FlashSem);
        semPost(vol->sem);
        break;
      }

      /*---------------------------------------------------------------*/
      /* Handle enable_sync() for the flash file system.               */
      /*---------------------------------------------------------------*/
      case ENABLE_SYNC:
        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        vol = (FlashGlob *)va_arg(ap, void *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Acquire exclusive access to volume and flash file system.   */
        /*-------------------------------------------------------------*/
        semPend(vol->sem, WAIT_FOREVER);
        semPend(FlashSem, WAIT_FOREVER);

        /*-------------------------------------------------------------*/
        /* Enable syncs.                                               */
        /*-------------------------------------------------------------*/
        vol->do_sync = TRUE;

        /*-------------------------------------------------------------*/
        /* If prior call to disable_sync() and volume mounted, sync.   */
        /*-------------------------------------------------------------*/
        if (vol->sync_deferred && (vol->sys.next || vol->sys.prev ||
                                   (&vol->sys == MountedList.head)))
        {
          Flash = vol;
          vp = FlashSync(DO_SYNC);
        }
        vol->sync_deferred = FALSE;

        /*-------------------------------------------------------------*/
        /* Release access to flash file system and this volume.        */
        /*-------------------------------------------------------------*/
        semPost(FlashSem);
        semPost(vol->sem);
        return vp;

      /*---------------------------------------------------------------*/
      /* Handle disable_sync() for the flash file system.              */
      /*---------------------------------------------------------------*/
      case DISABLE_SYNC:
        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        vol = (FlashGlob *)va_arg(ap, void *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Acquire exclusive access to volume and flash file system.   */
        /*-------------------------------------------------------------*/
        semPend(vol->sem, WAIT_FOREVER);
        semPend(FlashSem, WAIT_FOREVER);

        /*-------------------------------------------------------------*/
        /* Disable syncs.                                              */
        /*-------------------------------------------------------------*/
        vol->do_sync = FALSE;

        /*-------------------------------------------------------------*/
        /* Release access to flash file system and this volume.        */
        /*-------------------------------------------------------------*/
        semPost(FlashSem);
        semPost(vol->sem);
        return NULL;

      /*---------------------------------------------------------------*/
      /* Handle FSUID to name mapping for the flash file system.       */
      /*---------------------------------------------------------------*/
      case GET_NAME:
      {
        size_t size;
        int lookup;
        ui32 vid, fid;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        vol = (FlashGlob *)va_arg(ap, void *);
        name = va_arg(ap, char *);
        size = va_arg(ap, size_t);
        lookup = va_arg(ap, int);
        vid = va_arg(ap, ui32);
        fid = va_arg(ap, ui32);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Acquire exclusive access to volume and flash file system.   */
        /*-------------------------------------------------------------*/
        semPend(vol->sem, WAIT_FOREVER);
        semPend(FlashSem, WAIT_FOREVER);

        /*-------------------------------------------------------------*/
        /* If the volume id does not match, skip this volume.          */
        /*-------------------------------------------------------------*/
        if (vol->vid != vid)
        {
          semPost(FlashSem);
          semPost(vol->sem);
          return NULL;
        }

        /*-------------------------------------------------------------*/
        /* Perform lookup based on file id.                            */
        /*-------------------------------------------------------------*/
        vp = lookup_fsuid(vol, name, size, lookup, fid);

        /*-------------------------------------------------------------*/
        /* Release access to flash file system and this volume.        */
        /*-------------------------------------------------------------*/
        semPost(FlashSem);
        semPost(vol->sem);
        return vp;
      }


      /*---------------------------------------------------------------*/
      /* All unimplemented API functions return error.                 */
      /*---------------------------------------------------------------*/
      default:
      {
        set_errno(EINVAL);
        return (void *)-1;
      }
    }
  }
  else
  {
    void *handle = stream->handle;
    switch (code)
    {
      /*---------------------------------------------------------------*/
      /* Handle chdir() for the flash file system.                     */
      /*---------------------------------------------------------------*/
      case CHDIR:
        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        dir_ent = va_arg(ap, FFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Check for search permission for the directory.              */
        /*-------------------------------------------------------------*/
        if (CheckPerm(dir_ent->entry.dir.comm, F_EXECUTE))
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* Decrement previous working directory count if any.          */
        /*-------------------------------------------------------------*/
        FsReadCWD((void *)&vol, (void *)&tmp_ent);
        if (vol)
          vol->sys.ioctl(NULL, CHDIR_DEC, tmp_ent); /*lint !e522*/

        /*-------------------------------------------------------------*/
        /* Assign current working directory and increment count.       */
        /*-------------------------------------------------------------*/
        FsSaveCWD((ui32)Flash, (ui32)dir_ent);
        ++dir_ent->entry.dir.cwds;
        break;

      /*---------------------------------------------------------------*/
      /* Handle mkdir() for the flash file system.                     */
      /*---------------------------------------------------------------*/
      case MKDIR:
      {
        FFSEnt *fep;
        ui32 used;
#if QUOTA_ENABLED
        ui32 max_q = 0, min_q = 0;
#endif /* QUOTA_ENABLED */

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        mode = (mode_t)va_arg(ap, int);
        dir_ent = va_arg(ap, FFSEnt *);
#if QUOTA_ENABLED
        if (Flash->quota_enabled)
        {
          min_q = va_arg(ap, ui32);
          max_q = va_arg(ap, ui32);
        }
#endif /* QUOTA_ENABLED */
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* If invalid name, return error.                              */
        /*-------------------------------------------------------------*/
        if (InvalidName(name, TRUE))
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* Get pointer to the directory in which mkdir takes place.    */
        /*-------------------------------------------------------------*/
        dir = &dir_ent->entry.dir;

        /*-------------------------------------------------------------*/
        /* Check that parent directory has search/write permissions.   */
        /*-------------------------------------------------------------*/
        if (CheckPerm(dir->comm, F_WRITE | F_EXECUTE))
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* Figure out how much space this directory takes.             */
        /*-------------------------------------------------------------*/
        used = strlen(name) + 1 + OFDIR_SZ + OFCOM_SZ;

#if QUOTA_ENABLED
        /*-------------------------------------------------------------*/
        /* If quota enabled, check if mkdir can succeed.               */
        /*-------------------------------------------------------------*/
        if (Flash->quota_enabled)
        {
          /*-----------------------------------------------------------*/
          /* For parent with reserved, check for consistency.          */
          /*-----------------------------------------------------------*/
          if (min_q && dir->min_q)
          {
            if (dir->min_q < min_q + dir->res_below)
            {
              set_errno(EINVAL);
              return (void *)-1;
            }
          }

          /*-----------------------------------------------------------*/
          /* Check for quota consistency.                              */
          /*-----------------------------------------------------------*/
          if (max_q > dir->max_q)
          {
            set_errno(EINVAL);
            return (void *)-1;
          }

          /*-----------------------------------------------------------*/
          /* Add quota space to space used for directory.              */
          /*-----------------------------------------------------------*/
          used += OFDIR_QUOTA_SZ;

          /*-----------------------------------------------------------*/
          /* Check for free space.                                     */
          /*-----------------------------------------------------------*/
          if (dir->free < used + min_q)
          {
            set_errno(ENOMEM);
            return (void *)-1;
          }
        }
#endif /* QUOTA_ENABLED */

        /*-------------------------------------------------------------*/
        /* If no room left in the volume, return error.                */
        /*-------------------------------------------------------------*/
        if (!FlashRoom(used))
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* While there are terminating '/' charactors, remove them.    */
        /*-------------------------------------------------------------*/
        end = (ui32)(strlen(name) - 1);
        while (name[end] == '/')
          name[end--] = '\0';

        /*-------------------------------------------------------------*/
        /* Look for name duplication with the new directory.           */
        /*-------------------------------------------------------------*/
        for (fep = dir->first; fep; fep = fep->entry.dir.next)
        {
          /*-----------------------------------------------------------*/
          /* If a file with a given name exists, return error.         */
          /*-----------------------------------------------------------*/
          if (!strcmp(fep->entry.dir.name, name))
          {
            set_errno(EEXIST);
            return (void *)-1;
          }
        }

        /*-------------------------------------------------------------*/
        /* Look for an entry to hold the dir struct associated with    */
        /* this directory.                                             */
        /*-------------------------------------------------------------*/
        curr_table1 = find_entry(&i);

        /*-------------------------------------------------------------*/
        /* If no more free entries available, stop.                    */
        /*-------------------------------------------------------------*/
        if (curr_table1 == NULL)
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* Look for an entry to hold the file struct associated with   */
        /* this directory.                                             */
        /*-------------------------------------------------------------*/
        curr_table2 = find_entry(&j);

        /*-------------------------------------------------------------*/
        /* If no more entries available, stop.                         */
        /*-------------------------------------------------------------*/
        if (curr_table2 == NULL)
        {
          curr_table1->tbl[i].type = FEMPTY;
          ++curr_table1->free;
          ++Flash->total_free;
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* Setup the directory common entry.                           */
        /*-------------------------------------------------------------*/
        v_ent = &curr_table2->tbl[j];
        v_ent->type = FCOMMN;
        v_ent->entry.comm.open_mode = 0;
        v_ent->entry.comm.links = 1;
        v_ent->entry.comm.attrib = 0;
        v_ent->entry.comm.open_links = 0;
        v_ent->entry.comm.frst_sect = (ui16)-1;
        v_ent->entry.comm.last_sect = (ui16)-1;
        v_ent->entry.comm.mod_time =v_ent->entry.comm.ac_time=OsSecCount;
        v_ent->entry.comm.one_past_last.offset = 0;
        v_ent->entry.comm.one_past_last.sector = (ui16)-1;
        v_ent->entry.comm.addr = v_ent;
        v_ent->entry.comm.size = 0;

        /*-------------------------------------------------------------*/
        /* If fileno is passed 2^FID_LEN, have to check for unicity.   */
        /*-------------------------------------------------------------*/
        if (check_filenos(Flash->fileno_gen))
          v_ent->entry.comm.fileno = unique_fileno();
        else
          v_ent->entry.comm.fileno = Flash->fileno_gen++;

        /*-------------------------------------------------------------*/
        /* Setup the directory entry in the Flash->files_tbl.          */
        /*-------------------------------------------------------------*/
        v_ent = &curr_table1->tbl[i];
        v_ent->type = FDIREN;
        v_ent->entry.dir.cwds = 0;
        v_ent->entry.dir.dir = NULL;
        v_ent->entry.dir.first = NULL;
        v_ent->entry.dir.parent_dir = dir_ent;
        v_ent->entry.dir.prev = NULL;
        v_ent->entry.dir.comm = &(curr_table2->tbl[j].entry.comm);
        strcpy(v_ent->entry.dir.name, name);

        /*-------------------------------------------------------------*/
        /* Set the permissions for the directory.                      */
        /*-------------------------------------------------------------*/
        SetPerm(v_ent->entry.dir.comm, mode);

        /*-------------------------------------------------------------*/
        /* Add the new directory to its parent directory.              */
        /*-------------------------------------------------------------*/
        fep = dir->first;
        dir->first = v_ent;
        v_ent->entry.dir.next = fep;
        if (fep)
          fep->entry.dir.prev = v_ent;

        /*-------------------------------------------------------------*/
        /* Update file tables size.                                    */
        /*-------------------------------------------------------------*/
        Flash->tbls_size += used;

#if QUOTA_ENABLED
        /*-------------------------------------------------------------*/
        /* If quotas enabled, set new quota values.                    */
        /*-------------------------------------------------------------*/
        if (Flash->quota_enabled)
        {
          FFSEnt *root = &Flash->files_tbl->tbl[0];

          /*-----------------------------------------------------------*/
          /* Assign quota values for new directory.                    */
          /*-----------------------------------------------------------*/
          v_ent->entry.dir.used = 0;
          v_ent->entry.dir.res_below = 0;
          v_ent->entry.dir.free_below = 0;
          v_ent->entry.dir.max_q = max_q;
          v_ent->entry.dir.min_q = min_q;

          /*-----------------------------------------------------------*/
          /* Update used from parent directory to root.                */
          /*-----------------------------------------------------------*/
          for (fep = dir_ent; fep; fep = fep->entry.dir.parent_dir)
            fep->entry.dir.used += used;

          /*-----------------------------------------------------------*/
          /* Update free below.                                        */
          /*-----------------------------------------------------------*/
          FlashFreeBelow(root);

          /*-----------------------------------------------------------*/
          /* Recompute free at each node.                              */
          /*-----------------------------------------------------------*/
          FlashFree(root, root->entry.dir.max_q - FlashVirtUsed(root));

          /*-----------------------------------------------------------*/
          /* If a reserved value was specified, update res_below.      */
          /*-----------------------------------------------------------*/
          if (min_q)
            for (fep = dir_ent; fep; fep = fep->entry.dir.parent_dir)
            {
              fep->entry.dir.res_below += min_q;

              /*-------------------------------------------------------*/
              /* Stop at directory that already has a min_q because    */
              /* the current min_q comes from there.                   */
              /*-------------------------------------------------------*/
              if (fep->entry.dir.min_q)
                break;
            }
        }
#endif /* QUOTA_ENABLED */

        /*-------------------------------------------------------------*/
        /* Perform a sync before returning.                            */
        /*-------------------------------------------------------------*/
        return FlashSync(Flash->do_sync);
      }

#if QUOTA_ENABLED
      /*---------------------------------------------------------------*/
      /* Handle the get_quota() for the flash file system.             */
      /*---------------------------------------------------------------*/
      case GET_QUOTA:
      {
        ui32 *free_space, *quota;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        dir_ent = va_arg(ap, FFSEnt *);
        quota = va_arg(ap, ui32 *);
        free_space = va_arg(ap, ui32 *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* If the quota is 0, walk up parent list to get non-zero one. */
        /*-------------------------------------------------------------*/
        for (tmp_ent = dir_ent; tmp_ent;
             tmp_ent = tmp_ent->entry.dir.parent_dir)
        {
          PfAssert(tmp_ent->type == FDIREN);
          if (tmp_ent->entry.dir.max_q)
          {
            *quota = tmp_ent->entry.dir.max_q;
            break;
          }
        }

        /*-------------------------------------------------------------*/
        /* Report the free space for directory.                        */
        /*-------------------------------------------------------------*/
        *free_space = dir_ent->entry.dir.free;

        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle get_quota_min_max() for the flash file system.         */
      /*---------------------------------------------------------------*/
      case GET_QUOTAM:
      {
        FS_QUOTA_INFO *info;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        dir_ent = va_arg(ap, FFSEnt *);
        info = va_arg(ap, FS_QUOTA_INFO *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Fill out the info fields with quota values.                 */
        /*-------------------------------------------------------------*/
        info->used = dir_ent->entry.dir.used;
        info->free = dir_ent->entry.dir.free;
        info->reserved = dir_ent->entry.dir.min_q;
        info->reserved_below = dir_ent->entry.dir.res_below;
        info->free_below = dir_ent->entry.dir.free_below;

        /*-------------------------------------------------------------*/
        /* If the quota is 0, walk up parent list to get non-zero one. */
        /*-------------------------------------------------------------*/
        for (tmp_ent = dir_ent; tmp_ent;
             tmp_ent = tmp_ent->entry.dir.parent_dir)
        {
          PfAssert(tmp_ent->type == FDIREN);
          if (tmp_ent->entry.dir.max_q)
          {
            info->quota = tmp_ent->entry.dir.max_q;
            break;
          }
        }

        return NULL;
      }
#endif /* QUOTA_ENABLED */

      /*---------------------------------------------------------------*/
      /* Handle rmdir() for the flash file system.                     */
      /*---------------------------------------------------------------*/
      case RMDIR:
        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the argument.               */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        dir_ent = va_arg(ap, FFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Call helper function to do the actual remove.               */
        /*-------------------------------------------------------------*/
        return remove_dir(dir_ent);

      /*---------------------------------------------------------------*/
      /* Handle sortdir() for the flash file system.                   */
      /*---------------------------------------------------------------*/
      case SORTDIR:
      {
        int (*cmp)(const DirEntry *, const DirEntry *);
        DirEntry e1, e2;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        dir_ent = va_arg(ap, FFSEnt *);
        cmp = (int (*)())va_arg(ap, void *); /*lint !e611*/
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* If the directory is open, return error.                     */
        /*-------------------------------------------------------------*/
        if (dir_ent->entry.dir.comm->open_links)
        {
          set_errno(EBUSY);
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* If directory has at most one entry, return.                 */
        /*-------------------------------------------------------------*/
        if (dir_ent->entry.dir.first == NULL ||
            dir_ent->entry.dir.first->entry.dir.next == NULL)
          return NULL;

        /*-------------------------------------------------------------*/
        /* Start qsorting algorithm on directory entries.              */
        /*-------------------------------------------------------------*/
        QuickSort(&dir_ent->entry.dir.first, &tmp_ent, &e1, &e2, cmp);

        /*-------------------------------------------------------------*/
        /* Do a sync before returning.                                 */
        /*-------------------------------------------------------------*/
        return FlashSync(Flash->do_sync);
      }

      /*---------------------------------------------------------------*/
      /* Handle fstat() for the flash file system.                     */
      /*---------------------------------------------------------------*/
      case FSTAT:
      {
        struct stat *buf;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        buf = va_arg(ap, struct stat *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Set the buf struct with the right values for this entry.    */
        /*-------------------------------------------------------------*/
        statistics(buf, (FFSEnt *)handle);
        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle stat() for the flash file system.                      */
      /*---------------------------------------------------------------*/
      case STAT:
      {
        struct stat *buf;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        buf = va_arg(ap, struct stat *);
        tmp_ent = va_arg(ap, FFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Set the buf struct with the right values for this entry.    */
        /*-------------------------------------------------------------*/
        statistics(buf, tmp_ent);
        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle ftruncate() for the flash file system.                 */
      /*---------------------------------------------------------------*/
      case FTRUNCATE:
      {
        off_t length;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        length = va_arg(ap, off_t);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Truncate file to desired length.                            */
        /*-------------------------------------------------------------*/
        vp = truncatef((FFSEnt *)handle, length);

        /*-------------------------------------------------------------*/
        /* If truncate successful, do a sync before returning.         */
        /*-------------------------------------------------------------*/
        if (vp == NULL)
          return FlashSync(Flash->do_sync);
        break;
      }

      /*---------------------------------------------------------------*/
      /* Handle truncate() for the flash file system.                  */
      /*---------------------------------------------------------------*/
      case TRUNCATE:
      {
        off_t length;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        length = va_arg(ap, off_t);
        dir_ent = va_arg(ap, FFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Get pointer to parent directory for the file.               */
        /*-------------------------------------------------------------*/
        dir = &dir_ent->entry.dir;

        /*-------------------------------------------------------------*/
        /* Look for the file in the directory.                         */
        /*-------------------------------------------------------------*/
        if (find_file(dir, name, F_EXECUTE, &tmp_ent, F_ONLY))
          return (void *)-1;
        if (tmp_ent == NULL)
        {
          set_errno(ENOENT);
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* Truncate the file to desired length.                        */
        /*-------------------------------------------------------------*/
        vp = truncatef(tmp_ent, length);

        /*-------------------------------------------------------------*/
        /* If truncate successful, do a sync before returning.         */
        /*-------------------------------------------------------------*/
        if (vp == NULL)
          return FlashSync(Flash->do_sync);
        break;
      }

      /*---------------------------------------------------------------*/
      /* Handle fcntl(,FSET_FL,..) for the flash file system.          */
      /*---------------------------------------------------------------*/
      case SET_FL:
      {
        int oflag;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        oflag = va_arg(ap, int);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Set the open flag for the descriptor.                       */
        /*-------------------------------------------------------------*/
        link_ptr = &((FFSEnt *)stream->handle)->entry.file;
        oflag &= (O_APPEND | O_NONBLOCK | O_ASYNC);
        if ((oflag & O_APPEND) && (link_ptr->comm->open_mode &
                                   (F_WRITE | F_APPEND)))
          link_ptr->comm->open_mode |= F_APPEND;
        if (oflag & O_NONBLOCK)
          link_ptr->comm->open_mode |= F_NONBLOCK;
        if (oflag & O_ASYNC)
          link_ptr->comm->open_mode |= F_ASYNC;

        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle fcntl(,FGET_FL,..) for the flash file system.          */
      /*---------------------------------------------------------------*/
      case GET_FL:
      {
        int oflag = 0;

        /*-------------------------------------------------------------*/
        /* Get the open flag for the descriptor.                       */
        /*-------------------------------------------------------------*/
        link_ptr = &((FFSEnt *)stream->handle)->entry.file;
        if (link_ptr->comm->open_mode & F_ASYNC)
          oflag |= O_ASYNC;
        if (link_ptr->comm->open_mode & F_NONBLOCK)
          oflag |= O_NONBLOCK;
        if (link_ptr->comm->open_mode & F_READ)
        {
          if (link_ptr->comm->open_mode & F_APPEND)
            oflag |= (O_RDWR | O_APPEND);
          else if (link_ptr->comm->open_mode & F_WRITE)
            oflag |= O_RDWR;
          else
            oflag |= O_RDONLY;
        }
        else
        {
          if (link_ptr->comm->open_mode & F_APPEND)
            oflag |= (O_WRONLY | O_APPEND);
          else if (link_ptr->comm->open_mode & F_WRITE)
            oflag |= O_WRONLY;
        }

        return (void *)oflag;
      }

      /*---------------------------------------------------------------*/
      /* Handle opendir() for the flash file system.                   */
      /*---------------------------------------------------------------*/
      case OPENDIR:
        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        dir_ent = va_arg(ap, FFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Open directory, returning NULL if error.                    */
        /*-------------------------------------------------------------*/
        if (open_dir(stream, dir_ent) == NULL)
          return NULL;

        /*-------------------------------------------------------------*/
        /* Increment opendir() count and return control block pointer. */
        /*-------------------------------------------------------------*/
        ++Flash->opendirs;
        return stream;

      /*---------------------------------------------------------------*/
      /* Handle readdir() for the flash file system.                   */
      /*---------------------------------------------------------------*/
      case READDIR:
      {
        DIR *dirp = ((FFSEnt *)handle)->entry.dir.dir;

        /*-------------------------------------------------------------*/
        /* If the position indicator for the field is not set, set it  */
        /* to first entry in directory.                                */
        /*-------------------------------------------------------------*/
        if (dirp->pos == NULL)
        {
          /*-----------------------------------------------------------*/
          /* If there is at least one entry in dir, set position to    */
          /* first entry.                                              */
          /*-----------------------------------------------------------*/
          if (((FFSEnt *)dirp->handle)->entry.dir.first)
          {
            dirp->pos = ((FFSEnt *)dirp->handle)->entry.dir.first;
            strncpy(dirp->dent.d_name,
                    ((FFSEnt *)dirp->pos)->entry.dir.name,
                    FILENAME_MAX);
            return &dirp->dent;
          }
        }

        /*-------------------------------------------------------------*/
        /* Else if the position indicator does not point to the last   */
        /* entry, move to the next entry in the directory.             */
        /*-------------------------------------------------------------*/
        else if (((FFSEnt *)dirp->pos)->entry.dir.next)
        {
          dirp->pos = ((FFSEnt *)dirp->pos)->entry.dir.next;
          strncpy(dirp->dent.d_name,
                  ((FFSEnt *)dirp->pos)->entry.dir.name,
                  FILENAME_MAX);
          return &dirp->dent;
        }
        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle closedir() for the flash file system.                  */
      /*---------------------------------------------------------------*/
      case CLOSEDIR:
        /*-------------------------------------------------------------*/
        /* Close the directory.                                        */
        /*-------------------------------------------------------------*/
        close_dir(handle, stream);

        /*-------------------------------------------------------------*/
        /* Decrement opendir() count and garbage collect if needed.    */
        /*-------------------------------------------------------------*/
        if (--Flash->opendirs == 0)
        {
          if (Flash->total_free >= (FNUM_ENT * 5 / 3))
            FlashGarbageCollect();
        }

        return NULL;

      /*---------------------------------------------------------------*/
      /* Handle link() for the flash file system.                      */
      /*---------------------------------------------------------------*/
      case LINK:
      {
        char *old_name, *new_name;
        FFSEnt *old_dir, *new_dir;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        old_name = va_arg(ap, char *);
        new_name = va_arg(ap, char *);
        old_dir = va_arg(ap, FFSEnt *);
        new_dir = va_arg(ap, FFSEnt *);
        va_end(ap);

#if QUOTA_ENABLED
        /*-------------------------------------------------------------*/
        /* If volume has quota enabled, supress links.                 */
        /*-------------------------------------------------------------*/
        if (Flash->quota_enabled)
          return (void *)-1;
#endif /* QUOTA_ENABLED */

        /*-------------------------------------------------------------*/
        /* If new name is invalid, return error.                       */
        /*-------------------------------------------------------------*/
        if (InvalidName(new_name, FALSE))
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* Create link to the file.                                    */
        /*-------------------------------------------------------------*/
        return create_link(old_name, new_name, old_dir, new_dir);
      }

      /*---------------------------------------------------------------*/
      /* Handle rename() for the flash file system.                    */
      /*---------------------------------------------------------------*/
      case RENAME:
      {
        char *old_name, *new_name;
        FFSEnt *old_dir, *new_dir, *old_ent;
        FFSEnt *root = &Flash->files_tbl->tbl[0];
        size_t end_name;
        int is_dir;
#if QUOTA_ENABLED
        ui32 used;
        FFSEnt *ent;
#endif /* QUOTA_ENABLED */

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        old_name = va_arg(ap, char *);
        new_name = va_arg(ap, char *);
        old_dir = va_arg(ap, FFSEnt *);
        new_dir = va_arg(ap, FFSEnt *);
        is_dir = va_arg(ap, int);
        va_end(ap);

        vp = NULL;

        /*-------------------------------------------------------------*/
        /* If new name is invalid, return error.                       */
        /*-------------------------------------------------------------*/
        if (InvalidName(new_name, is_dir))
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* While there are terminating '/' characters, remove them.    */
        /*-------------------------------------------------------------*/
        end_name = strlen(old_name) - 1;
        while (old_name[end_name] == '/')
          old_name[end_name--] = '\0';
        end_name = strlen(new_name) - 1;
        while (new_name[end_name] == '/')
          new_name[end_name--] = '\0';

        /*-------------------------------------------------------------*/
        /* Check that both old and new dirs have execute access.       */
        /*-------------------------------------------------------------*/
        if (CheckPerm(old_dir->entry.dir.comm, F_EXECUTE) ||
            CheckPerm(new_dir->entry.dir.comm, F_EXECUTE))
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* If no old name, use old_dir, else look for entry in old_dir.*/
        /*-------------------------------------------------------------*/
        if (old_name[0] == '\0')
        {
          /*-----------------------------------------------------------*/
          /* Cannot move the root directory.                           */
          /*-----------------------------------------------------------*/
          if (old_dir == root)
          {
            set_errno(EINVAL);
            return (void *)-1;
          }
          old_ent = old_dir;
          old_dir = old_ent->entry.dir.parent_dir;
        }
        else
          for (old_ent = old_dir->entry.dir.first;;
               old_ent = old_ent->entry.dir.next)
          {
            /*---------------------------------------------------------*/
            /* Return error if old file could not be found.            */
            /*---------------------------------------------------------*/
            if (old_ent == NULL)
            {
              set_errno(ENOENT);
              return (void *)-1;
            }

            /*---------------------------------------------------------*/
            /* Break if old name is found.                             */
            /*---------------------------------------------------------*/
            if (!strcmp(old_ent->entry.dir.name, old_name))
            {
              PfAssert(old_ent->entry.dir.parent_dir == old_dir);
              break;
            }
          }

        /*-------------------------------------------------------------*/
        /* Make sure owner of file is trying to link to file.          */
        /*-------------------------------------------------------------*/
        FsGetId(&uid, &gid);
        if (uid != old_ent->entry.file.comm->user_id)
        {
          set_errno(EPERM);
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* Look for name duplication with the new link.                */
        /*-------------------------------------------------------------*/
        for (tmp_ent = new_dir->entry.dir.first; tmp_ent;
             tmp_ent = tmp_ent->entry.dir.next)
        {
          /*-----------------------------------------------------------*/
          /* If a file exists with the name, error.                    */
          /*-----------------------------------------------------------*/
          if (!strcmp(tmp_ent->entry.dir.name, new_name))
          {
            set_errno(EEXIST);
            return (void *)-1;
          }
        }

        /*-------------------------------------------------------------*/
        /* Check that the new directory has write permissions.         */
        /*-------------------------------------------------------------*/
        if (CheckPerm(new_dir->entry.dir.comm, F_WRITE))
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* If we're linking dirs, make sure the new one is not in a    */
        /* subdirectory of the old one.                                */
        /*-------------------------------------------------------------*/
        if (old_ent->type == FDIREN)
          for (tmp_ent = new_dir; tmp_ent != &Flash->files_tbl->tbl[0];
               tmp_ent = tmp_ent->entry.dir.parent_dir)
          {
            /*---------------------------------------------------------*/
            /* If new directory is subdirectory of old, return error.  */
            /*---------------------------------------------------------*/
            if (tmp_ent == old_ent)
            {
              set_errno(EINVAL);
              return (void *)-1;
            }
          }

        /*-------------------------------------------------------------*/
        /* If old_dir and new_dir are different, remove entry from old */
        /* dir and place it in new dir.                                */
        /*-------------------------------------------------------------*/
        if (old_dir != new_dir)
        {
          /*-----------------------------------------------------------*/
          /* Remove from old_dir.                                      */
          /*-----------------------------------------------------------*/
          if (old_ent->entry.dir.prev)
            old_ent->entry.dir.prev->entry.dir.next =
                                                 old_ent->entry.dir.next;
          else
            old_dir->entry.dir.first = old_ent->entry.dir.next;
          if (old_ent->entry.dir.next)
            old_ent->entry.dir.next->entry.dir.prev =
                                                 old_ent->entry.dir.prev;

#if QUOTA_ENABLED
          /*-----------------------------------------------------------*/
          /* If volume has quotas, check if the move is possible.      */
          /*-----------------------------------------------------------*/
          if (Flash->quota_enabled)
          {
            /*---------------------------------------------------------*/
            /* Compute the used space for old entry.                   */
            /*---------------------------------------------------------*/
            if (old_ent->type == FDIREN)
              used = strlen(old_ent->entry.dir.name) + 1 + OFDIR_SZ +
                     OFDIR_QUOTA_SZ + OFCOM_SZ + old_ent->entry.dir.used;
            else
            {
              PfAssert(old_ent->type == FFILEN);
              used = strlen(old_ent->entry.dir.name) + 1 + OFFIL_SZ +
                     OFCOM_SZ + ((old_ent->entry.file.comm->size +
                     Flash->sect_sz - 1) / Flash->sect_sz) *
                     Flash->sect_sz;
            }

            /*---------------------------------------------------------*/
            /* Update used from parent to root.                        */
            /*---------------------------------------------------------*/
            for (ent = old_dir; ent; ent = ent->entry.dir.parent_dir)
            {
              PfAssert(ent->type == FDIREN &&
                       ent->entry.dir.used >= used);
              ent->entry.dir.used -= used;
            }

            /*---------------------------------------------------------*/
            /* Update reserved below if old entry is directory.        */
            /*---------------------------------------------------------*/
            if (old_ent->type == FDIREN)
              for (ent = old_dir; ent; ent = ent->entry.dir.parent_dir)
              {
                PfAssert(ent->entry.dir.res_below >=
                         old_ent->entry.dir.min_q);
                ent->entry.dir.res_below -= old_ent->entry.dir.min_q;

                /*-----------------------------------------------------*/
                /* Stop when we reach a directory with a reservation   */
                /* because the directories above this one don't have   */
                /* their reserved below affected.                      */
                /*-----------------------------------------------------*/
                if (ent->entry.dir.min_q != 0)
                  break;
              }

            /*---------------------------------------------------------*/
            /* Update free below.                                      */
            /*---------------------------------------------------------*/
            FlashFreeBelow(root);

            /*---------------------------------------------------------*/
            /* Recompute free at each node.                            */
            /*---------------------------------------------------------*/
            FlashFree(root, root->entry.dir.max_q - FlashVirtUsed(root));

            /*---------------------------------------------------------*/
            /* Adjust used to account for difference in name.          */
            /*---------------------------------------------------------*/
            used += (strlen(new_name) - strlen(old_ent->entry.dir.name));

            /*---------------------------------------------------------*/
            /* If file, check if enough space for file move.           */
            /*---------------------------------------------------------*/
            if (old_ent->type == FFILEN)
            {
              if (new_dir->entry.dir.free < used)
              {
                set_errno(ENOMEM);
                used -= strlen(new_name);
                new_dir = old_dir;
                new_name = old_ent->entry.file.name;
                used += strlen(new_name);
                vp = (void *)-1;
              }
            }

            /*---------------------------------------------------------*/
            /* Else directory; check quota values consistency.         */
            /*---------------------------------------------------------*/
            else
            {
              PfAssert(old_ent->type == FDIREN);

              /*-------------------------------------------------------*/
              /* For parent with reserved, check for consistency.      */
              /*-------------------------------------------------------*/
              if (old_ent->entry.dir.min_q && new_dir->entry.dir.min_q)
              {
                if (new_dir->entry.dir.min_q < old_ent->entry.dir.min_q +
                    new_dir->entry.dir.res_below)
                {
                  set_errno(EINVAL);
                  used -= strlen(new_name);
                  new_dir = old_dir;
                  new_name = old_ent->entry.dir.name;
                  used += strlen(new_name);
                  vp = (void *)-1;
                }
              }

              /*-------------------------------------------------------*/
              /* Check quota consistency.                              */
              /*-------------------------------------------------------*/
              if (old_ent->entry.dir.max_q > new_dir->entry.dir.max_q)
              {
                set_errno(EINVAL);
                used -= strlen(new_name);
                new_dir = old_dir;
                new_name = old_ent->entry.dir.name;
                used += strlen(new_name);
                vp = (void *)-1;
              }

              /*-------------------------------------------------------*/
              /* Check free space for parent directory.                */
              /*-------------------------------------------------------*/
              if (new_dir->entry.dir.free < used +
                  old_ent->entry.dir.min_q)
              {
                set_errno(ENOMEM);
                used -= strlen(new_name);
                new_dir = old_dir;
                new_name = old_ent->entry.dir.name;
                used += strlen(new_name);
                vp = (void *)-1;
              }
            }
          }
#endif /* QUOTA_ENABLED */

          /*-----------------------------------------------------------*/
          /* Add to new_dir.                                           */
          /*-----------------------------------------------------------*/
          tmp_ent = new_dir->entry.dir.first;
          new_dir->entry.dir.first = old_ent;
          old_ent->entry.dir.next = tmp_ent;
          old_ent->entry.dir.prev = NULL;
          if (tmp_ent)
            tmp_ent->entry.dir.prev = old_ent;

          /*-----------------------------------------------------------*/
          /* Set parent_dir pointer to new_dir.                        */
          /*-----------------------------------------------------------*/
          old_ent->entry.dir.parent_dir = new_dir;

#if QUOTA_ENABLED
          /*-----------------------------------------------------------*/
          /* If volume has quotas enabled, recompute quota values.     */
          /*-----------------------------------------------------------*/
          if (Flash->quota_enabled)
          {
            /*---------------------------------------------------------*/
            /* Update used from parent to root.                        */
            /*---------------------------------------------------------*/
            for (ent = new_dir; ent; ent = ent->entry.dir.parent_dir)
            {
              PfAssert(ent->type == FDIREN);
              ent->entry.dir.used += used; /*lint !e644*/
            }

            /*---------------------------------------------------------*/
            /* Update free below.                                      */
            /*---------------------------------------------------------*/
            FlashFreeBelow(root);

            /*---------------------------------------------------------*/
            /* Recompute free at each node.                            */
            /*---------------------------------------------------------*/
            FlashFree(root, root->entry.dir.max_q - FlashVirtUsed(root));

            /*---------------------------------------------------------*/
            /* Update reserved below if old entry is directory.        */
            /*---------------------------------------------------------*/
            if (old_ent->type == FDIREN && old_ent->entry.dir.min_q != 0)
              for (ent = new_dir; ent; ent = ent->entry.dir.parent_dir)
              {
                ent->entry.dir.res_below += old_ent->entry.dir.min_q;

                /*-----------------------------------------------------*/
                /* Stop when we reach a directory with a reservation   */
                /* because the directories above this one don't have   */
                /* their reserved below affected.                      */
                /*-----------------------------------------------------*/
                if (ent->entry.dir.min_q != 0)
                  break;
              }
          }
#endif /* QUOTA_ENABLED */
        }

#if QUOTA_ENABLED
        /*-------------------------------------------------------------*/
        /* Else if quota enabled for volume, check renaming within same*/
        /* directory is allowed.                                       */
        /*-------------------------------------------------------------*/
        else if (Flash->quota_enabled)
        {
          ui32 old_len = strlen(old_name), new_len = strlen(new_name);

          /*-----------------------------------------------------------*/
          /* If old name is shorter, check free space for new_name.    */
          /*-----------------------------------------------------------*/
          if (old_len < new_len)
          {
            if (new_len - old_len > new_dir->entry.dir.free)
            {
              set_errno(ENOMEM);
              new_name = old_name;
              new_len = old_len;
              vp = (void *)-1;
            }
          }

          /*-----------------------------------------------------------*/
          /* If names have different lengths, adjust quota values.     */
          /*-----------------------------------------------------------*/
          if (old_len != new_len)
          {
            /*---------------------------------------------------------*/
            /* Adjust used from parent to root.                        */
            /*---------------------------------------------------------*/
            for (ent = new_dir; ent; ent = ent->entry.dir.parent_dir)
            {
              PfAssert(ent->type == FDIREN && ent->entry.dir.used >=
                                              old_len);
              ent->entry.dir.used -= old_len;
              ent->entry.dir.used += new_len;
            }
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
        /* Update file tables size.                                    */
        /*-------------------------------------------------------------*/
        Flash->tbls_size += (strlen(new_name) -
                             strlen(old_ent->entry.file.name));

        /*-------------------------------------------------------------*/
        /* Set new name.                                               */
        /*-------------------------------------------------------------*/
        strcpy(old_ent->entry.file.name, new_name);

        /*-------------------------------------------------------------*/
        /* Perform a sync before returning.                            */
        /*-------------------------------------------------------------*/
        if (FlashSync(Flash->do_sync))
          return (void *)-1;
        else
          return vp;
      }

      /*---------------------------------------------------------------*/
      /* Handle the duplication functions for the flash file system.   */
      /*---------------------------------------------------------------*/
      case DUP:
        /*-------------------------------------------------------------*/
        /* Simply increment the number of open links for this file.    */
        /*-------------------------------------------------------------*/
        ++((FFSEnt *)handle)->entry.file.comm->open_links;
        return NULL;

      /*---------------------------------------------------------------*/
      /* Handle open() for the flash file system.                      */
      /*---------------------------------------------------------------*/
      case OPEN:
      {
        int oflag;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        oflag = va_arg(ap, int);
        mode = (mode_t)va_arg(ap, int);
        dir_ent = va_arg(ap, FFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* If openning the file failed, return error.                  */
        /*-------------------------------------------------------------*/
        if (open_file(name, oflag, mode, dir_ent, stream, &sync_f) ==
            NULL)
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* If needed, perform a sync before returning.                 */
        /*-------------------------------------------------------------*/
        return sync_f ? FlashSync(Flash->do_sync) : NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle access() for the flash file system.                    */
      /*---------------------------------------------------------------*/
      case ACCESS:
      {
        char *path;
        int amode;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        path = va_arg(ap, char *);
        dir_ent = va_arg(ap, FFSEnt *);
        amode = va_arg(ap, int);
        va_end(ap);

        dir = &dir_ent->entry.dir;

        /*-------------------------------------------------------------*/
        /* Look for the file in the directory.                         */
        /*-------------------------------------------------------------*/
        if (find_file(dir, path, F_EXECUTE, &tmp_ent, F_AND_D))
          return (void *)-1;
        if (tmp_ent == NULL)
        {
          set_errno(ENOENT);
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* Check if we need to look for read permission(R_OK).         */
        /*-------------------------------------------------------------*/
        if (amode & R_OK)
        {
          /*-----------------------------------------------------------*/
          /* Check for read permission on the file.                    */
          /*-----------------------------------------------------------*/
          if (CheckPerm(tmp_ent->entry.file.comm, F_READ))
            return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* Check if we need to look for write permission(W_OK).        */
        /*-------------------------------------------------------------*/
        if (amode & W_OK)
        {
          /*-----------------------------------------------------------*/
          /* Check for write permission on the file.                   */
          /*-----------------------------------------------------------*/
          if (CheckPerm(tmp_ent->entry.file.comm, F_WRITE))
            return (void *)-1;

          /*-----------------------------------------------------------*/
          /* Check file is not open in write mode.                     */
          /*-----------------------------------------------------------*/
          if (tmp_ent->entry.file.comm->open_mode & (F_APPEND | F_WRITE))
          {
            set_errno(EACCES);
            return (void *)-1;
          }
        }

        /*-------------------------------------------------------------*/
        /* Check if we need to look for search permission(X_OK).       */
        /*-------------------------------------------------------------*/
        if (amode & X_OK)
        {
          /*-----------------------------------------------------------*/
          /* Check for search permission on the file.                  */
          /*-----------------------------------------------------------*/
          if (CheckPerm(tmp_ent->entry.file.comm, F_EXECUTE))
            return (void *)-1;
        }
        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle chmod() for the flash file system.                     */
      /*---------------------------------------------------------------*/
      case CHMOD:
        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        dir_ent = va_arg(ap, FFSEnt *);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);

        dir = &dir_ent->entry.dir;

        /*-------------------------------------------------------------*/
        /* Look for the file in the directory.                         */
        /*-------------------------------------------------------------*/
        if (find_file(dir, name, F_EXECUTE | F_WRITE, &tmp_ent, F_AND_D))
          return (void *)-1;
        if (tmp_ent == NULL)
        {
          set_errno(ENOENT);
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* Check that the owner of the file is trying to modify mode.  */
        /*-------------------------------------------------------------*/
        FsGetId(&uid, &gid);
        if (uid != tmp_ent->entry.file.comm->user_id)
        {
          set_errno(EPERM);
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* Set new mode for file.                                      */
        /*-------------------------------------------------------------*/
        SetPerm(tmp_ent->entry.file.comm, mode);

        /*-------------------------------------------------------------*/
        /* Perform a sync now to save the mode for the file/directory. */
        /*-------------------------------------------------------------*/
        return FlashSync(Flash->do_sync);

      /*---------------------------------------------------------------*/
      /* Handle chown() for the flash file system.                     */
      /*---------------------------------------------------------------*/
      case CHOWN:
      {
        uid_t old_uid;
        gid_t old_gid;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        dir_ent = va_arg(ap, FFSEnt *);
        uid = (uid_t)va_arg(ap, int);
        gid = (gid_t)va_arg(ap, int);
        va_end(ap);

        dir = &dir_ent->entry.dir;

        /*-------------------------------------------------------------*/
        /* Look for the file in the directory.                         */
        /*-------------------------------------------------------------*/
        if (find_file(dir, name, F_EXECUTE | F_WRITE, &tmp_ent, F_AND_D))
          return (void *)-1;
        if (tmp_ent == NULL)
        {
          set_errno(ENOENT);
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* Check that the owner of the file is trying to modify mode.  */
        /*-------------------------------------------------------------*/
        FsGetId(&old_uid, &old_gid);
        if (old_uid != tmp_ent->entry.file.comm->user_id)
        {
          set_errno(EPERM);
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* Set new user and group ID for file.                         */
        /*-------------------------------------------------------------*/
        tmp_ent->entry.file.comm->user_id = uid;
        tmp_ent->entry.file.comm->group_id = gid;

        /*-------------------------------------------------------------*/
        /* Perform a sync now to save user/group for file/directory.   */
        /*-------------------------------------------------------------*/
        return FlashSync(Flash->do_sync);
      }

      /*---------------------------------------------------------------*/
      /* Handle utime() for the flash file system.                     */
      /*---------------------------------------------------------------*/
      case UTIME:
      {
        struct utimbuf *times;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get arguments.                  */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        times = va_arg(ap, struct utimbuf *);
        dir_ent = va_arg(ap, FFSEnt *);
        va_end(ap);

        dir = &dir_ent->entry.dir;

        /*-------------------------------------------------------------*/
        /* While there are terminating '/' characters, remove them.    */
        /*-------------------------------------------------------------*/
        end = strlen(name) - 1;
        while (name[end] == '/')
          name[end--] = '\0';

        /*-------------------------------------------------------------*/
        /* Check directory has search/write permissions.               */
        /*-------------------------------------------------------------*/
        if (CheckPerm(dir->comm, F_EXECUTE | F_WRITE))
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* If name is empty, use the directory itself.                 */
        /*-------------------------------------------------------------*/
        if (name[0] == 0)
          link_ptr = &dir_ent->entry.file;

        /*-------------------------------------------------------------*/
        /* Else look through it for the entry.                         */
        /*-------------------------------------------------------------*/
        else
        {
          for (tmp_ent = dir->first;; tmp_ent = tmp_ent->entry.dir.next)
          {
            /*---------------------------------------------------------*/
            /* If specified file could not be found, return error.     */
            /*---------------------------------------------------------*/
            if (tmp_ent == NULL)
            {
              set_errno(ENOENT);
              return (void *)-1;
            }

            /*---------------------------------------------------------*/
            /* Assign link pointer and break if file found.            */
            /*---------------------------------------------------------*/
            if (!strcmp(tmp_ent->entry.dir.name, name))
            {
              link_ptr = &tmp_ent->entry.file;
              break;
            }
          }
        }

        /*-------------------------------------------------------------*/
        /* Check that the owner of the file is trying to modify time.  */
        /*-------------------------------------------------------------*/
        FsGetId(&uid, &gid);
        if (link_ptr->comm->user_id != uid)
        {
          set_errno(EPERM);
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* If present, store specified times, else store current time. */
        /*-------------------------------------------------------------*/
        if (times)
        {
          link_ptr->comm->mod_time = (ui32)times->modtime;
          link_ptr->comm->ac_time = (ui32)times->actime;
        }
        else
          link_ptr->comm->mod_time = link_ptr->comm->ac_time =OsSecCount;

        /*-------------------------------------------------------------*/
        /* Perform a sync before returning to save changes.            */
        /*-------------------------------------------------------------*/
        return FlashSync(Flash->do_sync);
      }

      /*---------------------------------------------------------------*/
      /* Handle fopen() for the flash file system.                     */
      /*---------------------------------------------------------------*/
      case FOPEN:
      {
        char *mode_str;
        int oflag;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get arguments.                  */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        mode_str = va_arg(ap, char *);
        dir_ent = va_arg(ap, FFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Convert mode string to open() oflag equivalent.             */
        /*-------------------------------------------------------------*/
        if (!strcmp(mode_str, "r") || !strcmp(mode_str, "rb"))
          oflag = O_RDONLY;
        else if (!strcmp(mode_str, "w") || !strcmp(mode_str, "wb"))
          oflag = O_WRONLY | O_CREAT | O_TRUNC;
        else if (!strcmp(mode_str, "a") || !strcmp(mode_str, "ab"))
          oflag = O_WRONLY | O_CREAT | O_APPEND;
        else if (!strcmp(mode_str, "r+") || !strcmp(mode_str, "rb+") ||
                 !strcmp(mode_str, "r+b"))
          oflag = O_RDWR;
        else if (!strcmp(mode_str, "w+") || !strcmp(mode_str, "wb+") ||
                 !strcmp(mode_str, "w+b"))
          oflag = O_RDWR | O_CREAT | O_TRUNC;
        else if (!strcmp(mode_str, "a+") || !strcmp(mode_str, "ab+") ||
                 !strcmp(mode_str, "a+b"))
          oflag = O_RDWR | O_CREAT | O_APPEND;
        else
        {
          set_errno(EINVAL);
          return NULL;
        }

        /*-------------------------------------------------------------*/
        /* Open file.                                                  */
        /*-------------------------------------------------------------*/
        vp = open_file(name, oflag, S_IRUSR | S_IWUSR | S_IXUSR, dir_ent,
                       stream, &sync_f);

        /*-------------------------------------------------------------*/
        /* If needed, perform a sync.                                  */
        /*-------------------------------------------------------------*/
        if (sync_f && FlashSync(Flash->do_sync))
          return NULL;

        break;
      }

      /*---------------------------------------------------------------*/
      /* Handle creatn() for the flash file sytem.                     */
      /*---------------------------------------------------------------*/
      case CREATN:
      {
        size_t size, index, num_sects;
        ui32 prev_sect = (ui32)-1;
        ui16 curr_sect;
        FILE *created_file;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get arguments.                  */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        mode = (mode_t)va_arg(ap, int);
        size = va_arg(ap, size_t);
        dir_ent = va_arg(ap, FFSEnt *);

        /*-------------------------------------------------------------*/
        /* Attempt to create the file.                                 */
        /*-------------------------------------------------------------*/
        created_file = open_file(name, O_RDWR | O_CREAT | O_TRUNC, mode,
                                 dir_ent, stream, &sync_f);
        if (created_file == NULL)
          return (void *)-1;
        link_ptr = &((FFSEnt *)stream->handle)->entry.file;

        /*-------------------------------------------------------------*/
        /* Adjust the file mode to indicate creatn() creation.         */
        /*-------------------------------------------------------------*/
        link_ptr->comm->mode |= S_CREATN;

        /*-------------------------------------------------------------*/
        /* Figure out how many sectors need to be added to file.       */
        /*-------------------------------------------------------------*/
        num_sects = (size + Flash->sect_sz - 1) / Flash->sect_sz;

#if QUOTA_ENABLED
        /*-------------------------------------------------------------*/
        /* If quotas enabled, check if new sectors can be added.       */
        /*-------------------------------------------------------------*/
        if (Flash->quota_enabled)
        {
          if (link_ptr->parent_dir->entry.dir.free < num_sects *
              Flash->sect_sz)
          {
            set_errno(ENOMEM);
            index = 0;
            goto creatn_error;
          }
        }
#endif /* QUOTA_ENABLED */

        /*-------------------------------------------------------------*/
        /* Add sectors to the file.                                    */
        /*-------------------------------------------------------------*/
        for (index = 0; index < num_sects; ++index)
        {
          /*-----------------------------------------------------------*/
          /* Set free sector to point where we can write to it.        */
          /*-----------------------------------------------------------*/
          if (FlashFrSect(link_ptr->comm))
            goto creatn_error;

          /*-----------------------------------------------------------*/
          /* Advance free sector.                                      */
          /*-----------------------------------------------------------*/
          curr_sect = (ui16)Flash->free_sect;
          --Flash->free_sects;
          Flash->free_sect = Flash->sect_tbl[Flash->free_sect].next;

          /*-----------------------------------------------------------*/
          /* If first sector, set link first sector.                   */
          /*-----------------------------------------------------------*/
          if (prev_sect == (ui32)-1)
          {
            link_ptr->comm->frst_sect = curr_sect;
            Flash->sect_tbl[curr_sect].prev = FLAST_SECT;
            FlashUpdateFCBs(created_file, link_ptr->comm);
          }

          /*-----------------------------------------------------------*/
          /* Else tie previous sector to this one.                     */
          /*-----------------------------------------------------------*/
          else
          {
            Flash->sect_tbl[prev_sect].next = curr_sect;
            Flash->sect_tbl[curr_sect].prev = (ui16)prev_sect;
          }

          /*-----------------------------------------------------------*/
          /* Mark last sector.                                         */
          /*-----------------------------------------------------------*/
          link_ptr->comm->last_sect = curr_sect;
          Flash->sect_tbl[curr_sect].next = FLAST_SECT;

          /*-----------------------------------------------------------*/
          /* Adjust the used counts.                                   */
          /*-----------------------------------------------------------*/
          ++Flash->used_sects;
          ++Flash->blocks[curr_sect / Flash->block_sects].used_sects;

          /*-----------------------------------------------------------*/
          /* Adjust file size and remember previous sector.            */
          /*-----------------------------------------------------------*/
          link_ptr->comm->size += Flash->sect_sz;
          prev_sect = link_ptr->comm->last_sect;
        }

        /*-------------------------------------------------------------*/
        /* Set one past last.                                          */
        /*-------------------------------------------------------------*/
        link_ptr->comm->one_past_last.sector = link_ptr->comm->last_sect;
        link_ptr->comm->one_past_last.offset = (ui16)Flash->sect_sz;

        /*-------------------------------------------------------------*/
        /* Set the stream curr_ptr and old_ptr.                        */
        /*-------------------------------------------------------------*/
        created_file->curr_ptr.sector = link_ptr->comm->frst_sect;
        created_file->curr_ptr.offset = 0;
        created_file->curr_ptr.sect_off = 0;
        created_file->old_ptr = created_file->curr_ptr;

#if QUOTA_ENABLED
        /*-------------------------------------------------------------*/
        /* If quotas enabled, adjust quota values due to new sectors.  */
        /*-------------------------------------------------------------*/
        if (Flash->quota_enabled)
        {
          FFSEnt *ent, *root = &Flash->files_tbl->tbl[0];

          /*-----------------------------------------------------------*/
          /* Update used from parent to root.                          */
          /*-----------------------------------------------------------*/
          for (ent = link_ptr->parent_dir; ent;
               ent = ent->entry.dir.parent_dir)
          {
            PfAssert(ent->type == FDIREN);
            ent->entry.dir.used += num_sects * Flash->sect_sz;
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
        /* If unable to add all sectors, delete file and return -1.    */
        /*-------------------------------------------------------------*/
creatn_error:
        if (index != num_sects)
        {
          link_ptr->file = NULL;
          close_file(dir_ent, created_file);
          remove_file(dir_ent, name);
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* Sync everything to save state.                              */
        /*-------------------------------------------------------------*/
        if (FlashSync(Flash->do_sync))
          return (void *)-1;
        break;
      }

      /*---------------------------------------------------------------*/
      /* Handle fclose() for the flash file system.                    */
      /*---------------------------------------------------------------*/
      case FCLOSE:
        /*-------------------------------------------------------------*/
        /* If type is directory, close the directory.                  */
        /*-------------------------------------------------------------*/
        if (((FFSEnt *)handle)->type == FDIREN)
          close_dir(handle, stream);

        /*-------------------------------------------------------------*/
        /* Else file is regular file.                                  */
        /*-------------------------------------------------------------*/
        else
        {
          /*-----------------------------------------------------------*/
          /* Close file, deleting it if previously unlinked.           */
          /*-----------------------------------------------------------*/
          if (close_file(handle, stream))
            return (void *)-1;

          /*-----------------------------------------------------------*/
          /* Check if file wasn't deleted.                             */
          /*-----------------------------------------------------------*/
          link_ptr = &((FFSEnt *)handle)->entry.file;
          if (link_ptr->comm)
          {
            /*---------------------------------------------------------*/
            /* If file modified and syncs enabled, sync and return.    */
            /*---------------------------------------------------------*/
            if ((stream->flags & FCB_MOD) && Flash->do_sync)
              return FlashSync(Flash->do_sync);

            /*---------------------------------------------------------*/
            /* Else if creatn() file, flush sectors and return.        */
            /*---------------------------------------------------------*/
            if (link_ptr->comm->mode & S_CREATN)
              return FlushFileSectors(&Flash->cache, link_ptr->comm);
          }
        }
        return NULL;

      /*---------------------------------------------------------------*/
      /* Handle feof() for the flash file system.                      */
      /*---------------------------------------------------------------*/
      case FEOF:
        link_ptr = &((FFSEnt *)handle)->entry.file;

        if ((((FFSEnt *)handle)->type != FFILEN) ||
            stream->curr_ptr.sector == FDRTY_SECT ||
            (stream->curr_ptr.sector ==
             link_ptr->comm->one_past_last.sector &&
             stream->curr_ptr.offset ==
             link_ptr->comm->one_past_last.offset))
          return (void *)1;
        return NULL;

      /*---------------------------------------------------------------*/
      /* Handle fgetpos() for the flash file system.                   */
      /*---------------------------------------------------------------*/
      case FGETPOS:
      {
        fpos_t *fp;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get arguments.                  */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        fp = va_arg(ap, fpos_t *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* If invalid handle, error, else return current position.     */
        /*-------------------------------------------------------------*/
        if (!handle || (((FFSEnt *)handle)->type != FFILEN))
          return (void *)1;
        else
        {
          fp->sector = stream->curr_ptr.sector;
          fp->offset = stream->curr_ptr.offset;
          fp->sect_off = stream->curr_ptr.sect_off;
          return NULL;
        }
      }

      /*---------------------------------------------------------------*/
      /* Handle fsetpos() for the flash file system.                   */
      /*---------------------------------------------------------------*/
      case FSETPOS:
      {
        fpos_t *pos;
        int set_free;

        link_ptr = &((FFSEnt *)handle)->entry.file;
        vp = (void *)-1;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        pos = va_arg(ap, fpos_t *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* If the file is not empty, change its curr_pos.              */
        /*-------------------------------------------------------------*/
        if (link_ptr->comm->frst_sect != FFREE_SECT)
        {
          /*-----------------------------------------------------------*/
          /* Mark flag to free current sector only if new position is  */
          /* in a different sector.                                    */
          /*-----------------------------------------------------------*/
          set_free = (pos->sector != stream->curr_ptr.sector);

          /*-----------------------------------------------------------*/
          /* If position passed EOF, set current pointer past EOF.     */
          /*-----------------------------------------------------------*/
          if (pos->sector == FDRTY_SECT)
          {
            /*---------------------------------------------------------*/
            /* If position still past EOF, set it.                     */
            /*---------------------------------------------------------*/
            if (link_ptr->comm->size < pos->sect_off * Flash->sect_sz +
                pos->offset)
            {
              stream->curr_ptr.sector = pos->sector;
              stream->curr_ptr.offset = pos->offset;
              stream->curr_ptr.sect_off = pos->sect_off;
            }

            /*---------------------------------------------------------*/
            /* Else, file size is bigger now and position lies within  */
            /* file.                                                   */
            /*---------------------------------------------------------*/
            else
            {
              ui32 sector = link_ptr->comm->frst_sect, count;

              for (count = 0; count < pos->sect_off; ++count)
                sector = Flash->sect_tbl[sector].next;
              stream->curr_ptr.sector = (ui16)sector;
              stream->curr_ptr.offset = pos->offset;
              stream->curr_ptr.sect_off = pos->sect_off;
            }
            vp = NULL;
          }

          /*-----------------------------------------------------------*/
          /* Check position is valid and set current pointer to it.    */
          /*-----------------------------------------------------------*/
          else
          {
            /*---------------------------------------------------------*/
            /* Look for a sector from this file to match pos->sector.  */
            /*---------------------------------------------------------*/
            i = link_ptr->comm->frst_sect;
            for (; i != FLAST_SECT; i = Flash->sect_tbl[i].next)
              if (i == (ui32)pos->sector)
                break;

            /*---------------------------------------------------------*/
            /* If such a sector exists, check if offset is valid.      */
            /*---------------------------------------------------------*/
            if ((i != FLAST_SECT) && (pos->offset < Flash->sect_sz))
            {
              /*-------------------------------------------------------*/
              /* If it's last sector, make sure offset is not bigger   */
              /* than the end of file location.                        */
              /*-------------------------------------------------------*/
              if (i == link_ptr->comm->last_sect)
              {
                /*-----------------------------------------------------*/
                /* If we're not going past EOF set position, else error*/
                /*-----------------------------------------------------*/
                if (link_ptr->comm->one_past_last.offset >= pos->offset)
                {
                  stream->curr_ptr.sector = pos->sector;
                  stream->curr_ptr.offset = pos->offset;
                  stream->curr_ptr.sect_off = pos->sect_off;
                  vp = NULL;
                }
                else
                {
                  set_errno(EINVAL);
                  return (void *)-1;
                }
              }

              /*-------------------------------------------------------*/
              /* Else position is valid, set current pointer.          */
              /*-------------------------------------------------------*/
              else
              {
                stream->curr_ptr.sector = pos->sector;
                stream->curr_ptr.offset = pos->offset;
                stream->curr_ptr.sect_off = pos->sect_off;
                vp = NULL;
              }
            }

            /*---------------------------------------------------------*/
            /* Else, either pos->sector does not belong to file or     */
            /* pos->offset >= Flash->sect_sz, so return error.         */
            /*---------------------------------------------------------*/
            else
            {
              set_errno(EINVAL);
              return (void *)-1;
            }
          }

          /*-----------------------------------------------------------*/
          /* If the stream has a valid cache entry, null it.           */
          /*-----------------------------------------------------------*/
          if (stream->cached && set_free &&
              FreeSector(&stream->cached, &Flash->cache))
            return (void *)-1;
        }
        break;
      }

      /*---------------------------------------------------------------*/
      /* Handle FsGetFileNo() for the flash file sytem.                */
      /*---------------------------------------------------------------*/
      case GET_FSUID:
        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        dir_ent = va_arg(ap, FFSEnt *);
        va_end(ap);

        dir = &dir_ent->entry.dir;

        /*-------------------------------------------------------------*/
        /* While there are terminating '/' characters, remove them.    */
        /*-------------------------------------------------------------*/
        end = strlen(name) - 1;
        while (name[end] == '/')
          name[end--] = '\0';

        /*-------------------------------------------------------------*/
        /* Look for the file/directory in parent directory.            */
        /*-------------------------------------------------------------*/
        if (find_file(dir, name, F_EXECUTE | F_WRITE, &tmp_ent, F_AND_D))
          return (void *)-1;
        if (tmp_ent == NULL)
        {
          set_errno(ENOENT);
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* Return the FSUID (combination of volume id and fileno).     */
        /*-------------------------------------------------------------*/
        return (void *)fsuid(Flash->vid,
                             tmp_ent->entry.file.comm->fileno);

      /*---------------------------------------------------------------*/
      /* Handle FsAttribute() for the flash file system.               */
      /*---------------------------------------------------------------*/
      case ATTRIB:
      {
        ui32 mask, *attrib, orig_attrib;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        dir_ent = va_arg(ap, FFSEnt *);
        mask = va_arg(ap, ui32);
        attrib = va_arg(ap, ui32 *);
        va_end(ap);

        dir = &dir_ent->entry.dir;

        /*-------------------------------------------------------------*/
        /* While there are terminating '/' characters, remove them.    */
        /*-------------------------------------------------------------*/
        end = strlen(name) - 1;
        while (name[end] == '/')
          name[end--] = '\0';

        /*-------------------------------------------------------------*/
        /* Look for the file/directory in parent directory.            */
        /*-------------------------------------------------------------*/
        if (find_file(dir, name, F_EXECUTE | F_WRITE, &tmp_ent, F_AND_D))
          return (void *)-1;
        if (tmp_ent == NULL)
        {
          set_errno(ENOENT);
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* Check that owner of file is trying to modify attributes.    */
        /*-------------------------------------------------------------*/
        FsGetId(&uid, &gid);
        if (uid != tmp_ent->entry.file.comm->user_id)
        {
          set_errno(EPERM);
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* Save copy of current attributes.                            */
        /*-------------------------------------------------------------*/
        orig_attrib = tmp_ent->entry.file.comm->attrib;

        /*-------------------------------------------------------------*/
        /* If attributes need to change, change them.                  */
        /*-------------------------------------------------------------*/
        if (mask)
          tmp_ent->entry.file.comm->attrib = (orig_attrib & ~mask) |
                                             (mask & *attrib);

        /*-------------------------------------------------------------*/
        /* Return original copy of attributes.                         */
        /*-------------------------------------------------------------*/
        *attrib = orig_attrib;
        break;
      }

      /*---------------------------------------------------------------*/
      /* Handle fflush() for the flash file system.                    */
      /*---------------------------------------------------------------*/
      case FFLUSH:
      {
        int status;

        link_ptr = &((FFSEnt *)handle)->entry.file;

        /*-------------------------------------------------------------*/
        /* No flushing needed for directory files.                     */
        /*-------------------------------------------------------------*/
        if (((FFSEnt *)handle)->type == FDIREN)
          return NULL;

        /*-------------------------------------------------------------*/
        /* Check if a recycle is needed.                               */
        /*-------------------------------------------------------------*/
        status = FlashRecChck(SKIP_CHECK_FULL);
        if (status == RECYCLE_OK)
          return NULL;
        else if (status == RECYCLE_FAILED)
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* Flush the data from the cache.                              */
        /*-------------------------------------------------------------*/
        status = FlushSectors(&Flash->cache);
        if (status == -1)
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* For normal files, write control only if sectors were written*/
        /*-------------------------------------------------------------*/
        if (status && !(link_ptr->comm->mode & S_CREATN))
        {
          if (FlashWrCtrl(ADJUST_ERASE))
            return (void *)-1;

          /*-----------------------------------------------------------*/
          /* Additionally, check if a recycle is needed.               */
          /*-----------------------------------------------------------*/
          if (FlashRecChck(SKIP_CHECK_FULL) == RECYCLE_FAILED)
            return (void *)-1;
        }
        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle fseek() for the flash file system.                     */
      /*---------------------------------------------------------------*/
      case FSEEK:
      {
        long offst;
        int whence;
        i32 offset, curr_pos;

        link_ptr = &((FFSEnt *)handle)->entry.file;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        offst = va_arg(ap, long);
        whence = va_arg(ap, int);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* If offset is not negative, we're seeking forward in file.   */
        /*-------------------------------------------------------------*/
        if (offst >= 0)
        {
          offset = (i32)offst;

          /*-----------------------------------------------------------*/
          /* In case the file is empty, force it to perform SEEK_SET.  */
          /*-----------------------------------------------------------*/
          if (link_ptr->comm->frst_sect == FFREE_SECT)
            whence = SEEK_SET;

          /*-----------------------------------------------------------*/
          /* Handle seek from beginning of file going towards the end. */
          /*-----------------------------------------------------------*/
          if (whence == SEEK_SET)
          {
            /*---------------------------------------------------------*/
            /* If file is empty, call seek_past_end() directly.        */
            /*---------------------------------------------------------*/
            if (link_ptr->comm->frst_sect == FFREE_SECT)
              vp = seek_past_end(offset, stream);

            /*---------------------------------------------------------*/
            /* Else perform seek within or past the end of the file.   */
            /*---------------------------------------------------------*/
            else
            {
              /*-------------------------------------------------------*/
              /* If seek goes beyond the end of the file, seek past    */
              /* what's left after subtracting the file size.          */
              /*-------------------------------------------------------*/
              if (link_ptr->comm->size < (ui32)offset)
              {
                offset -= (i32)link_ptr->comm->size;
                vp = seek_past_end(offset, stream);
              }

              /*-------------------------------------------------------*/
              /* Else the seek is within the file.                     */
              /*-------------------------------------------------------*/
              else
              {
                /*-----------------------------------------------------*/
                /* Figure out the current position.                    */
                /*-----------------------------------------------------*/
                if (stream->curr_ptr.sector == (ui16)-1)
                  curr_pos = (i32)link_ptr->comm->size;
                else
                  curr_pos = (i32)(stream->curr_ptr.sect_off *
                             Flash->sect_sz + stream->curr_ptr.offset);

                vp = seek_within_file(stream, offset, curr_pos);
              }
            }
          }

          /*-----------------------------------------------------------*/
          /* Handle the seek from the current file position indicator  */
          /* going towards the end of the file.                        */
          /*-----------------------------------------------------------*/
          else if (whence == SEEK_CUR)
          {
            /*---------------------------------------------------------*/
            /* Figure out the current position.                        */
            /*---------------------------------------------------------*/
            if (stream->curr_ptr.sector == (ui16)-1)
              curr_pos = (i32)link_ptr->comm->size;
            else
              curr_pos = (i32)(stream->curr_ptr.sect_off *
                                Flash->sect_sz +
                                stream->curr_ptr.offset);

            /*---------------------------------------------------------*/
            /* If seek goes beyond the end of the file, do seek past   */
            /* end on what's left after subtracting the difference     */
            /* between the current position and the end of file first. */
            /*---------------------------------------------------------*/
            if (curr_pos + offset > (i32)link_ptr->comm->size)
            {
              offset -= ((i32)link_ptr->comm->size - curr_pos);
              vp = seek_past_end(offset, stream);
            }

            /*---------------------------------------------------------*/
            /* Else the seek is within the file                        */
            /*---------------------------------------------------------*/
            else
              vp = seek_within_file(stream, offset + curr_pos, curr_pos);
          }

          /*-----------------------------------------------------------*/
          /* Handle seek from the end of file going beyond it, i.e.    */
          /* expanding the file.                                       */
          /*-----------------------------------------------------------*/
          else if (whence == SEEK_END)
          {
            /*---------------------------------------------------------*/
            /* If seek past end fails, return error.                   */
            /*---------------------------------------------------------*/
            vp = seek_past_end(offset, stream);
          }

          /*-----------------------------------------------------------*/
          /* Invalid option, return error.                             */
          /*-----------------------------------------------------------*/
          else
          {
            set_errno(EINVAL);
            return (void *)-1;
          }
        }

        /*-------------------------------------------------------------*/
        /* Else we're seeking backwards in the file.                   */
        /*-------------------------------------------------------------*/
        else
        {
          /*-----------------------------------------------------------*/
          /* Change offset to positive value (in backwards direction). */
          /*-----------------------------------------------------------*/
          offst = -offst;
          offset = (i32)offst;

          /*-----------------------------------------------------------*/
          /* If the file is empty, return error always.                */
          /*-----------------------------------------------------------*/
          if (link_ptr->comm->frst_sect == FFREE_SECT)
          {
            set_errno(EINVAL);
            return (void *)-1;
          }

          /*-----------------------------------------------------------*/
          /* Cannot go backwards if we're at the beginning of the file */
          /* so return error.                                          */
          /*-----------------------------------------------------------*/
          else if (whence == SEEK_SET)
          {
            set_errno(EINVAL);
            return (void *)-1;
          }

          /*-----------------------------------------------------------*/
          /* Handle the seek from the current file position indicator  */
          /* going towards the beginning of the file.                  */
          /*-----------------------------------------------------------*/
          else if (whence == SEEK_CUR)
          {
            /*---------------------------------------------------------*/
            /* Figure out the current position.                        */
            /*---------------------------------------------------------*/
            if (stream->curr_ptr.sector == (ui16)-1)
              curr_pos = (i32)link_ptr->comm->size;
            else
              curr_pos = (i32)(stream->curr_ptr.sect_off *
                         Flash->sect_sz + stream->curr_ptr.offset);

            /*---------------------------------------------------------*/
            /* If we're trying to go beyond the beginning, error.      */
            /*---------------------------------------------------------*/
            if (curr_pos < offset)
            {
              set_errno(EINVAL);
              return (void *)-1;
            }

            /*---------------------------------------------------------*/
            /* Else if current position is past EOF, either current    */
            /* seek still leaves us past EOF, or we seek within file.  */
            /*---------------------------------------------------------*/
            else if (stream->curr_ptr.sector == FDRTY_SECT)
            {
              FCOM_T *file = ((FFSEnt *)stream->handle)->entry.file.comm;
              i32 past_end = (i32)(stream->curr_ptr.sect_off *
                                   Flash->sect_sz +
                                   stream->curr_ptr.offset -
                                   file->size);

              /*-------------------------------------------------------*/
              /* Determine if we are still past the end of the file.   */
              /*-------------------------------------------------------*/
              if (past_end > offset)
                vp = seek_past_end(past_end - offset, stream);
              else
              {
                offset = (i32)(file->size - (offset - past_end));
                vp = seek_within_file(stream, offset, (i32)file->size);
              }
            }

            /*---------------------------------------------------------*/
            /* Else adjust offset and perform seek up.                 */
            /*---------------------------------------------------------*/
            else
            {
              offset = (curr_pos - offset);
              vp = seek_within_file(stream, offset, curr_pos);
            }
          }

          /*-----------------------------------------------------------*/
          /* Handle the seek from the end of file going towards the    */
          /* beginning of the file.                                    */
          /*-----------------------------------------------------------*/
          else if (whence == SEEK_END)
          {
            /*---------------------------------------------------------*/
            /* If we're trying to go beyond the beginning, error.      */
            /*---------------------------------------------------------*/
            if ((i32)link_ptr->comm->size < offset)
            {
              set_errno(EINVAL);
              return (void *)-1;
            }

            /*---------------------------------------------------------*/
            /* Else adjust offset and perform seek up.                 */
            /*---------------------------------------------------------*/
            else
            {
              /*-------------------------------------------------------*/
              /* Figure out the current position.                      */
              /*-------------------------------------------------------*/
              if (stream->curr_ptr.sector == (ui16)-1)
                curr_pos = (i32)link_ptr->comm->size;
              else
                curr_pos = (i32)(stream->curr_ptr.sect_off *
                           Flash->sect_sz + stream->curr_ptr.offset);

              offset = ((i32)link_ptr->comm->size - offset);
              vp = seek_within_file(stream, offset, curr_pos);
            }
          }
        }

        /*-------------------------------------------------------------*/
        /* If seek hasn't failed, figure out the new offset.           */
        /*-------------------------------------------------------------*/
        if (vp == NULL)
        {
          /*-----------------------------------------------------------*/
          /* If file is not empty, use curr_ptr to figure new offset.  */
          /*-----------------------------------------------------------*/
          if (link_ptr->comm->frst_sect != FFREE_SECT)
          {
            /*---------------------------------------------------------*/
            /* Add offset up to current sector.                        */
            /*---------------------------------------------------------*/
            vp = (void *)(stream->curr_ptr.sect_off * Flash->sect_sz);

            /*---------------------------------------------------------*/
            /* If at the end of a sector, add whole sector, else add   */
            /* sector offset.                                          */
            /*---------------------------------------------------------*/
            if (stream->curr_ptr.sector == (ui16)-1)
              vp = (void *)((ui32)vp + Flash->sect_sz);
            else
              vp = (void *)((ui32)vp + stream->curr_ptr.offset);
          }

          /*-----------------------------------------------------------*/
          /* If valid cache entry associated with stream, free it and  */
          /* null it out.                                              */
          /*-----------------------------------------------------------*/
          if (stream->cached && FreeSector(&stream->cached,
                                           &Flash->cache))
            return (void *)-1;
        }
        break;
      }

      /*---------------------------------------------------------------*/
      /* Handle ftell() for the flash file system.                     */
      /*---------------------------------------------------------------*/
      case FTELL:
        /*-------------------------------------------------------------*/
        /* If file is empty, return 0.                                 */
        /*-------------------------------------------------------------*/
        link_ptr = &((FFSEnt *)handle)->entry.file;
        if (link_ptr->comm->size == 0)
          return 0;

        /*-------------------------------------------------------------*/
        /* Current position = -1 implies end of file, return file size.*/
        /*-------------------------------------------------------------*/
        if (stream->curr_ptr.sector == (ui16)-1)
          return (void *)link_ptr->comm->size;

        /*-------------------------------------------------------------*/
        /* Otherwise, count bytes from beginning to curr_ptr.          */
        /*-------------------------------------------------------------*/
        return (void *)(stream->curr_ptr.sect_off * Flash->sect_sz +
                        stream->curr_ptr.offset);

      /*---------------------------------------------------------------*/
      /* Handle remove() for the flash file system.                    */
      /*---------------------------------------------------------------*/
      case REMOVE:
        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        dir_ent = va_arg(ap, FFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Call helper function to perform the actual remove.          */
        /*-------------------------------------------------------------*/
        return remove_file(dir_ent, name);

      case FSEARCH:
      {
        /*-------------------------------------------------------------*/
        /* Search a filename in the flash file system.                 */
        /*-------------------------------------------------------------*/
        int use_cwd, find_parent;
        ui32 dummy;
        FFSEnt *curr_dir;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        find_parent = va_arg(ap, int);
        use_cwd = va_arg(ap, int);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Start either from first or current working directory.       */
        /*-------------------------------------------------------------*/
        vol = stream->volume;
        FsReadCWD(&dummy, (void *)&curr_dir);
        if (use_cwd == FIRST_DIR || curr_dir == NULL)
          dir_ent = &vol->files_tbl->tbl[0];
        else
          dir_ent = curr_dir;

        /*-------------------------------------------------------------*/
        /* Loop to process each component of path.                     */
        /*-------------------------------------------------------------*/
        for (;;)
        {
          /*-----------------------------------------------------------*/
          /* Skip multiple "./" strings. Skip ending ".".              */
          /*-----------------------------------------------------------*/
          while (!strncmp(FSPath, "./", 2))
            FSPath += 2;
          if (!strcmp(FSPath, "."))
            ++FSPath;

          /*-----------------------------------------------------------*/
          /* Check if path moves us up one directory level.            */
          /*-----------------------------------------------------------*/
          if (!strncmp(FSPath, "..", 2))
          {
            /*---------------------------------------------------------*/
            /* Update file pathname pointer.                           */
            /*---------------------------------------------------------*/
            FSPath += 2;

            /*---------------------------------------------------------*/
            /* If already at volume's top level, return to FSearch().  */
            /*---------------------------------------------------------*/
            if (dir_ent == &vol->files_tbl->tbl[0])
              return NULL;

            /*---------------------------------------------------------*/
            /* Else move up in this volume and adjust filename.        */
            /*---------------------------------------------------------*/
            else
            {
              /*-------------------------------------------------------*/
              /* While there are leading '/' charactors, remove them.  */
              /*-------------------------------------------------------*/
              while (*FSPath == '/')
                ++FSPath;

              /*-------------------------------------------------------*/
              /* Move one level up in this volume's directory tree.    */
              /*-------------------------------------------------------*/
              dir_ent = dir_ent->entry.dir.parent_dir;
            }
          }

          /*-----------------------------------------------------------*/
          /* Else if we're at the path end, stop.                      */
          /*-----------------------------------------------------------*/
          else if (*FSPath == '\0')
          {
            /*---------------------------------------------------------*/
            /* If we're looking for parent directory, error.           */
            /*---------------------------------------------------------*/
            if (find_parent == PARENT_DIR)
            {
              set_errno(ENOENT);
              return (void *)-1;
            }

            /*---------------------------------------------------------*/
            /* Initialize file control block and set return value.     */
            /*---------------------------------------------------------*/
            stream->ioctl = FlashIoctl;
            stream->acquire = acquire;
            stream->release = release;
            if (stream->volume == NULL)
              stream->volume = vol;
            vp = dir_ent;
            break;
          }

          else
          {
            /*---------------------------------------------------------*/
            /* Check if we reached last name in filename path.         */
            /*---------------------------------------------------------*/
            i = IsLast(FSPath, &j);
            if (!i)
            {
              /*-------------------------------------------------------*/
              /* If looking for file's parent directory, stop here.    */
              /*-------------------------------------------------------*/
              if (find_parent == PARENT_DIR)
              {
                /*-----------------------------------------------------*/
                /* Initialize file control block and set return value. */
                /*-----------------------------------------------------*/
                stream->ioctl = FlashIoctl;
                stream->acquire = acquire;
                stream->release = release;
                if (stream->volume == NULL)
                  stream->volume = vol;
                vp = dir_ent;
                break;
              }

              /*-------------------------------------------------------*/
              /* Else we're looking for the actual filename. If it's a */
              /* directory name and it has a trailing '/', ignore it.  */
              /*-------------------------------------------------------*/
              else
              {
                ui32 len = strlen(FSPath) - 1;

                if (FSPath[len--] == '/')
                {
                  i = j - 1;
                  while (FSPath[len--] == '/')
                    --i;
                }
                else
                  i = j;

                /*-----------------------------------------------------*/
                /* If path too long, return error if no truncation,    */
                /* else truncate.                                      */
                /*-----------------------------------------------------*/
                if (i > PATH_MAX)
                {
  #if _PATH_NO_TRUNC
                  set_errno(ENAMETOOLONG);
                  return (void *)-1;
  #else
                  i = PATH_MAX;
  #endif
                }
              }
            }

            /*---------------------------------------------------------*/
            /* Check for search permission for the directory.          */
            /*---------------------------------------------------------*/
            if (CheckPerm(dir_ent->entry.dir.comm, F_EXECUTE))
              return (void *)-1;

            /*---------------------------------------------------------*/
            /* Look for subdirectory in directory.                     */
            /*---------------------------------------------------------*/
            for (tmp_ent = dir_ent->entry.dir.first;;
                 tmp_ent = tmp_ent->entry.dir.next)
            {
              /*-------------------------------------------------------*/
              /* If subdirectory not found, return error.              */
              /*-------------------------------------------------------*/
              if (tmp_ent == NULL)
              {
                set_errno(ENOENT);
                return (void *)-1;
              }

              /*-------------------------------------------------------*/
              /* Check if directory entry matches next name in path.   */
              /*-------------------------------------------------------*/
              if (!strncmp(tmp_ent->entry.dir.name, FSPath, i) &&
                  strlen(tmp_ent->entry.dir.name) == i)
              {
                /*-----------------------------------------------------*/
                /* Break if entry is directory file or it's file and   */
                /* we are looking for a file and it's last entry in    */
                /* the path.                                           */
                /*-----------------------------------------------------*/
                if (tmp_ent->type == FDIREN ||
                    (find_parent == DIR_FILE &&
                     !strcmp(tmp_ent->entry.dir.name, FSPath)))
                  break;
                else
                {
                  set_errno(ENOTDIR);
                  return (void *)-1;
                }
              }
            }

            /*---------------------------------------------------------*/
            /* Update path pointer and move to sub-directory.          */
            /*---------------------------------------------------------*/
            FSPath += j;
            dir_ent = tmp_ent;
          }
        }
        break;
      }

      /*---------------------------------------------------------------*/
      /* All unimplemented API functions return error.                 */
      /*---------------------------------------------------------------*/
      case TMPFILE:
      {
        set_errno(EINVAL);
        return NULL;
      }

      default:
      {
        set_errno(EINVAL);
        return (void *)-1;
      }
    }
  }

  return vp;
}
#endif /* NUM_FFS_VOLS */

