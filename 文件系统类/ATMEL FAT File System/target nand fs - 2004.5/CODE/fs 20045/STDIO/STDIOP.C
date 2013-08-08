/***********************************************************************/
/*                                                                     */
/*   Module:  stdiop.c                                                 */
/*   Release: 2004.5                                                   */
/*   Version: 2003.5                                                   */
/*   Purpose: stdio.h print formatting for non-floating point builds   */
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
/* Symbol Definitions                                                  */
/***********************************************************************/
#define BUF_BYTES   200           /* size of buf used in ProcessPrintf */

/***********************************************************************/
/* Local Function Definitions                                          */
/***********************************************************************/

/***********************************************************************/
/*    send_buf: Write multiple chars from buffer to stream             */
/*                                                                     */
/*      Inputs: stream = output stream                                 */
/*              buf = buffer from which charaters are sent             */
/*              num_bytes = number of bytes to send                    */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int send_buf(FILE *stream, char *buf, int num_bytes)
{
  if (stream->write(stream, (const ui8*)buf, (ui32)num_bytes) !=
      num_bytes)
  {
    stream->errcode = get_errno();
    return -1;
  }
  return 0;
}

/***********************************************************************/
/*  send_chars: Write a char multiple times to stream                  */
/*                                                                     */
/*      Inputs: stream = output stream                                 */
/*              c = character to send multiple time                    */
/*              num_bytes = number of bytes to send                    */
/*                                                                     */
/*     Returns: 0 on success, -1 on error                              */
/*                                                                     */
/***********************************************************************/
static int send_chars(FILE *stream, char c, int num_bytes)
{
  int i;

  for (i = 0; i < num_bytes; ++i)
    if (stream->write(stream, (const ui8*)&c, 1) != 1)
    {
      stream->errcode = get_errno();
      return -1;
    }

  return 0;
}

/***********************************************************************/
/*   send_char: Write character to stream                              */
/*                                                                     */
/*      Inputs: stream = output stream                                 */
/*              c = char to be displayed                               */
/*                                                                     */
/*     Returns: 0 on success, -1 on failure                            */
/*                                                                     */
/***********************************************************************/
static int send_char(FILE *stream, char c)
{
  /*-------------------------------------------------------------------*/
  /* If write to stream failed, return error.                          */
  /*-------------------------------------------------------------------*/
  if (stream->write(stream, (const ui8 *)&c, 1) != 1)
  {
    stream->errcode = get_errno();
    return -1;
  }

  return 0;
}

/***********************************************************************/
/* trnsfrm_val: Put the equivalent of val into buf                     */
/*                                                                     */
/*      Inputs: buf = destination string                               */
/*              val = value to be converted                            */
/*              base = base of conversion                              */
/*              x = for base 16, whether it's 'x' or 'X'               */
/*              p = precision                                          */
/*                                                                     */
/*     Returns: start position in buf for valid bytes                  */
/*                                                                     */
/***********************************************************************/
static int trnsfrm_val(char buf[], ui32 val, int base, int x, int p)
{
  ui32 t_val;
  int  i;

  /*-------------------------------------------------------------------*/
  /* Fill buf with string equivalent of val, starting from the end.    */
  /*-------------------------------------------------------------------*/
  for (i = BUF_BYTES - 1;; --i, val /= base)
  {
    /*-----------------------------------------------------------------*/
    /* Get the next digit.                                             */
    /*-----------------------------------------------------------------*/
    t_val = val % base;

    /*-----------------------------------------------------------------*/
    /* If base is 16 and t_val > 9, convert to A-F, else get 0-9.      */
    /*-----------------------------------------------------------------*/
    if ((base == 16) && (t_val > 9))
      buf[i] = x ? (char)('a' + t_val - 10) : (char)('A' + t_val - 10);
    else
      buf[i] = (char)('0' + t_val);

    /*-----------------------------------------------------------------*/
    /* Break whenever val gets below base so we can't divide anymore.  */
    /*-----------------------------------------------------------------*/
    if (val < base)
      break;
  }

  /*-------------------------------------------------------------------*/
  /* If padding is needed, fill with 0's.                              */
  /*-------------------------------------------------------------------*/
  while ((i > 0) && (p > BUF_BYTES - i))
    buf[--i] = '0';

  return i;
}

/***********************************************************************/
/*   print_buf: Send chars from buf to stream                          */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              buf = string to be printed                             */
/*              buf_size = size of buf in bytes                        */
/*              start = start of valid bytes in buf                    */
/*              flags, specialValue = special printf controls          */
/*              specifier = output specifier from format string        */
/*              width = size of the output                             */
/*              precision = precision from format string               */
/*                                                                     */
/*     Returns: Number of bytes printed, -1 on error                   */
/*                                                                     */
/***********************************************************************/
static int print_buf(FILE *stream, char buf[], int buf_size, int start,
                     int flags, int specifier, int width, int precision)
{
  int add = 0, curr = start, printed_bytes = 0;

  /*-------------------------------------------------------------------*/
  /* If size is 0 then there is nothing to be printed.                 */
  /*-------------------------------------------------------------------*/
  if (start == buf_size)
  {
    /*-----------------------------------------------------------------*/
    /* Output a ' ' if space flag is enabled.                          */
    /*-----------------------------------------------------------------*/
    if (flags & SPACE_FLAG)
    {
      if (send_char(stream, ' '))
        return -1;
      return 1;
    }
    return 0;
  }

  /*-------------------------------------------------------------------*/
  /* If specifier is either %d or %i check for the plus or space flags */
  /* to be enabled.                                                    */
  /*-------------------------------------------------------------------*/
  if (specifier == DI_SPEC)
  {
    /*-----------------------------------------------------------------*/
    /* If the plus flag is enabled and the number is not negative,     */
    /* then put a '+' in front.                                        */
    /*-----------------------------------------------------------------*/
    if ((flags & PLUS_FLAG) && (buf[curr] != '-'))
    {
      if (width > 0) --width;
      add = PLUS_SIGN;
    }

    /*-----------------------------------------------------------------*/
    /* Else if the space flag is on and the number is not negative,    */
    /* then put a ' ' in front.                                        */
    /*-----------------------------------------------------------------*/
    else if ((flags & SPACE_FLAG) && (buf[curr] != '-'))
    {
      if (width > 0) --width;
      add = SPACE_SIGN;
    }
  }

  /*-------------------------------------------------------------------*/
  /* If width is more than bytes in buf, pad with spaces to the right. */
  /*-------------------------------------------------------------------*/
  if ((flags & MINUS_FLAG) && (width > buf_size - curr))
  {
    /*-----------------------------------------------------------------*/
    /* Send either one '+' or ' ' if necessary.                        */
    /*-----------------------------------------------------------------*/
    if (add == PLUS_SIGN)
    {
      if (send_char(stream, '+'))
        return -1;
      ++printed_bytes;
    }
    else if (add == SPACE_SIGN)
    {
      if (send_char(stream, ' '))
        return -1;
      ++printed_bytes;
    }

    /*-----------------------------------------------------------------*/
    /* First output the contents of the buffer, buf.                   */
    /*-----------------------------------------------------------------*/
    if (send_buf(stream, &buf[curr], buf_size - curr))
      return -1;
    printed_bytes += (buf_size - curr);

    /*-----------------------------------------------------------------*/
    /* Update width and add spaces for padding.                        */
    /*-----------------------------------------------------------------*/
    width -= (buf_size - curr);
    if (send_chars(stream, ' ', width))
      return -1;
    if (width > 0)
      printed_bytes += width;
  }

  /*-------------------------------------------------------------------*/
  /* Else if width is more than bytes in buf, pad with ' ' or '+'.     */
  /*-------------------------------------------------------------------*/
  else if ((flags & ZERO_FLAG) && (width > buf_size - curr))
  {
    /*-----------------------------------------------------------------*/
    /* Send either one '+' or ' ' if necessary.                        */
    /*-----------------------------------------------------------------*/
    if (add == PLUS_SIGN)
    {
      if (send_char(stream, '+'))
        return -1;
      ++printed_bytes;
    }
    else if (add == SPACE_SIGN)
    {
      if (send_char(stream, ' '))
        return -1;
      ++printed_bytes;
    }

    /*-----------------------------------------------------------------*/
    /* Place '0X', '0x' if necessary.                                  */
    /*-----------------------------------------------------------------*/
    if ((flags & NUM_FLAG) && (specifier == X_SPEC))
    {
      if (send_buf(stream, &buf[curr], 2))
        return -1;
      width -= 2;
      curr += 2;
      printed_bytes += 2;
    }

    /*-----------------------------------------------------------------*/
    /* Pad with zeros to the left until width.                         */
    /*-----------------------------------------------------------------*/
    if (send_chars(stream, '0', width - (buf_size - curr)))
      return -1;
    if (width - (buf_size - curr) > 0)
      printed_bytes += (width - (buf_size - curr));

    /*-----------------------------------------------------------------*/
    /* Now send the actual buf.                                        */
    /*-----------------------------------------------------------------*/
    if (send_buf(stream, &buf[curr], buf_size - curr))
      return -1;
    printed_bytes += (buf_size - curr);
  }

  /*-------------------------------------------------------------------*/
  /* Else simply fill out buffer.                                      */
  /*-------------------------------------------------------------------*/
  else
  {
    /*-----------------------------------------------------------------*/
    /* Figure out how many spaces of padding are needed.               */
    /*-----------------------------------------------------------------*/
    width -= (buf_size - curr);

    /*-----------------------------------------------------------------*/
    /* Pad with spaces to the left until width.                        */
    /*-----------------------------------------------------------------*/
    if (send_chars(stream, ' ', width))
      return -1;
    if (width > 0)
      printed_bytes += width;

    /*-----------------------------------------------------------------*/
    /* Send either one '+' or ' ' if necessary.                        */
    /*-----------------------------------------------------------------*/
    if (add == PLUS_SIGN)
    {
      if (send_char(stream, '+'))
        return -1;
      ++printed_bytes;
    }
    else if (add == SPACE_SIGN)
    {
      if (send_char(stream, ' '))
        return -1;
      ++printed_bytes;
    }

    /*-----------------------------------------------------------------*/
    /* Now, output the contents of the buffer, buf.                    */
    /*-----------------------------------------------------------------*/
    if (send_buf(stream, &buf[curr], buf_size - curr))
      return -1;
    printed_bytes += (buf_size - curr);
  }
  return printed_bytes;
}

/***********************************************************************/
/* Global Function Definitions                                         */
/***********************************************************************/

/***********************************************************************/
/* StringWrite: String write function used by file control block       */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              buf = buffer to read data from                         */
/*              len = number of bytes to read                          */
/*                                                                     */
/*     Returns: Number of bytes successfully written, -1 on error      */
/*                                                                     */
/***********************************************************************/
int StringWrite(FILE *stream, const ui8 *buf, ui32 len)
{
  ui32 pos = (ui32)stream->pos;

  memcpy(&((ui8 *)stream->handle)[pos], buf, len);
  stream->pos = (void *)(pos + len);

  return (int)len;
}

/***********************************************************************/
/* StringWriteN: String write function used by file control block when */
/*              there is a limit on string size (snprintf, etc.)       */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              buf = buffer to read data from                         */
/*              len = number of bytes to read                          */
/*                                                                     */
/*     Returns: Number of bytes successfully written, -1 when string   */
/*              limit has been reached                                 */
/*                                                                     */
/***********************************************************************/
int StringWriteN(FILE *stream, const ui8 *buf, ui32 len)
{
  ui32 pos = (ui32)stream->pos;
  ui32 n = (ui32)stream->volume;

  /*-------------------------------------------------------------------*/
  /* Write to string only when n is positive.                          */
  /*-------------------------------------------------------------------*/
  if (n)
  {
    /*-----------------------------------------------------------------*/
    /* If not enough space to write, write as much as possible and     */
    /* stop processing.                                                */
    /*-----------------------------------------------------------------*/
    if (pos + len > n)
    {
      memcpy(&((ui8 *)stream->handle)[pos], buf, n - pos);
      stream->pos = (void *)n;
      return -1;
    }
    else
    {
      memcpy(&((ui8 *)stream->handle)[pos], buf, len);
      stream->pos = (void *)(pos + len);
    }
  }

  /*-------------------------------------------------------------------*/
  /* For n == 0, process the print function without writing to string. */
  /*-------------------------------------------------------------------*/
  else
    stream->pos = (void *)(pos + len);

  return (int)len;
}

/***********************************************************************/
/* ProcessPrintf: Print format and list_of_args to stream              */
/*                                                                     */
/*      Inputs: stream = pointer to file control block                 */
/*              format = string to be printed                          */
/*              list_of_args = arguments from the print functions      */
/*                                                                     */
/*     Returns: number of characters written to stream or s            */
/*                                                                     */
/***********************************************************************/
int ProcessPrintf(FILE *stream, const char *format, va_list list_of_args)
{
  int special_value, start, x, flags, count = 0, state = 0, bytes;
  int width, precision, sign, p, length, normal_chars = 0;
  char buf[BUF_BYTES + 1];
  i8 *str, c;
  ui32 unsigned_val;
  i32 val;

  buf[BUF_BYTES] = '\0';
  for (;;)
  {
    /*-----------------------------------------------------------------*/
    /* Read in a char at a time and go to right state.                 */
    /*-----------------------------------------------------------------*/
    c = *format;
    switch (state)
    {
    /*-----------------------------------------------------------------*/
    /* State 0 reads in chars until it finds '%' or '\0'.              */
    /*-----------------------------------------------------------------*/
    case 0:
      flags = special_value = DEFAULT_FLAG;
      width = 0;
      precision = 0;
      ++format;

      /*---------------------------------------------------------------*/
      /* If either a '%' or '\0' encountered, move to different state  */
      /* or quit based on which one found.                             */
      /*---------------------------------------------------------------*/
      if (c == '\0' || c == '%')
      {
        /*-------------------------------------------------------------*/
        /* First, print all the normal chars that have accumulated.    */
        /*-------------------------------------------------------------*/
        if (normal_chars)
        {
          if (send_buf(stream, (char *)(format - normal_chars - 1),
                       normal_chars))
            return -1;
          count += normal_chars;
        }

        /*-------------------------------------------------------------*/
        /* If '\0', return and if sprintf, terminate the string.       */
        /*-------------------------------------------------------------*/
        if (c == '\0')
          return count;

        /*-------------------------------------------------------------*/
        /* Else, '%', move to different state and reset normal chars   */
        /* counter.                                                    */
        /*-------------------------------------------------------------*/
        else
        {
          state = 1;
          normal_chars = 0;
        }
      }

      /*---------------------------------------------------------------*/
      /* Else accumulate normal character.                             */
      /*---------------------------------------------------------------*/
      else
        ++normal_chars;

      break;

    /*-----------------------------------------------------------------*/
    /* State 1 looks for special flags ('0', '-', '#', or ' ').        */
    /*-----------------------------------------------------------------*/
    case 1:
      ++format;
      if (c == '0')
        flags = flags | ZERO_FLAG;
      else if (c == '-')
        flags = flags | MINUS_FLAG;
      else if (c == '+')
        flags = flags | PLUS_FLAG;
      else if (c == '#')
        flags = flags | NUM_FLAG;
      else if (c == ' ')
        flags = flags | SPACE_FLAG;
      else if (c == '%')
      {
        /*-------------------------------------------------------------*/
        /* Write '%' and return back to initial state.                 */
        /*-------------------------------------------------------------*/
        if (send_char(stream, c))
          return -1;
        ++count;
        state = 0;
      }
      else
      {
        /*-------------------------------------------------------------*/
        /* No special flags found anymore, so go to next state.        */
        /*-------------------------------------------------------------*/
        state = 2;
        --format;
      }
      break;

    /*-----------------------------------------------------------------*/
    /* State 2 looks for the width field.                              */
    /*-----------------------------------------------------------------*/
    case 2:
      ++format;

      /*---------------------------------------------------------------*/
      /* If c is a digit, keep looking for digits until no more found. */
      /*---------------------------------------------------------------*/
      if (c >= '0' && c <= '9')
      {
        state = 3;
        width = c - '0';
      }

      /*---------------------------------------------------------------*/
      /* Else if c is '*', need to fetch integer value from list of    */
      /* arguments.                                                    */
      /*---------------------------------------------------------------*/
      else if (c == '*')
        state = 4;

      /*---------------------------------------------------------------*/
      /* Else if c is '.', a precision field is specified.             */
      /*---------------------------------------------------------------*/
      else if (c == '.')
        state = 5;

      /*---------------------------------------------------------------*/
      /* Else no special character found, put it back and continue.    */
      /*---------------------------------------------------------------*/
      else
      {
        state = 8;
        --format;
      }
      break;

    /*-----------------------------------------------------------------*/
    /* State 3 keeps looking for digits until no more found.           */
    /*-----------------------------------------------------------------*/
    case 3:
      ++format;

      /*---------------------------------------------------------------*/
      /* If c is a digit, add it to the width field.                   */
      /*---------------------------------------------------------------*/
      if (isdigit(c))
        width = 10 * width + c - '0';

      /*---------------------------------------------------------------*/
      /* Else if c is '.', a precision field is specified.             */
      /*---------------------------------------------------------------*/
      else if (c == '.')
        state = 5;

      /*---------------------------------------------------------------*/
      /* Else no special character found, put it back and continue.    */
      /*---------------------------------------------------------------*/
      else
      {
        state = 8;
        --format;
      }
      break;

    /*-----------------------------------------------------------------*/
    /* State 4 fetches an int from list of args to be the width.       */
    /*-----------------------------------------------------------------*/
    case 4:
      width = va_arg(list_of_args, int);

      /*---------------------------------------------------------------*/
      /* If width is negative, it's equivalent to having a minus sign  */
      /* in the flags.                                                 */
      /*---------------------------------------------------------------*/
      if (width < 0)
      {
        flags = flags | MINUS_FLAG;
        width = 0 - width;
      }

      /*---------------------------------------------------------------*/
      /* If c is '.', a precision field is specified.                  */
      /*---------------------------------------------------------------*/
      if (c == '.')
      {
        ++format;
        state = 5;
      }

      /*---------------------------------------------------------------*/
      /* Else no special character found, put it back and continue.    */
      /*---------------------------------------------------------------*/
      else
        state = 8;
      break;

    /*-----------------------------------------------------------------*/
    /* State 5 looks for the precision field.                          */
    /*-----------------------------------------------------------------*/
    case 5:
      ++format;

      /*---------------------------------------------------------------*/
      /* If c is digit, keep looking for digits until no more found.   */
      /*---------------------------------------------------------------*/
      if (c >= '0' && c <= '9')
      {
        precision = c - '0';
        state = 6;
      }

      /*---------------------------------------------------------------*/
      /* Else if c is '*', get integer value from list of arguments.   */
      /*---------------------------------------------------------------*/
      else if (c == '*')
        state = 7;

      /*---------------------------------------------------------------*/
      /* Else no special character found, put it back and continue.    */
      /*---------------------------------------------------------------*/
      else
      {
        state = 8;
        --format;
      }
      break;

    /*-----------------------------------------------------------------*/
    /* State 6 keeps looking for digits until no more found.           */
    /*-----------------------------------------------------------------*/
    case 6:
      /*---------------------------------------------------------------*/
      /* If c is a digit, add it to the precision field.               */
      /*---------------------------------------------------------------*/
      if (isdigit(c))
      {
        precision = 10 * precision + c - '0';
        ++format;
      }

      /*---------------------------------------------------------------*/
      /* Else no special character found, put it back and continue.    */
      /*---------------------------------------------------------------*/
      else
        state = 8;
      break;

    /*-----------------------------------------------------------------*/
    /* State 7 fetches an int from list of args to be the precision.   */
    /*-----------------------------------------------------------------*/
    case 7:
      precision = va_arg(list_of_args, int);

      /*---------------------------------------------------------------*/
      /* If precision is negative, set it to 0.                        */
      /*---------------------------------------------------------------*/
      if (precision < 0)
        precision = 0;

      state = 8;
      break;

    /*-----------------------------------------------------------------*/
    /* State 8 checks for 'l', 'L', 'h'.                               */
    /*-----------------------------------------------------------------*/
    case 8:
      ++format;
      if (c == 'l')
        special_value = l_VALUE;
      else if (c == 'L')
        special_value = L_VALUE;
      else if (c == 'h')
        special_value = h_VALUE;
      else
        --format;
      state = 9;
      break;

    /*-----------------------------------------------------------------*/
    /* State 9 checks for conversion specifiers ('x', 'd', etc).       */
    /*-----------------------------------------------------------------*/
    case 9:
      ++format;
      if (c == 'c')
        state = 10;
      else if (c == 'i' || c == 'd')
        state = 11;
      else if (c == 'n')
        state = 12;
      else if (c == 'o')
        state = 13;
      else if (c == 'p')
        state = 14;
      else if (c == 'x' || c == 'X')
      {
        /*-------------------------------------------------------------*/
        /* Sets x to distinguish later between 'x' and 'X'.            */
        /*-------------------------------------------------------------*/
        x = (c == 'x') ? 1 : 0;
        state = 15;
      }
      else if (c == 's')
        state = 16;
      else if (c == 'u')
        state = 17;
      else
      {
        /*-------------------------------------------------------------*/
        /* Go back to initial state.                                   */
        /*-------------------------------------------------------------*/
        state = 0;
        --format;
      }
      break;

    /*-----------------------------------------------------------------*/
    /* State 10 handles the 'c' specifier.                             */
    /*-----------------------------------------------------------------*/
    case 10:
      buf[BUF_BYTES - 1] = (char)va_arg(list_of_args, int);

      /*---------------------------------------------------------------*/
      /* Send buf to be printed.                                       */
      /*---------------------------------------------------------------*/
      bytes = print_buf(stream, buf, BUF_BYTES, BUF_BYTES - 1, flags,
                        S_SPEC, width, precision);
      if (bytes == -1)
        return -1;

      count += bytes;
      state = 0;
      break;

    /*-----------------------------------------------------------------*/
    /* State 11 handles the 'i' and 'd' specifiers                     */
    /*-----------------------------------------------------------------*/
    case 11:
      /*---------------------------------------------------------------*/
      /* Based on the special value get the correct argument           */
      /*---------------------------------------------------------------*/
      if (special_value == h_VALUE)
        val = (i16)va_arg(list_of_args, int);
      else if (special_value == l_VALUE)
        val = va_arg(list_of_args, long);
      else
        val = va_arg(list_of_args, int);

      /*---------------------------------------------------------------*/
      /* If val is negative, put '-' in front and make it positive.    */
      /*---------------------------------------------------------------*/
      if (val < 0)
      {
        sign = 1;
        val = -val;
      }
      else
        sign = 0;

      start = trnsfrm_val(buf, (ui32)val, 10, 0, precision);

      if (sign)
      {
        if (start > 0) --start;
        buf[start] = '-';
      }

      /*---------------------------------------------------------------*/
      /* Send buf to be printed.                                       */
      /*---------------------------------------------------------------*/
      bytes = print_buf(stream, buf, BUF_BYTES, start, flags, DI_SPEC,
                       width, precision);
      if (bytes == -1)
        return -1;

      count += bytes;

      /*---------------------------------------------------------------*/
      /* Go back to initial state.                                     */
      /*---------------------------------------------------------------*/
      state = 0;
      break;

    /*-----------------------------------------------------------------*/
    /* State 12 handles the 'n' specifier.                             */
    /*-----------------------------------------------------------------*/
    case 12:
      /*---------------------------------------------------------------*/
      /* Based on special_value get correct ptr, and store count.      */
      /*---------------------------------------------------------------*/
      if (special_value == h_VALUE)
        *va_arg(list_of_args, i16 *) = (i16)count;
      else if (special_value == l_VALUE)
        *va_arg(list_of_args, i32 *) = count;
      else
        *va_arg(list_of_args, int *) = count;

      /*---------------------------------------------------------------*/
      /* Go back to initial state.                                     */
      /*---------------------------------------------------------------*/
      state = 0;
      break;

    /*-----------------------------------------------------------------*/
    /* State 13 handles the 'o' specifier.                             */
    /*-----------------------------------------------------------------*/
    case 13:
      /*---------------------------------------------------------------*/
      /* Based on special_value get the correct argument.              */
      /*---------------------------------------------------------------*/
      if (special_value == h_VALUE)
        unsigned_val = (ui16)va_arg(list_of_args, int);
      else if (special_value == l_VALUE)
        unsigned_val = (ui32)va_arg(list_of_args, long);
      else
        unsigned_val = (uint)va_arg(list_of_args, int);
      start = trnsfrm_val(buf, unsigned_val, 8, 0, precision);

      /*---------------------------------------------------------------*/
      /* If '#' flag is set, add a '0' in front of the number.         */
      /*---------------------------------------------------------------*/
      if (flags & NUM_FLAG)
      {
        if (start > 0) --start;
        buf[start] = '0';
      }

      /*---------------------------------------------------------------*/
      /* Send buf to be printed.                                       */
      /*---------------------------------------------------------------*/
      bytes = print_buf(stream, buf, BUF_BYTES, start, flags, UO_SPEC,
                        width, precision);
      if (bytes == -1)
        return -1;

      count += bytes;

      /*---------------------------------------------------------------*/
      /* Go back to initial state.                                     */
      /*---------------------------------------------------------------*/
      state = 0;
      break;

    /*-----------------------------------------------------------------*/
    /* State 14 handles the 'p' specifier.                             */
    /*-----------------------------------------------------------------*/
    case 14:
      p = (int)va_arg(list_of_args, void *);
      start = trnsfrm_val(buf, (ui32)p, 16, 0, precision);

      /*---------------------------------------------------------------*/
      /* Send buf to be printed.                                       */
      /*---------------------------------------------------------------*/
      bytes = print_buf(stream, buf, BUF_BYTES, start, 0, P_SPEC, width,
                        precision);
      if (bytes == -1)
        return -1;

      count += bytes;

      /*---------------------------------------------------------------*/
      /* Go back to initial state.                                     */
      /*---------------------------------------------------------------*/
      state = 0;
      break;

    /*-----------------------------------------------------------------*/
    /* State 15 handles the 'x' and 'X' specifiers.                    */
    /*-----------------------------------------------------------------*/
    case 15:
      /*---------------------------------------------------------------*/
      /* Based on special_value get the correct argument.              */
      /*---------------------------------------------------------------*/
      if (special_value == h_VALUE)
        unsigned_val = (ui16)va_arg(list_of_args, int);
      else if (special_value == l_VALUE)
        unsigned_val = (ui32)va_arg(list_of_args, long);
      else
        unsigned_val = (uint)va_arg(list_of_args, int);
      start = trnsfrm_val(buf, unsigned_val, 16, x, precision);

      /*---------------------------------------------------------------*/
      /* If '#' flag is set, add '0x' or '0X' on front of the number.  */
      /*---------------------------------------------------------------*/
      if (flags & NUM_FLAG)
      {
        if (start > 0) --start;
        if (start > 0) --start;
        buf[start + 1] = (char)(x ? 'x' : 'X');
        buf[start] = '0';
      }

      /*---------------------------------------------------------------*/
      /* Send buf to be printed.                                       */
      /*---------------------------------------------------------------*/
      bytes = print_buf(stream, buf, BUF_BYTES, start, flags, X_SPEC,
                        width, precision);
      if (bytes == -1)
        return -1;

      count += bytes;

      /*---------------------------------------------------------------*/
      /* Go back to initial state.                                     */
      /*---------------------------------------------------------------*/
      state = 0;
      break;

    /*-----------------------------------------------------------------*/
    /* State 16 handles the 's' specifier.                             */
    /*-----------------------------------------------------------------*/
    case 16:

      /*---------------------------------------------------------------*/
      /* Get the string and figure out how many chars to output.       */
      /*---------------------------------------------------------------*/
      str = va_arg(list_of_args, i8 *);
      length = (int)strlen(str);
      if (precision && precision < length)
        length = precision;

      /*---------------------------------------------------------------*/
      /* Clear out the PLUS_, SPACE_, or ZERO_ FLAGS if present.       */
      /*---------------------------------------------------------------*/
      flags &= ~(PLUS_FLAG | SPACE_FLAG | ZERO_FLAG);

      /*---------------------------------------------------------------*/
      /* Send buf to be printed.                                       */
      /*---------------------------------------------------------------*/
      bytes = print_buf(stream, str, length, 0, flags, S_SPEC, width, 0);
      if (bytes == -1)
        return -1;

      count += bytes;

      /*---------------------------------------------------------------*/
      /* Go back to initial state.                                     */
      /*---------------------------------------------------------------*/
      state = 0;
      break;

    /*-----------------------------------------------------------------*/
    /* State 17 handles the 'u' specifier.                             */
    /*-----------------------------------------------------------------*/
    case 17:
      /*---------------------------------------------------------------*/
      /* Based on special_value get the correct argument.              */
      /*---------------------------------------------------------------*/
      if (special_value == h_VALUE)
        unsigned_val = (ui16)va_arg(list_of_args, int);
      else if (special_value == l_VALUE)
        unsigned_val = (ui32)va_arg(list_of_args, long);
      else
        unsigned_val = (uint)va_arg(list_of_args, int);
      start = trnsfrm_val(buf, unsigned_val, 10, 0, precision);

      /*---------------------------------------------------------------*/
      /* Send buf to be printed.                                       */
      /*---------------------------------------------------------------*/
      bytes = print_buf(stream, buf, BUF_BYTES, start, flags, UO_SPEC,
                        width, precision);
      if (bytes == -1)
        return -1;

      count += bytes;

      /*---------------------------------------------------------------*/
      /* Go back to initial state.                                     */
      /*---------------------------------------------------------------*/
      state = 0;
      break;

    default:
      break;
    }
  }
}

