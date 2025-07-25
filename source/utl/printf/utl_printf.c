/**
 * @author (c) Eyal Rozenberg <eyalroz1@gmx.com>
 *             2021, Haifa, Palestine/Israel
 * @author (c) Marco Paland (info@paland.com)
 *             2014-2019, PALANDesign Hannover, Germany
 *
 * @note Others have made smaller contributions to this file: see the
 * contributors page at https://github.com/eyalroz/printf/graphs/contributors
 * or ask one of the authors.
 *
 * @brief Small stand-alone implementation of the printf family of functions
 * (`(v)printf`, `(v)s(n)printf` etc., geared towards use on embedded systems with
 * a very limited resources.
 *
 * @note the implementations are thread-safe; re-entrant; use no functions from
 * the standard library; and do not dynamically allocate any memory.
 *
 * @license The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdbool.h>
#include <stdint.h>

#include "utl_printf.h"


// Define this globally (e.g. gcc -DPRINTF_INCLUDE_CONFIG_H ...) to include the
// printf_config.h header file
// default: undefined
#ifdef PRINTF_INCLUDE_CONFIG_H
#include "utl_printf_config.h"
#endif


// 'ntoa' conversion buffer size, this must be big enough to hold one converted
// numeric number including padded zeros (dynamically created on stack)
#ifndef PRINTF_NTOA_BUFFER_SIZE
#define PRINTF_NTOA_BUFFER_SIZE    32
#endif

// 'ftoa' conversion buffer size, this must be big enough to hold one converted
// float number including padded zeros (dynamically created on stack)
#ifndef PRINTF_FTOA_BUFFER_SIZE
#define PRINTF_FTOA_BUFFER_SIZE    32
#endif

// Support for the decimal notation floating point conversion specifiers (%f, %F)
#ifndef PRINTF_SUPPORT_FLOAT_SPECIFIERS
#define PRINTF_SUPPORT_FLOAT_SPECIFIERS 1
#endif

// Support for the exponential notatin floating point conversion specifiers (%e, %g, %E, %G)
#ifndef PRINTF_SUPPORT_EXPONENTIAL_SPECIFIERS
#define PRINTF_SUPPORT_EXPONENTIAL_SPECIFIERS 1
#endif

// Default precision for the floating point conversion specifiers (the C standard sets this at 6)
#ifndef PRINTF_DEFAULT_FLOAT_PRECISION
#define PRINTF_DEFAULT_FLOAT_PRECISION  6
#endif

// According to the C languages standard, printf() and related functions must be able to print any
// integral number in floating-point notation, regardless of length, when using the %f specifier -
// possibly hundreds of characters, potentially overflowing your buffers. In this implementation,
// all values beyond this threshold are switched to exponential notation.
#ifndef PRINTF_MAX_INTEGRAL_DIGITS_FOR_DECIMAL
#define PRINTF_MAX_INTEGRAL_DIGITS_FOR_DECIMAL 9
#endif

// Support for the long long integral types (with the ll, z and t length modifiers for specifiers
// %d,%i,%o,%x,%X,%u, and with the %p specifier). Note: 'L' (long double) is not supported.
#ifndef PRINTF_SUPPORT_LONG_LONG
#define PRINTF_SUPPORT_LONG_LONG 1
#endif

// Support for the ptrdiff_t length modifier (%t)
// ptrdiff_t is normally defined in <stddef.h> as long or long long type
#ifndef PRINTF_SUPPORT_PTRDIFF_LENGTH_MODIFIER
#define PRINTF_SUPPORT_PTRDIFF_LENGTH_MODIFIER 1
#endif

#if PRINTF_SUPPORT_LONG_LONG
#define NTOA_VALUE_TYPE unsigned long long
#else
#define NTOA_VALUE_TYPE unsigned long
#endif

#define PRINTF_PREFER_DECIMAL     false
#define PRINTF_PREFER_EXPONENTIAL true

///////////////////////////////////////////////////////////////////////////////

// The following will convert the number-of-digits into an exponential-notation literal
#define PRINTF_CONCATENATE(s1, s2) s1##s2
#define PRINTF_EXPAND_THEN_CONCATENATE(s1, s2) PRINTF_CONCATENATE(s1, s2)
#define PRINTF_FLOAT_NOTATION_THRESHOLD PRINTF_EXPAND_THEN_CONCATENATE(1e,PRINTF_MAX_INTEGRAL_DIGITS_FOR_DECIMAL)

// internal flag definitions
#define FLAGS_ZEROPAD   (1U <<  0U)
#define FLAGS_LEFT      (1U <<  1U)
#define FLAGS_PLUS      (1U <<  2U)
#define FLAGS_SPACE     (1U <<  3U)
#define FLAGS_HASH      (1U <<  4U)
#define FLAGS_UPPERCASE (1U <<  5U)
#define FLAGS_CHAR      (1U <<  6U)
#define FLAGS_SHORT     (1U <<  7U)
#define FLAGS_LONG      (1U <<  8U)
#define FLAGS_LONG_LONG (1U <<  9U)
#define FLAGS_PRECISION (1U << 10U)
#define FLAGS_ADAPT_EXP (1U << 11U)
#define FLAGS_POINTER   (1U << 12U)
// Note: Similar, but not identical, effect as FLAGS_HASH

#define BASE_BINARY    2
#define BASE_OCTAL     8
#define BASE_DECIMAL  10
#define BASE_HEX      16

typedef uint8_t numeric_base_t;

// import float.h for DBL_MAX
#if PRINTF_SUPPORT_FLOAT_SPECIFIERS
#include <float.h>
#endif

// Note in particular the behavior here on LONG_MIN or LLONG_MIN; it is valid
// and well-defined, but if you're not careful you can easily trigger undefined
// behavior with -LONG_MIN or -LLONG_MIN
#define NTOA_ABS(_x) ( (_x) > 0 ? (NTOA_VALUE_TYPE)(_x) : -((NTOA_VALUE_TYPE)_x) )

#define PRINTF_ABS(_x) ( (_x) > 0 ? (_x) : -(_x) )

typedef union {
  uint64_t U;
  double   F;
} double_with_bit_access;

// This is unnecessary in C99, since compound initializers can be used,
// but: 1. Some compilers are finicky about this; 2. Some people may want to convert this to C89;
// 3. If you try to use it as C++, only C++20 supports compound literals
static inline double_with_bit_access get_bit_access(double x)
{
  double_with_bit_access dwba;
  dwba.F = x;
  return dwba;
}

static inline int get_sign(double x)
{
  // The sign is in the highest bit, bit 63
  return get_bit_access(x).U >> 63U;
}

static inline int get_exp2(double_with_bit_access x)
{
  // The exponent is in bits 52...62 inclusive, but the exponent is signed, not unsigned.
  // Also, it's not signed like regular signed integers: The range is mapped to -1023..1024
  // (and -1023 and 1024 are reserved for special use, but we ignore that here).
  return (int)((x.U >> 52U) & 0x07FFU) - 1023;
}


// output function type
typedef void (*out_fct_type)(char character, void* buffer, size_t idx, size_t maxlen);


// wrapper (used as buffer) for output function type
typedef struct {
  void  (*fct)(char character, void* arg);
  void* arg;
} out_fct_wrap_type;


// internal buffer output
static inline void _out_buffer(char character, void* buffer, size_t idx, size_t maxlen)
{
  if (idx < maxlen) {
    ((char*)buffer)[idx] = character;
  }
}


// internal null output
static inline void _out_null(char character, void* buffer, size_t idx, size_t maxlen)
{
  (void)character; (void)buffer; (void)idx; (void)maxlen;
}


// internal _putchar wrapper
extern int __io_putchar(int ch) __attribute__((weak));
static inline void _out_char(char character, void* buffer, size_t idx, size_t maxlen)
{
  (void)buffer; (void)idx; (void)maxlen;
  if (character) {
    __io_putchar(character);
  }
}


// internal output function wrapper
static inline void _out_fct(char character, void* buffer, size_t idx, size_t maxlen)
{
  (void)idx; (void)maxlen;
  if (character) {
    // buffer is the output fct pointer
    ((out_fct_wrap_type*)buffer)->fct(character, ((out_fct_wrap_type*)buffer)->arg);
  }
}


// internal secure strlen
// @return The length of the string (excluding the terminating 0) limited by 'maxsize'
static inline unsigned int _strnlen_s(const char* str, size_t maxsize)
{
  const char* s;
  for (s = str; *s && maxsize--; ++s);
  return (unsigned int)(s - str);
}


// internal test if char is a digit (0-9)
// @return true if char is a digit
static inline bool _is_digit(char ch)
{
  return (ch >= '0') && (ch <= '9');
}


// internal ASCII string to unsigned int conversion
static unsigned int _atoi(const char** str)
{
  unsigned int i = 0U;
  while (_is_digit(**str)) {
    i = i * 10U + (unsigned int)(*((*str)++) - '0');
  }
  return i;
}


// output the specified string in reverse, taking care of any zero-padding
static size_t _out_rev(out_fct_type out, char* buffer, size_t idx, size_t maxlen, const char* buf, size_t len, unsigned int width, unsigned int flags)
{
  const size_t start_idx = idx;

  // pad spaces up to given width
  if (!(flags & FLAGS_LEFT) && !(flags & FLAGS_ZEROPAD)) {
    for (size_t i = len; i < width; i++) {
      out(' ', buffer, idx++, maxlen);
    }
  }

  // reverse string
  while (len) {
    out(buf[--len], buffer, idx++, maxlen);
  }

  // append pad spaces up to given width
  if (flags & FLAGS_LEFT) {
    while (idx - start_idx < width) {
      out(' ', buffer, idx++, maxlen);
    }
  }

  return idx;
}


// internal itoa format
static size_t _ntoa_format(out_fct_type out, char* buffer, size_t idx, size_t maxlen, char* buf, size_t len, bool negative, numeric_base_t base, unsigned int precision, unsigned int width, unsigned int flags)
{
  size_t unpadded_len = len;

  // pad with leading zeros
  {
    if (!(flags & FLAGS_LEFT)) {
      if (width && (flags & FLAGS_ZEROPAD) && (negative || (flags & (FLAGS_PLUS | FLAGS_SPACE)))) {
        width--;
      }
      while ((flags & FLAGS_ZEROPAD) && (len < width) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
        buf[len++] = '0';
      }
    }

    while ((len < precision) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = '0';
    }

    if (base == BASE_OCTAL && (len > unpadded_len)) {
      // Since we've written some zeros, we've satisfied the alternative format leading space requirement
      flags &= ~FLAGS_HASH;
    }
  }

  // handle hash
  if (flags & (FLAGS_HASH | FLAGS_POINTER)) {
    if (!(flags & FLAGS_PRECISION) && len && ((len == precision) || (len == width))) {
      // Let's take back some padding digits to fit in what will eventually
      // be the format-specific prefix
      if (unpadded_len < len) {
        len--;
      }
      if (len && (base == BASE_HEX)) {
        if (unpadded_len < len) {
          len--;
        }
      }
    }
    if ((base == BASE_HEX) && !(flags & FLAGS_UPPERCASE) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = 'x';
    }
    else if ((base == BASE_HEX) && (flags & FLAGS_UPPERCASE) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = 'X';
    }
    else if ((base == BASE_BINARY) && (len < PRINTF_NTOA_BUFFER_SIZE)) {
      buf[len++] = 'b';
    }
    if (len < PRINTF_NTOA_BUFFER_SIZE) {
      buf[len++] = '0';
    }
  }

  if (len < PRINTF_NTOA_BUFFER_SIZE) {
    if (negative) {
      buf[len++] = '-';
    }
    else if (flags & FLAGS_PLUS) {
      buf[len++] = '+';  // ignore the space if the '+' exists
    }
    else if (flags & FLAGS_SPACE) {
      buf[len++] = ' ';
    }
  }

  return _out_rev(out, buffer, idx, maxlen, buf, len, width, flags);
}


// internal itoa
static size_t _ntoa(out_fct_type out, char* buffer, size_t idx, size_t maxlen, NTOA_VALUE_TYPE value, bool negative, numeric_base_t base, unsigned int precision, unsigned int width, unsigned int flags)
{
  char buf[PRINTF_NTOA_BUFFER_SIZE];
  size_t len = 0U;

  if (!value) {
    if ( !(flags & FLAGS_PRECISION) ) {
      buf[len++] = '0';
      flags &= ~FLAGS_HASH;
      // We drop this flag this since either the alternative and regular modes of the specifier
      // don't differ on 0 values, or (in the case of octal) we've already provided the special
      // handling for this mode.
    }
    else if (base == BASE_HEX) {
      flags &= ~FLAGS_HASH;
      // We drop this flag this since either the alternative and regular modes of the specifier
      // don't differ on 0 values
    }
  }
  else {
    do {
      const char digit = (char)(value % base);
      buf[len++] = (char)(digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10);
      value /= base;
    } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
  }

  return _ntoa_format(out, buffer, idx, maxlen, buf, len, negative, base, precision, width, flags);
}

#if PRINTF_SUPPORT_FLOAT_SPECIFIERS

struct double_components {
  int_fast64_t integral;
  int_fast64_t fractional;
  bool is_negative;
};

#define NUM_DECIMAL_DIGITS_IN_INT64_T 18
#define PRINTF_MAX_PRECOMPUTED_POWER_OF_10  NUM_DECIMAL_DIGITS_IN_INT64_T
static const double powers_of_10[NUM_DECIMAL_DIGITS_IN_INT64_T] = {
  1e00, 1e01, 1e02, 1e03, 1e04, 1e05, 1e06, 1e07, 1e08,
  1e09, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17
};

#define PRINTF_MAX_SUPPORTED_PRECISION NUM_DECIMAL_DIGITS_IN_INT64_T - 1


// Break up a double number - which is known to be a finite non-negative number -
// into its base-10 parts: integral - before the decimal point, and fractional - after it.
// Taken the precision into account, but does not change it even internally.
static struct double_components get_components(double number, unsigned int precision)
{
  struct double_components number_;
  number_.is_negative = get_sign(number);
  double abs_number = (number_.is_negative) ? -number : number;
  number_.integral = (int_fast64_t)abs_number;
  double remainder = (abs_number - number_.integral) * powers_of_10[precision];
  number_.fractional = (int_fast64_t)remainder;

  remainder -= (double) number_.fractional;

  if (remainder > 0.5) {
    ++number_.fractional;
    // handle rollover, e.g. case 0.99 with precision 1 is 1.0
    if ((double) number_.fractional >= powers_of_10[precision]) {
      number_.fractional = 0;
      ++number_.integral;
    }
  }
  else if (remainder == 0.5) {
    if ((number_.fractional == 0U) || (number_.fractional & 1U)) {
      // if halfway, round up if odd OR if last digit is 0
      ++number_.fractional;
    }
  }

  if (precision == 0U) {
    remainder = abs_number - (double) number_.integral;
    if ((!(remainder < 0.5) || (remainder > 0.5)) && (number_.integral & 1)) {
      // exactly 0.5 and ODD, then round up
      // 1.5 -> 2, but 2.5 -> 2
      ++number_.integral;
    }
  }
  return number_;
}

struct scaling_factor {
  double raw_factor;
  bool multiply; // if true, need to multiply by raw_factor; otherwise need to divide by it
};

double apply_scaling(double num, struct scaling_factor normalization)
{
  return normalization.multiply ? num * normalization.raw_factor : num / normalization.raw_factor;
}

double unapply_scaling(double normalized, struct scaling_factor normalization)
{
  return normalization.multiply ? normalized / normalization.raw_factor : normalized * normalization.raw_factor;
}

struct scaling_factor update_normalization(struct scaling_factor sf, double extra_multiplicative_factor)
{
  struct scaling_factor result;
  if (sf.multiply) {
    result.multiply = true;
    result.raw_factor = sf.raw_factor * extra_multiplicative_factor;
  }
  else {
    int factor_exp2 = get_exp2(get_bit_access(sf.raw_factor));
    int extra_factor_exp2 = get_exp2(get_bit_access(extra_multiplicative_factor));

    // Divide the larger-exponent raw raw_factor by the smaller
    if (PRINTF_ABS(factor_exp2) > PRINTF_ABS(extra_factor_exp2)) {
      result.multiply = false;
      result.raw_factor = sf.raw_factor / extra_multiplicative_factor;
    }
    else {
      result.multiply = true;
      result.raw_factor = extra_multiplicative_factor / sf.raw_factor;
    }
  }
  return result;
}

static struct double_components get_normalized_components(bool negative, unsigned int precision, double non_normalized, struct scaling_factor normalization)
{
  struct double_components components;
  components.is_negative = negative;
  components.integral = (int_fast64_t) apply_scaling(non_normalized, normalization);
  double remainder = non_normalized - unapply_scaling(components.integral, normalization);
  double prec_power_of_10 = powers_of_10[precision];
  struct scaling_factor account_for_precision = update_normalization(normalization, prec_power_of_10);
  double scaled_remainder = apply_scaling(remainder, account_for_precision);
  double rounding_threshold = 0.5;

  if (precision == 0U) {
    components.fractional = 0;
    components.integral += (scaled_remainder >= rounding_threshold);
    if (scaled_remainder == rounding_threshold) {
      // banker's rounding: Round towards the even number (making the mean error 0)
      components.integral &= ~((int_fast64_t) 0x1);
    }
  }
  else {
    components.fractional = (int_fast64_t) scaled_remainder;
    scaled_remainder -= components.fractional;

    components.fractional += (scaled_remainder >= rounding_threshold);
    if (scaled_remainder == rounding_threshold) {
      // banker's rounding: Round towards the even number (making the mean error 0)
      components.fractional &= ~((int_fast64_t) 0x1);
    }
    // handle rollover, e.g. the case of 0.99 with precision 1 becoming (0,100),
    // and must then be corrected into (1, 0).
    if ((double) components.fractional >= prec_power_of_10) {
      components.fractional = 0;
      ++components.integral;
    }
  }
  return components;
}


static size_t sprint_broken_up_decimal(
  struct double_components number_, out_fct_type out, char *buffer, size_t idx, size_t maxlen, unsigned int precision,
  unsigned int width, unsigned int flags, char *buf, size_t len)
{
  if (precision != 0U) {
    // do fractional part, as an unsigned number

    unsigned int count = precision;

    if (flags & FLAGS_ADAPT_EXP && !(flags & FLAGS_HASH)) {
      // %g/%G mandates we skip the trailing 0 digits...
      if (number_.fractional > 0) {
        while(true) {
          int_fast64_t digit = number_.fractional % 10U;
          if (digit != 0) {
            break;
          }
          --count;
          number_.fractional /= 10U;
        }

      }
      // ... and even the decimal point if there are no
      // non-zero fractional part digits (see below)
    }

    if (number_.fractional > 0 || !(flags & FLAGS_ADAPT_EXP) || (flags & FLAGS_HASH) ) {
      while (len < PRINTF_FTOA_BUFFER_SIZE) {
        --count;
        buf[len++] = (char)('0' + number_.fractional % 10U);
        if (!(number_.fractional /= 10U)) {
          break;
        }
      }
      // add extra 0s
      while ((len < PRINTF_FTOA_BUFFER_SIZE) && (count-- > 0U)) {
        buf[len++] = '0';
      }
      if (len < PRINTF_FTOA_BUFFER_SIZE) {
        buf[len++] = '.';
      }
    }
  }
  else {
    if (flags & FLAGS_HASH) {
      if (len < PRINTF_FTOA_BUFFER_SIZE) {
        buf[len++] = '.';
      }
    }
  }

  // Write the integer part of the number (it comes after the fractional
  // since the character order is reversed)
  while (len < PRINTF_FTOA_BUFFER_SIZE) {
    buf[len++] = (char)('0' + (number_.integral % 10));
    if (!(number_.integral /= 10)) {
      break;
    }
  }

  // pad leading zeros
  if (!(flags & FLAGS_LEFT) && (flags & FLAGS_ZEROPAD)) {
    if (width && (number_.is_negative || (flags & (FLAGS_PLUS | FLAGS_SPACE)))) {
      width--;
    }
    while ((len < width) && (len < PRINTF_FTOA_BUFFER_SIZE)) {
      buf[len++] = '0';
    }
  }

  if (len < PRINTF_FTOA_BUFFER_SIZE) {
    if (number_.is_negative) {
      buf[len++] = '-';
    }
    else if (flags & FLAGS_PLUS) {
      buf[len++] = '+';  // ignore the space if the '+' exists
    }
    else if (flags & FLAGS_SPACE) {
      buf[len++] = ' ';
    }
  }

  return _out_rev(out, buffer, idx, maxlen, buf, len, width, flags);
}

      // internal ftoa for fixed decimal floating point
static size_t sprint_decimal_number(out_fct_type out, char* buffer, size_t idx, size_t maxlen, double number, unsigned int precision, unsigned int width, unsigned int flags, char* buf, size_t len)
{
  struct double_components value_ = get_components(number, precision);
  return sprint_broken_up_decimal(value_, out, buffer, idx, maxlen, precision, width, flags, buf, len);
}

#if PRINTF_SUPPORT_EXPONENTIAL_SPECIFIERS
// internal ftoa variant for exponential floating-point type, contributed by Martijn Jasperse <m.jasperse@gmail.com>
static size_t sprint_exponential_number(out_fct_type out, char* buffer, size_t idx, size_t maxlen, double number, unsigned int precision, unsigned int width, unsigned int flags, char* buf, size_t len)
{
  const bool negative = get_sign(number);
  // This number will decrease gradually (by factors of 10) as we "extract" the exponent out of it
  double abs_number =  negative ? -number : number;

  int exp10;
  bool abs_exp10_covered_by_powers_table;
  struct scaling_factor normalization;


  // Determine the decimal exponent
  if (abs_number == 0.0) {
    // TODO: This is a special-case for 0.0 (and -0.0); but proper handling is required for denormals more generally.
    exp10 = 0; // ... and no need to set a normalization factor or check the powers table
  }
  else  {
    double_with_bit_access conv = get_bit_access(abs_number);
    {
      // based on the algorithm by David Gay (https://www.ampl.com/netlib/fp/dtoa.c)
      int exp2 = get_exp2(conv);
      conv.U = (conv.U & ((1ULL << 52U) - 1U)) | (1023ULL << 52U);  // drop the exp10 so conv.F is now in [1,2)
      // now approximate log10 from the log2 integer part and an expansion of ln around 1.5
      exp10 = (int)(0.1760912590558 + exp2 * 0.301029995663981 + (conv.F - 1.5) * 0.289529654602168);
      // now we want to compute 10^exp10 but we want to be sure it won't overflow
      exp2 = (int)(exp10 * 3.321928094887362 + 0.5);
      const double z  = exp10 * 2.302585092994046 - exp2 * 0.6931471805599453;
      const double z2 = z * z;
      conv.U = (uint64_t)(exp2 + 1023) << 52U;
      // compute exp(z) using continued fractions, see https://en.wikipedia.org/wiki/Exponential_function#Continued_fractions_for_ex
      conv.F *= 1 + 2 * z / (2 - z + (z2 / (6 + (z2 / (10 + z2 / 14)))));
      // correct for rounding errors
      if (abs_number < conv.F) {
        exp10--;
        conv.F /= 10;
      }
    }
    abs_exp10_covered_by_powers_table = PRINTF_ABS(exp10) < PRINTF_MAX_PRECOMPUTED_POWER_OF_10;
    normalization.raw_factor = abs_exp10_covered_by_powers_table ? powers_of_10[PRINTF_ABS(exp10)] : conv.F;
  }

  // We now begin accounting for the widths of the two parts of our printed field:
  // the decimal part after decimal exponent extraction, and the base-10 exponent part.
  // For both of these, the value of 0 has a special meaning, but not the same one:
  // a 0 exponent-part width means "don't print the exponent"; a 0 decimal-part width
  // means "use as many characters as necessary".

  bool fall_back_to_decimal_only_mode = false;
  if (flags & FLAGS_ADAPT_EXP) {
    int required_significant_digits = (precision == 0) ? 1 : (int) precision;
    // Should we want to fall-back to "%f" mode, and only print the decimal part?
    fall_back_to_decimal_only_mode = (exp10 >= -4 && exp10 < required_significant_digits);
    // Now, let's adjust the precision
    // This also decided how we adjust the precision value - as in "%g" mode,
    // "precision" is the number of _significant digits_, and this is when we "translate"
    // the precision value to an actual number of decimal digits.
    int precision_ = (fall_back_to_decimal_only_mode) ?
        (int) precision - 1 - exp10 :
        (int) precision - 1; // the presence of the exponent ensures only one significant digit comes before the decimal point
    precision = (precision_ > 0 ? (unsigned) precision_ : 0U);
    flags |= FLAGS_PRECISION;   // make sure sprint_broken_up_decimal respects our choice above
  }

  normalization.multiply = (exp10 < 0 && abs_exp10_covered_by_powers_table);
  bool should_skip_normalization = (fall_back_to_decimal_only_mode || exp10 == 0);
  struct double_components decimal_part_components =
    should_skip_normalization ?
    get_components(negative ? -abs_number : abs_number, precision) :
    get_normalized_components(negative, precision, abs_number, normalization);

  // Account for roll-over, e.g. rounding from 9.99 to 100.0 - which effects
  // the exponent and may require additional tweaking of the parts
  if (fall_back_to_decimal_only_mode) {
    if ( (flags & FLAGS_ADAPT_EXP) && exp10 >= -1 && decimal_part_components.integral == powers_of_10[exp10 + 1]) {
      exp10++; // Not strictly necessary, since exp10 is no longer really used
      precision--;
      // ... and it should already be the case that decimal_part_components.fractional == 0
    }
    // TODO: What about rollover strictly within the fractional part?
  }
  else {
    if (decimal_part_components.integral >= 10) {
      exp10++;
      decimal_part_components.integral = 1;
      decimal_part_components.fractional = 0;
    }
  }

  // the exp10 format is "E%+03d" and largest number is "307", so set aside 4-5 characters
  unsigned int exp10_part_width = fall_back_to_decimal_only_mode ?
    0U : ((exp10 < 100) && (exp10 > -100)) ? 4U : 5U;

  unsigned int decimal_part_width =
    ((flags & FLAGS_LEFT) && exp10_part_width) ?
      // We're padding on the right, so the width constraint is the exponent part's
      // problem, not the decimal part's, so we'll use as many characters as we need:
      0U :
      // We're padding on the left; so the width constraint is the decimal part's
      // problem. Well, can both the decimal part and the exponent part fit within our overall width?
      ((width > exp10_part_width) ?
        // Yes, so we limit our decimal part's width.
        // (Note this is trivially valid even if we've fallen back to "%f" mode)
        width - exp10_part_width :
        // No; we just give up on any restriction on the decimal part and use as many
        // characters as we need
        0U);

  const size_t start_idx = idx;
  idx = sprint_broken_up_decimal(decimal_part_components, out, buffer, idx, maxlen, precision, decimal_part_width, flags, buf, len);

  if (! fall_back_to_decimal_only_mode) {
    out((flags & FLAGS_UPPERCASE) ? 'E' : 'e', buffer, idx++, maxlen);
    idx = _ntoa(out, buffer, idx, maxlen,
                NTOA_ABS(exp10),
                exp10 < 0, 10, 0, exp10_part_width - 1,
                FLAGS_ZEROPAD | FLAGS_PLUS);
    if (flags & FLAGS_LEFT) {
      // We need to right-pad with spaces to meet the width requirement
      while (idx - start_idx < width) out(' ', buffer, idx++, maxlen);
    }
  }
  return idx;
}
#endif  // PRINTF_SUPPORT_EXPONENTIAL_SPECIFIERS


static size_t sprint_floating_point(out_fct_type out, char* buffer, size_t idx, size_t maxlen, double value, unsigned int precision, unsigned int width, unsigned int flags, bool prefer_exponential)
{
  char buf[PRINTF_FTOA_BUFFER_SIZE];
  size_t len  = 0U;

  // test for special values
  if (value != value)
    return _out_rev(out, buffer, idx, maxlen, "nan", 3, width, flags);
  if (value < -DBL_MAX)
    return _out_rev(out, buffer, idx, maxlen, "fni-", 4, width, flags);
  if (value > DBL_MAX)
    return _out_rev(out, buffer, idx, maxlen, (flags & FLAGS_PLUS) ? "fni+" : "fni", (flags & FLAGS_PLUS) ? 4U : 3U, width, flags);

  if (!prefer_exponential && ((value > PRINTF_FLOAT_NOTATION_THRESHOLD) || (value < -PRINTF_FLOAT_NOTATION_THRESHOLD))) {
    // The required behavior of standard printf is to print _every_ integral-part digit -- which could mean
    // printing hundreds of characters, overflowing any fixed internal buffer and necessitating a more complicated
    // implementation.
#if PRINTF_SUPPORT_EXPONENTIAL_SPECIFIERS
    return sprint_exponential_number(out, buffer, idx, maxlen, value, precision, width, flags, buf, len);
#else
    return 0U;
#endif
  }

  // set default precision, if not set explicitly
  if (!(flags & FLAGS_PRECISION)) {
    precision = PRINTF_DEFAULT_FLOAT_PRECISION;
  }

  // limit precision so that our integer holding the fractional part does not overflow
  while ((len < PRINTF_FTOA_BUFFER_SIZE) && (precision > PRINTF_MAX_SUPPORTED_PRECISION)) {
    buf[len++] = '0'; // This respects the precision in terms of result length only
    precision--;
  }

  return prefer_exponential ?
         sprint_exponential_number(out, buffer, idx, maxlen, value, precision, width, flags, buf, len) :
         sprint_decimal_number(out, buffer, idx, maxlen, value, precision, width, flags, buf, len);
}

#endif  // PRINTF_SUPPORT_FLOAT_SPECIFIERS

// internal vsnprintf
static int _vsnprintf(out_fct_type out, char* buffer, const size_t maxlen, const char* format, va_list va)
{
  unsigned int flags, width, precision, n;
  size_t idx = 0U;

  if (!buffer) {
    // use null output function
    out = _out_null;
  }

  while (*format)
  {
    // format specifier?  %[flags][width][.precision][length]
    if (*format != '%') {
      // no
      out(*format, buffer, idx++, maxlen);
      format++;
      continue;
    }
    else {
      // yes, evaluate it
      format++;
    }

    // evaluate flags
    flags = 0U;
    do {
      switch (*format) {
        case '0': flags |= FLAGS_ZEROPAD; format++; n = 1U; break;
        case '-': flags |= FLAGS_LEFT;    format++; n = 1U; break;
        case '+': flags |= FLAGS_PLUS;    format++; n = 1U; break;
        case ' ': flags |= FLAGS_SPACE;   format++; n = 1U; break;
        case '#': flags |= FLAGS_HASH;    format++; n = 1U; break;
        default :                                   n = 0U; break;
      }
    } while (n);

    // evaluate width field
    width = 0U;
    if (_is_digit(*format)) {
      width = _atoi(&format);
    }
    else if (*format == '*') {
      const int w = va_arg(va, int);
      if (w < 0) {
        flags |= FLAGS_LEFT;    // reverse padding
        width = (unsigned int)-w;
      }
      else {
        width = (unsigned int)w;
      }
      format++;
    }

    // evaluate precision field
    precision = 0U;
    if (*format == '.') {
      flags |= FLAGS_PRECISION;
      format++;
      if (_is_digit(*format)) {
        precision = _atoi(&format);
      }
      else if (*format == '*') {
        const int precision_ = (int)va_arg(va, int);
        precision = precision_ > 0 ? (unsigned int)precision_ : 0U;
        format++;
      }
    }

    // evaluate length field
    switch (*format) {
      case 'l' :
        flags |= FLAGS_LONG;
        format++;
        if (*format == 'l') {
          flags |= FLAGS_LONG_LONG;
          format++;
        }
        break;
      case 'h' :
        flags |= FLAGS_SHORT;
        format++;
        if (*format == 'h') {
          flags |= FLAGS_CHAR;
          format++;
        }
        break;
#if PRINTF_SUPPORT_PTRDIFF_LENGTH_MODIFIER
      case 't' :
        flags |= (sizeof(ptrdiff_t) == sizeof(long) ? FLAGS_LONG : FLAGS_LONG_LONG);
        format++;
        break;
#endif
      case 'j' :
        flags |= (sizeof(intmax_t) == sizeof(long) ? FLAGS_LONG : FLAGS_LONG_LONG);
        format++;
        break;
      case 'z' :
        flags |= (sizeof(size_t) == sizeof(long) ? FLAGS_LONG : FLAGS_LONG_LONG);
        format++;
        break;
        default :
          break;
    }

    // evaluate specifier
    switch (*format) {
      case 'd' :
      case 'i' :
      case 'u' :
      case 'x' :
      case 'X' :
      case 'o' :
      case 'b' : {
        // set the base
        numeric_base_t base;
        if (*format == 'x' || *format == 'X') {
          base = BASE_HEX;
        }
        else if (*format == 'o') {
          base =  BASE_OCTAL;
        }
        else if (*format == 'b') {
          base =  BASE_BINARY;
        }
        else {
          base = BASE_DECIMAL;
          flags &= ~FLAGS_HASH;   // no hash for dec format
        }
        // uppercase
        if (*format == 'X') {
          flags |= FLAGS_UPPERCASE;
        }

        // no plus or space flag for u, x, X, o, b
        if ((*format != 'i') && (*format != 'd')) {
          flags &= ~(FLAGS_PLUS | FLAGS_SPACE);
        }

        // ignore '0' flag when precision is given
        if (flags & FLAGS_PRECISION) {
          flags &= ~FLAGS_ZEROPAD;
        }

        // convert the integer
        if ((*format == 'i') || (*format == 'd')) {
          // signed
          if (flags & FLAGS_LONG_LONG) {
#if PRINTF_SUPPORT_LONG_LONG
            const long long value = va_arg(va, long long);
            idx = _ntoa(out, buffer, idx, maxlen, NTOA_ABS(value), value < 0, base, precision, width, flags);
#endif
          }
          else if (flags & FLAGS_LONG) {
            const long value = va_arg(va, long);
            idx = _ntoa(out, buffer, idx, maxlen, NTOA_ABS(value), value < 0, base, precision, width, flags);
          }
          else {
            const int value = (flags & FLAGS_CHAR) ? (signed char)va_arg(va, int) : (flags & FLAGS_SHORT) ? (short int)va_arg(va, int) : va_arg(va, int);
            idx = _ntoa(out, buffer, idx, maxlen, NTOA_ABS(value), value < 0, base, precision, width, flags);
          }
        }
        else {
          // unsigned
          if (flags & FLAGS_LONG_LONG) {
#if PRINTF_SUPPORT_LONG_LONG
            idx = _ntoa(out, buffer, idx, maxlen, (NTOA_VALUE_TYPE) va_arg(va, unsigned long long), false, base, precision, width, flags);
#endif
          }
          else if (flags & FLAGS_LONG) {
            idx = _ntoa(out, buffer, idx, maxlen, (NTOA_VALUE_TYPE) va_arg(va, unsigned long), false, base, precision, width, flags);
          }
          else {
            const unsigned int value = (flags & FLAGS_CHAR) ? (unsigned char)va_arg(va, unsigned int) : (flags & FLAGS_SHORT) ? (unsigned short int)va_arg(va, unsigned int) : va_arg(va, unsigned int);
            idx = _ntoa(out, buffer, idx, maxlen, (NTOA_VALUE_TYPE) value, false, base, precision, width, flags);
          }
        }
        format++;
        break;
      }
#if PRINTF_SUPPORT_FLOAT_SPECIFIERS
      case 'f' :
      case 'F' :
        if (*format == 'F') flags |= FLAGS_UPPERCASE;
        idx = sprint_floating_point(out, buffer, idx, maxlen, va_arg(va, double), precision, width, flags, PRINTF_PREFER_DECIMAL);
        format++;
        break;
#if PRINTF_SUPPORT_EXPONENTIAL_SPECIFIERS
      case 'e':
      case 'E':
      case 'g':
      case 'G':
        if ((*format == 'g')||(*format == 'G')) flags |= FLAGS_ADAPT_EXP;
        if ((*format == 'E')||(*format == 'G')) flags |= FLAGS_UPPERCASE;
        idx = sprint_floating_point(out, buffer, idx, maxlen, va_arg(va, double), precision, width, flags, PRINTF_PREFER_EXPONENTIAL);
        format++;
        break;
#endif  // PRINTF_SUPPORT_EXPONENTIAL_SPECIFIERS
#endif  // PRINTF_SUPPORT_FLOAT_SPECIFIERS
      case 'c' : {
        unsigned int l = 1U;
        // pre padding
        if (!(flags & FLAGS_LEFT)) {
          while (l++ < width) {
            out(' ', buffer, idx++, maxlen);
          }
        }
        // char output
        out((char)va_arg(va, int), buffer, idx++, maxlen);
        // post padding
        if (flags & FLAGS_LEFT) {
          while (l++ < width) {
            out(' ', buffer, idx++, maxlen);
          }
        }
        format++;
        break;
      }

      case 's' : {
        const char* p = va_arg(va, char*);
        if (p == NULL) {
          idx = _out_rev(out, buffer, idx, maxlen, ")llun(", 6, width, flags);
        }
        else {
          unsigned int l = _strnlen_s(p, precision ? precision : (size_t)-1);
          // pre padding
          if (flags & FLAGS_PRECISION) {
            l = (l < precision ? l : precision);
          }
          if (!(flags & FLAGS_LEFT)) {
            while (l++ < width) {
              out(' ', buffer, idx++, maxlen);
            }
          }
          // string output
          while ((*p != 0) && (!(flags & FLAGS_PRECISION) || precision--)) {
            out(*(p++), buffer, idx++, maxlen);
          }
          // post padding
          if (flags & FLAGS_LEFT) {
            while (l++ < width) {
              out(' ', buffer, idx++, maxlen);
            }
          }
        }
        format++;
        break;
      }

      case 'p' : {
        width = sizeof(void*) * 2U + 2; // 2 hex chars per byte + the "0x" prefix
        flags |= FLAGS_ZEROPAD | FLAGS_POINTER;
        uintptr_t value = (uintptr_t)va_arg(va, void*);

        if (value == (uintptr_t) NULL) {
          idx = _out_rev(out, buffer, idx, maxlen, ")lin(", 5, width, flags);
        }
        else {
    #if PRINTF_SUPPORT_LONG_LONG
          const bool is_ll = sizeof(uintptr_t) == sizeof(long long);
          if (is_ll) {
            idx = _ntoa(out, buffer, idx, maxlen, (NTOA_VALUE_TYPE) value, false, BASE_HEX, precision, width, flags);
          }
          else {
    #endif
            idx = _ntoa(out, buffer, idx, maxlen, (NTOA_VALUE_TYPE)((uintptr_t)va_arg(va, void*)), false, BASE_HEX, precision, width, flags);
    #if PRINTF_SUPPORT_LONG_LONG
          }
    #endif
        }
        format++;
        break;
      }

      case '%' :
        out('%', buffer, idx++, maxlen);
        format++;
        break;

      default :
        out(*format, buffer, idx++, maxlen);
        format++;
        break;
    }
  }

  // termination
  out((char)0, buffer, idx < maxlen ? idx : maxlen - 1U, maxlen);

  // return written chars without terminating \0
  return (int)idx;
}


///////////////////////////////////////////////////////////////////////////////

//int printf_(const char* format, ...)
int utl_printf(const char* format, ...)
{
  va_list va;
  va_start(va, format);
  char buffer[1];
  const int ret = _vsnprintf(_out_char, buffer, (size_t)-1, format, va);
  va_end(va);
  return ret;
}


//int sprintf_(char* buffer, const char* format, ...)
int utl_sprintf(char* buffer, const char* format, ...)
{
  va_list va;
  va_start(va, format);
  const int ret = _vsnprintf(_out_buffer, buffer, (size_t)-1, format, va);
  va_end(va);
  return ret;
}


//int snprintf_(char* buffer, size_t count, const char* format, ...)
int utl_snprintf(char* buffer, size_t count, const char* format, ...)
{
  va_list va;
  va_start(va, format);
  const int ret = _vsnprintf(_out_buffer, buffer, count, format, va);
  va_end(va);
  return ret;
}


//int vprintf_(const char* format, va_list va)
int utl_vprintf(const char* format, va_list va)
{
  char buffer[1];
  return _vsnprintf(_out_char, buffer, (size_t)-1, format, va);
}

//int vsprintf_(char* buffer, const char* format, va_list va)
int utl_vsprintf(char* buffer, const char* format, va_list va)
{
  return _vsnprintf(_out_buffer, buffer, (size_t)-1, format, va);
}

//int vsnprintf_(char* buffer, size_t count, const char* format, va_list va)
int utl_vsnprintf(char* buffer, size_t count, const char* format, va_list va)
{
  return _vsnprintf(_out_buffer, buffer, count, format, va);
}


//int fctprintf(void (*out)(char character, void* arg), void* arg, const char* format, ...)
int utl_fctprintf(void (*out)(char character, void* arg), void* arg, const char* format, ...)
{
  va_list va;
  va_start(va, format);
  const int ret = utl_vfctprintf(out, arg, format, va);
  va_end(va);
  return ret;
}

//int vfctprintf(void (*out)(char character, void* arg), void* arg, const char* format, va_list va)
int utl_vfctprintf(void (*out)(char character, void* arg), void* arg, const char* format, va_list va)
{
  const out_fct_wrap_type out_fct_wrap = { out, arg };
  return _vsnprintf(_out_fct, (char*)(uintptr_t)&out_fct_wrap, (size_t)-1, format, va);
}
