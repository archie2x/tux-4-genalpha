/* Hands-and-fingers hint widget — see hand_display.h. */

#include "hand_display.h"

#include "funcs.h"
#include <string.h>

/* Map a braille dot key ('f','d','s','j','k','l') to its finger index in
 * the GetFinger() convention (0..9). 0xff = not a dot key. */
static unsigned char dot_to_finger(wchar_t ch)
{
    switch (ch)
    {
    case L's':
        return 1; /* left  ring   */
    case L'd':
        return 2; /* left  middle */
    case L'f':
        return 3; /* left  index  */
    case L'j':
        return 6; /* right index  */
    case L'k':
        return 7; /* right middle */
    case L'l':
        return 8; /* right ring   */
    default:
        return 0xff;
    }
}

static void blit_at(HandDisplay* hd, SDL_Surface* src, SDL_Surface* dst)
{
    if (src)
    {
        SDL_Rect r = hd->rect;
        SDL_BlitSurface(src, NULL, dst, &r);
    }
}

void Hand_Display_Init(HandDisplay* hd)
{
    if (hd)
    {
        memset(hd, 0, sizeof(*hd));
    }
}

int Hand_Display_Load(HandDisplay* hd)
{
    char fn[FNLEN];

    if (!hd)
    {
        return 0;
    }
    Hand_Display_Free(hd);

    hd->base = LoadImage("hands/hands.png", IMG_ALPHA);
    if (!hd->base)
    {
        return 0;
    }
    hd->rect.w = hd->base->w;
    hd->rect.h = hd->base->h;

    for (int i = 0; i < 10; i++)
    {
        snprintf(fn, FNLEN, "hands/%d.png", i);
        hd->finger[i] = LoadImage(fn, IMG_ALPHA);
        if (!hd->finger[i])
        {
            return 0;
        }
    }

    hd->shift[0] = LoadImage("hands/none.png", IMG_ALPHA);
    hd->shift[1] = LoadImage("hands/lshift.png", IMG_ALPHA);
    hd->shift[2] = LoadImage("hands/rshift.png", IMG_ALPHA);
    if (!hd->shift[0] || !hd->shift[1] || !hd->shift[2])
    {
        return 0;
    }
    return 1;
}

void Hand_Display_Free(HandDisplay* hd)
{
    if (!hd)
    {
        return;
    }
    if (hd->base)
    {
        SDL_DestroySurface(hd->base);
        hd->base = NULL;
    }
    for (int i = 0; i < 10; i++)
    {
        if (hd->finger[i])
        {
            SDL_DestroySurface(hd->finger[i]);
            hd->finger[i] = NULL;
        }
    }
    for (int i = 0; i < 3; i++)
    {
        if (hd->shift[i])
        {
            SDL_DestroySurface(hd->shift[i]);
            hd->shift[i] = NULL;
        }
    }
}

void Hand_Display_SetPosition(HandDisplay* hd, int x, int y)
{
    if (!hd)
    {
        return;
    }
    hd->rect.x = x;
    hd->rect.y = y;
}

void Hand_Display_DrawBase(HandDisplay* hd, SDL_Surface* dst)
{
    if (!hd || !dst)
    {
        return;
    }
    blit_at(hd, hd->base, dst);
}

void Hand_Display_DrawForKey(HandDisplay* hd, int finger, int shift,
                             SDL_Surface* dst)
{
    if (!hd || !dst)
    {
        return;
    }
    blit_at(hd, hd->base, dst);
    if (finger >= 0 && finger < 10)
    {
        blit_at(hd, hd->finger[finger], dst);
    }
    if (shift >= 0 && shift < 3)
    {
        blit_at(hd, hd->shift[shift], dst);
    }
}

void Hand_Display_DrawForChord(HandDisplay* hd, const wchar_t* dots, int n,
                               SDL_Surface* dst)
{
    if (!hd || !dst || !dots)
    {
        return;
    }
    blit_at(hd, hd->base, dst);
    for (int i = 0; i < n; i++)
    {
        unsigned char f = dot_to_finger(dots[i]);
        if (f < 10)
        {
            blit_at(hd, hd->finger[f], dst);
        }
    }
}
