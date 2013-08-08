/***********************************************************************/
/*                                                                     */
/*   Module:  flashfsp.h                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2004.4                                                   */
/*   Purpose: Flash File System Private Include File                   */
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
#ifndef _FLASHFSP_H /* Don't include this file more than once */
#define _FLASHFSP_H

#include <assert.h>

#include "../include/libc/errno.h"
#include "../include/libc/string.h"
#include "../include/libc/stdlib.h"

#include "../posix.h"
#include "../include/fsprivate.h"
#include "../include/sys.h"
#include "../include/cache.h"

#if NUM_FFS_VOLS
/***********************************************************************/
/* Configuration                                                       */
/***********************************************************************/
#define BACKWARD_COMPATIBLE     TRUE
#define FFS_DEBUG               FALSE

/*
** Extra free space (not available to the app) for faster performance.
** To be used in conjunction with vclean()
*/
#define EXTRA_FREE              0   /* in bytes */

/*
** If 1, changes the machine endianess for the control information
** when it is written to / read from flash.
*/
#define FFS_SWAP_ENDIAN         0

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
#define INIT_SIG        9
/*
** NAND signature bytes
*/
#define NAND_BYTE1      'T'
#define NAND_BYTE2      'A'
#define NAND_BYTE3      '1'
#define NAND_BYTE4      '1'
#define CURR_NAND_SIG   11

/*
** NOR signature bytes
*/
#define NOR_BYTE1       'T'
#define NOR_BYTE2       'A'
#define NOR_BYTE3       '1'
#define NOR_BYTE4       '2'
#define CURR_NOR_SIG    12

/*
** MLC signature bytes
*/
#define MLC_BYTE1       'T'
#define MLC_BYTE2       'A'
#define MLC_BYTE3       '1'
#define MLC_BYTE4       '0'
#define CURR_MLC_SIG    10

/*
** NOR/MLC slow mount flag values
*/
#define SLOW_MOUNT  0
#define FAST_MOUNT  0xFF

#define RES_BYTES   4       /* Number of reserved bytes in NOR header */

#define FFREE_SECT  0xFFFF /* Values for the sector table */
#define FDRTY_SECT  0xFFFE
#define FHDER_SECT  0xFFFD /* NOR only */
#define FNVLD_SECT  0xFFFD /* NAND only - MUST be same as FHDER_SECT */
#define FLAST_SECT  0xFFFC
#define FCONT_SGMT  FDRTY_SECT

#define SKIP_CHECK_FULL    FALSE
#define CHECK_FULL         TRUE

#define SKIP_RMV_FROM_CACHE FALSE
#define RMV_FROM_CACHE      TRUE

#define DATA_SECT       0xFFFFFFFF
#define CTRL_SECT       0x0000FFFF
#define OLD_CTRL_SECT   0xFFFF0000

#define USE_WEAR        TRUE
#define IGNORE_WEAR     FALSE

#define ADJUST_ERASE       TRUE
#define SKIP_ADJUST_ERASE  FALSE

#define CHECK_RECYCLE      TRUE
#define SKIP_CHECK_RECYCLE FALSE

#define FFS_RECYCLE     1
#define FFS_NO_RECYCLE  2

#define FLASH_SET_LIMIT 4

#define DO_SYNC         TRUE

#define CONSEC_DISTURBS    2

/*
** Number of bytes written to flash for each type of entry
*/
#define OFFIL_SZ           16
#define OFDIR_SZ           20
#define OFDIR_QUOTA_SZ     24
#if BACKWARD_COMPATIBLE
#define OFCOM_SZ           36
#else
#define OFCOM_SZ           35
#endif

/***********************************************************************/
/* Macro Definitions                                                   */
/***********************************************************************/
#define used_sect(sect)     (Flash->sect_tbl[sect].prev < FHDER_SECT)

#define sig_bytes_val(sig_byte2, sig_byte3) \
  ((sig_byte2 - '0') * 10 + (sig_byte3 - '0'))

#define check_filenos(fileno) ((fileno) & ~FID_MASK)

/***********************************************************************/
/* Type Declarations                                                   */
/***********************************************************************/
/*
** Structure to hold all global variables for instance of flash
*/
typedef struct
{
  /*
  ** Global Structure Variables
  */
  FileSys   sys;                /* node in MountedList */
  Cache     cache;              /* cache for flash */
  SEM       sem;                /* volume semaphore */
  ui32      free_sect;          /* first free sector available */
  ui32      free_ctrl_sect;     /* first free sector available for ctrl*/
  ui32      last_free_sect;     /* last free sector available */
  Sectors  *sect_tbl;           /* list of sectors */
  FFSEnts  *files_tbl;          /* table to hold fs entries */
  FFSBlock *blocks;             /* statistics about each block */
  int      *erase_set;          /* array of next erase set block #'s */
  ui32      set_blocks;         /* number of blocks in erase set  */
  ui32      total_free;         /* num of free fs entries */
  ui32      total;              /* num of total fs entries */
  ui32      free_sects;         /* num of free sectors */
  ui32      used_sects;         /* total number of used sectors */
  ui32      seq_num;            /* sequence number for ctrl info */
  ui32      high_wear;          /* highest wear count encountered */
  ui32      fileno_gen;         /* file number generator */
  ui32      tbls_size;          /* size of file tables in control info */
  ui32      ctrl_size;          /* control size in bytes */
  ui32      ctrl_sects;         /* number of ctrl sectors */
  ui32      ctrl_sect;          /* curr one for read/write ctrl info */
  ui32      frst_ctrl_sect;     /* pointer to first ctrl sector */
  ui32      last_ctrl_sect;     /* pointer to last ctrl sector */
  ui8      *tmp_sect;           /* temp sect for use by fs internals */
  ui32      opendirs;           /* # of outstanding opendir() calls */
  ui32      bad_blocks;         /* current number of bad blocks */
  ui32      free_sects_used;    /* free sects used b4 count ctrl */
  ui32      vid;                /* volume id to be used with FSUID */
  void     *comm_ptr;           /* used with free sects used */
  ui8       enforce_wear;       /* flag for FlashAdjustEraseSet() */
  ui8       type;               /* FFS_NOR or FFS_NAND */
  ui8       do_sync;            /* flag that enables FlashSync() */
  ui8       sync_deferred;      /* prior disable_sync() call */
  ui8       wr_ctrl;            /* indicate if ctrl just written */
  ui8       fast_mount;         /* flag to indicate if slow mount mark */
                                /*   for NOR has been set */
#if QUOTA_ENABLED
  ui8       quota_enabled;      /* flag to indicate if vol has quotas */
#endif /* QUOTA_ENABLED */

  /*
  ** NOR/NAND specific function handles
  */
  int (*write_ctrl)(const void *head, uint length);
  int (*write_sector)(void *buffer, ui32 sect_num, ui32 sect_type);
  int (*read_sector)(void *buffer, ui32 sect_num);
  int (*unformat_to_format)(void);
  int (*find_last_ctrl)(void);
  int (*empty_sect)(ui32 sect_num);
  int (*erase_block_wrapper)(ui32 addr);

  /*
  ** Driver Functions
  */
  FlashDriver driver;           /* union containing flash driver funcs */

  /*
  ** Driver Dependent Variables
  */
  ui32  sect_sz;        /* sector size in bytes */
  ui32  pages_per_sect; /* number of pages in a sector */
  ui32  num_sects;      /* total number of sectors in the flash */
  ui32  num_blocks;     /* total number of blocks */
  ui32  max_set_blocks; /* maximum number of block in erase set */
  ui32  block_size;     /* block size in bytes */
  ui32  page_size;      /* page size in bytes */
  ui32  block_sects;    /* number of sectors in a block */
  ui32  max_bad_blocks; /* maximum number of bad blocks */
  ui32  hdr_sects;      /* number of header sectors per block */
  ui32  mem_base;       /* beginning of flash space */
  void *vol;            /* argument passed up from the driver */
} FlashGlob;

/***********************************************************************/
/* Variable Declarations                                               */
/***********************************************************************/
extern FlashGlob  FlashGlobals[NUM_FFS_VOLS];
extern SEM        FlashSem;
extern FlashGlob *Flash;

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
int   FlashRdCtrl(int partial_read);
int   FlashWrCtrl(int adjust_erase);
int   FlashSemiMount(void);
void *FlashSync(int do_sync);

int   FlashWrite(FILE *stream, const ui8 *buf, ui32 len);
int   FlashRead(FILE *stream, ui8 *buf, ui32 len);
void *FlashIoctl(FILE *stream, int code, ...);
int   FlashWrprWr(CacheEntry *entry, int update);
int   FlashWriteCtrlSect(void);
void *FlashSeekPastEnd(i32 offset, FILE *stream, int adjust);

ui32  FlashRoom(ui32 create_entity_incr);
int   FlashFrSect(void *comm_ptr);
int   FlashRelocSect(ui32 curr_sect, ui32 new_sect, const void *buffer);
int   FlashEraseBlock(ui32 block);

FFSEnts *FlashGetTbl(void);
FFSEnt  *FlashConvertToPtr(ui32 entry);

void FlashRecycleValues(ui32 *ctrl_blks, ui32 *used, ui32 *blk_sects);
int  FlashRecChck(int check_full);
int  FlashRecNeeded(void);
int  FlashRecycle(int check_full);
void FlashAdjustEraseSet(ui32 candidate_block);
void FlashChooseEraseSet(int enforce_wear_limit);
void FlashCountAndChoose(int check_recycle);
void FlashCheckDelLinks(void);
void FlashTruncateNames(void);
void FlashUpdateFCBs(const FILE *skip, const FCOM_T *comm_ptr);

void FlashGarbageCollect(void);
void FlashIncrementTblEntries(const FFSEnt *ptr);
void FlashTruncZero(const FFIL_T *entry, int adjust);
void FlashPoint2End(fpos_t *pos_ptr, const FCOM_T *file_entry);
int  FlashInvalidSignature(void);
int  FlashClean(void);
int  FlashAddVol(const FfsVol *vol, int type);

#if QUOTA_ENABLED
ui32 FlashFreeBelow(FFSEnt *ent);
ui32 FlashVirtUsed(const FFSEnt *dir);
void FlashFree(FFSEnt *dir, ui32 root_spare);
#endif /* QUOTA_ENABLED */

/*
** NOR specific functions
*/
int FsNorWriteCtrl(const void *head, uint length);
int FsNorCheckCtrl(const void *head, uint length);
int FsNorWriteSect(void *buffer, ui32 sect_num, ui32 sect_type);
int FsNorReadSect(void *buffer, ui32 sect_num);
int FsNorEraseBlockWrapper(ui32 addr);
int FsNorUnformatToFormat(void);
int FsNorFindLastCtrl(void);
int FsNorEmptySect(ui32 sector);
int FsNorOldRdCtrl(void);

/*
** NAND specific functions
*/
int FsNandWriteCtrl(const void *head, uint length);
int FsNandWriteSect(void *buffer, ui32 sect_num, ui32 sect_type);
int FsNandReadSect(void *buffer, ui32 sect_num);
int FsNandEraseBlockWrapper(ui32 addr);
int FsNandUnformatToFormat(void);
int FsNandFindLastCtrl(void);
int FsNandEmptySect(ui32 sector);
int FsNandWriteType(ui32 sect_num, ui32 type);

/*
** MLC specific functions
*/
int FsMlcWriteCtrl(const void *head, uint length);
int FsMlcWriteSect(void *buffer, ui32 sect_num, ui32 sect_type);
int FsMlcReadSect(void *buffer, ui32 sect_num);
int FsMlcEraseBlockWrapper(ui32 addr);
int FsMlcUnformatToFormat(void);
int FsMlcFindLastCtrl(void);
int FsMlcEmptySect(ui32 sector);
int FsMlcWriteType(ui32 sect_num, ui32 type);

int FlashCheckSectTbl(FlashGlob *vol);

#endif /* NUM_FFS_VOLS */
#endif /* _FLASFSP_H */

