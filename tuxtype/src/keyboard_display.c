/* Keyboard overlay widget — load + scale keyboard.png + per-key green/red
 * overlays, position above a target row, bake into a background, draw
 * green/red key highlights. */

#include "keyboard_display.h"

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

void Kbd_Display_Init(KbdDisplay* keyboard)
{
    if (!keyboard)
    {
        return;
    }

    memset(keyboard, 0, sizeof(*keyboard));
}

int Kbd_Display_Load(KbdDisplay* keyboard, int alpha, int target_w)
{
    if (!keyboard)
    {
        return 0;
    }

    Kbd_Display_Free(keyboard);
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

void Kbd_Display_Free(KbdDisplay* keyboard)
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

void Kbd_Display_SetPosition(KbdDisplay* keyboard, int x, int y)
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

void Kbd_Display_SetPositionAbove(KbdDisplay* keyboard, int center_width,
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

    Kbd_Display_SetPosition(keyboard, x, y);
}

void Kbd_Display_BakeIntoBackground(KbdDisplay*  keyboard,
                                    SDL_Surface* background)
{
    if (!keyboard || !keyboard->base || !background)
    {
        return;
    }

    SDL_BlitSurface(keyboard->base, NULL, background, &keyboard->rect);
}

void Kbd_Display_DrawBase(KbdDisplay* keyboard, SDL_Surface* target)
{
    SDL_Surface* bg;

    if (!keyboard || !keyboard->base || !target)
    {
        return;
    }

    /* Base PNG has transparent pixels in the inter-key gaps. Restore the
     * background under our footprint first so previous green/red key
     * overlays don't bleed through those gaps. */
    bg = CurrentBkgd();
    if (bg)
    {
        SDL_BlitSurface(bg, &keyboard->rect, target, &keyboard->rect);
    }

    SDL_BlitSurface(keyboard->base, NULL, target, &keyboard->rect);
}

void Kbd_Display_QueueErase(KbdDisplay* keyboard)
{
    if (!keyboard || !keyboard->base)
    {
        return;
    }

    EraseObject(keyboard->base, keyboard->rect.x, keyboard->rect.y);
}

int Kbd_Display_DrawGreenKey(KbdDisplay* keyboard, int key_index)
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

int Kbd_Display_DrawGreenChar(KbdDisplay* keyboard, wchar_t ch)
{
    return Kbd_Display_DrawGreenKey(keyboard, GetIndex(ch));
}

int Kbd_Display_BlitGreenKey(KbdDisplay* keyboard, int key_index,
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

int Kbd_Display_DrawRedKey(KbdDisplay* keyboard, int key_index,
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

int Kbd_Display_DrawShiftForKey(KbdDisplay* keyboard, int key_index,
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
