/***********************************************************************/
/*                                                                     */
/*   Module:  time.h                                                   */
/*   Release: 2004.5                                                   */
/*   Purpose: Declares several functions that help manipulate times    */
/*                                                                     */
/***********************************************************************/
#ifndef _TIME_H
#define _TIME_H

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
#define CLOCKS_PER_SEC  OsTicksPerSec
#define ui32            unsigned int
#define size_t          unsigned int
#ifndef _TIME_T
#define _TIME_T
#define time_t          ui32
#endif

/***********************************************************************/
/* Type Definitions                                                    */
/***********************************************************************/
struct tm
{
  int tm_sec;       /* seconds after the minute (from 0) */
  int tm_min;       /* minutes after the hour (from 0) */
  int tm_hour;      /* hour of the day (from 0) */
  int tm_mday;      /* day of the month (from 1) */
  int tm_mon;       /* month of the year (from 0) */
  int tm_year;      /* years since 1900 (from 0) */
  int tm_wday;      /* days since Sunday (from 0) */
  int tm_yday;      /* day of the year (from 0) */
  int tm_isdst;     /* daylight saving time flag */
};
typedef ui32 clock_t;

/***********************************************************************/
/* Data Declarations                                                   */
/***********************************************************************/
extern ui32 OsTicksPerSec;

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
char *asctime(const struct tm *tptr);
clock_t clock(void);
char *ctime(const time_t *tod);
double difftime(time_t t1, time_t t0);
struct tm *gmtime(const time_t *tod);
struct tm *localtime(const time_t *tod);
time_t mktime(struct tm *tptr);
size_t strftime(char *s, size_t n, const char *format,
                const struct tm *tptr);
time_t time(time_t *tod);

#ifdef __cplusplus
}
#endif

#endif /* _TIME_H */

