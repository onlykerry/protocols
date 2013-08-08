/***********************************************************************/
/*                                                                     */
/*   Module:  sysioctl.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2004.6                                                   */
/*   Purpose: Implements functions related to the TTY drivers          */
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
#include <stdarg.h>
#include "../include/libc/string.h"
#include "../include/libc/ctype.h"
#include "../include/libc/errno.h"
#include "../include/libc/stdlib.h"
#include "../posix.h"
#include "../include/targetos.h"
#include "../include/sys.h"
#include "../include/fsprivate.h"

/***********************************************************************/
/* Configuration Data                                                  */
/***********************************************************************/
/*
** List of software modules in this build
*/
const Module ModuleList[] =
{
#if NUM_RFS_VOLS
  RfsModule,
#endif
#if NUM_FFS_VOLS
  FfsModule,
#endif
#if INC_NAND_FS
  NandDriverModule,
#endif
#if INC_NOR_FS
  NorDriverModule,
#endif
#if NUM_FAT_VOLS
  FatModule,
#if NUM_NAND_FTLS
  FtlNandDriverModule,
#endif
#if INC_FAT_FIXED
  FatDriverModule,
#endif
#endif
#if PCCARD_SUPPORT
  pccModule,
  pccDvrModule,
  AppModule,
#endif
  NULL,
};

/***********************************************************************/
/* Global Variable Declarations                                        */
/***********************************************************************/
HeadEntry MountedList =
{
  NULL, NULL,
};
FILE        Files[FOPEN_MAX + 1];
SEM         FileSysSem;
const char *FSPath;
int CurrFixedVols = 0;

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
int printf(const char *format, ...);

/***********************************************************************/
/* Local Function Definitions                                          */
/***********************************************************************/

/***********************************************************************/
/*  do_nothing: A do-nothing acquire/release routine                   */
/*                                                                     */
/***********************************************************************/
static void do_nothing(const FILE *file, int rwcode)
{
}

/***********************************************************************/
/* dir_file_write: Error function that gets called when a file write   */
/*              is attempted on a directory control block              */
/*                                                                     */
/*     Returns: Error always                                           */
/*                                                                     */
/***********************************************************************/
static int dir_file_write(FILE *stream, const ui8 *buf, ui32 len)
{
  stream->errcode = EISDIR;
  set_errno(EISDIR);
  return -1;
}

/***********************************************************************/
/* dir_file_read: Error function that gets called when a file read is  */
/*              attempted on a directory control block                 */
/*                                                                     */
/*     Returns: Error always                                           */
/*                                                                     */
/***********************************************************************/
static int dir_file_read(FILE *stream, ui8 *buf, ui32 len)
{
  stream->errcode = EISDIR;
  set_errno(EISDIR);
  return -1;
} /*lint !e818*/

/***********************************************************************/
/*  root_ioctl: Do POSIX functions as the root directory               */
/*                                                                     */
/*      Inputs: dir = holds dir ctrl block ptr                         */
/*              code = selects what function to do                     */
/*                                                                     */
/*     Returns: Depends on the function that was done                  */
/*                                                                     */
/***********************************************************************/
static void *root_ioctl(DIR *dir, int code, ...)
{
  void *r_value = NULL;
  static struct dirent read_dir;

  switch (code)
  {
    case GETCWD:
    {
      va_list list_of_args;
      char *buf;

      /*---------------------------------------------------------------*/
      /* Use the va_arg mechanism to fetch the args for fsearch.       */
      /*---------------------------------------------------------------*/
      va_start(list_of_args, code);
      buf = (va_arg(list_of_args, char *)); /*lint !e415 !e416 */
      va_end(list_of_args);

      /*---------------------------------------------------------------*/
      /* Malloc buffer if needed, and simply fill it with "/".         */
      /*---------------------------------------------------------------*/
      if (buf == NULL)
      {
        buf = malloc(2);
        if (buf == NULL)
          return NULL;
      }
      strcpy(buf, "/");
      r_value = buf;
      break;
    }

    case READDIR:
      /*---------------------------------------------------------------*/
      /* If there are entries in the mounted list, display the first   */
      /* mounted file system.                                          */
      /*---------------------------------------------------------------*/
      if (dir->pos == NULL)
      {
        if (MountedList.head == NULL)
          break;
        dir->pos = MountedList.head;
        strncpy(read_dir.d_name, ((FileSys *)dir->pos)->name,
                FILENAME_MAX);
        r_value = &read_dir;
      }

      /*---------------------------------------------------------------*/
      /* Else if it's not the last entry, advance to next one.         */
      /*---------------------------------------------------------------*/
      else if (((FileSys *)dir->pos)->next)
      {
        dir->pos = ((FileSys *)dir->pos)->next;
        strncpy(read_dir.d_name, ((FileSys *)dir->pos)->name,
                FILENAME_MAX);
        r_value = &read_dir;
      }

      break;

    case OPENDIR:
      /*---------------------------------------------------------------*/
      /* Set argument as pointer to root directory.                    */
      /*---------------------------------------------------------------*/
      dir->ioctl = Files[ROOT_DIR_INDEX].ioctl;
      dir->pos = NULL;
      r_value = dir;
      break;

    case CLOSEDIR:
      return NULL;

    case CHDIR:
    {
      ui32 cwd;
      FileSys *fsys;

      /*---------------------------------------------------------------*/
      /* Decrement previous working directory count if any.            */
      /*---------------------------------------------------------------*/
      FsReadCWD((void *)&fsys, &cwd);
      if (fsys)
        fsys->ioctl(NULL, CHDIR_DEC, cwd); /*lint !e522*/

      /*---------------------------------------------------------------*/
      /* Assign current working directory.                             */
      /*---------------------------------------------------------------*/
      FsSaveCWD((ui32)NULL, (ui32)NULL);
      break;
    }

    /*-----------------------------------------------------------------*/
    /* All unimplemented API functions return error.                   */
    /*-----------------------------------------------------------------*/
    case FOPEN:
    case TMPNAM:
    case TMPFILE:
      set_errno(EINVAL);
      return NULL;

    default:
      set_errno(EINVAL);
      return (void *)-1;
  }
  return r_value;
}

/***********************************************************************/
/* sw_mod_loop: Poll all installed modules with a request              */
/*                                                                     */
/*       Input: req = one of the module request codes defined in sys.h */
/*                                                                     */
/*     Returns: -1 if any module returns non-NULL, else 0              */
/*                                                                     */
/***********************************************************************/
static int sw_mod_loop(int req)
{
  int i;

  for (i = 0; ModuleList[i]; ++i)
    if (ModuleList[i](req))
      return -1;

  return 0;
}

/***********************************************************************/
/* fill_dir_entry: Fill in fields for DirEntry for a directory entry   */
/*                                                                     */
/*      Inputs: entry = pointer to directory entry                     */
/*              dir_entry = pointer to DirEntry to use with comparison */
/*                                                                     */
/***********************************************************************/
static void fill_dir_entry(const FDIR_T *entry, DirEntry *dir_entry)
{
  dir_entry->st_name = entry->name;
  dir_entry->st_ino = entry->comm->fileno;
  dir_entry->st_mode = entry->comm->mode;
  dir_entry->st_nlink = entry->comm->links;
  dir_entry->st_uid = entry->comm->user_id;
  dir_entry->st_gid = entry->comm->group_id;
  dir_entry->st_size = entry->comm->size;
  dir_entry->st_atime = entry->comm->ac_time;
  dir_entry->st_mtime = entry->comm->mod_time;
  dir_entry->st_ctime = entry->comm->mod_time;
}

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*   QuickSort: Perform quick sort on a list of directory entries      */
/*                                                                     */
/*      Inputs: head = start of list                                   */
/*              tail = end of list                                     */
/*              cmp = user comparison function                         */
/*                                                                     */
/***********************************************************************/
void QuickSort(FFSEnt **head, FFSEnt **tail, DirEntry *e1,
               DirEntry *e2, int (*cmp) (const DirEntry *,
                                         const DirEntry *))
{
  FFSEnt *less_head = NULL, *less_tail = NULL;
  FFSEnt *greater_head = NULL, *greater_tail = NULL;
  FFSEnt *pivot, *curr_ent;

  /*-------------------------------------------------------------------*/
  /* If list has one element, just return.                             */
  /*-------------------------------------------------------------------*/
  if (head == tail)
    return;

  /*-------------------------------------------------------------------*/
  /* Select the first element as the pivot.                            */
  /*-------------------------------------------------------------------*/
  pivot = *head;
  *head = (*head)->entry.dir.next;
  (*head)->entry.dir.prev = NULL;
  pivot->entry.dir.next = NULL;

  /*-------------------------------------------------------------------*/
  /* Fill in DirEntry for the pivot.                                   */
  /*-------------------------------------------------------------------*/
  fill_dir_entry(&pivot->entry.dir, e1);

  /*-------------------------------------------------------------------*/
  /* Now walk the rest of the list and place each entry into one of the*/
  /* two categories (<=, >) based on the comparison function.          */
  /*-------------------------------------------------------------------*/
  while ((curr_ent = *head) != NULL)
  {
    /*-----------------------------------------------------------------*/
    /* Advance head to next entry in list.                             */
    /*-----------------------------------------------------------------*/
    *head = (*head)->entry.dir.next;
    curr_ent->entry.dir.next = NULL;

    /*-----------------------------------------------------------------*/
    /* Fill in DirEntry for current entry.                             */
    /*-----------------------------------------------------------------*/
    fill_dir_entry(&curr_ent->entry.dir, e2);

    /*-----------------------------------------------------------------*/
    /* If entry >= pivot, place it in the 'less' list.                 */
    /*-----------------------------------------------------------------*/
    if (cmp(e1, e2) >= 0)
    {
      if (less_tail == NULL)
      {
        less_head = less_tail = curr_ent;
        curr_ent->entry.dir.prev = NULL;
      }
      else
      {
        less_tail->entry.dir.next = curr_ent;
        curr_ent->entry.dir.prev = less_tail;
        less_tail = curr_ent;
      }
    }

    /*-----------------------------------------------------------------*/
    /* If entry after pivot, place it in the 'greater' list.           */
    /*-----------------------------------------------------------------*/
    else
    {
      if (greater_tail == NULL)
      {
        greater_head = greater_tail = curr_ent;
        curr_ent->entry.dir.prev = NULL;
      }
      else
      {
        greater_tail->entry.dir.next = curr_ent;
        curr_ent->entry.dir.prev = greater_tail;
        greater_tail = curr_ent;
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* Sort both lists if non-empty.                                     */
  /*-------------------------------------------------------------------*/
  if (less_head)
    QuickSort(&less_head, &less_tail, e1, e2, cmp);
  if (greater_head)
    QuickSort(&greater_head, &greater_tail, e1, e2, cmp);

  /*-------------------------------------------------------------------*/
  /* Create sorted list: less->pivot->greater.                         */
  /*-------------------------------------------------------------------*/
  if (less_tail)
  {
    *head = less_head;
    less_tail->entry.dir.next = pivot;
  }
  else
    *head = pivot;
  pivot->entry.dir.prev = less_tail;
  if (greater_head)
  {
    *tail = greater_tail;
    greater_head->entry.dir.prev = pivot;
  }
  else
    *tail = pivot;
  pivot->entry.dir.next = greater_head;
}

/***********************************************************************/
/*     InitFFS: Initialize the flash                                   */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int InitFFS(void)
{
  /*-------------------------------------------------------------------*/
  /* Set the file system semaphore and global variables.               */
  /*-------------------------------------------------------------------*/
  FileSysSem = semCreate("FSYS_SEM", 1, OS_FIFO);
  if (FileSysSem == NULL)
    return -1;
  FsInitFCB(&Files[ROOT_DIR_INDEX], FCB_DIR);
  Files[ROOT_DIR_INDEX].ioctl = root_ioctl;

  /*-------------------------------------------------------------------*/
  /* Initialize all modules in the list.                               */
  /*-------------------------------------------------------------------*/
  return sw_mod_loop(kInitMod);
}

/***********************************************************************/
/*      IsLast: Given a string, look to see if it is last entry in the */
/*              path for a filename or a directory                     */
/*                                                                     */
/*      Inputs: name = string to check                                 */
/*              incr = pointer to num of chars to increment            */
/*                                                                     */
/*     Returns: length of entry if not last, 0 if it is                */
/*                                                                     */
/***********************************************************************/
ui32 IsLast(const char *name, ui32 *incr)
{
  ui32 i, inc = 0;

  for (;; ++name, ++inc)
  {
    /*-----------------------------------------------------------------*/
    /* If a '/' is encountered, it's not the last entry in the path.   */
    /*-----------------------------------------------------------------*/
    if (*name == '/')
    {
      /*---------------------------------------------------------------*/
      /* If there's nothing pointing past the '/' however, it is the   */
      /* last entry in the path.                                       */
      /*---------------------------------------------------------------*/
      for (i = inc;; ++i)
      {
        ++name;
        *incr = i + 1;
        if (*name == '\0')
          return 0;
        else if (*name != '/')
          return inc;
      }
    }

    /*-----------------------------------------------------------------*/
    /* If we've reached the end of the string, it is last entry.       */
    /*-----------------------------------------------------------------*/
    if (*name == '\0')
    {
      *incr = inc;
      return 0;
    }
  }
}

/***********************************************************************/
/* FSearchFSUID: Search for a filename(or full path) based on FSUID    */
/*                                                                     */
/*      Inputs: fsuid = FSUID number to look for                       */
/*              buf = buffer to place filename(path) in if non NULL    */
/*              size = size of buf when non NULL                       */
/*              lookup = flag to indicate filename or full path        */
/*                                                                     */
/*     Returns: filename(path), NULL on error                          */
/*                                                                     */
/***********************************************************************/
char *FSearchFSUID(ui32 fsuid, char *buf, size_t size, int lookup)
{
  ui32 vid, fid;
  FileSys *fsys;
  char *r_value = NULL;

  /*-------------------------------------------------------------------*/
  /* Get the volume id and file id from the FSUID.                     */
  /*-------------------------------------------------------------------*/
  vid = get_vid(fsuid);
  fid = get_fid(fsuid);

  /*-------------------------------------------------------------------*/
  /* Scan all the mounted volumes for the entry with FSUID.            */
  /*-------------------------------------------------------------------*/
  for (fsys = MountedList.head; fsys && r_value == NULL;
       fsys = fsys->next)
  {
    r_value = fsys->ioctl(NULL, GET_NAME, fsys->volume, buf, size,
                          lookup, vid, fid);
    if (r_value == (char *)-1)
      return NULL;
  }

  return r_value;
}

/***********************************************************************/
/*     FSearch: Search a path to determine if it is valid              */
/*                                                                     */
/*      Inputs: handle = handle to either dir or file ctrl block       */
/*              path = pointer to the path                             */
/*              dir_lookup = flag to indicate up to what level lookup  */
/*                           is performed (actual or parent directory) */
/*                                                                     */
/*     Returns: Pointer to directory that holds entity for which       */
/*              search was performed, or NULL if no directory exists   */
/*                                                                     */
/***********************************************************************/
void *FSearch(void *handle, const char **path, int dir_lookup)
{
  FILE *file = handle;
  void *r_value = (void *)-1;
  ui32 len, incr, dummy;
  FsVolume *volume;

  /*-------------------------------------------------------------------*/
  /* Set FSPath to point to *path.                                     */
  /*-------------------------------------------------------------------*/
  FSPath = *path;

  /*-------------------------------------------------------------------*/
  /* Keep looping until entry found or error.                          */
  /*-------------------------------------------------------------------*/
  do
  {
    /*-----------------------------------------------------------------*/
    /* Obtain current directory info. If never set before, set it to   */
    /* root.                                                           */
    /*-----------------------------------------------------------------*/
    FsReadCWD((void *)&volume, &dummy);

    /*-----------------------------------------------------------------*/
    /* If it's an absolute path, or current ioctl is root one, figure  */
    /* out to which file system to direct the search.                  */
    /*-----------------------------------------------------------------*/
    if (*FSPath == '/' || volume == NULL || r_value == NULL)
    {
      /*---------------------------------------------------------------*/
      /* Strip off the leading '/' in the path if there are any.       */
      /*---------------------------------------------------------------*/
      while (*FSPath == '/')
        ++FSPath;

      /*---------------------------------------------------------------*/
      /* Skip multiple "./" strings. Skip ending ".".                  */
      /*---------------------------------------------------------------*/
      while (!strncmp(FSPath, "./", 2))
        FSPath += 2;
      if (!strcmp(FSPath, "."))
        ++FSPath;

      /*---------------------------------------------------------------*/
      /* If pathname is empty, we're in the root directory, stop.      */
      /*---------------------------------------------------------------*/
      if (*FSPath == '\0')
      {
        file->ioctl = Files[ROOT_DIR_INDEX].ioctl;
        file->acquire = file->release = do_nothing;
        r_value = &Files[ROOT_DIR_INDEX];
      }

      /*---------------------------------------------------------------*/
      /* Else pathname is valid, check if any file systems are mounted.*/
      /*---------------------------------------------------------------*/
      else if (MountedList.head)
      {
        FileSys *fsys;

        /*-------------------------------------------------------------*/
        /* Check if we're looking at last name in path.                */
        /*-------------------------------------------------------------*/
        len = IsLast(FSPath, &incr);
        if (len == 0)
        {
          /*-----------------------------------------------------------*/
          /* If it's directory, strip off trailing '/' if it exists.   */
          /*-----------------------------------------------------------*/
          if (dir_lookup == ACTUAL_DIR || dir_lookup == DIR_FILE)
          {
            if ((FSPath)[incr - 1] == '/')
            {
              len = incr - 1;
              while (FSPath[len - 1] == '/')
                --len;
            }
            else
              len = incr;

            /*---------------------------------------------------------*/
            /* If path too long, return error if no truncation, else   */
            /* truncate.                                               */
            /*---------------------------------------------------------*/
            if (len > PATH_MAX)
            {
#if _PATH_NO_TRUNC
              set_errno(ENAMETOOLONG);
              return NULL;
#else
              len = PATH_MAX;
#endif
            }
          }

          /*-----------------------------------------------------------*/
          /* Else we are looking for a file and it can never be in the */
          /* root directory so return error.                           */
          /*-----------------------------------------------------------*/
          else
          {
            set_errno(ENOENT);
            return NULL;
          }
        }

        /*-------------------------------------------------------------*/
        /* Find path's root name among the mounted file systems.       */
        /*-------------------------------------------------------------*/
        for (fsys = MountedList.head;; fsys = fsys->next)
        {
          /*-----------------------------------------------------------*/
          /* If file system not found, return error.                   */
          /*-----------------------------------------------------------*/
          if (fsys == NULL)
          {
            r_value = (void *)-1;
            set_errno(ENXIO);
            break;
          }

          /*-----------------------------------------------------------*/
          /* If the names match, we found the entry.                   */
          /*-----------------------------------------------------------*/
          if (!strncmp(FSPath, fsys->name, len) &&
              strlen(fsys->name) == len)
          {
            /*---------------------------------------------------------*/
            /* Set the volume pointer before doing FSEARCH.            */
            /*---------------------------------------------------------*/
            file->volume = fsys->volume;

            /*---------------------------------------------------------*/
            /* Adjust the path.                                        */
            /*---------------------------------------------------------*/
            FSPath += incr;

            /*---------------------------------------------------------*/
            /* Call the file system's ioctl with FSEARCH to continue   */
            /* the look up process inside the file system.             */
            /*---------------------------------------------------------*/
            r_value = fsys->ioctl(file, FSEARCH, dir_lookup, FIRST_DIR);
            break;
          }
        }
      }

      /*---------------------------------------------------------------*/
      /* Else there are no mounted systems, return error.              */
      /*---------------------------------------------------------------*/
      else
      {
        set_errno(EINVAL);
        return NULL;
      }
    }

    /*-----------------------------------------------------------------*/
    /* Else call the current ioctl.                                    */
    /*-----------------------------------------------------------------*/
    else
    {
      file->volume = volume;
      r_value = volume->sys.ioctl(file, FSEARCH, dir_lookup, CURR_DIR);
    }
  }
  while (r_value == NULL);

  /*-------------------------------------------------------------------*/
  /* Set *path to the value of FSPath.                                 */
  /*-------------------------------------------------------------------*/
  *path = FSPath;

  return (r_value == (void *)-1) ? NULL : r_value;
}

/***********************************************************************/
/* InvalidName: Check that name is a valid dir or file name            */
/*                                                                     */
/*      Inputs: name = name to be checked                              */
/*              ignore_last = flag to ignore last chars if '/'         */
/*                                                                     */
/*     Returns: TRUE if invalid, FALSE if valid                        */
/*                                                                     */
/***********************************************************************/
int InvalidName(const char *name, int ignore_last)
{
#if UTF_ENABLED
  return !ValidUTF8((const ui8 *)name, strlen(name));
#else
  int i, j;
  uint len = strlen(name);

  /*-------------------------------------------------------------------*/
  /* First character must be '.', '_', '$', '#', or alphanumeric.      */
  /*-------------------------------------------------------------------*/
  if (name[0] != '.' && name[0] != '_' && name[0] != '$' &&
      name[0] != '#' && !isalnum(name[0]))
  {
    set_errno(EINVAL);
    return TRUE;
  }

  /*-------------------------------------------------------------------*/
  /* The remaining chars must be alnum or '.', '_', '-', '$', or '#'.  */
  /*-------------------------------------------------------------------*/
  for (i = 1; i < len; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* Check if the character is invalid.                              */
    /*-----------------------------------------------------------------*/
    if (!isalnum(name[i]) && name[i] != '.' && name[i] != '_' &&
        name[i] != '-' && name[i] != '$' && name[i] != '#')
    {
      /*---------------------------------------------------------------*/
      /* Ignore the last characters if they are '/' and flag is set.   */
      /*---------------------------------------------------------------*/
      if (name[i] == '/' && ignore_last)
      {
        for (j = i; j < len && name[j] == '/'; ++j) ;
        if (j < len)
        {
          set_errno(EINVAL);
          return TRUE;
        }
      }
      else
      {
        set_errno(EINVAL);
        return TRUE;
      }
    }
  }
  return FALSE;
#endif /* UTF_ENABLED */
}

/***********************************************************************/
/*   mount_all: Mount all installed volumes                            */
/*                                                                     */
/*       Input: verbose = flag to display status of each mount         */
/*                                                                     */
/*     Returns: 0 if all volumes mounted successfully, otherwise -1    */
/*                                                                     */
/***********************************************************************/
int mount_all(int verbose)
{
  FileSys *volume;
  char *name;
  int i, r_value, mount_cnt;

  /*-------------------------------------------------------------------*/
  /* Get exclusive access to file system.                              */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Loop through module list.                                         */
  /*-------------------------------------------------------------------*/
  for (i = r_value = mount_cnt = 0; ModuleList[i];)
  {
    /*-----------------------------------------------------------------*/
    /* Pass mount command to each module, with NULL volume name.       */
    /*-----------------------------------------------------------------*/
    volume = ModuleList[i](kMount, NULL, &name);

    /*-----------------------------------------------------------------*/
    /* If current module is done, go to next one.                      */
    /*-----------------------------------------------------------------*/
    if (volume == NULL)
    {
      ++i;
      continue;
    }

    /*-----------------------------------------------------------------*/
    /* If verbose and this is first mount, announce our intent.        */
    /*-----------------------------------------------------------------*/
    if (verbose && ++mount_cnt == 1)
      printf("Mounting all installed volumes\n");

    /*-----------------------------------------------------------------*/
    /* If error occurred, display message and move to next mount.      */
    /*-----------------------------------------------------------------*/
    if (volume == (FileSys *)-1)
    {
      r_value = -1;
      if (verbose)
        printf("Volume \"/%s\" failed to mount\n", name);
      ++i;
    }

    /*-----------------------------------------------------------------*/
    /* Else add device to list of mounted volumes.                     */
    /*-----------------------------------------------------------------*/
    else
    {
      /*---------------------------------------------------------------*/
      /* If in verbose mode, display successful mount message.         */
      /*---------------------------------------------------------------*/
      if (verbose)
        printf("Volume \"/%s\" mounted successfully\n", name);

      /*---------------------------------------------------------------*/
      /* If there's already mounted volumes, append this one.          */
      /*---------------------------------------------------------------*/
      if (MountedList.head)
      {
        volume->prev = MountedList.tail;
        MountedList.tail->next = volume;
      }

      /*---------------------------------------------------------------*/
      /* Else, set this device as the first mounted device.            */
      /*---------------------------------------------------------------*/
      else
      {
        volume->prev = NULL;
        MountedList.head = volume;
      }

      /*---------------------------------------------------------------*/
      /* Set the tail of the list of mounted volumes.                  */
      /*---------------------------------------------------------------*/
      volume->next = NULL;
      MountedList.tail = volume;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to file system and return.               */
  /*-------------------------------------------------------------------*/
  semPost(FileSysSem);
  return r_value;
}

/***********************************************************************/
/*   FsInitFCB: Initialize a file control block                        */
/*                                                                     */
/*      Inputs: file = pointer to file/dir control block               */
/*              type = type of control block (file or dir)             */
/*                                                                     */
/***********************************************************************/
void FsInitFCB(FILE *file, ui32 type)
{
  file->flags = type;
  if (type == FCB_DIR)
  {
    file->write = dir_file_write;
    file->read = dir_file_read;
  }
  else if (type == FCB_FILE)
  {
    file->write = NULL;
    file->read = NULL;
  }
  else if (type == FCB_TTY)
  {
    return;
  }
  file->acquire = file->release = do_nothing;
  file->ioctl = file->volume = file->handle = file->pos = NULL;
  file->hold_char = file->errcode = 0;
  file->cached = NULL;
}

#if 0
/*
** Included for easy incorporation in applications if user hasn't yet
** implemented or doesn't need per-task current working directories.
*/
static ui32 Word1, Word2;
/***********************************************************************/
/*   FsSaveCWD: Save current working directory state information       */
/*                                                                     */
/*      Inputs: word1 = 1 of 2 words to save                           */
/*              word2 = 2 of 2 words to save                           */
/*                                                                     */
/***********************************************************************/
void FsSaveCWD(ui32 word1, ui32 word2)
{
  Word1 = word1;
  Word2 = word2;
}

/***********************************************************************/
/*   FsReadCWD: Read current working directory state information       */
/*                                                                     */
/*     Outputs: word1 = 1 of 2 words to retrieve                       */
/*              word2 = 2 of 2 words to retrieve                       */
/*                                                                     */
/***********************************************************************/
void FsReadCWD(ui32 *word1, ui32 *word2)
{
  *word1 = Word1;
  *word2 = Word2;
}
#endif

