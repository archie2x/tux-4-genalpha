/* Menu-driven SFX/Music volume settings screen. Reachable from the
 * Options menu (RUN_SET_VOLUME). Hosts the T4K_VolumeWidget centered on
 * a darkened background with explanatory labels; SPACE/ESC returns to
 * the caller. No 'Q', no keyboard-guide toggle — those belong to in-game
 * Pause(). */

#include "globals.h"
#include "funcs.h"

#include <t4k/volume.h>

static const int label_font_size = 24;

static void draw_chrome(void)
{
    SDL_Surface* t;
    SDL_Rect     s;

    if (settings.sys_sound)
    {
        t = BlackOutline(_("Sound Effects Volume"), label_font_size, &white);
        if (t)
        {
            s.y = screen->h / 2 - 80;
            s.x = screen->w / 2 - t->w / 2;
            SDL_BlitSurface(t, NULL, screen, &s);
            SDL_DestroySurface(t);
        }

        t = BlackOutline(_("Music Volume"), label_font_size, &white);
        if (t)
        {
            s.y = screen->h / 2 + 20;
            s.x = screen->w / 2 - t->w / 2;
            SDL_BlitSurface(t, NULL, screen, &s);
            SDL_DestroySurface(t);
        }
    }
    else
    {
        t = BlackOutline(_("Sound & Music Disabled"), label_font_size, &white);
        if (t)
        {
            s.y = screen->h / 2 - 80;
            s.x = screen->w / 2 - t->w / 2;
            SDL_BlitSurface(t, NULL, screen, &s);
            SDL_DestroySurface(t);
        }
    }

    t = BlackOutline(_("'SPACE' or 'ESC' to return."), label_font_size, &white);
    if (t)
    {
        s.y = screen->h / 2 + 180;
        s.x = screen->w / 2 - t->w / 2;
        SDL_BlitSurface(t, NULL, screen, &s);
        SDL_DestroySurface(t);
    }
}

void SettingsVolume(void)
{
    T4K_VolumeWidget* widget = NULL;
    SDL_Event         event;

    LOG("Entering SettingsVolume()\n");

    SDL_ShowCursor();
    DarkenScreen(2);

    if (settings.sys_sound)
    {
        widget = T4K_Volume_Create(settings.sfx_volume, settings.mus_volume);
        T4K_Volume_SetLayout(widget,
                             screen->w / 2,      // cx
                             screen->h / 2 - 40, // sfx_y
                             screen->h / 2 + 60  // mus_y
        );
    }

    draw_chrome();
    if (widget)
    {
        T4K_Volume_Draw(widget, screen);
    }
    T4K_UpdateRect(screen, NULL);

    for (bool running = true; running;)
    {
        bool dirty = false;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                exit(0);
            }
            if (event.type == SDL_EVENT_KEY_DOWN &&
                (event.key.key == SDLK_SPACE || event.key.key == SDLK_ESCAPE))
            {
                running = false;
            }
            if (widget && T4K_Volume_HandleEvent(widget, &event))
            {
                dirty = true;
            }
        }

        if (widget && T4K_Volume_Tick(widget))
        {
            dirty = true;
        }

        if (dirty)
        {
            T4K_Volume_Draw(widget, screen);
            settings.sfx_volume = T4K_Volume_Sfx(widget);
            settings.mus_volume = T4K_Volume_Mus(widget);
            T4K_UpdateRect(screen, NULL);
        }

        SDL_Delay(33);
    }

    if (widget)
    {
        T4K_Volume_Destroy(widget);
    }
    SDL_HideCursor();

    LOG("Leaving SettingsVolume()\n");
}
