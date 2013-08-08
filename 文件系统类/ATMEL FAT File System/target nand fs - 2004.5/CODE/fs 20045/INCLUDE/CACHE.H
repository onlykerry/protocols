/***********************************************************************/
/*                                                                     */
/*   Module:  cache.h                                                  */
/*   Release: 2004.5                                                   */
/*   Version: 2004.1                                                   */
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
#ifndef _CACHE  /* Don't include this file more than once */
#define _CACHE

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
#define DIRTY_OLD    2
#define DIRTY_NEW    1
#define CLEAN        0

#define GET_OK             0
#define GET_WRITE_ERROR    1
#define GET_READ_ERROR     2

#define CACHE_UI8_ALLIGN   1
#define CACHE_UI16_ALLIGN  2
#define CACHE_UI32_ALLIGN  4

/***********************************************************************/
/* Type Declarations                                                   */
/***********************************************************************/
typedef int (*MedWFunc)(CacheEntry *entry, int update);
typedef int (*MedRFunc)(void *head, ui32 sect_num);

struct cache_entry
{
  CacheEntry *next_lru;      /* Next and prev for the LRU list is */
  CacheEntry *prev_lru;      /* a double linked list */
  CacheEntry *next_hash;     /* Next and prev for the hash list is */
  CacheEntry *prev_hash;     /* a double linked list */
  CacheEntry **hash_loc;     /* pointer to location in hash table */
  ui8        *sector;        /* Pointer to the data for this sector */
  void       *file_ptr;      /* Pointer to file control information */
  int        sect_num;       /* Sector number in actual medium */
  ui16       pin_cnt;        /* Pin counter: 0 = unpinned, > 0 pinned */
  ui16       dirty;          /* Dirty flag */
};

typedef struct
{
  CacheEntry *pool;          /* Array containing all cache entries */
  CacheEntry **hash_tbl;     /* Hash table to point to pool */
  CacheEntry *lru_head;      /* Head of the LRU list */
  CacheEntry *lru_tail;      /* Tail of the LRU list */
  int        pool_size;      /* Number of cache entries */
  int        sector_number;  /* Current sector being worked on */
  MedWFunc   write;          /* Write function to write sector back */
  MedRFunc   read;           /* Read function to read sector */
  ui8        dirty_old;      /* Flag set when dirty old sectors */
  ui8        dirty_new;      /* Flag set when dirty new sectors */
} Cache;

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
ui32 InitCache(Cache *C, int pool_size, MedWFunc writef, MedRFunc readf,
               int sect_sz, int temp_sectors, int alignment);
void ReinitCache(Cache *C, int sect_sz);
void DestroyCache(Cache *C);
void RemoveFromCache(Cache *C, int sector_number, int new_sector);
int  GetSector(Cache *C, int sector_number, int skip_read,
               void *file_ptr, CacheEntry **entry_ptr);
int  FreeSector(CacheEntry **entry, Cache *C);
int  FlushSectors(Cache *C);
void *FlushFileSectors(Cache *C, const void *file_ptr);
void UpdateCache(CacheEntry *entry, int new_sector, const Cache *C);
void UpdateCacheSectNum(Cache *C, int old_sector, int new_sector);
void UpdateFilePtrs(Cache *C, const void *old_fptr, void *new_fptr);
CacheEntry *InCache(const Cache *C, int sector, ui8 **buf);

#endif /* _CACHE */

