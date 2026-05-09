/* Reusable SFX/Music volume slider widget. Two horizontal bars: SFX
 * (left/right arrow buttons) and Music (up/down). Arrow art and tick
 * sound ship embedded in the library. Mixer gain is updated immediately
 * via T4K_SetSfxVolume / T4K_SetMusicVolume. */

#pragma once
#include <stdbool.h>
#include <SDL3/SDL.h>

#include <t4k/common.h>

typedef struct T4K_VolumeWidget T4K_VolumeWidget;

/* Volumes use the SDL2_mixer 0..128 convention. */
T4K_VolumeWidget* T4K_Volume_Create(int sfx_volume, int mus_volume);
void              T4K_Volume_Destroy(T4K_VolumeWidget* w);

/* Bars centered on cx; sfx-row buttons at sfx_y, mus-row buttons at mus_y.
 * Bars span 7*16 px on each side of cx. */
void T4K_Volume_SetLayout(T4K_VolumeWidget* w, int cx, int sfx_y, int mus_y);

void T4K_Volume_Draw(T4K_VolumeWidget* w, SDL_Surface* target);

/* Returns true if the event mutated volumes (mixer gain already updated). */
bool T4K_Volume_HandleEvent(T4K_VolumeWidget* w, const SDL_Event* event);

/* Per-frame nudge — drives held-button continuous adjust. Returns true on
 * change. Call once per frame after polling all events. */
bool T4K_Volume_Tick(T4K_VolumeWidget* w);

int T4K_Volume_Sfx(const T4K_VolumeWidget* w);
int T4K_Volume_Mus(const T4K_VolumeWidget* w);
