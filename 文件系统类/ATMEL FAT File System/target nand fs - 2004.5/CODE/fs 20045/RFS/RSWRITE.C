/***********************************************************************/
/*                                                                     */
/*   Module:  rswrite.c                                                */
/*   Release: 2004.5                                                   */
/*   Version: 2004.0                                                   */
/*   Purpose: Implements the function for writing bytes to a file      */
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
/*    RamWrite: Write len number of bytes to file                      */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              buf = buffer holding bytes to be written               */
/*              len = number of bytes to be written                    */
/*                                                                     */
/*     Returns: Number of bytes successfully written, -1 on error      */
/*                                                                     */
/***********************************************************************/
int RamWrite(FILE *stream, const ui8 *buf, ui32 len)
{
  int  adjust_size;
  ui32 remaining = len, num_sectors, i, space_left;
  ui8  *sector;
  RFIL_T *link_ptr = &((RFSEnt *)stream->handle)->entry.file;

  /*-------------------------------------------------------------------*/
  /* If the file mode does not permit writing, return error.           */
  /*-------------------------------------------------------------------*/
  if ((link_ptr->comm->open_mode & (F_APPEND | F_WRITE)) == 0)
  {
    set_errno(EACCES);
    return -1;
  }

  /*-------------------------------------------------------------------*/
  /* If file is in append mode, writes are always at the end.          */
  /*-------------------------------------------------------------------*/
  if (link_ptr->comm->open_mode & F_APPEND)
    RamPoint2End(&stream->curr_ptr, link_ptr->comm);

  /*-------------------------------------------------------------------*/
  /* Else if a seek past end was performed, validate it.               */
  /*-------------------------------------------------------------------*/
  else if (stream->curr_ptr.sector == SEEK_PAST_END)
  {
    if (RamSeekPastEnd((i32)(stream->curr_ptr.sect_off * RAM_SECT_SZ +
                             stream->curr_ptr.offset -
                             link_ptr->comm->size), stream))
      return -1;
    RamPoint2End(&stream->curr_ptr, link_ptr->comm);
  }

  /*-------------------------------------------------------------------*/
  /* If file is not empty, first write into current sector.            */
  /*-------------------------------------------------------------------*/
  if (link_ptr->comm->size)
  {
    sector = ((RamSect *)stream->curr_ptr.sector)->data;

    /*-----------------------------------------------------------------*/
    /* Figure out how much space is left in current sector.            */
    /*-----------------------------------------------------------------*/
    space_left = (ui32)(RAM_SECT_SZ - stream->curr_ptr.offset);

    /*-----------------------------------------------------------------*/
    /* If what has to be written fits in this sector, write it and     */
    /* set remaining to 0 so that we're done.                          */
    /*-----------------------------------------------------------------*/
    if (space_left >= remaining)
    {
      memcpy(&sector[stream->curr_ptr.offset], buf, remaining);

      /*---------------------------------------------------------------*/
      /* Adjust current offset.                                        */
      /*---------------------------------------------------------------*/
      stream->curr_ptr.offset += (ui16)remaining;

      /*---------------------------------------------------------------*/
      /* Adjust remaining to 0 because we're done writing everything.  */
      /*---------------------------------------------------------------*/
      remaining = 0;

      /*---------------------------------------------------------------*/
      /* If one past last needs to be updated, update it.              */
      /*---------------------------------------------------------------*/
      if (link_ptr->comm->last_sect ==
          (RamSect *)stream->curr_ptr.sector &&
          link_ptr->comm->one_past_last < stream->curr_ptr.offset)
      {
        /*-------------------------------------------------------------*/
        /* Adjust file size.                                           */
        /*-------------------------------------------------------------*/
        link_ptr->comm->size += (stream->curr_ptr.offset -
                                 link_ptr->comm->one_past_last);
        link_ptr->comm->one_past_last = stream->curr_ptr.offset;
      }
    }

    /*-----------------------------------------------------------------*/
    /* Else the current sector has not enough room for all the bytes   */
    /* that need to be written. Write enough to fill this sector,      */
    /* adjust remaining and later continue writing sectors.            */
    /*-----------------------------------------------------------------*/
    else if (space_left)
    {
      /*---------------------------------------------------------------*/
      /* Use memcpy to copy from buf to sector space_left bytes.       */
      /*---------------------------------------------------------------*/
      memcpy(&sector[stream->curr_ptr.offset], buf, space_left);

      /*---------------------------------------------------------------*/
      /* Adjust number of bytes left to be written and buf pointer.    */
      /*---------------------------------------------------------------*/
      remaining -= space_left;
      buf += space_left;

      /*---------------------------------------------------------------*/
      /* If this is the last sector, adjust both current sector number */
      /* and one past last sector number.                              */
      /*---------------------------------------------------------------*/
      if ((void *)stream->curr_ptr.sector == link_ptr->comm->last_sect)
      {
        link_ptr->comm->size += (RAM_SECT_SZ -
                                 link_ptr->comm->one_past_last);
        link_ptr->comm->one_past_last = RAM_SECT_SZ;
        stream->curr_ptr.offset += space_left;
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* Figure out how many sectors are needed to write what's left.      */
  /*-------------------------------------------------------------------*/
  num_sectors = (remaining + RAM_SECT_SZ - 1) / RAM_SECT_SZ;

  /*-------------------------------------------------------------------*/
  /* Loop through the number of sectors, either getting existing ones  */
  /* or creating new ones.                                             */
  /*-------------------------------------------------------------------*/
  for (i = 0; i < num_sectors; ++i)
  {
    /*-----------------------------------------------------------------*/
    /* If current pointer is at the end of the file, get new sector.   */
    /*-----------------------------------------------------------------*/
    if ((void *)stream->curr_ptr.sector == link_ptr->comm->last_sect)
    {
      /*---------------------------------------------------------------*/
      /* Get a new sector for the file. Return if unable.              */
      /*---------------------------------------------------------------*/
      if (RamNewSect(link_ptr->comm))
        return len - remaining;

      /*---------------------------------------------------------------*/
      /* Update current pointer.                                       */
      /*---------------------------------------------------------------*/
      stream->curr_ptr.sector = (ui32)link_ptr->comm->last_sect;

      /*---------------------------------------------------------------*/
      /* Set the add size factor to 1 so that file size increases.     */
      /*---------------------------------------------------------------*/
      adjust_size = TRUE;
    }

    /*-----------------------------------------------------------------*/
    /* Else we're writing over old stuff, get the next sector.         */
    /*-----------------------------------------------------------------*/
    else
    {
      stream->curr_ptr.sector =
                        (ui32)((RamSect *)stream->curr_ptr.sector)->next;

      /*---------------------------------------------------------------*/
      /* Set "adjust size" flag only if we go past the end of file.    */
      /*---------------------------------------------------------------*/
      adjust_size = (remaining > link_ptr->comm->one_past_last) &&
          (link_ptr->comm->last_sect == (void *)stream->curr_ptr.sector);
    }

    /*-----------------------------------------------------------------*/
    /* We are moving into a new sector.                                */
    /*-----------------------------------------------------------------*/
    stream->curr_ptr.offset = 0;
    if (link_ptr->comm->size)
      ++stream->curr_ptr.sect_off;

    /*-----------------------------------------------------------------*/
    /* Get the pointer to the sector data.                             */
    /*-----------------------------------------------------------------*/
    sector = ((RamSect *)stream->curr_ptr.sector)->data;

    /*-----------------------------------------------------------------*/
    /* If what's left fits into this sector, it's the last sector to   */
    /* be written (maybe only partially).                              */
    /*-----------------------------------------------------------------*/
    if (remaining <= RAM_SECT_SZ)
    {
      /*---------------------------------------------------------------*/
      /* Use memcpy to copy everything that's left into current sect.  */
      /*---------------------------------------------------------------*/
      memcpy(sector, buf, remaining);

      /*---------------------------------------------------------------*/
      /* Adjust current pointer.                                       */
      /*---------------------------------------------------------------*/
      stream->curr_ptr.offset = remaining;

      /*---------------------------------------------------------------*/
      /* Check if one past last and size need to be updated.           */
      /*---------------------------------------------------------------*/
      if (adjust_size &&
          (void *)stream->curr_ptr.sector == link_ptr->comm->last_sect)
      {
        /*-------------------------------------------------------------*/
        /* Adjust file size.                                           */
        /*-------------------------------------------------------------*/
        link_ptr->comm->size += (remaining -
                                          link_ptr->comm->one_past_last);
        link_ptr->comm->one_past_last = remaining;
      }
      remaining = 0;
    }

    /*-----------------------------------------------------------------*/
    /* Else have to write the whole sector.                            */
    /*-----------------------------------------------------------------*/
    else
    {
      /*---------------------------------------------------------------*/
      /* Use memcpy to write the whole sector.                         */
      /*---------------------------------------------------------------*/
      memcpy(sector, buf, RAM_SECT_SZ);

      /*---------------------------------------------------------------*/
      /* Update remaining and buf.                                     */
      /*---------------------------------------------------------------*/
      remaining -= RAM_SECT_SZ;
      buf = (void *)((ui32)buf + RAM_SECT_SZ);

      /*---------------------------------------------------------------*/
      /* If at file end, adjust file size and one past last.           */
      /*---------------------------------------------------------------*/
      if ((void *)stream->curr_ptr.sector == link_ptr->comm->last_sect)
      {
        link_ptr->comm->size += adjust_size * (RAM_SECT_SZ -
                                          link_ptr->comm->one_past_last);
        link_ptr->comm->one_past_last = RAM_SECT_SZ;
      }

      /*---------------------------------------------------------------*/
      /* Update current pointer offset.                                */
      /*---------------------------------------------------------------*/
      stream->curr_ptr.offset = RAM_SECT_SZ;
    }
  }

  /*-------------------------------------------------------------------*/
  /* Set access and modification times and return.                     */
  /*-------------------------------------------------------------------*/
  link_ptr->comm->mod_time = link_ptr->comm->ac_time = OsSecCount;
  stream->old_ptr = stream->curr_ptr;
  return (int)(len - remaining);
}
#endif /* NUM_RFS_VOLS */

