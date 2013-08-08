/*H**************************************************************************
* NAME:         fat32.h         
*----------------------------------------------------------------------------
* Copyright (c) 2003 Atmel.
*----------------------------------------------------------------------------
* RELEASE:      snd1c-refd-nf-4_0_3      
* REVISION:     1.2     
*----------------------------------------------------------------------------
* PURPOSE:
* FAT32 file-system basic functions definition
*****************************************************************************/

#ifndef _FAT_H_
#define _FAT_H_

/*_____ I N C L U D E S ____________________________________________________*/


/*_____ M A C R O S ________________________________________________________*/

/*----- FAT Configuration -----*/
#define FAT_PARTITIONNED  TRUE              /* TRUE: format with 1 partition, 
                                               FALSE: without partition */


#define READ              ((bit)0)
#define WRITE             ((bit)1)

#define FREE_CLUSTER      ((Uint16)0x0000)  /* free cluster value in FAT */
#define LAST_CLUSTER12    ((Uint16)0x0FFF)  /* last file cluster in FAT 12 */
#define BAD_CLUSTER12     ((Uint16)0x0FF7)  /* bad cluster value in FAT 12 */
#define LAST_CLUSTER16    ((Uint16)0xFFFF)  /* last file cluster in FAT 16 */
#define BAD_CLUSTER16     ((Uint16)0xFFF7)  /* bad cluster value in FAT 16  */

#define MBR_ADDRESS       ((Uint32)0)       /* Master Boot Record address */
#define ATTR_FILE         ((Byte)0x00)      /* Normal File Attribute */
#define ATTR_READ_ONLY    ((Byte)0x01)      /* Read Only File Attribute */
#define ATTR_HIDDEN       ((Byte)0x02)      /* Hidden File Attribute */
#define ATTR_SYSTEM       ((Byte)0x04)      /* System File Attribute */
#define ATTR_DIRECTORY    ((Byte)0x10)      /* Directory Attribute */
#define ATTR_ARCHIVE      ((Byte)0x20)      /* Archive Attribute */
#define ATTR_LFN_ENTRY    ((Byte)0x0F)      /* LFN entry attribute */
#define ATTR_ROOT_DIR     ((Byte)0x90)      /* internal use only */

#define PARTITION_ACTIVE  ((Byte)0x80)      /* the active partition state */

#define FILE_NOT_EXIST    ((char)0x00)      /* current file does not exist */
#define FILE_DELETED      ((char)0xE5)      /* current file is deleted */

#define LFN_SEQ_MASK      ((Byte)0x3F)      /* LFN sequence number mask */

#define ODD_EVEN_MASK     ((Byte)0x01)      /* the odd / even test mask */

#define FAT12             ((Byte)0x01)      /* FAT12 type */
#define FAT16_INF32M      ((Byte)0x04)      /* FAT16 type < 32 Mbytes */
#define FAT16_SUP32M      ((Byte)0x06)      /* FAT16 type > 32 Mbytes */
#define FAT32             ((Byte)0x0B)      /* FAT32 type not supported */
#define MAX_CLUSTERS12    ((Uint16)4084)    /* FAT12 if < FAT16 else */
#define MAX_CLUSTERS16    ((Uint16)65524)   /* FAT16 if < FAT32 else */
#define LAST_CLUSTER_32   ((Uint32)0x0FFFFFFF)


/* Format Info */
#define SECTOR_SIZE       ((Uint16)512)     /* supported sector size */
#define DIR_SIZE          ((Byte)32)        /* directory entry size */
#define NB_ROOT_ENTRY_32  ((Uint16)0)
#define NB_ROOT_ENTRY     ((Uint16)512)
#define NB_RESERVED       ((Byte)32)        /* number of reserved sectors */
#define NB_FATS           ((Byte)2)
#define HARD_DISK         ((Byte)0xF8)      /* hard disk device */
#define FAT_DRIVE_NUMBER  ((Byte)0x81)
#define FAT_EXT_SIGN      ((Byte)0x29)
#define BR_SIGNATURE      ((Uint16)0xAA55)  /* boot record signature */
#define MBR_ADDRESS       ((Uint32)0)       /* master boot record address */

/* FAT definition */
/* For LFN name, the name will be build with MAX_LFN_ENTRIES entries        */
/* That means tha the max file name length is                               */
/* 12 + (MAX_LFN_ENTRIES - 1 ) * 13                                         */
/* Size in pdata zone is 256 bytes.                                         */
/* gl_buffer use 32 bytes in pdata                                          */
/* We need 15 bytes for scrolling display                                   */
/* Maximum size for LFN name is 256 - 32 - 15 = 209 bytes                   */
/* And so MAX_LFN_ENTRIES = (209 - 12) / 13 + 1 = 16                        */
/* And size of LFN string is                                                */
/* MAX_FILENAME_LEN = 12 + (MAX_LFN_ENTRIES - 1) * 13 + 15 + 1 + 1          */
/*                                                           |   |          */
/*                                  +1 because we start at 0 -   |          */
/*                                  +1 for '\0' character    -----          */

/* maximum number of LFN entries */
#define MAX_LFN_ENTRIES   ((Byte)16)       
/* maximum file name length      */
#define MAX_FILENAME_LEN  ((Byte)(12 + (MAX_LFN_ENTRIES - 1) * 13 + 15 + 1 + 1))      

#define MAX_FILE_FRAGMENT_NUMBER ((Byte)75) /* maximum number of authorized
                                            /* fragment for a file          */
#define MAX_DIR_FRAGMENT_NUMBER ((Byte)10)  /* maximum number of authorized
                                            /* fragment for a directory     */
#define MAX_DIRECTORY_FILE ((Uint16)512)    /* maximum number of file in    */
                                            /* a directory                  */
//#define MAX_DIRECTORY_GAP_FILE ((Uint16)(65535))
#define MAX_DIRECTORY_GAP_FILE ((Byte)(255))
                                            /* maximum delta between 2      */
                                            /* specified entries            */
                                            /* Can be replace 255 or 65535  */
                                            /* case of type used for        */
                                            /* fat_directory_chain          */

#define DEL_RET_OK                        (Byte)(0)
#define DEL_RET_NO_MORE_FILE              (Byte)(1)
#define DEL_RET_ERROR_DIR                 (Byte)(2)

/*******************************************************************************
* PURPOSE:
* FAT Directory Entry structure
*******************************************************************************/
typedef struct
{
  Byte    attributes;                       /* Attributes bits                */
  /* Here could stand times variables, unused ...                             */
  Uint32  start_cluster;                    /* Starting cluster number        */
  Union32 size;                             /* File size in bytes             */
} fat_st_dir_entry;


/*******************************************************************************
* PURPOSE: 
* FAT cache of parent directory and current file or directory 
*******************************************************************************/
typedef struct
{
  fat_st_dir_entry parent;                     /* parent directory */
  fat_st_dir_entry current;                    /* current file/directory info */
} fat_st_cache;
  

/*******************************************************************************
* PURPOSE: 
* FAT cluster chain structure (each index is a fragment of the file)
*******************************************************************************/
typedef struct
{
  Uint32 cluster;                           /* starting cluster of fragment */
  Uint16 number;                            /* number of subsequent clusters in fragment */
} fat_st_clust_chain;

#define MAX_CL_PER_FRAG     ((0x01 << ((sizeof(fat_st_clust_chain) - 4) * 8)) - 1)

/******************************************************************************
* PURPOSE : 
* FAT fs functions
******************************************************************************/

bit           fat_get_root_directory (Byte id);
bit           fat_goto_next (void);
bit           fat_goto_prev (void);
bit           fat_seek_last (void);
bit           fat_seek_first (void);
bit           fat_goto_parentdir (Byte id);
bit           fat_goto_subdir (Byte id);

bit           fat_fopen (bit);
void          fat_fclose (void);
bit           fat_fcreate (char *, Byte);
void          fat_refresh_dir_file_info (Byte);
Byte          fat_fdelete (void);
Byte          fat_fgetc (void);       
void          fat_fputc (Byte);       
bit           fat_feof (void);
bit           fat_fseek (Int32);
void          fat_fseek_abs(Uint32);
void          fat_format (void);

Byte          fat_check_ext (void);

bit           fat_install (void);
Uint32        fat_file_get_pos(void);
bit           fat_feob(void);
void          fat_save_cluster_info(void) ;

char pdata *  fat_get_name (void);
void          fat_clear_file_name(void);

#endif  /* _FAT_H_ */

