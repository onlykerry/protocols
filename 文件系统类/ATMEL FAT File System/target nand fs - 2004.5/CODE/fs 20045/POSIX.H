/***********************************************************************/
/*                                                                     */
/*   Module:  posix.h                                                  */
/*   Release: 2004.5                                                   */
/*   Version: 2004.6                                                   */
/*   Purpose: Combines all standard POSIX files                        */
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
#ifndef _POSIX_H /* Don't include this file more than once */
#define _POSIX_H

#ifdef __cplusplus
extern "C"
{
#endif

/***********************************************************************/
/* Internal base types                                                 */
/***********************************************************************/
#define ui32       unsigned int
#define ui16       unsigned short
#define ui8        unsigned char
#define i32        int

/***********************************************************************/
/* Porting Definitions - must match Standard C headers                 */
/***********************************************************************/
#define FILENAME_MAX    63      /* must be compatible with <stdio.h> */
#define FOPEN_MAX       64      /* must be compatible with <stdio.h> */
#define size_t  unsigned int    /* must be compatible with <stdio.h> */
#define time_t          ui32    /* must be compatible with <time.h> */

/***********************************************************************/
/* Configuration                                                       */
/***********************************************************************/
#define DIROPEN_MAX     FOPEN_MAX   /* max number of open dirs */
#define PATH_MAX        FILENAME_MAX /* maximum path name length */
#define _PATH_NO_TRUNC  1           /* don't truncate path name */
#define IGNORE_CASE     0           /* no path name case sensitivity */

/*
** Cache policy - when set to 0 becomes copy back
*/
#define FFS_CACHE_WRITE_THROUGH     0

/*
** Turn on directory quota code - still need per volume enable flag
** 1 = ON, 0 = OFF
*/
#define QUOTA_ENABLED   0

/*
** Turn on UTF8 checking for new file/dir names for both TFFS/TFAT
** and the UTF8 <-> UTF16 conversions for TFAT only (when VFAT is on)
** 1 = ON, 0 = OFF
*/
#define UTF_ENABLED     0

/*
** Max Number of Volumes of Each Type
*/
#define NUM_RFS_VOLS    1
#define NUM_ZFS_VOLS    0
#define NUM_FFS_VOLS    1
#define NUM_FAT_VOLS    0

/*
** Type of flash support included
*/
#define INC_NAND_FS     1
#define INC_NOR_FS      0
#define INC_OLD_NOR_FS  0

/*
** Type of TargetFAT driver included
*/
#define NUM_NAND_FTLS   0
#define INC_FAT_FIXED   0
#define PCCARD_SUPPORT  0

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
#define dev_t       ui32
#define gid_t       ui16
#define ino_t       ui32
#define mode_t      ui16
#define nlink_t     ui32
#define off_t       i32
#define uid_t       ui16

#define O_RDONLY    0               /* open() flags */
#define O_WRONLY    (1 << 0)
#define O_RDWR      (1 << 1)
#define O_APPEND    (1 << 2)
#define O_CREAT     (1 << 3)
#define O_EXCL      (1 << 4)
#define O_TRUNC     (1 << 5)
#define O_NONBLOCK  (1 << 6)
#define O_ASYNC     (1 << 7)

#define F_DUPFD     1               /* fcntl() flags */
#define F_GETFD     2
#define F_SETFD     3
#define F_GETFL     4
#define F_SETFL     5

#define R_OK                 4      /* access() flags */
#define W_OK                 2
#define X_OK                 1
#define F_OK                 0

#define _PC_LINK_MAX         1      /* fpathconf() flags */
#define _PC_MAX_CANON        2
#define _PC_MAX_INPUT        3
#define _PC_NAME_MAX         4
#define _PC_PATH_MAX         5
#define _PC_PIPE_BUF         6
#define _PC_CHOWN_RESTRICTED 7
#define _PC_NO_TRUNC         8
#define _PC_VDISABLE         9

#define SEEK_SET             0
#define SEEK_CUR             1
#define SEEK_END             2

#define S_CREATN        020000 /* flag to identify creatn() files */
#define S_DIR           040000

#define S_ISUID         004000 /* mkdir()/open() mode flags */
#define S_ISGID         002000
#define S_IRUSR         000400
#define S_IWUSR         000200
#define S_IXUSR         000100
#define S_IRGRP         000040
#define S_IWGRP         000020
#define S_IXGRP         000010
#define S_IROTH         000004
#define S_IWOTH         000002
#define S_IXOTH         000001

#define S_ISDIR(m)      ((m) & S_DIR)
#define S_ISREG(m)      (((m) & S_DIR) == 0)
#define S_ISCHR(m)      0
#define S_ISBLK(m)      0
#define S_ISFIFO(m)     0

/*
** vstat() file system types
*/
#define FFS_VOL        1
#define FAT_VOL        2
#define RFS_VOL        3
#define ZFS_VOL        4

/*
** Various types of FAT
*/
#define FATANY         0
#define FAT12          1
#define FAT16          2
#define FAT32          3

/*
** The types of flash supported
*/
#define FFS_NOR     1
#define FFS_NAND    2

/***********************************************************************/
/* Type definitions                                                    */
/***********************************************************************/
#ifndef _FILE_FFS
#define _FILE_FFS
typedef struct file FILE_FFS;
struct file;
#endif

typedef struct
{
  ui32 vol_type;
  ui32 sect_size;       /* sector size in bytes */
  ui32 num_sects;       /* number of sectors */
  ui32 used_sects;      /* number of used sectors */
  ui32 avail_sects;     /* number of available sectors */
  ui32 sects_2recycle;  /* number of available sectors before recycle */
  ui32 flash_type;      /* FFS_NAND or FFS_NOR */
  ui32 wear_count;      /* maximum wear count */
  ui32 bad_blocks;      /* number of bad blocks in volume (NAND only) */
  ui32 max_bad_blocks;  /* max number of bad blocks allowed */
} vstat_ffs;

typedef struct
{
  ui32 vol_type;
  ui32 sect_size;       /* sector size in bytes */
  ui32 num_sects;       /* number of sectors */
} vstat_rfs;

typedef struct
{
  ui32 vol_type;
  ui32 num_entries;     /* number of files & directories */
  ui32 block_size;      /* uncompressed block size */
  ui32 cblock_size;     /* compressed block size */
} vstat_zfs;

typedef struct
{
  ui32 vol_type;
  ui32 clust_size;      /* cluster size in bytes */
  ui32 num_clusts;      /* number of clusters */
  ui32 used_clusts;     /* number of used clusters */
  ui32 free_clusts;     /* number of free clusters */
  ui32 fat_type;        /* FAT12, FAT16 or FAT32 */
  ui32 root_dir_sects;  /* number of root directory sectors */
  ui32 sect_size;       /* sector size in bytes */
  ui32 sects_per_fat;   /* FAT(1 table) size in sectors */
  ui32 num_fats;        /* number of FAT tables */
} vstat_fat;

union vstat
{
  vstat_ffs ffs;
  vstat_rfs rfs;
  vstat_zfs zfs;
  vstat_fat fat;
};

typedef struct file DIR;

struct dirent
{
  long d_ino;
  char d_name[FILENAME_MAX];
};

struct utimbuf
{
  time_t actime;   /* access time */
  time_t modtime;  /* modification time */
};

struct stat
{
  dev_t   st_dev;    /* ID of device containing this file */
  ino_t   st_ino;    /* file serial number */
  mode_t  st_mode;   /* file mode */
  nlink_t st_nlink;  /* number of links */
  uid_t   st_uid;    /* user ID of file's owner */
  gid_t   st_gid;    /* group ID of file's owner */
  dev_t   st_rdev;   /* device ID (if inode device) */
  off_t   st_size;   /* the file size in bytes */
  time_t  st_atime;  /* time of last access */
  time_t  st_mtime;  /* time of last data modification */
  time_t  st_ctime;  /* time of last status change */
};

/*
** Structure containing file/dir info for sortdir() comparisons
*/
typedef struct
{
  const char *st_name;   /* file name */
  ino_t   st_ino;    /* file serial number */
  mode_t  st_mode;   /* file mode */
  nlink_t st_nlink;  /* number of links */
  uid_t   st_uid;    /* user ID of file's owner */
  gid_t   st_gid;    /* group ID of file's owner */
  off_t   st_size;   /* file size in bytes */
  time_t  st_atime;  /* time of last access */
  time_t  st_mtime;  /* time of last data modification */
  time_t  st_ctime;  /* time of last status change */
} DirEntry;

/*
** Quota information
*/
typedef struct
{
  ui32 quota;          /* max usable space */
  ui32 used;           /* sum of data in dir and subdirs */
  ui32 free;           /* dir free space */
  ui32 reserved;       /* dir reserved space */
  ui32 reserved_below; /* sum of subdir reservations */
  ui32 free_below;     /* amount of free but reserved subdir space */
} FS_QUOTA_INFO;

/*
** sortdir() comparison function type
*/
typedef int (*CompFunc)(const DirEntry *, const DirEntry *);

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
/*
** Standard POSIX API functions
*/
int  access(const char *path, int amode);
int  chdir(const char *path);
int  chmod(const char *path, mode_t mode);
int  chown(const char *path, uid_t owner, gid_t group);
int  close(int fildes);
int  closedir(DIR *dirp);
int  creat(const char *path, mode_t mode);
int  dup2(int fildes, int fildes2);
int  dup(int fildes);
int  fcntl(int fildes, int cmd, ...);
long fpathconf(int fildes, int name);
int  fstat(int fildes, struct stat *buf);
int  ftruncate(int fid, off_t length);
char *getcwd(char *buf, size_t size);
int  isatty(int fildes);
int  link(const char *existing, const char *new_link);
off_t lseek(int fildes, off_t offset, int whence);
int  mkdir(const char *path, mode_t mode);
int  mount(const char *path);
int  open(const char *path, int oflag, ...);
DIR *opendir(const char *dirname);
long pathconf(const char *path, int name);
int  read(int fildes, void *buf, unsigned int nbyte);
struct dirent *readdir(DIR *dirp);
void rewinddir(DIR *dirp);
int  rmdir(const char *path);
int  stat(const char *path, struct stat *buf);
int  truncate(const char *path, off_t length);
int  unlink(const char *path);
int  unmount(const char *path);
int  utime(const char *path, const struct utimbuf *times);
int  write(int fildes, const void *buf, unsigned int nbyte);

/*
** Non-standard API functions
*/
int  creatn(const char *path, mode_t mode, size_t size);
int  disable_sync(const char *path);
int  enable_sync(const char *path);
int  format(const char *path);
int  mount_all(int verbose);
int  sync(int fildes);
int  unformat(const char *path);
int  vclean(const char *path);
int  vstat(const char *path, union vstat *buf);
int  sortdir(const char *path, int (*cmp)(const DirEntry *e1,
                                          const DirEntry *e2));

/*
** Auxiliary functions
*/
int  InitFFS(void);
int  RfsAddVol(const char *name);
int  RfsDelVol(const char *name);
int  ZfsAddVolArray(const char *volume, unsigned char image[]);
int  ZfsAddVolFile(const char *volume, const char *file_path);
int  ZfsDelVol(const char *name);
int  FfsDelVol(const char *name);
int  FatDelVol(const char *name);
int  FtlNandDelVol(const char *name);

void FsGetId(uid_t *uid, gid_t *gid);
void FsSetId(uid_t uid, gid_t gid);
void FsSaveCWD(ui32 word1, ui32 word2);
void FsReadCWD(ui32 *word1, ui32 *word2);

/*
** File Attributes
*/
int FsAttribute(const char *path, ui32 mask, ui32 *attrib);

/*
** File Identifiers
*/
ui32  FsGetFileNo(const char *path);
char *FsGetFileName(ui32 fsuid, char *buf, size_t max_sz);
char *FsGetPathName(ui32 fsuid, char *buf, size_t max_sz);

/*
** Quotas
*/
int mkdir_min_max(const char *path, mode_t mode, ui32 reserved,
                  ui32 quota);
int get_quota_min_max(const char *path, FS_QUOTA_INFO *info);
int get_quota(const char *path, ui32 *quota, ui32 *free);

/*
** UTF related functions
*/
int  ValidUTF8(const ui8 *utf8, ui32 utf8_len);
ui32 UTF8_UTF16(const ui8 *utf8, ui32 utf8_len, ui16 *utf16,
                ui32 max_utf16_len);
ui32 UTF16_UTF8(const ui16 *utf16, ui32 utf16_len, ui8 *utf8,
                ui32 max_utf8_len);

#ifdef __cplusplus
}
#endif

#endif /* _POSIX_H */

