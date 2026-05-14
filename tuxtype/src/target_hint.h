/* Reusable input-mode pedagogy hint for "target" objects (comets in
 * Comet Zap, fish in Cascade): renders a small glyph just below the
 * primary Latin letter that visualizes how the active input mode
 * reaches that character. Today: braille chord cell. Future: morse
 * pattern, semaphore flag pair. No-op in keyboard mode. */
#pragma once

#include <SDL3/SDL.h>
#include <wchar.h>

/* (letter_x, letter_y, letter_w, letter_h) describe the rect where the
 * primary letter was just drawn; the hint is centered on it
 * horizontally and placed just below. font_size sets the hint glyph
 * size. target_ch is lowercased internally so case-insensitive games
 * don't get the capital-prefix cell. */
void Target_DrawHintBelow(wchar_t target_ch, int letter_x, int letter_y,
                          int letter_w, int letter_h, int font_size,
                          SDL_Surface* dst);
