#include "target_hint.h"
#include "globals.h"
#include "braille.h"
#include "SDL_extras.h"
#include <t4k/common.h>
#include <wctype.h>

/* Cache rendered braille glyph surfaces, keyed by Unicode-Braille
 * codepoint offset (0..255 for U+2800..U+28FF). T4K_DrawObject queues
 * the source pointer for a deferred flush in T4K_UpdateScreen — so the
 * surface must outlive the queue. The cache satisfies that and avoids
 * re-rendering the same glyph every frame. Cache is invalidated when
 * font_size changes. */
#define BRAILLE_CACHE_N 256
static SDL_Surface* s_cache[BRAILLE_CACHE_N];
static int          s_cache_font_size = 0;

static SDL_Surface* get_glyph(wchar_t cp, int font_size)
{
    int idx = (int)cp - 0x2800;
    if (idx < 0 || idx >= BRAILLE_CACHE_N)
    {
        return NULL;
    }
    if (font_size != s_cache_font_size)
    {
        for (int i = 0; i < BRAILLE_CACHE_N; i++)
        {
            if (s_cache[i])
            {
                SDL_DestroySurface(s_cache[i]);
                s_cache[i] = NULL;
            }
        }
        s_cache_font_size = font_size;
    }
    if (!s_cache[idx])
    {
        wchar_t ltr[2] = {cp, 0};
        s_cache[idx]   = BlackOutline_w(ltr, font_size, &white, 1);
    }
    return s_cache[idx];
}

void Target_DrawHintBelow(wchar_t target_ch, int letter_x, int letter_y,
                          int letter_w, int letter_h, int font_size,
                          SDL_Surface* dst)
{
    (void)dst; /* T4K_DrawObject targets the registered screen surface */
    if (settings.input_mode != INPUT_BRAILLE)
    {
        return;
    }
    wchar_t cp = Braille_CodepointForChar(towlower(target_ch));
    if (!cp)
    {
        return;
    }
    SDL_Surface* b = get_glyph(cp, font_size);
    if (!b)
    {
        return;
    }
    T4K_DrawObject(b, letter_x + (letter_w - b->w) / 2,
                   letter_y + letter_h + 2);
}
