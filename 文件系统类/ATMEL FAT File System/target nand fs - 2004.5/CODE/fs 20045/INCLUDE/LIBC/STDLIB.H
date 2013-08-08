/***********************************************************************/
/*                                                                     */
/*   Module:  stdlib.h                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2004.0                                                   */
/*   Purpose: Declares four types and several functions of general     */
/*            utility and defines several symbols                      */
/*                                                                     */
/***********************************************************************/
#ifndef _STDLIB_H
#define _STDLIB_H

#ifdef __cplusplus
extern "C"
{
#endif

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
#ifndef NULL
#define NULL            (void *)0
#endif
#define EXIT_FAILURE    1
#define EXIT_SUCCESS    0
#define RAND_MAX        32767
#define MB_CUR_MAX      1

/***********************************************************************/
/* Type Definitions                                                    */
/***********************************************************************/
#define size_t      unsigned int
#define i32         int

typedef struct
{
  int  rem;
  int  quot;
} div_t;

typedef struct
{
  i32 rem;
  i32 quot;
} ldiv_t;

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
/*
** String Conversion Functions (not provided)
*/
double atof(const char *nptr);
int    atoi(const char *nptr);
long   atol(const char *nptr);
double strtod(const char *nptr, char **endptr);
long   strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);

/*
** Random Number Generator Functions
*/
int  rand(void);
void srand(unsigned int seed);

/*
** Memory Management Functions
*/
void *calloc(size_t nmemb, size_t size);
void  free(void *ptr);
void *malloc(size_t size);
void *realloc(void *ptr, size_t size);

/*
** Communication with the environment (not provided)
*/
void  abort(void);
int   atexit(void(*func)(void));
void  exit(int status);
char *getenv(const char *name);
int   system(const char *string);

/*
** Searching and sorting utilities (not provided)
*/
void *bsearch(const void *key, const void *base, size_t nmemb,
              size_t size, int (*compar)(const void*, const void*));
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void*, const void*));

/*
** Integer Arithmetic Functions (not provided)
*/
int    abs(int j);
div_t  div(int numer, int denom);
long   labs(long j);
ldiv_t ldiv(long numer, long denom);

/*
** Multibyte Character Functions (not provided)
*/
int mblen(const char *s, size_t n);
#if 0
int mbtowc(wchar_t *pwc, const char *s, size_t n);
int wctomb(char *s, wchar_t wchar);

/*
** Multibyte String Functions (not provided)
*/
size_t mbstowcs(wchar_t *pwcs, const char *s, size_t n);
size_t wcstombs(char *s, const wchar_t *pwcs, size_t n);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _STDLIB_H */

