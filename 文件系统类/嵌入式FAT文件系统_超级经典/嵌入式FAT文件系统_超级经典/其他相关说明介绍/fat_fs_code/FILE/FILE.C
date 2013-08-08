/*C**************************************************************************
* NAME:         file.c
*----------------------------------------------------------------------------
* Copyright (c) 2003 Atmel.
*----------------------------------------------------------------------------
* RELEASE:      snd1c-refd-nf-4_0_3      
* REVISION:     1.10     
*----------------------------------------------------------------------------
* PURPOSE:
* This file contains extention routines to the file system
*****************************************************************************/

/*_____ I N C L U D E S ____________________________________________________*/

#include "config.h"                         /* system configuration   */
#include "modules\display\disp_task.h"      /* display definition */
#include "file.h"                           /* file system definition */

/*_____ M A C R O S ________________________________________________________*/

#define F_SEEK_TIME ((Byte)4)

extern  bit     fs_memory;          /* selected file system */

/*_____ D E F I N I T I O N ________________________________________________*/


/*_____ D E C L A R A T I O N ______________________________________________*/


/*F**************************************************************************
* NAME: file_seek_prev
*----------------------------------------------------------------------------
* PARAMS:
*   id: file type identifier
*   loop: loop to the last file
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Select previous file with specified extension
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*   Depending on the time played, this function selects the previous file
*   or restarts the file under playing.
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit file_seek_prev (Byte id, bit loop)
{
  if ((disp_get_sec() < F_SEEK_TIME) && (disp_get_min() == 0))
  {
    while (File_goto_prev() == OK)
    {
      /* a file or a directory exists */
      if ((File_type() & id) != 0)            /* specified file type found */
      { 
        return TRUE;                          /* exit when found */
      }
    }
    /* beginning of dir */
    if (File_goto_last() == OK)               /* goto to the end of dir */
    {
      if (loop)                               /* loop = false when ask previous and play */
      {
        do
        {
          if ((File_type() & id) != 0)          /* look for a specified file */
          {                                     /* specified file type found */
            return TRUE;                        /* exit when found */
          }
        }
        while (File_goto_prev() == OK);
        return FALSE;
      }
      else                                    /* no loop */
      {
        return FALSE;                         /* Stop at the last */
      }
    }
    else
      return FALSE;
  }
  return TRUE;
}


/*F**************************************************************************
* NAME: file_seek_next
*----------------------------------------------------------------------------
* PARAMS:
*   id:   file type identifier
*   loop: loop to the first file
*
* return:
*----------------------------------------------------------------------------
* PURPOSE:
*   Select next file with specified extension
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit file_seek_next (Byte id, bit loop)
{
  
  while (File_goto_next() == OK)
  {
    /* file or dir found */
    if ((File_type() & id) != 0)
    { /* specified file type found */
      return TRUE;                          /* exit when found */
    }
  }
                                            /* end of dir */
  if (File_goto_first() == OK)              /* goto beginning of the dir */
  {
    if (loop)                               /* loop ? */
    {                                       /* re-start at the beginning */
      do
      {
        if ((File_type() & id) != 0)        /* look for a specified file */
        {                                   /* specified file type found */
          return TRUE;                      /* exit when found */
        }
      }
      while (File_goto_next() == OK);      
      return FALSE;                         /* no specified file in the dir */
    }
    else                                    /* no loop */
    {
      return FALSE;                         /* Stop at the beginning */
    }
  }
  else
    return FALSE;
}


/*F**************************************************************************
* NAME: file_entry_dir
*----------------------------------------------------------------------------
* PARAMS:
*   id: file type identifier
*
* return:
*   OK: file type found in dir
*   KO: file type not found in dir
*----------------------------------------------------------------------------
* PURPOSE:
*   Enter a directory
*----------------------------------------------------------------------------
* EXAMPLE:
*----------------------------------------------------------------------------
* NOTE:
*----------------------------------------------------------------------------
* REQUIREMENTS:
*****************************************************************************/
bit file_entry_dir (Byte id)
{
  if (File_type() == FILE_DIR)              /* only for directory! */
  { /* stopped on a directory */
    if (File_goto_child(id) == KO)
    {
      /* no file in dir */
      File_goto_parent(id);
      return KO;
    }
    else
     return OK;
  }
  else
  {
    return KO;
  }
}



