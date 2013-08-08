/***********************************************************************/
/*                                                                     */
/*   Module:  stdiop_f.c                                               */
/*   Release: 2004.5                                                   */
/*   Version: 2004.2                                                   */
/*   Purpose: stdio.h print formatting for floating point builds       */
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
#include <math.h>

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/
#define WZ             1
#define WOZ            0
#define PRINT_BUF_SZ   20
#define BUF_BYTES      200        /* size of buf used in ProcessPrintf */

/***********************************************************************/
/* Type Definitions                                                    */
/***********************************************************************/
typedef union
{
  unsigned int sign : 1;
  unsigned int exponent : 12;
  unsigned long whole : 32;
  double real;
} real;

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
/* get_exponent: Extract exponent from a floating point number         */
/*                                                                     */
/*       Input: f_ptr = pointer to the floating point number           */
/*                                                                     */
/*     Returns: exponent of the floating point number                  */
/*                                                                     */
/***********************************************************************/
static int get_exponent(double *f_ptr)
{
  int r_value = 0;
  real real_num;
  double i, exp;
  int temp;

  /*-------------------------------------------------------------------*/
  /* If a pointer to a zero value is passed, return 0.                 */
  /*-------------------------------------------------------------------*/
  if (!*f_ptr)
    return r_value;

  /*-------------------------------------------------------------------*/
  /* Get the exponent in base 2 and subtract the bias (1023).          */
  /*-------------------------------------------------------------------*/
  real_num.real = *f_ptr;
  temp = (int)real_num.exponent - 1023;
  exp = (double)temp;

  /*-------------------------------------------------------------------*/
  /* If exponent is negative, make it positive, transform it in base   */
  /* 10, and multiply.                                                 */
  /*-------------------------------------------------------------------*/
  if (exp < 0)
  {
    exp = -exp;
    exp *= log10(2);
    exp = modf(exp, &i);
    r_value = (int)-i;
    exp = modf(pow(10, i) + 0.5, &i);
    *f_ptr *= i;
  }

  /*-------------------------------------------------------------------*/
  /* Else if exponent positive, transform it in base 10, and divide.   */
  /*-------------------------------------------------------------------*/
  else if (exp > 0)
  {
    exp *= log10(2);
    exp = modf(exp, &i);
    r_value = (int)i;
    exp = modf(pow(10, i) + 0.5, &i);
    *f_ptr /= i;
  }

  /*-------------------------------------------------------------------*/
  /* As long as the float is not between 1 and 10, adjust it.          */
  /*-------------------------------------------------------------------*/
  while (*f_ptr < 1 || *f_ptr >= 10)
  {
    if (*f_ptr < 1)
    {
      *f_ptr *= 10;
      --r_value;
    }
    else
    {
      *f_ptr /= 10;
      ++r_value;
    }
  }

  return r_value;
}

/***********************************************************************/
/*    round_up: Round up the number in buf to precision and get rid of */
/*              trailing zeros if necessary.                           */
/*                                                                     */
/*      Inputs: buf = buffer that stores the number                    */
/*              precision = number of digits to be rounded             */
/*              zeros = flag to keep or disregard trailing zeros       */
/*              exponent = pointer to the value of the exponent        */
/*                                                                     */
/*     Returns: number of digits in buffer                             */
/*                                                                     */
/***********************************************************************/
static int round_up(char *buf, int precision, int zeros, int *exponent)
{
  int r_value = 2, i, exp, next_time = 0;

  /*-------------------------------------------------------------------*/
  /* If precision is 0, there is no rounding off, so just return the   */
  /* integral part of the number.                                      */
  /*-------------------------------------------------------------------*/
  if (!precision)
    return r_value;

  if (precision < 20 && buf[precision + 3] >= '5')
  {
    /*-----------------------------------------------------------------*/
    /* Round up the precision(th) digit.                               */
    /*-----------------------------------------------------------------*/
    i = precision + 2;
    for (;;)
    {
      /*---------------------------------------------------------------*/
      /* If i is 2, buf[i] = '.' so skip it.                           */
      /*---------------------------------------------------------------*/
      if (i != 2)
      {
        /*-------------------------------------------------------------*/
        /* If i is 1 and buf[i] = '9', it's 9.999... which rounded is  */
        /* 1.00000 with the exponent increased.                        */
        /*-------------------------------------------------------------*/
        if (i == 1 && buf[i] == '9')
        {
          for (i = precision + 3; i > 3; --i)
            buf[i] = '0';
          buf[3] = '0';
          buf[1] = '1';
          ++(*exponent);
          break;
        }

        /*-------------------------------------------------------------*/
        /* If buf[i] is 0-8, increment current digit by 1 and return.  */
        /*-------------------------------------------------------------*/
        if (buf[i] != '9')
        {
          buf[i] += 1;
          break;
        }

        /*-------------------------------------------------------------*/
        /* Else buf[i] is '9', so make digit '0' and move one digit    */
        /* to the left (i--).                                          */
        /*-------------------------------------------------------------*/
        else
        {
          buf[i--] = '0';
        }
      }

      /*---------------------------------------------------------------*/
      /* Else i is 2 so skip '.'.                                      */
      /*---------------------------------------------------------------*/
      else
        --i;
    }

    for (i = precision + 3; i < 23; ++i)
      /*---------------------------------------------------------------*/
      /* Fill out the rest of the buffer with 0's.                     */
      /*---------------------------------------------------------------*/
      buf[i] = '0';
  }

  /*-------------------------------------------------------------------*/
  /* Adjust the exponent.                                              */
  /*-------------------------------------------------------------------*/
  exp = *exponent;

  if (exp < 0)
    exp = -exp;

  for (i = 28; i >= 25; --i)
  {
    if (next_time)
      exp = 0;
    buf[i] = (char)(exp % 10 + '0');
    if (exp < 10)
      next_time = 1;
    else
      exp /= 10;
  }

  r_value = precision + 2;
  if (!zeros)
    for (i = precision + 2;; --i)
    {
      /*---------------------------------------------------------------*/
      /* Get rid of trailing zeros.                                    */
      /*---------------------------------------------------------------*/
      if (i <= 2)
        return 2;
      else if (buf[i] != '0')
        return r_value;
      else
        --r_value;
    }

  return r_value;
}

/***********************************************************************/
/*  print_rbuf: Print real_buf to stream                               */
/*                                                                     */
/*      Inputs: stream = output on which real_buf is printed           */
/*              real_buf = string that is printed to the output        */
/*              width = width of number to be printed                  */
/*              flags = printf flags (#, 0, -)                         */
/*              spec = 'f', 'e', or 'E' specifier                      */
/*              precision = precision specified (6 default)            */
/*              zeros = flag for trailing zeros                        */
/*                                                                     */
/*     Returns: Number of bytes printed, -1 on error                   */
/*                                                                     */
/***********************************************************************/
static int print_rbuf(FILE *stream, char *real_buf, int width, int flags,
                      char spec, int precision, int zeros)
{
  int exponent = 1000 * (real_buf[25] - '0') + 100 * (real_buf[26] - '0')
                 + 10 * (real_buf[27] - '0') + real_buf[28] - '0';
  int spaces = 0, change_point = 0, when = 0, d_digits = 0, start = 0;
  int start_count = 0, times = 0, added_sign = 0, e_d = 4, i, j;
  int num_digits, printed_bytes = 0;
  char spc;

  /*-------------------------------------------------------------------*/
  /* If function is called with 'g' and '#', include trailling zeros.  */
  /*-------------------------------------------------------------------*/
  if (!zeros && (flags & NUM_FLAG))
    zeros = WZ;

  /*-------------------------------------------------------------------*/
  /* If buf starts with '-', put sign in front of the number.          */
  /*-------------------------------------------------------------------*/
  if (real_buf[0] == '-')
    added_sign = 1;
  else if (flags & PLUS_FLAG)
    added_sign = 1;
  else if (flags & SPACE_FLAG)
  {
    added_sign = 1;
    real_buf[0] = 32;
  }

  /*-------------------------------------------------------------------*/
  /* Find out how many digits the number will have.                    */
  /*-------------------------------------------------------------------*/
  if (exponent > 99)
   ++e_d;
  if (exponent > 999)
   ++e_d;
  if (!precision && (flags & NUM_FLAG))
   ++e_d;
  num_digits = precision ? 2 + precision + added_sign : 1 + added_sign;
  if (spec != 'f')
  {
    exponent = (real_buf[24] == '-') ? -exponent : exponent;
    d_digits = round_up(real_buf, precision, zeros, &exponent);
  }
  num_digits = (spec == 'f') ? num_digits + exponent : d_digits + e_d;
  if (spec != 'f' && (d_digits == 2) && !(flags & NUM_FLAG))
    --num_digits;
  if (spec == 'f' && !zeros)
    for (i = precision + 2; real_buf[i] == '0'; --i, --num_digits) ;

  /*-------------------------------------------------------------------*/
  /* Get the correct sign for exponent and round up to precision + 1   */
  /* so that 0.9999... won't round up to 1.000... afterwards.          */
  /*-------------------------------------------------------------------*/
  exponent = (real_buf[24] == '-') ? -exponent : exponent;
  round_up(real_buf, precision + 1, zeros, &exponent);
  if (exponent == 0)
    real_buf[24] = '+';

  if (num_digits < width)
  {
    /*-----------------------------------------------------------------*/
    /* Pad, depending on the flag with 0s or spaces.                   */
    /*-----------------------------------------------------------------*/
    if (flags & MINUS_FLAG)
      spaces = width - num_digits;
    else
    {
      spc = (char)((flags & ZERO_FLAG) ? '0' : ' ');

      /*---------------------------------------------------------------*/
      /* If padding with 0s, put the sign first if necessary.          */
      /*---------------------------------------------------------------*/
      if ((spc == '0') && added_sign)
      {
        if (send_char(stream, real_buf[0]))
          return -1;
        ++printed_bytes;
      }
      if (send_chars(stream, spc, width - num_digits))
        return -1;
      printed_bytes += (width - num_digits);
      width = num_digits;
    }
  }

  /*-------------------------------------------------------------------*/
  /* If the sign is not already printed, print it first.               */
  /*-------------------------------------------------------------------*/
  if (!start && ((real_buf[0] == '-') || (real_buf[0] == 32) ||
                 (real_buf[0] == '+')) && added_sign)
  {
    if (send_char(stream, real_buf[0]))
      return -1;
    ++printed_bytes;
  }

  /*-------------------------------------------------------------------*/
  /* Check the specifier the function was called with.                 */
  /*-------------------------------------------------------------------*/
  if (spec == 'f')
  {
    /*-----------------------------------------------------------------*/
    /* Function called with the 'f' specifier.                         */
    /*-----------------------------------------------------------------*/
    for (; exponent < 0; ++times, ++exponent)
    {
      /*---------------------------------------------------------------*/
      /* Put 0s in front and increment exponent till it becomes 0.     */
      /*---------------------------------------------------------------*/
      if (times == 1)
      {
        /*-------------------------------------------------------------*/
        /* Put the decimal point after the first 0.                    */
        /*-------------------------------------------------------------*/
        start_count = 1;
        if (send_char(stream, '.'))
          return -1;
        ++printed_bytes;
      }
      if (start_count)
        ++d_digits;

      /*---------------------------------------------------------------*/
      /* If exponent exceeded the precision, and is negative, stop.    */
      /*---------------------------------------------------------------*/
      if (d_digits > precision)
        return printed_bytes;

      if (send_char(stream, '0'))
        return -1;
      ++printed_bytes;
    }

    if (times == 1)
    {
      /*---------------------------------------------------------------*/
      /* The exponent was -1, so put the decimal point now.            */
      /*---------------------------------------------------------------*/
      start_count = 1;
      if (send_char(stream, '.'))
        return -1;
      ++printed_bytes;
    }

    if (start_count)
    {
      /*---------------------------------------------------------------*/
      /* A decimal point was already printed, followed by 0s.          */
      /*---------------------------------------------------------------*/
      precision = precision - (d_digits + 1);

      /*---------------------------------------------------------------*/
      /* Finish printing decimal digits until precision.               */
      /*---------------------------------------------------------------*/
      d_digits = round_up(real_buf, precision, zeros, &exponent);
      for (j = 1; j <= d_digits; ++j)
      {
        if (j != 2)
        {
          if (send_char(stream, real_buf[j]))
            return -1;
          ++printed_bytes;
        }
      }

      /*---------------------------------------------------------------*/
      /* Pad, if necessary, with spaces at the end.                    */
      /*---------------------------------------------------------------*/
      if (send_chars(stream, ' ', spaces))
        return -1;
      printed_bytes += spaces;

      return printed_bytes;
    }

    /*-----------------------------------------------------------------*/
    /* If exponent is positive, find out where the decimal digit       */
    /* should be in the buffer.                                        */
    /*-----------------------------------------------------------------*/
    if (exponent > 0)
    {
      change_point = 1;
      when = 2 + exponent;
    }

    /*-----------------------------------------------------------------*/
    /* If precision or # flag specified, print the decimal point.      */
    /*-----------------------------------------------------------------*/
    if (precision || (flags & NUM_FLAG))
    {
      if (precision + exponent < 14)
        d_digits = round_up(real_buf, precision + exponent, zeros,
                            &exponent);
      else
        d_digits = round_up(real_buf, 13, zeros, &exponent);
      for (i = 1; i <= d_digits; ++i)
      {
        /*-------------------------------------------------------------*/
        /* If there are 0s in front, skip the decimal point.           */
        /*-------------------------------------------------------------*/
        if (!((times || change_point) && (real_buf[i] == '.')))
        {
          exponent = (change_point && (i > 2)) ? exponent - 1 :
                                                 exponent;

          /*-----------------------------------------------------------*/
          /* If we haven't reached the decimal point, send digits.     */
          /*-----------------------------------------------------------*/
          if (real_buf[i] != '.')
          {
            if (send_char(stream, real_buf[i]))
              return -1;
            ++printed_bytes;
          }

          /*-----------------------------------------------------------*/
          /* If we've reached decimal point, or its place, write it.   */
          /*-----------------------------------------------------------*/
          if ((change_point && (i == when)) || (real_buf[i] == '.'))
          {
            if ((i < d_digits) || (flags & NUM_FLAG))
            {
              if (send_char(stream, '.'))
                return -1;
              ++printed_bytes;
            }

            for (j = 0; (j < precision) && (i < d_digits); ++j)
            {
              /*-------------------------------------------------------*/
              /* Send precision digits to be printed after decimal pt. */
              /*-------------------------------------------------------*/
              ++i;

              /*-------------------------------------------------------*/
              /* If i < 23 send decimal digit, else send '0'.          */
              /*-------------------------------------------------------*/
              if (i < 23)
              {
                if (send_char(stream, real_buf[i]))
                  return -1;
              }
              else
              {
                if (send_char(stream, '0'))
                  return -1;
              }
              ++printed_bytes;
            }

            /*---------------------------------------------------------*/
            /* Pad with spaces if necessary.                           */
            /*---------------------------------------------------------*/
            if (send_chars(stream, ' ', spaces))
              return -1;
            printed_bytes += spaces;

            return printed_bytes;
          }
        }
      }
    }

    /*-----------------------------------------------------------------*/
    /* Else precision was 0, so send only digit before the decimal pt. */
    /*-----------------------------------------------------------------*/
    else
    {
      if (send_char(stream, real_buf[1]))
        return -1;
      ++printed_bytes;
    }

    /*-----------------------------------------------------------------*/
    /* Put 0s at the back and decrement exponent till it becomes 0.    */
    /*-----------------------------------------------------------------*/
    if (send_chars(stream, '0', exponent))
      return -1;
    printed_bytes += exponent;
    exponent = 0;

    if (precision && zeros)
    {
      if (send_char(stream, '.'))
        return -1;
      ++printed_bytes;

      /*---------------------------------------------------------------*/
      /* Fill in with 0s until precision digits if necessary.          */
      /*---------------------------------------------------------------*/
      if (d_digits < precision)
      {
        if (send_chars(stream, '0', precision - d_digits))
          return -1;
        printed_bytes += (precision - d_digits);
      }
    }
  }

  /*-------------------------------------------------------------------*/
  /* Else function called with the 'e' or 'E' specifier.               */
  /*-------------------------------------------------------------------*/
  else
  {
    /*-----------------------------------------------------------------*/
    /* Print the decimal point if precision or the # flag specified.   */
    /*-----------------------------------------------------------------*/
    if ((d_digits == 2) && (flags & NUM_FLAG))
    {
      if (send_buf(stream, &real_buf[1], 2))
        return -1;
      printed_bytes += 2;
    }
    else if (d_digits == 2)
    {
      if (send_char(stream, real_buf[1]))
        return -1;
      ++printed_bytes;
    }
    else
    {
      if (send_buf(stream, &real_buf[1], d_digits))
        return -1;
      printed_bytes += d_digits;
    }

    /*-----------------------------------------------------------------*/
    /* Print exponent in the form (e/E) (+/-) xxx.                     */
    /*-----------------------------------------------------------------*/
    if (send_char(stream, spec))
      return -1;
    ++printed_bytes;

    for (i = 24; i < 29; ++i)
      if (!(((i == 25) && (real_buf[i] == '0')) ||
         ((i == 26) && (real_buf[25] == '0') && (real_buf[i] == '0'))))
      {
        if (send_char(stream, real_buf[i]))
          return -1;
        ++printed_bytes;
      }
  }

  /*-------------------------------------------------------------------*/
  /* Pad, if necessary, with spaces at the end.                        */
  /*-------------------------------------------------------------------*/
  if (send_chars(stream, ' ', spaces))
    return -1;
  printed_bytes += spaces;

  return printed_bytes;
}

/***********************************************************************/
/*   store_buf: Store the value of the real into a string              */
/*                                                                     */
/*      Inputs: val = the real value to be stored                      */
/*              buf = string in which value is stored                  */
/*                                                                     */
/***********************************************************************/
static void store_buf(double val, char *buf)
{
  int exponent = 0, index;
  double mantissa, f, i, decimal;
  char temp_buf[4];

  /*-------------------------------------------------------------------*/
  /* If val is negative, put '-' sign, else put '+' sign.              */
  /*-------------------------------------------------------------------*/
  if (val < 0)
  {
    buf[0] = '-';
    val = -val;
  }
  else
    buf[0] = '+';

  f = val;

  /*-------------------------------------------------------------------*/
  /* Force val between 10 and 1 and store exponent.                    */
  /*-------------------------------------------------------------------*/
  if (val > 10 || val <= 1)
    exponent = get_exponent(&f);

  mantissa = f;
  for (index = 1; index < 17; ++index)
  {
    /*-----------------------------------------------------------------*/
    /* Get the mantissa.                                               */
    /*-----------------------------------------------------------------*/
    if (index != 2)
    {
      decimal = modf(mantissa, &i);
      mantissa = 10 * decimal;
      buf[index] = (char)(i + '0');
    }
    else
      buf[index] = '.';
  }

  for (index = 17; index < 23; ++index)
  {
    /*-----------------------------------------------------------------*/
    /* Fill the rest of the mantissa with 0s (x.xxx...000).            */
    /*-----------------------------------------------------------------*/
    buf[index] = '0';
  }

  /*-------------------------------------------------------------------*/
  /* Store the exponent as E+/-xxxx.                                   */
  /*-------------------------------------------------------------------*/
  buf[23] = 'E';

  /*-------------------------------------------------------------------*/
  /* Store the sign of the exponent (required); either '+' or '-'.     */
  /*-------------------------------------------------------------------*/
  if (exponent < 0)
  {
    exponent = 0 - exponent;
    buf[24] = '-';
  }
  else
    buf[24] = '+';

  for (index = 0; index < 4; ++index)
  {
    /*-----------------------------------------------------------------*/
    /* If exponent < 10, store it and set it to 0, else store it and   */
    /* divide it by 10.                                                */
    /*-----------------------------------------------------------------*/
    if (exponent < 10)
    {
      temp_buf[index] = (char)(exponent + '0');
      if (exponent)
        exponent = 0;
    }
    else
    {
      temp_buf[index] = (char)(exponent % 10 + '0');
      exponent = exponent / 10;
    }
  }

  for (index = 3; index >= 0; index--)
  {
    /*-----------------------------------------------------------------*/
    /* Store the value of the exponent (temp_buf) in buf.              */
    /*-----------------------------------------------------------------*/
    buf[28 - index] = temp_buf[index];
  }
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
  int width, precision, sign, e, p, length, normal_chars = 0;
  int precision_flag = 0, exponent;
  char buf[BUF_BYTES + 1], real_buf[29];
  i8 *str, c, spec;
  ui32 unsigned_val;
  i32 val;
  long double real_num;

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
      width = precision_flag = 0;
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
      /* Else accumulate normal characters.                            */
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
      /* Else if c is '*', get integer value from list of arguments.   */
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
      /* If width is negative, same as having a minus sign in flags.   */
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
      /* Else no special character found, continue.                    */
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
        precision_flag = 1;
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
      else if (c == 'f' || c == 'e' || c == 'E' || c == 'g' || c == 'G')
      {
        /*-------------------------------------------------------------*/
        /* Sets e to distinguish later between 'e'('g') and 'E'('G').  */
        /*-------------------------------------------------------------*/
        state = 18;
        e = ((c == 'e') || (c == 'g')) ? 1 : 0;
        spec = c;
      }
      else
      {
        /*-------------------------------------------------------------*/
        /* No valid specifiers following %, go back to initial state.  */
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

    /*-----------------------------------------------------------------*/
    /*  State 18 handles the 'f', 'e', 'E', 'g', 'G' specifiers.       */
    /*-----------------------------------------------------------------*/
    case 18:
      /*---------------------------------------------------------------*/
      /* If precision flag not mentioned, assume default value (6).    */
      /*---------------------------------------------------------------*/
      if (!precision_flag)
        precision = 6;

      /*---------------------------------------------------------------*/
      /* If precision exceeds 15, set it to 15.                        */
      /*---------------------------------------------------------------*/
      if (precision > 15)
        precision = 15;

      /*---------------------------------------------------------------*/
      /* Retrieve the real to be printed.                              */
      /*---------------------------------------------------------------*/
      real_num = va_arg(list_of_args, double);

      /*---------------------------------------------------------------*/
      /* If real is not a number, ignore all other flags.              */
      /*---------------------------------------------------------------*/
      if (real_num != real_num)
      {
        if (stream->write(stream, (const ui8*)"NaN", 3) != 3)
          return -1;
        count += 3;
        state = 0;
        break;
      }

      /*---------------------------------------------------------------*/
      /* If real is infinity, ignore all other flags.                  */
      /*---------------------------------------------------------------*/
      if ((real_num > 1 || real_num < -1) &&
          real_num / 10e20 == real_num)
      {
        if (stream->write(stream, (const ui8*)"Inf", 3) != 3)
          return -1;
        count += 3;
        state = 0;
        break;
      }

      /*---------------------------------------------------------------*/
      /* Print real according to printf flags.                         */
      /*---------------------------------------------------------------*/
      store_buf(real_num, real_buf);
      switch (spec)
      {
        case 'f':
        case 'e':
        case 'E':
          bytes = print_rbuf(stream, real_buf, width, flags, spec,
                             precision, WZ);
          if (bytes == -1)
            return -1;

          count += bytes;
          break;

        case 'g':
        case 'G':
          /*-----------------------------------------------------------*/
          /* Minimum precision with 'g' or 'G' is 1.                   */
          /*-----------------------------------------------------------*/
          if (precision < 1)
            precision = 1;

          /*-----------------------------------------------------------*/
          /* Figure out the value of the exponent from the 4 digits in */
          /* real_buf[25] through real_buf[28].                        */
          /*-----------------------------------------------------------*/
          exponent = 1000 * (real_buf[25] - '0') +
                     100 * (real_buf[26] - '0') +
                     10 * (real_buf[27] - '0') +
                     real_buf[28] - '0';

          /*-----------------------------------------------------------*/
          /* Figure out the sign of the exponent.                      */
          /*-----------------------------------------------------------*/
          exponent = (real_buf[24] == '-') ? -exponent : exponent;

          /*-----------------------------------------------------------*/
          /* If exp >= -4 and < precision, print it as if with  an 'f' */
          /* specifier, else print it as with 'e'/'E' specifier.       */
          /*-----------------------------------------------------------*/
          if ((exponent >= -4) && (exponent < precision))
            bytes = print_rbuf(stream, real_buf, width, flags, 'f',
                               precision, WOZ);
          else
            bytes = print_rbuf(stream, real_buf, width, flags,
                               (char)(spec - 2), precision, WOZ);
          if (bytes == -1)
            return -1;

          count += bytes;
          break;

        default:
          break;
      }
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

