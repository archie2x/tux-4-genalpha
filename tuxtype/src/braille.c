/*
   braille.c:

   Description: Functions for loding braille map and key's
                order correcter

   Copyright 2013.
   Author: Nalin.x.Linux < Nalin.x.Linux@gmail.com >
   Project email: <tux4kids-tuxtype-dev@lists.alioth.debian.org>
   Project website: http://tux4kids.alioth.debian.org

   braille.c is part of Tux Typing, a.k.a "tuxtype".

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

#include "globals.h"
#include <wctype.h>
#include <string.h>

/* Single-entry cache for Braille_DotsForChar. Cascade and Comet Zap call
 * the function every frame the highlight redraws, but the lowest target
 * stays the same for many frames — so this catches ~99% of calls.
 * Invalidated by Braille_LoadLanguage when the map changes. */
static wchar_t s_cache_ch = 0;
static wchar_t s_cache_dots[6];
static int     s_cache_n = -1;

/* Match ch against an entry's value_{begin,middle,end}[0]. Tries
 * towlower(ch) too — most braille maps are lowercase, so an uppercase
 * target should still find its chord. */
static int chord_matches(int idx, wchar_t ch)
{
    wchar_t lo = (wchar_t)towlower((wint_t)ch);
    return braille_key_value_map[idx].value_begin[0] == ch ||
           braille_key_value_map[idx].value_middle[0] == ch ||
           braille_key_value_map[idx].value_end[0] == ch ||
           braille_key_value_map[idx].value_begin[0] == lo ||
           braille_key_value_map[idx].value_middle[0] == lo ||
           braille_key_value_map[idx].value_end[0] == lo;
}

/* For a target character, fill `dots` with the chord keys to press.
 * Returns dot count, 0 if no chord produces ch. */
int Braille_DotsForChar(wchar_t ch, wchar_t dots[6])
{
    if (ch != 0 && ch == s_cache_ch && s_cache_n >= 0)
    {
        memcpy(dots, s_cache_dots, sizeof(s_cache_dots));
        return s_cache_n;
    }

    int n = 0;
    for (int i = 0; i < 100; i++)
    {
        wchar_t* k = braille_key_value_map[i].key;
        if (!k[0])
        {
            continue;
        }
        if (!chord_matches(i, ch))
        {
            continue;
        }
        for (int j = 0; k[j] && n < 6; j++)
        {
            dots[n++] = k[j];
        }
        break;
    }

    s_cache_ch = ch;
    s_cache_n  = n;
    memcpy(s_cache_dots, dots, sizeof(s_cache_dots));
    return n;
}

int Braille_PositionForChar(wchar_t ch)
{
    wchar_t lo = (wchar_t)towlower((wint_t)ch);
    for (int i = 0; i < 100; i++)
    {
        if (!braille_key_value_map[i].key[0])
        {
            continue;
        }
        if (braille_key_value_map[i].value_end[0] == ch ||
            braille_key_value_map[i].value_end[0] == lo)
        {
            return 2;
        }
        if (braille_key_value_map[i].value_middle[0] == ch ||
            braille_key_value_map[i].value_middle[0] == lo)
        {
            return 1;
        }
        if (braille_key_value_map[i].value_begin[0] == ch ||
            braille_key_value_map[i].value_begin[0] == lo)
        {
            return 0;
        }
    }
    return -1;
}

/* Reorder the given chord into canonical fdsjkl (dots 1,2,3,4,5,6) order
 * so the lookup against braille_key_value_map[].key works regardless of
 * which keys the user pressed first. */
void Braille_Reorder(wchar_t* disorder)
{
    static const wchar_t dots[] = L"fdsjkl";
    wchar_t              in[8]  = {0};
    int                  out    = 0;

    wcsncpy(in, disorder, 7);
    for (int i = 0; dots[i]; i++)
    {
        if (wcschr(in, dots[i]))
        {
            disorder[out++] = dots[i];
        }
    }
    disorder[out] = L'\0';
}

/* Braille map loading function
 *
 * The format of input file is as folows
 * keycombination<space>beginning_value<space>middle_value<space>end_value
 *
 * For some specific language's which have same braille code for
 * alphabets and signs at begining, middle and end position.
 *
 * The keyvombination should be writen in the order fdsjkl.
 * means first f second d so on. */
int Braille_LoadLanguage(char* language)
{
    int   iter = 0;
    FILE* fp;
    char  file[100];

    /* New map → drop the DotsForChar cache. */
    s_cache_n = -1;

    sprintf(file, "%s/braille/%s", settings.default_data_path, language);
    fp = fopen(file, "r");

    if (fp == NULL)
    {
        DEBUGCODE
        {
            fprintf(stderr, "Couldn't open map for reading");
        }
        return 0;
    }

    while (!feof(fp))
    {
        fscanf(fp, "%S %S %S %S\n", braille_key_value_map[iter].key,
               braille_key_value_map[iter].value_begin,
               braille_key_value_map[iter].value_middle,
               braille_key_value_map[iter].value_end);
        iter++;
    }
    return 1;
}
