/*C**************************************************************************
* NAME:         fs_variable.c
*----------------------------------------------------------------------------
* Copyright (c) 2003 Atmel.
*----------------------------------------------------------------------------
* RELEASE:      snd1c-refd-nf-4_0_3      
* REVISION:     1.3     
*----------------------------------------------------------------------------
* PURPOSE:
* File system variable definition
* 
* NOTES:
*   Arrangement :
*     FAT12/16 only
*     FAT32 only
*     FAT32 and ISO 9660
*     FAT12/16 and ISO 9660 ?
*****************************************************************************/

/*_____ I N C L U D E S ____________________________________________________*/

#include "config.h"                         /* system configuration */
#include "..\mem\hard.h"                    /* low level function definition */
#include "file.h"                           /* file function definition */

#if ((MEM_CHIP_FS == FS_FAT_32) || (MEM_CARD_FS == FS_FAT_32))
  #include "fat32.h"                          /* fat32 file-system definition */
#elif ((MEM_CHIP_FS == FS_FAT_12_16) || (MEM_CARD_FS == FS_FAT_12_16))
  #include "fat.h"
#endif

#if ((MEM_CHIP_FS == FS_ISO) || (MEM_CARD_FS == FS_ISO))
  #include "iso9660.h"
#endif



/*_____ M A C R O S ________________________________________________________*/


/*_____ D E F I N I T I O N ________________________________________________*/



extern  pdata Byte    gl_buffer[];


/* shared file system variables */

/* disk management */
data  Uint32  fat_ptr_data;                     /* address of the first byte 
                                                /* of data */
xdata Byte fat_buf_sector[512];                 /* 512 bytes buffer */

/* directory management */
idata Uint16  fat_dclust_byte_count;/* byte counter in directory sector */
idata Uint32  fat_dir_current_sect; /* sector of selected entry in dir list */
xdata Uint16  fat_dir_list_index;   /* index of current entry in dir list */
xdata Uint32  fat_dir_start_sect;   /* start sector of dir list */
xdata Uint16  fat_dir_list_last;    /* index of last entry in dir list */
idata Uint16  fat_dchain_index;     /* the number of the fragment of the dir, in fact */
                                    /* the index of the table in the cluster chain */
idata  Uint16  fat_fchain_nb_clust; /* the offset of the cluster from the first cluster */
                                    /* of the file fragment */


/* file management */
data  Uint16   fat_fclust_byte_count;     /* byte counter in file cluster */
xdata Byte current_ext;
xdata char  ext[3];                       /* file extension (limited to 3 characters) */
char  pdata *lfn_name = &(gl_buffer[32]); /* long filename limited to MAX_FILENAME_LEN chars */

/* Mode repeat A/B variables */
xdata  Byte    fat_fchain_index_save;         
xdata  Byte    fat_fchain_nb_clust_save;
xdata  Uint16  fat_fclust_byte_count_save;




/* Specific variables for fat file system */
#if ((MEM_CHIP_FS == FS_FAT_32) || (MEM_CHIP_FS == FS_FAT_12_16) || (MEM_CARD_FS == FS_FAT_32) || (MEM_CARD_FS == FS_FAT_12_16))
/* disk management */
data  Uint32  fat_ptr_fats;         /* address of the first byte of FAT */
data  Uint32  fat_ptr_rdir;
data  Byte    fat_cluster_size;     /* cluster size (sector count) */
idata Byte    fat_cluster_mask;     /* mask for end of cluster test */


bdata bit     fat_is_fat16;         /* TRUE: FAT16 - FALSE: FAT12 */
bdata bit     fat_open_mode;        /* READ or WRITE */
bdata bit     fat_2_is_present;     /* TRUE: 2 FATs - FALSE: 1 FAT */
bdata bit     flag_end_disk_file;

xdata Uint32  fat_count_of_clusters;/* number of cluster - 2 */
xdata Union32 fat_file_size;
xdata Uint32  fat_fat_size;         /* FAT size in sector count */

/* directory management */
xdata fat_st_clust_chain dclusters[MAX_DIR_FRAGMENT_NUMBER];
                                    /* cluster chain for the current directory */
bdata bit     dir_is_root;          /* TRUE: point the root directory  */
                                             
idata Byte    fat_dchain_nb_clust;  /* the offset of the cluster from the first cluster */
                                    /* of the dir fragment */
xdata Byte    fat_last_dclust_index;/* index of the last cluster in directory chain */
xdata Uint32  fat_dir_current_offs; /* entry offset from fat_dir_current_sect */
xdata fat_st_cache   fat_cache;     /* The cache structure, see the .h for more info */


/* file management */
xdata fat_st_clust_chain fclusters[MAX_FILE_FRAGMENT_NUMBER];
                                    /* cluster chain for the current file */
idata Byte    fat_last_clust_index;/* index of the last cluster in file chain */
idata Byte    fat_fchain_index;    /* the number of the fragment of the file, in fact */
                                    /* the index of the table in the cluster chain */
              
xdata Uint32  fat_current_file_size;
xdata Uint32  fat_rootclus_fat32;  /* root cluster address */
bdata bit fat_last_dir_cluster_full;
bdata bit fat_no_entries_free;
xdata Uint16 fat_total_clusters;
xdata Uint32 last_free_cluster;

xdata Uint16  fat_root_entry;       /* position in root dir */


idata Uint16 fat_current_end_entry_position;
idata Uint16 fat_current_start_entry_position;
xdata Uint16 fat_nb_deleted_entries;
xdata Uint16 fat_nb_total_entries;

bdata bit    fat_is_fat32;                   /* TRUE: FAT32 - FALSE: FAT12/FAT16 */




#endif


#if (MEM_CHIP_FS == FS_ISO) || (MEM_CARD_FS == FS_ISO)
/* iso9660 variables */
xdata iso_VolumeDescriptor iso_header;    /* iso header informations                          */
xdata iso_cache  iso_file_cache;          /* cache for the current file                       */
idata Uint16  iso_f_nb_sector;
idata Uint16  iso_f_max_sector;
bdata bit     iso_cd;                     /* if set to one cd is iso else cd is joliet format  */
#endif
