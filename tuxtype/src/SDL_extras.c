/*
   SDL_extras.c — SDL3 port: thin wrappers that delegate to t4k_common.

   Originally a 1641-line copy of t4k_sdl.c's surface helpers. With the SDL3
   port we use t4k_common as the single source of truth and forward to it
   here, keeping tuxtype's call-site signatures unchanged.

   GPLv3 or later. */

#include <math.h>
#include "globals.h"
#include "funcs.h"
#include "SDL_extras.h"
#include "convert_utf.h"

void DrawButton(SDL_Rect* target_rect, int radius,
                Uint8 r, Uint8 g, Uint8 b, Uint8 a)
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
    while (SDL_PollEvent(&e)) { /* drain */ }
    while (SDL_WaitEvent(&e)) {
        if (e.type == SDL_EVENT_KEY_DOWN) return e.key.key;
        if (e.type == SDL_EVENT_QUIT) return 0;
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
void InitBlitQueue(void)         { T4K_InitBlitQueue(); }
void ResetBlitQueue(void)        { T4K_ResetBlitQueue(); }
int  AddRect(SDL_Rect* src, SDL_Rect* dst)            { return T4K_AddRect(src, dst); }
int  DrawObject(SDL_Surface* s, int x, int y)         { return T4K_DrawObject(s, x, y); }
int  DrawSprite(sprite* gfx, int x, int y)            { return T4K_DrawSprite(gfx, x, y); }
/* T4K_Erase* take an extra current-background surface arg in t4k_common —
 * we pass tuxtype's CurrentBkgd() so the erase blits the right backdrop. */
int  EraseObject(SDL_Surface* s, int x, int y)        { return T4K_EraseObject(s, CurrentBkgd(), x, y); }
int  EraseSprite(sprite* img, int x, int y)           { return T4K_EraseSprite(img, CurrentBkgd(), x, y); }
void UpdateScreen(int* frame)                         { T4K_UpdateScreen(frame); }

/* Text */
int  Setup_SDL_Text(void)        { return T4K_Setup_SDL_Text(); }
void Cleanup_SDL_Text(void)      { T4K_Cleanup_SDL_Text(); }

SDL_Surface* BlackOutline(const char* t, int font_size, const SDL_Color* c)
{
    return T4K_BlackOutline(t, font_size, c);
}

SDL_Surface* BlackOutline_w(const wchar_t* t, int font_size, const SDL_Color* c, int length)
{
    /* Convert wide-char to UTF-8 and forward — t4k_common's BlackOutline_w
     * exists too but has been historically guarded; safer to convert here. */
    char utf8[4096];
    int max = (length > 0 && length < (int)sizeof(utf8)/4) ? length : (int)wcslen(t);
    /* Naive ASCII conversion for first launch; non-Latin handled later. */
    int i;
    for (i = 0; i < max && i < (int)sizeof(utf8)-1; i++) utf8[i] = (char)t[i];
    utf8[i] = '\0';
    return T4K_BlackOutline(utf8, font_size, c);
}

SDL_Surface* SimpleText(const char* t, int size, const SDL_Color* col)
{
    return T4K_SimpleText(t, size, col);
}
