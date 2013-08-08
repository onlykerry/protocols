/*C**************************************************************************
* NAME:         fat32.c
*----------------------------------------------------------------------------
* Copyright (c) 2003 Atmel.
*----------------------------------------------------------------------------
* RELEASE:      snd1c-refd-nf-4_0_3      
* REVISION:     1.4     
*----------------------------------------------------------------------------
* PURPOSE:
* FAT32 file-system basics functions
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
#include "fat32.h"                          /* fat32 file-system definition */


/*_____ M A C R O S ________________________________________________________*/


/*_____ D E F I N I T I O N ________________________________________________*/

extern  pdata Byte    gl_buffer[];

extern char    pdata *lfn_name;                 /* long filename limited to MAX_FILENAME_LEN chars */

extern xdata Byte fat_buf_sector[];         /* 512 bytes buffer */

/* disk management */
extern  data  Uint32  fat_ptr_fats;         /* address of the first byte of FAT */
extern  data  Uint32  fat_ptr_data;         /* address of the first byte of data */
extern  data  Byte    fat_cluster_size;     /* cluster size (sector count) */
extern  idata Byte    fat_cluster_mask;     /* mask for end of cluster test */

extern  bdata bit     dir_is_root;          /* TRUE: point the root directory  */
extern  bdata bit     fat_is_fat16;         /* TRUE: FAT16 - FALSE: FAT12 */
extern  bdata bit     fat_is_fat32;         /* TRUE: FAT32 - FALSE: FAT12/FAT16 */
extern  bdata bit     fat_open_mode;        /* READ or WRITE */
extern  bdata bit     fat_2_is_present;     /* TRUE: 2 FATs - FALSE: 1 FAT */
extern  bdata bit     flag_end_disk_file;

extern  xdata Uint32  fat_count_of_clusters;/* number of cluster - 2 */
extern  xdata Union32 fat_file_size;
extern  xdata Uint32  fat_fat_size;         /* FAT size in sector count */


/* directory management */
extern  xdata fat_st_clust_chain dclusters[MAX_DIR_FRAGMENT_NUMBER];
                                            /* cluster chain for the current directory */
extern  idata Uint16  fat_dclust_byte_count;/* byte counter in directory sector */

extern  idata Uint16  fat_dchain_index;     /* the number of the fragment of the dir, in fact
                                               the index of the table in the cluster chain */
extern  idata Byte    fat_dchain_nb_clust;  /* the offset of the cluster from the first cluster
                                               of the dir fragment */
extern  xdata Byte    fat_last_dclust_index;/* index of the last cluster in directory chain */

extern  xdata Uint16  fat_dir_list_index;   /* index of current entry in dir list */
extern  xdata Uint16  fat_dir_list_last;    /* index of last entry in dir list */
extern  xdata Uint32  fat_dir_start_sect;   /* start sector of dir list */
extern  idata Uint32  fat_dir_current_sect; /* sector of selected entry in dir list */
extern  xdata Uint32  fat_dir_current_offs; /* entry offset from fat_dir_current_sect */

extern  xdata fat_st_cache   fat_cache;     /* The cache structure, see the .h for more info */
extern  xdata char  ext[3];                 /* file extension (limited to 3 characters) */


/* file management */
extern  xdata fat_st_clust_chain fclusters[MAX_FILE_FRAGMENT_NUMBER];
                                            /* cluster chain for the current file */
extern  idata  Byte    fat_last_clust_index; /* index of the last cluster in file chain */

extern  data  Uint16   fat_fclust_byte_count;/* byte counter in file cluster */

extern  idata  Byte    fat_fchain_index;     /* the number of the fragment of the file, in fact
                                               the index of the table in the cluster chain */
              
extern  idata  Uint16  fat_fchain_nb_clust;  /* the offset of the cluster from the first cluster
                                               of the file fragment */

extern xdata  Uint32  fat_current_file_size;
extern xdata  Uint32  fat_rootclus_fat32;                  /* root cluster address */
extern bdata bit fat_last_dir_cluster_full;
extern bdata bit fat_no_entries_free;
extern xdata Uint16 fat_total_clusters;
extern xdata Uint32 last_free_cluster;

/* Mode repeat A/B variables */
extern xdata  Byte    fat_fchain_index_save;         
extern xdata  Byte    fat_fchain_nb_clust_save;
extern xdata  Uint16  fat_fclust_byte_count_save;

extern xdata Byte current_ext;

extern idata Uint16 fat_current_end_entry_position;
extern idata Uint16 fat_current_start_entry_position;
extern xdata Uint16 fat_nb_deleted_entries;
extern xdata Uint16 fat_nb_total_entries;


/*_____ D E C L A R A T I O N ______________________________________________*/

static  void    fat_get_dir_entry (fat_st_dir_entry xdata *);
static  void    fat_get_dir_file_list (Byte);
static  bit     fat_get_clusters (fat_st_clust_chain xdata *, Byte);
static  bit     fat_get_free_clusters (bit);
static  bit     fat_dseek (Int16);
static  Byte    fat_dgetc (void);


/*F**************************************************************************
* NAME: fat_install
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*   - OK: intallation succeeded
*   - KO: - partition 1 not active
*         - FAT type is not FAT16
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
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit fat_install (void)
{          
Byte i;
Uint32 tot_sect;
Uint32 fat_nb_sector;
Uint16 bpb_rsvd_sec_cnt;
Byte bpb_num_fat;
Uint16 bpb_root_ent_count;

  /* read and check usefull MBR info */
  /* go to the first partition field */
  Hard_read_open(MBR_ADDRESS);
  Hard_load_sector();
  Hard_read_close();

  fat_ptr_fats = 0x01;
  if ((fat_buf_sector[0] == 0xEB) && (fat_buf_sector[2] == 0x90)) /* Jump instruction to boot code */
  {
    if ((fat_buf_sector[21] & 0xF0) == 0xF0)  /* Media byte */
    {
      if ((fat_buf_sector[510] == 0x55) && (fat_buf_sector[511] == 0xAA)) /* signature */
      {
        fat_ptr_fats = 0x00000000;            /* disk may not be partitionned : first sector */
      }                                       /* is PBR */
      else
      {
        return KO;  /* no signature -> low level error */
      }
    }
  }

  if (fat_ptr_fats)                     /* if first sector is not a PBR */
  {
    if (Hard_read_open(MBR_ADDRESS) == OK) 
    {
      for (i = 446/2; i != 0; i--)
      {
        Hard_read_byte();                     /* go to first partition entry */
        Hard_read_byte();
      }
      Hard_read_byte();
      Hard_read_byte();                       /* dummy reads */
      Hard_read_byte();
      Hard_read_byte();
      Hard_read_byte();
      Hard_read_byte();
      Hard_read_byte();
      Hard_read_byte();
      /* read partition offset (in sectors) at offset 8 */
      ((Byte*)&fat_ptr_fats)[3] = Hard_read_byte();
      ((Byte*)&fat_ptr_fats)[2] = Hard_read_byte();
      ((Byte*)&fat_ptr_fats)[1] = Hard_read_byte();
      ((Byte*)&fat_ptr_fats)[0] = Hard_read_byte();
      /* go to the MBR signature field */
      for (i = 52; i != 0; i--)
      {
        Hard_read_byte();                   /* dummy reads */
      }
      /* check MBR signature */
      if ((Hard_read_byte() != LOW(BR_SIGNATURE)) &&
          (Hard_read_byte() != HIGH(BR_SIGNATURE)))
      {
        fat_ptr_fats = 0x00000000;          /* disk may not be partitionned */
      }
      Hard_read_close();                    /* close physical read */
    }
    else
    { /* low level error */
      return KO;
    }
  }


  /* read and check usefull PBR info */
  if (Hard_read_open(fat_ptr_fats) == OK) 
  {
    /* go to sector size field */
    for (i = 11; i != 0; i--)
    {
      Hard_read_byte();                     /* dummy reads */
    }
    /* read sector size (in bytes) */
    if ((Hard_read_byte() != LOW(SECTOR_SIZE)) ||
        (Hard_read_byte() != HIGH(SECTOR_SIZE)))
    {
      Hard_read_close();                    /* close physical read */
      return KO;
    }
    /* read cluster size (in sector) */
    fat_cluster_size = Hard_read_byte();
    fat_cluster_mask = HIGH((Uint16)fat_cluster_size * SECTOR_SIZE) - 1;
    /* reserved sector number */
    ((Byte*)&bpb_rsvd_sec_cnt)[1] = Hard_read_byte();
    ((Byte*)&bpb_rsvd_sec_cnt)[0] = Hard_read_byte();
    /* number of FATs */
    bpb_num_fat = Hard_read_byte();
    if (bpb_num_fat == 2)
      fat_2_is_present = TRUE;
    else
      fat_2_is_present = FALSE;    
    
    /* read number of dir entries*/
    ((Byte*)&bpb_root_ent_count)[3] = Hard_read_byte();
    ((Byte*)&bpb_root_ent_count)[2] = Hard_read_byte();

    /* read number of sector in partition (<32Mb) */
    ((Byte*)&fat_nb_sector)[3] = Hard_read_byte();
    ((Byte*)&fat_nb_sector)[2] = Hard_read_byte();
    ((Byte*)&fat_nb_sector)[1] = 0x00;
    ((Byte*)&fat_nb_sector)[0] = 0x00;
    Hard_read_byte();
    Hard_read_byte();                               /* FAT size for FAT12/16 */
    Hard_read_byte();
    
    Hard_read_byte();
    Hard_read_byte();
    Hard_read_byte();
    Hard_read_byte();
    Hard_read_byte();
    Hard_read_byte();
    Hard_read_byte();
    Hard_read_byte();

    /* read number of sector in partition (>32Mb) */
    ((Byte*)&fat_nb_sector)[3] += Hard_read_byte();
    ((Byte*)&fat_nb_sector)[2] += Hard_read_byte();
    ((Byte*)&fat_nb_sector)[1] += Hard_read_byte();
    ((Byte*)&fat_nb_sector)[0] += Hard_read_byte();

    fat_is_fat16 = FALSE;
    fat_is_fat32 = FALSE;

    fat_ptr_data = (bpb_root_ent_count * DIR_SIZE) / SECTOR_SIZE;

    /* Here start the structure for FAT32 */
    /* Offset 36 : 32 bits size of fat */
    ((Byte*)&fat_fat_size)[3] = Hard_read_byte();
    ((Byte*)&fat_fat_size)[2] = Hard_read_byte();
    ((Byte*)&fat_fat_size)[1] = Hard_read_byte();
    ((Byte*)&fat_fat_size)[0] = Hard_read_byte();

    tot_sect = fat_nb_sector - (Uint32)(((Uint32)(bpb_rsvd_sec_cnt) + (Uint32)((bpb_num_fat * (Uint32)(fat_fat_size))) + fat_ptr_data));
    fat_count_of_clusters = tot_sect / fat_cluster_size;
       
    /* Offset 40 : ExtFlags */
    Hard_read_byte();
    Hard_read_byte();
    /* Offset 42 : FS Version */
    Hard_read_byte();
    Hard_read_byte();
    /* Offset 44 : Root Cluster */
    ((Byte*)&fat_rootclus_fat32)[3] = Hard_read_byte();
    ((Byte*)&fat_rootclus_fat32)[2] = Hard_read_byte();
    ((Byte*)&fat_rootclus_fat32)[1] = Hard_read_byte();
    ((Byte*)&fat_rootclus_fat32)[0] = Hard_read_byte();

    fat_ptr_fats += bpb_rsvd_sec_cnt;
    fat_ptr_data = fat_ptr_fats + (bpb_num_fat * fat_fat_size);

    /* Offset 48 : FS Info */
    /* Offset 50 : Backup Boot Sector */
    /* Offset 52 : Reserved */
    /* Offset 64 - 89 : Data */
    /* Offset 90 : 510 : Free */
    for (i = 231; i != 0; i--)
    {
      Hard_read_byte();
      Hard_read_byte();
    }
    /* check partition signature */
    if ((Hard_read_byte() != LOW(BR_SIGNATURE)) &&
        (Hard_read_byte() != HIGH(BR_SIGNATURE)))
    {
      Hard_read_close();                    /* close physical read */
      return KO;
    }
    Hard_read_close();                      /* close physical read */
    return OK;
  }
  else
  { /* low level error */
    return KO;
  }
}

#define UPLOAD      0
#define DOWNLOAD    1
#define FETCH_NEXT  0
#define FETCH_PREV  1

/*F**************************************************************************
* NAME: fat_calc_cluster
*----------------------------------------------------------------------------
* PARAMS:
*   
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Calculate fat_dir_current_sect and update directory variable from the
*   value of fat_dir_current_offs.
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   
*----------------------------------------------------------------------------
* REQUIREMENTS:
*   
*****************************************************************************/
void fat_calc_cluster(void)
{
Uint32   i;
  fat_dchain_index = 0;
  fat_dchain_nb_clust = 0;
  for (i = (fat_dir_current_offs / SECTOR_SIZE / fat_cluster_size) + 1; i != 0; i--)
  {
    if (dclusters[fat_dchain_index].number == fat_dchain_nb_clust)
    { /* new fragment */
      fat_dchain_index++;
      fat_dchain_nb_clust = 1;
    }
    else
    { /* no new fragment */
      fat_dchain_nb_clust++;
    }
  }
  i = fat_dir_current_offs / SECTOR_SIZE;
  fat_dir_current_sect = (((Uint32)(dclusters[fat_dchain_index].cluster + fat_dchain_nb_clust - 1) * 
                                    fat_cluster_size)
                         + fat_ptr_data + (i % fat_cluster_size));

  if ((fat_dclust_byte_count == 0)/* && (fat_dchain_index == 0)*/)     /* If we are at the beginning of a directory */
    if (fat_dchain_nb_clust == 1)
      fat_dchain_nb_clust = 0;
}


/*F**************************************************************************
* NAME: fat_clear_dir_info
*----------------------------------------------------------------------------
* PARAMS:
*   
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Reset directory chain cluster value
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   
*----------------------------------------------------------------------------
* REQUIREMENTS:
*   
*****************************************************************************/
void fat_clear_dir_info(void)
{
  fat_dchain_nb_clust = 0;    /* position of a cluster for selected chain idx */
  fat_dchain_index = 0;       /* chain index position */  
  fat_dclust_byte_count = 0;  /* byte position inside a directory cluster */
  fat_dir_current_offs = 0;   /* general offset from the start of a directory */
}

/*F**************************************************************************
* NAME: fat_up_down_load_sector
*----------------------------------------------------------------------------
* PARAMS:
*   - sector address to load/download   
*   - bit to indicate if it's a download or an upload
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Download or upload a sector of 512b
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   
*----------------------------------------------------------------------------
* REQUIREMENTS:
*   
*****************************************************************************/
void fat_up_down_load_sector(Uint32 sector, bit up_down)
{
  if (up_down == UPLOAD)
  {
    Hard_read_open(sector);     
    Hard_load_sector();
    Hard_read_close();
  }
  else
  {
    Hard_write_open(sector);
    Hard_download_sector();
    Hard_write_close();
  }
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
*   Give information about the directory :
*     - total number of entries
*     - number of deleted entries
*     - number of filtered entries (filter is done by checking id value)
*     - total number of clusters used by the directory
*----------------------------------------------------------------------------
* EXAMPLE:
*
*----------------------------------------------------------------------------
* NOTE:
*   
*----------------------------------------------------------------------------
* REQUIREMENTS:
*   
*****************************************************************************/
void fat_get_dir_file_list (Byte id)
{
Byte   i;
bit   exit = FALSE;

  current_ext = id;                         /* save current extension */
  fat_last_dir_cluster_full = FALSE;        /* reset flag for full cluster */
  fat_no_entries_free = FALSE;              /* reset flag for presence of free entries inside a directory cluster */
  fat_dir_list_last = 0;                    /* last filtered directory entry */
  fat_dir_start_sect = fat_dir_current_sect;/* set fat_dir_start_sect (use by fat_dseek()) */
  fat_nb_deleted_entries = 0;               /* reset the number of entries that is marker as deleted */
  fat_nb_total_entries = 0;                 /* reset the number of total entries in the directory */
  fat_total_clusters = 0;                   /* reset the total number of clusters for a directory */

  for (i = 0; i <= fat_last_dclust_index; i++)
  {
    fat_total_clusters += dclusters[i].number;  /* calculate the total numbers of clusters */
  }

  fat_clear_dir_info();                         /* clear directory variable */
  
  Hard_read_open(fat_dir_start_sect);
  do
  {
    for (i = 0; i < DIR_SIZE; i++)
      gl_buffer[i] = fat_dgetc();

    if (gl_buffer[0] != FILE_NOT_EXIST)
    {
      if (gl_buffer[0] != FILE_DELETED)
      {
        fat_nb_total_entries++;                             /* increase total number of entries   */
        while (gl_buffer[11] == ATTR_LFN_ENTRY)
        {
          for (i = 0; i < DIR_SIZE; i++)
            gl_buffer[i] = fat_dgetc();
          fat_nb_total_entries++;                           /* increase total number of entries   */
        }
        
        fat_cache.current.attributes = gl_buffer[11];       /* filter on the file type */
        ext[0] = gl_buffer[8];
        ext[1] = gl_buffer[9];
        ext[2] = gl_buffer[10];
        if ((fat_check_ext() & current_ext) != FILE_XXX)    /* Check if file type is OK */
        {
          fat_dir_list_last++;          /* increase the number of selected entries */
        }
      }
      else
      {
        fat_nb_deleted_entries++;       /* increase number of deleted entries */
        fat_nb_total_entries++;         /* increase total number of entries   */
      }
    }
    else
    {
      exit = TRUE;                      /* "not exist" entries mark the end of directory */
    }

    if  ((((Byte*)&fat_dclust_byte_count)[1] == 0x00) &&
        ((((Byte*)&fat_dclust_byte_count)[0] & fat_cluster_mask) == 0x00) && 
        (fat_last_dclust_index == fat_dchain_index))
    {
      exit = TRUE;                          /* if the last entries of the last cluster is reached */
      fat_last_dir_cluster_full = TRUE;     /* then exit and set this flag                        */
    }
  }
  while (!exit);

  Hard_read_close();                        /* close disk */
  if (fat_last_dir_cluster_full)            /* if the last directory cluster is full        */
  {
    if (!fat_nb_deleted_entries)            /* and if there is no deleted entries detected  */
    {
      fat_no_entries_free = TRUE;           /* set this flag that there is no free entries  */
    }
  }

  fat_dir_current_sect = fat_dir_start_sect;
  fat_clear_dir_info();                     /* reset directory variable */

}


/*F**************************************************************************
* NAME: fat_fetch_file_info
*----------------------------------------------------------------------------
* PARAMS:
*   entry: directory entry structure
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Get information about a directory or file entry
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
void fat_fetch_file_info (fat_st_dir_entry xdata *entry, bit direction)
{
Uint16 i;
Uint16 j;
bit entry_found = FALSE;

  /* clear the name buffer */
  for (i = MAX_FILENAME_LEN; i != 0; lfn_name[--i] = '\0');

  if (direction == FETCH_PREV)
  {
    /* fetch previous file */
    fat_current_start_entry_position--;                             /* seek to the previous entry  */
    fat_dir_current_offs = (fat_current_start_entry_position) * DIR_SIZE; /* update fat_dir_current_offs */
    fat_calc_cluster();                                             /* update directory position variable */
    j = (fat_current_start_entry_position % 16) * DIR_SIZE;         /* seek to the previous entry */
    fat_up_down_load_sector(fat_dir_current_sect, UPLOAD);          /* load the directory sector */
    do
    {
      if (fat_buf_sector[j] != FILE_DELETED)                    /* if it's not a deleted entries */
      {
        entry->attributes = fat_buf_sector[j + 11];
        ext[0]= fat_buf_sector[j + 8];
        ext[1]= fat_buf_sector[j + 9];
        ext[2]= fat_buf_sector[j + 10];
        if ((fat_check_ext() & current_ext) != FILE_XXX)            /* check id of entry */ 
        {
          entry_found = TRUE;
        }
      }

      if (!entry_found )                   /* if id is not correct   */
      {
        fat_dir_current_offs -= DIR_SIZE;                           /* seek to previous entry */
        if (j == 0)                                                 /* sector change          */
        {
          fat_calc_cluster();
          fat_up_down_load_sector(fat_dir_current_sect, UPLOAD);    /* load the directory sector */
          j = SECTOR_SIZE - DIR_SIZE;
        }
        else
        {
          j -= DIR_SIZE;
        }
        fat_current_start_entry_position--;
      }
    }
    while (!entry_found);

    if (fat_current_start_entry_position != 0)
    {
      /* we find an entry. Now seek to previous entry for the process LFN */
      fat_dir_current_offs -= DIR_SIZE;                           /* seek to previous entry */
      if (j == 0)                                                 /* sector change          */
      {
        fat_calc_cluster();
        fat_up_down_load_sector(fat_dir_current_sect, UPLOAD);    /* load the directory sector */
        j = SECTOR_SIZE - DIR_SIZE;
      }
      else
      {
        j -= DIR_SIZE;
      }
      fat_current_start_entry_position--;
      if (fat_buf_sector[j + 11] != ATTR_LFN_ENTRY)
      {
        fat_dir_current_offs += DIR_SIZE;
        fat_current_start_entry_position++;
      }
      else
      {
        while ((fat_buf_sector[j + 11] == ATTR_LFN_ENTRY) && (fat_current_start_entry_position != 0))
        {
          fat_dir_current_offs -= DIR_SIZE;
          fat_current_start_entry_position--;
          if (j == 0)                                                 /* sector change          */
          {
            fat_calc_cluster();
            fat_up_down_load_sector(fat_dir_current_sect, UPLOAD);    /* load the directory sector */
            j = SECTOR_SIZE - DIR_SIZE;
          }
          else
          {
            j -= DIR_SIZE;
          }
        }
    
        if (fat_current_start_entry_position)
        {
          fat_dir_current_offs += DIR_SIZE;
          fat_current_start_entry_position++;
        }
      }

    }

    fat_current_end_entry_position = fat_current_start_entry_position;
    fat_dseek(0);
  }
  else      /* fetch next file */
  {
    fat_dseek((fat_current_end_entry_position - fat_current_start_entry_position) << 5);
  }

  do
  {
    fat_current_start_entry_position = fat_current_end_entry_position;
    for (i = 0; i < DIR_SIZE; i++)
      gl_buffer[i] = fat_dgetc();

    while (gl_buffer[0] == FILE_DELETED)                          /* find a non deleted file */
    {
      fat_current_start_entry_position++;
      fat_dir_current_offs += DIR_SIZE;
      for (i = 0; i < DIR_SIZE; i++)
        gl_buffer[i] = fat_dgetc();
    }

    fat_current_end_entry_position = fat_current_start_entry_position;

    if (gl_buffer[11] == ATTR_LFN_ENTRY)
    {
      for (j = gl_buffer[0] & 0x0F; j != 0; j--)                  /* computes lfn name */
      {
        for (i = 0; i < 5; i++)
          lfn_name[i + 13 * (j - 1)] = gl_buffer[2 * i + 1];
        for (i = 0; i < 6; i++)
          lfn_name[i + 5 + 13 * (j - 1)] = gl_buffer[2 * i + 14];
        for (i = 0; i < 2; i++)
          lfn_name[i + 11 + 13 * (j - 1)] = gl_buffer[2 * i + 28];
  
        for (i = 0; i < DIR_SIZE; i++)                            /* read the directory entry */
          gl_buffer[i] = fat_dgetc();
        fat_current_end_entry_position++;
      }
      i = 0;
      while (lfn_name[i] != '\0')                                 /* search for the end of the string */
      { 
        i++;
      }
      if (i <= 14)                                                /* append spaces for display reason (no scrolling) */
      { 
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
    else
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
      if ((gl_buffer[8] == ' ') &&
          (gl_buffer[9] == ' ') &&
          (gl_buffer[10] == ' '))
        lfn_name[i++] = ' ';
      else
        lfn_name[i++] = '.';                                  /* append extension */
      lfn_name[i++] = gl_buffer[8];
      lfn_name[i++] = gl_buffer[9];
      lfn_name[i++] = gl_buffer[10];
  
      for (; i != 14; i++)
      {
        lfn_name[i] = ' ';                                  /* append spaces for display reason */
      }
      lfn_name[i] = '\0';                                   /* end of string */
    } 
    fat_cache.current.attributes = gl_buffer[11];       /* filter on the file type */
    
    ext[0]= gl_buffer[8];                                   /* store extension */
    ext[1]= gl_buffer[9];
    ext[2]= gl_buffer[10];

    fat_current_end_entry_position++;
    if ((fat_check_ext() & current_ext) == FILE_XXX)
    {
      fat_dir_current_offs += ((fat_current_end_entry_position - fat_current_start_entry_position) << 5);  
    }
  }
  while ((fat_check_ext() & current_ext) == FILE_XXX);


  entry->start_cluster  = gl_buffer[26];                    /* starting cluster value */
  entry->start_cluster += ((Uint32) gl_buffer[27]) << 8;
  entry->start_cluster += ((Uint32) gl_buffer[20]) << 16;
  entry->start_cluster += ((Uint32) gl_buffer[21]) << 24;
  entry->size.b[3]      = gl_buffer[28];                    /* file size value        */
  entry->size.b[2]      = gl_buffer[29];
  entry->size.b[1]      = gl_buffer[30];
  entry->size.b[0]      = gl_buffer[31];
  Hard_read_close();                                        /* close physical read */

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
  dir_is_root = TRUE;                                       /* set directory root flag */
  fat_cache.current.start_cluster = fat_rootclus_fat32 ;    /* #cluster root directory */
  fat_get_clusters(&dclusters, MAX_DIR_FRAGMENT_NUMBER);    /* Construct root directory cluster chain */
  fat_last_dclust_index = fat_last_clust_index;             /* save last index position for chain cluster */

                      
  /* computes sector address from allocation table */
  fat_dir_current_sect = (((Uint32)(dclusters[0].cluster)) * fat_cluster_size)
                       + fat_ptr_data;

  fat_get_dir_file_list(id);                                /* create list of entries */
  if (fat_dir_list_last == 0)
    return KO;                                              /* no requested (id) entry */

  fat_dir_list_index = 1;                                   /* point on first root entry */
  fat_current_start_entry_position = 0;
  fat_current_end_entry_position = 0;
  fat_fetch_file_info(&fat_cache.current, FETCH_NEXT);
  fat_cache.parent.start_cluster = fat_rootclus_fat32;    /* parent dir is also root */   
  fat_cache.parent.attributes = ATTR_DIRECTORY;           /* mark as directory */
  return OK;
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
  if (fat_dir_list_index < fat_dir_list_last)
  {
    fat_dir_list_index++;
    fat_fetch_file_info(&fat_cache.current, FETCH_NEXT);
    return OK;
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
    min = 1;
  else
    min = 3;

  if (fat_dir_list_index != min)            /* first file of the directory? */
  {
    fat_dir_list_index--;
    fat_fetch_file_info(&fat_cache.current, FETCH_PREV);
    return OK;
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
bit result;
  do
  {
    result = fat_goto_next();
  }
  while (result == OK);
  return result;
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
bit result;
Uint16 temp;

  temp = fat_dir_list_index - 1;
  fat_seek_first();
  for (; temp != 0; temp--)
    result = fat_goto_next();
  return result;
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
  fat_clear_dir_info(); 
  fat_dir_current_sect = fat_dir_start_sect;
  fat_current_start_entry_position = 0;
  fat_current_end_entry_position = 0;

  if (dir_is_root)
  { /* root diretory */
    fat_dir_list_index = 1;                         
    fat_fetch_file_info(&fat_cache.current, FETCH_NEXT);  /* fetch first root entry */
    return OK;
  }
  else
  { /* not root dir */
    fat_fetch_file_info(&fat_cache.parent, FETCH_NEXT);   /* dot entry          */
    fat_fetch_file_info(&fat_cache.parent, FETCH_NEXT);   /* dotdot entry       */
    fat_dir_list_index = 2;                               /* update entry index */
    return fat_goto_next();                               /* update first file info */
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
    if (fat_cache.current.start_cluster == fat_rootclus_fat32)
    {
      return fat_get_root_directory(id);                    /* go to root dir */
    }

    /* go to not root dir */ 
    fat_get_clusters(&dclusters, MAX_DIR_FRAGMENT_NUMBER);  /* get directory allocation table */                                         
    fat_last_dclust_index = fat_last_clust_index;           /* save last index position for chain cluster */
    fat_dir_current_sect = (((Uint32)(dclusters[0].cluster)) * fat_cluster_size)    /* computes sector address from allocation table */
                           + fat_ptr_data;

    fat_get_dir_file_list(id);                              /* create list of entries */
    fat_dir_list_index = 1;                                 /* point on first root entry */
    fat_current_start_entry_position = 0;
    fat_current_end_entry_position = 0;    
    
    fat_fetch_file_info(&fat_cache.current, FETCH_NEXT);    /* dot entry    */   
    fat_fetch_file_info(&fat_cache.parent, FETCH_NEXT);     /* dotdot entry */
    fat_dir_list_index = 2;                                 /* update index position entry */
    if(fat_cache.parent.start_cluster == 0x00)              /* if parent dir is root */
    {
      fat_cache.parent.start_cluster = fat_rootclus_fat32;  /* then update start cluster value */
    }
    dir_is_root = FALSE;
    return fat_goto_next();
  }
  else
    return KO;                                /* current entry is not a dir */
}
  

/*F**************************************************************************
* NAME: fat_goto_parentdir
*----------------------------------------------------------------------------
* PARAMS: 
*   id: file extension to select
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Go to the parent directory
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/ 
bit fat_goto_parentdir (Byte id)
{ 
Uint32 temp_cluster;
  temp_cluster = dclusters[0].cluster + 2;        /* save cluster info */
  if (temp_cluster != fat_rootclus_fat32)
  {
    fat_cache.current = fat_cache.parent;         /* goto the parent directory */
  
    /* issue the equivalent to a cd .. DOS command */
    if (fat_goto_subdir(id))
    {
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
  else
  {
    return OK; /* Nothing to do because we are in the root directory */
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
  fat_up_down_load_sector(fat_ptr_fats + sector_number, DOWNLOAD);                   /* FAT 1 update */
  if (fat_2_is_present == TRUE)
  {
    fat_up_down_load_sector(fat_ptr_fats + sector_number + fat_fat_size, DOWNLOAD);  /* FAT 2 update */
  }      
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
*   
*****************************************************************************/
void fat_update_entry_fat (void)
{
Byte index;
Uint16 chain_index;
Uint16 sector_number;
Byte id;
Uint16 i;
Uint16 j;
Uint32 cluster;
Uint16 temp;
Uint32 nb_cluster;

/*********************/
/* Update directory entry */
/*********************/
  fat_clear_dir_info();
  fat_dir_current_offs = (fat_current_end_entry_position - 1) * 32;
  fat_calc_cluster();
  fat_up_down_load_sector(fat_dir_current_sect, UPLOAD);
  j = ((fat_current_end_entry_position - 1) % 16) * 32 ;                /* Position of entry in the sector */

  /* Update file size */
  if (fat_cache.current.size.l <= fat_file_size.l)
    fat_cache.current.size.l = fat_file_size.l;
  fat_buf_sector[j + 28] = fat_cache.current.size.b[3];
  fat_buf_sector[j + 29] = fat_cache.current.size.b[2];
  fat_buf_sector[j + 30] = fat_cache.current.size.b[1];
  fat_buf_sector[j + 31] = fat_cache.current.size.b[0];

  ext[0] = fat_buf_sector[j + 8];
  ext[1] = fat_buf_sector[j + 9];
  ext[2] = fat_buf_sector[j + 10];
  id = fat_check_ext();

  fat_up_down_load_sector(fat_dir_current_sect, DOWNLOAD);

/********************/
/* Update fat 1 & 2 */
/********************/
  /* Calculate file size cluster */
  nb_cluster = (fat_cache.current.size.l / SECTOR_SIZE) / fat_cluster_size;
  if ((fat_cache.current.size.l % (fat_cluster_size * SECTOR_SIZE)))
  {
    nb_cluster++;
  }
  j = 1;
  index = 0;

/********************/
/* FAT32 management */
/********************/

  /* init the starting cluster value */
  cluster = fclusters[0].cluster + 2;
  /* Start at first chain cluster */
  sector_number = cluster / 128;
  /* Bufferize fat sector */
  fat_up_down_load_sector(fat_ptr_fats + sector_number, UPLOAD);
  /* i -> word fat sector position */
  i = (cluster * 4) & 0x1FF;
  chain_index = 1;

  while (j < nb_cluster)
  {
    /* Determinate the value of the next cluster */
    if (fclusters[index].number == chain_index)
    {
      /* increase index */
      index++;
      cluster = fclusters[index].cluster + 2;
      fat_buf_sector[i++] = ((Byte*)&cluster)[3];
      fat_buf_sector[i++] = ((Byte*)&cluster)[2];
      fat_buf_sector[i++] = ((Byte*)&cluster)[1];
      fat_buf_sector[i]   = ((Byte*)&cluster)[0];
      chain_index = 1;
      if ( (cluster / 128) != sector_number)
      { /* Fat change sector */
        fat_update_fat_sector(sector_number);
        sector_number = (Uint16)(cluster / 128);
        fat_up_down_load_sector(fat_ptr_fats + sector_number, UPLOAD);
      }
      i = (cluster * 4) & 0x1FF;

    }
    else
    {
      cluster++;
      fat_buf_sector[i++] = ((Byte*)&cluster)[3];
      fat_buf_sector[i++] = ((Byte*)&cluster)[2];
      fat_buf_sector[i++] = ((Byte*)&cluster)[1];
      fat_buf_sector[i++] = ((Byte*)&cluster)[0];
      chain_index++;
      if (i == SECTOR_SIZE)     /* Sector is full ? */   
      {                                                  
        fat_update_fat_sector(sector_number);            
        sector_number++;                                 
        fat_up_down_load_sector(fat_ptr_fats + sector_number, UPLOAD);   
        i = 0;                                           
      }            
    }
    j++;
  }
  /* End of file indicate by 0x0FFFFF */
  fat_buf_sector[i++] = 0xFF;
  fat_buf_sector[i++] = 0xFF;
  fat_buf_sector[i++] = 0xFF;
  fat_buf_sector[i]   = 0x0F;
  fat_update_fat_sector(sector_number);

  temp = fat_dir_list_index - 1;
  fat_seek_first();
  for (; temp != 0; temp--)
    fat_goto_next();
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
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit fat_fopen (bit mode)
{
  if (mode == READ)
  {
    if (fat_cache.current.size.l == 0)
    {
      return KO;                                              /* empty file */
    }
    else
    {
      fat_fclust_byte_count = 0;                              /* byte 0 of cluster */
      fat_fchain_index = 0;                                   /* reset the allocation list variable */
      fat_fchain_nb_clust = 0;                                /* start on first contiguous cluster */
      fat_get_clusters(&fclusters, MAX_FILE_FRAGMENT_NUMBER); /* get file allocation list */
  
      fat_open_mode = READ;                                   /* seek to the beginning of the file */
      return Hard_read_open(fat_ptr_data + ((Uint32)(fclusters[0].cluster) 
                                               * fat_cluster_size));
    }
  }
  else
  {
    fat_fclust_byte_count = 0;                                /* byte 0 of cluster */
    fat_file_size.l = 0;  
    flag_end_disk_file = FALSE;                               /* reset end disk flag  */
    fat_fchain_index = 0;                                     /* reset the allocation list variable */
    fat_fchain_nb_clust = 0;                                  /* start on first contiguous cl */
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
*    
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Prepare creation of a wav file in the root directory
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   This function creates first the free cluster chain from fat1 and then
*   creates an entry in root directory
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit fat_fcreate (char *file_name, Byte attribute)
{
Byte temp_byte;
Byte i;
Uint16 j;
Uint16 index;
xdata Uint32 temp;
xdata Uint32 cluster;
xdata Uint32 temp_cluster;

  if (fat_no_entries_free == TRUE)                /* If last directory cluster is full */
  {
    cluster = 0;
    Hard_read_open(fat_ptr_fats);
    do
    {
      ((Byte*)&temp)[3] = Hard_read_byte();
      ((Byte*)&temp)[2] = Hard_read_byte();
      ((Byte*)&temp)[1] = Hard_read_byte();
      ((Byte*)&temp)[0] = Hard_read_byte();
      cluster++;
    }
    while (temp != 0x00000000);
    Hard_read_close();
    cluster--;                                                  /* Free cluster value */
    temp_cluster = dclusters[fat_last_dclust_index].cluster + dclusters[fat_last_dclust_index].number - 1;

    temp =  (temp_cluster + 2) / 128;                           /* End of cluster chain */
    fat_up_down_load_sector(fat_ptr_fats + temp, UPLOAD);
    j = (temp_cluster * 4) & 0x1FF;
    fat_buf_sector[j++] = ((Byte*)&cluster)[3];                 /* Update with the new cluster value */
    fat_buf_sector[j++] = ((Byte*)&cluster)[2];
    fat_buf_sector[j++] = ((Byte*)&cluster)[1];
    fat_buf_sector[j]   = ((Byte*)&cluster)[0];

    if ((cluster / 128) != temp)
    {
      fat_update_fat_sector(temp);
      temp = cluster / 128;
      fat_up_down_load_sector(fat_ptr_fats + temp, UPLOAD);
    }
    j = (cluster * 4) & 0x1FF;                                  /* Update end chain marker */
    fat_buf_sector[j++] = 0xFF;
    fat_buf_sector[j++] = 0xFF;
    fat_buf_sector[j++] = 0xFF;
    fat_buf_sector[j]   = 0x0F;
    fat_update_fat_sector(temp);                                /* Update fat sector */
    fat_last_dclust_index++;
    dclusters[fat_last_dclust_index].cluster = cluster - 2;     /* Update chain cluster value */
    dclusters[fat_last_dclust_index].number = 1;
    fat_last_dir_cluster_full = FALSE;
    fat_no_entries_free = FALSE;
    fat_total_clusters++;
  }

  fat_clear_dir_info();
  fat_dseek(0);

  temp_byte = fat_dgetc();          
  index = 0;
  while ((temp_byte != FILE_NOT_EXIST) && (temp_byte != FILE_DELETED))
  {
    for (i = DIR_SIZE - 1; i != 0; i--)
      fat_dgetc();
    temp_byte = fat_dgetc();
    index++;
  }
  if (temp_byte == FILE_DELETED)
  {
    fat_nb_deleted_entries--;
    if ((!fat_nb_deleted_entries) && (fat_last_dir_cluster_full == TRUE))
    {
      fat_no_entries_free = TRUE;
    }
  }
  else                              /* no deleted file entry */
  {
    fat_nb_total_entries++;         /* It's a new entry */
    if ((fat_nb_total_entries / 16) == fat_total_clusters * fat_cluster_size)
    {
      fat_last_dir_cluster_full = TRUE;
      fat_no_entries_free = TRUE;     /* Next time, add a cluster to the directory cluster chain */
    }
  }

  Hard_read_close();

  fat_get_free_clusters(1);
  cluster = fclusters[0].cluster + 2;
  /* Construct the entry */
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
  gl_buffer[11] = attribute;                         /* Attribute : archive */
  gl_buffer[12] = 0x00;                         /* Millisecond stamp at time creation */
  gl_buffer[13] = 0x00;                         /* Time */
  gl_buffer[14] = 0x00;
  gl_buffer[15] = 0x00;                         /* Date */
  gl_buffer[16] = 0x00;
  gl_buffer[18] = 0x00;                         /* Last access date */
  gl_buffer[19] = 0x00;
  gl_buffer[20] = ((Byte*)&cluster)[1];         /* High word First cluster number*/
  gl_buffer[21] = ((Byte*)&cluster)[0];
  gl_buffer[22] = 0x00;                         /* Time of last write */
  gl_buffer[23] = 0x00;
  gl_buffer[24] = 0x00;                         /* Date of last write */
  gl_buffer[25] = 0x00;
  gl_buffer[26] = ((Byte*)&cluster)[3];         /* Low word first cluster number */
  gl_buffer[27] = ((Byte*)&cluster)[2];
  gl_buffer[28] = 0x00;                         /* Size */
  gl_buffer[29] = 0x00;
  gl_buffer[30] = 0x00;
  gl_buffer[31] = 0x00;

  fat_dir_current_sect += ((fat_dclust_byte_count % (fat_cluster_size * SECTOR_SIZE))/512);

  fat_up_down_load_sector(fat_dir_current_sect, 0);
  j = (index % 16) * DIR_SIZE ;                 /* Position of entry in the sector */
  for (i = 0; i < DIR_SIZE; i++)
    fat_buf_sector[j++] = gl_buffer[i];

  fat_up_down_load_sector(fat_dir_current_sect, DOWNLOAD);

  fat_dir_list_last++;                          /* Increase max file number */
  fat_dir_list_index = 0;
  
  fat_current_start_entry_position = index;
  fat_current_end_entry_position = index;
  fat_clear_dir_info();
  fat_dir_current_offs = index * 32;
  fat_calc_cluster();
  fat_fetch_file_info(&fat_cache.current, FETCH_NEXT);
  /* open file in write mode */
  fat_fclust_byte_count = 0;                                        /* byte 0 of cluster */
  fat_file_size.l = 0;
  fat_cache.current.size.l = 0;
  fat_fchain_index = 0;                                             /* reset the allocation list variable */
  fat_fchain_nb_clust = 0;                                          /* start on first contiguous cl */
  fat_open_mode = WRITE;
  return Hard_write_open(fat_ptr_data + ((Uint32)(fclusters[0].cluster) 
                                         * fat_cluster_size));
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
  fat_dir_current_sect = (((Uint32)(dclusters[0].cluster)) * fat_cluster_size)
                           + fat_ptr_data;

  fat_get_dir_file_list(id); /* Refresh file list                          */
  fat_seek_entry_record();
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
*****************************************************************************/
void fat_clear_fat (Uint32 start_cluster)
{
Uint16 i;
Uint16 sector_number;
Uint32 cluster;

  /* init the starting cluster value */
  cluster = start_cluster;
  /* Start at first chain cluster */
  sector_number = cluster / 128;
  /* Bufferize fat sector */
  fat_up_down_load_sector(fat_ptr_fats + sector_number, UPLOAD);
  do 
  {
    if ((cluster / 128) != sector_number)
    {
      fat_update_fat_sector(sector_number);
      sector_number =  (Uint32)(cluster / 128);
      fat_up_down_load_sector(fat_ptr_fats + sector_number, UPLOAD);
    }
    i = (cluster * 4) & 0x1FF;
    ((Byte*)&cluster)[3] = fat_buf_sector[i];
    fat_buf_sector[i++] = 0;
    ((Byte*)&cluster)[2] = fat_buf_sector[i];
    fat_buf_sector[i++] = 0;
    ((Byte*)&cluster)[1] = fat_buf_sector[i];
    fat_buf_sector[i++] = 0;
    ((Byte*)&cluster)[0] = fat_buf_sector[i];
    fat_buf_sector[i] = 0;
  }
  while (cluster != 0x0FFFFFFF);
  fat_update_fat_sector(sector_number);
}


/*F**************************************************************************
* NAME: fat_fdelete
*----------------------------------------------------------------------------
* PARAMS:
*    
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Delete a selected file
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   This function works only on a root directory entry
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
Byte fat_fdelete (void)
{
Uint16 i;

  if (fat_check_ext() != FILE_DIR)
  {
    fat_clear_dir_info();
    fat_dir_current_offs = fat_current_start_entry_position * 32;
    fat_calc_cluster();
    fat_up_down_load_sector(fat_dir_current_sect, UPLOAD);
    i = (fat_current_start_entry_position % 16) * 32 ;                /* Position of entry in the sector */

    while (fat_buf_sector[i+11] == ATTR_LFN_ENTRY)
    {
      fat_buf_sector[i] = FILE_DELETED;               /* mark file as deleted */
      i += 32;
      fat_dclust_byte_count += 32;
  
      if (i == SECTOR_SIZE)
      {
        fat_up_down_load_sector(fat_dir_current_sect, DOWNLOAD);
        if  ((((Byte*)&fat_dclust_byte_count)[1] == 0x00) &&                  /* check if we are at the end of a cluster */
            ((((Byte*)&fat_dclust_byte_count)[0] & fat_cluster_mask) == 0x00))
        {
          /* extract if necessary the next cluster from the allocation list */
          if (dclusters[fat_dchain_index].number == fat_dchain_nb_clust)
          { /* new fragment */
            fat_dchain_index++;
            fat_dchain_nb_clust = 1;
            fat_dir_current_sect = (fat_ptr_data + ((Uint32)(dclusters[fat_dchain_index].cluster) * fat_cluster_size));
          }
          else
          {
            fat_dchain_nb_clust++;                          /* one more cluster read */
            fat_dir_current_sect++;                         /* Contiguous cluster    */
          }
        }
        else
        {
          fat_dir_current_sect++;
        }
        fat_up_down_load_sector(fat_dir_current_sect, UPLOAD);
        i = 0;
      }
  
    }
    fat_buf_sector[i] = FILE_DELETED;
    fat_up_down_load_sector(fat_dir_current_sect, DOWNLOAD);
    fat_clear_fat(fat_cache.current.start_cluster);         /* Clear fat cluster */

    fat_dir_list_last--;
    fat_nb_deleted_entries++;
    fat_last_dir_cluster_full = FALSE;
    fat_no_entries_free = FALSE;

    if ( ((dir_is_root == TRUE) && (fat_dir_list_last == 0)) ||     
         ((dir_is_root == FALSE) && (fat_dir_list_last == 2))      /* . and .. directory */
       )
      return DEL_RET_NO_MORE_FILE;

    if (fat_dir_list_index > fat_dir_list_last )
    {
      fat_dir_list_index = fat_dir_list_last;
    }
    return DEL_RET_OK;
  }
  else
  {
    return DEL_RET_ERROR_DIR;
  }
}


/*F**************************************************************************
* NAME: fat_get_free_clusters
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
bit fat_get_free_clusters (bit init)
{
#define NB_MAX_SECTOR             5
#define NB_MAX_CLUSTER_AT_INIT    3000
#define NB_MAX_CLUSTER            256
static idata Uint16 fat_sector;
static bit end_disk;
bit       cluster_free;
Byte      i;
Uint16    max_cluster;
Uint16    max_sector = 0;
Uint32    cluster;
Uint32    tot_cluster;

  if (init)
  {
    cluster = 0;
    fat_sector = 0;
    max_cluster = NB_MAX_CLUSTER_AT_INIT;
    last_free_cluster = 0;
    fat_last_clust_index = 0;
    end_disk = FALSE;
  }
  else
  {
    if (!end_disk)
    {
      cluster = last_free_cluster + 1;
      fat_sector = cluster / 128;
      max_cluster = NB_MAX_CLUSTER;
    }
    else
    {
      return KO;
    }
  }
  cluster_free = FALSE;
  tot_cluster = fat_count_of_clusters + 2;
  Hard_read_open(fat_ptr_fats + fat_sector);

  for (i = (cluster % 128); i != 0; i--)            /* dummy reads */
  {
    Hard_read_long_big_endian();
  }

  do                                      /* search for first free cluster */
  {
    if (Hard_read_long_big_endian() == 0x00000000)
    {
      cluster_free = TRUE;
    }
    else
    {
      cluster++;
      if (!(cluster & 0x7F) && (!init))
      {
        max_sector++;
      }
    }
  }
  while ((cluster != tot_cluster) && (!cluster_free) && (max_sector < NB_MAX_SECTOR));
   
  if (cluster == fat_count_of_clusters)
  {
    end_disk = TRUE;
  }

  if (!cluster_free)
  {
    last_free_cluster = cluster - 1;
    Hard_read_close();
    return KO;                            /* no free cluster found */
  }

  if (init)
  {
    fclusters[fat_last_clust_index].number = 1;  
    fclusters[fat_last_clust_index].cluster = cluster - 2;         /* store first cluster */
  }
  else
  {
    if (cluster == last_free_cluster + 1)
    {
     fclusters[fat_last_clust_index].number++;
    }
    else
    {
      fat_last_clust_index++;
      if (fat_last_clust_index == MAX_FILE_FRAGMENT_NUMBER)
      {
        Hard_read_close();
        fat_last_clust_index--;
        end_disk = TRUE;
        return OK;
      }
      fclusters[fat_last_clust_index].number = 1;
      fclusters[fat_last_clust_index].cluster = cluster - 2;
    }
  }
  cluster++;
  max_cluster--;

  if (cluster != tot_cluster)
  {
    do                                      /* construct the list */
    {
      cluster_free = FALSE;
      if (Hard_read_long_big_endian() == 0x00000000)
        cluster_free = TRUE;
      else
        cluster++;

      if (!(cluster & 0x7F) && (!init))
      {
        max_sector++;
      }

      if (cluster_free)
      { /* free cluster: add it to the list */
        if (fclusters[fat_last_clust_index].number == MAX_CL_PER_FRAG) 
        {
          fat_last_clust_index++;
          if (fat_last_clust_index == MAX_FILE_FRAGMENT_NUMBER)
          {
            Hard_read_close();
            fat_last_clust_index--;
            end_disk = TRUE;
            return OK;
          }
          fclusters[fat_last_clust_index].number = 1;
          fclusters[fat_last_clust_index].cluster = cluster - 2;
        }
        else
        {
          fclusters[fat_last_clust_index].number++;
        }
        cluster++;
        max_cluster--;
        if (!(cluster & 0x7F) && (!init))
        {
          max_sector++;
        }
      }
      else if (cluster != tot_cluster)
      { /* cluster already used */
        do                                  /* search for next free fragment */
        {
          if (Hard_read_long_big_endian() == 0x00000000)
          {
            cluster_free = TRUE;
          }
          else
          {
            cluster++;
            if (!(cluster & 0x7F) && (!init))
            {
              max_sector++;
            }
          }
        }
        while ( (cluster != tot_cluster) && (!cluster_free) && (max_sector < NB_MAX_SECTOR));

        if (!cluster_free)
        {
          last_free_cluster = cluster - 1;
          Hard_read_close();
          return OK;                        /* end of partition reached */
        }

        fat_last_clust_index++;
        fclusters[fat_last_clust_index].number = 1;
        fclusters[fat_last_clust_index].cluster = cluster - 2;
        cluster++;
        max_cluster--;
        if (!(cluster & 0x7F) && (!init))
        {
          max_sector++;
        }
      }
    }
    while ((max_sector < NB_MAX_SECTOR) && (fat_last_clust_index < MAX_FILE_FRAGMENT_NUMBER) && (cluster < tot_cluster) && (max_cluster != 0));
  }

  if ((cluster >= tot_cluster) || (fat_last_clust_index >= MAX_FILE_FRAGMENT_NUMBER))
    end_disk = TRUE;

  last_free_cluster = cluster - 1;
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
Byte   index;                        /* index in chain */
Uint32 i; 
Uint32 new_cluster;
Uint32 old_cluster;

  nb_frag = nb_frag - 1;                    /* set limit (index start at 0) */

  /* build the first entry of the allocation list */
  chain[0].number = 1;
  chain[0].cluster = fat_cache.current.start_cluster - 2; /* 2 = 1st cluster */
  old_cluster = fat_cache.current.start_cluster;
  index = 0;

  Hard_read_open(fat_ptr_fats + (old_cluster * 4 / SECTOR_SIZE));
  
  /* compute offset in sector */
  for (i = (old_cluster % (128)); i != 0; i--)
  {
    Hard_read_long_big_endian();      /* dummy FAT read */
  }
  /* read first entry */
  new_cluster = Hard_read_long_big_endian(); 
  
  while (new_cluster != LAST_CLUSTER_32)   /* loop until last cluster found */
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
      Hard_read_close();
      Hard_read_open(fat_ptr_fats + (new_cluster * 4 / SECTOR_SIZE));
  
      /* compute offset in sector */
      for (i = (new_cluster % (128)); i != 0; i--)
      {
        Hard_read_long_big_endian();      /* dummy FAT read */
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
      new_cluster = Hard_read_long_big_endian(); 
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
    Hard_write_close();
    fat_get_free_clusters(0);
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
      }
    }
    else
    { /* no new fragment */
      fat_fchain_nb_clust++;                /* one more cluster read */
    }
    Hard_write_open(fat_ptr_data + ((Uint32)(fclusters[fat_fchain_index].cluster + fat_fchain_nb_clust - 1) * fat_cluster_size));
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
Uint16 i;
Uint16 nb_byte;                     /* number of bytes to seek */

  fat_dir_current_offs += offset;           /* Calculate the absolute byte pos */
  nb_byte = (Uint16)((fat_dir_current_offs) % (SECTOR_SIZE * fat_cluster_size));
  fat_dclust_byte_count = nb_byte;
  nb_byte %= SECTOR_SIZE;                   

  fat_calc_cluster();

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
Uint32 next_cluster;                      /* new cluster in allocation list */

  /* check if we are at the end of a cluster */
  if ((((Byte*)&fat_dclust_byte_count)[1] == 0x00) &&
      ((((Byte*)&fat_dclust_byte_count)[0] & fat_cluster_mask) == 0x00))
  {
      /* extract if necessary the next cluster from the allocation list */
      if (dclusters[fat_dchain_index].number == fat_dchain_nb_clust)
      { /* new fragment */
        fat_dchain_index++;
        fat_dchain_nb_clust = 1;
        Hard_read_open(fat_ptr_data + ((Uint32)(dclusters[fat_dchain_index].cluster) * fat_cluster_size));
        next_cluster = dclusters[fat_dchain_index].cluster;
      }
      else
      { /* no new fragment */
        next_cluster = dclusters[fat_dchain_index].cluster + fat_dchain_nb_clust;
        fat_dchain_nb_clust++;                /* one more cluster read */
      }
      fat_dir_current_sect = fat_ptr_data + ((Uint32)(next_cluster) * fat_cluster_size);
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
Uint32  cluster_offset;                     /* cluster offset in file */
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


  if ( fat_fclust_byte_count % (fat_cluster_size * SECTOR_SIZE) == 0)
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
xdata Uint32  cluster_offset;                     /* cluster offset in file */
Byte    sector_offset;                      /* sector offset in cluster */
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
Uint16 i;

  for (i = 0; i < 256; i++)
    gl_buffer[i] = '\0';
}


void fat_reset_sector(void)
{
  Byte i;
  for (i = 512 / 16; i != 0; i--)
  {
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
  }
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

Uint16 j;
Byte i;
Uint32 nb_total_sectors;
xdata Uint32 nb_sector_fat;
xdata Uint32 nb_free_clusters;
s_format *tab;
Uint16 hidden_sector;
Uint16 format_nb_sector;
Uint16 nb_head;
Uint16 nb_cylinder;
bit repeat;

  tab = Hard_format();

  hidden_sector = FORMAT_NB_HIDDEN_SECTOR;
  format_nb_sector = FORMAT_NB_SECTOR;
  nb_head = FORMAT_NB_HEAD;
  nb_cylinder = FORMAT_NB_CYLINDER;
  fat_cluster_size = FORMAT_NB_SECTOR_PER_CLUSTER;

  nb_total_sectors = Hard_get_capacity() - hidden_sector;
  nb_sector_fat = ((nb_total_sectors / fat_cluster_size) * 4 / SECTOR_SIZE) + 1;
  nb_free_clusters = (nb_total_sectors - (nb_sector_fat * 2) - hidden_sector) / fat_cluster_size - 1;



#if FAT_PARTITIONNED == TRUE
  /* -- MASTER BOOT RECORD -- */
  Hard_write_open(MBR_ADDRESS);
  for (i = 446/2; i != 0; i--)              /* Boot Code */
  {
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
  }

  /* First Partition entry */
  Hard_write_byte(0x80);                                                      /* Default Boot Partition */
  Hard_write_byte((Byte)(hidden_sector / format_nb_sector));                  /* Start head */
  Hard_write_byte((Byte)((hidden_sector % format_nb_sector) + 1));            /* Start Sector */
  Hard_write_byte(0x00);                                                      /* Start Cylinder */

  Hard_write_byte(FAT32);                                                     /* FAT32  */

  Hard_write_byte((Byte)(nb_head - 1));                                       /* Endhead-Zero-based(0)head number */
  Hard_write_byte((Byte)((((nb_cylinder - 1) / 0x04) & 0xC0) +  format_nb_sector));  
                                                                              /* EndSector-Zero-based(1) sector number */
  Hard_write_byte((Byte)((nb_cylinder - 1) & 0xFF));                          /* EndCylinder */
  Hard_write_byte((Byte)(hidden_sector));                                     /* Start sector */
  Hard_write_byte(0x00);
  Hard_write_byte(0x00);
  Hard_write_byte(0x00);
  
  Hard_write_byte(((Byte*)&nb_total_sectors)[3]);
  Hard_write_byte(((Byte*)&nb_total_sectors)[2]);
  Hard_write_byte(((Byte*)&nb_total_sectors)[1]);
  Hard_write_byte(((Byte*)&nb_total_sectors)[0]);

  for (i = 3; i != 0; i--)                /* Other partition entry */
  {
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
  }
  
  Hard_write_byte(0x55);                    /* Signature Word */
  Hard_write_byte(0xAA);

  /* -- HIDDEN SECTORS -- */
  for (j = hidden_sector - 1; j != 0; j--)
  {
    fat_reset_sector();
  }
#else   /* fat not partitionned */
  Hard_write_open(MBR_ADDRESS);
#endif  /* FAT_PARTITIONNED == TRUE */

  repeat = 0;
  do
  {
    repeat = ~repeat;
    /* -- PARTITION BOOT RECORD -- */
    Hard_write_byte(0xEB);                    /* JMP inst to PBR boot code */
    Hard_write_byte(0x3C);
    Hard_write_byte(0x90);
    Hard_write_byte('M');                     /* OEM name */
    Hard_write_byte('S');
    Hard_write_byte('W');
    Hard_write_byte('I');
    Hard_write_byte('N');
    Hard_write_byte('4');
    Hard_write_byte('.');
    Hard_write_byte('1');
    Hard_write_byte(SECTOR_SIZE);             /* number of bytes per sector */
    Hard_write_byte(SECTOR_SIZE >> 8);
    Hard_write_byte(fat_cluster_size);        /* Number of sector per cluster */
    Hard_write_byte(NB_RESERVED);             /* number of reserved sector */
    Hard_write_byte(NB_RESERVED >> 8);
    Hard_write_byte(NB_FATS);                 /* Number of FAT */
    Hard_write_byte(NB_ROOT_ENTRY_32);        /* number of root directory entries */
    Hard_write_byte(NB_ROOT_ENTRY_32 >> 8);
                                                              
    /* Total Sectors */
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
  
    Hard_write_byte(HARD_DISK);                             /* Media Byte */
    /* Number of sector in each FAT */
    Hard_write_byte(0x00);                                  /* 0x0000 for FAT32 */
    Hard_write_byte(0x00);
    Hard_write_byte(((Byte*)&format_nb_sector)[1]);         /* Number of sectors on a track */
    Hard_write_byte(((Byte*)&format_nb_sector)[0]);
    Hard_write_byte(((Byte*)&nb_head)[1]);                  /* Number of heads */
    Hard_write_byte(((Byte*)&nb_head)[0]);
    Hard_write_byte(((Byte*)&hidden_sector)[1]);            /* Number of hidden sectors */
    Hard_write_byte(((Byte*)&hidden_sector)[0]);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
     /* number of sectors > 65535 */
    Hard_write_byte(((Byte*)&nb_total_sectors)[3]);
    Hard_write_byte(((Byte*)&nb_total_sectors)[2]);
    Hard_write_byte(((Byte*)&nb_total_sectors)[1]);
    Hard_write_byte(((Byte*)&nb_total_sectors)[0]);
  
    Hard_write_byte(((Byte*)&nb_sector_fat)[3]);           /* nb sector for each fat */
    Hard_write_byte(((Byte*)&nb_sector_fat)[2]);
    Hard_write_byte(((Byte*)&nb_sector_fat)[1]);
    Hard_write_byte(((Byte*)&nb_sector_fat)[0]);
  
  
    /* Ext Flags */
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    /* FS Version */
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    /* Root Cluster */
    Hard_write_byte(0x02);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    /*FSInfo sector */
    Hard_write_byte(0x01);
    Hard_write_byte(0x00);
    /* Backup Boot Sector */
    Hard_write_byte(0x06);
    Hard_write_byte(0x00);
  
    /* Reserved */
    for (i = 12; i != 0; i--)
      Hard_write_byte(0x00);
  
    Hard_write_byte(FAT_DRIVE_NUMBER);        /* Driver number */ 
    Hard_write_byte(0x00);                    /* not used */
    Hard_write_byte(FAT_EXT_SIGN);            /* extended boot signature */
    Hard_write_byte(0x00);                    /* volume ID */
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte('N');                     /* Volume Label */
    Hard_write_byte('O');
    Hard_write_byte(' ');
    Hard_write_byte('N');
    Hard_write_byte('A');
    Hard_write_byte('M');
    Hard_write_byte('E');
    Hard_write_byte(' ');
    Hard_write_byte(' ');
    Hard_write_byte(' ');
    Hard_write_byte(' ');
    Hard_write_byte('F');                     /* File System Type in ASCII */
    Hard_write_byte('A');
    Hard_write_byte('T');
    Hard_write_byte('3');
    Hard_write_byte('2');
    Hard_write_byte(' ');
    Hard_write_byte(' ');
    Hard_write_byte(' ');
  
    for (i = 420/2; i != 0; i--)              /* Boot Code */
    {
      Hard_write_byte(0x00);
      Hard_write_byte(0x00);
    }
    Hard_write_byte(0x55);                    /* Signature word */
    Hard_write_byte(0xAA);
  
    /* FSInfo Sector  */
  
    Hard_write_byte(0x52);    /* Lead Signature */
    Hard_write_byte(0x52);
    Hard_write_byte(0x61);
    Hard_write_byte(0x41);
    for (i = 480 / 2; i != 0; i--)  /* Reserved */
    {
      Hard_write_byte(0x00);
      Hard_write_byte(0x00);
    }
    Hard_write_byte(0x72);    /* Structure signature */
    Hard_write_byte(0x72);
    Hard_write_byte(0x41);
    Hard_write_byte(0x61);
      
  
    Hard_write_byte(((Byte*)&nb_free_clusters)[3]);    /* Free cluster count */
    Hard_write_byte(((Byte*)&nb_free_clusters)[2]);
    Hard_write_byte(((Byte*)&nb_free_clusters)[1]);
    Hard_write_byte(((Byte*)&nb_free_clusters)[0]);
  
    Hard_write_byte(0x02);    /* Next free cluster */
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
  
    for (i = 12 / 4; i != 0; i--)  /* Reserved */
    {
      Hard_write_byte(0x00);
      Hard_write_byte(0x00);
      Hard_write_byte(0x00);
      Hard_write_byte(0x00);
    }
  
    Hard_write_byte(0x00);  /* Trail signature */
    Hard_write_byte(0x00);
    Hard_write_byte(0x55);
    Hard_write_byte(0xAA);
  
    for (i = 510/2; i != 0; i--)
    {
      Hard_write_byte(0x00);
      Hard_write_byte(0x00);
    }
    Hard_write_byte(0x55);
    Hard_write_byte(0xAA);
  
    for (i = 3; i != 0; i--)
    {
      fat_reset_sector();
    }
  }
  while (repeat);
  
  for (i = 510/2; i != 0; i--)
  {
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
  }
  Hard_write_byte(0x55);
  Hard_write_byte(0xAA);

  for (i = NB_RESERVED - 13 ; i != 0; i--)
  {
    fat_reset_sector();
  }
 
  /* -- FATS -- */
  /* -- FAT 1 -- */
  Hard_write_byte(0xF8);                    /* reserved clusters 0 & 1 */
  Hard_write_byte(0xFF);
  Hard_write_byte(0xFF);
  Hard_write_byte(0x0F);
  Hard_write_byte(0xFF);                    /* reserved clusters 0 & 1 */
  Hard_write_byte(0xFF);
  Hard_write_byte(0xFF);
  Hard_write_byte(0x0F);
  Hard_write_byte(0xFF);                    /* reserved clusters 2 : Root */
  Hard_write_byte(0xFF);
  Hard_write_byte(0xFF);
  Hard_write_byte(0x0F);

  /* free clusters in first FAT sector */
  for (i = (SECTOR_SIZE - 12) / 4; i != 0; i--)
  {
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
  }
  /* free clusters in other FAT sectors */
  for (j = nb_sector_fat - 1; j != 0; j--)
  {
    fat_reset_sector();
  }

  /* -- FAT 2 -- */
  Hard_write_byte(0xF8);                    /* reserved clusters 0 & 1 */
  Hard_write_byte(0xFF);
  Hard_write_byte(0xFF);
  Hard_write_byte(0x0F);
  Hard_write_byte(0xFF);                    /* reserved clusters 0 & 1 */
  Hard_write_byte(0xFF);
  Hard_write_byte(0xFF);
  Hard_write_byte(0x0F);
  Hard_write_byte(0xFF);                    /* reserved clusters 2 : Root */
  Hard_write_byte(0xFF);
  Hard_write_byte(0xFF);
  Hard_write_byte(0x0F);

  /* free clusters in first FAT sector */
  for (i = (SECTOR_SIZE - 12) / 4; i != 0; i--)
  {
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
    Hard_write_byte(0x00);
  }
  /* free clusters in other FAT sectors */
  for (j = nb_sector_fat - 1; j != 0; j--)
  {
    fat_reset_sector();
  }


  /* -- ROOT DIRECTORY ENTRIES -- */
  for (j = FORMAT_NB_SECTOR_PER_CLUSTER; j != 0; j--)
  {
    fat_reset_sector();
  }
  Hard_write_close();
}


