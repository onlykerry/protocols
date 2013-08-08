/*C**************************************************************************
* NAME:         fat.c
*----------------------------------------------------------------------------
* Copyright (c) 2003 Atmel.
*----------------------------------------------------------------------------
* RELEASE:      snd1c-refd-nf-4_0_3      
* REVISION:     1.41     
*----------------------------------------------------------------------------
* PURPOSE:
* FAT16/FAT12 file-system basics functions
* 
* NOTES:
* Supports only the first partition
* Supports only 512 bytes sector size
* Supports only file fragmentation < MAX_FILE_FRAGMENT_NUMBER
* Supports only one file openned at a time
*
* Global Variables:
*   - gl_buffer: array of bytes in pdata space
*****************************************************************************/

/*_____ I N C L U D E S ____________________________________________________*/

#include "config.h"                         /* system configuration */
#include "..\mem\hard.h"                    /* low level function definition */
#include "file.h"                           /* file function definition */
#include "fat.h"                            /* fat file-system definition */


/*_____ M A C R O S ________________________________________________________*/


/*_____ D E F I N I T I O N ________________________________________________*/

extern        bit     reserved_disk_space;
extern  pdata Byte    gl_buffer[];
extern  xdata Byte    fat_buf_sector[];  /* 512 bytes buffer */

extern  char  pdata *lfn_name; /* long filename limited to MAX_FILENAME_LEN chars */

/* disk management */
extern  data  Uint32  fat_ptr_fats;         /* address of the first byte of FAT */
extern  data  Uint32  fat_ptr_rdir;         /* address of the first byte of root dir */
extern  data  Uint32  fat_ptr_data;         /* address of the first byte of data */
extern  data  Byte    fat_cluster_size;     /* cluster size (sector count) */
extern  idata Byte    fat_cluster_mask;     /* mask for end of cluster test */

extern  bdata bit     dir_is_root;          /* TRUE: point the root directory  */
extern  bdata bit     fat_is_fat16;         /* TRUE: FAT16 - FALSE: FAT12 */
extern  bdata bit     fat_open_mode;        /* READ or WRITE */
extern  bdata bit     fat_2_is_present;     /* TRUE: 2 FATs - FALSE: 1 FAT */
extern  bdata bit     flag_end_disk_file;


extern  xdata Uint32  fat_count_of_clusters;/* number of cluster - 2 */
extern  xdata Uint16  fat_root_entry;       /* position in root dir */
extern  xdata Union32 fat_file_size;
extern  xdata Uint16  fat_fat_size;         /* FAT size in sector count */


/* directory management */
extern  idata Uint32  fat_dir_current_sect; /* sector of selected entry in dir list */
extern  xdata Uint16  fat_dir_list_index;   /* index of current entry in dir list */
extern  xdata Uint16  fat_dir_list_last;    /* index of last entry in dir list */
extern  idata Uint16  fat_dclust_byte_count;/* byte counter in directory sector */
extern  idata Uint16  fat_dchain_index;     /* the number of the fragment of the dir, in fact
                                               the index of the table in the cluster chain */
extern  idata Byte    fat_dchain_nb_clust;  /* the offset of the cluster from the first cluster
                                               of the dir fragment */
extern  xdata Uint32  fat_dir_start_sect;   /* start sector of dir list */
extern  xdata Uint16  fat_dir_current_offs; /* entry offset from fat_dir_current_sect */
extern  xdata Byte    fat_last_dclust_index;/* index of the last cluster in directory chain */
extern  xdata fat_st_cache   fat_cache;     /* The cache structure, see the .h for more info */
extern  xdata fat_st_clust_chain dclusters[MAX_DIR_FRAGMENT_NUMBER];
                                            /* cluster chain for the current directory */
extern  xdata char  ext[3];                 /* file extension (limited to 3 characters) */
#define fat_dir_entry_list  fat_buf_sector  /* manual variable overlay */


/* file management */
extern  data  Uint16  fat_fclust_byte_count;/* byte counter in file cluster */
extern  idata Byte    fat_last_clust_index; /* index of the last cluster in file chain */
extern  idata Byte    fat_fchain_index;     /* the number of the fragment of the file, in fact
                                               the index of the table in the cluster chain */
extern  idata Uint16  fat_fchain_nb_clust;  /* the offset of the cluster from the first cluster
                                               of the file fragment */

extern  xdata fat_st_clust_chain fclusters[MAX_FILE_FRAGMENT_NUMBER];
                                            /* cluster chain for the current file */


/* Mode repeat A/B variables */
extern xdata  Byte    fat_fchain_index_save;         
extern xdata  Byte    fat_fchain_nb_clust_save;
extern xdata  Uint16  fat_fclust_byte_count_save;

static xdata fat_st_free_space free_space;

/*_____ D E C L A R A T I O N ______________________________________________*/

static  void    fat_get_dir_entry (fat_st_dir_entry xdata *);
static  void    fat_get_dir_file_list (Byte);
static  bit     fat_get_clusters (fat_st_clust_chain xdata *, Byte);
static  bit     fat_set_clusters (void);
static  bit     fat_dopen (void);
static  bit     fat_dseek (Int16);
static  Byte    fat_dgetc (void);

code Byte PBR_record_part1[] =
{
  0xEB, 0x3C, 0x90, /* JMP instruction to boot code */
  'O', 'E', 'M', ' ', 'N', 'A', 'M', 'E', /* OEM name */
  SECTOR_SIZE, SECTOR_SIZE >> 8, /* number of bytes per sector */
  0x00, /* number of sector per cluster */
  NB_RESERVED, NB_RESERVED >> 8, /* number of reserved sector */
  NB_FATS, /* number of FAT */
  NB_ROOT_ENTRY, NB_ROOT_ENTRY >> 8, /* number of root directory entries */
  0x00, 0x00, /* total sectors if less than 65535 */
  HARD_DISK, /* media byte */
};

code Byte PBR_record_part2[] =
{
  FAT_DRIVE_NUMBER, /* Drive number */
  0x00, /* not used */
  FAT_EXT_SIGN, /* extended boot signature */
  0x00, 0x00, 0x00, 0x00, /* volume ID */
  'N', 'O', ' ', 'N', 'A', 'M', 'E', ' ', ' ', ' ', ' ', /* volume label */
  'F', 'A', 'T', '1', 0x00, ' ', ' ', ' ', /* File system type in ASCII */

};

/*F**************************************************************************
* NAME: fat_load_sector
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   This function load a sector in fat_buf_sector
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*   
*****************************************************************************/
bit fat_load_sector(Uint32 sector)
{
Uint16 i;
  if (Hard_read_open(sector) == OK)
  {
    for (i = 0; i < (SECTOR_SIZE); i++)
    {
      fat_buf_sector[i++] = Hard_read_byte();
      fat_buf_sector[i++] = Hard_read_byte();
      fat_buf_sector[i++] = Hard_read_byte();
      fat_buf_sector[i]   = Hard_read_byte();
    }
    Hard_read_close();
    return OK;
  }
  else
  {
    return KO;
  }
}

/*F**************************************************************************
* NAME: fat_install
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*   - OK: intallation succeeded
*   - KO: - partition 1 signature not recognized
*         - FAT type is not FAT12/FAT16
*         - sector size is not 512 bytes
*         - MBR or PBR signatures are not correct
*         - low level read open failure
*----------------------------------------------------------------------------
* PURPOSE:
*   Install the fat system, read mbr, bootrecords...
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   if MBR not found, try to mount unpartitionned FAT
*   sector size is fixed to 512 bytes to simplify low level drivers
*   fat_ptr_fats = partition offset + nb_reserved_sector
*   fat_ptr_rdir = fat_ptr_fat + fat_size * nb_fat
*   fat_ptr_data = fat_ptr_rdir + nb_root_entries * 32 / 512
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit fat_install (void)
{          
Uint32 fat_nb_sector;

  /* MBR/PBR determination */
  fat_ptr_fats = 1;
  if (fat_load_sector(MBR_ADDRESS) == OK)
  {
    
    if ((fat_buf_sector[0] == 0xEB) &&      /* PBR Byte 0 */
        (fat_buf_sector[2] == 0x90) &&      /* PBR Byte 2 */
        ((fat_buf_sector[21] & 0xF0) == 0xF0)) /* PBR Byte 21 : Media byte */
    {
      if ((fat_buf_sector[510] != LOW(BR_SIGNATURE)) &&         /* check PBR signature */
          (fat_buf_sector[511] != HIGH(BR_SIGNATURE)))
      {
        return KO;
      }
      else
      {
        fat_ptr_fats = 0x00000000; /* first sector is PBR */
      }
    }
    else
    {   /* first sector is MBR */
      if ((fat_buf_sector[446] != PARTITION_ACTIVE) && 
          (fat_buf_sector[446] != 0x00))
      {
        return KO;                            /* not a MBR */
      }
      else
      {
        /* read partition offset (in sectors) at offset 8 */
        ((Byte*)&fat_ptr_fats)[3] = fat_buf_sector[454];
        ((Byte*)&fat_ptr_fats)[2] = fat_buf_sector[455];
        ((Byte*)&fat_ptr_fats)[1] = fat_buf_sector[456];
        ((Byte*)&fat_ptr_fats)[0] = fat_buf_sector[457];
        if ((fat_buf_sector[510] != LOW(BR_SIGNATURE)) &&         /* check PBR signature */
            (fat_buf_sector[511] != HIGH(BR_SIGNATURE)))
        {
          return KO;
        }

      }

    }
  }
  else
  {
    return KO;
  }

  /* read and check usefull PBR info */
  if (fat_load_sector(fat_ptr_fats) == OK) 
  {
    if ((fat_buf_sector[11] != LOW(SECTOR_SIZE)) ||  /* read sector size (in bytes) */
        (fat_buf_sector[12] != HIGH(SECTOR_SIZE)))
    {
      return KO;
    }

    /* read cluster size (in sector) */
    fat_cluster_size = fat_buf_sector[13];
    fat_cluster_mask = HIGH((Uint16)fat_cluster_size * SECTOR_SIZE) - 1;
    /* compute FATs sector address: add reserved sector number */
    fat_ptr_fats += fat_buf_sector[14];
    fat_ptr_fats += (Uint16)fat_buf_sector[15] << 8;
    /* read number of FATs */
    if (fat_buf_sector[16] == 2)
      fat_2_is_present = TRUE;
    else
      fat_2_is_present = FALSE;
    /* read number of dir entries  and compute rdir offset */
    ((Byte*)&fat_ptr_data)[3] = fat_buf_sector[17];
    ((Byte*)&fat_ptr_data)[2] = fat_buf_sector[18];
    ((Byte*)&fat_ptr_data)[1] = 0;
    ((Byte*)&fat_ptr_data)[0] = 0;
    fat_ptr_data = (fat_ptr_data * DIR_SIZE) / SECTOR_SIZE;
    /* read number of sector in partition (<32Mb) */
    ((Byte*)&fat_nb_sector)[3] = fat_buf_sector[19];
    ((Byte*)&fat_nb_sector)[2] = fat_buf_sector[20];
    ((Byte*)&fat_nb_sector)[1] = 0x00;
    ((Byte*)&fat_nb_sector)[0] = 0x00;
    /* compute root directory sector address */
    ((Byte*)&fat_fat_size)[1] = fat_buf_sector[22];
    ((Byte*)&fat_fat_size)[0] = fat_buf_sector[23];
    
    fat_ptr_rdir = fat_buf_sector[16] * fat_fat_size;
    fat_ptr_rdir += fat_ptr_fats;

    /* read number of sector in partition (>32Mb) */
    if (!fat_nb_sector)
    {
      ((Byte*)&fat_nb_sector)[3] = fat_buf_sector[32];
      ((Byte*)&fat_nb_sector)[2] = fat_buf_sector[33];
      ((Byte*)&fat_nb_sector)[1] = fat_buf_sector[34];
      ((Byte*)&fat_nb_sector)[0] = fat_buf_sector[35];
    }

    fat_count_of_clusters = (fat_nb_sector - (1 + (fat_buf_sector[16] * fat_fat_size) + fat_ptr_data)) 
                            / fat_cluster_size;
    if (fat_count_of_clusters <= MAX_CLUSTERS12)
      fat_is_fat16 = FALSE;
    else
      if (fat_count_of_clusters <= MAX_CLUSTERS16)
        fat_is_fat16 = TRUE;
      /* else is FAT32 not supported */

    /* compute data sector address */
    fat_ptr_data += fat_ptr_rdir;
    /* check partition signature */
    if ((fat_buf_sector[510] != LOW(BR_SIGNATURE)) &&
        (fat_buf_sector[511] != HIGH(BR_SIGNATURE)))
    {
      return KO;
    }

    return OK;
  }
  else
  { /* low level error */
    return KO;
  }
}


/*F**************************************************************************
* NAME: fat_get_dir_entry
*----------------------------------------------------------------------------
* PARAMS:
*   entry: directory entry structure
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Get from directory all information about a directory or file entry
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   This function reads directly datas from sectors
*   It automaticaly computes difference between LFN and normal entries
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
void fat_get_dir_entry (fat_st_dir_entry xdata *entry)
{
bit     exit_flag = FALSE;
bit     lfn_entry_found = FALSE;
Byte    i;

  /* clear the name buffer */
  for (i = MAX_FILENAME_LEN; i != 0; lfn_name[--i] = '\0');

  while (!exit_flag)
  /* loop while the entry is not a normal one. */
  {
    /* read the directory entry */
    if (dir_is_root == TRUE)
    { /* root dir is linear -> Hard_read_byte() */
      for (i = 0; i < DIR_SIZE; i++)
        gl_buffer[i] = Hard_read_byte();
    }
    else
    { /* subdir can be fragmented -> dgetc() */
      for (i = 0; i < DIR_SIZE; i++)
        gl_buffer[i] = fat_dgetc();
    }

    /*computes gathered data
    /* check if we have a LFN entry */
    if (gl_buffer[11] != ATTR_LFN_ENTRY)
    {
      if (!lfn_entry_found)
      {
        /* true DOS 8.3 entry format */
        for (i = 0; i < 8; i++)
        {
          lfn_name[i] = gl_buffer[i];
          
          if (lfn_name[i] == ' ')
          { /* space is end of name */
            break;
          }
        }
        /* append extension */
        lfn_name[i++] = '.';
        lfn_name[i++] = gl_buffer[8];
        lfn_name[i++] = gl_buffer[9];
        lfn_name[i++] = gl_buffer[10];

        for (; i != 14; i++)
        {
          lfn_name[i] = ' ';       /* append spaces for display reason */
        }
        lfn_name[i] = '\0';        /* end of string */
      }
        
      else
      { /* LFN name treatment */
        i = 0;
        /* search for the end of the string */
        while (lfn_name[i] != '\0')
        { 
          i++;
        }
        if (i <= 14)
        { /* append spaces for display reason (no scrolling) */
          while (i != 14)
          {
            lfn_name[i++] = ' ';
          }
        }
        else
        { /* append beginning of name to ease scrolling display */
          lfn_name[i++] = ' ';
          lfn_name[i++] = ' ';
          lfn_name[i++] = lfn_name[0];
          lfn_name[i++] = lfn_name[1];
          lfn_name[i++] = lfn_name[2];
          lfn_name[i++] = lfn_name[3];
          lfn_name[i++] = lfn_name[4];
          lfn_name[i++] = lfn_name[5];
          lfn_name[i++] = lfn_name[6];
          lfn_name[i++] = lfn_name[7];
          lfn_name[i++] = lfn_name[8];
          lfn_name[i++] = lfn_name[9];
          lfn_name[i++] = lfn_name[10];
          lfn_name[i++] = lfn_name[11];
          lfn_name[i++] = lfn_name[12];
        }
        lfn_name[i] = '\0';        /* end of name */
      }

      /* store extension */
      ext[0]= gl_buffer[8];
      ext[1]= gl_buffer[9];
      ext[2]= gl_buffer[10];

      /* standard computing for normal entry */
      entry->attributes = gl_buffer[11];
      entry->start_cluster = gl_buffer[26];
      entry->start_cluster += ((Uint16) gl_buffer[27]) << 8;
      entry->size.b[3] = gl_buffer[28];
      entry->size.b[2] = gl_buffer[29];
      entry->size.b[1] = gl_buffer[30];
      entry->size.b[0] = gl_buffer[31];
      /* now it's time to stop */
      exit_flag = TRUE;
    }
    else
    { /* LFN entry format */
      lfn_entry_found = TRUE;             /* a 8.3 name will follow */

      if ((gl_buffer[0] & LFN_SEQ_MASK) <= MAX_LFN_ENTRIES)   
      {                         /* Maximum number of entries for LFN? */
        for (i = 0; i < 5; i++)
          lfn_name[i + 13*((gl_buffer[0] & LFN_SEQ_MASK) - 1)] = gl_buffer[2*i + 1];
        for (i = 0; i < 6; i++)
          lfn_name[i + 5 + 13*((gl_buffer[0] & LFN_SEQ_MASK) - 1)] = gl_buffer[2*i + 14];
        for (i = 0; i < 2; i++)
          lfn_name[i + 11 + 13*((gl_buffer[0] & LFN_SEQ_MASK) - 1)] = gl_buffer[2*i + 28];
      }
    }
  }
  Hard_read_close();                        /* close physical read */
}


/*F**************************************************************************
* NAME: fat_get_dir_file_list
*----------------------------------------------------------------------------
* PARAMS:
*   id: file extension to select
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Construct the file directory list
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   The value are relative position with the previous file.
*   Call to this function assume that the dir fragment chain has been created
*----------------------------------------------------------------------------
* REQUIREMENTS:
*   Maximum of 256 entries between 2 authorized file (id extension)
*   because the relative position is stored on one byte.
*   To allow more than 256 entries (-> 32768), change type of
*   fat_dir_entry_list[] from Byte to Uint16 (no overflow management)
*   and change MAX_DIRECTORY_GAP_FILE from 255 to 32767
*****************************************************************************/
void fat_get_dir_file_list (Byte id)
{
Uint16 index;                               /* chain index */
Uint16 counter_entry;                       /* entry counter: 0..MAX_DIRECTORY_FILE */
Uint16 entry_pos;                           /* relative entry position */
Uint16 entry_pos_saved;                     /* used when the file is not the id etension */
Byte   i;

  index = 0;
  fat_dir_list_last = 0;
  counter_entry = 0;
  entry_pos = 0;   
  fat_dir_start_sect = fat_dir_current_sect;
  fat_dir_current_offs = 0;
  
  Hard_read_open(fat_dir_start_sect);
  
  do /* scan all entries */
  {
    if (dir_is_root == TRUE)
    { /* root dir is linear -> Hard_read_byte() */
      for (i = 0; i < DIR_SIZE; i++)
        gl_buffer[i] = Hard_read_byte();
    }
    else
    { /* subdir can be fragmented -> dgetc() */
      for (i = 0; i < DIR_SIZE; i++)
        gl_buffer[i] = fat_dgetc();
    }
    counter_entry++;                          /* increase the # entry         */

    if ((gl_buffer[0] != FILE_DELETED) && (gl_buffer[0] != FILE_NOT_EXIST))
    { /* Existing file ? */ 
      fat_dir_entry_list[index] = entry_pos;  /* save the relative position   */
      entry_pos_saved = entry_pos;
      entry_pos = 1;                          /* reset the relative position  */
      index++;                                /* increase the index           */

      while (gl_buffer[11] == ATTR_LFN_ENTRY) /* LFN entry ?                  */
      {                                       /* then read all the LFN entry  */
        if (dir_is_root == TRUE)
        { /* root dir is linear -> Hard_read_byte() */
          for (i = 0; i < DIR_SIZE; i++)
            gl_buffer[i] = Hard_read_byte();
        }
        else
        { /* subdir can be fragmented -> dgetc() */
          for (i = 0; i < DIR_SIZE; i++)
            gl_buffer[i] = fat_dgetc();
        }

        counter_entry++;                    /* increase the # entry */
        entry_pos++;                        /* increase the relative position */
                                            /* for the next file */
      }

      /* filter on the file type */
      fat_cache.current.attributes = gl_buffer[11];
      ext[0] = gl_buffer[8];
      ext[1] = gl_buffer[9];
      ext[2] = gl_buffer[10];
      if ((fat_check_ext() & id) == FILE_XXX)
      {                                     /* Don't valid the entry */
        index--;                                              
        entry_pos += entry_pos_saved;
      }
    }
    else   /* Not an existing file */
      entry_pos++;

    fat_dir_list_last = index;              /* update last file index */
    /* For sub-directory, there is no logical limit for the number of entries */
    /* In order to detect the last file, we check gl_buffer[0]                */
    /* We can put in the chain directory MAX_DIRECTORY_FILE selected file     */
    if (gl_buffer[0] == FILE_NOT_EXIST)
      index = MAX_DIRECTORY_FILE;
    /* Overflow of entry_pos */
    if (entry_pos > MAX_DIRECTORY_GAP_FILE)
      index = MAX_DIRECTORY_FILE;
    /* For Root directory, the maximum entries is 512!                        */
    if ((dir_is_root == TRUE) && (counter_entry == MAX_DIRECTORY_FILE))
      index = MAX_DIRECTORY_FILE;
    if (dir_is_root == FALSE)
    {
      /* check if we are at the end of the directory */
      if ((((Byte*)&fat_dclust_byte_count)[1] == 0x00) &&
      ((((Byte*)&fat_dclust_byte_count)[0] & fat_cluster_mask) == 0x00) && 
      (fat_last_dclust_index == fat_dchain_index))
        index = MAX_DIRECTORY_FILE;
    }
  }
  while (index < MAX_DIRECTORY_FILE);

  fat_dir_current_sect = fat_dir_start_sect;
  Hard_read_close();
}


/*F**************************************************************************
* NAME: fat_get_root_directory
*----------------------------------------------------------------------------
* PARAMS:
*   id: file extension to select
*
* return:
*   - OK: file available
*   - KO: no requested file found
*   - KO: low_level memory error
*----------------------------------------------------------------------------
* PURPOSE:
*   Select first available file/dir in root diretory
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   Fill all the cache information for the first time
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit fat_get_root_directory (Byte id)
{
  /* select first root dir entry */
  fat_dir_current_sect = fat_ptr_rdir;
  fat_dclust_byte_count = 0;
  dir_is_root = TRUE;

  fat_get_dir_file_list(id);                /* create list of entries */
  if (fat_dir_list_last == 0)
    return KO;                              /* no requested (id) entry */

  fat_dir_list_index = 0;                   /* point on first root entry */

  /* extract info from table */
  if (fat_dseek(fat_dir_entry_list[0] * DIR_SIZE) == OK)
  {
    fat_get_dir_entry(&fat_cache.current);  /* update current file info */
    /* parent dir is also root */
    fat_cache.parent.start_cluster = 0;   
    fat_cache.parent.attributes = ATTR_ROOT_DIR;  /* mark as root dir */
    return OK;
  }
  else
    return KO;                              /* low level error */
}


/*F**************************************************************************
* NAME: fat_goto_next
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*   - OK: next file available
*   - KO: last file reached
*   - KO: low_level memory error
*----------------------------------------------------------------------------
* PURPOSE:
*   Fetch the next dir/file info in cache
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/ 
bit fat_goto_next (void)
{
  if (fat_dir_list_index < (fat_dir_list_last - 1))
  {
    fat_dir_list_index++;

    if (fat_dseek((Int16)(fat_dir_entry_list[fat_dir_list_index] * DIR_SIZE)) == OK)
    {
      fat_get_dir_entry(&fat_cache.current);/* update current file info */
      return OK;
    }
    else
      return KO;                            /* low level error */
  }
  else
    return KO;                              /* already on last file */
}


/*F**************************************************************************
* NAME: fat_goto_prev
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*   - OK: previous file available
*   - KO: first file reached
*   - KO: low_level memory error
*----------------------------------------------------------------------------
* PURPOSE:
*   Fetch the previous directory info in cache
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/ 
bit fat_goto_prev (void)
{
Byte min;
  
  if (dir_is_root)
    min = 0;
  else
    min = 2;

  if (fat_dir_list_index != min)            /* first file of the directory? */
  {
    if (fat_dseek((Int16)(fat_dir_entry_list[fat_dir_list_index] * (-DIR_SIZE))) == OK)
    { /* go to previous file */
      fat_dir_list_index--;
      fat_get_dir_entry(&fat_cache.current);/* update current file info */
      return OK;
    }
    else
      return KO;                            /* low level error */
  }
  else
    return KO;                              /* already on first file */
}

/*F**************************************************************************
* NAME: fat_seek_last
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*   OK: last file available
*   KO: low level error
*----------------------------------------------------------------------------
* PURPOSE:
*   Fetch the last directory info in cache
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/ 
bit fat_seek_last (void)
{
Uint16 gl_offset;
Uint16 i;
  
  for (i = fat_dir_list_index + 1, gl_offset = 0; i < fat_dir_list_last; fat_dir_list_index++, i++)
    gl_offset += fat_dir_entry_list[i];

  if (fat_dseek(gl_offset * DIR_SIZE) == OK)
  {
    fat_get_dir_entry(&fat_cache.current);      
    return OK;
  }
  else
    return KO;                              /* low level error */
}

/*F**************************************************************************
* NAME: fat_seek_entry_record
*----------------------------------------------------------------------------
* PARAMS:
*   fat_dir_list_index : # of the fetched entry
*   
* return:
*   OK: file available
*   KO: low level error
*----------------------------------------------------------------------------
* PURPOSE:
*   Fetch the selected entry
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/ 
bit fat_seek_entry_record (void)
{
Uint16 gl_offset = 0;
Uint16 i;
  if (dir_is_root)
  {
    fat_dir_current_sect = fat_ptr_rdir;
  }
  else
  {
    fat_dir_current_sect = (((Uint32)(dclusters[0].cluster)) * fat_cluster_size)
                           + fat_ptr_data;
  }
  fat_dir_current_offs = 0;                 /* reset the global offset */

  for (i = 0; i <= fat_dir_list_index; i++)
    gl_offset += fat_dir_entry_list[i];

  return fat_dseek(gl_offset * DIR_SIZE);
}

/*F**************************************************************************
* NAME: fat_seek_first
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*   - OK: first file found
*   - KO: low level error
*----------------------------------------------------------------------------
* PURPOSE:
*   Fetch the first directory info in cache
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/ 
bit fat_seek_first (void)
{
  fat_dir_current_offs = 0;                 /* reset the global offset */

  if (dir_is_root)
  { /* root diretory */
    fat_dir_list_index = 0;                 /* point first root entry */
    if (fat_dseek((Int16)(fat_dir_entry_list[0] * DIR_SIZE)) == OK)
    {
      fat_get_dir_entry(&fat_cache.current);/* update first file info */      
      return OK;
    }
    else
    {
      return KO;                            /* low level error */
    }
  }
  else
  { /* not root dir */
    fat_dir_list_index = 1;                 /* point ".." entry */
    if (fat_dseek((Int16)(fat_dir_entry_list[1] * DIR_SIZE)) == OK)
    {
      fat_get_dir_entry(&fat_cache.parent); /* update parent dir info */
      return fat_goto_next();               /* update first file info */
    }
    else
      return KO;                            /* low level error */
  }
}


/*F**************************************************************************
* NAME: fat_goto_subdir
*----------------------------------------------------------------------------
* PARAMS:
*   id: file extension to select
*
* return:
*   - OK: subdir selected
*   - KO: current entry not a directory
*   - KO: no file in subdir
*   - KO: low level error
*----------------------------------------------------------------------------
* PURPOSE:
*   Go to the subdir if current is a directory
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   Also called by goto_parentdir() with current info from parent info
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/ 
bit fat_goto_subdir (Byte id)
{                        
  /* check if current file is a directory */
  if ((fat_cache.current.attributes & ATTR_DIRECTORY) == ATTR_DIRECTORY)
  {
    /* computes the sector address (RELATIVE) */
    if (fat_cache.current.start_cluster != 0)
    { /* go to not root dir */ 
      dir_is_root = FALSE;                  /* not the root dir */
      /* get directory allocation table */
      fat_get_clusters(&dclusters, MAX_DIR_FRAGMENT_NUMBER);

      /* Save last index position for chain cluster */
      fat_last_dclust_index = fat_last_clust_index;
      /* initialize fat pointers */
      fat_dchain_nb_clust = 0;
      fat_dchain_index = 0;
      fat_dclust_byte_count = 0;
                          
      /* computes sector address from allocation table */
      fat_dir_current_sect = (((Uint32)(dclusters[0].cluster)) * fat_cluster_size)
                           + fat_ptr_data;
    }
    else
    { /* go to root dir */
      return fat_get_root_directory(id);
    }

    fat_get_dir_file_list(id);              /* create list of entries */

    fat_dir_list_index = 1;                 /* point ".." entry */
    if (fat_dseek((Int16)(fat_dir_entry_list[1] * DIR_SIZE)) == OK)
    {
      fat_get_dir_entry(&fat_cache.parent); /* update parent dir info */
      return fat_goto_next();               /* update first file info */
    }
    else
      return KO;                            /* low level error */
  }
  else
    return KO;                              /* current entry is not a dir */
}
  

/*F**************************************************************************
* NAME: fat_goto_parentdir
*----------------------------------------------------------------------------
* PARAMS: 
*   id: file extension to select
*
* return:
*   status: OK: parent_dir selected
*           KO: no parent dir (root)
*----------------------------------------------------------------------------
* PURPOSE:
*   Go to the parent directory
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   File pointed is sub-dir if parent dir is not root or first file if root
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/ 
bit fat_goto_parentdir (Byte id)
{ 
Uint16 temp_cluster;

  if (dir_is_root)
  { /* already in root dir */
    fat_seek_first();                       /* point on first file */
    return KO;
  }
  else
  { /* not in root dir */
    temp_cluster = dclusters[0].cluster + 2;/* save cluster info */
    fat_cache.current = fat_cache.parent;  /* goto the parent directory */

    /* issue the equivalent to a cd .. DOS command */
    if (fat_goto_subdir(id))
    { /* reselect the dir entry in list */
      while (temp_cluster != fat_cache.current.start_cluster)
      {
        if (fat_goto_next() == KO)
          break;
      }
      if (temp_cluster == fat_cache.current.start_cluster)
        return OK;
      else
        return KO;
    }
    else
    {
      return KO;
    }
  }
}


/*F**************************************************************************
* NAME: fat_update_fat_sector
*----------------------------------------------------------------------------
* PARAMS: 
*   sector_number : fat sector position
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   update a sector of fat
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   This function check if there is 2 fats to be updated
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/  
void fat_update_fat_sector (Uint16 sector_number)
{
Uint16 i;

  /* FAT 1 update */
  Hard_write_open(fat_ptr_fats + sector_number);
  for (i = 0; i < SECTOR_SIZE; i++)
    Hard_write_byte(fat_buf_sector[i]);
  Hard_write_close();
  if (fat_2_is_present == TRUE)
  {
    /* FAT 2 update */
    Hard_write_open(fat_ptr_fats + sector_number + fat_fat_size);
    for (i = 0; i < SECTOR_SIZE; i++)
      Hard_write_byte(fat_buf_sector[i]);
    Hard_write_close();
  }      
}


    

   
/*F**************************************************************************
* NAME: fat_update_buf_fat
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   This function check if a fat sector have to be writen.
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*   fat_root_entry must be updated.
*****************************************************************************/
void fat_update_buf_fat(Uint16 cluster_old, Uint16 cluster, bit end)
{
bit fat_12_parity;
Byte sector_number;
#define i cluster_old

  fat_12_parity = ((Byte*)&cluster_old)[1] & 0x01;
  sector_number = cluster_old * 3 / 1024;
  i = (cluster_old * 3 / 2) & 0x1FF;

  if (end == TRUE)
    cluster = 0xFFF;
  if (fat_12_parity == 0)
  {
    fat_buf_sector[i++] = ((Byte*)&cluster)[1];
    if (((Byte*)&i)[0] == 0x02)
    {
      fat_update_fat_sector(sector_number); 
      sector_number++;
      fat_load_sector(fat_ptr_fats + sector_number);
      ((Byte*)&i)[0] = 0x00;
    }
    fat_buf_sector[i] &= 0xF0;
    fat_buf_sector[i] |= ((Byte*)&cluster)[0] & 0x0F;
  }
  else
  {
    fat_buf_sector[i] &= 0x0F;
    fat_buf_sector[i] |= (((Byte*)&cluster)[1] & 0x0F) << 4;
    i++;
    if (((Byte*)&i)[0] == 0x02)
    {
      fat_update_fat_sector(sector_number); 
      sector_number++;
      fat_load_sector(fat_ptr_fats + sector_number);
      ((Byte*)&i)[0] = 0x00;
    }
    fat_buf_sector[i++] = (cluster & 0x0FF0) >> 4;
    if (((Byte*)&i)[0] == 0x02)
    {
      fat_update_fat_sector(sector_number); 
      sector_number++;
      fat_load_sector(fat_ptr_fats + sector_number);
      ((Byte*)&i)[0] = 0x00;
    }
  }
  if (end == TRUE)
    fat_update_fat_sector(sector_number);
  #undef i

}
/*F**************************************************************************
* NAME: fat_update_entry_fat
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Update root entry and FAT after a writing file session (create or re-write)
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*   fat_root_entry must be updated.
*****************************************************************************/
void fat_update_entry_fat (void)
{
Byte index;
Byte chain_index;
Byte sector_number;
Uint16 i;
Uint16 cluster;
Uint16 nb_cluster;

/*********************/
/* Update root entry */
/*********************/
  fat_load_sector(fat_ptr_rdir + (fat_root_entry >> 4));
  i = (fat_root_entry % 16) * 32 ;               /* Position of entry in the sector */
  /* Update file size */
  if (fat_cache.current.size.l <= fat_file_size.l)
    fat_cache.current.size.l = fat_file_size.l;
  fat_buf_sector[i + 28] = fat_cache.current.size.b[3];
  fat_buf_sector[i + 29] = fat_cache.current.size.b[2];
  fat_buf_sector[i + 30] = fat_cache.current.size.b[1];
  fat_buf_sector[i + 31] = fat_cache.current.size.b[0];

  ext[0] = fat_buf_sector[i + 8];
  ext[1] = fat_buf_sector[i + 9];
  ext[2] = fat_buf_sector[i + 10];
  

  Hard_write_open(fat_ptr_rdir + (fat_root_entry >> 4));
  for (i= 0; i< SECTOR_SIZE; i++)
    Hard_write_byte(fat_buf_sector[i]);
  Hard_write_close();


/********************/
/* Update fat 1 & 2 */
/********************/
  /* Calculate file size cluster */
  nb_cluster = (fat_cache.current.size.l / SECTOR_SIZE) / fat_cluster_size;
  if ((fat_cache.current.size.l % (fat_cluster_size * SECTOR_SIZE)))
  {
    nb_cluster++;
  }
  nb_cluster--;
  index = 0;

/********************/
/* FAT16 management */
/********************/
  if (fat_is_fat16)
  {
    /* init the starting cluster value */
    cluster = fclusters[0].cluster + 2;
    /* Start at first chain cluster */
    sector_number = cluster / 256;
    /* Bufferize fat sector */
    fat_load_sector(fat_ptr_fats + sector_number);
    /* i -> word fat sector position */
    i = (cluster * 2) & 0x1FF;
    chain_index = 1;
  
    while (nb_cluster != 0)
    {
      /* Determinate the value of the next cluster */
      if (fclusters[index].number == chain_index)
      {
        /* increase index */
        index++;
        cluster = fclusters[index].cluster + 2;
        fat_buf_sector[i++] = ((Byte*)&cluster)[1];
        fat_buf_sector[i]   = ((Byte*)&cluster)[0];
        chain_index = 1;
        if ( (cluster / 256) != sector_number)
        { /* Fat change sector */
          fat_update_fat_sector(sector_number);
          sector_number = (Uint16)(cluster / 256);
          fat_load_sector(fat_ptr_fats + sector_number);
        }
        i = (cluster * 2) & 0x1FF;

      }
      else
      {
        cluster++;
        fat_buf_sector[i++] = ((Byte*)&cluster)[1];
        fat_buf_sector[i++] = ((Byte*)&cluster)[0];
        chain_index++;
        if (((Byte*)&i)[0] == 0x02)
        {
          fat_update_fat_sector(sector_number); 
          sector_number++;
          fat_load_sector(fat_ptr_fats + sector_number);
          ((Byte*)&i)[0] = 0x00;
        }
      }
      nb_cluster--;
    }
    /* End of file indicate by 0xFFFF */
    fat_buf_sector[i++] = 0xFF;
    fat_buf_sector[i]   = 0xFF;
    fat_update_fat_sector(sector_number);
  }
/********************/
/* FAT12 management */
/********************/
  else    
  { 
    cluster = fclusters[index].cluster + 2;
    sector_number = cluster * 3 / 1024;
    /* Bufferize fat sector */
    fat_load_sector(fat_ptr_fats + sector_number);
    i = cluster;
    chain_index = 1;
    while (nb_cluster != 0)
    {
      /* Determinate the value of the next cluster */
      if (fclusters[index].number == chain_index)
      {
        /* increase index */
        index++;
        fat_update_buf_fat(cluster, fclusters[index].cluster + 2, 0);
        cluster = fclusters[index].cluster + 2;
        chain_index = 1;
        i = cluster * 3 / 1024;
        if ( i != sector_number)
        { /* Fat change sector */
          fat_update_fat_sector(sector_number);
          sector_number = i;
          fat_load_sector(fat_ptr_fats + sector_number);
        }
      }
      else
      {
        cluster++;
        fat_update_buf_fat(cluster - 1, cluster, 0);
        chain_index++;
      }
      nb_cluster--;
    }
    fat_update_buf_fat(cluster, cluster, 1);
  }

  /* Reconstruct list file */
  i = fat_dir_list_index;

  fat_dir_current_sect = fat_ptr_rdir;
  fat_dclust_byte_count = 0;
  dir_is_root = TRUE;

  fat_get_dir_file_list(fat_check_ext()); /* create list of entries */
  fat_dir_list_index = i;
  for (i = 0; i <= fat_dir_list_index; i++)
    fat_dseek(fat_dir_entry_list[i] * DIR_SIZE);

  fat_get_dir_entry(&fat_cache.current);          /* update current file info */
}


/*F**************************************************************************
* NAME: fat_fopen
*----------------------------------------------------------------------------
* PARAMS:
*   mode: READ:   open file for read
*         WRITE:  open file for write
*
* return:
*   - OK: file opened
*   - KO: file not opened: - file is empty
*                          - low level read error
*----------------------------------------------------------------------------
* PURPOSE:
*   Open the file in read or write mode
*----------------------------------------------------------------------------
* EXAMPLE:
*   if (fat_get_root_directory(FILE_WAV) == OK)       // Select first WAV file in root
*   {
*     fat_fopen(WRITE);                               // Open this file in WRITE mode
*     for (j = 0; j < 10; j++)
*       fat_fputc(buff[j]);
*     fat_fclose();
*   }
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*   For write mode, there must be an entry in the root and entry data must be
*   updated.
*****************************************************************************/
bit fat_fopen (bit mode)
{
  if (mode == READ)
  {
    if (fat_cache.current.size.l == 0)
    {
      return KO;                            /* file empty */
    }
    else
    {
      fat_fclust_byte_count = 0;            /* byte 0 of cluster */
  
      /* reset the allocation list variable */
      fat_fchain_index = 0;
      fat_fchain_nb_clust = 0;              /* start on first contiguous cl */
      /* get file allocation list */
      fat_get_clusters(&fclusters, MAX_FILE_FRAGMENT_NUMBER);
  
      /* seek to the beginning of the file */
      fat_open_mode = READ;
      return Hard_read_open(fat_ptr_data + ((Uint32)(fclusters[0].cluster) 
                                               * fat_cluster_size));
    }
  }
  else
  {
    fat_fclust_byte_count = 0;              /* byte 0 of cluster */
    fat_file_size.l = 0;
    flag_end_disk_file = FALSE;
    /* reset the allocation list variable */
    fat_fchain_index = 0;
    fat_fchain_nb_clust = 0;                /* start on first contiguous cl */
    fat_root_entry = fat_dclust_byte_count / 32;
    /* get file allocation list */
    fat_get_clusters(&fclusters, MAX_FILE_FRAGMENT_NUMBER);
    fat_open_mode = WRITE;
    return Hard_write_open(fat_ptr_data + ((Uint32)(fclusters[0].cluster) 
                                           * fat_cluster_size));
  }
}


/*F**************************************************************************
* NAME: fat_fclose
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Close opened file
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
void fat_fclose (void)
{
  if (fat_open_mode == READ)
  {
    Hard_read_close();                      /* close reading */
  }
  else
  {
    Hard_write_close();                     /* close writing */
    fat_update_entry_fat();                 /* Update entry and fat */
  }
}


/*F**************************************************************************
* NAME: fat_fcreate
*----------------------------------------------------------------------------
* PARAMS:
*   file_name   : file name of the file to be created
*   attribute   : file attribute (see fat.h)
*    
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Create a new file in the root directory.
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   This function update the root directory entry
*----------------------------------------------------------------------------
* REQUIREMENTS:
*
*****************************************************************************/
bit fat_fcreate (char *file_name, Byte attribute)
{
Byte temp_byte;
Uint16 j;
Uint16 index;
  /* Check file type */
  ext[0] = file_name[8];
  ext[1] = file_name[9];
  ext[2] = file_name[10];
  fat_cache.current.attributes = attribute;
  if ((fat_check_ext() == FILE_DIR) || (attribute == ATTR_DIRECTORY))
    return KO;

  /* get free clusters list */
  if ( fat_set_clusters() == KO )
    return KO;

  /* no more place in file liste */
  if (fat_dir_list_last == (MAX_DIRECTORY_FILE - 1))
    return KO;

  /* Find the first free entry in root */
  index = 0;
  if (Hard_read_open(fat_ptr_rdir) == KO) 
    return KO;
  
  temp_byte = Hard_read_byte();
  while ((temp_byte != FILE_NOT_EXIST) && (temp_byte != FILE_DELETED))
  {
    for (temp_byte = DIR_SIZE - 1; temp_byte != 0; temp_byte--)
      Hard_read_byte();
    temp_byte = Hard_read_byte();
    index++;
  }

  Hard_read_close();

  if ((dir_is_root == TRUE) && (index >= NB_ROOT_ENTRY))    /* Maximum entries in root directory */
    return KO;
  /* Construct the entry */
  for (temp_byte = 12; temp_byte < 32; temp_byte++)
    gl_buffer[temp_byte] = 0x00;
  gl_buffer[0] = file_name[0];
  gl_buffer[1] = file_name[1];
  gl_buffer[2] = file_name[2];
  gl_buffer[3] = file_name[3];
  gl_buffer[4] = file_name[4];
  gl_buffer[5] = file_name[5];
  gl_buffer[6] = file_name[6];
  gl_buffer[7] = file_name[7];
  gl_buffer[8] = file_name[8];
  gl_buffer[9] = file_name[9];
  gl_buffer[10] = file_name[10];;
  gl_buffer[11] = attribute;        /* Attribute : archive */
  gl_buffer[26] = fclusters[0].cluster + 2;    /* Low word first cluster number */
  gl_buffer[27] = (fclusters[0].cluster + 2) >> 8;

  fat_load_sector(fat_ptr_rdir + (index / 16));
  j = (index % 16) * DIR_SIZE ;             /* Position of entry in the sector */
  for (temp_byte = 0; temp_byte < DIR_SIZE; temp_byte++)
    fat_buf_sector[j++] = gl_buffer[temp_byte];

  if (Hard_write_open(fat_ptr_rdir + (index / 16)) == KO)
    return KO;
  for (j = 0; j < SECTOR_SIZE; j++)
    Hard_write_byte(fat_buf_sector[j]);
  Hard_write_close();
  fat_root_entry = index;

  /* Reconstruct file list */
  fat_dir_current_sect = fat_ptr_rdir;
  fat_dclust_byte_count = 0;
  dir_is_root = TRUE;

  fat_get_dir_file_list(fat_check_ext());                /* create list of entries */

  fat_dir_list_index = 0;
  j = fat_dir_entry_list[0];
  while (j < index)
  {    
    if ( fat_dseek(fat_dir_entry_list[fat_dir_list_index] * DIR_SIZE) == KO)
      return KO;
    fat_dir_list_index++;                         /* point on next root entry */
    j += fat_dir_entry_list[fat_dir_list_index];
  }
  
  if (fat_dseek(fat_dir_entry_list[fat_dir_list_index] * DIR_SIZE) == KO)
    return KO;


  fat_get_dir_entry(&fat_cache.current);          /* update current file info */
  /* parent dir is also root */
  fat_cache.parent.start_cluster = 0;   
  fat_cache.parent.attributes = ATTR_ROOT_DIR;    /* mark as root dir */

  /* open file in write mode */
  fat_fclust_byte_count = 0;                      /* byte 0 of cluster */
  fat_file_size.l = 0;
  fat_cache.current.size.l = 0;
  flag_end_disk_file = FALSE;
  fat_fchain_index = 0;                           /* reset the allocation list variable */
  fat_fchain_nb_clust = 0;                        /* start on first contiguous cl */
  fat_open_mode = WRITE;
  return Hard_write_open(fat_ptr_data + ((Uint32)(fclusters[0].cluster) 
                                         * fat_cluster_size));
}


/*F**************************************************************************
* NAME: fat_clear_fat
*----------------------------------------------------------------------------
* PARAMS:
*    
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Reset FAT clusters value
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   
*----------------------------------------------------------------------------
* REQUIREMENTS:
*   fclusters[] variable must be updated.
*****************************************************************************/
void fat_clear_fat (void)
{
Uint16 sector_number;
Uint16 i;
Uint16 cluster;
Uint16 temp;
bit end;

  /* init the starting cluster value */
  cluster = fclusters[0].cluster + 2;
  /* Start at first chain cluster */
  sector_number = (fat_is_fat16 == TRUE) ? cluster / 256 : cluster * 3 / 1024;
  /* Bufferize fat sector */
  fat_load_sector(fat_ptr_fats + sector_number);
  end = FALSE;
  do 
  {
    temp = (fat_is_fat16 == TRUE) ? cluster / 256 : cluster * 3 / 1024;
    if (temp != sector_number)
    {
      fat_update_fat_sector(sector_number);
      sector_number =  temp;
      fat_load_sector(fat_ptr_fats + sector_number);
    }
    if (fat_is_fat16 == TRUE)
    {
      i = (cluster * 2) & 0x1FF;
      ((Byte *)&cluster)[1] = fat_buf_sector[i];
      fat_buf_sector[i++] = 0;
      ((Byte *)&cluster)[0] = fat_buf_sector[i];
      fat_buf_sector[i] = 0;
      end = (cluster == 0xFFFF);
    }
    else
    {
      i = (cluster * 3 / 2) & 0x1FF;
      if ((cluster & 0x01) == 0)
      {
        ((Byte *)&cluster)[1] = fat_buf_sector[i];
        fat_buf_sector[i] = 0x00;
        i++;
        if (i == 512)
        {
          fat_update_fat_sector(sector_number);
          sector_number++;
          fat_load_sector(fat_ptr_fats + sector_number);
          i = 0;
        }
        ((Byte *)&cluster)[0] = fat_buf_sector[i] & 0x0F;
        fat_buf_sector[i] &= 0xF0;
      }
      else
      {
        cluster = (fat_buf_sector[i] & 0xF0) >> 4;
        fat_buf_sector[i] &=  0x0F;
        i++;
        if (i == 512)
        {
          fat_update_fat_sector(sector_number);
          sector_number++;
          fat_load_sector(fat_ptr_fats + sector_number);
          i = 0;
        }
        cluster += (fat_buf_sector[i] << 4);
        fat_buf_sector[i] = 0x00;
      }
      end = (cluster == 0xFFF);

    }

  }
  while (!end);
  fat_update_fat_sector(sector_number);
}


/*F**************************************************************************
* NAME: fat_refresh_dir_file_info
*----------------------------------------------------------------------------
* PARAMS:
*    
* return:
*   
*----------------------------------------------------------------------------
* PURPOSE:
*   Reconstruct the file directory list and seek to the file pointed by
*   fat_dir_list_index
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   
*----------------------------------------------------------------------------
* REQUIREMENTS:
*   
*****************************************************************************/
void fat_refresh_dir_file_info (Byte id)
{
  /* update fat_dir_current_sect with directory starting value */
  if (dir_is_root)
  {
    fat_dir_current_sect = fat_ptr_rdir;                          
  }
  else
  {
    fat_dir_current_sect = (((Uint32)(dclusters[0].cluster)) * fat_cluster_size)
                           + fat_ptr_data;
  }

  fat_get_dir_file_list(id);                /* Refresh file list */
  fat_seek_entry_record();                  /* Re-fetch the entry <-> fat_dir_list_index */
  fat_get_dir_entry(&fat_cache.current);    /* update current file info */  
}


/*F**************************************************************************
* NAME: fat_fdelete
*----------------------------------------------------------------------------
* PARAMS:
*    
* return:
*   - DEL_RET_OK:           delete done & dir is not empty
*   - DEL_RET_NO_MORE_FILE: dir is empty after delete or not
*   - DEL_RET_ERROR_DIR:    dir can not be deleted
*----------------------------------------------------------------------------
* PURPOSE:
*   Delete a selected file, in the root directory or in a sub-dir
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   fat_frefresh may be called after fdelete to rebuilt the dir content list
*----------------------------------------------------------------------------
* REQUIREMENTS:
*   File variables must be updated (fat_dclust_byte_count, entry record, ...)
*****************************************************************************/
Byte fat_fdelete (void)
{
Uint16 i;
Uint32 dir_sector; 

  if (((dir_is_root == TRUE) && (fat_dir_list_last == 0)) ||     
      ((dir_is_root == FALSE) && (fat_dir_list_last == 2)))      /* . and .. directory */
    return DEL_RET_NO_MORE_FILE;            /* directory already empty */

  if (fat_check_ext() != FILE_DIR)
  {
    fat_seek_entry_record();                /* Re-fetch the entry <-> fat_dir_list_index */
    Hard_read_close();

    dir_sector = fat_dir_current_sect;
    fat_root_entry = fat_dir_current_offs / 32; /* fat_dir_current_offs give the offset in byte starting */
    fat_file_size.l = fat_cache.current.size.l; /* at the beginning of directory */

    fat_load_sector(dir_sector);            /* Load directory sector  */
    i = (fat_root_entry % 16) * 32 ;        /* position of entry in the sector */
    while (fat_buf_sector[i + 11] == ATTR_LFN_ENTRY)
    {
      /* mark file as deleted */
      fat_buf_sector[i] = FILE_DELETED;
      i += 32;

      if (!dir_is_root)
        fat_dclust_byte_count += 32;

      if (i == SECTOR_SIZE)
      {
        Hard_write_open(dir_sector);
        for (i = 0; i < SECTOR_SIZE; i++)
          Hard_write_byte(fat_buf_sector[i]);
        Hard_write_close();

        if (!dir_is_root)                   /* sub-directory */
        {
          /* check if we are at the end of a cluster */
          if ((((Byte*)&fat_dclust_byte_count)[1] == 0x00) &&
            ((((Byte*)&fat_dclust_byte_count)[0] & fat_cluster_mask) == 0x00))
          {
            /* extract if necessary the next cluster from the allocation list */
            if (dclusters[fat_dchain_index].number == fat_dchain_nb_clust)
            { /* new fragment */
              fat_dchain_index++;
              fat_dchain_nb_clust = 1;
              dir_sector = (fat_ptr_data + ((Uint32)(dclusters[fat_dchain_index].cluster) * fat_cluster_size));
            }
            else
            {
              fat_dchain_nb_clust++;        /* one more cluster read */
              dir_sector++;                 /* Contiguous cluster    */
            }
          }
          else
          { /* Don't change the cluster */
            dir_sector++;
          }
        }
        else
        { /* Root directory is linear */
          dir_sector++;
        }
        fat_load_sector(dir_sector);
        i = 0;
      }
    }
    fat_buf_sector[i] = FILE_DELETED;
    Hard_write_open(dir_sector);
    for (i = 0; i < SECTOR_SIZE; i++)
      Hard_write_byte(fat_buf_sector[i]);
    Hard_write_close();

    /* FAT update */
    fat_fclust_byte_count = 0;              /* byte 0 of cluster */
    /* reset the allocation list variable */
    fat_fchain_index = 0;
    fat_fchain_nb_clust = 0;                /* start on first contiguous cl */
    /* get file allocation list */
    fat_get_clusters(&fclusters, MAX_FILE_FRAGMENT_NUMBER);
    /* Clear fat cluster */
    fat_clear_fat();
    /* NF correction */
    for (i = 0; i < 256; i++)
      gl_buffer[i] = 0x00;
    fat_dir_list_last--;                    /* one file deleted */

    if (((dir_is_root == TRUE) && (fat_dir_list_last == 0)) ||     
        ((dir_is_root == FALSE) && (fat_dir_list_last == 2)))      /* . and .. directory */
      return DEL_RET_NO_MORE_FILE;          /* directory now empty */

    if (fat_dir_list_index == fat_dir_list_last)
    {
      fat_dir_list_index--;                 /* in case of last file delete */
    }
    return DEL_RET_OK;
  }
  else
  {
    return DEL_RET_ERROR_DIR;
  }
}

/*F**************************************************************************
* NAME: fat_free_space
*----------------------------------------------------------------------------
* PARAMS:
*   
* return:
*   number of free cluster
*----------------------------------------------------------------------------
* PURPOSE:
*   Get free space   
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
fat_st_free_space fat_free_space(void)
{
Uint32 temp;
Uint32 i;
Uint16 cluster;
bdata bit fat12_parity;
xdata fat_st_free_space free_space;

  Hard_read_open(fat_ptr_fats);
  temp = 2;
  if (fat_is_fat16)     /* FAT16 management */
  {
    for (i = 0; i < fat_count_of_clusters; i++)
    {
      ((Byte*)&cluster)[1] = Hard_read_byte();
      ((Byte*)&cluster)[0] = Hard_read_byte();
      if (cluster == 0x0000)
        temp++;
    }
    Hard_read_close();
    free_space.free_cluster = temp;
    free_space.cluster_size = fat_cluster_size; 
    return free_space;
  }
  else
  {
    fat12_parity = 0;
    for (i = 0; i < fat_count_of_clusters; i++)
    {
      if (fat12_parity == 0)
      {
        ((Byte*)&cluster)[1] = Hard_read_byte();
        ((Byte*)&cluster)[0] = Hard_read_byte();
        fat12_parity = 1;
      }
      else
      {
        cluster = (cluster & 0xF000) >> 12;
        cluster += (Hard_read_byte() << 4);
        fat12_parity = 0;
      }
      if (!(cluster & 0x0FFF))
        temp++;
    }
    Hard_read_close();
    free_space.free_cluster = temp;
    free_space.cluster_size = fat_cluster_size; 
    return free_space;
  }
}


/*F**************************************************************************
* NAME: fat_read_cluster12
*----------------------------------------------------------------------------
* PARAMS:
*   init : initialize the parity bit or not
* return:
*   FAT12 cluster value
*----------------------------------------------------------------------------
* PURPOSE:
*   Read in fat12 file system a cluster value   
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
Uint16 fat_read_cluster (bit init)
{
static bit fat12_parity;
static idata Uint16 cluster;

  if (fat_is_fat16)
  {
    ((Byte*)&cluster)[1] = Hard_read_byte();
    ((Byte*)&cluster)[0] = Hard_read_byte();
    return cluster;
  }

  if (init)
  {
    fat12_parity = 0;
    cluster = 0;
  }

  if (fat12_parity == 0)
  {
    ((Byte*)&cluster)[1] = Hard_read_byte();
    ((Byte*)&cluster)[0] = Hard_read_byte();
    fat12_parity = 1;
    return (cluster & 0x0FFF);
  }
  else
  {
    cluster = (cluster & 0xF000) >> 12;
    cluster += (Hard_read_byte() << 4);
    fat12_parity = 0;
    return (cluster);
  }
}


/*F**************************************************************************
* NAME: fat_set_clusters
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*   - OK: allocation done
*   - KO: allocation cannot be done : no free cluster
*----------------------------------------------------------------------------
* PURPOSE:
*   Prepare a list of the free clusters:
*     chain[n].cluster contains the starting cluster number of a fragment
*     chain[n].number contains the number of contiguous clusters in fragment
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   Free cluster list is limited by the nb_frag parameter.
*   If memory is too much fragmented, created file may be limited in size.
*   Last list item always has single cluster
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit fat_set_clusters (void)
{
bit     cluster_free;
Uint16  cluster;

  cluster = 0;
  cluster_free = FALSE;
  fat_last_clust_index = 0;
  Hard_read_open(fat_ptr_fats);

  /* search the first free cluster in fat */
  fat_read_cluster(1);
  fat_read_cluster(0);
  do                                      /* search for first free cluster */
  {
    if (fat_read_cluster(0) == 0x0000)
      cluster_free = TRUE;
    else 
      cluster++;
  }
  while ((cluster != fat_count_of_clusters) && (!cluster_free));
   
  if (!cluster_free)
  {
    Hard_read_close();
    return KO;                                        /* no free cluster found */
  }

  fclusters[fat_last_clust_index].number = 1;  
  fclusters[fat_last_clust_index].cluster = cluster;                     /* store first cluster */
  cluster++;

  if (cluster != fat_count_of_clusters)
  {
    do                                                      /* construct the list */
    {
      cluster_free = FALSE;
      if (fat_read_cluster(0) == 0x0000)
        cluster_free = TRUE;
      else
        cluster++;

      if (cluster_free)                                     /* It's a contiguous cluster      */
      {                                                     /* add it to the list             */
        if (fclusters[fat_last_clust_index].number == MAX_CL_PER_FRAG) 
        {
          fat_last_clust_index++;
          if (fat_last_clust_index == MAX_FILE_FRAGMENT_NUMBER)
          {
            Hard_read_close();
            fat_last_clust_index--;
            return OK;
          }
          fclusters[fat_last_clust_index].number = 1;
          fclusters[fat_last_clust_index].cluster = cluster;
        }
        else
        {
          fclusters[fat_last_clust_index].number++;
        }
        cluster++;                                          /* process next cluster           */
      }
      else if (cluster != fat_count_of_clusters)
      {                                                     /* cluster is already used        */
        do                                                  /* search for next free fragment  */
        {
          if (fat_read_cluster(0) == 0x0000)
            cluster_free = TRUE;
          else
            cluster++;
        }
        while ((cluster != fat_count_of_clusters) && (!cluster_free));
    
        if (!cluster_free)                                  /* no more free cluster           */
        {
          Hard_read_close();
          return OK;                        /* end of partition reached */
        }
        
        fat_last_clust_index++;                                            /* new free fragment cluster      */
        if (fat_last_clust_index == MAX_FILE_FRAGMENT_NUMBER)
        {
          Hard_read_close();
          fat_last_clust_index--;
          return OK;
        }
        fclusters[fat_last_clust_index].number = 1;
        fclusters[fat_last_clust_index].cluster = cluster;
        cluster++;                                          /* process next cluster           */
      }
    }
    while ((fat_last_clust_index < MAX_FILE_FRAGMENT_NUMBER) && (cluster < fat_count_of_clusters));
  }

  Hard_read_close();
  return OK;
}


/*F**************************************************************************
* NAME: fat_save_cluster_info
*----------------------------------------------------------------------------
* PARAMS:
*   
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Save in locale variables cluster information for the current opened file
*   - cluster index
*   - number of the cluster 
*   - number of bytes in the cluster
*
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
void fat_save_cluster_info (void)
{
  fat_fchain_index_save = fat_fchain_index;
  fat_fchain_nb_clust_save = fat_fchain_nb_clust;
  fat_fclust_byte_count_save = fat_fclust_byte_count % (fat_cluster_size * SECTOR_SIZE);
}


/*F**************************************************************************
* NAME: fat_file_get_pos
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*   current file position in bytes
*----------------------------------------------------------------------------
* PURPOSE:
*   Calculate the current file position
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
Uint32 fat_file_get_pos (void)
{
Byte i;
Uint32 temp;

  if (fat_fchain_nb_clust == 0)
  {
    return 0x00000000;
  }

  temp = 0;
  for (i=0; i < fat_fchain_index; i++)
    temp += fclusters[i].number;
  temp += (fat_fchain_nb_clust - 1);
  temp = temp * fat_cluster_size * SECTOR_SIZE;
  temp = temp + (fat_fclust_byte_count % (fat_cluster_size * SECTOR_SIZE));

  return temp;
}


/*F**************************************************************************
* NAME: fat_feob
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*   - TRUE : B position have been reached
*   - FALSE : B position have not benn reached
*----------------------------------------------------------------------------
* PURPOSE:
*   Determine if B position have been reached in mode repeat A/B
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit fat_feob (void)
{
  if (fat_fchain_index >= fat_fchain_index_save)
  {
    if (fat_fchain_nb_clust >= fat_fchain_nb_clust_save)
      return (fat_fclust_byte_count >= fat_fclust_byte_count_save);
    else
      return FALSE;
  }
  else
  {
    return FALSE;
  }
}


/*F**************************************************************************
* NAME: fat_get_clusters
*----------------------------------------------------------------------------
* PARAMS:
*   chain:   allocation list address
*   nb_frag: maximum number of fragment 
*
* return:
*   - OK: allocation done
*   - KO: allocation done but truncated: file too much fragmented
*----------------------------------------------------------------------------
* PURPOSE:
*   Prepare a list of the file clusters:
*     chain[n].cluster contains the starting cluster number of a fragment
*     chain[n].number contains the number of contiguous clusters in fragment
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   File cluster list is limited by the nb_frag parameter.
*   If memory is too much fragmented, file may not be fully played.
*   Last list item always has single cluster
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit fat_get_clusters (fat_st_clust_chain xdata *chain, Byte nb_frag)
{
Byte   index;                               /* index in chain */
Uint16 i; 
Uint16 new_cluster;
Uint16 old_cluster;
Uint16 temp;
  nb_frag = nb_frag - 1;                    /* set limit (index start at 0) */

  /* build the first entry of the allocation list */
  chain[0].number = 1;
  chain[0].cluster = fat_cache.current.start_cluster - 2; /* 2 = 1st cluster */
  old_cluster = fat_cache.current.start_cluster;
  index = 0;

  
  /* calculate the first offset in fat and read the corresponding sector */  
  if (fat_is_fat16)
  {
    Hard_read_open(fat_ptr_fats + (old_cluster / (SECTOR_SIZE / 2)));
    /* compute offset in sector */
    for (i = (old_cluster % (SECTOR_SIZE / 2)); i != 0; i--)
    { 
      fat_read_cluster(0);
    }
    /* read first entry */
    new_cluster = fat_read_cluster(0);
    temp = LAST_CLUSTER16;
  }
  else
  {
    Hard_read_open(fat_ptr_fats);
    new_cluster = fat_read_cluster(1);
    for (i = old_cluster; i != 0; i--)
    {
      new_cluster = fat_read_cluster(0);
    }
    temp = LAST_CLUSTER12;
  }
    
  while (new_cluster != temp)   /* loop until last cluster found */
  {
    if ((new_cluster == (old_cluster + 1)) && (chain[index].number != MAX_CL_PER_FRAG))
    { /* contiguous cluster up to 255 or 65535 */
      chain[index].number++;
    }
    else
    { /* compute fragmentation */
      index++;
      chain[index].number = 1;
      chain[index].cluster = new_cluster - 2;  /* 2 = 1st cluster */
      for (i = new_cluster - old_cluster - 1; i != 0; i--)
      {
        fat_read_cluster(0);
      }
    }
    old_cluster = new_cluster;
    if (index == nb_frag)
    { /* end of chain reached */
      /* last fragment always contains a single cluster */
      chain[nb_frag].number = 1;          
      fat_last_clust_index = nb_frag;
      Hard_read_close();                  /* close physical read */
      return  KO;                         /* file too much fragmented */
    }
    else
    {
      /* read new entry */
      new_cluster = fat_read_cluster(0);
    }
  }

  /* end of file: last fragment must always contain a single cluster */
  if (chain[index].number == 1)
  { /* last cluster is the current one */
    fat_last_clust_index = index;
  }
  else
  {
    fat_last_clust_index = index + 1;
    chain[index].number--;
    chain[fat_last_clust_index].cluster = chain[index].cluster + chain[index].number;
    chain[fat_last_clust_index].number = 1; 
  }
  Hard_read_close();                        /* close physical read */
  return OK;
}


/*F**************************************************************************
* NAME: fat_fgetc
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*   byte read
*----------------------------------------------------------------------------
* PURPOSE:
*   Read one byte from file
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   As this function is called very often it must be short and optimized
*   in execution time
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
Byte fat_fgetc (void)
{
  /* check if we are at the end of a cluster */
  if ((((Byte*)&fat_fclust_byte_count)[1] == 0x00) &&
      ((((Byte*)&fat_fclust_byte_count)[0] & fat_cluster_mask) == 0x00))
  {
    /* extract if necessary the next cluster from the allocation list */
    if (fclusters[fat_fchain_index].number == fat_fchain_nb_clust)
    { /* new fragment */
      fat_fchain_index++;
      fat_fchain_nb_clust = 1;
      Hard_read_close();
      Hard_read_open(fat_ptr_data + ((Uint32)(fclusters[fat_fchain_index].cluster) * fat_cluster_size));
    }
    else
    { /* no new fragment */
      fat_fchain_nb_clust++;                /* one more cluster read */
    }
  }
  fat_fclust_byte_count++;                  /* one more byte read */
  return Hard_read_byte();
}


/*F**************************************************************************
* NAME: fat_fputc
*----------------------------------------------------------------------------
* PARAMS:
*   d: data byte to write
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Write one byte to file
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   As this function is called very often it must be short and optimized
*   in execution time
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
void fat_fputc (Byte d)
{
  /* check if we are at the end of a cluster */
  if ((((Byte*)&fat_fclust_byte_count)[1] == 0x00) &&
      ((((Byte*)&fat_fclust_byte_count)[0] & fat_cluster_mask) == 0x00))
  {
    /* extract if necessary the next cluster from the allocation list */
    if (fclusters[fat_fchain_index].number == fat_fchain_nb_clust)
    { /* new fragment */
      if (fat_fchain_index == fat_last_clust_index)
      {
        flag_end_disk_file = TRUE;
      }
      else
      {
        fat_fchain_index++;
        fat_fchain_nb_clust = 1;
        Hard_write_close();
        Hard_write_open(fat_ptr_data + ((Uint32)(fclusters[fat_fchain_index].cluster) * fat_cluster_size));
      }
    }
    else
    { /* no new fragment */
      fat_fchain_nb_clust++;                /* one more cluster read */
    }
  }
  if ( !flag_end_disk_file)
  {
    fat_fclust_byte_count++;                  /* one more byte read */
    fat_file_size.l++;
    Hard_write_byte(d);
  }
}


/*F**************************************************************************
* NAME: fat_feof
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Return the file end flag
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit fat_feof (void)
{
  if (fat_fchain_index >= fat_last_clust_index)
  {
    if (fat_open_mode == READ)
      return (fat_fclust_byte_count >= (Uint16)fat_cache.current.size.l);
    else
      return flag_end_disk_file;
  }
  else
  {
    return FALSE;
  }
}


/*F**************************************************************************
* NAME: fat_dseek
*----------------------------------------------------------------------------
* PARAMS:
*   offset: offset to current position in signed word value
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Seek from the current position to a new offset computing relative 
*   poisition +/- scan size limited to a 16 bit offset
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   We consider here that the seek size is minor to the cluster size !!!
*   if you want to do a more than a cluster seek, issue two successive 
*   dseek commands
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/ 
bit fat_dseek (Int16 offset)
{
Byte     nb_sect;                     /* number of sectors to seek */
Uint16   nb_byte;                     /* number of bytes to seek */
Uint16   target_cluster;              /* the cluster to reach */
Uint16   i;

  fat_dir_current_offs += offset;           /* Calculate the absolute byte pos */
  nb_sect = (Byte)((fat_dir_current_offs) / SECTOR_SIZE);
  fat_dir_current_sect = fat_dir_start_sect + nb_sect;
  nb_byte = (Uint16)((fat_dir_current_offs) % (SECTOR_SIZE * fat_cluster_size));
  fat_dclust_byte_count = nb_byte;
  nb_byte %= SECTOR_SIZE;                   
  
  if (dir_is_root == FALSE)                 /* Sub-directory ? */
  {                                         /* Find the # cluster */
    target_cluster = (nb_sect / fat_cluster_size);
    fat_dchain_index = 0;
    fat_dchain_nb_clust = 0;
    for (i = 0; i <= target_cluster; i++)
    {
      if (dclusters[fat_dchain_index].number == fat_dchain_nb_clust)
      {
        fat_dchain_index++;                 /* next fragment */
        fat_dchain_nb_clust = 1;            /* reset the nb cluster in this new fragment */
      }
      else
      {
        fat_dchain_nb_clust++;              /* next contiguous cluster */
      }
    }
    /* update fat_dir_current_sect value */
    fat_dir_current_sect = (((Uint32)(dclusters[fat_dchain_index].cluster + fat_dchain_nb_clust - 1) * fat_cluster_size)
                           + fat_ptr_data + (nb_sect % fat_cluster_size));

    if (!fat_dclust_byte_count)
    {
      if ((target_cluster == 0) || (dclusters[fat_dchain_index].number == 1))  /* If seek is first directory byte, reset */
      {                                                           /* fat_dchain_nb_clust (see fat_dgetc())  */
        fat_dchain_nb_clust = 0;
     
    } }

  }

  Hard_read_close();

  if (Hard_read_open(fat_dir_current_sect) == OK)
  { /* seek in current sector */
    for (i = 0;  i < nb_byte; i += 2)
    {
      Hard_read_byte();                     /* dummy reads */
      Hard_read_byte();
    }
    return OK;
  }
  else
  {
    return KO;                              /* low level error */
  }
}


/*F**************************************************************************
* NAME: fat_dgetc
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Return the directory data byte at the current position
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/ 
Byte fat_dgetc (void)
{
  /* check if we are at the end of a cluster */
  if ((((Byte*)&fat_dclust_byte_count)[1] == 0x00) &&
      ((((Byte*)&fat_dclust_byte_count)[0] & fat_cluster_mask) == 0x00))
  {
    /* extract if necessary the next cluster from the allocation list */
    if (dclusters[fat_dchain_index].number == fat_dchain_nb_clust)
    { /* new fragment */
      fat_dchain_index++;
      fat_dchain_nb_clust = 1;
      Hard_read_close();
      Hard_read_open(fat_ptr_data + ((Uint32)(dclusters[fat_dchain_index].cluster) * fat_cluster_size));
      fat_dir_current_sect = fat_ptr_data + ((Uint32)(dclusters[fat_dchain_index].cluster) * fat_cluster_size);
    }
    else
    { /* no new fragment */
      fat_dir_current_sect = fat_ptr_data + ((Uint32)(dclusters[fat_dchain_index].cluster + fat_dchain_nb_clust) * fat_cluster_size);
      fat_dchain_nb_clust++;                /* one more cluster read */
    }
  }
  fat_dclust_byte_count++;                  /* one more byte read */
  return Hard_read_byte();
}


/*F**************************************************************************
* NAME: fat_fseek
*----------------------------------------------------------------------------
* PARAMS:
*   offset: relative signed seek offset in file
*
* return:
*   seek status:  - OK: seek done
*                 - KO: out of file seek
*----------------------------------------------------------------------------
* PURPOSE:
*   Change file pointer of an openned file
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   In read mode, seek is done with byte boundary
*   In write mode, seek is done with sector boundary
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit fat_fseek (Int32 offset)
{
Uint32  file_pos;                           /* current position in file */
Uint16  cluster_offset;                     /* cluster offset in file */
Byte    sector_offset;                      /* sector offset in cluster */
#define i               sector_offset       /* local variable overlay */
#define byte_offset     cluster_offset      /* local variable overlay */

  /* File size information update for write mode*/ 
  if (fat_open_mode == WRITE)
  {
    if (fat_cache.current.size.l <= fat_file_size.l)
      fat_cache.current.size.l = fat_file_size.l;    
  }
  

  /* init file pos with the current cluster in the index */
  if (fat_fchain_nb_clust != 0)
    file_pos = fat_fchain_nb_clust - 1;
  else
    file_pos = 0;
  /* Add the previous cluster number */
  for (i = 0; i != fat_fchain_index; i++)
  {
    file_pos += fclusters[i].number;
  }
  /* convert absolute cluster value in byte position */
  file_pos *= SECTOR_SIZE * fat_cluster_size; 

  if ((fat_fchain_nb_clust != 0) && ( fat_fclust_byte_count % (fat_cluster_size * SECTOR_SIZE) == 0))
  {
    file_pos += (fat_cluster_size * SECTOR_SIZE);
  }
  else
  {
    ((Byte*)&fat_fclust_byte_count)[0] &= fat_cluster_mask;
    file_pos += fat_fclust_byte_count;        /* offset in cluster */
  }

  /* Check range value */
  if (((file_pos + offset) < 0) ||
      ((file_pos + offset) > fat_cache.current.size.l))
  {
    return KO;                              /* out of file limits */
  }

  file_pos += offset;    /* new position */

  /* Calculate byte position in cluster */
  ((Byte*)&fat_fclust_byte_count)[1] = ((Byte*)&file_pos)[3];
  ((Byte*)&fat_fclust_byte_count)[0] = ((Byte*)&file_pos)[2];

  /* Calculate the absolute cluster position */
  cluster_offset = file_pos / (SECTOR_SIZE * fat_cluster_size);
  fat_fchain_index = 0;                     /* reset fragment number */

  /* Determinate the index for the chain cluster */
  while (cluster_offset >= fclusters[fat_fchain_index].number)
  {
    cluster_offset -= fclusters[fat_fchain_index].number;
    fat_fchain_index++;                     /* ome more fragment */
  }

  /* Determinate the cluster offset value for the selected index */
  fat_fchain_nb_clust = cluster_offset;

  /* Determinate the sector offset value in the selected cluster */
  sector_offset = (fat_fclust_byte_count  & ((fat_cluster_size * SECTOR_SIZE) - 1)) / SECTOR_SIZE;

  /* seek into sector */
  byte_offset = file_pos % SECTOR_SIZE;

  /* re-open file in read or write mode */
  if (fat_open_mode == READ)
  {
    Hard_read_close();                      /* close  reading */
    Hard_read_open(((Uint32)(fclusters[fat_fchain_index].cluster + fat_fchain_nb_clust) * fat_cluster_size)
                   + sector_offset + fat_ptr_data);
  
    if ((fat_fchain_nb_clust != 0) || (sector_offset != 0) || (byte_offset != 0))
    { /* if no offset, nb_clust incremented in fgetc() function */
      fat_fchain_nb_clust++;                /* one-based variable */
    }
    while (byte_offset != 0)
    {
      Hard_read_byte();                     /* dummy read */
      byte_offset--;
    }
  }
  else
  {
    Hard_write_close();                     /* close writing */
    Hard_write_open(((Uint32)(fclusters[fat_fchain_index].cluster + fat_fchain_nb_clust) * fat_cluster_size)
                   + sector_offset + fat_ptr_data);
    byte_offset = file_pos % SECTOR_SIZE;
    if (byte_offset != 0)
    { /* if no offset, nb_clust incremented in fputc() function */
      fat_fchain_nb_clust++;                /* one-based variable */
    }
    fat_file_size.l += offset;                      /* Update byte position in write mode */

    if ((((Byte*)&fat_fclust_byte_count)[1] == 0x00) &&
      ((((Byte*)&fat_fclust_byte_count)[0] & fat_cluster_mask) == 0x00))
    {
      if ((fclusters[fat_fchain_index].number == fat_fchain_nb_clust) && (fat_fchain_index == fat_last_clust_index))
      {
        flag_end_disk_file = TRUE;
      }
      else
      {
        flag_end_disk_file = FALSE;
      }
    }
  }
  return OK;
}


/*F**************************************************************************
* NAME: fat_fseek_abs
*----------------------------------------------------------------------------
* PARAMS:
*   offset: absolute seek offset in file
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Move ahead file read pointer of an openned file
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
void fat_fseek_abs (Uint32 offset)
{
Byte    sector_offset;                      /* sector offset in cluster */
Uint16  cluster_offset;                     /* cluster offset in file */
#define i               sector_offset       /* local variable overlay */
#define byte_offset     cluster_offset      /* local variable overlay */

  /* Calculate byte position in cluster */
  ((Byte*)&fat_fclust_byte_count)[1] = ((Byte*)&offset)[3];
  ((Byte*)&fat_fclust_byte_count)[0] = (((Byte*)&offset)[2] & fat_cluster_mask);

  /* Calculate the absolute cluster position */
  cluster_offset = offset / (SECTOR_SIZE * fat_cluster_size);
  fat_fchain_index = 0;                     /* reset fragment number */

  /* Determinate the index for the chain cluster */
  while (cluster_offset > fclusters[fat_fchain_index].number)
  {
    cluster_offset -= fclusters[fat_fchain_index].number;
    fat_fchain_index++;                     /* ome more fragment */
  }
  /* Determinate the cluster offset value for the selected index */
  fat_fchain_nb_clust = cluster_offset;
  /* Determinate the sector offset value in the selected cluster */
  sector_offset = fat_fclust_byte_count / (SECTOR_SIZE);

  /* re-open file in read or write mode */
  if (fat_open_mode == READ)
  {
    Hard_read_close();                      /* close  reading */
    Hard_read_open(((Uint32)(fclusters[fat_fchain_index].cluster + fat_fchain_nb_clust) * fat_cluster_size)
                   + sector_offset + fat_ptr_data);
    /* seek into sector */
    byte_offset = offset % SECTOR_SIZE;
    if ((fat_fchain_nb_clust != 0) || (sector_offset != 0) || (byte_offset != 0))
    { /* if no offset, nb_clust incremented in fgetc() function */
      fat_fchain_nb_clust++;                /* one-based variable */
    }
    while (byte_offset != 0)
    {
      Hard_read_byte();                     /* dummy read */
      byte_offset--;
    }
  }
  else
  {
    Hard_write_close();                     /* close writing */
    Hard_write_open(((Uint32)(fclusters[fat_fchain_index].cluster + fat_fchain_nb_clust) * fat_cluster_size)
                   + sector_offset + fat_ptr_data);
    byte_offset = offset % SECTOR_SIZE;
    if ((byte_offset != 0) || (sector_offset != 0))
    { /* if no offset, nb_clust incremented in fputc() function */
      fat_fchain_nb_clust++;                /* one-based variable */
    }
  }
  #undef i
  #undef byte_offset
}


/*F**************************************************************************
* NAME: fat_check_ext
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Return the type of the file
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
Byte fat_check_ext (void)
{
  if ((fat_cache.current.attributes & ATTR_DIRECTORY) == ATTR_DIRECTORY)
  {
    return FILE_DIR;
  }
  else
  {
    if ((ext[0] == 'M') &&
        (ext[1] == 'P') &&
        (ext[2] == '3'))
    {
      return FILE_MP3;
    }
    else
    {
      if ((ext[0] == 'W') &&
          (ext[1] == 'A') &&
          (ext[2] == 'V'))
      {
        return FILE_WAV;
      }
      else
      {
        if ((ext[0] == 'S') &&
            (ext[1] == 'Y') &&
            (ext[2] == 'S'))
        {
          return FILE_SYS;
        }
        else
        {
          return FILE_XXX;
        }
      }
    }
  }
}


/*F**************************************************************************
* NAME: fat_get_name
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Return the address of the file name string
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
char pdata * fat_get_name (void)
{
  return (lfn_name);
}


/*F**************************************************************************
* NAME: fat_clear_file_name
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Initialise the file name string
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
void fat_clear_file_name (void)
{
Byte i;

  i = 0;
  do
  {
    gl_buffer[i++] = '\0';
  }
  while (i != 0);
}


/*F**************************************************************************
* NAME: fat_format
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Create single FAT12 or FAT16 partition and format the selected memory 
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   - Single partition
*   - Cluster size is 4 or 8 Kbytes
*   - Sector size is 512 bytes
*   - 2 fats management
*   - 512 entries in the root directory
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
void fat_format (void)
{
#define FORMAT_NB_CYLINDER            (*tab).nb_cylinder
#define FORMAT_NB_HEAD                (*tab).nb_head
#define FORMAT_NB_SECTOR              (*tab).nb_sector
#define FORMAT_NB_HIDDEN_SECTOR       (*tab).nb_hidden_sector
#define FORMAT_NB_SECTOR_PER_CLUSTER  (*tab).nb_sector_per_cluster

#ifndef MEM_RESERVED_SIZE
  #define MEM_RESERVED_SIZE 0
#endif

Byte j;
Byte nb_hidden_sector;
Byte nb_sector;
Byte nb_head;
#define k nb_head
Uint16 nb_sector_fat;
Uint16 i;
xdata Uint32 nb_total_sectors;
bit parity_bit;
xdata s_format *tab;
#define reserved_cluster_nb nb_sector 
#define nb_tot_cluster nb_total_sectors


  tab = Hard_format();

  nb_hidden_sector = FORMAT_NB_HIDDEN_SECTOR;
  nb_sector = FORMAT_NB_SECTOR;
  nb_head = FORMAT_NB_HEAD;

  fat_cluster_size = FORMAT_NB_SECTOR_PER_CLUSTER;
  nb_total_sectors = (Uint32)FORMAT_NB_CYLINDER * nb_head * nb_sector;

  /* FAT type caculation */
  fat_is_fat16 = (((nb_total_sectors - (Uint32)(nb_hidden_sector))  / fat_cluster_size) > MAX_CLUSTERS12);

#if FAT_PARTITIONNED == TRUE
  /* -- MASTER BOOT RECORD -- */
  Hard_write_open(MBR_ADDRESS);
  for (i = 0; ((Byte*)&i)[0] != 0x02; i++)              /* Boot Code */
  {
    fat_buf_sector[i] = 0x00;
  }
  /* First Partition entry */
  fat_buf_sector[446] = 0x80; /* Default Boot Partition */
  fat_buf_sector[447] = nb_hidden_sector / nb_sector;       /* start head */
  fat_buf_sector[448] = (nb_hidden_sector % nb_sector) + 1; /* Start Sector */
  fat_buf_sector[449] = 0x00;                    /* Start Cylinder */
  if (fat_is_fat16)
  {
    if (nb_total_sectors > 0xFFFF)          /* Total Sectors */
    {
      fat_buf_sector[450] = FAT16_SUP32M;   /* FAT16 > 32 Mbytes */
    }
    else
    {
      fat_buf_sector[450] = FAT16_INF32M;  /* FAT16 < 32 Mbytes */
    }
  }
  else
  {
    fat_buf_sector[450] = FAT12; /* FAT12 */
  }
  fat_buf_sector[451] = FORMAT_NB_HEAD - 1; /* Endhead-Zero-based(0) head number */
  fat_buf_sector[452] = (Byte)((((FORMAT_NB_CYLINDER - 1) / 0x04) & 0xC0) +  nb_sector);  
                                                            /* EndSector-Zero-based(1) sector number */
  fat_buf_sector[453] = (Byte)((FORMAT_NB_CYLINDER - 1) & 0xFF); /* EndCylinder */
  fat_buf_sector[454] = nb_hidden_sector; /* Start sector */
  fat_buf_sector[455] = 0x00;
  fat_buf_sector[456] = 0x00;
  fat_buf_sector[457] = 0x00;
  
  nb_total_sectors -= (Uint32)(nb_hidden_sector);
  fat_buf_sector[458] = ((Byte*)&nb_total_sectors)[3];
  fat_buf_sector[459] = ((Byte*)&nb_total_sectors)[2];
  fat_buf_sector[460] = ((Byte*)&nb_total_sectors)[1];
  fat_buf_sector[461] = ((Byte*)&nb_total_sectors)[0];
  fat_buf_sector[510] = 0x55;                    /* Signature Word */
  fat_buf_sector[511] = 0xAA;

  for (i = 0; ((Byte*)&i)[0] != 0x02; i++)
  {
    Hard_write_byte(fat_buf_sector[i]);
  }
  for (i = 0; ((Byte*)&i)[0] != 0x02; i++)
    fat_buf_sector[i] = 0x00;

  /* -- HIDDEN SECTORS -- */
  for (j = nb_hidden_sector - 1; j != 0; j--)
  {
    for (i = 0; ((Byte*)&i)[0] != 0x02; i++)
    {
      Hard_write_byte(fat_buf_sector[i]);
    }
  }
#else   /* fat not partitionned */
  nb_total_sectors -= (Uint32)(nb_hidden_sector);
  for (i = 0; ((Byte*)&i)[0] != 0x02; i++)
    fat_buf_sector[i] = 0x00;
  Hard_write_open(MBR_ADDRESS);
#endif  /* FAT_PARTITIONNED == TRUE */

  /* -- PARTITION BOOT RECORD -- */
  for (j = 0; j < 22; j++)
    fat_buf_sector[j] = PBR_record_part1[j];
  fat_buf_sector[13] = fat_cluster_size; /* Number of sector per cluster */
  if (nb_total_sectors <= 0xFFFF)        /* Total Sectors */
  {
    fat_buf_sector[19] = ((Byte*)&nb_total_sectors)[3];
    fat_buf_sector[20] = ((Byte*)&nb_total_sectors)[2];
  }

  /* Number of sector in each FAT */
  if (fat_is_fat16) 
    nb_sector_fat = (((nb_total_sectors - 1 - 32) * 2) + (512 * fat_cluster_size)) / 
                      ((512 * fat_cluster_size) + 4);
  else
    nb_sector_fat = (((nb_total_sectors - 1 - 32) * 3) + (1024 * fat_cluster_size)) / 
                      ((1024 * fat_cluster_size) + 6);

  fat_buf_sector[22] = ((Byte*)&nb_sector_fat)[1]; /* number of sector for each fat */
  fat_buf_sector[23] = ((Byte*)&nb_sector_fat)[0];
  fat_buf_sector[24] = nb_sector;         /* number of sectors on a track */
  fat_buf_sector[25] = 0x00;              /* number of sectors on a track */
  fat_buf_sector[26] = nb_head;           /* number of heads */
  fat_buf_sector[27] = 0x00;              /* number of heads */
  fat_buf_sector[28] = nb_hidden_sector;  /* number of hidden sectors */
  fat_buf_sector[29] = 0x00;              /* number of hidden sectors */
  if (nb_total_sectors > 0xFFFF)
  { /* number of sectors > 65535 */
    fat_buf_sector[32] =  ((Byte*)&nb_total_sectors)[3];
    fat_buf_sector[33] =  ((Byte*)&nb_total_sectors)[2];
    fat_buf_sector[34] =  ((Byte*)&nb_total_sectors)[1];
    fat_buf_sector[35] =  ((Byte*)&nb_total_sectors)[0];
  }
  for (j = 0, nb_head = 36; j < 26; j++,nb_head++)
  {
    fat_buf_sector[nb_head] = PBR_record_part2[j];
  }
  if (fat_is_fat16)
  {
    fat_buf_sector[58] = '6';
  }
  else
  {
    fat_buf_sector[58] = '2';
  }
  fat_buf_sector[510] = 0x55;
  fat_buf_sector[511] = 0xAA;

  for (i = 0; ((Byte*)&i)[0] != 0x02; i++)
  {
    Hard_write_byte(fat_buf_sector[i]);
  }
  
  /* -- FATS -- */

  nb_tot_cluster = ((nb_total_sectors - (1 + (2 * nb_sector_fat) + 32))
                            / fat_cluster_size) + 2 - (MEM_RESERVED_SIZE / FORMAT_NB_SECTOR_PER_CLUSTER);

  for (k = 0; k < 2; k++)
  {
    if (reserved_disk_space == FALSE)
    {
      for (i = 0; ((Byte*)&i)[0] != 0x02; i++)
      {
        fat_buf_sector[i] = 0x00;
      }
    }
    else
    {
      for (i = 0; ((Byte*)&i)[0] != 0x02; i++)
      {
        fat_buf_sector[i] = 0xFF;
      }
    }

    fat_buf_sector[0] = 0xF8;
    fat_buf_sector[1] = 0xFF;
    fat_buf_sector[2] = 0xFF;
    if (fat_is_fat16)
    { /* FAT16 */
      fat_buf_sector[3] = 0xFF;
    }
    
    if (fat_is_fat16)
    {
      reserved_cluster_nb = nb_sector_fat - (nb_tot_cluster / 256) - 1;
      nb_sector_fat = nb_tot_cluster / 256;
    }
    else
    {
      reserved_cluster_nb = nb_sector_fat - (nb_tot_cluster * 3 / 1024) - 1;
      nb_sector_fat = nb_tot_cluster * 3 / 1024;
    }

    
    for (; nb_sector_fat != 0; nb_sector_fat--)
    {
      for (i = 0; ((Byte*)&i)[0] != 0x02; i++)
      {
        Hard_write_byte(fat_buf_sector[i]);
      }
      if (reserved_disk_space == FALSE)
      {
        fat_buf_sector[0] = 0x00;
        fat_buf_sector[1] = 0x00;
        fat_buf_sector[2] = 0x00;
        fat_buf_sector[3] = 0x00;
      }
      else
      {
        fat_buf_sector[0] = 0xFF;
      }
    }

    if (fat_is_fat16)
    {
      i = (nb_tot_cluster % 256) * 2;
    }
    else
    {
      parity_bit = nb_tot_cluster & 0x01;
      i = (nb_tot_cluster * 3 / 2) & 0x1FF;
      if (parity_bit == 0)
      {
        fat_buf_sector[i] = 0x00;
      }
      else
      {
        fat_buf_sector[i] &=  0x0F;
      }
      i++;
    }
    for (; ((Byte*)&i)[0] != 0x02; i++)
      fat_buf_sector[i] = 0x00;
    for (i = 0; ((Byte*)&i)[0] != 0x02; i++)
    {
      Hard_write_byte(fat_buf_sector[i]);
    }
    for (i = 0; ((Byte*)&i)[0] != 0x02; i++)
      fat_buf_sector[i] = 0x00;
    for ( ; reserved_cluster_nb != 0; reserved_cluster_nb--)
    {
      for (i = 0; ((Byte*)&i)[0] != 0x02; i++)
      {
        Hard_write_byte(fat_buf_sector[i]);
      }
    }


  }



  /* -- ROOT DIRECTORY ENTRIES -- */
  for (j = NB_ROOT_ENTRY / 16; j != 0 ; j--)
  {
    for (i = 0; ((Byte*)&i)[0] != 0x02; i++)
    {
      Hard_write_byte(fat_buf_sector[i]);
    }
  }
  Hard_write_close();
  #undef k
  #undef reserved_cluster_nb
  #undef nb_tot_cluster

}



