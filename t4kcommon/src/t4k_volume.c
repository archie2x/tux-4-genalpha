/* SFX/Music volume slider widget. Extracted from tuxtype's pause overlay
 * so tuxmath (and any other consumer) can reuse the same UI. The widget
 * handles its own mouse + arrow-key input, drives mixer gain via
 * T4K_SetSfxVolume / T4K_SetMusicVolume, and plays an optional tick
 * Mix_Chunk as feedback while the user drags. */

#include <t4k/common.h>
#include <t4k/volume.h>

#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <stdlib.h>

#include "t4k_volume_assets.h" /* embedded SVG + WAV byte arrays */

/* Same forward decl t4k_loaders.c uses — internal accessor for the
 * single mixer instance. */
extern MIX_Mixer* t4k_audio_get_mixer(void);

#define MIX_MAX_VOLUME 128
#define BAR_SEGMENTS 32
#define BAR_SEGMENT_W 5
#define BAR_SEGMENT_PITCH 7
#define BAR_SEGMENT_H 40
#define VOLUME_STEP 4
#define HALF_BAR_PIXELS (7 * 16)
#define TICK_THROTTLE 4
#define DEFAULT_ARROW_SIZE 32

struct T4K_VolumeWidget
{
    SDL_Surface* up;
    SDL_Surface* down;
    SDL_Surface* left;
    SDL_Surface* right;
    Mix_Chunk*   tick;
    SDL_Rect     rect_up;
    SDL_Rect     rect_down;
    SDL_Rect     rect_left;
    SDL_Rect     rect_right;
    int          sfx;
    int          mus;
    int          mouse_pressed;
    int          last_mx;
    int          last_my;
    int          tocks;
};

static SDL_Surface* load_arrow(const unsigned char* svg, size_t len)
{
    SDL_IOStream* io = SDL_IOFromConstMem(svg, len);
    if (!io)
    {
        return NULL;
    }
    /* IMG_LoadSizedSVG_IO closes the stream regardless of success. */
    return IMG_LoadSizedSVG_IO(io, DEFAULT_ARROW_SIZE, DEFAULT_ARROW_SIZE);
}

static int clamp_volume(int v)
{
    if (v < 0)
    {
        return 0;
    }
    if (v > MIX_MAX_VOLUME)
    {
        return MIX_MAX_VOLUME;
    }
    return v;
}

static void apply_sfx(T4K_VolumeWidget* w, int new_sfx)
{
    new_sfx = clamp_volume(new_sfx);
    if (new_sfx == w->sfx)
    {
        return;
    }
    w->sfx = new_sfx;
    T4K_SetSfxVolume(new_sfx);
    if (w->tick && w->tocks % TICK_THROTTLE == 0)
    {
        T4K_PlaySound(w->tick);
    }
    w->tocks++;
}

static void apply_mus(T4K_VolumeWidget* w, int new_mus)
{
    new_mus = clamp_volume(new_mus);
    if (new_mus == w->mus)
    {
        return;
    }
    w->mus = new_mus;
    T4K_SetMusicVolume(new_mus);
}

static Mix_Chunk* load_tick(void)
{
    MIX_Mixer* mixer = t4k_audio_get_mixer();
    if (!mixer)
    {
        return NULL;
    }
    SDL_IOStream* io = SDL_IOFromConstMem(t4k_tock_wav, sizeof t4k_tock_wav);
    if (!io)
    {
        return NULL;
    }
    /* closeio=true: MIX_LoadAudio_IO consumes the stream. */
    return MIX_LoadAudio_IO(mixer, io, true, true);
}

T4K_VolumeWidget* T4K_Volume_Create(int sfx_volume, int mus_volume)
{
    T4K_VolumeWidget* w = SDL_calloc(1, sizeof(*w));
    if (!w)
    {
        return NULL;
    }

    w->up    = load_arrow(t4k_up_svg, sizeof t4k_up_svg);
    w->down  = load_arrow(t4k_down_svg, sizeof t4k_down_svg);
    w->left  = load_arrow(t4k_left_svg, sizeof t4k_left_svg);
    w->right = load_arrow(t4k_right_svg, sizeof t4k_right_svg);
    if (!w->up || !w->down || !w->left || !w->right)
    {
        T4K_Volume_Destroy(w);
        return NULL;
    }

    w->tick = load_tick(); /* NULL is fine; apply_sfx no-ops gracefully */
    w->sfx  = clamp_volume(sfx_volume);
    w->mus  = clamp_volume(mus_volume);

    w->rect_up.w    = w->up->w;
    w->rect_up.h    = w->up->h;
    w->rect_down.w  = w->down->w;
    w->rect_down.h  = w->down->h;
    w->rect_left.w  = w->left->w;
    w->rect_left.h  = w->left->h;
    w->rect_right.w = w->right->w;
    w->rect_right.h = w->right->h;

    return w;
}

void T4K_Volume_Destroy(T4K_VolumeWidget* w)
{
    if (!w)
    {
        return;
    }
    SDL_DestroySurface(w->up);
    SDL_DestroySurface(w->down);
    SDL_DestroySurface(w->left);
    SDL_DestroySurface(w->right);
    if (w->tick)
    {
        MIX_DestroyAudio(w->tick);
    }
    SDL_free(w);
}

void T4K_Volume_SetLayout(T4K_VolumeWidget* w, int cx, int sfx_y, int mus_y)
{
    if (!w)
    {
        return;
    }

    w->rect_left.y  = sfx_y;
    w->rect_right.y = sfx_y;
    w->rect_down.y  = mus_y;
    w->rect_up.y    = mus_y;

    w->rect_left.x  = cx - HALF_BAR_PIXELS - w->rect_left.w - 4;
    w->rect_down.x  = cx - HALF_BAR_PIXELS - w->rect_down.w - 4;
    w->rect_right.x = cx + HALF_BAR_PIXELS + 4;
    w->rect_up.x    = cx + HALF_BAR_PIXELS + 4;
}

static void draw_bar(SDL_Surface* target, int x0, int y, int volume)
{
    SDL_Rect cell = {0, y, BAR_SEGMENT_W, BAR_SEGMENT_H};
    Uint32   on = SDL_MapRGB(SDL_GetPixelFormatDetails(target->format), NULL, 0,
                             0, 127 + volume);
    Uint32   off =
        SDL_MapRGB(SDL_GetPixelFormatDetails(target->format), NULL, 0, 0, 0);
    cell.x = x0;
    for (int i = 1; i <= BAR_SEGMENTS; i++)
    {
        SDL_FillSurfaceRect(target, &cell,
                            volume >= i * VOLUME_STEP ? on : off);
        cell.x += BAR_SEGMENT_PITCH;
    }
}

void T4K_Volume_Draw(T4K_VolumeWidget* w, SDL_Surface* target)
{
    if (!w || !target)
    {
        return;
    }

    SDL_BlitSurface(w->left, NULL, target, &w->rect_left);
    SDL_BlitSurface(w->right, NULL, target, &w->rect_right);
    SDL_BlitSurface(w->down, NULL, target, &w->rect_down);
    SDL_BlitSurface(w->up, NULL, target, &w->rect_up);

    draw_bar(target, w->rect_left.x + w->rect_left.w + BAR_SEGMENT_W,
             w->rect_left.y, w->sfx);
    draw_bar(target, w->rect_down.x + w->rect_down.w + BAR_SEGMENT_W,
             w->rect_down.y, w->mus);
}

bool T4K_Volume_HandleEvent(T4K_VolumeWidget* w, const SDL_Event* event)
{
    if (!w || !event)
    {
        return false;
    }

    int old_sfx = w->sfx;
    int old_mus = w->mus;

    switch (event->type)
    {
    case SDL_EVENT_KEY_UP:
        if (event->key.key == SDLK_RIGHT || event->key.key == SDLK_LEFT)
        {
            w->tocks = 0;
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_RIGHT)
        {
            apply_sfx(w, w->sfx + VOLUME_STEP);
        }
        else if (event->key.key == SDLK_LEFT)
        {
            apply_sfx(w, w->sfx - VOLUME_STEP);
        }
        else if (event->key.key == SDLK_UP)
        {
            apply_mus(w, w->mus + VOLUME_STEP);
        }
        else if (event->key.key == SDLK_DOWN)
        {
            apply_mus(w, w->mus - VOLUME_STEP);
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        w->last_mx = (int)event->motion.x;
        w->last_my = (int)event->motion.y;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        w->mouse_pressed = 1;
        w->tocks         = 0;
        w->last_mx       = (int)event->button.x;
        w->last_my       = (int)event->button.y;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        w->mouse_pressed = 0;
        break;
    default:
        break;
    }

    return w->sfx != old_sfx || w->mus != old_mus;
}

bool T4K_Volume_Tick(T4K_VolumeWidget* w)
{
    if (!w || !w->mouse_pressed)
    {
        return false;
    }

    int x       = w->last_mx;
    int y       = w->last_my;
    int old_sfx = w->sfx;
    int old_mus = w->mus;

    if (T4K_inRect(w->rect_up, x, y))
    {
        apply_mus(w, w->mus + VOLUME_STEP);
    }
    else if (T4K_inRect(w->rect_down, x, y))
    {
        apply_mus(w, w->mus - VOLUME_STEP);
    }
    else if (T4K_inRect(w->rect_right, x, y))
    {
        apply_sfx(w, w->sfx + VOLUME_STEP);
    }
    else if (T4K_inRect(w->rect_left, x, y))
    {
        apply_sfx(w, w->sfx - VOLUME_STEP);
    }
    else
    {
        /* Click-on-bar: jump volume to where the user clicked. SFX bar
         * sits between rect_left's right edge and rect_right's left edge
         * (same x-span for music, just at rect_down/up's y). */
        int bar_x0 = w->rect_left.x + w->rect_left.w + 1;
        int bar_x1 = w->rect_right.x;
        if (x > bar_x0 && x < bar_x1 && bar_x1 > bar_x0 + 1)
        {
            int v =
                VOLUME_STEP +
                (int)(128.0 * ((x - bar_x0 - 1.0) / (bar_x1 - bar_x0 - 2.0)));
            if (y >= w->rect_left.y && y <= w->rect_left.y + w->rect_left.h)
            {
                apply_sfx(w, v);
            }
            else if (y >= w->rect_down.y &&
                     y <= w->rect_down.y + w->rect_down.h)
            {
                apply_mus(w, v);
            }
        }
    }

    return w->sfx != old_sfx || w->mus != old_mus;
}

int T4K_Volume_Sfx(const T4K_VolumeWidget* w)
{
    return w ? w->sfx : 0;
}

int T4K_Volume_Mus(const T4K_VolumeWidget* w)
{
    return w ? w->mus : 0;
}
