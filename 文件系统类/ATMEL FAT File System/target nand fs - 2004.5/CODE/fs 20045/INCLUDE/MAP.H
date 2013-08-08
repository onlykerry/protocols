/***********************************************************************/
/*                                                                     */
/*   Module:  map.h                                                    */
/*   Release: 2004.5                                                   */
/*   Version: 2004.1                                                   */
/*   Purpose: Mapping definitions                                      */
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
#ifndef _MAP_H /* Don't include this file more than once */
#define _MAP_H

#include "libc/stdlib.h"

/***********************************************************************/
/* Macro Declarations                                                  */
/***********************************************************************/
#define ui32       unsigned int

/***********************************************************************/
/* Type Declarations                                                   */
/***********************************************************************/
typedef ui32 (*MapRdFunc)(ui32, void *, int *);
typedef ui32 (*MapWrFunc)(ui32, void *);

typedef struct map_entry MappingEntry;
struct map_entry
{
  ui32 key;
  ui32 val;
  MappingEntry *next, *prev;
  MappingEntry *next_lru, *prev_lru;
};

typedef struct
{
  ui32 num_entries;
  void *vol;
  MapRdFunc read_val;
  MapWrFunc write_val;
  MappingEntry **hash_tbl;
  MappingEntry *entries;
  MappingEntry *lru_head;
} Mapping;

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
Mapping *CreateMapping(ui32 num_entries, MapRdFunc read_val_func,
                       MapWrFunc write_val_func, void *vol);
void DestroyMapping(Mapping **mapping);
ui32 MapKey(Mapping *mapping, ui32 key, int *new_mapping);
void MapUpdate(const Mapping *mapping, ui32 key, ui32 new_val);
int  MapAdd(Mapping *mapping, ui32 key, ui32 val);

#endif /* _MAP_H */

