/***********************************************************************/
/*                                                                     */
/*   Module:  fsread.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2003.5                                                   */
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
#include "flashfsp.h"

#if NUM_FFS_VOLS

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*   FlashRead: Read len worth of bytes from a file                    */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              buf    = buffer in which to read the data              */
/*              len    = number of bytes to be read                    */
/*                                                                     */
/*     Returns: Number of bytes succesfully read or -1 if error        */
/*                                                                     */
/***********************************************************************/
int FlashRead(FILE *stream, ui8 *buf, ui32 len)
{
  FFIL_T *link_ptr = &((FFSEnt *)stream->handle)->entry.file;
  int  unfinished = FALSE, status = GET_OK;
  ui32 i, num_sectors, space_left, remaining = len;
  ui32 full_len = Flash->sect_sz;
  ui8 *sector;

  /*-------------------------------------------------------------------*/
  /* If the file mode does not permit reading, return error.           */
  /*-------------------------------------------------------------------*/
  if (!(link_ptr->comm->open_mode & F_READ))
  {
    set_errno(EACCES);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* If len is 0, return 0.                                            */
  /*-------------------------------------------------------------------*/
  if (len == 0)
    return 0;

  /*-------------------------------------------------------------------*/
  /* If curr_ptr is invalid, return zero.                              */
  /*-------------------------------------------------------------------*/
  if (stream->curr_ptr.sector == (ui16)-1 ||
      stream->curr_ptr.sector == FDRTY_SECT ||
      (stream->curr_ptr.sector == link_ptr->comm->one_past_last.sector &&
       stream->curr_ptr.offset == link_ptr->comm->one_past_last.offset))
    return 0;

  /*-------------------------------------------------------------------*/
  /* If we're in the middle of a sector, read what's left from it.     */
  /*-------------------------------------------------------------------*/
  if (stream->curr_ptr.offset)
  {
    /*-----------------------------------------------------------------*/
    /* If the stream does not have a cache entry, get the sector from  */
    /* the cache.                                                      */
    /*-----------------------------------------------------------------*/
    if (stream->cached == NULL)
    {
      status = GetSector(&Flash->cache, (int)stream->curr_ptr.sector,
                         FALSE, link_ptr->comm, &stream->cached);
      if (status == GET_WRITE_ERROR)
        return -1;
    }

    /*-----------------------------------------------------------------*/
    /* Get the pointer to the sector data.                             */
    /*-----------------------------------------------------------------*/
    sector = ((CacheEntry *)stream->cached)->sector;

    /*-----------------------------------------------------------------*/
    /* Figure out how much space is left in current sector.            */
    /*-----------------------------------------------------------------*/
    space_left = (ui32)(Flash->sect_sz - stream->curr_ptr.offset);

    /*-----------------------------------------------------------------*/
    /* If what has to be read comes only from this sector, read it and */
    /* set remaining to 0 so that we're done.                          */
    /*-----------------------------------------------------------------*/
    if (space_left >= remaining)
    {
      /*---------------------------------------------------------------*/
      /* If we're trying to read from past the end of the file, set    */
      /* unfinished flag to true and read as much as possible.         */
      /*---------------------------------------------------------------*/
      if (link_ptr->comm->one_past_last.sector ==
                                               stream->curr_ptr.sector &&
          link_ptr->comm->one_past_last.offset <stream->curr_ptr.offset +
          (int)remaining)
      {
        unfinished = TRUE;
        remaining = (ui32)(link_ptr->comm->one_past_last.offset -
                           stream->curr_ptr.offset);
        len = remaining;
      }

      /*---------------------------------------------------------------*/
      /* Use memcpy to copy from sector to buf, remaining bytes.       */
      /*---------------------------------------------------------------*/
      memcpy(buf, &sector[stream->curr_ptr.offset], remaining);

      /*---------------------------------------------------------------*/
      /* If there was an error reading sector, stop.                   */
      /*---------------------------------------------------------------*/
      if (status == GET_READ_ERROR)
      {
        unfinished = TRUE;
        len = (ui32)-1;
        goto read_exit;
      }

      /*---------------------------------------------------------------*/
      /* Adjust current offset.                                        */
      /*---------------------------------------------------------------*/
      stream->curr_ptr.offset += (ui16)remaining;

      /*---------------------------------------------------------------*/
      /* We're done reading so return.                                 */
      /*---------------------------------------------------------------*/
      goto read_exit;
    }

    /*-----------------------------------------------------------------*/
    /* Else read what's left of this sector, adjust remaining and      */
    /* later, continue reading the other sectors.                      */
    /*-----------------------------------------------------------------*/
    else
    {
      /*---------------------------------------------------------------*/
      /* If we're trying to read from past the end of the file, read   */
      /* as much as possible before returning.                         */
      /*---------------------------------------------------------------*/
      if (stream->curr_ptr.sector == link_ptr->comm->last_sect)
      {
        /*-------------------------------------------------------------*/
        /* If we don't have to the end of the sector, adjust how many  */
        /* more bytes are available for read.                          */
        /*-------------------------------------------------------------*/
        if (link_ptr->comm->one_past_last.sector != (ui16)-1)
          space_left = (ui32)(link_ptr->comm->one_past_last.offset -
                              stream->curr_ptr.offset);
        len = space_left;
      }

      /*---------------------------------------------------------------*/
      /* Use memcpy to copy space_left bytes from sector to buffer.    */
      /*---------------------------------------------------------------*/
      memcpy(buf, &sector[stream->curr_ptr.offset], space_left);

      /*---------------------------------------------------------------*/
      /* If there was an error reading sector, stop.                   */
      /*---------------------------------------------------------------*/
      if (status == GET_READ_ERROR)
      {
        unfinished = TRUE;
        len = (ui32)-1;
        goto read_exit;
      }

      /*---------------------------------------------------------------*/
      /* Adjust remaining (number of bytes left to be read) and the    */
      /* buf pointer.                                                  */
      /*---------------------------------------------------------------*/
      remaining -= space_left;
      buf += space_left;

      /*---------------------------------------------------------------*/
      /* Adjust current offset.                                        */
      /*---------------------------------------------------------------*/
      stream->curr_ptr.offset = 0;

      /*---------------------------------------------------------------*/
      /* If this is the last sector, we have to stop, else get next    */
      /* sector to read from.                                          */
      /*---------------------------------------------------------------*/
      if (stream->curr_ptr.sector == link_ptr->comm->last_sect)
      {
        stream->curr_ptr.sector = link_ptr->comm->one_past_last.sector;
        stream->curr_ptr.offset = link_ptr->comm->one_past_last.offset;
        unfinished = TRUE;
        goto read_exit;
      }
      else
      {
        stream->curr_ptr.sector =
                           Flash->sect_tbl[stream->curr_ptr.sector].next;
        ++stream->curr_ptr.sect_off;
      }

      /*---------------------------------------------------------------*/
      /* Free current sector because we're done reading from it.       */
      /*---------------------------------------------------------------*/
      FreeSector(&stream->cached, &Flash->cache);
    }
  }

  /*-------------------------------------------------------------------*/
  /* Figure out how many sectors need to be read.                      */
  /*-------------------------------------------------------------------*/
  num_sectors = (remaining + Flash->sect_sz - 1) / Flash->sect_sz;

  /*-------------------------------------------------------------------*/
  /* Loop through number of sectors, reading them in one at a time.    */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < num_sectors; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* Get pointer to the cached sector entry.                         */
    /*-----------------------------------------------------------------*/
    status = GetSector(&Flash->cache, (int)stream->curr_ptr.sector,
                       FALSE, link_ptr->comm, &stream->cached);
    if (status == GET_WRITE_ERROR)
    {
      len -= remaining;
      goto read_exit;
    }

    /*-----------------------------------------------------------------*/
    /* Get pointer to the sector data.                                 */
    /*-----------------------------------------------------------------*/
    sector = ((CacheEntry *)stream->cached)->sector;

    /*-----------------------------------------------------------------*/
    /* If what's left to be read is from this sector, it's the last    */
    /* sector to be read (maybe only partially).                       */
    /*-----------------------------------------------------------------*/
    if (remaining <= Flash->sect_sz)
    {
      /*---------------------------------------------------------------*/
      /* If we're trying to read from past the end of the file, set    */
      /* unfinished flag and read as much as possible.                 */
      /*---------------------------------------------------------------*/
      if (link_ptr->comm->one_past_last.sector ==
                                               stream->curr_ptr.sector &&
          link_ptr->comm->one_past_last.offset < (int)remaining)
      {
        unfinished = TRUE;
        len -= (remaining - link_ptr->comm->one_past_last.offset);
        remaining = link_ptr->comm->one_past_last.offset;
      }

      /*---------------------------------------------------------------*/
      /* Use memcpy to read everything that's left from this sector.   */
      /*---------------------------------------------------------------*/
      memcpy(buf, sector, remaining);

      /*---------------------------------------------------------------*/
      /* If there was an error reading sector, stop.                   */
      /*---------------------------------------------------------------*/
      if (status == GET_READ_ERROR)
      {
        unfinished = TRUE;
        len -= remaining;
        goto read_exit;
      }

      /*---------------------------------------------------------------*/
      /* Adjust current pointer.                                       */
      /*---------------------------------------------------------------*/
      if (remaining == Flash->sect_sz)
      {
        stream->curr_ptr.offset = 0;

        /*-------------------------------------------------------------*/
        /* If this is the last sector, set current sector number to    */
        /* invalid (-1), else set it to the next sector in the file.   */
        /*-------------------------------------------------------------*/
        if (link_ptr->comm->last_sect == stream->curr_ptr.sector)
          stream->curr_ptr.sector = (ui16)-1;
        else
        {
          stream->curr_ptr.sector =
                           Flash->sect_tbl[stream->curr_ptr.sector].next;
          ++stream->curr_ptr.sect_off;
        }

        /*-------------------------------------------------------------*/
        /* Free current sector because we're done reading from it.     */
        /*-------------------------------------------------------------*/
        FreeSector(&stream->cached, &Flash->cache);
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
      if (link_ptr->comm->last_sect == stream->curr_ptr.sector)
      {
        unfinished = TRUE;
        if (link_ptr->comm->one_past_last.offset)
          full_len = link_ptr->comm->one_past_last.offset;
      }

      /*---------------------------------------------------------------*/
      /* Use memcpy to read the whole sector.                          */
      /*---------------------------------------------------------------*/
      memcpy(buf, sector, full_len);

      /*---------------------------------------------------------------*/
      /* If there was an error reading sector, stop.                   */
      /*---------------------------------------------------------------*/
      if (status == GET_READ_ERROR)
      {
        unfinished = TRUE;
        len -= remaining;
        goto read_exit;
      }

      /*---------------------------------------------------------------*/
      /* Update remaining and buf.                                     */
      /*---------------------------------------------------------------*/
      remaining -= full_len;
      buf += full_len;

      /*---------------------------------------------------------------*/
      /* Free current sector because we're done reading from it.       */
      /*---------------------------------------------------------------*/
      FreeSector(&stream->cached, &Flash->cache);

      /*---------------------------------------------------------------*/
      /* If no more sectors stop, else go to next sector in file.      */
      /*---------------------------------------------------------------*/
      if (unfinished)
      {
        stream->curr_ptr.sector = link_ptr->comm->one_past_last.sector;
        stream->curr_ptr.offset = link_ptr->comm->one_past_last.offset;
        len -= remaining;
        goto read_exit;
      }
      else
      {
        stream->curr_ptr.sector =
                           Flash->sect_tbl[stream->curr_ptr.sector].next;
        ++stream->curr_ptr.sect_off;
        stream->curr_ptr.offset = 0;
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* Before returning, mark access time and, if read could not be      */
  /* finished, free current sector.                                    */
  /*-------------------------------------------------------------------*/
read_exit:
  link_ptr->comm->ac_time = OsSecCount;
  if (unfinished && stream->cached)
    FreeSector(&stream->cached, &Flash->cache);

  return (int)len;
}
#endif /* NUM_FFS_VOLS */

