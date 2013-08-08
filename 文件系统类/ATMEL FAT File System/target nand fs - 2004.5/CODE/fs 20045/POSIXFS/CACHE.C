/***********************************************************************/
/*                                                                     */
/*   Module:  cache.c                                                  */
/*   Release: 2004.5                                                   */
/*   Version: 2004.2                                                   */
/*   Purpose: Cache interface for the file systems                     */
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
#include "posixfsp.h"
#include "../include/cache.h"

/***********************************************************************/
/* Local Function Definitions                                          */
/***********************************************************************/

/***********************************************************************/
/* remove_from_replacer: Remove specified entry from cache LRU list    */
/*                                                                     */
/*      Inputs: entry = pointer to cache entry                         */
/*              C = cache to which LRU list belongs                    */
/*                                                                     */
/***********************************************************************/
static void remove_from_replacer(CacheEntry *entry, Cache *C)
{
  /*-------------------------------------------------------------------*/
  /* Adjust the double linked list (LRU) by taking the entry out.      */
  /*-------------------------------------------------------------------*/
  if (entry->prev_lru)
    entry->prev_lru->next_lru = entry->next_lru;
  else
    C->lru_head = entry->next_lru;

  if (entry->next_lru)
    entry->next_lru->prev_lru = entry->prev_lru;
  else
    C->lru_tail = entry->prev_lru;

  /*-------------------------------------------------------------------*/
  /* Null out the pointers for the taken out entry.                    */
  /*-------------------------------------------------------------------*/
  entry->prev_lru = entry->next_lru = NULL;
}

/***********************************************************************/
/* put_into_tail: Add an entry to the end of the linked list           */
/*                                                                     */
/*      Inputs: entry = pointer to cache entry to be added             */
/*              C = cache to which the list belongs                    */
/*                                                                     */
/***********************************************************************/
static void put_into_tail(CacheEntry *entry, Cache *C)
{
  /*-------------------------------------------------------------------*/
  /* Set next to NULL since entry will be last entry, and prev to what */
  /* tail of list was before.                                          */
  /*-------------------------------------------------------------------*/
  entry->next_lru = NULL;
  entry->prev_lru = C->lru_tail;

  /*-------------------------------------------------------------------*/
  /* If the list was not empty, set the next of tail to new entry,     */
  /* else, because list was empty, set also head of list to new entry. */
  /*-------------------------------------------------------------------*/
  if (C->lru_tail)
    C->lru_tail->next_lru = entry;
  else
    C->lru_head = entry;

  /*-------------------------------------------------------------------*/
  /* Set tail of list to new entry.                                    */
  /*-------------------------------------------------------------------*/
  C->lru_tail = entry;
}

/***********************************************************************/
/*        hash: Hash based on the sector number                        */
/*                                                                     */
/*      Inputs: sector_number = value on which hash is done            */
/*              size = size of the table                               */
/*                                                                     */
/*     Returns: Index into the hash table where value gets hashed      */
/*                                                                     */
/***********************************************************************/
static int hash(int sector_number, int size)
{
  return (19823 * sector_number + 321043) % size;
}

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*   InitCache: Initialize and allocate memory for the specified cache */
/*                                                                     */
/*      Inputs: C = cache to be initialized                            */
/*              pool_size = number of sectors in the cache             */
/*              writef = write sector function                         */
/*              readf = read sector function                           */
/*              sect_sz = sector size for file system                  */
/*              temp_sects = number of temporary sectors               */
/*              alignment = memory alignment                           */
/*                                                                     */
/*     Returns: Beginning of pool on success, 0 on failure             */
/*                                                                     */
/***********************************************************************/
ui32 InitCache(Cache *C, int pool_size, MedWFunc writef, MedRFunc readf,
               int sect_sz, int temp_sects, int alignment)
{
  ui32 *sectors;

  /*-------------------------------------------------------------------*/
  /* Set the write and read functions for the cache.                   */
  /*-------------------------------------------------------------------*/
  C->write = writef;
  C->read = readf;

  /*-------------------------------------------------------------------*/
  /* Set the number of sectors in cache.                               */
  /*-------------------------------------------------------------------*/
  C->pool_size = pool_size;

  /*-------------------------------------------------------------------*/
  /* Allocate memory for the pool and hash table.                      */
  /*-------------------------------------------------------------------*/
  C->pool = calloc((size_t)C->pool_size * sizeof(CacheEntry), (size_t)1);
  if (C->pool == NULL)
    return 0;
  C->hash_tbl = calloc((size_t)C->pool_size * sizeof(CacheEntry *),
                       (size_t)1);
  if (C->hash_tbl == NULL)
  {
    free(C->pool);
    return 0;
  }

  /*-------------------------------------------------------------------*/
  /* Allocate memory for all the sectors. This is where data goes.     */
  /* Also, allocate extra sectors for use by file sys. internals.      */
  /* Pointers to extra sectors will be set in file system init routine.*/
  /*-------------------------------------------------------------------*/
  sectors = calloc((size_t)(sect_sz * (C->pool_size + temp_sects) /
                            alignment), (size_t)alignment);
  if (sectors == NULL)
  {
    free(C->pool);
    free(C->hash_tbl);
    return 0;
  }

  /*-------------------------------------------------------------------*/
  /* Initialize the pool for the cache.                                */
  /*-------------------------------------------------------------------*/
  C->pool[0].sector = (ui8 *)sectors;
  ReinitCache(C, sect_sz);

  return (ui32)sectors;
}

/***********************************************************************/
/* ReinitCache: Re-initialize the cache                                */
/*                                                                     */
/*      Inputs: C = cache to be reinitialized                          */
/*              sect_sz = sector size for file system                  */
/*                                                                     */
/***********************************************************************/
void ReinitCache(Cache *C, int sect_sz)
{
  int i;
  ui8 *mem = C->pool[0].sector;

  /*-------------------------------------------------------------------*/
  /* Loop to initialize each entry in the cache.                       */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < C->pool_size; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* Assign a sector chunk of the preallocated memory.               */
    /*-----------------------------------------------------------------*/
    C->pool[i].sector = mem;
    mem += sect_sz;

    /*-----------------------------------------------------------------*/
    /* Set the entry in the pool to have an invalid sector number, -1, */
    /* not dirty, 0 pin count, no hash location and file_ptr.          */
    /*-----------------------------------------------------------------*/
    C->pool[i].sect_num = -1;
    C->pool[i].dirty = CLEAN;
    C->pool[i].pin_cnt = 0;
    C->pool[i].hash_loc = NULL;
    C->pool[i].file_ptr = NULL;

    /*-----------------------------------------------------------------*/
    /* If the first entry in pool, set its prev_lru to NULL.           */
    /*-----------------------------------------------------------------*/
    if (!i)
    {
      C->pool[i].prev_lru = NULL;
      if (C->pool_size == 1)
        C->pool[i].next_lru = NULL;
      else
        C->pool[i].next_lru = &C->pool[i + 1];
    }

    /*-----------------------------------------------------------------*/
    /* Else if last entry in pool, set its next_lru to NULL.           */
    /*-----------------------------------------------------------------*/
    else if (i == (C->pool_size - 1))
    {
      C->pool[i].prev_lru = &C->pool[i - 1];
      C->pool[i].next_lru = NULL;
    }

    /*-----------------------------------------------------------------*/
    /* Else set prev_lru to previous and next_lru to next entry.       */
    /*-----------------------------------------------------------------*/
    else
    {
      C->pool[i].prev_lru = &C->pool[i - 1];
      C->pool[i].next_lru = &C->pool[i + 1];
    }

    /*-----------------------------------------------------------------*/
    /* Initialize also the hash table entry.                           */
    /*-----------------------------------------------------------------*/
    C->pool[i].next_hash = C->pool[i].prev_hash = NULL;
    C->pool[i].hash_loc = NULL;
    C->hash_tbl[i] = NULL;
  }

  /*-------------------------------------------------------------------*/
  /* Set the dirty flags.                                              */
  /*-------------------------------------------------------------------*/
  C->dirty_old = C->dirty_new = FALSE;

  /*-------------------------------------------------------------------*/
  /* Initialize the current sector being worked on.                    */
  /*-------------------------------------------------------------------*/
  C->sector_number = -1;

  /*-------------------------------------------------------------------*/
  /* Set the LRU head to first and LRU tail to last entry in pool.     */
  /*-------------------------------------------------------------------*/
  C->lru_head = &C->pool[0];
  C->lru_tail = &C->pool[C->pool_size - 1];
}

/***********************************************************************/
/* DestroyCache: Deallocate all the memory taken by cache C            */
/*                                                                     */
/*       Input: C = cache to be deleted                                */
/*                                                                     */
/***********************************************************************/
void DestroyCache(Cache *C)
{
  /*-------------------------------------------------------------------*/
  /* Write back all the dirty sectors.                                 */
  /*-------------------------------------------------------------------*/
  FlushSectors(C);

  /*-------------------------------------------------------------------*/
  /* Deallocate the memory for the pool and the hash table.            */
  /*-------------------------------------------------------------------*/
  free(C->pool[0].sector);
  free(C->pool);
  free(C->hash_tbl);

  /*-------------------------------------------------------------------*/
  /* Null out the LRU head and tail.                                   */
  /*-------------------------------------------------------------------*/
  C->lru_head = C->lru_tail = NULL;
}

/***********************************************************************/
/*   GetSector: Return pointer to cache entry containing the specified */
/*              sector number, bringing the sector into the cache if   */
/*              not already there.                                     */
/*                                                                     */
/*      Inputs: C = cache pointer                                      */
/*              sector_number = sector to return pointer to            */
/*              skip_read = if TRUE, don't read new sector from media  */
/*              file_ptr = pointer to file control information         */
/*              entry_ptr = cache entry with new sector                */
/*                                                                     */
/*     Returns: GET_OK on success, GET_WRITE_ERROR on write error,     */
/*              GET_READ_ERROR on read error                           */
/*                                                                     */
/***********************************************************************/
int GetSector(Cache *C, int sector_number, int skip_read, void *file_ptr,
              CacheEntry **entry_ptr)
{
  int index, r_val = GET_OK;
  CacheEntry *entry;

  /*-------------------------------------------------------------------*/
  /* Remember the current sector being chached and get the head of the */
  /* list in which the corresponding entry would be in the cache.      */
  /*-------------------------------------------------------------------*/
  C->sector_number = sector_number;
  entry = C->hash_tbl[hash(C->sector_number, C->pool_size)];

  /*-------------------------------------------------------------------*/
  /* Look first if sector is already in the cache.                     */
  /*-------------------------------------------------------------------*/
  for (; entry; entry = entry->next_hash)
    if (entry->sect_num == C->sector_number)
    {
      /*---------------------------------------------------------------*/
      /* If entry is on replacer list, take it out.                    */
      /*---------------------------------------------------------------*/
      if (entry->pin_cnt++ == 0)
        remove_from_replacer(entry, C);

      *entry_ptr = entry;
      return r_val;
    }

  /*-------------------------------------------------------------------*/
  /* The sector is not in the cache. Choose one to replace with LRU    */
  /* replacement policy.                                               */
  /*-------------------------------------------------------------------*/
  entry = C->lru_head;
  remove_from_replacer(C->lru_head, C);

  /*-------------------------------------------------------------------*/
  /* There should always be an available sector.                       */
  /*-------------------------------------------------------------------*/
  PfAssert(entry);

  /*-------------------------------------------------------------------*/
  /* If dirty sector, write it back to its medium.                     */
  /*-------------------------------------------------------------------*/
  if (entry->dirty != CLEAN && C->write(entry, FALSE))
  {
    entry->dirty = CLEAN;
    put_into_tail(entry, C);
    *entry_ptr = NULL;
    return GET_WRITE_ERROR;
  }

  /*-------------------------------------------------------------------*/
  /* If the entry is in the hash table, remove it from there.          */
  /*-------------------------------------------------------------------*/
  if (entry->hash_loc)
  {
    /*-----------------------------------------------------------------*/
    /* If entry is not first, update previous one, else update head.   */
    /*-----------------------------------------------------------------*/
    if (entry->prev_hash)
      entry->prev_hash->next_hash = entry->next_hash;
    else
      *(entry->hash_loc) = entry->next_hash;

    /*-----------------------------------------------------------------*/
    /* If next entry needs to be updated, update it.                   */
    /*-----------------------------------------------------------------*/
    if (entry->next_hash)
      entry->next_hash->prev_hash = entry->prev_hash;
  }

  /*-------------------------------------------------------------------*/
  /* Read new sector into the cache if skip_read is not set.           */
  /*-------------------------------------------------------------------*/
  if (skip_read == FALSE &&
      C->read(entry->sector, (ui32)C->sector_number))
  {
    set_errno(EIO);
    r_val = GET_READ_ERROR;
  }

  /*-------------------------------------------------------------------*/
  /* Set entry for new sector.                                         */
  /*-------------------------------------------------------------------*/
  entry->sect_num = C->sector_number;
  entry->dirty = CLEAN;
  entry->pin_cnt = 1;
  entry->file_ptr = file_ptr;

  /*-------------------------------------------------------------------*/
  /* Add new entry into the hash table.                                */
  /*-------------------------------------------------------------------*/
  index = hash(C->sector_number, C->pool_size);
  entry->prev_hash = NULL;

  if (C->hash_tbl[index])
  {
    entry->next_hash = C->hash_tbl[index];
    C->hash_tbl[index]->prev_hash = entry;
  }
  else
    entry->next_hash = NULL;

  C->hash_tbl[index] = entry;
  entry->hash_loc = &C->hash_tbl[index];

  /*-------------------------------------------------------------------*/
  /* No need to remember the sector being cached any longer.           */
  /*-------------------------------------------------------------------*/
  C->sector_number = -1;

  *entry_ptr = entry;
  return r_val;
}

/***********************************************************************/
/* FlushFileSectors: Flush all sectors belonging to a file             */
/*                                                                     */
/*      Inputs: C = cache for which flush is to be performed           */
/*              file_ptr = pointer to file                             */
/*                                                                     */
/*     Returns: NULL on success, (void)-1 on failure                   */
/*                                                                     */
/***********************************************************************/
void *FlushFileSectors(Cache *C, const void *file_ptr)
{
  int i;
  void *r_val = NULL;

  /*-------------------------------------------------------------------*/
  /* If no dirty sectors, return success.                              */
  /*-------------------------------------------------------------------*/
  if (!C->dirty_old && !C->dirty_new)
    return NULL;

  /*-------------------------------------------------------------------*/
  /* Loop over cache entries, flushing each one.                       */
  /*-------------------------------------------------------------------*/
  C->dirty_old = C->dirty_new = FALSE;
  for (i = 0; i < C->pool_size; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* If sector is not clean, either it belongs to the file or have   */
    /* to set the dirty flag.                                          */
    /*-----------------------------------------------------------------*/
    if (C->pool[i].dirty != CLEAN)
    {
      /*---------------------------------------------------------------*/
      /* If it belongs to the file, write it and mark it clear.        */
      /*---------------------------------------------------------------*/
      if (C->pool[i].file_ptr == file_ptr)
      {
        if (C->write(&C->pool[i], TRUE))
          r_val = (void *)-1;
        C->pool[i].dirty = CLEAN;
      }

      /*---------------------------------------------------------------*/
      /* Else, mark the appropriate flag.                              */
      /*---------------------------------------------------------------*/
      else if (C->pool[i].dirty == DIRTY_NEW)
        C->dirty_new = TRUE;
      else
        C->dirty_old = TRUE;
    }
  }
  return r_val;
}

/***********************************************************************/
/* FlushSectors: Go through cache and flush all dirty sectors          */
/*                                                                     */
/*       Input: C = cache for which flush is to be performed           */
/*                                                                     */
/*     Returns: -1 on failure, 0 if no sectors written, 1 otherwise    */
/*                                                                     */
/***********************************************************************/
int FlushSectors(Cache *C)
{
  int i, r_val = 0, written;

  /*-------------------------------------------------------------------*/
  /* If no dirty sectors, return.                                      */
  /*-------------------------------------------------------------------*/
  if (!C->dirty_old && !C->dirty_new)
    return 0;

  /*-------------------------------------------------------------------*/
  /* Loop over cache entries, flushing each one.                       */
  /*-------------------------------------------------------------------*/
  for (i = 0, written = FALSE; i < C->pool_size; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* If sector is not clean, write it and then mark it clean.        */
    /*-----------------------------------------------------------------*/
    if (C->pool[i].dirty != CLEAN)
    {
      written = TRUE;
      if (C->write(&C->pool[i], TRUE))
        r_val = -1;
      C->pool[i].dirty = CLEAN;
    }
  }
  if (written && !r_val)
    r_val = 1;

  /*-------------------------------------------------------------------*/
  /* Reset the dirty flags.                                            */
  /*-------------------------------------------------------------------*/
  C->dirty_old = C->dirty_new = FALSE;

  return r_val;
}

/***********************************************************************/
/*  FreeSector: Unpin the specified cache entry                        */
/*                                                                     */
/*      Inputs: ep = pointer to sector's cache entry                   */
/*              C = cache                                              */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
int FreeSector(CacheEntry **ep, Cache *C)
{
  int r_val = 0;
  CacheEntry *entry = *ep;

  /*-------------------------------------------------------------------*/
  /* Pin count can never be less than one.                             */
  /*-------------------------------------------------------------------*/
  PfAssert(entry->pin_cnt > 0);

  /*-------------------------------------------------------------------*/
  /* Decrement pin_count and if 0, put it on the LRU list.             */
  /*-------------------------------------------------------------------*/
  if (--entry->pin_cnt == 0)
    put_into_tail(entry, C);

  /*-------------------------------------------------------------------*/
  /* Clear the entry.                                                  */
  /*-------------------------------------------------------------------*/
  *ep = NULL;

#if FFS_CACHE_WRITE_THROUGH
  /*-------------------------------------------------------------------*/
  /* If write through cache and sector dirty, write it now.            */
  /*-------------------------------------------------------------------*/
  if (entry->dirty != CLEAN)
  {
    if (C->write(entry, TRUE))
      r_val = -1;
    entry->dirty = CLEAN;
  }
#endif

  return r_val;
}

/***********************************************************************/
/* RemoveFromCache: Remove specified sector from cache, if present     */
/*                                                                     */
/*      Inputs: C = cache to look into                                 */
/*              sector_number = sector to look for                     */
/*              new_sector = new value for sector_number if it matches */
/*                           the sector to be removed                  */
/*                                                                     */
/***********************************************************************/
void RemoveFromCache(Cache *C, int sector_number, int new_sector)
{
  int location = hash(sector_number, C->pool_size);
  CacheEntry *entry;

  /*-------------------------------------------------------------------*/
  /* If getting sector_number, update it with the new value.           */
  /*-------------------------------------------------------------------*/
  if (new_sector != -1 && C->sector_number == sector_number)
  {
    C->sector_number = new_sector;
    return;
  }

  /*-------------------------------------------------------------------*/
  /* Loop through hash bucket, looking for specified entry.            */
  /*-------------------------------------------------------------------*/
  for (entry = C->hash_tbl[location]; entry; entry = entry->next_hash)
  {
    /*-----------------------------------------------------------------*/
    /* If matching entry is found, remove it from the cache.           */
    /*-----------------------------------------------------------------*/
    if (entry->sect_num == sector_number)
    {
      /*---------------------------------------------------------------*/
      /* If entry is not on LRU list, add it.                          */
      /*---------------------------------------------------------------*/
      if (entry->pin_cnt)
        put_into_tail(entry, C);

      /*---------------------------------------------------------------*/
      /* Reset the entry.                                              */
      /*---------------------------------------------------------------*/
      entry->sect_num = -1;
      entry->dirty = CLEAN;
      entry->pin_cnt = 0;
      entry->hash_loc = NULL;

      /*---------------------------------------------------------------*/
      /* Remove entry from hash bucket list.                           */
      /*---------------------------------------------------------------*/
      if (entry->prev_hash)
        entry->prev_hash->next_hash = entry->next_hash;
      else
        C->hash_tbl[location] = entry->next_hash;
      if (entry->next_hash)
        entry->next_hash->prev_hash = entry->prev_hash;
      entry->next_hash = entry->prev_hash = NULL;
      entry->hash_loc = NULL;

      break;
    }
  }
}

/***********************************************************************/
/* UpdateFilePtrs: Adjust all cache entry file_ptrs that point to old  */
/*              location                                               */
/*                                                                     */
/*      Inputs: C = pointer to cache to be used                        */
/*              old_fptr = old file_ptr value                          */
/*              new_fptr = new file_ptr value                          */
/*                                                                     */
/***********************************************************************/
void UpdateFilePtrs(Cache *C, const void *old_fptr, void *new_fptr)
{
  int i;

  /*-------------------------------------------------------------------*/
  /* Loop over every sector in cache.                                  */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < C->pool_size; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* If a cache entry file_ptr points to old, change to new.         */
    /*-----------------------------------------------------------------*/
    if (C->pool[i].file_ptr == old_fptr)
      C->pool[i].file_ptr = new_fptr;
  }
}

/***********************************************************************/
/* UpdateCache: Reposition the specified entry in the cache with a new */
/*              sector number                                          */
/*                                                                     */
/*      Inputs: entry = entry to sector in cache                       */
/*              new_sector = new sector value                          */
/*              C = pointer to cache to be used                        */
/*                                                                     */
/***********************************************************************/
void UpdateCache(CacheEntry *entry, int new_sector, const Cache *C)
{
  int new_location = hash(new_sector, C->pool_size);

  /*-------------------------------------------------------------------*/
  /* Update the sector value to new one.                               */
  /*-------------------------------------------------------------------*/
  entry->sect_num = new_sector;

  /*-------------------------------------------------------------------*/
  /* Take entry for old sector out of hash table.                      */
  /*-------------------------------------------------------------------*/
  if (entry->prev_hash)
    entry->prev_hash->next_hash = entry->next_hash;
  else
    *entry->hash_loc = entry->next_hash;
  if (entry->next_hash)
    entry->next_hash->prev_hash = entry->prev_hash;

  /*-------------------------------------------------------------------*/
  /* Put entry for new sector in hash table.                           */
  /*-------------------------------------------------------------------*/
  entry->next_hash = C->hash_tbl[new_location];
  entry->prev_hash = NULL;
  if (entry->next_hash)
    C->hash_tbl[new_location]->prev_hash = entry;
  C->hash_tbl[new_location] = entry;
  entry->hash_loc = &C->hash_tbl[new_location];
}

/***********************************************************************/
/* UpdateCacheSectNum: Update an entry's sector number                 */
/*                                                                     */
/*      Inputs: C = pointer to cache structure                         */
/*              old_sector = sector to be replaced                     */
/*              new_sector = replace value                             */
/*                                                                     */
/***********************************************************************/
void UpdateCacheSectNum(Cache *C, int old_sector, int new_sector)
{
  CacheEntry *entry;

  /*-------------------------------------------------------------------*/
  /* If sector being replaced is the current sector being cached,      */
  /* adjust current sector being cached.                               */
  /*-------------------------------------------------------------------*/
  if (old_sector == C->sector_number)
    C->sector_number = new_sector;

  /*-------------------------------------------------------------------*/
  /* Get head of list where entry for old sector would be in cache.    */
  /*-------------------------------------------------------------------*/
  entry = C->hash_tbl[hash(old_sector, C->pool_size)];

  /*-------------------------------------------------------------------*/
  /* Look if sector is in the cache.                                   */
  /*-------------------------------------------------------------------*/
  for (; entry; entry = entry->next_hash)
    if (entry->sect_num == old_sector)
      UpdateCache(entry, new_sector, C);
}

/***********************************************************************/
/*     InCache: Check if a sector is in cache                          */
/*                                                                     */
/*      Inputs: C = pointer to cache structure                         */
/*              sector = sector to check for                           */
/*              buf = pointer to data if sector in cache               */
/*                                                                     */
/*     Returns: Cache entry if sector in cache, NULL otherwise         */
/*                                                                     */
/***********************************************************************/
CacheEntry* InCache(const Cache *C, int sector, ui8 **buf)
{
  CacheEntry *entry = C->hash_tbl[hash(sector, C->pool_size)];

  /*-------------------------------------------------------------------*/
  /* Look if sector is in the cache.                                   */
  /*-------------------------------------------------------------------*/
  for (; entry; entry = entry->next_hash)
    if (entry->sect_num == sector)
    {
      *buf = entry->sector;
      return entry;
    }

  return NULL;
}

