/***********************************************************************/
/*                                                                     */
/*   Module:  rsioctl.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2005.0                                                   */
/*   Purpose: Implements RAM file system ioctl()                       */
/*                                                                     */
/*---------------------------------------------------------------------*/
/*                                                                     */
/*               Copyright 2005, Blunk Microsystems                    */
/*                      ALL RIGHTS RESERVED                            */
/*                                                                     */
/*   Licensees have the non-exclusive right to use, modify, or extract */
/*   this computer program for software development at a single site.  */
/*   This program may be resold or disseminated in executable format   */
/*   only. The source code may not be redistributed or resold.         */
/*                                                                     */
/***********************************************************************/
#include <stddef.h>
#include "ramfsp.h"

#if NUM_RFS_VOLS

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
#define F_ONLY             0
#define F_AND_D            1

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
static void garbageCollect(void);

/***********************************************************************/
/* Local Function Definitions                                          */
/***********************************************************************/

/***********************************************************************/
/* incTblEntries: Increment the number of free entries                 */
/*                                                                     */
/*       Input: entry = pointer to freed table entry                   */
/*                                                                     */
/***********************************************************************/
static void incTblEntries(const RFSEnt *entry)
{
  RFSEnts *table;

  /*-------------------------------------------------------------------*/
  /* Look for table that contains the freed entry.                     */
  /*-------------------------------------------------------------------*/
  for (table = Ram->files_tbl; table; table = table->next_tbl)
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
/*   find_file: Look for a file with name path in dir                  */
/*                                                                     */
/*      Inputs: dir = parent directory to look into                    */
/*              path = file name                                       */
/*              mode = permissions directory must have                 */
/*              entry = place to store found file                      */
/*              search_flag = look for file only/ file and dir         */
/*                                                                     */
/*     Returns: -1 if error in searching, 0 otherwise(entry will have  */
/*              either pointer to found file or NULL otherwise)        */
/*                                                                     */
/***********************************************************************/
static int find_file(const RDIR_T *dir, const char *path, int mode,
                     RFSEnt **entry, int search_flag)
{
  /*-------------------------------------------------------------------*/
  /* If path ends in '/', return error (it's not a file).              */
  /*-------------------------------------------------------------------*/
  if (path[strlen(path) - 1] == '/')
  {
    set_errno(EINVAL);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Check that parent directory has permissions.                      */
  /*-------------------------------------------------------------------*/
  if (CheckPerm((void *)dir->comm, mode))
    return -1;

  /*-------------------------------------------------------------------*/
  /* Look for file in its parent directory.                            */
  /*-------------------------------------------------------------------*/
  for (*entry = dir->first; *entry; *entry = (*entry)->entry.dir.next)
  {
    /*-----------------------------------------------------------------*/
    /* Check if entry's name matches our file's name.                  */
    /*-----------------------------------------------------------------*/
    if (!strncmp((*entry)->entry.dir.name, path, FILENAME_MAX))
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
  ui32 num_sects, i;

  /*-------------------------------------------------------------------*/
  /* If seek goes past start sector, subtract starting position from   */
  /* offset.                                                           */
  /*-------------------------------------------------------------------*/
  if (offset > (RAM_SECT_SZ - start_pos.offset))
    offset -= (RAM_SECT_SZ - start_pos.offset);

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
  start_pos.sector = (ui32)((RamSect *)start_pos.sector)->next;

  /*-------------------------------------------------------------------*/
  /* Figure out how many sectors we have to go up.                     */
  /*-------------------------------------------------------------------*/
  num_sects = (ui32)offset / RAM_SECT_SZ;
  start_pos.offset = (ui32)offset % RAM_SECT_SZ;

  /*-------------------------------------------------------------------*/
  /* Step through to the desired num_sects.                            */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < num_sects; ++i)
    start_pos.sector = (ui32)((RamSect *)start_pos.sector)->next;
  start_pos.sect_off += num_sects;

  /*-------------------------------------------------------------------*/
  /* Update curr_ptr and return success.                               */
  /*-------------------------------------------------------------------*/
  stream->curr_ptr = start_pos;
  return NULL;
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
  /* Get valid position from old_ptr.                                  */
  /*-------------------------------------------------------------------*/
  old_pos = (i32)stream->old_ptr.sect_off * RAM_SECT_SZ +
            stream->old_ptr.offset;

  /*-------------------------------------------------------------------*/
  /* Choose among curr_pos, old_pos, or beginning of file, whichever   */
  /* is closer to where we want to get. The only time when beginning   */
  /* of file is better than the other two is when both curr and old    */
  /* position are beyond where we want to seek.                        */
  /*-------------------------------------------------------------------*/
  if (curr_pos > offset && old_pos > offset)
  {
    beg_ptr.sector =
            (ui32)((RFSEnt *)stream->handle)->entry.file.comm->frst_sect;
    beg_ptr.offset = beg_ptr.sect_off = 0;
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
/***********************************************************************/
static void seek_past_end(i32 offset, FILE *stream)
{
  RCOM_T *file = ((RFSEnt *)stream->handle)->entry.file.comm;

  /*-------------------------------------------------------------------*/
  /* If offset is zero, set current pointer to EOF.                    */
  /*-------------------------------------------------------------------*/
  if (offset == 0)
    RamPoint2End(&stream->curr_ptr, file);

  /*-------------------------------------------------------------------*/
  /* Else set current pointer past the EOF.                            */
  /*-------------------------------------------------------------------*/
  else
  {
    /*-----------------------------------------------------------------*/
    /* Position is given by sect_off and offset.                       */
    /*-----------------------------------------------------------------*/
    stream->curr_ptr.sector = SEEK_PAST_END;
    stream->curr_ptr.sect_off = (file->size + offset) / RAM_SECT_SZ;
    stream->curr_ptr.offset = (file->size + offset) % RAM_SECT_SZ;
  }
}

/***********************************************************************/
/* ram_trunc_zero: Make a file zero length                             */
/*                                                                     */
/*       Input: entry = pointer to file in files table                 */
/*                                                                     */
/***********************************************************************/
static void ram_trunc_zero(const RFIL_T *entry)
{
  ui32 i;

  /*-------------------------------------------------------------------*/
  /* If the file has non-zero length, erase all the bytes.             */
  /*-------------------------------------------------------------------*/
  if (entry->comm->size)
  {
    /*-----------------------------------------------------------------*/
    /* Truncate the file by freeing all of its sectors.                */
    /*-----------------------------------------------------------------*/
    while (entry->comm->last_sect)
      RamFreeSect(entry->comm);
    entry->comm->size = 0;

    /*-----------------------------------------------------------------*/
    /* Invalidate curr_ptr for any stream pointing to this file.       */
    /*-----------------------------------------------------------------*/
    for (i = 0; i < FOPEN_MAX; ++i)
    {
      if (Files[i].ioctl == RamIoctl &&
          ((RFSEnt *)Files[i].handle)->entry.file.comm == entry->comm)
      {
        Files[i].curr_ptr.sector = (ui32)NULL;
        Files[i].curr_ptr.sect_off = 0;
        Files[i].old_ptr.sector = (ui32)NULL;
        Files[i].old_ptr.sect_off = 0;
      }
    }
  }
}

/***********************************************************************/
/*   truncatef: Truncate the length of a file to desired length        */
/*                                                                     */
/*      Inputs: entry = pointer to file in files table                 */
/*              length = desired file length                           */
/*                                                                     */
/*     Returns: NULL on success, (void *)-1 otherwise                  */
/*                                                                     */
/***********************************************************************/
static void *truncatef(RFSEnt *entry, off_t length)
{
  ui16 last_sect_offset;
  int  count, num_sects, i;
  RFIL_T *lnk = &entry->entry.file;
  RamSect *sect;

  /*-------------------------------------------------------------------*/
  /* Just return if the actual length equals the desired length.       */
  /*-------------------------------------------------------------------*/
  if (lnk->comm->size == length)
    return NULL;

  /*-------------------------------------------------------------------*/
  /* If length is 0, remove whole data content.                        */
  /*-------------------------------------------------------------------*/
  if (length == 0)
    ram_trunc_zero(lnk);

  /*-------------------------------------------------------------------*/
  /* Else if actual size is less then desired size, append.            */
  /*-------------------------------------------------------------------*/
  else if (lnk->comm->size < length)
  {
    /*-----------------------------------------------------------------*/
    /* Add the extra zero-valued bytes to the file.                    */
    /*-----------------------------------------------------------------*/
    if (RamSeekPastEnd(length - lnk->comm->size, lnk->file))
      return (void *)-1;
  }

  /*-------------------------------------------------------------------*/
  /* Else actual size is greater than desired size, truncate.          */
  /*-------------------------------------------------------------------*/
  else if (lnk->comm->size > length)
  {
    /*-----------------------------------------------------------------*/
    /* Calculate number of remaining sectors and last sector offset.   */
    /*-----------------------------------------------------------------*/
    num_sects = length / RAM_SECT_SZ;
    last_sect_offset = length % RAM_SECT_SZ;
    if (num_sects && last_sect_offset == 0)
    {
      --num_sects;
      last_sect_offset = RAM_SECT_SZ;
    }

    /*-----------------------------------------------------------------*/
    /* Go to last new sector.                                          */
    /*-----------------------------------------------------------------*/
    sect = lnk->comm->frst_sect;
    for (count = 0; count < num_sects; ++count)
      sect = sect->next;

    /*-----------------------------------------------------------------*/
    /* Remove all sectors from current one to end.                     */
    /*-----------------------------------------------------------------*/
    while (lnk->comm->last_sect != sect)
      RamFreeSect(lnk->comm);

    /*-----------------------------------------------------------------*/
    /* Update file's one past last pointer and size.                   */
    /*-----------------------------------------------------------------*/
    lnk->comm->one_past_last = last_sect_offset;
    lnk->comm->size = num_sects * RAM_SECT_SZ + last_sect_offset;

    /*-----------------------------------------------------------------*/
    /* Check if any file descriptor need to be updated.                */
    /*-----------------------------------------------------------------*/
    for (i = 0; i < FOPEN_MAX; ++i)
      if (Files[i].handle == lnk && Files[i].ioctl == RamIoctl)
      {
        /*-------------------------------------------------------------*/
        /* If curr_ptr is invalid, set it to end of file.              */
        /*-------------------------------------------------------------*/
        if (Files[i].curr_ptr.sector != SEEK_PAST_END &&
            (Files[i].curr_ptr.sect_off > num_sects ||
             (Files[i].curr_ptr.sect_off == num_sects &&
              Files[i].curr_ptr.offset > last_sect_offset)))
          Files[i].curr_ptr.sector = SEEK_PAST_END;

        /*-------------------------------------------------------------*/
        /* If old_ptr is invalid, set it to end of file.               */
        /*-------------------------------------------------------------*/
        if (Files[i].old_ptr.sector != SEEK_PAST_END &&
            (Files[i].old_ptr.sect_off > num_sects ||
             (Files[i].old_ptr.sect_off == num_sects &&
              Files[i].old_ptr.offset > last_sect_offset)))
          Files[i].old_ptr.sector = SEEK_PAST_END;
      }
  }

  /*-------------------------------------------------------------------*/
  /* Update file access and modified times. Return success.            */
  /*-------------------------------------------------------------------*/
  lnk->comm->ac_time = lnk->comm->mod_time = OsSecCount;
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
static void *remove_dir(RFSEnt *dir_ent)
{
  RDIR_T *dir = &dir_ent->entry.dir;
  uid_t uid;
  gid_t gid;
  uint i;

  /*-------------------------------------------------------------------*/
  /* If we're trying to delete the root directory, return error.       */
  /*-------------------------------------------------------------------*/
  if (dir_ent == &Ram->files_tbl->tbl[0])
  {
    set_errno(ENOENT);
    return (void *)-1;
  }

  /*-------------------------------------------------------------------*/
  /* Check that parent directory has write permissions.                */
  /*-------------------------------------------------------------------*/
  if (CheckPerm((void *)dir->parent_dir->entry.dir.comm, F_WRITE))
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
  if ((dir->first && dir->comm->links == 1))
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
    if (Files[i].ioctl == RamIoctl && Files[i].pos == dir_ent)
      Files[i].pos = dir->prev;

  /*-------------------------------------------------------------------*/
  /* Remove the directory from the files table.                        */
  /*-------------------------------------------------------------------*/
  if (dir->prev)
    dir->prev->entry.dir.next = dir->next;
  else
    dir->parent_dir->entry.dir.first = dir->next;
  if (dir->next)
    dir->next->entry.dir.prev = dir->prev;

  /*-------------------------------------------------------------------*/
  /* If this was the only link remove the file entry as well.          */
  /*-------------------------------------------------------------------*/
  if (--dir->comm->links == 0)
  {
    dir->comm->addr->type = FEMPTY;
    ++Ram->total_free;
    incTblEntries(dir->comm->addr);
  }

  /*-------------------------------------------------------------------*/
  /* Clear out the entry for the removed directory.                    */
  /*-------------------------------------------------------------------*/
  dir_ent->type = FEMPTY;
  dir->next = dir->prev = dir->first = NULL;

  /*-------------------------------------------------------------------*/
  /* Adjust the number of free file entries.                           */
  /*-------------------------------------------------------------------*/
  ++Ram->total_free;
  incTblEntries(dir_ent);

  /*-------------------------------------------------------------------*/
  /* Do garbage collection if necessary.                               */
  /*-------------------------------------------------------------------*/
  if (Ram->total_free >= (FNUM_ENT * 5 / 3))
    garbageCollect();

  return NULL;
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
static void *remove_file(RFSEnt *dir_ent, const char *fname)
{
  RFIL_T *link_ptr;
  RFSEnt *tmp_ent;
  uid_t  uid;
  gid_t  gid;
  uint   i;
  int    rmv_file;
  RDIR_T *dir = &dir_ent->entry.dir;

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
    ram_trunc_zero(link_ptr);

    /*-----------------------------------------------------------------*/
    /* Remove file from the files table and adjust free entries.       */
    /*-----------------------------------------------------------------*/
    link_ptr->comm->addr->type = FEMPTY;
    ++Ram->total_free;
    incTblEntries(link_ptr->comm->addr);
  }

  /*-------------------------------------------------------------------*/
  /* If an open dir structure has pointer to this entry, update it.    */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < FOPEN_MAX; ++i)
    if (Files[i].ioctl == RamIoctl && Files[i].pos == tmp_ent)
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
  /* If the file is to be removed, clear the comm ptr and set the link */
  /* entry to empty, clear file, next and prev.                        */
  /*-------------------------------------------------------------------*/
  if (rmv_file)
  {
    link_ptr->comm = NULL;
    link_ptr->parent_dir = link_ptr->next = link_ptr->prev = NULL;
    link_ptr->file = NULL;
    tmp_ent->type = FEMPTY;

    /*-----------------------------------------------------------------*/
    /* Increment number of free entries.                               */
    /*-----------------------------------------------------------------*/
    ++Ram->total_free;
    incTblEntries(tmp_ent);

    /*-----------------------------------------------------------------*/
    /* Do garbage collection if necessary.                             */
    /*-----------------------------------------------------------------*/
    if (Ram->total_free >= (FNUM_ENT * 5 / 3))
      garbageCollect();
  }

  /*-------------------------------------------------------------------*/
  /* Else don't free the entry yet. Free it when the last open file    */
  /* descriptor for this link is closed. For now, just set next and    */
  /* prev to REMOVED_LINK.                                             */
  /*-------------------------------------------------------------------*/
  else
    link_ptr->next = link_ptr->prev = (RFSEnt *)REMOVED_LINK;

  /*-------------------------------------------------------------------*/
  /* Do a sync to save new state of file system.                       */
  /*-------------------------------------------------------------------*/
  return NULL;
}

/***********************************************************************/
/* find_new_place: Find a free entry from among the list of tables     */
/*                                                                     */
/*       Input: smallest = ptr to table to be removed                  */
/*                                                                     */
/*     Returns: pointer to found entry or NULL if no entry was found   */
/*                                                                     */
/***********************************************************************/
static RFSEnt *find_new_place(const RFSEnts *smallest)
{
  RFSEnts *tbl;
  int i;

  /*-------------------------------------------------------------------*/
  /* Loop over every file table.                                       */
  /*-------------------------------------------------------------------*/
  for (tbl = Ram->files_tbl; tbl; tbl = tbl->next_tbl)
  {
    /*-----------------------------------------------------------------*/
    /* Check if table is not one being removed and has a free entry.   */
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
/*      Inputs: old = pointer to old entry                             */
/*              new = pointer to new entry                             */
/*                                                                     */
/***********************************************************************/
static void update_entries(const RFSEnt *old, RFSEnt *new)
{
  RFSEnts *curr_tbl = Ram->files_tbl;
  RFSEnt *curr_dir;
  RDIR_T *dir;
  RFIL_T *link_ptr;
  int i;
  ui32 dummy;

  for (i = 0; curr_tbl; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* If entry is a directory, look at next, prev, parent, first, and */
    /* actual file to see if any is equal to the old entry.            */
    /*-----------------------------------------------------------------*/
    if (curr_tbl->tbl[i].type == FDIREN)
    {
      dir = &(curr_tbl->tbl[i].entry.dir);
      if (dir->next == old)
        dir->next = new;
      else if (dir->prev == old)
        dir->prev = new;
      else if (dir->parent_dir == old)
        dir->parent_dir = new;
      else if (dir->first == old)
        dir->first = new;
      else if (dir->comm->addr == old)
      {
        new->entry.comm.addr = new;
        dir->comm = &(new->entry.comm);
      }
    }

    /*-----------------------------------------------------------------*/
    /* Else if entry is link, look at next, prev, and the actual file  */
    /* to see if any is equal to the old entry.                        */
    /*-----------------------------------------------------------------*/
    else if (curr_tbl->tbl[i].type == FFILEN)
    {
      link_ptr = &(curr_tbl->tbl[i].entry.file);
      if (link_ptr->next == old)
        link_ptr->next = new;
      else if (link_ptr->prev == old)
        link_ptr->prev = new;
      else if (link_ptr->comm->addr == old)
      {
        new->entry.comm.addr = new;
        link_ptr->comm = &(new->entry.comm);
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
    if (Files[i].handle == old)
      Files[i].handle = new;

  /*-------------------------------------------------------------------*/
  /* Finally, if FlashCurrDir is the same as old, update it.           */
  /*-------------------------------------------------------------------*/
  FsReadCWD(&dummy, (void *)&curr_dir);
  if (curr_dir == old)
    FsSaveCWD((ui32)Ram, (ui32)new);
}

/***********************************************************************/
/* garbageCollect: Remove table with most free entries                 */
/*                                                                     */
/***********************************************************************/
static void garbageCollect(void)
{
  int i, max_free = 0;
  RFSEnt *entry;
  RFSEnts *table = NULL, *curr_tbl;

  /*-------------------------------------------------------------------*/
  /* Skip if any opendir() has unmatched closedir().                   */
  /*-------------------------------------------------------------------*/
  if (Ram->opendirs)
    return;

  /*-------------------------------------------------------------------*/
  /* Walk thru all tables except the first one, finding the one with   */
  /* most free entries.                                                */
  /*-------------------------------------------------------------------*/
  for (curr_tbl = Ram->files_tbl->next_tbl; curr_tbl;
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
      PfAssert(Ram->files_tbl != table);

      /*---------------------------------------------------------------*/
      /* Remove and free table. Update counters, and then return.      */
      /*---------------------------------------------------------------*/
      if (table->prev_tbl)
        table->prev_tbl->next_tbl = table->next_tbl;
      if (table->next_tbl)
        table->next_tbl->prev_tbl = table->prev_tbl;
      table->next_tbl = table->prev_tbl = NULL;
      free(table);
      Ram->total_free -= FNUM_ENT;
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
/*   close_dir: Close a directory                                      */
/*                                                                     */
/*      Inputs: entry  = pointer to dir entry in files table           */
/*              stream = pointer to dir control block                  */
/*                                                                     */
/***********************************************************************/
static void close_dir(RFSEnt *entry, const FILE *stream)
{
  RDIR_T *dir_ptr = &entry->entry.dir;
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
        PfAssert(Files[i].ioctl == RamIoctl &&
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
/*     Returns: NULL on success, (void *)-1 on error                   */
/*                                                                     */
/***********************************************************************/
static void *close_file(RFSEnt *entry, FILE *stream)
{
  RFIL_T *link_ptr = &entry->entry.file;
  int     i;

  /*-------------------------------------------------------------------*/
  /* Free the file control block associated with this file.            */
  /*-------------------------------------------------------------------*/
  stream->read = NULL;
  stream->write = NULL;
  stream->curr_ptr.sector = (ui16)-1;
  stream->curr_ptr.offset = stream->curr_ptr.sect_off = 0;

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
    /* If it's a temporary file, remove it without any checking.       */
    /*-----------------------------------------------------------------*/
    if (link_ptr->comm->temp)
    {
      if (remove_file(entry->entry.file.parent_dir,
                      entry->entry.file.name))
        return (void *)-1;
    }

    /*-----------------------------------------------------------------*/
    /* If the number of links to this file is 0 need to remove the     */
    /* contents of the file as well.                                   */
    /*-----------------------------------------------------------------*/
    else if (link_ptr->comm->links == 0)
    {
      /*---------------------------------------------------------------*/
      /* Truncate file size to zero.                                   */
      /*---------------------------------------------------------------*/
      ram_trunc_zero(link_ptr);

      /*---------------------------------------------------------------*/
      /* Remove file from the files table and adjust free entries.     */
      /*---------------------------------------------------------------*/
      link_ptr->comm->addr->type = FEMPTY;
      ++Ram->total_free;
      incTblEntries(link_ptr->comm->addr);
    }

    /*-----------------------------------------------------------------*/
    /* If the file entry was previously removed, clear it.             */
    /*-----------------------------------------------------------------*/
    if (link_ptr->next == (RFSEnt *)REMOVED_LINK &&
        link_ptr->prev == (RFSEnt *)REMOVED_LINK)
    {

      /*---------------------------------------------------------------*/
      /* Clear struct fields.                                          */
      /*---------------------------------------------------------------*/
      link_ptr->comm = NULL;
      link_ptr->next = link_ptr->prev = NULL;
      link_ptr->file = NULL;
      entry->type = FEMPTY;

      /*---------------------------------------------------------------*/
      /* Increment number of free entries.                             */
      /*---------------------------------------------------------------*/
      ++Ram->total_free;
      incTblEntries(entry);

      /*---------------------------------------------------------------*/
      /* Do garbage collection if necessary.                           */
      /*---------------------------------------------------------------*/
      if (Ram->total_free >= (FNUM_ENT * 5 / 3))
        garbageCollect();
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
        PfAssert(Files[i].ioctl == RamIoctl &&
                 Files[i].flags & FCB_FILE);
        link_ptr->file = &Files[i];
      }
  }

  return NULL;
}

/***********************************************************************/
/*  statistics: Auxiliary function for FSTAT and STAT to fill out the  */
/*              struct stat contents                                   */
/*                                                                     */
/*      Inputs: buf = stat structure to be filled                      */
/*              entry = pointer to the entry for the stat              */
/*                                                                     */
/***********************************************************************/
static void statistics(struct stat *buf, RFSEnt *entry)
{
  RFIL_T *link_ptr = &entry->entry.file;

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
    buf->st_size = link_ptr->comm->size;
  else
    buf->st_size = 0;

  /*-------------------------------------------------------------------*/
  /* Look at the access and modified times.                            */
  /*-------------------------------------------------------------------*/
  buf->st_atime = link_ptr->comm->ac_time;
  buf->st_mtime = link_ptr->comm->mod_time;
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
  RFSEnts *curr_tbl;

  /*-------------------------------------------------------------------*/
  /* Go through all mounted volumes.                                   */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < NUM_RFS_VOLS; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* If this volume is mounted, check all its entries for name dup.  */
    /*-----------------------------------------------------------------*/
    if (MountedList.head == &RamGlobals[i].sys ||
        RamGlobals[i].sys.prev || RamGlobals[i].sys.next)
    {
      /*---------------------------------------------------------------*/
      /* Go through each table in volume.                              */
      /*---------------------------------------------------------------*/
      for (curr_tbl = RamGlobals[i].files_tbl; curr_tbl;
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
              !strncmp(curr_tbl->tbl[i].entry.dir.name, name,
                       FILENAME_MAX))
            return 1;
        }
      }
    }
  }
  return 0;
}

/***********************************************************************/
/*  find_entry: Find an empty entry in the file tables                 */
/*                                                                     */
/*      Output: *indexp = index of empty entry in table                */
/*                                                                     */
/*     Returns: NULL if no empty entry, or ptr to table with entry     */
/*                                                                     */
/***********************************************************************/
static RFSEnts *find_entry(ui32 *indexp)
{
  int t;
  RFSEnts *curr_table, *prev_table;

  /*-------------------------------------------------------------------*/
  /* Look through all the tables for an empty entry.                   */
  /*-------------------------------------------------------------------*/
  curr_table = Ram->files_tbl;
  do
  {
    /*-----------------------------------------------------------------*/
    /* Search current table for an empty entry.                        */
    /*-----------------------------------------------------------------*/
    for (t = 0; t < FNUM_ENT; ++t)
    {
      /*---------------------------------------------------------------*/
      /* If an empty entry is found, take it and remember its value.   */
      /*---------------------------------------------------------------*/
      if (curr_table->tbl[t].type == FEMPTY)
      {
        curr_table->tbl[t].type = (ui8)EOF;
        --curr_table->free;
        --Ram->total_free;
        *indexp = t;
        return curr_table;
      }
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
  curr_table = GetRamTable();

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
  Ram->total_free += (FNUM_ENT - 1);

  /*-------------------------------------------------------------------*/
  /* Mark not empty, output entry index, and return table pointer.     */
  /*-------------------------------------------------------------------*/
  curr_table->tbl[0].type = (ui8)EOF;
  *indexp = 0;
  return curr_table;
}

/***********************************************************************/
/*    get_path: Get name of current working directory                  */
/*                                                                     */
/*      Inputs: buf = pointer to buffer or NULL                        */
/*              max = size of buffer, if provided                      */
/*              curr_dir = current working directory handle            */
/*                                                                     */
/***********************************************************************/
static char *get_path(char *buf, size_t max, RFSEnt *curr_dir)
{
  size_t size;
  int depth, i;
  char *rval;
  RDIR_T *dir = &curr_dir->entry.dir;

  /*-------------------------------------------------------------------*/
  /* Count directories between CWD and root, and determine path size.  */
  /*-------------------------------------------------------------------*/
  size = strlen(dir->name) + 2;
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
    if ((int)max < 0 || max < size + 1)
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
      return NULL;
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
  /* Add a last '/' and end the buffer with '\0' before returning.     */
  /*-------------------------------------------------------------------*/
  *buf++ = '/';
  *buf = '\0';
  return rval;
}

/***********************************************************************/
/*   set_fmode: Set access mode for a file in the RamFileTbl           */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              mode = string describing the access mode               */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
static int set_fmode(FILE *stream, const char *mode)
{
  RFIL_T *entry = &(((RFSEnt *)stream->handle)->entry.file);

  /*-------------------------------------------------------------------*/
  /* For read only, set curr_ptr to the beginning of the file.         */
  /*-------------------------------------------------------------------*/
  if (!strcmp(mode, "r") || !strcmp(mode, "rb"))
  {
    entry->comm->open_mode |= F_READ;
    stream->curr_ptr.sector = (ui32)entry->comm->frst_sect;
    stream->curr_ptr.offset = 0;
    stream->curr_ptr.sect_off = 0;
  }

  /*-------------------------------------------------------------------*/
  /* For write only mode, truncate file.                               */
  /*-------------------------------------------------------------------*/
  else if (!strcmp(mode, "w") || !strcmp(mode, "wb"))
  {
    entry->comm->open_mode |= F_WRITE;
    ram_trunc_zero(entry);
    stream->curr_ptr.sector = (ui32)NULL;
    stream->curr_ptr.offset = 0;
    stream->curr_ptr.sect_off = 0;
  }

  /*-------------------------------------------------------------------*/
  /* For append mode, set curr_ptr to the end of the file.             */
  /*-------------------------------------------------------------------*/
  else if (!strcmp(mode, "a") || !strcmp(mode, "ab"))
  {
    RamPoint2End(&stream->curr_ptr, entry->comm);
    entry->comm->open_mode |= F_APPEND;
  }

  /*-------------------------------------------------------------------*/
  /* For read/write, set curr_ptr to the end of the file.              */
  /*-------------------------------------------------------------------*/
  else if (!strcmp(mode, "r+") || !strcmp(mode, "rb+") ||
           !strcmp(mode, "r+b"))
  {
    stream->curr_ptr.sector = (ui32)entry->comm->frst_sect;
    stream->curr_ptr.offset = 0;
    stream->curr_ptr.sect_off = 0;
    entry->comm->open_mode |= (F_READ | F_WRITE);
  }

  /*-------------------------------------------------------------------*/
  /* For write/read, truncate file.                                    */
  /*-------------------------------------------------------------------*/
  else if (!strcmp(mode, "w+") || !strcmp(mode, "wb+") ||
           !strcmp(mode, "w+b"))
  {
    entry->comm->open_mode |= (F_READ | F_WRITE);
    ram_trunc_zero(entry);
    stream->curr_ptr.sector = (ui32)NULL;
    stream->curr_ptr.offset = 0;
    stream->curr_ptr.sect_off = 0;
  }

  /*-------------------------------------------------------------------*/
  /* For append/read, set curr_ptr to the end of the file.             */
  /*-------------------------------------------------------------------*/
  else if (!strcmp(mode, "a+") || !strcmp(mode, "ab+") ||
           !strcmp(mode, "a+b"))
  {
    RamPoint2End(&stream->curr_ptr, entry->comm);
    entry->comm->open_mode |= (F_READ | F_APPEND);
  }

  /*-------------------------------------------------------------------*/
  /* If mode is invalid, return error.                                 */
  /*-------------------------------------------------------------------*/
  else
  {
    set_errno(EINVAL);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Set old_ptr to the same value as curr_ptr when opening a file.    */
  /*-------------------------------------------------------------------*/
  stream->old_ptr = stream->curr_ptr;
  return 0;
}

/***********************************************************************/
/*     acquire: Acquire the RAM file system lock                       */
/*                                                                     */
/*      Inputs: handle = pointer to file control block                 */
/*              code   = if more than 1 sem, select which one          */
/*                                                                     */
/***********************************************************************/
static void acquire(const FILE *file, int code)
{
  semPend(RamSem, WAIT_FOREVER);
  Ram = file->volume;
}

/***********************************************************************/
/*     release: Release the RAM file system lock                       */
/*                                                                     */
/*      Inputs: handle = pointer to file control block                 */
/*              code   = if more than 1 sem, select which one          */
/*                                                                     */
/***********************************************************************/
static void release(const FILE *file, int code)
{
  Ram = NULL;
  semPost(RamSem);
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
static FILE *set_file(FILE *file, RFSEnt *entry, int set_stream)
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
  file->ioctl = RamIoctl;
  file->acquire = acquire;
  file->release = release;
  file->write = RamWrite;
  file->read = RamRead;
  file->errcode = 0;
  file->hold_char = '\0';
  file->cached = NULL;
  file->flags = FCB_FILE;

  return file;
}

/***********************************************************************/
/*    open_dir: open a directory                                       */
/*                                                                     */
/*       Input: dir = pointer to directory's file control block        */
/*              dir_ent = pointer to directory's file table entry      */
/*                                                                     */
/*     Returns: file control block pointer or NULL if error            */
/*                                                                     */
/***********************************************************************/
static void *open_dir(DIR *dir, RFSEnt *dir_ent)
{
  uid_t uid;
  gid_t gid;

  /*-------------------------------------------------------------------*/
  /* Check read permissions only if not owner is opening directory.    */
  /*-------------------------------------------------------------------*/
  FsGetId(&uid, &gid);
  if (uid != dir_ent->entry.dir.comm->user_id)
  {
    if (CheckPerm((void *)dir_ent->entry.dir.comm, F_READ))
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
  dir->ioctl = RamIoctl;
  dir->handle = dir_ent;
  return dir;
}

/***********************************************************************/
/*   rfs_fopen: Open a stream                                          */
/*                                                                     */
/*      Inputs: filename = pointer to path of file to open             */
/*              mode = pointer to a char string describing the mode    */
/*              d   = pointer to directory where file is to be open    */
/*              fcbp = pointer to file control block                   */
/*                                                                     */
/*     Returns: Pointer to file control block or NULL if open failed   */
/*                                                                     */
/***********************************************************************/
static FILE *rfs_fopen(const char *filename, const char *mode, RFSEnt *d,
                       FILE *fcbp)
{
  RFSEnt *entry, *v_ent;
  RFIL_T *fep;
  RDIR_T *dir = &d->entry.dir;
  FILE   *r_value = NULL;
  ui32    i, j;
  RFSEnts *curr_table1, *curr_table2;
  int     permissions = 0, set_stream = TRUE;

  /*-------------------------------------------------------------------*/
  /* Check that parent directory has search permissions.               */
  /*-------------------------------------------------------------------*/
  if (CheckPerm((void *)dir->comm, F_EXECUTE))
    return NULL;

  /*-------------------------------------------------------------------*/
  /* We're in right directory. Need to just add or open the file.      */
  /*-------------------------------------------------------------------*/
  for (entry = dir->first; entry; entry = entry->entry.dir.next)
  {
    /*-----------------------------------------------------------------*/
    /* Check if name matches name of existing file.                    */
    /*-----------------------------------------------------------------*/
    if (!strncmp(entry->entry.dir.name, filename, FILENAME_MAX))
      break;
  }

  /*-------------------------------------------------------------------*/
  /* If file exists, open it.                                          */
  /*-------------------------------------------------------------------*/
  if (entry)
  {
    /*-----------------------------------------------------------------*/
    /* If it's a directory, ensure mode is read-only and call opendir. */
    /*-----------------------------------------------------------------*/
    if (entry->type == FDIREN)
    {
      if (strcmp(mode, "r") && strcmp(mode, "rb"))
      {
        set_errno(EISDIR);
        return NULL;
      }
      return open_dir(fcbp, entry);
    }

    /*-----------------------------------------------------------------*/
    /* If there are any open links, check for mode compatibility.      */
    /*-----------------------------------------------------------------*/
    fep = &entry->entry.file;
    if (fep->comm->open_links)
    {
      /*---------------------------------------------------------------*/
      /* If mode is write/append, new mode should be read only.        */
      /*---------------------------------------------------------------*/
      if (fep->comm->open_mode & (F_APPEND | F_WRITE))
      {
        if (strcmp(mode, "r") && strcmp(mode, "rb"))
        {
          set_errno(EACCES);
          return NULL;
        }
        set_stream = FALSE;
      }

      /*---------------------------------------------------------------*/
      /* If new mode is write/append, mode should be read only.        */
      /*---------------------------------------------------------------*/
      if (fep->comm->open_mode & F_READ &&
          (!strcmp(mode, "w") || !strcmp(mode, "w+") ||
           !strcmp(mode, "wb") || !strcmp(mode, "w+b") ||
           !strcmp(mode, "wb+")))
      {
        set_errno(EACCES);
        return NULL;
      }
    }
    else
      fep->comm->open_mode = 0;

    /*-----------------------------------------------------------------*/
    /* Check for permissions for the file.                             */
    /*-----------------------------------------------------------------*/
    if (!strcmp(mode, "r") || !strcmp(mode, "rb"))
      permissions |= F_READ;
    if (!strcmp(mode, "w") || !strcmp(mode, "wb") ||
        !strcmp(mode, "a") || !strcmp(mode, "ab"))
      permissions |= F_WRITE;
    if (!strcmp(mode, "r+") || !strcmp(mode, "w+") ||
        !strcmp(mode, "a+") || !strcmp(mode, "r+b") ||
        !strcmp(mode, "rb+") || !strcmp(mode, "w+b") ||
        !strcmp(mode, "wb+") || !strcmp(mode, "a+b") ||
        !strcmp(mode, "ab+"))
      permissions |= (F_READ | F_WRITE);
    if (CheckPerm((void *)fep->comm, permissions))
      return NULL;

    /*-----------------------------------------------------------------*/
    /* If the file has no FILE*, associate one with it.                */
    /*-----------------------------------------------------------------*/
    if (fep->file == NULL)
      fep->file = fcbp;

    /*-----------------------------------------------------------------*/
    /* Initialize the file control block.                              */
    /*-----------------------------------------------------------------*/
    r_value = set_file(fcbp, entry, set_stream);

    /*-----------------------------------------------------------------*/
    /* Set the mode for the file and increase the number of links.     */
    /*-----------------------------------------------------------------*/
    if (set_fmode(fcbp, mode))
      return NULL;
    ++fep->comm->open_links;
  }

  /*-------------------------------------------------------------------*/
  /* Else if we're trying to create it with read only, return error.   */
  /*-------------------------------------------------------------------*/
  else if (!strcmp(mode, "r") || !strcmp(mode, "rb"))
    set_errno(EBADF);

  /*-------------------------------------------------------------------*/
  /* Else create the file in the directory.                            */
  /*-------------------------------------------------------------------*/
  else
  {
    /*-----------------------------------------------------------------*/
    /* Check the filename is valid.                                    */
    /*-----------------------------------------------------------------*/
    if (InvalidName(filename, FALSE))
      return NULL;

    /*-----------------------------------------------------------------*/
    /* Check for write permissions for parent directory.               */
    /*-----------------------------------------------------------------*/
    if (CheckPerm((void *)dir->comm, F_WRITE))
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
      ++Ram->total_free;
      return NULL;
    }

    /*-----------------------------------------------------------------*/
    /* Set the file entry in FlashFiletbl.                             */
    /*-----------------------------------------------------------------*/
    v_ent = &curr_table2->tbl[j];
    v_ent->type = FCOMMN;
    v_ent->entry.comm.addr = v_ent;
    v_ent->entry.comm.frst_sect = NULL;
    v_ent->entry.comm.last_sect = NULL;
    v_ent->entry.comm.one_past_last = 0;
    v_ent->entry.comm.size = 0;
    v_ent->entry.comm.links = 1;
    v_ent->entry.comm.open_links = 1;
    v_ent->entry.comm.open_mode = 0;
    v_ent->entry.comm.ac_time = v_ent->entry.comm.mod_time = OsSecCount;
    v_ent->entry.comm.fileno = Ram->fileno_gen++;
    v_ent->entry.comm.temp = FALSE;

    /*-----------------------------------------------------------------*/
    /* Set the link entry in the files table.                          */
    /*-----------------------------------------------------------------*/
    v_ent = &curr_table1->tbl[i];
    v_ent->type = FFILEN;
    v_ent->entry.file.parent_dir = d;
    v_ent->entry.file.comm = &curr_table2->tbl[j].entry.comm;
    strncpy(v_ent->entry.file.name, filename, FILENAME_MAX);

    /*-----------------------------------------------------------------*/
    /* Initialize the file control block.                              */
    /*-----------------------------------------------------------------*/
    r_value = set_file(fcbp, v_ent, TRUE);

    /*-----------------------------------------------------------------*/
    /* Set the mode for the entry in the files table.                  */
    /*-----------------------------------------------------------------*/
    if (set_fmode(fcbp, mode))
      return NULL;

    /*-----------------------------------------------------------------*/
    /* Set the permissions for the file.                               */
    /*-----------------------------------------------------------------*/
    SetPerm((void *)v_ent->entry.file.comm, S_IRUSR | S_IWUSR | S_IXUSR);

    /*-----------------------------------------------------------------*/
    /* Add new link to its parent directory.                           */
    /*-----------------------------------------------------------------*/
    entry = dir->first;
    dir->first = v_ent;
    v_ent->entry.file.next = entry;
    v_ent->entry.file.prev = NULL;
    if (entry)
      entry->entry.dir.prev = v_ent;
  }
  return r_value;
}

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/* RamPoin2End: Point file entry's position indicator to file's end    */
/*                                                                     */
/*      Inputs: pos_ptr = ptr to file control block position record    */
/*              file_entry = ptr to TargetRFS table file entry         */
/*                                                                     */
/***********************************************************************/
void RamPoint2End(fpos_t *pos_ptr, const RCOM_T *file_entry)
{
  pos_ptr->sector = (ui32)file_entry->last_sect;
  pos_ptr->sect_off = (file_entry->size - file_entry->one_past_last)
                    / RAM_SECT_SZ;
  pos_ptr->offset = file_entry->one_past_last;
}

/***********************************************************************/
/* RamSeekPastEnd: Extend a file size with 0 value bytes               */
/*                                                                     */
/*      Inputs: offset = length of fseek in bytes                      */
/*              stream = pointer to file control block (FILE)          */
/*                                                                     */
/*     Returns: NULL on success, (void *)-1 on error                   */
/*                                                                     */
/***********************************************************************/
void *RamSeekPastEnd(i32 offset, FILE *stream)
{
  RCOM_T *file = ((RFSEnt *)stream->handle)->entry.file.comm;
  ui32 num_sects, i, one_past_last;
  ui8 *sector;
  int adjust_old_ptr = (file->size == 0);

  /*-------------------------------------------------------------------*/
  /* If in read only mode, can't seek past file end unless offset is 0.*/
  /*-------------------------------------------------------------------*/
  if (!(file->open_mode & (F_APPEND | F_WRITE)) && offset)
  {
    set_errno(EACCES);
    return (void *)-1;
  }

  /*-------------------------------------------------------------------*/
  /* If file is not empty, use as much from last sector as possible.   */
  /*-------------------------------------------------------------------*/
  if (file->size)
  {
    sector = ((RamSect *)file->last_sect)->data;

    /*-----------------------------------------------------------------*/
    /* If seek fits within this sector stop here.                      */
    /*-----------------------------------------------------------------*/
    if (file->one_past_last + offset <= RAM_SECT_SZ)
    {
      /*---------------------------------------------------------------*/
      /* Zero out the new part of the file.                            */
      /*---------------------------------------------------------------*/
      memset(&sector[file->one_past_last], 0, offset);

      /*---------------------------------------------------------------*/
      /* Adjust offset for one past last and file size.                */
      /*---------------------------------------------------------------*/
      file->one_past_last += offset;
      file->size += (ui32)offset;
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
      memset(&sector[file->one_past_last], 0,
             RAM_SECT_SZ - file->one_past_last);

      /*---------------------------------------------------------------*/
      /* Adjust file size.                                             */
      /*---------------------------------------------------------------*/
      file->size += (RAM_SECT_SZ - file->one_past_last);
      offset -= (RAM_SECT_SZ - file->one_past_last);
    }
  }

  /*-------------------------------------------------------------------*/
  /* Figure out how many new sectors we need.                          */
  /*-------------------------------------------------------------------*/
  num_sects = (ui32)(offset + RAM_SECT_SZ - 1) / RAM_SECT_SZ;
  one_past_last = offset % RAM_SECT_SZ;
  if (num_sects && one_past_last == 0)
    one_past_last = RAM_SECT_SZ;

  /*-------------------------------------------------------------------*/
  /* Try to assign num_sects worth of free sectors to the file.        */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < num_sects; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* Get new sector.                                                 */
    /*-----------------------------------------------------------------*/
    if (RamNewSect(file))
      goto err_return;

    /*-----------------------------------------------------------------*/
    /* Increment file size by sector size.                             */
    /*-----------------------------------------------------------------*/
    if (i != num_sects - 1)
      file->size += RAM_SECT_SZ;

    sector = ((RamSect *)file->last_sect)->data;

    /*-----------------------------------------------------------------*/
    /* Zero out all sector if not last one and only offset if last.    */
    /*-----------------------------------------------------------------*/
    if (i == num_sects - 1)
      memset(sector, 0, one_past_last);
    else
      memset(sector, 0, RAM_SECT_SZ);
  }

  /*-------------------------------------------------------------------*/
  /* Increment file size by one_past_last.                             */
  /*-------------------------------------------------------------------*/
  file->size += one_past_last;
  file->one_past_last = one_past_last;

  /*-------------------------------------------------------------------*/
  /* Adjust curr_ptr and return.                                       */
  /*-------------------------------------------------------------------*/
  if (adjust_old_ptr)
    stream->old_ptr = stream->curr_ptr;
  return NULL;

err_return:
  file->one_past_last = 0;
  return (void *)-1;
}

/***********************************************************************/
/*    RamIoctl: Perform multiple file functions for RAM file system    */
/*                                                                     */
/*      Inputs: stream = holds either file or dir ctrl block ptr       */
/*              code = selects the function to be performed            */
/*                                                                     */
/*     Returns: The value of the function called                       */
/*                                                                     */
/***********************************************************************/
void *RamIoctl(FILE *stream, int code, ...)
{
  void *vp = NULL;
  va_list ap;
  RFSEnts *curr_table, *curr_table1, *curr_table2;
  char *name;
  RFSEnt *dir_ent, *tmp_ent, *v_ent;
  RFIL_T *link_ptr;
  RDIR_T *dir;
  ui32 i, j, k, end;
  mode_t mode;
  uid_t uid;
  gid_t gid;
  RamGlob *vol;
  RamSect *sect;

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
        tmp_ent = va_arg(ap, RFSEnt *);
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
      /* Handle getcwd() for the RAM file system.                      */
      /*---------------------------------------------------------------*/
      case GETCWD:
      {
        char *buf;
        size_t size;
        RFSEnt *curr_dir;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        buf = va_arg(ap, char *);
        size = va_arg(ap, size_t);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Get exclusive access to the RAM file system.                */
        /*-------------------------------------------------------------*/
        semPend(RamSem, WAIT_FOREVER);

        /*-------------------------------------------------------------*/
        /* If there is a current working directory get its name, else  */
        /* return error.                                               */
        /*-------------------------------------------------------------*/
        FsReadCWD((void *)&Ram, (void *)&curr_dir);
        if (curr_dir)
          buf = get_path(buf, size, curr_dir);
        else
        {
          set_errno(EINVAL);
          buf = NULL;
        }
        semPost(RamSem);
        return buf;
      }

      /*---------------------------------------------------------------*/
      /* Handle vstat() for the RAM file system.                       */
      /*---------------------------------------------------------------*/
      case VSTAT:
      {
        union vstat *buf;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        vol = (RamGlob *)va_arg(ap, void *);
        buf = va_arg(ap, union vstat *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Get exclusive access to the flash file system.              */
        /*-------------------------------------------------------------*/
        semPend(RamSem, WAIT_FOREVER);

        /*-------------------------------------------------------------*/
        /* Assign total # sectors, # sectors for data, and sector size.*/
        /*-------------------------------------------------------------*/
        buf->rfs.vol_type = RFS_VOL;
        buf->rfs.num_sects = vol->num_sects;
        buf->rfs.sect_size = RAM_SECT_SZ;

        /*-------------------------------------------------------------*/
        /* Release exclusive access and return success.                */
        /*-------------------------------------------------------------*/
        semPost(RamSem);
        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle tmpnam() for the RAM file system.                      */
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
      /* Handle tmpfile() for the RAM file system.                     */
      /*---------------------------------------------------------------*/
      case TMPFILE:
        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get arguments.                  */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        Ram = va_arg(ap, void *);
        name = va_arg(ap, char *);
        va_end(ap);

        dir = &(Ram->files_tbl->tbl[0]).entry.dir;

        /*-------------------------------------------------------------*/
        /* Check that parent directory has write, execute permissions. */
        /*-------------------------------------------------------------*/
        if (CheckPerm((void *)dir->comm, F_WRITE | F_EXECUTE))
        {
          set_errno(EACCES);
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* The file does not exist in the directory so need to make an */
        /* entry for the link.                                         */
        /*-------------------------------------------------------------*/
        curr_table1 = find_entry(&i);

        /*-------------------------------------------------------------*/
        /* If unable to obtain free entry, return error.               */
        /*-------------------------------------------------------------*/
        if (curr_table1 == NULL)
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* Find a free entry for the actual file.                      */
        /*-------------------------------------------------------------*/
        curr_table2 = find_entry(&j);

        /*-------------------------------------------------------------*/
        /* If unable to obtain free entry, return error.               */
        /*-------------------------------------------------------------*/
        if (curr_table2 == NULL)
        {
          curr_table1->tbl[i].type = FEMPTY;
          ++curr_table1->free;
          ++Ram->total_free;
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* Return error if all file control blocks are in use.         */
        /*-------------------------------------------------------------*/
        for (k = 0; Files[k].ioctl; ++k)
        {
          if (k == FOPEN_MAX - 1)
          {
            set_errno(EMFILE);
            curr_table1->tbl[i].type = FEMPTY;
            curr_table2->tbl[j].type = FEMPTY;
            ++curr_table1->free;
            ++curr_table2->free;
            Ram->total_free += 2;
            return (void *)-1;
          }
        }

        /*-------------------------------------------------------------*/
        /* Set the file entry in files table.                          */
        /*-------------------------------------------------------------*/
        v_ent = &curr_table2->tbl[j];
        v_ent->type = FCOMMN;
        v_ent->entry.comm.addr = v_ent;
        v_ent->entry.comm.frst_sect = NULL;
        v_ent->entry.comm.last_sect = NULL;
        v_ent->entry.comm.one_past_last = 0;
        v_ent->entry.comm.size = 0;
        v_ent->entry.comm.temp = TRUE;
        v_ent->entry.comm.links = 1;
        v_ent->entry.comm.open_links = 1;
        v_ent->entry.comm.fileno = Ram->fileno_gen++;
        v_ent->entry.comm.ac_time =v_ent->entry.comm.mod_time=OsSecCount;

        /*-------------------------------------------------------------*/
        /* Set the FILE* associated with this opened file.             */
        /*-------------------------------------------------------------*/
        Files[k].curr_ptr.sector = (ui32)NULL;
        Files[k].curr_ptr.offset = 0;
        Files[k].curr_ptr.sect_off = 0;
        Files[k].old_ptr = Files[k].curr_ptr;

        /*-------------------------------------------------------------*/
        /* Set the link entry in the files table.                      */
        /*-------------------------------------------------------------*/
        v_ent = &curr_table1->tbl[i];
        v_ent->type = FFILEN;
        v_ent->entry.file.parent_dir = &Ram->files_tbl->tbl[0];
        v_ent->entry.file.comm = &curr_table2->tbl[j].entry.comm;
        strncpy(v_ent->entry.file.name, name, FILENAME_MAX);

        /*-------------------------------------------------------------*/
        /* Initialize the file control block.                          */
        /*-------------------------------------------------------------*/
        vp = set_file(&Files[k], v_ent, TRUE);
        Files[k].volume = Ram;

        /*-------------------------------------------------------------*/
        /* For temp files, always open them with "w+" mode.            */
        /*-------------------------------------------------------------*/
        set_fmode(&Files[k], "w+");

        /*-------------------------------------------------------------*/
        /* Set the permissions for the file.                           */
        /*-------------------------------------------------------------*/
        SetPerm((void *)v_ent->entry.file.comm, 0700);

        /*-------------------------------------------------------------*/
        /* Add new link to its parent directory.                       */
        /*-------------------------------------------------------------*/
        tmp_ent = dir->first;
        dir->first = v_ent;
        v_ent->entry.file.next = tmp_ent;
        v_ent->entry.file.prev = NULL;
        if (tmp_ent)
          tmp_ent->entry.dir.prev = v_ent;

        /*-------------------------------------------------------------*/
        /* Return the pointer to the stream.                           */
        /*-------------------------------------------------------------*/
        return &Files[k];

      /*---------------------------------------------------------------*/
      /* Handle unmount() for the RAM file system.                     */
      /*---------------------------------------------------------------*/
      case UNMOUNT:
      {
        FsVolume *volume;
        ui32 dummy;

        /*-------------------------------------------------------------*/
        /* Get exclusive access to the RAM file system.                */
        /*-------------------------------------------------------------*/
        semPend(RamSem, WAIT_FOREVER);

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get pointer to volume ctrls.    */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        Ram = (RamGlob *)va_arg(ap, void *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Make sure that all FILE*s and DIR*s associated with this    */
        /* volume are freed.                                           */
        /*-------------------------------------------------------------*/
        for (i = 0; i < FOPEN_MAX; ++i)
        {
          /*-----------------------------------------------------------*/
          /* If a FILE/DIR is associated with this volume, clear it.   */
          /*-----------------------------------------------------------*/
          if (Files[i].ioctl == RamIoctl)
          {
            if (Files[i].flags & FCB_FILE)
              close_file(Files[i].handle, &Files[i]);
            else
              close_dir(Files[i].handle, &Files[i]);
            FsInitFCB(&Files[i], FCB_FILE);
          }
        }

        /*-------------------------------------------------------------*/
        /* If ioctl pointer is set to this volume, reset it to root.   */
        /*-------------------------------------------------------------*/
        FsReadCWD((void *)&volume, &dummy);
        if (Ram == (void *)volume)
          FsSaveCWD((ui32)NULL, (ui32)NULL);

        /*-------------------------------------------------------------*/
        /* Release RAM file system exclusive access.                   */
        /*-------------------------------------------------------------*/
        semPost(RamSem);
        break;
      }

      /*---------------------------------------------------------------*/
      /* Handle FSUID to name mapping for the RAM file system.         */
      /*---------------------------------------------------------------*/
      case GET_NAME:
        return NULL;

      default:
        break;
    }
  }
  else
  {
    void *handle = stream->handle;
    switch (code)
    {
      /*---------------------------------------------------------------*/
      /* Handle fcntl(,FSET_FL,..) for the RAM file system.            */
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
        link_ptr = &((RFSEnt *)stream->handle)->entry.file;
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
      /* Handle fcntl(,FGET_FL,..) for the RAM file system.            */
      /*---------------------------------------------------------------*/
      case GET_FL:
      {
        int oflag = 0;

        /*-------------------------------------------------------------*/
        /* Get the open flag for the descriptor.                       */
        /*-------------------------------------------------------------*/
        link_ptr = &((RFSEnt *)stream->handle)->entry.file;
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
      /* Handle fstat() for the RAM file system.                       */
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
        statistics(buf, (RFSEnt *)handle);
        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle ftruncate() for the RAM file system.                   */
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
        return truncatef((RFSEnt *)handle, length);
      }

      /*---------------------------------------------------------------*/
      /* Handle truncate() for the RAM file system.                    */
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
        dir_ent = va_arg(ap, RFSEnt *);
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
        return truncatef(tmp_ent, length);
      }

      /*---------------------------------------------------------------*/
      /* Handle readdir() for the RAM file system.                     */
      /*---------------------------------------------------------------*/
      case READDIR:
      {
        DIR *dirp = ((RFSEnt *)handle)->entry.dir.dir;

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
          if (((RFSEnt *)dirp->handle)->entry.dir.first)
          {
            dirp->pos = ((RFSEnt *)dirp->handle)->entry.dir.first;
            strncpy(dirp->dent.d_name,
                    ((RFSEnt *)dirp->pos)->entry.dir.name,
                    FILENAME_MAX);
            return &dirp->dent;
          }
        }

        /*-------------------------------------------------------------*/
        /* Else if the position indicator does not point to the last   */
        /* entry, move to the next entry in the directory.             */
        /*-------------------------------------------------------------*/
        else if(((RFSEnt *)dirp->pos)->entry.dir.next)
        {
          dirp->pos = ((RFSEnt *)dirp->pos)->entry.dir.next;
          strncpy(dirp->dent.d_name,
                  ((RFSEnt *)dirp->pos)->entry.dir.name,
                  FILENAME_MAX);
          return &dirp->dent;
        }
        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle closedir() for the RAM file system.                    */
      /*---------------------------------------------------------------*/
      case CLOSEDIR:
        /*-------------------------------------------------------------*/
        /* Close the directory.                                        */
        /*-------------------------------------------------------------*/
        close_dir(handle, stream);

        /*-------------------------------------------------------------*/
        /* Decrement the opendir() count and garbage collect if needed.*/
        /*-------------------------------------------------------------*/
        if (--Ram->opendirs == 0)
        {
          if (Ram->total_free >= (FNUM_ENT * 5 / 3))
            garbageCollect();
        }

        return NULL;

      /*---------------------------------------------------------------*/
      /* Handle the duplication functions for the RAM file system.     */
      /*---------------------------------------------------------------*/
      case DUP:
        /*-------------------------------------------------------------*/
        /* Simply increment the number of open links for this file.    */
        /*-------------------------------------------------------------*/
        ++((RFSEnt *)handle)->entry.file.comm->open_links;
        return NULL;

      /*---------------------------------------------------------------*/
      /* Handle open() for the RAM file system.                        */
      /*---------------------------------------------------------------*/
      case OPEN:
      {
        int oflag, permissions = 0, set_stream = TRUE;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        oflag = va_arg(ap, int);
        mode = (mode_t)va_arg(ap, int);
        dir_ent = va_arg(ap, RFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Set pointer to the file's parent directory.                 */
        /*-------------------------------------------------------------*/
        dir = &dir_ent->entry.dir;

        /*-------------------------------------------------------------*/
        /* Look for the file in the directory.                         */
        /*-------------------------------------------------------------*/
        if (find_file(dir, name, F_EXECUTE, &tmp_ent, F_AND_D))
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* Check if file exists.                                       */
        /*-------------------------------------------------------------*/
        if (tmp_ent)
        {
          /*-----------------------------------------------------------*/
          /* If oflag is set to exclusive, return error.               */
          /*-----------------------------------------------------------*/
          if ((oflag & O_CREAT) && (oflag & O_EXCL))
          {
            set_errno(EEXIST);
            return (void *)-1;
          }

          /*-----------------------------------------------------------*/
          /* If it's a directory, check mode is not RDWR or WRONLY.    */
          /*-----------------------------------------------------------*/
          if (tmp_ent->type == FDIREN)
          {
            if (oflag & (O_RDWR | O_WRONLY))
            {
              set_errno(EISDIR);
              return (void *)-1;
            }
            return open_dir(stream, tmp_ent);
          }

          /*-----------------------------------------------------------*/
          /* If there are any open links, check for mode compatibility.*/
          /*-----------------------------------------------------------*/
          link_ptr = &tmp_ent->entry.file;
          if (link_ptr->comm->open_links)
          {
            /*---------------------------------------------------------*/
            /* If mode is write/append, new mode should be read only.  */
            /*---------------------------------------------------------*/
            if (link_ptr->comm->open_mode & (F_APPEND | F_WRITE))
            {
              if (oflag & (O_RDWR | O_WRONLY))
              {
                set_errno(EACCES);
                return (void *)-1;
              }
              set_stream = FALSE;
            }

            /*---------------------------------------------------------*/
            /* If new mode is write/append, mode should be read only.  */
            /*---------------------------------------------------------*/
            if ((oflag & (O_RDWR | O_WRONLY)) &&
                (link_ptr->comm->open_mode != F_READ))
            {
              set_errno(EACCES);
              return (void *)-1;
            }
          }
          else
            link_ptr->comm->open_mode = 0;

          /*-----------------------------------------------------------*/
          /* Check for permissions for the file.                       */
          /*-----------------------------------------------------------*/
          if (oflag & O_WRONLY)
            permissions = F_WRITE;
          else if (oflag & O_RDWR)
            permissions = F_WRITE | F_READ;
          else
            permissions = F_READ;
          if (CheckPerm((void *)link_ptr->comm, permissions))
            return (void *)-1;

          /*-----------------------------------------------------------*/
          /* If the file has no FILE*, associate one with it.          */
          /*-----------------------------------------------------------*/
          if (link_ptr->file == NULL)
            link_ptr->file = stream;

          /*-----------------------------------------------------------*/
          /* Set the file control block.                               */
          /*-----------------------------------------------------------*/
          vp = set_file(stream, tmp_ent, set_stream);

          /*-----------------------------------------------------------*/
          /* If file is not empty, set curr_ptr to point to beginning. */
          /*-----------------------------------------------------------*/
          if (link_ptr->comm && (link_ptr->comm->frst_sect != NULL))
          {
            stream->curr_ptr.sector = (ui32)link_ptr->comm->frst_sect;
            stream->curr_ptr.offset = 0;
          }
          else
            stream->curr_ptr.sector = (ui32)NULL;

          /*-----------------------------------------------------------*/
          /* sect_off will be 0 regardless of wether file is empty.    */
          /*-----------------------------------------------------------*/
          stream->curr_ptr.sect_off = 0;

          /*-----------------------------------------------------------*/
          /* When opening a file, set old_ptr to curr_ptr.             */
          /*-----------------------------------------------------------*/
          stream->old_ptr = stream->curr_ptr;

          /*-----------------------------------------------------------*/
          /* Set file mode to either read, write or append or a combo. */
          /*-----------------------------------------------------------*/
          if (oflag & O_WRONLY)
          {
            if (oflag & O_APPEND)
              link_ptr->comm->open_mode |= F_APPEND;
            else
              link_ptr->comm->open_mode |= F_WRITE;
          }
          else if (oflag & O_RDWR)
          {
            if (oflag & O_APPEND)
              link_ptr->comm->open_mode |= (F_APPEND | F_READ);
            else
              link_ptr->comm->open_mode |= (F_WRITE | F_READ);
          }
          else
            link_ptr->comm->open_mode |= F_READ;

          /*-----------------------------------------------------------*/
          /* Increment the number of links for the file.               */
          /*-----------------------------------------------------------*/
          ++link_ptr->comm->open_links;

          /*-----------------------------------------------------------*/
          /* If truncate flag is set, truncate file before write.      */
          /*-----------------------------------------------------------*/
          if ((oflag & O_TRUNC) &&
              (link_ptr->comm->open_mode & (F_APPEND | F_WRITE)))
            ram_trunc_zero(link_ptr);
        }

        /*-------------------------------------------------------------*/
        /* Else file does not exist.                                   */
        /*-------------------------------------------------------------*/
        else
        {
          /*-----------------------------------------------------------*/
          /* If file should not be created, return error.              */
          /*-----------------------------------------------------------*/
          if (!(oflag & O_CREAT))
          {
            set_errno(ENOENT);
            return (void *)-1;
          }

          /*-----------------------------------------------------------*/
          /* Check write permissions for parent directory.             */
          /*-----------------------------------------------------------*/
          if (CheckPerm((void *)dir->comm, F_WRITE))
            return (void *)-1;

          /*-----------------------------------------------------------*/
          /* First find room for the link.                             */
          /*-----------------------------------------------------------*/
          curr_table1 = find_entry(&i);

          /*-----------------------------------------------------------*/
          /* If no more free entries, stop.                            */
          /*-----------------------------------------------------------*/
          if (curr_table1 == NULL)
            return (void *)-1;

          /*-----------------------------------------------------------*/
          /* Now find room for the file.                               */
          /*-----------------------------------------------------------*/
          curr_table2 = find_entry(&j);

          /*-----------------------------------------------------------*/
          /* If no more free entries, stop.                            */
          /*-----------------------------------------------------------*/
          if (curr_table2 == NULL)
          {
            curr_table1->tbl[i].type = FEMPTY;
            ++curr_table1->free;
            ++Ram->total_free;
            return (void *)-1;
          }

          /*-----------------------------------------------------------*/
          /* Initialize the file control block.                        */
          /*-----------------------------------------------------------*/
          vp = set_file(stream, &(curr_table1->tbl[i]), TRUE);

          /*-----------------------------------------------------------*/
          /* Set the file entry in Flash->files_tbl.                   */
          /*-----------------------------------------------------------*/
          v_ent = &curr_table2->tbl[j];
          v_ent->type = FCOMMN;
          v_ent->entry.comm.addr = v_ent;
          v_ent->entry.comm.frst_sect = NULL;
          v_ent->entry.comm.last_sect = NULL;
          v_ent->entry.comm.one_past_last = 0;
          v_ent->entry.comm.size = 0;
          v_ent->entry.comm.temp = FALSE;
          v_ent->entry.comm.links = 1;
          v_ent->entry.comm.open_links = 1;
          v_ent->entry.comm.fileno = Ram->fileno_gen++;

          /*-----------------------------------------------------------*/
          /* Set the correct mode for the file entry.                  */
          /*-----------------------------------------------------------*/
          if (oflag & O_WRONLY)
          {
            if (oflag & O_APPEND)
              v_ent->entry.comm.open_mode = F_APPEND;
            else
              v_ent->entry.comm.open_mode = F_WRITE;
          }
          else if (oflag & O_RDWR)
          {
            if (oflag & O_APPEND)
              v_ent->entry.comm.open_mode = F_APPEND | F_READ;
            else
              v_ent->entry.comm.open_mode = F_WRITE | F_READ;
          }
          else
            v_ent->entry.comm.open_mode = F_READ;

          /*-----------------------------------------------------------*/
          /* Set the access and modified times.                        */
          /*-----------------------------------------------------------*/
          v_ent->entry.comm.ac_time = v_ent->entry.comm.mod_time =
                                                              OsSecCount;

          /*-----------------------------------------------------------*/
          /* Set the FILE* associated with this opened file.           */
          /*-----------------------------------------------------------*/
          stream->curr_ptr.sector = (ui32)NULL;
          stream->curr_ptr.offset = 0;
          stream->curr_ptr.sect_off = 0;
          stream->old_ptr = stream->curr_ptr;

          /*-----------------------------------------------------------*/
          /* Set the link entry in Flash->files_tbl.                   */
          /*-----------------------------------------------------------*/
          v_ent = &curr_table1->tbl[i];
          v_ent->type = FFILEN;
          strncpy(v_ent->entry.file.name, name, FILENAME_MAX);
          v_ent->entry.file.comm = &(curr_table2->tbl[j].entry.comm);
          v_ent->entry.file.parent_dir = dir_ent;
          v_ent->entry.file.file = (FILE*)vp;

          /*-----------------------------------------------------------*/
          /* Set the permissions for the file.                         */
          /*-----------------------------------------------------------*/
          SetPerm((void *)v_ent->entry.file.comm, mode);

          /*-----------------------------------------------------------*/
          /* Add the new link to its parent directory.                 */
          /*-----------------------------------------------------------*/
          tmp_ent = dir->first;
          dir->first = v_ent;
          v_ent->entry.dir.next = tmp_ent;
          v_ent->entry.dir.prev = NULL;
          if (tmp_ent)
            tmp_ent->entry.dir.prev = v_ent;

          return NULL;
        }
        break;
      }

      /*---------------------------------------------------------------*/
      /* Handle fclose() for the RAM file system.                      */
      /*---------------------------------------------------------------*/
      case FCLOSE:
        /*-------------------------------------------------------------*/
        /* If type is directory, close the directory.                  */
        /*-------------------------------------------------------------*/
        if (((RFSEnt *)handle)->type == FDIREN)
          close_dir(handle, stream);

        /*-------------------------------------------------------------*/
        /* Else close regular file.                                    */
        /*-------------------------------------------------------------*/
        else
          return close_file(handle, stream);
        break;

      /*---------------------------------------------------------------*/
      /* Handle feof() for the RAM file system.                        */
      /*---------------------------------------------------------------*/
      case FEOF:
      {
        RFIL_T *file = &(((RFSEnt *)handle)->entry.file);

        if (!handle || (((RFSEnt *)handle)->type != FFILEN) ||
            stream->curr_ptr.sector == SEEK_PAST_END ||
            (stream->curr_ptr.sector == (ui32)file->comm->last_sect &&
             stream->curr_ptr.offset == file->comm->one_past_last))
          vp = (void *)1;
        break;
      }

      /*---------------------------------------------------------------*/
      /* Handle fgetpos() for the RAM file system.                     */
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
        if (!handle || (((RFSEnt *)handle)->type != FFILEN))
          vp = (void *)1;
        else
        {
          fp->sector = stream->curr_ptr.sector;
          fp->offset = stream->curr_ptr.offset;
          fp->sect_off = stream->curr_ptr.sect_off;
        }
        break;
      }

      /*---------------------------------------------------------------*/
      /* Handle fseek() for the RAM file system.                       */
      /*---------------------------------------------------------------*/
      case FSEEK:
      {
        long offst;
        i32 offset, curr_pos;
        int whence;
        link_ptr = &((RFSEnt *)handle)->entry.file;

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
          /* In case the file is empty, force it to perform SEEK_SET,  */
          /* since it's all the same and it avoids code duplication.   */
          /*-----------------------------------------------------------*/
          if (link_ptr->comm->frst_sect == NULL)
            whence = SEEK_SET;

          /*-----------------------------------------------------------*/
          /* Handle seek from beginning of file going towards the end. */
          /*-----------------------------------------------------------*/
          if (whence == SEEK_SET)
          {
            /*---------------------------------------------------------*/
            /* If file is empty, call seek_past_end() directly.        */
            /*---------------------------------------------------------*/
            if (link_ptr->comm->frst_sect == NULL)
              seek_past_end(offset, stream);

            /*---------------------------------------------------------*/
            /* Else perform seek within or past the end of the file.   */
            /*---------------------------------------------------------*/
            else
            {
              /*-------------------------------------------------------*/
              /* If the seek goes beyond the end of the file, seek past*/
              /* what's left after subtracting the file size.          */
              /*-------------------------------------------------------*/
              if (link_ptr->comm->size < (ui32)offset)
              {
                offset -= (i32)link_ptr->comm->size;
                seek_past_end(offset, stream);
              }

              /*-------------------------------------------------------*/
              /* Else the seek is within the file.                     */
              /*-------------------------------------------------------*/
              else
              {
                /*-----------------------------------------------------*/
                /* Figure out the current position.                    */
                /*-----------------------------------------------------*/
                curr_pos = (i32)stream->curr_ptr.sect_off *
                           RAM_SECT_SZ + stream->curr_ptr.offset;
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
            curr_pos = (i32)stream->curr_ptr.sect_off * RAM_SECT_SZ +
                       stream->curr_ptr.offset;

            /*---------------------------------------------------------*/
            /* If seek goes beyond the end of the file, do seek past   */
            /* end on what's left after subtracting the difference     */
            /* between the current position and the end of file first. */
            /*---------------------------------------------------------*/
            if (curr_pos + offset > (i32)link_ptr->comm->size)
            {
              offset -= ((i32)link_ptr->comm->size - curr_pos);
              seek_past_end(offset, stream);
            }

            /*---------------------------------------------------------*/
            /* Else the seek is within the file                        */
            /*---------------------------------------------------------*/
            else
              vp = seek_within_file(stream, offset + curr_pos, curr_pos);
          }

          /*-----------------------------------------------------------*/
          /* Handle the seek from the end of file going beyond it, i.e.*/
          /* expanding the file.                                       */
          /*-----------------------------------------------------------*/
          else if (whence == SEEK_END)
            seek_past_end(offset, stream);

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
          if (link_ptr->comm->frst_sect == NULL)
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
            curr_pos = (i32)stream->curr_ptr.sect_off * RAM_SECT_SZ +
                       stream->curr_ptr.offset;

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
            else if (stream->curr_ptr.sector == SEEK_PAST_END)
            {
              RCOM_T *file = ((RFSEnt *)stream->handle)->entry.file.comm;
              i32 past_end = (i32)(stream->curr_ptr.sect_off *
                                   RAM_SECT_SZ +
                                   stream->curr_ptr.offset -
                                   file->size);

              /*-------------------------------------------------------*/
              /* Determine if we are still past the EOF.               */
              /*-------------------------------------------------------*/
              if (past_end > offset)
                seek_past_end(past_end - offset, stream);
              else
              {
                offset = file->size - (offset - past_end);
                seek_within_file(stream, offset, file->size);
              }
            }

            /*---------------------------------------------------------*/
            /* Else adjust offset and perform seek up.                 */
            /*---------------------------------------------------------*/
            else
            {
              offset = (curr_pos - offset);
              seek_within_file(stream, offset, curr_pos);
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
              curr_pos = (i32)stream->curr_ptr.sect_off *
                         RAM_SECT_SZ + stream->curr_ptr.offset;

              offset = ((i32)link_ptr->comm->size - offset);
              seek_within_file(stream, offset, curr_pos);
            }
          }
        }

        /*-------------------------------------------------------------*/
        /* If seek hasn't failed, figure out the new offset.           */
        /*-------------------------------------------------------------*/
        if (vp == NULL)
        {
          /*-----------------------------------------------------------*/
          /* If file is not empty, use curr_ptr to figure new offset   */
          /*-----------------------------------------------------------*/
          if (link_ptr->comm->frst_sect != NULL)
            vp = (void *)(stream->curr_ptr.sect_off * RAM_SECT_SZ +
                          stream->curr_ptr.offset);
        }
        break;
      }

      /*---------------------------------------------------------------*/
      /* Handle fsetpos() for the RAM file system.                     */
      /*---------------------------------------------------------------*/
      case FSETPOS:
      {
        fpos_t *pos;

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
        link_ptr = &((RFSEnt *)handle)->entry.file;
        if (link_ptr->comm->frst_sect)
        {
          /*-----------------------------------------------------------*/
          /* If position past EOF, set current pointer past EOF.       */
          /*-----------------------------------------------------------*/
          if (pos->sector == SEEK_PAST_END)
          {
            /*---------------------------------------------------------*/
            /* If position still past EOF, set it.                     */
            /*---------------------------------------------------------*/
            if (link_ptr->comm->size < pos->sect_off * RAM_SECT_SZ +
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
              RamSect *sector = (RamSect *)link_ptr->comm->frst_sect;
              int count;

              for (count = 0; count < pos->sect_off; ++count)
                sector = sector->next;
              stream->curr_ptr.sector = (ui32)sector;
              stream->curr_ptr.offset = pos->offset;
              stream->curr_ptr.sect_off = pos->sect_off;
            }
            vp = NULL;
          }

          /*-----------------------------------------------------------*/
          /* Else, check position is valid and set current pointer.    */
          /*-----------------------------------------------------------*/
          else
          {
            /*---------------------------------------------------------*/
            /* Look for a sector from this file to match pos->sector.  */
            /*---------------------------------------------------------*/
            for (sect = link_ptr->comm->frst_sect; sect; sect=sect->next)
              if (sect == (RamSect *)pos->sector)
                break;

            /*---------------------------------------------------------*/
            /* If such a sector exists, check if offset is valid.      */
            /*---------------------------------------------------------*/
            if ((sect != NULL) && (pos->offset < RAM_SECT_SZ))
            {
              /*-------------------------------------------------------*/
              /* If it's last sector, make sure offset is not bigger   */
              /* than the end of file location.                        */
              /*-------------------------------------------------------*/
              if (sect == link_ptr->comm->last_sect)
              {
                /*-----------------------------------------------------*/
                /* If not going past EOF, set position. Else set error.*/
                /*-----------------------------------------------------*/
                if (link_ptr->comm->one_past_last >= pos->offset)
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
            /* pos->offset >= sector size, so return error.            */
            /*---------------------------------------------------------*/
            else
            {
              set_errno(EINVAL);
              return (void *)-1;
            }
          }
        }
        break;
      }

      /*---------------------------------------------------------------*/
      /* Handle ftell() for the RAM file system.                       */
      /*---------------------------------------------------------------*/
      case FTELL:
        /*-------------------------------------------------------------*/
        /* If file is empty, return 0.                                 */
        /*-------------------------------------------------------------*/
        link_ptr = &((RFSEnt *)handle)->entry.file;
        if (link_ptr->comm->size == 0)
          return 0;

        /*-------------------------------------------------------------*/
        /* Count bytes from beginning to curr_ptr.                     */
        /*-------------------------------------------------------------*/
        return (void *)(stream->curr_ptr.sect_off * RAM_SECT_SZ +
                        stream->curr_ptr.offset);

      /*---------------------------------------------------------------*/
      /* Handle chdir() for the RAM file system.                       */
      /*---------------------------------------------------------------*/
      case CHDIR:
        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        dir_ent = va_arg(ap, RFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Check for search permission for the directory.              */
        /*-------------------------------------------------------------*/
        if (CheckPerm((void *)dir_ent->entry.dir.comm, F_EXECUTE))
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
        FsSaveCWD((ui32)Ram, (ui32)dir_ent);
        ++dir_ent->entry.dir.cwds;

        break;

      /*---------------------------------------------------------------*/
      /* Handle mkdir() for the RAM file system.                       */
      /*---------------------------------------------------------------*/
      case MKDIR:
      {
        RFSEnt *fep;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        mode = (mode_t)va_arg(ap, int);
        dir_ent = va_arg(ap, RFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Get the pointer to the directory in which mkdir takes place */
        /*-------------------------------------------------------------*/
        dir = &dir_ent->entry.dir;

        /*-------------------------------------------------------------*/
        /* Set the return value to error.                              */
        /*-------------------------------------------------------------*/
        vp = (void *)-1;

        /*-------------------------------------------------------------*/
        /* Check that parent directory has search/write permissions.   */
        /*-------------------------------------------------------------*/
        if (CheckPerm((void *)dir->comm, F_WRITE | F_EXECUTE))
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
          /* If a directory with a given name exists, return error.    */
          /*-----------------------------------------------------------*/
          if (!strncmp(fep->entry.dir.name, name, FILENAME_MAX))
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
        /* If no more entries available stop.                          */
        /*-------------------------------------------------------------*/
        if (curr_table2 == NULL)
        {
          curr_table1->tbl[i].type = FEMPTY;
          ++curr_table1->free;
          ++Ram->total_free;
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* Setup the directory common entry.                           */
        /*-------------------------------------------------------------*/
        v_ent = &curr_table2->tbl[j];
        v_ent->type = FCOMMN;
        v_ent->entry.comm.open_mode = 0;
        v_ent->entry.comm.links = 1;
        v_ent->entry.comm.open_links = 0;
        v_ent->entry.comm.frst_sect = NULL;
        v_ent->entry.comm.last_sect = NULL;
        v_ent->entry.comm.one_past_last = 0;
        v_ent->entry.comm.mod_time =v_ent->entry.comm.ac_time=OsSecCount;
        v_ent->entry.comm.fileno = Ram->fileno_gen++;
        v_ent->entry.comm.temp = 0;
        v_ent->entry.comm.addr = v_ent;
        v_ent->entry.comm.size = 0;

        /*-------------------------------------------------------------*/
        /* Setup the directory entry in the RamFileTbl.                */
        /*-------------------------------------------------------------*/
        v_ent = &curr_table1->tbl[i];
        v_ent->type = FDIREN;
        v_ent->entry.dir.cwds = 0;
        v_ent->entry.dir.dir = NULL;
        v_ent->entry.dir.first = NULL;
        v_ent->entry.dir.parent_dir = dir_ent;
        v_ent->entry.dir.prev = NULL;
        v_ent->entry.dir.comm = &(curr_table2->tbl[j].entry.comm);
        strncpy(v_ent->entry.dir.name, name, FILENAME_MAX);

        /*-------------------------------------------------------------*/
        /* Set the permissions for the directory.                      */
        /*-------------------------------------------------------------*/
        SetPerm((void *)v_ent->entry.dir.comm, mode);

        /*-------------------------------------------------------------*/
        /* Add the new directory to its parent directory.              */
        /*-------------------------------------------------------------*/
        fep = dir->first;
        dir->first = v_ent;
        v_ent->entry.dir.next = fep;
        if (fep)
          fep->entry.dir.prev = v_ent;

        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle rmdir() for the RAM file system.                       */
      /*---------------------------------------------------------------*/
      case RMDIR:
        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        dir_ent = va_arg(ap, RFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Call helper function to do the actual remove.               */
        /*-------------------------------------------------------------*/
        return remove_dir(dir_ent);

      /*---------------------------------------------------------------*/
      /* Handle sortdir() for the RAM file system.                     */
      /*---------------------------------------------------------------*/
      case SORTDIR:
      {
        CompFunc cmp;
        DirEntry e1, e2;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        dir_ent = va_arg(ap, RFSEnt *);
        cmp = va_arg(ap, CompFunc);
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
        QuickSort((void *)&dir_ent->entry.dir.first,
                  (void *)&tmp_ent, &e1, &e2, cmp);

        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle stat() for the RAM file system.                        */
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
        tmp_ent = va_arg(ap, RFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Set the buf struct with the right values for this entry.    */
        /*-------------------------------------------------------------*/
        statistics(buf, tmp_ent);
        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle opendir() for the RAM file system.                     */
      /*---------------------------------------------------------------*/
      case OPENDIR:
        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        dir_ent = va_arg(ap, RFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Open directory, returning NULL if error.                    */
        /*-------------------------------------------------------------*/
        if (open_dir(stream, dir_ent) == NULL)
          return NULL;

        /*-------------------------------------------------------------*/
        /* Increment opendir() count and return control block pointe   */
        /*-------------------------------------------------------------*/
        ++Ram->opendirs;
        return stream;

      /*---------------------------------------------------------------*/
      /* Handle link() for the RAM file system.                        */
      /*---------------------------------------------------------------*/
      case LINK:
      {
        char *old, *new_link;
        RFSEnt *dir_old, *dir_new, *temp_old, *temp_new, *temp_dir;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        old = va_arg(ap, char *);
        new_link = va_arg(ap, char *);
        dir_old = va_arg(ap, RFSEnt *);
        dir_new = va_arg(ap, RFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* While there are terminating '/' charactors, remove them.    */
        /*-------------------------------------------------------------*/
        end = strlen(old) - 1;
        while (old[end] == '/')
          old[end--] = '\0';
        end = strlen(new_link) - 1;
        while (new_link[end] == '/')
          new_link[end--] = '\0';

        /*-------------------------------------------------------------*/
        /* Check that both old and new dirs have execute access.       */
        /*-------------------------------------------------------------*/
        if (CheckPerm((void *)dir_old->entry.dir.comm, F_EXECUTE) ||
            CheckPerm((void *)dir_new->entry.dir.comm, F_EXECUTE))
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* If no old name is provided, use the old directory, else look*/
        /* for old entry in dir_old.                                   */
        /*-------------------------------------------------------------*/
        if (old[0] == '\0')
          temp_old = dir_old;
        else
          for (temp_old = dir_old->entry.dir.first;;
               temp_old = temp_old->entry.dir.next)
          {
            /*---------------------------------------------------------*/
            /* Return error if old file could not be found.            */
            /*---------------------------------------------------------*/
            if (temp_old == NULL)
            {
              set_errno(ENOENT);
              return (void *)-1;
            }

            /*---------------------------------------------------------*/
            /* Break if old name is found.                             */
            /*---------------------------------------------------------*/
            if (!strncmp(temp_old->entry.dir.name, old, FILENAME_MAX))
              break;
          }

        /*-------------------------------------------------------------*/
        /* Make sure owner of file is trying to link to file.          */
        /*-------------------------------------------------------------*/
        FsGetId(&uid, &gid);
        if (uid != temp_old->entry.file.comm->user_id)
        {
          set_errno(EPERM);
          return (void *)-1;
        }

        /*-------------------------------------------------------------*/
        /* Look for name duplication with the new link.                */
        /*-------------------------------------------------------------*/
        for (temp_new = dir_new->entry.dir.first; temp_new;
             temp_new = temp_new->entry.dir.next)
        {
          /*-----------------------------------------------------------*/
          /* If a file exists with the name, error.                    */
          /*-----------------------------------------------------------*/
          if (!strncmp(temp_new->entry.dir.name, new_link, FILENAME_MAX))
          {
            set_errno(EEXIST);
            return (void *)-1;
          }
        }

        /*-------------------------------------------------------------*/
        /* Check that the new directory has write permissions.         */
        /*-------------------------------------------------------------*/
        if (CheckPerm((void *)dir_new->entry.dir.comm, F_WRITE))
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* If we're linking dirs, make sure the new one is not in a    */
        /* subdirectory of the old one.                                */
        /*-------------------------------------------------------------*/
        for (temp_dir = dir_new; temp_dir != &Ram->files_tbl->tbl[0];
             temp_dir = temp_dir->entry.dir.parent_dir)
        {
          /*-----------------------------------------------------------*/
          /* If new directory is subdirectory of old, return error.    */
          /*-----------------------------------------------------------*/
          if (temp_dir == temp_old)
          {
            set_errno(EINVAL);
            return (void *)-1;
          }
        }

        /*-------------------------------------------------------------*/
        /* Find a space in the files tables for the link.              */
        /*-------------------------------------------------------------*/
        curr_table = find_entry(&i);

        /*-------------------------------------------------------------*/
        /* If no more free entries found, stop.                        */
        /*-------------------------------------------------------------*/
        if (curr_table == NULL)
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* Set the new link to point to the original file/dir.         */
        /*-------------------------------------------------------------*/
        v_ent = &curr_table->tbl[i];
        v_ent->type = temp_old->type;
        v_ent->entry.file.comm = temp_old->entry.file.comm;

        /*-------------------------------------------------------------*/
        /* Set the link name.                                          */
        /*-------------------------------------------------------------*/
        strncpy(v_ent->entry.file.name, new_link, FILENAME_MAX);

        /*-------------------------------------------------------------*/
        /* Set the rest of the entry fields.                           */
        /*-------------------------------------------------------------*/
        v_ent->entry.dir.parent_dir = dir_new;
        if (temp_old->type == FFILEN)
          v_ent->entry.file.file = NULL;
        else
        {
          v_ent->entry.dir.parent_dir = dir_new;
          v_ent->entry.dir.first = temp_old->entry.dir.first;
          v_ent->entry.dir.dir = NULL;
          v_ent->entry.dir.cwds = 0;
        }

        /*-------------------------------------------------------------*/
        /* Increment the number of links for the original file/dir.    */
        /*-------------------------------------------------------------*/
        ++temp_old->entry.file.comm->links;

        /*-------------------------------------------------------------*/
        /* Add the new link to its parent directory.                   */
        /*-------------------------------------------------------------*/
        temp_new = dir_new->entry.dir.first;
        dir_new->entry.dir.first = v_ent;
        v_ent->entry.dir.next = temp_new;
        v_ent->entry.dir.prev = NULL;
        if (temp_new)
          temp_new->entry.dir.prev = v_ent;

        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle rename() for the RAM file system.                      */
      /*---------------------------------------------------------------*/
      case RENAME:
      {
        char *old_name, *new_name;
        RFSEnt *old_dir, *new_dir, *old_ent;
        size_t end_name;
        int is_dir;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        old_name = va_arg(ap, char *);
        new_name = va_arg(ap, char *);
        old_dir = va_arg(ap, RFSEnt *);
        new_dir = va_arg(ap, RFSEnt *);
        is_dir = va_arg(ap, int);
        va_end(ap);

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
        if (CheckPerm((void *)old_dir->entry.dir.comm, F_EXECUTE) ||
            CheckPerm((void *)new_dir->entry.dir.comm, F_EXECUTE))
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* If no old name, use old_dir, else look for entry in old_dir.*/
        /*-------------------------------------------------------------*/
        if (old_name[0] == '\0')
          old_ent = old_dir;
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
              break;
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
        if (CheckPerm((void *)new_dir->entry.dir.comm, F_WRITE))
          return (void *)-1;

        /*-------------------------------------------------------------*/
        /* If we're linking dirs, make sure the new one is not in a    */
        /* subdirectory of the old one.                                */
        /*-------------------------------------------------------------*/
        if (old_ent->type == FDIREN)
          for (tmp_ent = new_dir; tmp_ent != &Ram->files_tbl->tbl[0];
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
        /* Set new name.                                               */
        /*-------------------------------------------------------------*/
        strcpy(old_ent->entry.file.name, new_name);

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
          /* If directory, set parent_dir pointer to new_dir.          */
          /*-----------------------------------------------------------*/
          if (old_ent->type == FDIREN)
            old_ent->entry.dir.parent_dir = new_dir;
        }
        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle access() for the RAM file system.                      */
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
        dir_ent = va_arg(ap, RFSEnt *);
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
          if (CheckPerm((void *)tmp_ent->entry.file.comm, F_READ))
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
          if (CheckPerm((void *)tmp_ent->entry.file.comm, F_WRITE))
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
          if (CheckPerm((void *)tmp_ent->entry.file.comm, F_EXECUTE))
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
        dir_ent = va_arg(ap, RFSEnt *);
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
        SetPerm((void *)tmp_ent->entry.file.comm, mode);

        return NULL;

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
        dir_ent = va_arg(ap, RFSEnt *);
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
        /* Check that the owner of the file is trying to modify mode   */
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

        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle utime() for the RAM file system.                       */
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
        dir_ent = va_arg(ap, RFSEnt *);
        va_end(ap);

        dir = &dir_ent->entry.dir;

        /*-------------------------------------------------------------*/
        /* While there are terminating '/' charactors, remove them.    */
        /*-------------------------------------------------------------*/
        end = strlen(name) - 1;
        while (name[end] == '/')
          name[end--] = '\0';

        /*-------------------------------------------------------------*/
        /* Check directory has search/write permissions.               */
        /*-------------------------------------------------------------*/
        if (CheckPerm((void *)dir->comm, F_EXECUTE | F_WRITE))
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
            if (!strncmp(tmp_ent->entry.dir.name, name, FILENAME_MAX))
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
          link_ptr->comm->mod_time = times->modtime;
          link_ptr->comm->ac_time = times->actime;
        }
        else
          link_ptr->comm->mod_time = link_ptr->comm->ac_time =OsSecCount;

        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle fopen() for the RAM file system.                       */
      /*---------------------------------------------------------------*/
      case FOPEN:
      {
        char *mode_str;

        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get arguments.                  */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        mode_str = va_arg(ap, char *);
        dir_ent = va_arg(ap, RFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* If name is not directory name, call rfs_open().             */
        /*-------------------------------------------------------------*/
        if ((*name != '\0') && (name[strlen(name) - 1] != '/'))
          return rfs_fopen(name, mode_str, dir_ent, stream);
        return NULL;
      }

      /*---------------------------------------------------------------*/
      /* Handle remove() for the RAM file system.                      */
      /*---------------------------------------------------------------*/
      case REMOVE:
        /*-------------------------------------------------------------*/
        /* Use the va_arg mechanism to get the arguments.              */
        /*-------------------------------------------------------------*/
        va_start(ap, code);
        name = va_arg(ap, char *);
        dir_ent = va_arg(ap, RFSEnt *);
        va_end(ap);

        /*-------------------------------------------------------------*/
        /* Call helper function to perform the actual remove.          */
        /*-------------------------------------------------------------*/
        return remove_file(dir_ent, name);

      /*---------------------------------------------------------------*/
      /* Handle searching for the RAM file system.                     */
      /*---------------------------------------------------------------*/
      case FSEARCH:
      {
        /*-------------------------------------------------------------*/
        /* Search a filename in the flash file system.                 */
        /*-------------------------------------------------------------*/
        int use_cwd, find_parent;
        ui32 dummy;
        RFSEnt *curr_dir;

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
        if (use_cwd == FIRST_DIR)
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
            stream->ioctl = RamIoctl;
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
                stream->ioctl = RamIoctl;
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
            if (CheckPerm((void *)dir_ent->entry.dir.comm, F_EXECUTE))
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
      /* Break to return NULL for success. flush is no-op for RAM.     */
      /*---------------------------------------------------------------*/
      case FFLUSH:
        break;

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

  return vp;
}
#endif /* NUM_RFS_VOLS */

