/* SDL_extras.h — tuxtype-only helpers that go beyond t4k_common.
 *
 * Most surface/text/blit-queue helpers are now T4K_* functions — call
 * those directly. What's left here has a different signature than the
 * t4k_common original (BlackOutline_w takes wchar_t; EraseObject auto-
 * passes CurrentBkgd; TransWipe takes a const surface) or a tuxtype-
 * specific implementation (WaitForKeypress).
 *
 * Copyright 2007, 2008, 2009, 2010 (David Bruce, Tim Holy). GPLv3+. */

#pragma once
#include <SDL3/SDL.h>
#include <t4k/common.h> /* sprite, MAX_SPRITE_FRAMES, color constants, WIPE_* */

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
# define rmask 0xff000000
# define gmask 0x00ff0000
# define bmask 0x0000ff00
# define amask 0x000000ff
#else
# define rmask 0x000000ff
# define gmask 0x0000ff00
# define bmask 0x00ff0000
# define amask 0xff000000
#endif

#define NEXT_FRAME(SPRITE)                                                     \
    if ((SPRITE)->num_frames)                                                  \
        (SPRITE)->cur = (((SPRITE)->cur) + 1) % (SPRITE)->num_frames;
#define REWIND(SPRITE) (SPRITE)->cur = 0;

int WaitForKeypress(void);

/* Wide-char variant of T4K_BlackOutline: encodes wchar_t→UTF-8 inline.
 * Returns NULL when length<=0 so empty practice-prompt lines don't
 * redraw under the user's typed area. */
SDL_Surface* BlackOutline_w(const wchar_t* t, int font_size, const SDL_Color* c,
                            int length);

/* Auto-pass CurrentBkgd() to T4K_Erase{Object,Sprite}. */
int EraseObject(SDL_Surface* surf, int x, int y);
int EraseSprite(sprite* img, int x, int y);

/* T4K_TransWipe wants non-const SDL_Surface*. */
int TransWipe(const SDL_Surface* newbkg, int type, int segments, int duration);
