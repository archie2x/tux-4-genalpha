/*
   t4k_sdl.c

   Wrapper and utility functions to simplify use of the SDL libraries
   in the Tux4Kids programs (Tux Math and Tux Typing).

   Copyright 2000, 2003, 2007, 2008, 2009, 2010.
Authors: David Bruce, Sam Hart, Bill Kendrick, Tim Holy,
Boleslaw Kulbabinski, Brendan Luchen.
Project email: <tuxmath-devel@lists.sourceforge.net>
Project website: http://tux4kids.alioth.debian.org

t4k_sdl.c is part of the t4k_common library.

t4k_common is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

t4k_common is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */



#include <math.h>
#include <stdlib.h>

#include "t4k_common.h"
#include "t4k_globals.h"

SDL_Surface* screen = NULL;
SDL_Window*  t4k_window = NULL;

static ResSwitchCallback res_switch_callback = NULL;
static ResSwitchCallback internal_res_switch_callback = NULL;

/* SDL3 port: we render everything into a fixed-size logical backing surface
 * so games designed for 640x480 keep their layout regardless of the actual
 * window/fullscreen resolution. T4K_UpdateRect blits this scaled (preserving
 * aspect, letterboxed) onto the real window surface. */
#define T4K_LOGICAL_W 640
#define T4K_LOGICAL_H 480
static SDL_Surface* t4k_backing = NULL;

/* Idle cursor hiding: any mouse activity (motion, button) makes the cursor
 * visible; after T4K_CURSOR_IDLE_MS of stillness, hide it again. Driven by
 * t4k_event_filter (resets the timer) + t4k_present (polls each frame).
 * We query SDL_CursorVisible() instead of caching, so direct SDL_*Cursor
 * calls elsewhere can't desync our state. */
#define T4K_CURSOR_IDLE_MS 2000
static Uint64 last_mouse_activity_ms = 0;

static bool SDLCALL t4k_event_filter(void* userdata, SDL_Event* event);

/* window size */
int win_res_x = T4K_LOGICAL_W;
int win_res_y = T4K_LOGICAL_H;

/* full screen size (set in initialize_SDL() ) */
int fs_res_x = 0;
int fs_res_y = 0;

/* SDL3 has no global "video surface". Apps create a window and we cache the
 * pointer here so T4K_GetScreen()/T4K_UpdateRect()/T4K_SwitchScreenMode work
 * unchanged for callers. tuxtype (or any consumer) calls this after creating
 * its window. */
SDL_Window* T4K_GetWindow(void)
{
    return t4k_window;
}

void T4K_RegisterWindow(SDL_Window* w)
{
    t4k_window = w;
    if (t4k_backing)
    {
        SDL_DestroySurface(t4k_backing);
        t4k_backing = NULL;
    }
    if (w)
    {
        int w_x, w_y;
        SDL_GetWindowSize(w, &w_x, &w_y);
        win_res_x = w_x;
        win_res_y = w_y;
        /* Allocate the backing surface at logical resolution. All game
	 * drawing goes here, then T4K_UpdateRect scales it to the window. */
        SDL_Surface*    win_surf = SDL_GetWindowSurface(w);
        SDL_PixelFormat fmt =
            win_surf ? win_surf->format : SDL_PIXELFORMAT_RGBA8888;
        t4k_backing = SDL_CreateSurface(T4K_LOGICAL_W, T4K_LOGICAL_H, fmt);
        screen      = t4k_backing;
        last_mouse_activity_ms = SDL_GetTicks();
        SDL_SetEventFilter(t4k_event_filter, NULL);
    }
    else
    {
        screen = NULL;
        SDL_SetEventFilter(NULL, NULL);
    }
}

/* Compute the target rect on the window surface for the backing blit,
 * preserving aspect ratio with letterbox/pillarbox. */
static void t4k_compute_present_rect(SDL_Rect* out)
{
    int win_w = T4K_LOGICAL_W, win_h = T4K_LOGICAL_H;
    if (t4k_window)
    {
        SDL_GetWindowSize(t4k_window, &win_w, &win_h);
    }
    /* Surface size may differ from logical window size on HiDPI; use the
     * window-surface dims for the actual blit target. */
    SDL_Surface* ws = t4k_window ? SDL_GetWindowSurface(t4k_window) : NULL;
    if (ws)
    {
        win_w = ws->w;
        win_h = ws->h;
    }

    float src_aspect = (float)T4K_LOGICAL_W / (float)T4K_LOGICAL_H;
    float dst_aspect = (float)win_w / (float)win_h;
    if (dst_aspect > src_aspect)
    {
        /* Window is wider than logical → pillarbox left/right */
        out->h = win_h;
        out->w = (int)(win_h * src_aspect + 0.5f);
        out->x = (win_w - out->w) / 2;
        out->y = 0;
    }
    else
    {
        /* Window is taller than logical → letterbox top/bottom */
        out->w = win_w;
        out->h = (int)(win_w / src_aspect + 0.5f);
        out->x = 0;
        out->y = (win_h - out->h) / 2;
    }
}

/* SDL event filter: rewrite mouse coordinates from window-space into the
 * 640x480 backing-surface space all hit-tests are written against. Without
 * this, fullscreen / non-1:1 windows would receive clicks the rest of the
 * code can't match against any rect. Installed once, in T4K_RegisterWindow. */
static bool SDLCALL t4k_event_filter(void* userdata, SDL_Event* event)
{
    (void)userdata;
    if (!t4k_window)
    {
        return true;
    }

    float* px = NULL;
    float* py = NULL;
    switch (event->type)
    {
    case SDL_EVENT_MOUSE_MOTION:
        px = &event->motion.x;
        py = &event->motion.y;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        px = &event->button.x;
        py = &event->button.y;
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        px = &event->wheel.mouse_x;
        py = &event->wheel.mouse_y;
        break;
    default:
        return true;
    }

    /* Any mouse activity counts: refresh the idle timer and ensure the
     * cursor is shown. SDL_ShowCursor is a no-op if already visible. */
    last_mouse_activity_ms = SDL_GetTicks();
    if (!SDL_CursorVisible())
    {
        SDL_ShowCursor();
    }

    int win_w = T4K_LOGICAL_W, win_h = T4K_LOGICAL_H;
    SDL_GetWindowSize(t4k_window, &win_w, &win_h);
    SDL_Surface* ws     = SDL_GetWindowSurface(t4k_window);
    int          surf_w = ws ? ws->w : win_w;
    int          surf_h = ws ? ws->h : win_h;

    SDL_Rect dst;
    t4k_compute_present_rect(&dst);
    if (dst.w <= 0 || dst.h <= 0)
    {
        return true;
    }

    /* event coords are in window-logical units; scale to surface units,
     * subtract the present-rect offset, then scale into 640x480. */
    float sx     = (float)surf_w / (float)win_w;
    float sy     = (float)surf_h / (float)win_h;
    float surf_x = (*px) * sx;
    float surf_y = (*py) * sy;
    *px          = (surf_x - dst.x) * (float)T4K_LOGICAL_W / (float)dst.w;
    *py          = (surf_y - dst.y) * (float)T4K_LOGICAL_H / (float)dst.h;
    return true;
}

/* Push the backing surface to the window, scaling to fit + letterboxing. */
static void t4k_present(void)
{
    if (!t4k_window || !t4k_backing)
    {
        return;
    }
    SDL_Surface* ws = SDL_GetWindowSurface(t4k_window);
    if (!ws)
    {
        return;
    }

    if (SDL_CursorVisible() &&
        SDL_GetTicks() - last_mouse_activity_ms > T4K_CURSOR_IDLE_MS)
    {
        SDL_HideCursor();
    }
    SDL_Rect dst;
    t4k_compute_present_rect(&dst);
    /* Clear the letterbox area to black before scaling the backing in. */
    if (dst.x > 0 || dst.y > 0 || dst.w < ws->w || dst.h < ws->h)
    {
        SDL_FillSurfaceRect(
            ws, NULL,
            SDL_MapRGB(SDL_GetPixelFormatDetails(ws->format), NULL, 0, 0, 0));
    }
    SDL_BlitSurfaceScaled(t4k_backing, NULL, ws, &dst, SDL_SCALEMODE_LINEAR);
    SDL_UpdateWindowSurface(t4k_window);
}

const char* s_font_name = DEFAULT_FONT_NAME;

void T4K_SetFontName(const char* name)
{
    DEBUGMSG(debug_sdl, "Switching font to %s\n", name);
    s_font_name = name;
}

const char* T4K_AskFontName()
{
    return s_font_name;
}

/* Returns the logical backing surface (always T4K_LOGICAL_W x T4K_LOGICAL_H).
 * All game rendering targets this; T4K_UpdateRect scales it to the window. */
SDL_Surface* T4K_GetScreen()
{
    if (!t4k_backing)
    {
        fprintf(stderr, "T4K_GetScreen(): no backing surface. "
                        "Call T4K_RegisterWindow() after SDL_CreateWindow.\n");
        return NULL;
    }
    return t4k_backing;
}

/*
 * T4K_GetResolutions() takes int pointer args for the windowed and
 * fullscreen resolutions and fills them in with the current values.
 * Returns 1 if successful, 0 otherwise.
 */

int T4K_GetResolutions(int* win_x, int* win_y, int* full_x, int* full_y)
{
    if(!win_x || !win_y || !full_x || !full_y)
    {
	fprintf(stderr, "T4K_GetResolutions() - invalid pointer arg");
    return 0;
    }

    *win_x = win_res_x;
    *win_y = win_res_y;
    *full_x = fs_res_x;
    *full_y = fs_res_y;

    return 1;
}

/* T4K_DrawButton() creates a translucent button with rounded ends
   and draws it on the screen.
   All colors and alpha values are supported.*/
void T4K_DrawButton(SDL_Rect* target_rect,
	int radius,
	Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    T4K_DrawButtonOn(screen, target_rect, radius, r, g, b, a);
}

void T4K_DrawButtonOn(SDL_Surface* target,
	SDL_Rect* target_rect,
	int radius,
	Uint8 r, Uint8 g, Uint8 b, Uint8 a)

{
    SDL_Surface* tmp_surf = T4K_CreateButton(target_rect->w, target_rect->h,
	    radius, r, g, b, a);
    SDL_BlitSurface(tmp_surf, NULL, target, target_rect);
    SDL_DestroySurface(tmp_surf);
}



/* T4K_CreateButton() creates a translucent button with rounded ends
   All colors and alpha values are supported.*/
SDL_Surface* T4K_CreateButton(int w, int h, int radius,
	Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    /* NOTE - we use a 32-bit temp surface even if we have a 16-bit */
    /* screen - it gets converted during blitting.                  */
    SDL_Surface* tmp_surf = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA8888);
    /* SDL3 surfaces default to SDL_BLENDMODE_NONE; without this the alpha
     * channel is ignored on blit and the button appears fully opaque. */
    SDL_SetSurfaceBlendMode(tmp_surf, SDL_BLENDMODE_BLEND);

    Uint32 color = SDL_MapRGBA(SDL_GetPixelFormatDetails(tmp_surf->format),
                               NULL, r, g, b, a);
    SDL_FillSurfaceRect(tmp_surf, NULL, color);
    T4K_RoundCorners(tmp_surf, radius);
    return tmp_surf;
}


void T4K_RoundCorners(SDL_Surface* s, Uint16 radius)
{
    int y = 0;
    int x_dist, y_dist;
    Uint32* p = NULL;
    Uint32 alpha_mask;
    int bytes_per_pix;

    if (!s)
	return;
    if (!SDL_LockSurface(s))
    {
        return;
    }

    bytes_per_pix = SDL_BYTESPERPIXEL(s->format);
    if (bytes_per_pix != 4)
	return;

    /* radius cannot be more than half of width or height: */
    if (radius > (s->w)/2)
	radius = (s->w)/2;
    if (radius > (s->h)/2)
	radius = (s->h)/2;

    alpha_mask = SDL_GetPixelFormatDetails(s->format)->Amask;

    /* Now round off corners: */
    /* upper left:            */
    for (y = 0; y < radius; y++)
    {
	p = (Uint32*)(s->pixels + (y * s->pitch));
	x_dist = radius;
	y_dist = radius - y;

	while (((x_dist * x_dist) + (y_dist * y_dist)) > (radius * radius))
	{
	    /* (make pixel (x,y) transparent) */
	    *p = *p & ~alpha_mask;
	    p++;
	    x_dist--;
	}
    }

    /* upper right:            */
    for (y = 0; y < radius; y++)
    {
	/* start at end of top row: */
	p = (Uint32*)(s->pixels + ((y + 1) * s->pitch) - bytes_per_pix);

	x_dist = radius;
	y_dist = radius - y;

	while (((x_dist * x_dist) + (y_dist * y_dist)) > (radius * radius))
	{
	    /* (make pixel (x,y) transparent) */
	    *p = *p & ~alpha_mask;
	    p--;
	    x_dist--;
	}
    }

    /* bottom left:            */
    for (y = (s->h - 1); y > (s->h - radius); y--)
    {
	/* start at beginning of bottom row */
	p = (Uint32*)(s->pixels + (y * s->pitch));
	x_dist = radius;
	y_dist = y - (s->h - radius);

	while (((x_dist * x_dist) + (y_dist * y_dist)) > (radius * radius))
	{
	    /* (make pixel (x,y) transparent) */
	    *p = *p & ~alpha_mask;
	    p++;
	    x_dist--;
	}
    }

    /* bottom right:            */
    for (y = (s->h - 1); y > (s->h - radius); y--)
    {
	/* start at end of bottom row */
	p = (Uint32*)(s->pixels + ((y + 1) * s->pitch) - bytes_per_pix);
	x_dist = radius;
	y_dist = y - (s->h - radius);

	while (((x_dist * x_dist) + (y_dist * y_dist)) > (radius * radius))
	{
	    /* (make pixel (x,y) transparent) */
	    *p = *p & ~alpha_mask;
	    p--;
	    x_dist--;
	}
    }
    SDL_UnlockSurface(s);
}

/**********************
Flip:
input: a SDL_Surface, x, y
output: a copy of the SDL_Surface flipped via rules:

if x is a nonzero value, then flip horizontally
if y is a nonzero value, then flip vertically

note: you can have it flip both
 **********************/
SDL_Surface* T4K_Flip( SDL_Surface *in, int x, int y ) {
    SDL_Surface* out;
    SDL_Rect from_rect, to_rect;
    Uint32       colorkey     = 0;
    bool         has_colorkey = SDL_SurfaceHasColorKey(in);

    if (has_colorkey)
    {
        SDL_GetSurfaceColorKey(in, &colorkey);
        /* Temporarily disable color key during blit */
        SDL_SetSurfaceColorKey(in, false, 0);
    }

    /* --- create our new surface --- */
    out = SDL_CreateSurface(in->w, in->h, SDL_PIXELFORMAT_RGBA8888);
    if (!out)
    {
        return NULL;
    }

    /* --- flip horizontally if requested --- */
    if (x) {
	from_rect.h = to_rect.h = in->h;
	from_rect.w = to_rect.w = 1;
	from_rect.y = to_rect.y = 0;
	from_rect.x = 0;
	to_rect.x = in->w - 1;

	do {
	    SDL_BlitSurface(in, &from_rect, out, &to_rect);
	    from_rect.x++;
	    to_rect.x--;
	} while (to_rect.x >= 0);
    }

    /* --- flip vertically if requested --- */
    if (y) {
	from_rect.h = to_rect.h = 1;
	from_rect.w = to_rect.w = in->w;
	from_rect.x = to_rect.x = 0;
	from_rect.y = 0;
	to_rect.y = in->h - 1;

	do {
	    SDL_BlitSurface(in, &from_rect, out, &to_rect);
	    from_rect.y++;
	    to_rect.y--;
	} while (to_rect.y >= 0);
    }

    /* --- restore colorkey on in and replicate on out --- */
    if (has_colorkey)
    {
        SDL_SetSurfaceColorKey(in, true, colorkey);
        SDL_SetSurfaceColorKey(out, true, colorkey);
    }

    SDL_UnlockSurface(in);

    return out;
}

/* Blend two surfaces together. The third argument is between 0.0 and
   1.0, and represents the weight assigned to the first surface.  If
   the pointer to the second surface is NULL, this performs fading.

   Currently this works only with RGBA images, but this is largely to
   make the (fast) pointer arithmetic work out; it could be easily
   generalized to other image types. */
SDL_Surface* T4K_Blend(SDL_Surface *S1, SDL_Surface *S2, float gamma)
{
    const SDL_PixelFormatDetails *fmt1, *fmt2;
    Uint8 r1, r2, g1, g2, b1, b2, a1, a2;
    SDL_Surface *tmpS, *ret;
    Uint32 *cpix1, *epix1, *cpix2, *epix2;
    float gamflip;

    if (!S1)
	return NULL;

    fmt1 = fmt2 = NULL;
    tmpS = ret = NULL;

    gamflip = 1.0 - gamma;
    if (gamma < 0 || gamflip < 0)
    {
	perror("gamma must be between 0 and 1");
	exit(0);
    }

    fmt1 = SDL_GetPixelFormatDetails(S1->format);

    if (fmt1 && fmt1->bits_per_pixel != 32)
    {
	perror("This works only with RGBA images");
	return S1;
    }
    if (S2 != NULL)
    {
        fmt2 = SDL_GetPixelFormatDetails(S2->format);
        if (fmt2->bits_per_pixel != 32)
        {
            perror("This works only with RGBA images");
            return S1;
        }
        // Check that both images have the same width dimension
        if (S1->w != S2->w)
        {
            fprintf(stderr, "S1->w %d, S2->w %d;  S1->h %d, S2->h %d\n", S1->w,
                    S2->w, S1->h, S2->h);
            fprintf(stderr,
                    "Both images must have the same width dimensions\n");
            return S1;
        }
    }

    tmpS = SDL_ConvertSurface(S1, S1->format);
    if (tmpS == NULL)
    {
	perror("SDL_ConvertSurface() failed");
	return S1;
    }
    if (!SDL_LockSurface(tmpS))
    {
	perror("SDL_LockSurface() failed");
	return S1;
    }

    // We're going to go through the pixels in reverse order, to start
    // from the bottom of each image. That way, we can blend things that
    // are not of the same height and have them align at the bottom.
    epix1 = (Uint32*) tmpS->pixels - 1;
    cpix1 = epix1 + tmpS->w * tmpS->h;
    if (S2 != NULL && SDL_LockSurface(S2))
    {
	epix2 = (Uint32*) S2->pixels - 1;
	cpix2 = epix2 + S2->w * S2->h;
    }
    else
    {
	epix2 = epix1;
	cpix2 = cpix1;
    }

    for (; cpix1 > epix1; cpix1--, cpix2--)
    {
        SDL_GetRGBA(*cpix1, fmt1, NULL, &r1, &g1, &b1, &a1);
        a1 = gamma * a1;
        if (S2 != NULL && cpix2 > epix2)
        {
            SDL_GetRGBA(*cpix2, fmt2, NULL, &r2, &g2, &b2, &a2);
            r1 = gamma * r1 + gamflip * r2;
            g1 = gamma * g1 + gamflip * g2;
            b1 = gamma * b1 + gamflip * b2;
            a1 += gamflip * a2;
        }
        *cpix1 = SDL_MapRGBA(fmt1, NULL, r1, g1, b1, a1);
    }

    SDL_UnlockSurface(tmpS);

    if (S2 != NULL)
	SDL_UnlockSurface(S2);

    ret = SDL_DuplicateSurface(tmpS);
    SDL_DestroySurface(tmpS);

    return ret;
}


/* free every surface in the array together with the array itself */
void T4K_FreeSurfaceArray(SDL_Surface** surfs, int length)
{
    int i;

    if(surfs == NULL)
	return;

    for(i = 0; i < length; i++)
	if(surfs[i] != NULL)
        SDL_DestroySurface(surfs[i]);
    free(surfs);
}

int T4K_inRect( SDL_Rect r, int x, int y) {
    if ((x < r.x) || (y < r.y) || (x > r.x + r.w) || (y > r.y + r.h))
	return 0;
    return 1;
}

void T4K_UpdateRect(SDL_Surface* surf, SDL_Rect* rect)
{
    /* In SDL3 surface mode the only relevant update is on the window
     * surface — everything else is just an in-memory blit target. The
     * partial-rect optimization is gone; full-window update is fast. */
    (void)surf;
    (void)rect;
    if (t4k_window)
    {
        t4k_present();
    }
}

void T4K_SetRect(SDL_Rect* rect, const float* pos)
{
    rect->x = pos[0] * screen->w;
    rect->y = pos[1] * screen->h;
    rect->w = pos[2] * screen->w;
    rect->h = pos[3] * screen->h;
}

/* Darkens the screen by a factor of 2^bits */
void T4K_DarkenScreen(Uint8 bits)
{
#if PIXEL_BITS == 32
    Uint32* p;
#elif PIXEL_BITS == 16
    Uint16* p;
#else
    Uint16* p;
    return;
#endif
    Uint32 rm = SDL_GetPixelFormatDetails(screen->format)->Rmask;
    Uint32 gm = SDL_GetPixelFormatDetails(screen->format)->Gmask;
    Uint32 bm = SDL_GetPixelFormatDetails(screen->format)->Bmask;
    /* Preserve alpha. Without this, on ARGB surfaces the cleared alpha
     * channel makes the darkened backing transparent at present time and
     * the dim is invisible. */
    Uint32 am = SDL_GetPixelFormatDetails(screen->format)->Amask;

    int x, y;

    /* (realistically, 1 and 2 are the only useful values) */
    if (bits > 8)
	return;

    p = screen->pixels;

    for (y = 0; y < screen->h; y++)
    {
	for (x = 0; x < screen->w; x++)
	{
        *p = (((*p & rm) >> bits) & rm) | (((*p & gm) >> bits) & gm) |
             (((*p & bm) >> bits) & bm) | ((*p) & am);
        p++;
    }
    }
}

/* change window size (works only in windowed mode) */
void T4K_ChangeWindowSize(int new_res_x, int new_res_y)
{
    if (!t4k_window)
    {
        return;
    }

    if (SDL_GetWindowFlags(t4k_window) & SDL_WINDOW_FULLSCREEN)
    {
        DEBUGMSG(debug_sdl,
                 "T4K_ChangeWindowSize() can be run only in windowed mode!\n");
        return;
    }

    SDL_SetWindowSize(t4k_window, new_res_x, new_res_y);
    int w, h;
    SDL_GetWindowSize(t4k_window, &w, &h);
    win_res_x = w;
    win_res_y = h;
    DEBUGMSG(debug_sdl, "T4K_ChangeWindowSize(): %d x %d\n", w, h);

    if (res_switch_callback)
    {
        res_switch_callback(win_res_x, win_res_y);
    }

    t4k_present();
}

/* switch between fullscreen and windowed mode */
void T4K_SwitchScreenMode(void)
{
    if (!t4k_window)
    {
        return;
    }

    bool was_fullscreen =
        (SDL_GetWindowFlags(t4k_window) & SDL_WINDOW_FULLSCREEN) != 0;

    if (!SDL_SetWindowFullscreen(t4k_window, !was_fullscreen))
    {
        fprintf(stderr, "\nError: I could not switch to %s mode: %s\n",
                was_fullscreen ? "windowed" : "fullscreen", SDL_GetError());
        return;
    }

    DEBUGMSG(debug_sdl, "Switched screen mode to %s\n",
             was_fullscreen ? "windowed" : "fullscreen");

    int w, h;
    SDL_GetWindowSize(t4k_window, &w, &h);
    if (res_switch_callback)
    {
        res_switch_callback(w, h);
    }
    if (internal_res_switch_callback)
    {
        internal_res_switch_callback(w, h);
    }

    t4k_present();
}

void internal_res_switch_handler(ResSwitchCallback callback)
{
    internal_res_switch_callback = callback;
}

void T4K_OnResolutionSwitch (ResSwitchCallback callback)
{
    res_switch_callback = callback;
}

/* T4K_WaitForEvent removed in SDL3 port — SDL_EventMask/SDL_EVENTMASK are gone
 * in SDL3 and the function had no callers outside this file. */

/* Swiped shamelessly from TuxPaint
   Based on code from: http://www.codeproject.com/cs/media/imageprocessing4.asp
   copyright 2002 Christian Graus */

SDL_Surface* T4K_zoom(SDL_Surface* src, int new_w, int new_h)
{
    SDL_Surface* s;

    /* These function pointers will point to the appropriate */
    /* putpixel() and getpixel() variants to be used in the  */
    /* current colorspace:                                   */
    void (*putpixel) (SDL_Surface*, int, int, Uint32);
    Uint32(*getpixel) (SDL_Surface*, int, int);

    float xscale, yscale;
    int x, y;
    int floor_x, ceil_x,
	floor_y, ceil_y;
    float fraction_x, fraction_y,
	  one_minus_x, one_minus_y;
    float n1, n2;
    Uint8 r1, g1, b1, a1;
    Uint8 r2, g2, b2, a2;
    Uint8 r3, g3, b3, a3;
    Uint8 r4, g4, b4, a4;
    Uint8 r, g, b, a;

    DEBUGMSG(debug_sdl, "Entering T4K_zoom():\n");

    /* Create surface for zoom: */

    s = SDL_CreateSurface(new_w, new_h, src->format);

    if (s == NULL)
    {
	fprintf(stderr, "\nError: Can't build zoom surface\n"
		"The Simple DirectMedia Layer error that occurred was:\n"
		"%s\n\n", SDL_GetError());
	return NULL;
	//    cleanup();
	//    exit(1);
    }

    DEBUGMSG(debug_sdl, "T4K_zoom(): orig surface %dx%d, %d bytes per pixel\n",
             src->w, src->h, SDL_BYTESPERPIXEL(src->format));
    DEBUGMSG(debug_sdl, "T4K_zoom(): new surface %dx%d, %d bytes per pixel\n",
             s->w, s->h, SDL_BYTESPERPIXEL(s->format));

    /* Now assign function pointers to correct functions based */
    /* on data format of original and zoomed surfaces:         */
    getpixel = getpixels[SDL_BYTESPERPIXEL(src->format)];
    putpixel = putpixels[SDL_BYTESPERPIXEL(s->format)];

    SDL_LockSurface(src);
    SDL_LockSurface(s);

    xscale = (float) src->w / (float) new_w;
    yscale = (float) src->h / (float) new_h;

    for (x = 0; x < new_w; x++)
    {
	for (y = 0; y < new_h; y++)
	{
	    /* Here we calculate the new RGBA values for each pixel */
	    /* using a "weighted average" of the four pixels in the */
	    /* corresponding location in the orginal surface:       */

	    /* figure out which original pixels to use in the calc: */
        floor_x = floorf((float)x * xscale);
        ceil_x = floor_x + 1;
	    if (ceil_x >= src->w)
		ceil_x = floor_x;

        floor_y = floorf((float)y * yscale);
        ceil_y = floor_y + 1;
	    if (ceil_y >= src->h)
		ceil_y = floor_y;

	    fraction_x = x * xscale - floor_x;
	    fraction_y = y * yscale - floor_y;

	    one_minus_x = 1.0 - fraction_x;
	    one_minus_y = 1.0 - fraction_y;

	    /* Grab their values:  */
        SDL_GetRGBA(getpixel(src, floor_x, floor_y),
                    SDL_GetPixelFormatDetails(src->format), NULL, &r1, &g1, &b1,
                    &a1);
        SDL_GetRGBA(getpixel(src, ceil_x, floor_y),
                    SDL_GetPixelFormatDetails(src->format), NULL, &r2, &g2, &b2,
                    &a2);
        SDL_GetRGBA(getpixel(src, floor_x, ceil_y),
                    SDL_GetPixelFormatDetails(src->format), NULL, &r3, &g3, &b3,
                    &a3);
        SDL_GetRGBA(getpixel(src, ceil_x, ceil_y),
                    SDL_GetPixelFormatDetails(src->format), NULL, &r4, &g4, &b4,
                    &a4);

        /* Create the weighted averages: */
        n1 = (one_minus_x * r1 + fraction_x * r2);
        n2 = (one_minus_x * r3 + fraction_x * r4);
	    r = (one_minus_y * n1 + fraction_y * n2);

	    n1 = (one_minus_x * g1 + fraction_x * g2);
	    n2 = (one_minus_x * g3 + fraction_x * g4);
	    g = (one_minus_y * n1 + fraction_y * n2);

	    n1 = (one_minus_x * b1 + fraction_x * b2);
	    n2 = (one_minus_x * b3 + fraction_x * b4);
	    b = (one_minus_y * n1 + fraction_y * n2);

	    n1 = (one_minus_x * a1 + fraction_x * a2);
	    n2 = (one_minus_x * a3 + fraction_x * a4);
	    a = (one_minus_y * n1 + fraction_y * n2);

	    /* and put them into our new surface: */
        putpixel(s, x, y,
                 SDL_MapRGBA(SDL_GetPixelFormatDetails(s->format), NULL, r, g,
                             b, a));
    }
    }

    SDL_UnlockSurface(s);
    SDL_UnlockSurface(src);

    DEBUGMSG(debug_sdl, "Leaving T4K_zoom():\n");

    return s;
}

/*************************************************/
/* TransWipe: Performs various wipes to new bkgs */
/*************************************************/
/*
 * Given a wipe request type, and any variables
 * that wipe requires, will perform a wipe from
 * the current screen image to a new one.
 * NOTE duration should be given in tenths-of-seconds
 * NOTE this transition is uninterruptible!
 */
int T4K_TransWipe(const SDL_Surface* newbkg, WipeStyle type, int segments, int duration)
{
    int i, j, x1, x2, y1, y2;
    int step1, step2, step3, step4;
    //int frame;
    SDL_Rect src;
    SDL_Rect dst;

    T4K_ResetBlitQueue();

    /* Input validation: ----------------------- */
    if (!newbkg)
    {
	fprintf(stderr, "T4K_TransWipe() - 'newbkg' arg invalid!\n");
	return 0;
    }

    /* FIXME should support scaling here - DSB */
    if(newbkg->w != screen->w || newbkg->h != screen->h)
    {
	fprintf(stderr, "T4K_TransWipe() - wrong size newbkg* arg");
	return 0;
    }

    /* segments is num of divisions */
    /* duration is how many frames animation should take */

    if(segments < 1)
	segments = 1;
    if(duration < 1)
	duration = 1;

    /* Pick a card, any card...            */
    while(type == RANDOM_WIPE)
	type = rand() % NUM_WIPES;


    T4K_ResetBlitQueue();
    //frame = 0;

    DEBUGVARX(debug_sdl, type);

    switch(type)
    {
	case WIPE_BLINDS_VERT:
	    {

		step1 = screen->w/segments;
		step2 = step1/duration;

		src.y = 0;
		dst.y = 0;
		src.h = screen->h;
		dst.h = screen->h;
		src.w = step2;
		dst.w = step2;

		for(i = 0; i <= duration; i++)
		{
		    for(j = 0; j <= segments; j++)
		    {
			x1 = step1 * (j - 0.5) - i * step2 + 1;
			x2 = step1 * (j - 0.5) + i * step2 + 1;
			src.x = x1;
			dst.x = x2;
			SDL_BlitSurface((SDL_Surface*)newbkg, &src, screen, &src);
			SDL_BlitSurface((SDL_Surface*)newbkg, &dst, screen, &dst);
			T4K_AddRect(&src, &src);
			T4K_AddRect(&dst, &dst);
		    }
            t4k_present();
            SDL_Delay(10);
        }

        src.x = 0;
		src.y = 0;
		src.w = screen->w;
		src.h = screen->h;
		SDL_BlitSurface((SDL_Surface*)newbkg, NULL, screen, &src);
        t4k_present();

        break;
        }

    case WIPE_BLINDS_HORIZ:
	    {

		step1 = screen->h / segments;
		step2 = step1 / duration;

		src.x = 0;
		dst.x = 0;
		src.w = screen->w;
		dst.w = screen->w;
		src.h = step2;
		dst.h = step2;

		for(i = 0; i <= duration; i++)
		{
		    for(j = 0; j <= segments; j++)
		    {
			y1 = step1 * (j - 0.5) - i * step2 + 1;
			y2 = step1 * (j - 0.5) + i * step2 + 1;
			src.y = y1;
			dst.y = y2;
			SDL_BlitSurface((SDL_Surface*)newbkg, &src, screen, &src);
			SDL_BlitSurface((SDL_Surface*)newbkg, &dst, screen, &dst);
			T4K_AddRect(&src, &src);
			T4K_AddRect(&dst, &dst);
		    }
            t4k_present();
            SDL_Delay(10);
        }

        src.x = 0;
		src.y = 0;
		src.w = screen->w;
		src.h = screen->h;
		SDL_BlitSurface((SDL_Surface*)newbkg, NULL, screen, &src);
        t4k_present();

        break;
        }

    case WIPE_BLINDS_BOX:
	    {

		step1 = screen->w/segments;
		step2 = step1/duration;
		step3 = screen->h/segments;
		step4 = step1/duration;

		for(i = 0; i <= duration; i++)
		{
		    for(j = 0; j <= segments; j++)
		    {
			x1 = step1 * (j - 0.5) - i * step2 + 1;
			x2 = step1 * (j - 0.5) + i * step2 + 1;
			src.x = x1;
			dst.x = x2;
			dst.y = 0;
			dst.w = step2;
			dst.h = screen->h;
			SDL_BlitSurface((SDL_Surface*)newbkg, &src, screen, &src);
			SDL_BlitSurface((SDL_Surface*)newbkg, &dst, screen, &dst);
			T4K_AddRect(&src, &src);
			T4K_AddRect(&dst, &dst);

			y1 = step3 * (j - 0.5) - i * step4 + 1;
			y2 = step3 * (j - 0.5) + i * step4 + 1;
			src.x = 0;
			src.y = y1;
			src.w = screen->w;
			src.h = step4;
			dst.x = 0;
			dst.y = y2;
			dst.w = screen->w;
			dst.h = step4;
			SDL_BlitSurface((SDL_Surface*)newbkg, &src, screen, &src);
			SDL_BlitSurface((SDL_Surface*)newbkg, &dst, screen, &dst);
			T4K_AddRect(&src, &src);
			T4K_AddRect(&dst, &dst);
		    }
            t4k_present();
            SDL_Delay(10);
        }

        src.x = 0;
		src.y = 0;
		src.w = screen->w;
		src.h = screen->h;
		SDL_BlitSurface((SDL_Surface*)newbkg, NULL, screen, &src);
        t4k_present();

        break;
        }
    default:
	    break;
    }
    return 1;
}






/************************************************************************/
/*                                                                      */
/*        Begin blit queue support                                      */
/*                                                                      */
/* This code (modified from Sam Lantinga's "Alien" example program)     */
/* implements a blit queue to perform screen updates in a more          */
/* optimized fashion.                                                   */
/************************************************************************/

//With fullscreen, we need more updates - 180 wasn't enough
#define MAX_UPDATES 512

/* --- Data Structure for Dirty Blitting --- */
static SDL_Rect srcupdate[MAX_UPDATES];
static SDL_Rect dstupdate[MAX_UPDATES];
static int numupdates = 0; // tracks how many blits to be done

struct blit {
    SDL_Surface* src;
    SDL_Rect* srcrect;
    SDL_Rect* dstrect;
    unsigned char type;
} blits[MAX_UPDATES];



/***********************
  T4K_InitBlitQueue()
 ***********************/
void T4K_InitBlitQueue(void)
{
    int i;

    /* --- Set up the update rectangle pointers --- */
    for (i = 0; i < MAX_UPDATES; ++i)
    {
	blits[i].srcrect = &srcupdate[i];
	blits[i].dstrect = &dstupdate[i];
    }
    numupdates = 0;
}


/**************************
  ResetBlitQueue(): just set the number
  of pending updates to zero
 ***************************/
void T4K_ResetBlitQueue(void)
{
    numupdates = 0;
}


/******************************
AddRect : Don't actually blit a surface,
but add a rect to be updated next
update
 *******************************/
int T4K_AddRect(SDL_Rect* src, SDL_Rect* dst)
{

    /*borrowed from SL's alien (and modified)*/
    struct blit* update;

    if(!src)
    {
	fprintf(stderr, "T4K_AddRect() - invalid 'src' arg!\n");
	return 0;
    }

    if(!dst)
    {
	fprintf(stderr, "T4K_AddRect() - invalid 'dst' arg!\n");
	return 0;
    }

    if(numupdates >= MAX_UPDATES)
    {
	fprintf(stderr, "Warning - MAX_UPDATES exceeded, cannot add blit to queue\n");
	return 0;
    }

    update = &blits[numupdates++];

    if(!update || !update->srcrect || !update->dstrect)
    {
	fprintf(stderr, "T4K_AddRect() - 'update' ptr invalid!\n");
	return 0;
    }

    update->srcrect->x = src->x;
    update->srcrect->y = src->y;
    update->srcrect->w = src->w;
    update->srcrect->h = src->h;
    update->dstrect->x = dst->x;
    update->dstrect->y = dst->y;
    update->dstrect->w = dst->w;
    update->dstrect->h = dst->h;
    update->type = 'I';

    return 1;
}



int T4K_DrawSprite(sprite* gfx, int x, int y)
{
    if (!gfx || !gfx->frame[gfx->cur])
    {
	fprintf(stderr, "T4K_DrawSprite() - 'gfx' arg invalid!\n");
	return 0;
    }
    return T4K_DrawObject(gfx->frame[gfx->cur], x, y);
}



/**********************
DrawObject : Draw an object at the specified
location. No respect to clipping!
 *************************/
int T4K_DrawObject(SDL_Surface* surf, int x, int y)
{
    struct blit *update;

    if (!surf)
    {
	fprintf(stderr, "T4K_DrawObject() - invalid 'surf' arg!\n");
	return 0;
    }

    if(numupdates >= MAX_UPDATES)
    {
	fprintf(stderr, "Warning - MAX_UPDATES exceeded, cannot add blit to queue\n");
	return 0;
    }

    update = &blits[numupdates++];

    if(!update || !update->srcrect || !update->dstrect)
    {
	fprintf(stderr, "T4K_DrawObject() - 'update' ptr invalid!\n");
	return 0;
    }

    update->src = surf;
    update->srcrect->x = 0;
    update->srcrect->y = 0;
    update->srcrect->w = surf->w;
    update->srcrect->h = surf->h;
    update->dstrect->x = x;
    update->dstrect->y = y;
    update->dstrect->w = surf->w;
    update->dstrect->h = surf->h;
    update->type = 'D';

    return 1;
}



/************************
UpdateScreen : Update the screen and increment the frame num
 ***************************/
void T4K_UpdateScreen(int* frame)
{
    int i;

    /* -- First erase everything we need to -- */
    for (i = 0; i < numupdates; i++)
    {
	if (blits[i].type == 'E')
	{
	    //       DEBUGCODE(debug_sdl)
	    //       {
	    //         fprintf(stderr, "Erasing blits[%d]\n", i);
	    //         fprintf(stderr, "srcrect->x = %d\t srcrect->y = %d\t srcrect->w = %d\t srcrect->h = %d\n",
	    //               blits[i].srcrect->x, blits[i].srcrect->y, blits[i].srcrect->w, blits[i].srcrect->h);
	    //         fprintf(stderr, "dstrect->x = %d\t dstrect->y = %d\t dstrect->w = %d\t dstrect->h = %d\n",
	    //               blits[i].dstrect->x, blits[i].dstrect->y, blits[i].dstrect->w, blits[i].dstrect->h);
	    //       }

        SDL_BlitSurfaceUnchecked(blits[i].src, blits[i].srcrect, screen,
                                 blits[i].dstrect);
    }
    }

    //  SNOW_erase();

    /* -- then draw -- */
    for (i = 0; i < numupdates; i++)
    {
	if (blits[i].type == 'D')
	{
	    //       DEBUGCODE(debug_sdl)
	    //       {
	    //         fprintf(stderr, "drawing blits[%d]\n", i);
	    //         fprintf(stderr, "srcrect->x = %d\t srcrect->y = %d\t srcrect->w = %d\t srcrect->h = %d\n",
	    //               blits[i].srcrect->x, blits[i].srcrect->y, blits[i].srcrect->w, blits[i].srcrect->h);
	    //         fprintf(stderr, "dstrect->x = %d\t dstrect->y = %d\t dstrect->w = %d\t dstrect->h = %d\n",
	    //               blits[i].dstrect->x, blits[i].dstrect->y, blits[i].dstrect->w, blits[i].dstrect->h);
	    //       }

	    SDL_BlitSurface(blits[i].src, blits[i].srcrect, screen, blits[i].dstrect);
	}
    }

    //  SNOW_draw();

    /* -- update the screen only where we need to! -- */
    //  if (SNOW_on)
    //    SDL_UpdateRects(screen, SNOW_add( (SDL_Rect*)&dstupdate, numupdates ), SNOW_rects);
    //  else
    /* Backing-surface model: ignore the per-rect optimization and just
     * scale-blit the whole backing on every update. */
    (void)dstupdate;
    (void)numupdates;
    t4k_present();

    numupdates = 0;
    *frame = *frame + 1;
}


/* basically puts in an order to overdraw sprite with corresponding */
/* rect of bkgd img                                                 */
int T4K_EraseSprite(sprite* img, SDL_Surface* curr_bkgd, int x, int y)
{
    if( !img
	    || img->cur < 0
	    || img->cur > MAX_SPRITE_FRAMES
	    || !img->frame[img->cur])
    {
	fprintf(stderr, "T4K_EraseSprite() - invalid 'img' arg!\n");
	return 0;
    }
    return T4K_EraseObject(img->frame[img->cur], curr_bkgd, x, y);
}



/*************************
EraseObject : Erase an object from the screen
 **************************/
int T4K_EraseObject(SDL_Surface* surf, SDL_Surface* curr_bkgd, int x, int y)
{
    struct blit* update = NULL;

    if(!surf)
    {
	fprintf(stderr, "T4K_EraseObject() - invalid 'surf' arg!\n");
	return 0;
    }

    if(numupdates >= MAX_UPDATES)
    {
	fprintf(stderr, "Warning - MAX_UPDATES exceeded, cannot add blit to queue\n");
	return 0;
    }

    update = &blits[numupdates++];

    if(!update || !update->srcrect || !update->dstrect)
    {
	fprintf(stderr, "T4K_EraseObject() - 'update' ptr invalid!\n");
	return 0;
    }

    update->src = curr_bkgd;

    /* take dimentsions from src surface: */
    update->srcrect->x = x;
    update->srcrect->y = y;
    update->srcrect->w = surf->w;
    update->srcrect->h = surf->h;

    /* NOTE this is needed because the letters may go beyond the size of */
    /* the fish, and we only erase the fish image before we redraw the   */
    /* fish followed by the letter - DSB                                 */
    /* add margin of a few pixels on each side: */
    update->srcrect->x -= ERASE_MARGIN;
    update->srcrect->y -= ERASE_MARGIN;
    update->srcrect->w += (ERASE_MARGIN * 2);
    update->srcrect->h += (ERASE_MARGIN * 2);


    /* Adjust srcrect so it doesn't go past bkgd: */
    if (update->srcrect->x < 0)
    {
	update->srcrect->w += update->srcrect->x; //so right edge stays correct
	update->srcrect->x = 0;
    }
    if (update->srcrect->y < 0)
    {
	update->srcrect->h += update->srcrect->y; //so bottom edge stays correct
	update->srcrect->y = 0;
    }

    if (update->srcrect->x + update->srcrect->w > curr_bkgd->w)
	update->srcrect->w = curr_bkgd->w - update->srcrect->x;
    if (update->srcrect->y + update->srcrect->h > curr_bkgd->h)
	update->srcrect->h = curr_bkgd->h - update->srcrect->y;


    update->dstrect->x = update->srcrect->x;
    update->dstrect->y = update->srcrect->y;
    update->dstrect->w = update->srcrect->w;
    update->dstrect->h = update->srcrect->h;
    update->type = 'E';

    return 1;
}

//#if 0

/************************************************************************/
/*                                                                      */
/*        Begin text drawing functions                                  */
/*                                                                      */
/* These functions support text drawing using either SDL_Pango          */
/* or SDL_ttf. SDL_Pango is preferable but is not available on all      */
/* platforms. Code outside of this file does not have to worry about    */
/* which library is used to do the actual rendering.                    */
/************************************************************************/

#define MAX_FONT_SIZE 40
#define DEFAULT_FONT_SIZE 10

//NOTE to test program with SDL_ttf, do "./configure --without-sdlpango"

/*-- file-scope variables and local file prototypes for SDL3_ttf-based code: */
#include <SDL3_ttf/SDL_ttf.h>
/* We cache fonts here once loaded to improve performance: */
TTF_Font* font_list[MAX_FONT_SIZE + 1] = {NULL};
static void free_font_list(void);
static TTF_Font* get_font(int size);
static TTF_Font* load_font(const char* font_name, int font_size);

/* "Public" functions called from other files that use either */
/*SDL_Pango or SDL_ttf:                                       */

/* Initialize SDL3_ttf: */
int T4K_Setup_SDL_Text(void)
{
    DEBUGMSG(debug_sdl, "T4K_Setup_SDL_Text() - using SDL3_ttf\n");

    if (!TTF_Init())
    {
        fprintf(stderr, "\nError: I could not initialize SDL3_ttf\n");
        return 0;
    }
    return 1;
}



void T4K_Cleanup_SDL_Text(void)
{
    free_font_list();
    TTF_Quit();
}

/* T4K_BlackOutline() creates a surface containing text of the designated */
/* foreground color, surrounded by a black shadow, on a transparent    */
/* background.  The appearance can be tuned by adjusting the number of */
/* background copies and the offset where the foreground text is       */
/* finally written (see below).                                        */
SDL_Surface* T4K_BlackOutline(const char* t, int size, const SDL_Color* c)
{
    SDL_Surface* out = NULL;
    SDL_Surface* black_letters = NULL;
    SDL_Surface* white_letters = NULL;
    SDL_Surface* bg = NULL;
    SDL_Rect dstrect;
    Uint32 color_key;

    /* Make sure everything is sane before we proceed: */
    TTF_Font* font = get_font(size);
    if (!font)
    {
	fprintf(stderr, "T4K_BlackOutline(): could not load needed font - returning.\n");
	return NULL;
    }

    if (!t || !c)
    {
	fprintf(stderr, "T4K_BlackOutline(): invalid ptr parameter, returning.\n");
	return NULL;
    }

    if (t[0] == '\0')
    {
	fprintf(stderr, "T4K_BlackOutline(): empty string, returning\n");
	return NULL;
    }

    DEBUGMSG(debug_sdl, "Entering T4K_BlackOutline():\n");
    DEBUGMSG(debug_sdl, "BlackOutline of \"%s\"\n", t );

    /* If caller already inserted newlines (e.g. T4K_LineWrapInsBreaks for
     * the menu description panel), use the wrapping variant so they're
     * honored. wrapLength=0 means "wrap at newlines only". */
    bool multiline = (strchr(t, '\n') != NULL);
    if (multiline)
    {
        black_letters = TTF_RenderText_Blended_Wrapped(font, t, 0, black, 0);
    }
    else
    {
        black_letters = TTF_RenderText_Blended(font, t, 0, black);
    }

    if (!black_letters)
    {
	fprintf (stderr, "Warning - T4K_BlackOutline() could not create image for %s\n", t);
	return NULL;
    }

    bg = SDL_CreateSurface((black_letters->w) + 5, (black_letters->h) + 5,
                           SDL_PIXELFORMAT_RGBA8888);
    /* Use color key for eventual transparency: */
    color_key =
        SDL_MapRGB(SDL_GetPixelFormatDetails(bg->format), NULL, 30, 30, 30);
    SDL_FillSurfaceRect(bg, NULL, color_key);

    /* Now draw black outline/shadow 2 pixels on each side: */
    dstrect.w = black_letters->w;
    dstrect.h = black_letters->h;

    /* NOTE: can make the "shadow" more or less pronounced by */
    /* changing the parameters of these loops.                */
    for (dstrect.x = 1; dstrect.x < 5; dstrect.x++)
	for (dstrect.y = 1; dstrect.y < 5; dstrect.y++)
	    SDL_BlitSurface(black_letters , NULL, bg, &dstrect );

    SDL_DestroySurface(black_letters);

    /* --- Put the color version of the text on top! --- */
    if (multiline)
    {
        white_letters = TTF_RenderText_Blended_Wrapped(font, t, 0, *c, 0);
    }
    else
    {
        white_letters = TTF_RenderText_Blended(font, t, 0, *c);
    }

    if (!white_letters)
    {
	fprintf (stderr, "Warning - T4K_BlackOutline() could not create image for %s\n", t);
	return NULL;
    }

    dstrect.x = 1;
    dstrect.y = 1;
    SDL_BlitSurface(white_letters, NULL, bg, &dstrect);
    SDL_DestroySurface(white_letters);

    /* --- Convert to the screen format for quicker blits --- */
    SDL_SetSurfaceColorKey(bg, true, color_key);
    out = SDL_DuplicateSurface(bg);
    SDL_DestroySurface(bg);

    DEBUGMSG(debug_sdl, "\nLeaving T4K_BlackOutline(): \n");

    return out;
}


/* This (fast) function just returns a non-outlined surf */
/* using either SDL_Pango or SDL_ttf                     */
SDL_Surface* T4K_SimpleText(const char *t, int size, const SDL_Color* col)
{
    SDL_Surface* surf = NULL;

    if (!t)
	return NULL;
    if (!col)
	col = &black;

    {
	TTF_Font* font = get_font(size);
	if (!font)
	    return NULL;
    surf = TTF_RenderText_Blended(font, t, 0, *col);
    }

    return surf;
}

/* Here we calculate an estimate of the string length that will
 * fit within a box 'pixel_width' pixels wide.  The letter 'x'
 * was chosen for this calculation based on some googling that
 * suggested it gave a reasonable estimate - DSB.
 */
int T4K_CharsForWidth(int fontsize, int pixel_width)
{
    char buf[256];
    int i = 0;
    int done = 0;
    SDL_Surface* s;
    for(i = 0; i < 255 && !done; i++)
    {
	buf[i] = 'x';
	buf[i + 1] = '\0';
	s = T4K_SimpleText(buf, fontsize, &white);
	if(s && s->w > pixel_width)  //means string of (i++) 'x' exceeds width
	    done = 1;
    SDL_DestroySurface(s);
    }
    return  i;
}

int size_text(const char* text, int font_size, int* width, int* height)
{
    return TTF_GetStringSize(get_font(font_size), text, 0, width, height) ? 0
                                                                          : -1;
}
/* This (fast) function just returns a non-outlined surf */
/* using SDL_Pango if available, SDL_ttf as fallback     */
SDL_Surface* T4K_SimpleTextWithOffset(const char *t, int size, const SDL_Color* col, int *glyph_offset)
{
    SDL_Surface* surf = NULL;

    if (!t||!col)
	return NULL;

    {
	TTF_Font* font = get_font(size);
	if (!font)
	    return NULL;
    surf = TTF_RenderText_Blended(font, t, 0, *col);
    {
        int h;
        int hmax = 0;
	    int len = strlen(t);
	    int i;
	    for (i = 0; i < len; i++)
	    {
            TTF_GetGlyphMetrics(font, t[i], NULL, NULL, NULL, &h, NULL);
            if (h > hmax)
            {
                hmax = h;
            }
        }
        *glyph_offset = hmax - TTF_GetFontAscent(font);
    }
    }

    return surf;
}



/*-----------------------------------------------------------*/
/* Local functions, callable only within SDL_extras, divided */
/* according with which text lib we are using:               */
/*-----------------------------------------------------------*/

/* Local functions used by SDL3_ttf code above: */

static void free_font_list(void)
{
    int i;
    for(i = 0; i < MAX_FONT_SIZE; i++)
    {
	if(font_list[i])
	{
	    TTF_CloseFont(font_list[i]);
	    font_list[i] = NULL;
	}
    }
}

/* FIXME - could combine this with load_font() below:         */
/* Loads and caches fonts in each size as they are requested: */
/* We use the font size as an array index, keeping each size  */
/* font in memory once loaded until cleanup.                  */
static TTF_Font* get_font(int size)
{
    static char prev_font_name[FONT_NAME_LENGTH];
    if (size < 0)
    {
	fprintf(stderr, "Error - requested font size %d is negative\n", size);
	return NULL;
    }

    if (size > MAX_FONT_SIZE)
    {
	fprintf(stderr, "Error - requested font size %d exceeds max = %d, resetting.\n",
		size, MAX_FONT_SIZE);
	size = MAX_FONT_SIZE;
    }

    /* If the font has changed, we need to wipe out the old ones: */
    if (0 != strncmp(prev_font_name, T4K_AskFontName(), sizeof(prev_font_name)))
    {
	free_font_list();
	strncpy(prev_font_name, T4K_AskFontName(), sizeof(prev_font_name));
    }

    if(font_list[size] == NULL)
    {
        /* Load the default (Latin) font as primary, then chain the theme
	 * font as a fallback so glyphs missing from the default — e.g.
	 * Devanagari, Bengali — get rendered from the theme font. UI
	 * chrome (English menu labels) stays in Andika; theme content
	 * (Hindi wordlist titles, Hindi falling fish) renders via the
	 * theme font automatically. */
        font_list[size]        = load_font(DEFAULT_FONT_NAME, size);
        const char* theme_font = T4K_AskFontName();
        if (font_list[size] && theme_font &&
            strcmp(theme_font, DEFAULT_FONT_NAME) != 0)
        {
            TTF_Font* fb = load_font(theme_font, size);
            if (fb)
            {
                TTF_AddFallbackFont(font_list[size], fb);
            }
        }
    }
    return font_list[size];
}

/* FIXME: I think we need to provide a single default font with the program data, */
/* then more flexible code to try to locate or load system fonts. DSB             */
/* Returns ptr to loaded font if successful, NULL otherwise. */
static TTF_Font* load_font(const char* font_name, int font_size)
{
    TTF_Font* f = NULL;
    char      rel[T4K_PATH_MAX];
    char      fontfile[T4K_PATH_MAX];

    /* Search the registered data prefix list (T4K_AddDataPrefix) for fonts/<name>.
     * The host app (tuxtype/tuxmath) registers its data dir there, which is
     * where the bundled font lives. */
    snprintf(rel, T4K_PATH_MAX, "fonts/%s", font_name);
    const char* found = find_file(rel);
    if (found && found[0])
    {
        strncpy(fontfile, found, T4K_PATH_MAX - 1);
        fontfile[T4K_PATH_MAX - 1] = '\0';
        f                          = TTF_OpenFont(fontfile, font_size);
    }

    /* Fallbacks: t4k_common's own data dir, then a typical Debian path. */
    if (!f)
    {
        snprintf(fontfile, T4K_PATH_MAX, "%s/fonts/%s", COMMON_DATA_PREFIX,
                 font_name);
        f = TTF_OpenFont(fontfile, font_size);
    }
    if (!f)
    {
        snprintf(fontfile, T4K_PATH_MAX,
                 "/usr/share/fonts/truetype/ttf-sil-andika/AndikaDesRevG.ttf");
        f = TTF_OpenFont(fontfile, font_size);
    }

    if (f)
    {
	DEBUGMSG(debug_sdl, "LoadFont(): %s loaded successfully\n\n", fontfile);
	return f;
    }
    else
    {
	fprintf(stderr, "LoadFont(): %s NOT loaded successfully.\n", fontfile);
	return NULL;
    }
}
