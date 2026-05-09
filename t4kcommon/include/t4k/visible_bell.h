/* Visible-bell overlay — brief whole-screen white flash that fades.
 * Surface-based: blits a fading white overlay onto a target SDL_Surface
 * just before the frame is presented. Time-based fade (alpha decays as
 * end_time approaches), so it stays smooth regardless of frame rate.
 *
 * Usage:
 *   static T4K_VisibleBell wrong_bell;
 *   T4K_VisibleBell_Init(&wrong_bell, 200, 110);  // 200ms, peak alpha 110
 *   ... in event handler:
 *   T4K_VisibleBell_Trigger(&wrong_bell);
 *   ... at end of each frame:
 *   T4K_VisibleBell_Render(&wrong_bell, screen);
 *   ... at shutdown:
 *   T4K_VisibleBell_Free(&wrong_bell); */

#pragma once

#include <stdbool.h>
#include <SDL3/SDL.h>

typedef struct T4K_VisibleBell
{
    Uint64       end_time;
    Uint32       duration_ms;
    Uint8        peak_alpha;
    SDL_Surface* overlay; /* lazy-created on first Render, sized to target */
} T4K_VisibleBell;

/* duration_ms=0 → default 200ms. peak_alpha=0 → default 110. */
void T4K_VisibleBell_Init(T4K_VisibleBell* bell, Uint32 duration_ms,
                          Uint8 peak_alpha);
void T4K_VisibleBell_Free(T4K_VisibleBell* bell);

void T4K_VisibleBell_Trigger(T4K_VisibleBell* bell);
bool T4K_VisibleBell_Active(const T4K_VisibleBell* bell);

/* Renders the overlay onto target if currently active. Lazy-creates the
 * cached overlay surface to match target size; recreates if target was
 * resized (fullscreen toggle). No-op when inactive. */
void T4K_VisibleBell_Render(T4K_VisibleBell* bell, SDL_Surface* target);
