/***********************************************************************/
/*                                                                     */
/*   Module:  stdio.h                                                  */
/*   Release: 2004.5                                                   */
/*   Purpose: Declares three types, several macros, and many functions */
/*            for performing input and output                          */
/*                                                                     */
/***********************************************************************/
#ifndef _STDIO_H
#define _STDIO_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdarg.h>

/***********************************************************************/
/* Configuration                                                       */
/***********************************************************************/
#define FOPEN_MAX       64
#define FILENAME_MAX    63
#define L_tmpnam        14
#define TMP_MAX         10000
#define BUFSIZ          256

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
#ifndef NULL
#define NULL            (void *)0
#endif
#define _IOFBF          0
#define _IOLBF          1
#define _IONBF          2
#define EOF             (-1)
#define SEEK_CUR        1
#define SEEK_END        2
#define SEEK_SET        0

/***********************************************************************/
/* Type Definitions                                                    */
/***********************************************************************/
#define ui32       unsigned int
#define size_t     unsigned int

#define FILE FILE_FFS
#ifndef _FILE_FFS
#define _FILE_FFS
typedef struct file FILE;
struct file;
#endif

#define fpos_t fpos_tFFS
#ifndef _FPOS_T_FFS
#define _FPOS_T_FFS
typedef struct
{
  ui32 sect_off;
  ui32 sector;
  ui32 offset;
} fpos_t;
#endif

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
/*
** Operations on files
*/
#define remove removeFFS
int     remove(const char *filename);
#define rename renameFFS
int     rename(const char *old, const char *new_name);

/*
** Conversion between file handle and file ID
*/
#define fileno filenoFFS
int     fileno(FILE *stream);
#define fdopen fdopenFFS
FILE   *fdopen(int fildes, const char *type);

/*
** File Access Functions
*/
#define fclose fcloseFFS
int     fclose(FILE *stream);
#define fflush fflushFFS
int     fflush(FILE *stream);
#define fopen fopenFFS
FILE   *fopen(const char *filename, const char *mode);
#define freopen freopenFFS
FILE   *freopen(const char *filename, const char *mode, FILE *stream);
#define setbuf setbufFFS
void    setbuf(FILE *stream, char *buf);
#define setvbuf setvbufFFS
int     setvbuf(FILE *stream, char *buf, int mode, size_t size);

/*
** Formatted Input/Output Functions
*/
#define fprintf fprintfFFS
int     fprintf(FILE *stream, const char *format, ...);
#define fscanf fscanfFFS
int     fscanf(FILE *stream, const char *format, ...);
int     printf(const char *format, ...);
int     scanf(const char *format, ...);
int     sprintf(char *s, const char *format, ...);
#define snprintf snprintfFFS
int     snprintf(char *s, size_t n, const char *format, ...);
#define sscanf sscanfFFS
int     sscanf(const char *s, const char *format, ...);
#define vfprintf vfprintfFFS
int     vfprintf(FILE *stream, const char *format, va_list arg);
int     vprintf(const char *format, va_list arg);
#define vsprintf vsprintfFFS
int     vsprintf(char *s, const char *format, va_list arg);
#define vsnprintf vsnprintfFFS
int     vsnprintf(char *s, size_t n, const char *format, va_list arg);

/*
** Character Input/Output Functions
*/
#define fgetc fgetcFFS
int     fgetc(FILE *stream);
#define fgets fgetsFFS
char    *fgets(char *s, int n, FILE *stream);
#define fputc fputcFFS
int     fputc(int c, FILE *stream);
#define fputs fputsFFS
int     fputs(const char *s, FILE *stream);
#define getc getcFFS
int     getc(FILE *stream);
int     getchar(void);
char   *gets(char *s);
#define putc putcFFS
int     putc(int c, FILE *stream);
int     putchar(int c);
int     puts(const char *s);
#define ungetc ungetcFFS
int     ungetc(int c, FILE *stream);

/*
** Direct Input/Output Functions
*/
#define fread freadFFS
size_t  fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
#define fwrite fwriteFFS
size_t  fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

/*
** File Positioning Functions
*/
#define fgetpos fgetposFFS
int     fgetpos(FILE *stream, fpos_t *pos);
#define fseek fseekFFS
int     fseek(FILE *stream, long offset, int whence);
#define fsetpos fsetposFFS
int     fsetpos(FILE *stream, const fpos_t *pos);
#define ftell ftellFFS
long    ftell(FILE *stream);
#define rewind rewindFFS
void    rewind(FILE *stream);

/*
** Error-handling Functions
*/
#define clearerr clearerrFFS
void    clearerr(FILE *stream);
#define feof feofFFS
int     feof(FILE *stream);
#define ferror ferrorFFS
int     ferror(FILE *stream);
void perror(const char *s);

/*
** Temporary File Functions
*/
#define tmpfile tmpfileFFS
FILE   *tmpfile(void);
#define tmpnam tmpnamFFS
char   *tmpnam(char *s);

#ifdef __cplusplus
}
#endif

#endif /* _STDIO_H */

