/* Keyboard overlay widget — load keyboard.png, scale it to a target
 * width, position above a row, render base + per-key green/red
 * highlights. Used by Cascade, Comet Zap, and Practice as a visual
 * typing guide. */

#pragma once
#include "globals.h"

typedef struct KbdDisplay
{
    SDL_Surface* base;
    SDL_Surface* green[MAX_UNICODES];
    SDL_Surface* red[MAX_UNICODES];
    SDL_Surface* shift[3];
    SDL_Rect     rect;
    int          alpha;
} KbdDisplay;

void Kbd_Display_Init(KbdDisplay* keyboard);
/* target_w: scale the keyboard image to this width (preserving aspect).
 * Pass 0 for natural-size load. */
int  Kbd_Display_Load(KbdDisplay* keyboard, int alpha, int target_w);
void Kbd_Display_Free(KbdDisplay* keyboard);

void Kbd_Display_SetPosition(KbdDisplay* keyboard, int x, int y);
void Kbd_Display_SetPositionAbove(KbdDisplay* keyboard, int center_width,
                                  int top_y, int screen_height);
void Kbd_Display_BakeIntoBackground(KbdDisplay*  keyboard,
                                    SDL_Surface* background);
void Kbd_Display_DrawBase(KbdDisplay* keyboard, SDL_Surface* target);
void Kbd_Display_QueueErase(KbdDisplay* keyboard);

int Kbd_Display_DrawGreenKey(KbdDisplay* keyboard, int key_index);
int Kbd_Display_DrawGreenChar(KbdDisplay* keyboard, wchar_t ch);
int Kbd_Display_BlitGreenKey(KbdDisplay* keyboard, int key_index,
                             SDL_Surface* target);
int Kbd_Display_DrawRedKey(KbdDisplay* keyboard, int key_index,
                           SDL_Surface* target);
int Kbd_Display_DrawShiftForKey(KbdDisplay* keyboard, int key_index,
                                SDL_Surface* target);
