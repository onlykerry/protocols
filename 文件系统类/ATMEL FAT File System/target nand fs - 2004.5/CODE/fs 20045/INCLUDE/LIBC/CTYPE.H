/***********************************************************************/
/*                                                                     */
/*   Module:  ctype.h                                                  */
/*   Release: 2004.5                                                   */
/*   Purpose: Declares several functions/macros useful for testing     */
/*            and mapping characters                                   */
/*                                                                     */
/***********************************************************************/
#ifndef _CTYPE_H
#define _CTYPE_H

#ifdef __cplusplus
extern "C"
{
#endif

/***********************************************************************/
/* Function Prototypes                                                 */
/***********************************************************************/
int (isalnum)(int c);
int (isalpha)(int c);
int (iscntrl)(int c);
int (isdigit)(int c);
int (isgraph)(int c);
int (islower)(int c);
int (isprint)(int c);
int (ispunct)(int c);
int (isspace)(int c);
int (isupper)(int c);
int (isxdigit)(int c);
int (tolower)(int c);
int (toupper)(int c);

#ifdef __cplusplus
}
#endif

#endif /* _CTYPE_H */

