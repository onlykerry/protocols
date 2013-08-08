/*H**************************************************************************
* NAME:         file.h
*----------------------------------------------------------------------------
* Copyright (c) 2003 Atmel.
*----------------------------------------------------------------------------
* RELEASE:      snd1c-refd-nf-4_0_3      
* REVISION:     1.15     
*----------------------------------------------------------------------------
* PURPOSE:
* This file contains the file function definition
*****************************************************************************/

#ifndef _FILE_H_
#define _FILE_H_

/*_____ I N C L U D E S ____________________________________________________*/
#include "fs.h"                             /* file system definition */
#include "..\mem\hard.h"                    /* low level function definition */
#if (MEM_CHIP_FS == FS_FAT_32) || (MEM_CARD_FS == FS_FAT_32)
#include "fat32.h"                            /* FAT file-system definition */
#endif
#if (MEM_CHIP_FS == FS_FAT_12_16) || (MEM_CARD_FS == FS_FAT_12_16)
#include "fat.h"                            /* FAT file-system definition */
#endif
#if (MEM_CHIP_FS == FS_ISO) || (MEM_CARD_FS == FS_ISO)
#include "iso9660.h"                          /* ISO 9660 file-system definition */
#endif


/*_____ M A C R O S ________________________________________________________*/

/* File Type */
   /* ! must be defined as power of 2 */
#define FILE_XXX    ((Byte)0)
#define FILE_DIR    ((Byte)1)
#define FILE_SYS    ((Byte)2)
#define FILE_MP3    ((Byte)4)
#define FILE_WAV    ((Byte)8)


/*_____ D E F I N I T I O N ________________________________________________*/


/*_____ D E C L A R A T I O N ______________________________________________*/

bit     file_seek_prev (Byte, bit);
bit     file_seek_next (Byte, bit);
bit     file_entry_dir (Byte);

#if (MEM_CHIP_FS == FS_FAT_12_16) || (MEM_CARD_FS == FS_FAT_12_16) || (MEM_CHIP_FS == FS_FAT_32) || (MEM_CARD_FS == FS_FAT_32)
  #define Fat_fcreate(name, attr)     (bit)fat_fcreate(name, attr)
  #define Fat_fopen(m)                (bit)fat_fopen(m)
  #define Fat_fclose()                (void)fat_fclose()
  #define Fat_fseek(o)                (bit)fat_fseek(o)
  #define Fat_fseek_abs(o)            (void)fat_fseek_abs(o)
  #define Fat_feof()                  (bit)fat_feof()
  #define Fat_feob()                  (bit)fat_feob()
  #define Fat_feod()                  (bit)fat_feof()
  #define Fat_refresh_dir_info(m)     (void)fat_refresh_dir_file_info(m)
  #define Fat_fdelete()               (Byte)fat_fdelete()
  #define Fat_fgetc()                 (Byte)fat_fgetc()
  #define Fat_fputc(d)                (void)fat_fputc(d)
  #define Fat_format()                (void)fat_format()
  #define Fat_file_goto_next()        (bit)fat_goto_next()
  #define Fat_file_goto_prev()        (bit)fat_goto_prev()
  #define Fat_file_goto_last()        (bit)fat_seek_last()
  #define Fat_file_goto_first()       (bit)fat_seek_first()
  #define Fat_file_entry_root(id)     (bit)fat_get_root_directory(id)
  #define Fat_file_goto_parent(id)    (bit)fat_goto_parentdir(id)
  #define Fat_file_goto_child(id)     (bit)fat_goto_subdir(id)
  #define Fat_file_type()             (Byte)fat_check_ext()
  #define Fat_fs_mount()              (bit)fat_install()
  #define Fat_file_get_name()         (char pdata *)fat_get_name()                
  #define Fat_file_clear_name()       (void)fat_clear_file_name()
  #define Fat_fget_pos()              (Uint32)fat_file_get_pos()
  #define Fat_save_cluster_info()     (void)fat_save_cluster_info()
#endif


#if (MEM_CHIP_FS == FS_ISO) || (MEM_CARD_FS == FS_ISO)
  #define Iso_fopen(m)                  (bit)iso_fopen()
  #define Iso_fclose()                  (void)iso_fclose()
  #define Iso_fseek(o)                  (bit)iso_fseek(o)
  #define Iso_fseek_abs(o)              (void)iso_fseek_abs(o)
  
  #define Iso_feof()                    (bit)iso_feof()
  #define Iso_feob()                    (bit)iso_feob()
  #define Iso_feod()                    (bit)iso_feof()
  
  #define Iso_fgetc()                   (Byte)iso_fgetc()
  #define Iso_fputc(d)                  (void)iso_fputc(d)
  #define Iso_format()                  (void)iso_format()
  #define Iso_file_goto_next()          (bit)iso_goto_next()
  #define Iso_file_goto_prev()          (bit)iso_goto_prev()
  #define Iso_file_goto_last()          (bit)iso_goto_last()
  #define Iso_file_goto_first()         (bit)iso_goto_first()
  #define Iso_file_entry_root(id)       (bit)iso_get_directory(id, TRUE)
  #define Iso_file_goto_parent(id)      (bit)iso_goto_parent_dir()
  #define Iso_file_goto_child(id)       (bit)iso_get_directory(id, FALSE)
  #define Iso_file_type()               (Byte)iso_check_ext()
  #define Iso_fs_mount()                (bit)iso_install()
  #define Iso_file_get_name()           (char pdata *)iso_get_name()
  #define Iso_file_clear_name()         (void)fat_clear_file_name()
  #define Iso_file_get_pos()            (Uint32)iso_file_get_pos()
  #define Iso_save_file_pos()           (void)iso_save_file_pos()

#endif

#if (MEM_CHIP_FS == MEM_CARD_FS) || (MEM_CARD_FS == FS_NONE) || (MEM_CHIP_FS == FS_NONE)
  /* If only one fs */
  #if (MEM_CARD_FS == FS_ISO) || (MEM_CHIP_FS == FS_ISO)
    #define Fdelete()                   KO
    #define Frefresh(m)                 
    #define Fcreate(name, attr)         KO
    #define Fputc(d)                    
    #define Fformat()                   
    #define Fopen(m)                    Iso_fopen(m)
    #define Fclose()                    Iso_fclose()
    #define Fseek(o)                    Iso_fseek(o)
    #define Fseek_abs(o)                Iso_fseek_abs(o)
    #define Feof()                      Iso_feof()
    #define Feob()                      Iso_feob()
    #define Feod()                      Iso_feod()
    #define Fgetc()                     Iso_fgetc()
    #define File_goto_next()            Iso_file_goto_next()
    #define File_goto_prev()            Iso_file_goto_prev()
    #define File_goto_last()            Iso_file_goto_last() 
    #define File_goto_first()           Iso_file_goto_first()
    #define File_entry_root(id)         Iso_file_entry_root(id)
    #define File_goto_parent(id)        Iso_file_goto_parent(id)
    #define File_goto_child(id)         Iso_file_goto_child(id)
    #define File_type()                 Iso_file_type()
    #define File_mount()                Iso_fs_mount() 
    #define File_get_name()             Iso_file_get_name()
    #define File_clear_name()           Iso_file_clear_name()
    #define Fget_pos()                  Iso_file_get_pos()
    #define Fsave_cluster_info()        Iso_save_file_pos()
  #else
    #define Fcreate(name, attr)         Fat_fcreate(name, attr)
    #define Frefresh(m)                 Fat_refresh_dir_info(m)
    #define Fdelete()                   Fat_fdelete()
    #define Fopen(m)                    Fat_fopen(m)
    #define Fclose()                    Fat_fclose()
    #define Fseek(o)                    Fat_fseek(o)
    #define Fseek_abs(o)                Fat_fseek_abs(o)
    #define Feof()                      Fat_feof()
    #define Feob()                      Fat_feob()
    #define Feod()                      Fat_feod()
    #define Fgetc()                     Fat_fgetc()
    #define Fputc(d)                    Fat_fputc(d)
    #define Fformat()                   Fat_format()
    #define File_goto_next()            Fat_file_goto_next()
    #define File_goto_prev()            Fat_file_goto_prev()
    #define File_goto_last()            Fat_file_goto_last() 
    #define File_goto_first()           Fat_file_goto_first()
    #define File_entry_root(id)         Fat_file_entry_root(id)
    #define File_goto_parent(id)        Fat_file_goto_parent(id)
    #define File_goto_child(id)         Fat_file_goto_child(id)
    #define File_type()                 Fat_file_type()
    #define File_mount()                Fat_fs_mount() 
    #define File_get_name()             Fat_file_get_name()
    #define File_clear_name()           Fat_file_clear_name()
    #define Fget_pos()                  Fat_fget_pos()
    #define Fsave_cluster_info()        Fat_save_cluster_info()
  #endif
#else
    #define Fcreate(name, attr)         Fat_fcreate(name, attr)
    #define Frefresh(m)                 Fat_refresh_dir_info(m)
    #define Fdelete()                   Fat_fdelete()
    #define Fputc(d)                    Fat_fputc(d)
    #define Fformat()                   Fat_format()
    #define Fopen(m)                    ((fs_memory != FS_ISO) ? Fat_fopen(m) : Iso_fopen(m))
    #define Fclose()                    ((fs_memory != FS_ISO) ? Fat_fclose() : Iso_fclose())
    #define Fseek(o)                    ((fs_memory != FS_ISO) ? Fat_fseek(o) : Iso_fseek(o))
    #define Fseek_abs(o)                ((fs_memory != FS_ISO) ? Fat_fseek_abs(o) : Iso_fseek_abs(o))
    #define Feof()                      ((fs_memory != FS_ISO) ? Fat_feof() : Iso_feof())
    #define Feob()                      ((fs_memory != FS_ISO) ? Fat_feob() : Iso_feob())
    #define Feod()                      ((fs_memory != FS_ISO) ? Fat_feof() : Iso_feof())
    #define Fgetc()                     ((fs_memory != FS_ISO) ? Fat_fgetc() : Iso_fgetc())
    #define File_goto_next()            ((fs_memory != FS_ISO) ? Fat_file_goto_next(): Iso_file_goto_next())
    #define File_goto_prev()            ((fs_memory != FS_ISO) ? Fat_file_goto_prev(): Iso_file_goto_prev())
    #define File_goto_last()            ((fs_memory != FS_ISO) ? Fat_file_goto_last() : Iso_file_goto_last())
    #define File_goto_first()           ((fs_memory != FS_ISO) ? Fat_file_goto_first() : Iso_file_goto_first())
    #define File_entry_root(id)         ((fs_memory != FS_ISO) ? Fat_file_entry_root(id) : Iso_file_entry_root(id))
    #define File_goto_parent(id)        ((fs_memory != FS_ISO) ? Fat_file_goto_parent(id) : Iso_file_goto_parent(id))
    #define File_goto_child(id)         ((fs_memory != FS_ISO) ? Fat_file_goto_child(id) : Iso_file_goto_child(id))
    #define File_type()                 ((fs_memory != FS_ISO) ? Fat_file_type() : Iso_file_type())
    #define File_mount()                ((fs_memory != FS_ISO) ? Fat_fs_mount() : Iso_fs_mount())
    #define File_get_name()             ((fs_memory != FS_ISO) ? Fat_file_get_name() : Iso_file_get_name())                
#define File_clear_name()           (void)fat_clear_file_name()
    #define Fget_pos()                  ((fs_memory != FS_ISO) ? (Uint32)fat_file_get_pos() : Iso_file_get_pos())
    #define Fsave_cluster_info()        ((fs_memory != FS_ISO) ? (void)fat_save_cluster_info() : Iso_save_file_pos())
#endif

#endif  /* _FILE_H_ */

