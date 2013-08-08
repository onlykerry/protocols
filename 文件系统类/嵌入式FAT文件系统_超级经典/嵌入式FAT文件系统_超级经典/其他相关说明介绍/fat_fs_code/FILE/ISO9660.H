/*H**************************************************************************
* NAME:         iso9660.h         
*----------------------------------------------------------------------------
* Copyright (c) 2003 Atmel.
*----------------------------------------------------------------------------
* RELEASE:      snd1c-refd-nf-4_0_3      
* REVISION:     1.3     
*----------------------------------------------------------------------------
* PURPOSE:
* ISO filesystem basic functions
*****************************************************************************/

#ifndef _ISO9660_
#define _ISO9660_


#define TYPE_BOOT_RECORD              0x00
#define TYPE_PRIMARY_VD               0x01
#define TYPE_SUPPLEMENTARY_VD         0x02
#define TYPE_PARTITION_VD             0x03
#define TYPE_VOLUME_SET_TERMINATOR    0xFF

#define ATTR_ISO_DIR                  0x02

#define DEL_RET_OK                        (Byte)(0)
#define DEL_RET_NO_MORE_FILE              (Byte)(1)
#define DEL_RET_ERROR_DIR                 (Byte)(2)

/* maximum number of LFN entries */
#define MAX_LFN_ENTRIES   ((Byte)16)       
/* maximum file name length      */
#define MAX_FILENAME_LEN  ((Byte)(12 + (MAX_LFN_ENTRIES - 1) * 13 + 15 + 1 + 1))      
#define MAX_DIRECTORY_FILE ((Uint16)512)    /* maximum number of file in    */


/*******************************************************************************
* PURPOSE: 
* Directory Record Structure
*******************************************************************************/
typedef struct
{
  Byte length;            /* length of the record     */
  Uint32 extend_location; /* location of the extend   */
  Uint32 data_length;     /* length of directory/file */
} iso_DirectoryRecord;

/*******************************************************************************
* PURPOSE:
* Volume Descriptor Record Structure
*******************************************************************************/
typedef struct 
{
  Uint16 logical_block_size;  /* Size of the logical block usually 2048 bytes */
  iso_DirectoryRecord root;   /* root directory record                        */
  Uint32 volume_size;         /* volume size                                  */
} iso_VolumeDescriptor;

/*******************************************************************************
* PURPOSE:
* ISO cache of parent directory and current file or directory 
*******************************************************************************/
typedef struct 
{
  Byte attributes;
  Uint32 extend_location;
  Uint32 extend_size;
  Uint16 entry_len;
} iso_file;

typedef struct
{
  iso_file info;
  iso_file parent;
} iso_cache;


/*******************************************************************************
* PURPOSE: 
* ISO file system functions (fs/iso9660/iso9660.c)
*******************************************************************************/
bit     iso_install(void);
bit     iso_read_volume_descriptor(Uint32);
bit     iso_goto_first(void);
bit     iso_goto_next(void);
bit     iso_goto_prev(void);
Byte    iso_check_ext(void);
bit     iso_goto_subdir(void);
bit     iso_goto_parent_dir(void);
bit     iso_fopen(void);
Byte    iso_fgetc(void);
bit     iso_feof(void);
bit     iso_feob(void);
void    iso_fclose(void);
char    pdata  *iso_get_name(void);
bit     iso_fseek(Int16);
Uint32  iso_file_get_pos (void);
void    iso_save_file_pos (void);
bit     iso_fseek_abs (Uint32 offset);
bit     iso_get_directory(Byte,bit);   
#define iso_goto_subdir(id)  iso_get_directory(id, FALSE) 
bit     iso_goto_last (void);


#endif /*_ISO9660_*/
