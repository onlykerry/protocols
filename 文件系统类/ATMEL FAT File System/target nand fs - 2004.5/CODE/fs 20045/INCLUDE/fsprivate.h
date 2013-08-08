/***********************************************************************/
/*                                                                     */
/*   Module:  fsprivate.h                                              */
/*   Release: 2004.5                                                   */
/*   Version: 2004.9                                                   */
/*   Purpose: File Systems private include file                        */
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
#ifndef _FSPRIVATE_H   /* Don't include this file more than once */
#define _FSPRIVATE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "targetos.h"
#include "kernel.h"
#include "libc/stdio.h"

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
#define F_ASYNC     (1 << 5)
#define F_NONBLOCK  (1 << 4)
#define F_APPEND    (1 << 3)
#define F_EXECUTE   (1 << 2)
#define F_READ      (1 << 1)
#define F_WRITE     (1 << 0)

/*
** FSEARCH codes
*/
#define FIRST_DIR   0
#define CURR_DIR    1
#define PARENT_DIR  2
#define ACTUAL_DIR  3
#define DIR_FILE    4

/*
** The different types of FileTable entries
*/
#define FEMPTY      0
#define FDIREN      1
#define FCOMMN      2
#define FFILEN      3

/*
** The different types of sectors
*/
#define FOTHR       0
#define FHEAD       1
#define FTAIL       2

/*
** Return values for the read control function
*/
#define FREAD_ERR   -1
#define FREAD_VAL   0
#define FREAD_EMP   1

#define FNUM_ENT    20   /* Number of entries per file table */

/*
** Flag values for writing/skipping control information
*/
#define FFLUSH_DO   1
#define FFLUSH_DONT 0

/*
** Flag values for adjusting file pointers in file control blocks
*/
#define ADJUST_FCBs        TRUE
#define DONT_ADJUST_FCBs   FALSE

/*
** Flag values for the file/directory structure
*/
#define FCB_DIR     1
#define FCB_FILE    2
#define FCB_TTY     4
#define FCB_MOD     8

/*
** Flag values for FSearchFSUID()
*/
#define FILE_NAME   1
#define PATH_NAME   2

/*
** Flag values for the FFS driver flag
*/
#define FFS_QUOTA_ENABLED   (1 << 0)

/*
** Return values for TFFS recycle function
*/
#define RECYCLE_FAILED      -1
#define RECYCLE_NOT_NEEDED  1
#define RECYCLE_OK          0

#define REMOVED_LINK        0xFFFFFFFF  /* Value assigned to next and */
                                        /* prev for removed link */
#define OFF_REMOVED_LINK    0xFFFE

#define ROOT_DIR_INDEX      FOPEN_MAX   /* index in Files[] for root */

/*
** Maximum allowed number of FFS_VOLS + FAT_VOLS with NAND_FTLS or fixed
** This value is used to issue volume ID numbers for the FSUID when a
** serial number is not provided
*/
#define MAX_FIXED_VOLS  16

/*
** Number of bits the file id, volume id take in the 32 bit FSUID
*/
#define FID_LEN         20
#define VID_LEN         8

#define FID_MASK        0xFFFFF /* MUST be 2^FID_LEN - 1 */
#define VID_MASK        0xFF    /* MUST be 2^VID_LEN - 1 */
#define FSUID_MASK      ((VID_MASK << FID_LEN) | FID_MASK)

/***********************************************************************/
/* Macro Definitions                                                   */
/***********************************************************************/
#if FALSE              /* TRUE for flash testing, FALSE for production */
void AssertError(int line, char *file);
#define PfAssert(c) if (c)  \
                      {}    \
                    else    \
                      AssertError(__LINE__, __FILE__)
#else
#define PfAssert(c)
#endif

extern struct tcb *RunningTask;
#define set_errno(e)    (*(int *)((ui32)RunningTask + 12) = (e))
#define get_errno()     (*(int *)((ui32)RunningTask + 12))

/*
** Create an FSUID val(ui32) based on volume number and file number
*/
#define fsuid(vid, fid) \
  (ui32)((((vid) & VID_MASK) << FID_LEN) | ((fid) & FID_MASK))

/*
** Accessor macros for volume id, file id from an FSUID
*/
#define get_vid(fsuid) (((fsuid) & (VID_MASK << FID_LEN)) >> FID_LEN)
#define get_fid(fsuid) (fsuid & FID_MASK)

#if IGNORE_CASE
/*
** Map standard strcmp/strncmp to case-insensitive string comparision
** functions
*/
#define strcmp   strcasecmp
#define strncmp  strncasecmp
#endif

/*
** Valid TargetFAT partition types
*/
#define FAT_12BIT  0x01
#define FAT_16BIT  0x04
#define FAT_BIGDOS 0x06

/***********************************************************************/
/* Type Definitions                                                    */
/***********************************************************************/
typedef struct file_sys FileSys;
struct file_sys
{
  FileSys *next;
  FileSys *prev;
  char     name[FILENAME_MAX];
  void    *(*ioctl)(FILE *stream, int code, ...);
  void    *volume;
};

typedef struct
{
  ui32 wear_count;
  ui16 used_sects;
  ui8  ctrl_block;
  ui8  bad_block;
} FFSBlock;

typedef struct head_entry
{
  FileSys *head;
  FileSys *tail;
} HeadEntry;

typedef struct
{
  int  (*write_page)(void *buffer, ui32 addr, ui32 type, ui32 wear,
                     void *vol);
  int  (*write_type)(ui32 addr, ui32 type, void *vol);
  int  (*read_page)(void *buffer, ui32 addr, ui32 wear, void *vol);
  ui32 (*read_type)(ui32 addr, void *vol);
  int  (*page_erased)(ui32 addr, void *vol);
  int  (*erase_block)(ui32 addr, void *vol);
} FsNandDriver;

typedef struct
{
  int (*write_byte)(ui32 addr, ui8 data, void *vol);
  int (*write_page)(void *buffer, ui32 addr, void *vol);
  int (*page_erased)(ui32 addr, void *vol);
  int (*read_page)(void *buffer, ui32 addr, void *vol);
  int (*erase_block)(ui32 addr, void *vol);
} FsNorDriver;

typedef union
{
  FsNandDriver nand;
  FsNorDriver  nor;
} FlashDriver;

typedef struct
{
  char *name;        /* name of this volume */
  ui32 block_size;   /* minimum erasable block size in bytes */
  ui32 page_size;    /* page size in bytes */
  ui32 num_blocks;   /* number of blocks in volume */
  ui32 mem_base;     /* volume base address */
  void *vol;         /* driver's volume pointer */
  ui32 max_bad_blocks;
  ui32 flag;
  FlashDriver driver;
} FfsVol;

/*
** A partition entry in the partition table
** Following values for system_id are supported:
** 0x01 = 12 bit FAT primary with fewer than 32680 sectors
** 0x04 = 16 bit FAT primary with between 32680 and 65535 sectors
** 0x05 = extended partition
** 0x06 = BIGDOS FAT primary or logical drive
*/
typedef struct
{
  ui8  boot_id;      /* 0x80 if system partition, 0x00 otherwise */
  ui8  start_head;   /* starting head */
  ui8  start_sect;   /* starting sector in track */
  ui16 start_cyl;    /* starting cylinder */
  ui8  system_id;    /* type of volume (FAT, etc.) */
  ui8  end_head;     /* ending head */
  ui8  end_sect;     /* ending sector in track */
  ui16 end_cyl;      /* ending cylinder */
  ui32 first_sect;   /* first actual sector of partition (from 0) */
  ui32 num_sects;    /* total number of sectors in partition */
} Partition;

typedef struct
{
  char *name;                    /* volume name */
  ui32  serial_num;              /* volume serial number - for FSUID */
  ui32  cached_fat_sects;        /* how many RAM sectors to hold FAT */
  ui32  num_heads;               /* number of heads */
  ui32  sects_per_trk;           /* sectors per track */
  ui32  start_sect;              /* starting sector for partition */
  ui32  num_sects;               /* total volume number of sectors */
  ui8   desired_sects_per_clust; /* desired cluster size */
  ui8   desired_type;            /* desired FAT type */
  ui8   fixed;                   /* fixed/removable media flag */
  void *vol;                     /* driver's volume pointer */

  /*
  ** Driver functions
  */
  int (*write_sectors)(void *buf, ui32 first_sect, int count, void *vol);
  int (*read_sectors)(void *buf, ui32 first_sect, int count, void *vol);
  int (*report) (void *vol, ui32 msg);
} FatVol;

typedef struct
{
  ui32 block_size;     /* size of a block in bytes */
  ui32 num_blocks;     /* total number of blocks */
  ui32 max_bad_blocks; /* maximum number that can go bad */
  ui32 mem_base;       /* base address */
  void *vol;           /* driver's volume pointer */

  /*
  ** Driver functions
  */
  int  (*write_page)(ui32 addr, void *data, void *spare, void *vol);
  int  (*read_page)(ui32 addr, void *data, void *spare, void *vol);
  int  (*write_spare)(ui32 addr, void *spare, void *vol);
  void (*read_spare)(ui32 addr, void *spare, void *vol);
  int  (*page_erased)(ui32 addr, void *vol);
  int  (*erase_block)(ui32 addr, void *vol);
} FtlNandVol;

/*
** FILE implementation for stdio.h, DIR implementation for posix.h
*/
typedef struct cache_entry CacheEntry;
struct file
{
  void   (*acquire)(const FILE *file, int rwcode);
  void   (*release)(const FILE *file, int rwcode);
  void   *handle;       /* self handle to be passed to ioctl */
  void   *volume;       /* file system file/dir belongs to */
  void   *pos;          /* directory position; used for readdir */
  void   *(*ioctl)(FILE *stream, int code, ...);
  int    (*read)(FILE *stream, ui8 *buf, ui32 len);
  int    (*write)(FILE *stream, const ui8 *buf, ui32 len);
  int    hold_char;
  int    errcode;
  struct dirent dent;   /* to be used for readdir() */
  CacheEntry *cached;   /* pointer to current cached sector */
  fpos_t curr_ptr;      /* current position in file */
  fpos_t old_ptr;       /* previous position in file */
  ui32   flags;         /* file/dir specific flags */
  ui32   parent;        /* parent directory */
};

/*
** Ioctl() Commands
*/
typedef enum
{
  FFLUSH = 4, FSEEK, FTELL, FOPEN, FEOF, FGETPOS, FCLOSE, FREOPEN,
  FSETPOS, REMOVE, FSEARCH, SETVBUF, CHDIR, GETCWD, MKDIR, RMDIR,
  STAT, UTIME, OPENDIR, READDIR, CLOSEDIR, CLOSE, LINK, OPEN, TMPFILE,
  TMPNAM, FSTAT, CURR_PTR, STATS, UNMOUNT, VSTAT, CHMOD, CHOWN,
  ACCESS, DUP, ENABLE_SYNC, DISABLE_SYNC, FTRUNCATE, TRUNCATE,
  SET_FL, GET_FL, CREATN, RENAME, VCLEAN, CHDIR_DEC, SORTDIR, ATTRIB,
  GET_FSUID, GET_NAME, GET_QUOTAM, GET_QUOTA
} IOCTLS;

/*
** FAT Events
*/
typedef enum
{
  FAT_MOUNT, FAT_UNMOUNT
} FAT_EVENTS;

typedef struct f_f_e FFSEnt;
typedef struct r_f_e RFSEnt;

typedef struct
{
  ui16 sector;
  ui16 offset;
} OffLoc;

/*
** Represents structure of common info between dir and file in RAM
*/
typedef struct
{
  FFSEnt *addr;                /* entry location in RAM */
  time_t  mod_time;            /* last modified time for entry */
  time_t  ac_time;             /* last access time for entry */
  ui32    size;                /* size of file (0 for dirs) */
  ui32    fileno;              /* file/dir number */
  ui32    attrib;              /* FsAttribute() field */
  uid_t   user_id;             /* used ID */
  gid_t   group_id;            /* group ID */
  mode_t  mode;                /* file/dir create mode */
  ui8     links;               /* number of links to file/dir */
  ui8     open_links;          /* number of open links to file/dir */
  ui8     open_mode;           /* file/dir open mode */
  OffLoc  one_past_last;       /* pointer to one past the EOF */
  ui16    frst_sect;           /* first sector for files (0 for dirs) */
  ui16    last_sect;           /* last sector for files (0 for dirs) */
} FCOM_T;

/*
** RFS variation of comm (MUST agree with FCOM_T up to mode_t)
** otherwise SetPerm() and CheckPerm() won't work
*/
typedef struct
{
  RFSEnt *addr;                /* entry location in RAM */
  time_t  mod_time;            /* last modified time for entry */
  time_t  ac_time;             /* last access time for entry */
  ui32    size;                /* size of file (0 for dirs) */
  ui32    fileno;              /* file/dir number */
  ui32    attrib;              /* FsAttribute() field */
  uid_t   user_id;             /* user ID */
  gid_t   group_id;            /* group ID */
  mode_t  mode;                /* file/dir create mode */
  ui8     links;               /* number of links to file/dir */
  ui8     open_links;          /* number of open links to file/dir */
  ui8     open_mode;           /* file/dir open mode */
  ui32    one_past_last;       /* offset within last sector */
  void   *frst_sect;           /* first sector in file */
  void   *last_sect;           /* last sector in file */
  ui8     temp;                /* indicates temporary file */
} RCOM_T;

/*
** Represents structure of a link to a file as it is used in RAM
*/
typedef struct
{
  char    name[FILENAME_MAX+1];/* link name */
  FFSEnt *next;                /* next entry in parent directory */
  FFSEnt *prev;                /* prev entry in parent directory */
  FCOM_T *comm;                /* pointer to actual file info */
  FFSEnt *parent_dir;          /* pointer to parent directory */
  FILE   *file;                /* FILE* associated with this link */
} FFIL_T;

/*
** RFS variation of link
*/
typedef struct
{
  char    name[FILENAME_MAX+1];/* link name */
  RFSEnt *next;                /* next entry in parent directory */
  RFSEnt *prev;                /* prev entry in parent directory */
  RCOM_T *comm;                /* pointer to actual file info */
  RFSEnt *parent_dir;          /* pointer to parent directory */
  FILE   *file;                /* FILE* associated with this link */
} RFIL_T;

/*
** Represents structure of a link to a dir as it is used in RAM
*/
typedef struct
{
  char    name[FILENAME_MAX+1];/* directory name */
  FFSEnt *next;                /* next entry in parent directory */
  FFSEnt *prev;                /* prev entry in parent directory */
  FCOM_T *comm;                /* pointer to actual dir info */
  FFSEnt *parent_dir;          /* pointer to parent directory */
  DIR    *dir;                 /* DIR* associated with this link */
  FFSEnt *first;               /* head of contents list for dir */
  ui32    cwds;                /* current working directory count */
#if QUOTA_ENABLED
  ui32    max_q;               /* max quota */
  ui32    min_q;               /* min quota */
  ui32    used;                /* used space */
  ui32    free;                /* free space */
  ui32    free_below;          /* free space below */
  ui32    res_below;           /* reserved below */
#endif /* QUOTA_ENABLED */
} FDIR_T;

/*
** RFS variation of dir
*/
typedef struct
{
  char    name[FILENAME_MAX+1];/* directory name */
  RFSEnt *next;                /* next entry in parent directory */
  RFSEnt *prev;                /* prev entry in parent directory */
  RCOM_T *comm;                /* pointer to actual dir info */
  RFSEnt *parent_dir;          /* pointer to parent directory */
  DIR    *dir;                 /* DIR* associated with this link */
  RFSEnt *first;               /* head of contents list for dir */
  ui32    cwds;                /* current working directory count */
} RDIR_T;

/*
** Represents the type for an entry in RAM (dir, file or link)
*/
union f_file_entry
{
  FDIR_T dir;
  FCOM_T comm;
  FFIL_T file;
};

/*
** Represents the type for an entry for RFS (dif, file or link)
*/
union r_file_entry
{
  RDIR_T dir;
  RCOM_T comm;
  RFIL_T file;
};

/*
** An entry in RAM consists of its type and value as a union
*/
struct f_f_e
{
  union f_file_entry entry;
  ui8   type;
};

/*
** An entry in RFS consists of its type and value as a union
*/
struct r_f_e
{
  union r_file_entry entry;
  ui8   type;
};

/*
** Holds a table full of entries, pointers to next and prev, num free
*/
typedef struct flash_entries FFSEnts;
struct flash_entries
{
  FFSEnt   tbl[FNUM_ENT];
  FFSEnts *next_tbl;
  FFSEnts *prev_tbl;
  int      free;
};

typedef struct ram_entries RFSEnts;
struct ram_entries
{
  RFSEnt   tbl[FNUM_ENT];
  RFSEnts *next_tbl;
  RFSEnts *prev_tbl;
  int      free;
};

/*
** Type of an entry in the Sectors table
*/
typedef struct
{
  ui16  next;  /* index of next sector in list of used sectors */
  ui16  prev;  /* index of prev sector in list of used sectors */
} Sectors;

/*
** Structure to cast all volume structures to, to retrieve ioctl_func
*/
typedef struct
{
  FileSys sys;
} FsVolume;

/***********************************************************************/
/* Variable Declarations                                               */
/***********************************************************************/
#define Files  FilesFFS
extern FILE  Files[FOPEN_MAX + 1];

#define MountedList     MountedListFFS
extern HeadEntry MountedList;

#define FileSysSem      FileSysSemFFS
extern SEM       FileSysSem;

#define FSPath          FSPathFFS
extern const char *FSPath;

#define CurrFixedVols   CurrFixedVolsFFS
extern int CurrFixedVols;

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
#define InvalidName     InvalidNameFFS
int  InvalidName(const char *name, int ignore_last);

void FileSysInit(void);

#define IsLast          IsLastFFS
ui32 IsLast(const char *name, ui32 *incr);

int  FsNorAddVol(const FfsVol *vol);
int  FsNandAddVol(const FfsVol *vol);
int  FatAddVol(const FatVol *vol);
int  FtlNandAddVol(FtlNandVol *ftl_drvr, FatVol *fat_drvr);

#define FSearch         FSearchFS
void *FSearch(void *handle, const char **path, int dir_lookup);

#define FSearchFSUID    FSearchFSUIDFS
char *FSearchFSUID(ui32 fsuid, char *buf, size_t size, int lookup);

#define CheckPerm       CheckPermFFS
int  CheckPerm(FCOM_T *comm_ptr, int permissions);

#define SetPerm         SetPermFFS
void SetPerm(FCOM_T *comm_ptr, mode_t mode);

#define FsInitFCB       FsInitFCBFFS
void FsInitFCB(FILE *file, ui32 type);

int ReadPartitions(const FatVol *fat_vol, Partition *partitions,
                   int max_partitions);
int WritePartitions(const FatVol *fat_vol, Partition *partitions,
                    int num_partitions);

void GenEccTable(void);
void EncodeEcc(const ui32 *page, ui8 *ecc);
int  DecodeEcc(ui32 *page, const ui8 *ecc);

void FtlNandEncode(const ui8 *data, ui8 *ecc);
int  FtlNandDecode(ui8 *data, const ui8 *ecc);

#define QuickSort       QuickSortFFS
void QuickSort(FFSEnt **head, FFSEnt **tail, DirEntry *e1,
               DirEntry *e2, int (*cmp) (const DirEntry *,
                                         const DirEntry *));

#ifdef __cplusplus
}
#endif

#endif /* _FSPRIVATE_H */

