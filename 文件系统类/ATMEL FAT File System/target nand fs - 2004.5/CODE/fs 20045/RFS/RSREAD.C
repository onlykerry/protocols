/***********************************************************************/
/*                                                                     */
/*   Module:  rsread.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2003.0                                                   */
/*   Purpose: Implements the function for reading bytes from a file    */
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
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*     RamRead: Read len number of bytes from file                     */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              buf    = buffer to read data into                      */
/*              len    = number of bytes to be read                    */
/*                                                                     */
/*     Returns: Character read from file on success, -1 on failure     */
/*                                                                     */
/***********************************************************************/
int RamRead(FILE *stream, ui8 *buf, ui32 len)
{
  RFIL_T *link_ptr = &((RFSEnt *)stream->handle)->entry.file;
  int  unfinished = FALSE;
  ui8 *sector;
  ui32 i, remaining = len, num_sectors, space_left;
  ui32 full_len = RAM_SECT_SZ;

  /*-------------------------------------------------------------------*/
  /* If the file mode does not permit reading, return error.           */
  /*-------------------------------------------------------------------*/
  if (!(link_ptr->comm->open_mode & F_READ))
  {
    set_errno(EACCES);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* If len is 0 or file size is 0, stop.                              */
  /*-------------------------------------------------------------------*/
  if (len == 0 || link_ptr->comm->size == 0)
    return 0;

  /*-------------------------------------------------------------------*/
  /* If curr_ptr is invalid, return zero.                              */
  /*-------------------------------------------------------------------*/
  if (link_ptr->comm->last_sect == NULL ||
      stream->curr_ptr.sector == SEEK_PAST_END ||
      (stream->curr_ptr.sector == (ui32)link_ptr->comm->last_sect) &&
      (stream->curr_ptr.offset >= link_ptr->comm->one_past_last))
    return 0;

  /*-------------------------------------------------------------------*/
  /* Read what's left from current sector.                             */
  /*-------------------------------------------------------------------*/
  sector = ((RamSect *)stream->curr_ptr.sector)->data;

  /*-------------------------------------------------------------------*/
  /* Figure out how much space is left in current sector.              */
  /*-------------------------------------------------------------------*/
  space_left = (ui32)(RAM_SECT_SZ - stream->curr_ptr.offset);

  /*-------------------------------------------------------------------*/
  /* If what has to be read comes only from this sector, read it and   */
  /* set remaining to 0 so that we're done.                            */
  /*-------------------------------------------------------------------*/
  if (space_left >= remaining)
  {
    /*-----------------------------------------------------------------*/
    /* If we're trying to read from past the end of the file, set      */
    /* unfinished flag to true and read as much as possible.           */
    /*-----------------------------------------------------------------*/
    if (link_ptr->comm->last_sect == (void *)stream->curr_ptr.sector &&
        link_ptr->comm->one_past_last < stream->curr_ptr.offset +
        (int)remaining)
    {
      unfinished = TRUE;
      remaining = link_ptr->comm->one_past_last- stream->curr_ptr.offset;
      len = remaining;
    }

    /*-----------------------------------------------------------------*/
    /* Use memcpy to copy from sector to buf, remaining bytes.         */
    /*-----------------------------------------------------------------*/
    memcpy(buf, &sector[stream->curr_ptr.offset], remaining);

    /*-----------------------------------------------------------------*/
    /* Adjust current offset.                                          */
    /*-----------------------------------------------------------------*/
    stream->curr_ptr.offset += (ui16)remaining;

    /*-----------------------------------------------------------------*/
    /* We're done reading so return.                                   */
    /*-----------------------------------------------------------------*/
    goto read_exit;
  }

  /*-------------------------------------------------------------------*/
  /* Else read what's left of this sector, adjust remaining and later  */
  /* continue reading the other sectors.                               */
  /*-------------------------------------------------------------------*/
  else
  {
    /*-----------------------------------------------------------------*/
    /* If we're trying to read from past the end of the file, read as  */
    /* much as possible before returning.                              */
    /*-----------------------------------------------------------------*/
    if (stream->curr_ptr.sector == (ui32)link_ptr->comm->last_sect)
    {
      /*---------------------------------------------------------------*/
      /* If we don't have to the end of the sector, adjust how many    */
      /* more bytes are available for read.                            */
      /*---------------------------------------------------------------*/
      space_left = link_ptr->comm->one_past_last-stream->curr_ptr.offset;
      len = space_left;
    }

    /*-----------------------------------------------------------------*/
    /* Use memcpy to copy from sector to buf space_left bytes.         */
    /*-----------------------------------------------------------------*/
    memcpy(buf, &sector[stream->curr_ptr.offset], space_left);

    /*-----------------------------------------------------------------*/
    /* Adjust remaining (number of bytes left to be read), and the buf */
    /* pointer.                                                        */
    /*-----------------------------------------------------------------*/
    remaining -= space_left;
    buf += space_left;

    /*-----------------------------------------------------------------*/
    /* If this is the last sector, we have to stop, else get next      */
    /* sector to read from.                                            */
    /*-----------------------------------------------------------------*/
    if (stream->curr_ptr.sector == (ui32)link_ptr->comm->last_sect)
    {
      stream->curr_ptr.offset = link_ptr->comm->one_past_last;
      unfinished = TRUE;
      goto read_exit;
    }
    else
    {
      stream->curr_ptr.sector =
                        (ui32)((RamSect *)stream->curr_ptr.sector)->next;
      ++stream->curr_ptr.sect_off;
      stream->curr_ptr.offset = 0;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Figure out how many sectors need to be read.                      */
  /*-------------------------------------------------------------------*/
  num_sectors = (remaining + RAM_SECT_SZ - 1) / RAM_SECT_SZ;

  /*-------------------------------------------------------------------*/
  /* Loop through number of sectors, reading them in one at a time.    */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < num_sectors; ++i)
  {
    sector = ((RamSect *)stream->curr_ptr.sector)->data;

    /*-----------------------------------------------------------------*/
    /* If what's left to be read is from this sector, it's the last    */
    /* sector to be read (maybe only partially).                       */
    /*-----------------------------------------------------------------*/
    if (remaining <= RAM_SECT_SZ)
    {
      /*---------------------------------------------------------------*/
      /* If we're trying to read from past the end of the file, set    */
      /* unfinished flag and read as much as possible.                 */
      /*---------------------------------------------------------------*/
      if (link_ptr->comm->last_sect == (void *)stream->curr_ptr.sector &&
          link_ptr->comm->one_past_last < (int)remaining)
      {
        unfinished = TRUE;
        len -= (remaining - link_ptr->comm->one_past_last);
        remaining = link_ptr->comm->one_past_last;
      }

      /*---------------------------------------------------------------*/
      /* Use memcpy to read everything that's left from this sector.   */
      /*---------------------------------------------------------------*/
      memcpy(buf, sector, remaining);

      /*---------------------------------------------------------------*/
      /* Adjust current pointer.                                       */
      /*---------------------------------------------------------------*/
      if (remaining == RAM_SECT_SZ)
      {
        /*-------------------------------------------------------------*/
        /* Move to next sector in file if possible.                    */
        /*-------------------------------------------------------------*/
        if (link_ptr->comm->last_sect == (void *)stream->curr_ptr.sector)
          stream->curr_ptr.offset = RAM_SECT_SZ;
        else
        {
          stream->curr_ptr.sector =
                        (ui32)((RamSect *)stream->curr_ptr.sector)->next;
          ++stream->curr_ptr.sect_off;
          stream->curr_ptr.offset = 0;
        }
      }
      else
        stream->curr_ptr.offset = (ui16)remaining;

      /*---------------------------------------------------------------*/
      /* We're done reading so return.                                 */
      /*---------------------------------------------------------------*/
      goto read_exit;
    }

    /*-----------------------------------------------------------------*/
    /* Else more than one sector remains to be read.                   */
    /*-----------------------------------------------------------------*/
    else
    {
      /*---------------------------------------------------------------*/
      /* If we're trying to read from past the end of the file, set    */
      /* unfinished flag and read as much as possible before returning.*/
      /*---------------------------------------------------------------*/
      if (link_ptr->comm->last_sect == (void *)stream->curr_ptr.sector)
      {
        unfinished = TRUE;
        full_len = link_ptr->comm->one_past_last;
      }

      /*---------------------------------------------------------------*/
      /* Use memcpy to read the whole sector.                          */
      /*---------------------------------------------------------------*/
      memcpy(buf, sector, full_len);

      /*---------------------------------------------------------------*/
      /* Update remaining and buf.                                     */
      /*---------------------------------------------------------------*/
      remaining -= full_len;
      buf += full_len;

      /*---------------------------------------------------------------*/
      /* If no more sectors stop, else go to next sector in file.      */
      /*---------------------------------------------------------------*/
      if (unfinished)
      {
        stream->curr_ptr.sector = (ui32)link_ptr->comm->last_sect;
        stream->curr_ptr.offset = link_ptr->comm->one_past_last;
      }
      else
      {
        stream->curr_ptr.sector =
                        (ui32)((RamSect *)stream->curr_ptr.sector)->next;
        ++stream->curr_ptr.sect_off;
        stream->curr_ptr.offset = 0;
      }

      /*---------------------------------------------------------------*/
      /* If there's no more data in file, go to exit.                  */
      /*---------------------------------------------------------------*/
      if (unfinished)
      {
        len -= remaining;
        goto read_exit;
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* Before returning, mark access time and, if read could not be      */
  /* finished, free current sector.                                    */
  /*-------------------------------------------------------------------*/
read_exit:
  link_ptr->comm->ac_time = OsSecCount;
  return (int)len;
}
#endif /* NUM_RFS_VOLS */

