/***********************************************************************/
/*                                                                     */
/*   Module:  perm.c                                                   */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Implements the check and set permission functions        */
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
#include "../posix.h"
#include "../include/libc/errno.h"
#include "../include/fsprivate.h"

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/*     SetPerm: Set permissions on a file/dir about to be created      */
/*                                                                     */
/*      Inputs: comm_ptr = pointer to common control block             */
/*              mode =  mode for file/directory                        */
/*                                                                     */
/***********************************************************************/
void SetPerm(FCOM_T *comm_ptr, mode_t mode)
{
  uid_t uid;
  gid_t gid;

  /*-------------------------------------------------------------------*/
  /* Get group and user ID for current process.                        */
  /*-------------------------------------------------------------------*/
  FsGetId(&uid, &gid);

  /*-------------------------------------------------------------------*/
  /* Set the permission mode.                                          */
  /*-------------------------------------------------------------------*/
  comm_ptr->mode = mode;

  /*-------------------------------------------------------------------*/
  /* Set user and group ID.                                            */
  /*-------------------------------------------------------------------*/
  comm_ptr->user_id = uid;
  comm_ptr->group_id = gid;
}

/***********************************************************************/
/*   CheckPerm: Check permissions on existing file or directory        */
/*                                                                     */
/*      Inputs: comm_ptr = pointer to common control block             */
/*              permissions = permissions to check for file/dir        */
/*                                                                     */
/*     Returns: 0 if permissions match, -1 otherwise                   */
/*                                                                     */
/***********************************************************************/
int CheckPerm(FCOM_T *comm_ptr, int permissions)
{
  uid_t uid;
  gid_t gid;

  /*-------------------------------------------------------------------*/
  /* Get group and user ID for current process.                        */
  /*-------------------------------------------------------------------*/
  FsGetId(&uid, &gid);

  /*-------------------------------------------------------------------*/
  /* Check if we need to look at read permissions.                     */
  /*-------------------------------------------------------------------*/
  if (permissions & F_READ)
  {
    /*-----------------------------------------------------------------*/
    /* If other has no read permission, check group.                   */
    /*-----------------------------------------------------------------*/
    if ((comm_ptr->mode & S_IROTH) == FALSE)
    {
      /*---------------------------------------------------------------*/
      /* If not in group or group has no read permission, check owner. */
      /*---------------------------------------------------------------*/
      if ((gid != comm_ptr->group_id) || !(comm_ptr->mode & S_IRGRP))
      {
        /*-------------------------------------------------------------*/
        /* If not user or user has no read permissions, return error.  */
        /*-------------------------------------------------------------*/
        if ((uid != comm_ptr->user_id) || !(comm_ptr->mode & S_IRUSR))
        {
          set_errno(EACCES);
          return -1;
        }
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* Check if we need to look at write permissions.                    */
  /*-------------------------------------------------------------------*/
  if (permissions & F_WRITE)
  {
    /*-----------------------------------------------------------------*/
    /* If other has no write permission, check group.                  */
    /*-----------------------------------------------------------------*/
    if ((comm_ptr->mode & S_IWOTH) == 0)
    {
      /*---------------------------------------------------------------*/
      /* If not in group or group has no write permission, check owner.*/
      /*---------------------------------------------------------------*/
      if ((gid != comm_ptr->group_id) || !(comm_ptr->mode & S_IWGRP))
      {
        /*-------------------------------------------------------------*/
        /* If not user or user has no write permissions, return error. */
        /*-------------------------------------------------------------*/
        if ((uid != comm_ptr->user_id) || !(comm_ptr->mode & S_IWUSR))
        {
          set_errno(EACCES);
          return -1;
        }
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* Check if we need to look at execute permissions.                  */
  /*-------------------------------------------------------------------*/
  if (permissions & F_EXECUTE)
  {
    /*-----------------------------------------------------------------*/
    /* If other has no execute permission, check group.                */
    /*-----------------------------------------------------------------*/
    if ((comm_ptr->mode & S_IXOTH) == 0)
    {
      /*---------------------------------------------------------------*/
      /* If not in group or group can't execute, check owner.          */
      /*---------------------------------------------------------------*/
      if ((gid != comm_ptr->group_id) || !(comm_ptr->mode & S_IXGRP))
      {
        /*-------------------------------------------------------------*/
        /* If not user or user can't execute, return error.            */
        /*-------------------------------------------------------------*/
        if ((uid != comm_ptr->user_id) || !(comm_ptr->mode & S_IXUSR))
        {
          set_errno(EACCES);
          return -1;
        }
      }
    }
  }
  return 0;
}

