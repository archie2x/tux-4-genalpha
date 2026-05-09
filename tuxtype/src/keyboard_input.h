/* Shared typing-input decoder. Each game still handles its own
 * control-key events (ESC, F-keys, arrows). For typing, pass every
 * SDL_Event through Kbd_Input_HandleEvent — it returns a decoded
 * wchar_t when one is ready (TEXT_INPUT in normal mode, KEY_UP
 * chord-decode in braille mode). All three games share the same
 * braille chord state via file-static storage in keyboard_input.c. */

#pragma once
#include "globals.h"

typedef struct
{
    int     ready; /* 1 if a typed character was decoded this event */
    wchar_t ch;    /* the decoded wide char (post-braille, post-uppercase) */
    int     key_index; /* GetIndex(ch), or -1 */
} KbdTyped;

void Kbd_Input_Reset(void);
/* braille_letter_pos: 0 (word-start), 1 (middle), 2 (end) — picks which
 * column of braille_key_value_map[] to look up. The caller tracks this
 * since it depends on game-specific word progression. */
void Kbd_Input_HandleEvent(const SDL_Event* event, int braille_letter_pos,
                           KbdTyped* out);
