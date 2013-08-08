/***********************************************************************/
/*                                                                     */
/*   Module:  string.h                                                 */
/*   Release: 2004.5                                                   */
/*   Purpose: Defines functions useful for manipulating arrays of char */
/*                                                                     */
/***********************************************************************/
#ifndef _STRING_H
#define _STRING_H

#ifdef __cplusplus
extern "C"
{
#endif

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
#ifndef NULL
#define NULL     (void *)0
#endif

/***********************************************************************/
/* Type Definitions                                                    */
/***********************************************************************/
#define size_t      unsigned int

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
/*
** Copying Functions
*/
void *memcpy(void *dst, const void *src, size_t len);
void *memmove(void *dst, const void *src, size_t n);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);

/*
** Concatenation Functions
*/
char *strcat(char *s1, const char *s2);
char *strncat(char *s1, const char *s2, size_t n);

/*
** Comparison Functions
*/
int memcmp(const void *s1, const void *s2, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
size_t strxfrm(char *s1, const char *s2, size_t n);  /* not provided */
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);

/*
** Search Functions
*/
void *memchr(const void *s, int c, size_t n);
char *strchr(const char *s, int c);
size_t strcspn(const char *s1, const char *s2);
char *strpbrk(const char *s1, const char *s2);
char *strrchr(const char *s, int c);
size_t strspn(const char *s1, const char *s2);
char *strstr(const char *s1, const char *s2);
char *strtok(char *s1, const char *s2);

/*
** Miscellaneous Functions
*/
void *memset(void *s, int c, size_t n);
char *strerror(int errnum);
size_t strlen(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* _STRING_H */

