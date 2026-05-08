/*
   game_keyboard.c:

   Shared keyboard overlay/widget used by typing activities.
*/

#include "game_keyboard.h"

#include "funcs.h"
#include <string.h>

/* Scale src to (w,h) using bilinear filtering. Frees src; returns the new
 * surface, or src untouched if (w,h) already matches or allocation fails. */
static SDL_Surface* scale_surface(SDL_Surface* src, int w, int h)
{
    SDL_Surface* dst;

    if (!src || (src->w == w && src->h == h))
    {
        return src;
    }

    dst = SDL_CreateSurface(w, h, src->format);
    if (!dst)
    {
        return src;
    }

    SDL_BlitSurfaceScaled(src, NULL, dst, NULL, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(src);
    return dst;
}

static SDL_Surface* load_key_surface(SDL_Surface** cache, int         key_index,
                                     void (*path_fn)(int, char*), int target_w,
                                     int target_h)
{
    char fn[FNLEN];

    if (key_index < 0 || key_index >= MAX_UNICODES)
    {
        return NULL;
    }

    if (!cache[key_index])
    {
        path_fn(key_index, fn);
        cache[key_index] =
            scale_surface(LoadImage(fn, IMG_ALPHA), target_w, target_h);
    }

    return cache[key_index];
}

void GameKeyboard_Init(GameKeyboard* keyboard)
{
    if (!keyboard)
    {
        return;
    }

    memset(keyboard, 0, sizeof(*keyboard));
}

int GameKeyboard_Load(GameKeyboard* keyboard, int alpha, int target_w)
{
    if (!keyboard)
    {
        return 0;
    }

    GameKeyboard_Free(keyboard);
    keyboard->base = LoadImage("keyboard/keyboard.png", IMG_ALPHA);
    if (!keyboard->base)
    {
        return 0;
    }

    /* GenerateKeyboard paints labels at original-size pixel offsets, so
     * run it before scaling. Then bilinear-zoom the whole composite. */
    GenerateKeyboard(keyboard->base);

    if (target_w > 0 && target_w != keyboard->base->w)
    {
        int target_h =
            (int)((long long)keyboard->base->h * target_w / keyboard->base->w);
        keyboard->base = scale_surface(keyboard->base, target_w, target_h);
    }

    keyboard->alpha = alpha;
    SDL_SetSurfaceAlphaMod(keyboard->base, alpha);
    keyboard->rect.w = keyboard->base->w;
    keyboard->rect.h = keyboard->base->h;

    return 1;
}

void GameKeyboard_Free(GameKeyboard* keyboard)
{
    int i;

    if (!keyboard)
    {
        return;
    }

    if (keyboard->base)
    {
        SDL_DestroySurface(keyboard->base);
    }
    keyboard->base = NULL;

    for (i = 0; i < MAX_UNICODES; i++)
    {
        if (keyboard->green[i])
        {
            SDL_DestroySurface(keyboard->green[i]);
        }
        if (keyboard->red[i])
        {
            SDL_DestroySurface(keyboard->red[i]);
        }
        keyboard->green[i] = NULL;
        keyboard->red[i]   = NULL;
    }

    for (i = 0; i < 3; i++)
    {
        if (keyboard->shift[i])
        {
            SDL_DestroySurface(keyboard->shift[i]);
        }
        keyboard->shift[i] = NULL;
    }

    keyboard->rect  = (SDL_Rect){0, 0, 0, 0};
    keyboard->alpha = 0;
}

void GameKeyboard_SetPosition(GameKeyboard* keyboard, int x, int y)
{
    if (!keyboard)
    {
        return;
    }

    keyboard->rect.x = x;
    keyboard->rect.y = y;
    if (keyboard->base)
    {
        keyboard->rect.w = keyboard->base->w;
        keyboard->rect.h = keyboard->base->h;
    }
}

void GameKeyboard_SetPositionAbove(GameKeyboard* keyboard, int center_width,
                                   int top_y, int screen_height)
{
    int x, y;

    if (!keyboard || !keyboard->base)
    {
        return;
    }

    x = (center_width - keyboard->base->w) / 2;
    y = top_y - keyboard->base->h - 8;
    y = (y < 48) ? 48 : y;
    if (y > screen_height - keyboard->base->h - 8)
    {
        y = screen_height - keyboard->base->h - 8;
    }

    GameKeyboard_SetPosition(keyboard, x, y);
}

void GameKeyboard_BakeIntoBackground(GameKeyboard* keyboard,
                                     SDL_Surface*  background)
{
    if (!keyboard || !keyboard->base || !background)
    {
        return;
    }

    SDL_BlitSurface(keyboard->base, NULL, background, &keyboard->rect);
}

void GameKeyboard_DrawBase(GameKeyboard* keyboard, SDL_Surface* target)
{
    if (!keyboard || !keyboard->base || !target)
    {
        return;
    }

    SDL_BlitSurface(keyboard->base, NULL, target, &keyboard->rect);
}

void GameKeyboard_QueueErase(GameKeyboard* keyboard)
{
    if (!keyboard || !keyboard->base)
    {
        return;
    }

    EraseObject(keyboard->base, keyboard->rect.x, keyboard->rect.y);
}

int GameKeyboard_DrawGreenKey(GameKeyboard* keyboard, int key_index)
{
    SDL_Surface* key;

    if (!keyboard)
    {
        return 0;
    }

    key = load_key_surface(keyboard->green, key_index, GetKeyPos,
                           keyboard->rect.w, keyboard->rect.h);
    if (!key)
    {
        return 0;
    }

    return DrawObject(key, keyboard->rect.x, keyboard->rect.y);
}

int GameKeyboard_DrawGreenChar(GameKeyboard* keyboard, wchar_t ch)
{
    return GameKeyboard_DrawGreenKey(keyboard, GetIndex(ch));
}

int GameKeyboard_BlitGreenKey(GameKeyboard* keyboard, int key_index,
                              SDL_Surface* target)
{
    SDL_Surface* key;

    if (!keyboard || !target)
    {
        return 0;
    }

    key = load_key_surface(keyboard->green, key_index, GetKeyPos,
                           keyboard->rect.w, keyboard->rect.h);
    if (!key)
    {
        return 0;
    }

    SDL_BlitSurface(key, NULL, target, &keyboard->rect);
    return 1;
}

int GameKeyboard_DrawRedKey(GameKeyboard* keyboard, int key_index,
                            SDL_Surface* target)
{
    SDL_Surface* key;

    if (!keyboard || !target)
    {
        return 0;
    }

    key = load_key_surface(keyboard->red, key_index, GetWrongKeyPos,
                           keyboard->rect.w, keyboard->rect.h);
    if (!key)
    {
        return 0;
    }

    SDL_BlitSurface(key, NULL, target, &keyboard->rect);
    return 1;
}

int GameKeyboard_DrawShiftForKey(GameKeyboard* keyboard, int key_index,
                                 SDL_Surface* target)
{
    char         fn[FNLEN];
    int          shift;
    SDL_Surface* surf;

    if (!keyboard || !target)
    {
        return 0;
    }

    shift = GetShift(key_index);
    if (shift < 0 || shift > 2)
    {
        return 0;
    }

    if (!keyboard->shift[shift])
    {
        GetKeyShift(key_index, fn);
        keyboard->shift[shift] = scale_surface(
            LoadImage(fn, IMG_ALPHA), keyboard->rect.w, keyboard->rect.h);
    }

    surf = keyboard->shift[shift];
    if (!surf)
    {
        return 0;
    }

    SDL_BlitSurface(surf, NULL, target, &keyboard->rect);
    return 1;
}
