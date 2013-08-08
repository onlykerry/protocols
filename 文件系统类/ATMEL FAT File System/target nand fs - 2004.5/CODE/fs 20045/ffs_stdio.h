/***********************************************************************/
/*                                                                     */
/*   Module:  ffs_stdio.h                                              */
/*   Release: 2004.5                                                   */
/*   Version: 2004.0                                                   */
/*   Purpose: Name mangled version of stdio to be used by applications */
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
#ifndef _FFS_STDIO_H /* Don't include this file more than once */
#define _FFS_STDIO_H
#include <stdarg.h>

#ifdef __cplusplus
extern "C"
{
#endif

/***********************************************************************/
/* Type Definitions                                                    */
/***********************************************************************/
#define ui32        unsigned int

#ifndef _FILE_FFS
#define _FILE_FFS
typedef struct file FILE_FFS;
struct file;
#endif

#ifndef _FPOS_T_FFS
#define _FPOS_T_FFS
struct pos
{
  ui32 sect_off;
  ui32 sector;
  ui32 offset;
};
typedef struct pos fpos_tFFS;
#endif

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
/*
** Operations on files
*/
int removeFFS(const char *filename);
int renameFFS(const char *old, const char *new_name);

/*
** Conversion between file handle and file ID
*/
int filenoFFS(FILE_FFS *stream);
FILE_FFS *fdopenFFS(int fildes, const char *type);

/*
** File Access Functions
*/
int       fcloseFFS(FILE_FFS *stream);
int       fflushFFS(FILE_FFS *stream);
FILE_FFS *fopenFFS(const char *filename, const char *mode);
FILE_FFS *freopenFFS(const char *filename, const char *mode,
                     FILE_FFS *stream);
void      setbufFFS(FILE_FFS *stream, char *buf);
int       setvbufFFS(FILE_FFS *stream, char *buf, int mode, size_t size);

/*
** Formatted Input/Output Functions
*/
int fprintfFFS(FILE_FFS *stream, const char *format, ...);
int fscanfFFS(FILE_FFS *stream, const char *format, ...);
int sprintfFFS(char *s, const char *format, ...);
int sscanfFFS(const char *s, const char *format, ...);
int vfprintfFFS(FILE_FFS *stream, const char *format, va_list arg);
int vsprintfFFS(char *s, const char *format, va_list arg);

/*
** Character Input/Output Functions
*/
int   fgetcFFS(FILE_FFS *stream);
char *fgetsFFS(char *s, int n, FILE_FFS *stream);
int   fputcFFS(int c, FILE_FFS *stream);
int   fputsFFS(const char *s, FILE_FFS *stream);

int getcFFS(FILE_FFS *stream);                    /* both a function */
#define getcFFS(stream) fgetcFFS(stream)          /* and a macro */

int putcFFS(int c, FILE_FFS *stream);             /* both a function */
#define putcFFS(c, stream) fputcFFS(c, stream)    /* and a macro */

int ungetcFFS(int c, FILE_FFS *stream);

/*
** Direct Input/Output Functions
*/
size_t freadFFS(void *ptr, size_t size, size_t nmemb, FILE_FFS *stream);
size_t fwriteFFS(const void *ptr, size_t size, size_t nmemb,
                 FILE_FFS *stream);

/*
** File Positioning Functions
*/
int  fgetposFFS(FILE_FFS *stream, fpos_tFFS *pos);
int  fseekFFS(FILE_FFS *stream, long offset, int whence);
int  fsetposFFS(FILE_FFS *stream, const fpos_tFFS *pos);
long ftellFFS(FILE_FFS *stream);
void rewindFFS(FILE_FFS *stream);

/*
** Error-handling Functions
*/
void clearerrFFS(FILE_FFS *stream);
int  feofFFS(FILE_FFS *stream);
int  ferrorFFS(FILE_FFS *stream);

/*
** Temporary File Functions
*/
FILE_FFS *tmpfileFFS(void);
char     *tmpnamFFS(char *s);

#ifdef __cplusplus
}
#endif

#endif /* _FFS_STDIO_H */

