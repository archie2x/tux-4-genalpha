/* SDL_extras.c — tuxtype-only helpers beyond t4k_common. See header. */

#include "globals.h"
#include "funcs.h"
#include "SDL_extras.h"

/* Drain pending events, then block until any keypress / quit. */
int WaitForKeypress(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    { /* drain */
    }
    while (SDL_WaitEvent(&e))
    {
        if (e.type == SDL_EVENT_KEY_DOWN)
        {
            return e.key.key;
        }
        if (e.type == SDL_EVENT_QUIT)
        {
            return 0;
        }
    }
    return 0;
}

int TransWipe(const SDL_Surface* newbkg, int type, int segments, int duration)
{
    return T4K_TransWipe((SDL_Surface*)newbkg, type, segments, duration);
}

int EraseObject(SDL_Surface* s, int x, int y)
{
    return T4K_EraseObject(s, CurrentBkgd(), x, y);
}

int EraseSprite(sprite* img, int x, int y)
{
    return T4K_EraseSprite(img, CurrentBkgd(), x, y);
}

SDL_Surface* BlackOutline_w(const wchar_t* t, int font_size, const SDL_Color* c,
                            int length)
{
    /* Naive byte-cast of wchar_t to char truncates 'ü' (U+00FC) to 0xFC
     * and SDL_ttf rejects that as invalid UTF-8 — encode wide → UTF-8
     * inline without leaning on the locale-dependent wcstombs. length
     * <= 0 means "user hasn't typed yet" in the practice echo area;
     * return NULL so the caller skips rendering an empty prompt. */
    if (length <= 0)
    {
        return NULL;
    }
    char utf8[4096];
    int  max = length;
    int  o   = 0;
    for (int i = 0; i < max && o + 4 < (int)sizeof(utf8); i++)
    {
        wchar_t cp = t[i];
        if (cp < 0x80)
        {
            utf8[o++] = (char)cp;
        }
        else if (cp < 0x800)
        {
            utf8[o++] = (char)(0xC0 | (cp >> 6));
            utf8[o++] = (char)(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            utf8[o++] = (char)(0xE0 | (cp >> 12));
            utf8[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            utf8[o++] = (char)(0x80 | (cp & 0x3F));
        }
        else
        {
            utf8[o++] = (char)(0xF0 | (cp >> 18));
            utf8[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            utf8[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            utf8[o++] = (char)(0x80 | (cp & 0x3F));
        }
    }
    utf8[o] = '\0';
    return T4K_BlackOutline(utf8, font_size, c);
}
