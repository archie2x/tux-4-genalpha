/*
   game_keyboard.h:

   Shared keyboard overlay/widget used by typing activities.
*/

#ifndef TUXTYPE_GAME_KEYBOARD_H
#define TUXTYPE_GAME_KEYBOARD_H

#include "globals.h"

typedef struct GameKeyboard
{
    SDL_Surface* base;
    SDL_Surface* green[MAX_UNICODES];
    SDL_Surface* red[MAX_UNICODES];
    SDL_Surface* shift[3];
    SDL_Rect     rect;
    int          alpha;
} GameKeyboard;

void GameKeyboard_Init(GameKeyboard* keyboard);
/* target_w: scale the keyboard image to this width (preserving aspect).
 * Pass 0 for natural-size load. */
int  GameKeyboard_Load(GameKeyboard* keyboard, int alpha, int target_w);
void GameKeyboard_Free(GameKeyboard* keyboard);

void GameKeyboard_SetPosition(GameKeyboard* keyboard, int x, int y);
void GameKeyboard_SetPositionAbove(GameKeyboard* keyboard, int center_width,
                                   int top_y, int screen_height);
void GameKeyboard_BakeIntoBackground(GameKeyboard* keyboard,
                                     SDL_Surface*  background);
void GameKeyboard_DrawBase(GameKeyboard* keyboard, SDL_Surface* target);
void GameKeyboard_QueueErase(GameKeyboard* keyboard);

int GameKeyboard_DrawGreenKey(GameKeyboard* keyboard, int key_index);
int GameKeyboard_DrawGreenChar(GameKeyboard* keyboard, wchar_t ch);
int GameKeyboard_BlitGreenKey(GameKeyboard* keyboard, int key_index,
                              SDL_Surface* target);
int GameKeyboard_DrawRedKey(GameKeyboard* keyboard, int key_index,
                            SDL_Surface* target);
int GameKeyboard_DrawShiftForKey(GameKeyboard* keyboard, int key_index,
                                 SDL_Surface* target);

#endif
