/*
   pause.c:


   Supplies pause screen feature.

   Copyright 2003, 2010.
   Authors: Jesse Andrews, David Bruce.

   Project email: <tux4kids-tuxtype-dev@lists.alioth.debian.org>
   Project website: http://tux4kids.alioth.debian.org

   pause.c is part of Tux Typing, a.k.a "tuxtype".

Tux Typing is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

Tux Typing is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "globals.h"
#include "funcs.h"

#include <t4k/volume.h>

static T4K_VolumeWidget* pause_volume = NULL;
static SDL_Rect     rectKbdToggle; /* fixed area used for the toggle label */
static SDL_Surface* pause_kbd_bkg    = NULL; /* snapshot pixels under rect */
const int           pause_font_size1 = 24;
const int           pause_font_size2 = 36;

/* Local function prototypes: */
static void pause_draw(void);
static void draw_kbd_toggle(void);
static void pause_load_media(void);
static void pause_unload_media(void);

// QUESTION: For usability sake, should escape return to the game
//           and the user have to choose to quit the game, or ???
/**********************
Pause : Pause the game
***********************/
/* in_game = 1 when called from gameplay (text mentions returning to game,
 *               return value 1 = quit-to-menu).
 * in_game = 0 when called from a menu (Volume Settings) — text mentions
 *               returning to menu, return value is ignored. */
static int g_pause_in_game = 1;
int        Pause(int in_game)
{
    g_pause_in_game = in_game;
    int       quit  = 0;
    SDL_Event event;

    LOG("Entering Pause()\n");

    pause_load_media();

    SDL_ShowCursor();

    /* Darken the screen — bits=2 (quarter brightness) keeps the volume
     * sliders and labels readable; bits=1 (half) was too pale. */
    DarkenScreen(2);

    pause_draw();

    if (pause_volume)
    {
        T4K_Volume_Draw(pause_volume, screen);
    }

    T4K_UpdateRect(screen, NULL);

    int paused = 1;
    while (paused)
    {
        bool dirty = false;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                exit(0);
            }

            /* SPACE / ESC return to caller (game or options menu). 'Q'
             * quits to menu, but only from a game — from options it's a
             * no-op so the user can't drop their place in the menu. */
            if (event.type == SDL_EVENT_KEY_DOWN)
            {
                if (event.key.key == SDLK_SPACE || event.key.key == SDLK_ESCAPE)
                {
                    paused = 0;
                }
                else if (event.key.key == SDLK_Q && in_game)
                {
                    paused = 0;
                    quit   = 1;
                }
                else if (event.key.key == SDLK_K && in_game)
                {
                    settings.show_keyboard = !settings.show_keyboard;
                    draw_kbd_toggle();
                    T4K_UpdateRect(screen, &rectKbdToggle);
                }
            }

            /* event coords come pre-scaled to 640x480 backing-surface units
             * via t4kcommon's event filter; SDL_GetMouseState would return
             * window units instead. Intercept clicks on the keyboard
             * toggle before letting the volume widget see the event. */
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && in_game &&
                rectKbdToggle.w > 0 &&
                inRect(rectKbdToggle, (int)event.button.x, (int)event.button.y))
            {
                settings.show_keyboard = !settings.show_keyboard;
                draw_kbd_toggle();
                T4K_UpdateRect(screen, &rectKbdToggle);
                continue;
            }

            if (pause_volume && T4K_Volume_HandleEvent(pause_volume, &event))
            {
                dirty = true;
            }
        }

        if (pause_volume && T4K_Volume_Tick(pause_volume))
        {
            dirty = true;
        }

        if (dirty)
        {
            T4K_Volume_Draw(pause_volume, screen);
            settings.sfx_volume = T4K_Volume_Sfx(pause_volume);
            settings.mus_volume = T4K_Volume_Mus(pause_volume);
            T4K_UpdateRect(screen, NULL);
        }

        SDL_Delay(33);
    }

    /* --- Return to previous state --- */

    /* SDL_EnableKeyRepeat removed in SDL3 (no equivalent needed). */

    SDL_HideCursor();

    /* Audio resume stubbed (task #13). */

    pause_unload_media();

    LOG("Leaving Pause()\n");

    return (quit);
}

static void pause_load_media(void)
{
    if (!settings.sys_sound)
    {
        return;
    }
    pause_volume = T4K_Volume_Create(settings.sfx_volume, settings.mus_volume);
    if (pause_volume)
    {
        T4K_Volume_SetLayout(pause_volume, screen->w / 2, screen->h / 2 - 40,
                             screen->h / 2 + 60);
    }
}

static void pause_unload_media(void)
{
    if (pause_volume)
    {
        T4K_Volume_Destroy(pause_volume);
        pause_volume = NULL;
    }
    if (pause_kbd_bkg)
    {
        SDL_DestroySurface(pause_kbd_bkg);
        pause_kbd_bkg = NULL;
    }
}

/******************************************/
/*                                        */
/*       Local ("private") functions      */
/*                                        */
/******************************************/



static void pause_draw(void)
{
  SDL_Rect s;
  SDL_Surface* t = NULL;

  LOG("Entering pause_draw()\n");

  if (settings.sys_sound)
  {
    t = BlackOutline(_("Sound Effects Volume"), pause_font_size1, &white);
    if (t)
    {
        s.y = screen->h / 2 - 80;
        s.x = screen->w / 2 - t->w / 2;
        SDL_BlitSurface(t, NULL, screen, &s);
        SDL_DestroySurface(t);
    }

    t = BlackOutline(gettext("Music Volume"), pause_font_size1, &white);
    if (t)
    {
      s.y = screen->h/2 + 20;
      s.x = screen->w/2 - t->w/2;
      SDL_BlitSurface(t, NULL, screen, &s);
      SDL_DestroySurface(t);
    }
  }
  else  /* No sound: */
  {
    t = BlackOutline(gettext("Sound & Music Disabled"), pause_font_size1, &white);
    if (t)
    {
      s.y = screen->h/2 - 80;
      s.x = screen->w/2 - t->w/2;
      SDL_BlitSurface(t, NULL, screen, &s);
      SDL_DestroySurface(t);
    }
  }

  if (g_pause_in_game)
  {
      t = BlackOutline(gettext("Paused!"), pause_font_size2, &white);
      if (t)
      {
          s.y = screen->h / 2 - 180; //60;
          s.x = screen->w / 2 - t->w / 2;
          SDL_BlitSurface(t, NULL, screen, &s);
          SDL_DestroySurface(t);
      }
  }

  t = BlackOutline(gettext("'SPACE' or 'ESC' to return."), pause_font_size1,
                   &white);
  if (t)
  {
      s.y = screen->h / 2 + (g_pause_in_game ? 160 : 180);
      s.x = screen->w / 2 - t->w / 2;
      SDL_BlitSurface(t, NULL, screen, &s);
      SDL_DestroySurface(t);
  }

  if (g_pause_in_game)
  {
      t = BlackOutline(gettext("'Q' to quit."), pause_font_size1, &white);
      if (t)
      {
          s.y = screen->h / 2 + 200;
          s.x = screen->w / 2 - t->w / 2;
          SDL_BlitSurface(t, NULL, screen, &s);
          SDL_DestroySurface(t);
      }

      draw_kbd_toggle();
  }
  else
  {
      rectKbdToggle = (SDL_Rect){0, 0, 0, 0};
  }

  LOG("Leaving pause_draw()\n");
}

/* Draw (or redraw) the "'K' Show keyboard: ON/OFF" label centered in
 * rectKbdToggle. First call captures the underlying pixels so subsequent
 * calls (after a toggle) can restore-then-redraw and avoid old-glyph
 * artifacts at the edges where ON and OFF differ in width. */
static void draw_kbd_toggle(void)
{
    SDL_Surface* t;
    SDL_Rect     dst;
    /* Use the wider of the two strings to size the snapshot rect once. */
    const char* sample_str = gettext("'K' Show keyboard: OFF");
    const char* label =
        settings.show_keyboard ? gettext("'K' Show keyboard: ON") : sample_str;

    if (!pause_kbd_bkg)
    {
        SDL_Surface* sample =
            BlackOutline(sample_str, pause_font_size1, &white);
        if (!sample)
        {
            return;
        }
        rectKbdToggle.x = screen->w / 2 - sample->w / 2;
        rectKbdToggle.y = screen->h / 2 + 130;
        rectKbdToggle.w = sample->w;
        rectKbdToggle.h = sample->h;
        SDL_DestroySurface(sample);

        pause_kbd_bkg =
            SDL_CreateSurface(rectKbdToggle.w, rectKbdToggle.h, screen->format);
        if (pause_kbd_bkg)
        {
            SDL_BlitSurface(screen, &rectKbdToggle, pause_kbd_bkg, NULL);
        }
    }
    else if (pause_kbd_bkg)
    {
        SDL_BlitSurface(pause_kbd_bkg, NULL, screen, &rectKbdToggle);
    }

    t = BlackOutline(label, pause_font_size1, &white);
    if (t)
    {
        dst.x = screen->w / 2 - t->w / 2;
        dst.y = rectKbdToggle.y;
        dst.w = t->w;
        dst.h = t->h;
        SDL_BlitSurface(t, NULL, screen, &dst);
        SDL_DestroySurface(t);
    }
}
