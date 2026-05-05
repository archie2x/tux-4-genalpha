/*
   convert_utf.c:

   Description: simple wrapper functions to convert
   wchar_t and utf8 strings using GNU iconv().
   
   Copyright 2009, 2010.
   Author: David Bruce.
   Project email: <tux4kids-tuxtype-dev@lists.alioth.debian.org>
   Project website: http://tux4kids.alioth.debian.org

   convert_utf.c is part of Tux Typing, a.k.a "tuxtype".

Tux Typing is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

Tux Typing is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/



#include "convert_utf.h"
#include "globals.h"

#include <iconv.h>

#define UTF_BUF_LENGTH 1024

/* GNU iconv()-based implementation:   */

int ConvertFromUTF8(wchar_t* wide_word, const char* UTF8_word, int max_length)
{
  if(max_length > UTF_BUF_LENGTH)
  {
    fprintf(stderr, "ConvertFromUTF8() - error - requested string length %d exceeds buffer length %d\n",
            max_length, UTF_BUF_LENGTH);
    return 0;
  }

  DEBUGCODE {fprintf(stderr, "ConvertFromUTF8(): UTF8_word = %s\n", UTF8_word);}

  /* mbstowcs respects the current LC_CTYPE locale, so it correctly
   * decodes multi-byte UTF-8 — Devanagari, Cyrillic, etc. — into wchar_t
   * code points. Force LC_CTYPE to UTF-8 here so the conversion works
   * even if the theme's theme_locale_name doesn't include .UTF-8 (e.g.
   * Hindi ships "hi_IN" without the suffix and macOS won't infer it).
   *
   * Old code used iconv("wchar_t", "UTF-8") which works on GNU iconv but
   * is not a recognized encoding on macOS's libiconv, so the conversion
   * silently failed and every non-ASCII word produced 0 wide chars. */
  char* prev_ctype = setlocale(LC_CTYPE, NULL);
  char  saved_ctype[64];
  if (prev_ctype) { strncpy(saved_ctype, prev_ctype, sizeof(saved_ctype)-1); saved_ctype[sizeof(saved_ctype)-1] = '\0'; }
  else            { saved_ctype[0] = '\0'; }
  if (!setlocale(LC_CTYPE, "en_US.UTF-8"))
      setlocale(LC_CTYPE, "C.UTF-8");

  size_t n = mbstowcs(wide_word, UTF8_word, (size_t)max_length - 1);

  if (saved_ctype[0]) setlocale(LC_CTYPE, saved_ctype);
  if (n == (size_t)-1)
  {
    fprintf(stderr, "ConvertFromUTF8(): invalid UTF-8 in '%s'\n", UTF8_word);
    wide_word[0] = L'\0';
    return -1;
  }
  wide_word[n] = L'\0';

  DEBUGCODE {fprintf(stderr, "ConvertFromUTF8(): wide_word = %ls\n", wide_word);}
  return (int)n;
}


/* Now this uses GNU iconv and works correctly!   */
/* This probably could be simplified - not certain */
/* we have to copy into and out of the buffers     */

/******************To be used for savekeyboard*************/
/***Converts wchar_t string to char string*****************/
int ConvertToUTF8(const wchar_t* wide_word, char* UTF8_word, int max_length)
{
  char temp_UTF8[UTF_BUF_LENGTH];
  /* NOTE we need this because iconv_open() needs a char**.  We can't   */
  /* just pass "&temp_UTF8" because "temp_UTF8" is really a shorthand   */
  /* for "&temp_UTF8[0]", not its own memory location, so it doesn't    */
  /* have its own address. We ought to be able to do this directly into */
  /* into the argument UTF8_word string, but so far have had errors.    */
  char* UTF8_Start = temp_UTF8;

  iconv_t conv_descr;
  size_t bytes_converted;
  size_t in_length = (size_t)UTF_BUF_LENGTH;
  size_t out_length = (size_t)UTF_BUF_LENGTH;

  DEBUGCODE {fprintf(stderr, "ConvertToUTF8(): wide_word = %S\n", wide_word);}

  if(max_length > UTF_BUF_LENGTH)
  {
    fprintf(stderr, "ConvertToUTF8() - error - requested string length %d exceeds buffer length %d\n",
            max_length, UTF_BUF_LENGTH);
    return 0;
  }

  /* NOTE although we *should* be just able to pass "wchar_t" as the in_type, */
  /* iconv_open() segfaults on Windows if this is done - grrr....             */
#ifdef WIN32
  conv_descr = iconv_open("UTF-8", "UTF-16LE");
#else
  conv_descr = iconv_open("UTF-8", "wchar_t");
#endif

  bytes_converted = iconv(conv_descr,
                          (char**) &wide_word, &in_length,
                          (char**) &UTF8_Start, &out_length);
  iconv_close(conv_descr);
  strncpy(UTF8_word, temp_UTF8, max_length);

  DEBUGCODE {fprintf(stderr, "ConvertToUTF8(): UTF8_word = %s\n", UTF8_word);}

  return strlen(UTF8_word);
}
