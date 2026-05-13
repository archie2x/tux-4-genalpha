/* Hands-and-fingers visual hint widget. Pairs with keyboard_display:
 * sits below the keyboard guide and shows the next finger to use. */

#pragma once
#include "globals.h"

typedef struct HandDisplay
{
    SDL_Surface* base;       /* hands.png — gray silhouette of both hands */
    SDL_Surface* finger[10]; /* per-finger highlight overlays, 0..9 */
    SDL_Surface* shift[3];   /* 0 = none, 1 = lshift, 2 = rshift */
    SDL_Rect     rect;       /* blit location on the target surface */
} HandDisplay;

void Hand_Display_Init(HandDisplay* hd);
int  Hand_Display_Load(HandDisplay* hd);
void Hand_Display_Free(HandDisplay* hd);
void Hand_Display_SetPosition(HandDisplay* hd, int x, int y);

/* Draw just the silhouette — used when there's nothing else to highlight
 * (e.g. braille mode's space character). */
void Hand_Display_DrawBase(HandDisplay* hd, SDL_Surface* dst);

/* Normal typing hint: silhouette + the finger that types this key, plus
 * the opposite-hand pinky if shift is needed.
 *   finger:  GetFinger() return value, 0..9 (-1 = no highlight)
 *   shift:   0 = none, 1 = lshift, 2 = rshift */
void Hand_Display_DrawForKey(HandDisplay* hd, int finger, int shift,
                             SDL_Surface* dst);

/* Braille hint: silhouette + per-dot finger overlays stacked.
 *   dots:   chord keys ('f','d','s','j','k','l')
 *   n:      number of dots in the chord (1..6) */
void Hand_Display_DrawForChord(HandDisplay* hd, const wchar_t* dots, int n,
                               SDL_Surface* dst);
