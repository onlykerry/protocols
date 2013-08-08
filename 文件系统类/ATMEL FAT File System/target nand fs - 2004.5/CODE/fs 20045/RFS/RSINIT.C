/***********************************************************************/
/*                                                                     */
/*   Module:  rsinit.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2003.0                                                   */
/*   Purpose: Initialization functions for RAM file system             */
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
#include "ramfsp.h"

#if NUM_RFS_VOLS

/***********************************************************************/
/* Global Variable Declarations                                        */
/***********************************************************************/
SEM      RamSem;
RamGlob *Ram;
RamGlob  RamGlobals[NUM_RFS_VOLS];

/***********************************************************************/
/* Local Function Definitions                                          */
/***********************************************************************/

/***********************************************************************/
/*  free_files: Clear memory associated with files in a table          */
/*                                                                     */
/*       Input: table = pointer to files table                         */
/*                                                                     */
/***********************************************************************/
static void free_files(const RFSEnts *table)
{
  int i;
  RamSect *curr_sect, *next_sect;

  /*-------------------------------------------------------------------*/
  /* For each file, free its sector list.                              */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < FNUM_ENT; ++i)
    if (table->tbl[i].type == FCOMMN)
    {
      for (curr_sect = (RamSect *)table->tbl[i].entry.comm.frst_sect;
           curr_sect;)
      {
        next_sect = curr_sect->next;
        free(curr_sect);
        curr_sect = next_sect;
      }
    }
}

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/* GetRamTable: Allocate new RAM file system table                     */
/*                                                                     */
/*     Returns: pointer to allocated table or NULL on error            */
/*                                                                     */
/***********************************************************************/
RFSEnts *GetRamTable(void)
{
  RFSEnts *r_value = malloc(sizeof(RFSEnts));
  int i;

  /*-------------------------------------------------------------------*/
  /* If malloc() succeeded, allocate new table.                        */
  /*-------------------------------------------------------------------*/
  if (r_value)
  {
    r_value->next_tbl = NULL;
    r_value->free = FNUM_ENT - 1;

    /*-----------------------------------------------------------------*/
    /* Empty out all the table.                                        */
    /*-----------------------------------------------------------------*/
    for (i = 0; i < FNUM_ENT; ++i)
      r_value->tbl[i].type = FEMPTY;
  }
  else
    set_errno(ENOSPC);

  return r_value;
}

/***********************************************************************/
/*   RfsModule: RAM file system interface to software object manager   */
/*                                                                     */
/*       Input: req = module request code                              */
/*              ... = additional parameters specific to request        */
/*                                                                     */
/***********************************************************************/
void *RfsModule(int req, ...)
{
  void *r_value = NULL;
  va_list ap;
  char *device;
  int i, v;
  RamGlob *vol;

  switch (req)
  {
    case kInitMod:
    {
      /*---------------------------------------------------------------*/
      /* Allocate the synchronization mechanism data structures.       */
      /*---------------------------------------------------------------*/
      RamSem = semCreate("RAM_SEM", 1, OS_FIFO);
      if (RamSem == NULL)
        r_value = (void *)-1;

      /*---------------------------------------------------------------*/
      /* Mark all potential volume holders as empty.                   */
      /*---------------------------------------------------------------*/
      for (i = 0; i < NUM_RFS_VOLS; ++i)
        RamGlobals[i].num_sects = (ui32)-1;
      break;
    }

    case kFormat:
    {
      RFSEnts *temp_table, *prev_table;
      RFSEnt *curr_dir;
      ui32 dummy;

      /*---------------------------------------------------------------*/
      /* Use the va_arg mechanism to fetch the device name.            */
      /*---------------------------------------------------------------*/
      va_start(ap, req);
      device = va_arg(ap, char *);
      va_end(ap);

      /*---------------------------------------------------------------*/
      /* Go through all the existing volumes (mounted and not mounted) */
      /* and if any has the given name, format it.                     */
      /*---------------------------------------------------------------*/
      for (v = 0; v < NUM_RFS_VOLS; ++v)
      {
        vol = &RamGlobals[v];

        /*-------------------------------------------------------------*/
        /* If this entry contains a valid volume and the names match,  */
        /* format the volume.                                          */
        /*-------------------------------------------------------------*/
        if (vol->num_sects != (ui32)-1 && !strcmp(device, vol->sys.name))
        {
          /*-----------------------------------------------------------*/
          /* Acquire exclusive access to RAM.                          */
          /*-----------------------------------------------------------*/
          semPend(RamSem, WAIT_FOREVER);

          /*-----------------------------------------------------------*/
          /* If there are more than one FileTables, delete the rest.   */
          /*-----------------------------------------------------------*/
          FsReadCWD(&dummy, (void *)&curr_dir);
          for (temp_table = vol->files_tbl->next_tbl; temp_table;)
          {
            /*---------------------------------------------------------*/
            /* If the current directory is in this table set it to     */
            /* first entry for this volume, the root directory.        */
            /*---------------------------------------------------------*/
            if (curr_dir >= &temp_table->tbl[0] &&
                curr_dir <= &temp_table->tbl[FNUM_ENT])
            {
              ++vol->files_tbl->tbl[0].entry.dir.cwds;
              FsSaveCWD(dummy, (ui32)&vol->files_tbl->tbl[0]);
            }

            /*---------------------------------------------------------*/
            /* Free all sectors from all files in this table.          */
            /*---------------------------------------------------------*/
            free_files(temp_table);

            /*---------------------------------------------------------*/
            /* Free table.                                             */
            /*---------------------------------------------------------*/
            prev_table = temp_table;
            temp_table = temp_table->next_tbl;
            free(prev_table);
          }
          vol->files_tbl->next_tbl = NULL;

          /*-----------------------------------------------------------*/
          /* Set all FileTable entries to empty, except for first two. */
          /*-----------------------------------------------------------*/
          free_files(vol->files_tbl);
          for (i = 2; i < FNUM_ENT; ++i)
          {
            vol->files_tbl->tbl[i].type = FEMPTY;

            /*---------------------------------------------------------*/
            /* If the current directory is this entry, set it to first */
            /* entry in the table, the root directory.                 */
            /*---------------------------------------------------------*/
            if (curr_dir == &vol->files_tbl->tbl[i])
            {
              ++vol->files_tbl->tbl[0].entry.dir.cwds;
              FsSaveCWD(dummy, (ui32)&vol->files_tbl->tbl[0]);
            }
          }
          vol->total_free = FNUM_ENT - 2;

          /*-----------------------------------------------------------*/
          /* Set the root dir's first entry to NULL.                   */
          /*-----------------------------------------------------------*/
          vol->files_tbl->tbl[0].entry.dir.first = NULL;

          /*-----------------------------------------------------------*/
          /* Release exclusive access to RAM.                          */
          /*-----------------------------------------------------------*/
          semPost(RamSem);

          /*-----------------------------------------------------------*/
          /* If volume is not mounted, just return success.            */
          /*-----------------------------------------------------------*/
          if ((vol->sys.next == NULL) && (vol->sys.prev == NULL) &&
              (&vol->sys != MountedList.head))
            return (void *)1;

          /*-----------------------------------------------------------*/
          /* Make sure no FILE* or DIR* is associated with the system. */
          /*-----------------------------------------------------------*/
          for (i = 0; i < FOPEN_MAX; ++i)
          {
            /*---------------------------------------------------------*/
            /* If a FILE is associated with this volume, clear it.     */
            /*---------------------------------------------------------*/
            if (Files[i].volume == vol)
            {
              Files[i].ioctl = NULL;
              Files[i].volume = NULL;
              Files[i].handle = Files[i].cached = NULL;
            }
          }
          r_value = (void *)1;
        }
      }
      break;
    }

    case kVolName:
    {
      /*---------------------------------------------------------------*/
      /* Use the va_arg mechanism to fetch the name to look for.       */
      /*---------------------------------------------------------------*/
      va_start(ap, req);
      device = va_arg(ap, char *);
      va_end(ap);

      /*---------------------------------------------------------------*/
      /* Look for name duplication.                                    */
      /*---------------------------------------------------------------*/
      for (i = 0; i < NUM_RFS_VOLS; ++i)
        if (RamGlobals[i].num_sects != (ui32)-1 &&
            !strcmp(device, RamGlobals[i].sys.name))
        {
          set_errno(EEXIST);
          r_value = (void *)-1;
          break;
        }
      break;
    }

    case kMount:
    {
      char **device_name;

      /*---------------------------------------------------------------*/
      /* Use the va_arg mechanism to fetch the device name.            */
      /*---------------------------------------------------------------*/
      va_start(ap, req);
      device = va_arg(ap, char *);
      device_name = va_arg(ap, char **);
      va_end(ap);

      /*---------------------------------------------------------------*/
      /* Look for the device name among the RAM volumes.               */
      /*---------------------------------------------------------------*/
      for (v = 0; v < NUM_RFS_VOLS; ++v)
      {
        Ram = &RamGlobals[v];

        /*-------------------------------------------------------------*/
        /* If matching entry found, mount it.                          */
        /*-------------------------------------------------------------*/
        if (Ram->num_sects != (ui32)-1 &&
            ((device == NULL || !strcmp(device, Ram->sys.name)) &&
             Ram->sys.prev == NULL && Ram->sys.next == NULL &&
             &Ram->sys != MountedList.head))
        {
          *device_name = Ram->sys.name;
          r_value = &Ram->sys;
          break;
        }
      }
      break;
    }

    default:
      break;
  }
  return r_value;
}

/***********************************************************************/
/*  RamNewSect: Allocate space for new sector and add it to file       */
/*                                                                     */
/*       Input: entry = file entry                                     */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
int RamNewSect(RCOM_T *entry)
{
  RamSect *new_sect;
  int i;

  /*-------------------------------------------------------------------*/
  /* Assign space for new sector.                                      */
  /*-------------------------------------------------------------------*/
  new_sect = malloc(sizeof(RamSect));
  if (new_sect == NULL)
    return -1;

  /*-------------------------------------------------------------------*/
  /* Append new sector to end of sector list.                          */
  /*-------------------------------------------------------------------*/
  if (entry->frst_sect == NULL)
  {
    entry->frst_sect = new_sect;

    /*-----------------------------------------------------------------*/
    /* In case it is the first sector, update current FCB pointer for  */
    /* all the ones pointing to this file.                             */
    /*-----------------------------------------------------------------*/
    for (i = 0; i < FOPEN_MAX; ++i)
      if (Files[i].ioctl == RamIoctl && Files[i].volume == Ram &&
          ((RFIL_T *)Files[i].handle)->comm == entry)
      {
        PfAssert(Files[i].curr_ptr.sector == (ui32)NULL ||
                 Files[i].curr_ptr.sector == SEEK_PAST_END);
        Files[i].curr_ptr.sector = (ui32)new_sect;
        Files[i].curr_ptr.offset = 0;
        Files[i].curr_ptr.sect_off = 0;
        Files[i].old_ptr = Files[i].curr_ptr;
      }
  }
  else
    ((RamSect *)entry->last_sect)->next = new_sect;
  new_sect->next = NULL;
  new_sect->prev = (RamSect *)entry->last_sect;
  entry->last_sect = new_sect;
  entry->one_past_last = 0;

  ++Ram->num_sects;

  return 0;
}

/***********************************************************************/
/* RamFreeSect: Remove last sector from a file's sector list           */
/*                                                                     */
/*       Input: entry = file entry                                     */
/*                                                                     */
/***********************************************************************/
void RamFreeSect(RCOM_T *entry)
{
  RamSect *new_last_sect;

  /*-------------------------------------------------------------------*/
  /* Remove last sector from sector list.                              */
  /*-------------------------------------------------------------------*/
  entry->size -= entry->one_past_last;
  new_last_sect = ((RamSect *)entry->last_sect)->prev;
  if (new_last_sect == NULL)
  {
    entry->frst_sect = NULL;
    PfAssert(entry->size == 0);
  }
  else
  {
    new_last_sect->next = NULL;
    entry->one_past_last = RAM_SECT_SZ;
  }
  free(entry->last_sect);
  entry->last_sect = new_last_sect;

  --Ram->num_sects;
}

/***********************************************************************/
/*   RfsDelVol: Remove volume from list of RAM volumes and free up its */
/*              resources                                              */
/*                                                                     */
/*       Input: name = name of volume to be deleted                    */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
int RfsDelVol(const char *name)
{
  int i;
  RFSEnts *curr_tbl, *next_tbl;

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive acess to the RAM file system.                   */
  /*-------------------------------------------------------------------*/
  semPend(RamSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Look for volume in the RamGlobals array.                          */
  /*-------------------------------------------------------------------*/
  for (i = 0;; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* If the volume was not found, return error.                      */
    /*-----------------------------------------------------------------*/
    if (i == NUM_RFS_VOLS)
    {
      set_errno(ENOENT);
      semPost(RamSem);
      return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Break if match is found.                                        */
    /*-----------------------------------------------------------------*/
    if (RamGlobals[i].num_sects != (ui32)-1 &&
        !strcmp(name, RamGlobals[i].sys.name))
      break;
  }

  /*-------------------------------------------------------------------*/
  /* If volume is mounted, return error.                               */
  /*-------------------------------------------------------------------*/
  if (RamGlobals[i].sys.next != NULL ||
      RamGlobals[i].sys.prev != NULL ||
      &RamGlobals[i].sys == MountedList.head)
  {
    set_errno(EBUSY);
    semPost(RamSem);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* Free up volume resources.                                         */
  /*-------------------------------------------------------------------*/
  for (curr_tbl = RamGlobals[i].files_tbl; curr_tbl; curr_tbl = next_tbl)
  {
    next_tbl = curr_tbl->next_tbl;
    curr_tbl->next_tbl = curr_tbl->prev_tbl = NULL;
    free_files(curr_tbl);
    free(curr_tbl);
  }
  RamGlobals[i].files_tbl = NULL;

  /*-------------------------------------------------------------------*/
  /* Reset state variables.                                            */
  /*-------------------------------------------------------------------*/
  Ram->fileno_gen = Ram->total_free = 0;
  Ram->num_sects = (ui32)-1;
  Ram->opendirs = 0;

  /*-------------------------------------------------------------------*/
  /* Release exclusive acess to TargetRFS and return success.          */
  /*-------------------------------------------------------------------*/
  semPost(RamSem);
  return 0;
}

/***********************************************************************/
/*   RfsAddVol: Add a new RAM volume                                   */
/*                                                                     */
/*       Input: name = volume name                                     */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
int RfsAddVol(const char *name)
{
  int i, r_value = -1;
  RamGlob *vol = NULL;
  RFSEnt *v_ent;
  Module fp;

  /*-------------------------------------------------------------------*/
  /* If the volume name is invalid, return error.                      */
  /*-------------------------------------------------------------------*/
  if (InvalidName(name, FALSE))
  {
    set_errno(EINVAL);
    return r_value;
  }

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive access to upper file system.                    */
  /*-------------------------------------------------------------------*/
  semPend(FileSysSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* Acquire exclusive acess to the RAM file system.                   */
  /*-------------------------------------------------------------------*/
  semPend(RamSem, WAIT_FOREVER);

  /*-------------------------------------------------------------------*/
  /* First look that there is no name duplication.                     */
  /*-------------------------------------------------------------------*/
  for (i = 0; ; ++i)
  {
    fp = ModuleList[i];
    if (fp == NULL)
      break;

    if (fp(kVolName, name))
    {
      semPost(FileSysSem);
      set_errno(EEXIST);
      goto RamAddVol_error_exit;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to upper file system.                    */
  /*-------------------------------------------------------------------*/
  semPost(FileSysSem);

  /*-------------------------------------------------------------------*/
  /* Look for an empty entry in the RamGlobals array to place new      */
  /* volume controls.                                                  */
  /*-------------------------------------------------------------------*/
  for (i = 0;; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* If no space, set errno and go to exit.                          */
    /*-----------------------------------------------------------------*/
    if (i == NUM_RFS_VOLS)
    {
      set_errno(ENOMEM);
      goto RamAddVol_error_exit;
    }

    if (RamGlobals[i].num_sects == (ui32)-1)
    {
      vol = &RamGlobals[i];
      break;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Attempt to allocate memory for the first files table.             */
  /*-------------------------------------------------------------------*/
  vol->files_tbl = GetRamTable();
  if (vol->files_tbl == NULL)
    goto RamAddVol_error_exit;

  /*-------------------------------------------------------------------*/
  /* Set the sys node so it can later be added to the mounted list.    */
  /*-------------------------------------------------------------------*/
  vol->sys.volume = vol;
  vol->sys.ioctl = RamIoctl;
  if (strlen(name) >= FILENAME_MAX)
  {
    set_errno(ENAMETOOLONG);
    goto RamAddVol_error_exit;
  }
  strcpy(vol->sys.name, name);

  /*-------------------------------------------------------------------*/
  /* First two entries in the files table are reserved for root.       */
  /*-------------------------------------------------------------------*/
  vol->files_tbl->free -= 1;

  /*-------------------------------------------------------------------*/
  /* Get pointer to the second entry in the files table.               */
  /*-------------------------------------------------------------------*/
  v_ent = &vol->files_tbl->tbl[1];

  /*-------------------------------------------------------------------*/
  /* Set second entry in files table to be the file struct associated  */
  /* with the root directory.                                          */
  /*-------------------------------------------------------------------*/
  v_ent->type = FCOMMN;
  v_ent->entry.comm.open_mode = 0;
  v_ent->entry.comm.links = 1;
  v_ent->entry.comm.open_links = 0;
  v_ent->entry.comm.frst_sect = NULL;
  v_ent->entry.comm.last_sect = NULL;
  v_ent->entry.comm.mod_time = v_ent->entry.comm.ac_time = OsSecCount;
  v_ent->entry.comm.fileno = vol->fileno_gen++;
  v_ent->entry.comm.one_past_last = 0;
  v_ent->entry.comm.addr = v_ent;

  /*-------------------------------------------------------------------*/
  /* Get pointer to the first entry in the files table.                */
  /*-------------------------------------------------------------------*/
  v_ent = &vol->files_tbl->tbl[0];

  /*-------------------------------------------------------------------*/
  /* Set the first entry in files table to be root dir.                */
  /*-------------------------------------------------------------------*/
  v_ent->type = FDIREN;
  v_ent->entry.dir.cwds = 0;
  v_ent->entry.dir.next = NULL;
  v_ent->entry.dir.prev = NULL;
  v_ent->entry.dir.comm = &vol->files_tbl->tbl[1].entry.comm;
  v_ent->entry.dir.parent_dir = NULL;
  v_ent->entry.dir.first = NULL;
  strncpy(v_ent->entry.dir.name, vol->sys.name, FILENAME_MAX);

  /*-------------------------------------------------------------------*/
  /* Set the other variables for this volume.                          */
  /*-------------------------------------------------------------------*/
  vol->total_free = FNUM_ENT - 2;
  vol->num_sects = 0;

  /*-------------------------------------------------------------------*/
  /* Set the permissions for the root directory.                       */
  /*-------------------------------------------------------------------*/
  SetPerm((void *)v_ent->entry.dir.comm, S_IROTH | S_IWOTH | S_IXOTH);

  /*-------------------------------------------------------------------*/
  /* Release exclusive access to TargetRFS and return success.         */
  /*-------------------------------------------------------------------*/
  semPost(RamSem);
  return 0;

  /*-------------------------------------------------------------------*/
  /* In case of error, clean exit.                                     */
  /*-------------------------------------------------------------------*/
RamAddVol_error_exit:
  if (vol)
  {
    if (vol->files_tbl)
    {
      free(vol->files_tbl);
      vol->files_tbl = NULL;
    }
    vol->num_sects = vol->total_free = 0;
  }
  semPost(RamSem);
  return -1;
}
#endif /* NUM_RFS_VOLS */

