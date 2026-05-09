#ifndef TUXTYPE_BRAILLE_H
#define TUXTYPE_BRAILLE_H

#include <wchar.h>

void Braille_Reorder(wchar_t* disorder);
int  Braille_LoadLanguage(char* language);

/* For a target character, fill `dots` (capacity 6) with the chord
 * keys (subset of fdsjkl) the user must press. Searches the active
 * braille_key_value_map for any chord whose value at begin/middle/end
 * produces ch (or towlower(ch), since most maps are lowercase).
 * Returns the number of dots written (0 if no match). */
int Braille_DotsForChar(wchar_t ch, wchar_t dots[6]);

/* Word-position at which ch appears in the active map:
 *   2 if matches value_end, 1 if value_middle, 0 if value_begin,
 *  -1 if no match. */
int Braille_PositionForChar(wchar_t ch);

#endif
