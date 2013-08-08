/***********************************************************************/
/*                                                                     */
/*   Module:  xscanf.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2003.5                                                   */
/*   Purpose: Implements fscanf(), scanf(), sscanf() helper function   */
/*            for integer-only runtime library "runlib.a"              */
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
#include "stdiop.h"

/***********************************************************************/
/* Macro Definitions                                                   */
/***********************************************************************/
#define INT           1
#define SHORT_INT     3
#define LONG_INT      5
#define U_INT         12
#define U_SHORT_INT   14
#define U_LONG_INT    16
#define SIGNED        1

/***********************************************************************/
/* Global Variables                                                    */
/***********************************************************************/
static int UnusedInt;

/***********************************************************************/
/* Local Function Definitions                                          */
/***********************************************************************/

/***********************************************************************/
/*    put_char: Put a character back onto the input stream             */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              index = index into string                              */
/*              c = char to be place back on the input stream          */
/*                                                                     */
/***********************************************************************/
static void put_char(FILE *stream, int *index, char c)
{
  /*-------------------------------------------------------------------*/
  /* If not EOF or full, save in control block.                        */
  /*-------------------------------------------------------------------*/
  if (stream)
  {
    if ((c != EOF) && !stream->hold_char)
      stream->hold_char = c;
  }
  else
    --(*index);
}

/***********************************************************************/
/*    get_char: Read a character from the input                        */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              s = input string                                       */
/*              index = index into string                              */
/*                                                                     */
/*     Returns: the last character read from the input                 */
/*                                                                     */
/***********************************************************************/
static char get_char(FILE *stream, const char *s, int *index)
{
  ui8 rv;

  /*-------------------------------------------------------------------*/
  /* If stream is valid, use it as input, else use string s.           */
  /*-------------------------------------------------------------------*/
  if (stream)
  {
    /*-----------------------------------------------------------------*/
    /* Read either pushed-back character or new input character.       */
    /*-----------------------------------------------------------------*/
    if (stream->hold_char)
    {
      rv = (ui8)stream->hold_char;
      stream->hold_char = 0;
    }
    else
    {
      if (stream->read(stream, &rv, 1) != 1)
        return (char)-1;
    }
  }
  else
  {
    /*-----------------------------------------------------------------*/
    /* Read in character from an input string and increment string.    */
    /*-----------------------------------------------------------------*/
    rv = (ui8)s[*index];
    ++(*index);
  }
  return (char)rv;
}

/***********************************************************************/
/*     get_num: Read a sequence of characters and convert them into a  */
/*              number in the specified base                           */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              s = input string                                       */
/*              index = index into string                              */
/*              c = first char read from input                         */
/*              count = number of chars read in so far                 */
/*              base = base for the conversion                         */
/*              value = value in which the coversion is stored         */
/*              width = maximum number of digits                       */
/*              plus = the sign for value                              */
/*              which_pointer = the type for value                     */
/*                                                                     */
/*     Returns: number new of characters read so far                   */
/*                                                                     */
/***********************************************************************/
static int get_num(FILE *stream, const char *s, int *index, char c,
                   int count, int base, void *value, int width, int plus,
                   int which_pointer)
{
  int cnt = 0, factor;
  long signed_val = 0;
  unsigned long unsigned_val = 0;

  for (;;)
  {
    /*-----------------------------------------------------------------*/
    /* If we've reached the end of string, put back character.         */
    /*-----------------------------------------------------------------*/
    if (((base == 10) && !isdigit(c)) ||
        ((base == 8) && !(c >= '0' && c <= '7')) ||
        ((base == 16) && !isxdigit(c)))
    {
      while ((isdigit(c) || (base == 16 && isxdigit(c))) &&
             (width > cnt || !width))
      {
        ++cnt;
        c = get_char(stream, s, index);
      }
      put_char(stream, index, c);
      break;
    }

    /*-----------------------------------------------------------------*/
    /* If we haven't exceeded the width, update value.                 */
    /*-----------------------------------------------------------------*/
    if ((width && (width >= cnt)) || !width)
    {
      if (c <= '9')
        factor = c - '0';
      else if (c < 'a')
        factor = c - 'A' + 10;
      else
        factor = c - 'a' + 10;

      if (which_pointer & SIGNED)
        signed_val = base * signed_val + factor;
      else
        unsigned_val = base * unsigned_val + factor;
    }

    /*-----------------------------------------------------------------*/
    /* Get the next char from the input.                               */
    /*-----------------------------------------------------------------*/
    ++count;
    ++cnt;

    /*-----------------------------------------------------------------*/
    /* If we've got width characters from the input, stop.             */
    /*-----------------------------------------------------------------*/
    if (width <= cnt && width)
    {
      break;
    }

    c = get_char(stream, s, index);
  }

  /*-------------------------------------------------------------------*/
  /* If it's a negative number, need to change the sign.               */
  /*-------------------------------------------------------------------*/
  if (!plus)
  {
    if (which_pointer & SIGNED)
      signed_val = -signed_val;
    else
      unsigned_val = -unsigned_val;
  }

  /*-------------------------------------------------------------------*/
  /* Store the value in the return pointer.                            */
  /*-------------------------------------------------------------------*/
  switch (which_pointer)
  {
    case INT:
    case LONG_INT:
      *(long *)value = signed_val;
      break;
    case SHORT_INT:
      *(short *)value = (short)signed_val;
      break;
    case U_INT:
    case U_LONG_INT:
      *(unsigned long *)value = unsigned_val;
      break;
    case U_SHORT_INT:
      *(unsigned short *)value = (unsigned short)unsigned_val;
      break;
  }

  return count;
}

/***********************************************************************/
/* ignore_plus: Read char from input and one more if first is '+'      */
/*                                                                     */
/*      Inputs: st = pointer to file control block                     */
/*              s = input string                                       */
/*              i = index into string                                  */
/*              cnt = number of chars read in so far                   */
/*              c_i = the sign character                               */
/*                                                                     */
/*     Returns: last character read from the input                     */
/*                                                                     */
/***********************************************************************/
static char ignore_plus(FILE *st, const char *s, int *i, int *cnt,
                        char c_i)
{
  char r_value = c_i;

  if (r_value == '+')
  {
    ++(*cnt);
    r_value = get_char(st, s, i);
  }
  return r_value;
}

/***********************************************************************/
/*    get_sign: Read char from input and one more if '+' or '-'        */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              s = input string                                       */
/*              index = index into string                              */
/*              c = last character read from the input                 */
/*              ct = number of chars read in so far                    */
/*                                                                     */
/*     Returns: the sign, 1 for '+', 0 for '-', default is '+'         */
/*                                                                     */
/***********************************************************************/
static int get_sign(FILE *stream, const char *s, int *index, char *c,
                    int *ct)
{
  int r_value = _PLUS;

  /*-------------------------------------------------------------------*/
  /* If there is a sign, read in an extra character.                   */
  /*-------------------------------------------------------------------*/
  if (*c == '-' || *c == '+')
  {
    r_value = (*c == '-') ? _MINUS : _PLUS;
    ++(*ct);
    *c = get_char(stream, s, index);
  }
  return r_value;
}

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/* ProcessScanf: Read variable acording to format into args            */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              format = string with reading directives                */
/*              args = arguments in which the values read are stored   */
/*              s = input string                                       */
/*                                                                     */
/*     Returns: number of successful assignments or EOF                */
/*                                                                     */
/***********************************************************************/
int ProcessScanf(FILE *stream, const char *format, va_list args,
                 const char *s)
{
  int vars_assigned = 0, count = 0, state = 0, index = 0;
  char c, c_i;
  int width, value, store;
  int plus;

  for (;;)
  {
    /*-----------------------------------------------------------------*/
    /* Look at the format a character at a time and go to right state. */
    /*-----------------------------------------------------------------*/
    c = *format;
    switch (state)
    {
    /*-----------------------------------------------------------------*/
    /* State 0 reads in chars and goes to right state or terminates.   */
    /*-----------------------------------------------------------------*/
    case 0:
    {
      ++format;

      /*---------------------------------------------------------------*/
      /* If we've reached the end of format, return num of successful  */
      /* assignments or EOF.                                           */
      /*---------------------------------------------------------------*/
      if (c == '\0')
        return (vars_assigned) ? vars_assigned : EOF;

      /*---------------------------------------------------------------*/
      /* Else if c is format specifier, go ahead and parse format.     */
      /*---------------------------------------------------------------*/
      else if (c == '%')
        state = 1;

      /*---------------------------------------------------------------*/
      /* If c is white space, ignore it.                               */
      /*---------------------------------------------------------------*/
      else if (isspace(c))
        state = 2;

      /*---------------------------------------------------------------*/
      /* Else match character from format with input character.        */
      /*---------------------------------------------------------------*/
      else
      {
        --format;
        state = 3;
      }
      break;
    }

    /*-----------------------------------------------------------------*/
    /* State 1 parses a format specifier, looking for a store first.   */
    /*-----------------------------------------------------------------*/
    case 1:
    {
      ++format;
      store = 1;
      value = width = 0;

      if (c == '*')
        store = 0;
      else if (c == '%')
        state = 0;
      else
        --format;
      state = 4;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* State 2 reads in all white spaces from input.                   */
    /*-----------------------------------------------------------------*/
    case 2:
    {
      c_i = get_char(stream, s, &index);
      for (; isspace(c_i); ++count, c_i = get_char(stream, s, &index)) ;
      put_char(stream, &index, c_i);

      state = 0;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* State 3 matches a character from format with a char from input. */
    /*-----------------------------------------------------------------*/
    case 3:
    {
      ++count;
      c_i = get_char(stream, s, &index);
      if (c_i != c)
      {
        return (vars_assigned) ? vars_assigned : EOF;
      }
      ++format;

      state = 0;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* State 4 looks for the width.                                    */
    /*-----------------------------------------------------------------*/
    case 4:
    {
      for (; isdigit(c); ++format, c = *format)
        width = 10 * width + c - '0';

      state = 5;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* State 5 looks for an 'l', 'L', or 'h'.                          */
    /*-----------------------------------------------------------------*/
    case 5:
    {
      ++format;
      if (c == 'l')
        value = l_VALUE;
      else if (c == 'h')
        value = h_VALUE;
      else
        --format;
      state = 6;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* State 6 gets the conversion specifier.                          */
    /*-----------------------------------------------------------------*/
    case 6:
    {
      ++format;
      if (c == 'd')
        state = 7;
      else if (c == 'i')
        state = 8;
      else if (c == 'o')
        state = 9;
      else if (c == 'u')
        state = 10;
      else if (c == 's')
        state = 11;
      else if (c == 'p')
        state = 12;
      else if (c == 'x' || c == 'X')
        state = 13;
      else if (c == '[')
        /*-------------------------------------------------------------*/
        /* Handle the scanset (mentioned between the ']'s).            */
        /*-------------------------------------------------------------*/
        state = 14;
      else if (c == 'n')
        state = 15;
      else if (c == 'c')
        state = 16;
      else if (c == 'e' || c == 'f' || c == 'g' || c == 'E' || c == 'G')
        state = 17;
      else
      {
        /*-------------------------------------------------------------*/
        /* For an invalid conversion specifier return.                 */
        /*-------------------------------------------------------------*/
        return vars_assigned ? vars_assigned : EOF;
      }

      /*---------------------------------------------------------------*/
      /* Except for the 'c' specifier, skip leading white spaces.      */
      /*---------------------------------------------------------------*/
      if (state && (c != 'c' && c != '[' && c != 'n'))
      {
        c_i = get_char(stream, s, &index);
        for (; isspace(c_i); ++count)
          c_i = get_char(stream, s, &index);
      }

      break;
    }

    /*-----------------------------------------------------------------*/
    /* State 7 handles the 'd' specifier.                              */
    /*-----------------------------------------------------------------*/
    case 7:
    {
      void *curr_ptr;
      int which_pointer;

      /*---------------------------------------------------------------*/
      /* Set curr_ptr to the appropriate value.                        */
      /*---------------------------------------------------------------*/
      if (store)
      {
        if (value == DEFAULT_VALUE)
        {
          which_pointer = INT;
          curr_ptr = va_arg(args, int *);
        }
        else if (value == l_VALUE)
        {
          which_pointer = LONG_INT;
          curr_ptr = va_arg(args, long *);
        }
        else
        {
          which_pointer = SHORT_INT;
          curr_ptr = va_arg(args, short *);
        }
        ++vars_assigned;
      }
      else
      {
        which_pointer = INT;
        curr_ptr = &UnusedInt;
      }

      plus = get_sign(stream, s, &index, &c_i, &count);

      /*---------------------------------------------------------------*/
      /* Get the actual number, as a sequence of digits.               */
      /*---------------------------------------------------------------*/
      count = get_num(stream, s, &index, c_i, count, 10, curr_ptr,
                      width, plus, which_pointer);
      state = 0;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* State 8 handles the 'i' specifier.                              */
    /*-----------------------------------------------------------------*/
    case 8:
    {
      void *curr_ptr;
      int which_pointer;

      /*---------------------------------------------------------------*/
      /* Set curr_ptr to the appropriate value.                        */
      /*---------------------------------------------------------------*/
      if (store)
      {
        if (value == DEFAULT_VALUE)
        {
          which_pointer = INT;
          curr_ptr = va_arg(args, int *);
        }
        else if (value == l_VALUE)
        {
          which_pointer = LONG_INT;
          curr_ptr = va_arg(args, long *);
        }
        else
        {
          which_pointer = SHORT_INT;
          curr_ptr = va_arg(args, short *);
        }
        ++vars_assigned;
      }
      else
      {
        which_pointer = INT;
        curr_ptr = &UnusedInt;
      }

      plus = get_sign(stream, s, &index, &c_i, &count);

      /*---------------------------------------------------------------*/
      /* If c_i is a number, look for the base.                        */
      /*---------------------------------------------------------------*/
      if (isdigit(c_i))
      {
        /*-------------------------------------------------------------*/
        /* If c_i is 0, it's either octal or hexadecimal, else it's    */
        /* decimal.                                                    */
        /*-------------------------------------------------------------*/
        if (c_i == '0')
        {
          ++count;
          c_i = get_char(stream, s, &index);

          /*-----------------------------------------------------------*/
          /* If c_i is 'x' or 'X', it's hexadecimal, else it's octal.  */
          /*-----------------------------------------------------------*/
          if (c_i == 'x' || c_i == 'X')
          {
            ++count;
            c_i = get_char(stream, s, &index);
            count = get_num(stream, s, &index, c_i, count, 16, curr_ptr,
                            width, plus, which_pointer);
          }
          else
            count = get_num(stream, s, &index, c_i, count, 8, curr_ptr,
                            width, plus, which_pointer);
        }
        else
          count = get_num(stream, s, &index, c_i, count, 10, curr_ptr,
                          width, plus, which_pointer);
      }

      state = 0;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* State 9 handles the 'o' specifier.                              */
    /*-----------------------------------------------------------------*/
    case 9:
    {
      void *curr_ptr;
      int which_pointer;

      /*---------------------------------------------------------------*/
      /* Set curr_ptr to the appropriate value.                        */
      /*---------------------------------------------------------------*/
      if (store)
      {
        if (value == DEFAULT_VALUE)
        {
          which_pointer = U_INT;
          curr_ptr = va_arg(args, unsigned int *);
        }
        else if (value == l_VALUE)
        {
          which_pointer = U_LONG_INT;
          curr_ptr = va_arg(args, unsigned long *);
        }
        else
        {
          which_pointer = U_SHORT_INT;
          curr_ptr = va_arg(args, unsigned short *);
        }
        ++vars_assigned;
      }

      c_i = ignore_plus(stream, s, &index, &count, c_i);

      /*---------------------------------------------------------------*/
      /* Get the actual number, as a sequence of digits.               */
      /*---------------------------------------------------------------*/
      count = get_num(stream, s, &index, c_i, count, 8, curr_ptr,
                      width, _PLUS, which_pointer);

      state = 0;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* State 10 handles the 'u' specifier.                             */
    /*-----------------------------------------------------------------*/
    case 10:
    {
      void *curr_ptr;
      int which_pointer;

      /*---------------------------------------------------------------*/
      /* Set curr_ptr to the appropriate value.                        */
      /*---------------------------------------------------------------*/
      if (store)
      {
        if (value == DEFAULT_VALUE)
        {
          which_pointer = U_INT;
          curr_ptr = va_arg(args, unsigned int *);
        }
        else if (value == l_VALUE)
        {
          which_pointer = U_LONG_INT;
          curr_ptr = va_arg(args, unsigned long *);
        }
        else
        {
          which_pointer = U_SHORT_INT;
          curr_ptr = va_arg(args, unsigned short *);
        }
        ++vars_assigned;
      }

      c_i = ignore_plus(stream, s, &index, &count, c_i);

      /*---------------------------------------------------------------*/
      /* Get the actual number, as a sequence of digits.               */
      /*---------------------------------------------------------------*/
      count = get_num(stream, s, &index, c_i, count, 10, curr_ptr,
                      width, _PLUS, which_pointer);

      state = 0;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* State 11 handles the 's' specifier.                             */
    /*-----------------------------------------------------------------*/
    case 11:
    {
      int cnt = 0;
      char *curr_ptr;

      /*---------------------------------------------------------------*/
      /* Set curr_ptr to the appropriate value.                        */
      /*---------------------------------------------------------------*/
      if (store)
      {
        curr_ptr = va_arg(args, char *);
        ++vars_assigned;
      }

      for (; !isspace(c_i) && c_i != EOF; ++count, ++cnt)
      {
        if (store && (width > cnt || !width))
          *curr_ptr++ = c_i;
        c_i = get_char(stream, s, &index);
      }
      if (store)
        *curr_ptr = '\0';
      put_char(stream, &index, c_i);

      state = 0;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* State 12 handles the 'p' specifier.                             */
    /*-----------------------------------------------------------------*/
    case 12:
    {
      int *curr_ptr;

      /*---------------------------------------------------------------*/
      /* Set curr_ptr to the appropriate value.                        */
      /*---------------------------------------------------------------*/
      if (store)
      {
        curr_ptr = va_arg(args, int *);
        ++vars_assigned;
      }
      else
        curr_ptr = &UnusedInt;

      /*---------------------------------------------------------------*/
      /* Get the actual number, as a sequence of digits.               */
      /*---------------------------------------------------------------*/
      count = get_num(stream, s, &index, c_i, count, 16, curr_ptr,
                      width, _PLUS, INT);

      state = 0;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* State 13 handles the 'x'('X') specifier.                        */
    /*-----------------------------------------------------------------*/
    case 13:
    {
      void *curr_ptr;
      int which_pointer;

      /*---------------------------------------------------------------*/
      /* Set curr_ptr to the appropriate value.                        */
      /*---------------------------------------------------------------*/
      if (store)
      {
        if (value == DEFAULT_VALUE)
        {
          which_pointer = U_INT;
          curr_ptr = va_arg(args, unsigned int *);
        }
        else if (value == l_VALUE)
        {
          which_pointer = U_LONG_INT;
          curr_ptr = va_arg(args, unsigned long *);
        }
        else if (value == h_VALUE)
        {
          which_pointer = U_SHORT_INT;
          curr_ptr = va_arg(args, unsigned short *);
        }
        ++vars_assigned;
      }
      else
      {
        which_pointer = INT;
        curr_ptr = &UnusedInt;
      }

      c_i = ignore_plus(stream, s, &index, &count, c_i);

      /*---------------------------------------------------------------*/
      /* If c_i is 0, ignore the leading 0x or 0X if ther is one.      */
      /*---------------------------------------------------------------*/
      if (c_i == '0')
      {
        ++count;
        c_i = get_char(stream, s, &index);
        if (c_i == 'x' || c_i == 'X')
        {
          ++count;
          c_i = get_char(stream, s, &index);
        }
      }

      /*---------------------------------------------------------------*/
      /* Get the actual number, as a sequence of digits.               */
      /*---------------------------------------------------------------*/
      count = get_num(stream, s, &index, c_i, count, 16, curr_ptr,
                      width, _PLUS, which_pointer);

      state = 0;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* State 14 handles the '[' specifier, the scanset.                */
    /*-----------------------------------------------------------------*/
    case 14:
    {
      int neg = FALSE, size = 0, cnt = 0;
      char *scanset, *c_ptr, *curr_ptr;

      /*---------------------------------------------------------------*/
      /* Check for 'not' flag.                                         */
      /*---------------------------------------------------------------*/
      if (*format == '^')
      {
        ++format;
        neg = TRUE;
      }

      /*---------------------------------------------------------------*/
      /* Beginning of the scanset.                                     */
      /*---------------------------------------------------------------*/
      scanset = (char *)format;
      if (*format == ']')
      {
        ++format;
        ++size;
      }

      for (; *format != ']'; ++format, ++size)
      {
        /*-------------------------------------------------------------*/
        /* If the NULL character is specified in format, stop.         */
        /*-------------------------------------------------------------*/
        if (!(*format))
          return vars_assigned ? vars_assigned : EOF;
      }

      /*---------------------------------------------------------------*/
      /* Set curr_ptr to the appropriate value.                        */
      /*---------------------------------------------------------------*/
      if (store)
        curr_ptr = va_arg(args, char *);

      for (; cnt < width || !width; ++curr_ptr, ++cnt, ++count)
      {
        c_i = get_char(stream, s, &index);

        /*-------------------------------------------------------------*/
        /* Read in width chrs or, if not mentioned, till end of input. */
        /*-------------------------------------------------------------*/
        if (c_i != '\0')
        {
          c_ptr = memchr(scanset, c_i, (size_t)size);

          /*-----------------------------------------------------------*/
          /* If the input character doesn't match the scanset, stop.   */
          /*-----------------------------------------------------------*/
          if ((neg && c_ptr) || (!neg && !c_ptr))
            break;

          if (store)
            *curr_ptr = c_i;
        }
        else
          break;
      }

      /*---------------------------------------------------------------*/
      /* String must end with a NULL character.                        */
      /*---------------------------------------------------------------*/
      if (store)
      {
        *curr_ptr = '\0';
        ++vars_assigned;
      }

      state = 0;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* State 15 handles the 'n' specifier.                             */
    /*-----------------------------------------------------------------*/
    case 15:
    {
      int *curr_ptr;

      /*---------------------------------------------------------------*/
      /* Set curr_ptr to the appropriate value.                        */
      /*---------------------------------------------------------------*/
      if (store)
      {
        curr_ptr = va_arg(args, int *);
        *curr_ptr = count;
      }

      state = 0;
      break;
    }

    /*-----------------------------------------------------------------*/
    /* State 16 handles the 'c' specifier.                             */
    /*-----------------------------------------------------------------*/
    case 16:
    {
      int cnt = 0;
      char *curr_ptr;

      /*---------------------------------------------------------------*/
      /* Set curr_ptr to the appropriate value.                        */
      /*---------------------------------------------------------------*/
      if (store)
      {
        curr_ptr = va_arg(args, char *);
        ++vars_assigned;
      }

      /*---------------------------------------------------------------*/
      /* Read in at least one character.                               */
      /*---------------------------------------------------------------*/
      width = width ? width : 1;
      for (; cnt < width; ++cnt, ++count)
      {
        c = get_char(stream, s, &index);
        if (store)
          *curr_ptr = c;
      }

      state = 0;
      break;
    }

    default:
      break;
    }
  }
}

