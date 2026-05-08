/* UTF-8 ↔ wchar_t via mbstowcs / wcstombs.
 *
 * LC_CTYPE is pushed to a UTF-8 locale around each call so the conversion
 * works even when the global locale lacks a .UTF-8 suffix.
 */

#include <t4k/common.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>
#include <errno.h>

#define UTF_BUF_LENGTH 1024

/* Switch LC_CTYPE to a UTF-8 locale; on success caller must restore the
 * returned previous setting via setlocale(LC_CTYPE, prev). Returns NULL
 * if no UTF-8 locale could be selected (in which case mbstowcs/wcstombs
 * will still work using the existing locale, just without UTF-8 awareness). */
static const char* push_utf8_ctype(char* save, size_t save_len)
{
    const char* prev = setlocale(LC_CTYPE, NULL);
    if (prev)
    {
        strncpy(save, prev, save_len - 1);
        save[save_len - 1] = '\0';
    }
    else
    {
        save[0] = '\0';
    }
    if (setlocale(LC_CTYPE, "en_US.UTF-8"))
    {
        return save[0] ? save : NULL;
    }
    if (setlocale(LC_CTYPE, "C.UTF-8"))
    {
        return save[0] ? save : NULL;
    }
    return NULL;
}

int T4K_ConvertFromUTF8(wchar_t* wide_word, const char* UTF8_word, int max_length)
{
    if (max_length <= 0)
    {
        return 0;
    }

    DEBUGMSG(debug_i18n, " T4K_ConvertFromUTF8(): UTF8_word = %s\n", UTF8_word);

    char        saved[64];
    const char* prev = push_utf8_ctype(saved, sizeof(saved));

    size_t n = mbstowcs(wide_word, UTF8_word, (size_t)max_length - 1);

    if (prev)
    {
        setlocale(LC_CTYPE, prev);
    }

    if (n == (size_t)-1)
    {
        DEBUGMSG(debug_i18n, "T4K_ConvertFromUTF8(): invalid UTF-8: %s\n",
                 UTF8_word);
        wide_word[0] = L'\0';
        return -1;
    }
    wide_word[n] = L'\0';

    DEBUGMSG(debug_i18n, " T4K_ConvertFromUTF8(): wide_word = %ls\n",
             wide_word);
    return (int)n;
}

int T4K_ConvertToUTF8(const wchar_t* wide_word, char* UTF8_word, int max_length)
{
    if (max_length <= 0)
    {
        return 0;
    }

    DEBUGMSG(debug_i18n, "T4K_ConvertToUTF8(): wide_word = %S\n", wide_word);

    char        saved[64];
    const char* prev = push_utf8_ctype(saved, sizeof(saved));

    size_t n = wcstombs(UTF8_word, wide_word, (size_t)max_length - 1);

    if (prev)
    {
        setlocale(LC_CTYPE, prev);
    }

    if (n == (size_t)-1)
    {
        DEBUGMSG(debug_i18n, "T4K_ConvertToUTF8(): invalid wide string\n");
        UTF8_word[0] = '\0';
        return 0;
    }
    UTF8_word[n] = '\0';

    DEBUGMSG(debug_i18n, " T4K_ConvertToUTF8(): UTF8_word = %s\n", UTF8_word);
    return (int)n;
}
