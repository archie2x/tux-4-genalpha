/* sdl3_compat.c — small SDL1/SDL_gfx → SDL3 shims for the port.
 *
 * Provides minimal stand-ins for legacy APIs that don't exist in SDL3.
 * These are best-effort: rotozoomSurface returns a non-rotated copy for
 * now (Factoroids' asteroid spin will look static — TODO: render via
 * SDL_Renderer with angle for a proper fix). */

#include <SDL3/SDL.h>

/* Returns a fresh duplicate of src — angle/zoom ignored.
 * Real rotation would need SDL_Renderer or pre-rendered frames. */
SDL_Surface* rotozoomSurface(SDL_Surface* src, double angle, double zoom, int smooth)
{
    (void)angle; (void)zoom; (void)smooth;
    if (!src) return NULL;
    return SDL_DuplicateSurface(src);
}
