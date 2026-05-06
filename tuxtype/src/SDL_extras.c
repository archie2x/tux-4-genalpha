/*
   SDL_extras.c — SDL3 port: thin wrappers that delegate to t4k_common.

   Originally a 1641-line copy of t4k_sdl.c's surface helpers. With the SDL3
   port we use t4k_common as the single source of truth and forward to it
   here, keeping tuxtype's call-site signatures unchanged.

   GPLv3 or later. */

#include "globals.h"
#include "funcs.h"
#include "SDL_extras.h"

void DrawButton(SDL_Rect* target_rect, int radius, Uint8 r, Uint8 g, Uint8 b,
                Uint8 a)
{
    T4K_DrawButton(target_rect, radius, r, g, b, a);
}

void RoundCorners(SDL_Surface* s, Uint16 radius)
{
    T4K_RoundCorners(s, radius);
}

SDL_Surface* Flip(SDL_Surface* in, int x, int y)
{
    return T4K_Flip(in, x, y);
}

int inRect(SDL_Rect r, int x, int y)
{
    return T4K_inRect(r, x, y);
}

void DarkenScreen(Uint8 bits)
{
    T4K_DarkenScreen(bits);
}

void SwitchScreenMode(void)
{
    T4K_SwitchScreenMode();
}

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

SDL_Surface* Blend(SDL_Surface* S1, SDL_Surface* S2, float gamma)
{
    return T4K_Blend(S1, S2, gamma);
}

SDL_Surface* zoom(SDL_Surface* src, int new_w, int new_h)
{
    return T4K_zoom(src, new_w, new_h);
}

int TransWipe(const SDL_Surface* newbkg, int type, int segments, int duration)
{
    /* T4K_TransWipe takes non-const SDL_Surface* */
    return T4K_TransWipe((SDL_Surface*)newbkg, type, segments, duration);
}

/* Blit queue */
void InitBlitQueue(void)
{
    T4K_InitBlitQueue();
}
void ResetBlitQueue(void)
{
    T4K_ResetBlitQueue();
}
int AddRect(SDL_Rect* src, SDL_Rect* dst)
{
    return T4K_AddRect(src, dst);
}
int DrawObject(SDL_Surface* s, int x, int y)
{
    return T4K_DrawObject(s, x, y);
}
int DrawSprite(sprite* gfx, int x, int y)
{
    return T4K_DrawSprite(gfx, x, y);
}
/* T4K_Erase* take an extra current-background surface arg in t4k_common —
 * we pass tuxtype's CurrentBkgd() so the erase blits the right backdrop. */
int EraseObject(SDL_Surface* s, int x, int y)
{
    return T4K_EraseObject(s, CurrentBkgd(), x, y);
}
int EraseSprite(sprite* img, int x, int y)
{
    return T4K_EraseSprite(img, CurrentBkgd(), x, y);
}
void UpdateScreen(int* frame)
{
    T4K_UpdateScreen(frame);
}

/* Text */
int Setup_SDL_Text(void)
{
    return T4K_Setup_SDL_Text();
}
void Cleanup_SDL_Text(void)
{
    T4K_Cleanup_SDL_Text();
}

SDL_Surface* BlackOutline(const char* t, int font_size, const SDL_Color* c)
{
    return T4K_BlackOutline(t, font_size, c);
}

SDL_Surface* BlackOutline_w(const wchar_t* t, int font_size, const SDL_Color* c, int length)
{
    /* Convert wide-char to UTF-8 then forward to T4K_BlackOutline (which
     * expects UTF-8). A naive byte-cast truncates 'ü' (U+00FC) to 0xFC and
     * SDL_ttf rejects it as invalid UTF-8 — that's why umlauts/accents
     * rendered blank. Inline encoder avoids locale dependence of wcstombs. */
    /* length == 0 means "no chars typed yet" in the practice echo area —
     * return NULL so the caller skips rendering. (Old behavior fell back
     * to wcslen(t), which redrew the whole prompt under the user's typed
     * line before they had typed anything.) */
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

SDL_Surface* SimpleText(const char* t, int size, const SDL_Color* col)
{
    return T4K_SimpleText(t, size, col);
}
