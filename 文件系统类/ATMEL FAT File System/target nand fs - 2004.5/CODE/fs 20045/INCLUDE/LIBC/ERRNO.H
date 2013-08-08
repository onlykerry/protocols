/***********************************************************************/
/*                                                                     */
/*   Module:  errno.h                                                  */
/*   Release: 2004.5                                                   */
/*   Version: 2002.0                                                   */
/*   Purpose: Defines several symbols relating to reporting error      */
/*            conditions                                               */
/*                                                                     */
/***********************************************************************/
#ifndef _ERRNO_H
#define _ERRNO_H

#ifdef __cplusplus
extern "C"
{
#endif

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
#define EPERM      1  /* no permission to do operation */
#define ENOENT     2  /* a file or directory does not exist */
#define EINTR      4  /* interrupted system call */
#define EIO        5  /* I/O error */
#define ENXIO      6  /* no such device, or device not ready */
#define EBADF      9  /* bad file descriptor */
#define ENOMEM     12 /* out of memory */
#define EACCES     13 /* permission denied */
#define EFAULT     14 /* bad address */
#define EBUSY      16 /* directory in use */
#define EEXIST     17 /* the named file already exists */
#define EXDEV      18 /* improper link */
#define ENOTDIR    20 /* not a directory */
#define EISDIR     21 /* is a directory */
#define EINVAL     22 /* invalid argument */
#define ENFILE     23 /* file table overflow */
#define EMFILE     24 /* too many open files */
#define ENOTTY     25 /* not a terminal device */
#define EFBIG      27 /* file too big */
#define ENOSPC     28 /* device is full */
#define EROFS      30 /* read-only file system */
#define EDOM       33 /* input value to math function not in domain */
#define ERANGE     34 /* output value from math function out of range */
#define ENAMETOOLONG 38 /* filename length exceeds PATH_MAX */
#define ENOTEMPTY  41 /* attempt to delete non-empty dir or CWD */
#define EREADERROR 44 /* failed to read character from file */

#ifdef __cplusplus
}
#endif

#endif /* _ERRNO_H */

