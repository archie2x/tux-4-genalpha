/* Visible-bell overlay (see t4k/visible_bell.h). Surface-based mirror of
 * the renderer-style design: a single pre-allocated white SDL_Surface
 * that's alpha-blended over the target each frame while active. */

#include <t4k/visible_bell.h>

#define DEFAULT_DURATION_MS 200
#define DEFAULT_PEAK_ALPHA 40 // opacity, 0 to 255

void T4K_VisibleBell_Init(T4K_VisibleBell* bell, Uint32 duration_ms,
                          Uint8 peak_alpha)
{
    if (!bell)
    {
        return;
    }
    bell->end_time    = 0;
    bell->duration_ms = duration_ms ? duration_ms : DEFAULT_DURATION_MS;
    bell->peak_alpha  = peak_alpha ? peak_alpha : DEFAULT_PEAK_ALPHA;
    bell->overlay     = NULL;
}

void T4K_VisibleBell_Free(T4K_VisibleBell* bell)
{
    if (!bell || !bell->overlay)
    {
        return;
    }
    SDL_DestroySurface(bell->overlay);
    bell->overlay = NULL;
}

void T4K_VisibleBell_Trigger(T4K_VisibleBell* bell)
{
    if (!bell)
    {
        return;
    }
    bell->end_time = SDL_GetTicks() + bell->duration_ms;
}

bool T4K_VisibleBell_Active(const T4K_VisibleBell* bell)
{
    return bell && SDL_GetTicks() < bell->end_time;
}

void T4K_VisibleBell_Render(T4K_VisibleBell* bell, SDL_Surface* target)
{
    if (!bell || !target || !T4K_VisibleBell_Active(bell))
    {
        return;
    }

    /* (Re)create overlay if missing or target was resized. RGBA32 chosen
     * so SDL_BLENDMODE_BLEND + alpha-mod work regardless of target's
     * native pixel format. */
    if (!bell->overlay || bell->overlay->w != target->w ||
        bell->overlay->h != target->h)
    {
        if (bell->overlay)
        {
            SDL_DestroySurface(bell->overlay);
        }
        bell->overlay =
            SDL_CreateSurface(target->w, target->h, SDL_PIXELFORMAT_RGBA32);
        if (!bell->overlay)
        {
            return;
        }
        SDL_FillSurfaceRect(
            bell->overlay, NULL,
            SDL_MapRGBA(SDL_GetPixelFormatDetails(bell->overlay->format), NULL,
                        255, 255, 255, 255));
        SDL_SetSurfaceBlendMode(bell->overlay, SDL_BLENDMODE_BLEND);
    }

    Uint64 now = SDL_GetTicks();
    float  t   = (float)(bell->end_time - now) / (float)bell->duration_ms;
    if (t < 0.0f)
    {
        t = 0.0f;
    }
    if (t > 1.0f)
    {
        t = 1.0f;
    }
    Uint8 alpha = (Uint8)(bell->peak_alpha * t);

    SDL_SetSurfaceAlphaMod(bell->overlay, alpha);
    SDL_BlitSurface(bell->overlay, NULL, target, NULL);
}
