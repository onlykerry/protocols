/***********************************************************************/
/*                                                                     */
/*   Module:  ramfsp.h                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2004.0                                                   */
/*   Purpose: RAM file system private include file                     */
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
#ifndef _RAMFS_H /* Don't include this file more than once */
#define _RAMFS_H

#include "../include/libc/errno.h"
#include "../include/libc/string.h"
#include "../include/libc/stdlib.h"
#include "../posix.h"
#include "../include/fsprivate.h"
#include "../include/sys.h"

#if NUM_RFS_VOLS
/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
#define RAM_SECT_SZ        1000
#define SEEK_PAST_END      0xFFFFFFFD

/***********************************************************************/
/* Type Declarations                                                   */
/***********************************************************************/
/*
** Structure to hold all global variables for instance of RAM
*/
typedef struct
{
  FileSys  sys;                 /* node in MountedList */
  RFSEnts *files_tbl;           /* table to hold fs entries */
  ui32     fileno_gen;          /* file number generator */
  ui32     total_free;          /* num of free fs entries */
  ui32     opendirs;            /* # of outstanding opendir() calls */
  ui32     num_sects;           /* total number of sectors */
} RamGlob;

typedef struct ram_sect RamSect;

struct ram_sect
{
  RamSect *prev;
  RamSect *next;
  ui8      data[RAM_SECT_SZ];
};

/***********************************************************************/
/* Variable Declarations                                               */
/***********************************************************************/
extern RamGlob *Ram;
extern RamGlob  RamGlobals[NUM_RFS_VOLS];
extern SEM      RamSem;

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
RFSEnts *GetRamTable(void);
int RamWrite(FILE *stream, const ui8 *buf, ui32 len);
int RamRead(FILE *stream, ui8 *buf, ui32 len);
void *RamIoctl(FILE *stream, int code, ...);
void *RamSeekPastEnd(i32 offset, FILE *stream);
int RamNewSect(RCOM_T *entry);
void RamFreeSect(RCOM_T *entry);
void RamPoint2End(fpos_t *pos_ptr, const RCOM_T *file_entry);

#endif /* NUM_RFS_VOLS */
#endif /* _RAMFILESYS_H */

