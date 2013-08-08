/*C**************************************************************************
* NAME:         iso9660.c
*----------------------------------------------------------------------------
* Copyright (c) 2003 Atmel.
*----------------------------------------------------------------------------
* RELEASE:      snd1c-refd-nf-4_0_3      
* REVISION:     1.3     
*----------------------------------------------------------------------------
* PURPOSE:
* ISO9660 file-system basics functions
* 
* NOTES:
*   Some variables are shared with fat.c module :
*     data  Uint32 fat_ptr_data
*     data  Uint16 fat_fclust_byte_count
*     idata Uint16 fat_dclust_byte_count
*     idata Uint32 fat_dir_current_sect
*     idata Uint16 fat_dir_list_index
*     xdata char   ext[]
*     xdata Byte   fat_buf_sector[]
*   Global variable :
*     pdata Byte   gl_buffer[]
*
*****************************************************************************/

/*_____ I N C L U D E S ____________________________________________________*/

#include "config.h"                         /* system configuration */
#include "..\mem\hard.h"                    /* low level function definition */
#include "file.h"                           /* file function definition */
#include "iso9660.h"                        /* iso9660 file-system definition */


/*_____ M A C R O S ________________________________________________________*/


/*_____ D E F I N I T I O N ________________________________________________*/

/* shared variables with fat.c module */
extern  data  Uint32 fat_ptr_data;
extern  data  Uint16 fat_fclust_byte_count;
extern  idata Uint16 fat_dclust_byte_count;
extern  idata Uint32 fat_dir_current_sect;
extern  xdata Uint16 fat_dir_list_index;   
extern  xdata char   ext[]; 
extern  xdata Byte   fat_buf_sector[];
extern  pdata Byte   gl_buffer[];
extern  xdata Uint32 fat_dir_start_sect;   
extern  xdata Uint16  fat_dir_list_last;    
extern  xdata Byte current_ext;
extern  idata Uint16  fat_dchain_index;
extern  idata Uint16  fat_fchain_nb_clust;  


extern xdata  Byte    fat_fchain_index_save;         
extern xdata  Byte    fat_fchain_nb_clust_save;
extern xdata  Uint16  fat_fclust_byte_count_save;



#define iso_dir_current_sect      fat_dir_current_sect
#define iso_dir_byte_count        fat_dclust_byte_count
#define iso_f_current_sect        fat_ptr_data
#define iso_current_byte_counter  fat_fclust_byte_count
#define iso_current_dir_file      fat_buf_sector
#define iso_file_index            fat_dir_list_index
#define iso_file_max_index        fat_dir_list_last
#define iso_dir_start_sect        fat_dir_start_sect
#define iso_dir_size              fat_dchain_index

#define iso_f_nb_sector_save      fat_fclust_byte_count_save
#define iso_f_nb_byte_save        fat_fchain_nb_clust


extern  char    pdata *lfn_name;                  /* long filename limited to MAX_FILENAME_LEN chars  */
extern  xdata iso_VolumeDescriptor iso_header;    /* iso header informations                          */
extern  xdata iso_cache  iso_file_cache;          /* cache for the current file                       */
extern  idata Uint16  iso_f_nb_sector;
extern  idata Uint16  iso_f_max_sector;
extern  bdata bit     iso_cd;                     /* if set to one cd is iso else cd is joliet format  */

/*_____ D E C L A R A T I O N ______________________________________________*/
Uint16 iso_dgetw(void);




/*F**************************************************************************
* NAME: fat_install
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*   - OK: intallation succeeded
*   - KO: no primary or supplementary volume descriptor found
*         
*----------------------------------------------------------------------------
* PURPOSE:
*   Install the iso file system
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit iso_install(void)
{

  iso_f_current_sect = Hard_iso_read_toc() + 1 + 16;
  iso_cd = 0;

  if (Hard_iso_read_open(iso_f_current_sect) == OK)
  {
    if ((Hard_iso_read_word() & 0xFF) == 0x01)                    /* ISO CD */
    {
      iso_f_current_sect--;
      iso_cd = 1;
    }
    Hard_iso_read_close();
  }
  
  if (iso_read_volume_descriptor(iso_f_current_sect) == OK)   /* read volume descriptor */
    return OK;
  else
    return KO;
}


/*F**************************************************************************
* NAME: iso_read_volume_descriptor
*----------------------------------------------------------------------------
* PARAMS:
*   sector: firt sector of ISO block location
* return:
*   - OK: intallation succeeded
*   - KO: error
*----------------------------------------------------------------------------
* PURPOSE:
*   Read CD volumes descriptors.
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit iso_read_volume_descriptor (Uint32 sector)
{
Byte i;
Uint16 tmp_word;

  /* settting current offset */
  iso_dir_current_sect = sector;
  
  if (Hard_iso_read_open(iso_dir_current_sect) == OK)
  {
    /* Byte 1 - 2 : Volume descriptor type & Standard identifier                          */
    /* Byte 3 - 4 : Standard identifier                                                   */
    /* Byte 5 - 6 : Standard identifier                                                   */
    /* Standard identifier is 'CD001'                                                     */
    tmp_word  = Hard_iso_read_word();
    if ( (((Byte*)&tmp_word)[0] != 0x43) ||   /* check if it is a standard iso cd volume  */
         (Hard_iso_read_word() != 0x3044) ||  /* string 0D                                */
         (Hard_iso_read_word() != 0x3130))    /* string 10                                */
    {
      Hard_iso_read_close();
      return KO;
    }
    
    switch (((Byte*)&tmp_word)[1])
    {
      case TYPE_BOOT_RECORD:                  /* Boot record                              */
      {
        break;
      }

      case TYPE_PRIMARY_VD:                   /* Primary Volume Descriptor or             */
      case TYPE_SUPPLEMENTARY_VD:             /* Supplementary Volume Descriptor          */
      {
        i = 3;
        while (i != 40)
        {
          Hard_iso_read_word();               /* dummy read                               */
          i++;
        }
        
        iso_header.volume_size = Hard_iso_read_word();                /* Byte 81 - 82 */
        iso_header.volume_size += ((Uint32)(Hard_iso_read_word()) << 16);          /* Byte 83 - 84 */
        Hard_iso_read_word();                                         /* Byte 85 - 86 */
        Hard_iso_read_word();                                         /* Byte 87 - 88 */
        i = 44;
        while (i != 64)
        {
          Hard_iso_read_word();               /* dummy read                               */
          i++;
        }
        
        /* Byte 129 - 132 : Logical Block Size - Both byte order                          */
        iso_header.logical_block_size = Hard_iso_read_word();
        Hard_iso_read_word();

        while (i != 76)
        {
          tmp_word = Hard_iso_read_word();    /* dummy read                               */
          i++;
        }
        /* Byte 157 - 190 : Directory record for root directory                           */
        /* Sbyte 1 : Length of directoy record                                            */
        /* Sbyte 2 : Extended attribute record length                                     */ 
        iso_header.root.length = (Hard_iso_read_word() & 0xFF); 
        /* Sbyte 3 - 10 : Location of extent - Both byte order                            */
        iso_header.root.extend_location = Hard_iso_read_word();
        iso_header.root.extend_location += ((Uint32)(Hard_iso_read_word()) << 16);
        Hard_iso_read_word();
        Hard_iso_read_word();
        /* Sbyte 11 - 18 : Data Length - Both Byte order                                  */ 
        iso_header.root.data_length = Hard_iso_read_word();
        iso_header.root.data_length += ((Uint32)(Hard_iso_read_word()) << 16);
        iso_dir_size = iso_header.root.data_length / iso_header.logical_block_size;
        break;
      }
  
      case TYPE_PARTITION_VD:               /* Partition Volume Descriptor                */
      {
        break;
      }
      case TYPE_VOLUME_SET_TERMINATOR:      /* Volume set terminator                      */
      {
        break;
      }

      default:                              /* error                                      */
      {
        return KO;
      }
    }
    Hard_iso_read_close();
    return OK;
  }
  else
    return KO;
}


/*F**************************************************************************
* NAME: iso_fseek
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
*   Seek is done with byte boundary
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit iso_fseek(Int16 offset)
{
Uint16 i;         /* generic counter */
Uint32 file_pos;

  file_pos = ((iso_f_current_sect - iso_file_cache.info.extend_location) * iso_header.logical_block_size) 
             + (Uint32)(iso_current_byte_counter);

  if ((file_pos + offset) < 0)
    return KO;

  file_pos += offset;

  iso_f_nb_sector = (file_pos / iso_header.logical_block_size);
  iso_f_current_sect = (Uint32)(iso_f_nb_sector) + iso_file_cache.info.extend_location;
  iso_current_byte_counter =  ((Uint16)file_pos % iso_header.logical_block_size);

  Hard_iso_read_close();
  Hard_iso_read_open(iso_f_current_sect);
  for (i = 0; i < iso_current_byte_counter; i++)
  {
    Hard_iso_read_byte();
  }
  return OK;
}



/*F**************************************************************************
* NAME: iso_fseek_abs
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
bit iso_fseek_abs(Uint32 offset)
{
Uint16 i;         /* generic counter           */

  iso_f_nb_sector = (Uint16)(offset / iso_header.logical_block_size);
  iso_current_byte_counter = (Uint16)(offset % iso_header.logical_block_size);
  iso_f_current_sect = (Uint32)(iso_f_nb_sector) + iso_file_cache.info.extend_location;
  Hard_iso_read_close();
  Hard_iso_read_open(iso_f_current_sect);
  for (i = 0; i < iso_current_byte_counter; i++)
  {
    Hard_iso_read_byte();
  }
  return OK;
}

/*F**************************************************************************
* NAME: iso_dseek
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
*   
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/ 
bit iso_dseek(Int16 offset)
{
Uint16 i;         /* generic counter */
Uint32 dir_pos;

  dir_pos = ((iso_dir_current_sect - iso_header.root.extend_location) * iso_header.logical_block_size) 
            + iso_dir_byte_count;
  if ((dir_pos + offset) < 0)
    return KO;

  dir_pos += offset;

  iso_dir_current_sect = (dir_pos / iso_header.logical_block_size) 
                        + iso_header.root.extend_location;
  iso_dir_byte_count = dir_pos % iso_header.logical_block_size;

  Hard_iso_read_close();
  Hard_iso_read_open(iso_dir_current_sect);
  for (i = 0; i < iso_dir_byte_count; i++)
  {
    Hard_iso_read_byte();
  }
  return OK;
}


/*F**************************************************************************
* NAME: iso_get_file_dir
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Give information about the directory :
*     - total number of entries
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
void iso_get_file_dir(void)
{
Byte attributes;
Byte len;
Byte i;
Byte j;
Byte k;
Byte entry_len;
Byte tmp_byte;
Byte byte_to_read;
Byte padding_byte;
Byte entry_rel;
Uint16 tmp_word;
bit no_more_entry;
bit end_of_name;


  no_more_entry = FALSE;
  entry_rel = 0;
  fat_dir_list_index = 0;
  do
  {
    padding_byte = 0;
    do
    {
      tmp_word = iso_dgetw();
      padding_byte++;
      if ((iso_dir_current_sect - iso_dir_start_sect) >= iso_dir_size)
      {
        no_more_entry = TRUE;
      }
    }
    while ((tmp_word == 0) && (!no_more_entry));

    if (!no_more_entry)
    {

      /* Byte 1 : length of directory record      */
      /* Byte 2 : Extended attibute record length */
      entry_len = tmp_word & 0xFF;
      byte_to_read = entry_len;
      /* Byte 3 - 10 : location of extent : Logical Block Number of the first Logical Block affected to the file */
      iso_dgetw();                              /* Byte 3 - 4  */
      iso_dgetw();                              /* Byte 5 - 6  */
      iso_dgetw();                              /* Byte 7 - 8  */
      iso_dgetw();                              /* Byte 9 - 10 */
      
      /* Byte 11 - 18 : Data Length : Length of the file section in bytes */
      iso_dgetw();                                  /* Byte 11 - 12 */
      iso_dgetw();                                  /* Byte 13 - 14 */
      iso_dgetw();                                  /* Byte 15 - 16 */
      iso_dgetw();                                  /* Byte 17 - 18 */
      
      /* Byte 19 - 25 : Recording Time and Date */
      iso_dgetw();                                  /* Byte 19 - 20 */
      iso_dgetw();                                  /* Byte 21 - 22 */
      iso_dgetw();                                  /* Byte 23 - 24 */
     
      /* Byte 26 : File flags :
        bit 0 : Hidden file
        bit 1 : Directory entry
        bit 2 : Associated file 
        bit 3 : extend attribute
        bit 4 : extend attribute
        bit 5 : reserved
        bit 6 : reserved
        bit 7 : file has more than one directory record
      */
      attributes = (Byte) (iso_dgetw() >> 8);      /* Byte 25 - 26 */
    
      /* Byte 27 : File Unit Size */
      /* Byte 28 : Interleave Gap */
      iso_dgetw();                                 /* Byte 27 - 28 */
    
      /* Byte 29 - 32 : Volume Sequence Number*/
      iso_dgetw();                                 /* Byte 29 - 30 */
      iso_dgetw();                                 /* Byte 31 - 32 */
      
      /* Byte 33 : Length of File Identifier (LEN_FI) in byte */
      tmp_word = iso_dgetw();                      /* Byte 33 - 34 */
      len = tmp_word & 0xFF;
      
      byte_to_read -= 34;
      byte_to_read >>= 1;
      if (iso_cd == 0)
        len = len >> 1;
      end_of_name = FALSE;

      for (i = 0; i < byte_to_read; i++)
      {
        if (iso_cd)
        {
          if ( (i & 0x01) == 1)                         /* iso data extraction */
          {
            tmp_word = iso_dgetw();
            tmp_byte = ((Byte*)&tmp_word)[1];
          }
          else
          {
            tmp_byte = ((Byte*)&tmp_word)[0];
          }
        }
        else
        {
          tmp_byte = iso_dgetw() & 0xFF;      /* joliet data extraction */
        }
        switch (tmp_byte)
        {
          case 0x2E:                /* SEPARATOR 1                                              */
            j = 0;
            break;
    
          case 0x00:
          case 0x3B:                /* SEPARATOR 2 */
            end_of_name = TRUE;
            k = i;
          break;
    
          default:
          {
            if (j < 3)
            {
              if ((tmp_byte <= 'z') && (tmp_byte >= 'a'))
              {
                tmp_byte = tmp_byte - ('a' - 'A');
              }
              ext[j] = tmp_byte;
              j++;
            }
            break;
          }
        }
  
      }
      iso_file_cache.info.attributes = attributes;
      padding_byte--;
      if ((iso_check_ext() & current_ext) == FILE_XXX)
      {
       entry_rel += (entry_len + padding_byte);
      }
      else
      {
        iso_current_dir_file[fat_dir_list_index] = entry_rel + (padding_byte << 1);
        fat_dir_list_index++;
        entry_rel = entry_len;
      }
    }
  }
  while (no_more_entry == FALSE);
  iso_file_max_index = fat_dir_list_index;
}

/*F**************************************************************************
* NAME: iso_fetch_directory_info
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
void iso_fetch_directory_info (iso_file *entry)
{
Byte i;
Byte len;
Byte j;
Byte k;
Byte tmp_byte;
bdata bit flag; 
Uint16 tmp_word;
Uint16 entry_len;
bdata bit end_of_name;

    tmp_word = Hard_iso_read_word();
    /* Byte 1 : length of directory record      */
    /* Byte 2 : Extended attibute record length */
    entry->entry_len = tmp_word & 0xFF;
    entry_len = entry->entry_len;
    
    /* Byte 3 - 10 : location of extent : Logical Block Number of the first   */
    /* Logical Block affected to the file                                     */
    entry->extend_location =  Hard_iso_read_word();            
    entry->extend_location += (Uint32) ((Uint32)(Hard_iso_read_word()) << 16);  
    Hard_iso_read_word();                                      
    Hard_iso_read_word();                                      
    
    /* Byte 11 - 18 : Data Length : Length of the file section in bytes */
    entry->extend_size = Hard_iso_read_word();                  
    entry->extend_size += ((Uint32) (Hard_iso_read_word())) << 16;              
    Hard_iso_read_word();                                       
    Hard_iso_read_word();                                       

    /* Byte 19 - 25 : Recording Time and Date */
    Hard_iso_read_word();                                       
    Hard_iso_read_word();                                       
    Hard_iso_read_word();                                       
   
    /* Byte 26 : File flags :
      bit 0 : Hidden file
      bit 1 : Directory entry
      bit 2 : Associated file 
      bit 3 : extend attribute
      bit 4 : extend attribute
      bit 5 : reserved
      bit 6 : reserved
      bit 7 : file has more than one directory record
    */
    entry->attributes = (Byte) (Hard_iso_read_word() >> 8);     
  
    /* Byte 27 : File Unit Size */
    /* Byte 28 : Interleave Gap */
    Hard_iso_read_word();                            
  
    /* Byte 29 - 32 : Volume Sequence Number*/
    Hard_iso_read_word();
    Hard_iso_read_word();
    
    /* Byte 33 : Length of File Identifier (LEN_FI) in byte */
    tmp_word = Hard_iso_read_word(); 
    len = tmp_word & 0xFF;
    
    /* clear the name buffer */
    for (i = 0; i < MAX_FILENAME_LEN; i++)
      lfn_name[i] = 0;
    flag = 0;
    end_of_name = FALSE;
    if (iso_cd == 0)
      len = len >> 1;
  
    for (i = 0; i < len; i++)
    {
      if (iso_cd)
      {
        if ( (i & 0x01) == 1)                   /* iso data extraction */
        {
          tmp_word = Hard_iso_read_word();
          tmp_byte = ((Byte*)&tmp_word)[1];
        }
        else
        {
          tmp_byte = ((Byte*)&tmp_word)[0];
        }
      }
      else
      {
        tmp_byte = Hard_iso_read_word() & 0xFF; /* joliet data extraction */
      }
  
      switch (tmp_byte)
      {
        case 0x2E:                /* SEPARATOR 1 */
          j = i + 1;
          lfn_name[i] = tmp_byte;
          break;
  
        case 0x00:
        case 0x3B:                /* SEPARATOR 2 */
          end_of_name = TRUE;
          k = i;
        break;
  
        default:
        {
          if (!end_of_name)
            lfn_name[i] = tmp_byte;
          break;
        }
      }
    }
  
    /* extension : last time we found 0x2E : j position */
    if (end_of_name) i = k;
    if (len < 14)
    {
      for (; i < 14; i++)
      {
        lfn_name[i] = ' ';  /* append spaces for display reason */
      }
    }
    else
    {
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
    lfn_name[i] = '\0';
    for (i = 0; i < 3; i++)
    {
      if (j + i <= len)
      {
        tmp_byte = lfn_name[i + j];
        if ((tmp_byte <= 'z') && (tmp_byte >= 'a'))
        {
          tmp_byte = tmp_byte - ('a' - 'A');
        }
        ext[i] = tmp_byte;
      }
    }
    Hard_iso_read_close();
}


/*F**************************************************************************
* NAME: iso_get_directory
*----------------------------------------------------------------------------
* PARAMS:
*   id: file extension to select
*   root : root directory or sub-directory
* return:
*   - OK: file available
*   - KO: no requested file found
*   - KO: low_level memory error
*----------------------------------------------------------------------------
* PURPOSE:
*   Select first available file/dir in any diretory
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   Fill all the cache information for the first time
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit iso_get_directory(Byte id, bit root)
{
  current_ext = id;
  if (root == TRUE)
  {
    iso_dir_current_sect = iso_header.root.extend_location;
    iso_dir_size = iso_header.root.data_length / iso_header.logical_block_size;
  }
  else
  {
    if ((iso_file_cache.info.attributes & ATTR_ISO_DIR) == ATTR_ISO_DIR)
    {
      iso_dir_current_sect = iso_file_cache.info.extend_location;
      iso_dir_size = iso_file_cache.info.extend_size / iso_header.logical_block_size;
    }
    else
    {
      return KO;
    }
  }
  iso_dir_start_sect = iso_dir_current_sect;
  iso_dir_byte_count = 0;
  Hard_iso_read_close();
  Hard_iso_read_open(iso_dir_current_sect);
  iso_get_file_dir();
  Hard_iso_read_close();

  if (iso_file_max_index == 0)
  {
    iso_goto_parent_dir();
    return KO;
  }
  iso_dir_current_sect = iso_dir_start_sect;
  iso_dir_byte_count = 0;
  Hard_iso_read_open(iso_dir_current_sect);
  iso_dseek(iso_current_dir_file[0]);
  iso_fetch_directory_info(&iso_file_cache.parent);         /* the . dir */

  iso_dseek(iso_current_dir_file[1]);
  iso_fetch_directory_info(&iso_file_cache.parent);         /* the .. dir */

  iso_file_index = 1;

  return iso_goto_next();
}


/*F**************************************************************************
* NAME: iso_goto_first
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
bit iso_goto_first(void)       
{                    
  iso_dir_current_sect = iso_dir_start_sect;
  iso_dir_byte_count = 0;
  Hard_iso_read_open(iso_dir_current_sect);
  iso_dseek(iso_current_dir_file[0]);
  iso_fetch_directory_info(&iso_file_cache.parent); /* the . dir  */

  iso_dseek(iso_current_dir_file[1]);
  iso_fetch_directory_info(&iso_file_cache.parent); /* the .. dir */

  iso_file_index = 1;
  return iso_goto_next();
}


/*F**************************************************************************
* NAME: iso_goto_last
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
bit iso_goto_last (void)
{
Uint16 gl_offset;
Uint16 i;
  
  for (i = iso_file_index + 1, gl_offset = 0; i < fat_dir_list_last; iso_file_index++, i++)
    gl_offset += iso_current_dir_file[i];

  if (iso_dseek(gl_offset) == OK)
  iso_fetch_directory_info(&iso_file_cache.info);
  return OK;
}



/*F**************************************************************************
* NAME: iso_goto_next
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
bit iso_goto_next(void)
{
  if (iso_file_index < (iso_file_max_index - 1))
  {
    iso_file_index++;
    iso_dseek(iso_current_dir_file[iso_file_index]);
    iso_fetch_directory_info(&iso_file_cache.info);
    return OK;
  }
  return KO;
}

/*F**************************************************************************
* NAME: iso_goto_prev
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
bit iso_goto_prev(void)
{
  if (iso_file_index != 2)            /* first file of the directory? */
  {
    iso_dseek((Int16) (-iso_current_dir_file[iso_file_index]));
    iso_file_index--;
    iso_fetch_directory_info(&iso_file_cache.info);
    return OK;
  }
  else
    return KO;  
}



/*F**************************************************************************
* NAME: iso_goto_parent_dir
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*   - OK: there is a selected file in the parent dir
*   - KO: there is not a selected file in the parent dir
*----------------------------------------------------------------------------
* PURPOSE:
*   go to the parent directory (equivalent to a cd ..)
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/ 
bit iso_goto_parent_dir(void)
{
  iso_dir_current_sect = iso_file_cache.parent.extend_location;
  iso_dir_size = iso_file_cache.parent.extend_size / iso_header.logical_block_size;

  iso_dir_start_sect = iso_dir_current_sect;
  iso_dir_byte_count = 0;
  Hard_iso_read_open(iso_dir_current_sect);
  iso_get_file_dir();

  iso_dir_current_sect = iso_dir_start_sect;
  iso_dir_byte_count = 0;
  Hard_iso_read_open(iso_dir_current_sect);
  iso_dseek(iso_current_dir_file[0]);
  iso_fetch_directory_info(&iso_file_cache.parent);         /* the . dir */

  iso_dseek(iso_current_dir_file[1]);
  iso_fetch_directory_info(&iso_file_cache.parent);         /* the .. dir */

  iso_file_index = 1;

  return iso_goto_next();
}



/*F**************************************************************************
* NAME: iso_get_name
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
char pdata * iso_get_name(void)
{
  return (lfn_name);
}


/*F**************************************************************************
* NAME: iso_check_ext
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
Byte iso_check_ext(void)
{
 if ((iso_file_cache.info.attributes & ATTR_ISO_DIR) == ATTR_ISO_DIR)
  {
    return FILE_DIR;
  }
  else
  {
    if ( (ext[0] == 'M')
        && (ext[1] == 'P')
        && (ext[2] == '3'))
    {
      return FILE_MP3;
    }
    else
    if ((ext[0] == 'W')
        && (ext[1] == 'A')
        && (ext[2] == 'V'))
    {
      return FILE_WAV;
    }
    else
    if ((ext[0] == 'S')
        && (ext[1] == 'Y')
        && (ext[2] == 'S'))
    {
      return FILE_SYS;
    }
    else
    {
      return FILE_XXX;
    }
  }
}

/*F**************************************************************************
* NAME: iso_fopen
*----------------------------------------------------------------------------
* PARAMS:
*  
* return:
*   - OK: file opened
*   - KO: file not opened: low level read error
*                          
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
bit iso_fopen(void)
{
  iso_f_current_sect = iso_file_cache.info.extend_location;
  iso_current_byte_counter = 0;
  iso_f_max_sector = (Uint16 )((iso_file_cache.info.extend_size / 2048) + 1);
  iso_f_nb_sector = 0;
  return (Hard_iso_read_open(iso_f_current_sect));
}


/*F**************************************************************************
* NAME: iso_fgetc
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
Byte iso_fgetc(void)
{

  if (((Byte*)&iso_current_byte_counter)[0] == 0x08)
  { 
    iso_f_current_sect++;
    iso_f_nb_sector++;
    iso_current_byte_counter = 0;
  }
  iso_current_byte_counter++;
  return Hard_iso_read_byte();
}


/*F**************************************************************************
* NAME: iso_dgetw
*----------------------------------------------------------------------------
* PARAMS:
*
* return:
*   word read
*----------------------------------------------------------------------------
* PURPOSE:
*   Read one word from directory
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
Uint16 iso_dgetw(void)
{
  if (((Byte*)&iso_dir_byte_count)[0] == 0x08)
  {
    iso_dir_current_sect++;
    iso_dir_byte_count = 2;
    Hard_iso_read_open(iso_dir_current_sect);
  }
  else
  {
    iso_dir_byte_count += 2;
  }
  return Hard_iso_read_word();
}



/*F**************************************************************************
* NAME: iso_feof
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
bit iso_feof(void)
{
  if (iso_f_nb_sector > iso_f_max_sector)
  {
    return OK;
  }
  if (iso_f_nb_sector == iso_f_max_sector)
  {
    if (iso_current_byte_counter >= (iso_file_cache.info.extend_size & 0x7FF) )
      return OK;
  }
  return KO;
}


/*F**************************************************************************
* NAME: iso_feob
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
bit iso_feob(void)
{
  if (iso_f_nb_sector > iso_f_nb_sector_save)
  {
    return OK;
  }
  if (iso_f_nb_sector == iso_f_nb_sector_save)
    if (iso_current_byte_counter > iso_f_nb_byte_save)
      return OK;

  return KO;
}


/*F**************************************************************************
* NAME: iso_fclose
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
void iso_fclose(void)
{
  Hard_iso_read_close();
}



/*F**************************************************************************
* NAME: iso_save_file_pos
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
void iso_save_file_pos(void)
{
  iso_f_nb_sector_save = iso_f_nb_sector;
  iso_f_nb_byte_save = iso_current_byte_counter;
}


/*F**************************************************************************
* NAME: iso_file_get_pos
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
Uint32 iso_file_get_pos(void)
{
  return ( ((Uint32)(iso_f_nb_sector) * 2048) + (Uint32)(iso_current_byte_counter));
}




