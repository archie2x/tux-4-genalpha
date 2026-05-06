/* sdl3_compat.c — small SDL1/SDL_gfx → SDL3 shims for the port.
 *
 * Provides minimal stand-ins for legacy APIs that don't exist in SDL3.
 * These are best-effort: rotozoomSurface returns a non-rotated copy for
 * now (Factoroids' asteroid spin will look static — TODO: render via
 * SDL_Renderer with angle for a proper fix). */

#include <SDL3/SDL.h>
#include <math.h>
#include "sdl3_compat.h"

/* qsort comparator for descending score; used by the LAN game-over code
 * which still compiles (and silently runs as single-player) in this port. */
int compare_scores(const void* p1, const void* p2)
{
    const lan_player_type* a = (const lan_player_type*)p1;
    const lan_player_type* b = (const lan_player_type*)p2;
    return b->score - a->score;
}

/* Software rotate-and-zoom replacement for SDL_gfx's rotozoomSurface.
 *
 * Inverse mapping: for each output pixel compute the source coordinate by
 * rotating backward and dividing by zoom, then nearest-neighbour sample.
 * Output is always RGBA32 (BLEND mode) so transparent fill is well-defined
 * regardless of source format. The factoroids precompute table calls this
 * 360/DEG_PER_ROTATION × 6 ≈ 1k times once at game start, so naive
 * per-pixel cost is fine.
 *
 * `smooth` (bilinear) is honored when nonzero. */
SDL_Surface* rotozoomSurface(SDL_Surface* src, double angle, double zoom,
                             int smooth)
{
    if (!src)
    {
        return NULL;
    }
    if (zoom <= 0.0)
    {
        zoom = 1.0;
    }

    /* Normalize source to RGBA32 so we can read pixels with a single load. */
    SDL_Surface* rgba      = src;
    bool         free_rgba = false;
    if (src->format != SDL_PIXELFORMAT_RGBA32)
    {
        rgba = SDL_ConvertSurface(src, SDL_PIXELFORMAT_RGBA32);
        if (!rgba)
        {
            return NULL;
        }
        free_rgba = true;
    }

    /* SDL_gfx's convention: positive angle rotates counter-clockwise on
     * screen. Negate so our inverse mapping matches that direction. */
    const double rad = -angle * M_PI / 180.0;
    const double c = cos(rad), s = sin(rad);

    /* Bounding box of the rotated+zoomed source. */
    const int sw = rgba->w, sh = rgba->h;
    int       ow = (int)ceil((fabs(sw * c) + fabs(sh * s)) * zoom);
    int       oh = (int)ceil((fabs(sw * s) + fabs(sh * c)) * zoom);
    if (ow < 1)
    {
        ow = 1;
    }
    if (oh < 1)
    {
        oh = 1;
    }

    SDL_Surface* dst = SDL_CreateSurface(ow, oh, SDL_PIXELFORMAT_RGBA32);
    if (!dst)
    {
        if (free_rgba)
        {
            SDL_DestroySurface(rgba);
        }
        return NULL;
    }
    SDL_SetSurfaceBlendMode(dst, SDL_BLENDMODE_BLEND);

    SDL_LockSurface(rgba);
    SDL_LockSurface(dst);

    const Uint32* sp       = (const Uint32*)rgba->pixels;
    Uint32*       dp       = (Uint32*)dst->pixels;
    const int     s_stride = rgba->pitch / 4;
    const int     d_stride = dst->pitch / 4;

    const double scx = sw * 0.5, scy = sh * 0.5;
    const double dcx = ow * 0.5, dcy = oh * 0.5;
    const double inv_zoom = 1.0 / zoom;

    for (int y = 0; y < oh; y++)
    {
        for (int x = 0; x < ow; x++)
        {
            const double dx = (x - dcx) * inv_zoom;
            const double dy = (y - dcy) * inv_zoom;
            const double fx = c * dx + s * dy + scx;
            const double fy = -s * dx + c * dy + scy;

            Uint32 px = 0; /* transparent fill */
            if (smooth)
            {
                /* Bilinear: weighted average of 4 neighbours. */
                int x0 = (int)floor(fx), y0 = (int)floor(fy);
                int x1 = x0 + 1, y1 = y0 + 1;
                if (x1 >= 0 && x0 < sw && y1 >= 0 && y0 < sh)
                {
                    double tx = fx - x0, ty = fy - y0;
                    double w00 = (1 - tx) * (1 - ty);
                    double w10 = tx * (1 - ty);
                    double w01 = (1 - tx) * ty;
                    double w11 = tx * ty;
                    Uint32 p00 =
                        (x0 >= 0 && y0 >= 0) ? sp[y0 * s_stride + x0] : 0;
                    Uint32 p10 =
                        (x1 < sw && y0 >= 0) ? sp[y0 * s_stride + x1] : 0;
                    Uint32 p01 =
                        (x0 >= 0 && y1 < sh) ? sp[y1 * s_stride + x0] : 0;
                    Uint32 p11 =
                        (x1 < sw && y1 < sh) ? sp[y1 * s_stride + x1] : 0;
                    double r =
                        ((p00 >> 0) & 0xff) * w00 + ((p10 >> 0) & 0xff) * w10 +
                        ((p01 >> 0) & 0xff) * w01 + ((p11 >> 0) & 0xff) * w11;
                    double g =
                        ((p00 >> 8) & 0xff) * w00 + ((p10 >> 8) & 0xff) * w10 +
                        ((p01 >> 8) & 0xff) * w01 + ((p11 >> 8) & 0xff) * w11;
                    double b = ((p00 >> 16) & 0xff) * w00 +
                               ((p10 >> 16) & 0xff) * w10 +
                               ((p01 >> 16) & 0xff) * w01 +
                               ((p11 >> 16) & 0xff) * w11;
                    double a = ((p00 >> 24) & 0xff) * w00 +
                               ((p10 >> 24) & 0xff) * w10 +
                               ((p01 >> 24) & 0xff) * w01 +
                               ((p11 >> 24) & 0xff) * w11;
                    px       = ((Uint32)r) | (((Uint32)g) << 8) |
                               (((Uint32)b) << 16) | (((Uint32)a) << 24);
                }
            }
            else
            {
                int sx = (int)(fx + 0.5), sy = (int)(fy + 0.5);
                if (sx >= 0 && sx < sw && sy >= 0 && sy < sh)
                {
                    px = sp[sy * s_stride + sx];
                }
            }
            dp[y * d_stride + x] = px;
        }
    }

    SDL_UnlockSurface(dst);
    SDL_UnlockSurface(rgba);
    if (free_rgba)
    {
        SDL_DestroySurface(rgba);
    }
    return dst;
}

/* LAN/network play stubs. The original tuxmath used SDL_net for LAN
 * multiplayer; SDL_net doesn't have an SDL3 release yet, and the
 * networking code isn't ifdef-gated everywhere. Stub everything to
 * return zeros so single-player still works. */
const char* LAN_PlayerName(int i)
{
    (void)i;
    return "";
}
int LAN_PlayerScore(int i)
{
    (void)i;
    return 0;
}
int LAN_PlayerMine(int i)
{
    (void)i;
    return 0;
}
int LAN_PlayerConnected(int i)
{
    (void)i;
    return 0;
}
int LAN_MyIndex(void)
{
    return 0;
}
int LAN_RequestIndex(void)
{
    return 0;
}

/* Server stubs */
int OurServerRunning(void)
{
    return 0;
}
int SrvrGameInProgress(void)
{
    return 0;
}
void StopServer(void)
{
}
