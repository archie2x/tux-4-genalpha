/*
   playgame.c:

   Code for "Fish Cascade" typing activity.

   Copyright 2000, 2003, 2007, 2008, 2009, 2010, 2011.
   Authors: Sam Hart, Jesse Andrews, David Bruce, George Stephanos.

   Project email: <tux4kids-tuxtype-dev@lists.alioth.debian.org>
   Project website: http://tux4kids.alioth.debian.org

   playgame.c is part of Tux Typing, a.k.a "tuxtype".

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
#include "playgame.h"
#include "snow.h"
#include "SDL_extras.h"
#include <ctype.h>
#include <wctype.h>
#include "braille.h"
#include "target_hint.h"
#include "input.h"
#include "keyboard_display.h"

/* Should these be constants? */
static int tux_max_width        = 0; // the max width of the images of tux
static int number_max_w         = 0; // the max width of a number image
static int tts_announcer_switch = 1;
static int braille_letter_pos   = 0;

// static SDL_Surface* background = NULL;
static SDL_Surface* level[NUM_LEVELS] = {NULL};
static SDL_Surface* number[NUM_NUMS]  = {NULL};
static SDL_Surface* curlev            = NULL;
static SDL_Surface* lives             = NULL;
static SDL_Surface* fish              = NULL;
static KbdDisplay   cascade_keyboard;
static Input*       cascade_input = NULL;
/* The bkg filename in use for the current level — cached so the keyboard-
 * guide toggle in the pause overlay can re-bake without re-randomizing. */
static char         cascade_current_bkg[FNLEN];
static SDL_Surface* congrats[CONGRATS_FRAMES] = {NULL};
static SDL_Surface* ohno[OH_NO_FRAMES]        = {NULL};
static Mix_Chunk*   sound[NUM_WAVES];

static sprite* fish_sprite  = NULL;
static sprite* splat_sprite = NULL;

/* Local function prototypes: */
static void AddSplat(int* splats, struct fishypoo* f, int* curlives,
                     int* frame);
static void CheckCollision(int fishies, int* fish_left, int frame);
static void CheckFishies(int* fishies, int* splats);
static int  check_word(int f);
static void DrawBackground(void);
static void AddKeyboardToBackground(void);
static void DrawActiveKeyboardHighlights(int fishies);
static void draw_bar(int curlevel, int diflevel, int curlives, int oldlives,
                     int fish_left, int oldfish_left);
static void DrawFish(int which);
static void DrawNumbers(int num, int x, int y, int places);
static void EraseNumbers(int num, int x, int y, int places);

static float float_restrict(float a, float x, float b);
static void  FreeGame(void);

static void stop_tts_announcer();
static int  tts_announcer(void* struct_address);

static void set_braille_letter_pos(int fishies);

static int  int_restrict(int a, int x, int b);
static void LoadFishies(void);
static void LoadOthers(void);
static int  LoadTuxAnims(void);
static void MoveFishies(int* fishies, int* splats, int* lifes, int* frame);
static void MoveTux(int frame, int fishies);
static void next_tux_frame(void);
static void ResetObjects(void);
static void SpawnFishies(int diflevel, int* fishies, int* frame);
static void UpdateTux(wchar_t letter_pressed, int fishies, int frame);
static void WaitFrame(void);

int playing_level, fish_left, curlives;

/************************************************************************/
/*                                                                      */
/*         "Public" functions (callable throughout program)             */
/*                                                                      */
/************************************************************************/

/*************************************************************************
 * PlayCascade : This is the main Cascade game loop               *
 *************************************************************************/
int PlayCascade(int diflevel)
{
    /* Enable text input so dead-key composition (e.g. macOS Option+U U → ü)
     * reaches us as SDL_EVENT_TEXT_INPUT. SDL3 only delivers composed glyphs
     * via TEXT_INPUT — KEY_DOWN carries raw keycodes only. */
    SDL_StartTextInput(tt_window);

    char filename[FNLEN];
    int  still_playing             = 1;
    playing_level                  = 1;
    int setup_new_level            = 1;
    int won_level                  = 0;
    int quitting                   = 0;
    int curlevel                   = 0;
    int i                          = 0;
    curlives                       = 0;
    int oldlives                   = 0;
    int oldfish_left               = 0;
    fish_left                      = 0;
    int          fishies           = 0;
    int          local_max_fishies = 1;
    int          frame             = 0;
    int          done_frames;
    int          splats = 0;
    SDL_Event    event;
    SDL_Surface* temp_text[CONGRATS_FRAMES + OH_NO_FRAMES];
    SDL_Rect     text_rect;
    int          text_y_end;
    int          xamp;
    int          yamp;
    int          x_not;
    int          y_not;
    int          temp_text_frames;
    int          temp_text_count;
    Uint32       last_time, now_time;

    // Structure which contain the address of above struct
    // Why ? : Only one argument can be passed through thread
    // Structure which contain the address of variables
    struct tts_announcer_cascade_data_struct struct_with_data_address;

    // Giving address of variables
    struct_with_data_address.address_of_fishies = &fishies;

    // Call announcer function in thread which annonces the word to type
    if (settings.tts)
    {
        tts_announcer_thread = SDL_CreateThread(tts_announcer, "tt_thread",
                                                &struct_with_data_address);
    }

    DEBUGCODE
    {
        fprintf(stderr, "->Entering PlayCascade(): level=%i\n", diflevel);
    }

    //  SDL_HideCursor(); //don't really need this and it causes a bug on
    //  windows

    //	SNOW_init();

    if (!LoadTuxAnims())
    {
        fprintf(
            stderr,
            "PlayCascade() - LoadTuxAnims() failed - returning to menu!\n\n\n");
        FreeGame();
        SDL_StopTextInput(tt_window);
        return 0;
    }

    LoadFishies();
    LoadOthers();

    /* Make sure everything in the word list is "typable" according to the
     * current */
    /* theme's keyboard.lst: */
    if (!CheckNeededGlyphs())
    {
        fprintf(stderr,
                "PlayCascade() - did not find all needed characters in theme's "
                "keyboard.lst file - returning to menu!\n\n\n");
        FreeGame();
        SDL_StopTextInput(tt_window);
        return 0;
    }

    /*  --------- Begin outer game loop (cycles once per level): -------------
     */

    LOG(" starting game \n ");

    while (still_playing)
    {
        /* FIXME split out as smaller function */
        if (setup_new_level)
        {
            switch (diflevel)
            {
            default:
            case EASY:
                fish_left = MAX_FISHIES_EASY;

                if (settings.o_lives > LIVES_INIT_EASY)
                {
                    curlives = settings.o_lives;
                }
                else
                {
                    curlives = LIVES_INIT_EASY;
                }
                break;

            case MEDIUM:
                fish_left = MAX_FISHIES_MEDIUM;

                if (settings.o_lives > LIVES_INIT_MEDIUM)
                {
                    curlives = settings.o_lives;
                }
                else
                {
                    curlives = LIVES_INIT_MEDIUM;
                }
                break;

            case HARD:
                fish_left = MAX_FISHIES_HARD;

                if (settings.o_lives > LIVES_INIT_HARD)
                {
                    curlives = settings.o_lives;
                }
                else
                {
                    curlives = LIVES_INIT_HARD;
                }
                break;
            }

            local_max_fishies = fish_left;

            /* ------- Load and draw background: ----------------- */

            if (curlevel != 0)
            {
                FreeBothBkgds();
            }

            if (diflevel == INF_PRACT)
            {
                sprintf(filename, "pract.png");
            }
            else
            {
                sprintf(filename, "kcas%d.jpg", rand() % 12);
            }
            /* ---  Special Hidden Code  --- */

            if (settings.hidden && curlevel == 3)
            {
                sprintf(filename, "hidden.jpg");
            }

            DEBUGCODE
            {
                fprintf(stderr, "->>Loading background: %s\n", filename);
            }
            LoadBothBkgds(filename);
            strncpy(cascade_current_bkg, filename, FNLEN);
            cascade_current_bkg[FNLEN - 1] = '\0';
            //			SNOW_setBkg( background );

            ResetObjects();
            AddKeyboardToBackground();
            DrawBackground();

            if (settings.sys_sound)
            {
                // TODO make use of more music files
                if (rand() % 2)
                {
                    sprintf(filename, "amidst_the_raindrops.ogg");
                }
                else
                {
                    sprintf(filename, "chiptune2.ogg");
                }
                MusicLoad(filename, -1);
            }

            setup_new_level = 0;

            LOG("->>PLAYING THE GAME\n");
        }

        /*  --------- Begin main game loop (cycles once per frame):
         * ------------- */

        /* (Re-)create the input decoder for this game session. */
        Input_Free(cascade_input);
        cascade_input = (settings.input_mode == INPUT_BRAILLE)
                            ? Input_NewBraille(NULL, &cascade_keyboard)
                            : Input_NewTypewriter(NULL, &cascade_keyboard);
        Input_SetKbdVisible(cascade_input, settings.show_keyboard);

        while (playing_level)
        {
            if (T4K_QuitConfirmed())
            {
                exit(0);
            }

            last_time = SDL_GetTicks();

            oldlives     = curlives;
            oldfish_left = fish_left;

            EraseSprite(tux_object.spr[tux_object.state][tux_object.facing],
                        tux_object.x, tux_object.y);

            /* --- Poll input queue, get keyboard info --- */
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT)
                {
                    exit(0); /* FIXME does memory get cleaned up properly if we
                                do this? */
                }
                else
                {
                    if (event.type == SDL_EVENT_KEY_DOWN)
                    {
                        switch (event.key.key)
                        {
                        case SDLK_F11:
                            SDL_SaveBMP(screen, "screenshot.bmp");
                            break;

                        case SDLK_F6:
                            settings.o_lives = settings.o_lives - 10;
                            curlives         = curlives - 10;
                            break;

                        case SDLK_F7:
                            settings.o_lives = settings.o_lives + 10;
                            curlives         = curlives + 10;
                            break;

                        case SDLK_RALT:
                            braille_letter_pos = !braille_letter_pos;
                            break;

                        case SDLK_F10:
                            /* NOTE this could be used to "cheat" by wiping out
                             * all the */
                            /* current words if the player is about to lose. */
                            /* first wipe out old blits because screen size is
                             * changing */
                            /* and otherwise we would segfault: */
                            T4K_ResetBlitQueue();
                            // numupdates = 0;
                            T4K_SwitchScreenMode();
                            DrawBackground();
                            ResetObjects();
                            break;

                        case SDLK_F12:
                            // SNOW_toggle();
                            break;

                        /* Fish left */
                        case SDLK_F1:
                            tts_announcer_switch = 2;
                            break;

                        /* lives remaining */
                        case SDLK_F2:
                            tts_announcer_switch = 3;
                            break;

                        case SDLK_ESCAPE:
                            if (settings.tts)
                            {
                                stop_tts_announcer();
                            }
                            T4K_Tts_say(DEFAULT_VALUE, DEFAULT_VALUE, INTERRUPT,
                                        gettext("Game Paused."));

                            /* Pause() returns 1 if quitting, */
                            /* 0 if returning to game:        */
                            int prev_show_kbd = settings.show_keyboard;
                            if (Pause() == 1)
                            {
                                playing_level = 0;
                                still_playing = 0;
                                quitting      = 1;
                            }
                            else /* Returning to game */
                            {
                                if (settings.show_keyboard != prev_show_kbd &&
                                    cascade_current_bkg[0])
                                {
                                    /* Re-bake the background so the toggle
                                     * takes effect immediately mid-game. */
                                    FreeBothBkgds();
                                    LoadBothBkgds(cascade_current_bkg);
                                    AddKeyboardToBackground();
                                    DrawBackground();
                                }
                                T4K_Tts_say(DEFAULT_VALUE, DEFAULT_VALUE,
                                            INTERRUPT,
                                            gettext("Pause Released!"));
                                // Call announcer function in thread which
                                // annonces the word to type
                                if (settings.tts)
                                {
                                    tts_announcer_thread = SDL_CreateThread(
                                        tts_announcer, "tt_thread",
                                        &struct_with_data_address);
                                }
                                DrawBackground();
                            }
                            break;

                            /*  Don't count modifier keys as keystrokes in game:
                             */
                        case SDLK_RSHIFT:
                        case SDLK_LSHIFT:
                        case SDLK_RCTRL:
                        case SDLK_LCTRL:
                        case SDLK_LALT:
                        case SDLK_RMETA:
                        case SDLK_LMETA:
                        case SDLK_LGUI:
                        case SDLK_RGUI:
                            // T4K_Tts_say(DEFAULT_VALUE,DEFAULT_VALUE,INTERRUPT,"Please
                            // don't press modifier keys!");
                            break;

                        default:
                            /* Player typed something. Shared decoder handles it
                             * below; nothing to do here. */
                            break;
                        }
                    }

                    /* Pass event to active input decoder; returns ready=1
                     * with a wchar_t when typewriter TEXT_INPUT or braille
                     * KEY_UP produces a character. */
                    Input_SetWordPosition(cascade_input, braille_letter_pos);
                    InputToken typed = Input_Consume(cascade_input, &event);

                    if (typed.ready)
                    {
                        /* Cascade matches fish words case-insensitively by
                         * uppercasing typed input. Range-shift handles ASCII
                         * (a-z), Latin-1 Supplement (à-ÿ), and a Hungarian
                         * O-double-acute (256-382) special case. */
                        wchar_t key_unicode = typed.ch;
                        if (key_unicode >= 97 && key_unicode <= 122)
                        {
                            key_unicode -= 32;
                        }
                        if (key_unicode >= 224 && key_unicode <= 255)
                        {
                            key_unicode -= 32;
                        }
                        if (key_unicode >= 256 && key_unicode <= 382)
                        {
                            key_unicode -= 1;
                        }
                        UpdateTux(key_unicode, fishies, frame);
                    }
                }
            } /* ------ End of 'while' loop for handling user input ------- */

            /* --- fishy updates --- */
            if ((frame % 3) == 0)
            {
                NEXT_FRAME(fish_sprite);
            }

            if (fishies < local_max_fishies)
            {
                SpawnFishies(diflevel, &fishies, &frame);
            }

            DrawActiveKeyboardHighlights(fishies);

            MoveTux(frame, fishies);
            CheckCollision(fishies, &fish_left, frame);
            T4K_DrawSprite(tux_object.spr[tux_object.state][tux_object.facing],
                           tux_object.x, tux_object.y);
            MoveFishies(&fishies, &splats, &curlives, &frame);
            CheckFishies(&fishies, &splats);
            //      SNOW_update();

            /* --- update top score/info bar --- */

            if (diflevel != INF_PRACT)
            {
                draw_bar(curlevel, diflevel, curlives, oldlives, fish_left,
                         oldfish_left);

                if (curlives <= 0)
                {
                    playing_level = 0;
                    still_playing = 0;
                }
            }
            else
            {
                fish_left = 1; // in practice there is always 1 fish left!
            }

            if (fish_left <= 0)
            {
                won_level     = 1;
                playing_level = 0;
                curlevel++;
                setup_new_level = 1;
                still_playing   = 1;
            }

            if (!quitting)
            {
                /* This does all the blits that we have queued up this frame: */
                T4K_UpdateScreen(&frame);
            }

            /* Pause (keep frame-rate event) */
            now_time = SDL_GetTicks();

            DEBUGCODE
            {
                fprintf(stderr,
                        "now_time = %d\tlast_time = %d, elapsed time = %d\n",
                        now_time, last_time, now_time - last_time);
            }

            if (now_time < last_time + 1000 / FRAMES_PER_SEC)
            {
                // Prevent any possibility of a time wrap-around
                //  (this is a very unlikely problem unless there is an SDL bug
                //   or you leave tuxmath running for 49 days...)
                now_time = (last_time + 1000 / FRAMES_PER_SEC) -
                           now_time; // this holds the delay
                if (now_time > 1000 / FRAMES_PER_SEC)
                {
                    now_time = 1000 / FRAMES_PER_SEC;
                }
                SDL_Delay(now_time);
            }
            else
            {
                LOG("Did not achieve desired frame rate!\n");
            }
        } /* End per-frame game loop - level completed */

        if (settings.sys_sound)
        {
            T4K_AudioMusicUnload();
        }

        DrawBackground();

        /* Victory sequence, defeat sequence, or go to next level: */
        if (quitting == 0)
        {
            /* Level completed successfully: */
            if (won_level)
            {

                if (settings.tts)
                {
                    stop_tts_announcer();
                }

                if (settings.sys_sound)
                {
                    PlaySound(sound[WIN_WAV]);
                }

                if (curlevel < 4) /* Advance to next level */
                {
                    LOG("--->NEXT LEVEL!\n");
                    done_frames   = MAX_END_FRAMES_BETWEEN_LEVELS;
                    playing_level = 1;
                    xamp          = 0;
                    yamp          = 0;
                    won_level     = 0;
                    T4K_Tts_say(DEFAULT_VALUE, DEFAULT_VALUE, INTERRUPT,
                                gettext("Congratulation! Welcome to level %d!"),
                                curlevel + 1);
                }
                else
                {
                    LOG("--->WINNER!\n");
                    done_frames   = MAX_END_FRAMES_WIN_GAME;
                    still_playing = 0;
                    xamp          = WIN_GAME_XAMP;
                    yamp          = WIN_GAME_YAMP;
                    T4K_Tts_say(DEFAULT_VALUE, DEFAULT_VALUE, INTERRUPT,
                                gettext("Congratulations! Thank you! please "
                                        "wait till it return to main menu."));
                }

                for (i = 0; i < CONGRATS_FRAMES; i++)
                {
                    temp_text[i] = congrats[i];
                }

                temp_text_frames = CONGRATS_FRAMES;
                tux_object.state = TUX_WINNING;
            }
            else /* Did not win the level  :-(     */
            {
                LOG("--->LOST :(\n");
                done_frames = MAX_END_FRAMES_GAMEOVER;
                xamp        = 0;
                yamp        = 0;

                if (settings.tts)
                {
                    stop_tts_announcer();
                }

                if (settings.sys_sound)
                {
                    PlaySound(sound[LOSE_WAV]);
                }

                T4K_Tts_say(
                    DEFAULT_VALUE, DEFAULT_VALUE, INTERRUPT,
                    gettext(
                        "yep you miss it. hahh hahh haa. game over! goodbye!"));

                for (i = 0; i < OH_NO_FRAMES; i++)
                {
                    temp_text[i] = ohno[i];
                }

                temp_text_frames = OH_NO_FRAMES;
                tux_object.state = TUX_YIPING;
            }

            /* --- draw the animation here --- */

            temp_text_count = 0;
            text_y_end      = (screen->h / 2) - (temp_text[0]->h / 2);
            text_rect.x     = (screen->w / 2) - (temp_text[0]->w / 2);
            text_rect.y     = screen->h - temp_text[0]->h - 1;
            x_not           = text_rect.x;

            LOG("--->Starting Ending Animation\n");

            for (i = 0; i <= done_frames; i++)
            {
                /* Drain the event queue each frame — without this the OS treats
                 * the process as hung after a few hundred ms of no event
                 * activity. */
                SDL_PumpEvents();

                /* Honor the quit gesture mid-animation. */
                if (T4K_QuitConfirmed())
                {
                    exit(0);
                }

                temp_text_count = (temp_text_count + 1) % temp_text_frames;

                text_rect.y -= END_FRAME_DY;
                y_not = text_rect.y;

                if (text_rect.y < text_y_end)
                {
                    y_not = text_y_end + yamp * sin(i / WIN_GAME_ANGLE_MULT);
                    text_rect.y = text_y_end;
                    text_rect.x = x_not + xamp * cos(i / WIN_GAME_ANGLE_MULT);
                }

                T4K_DrawSprite(
                    tux_object.spr[tux_object.state][tux_object.facing],
                    tux_object.x, tux_object.y);
                T4K_DrawObject(temp_text[temp_text_count], text_rect.x, y_not);
                T4K_DrawObject(level[diflevel], 1, 1);
                draw_bar(curlevel - 1, diflevel, curlives, oldlives, fish_left,
                         oldfish_left);

                /* Match MoveTux's pacing — advance the animation every 3rd
                 * frame (≈5 fps) so the win/lose Tux doesn't flap his arms
                 * wildly. */
                if ((i % 3) == 0)
                {
                    next_tux_frame();
                }
                //        SNOW_update();
                /* Do all pending blits and increment frame counter: */
                T4K_UpdateScreen(&frame);

                EraseSprite(tux_object.spr[tux_object.state][tux_object.facing],
                            tux_object.x, tux_object.y);
                EraseObject(temp_text[temp_text_count], text_rect.x, y_not);

                if (!settings.speed_up)
                {
                    WaitFrame();
                }

            } /* End of animation for end of game */
            if (still_playing)
            {
                fishies = 0; // Otherwise thread will announce old words and
                             // cause segfault
                if (settings.tts)
                {
                    tts_announcer_thread = SDL_CreateThread(
                        tts_announcer, "tt_thread", &struct_with_data_address);
                }
            }

        } /* End of post-level wrap-up  */

    } /*   -------- End outer game loop -------------- */

    // N.x.L
    fprintf(stderr, "Exiting game");
    if (settings.tts)
    {
        stop_tts_announcer();
    }

    //  SNOW_on = 0;

    LOG("->Done with level... cleaning up\n");

    FreeGame();

    SDL_StopTextInput(tt_window);

    LOG("->PlayCascade(): END\n");

    return 1;
}

/************************************************************************/
/*                                                                      */
/*         "Private" functions (local to playgame.c)                    */
/*                                                                      */
/************************************************************************/

static int check_word(int f)
{
    int i;

    if (wcslen(fish_object[f].word) > tux_object.wordlen)
    {
        return 0;
    }

    for (i = 0; i < wcslen(fish_object[f].word); i++)
    {
        if (fish_object[f].word[i] !=
            tux_object
                .word[tux_object.wordlen - wcslen(fish_object[f].word) + i])
        {
            return 0;
        }
    }

    return 1;
}

/***************************************
    WaitFrame: wait for the next frame
***************************************/
void WaitFrame(void)
{
    static Uint32 next_tick = 0;
    Uint32        this_tick = SDL_GetTicks();

    if (this_tick < next_tick)
    {
        SDL_Delay(next_tick - this_tick);
    }

    next_tick = this_tick + (1000 / FRAMES_PER_SEC);
}

/****************************************************
 ResetObjects : Clear and reset all objects to dead
****************************************************/

static void ResetObjects(void)
{
    int i;

    LOG("RESETTING OBJECTS\n");

    for (i = 0; i < MAX_FISHIES_HARD + 1; i++)
    {
        fish_object[i]  = null_fishy;
        splat_object[i] = null_splat;
    }

    tux_object.facing  = RIGHT;
    tux_object.x       = screen->w / 2;
    tux_object.y       = screen->h - tux_object.spr[0][RIGHT]->frame[0]->h - 1;
    tux_object.dx      = 0;
    tux_object.dy      = 0;
    tux_object.endx    = tux_object.x;
    tux_object.endy    = tux_object.y;
    tux_object.state   = TUX_STANDING;
    tux_object.word[0] = 0;
    tux_object.wordlen = 0;

    LOG("OBJECTS RESET\n");
}

/*********************
LoadOthers : Load all other graphics
**********************/
static void LoadOthers(void)
{
    int  i;
    char filename[FNLEN];

    LOG("=LoadOthers()\n");
    DEBUGCODE
    {
        fprintf(stderr, "settings.theme_font_name is %s\n",
                settings.theme_font_name);
    }

    RenderLetters(FISHY_FONT_SIZE);
    LOG(" Done rendering letters \n ");

    curlev = T4K_BlackOutline(gettext("Level"), LABEL_FONT_SIZE, &white);
    lives  = T4K_BlackOutline(gettext("Lives"), LABEL_FONT_SIZE, &white);
    fish   = T4K_BlackOutline(gettext("Fish"), LABEL_FONT_SIZE, &white);

    level[0] = T4K_BlackOutline(gettext("Easy"), LABEL_FONT_SIZE, &white);
    level[1] = T4K_BlackOutline(gettext("Medium"), LABEL_FONT_SIZE, &white);
    level[2] = T4K_BlackOutline(gettext("Hard"), LABEL_FONT_SIZE, &white);
    level[3] = T4K_BlackOutline(gettext("Practice"), LABEL_FONT_SIZE, &white);

    number_max_w = 0;
    for (i = 0; i < NUM_NUMS; i++)
    {
        sprintf(filename, "num_%i.png", i);
        number[i] = LoadImage(filename, IMG_ALPHA);
        if (number[i]->w > number_max_w)
        {
            number_max_w = number[i]->w;
        }
    }

    for (i = 0; i < CONGRATS_FRAMES; i++)
    {
        congrats[i] = T4K_BlackOutline(gettext("Congratulations"),
                                       LABEL_FONT_SIZE, &white);
    }

    for (i = 0; i < OH_NO_FRAMES; i++)
    {
        ohno[i] = T4K_BlackOutline(gettext("Oh No!"), LABEL_FONT_SIZE, &white);
    }

    Kbd_Display_Load(&cascade_keyboard, 96, screen->w * 9 / 10);

    if (settings.sys_sound)
    {
        LOG("=Loading Sound FX\n");

        sound[WIN_WAV]      = LoadSound("win.wav");
        sound[WINFINAL_WAV] = LoadSound("winfinal.wav");
        sound[BITE_WAV]     = LoadSound("bite.wav");
        sound[LOSE_WAV]     = LoadSound("lose.wav");
        sound[RUN_WAV]      = LoadSound("run.wav");
        sound[SPLAT_WAV]    = LoadSound("splat.wav");
        sound[EXCUSEME_WAV] = LoadSound("excuseme.wav");

        LOG("=Done Loading Sound FX\n");
    }
    else
    {
        LOG("=NO SOUND FX LOADED (not selected)\n");
    }

    //	PauseLoadMedia();

    LOG("=Setting NULL fish & splat & word\n");

    null_fishy.alive   = 0;
    null_fishy.can_eat = 0;
    null_fishy.word    = NULL;
    null_fishy.x       = 0;
    null_fishy.y       = 0;
    null_fishy.dy      = 0;

    null_splat.x     = 0;
    null_splat.y     = 0;
    null_splat.alive = 0;

    LOG("=LoadOthers() END\n");
}

/***************************
LoadFishies : Load the fish animations and graphics
*****************************/
static void LoadFishies(void)
{
    int i;

    LOG("=LoadFishies()\n");

    fish_sprite  = LoadSprite("fishy", IMG_ALPHA);
    splat_sprite = LoadSprite("splat", IMG_ALPHA);

    for (i = 0; i < MAX_FISHIES_HARD; i++)
    {
        fish_object[i].alive   = 1;
        fish_object[i].can_eat = 0;
        splat_object[i].alive  = 0;
    }

    LOG("=LoadFishies(): END\n");
}

/******************************
LoadTuxAnims : Load the Tux graphics and animations
*******************************/
static int LoadTuxAnims(void)
{
    int i;

    LOG("LoadTuxAnims(): Loading Tux Animations\n");

    for (i = 0; i < TUX_NUM_STATES; i++)
    {
        tux_object.spr[i][RIGHT] = LoadSprite(tux_sprite_fns[i], IMG_ALPHA);
        /* make sure image got loaded: */
        if (tux_object.spr[i][RIGHT] == NULL)
        {
            fprintf(stderr, "Warning - image %d failed to load\n", i);
            return 0;
        }
        tux_object.spr[i][LEFT] = FlipSprite(tux_object.spr[i][RIGHT], 1, 0);
    }

    tux_max_width = tux_object.spr[TUX_STANDING][RIGHT]->frame[0]->w;

    LOG("LoadTuxAnims(): END\n");

    return 1;
}

/******************************
DrawNumbers : Draw numbers at
a certain x,y. See "usage"
below
*******************************/
static void DrawNumbers(int num, int x, int y, int places)
{
    LOG("\nEntering DrawNumbers()\n");

    // usage:
    //       num    = number to draw onscreen
    //       x, y   = coords to place number (starting upper left)
    //       places = number of places to fit it into (i.e., if
    //                                        number = 5 and places = 2, would
    //                                        draw "05") if places = 0, then
    //                                        will simply display as many as
    //                                        necessary
    char numnuts[FNLEN];
    int  needed_places, i;
    int  uddernumber;

    sprintf(numnuts, "%d", num);
    i             = 0;
    needed_places = strlen(numnuts);

    if (needed_places < FNLEN && needed_places <= places)
    {
        if (places > 0)
        {
            for (i = 1; i <= (places - needed_places); i++)
            {
                T4K_DrawObject(number[0], x, y);
                x += number[0]->w;
            }
        }
    }

    for (i = 0; i < needed_places; i++)
    {
        uddernumber = numnuts[i] - '0';
        T4K_DrawObject(number[uddernumber], x, y);
        x += number[uddernumber]->w;
    }
    LOG("\nLeaving DrawNumbers()\n");
}

/*************************
EraseNumbers: Erase numbers
from the screen. See "usage"
*****************************/
static void EraseNumbers(int num, int x, int y, int places)
{
    LOG("\nEntering EraseNumbers()\n");
    // usage:
    //       num    = number to draw onscreen
    //       x, y   = coords to place number (starting upper left)
    //       places = number of places to fit it into (i.e., if
    //                                        number = 5 and places = 2, would
    //                                        draw "05") if places = 0, then
    //                                        will simply display as many as
    //                                        necessary
    char numnuts[FNLEN];
    int  needed_places, i;
    int  uddernumber;

    sprintf(numnuts, "%d", num);
    i             = 0;
    needed_places = strlen(numnuts);

    if (needed_places < FNLEN && needed_places <= places)
    {
        if (places > 0)
        {
            for (i = 1; i <= (places - needed_places); i++)
            {
                EraseObject(number[0], x, y);
                x += number[0]->w;
            }
        }
    }
    for (i = 0; i < needed_places; i++)
    {
        uddernumber = numnuts[i] - '0';
        EraseObject(number[uddernumber], x, y);
        x += number[uddernumber]->w;
    }

    LOG("\nLeaving EraseNumbers()\n");
}

/**********************
FreeGame : Free all
the game elements
***********************/
static void FreeGame(void)
{
    int i;

    FreeLetters();

    LOG("FreeGame():\n-Freeing Tux Animations\n");

    for (i = 0; i < TUX_NUM_STATES; i++)
    {
        if (tux_object.spr[i][RIGHT])
        {
            FreeSprite(tux_object.spr[i][RIGHT]);
        }
        tux_object.spr[i][RIGHT] = NULL;

        if (tux_object.spr[i][LEFT])
        {
            FreeSprite(tux_object.spr[i][LEFT]);
        }
        tux_object.spr[i][LEFT] = NULL;
    }

    LOG("-Freeing fishies\n");

    if (fish_sprite)
    {
        FreeSprite(fish_sprite);
    }
    if (splat_sprite)
    {
        FreeSprite(splat_sprite);
    }
    fish_sprite = splat_sprite = NULL;

    LOG("-Freeing other game graphics\n");

    FreeBothBkgds();

    if (curlev)
    {
        SDL_DestroySurface(curlev);
    }
    if (fish)
    {
        SDL_DestroySurface(fish);
    }
    if (lives)
    {
        SDL_DestroySurface(lives);
    }
    curlev = fish = lives = NULL;
    Kbd_Display_Free(&cascade_keyboard);

    for (i = 0; i < NUM_LEVELS; i++)
    {
        if (level[i])
        {
            SDL_DestroySurface(level[i]);
        }
        level[i] = NULL;
    }
    for (i = 0; i < NUM_NUMS; i++)
    {
        if (number[i])
        {
            SDL_DestroySurface(number[i]);
        }
        number[i] = NULL;
    }
    for (i = 0; i < CONGRATS_FRAMES; i++)
    {
        if (congrats[i])
        {
            SDL_DestroySurface(congrats[i]);
        }
        congrats[i] = NULL;
    }
    for (i = 0; i < OH_NO_FRAMES; i++)
    {
        if (ohno[i])
        {
            SDL_DestroySurface(ohno[i]);
        }
        ohno[i] = NULL;
    }
    if (settings.sys_sound)
    {
        LOG("-Freeing sound\n");
        for (i = 0; i < NUM_WAVES; ++i)
        {
            if (sound[i])
            {
                MIX_DestroyAudio(sound[i]);
            }
            sound[i] = NULL;
        }
    }

    //  PauseUnloadMedia();

    LOG("FreeGame(): END\n");
}

/***************************
DrawBackground : This
function updates the entire
background. Usefull when
loading new backgrounds,
or clearing game screen
****************************/
static void DrawBackground(void)
{
    T4K_ResetBlitQueue();
    T4K_DrawObject(CurrentBkgd(), 0, 0);

    // //    struct blit *update;
    //
    //     LOG("-DrawBackground(): Updating entire background\n");
    //
    //     numupdates=0;  // drawing entire background writes over all other
    //     stuff, so don't draw them
    //
    //     update = &blits[numupdates++];
    //     update->src = CurrentBkgd();
    //
    //     update->srcrect->x = update->dstrect->x = 0;
    //     update->srcrect->y = update->dstrect->y = 0;
    //     update->srcrect->w = update->dstrect->w = update->src->w;
    //     update->srcrect->h = update->dstrect->h = update->src->h;
    //
    //     update->type = 'D';
}

/***************************
AddKeyboardToBackground : Composite a faint keyboard guide into Cascade's
background so sprite erases restore the keyboard layer cleanly. Gated on
settings.show_keyboard — when off, just position (so a later toggle can
re-bake without re-positioning).
****************************/
static void AddKeyboardToBackground(void)
{
    Kbd_Display_SetPositionAbove(&cascade_keyboard, screen->w, tux_object.y,
                                 screen->h);
    if (settings.show_keyboard)
    {
        Kbd_Display_BakeIntoBackground(&cascade_keyboard, CurrentBkgd());
    }
}

/***************************
DrawActiveKeyboardHighlights : Highlight keys that can advance a live fish.
****************************/
static void DrawActiveKeyboardHighlights(int fishies)
{
    int drawn[MAX_UNICODES] = {0};
    int lowest              = -1;
    int lowest_target       = 0;
    int i, j;

    if (!cascade_keyboard.base || !settings.show_keyboard)
    {
        return;
    }

    Kbd_Display_QueueErase(&cascade_keyboard);

    /* Walk every alive shootable fish. Normal mode highlights each one's
     * next-char-to-type (dedup'd via drawn[]). Braille mode tracks the
     * lowest-on-screen fish (largest .y, since SDL y grows downward) —
     * its chord is the single highlight, since every chord shares the
     * fdsjkl keys and showing multiple at once is unreadable. */
    for (i = 0; i < fishies; i++)
    {
        int fish_len;
        int match_len = 0;
        int key;

        if (!fish_object[i].alive || fish_object[i].can_eat ||
            !fish_object[i].word)
        {
            continue;
        }

        fish_len = (int)wcslen(fish_object[i].word);
        for (j = 0; j < tux_object.wordlen; j++)
        {
            int k;
            int candidate = tux_object.wordlen - j;

            if (candidate > fish_len)
            {
                continue;
            }
            for (k = 0; k < candidate; k++)
            {
                if (fish_object[i].word[k] != tux_object.word[j + k])
                {
                    break;
                }
            }
            if (k == candidate)
            {
                match_len = candidate;
                break;
            }
        }

        if (match_len >= fish_len)
        {
            continue;
        }

        if (settings.input_mode == INPUT_BRAILLE)
        {
            if (lowest < 0 || fish_object[i].y > fish_object[lowest].y)
            {
                lowest        = i;
                lowest_target = match_len;
            }
            continue;
        }

        /* Cascade is case-insensitive; use the lowercase rune for hint
         * lookup so typewriter mode doesn't suggest shift and braille
         * mode doesn't suggest the dot-6 capital prefix. */
        wchar_t hint_ch = towlower(fish_object[i].word[match_len]);
        key             = GetIndex(hint_ch);
        if (key < 0 || key >= MAX_UNICODES || drawn[key])
        {
            continue;
        }
        if (Kbd_Display_DrawGreenKey(&cascade_keyboard, key))
        {
            drawn[key] = 1;
        }
    }

    if ((settings.input_mode == INPUT_BRAILLE) && lowest >= 0)
    {
        wchar_t dots[6];
        int     n = Braille_DotsForChar(
            towlower(fish_object[lowest].word[lowest_target]), dots);
        for (int d = 0; d < n; d++)
        {
            int key = GetIndex(dots[d]);
            if (key >= 0 && key < MAX_UNICODES)
            {
                Kbd_Display_DrawGreenKey(&cascade_keyboard, key);
            }
        }
    }
}

/****************************
SpawnFishies: Spawn the fishes
in the key cascade game
*****************************/
static void SpawnFishies(int diflevel, int* fishies, int* frame)
{
    int      i, spacing, max_length;
    wchar_t* new_word;

    LOG("Enter SpawnFishies()\n");

    /* Some safety checks: */

    if (!fishies || !frame)
    {
        fprintf(stderr, "SpawnFishies() - invalid ptr arg, returning");
        return;
    }

    /* Ensure that *fishies is a valid index for fish_object[]: */
    if (*fishies < 0 || *fishies > MAX_FISHIES_HARD)
    {
        fprintf(stderr, "SpawnFishies() - invalid *fishies: %d - returning",
                *fishies);
        return;
    }

    /* prevent segfault if for some reason fish_sprite invalid: */
    if (!fish_sprite || !fish_sprite->frame[0])
    {
        fprintf(stderr, "SpawnFishies() - invalid fish_sprite - returning");
        return;
    }

    switch (diflevel)
    {
    case INF_PRACT:
    case EASY:
        spacing = FISH_SPACING_EASY;
        break;
    case MEDIUM:
        spacing = FISH_SPACING_MEDIUM;
        break;
    case HARD:
        spacing = FISH_SPACING_HARD;
        break;
    default:
        spacing = FISH_SPACING_EASY;
        fprintf(stderr, "SpawnFishies() - invalid diflevel: %d\n", diflevel);
    }

    /* --- return without spawn if there isn't room yet --- */

    for (i = 0; i < *fishies; i++)
    {
        if (fish_object[i].y < (fish_sprite->frame[0]->h + spacing))
        {
            return;
        }
    }

    /* See how long of a word will fit the length of our screen: */
    max_length = screen->w / fish_sprite->frame[0]->w;

    do
    {
        new_word = GetWord();
        if (!new_word)
        {
            break;
        }
    } while (wcslen(new_word) > max_length);

    /* See if we get a valid word before we move on: */
    if (!new_word)
    {
        fprintf(stderr, "SpawnFishies() - could not get word - returning.\n");
        return;
    }

    /* If we get to here, it should be OK to actually spawn the fishy: */
    fish_object[*fishies].word    = new_word;
    fish_object[*fishies].len     = wcslen(new_word); // using wchar_t[] now
    fish_object[*fishies].alive   = 1;
    fish_object[*fishies].can_eat = 0;
    fish_object[*fishies].excused = 0;
    fish_object[*fishies].w =
        fish_sprite->frame[0]->w * fish_object[*fishies].len;
    fish_object[*fishies].x = rand() % (screen->w - fish_object[*fishies].w);
    fish_object[*fishies].y = 0;

    /* set the percentage of the speed based on length */
    fish_object[*fishies].dy = pow(0.92, fish_object[*fishies].len - 1);
    /* ex: a 9 letter word will be roughly twice as slow! 0.92^8 */

    /* give it a random variance so the fish "crunch" isn't constant */
    fish_object[*fishies].dy *= ((75 + rand() % 25) / 100.0);

    switch (diflevel)
    {
    case INF_PRACT:
        fish_object[*fishies].dy = DEFAULT_SPEED;
        break;
    case EASY:
        fish_object[*fishies].dy *= EASY_SPEED_FACTOR;
        break;
    case MEDIUM:
        fish_object[*fishies].dy *= MEDIUM_SPEED_FACTOR;
        break;
    case HARD:
        fish_object[*fishies].dy *= HARD_SPEED_FACTOR;
        break;
    default:
        fprintf(stderr, "SpawnFishies() - invalid diflevel: %d\n", diflevel);
    }

    /* Calculate the frame number at which the bottom of the fish will reach the
     * top of Tux */
    fish_object[*fishies].splat_time =
        *frame + (screen->h - fish_sprite->frame[0]->h -
                  tux_object.spr[TUX_STANDING][0]->frame[0]->h) /
                     (fish_object[*fishies].dy);

    DEBUGCODE
    {
        /* NOTE need %S rather than %s because of wide characters */
        fprintf(stderr, "Spawn fishy with word '%S'\n",
                fish_object[*fishies].word);
        fprintf(stderr, "Byte length is: %d\n",
                (int)wcslen(fish_object[*fishies].word));
        fprintf(stderr, "UTF-8 char length is: %d\n",
                (int)fish_object[*fishies].len);
    }

    *fishies = *fishies + 1;

    LOG("Leave SpawnFishies()\n");
}

/***************************
CheckFishies : Check all the fishies and splats.
               sort the splats and fishies
****************************/
static void CheckFishies(int* fishies, int* splats)
{
    int             forward, backward;
    struct fishypoo fish_temp;
    struct splatter splat_temp;

    LOG("Entering CheckFishies()\n");

    /* move any fish from the rear to fill up gaps in the
     * front
     */
    forward  = 0;
    backward = MAX_FISHIES_HARD;
    while (forward < backward)
    {
        while (backward > 0 && !fish_object[backward].alive)
        {
            backward--;
        }
        while (forward < backward && fish_object[forward].alive)
        {
            forward++;
        }
        if ((fish_object[backward].alive) && (!fish_object[forward].alive))
        {
            /* swap alive to be infront of dead */
            fish_temp             = fish_object[backward];
            fish_object[backward] = fish_object[forward];
            fish_object[forward]  = fish_temp;
        }
    }

    /* same thing for splats */

    forward  = 0;
    backward = MAX_FISHIES_HARD;
    while (forward < backward)
    {
        while (backward > 0 && !splat_object[backward].alive)
        {
            backward--;
        }
        while (forward < backward && splat_object[forward].alive)
        {
            forward++;
        }
        if ((splat_object[backward].alive) && (!splat_object[forward].alive))
        {
            /* swap alive to be infront of dead */
            splat_temp             = splat_object[backward];
            splat_object[backward] = splat_object[forward];
            splat_object[forward]  = splat_temp;
        }
    }

    /* update the count for fishies & splats */

    *splats = *fishies = 0;

    for (forward = 0; forward < MAX_FISHIES_HARD; forward++)
    {
        if (fish_object[forward].alive)
        {
            *fishies += 1;
        }
        else
        {
            break;
        }
    }

    for (forward = 0; forward < MAX_FISHIES_HARD; forward++)
    {
        if (splat_object[forward].alive)
        {
            *splats += 1;
        }
        else
        {
            break;
        }
    }

    LOG("Leaving CheckFishies()\n");
}

// Restrict x to a value in the range from a ... b
static int int_restrict(int a, int x, int b)
{
    if (x < a)
    {
        x = a;
    }
    if (x > b)
    {
        x = b;
    }
    return x;
}

static float float_restrict(float a, float x, float b)
{
    if (x < a)
    {
        x = a;
    }
    if (x > b)
    {
        x = b;
    }
    return x;
}

/***************************
AddSplat: A fish has died, add a splat where he used to be
****************************/
static void AddSplat(int* splats, struct fishypoo* f, int* curlives, int* frame)
{
    int i;

    LOG("Enterint AddSplat()\n");

    for (i = 0; i < f->len; i++)
    {
        splat_object[*splats].x =
            int_restrict(0,
                         f->x + (fish_sprite->frame[0]->w * i) +
                             ((fish_sprite->frame[fish_sprite->cur]->w) >> 1) -
                             ((splat_sprite->frame[0]->w) >> 1),
                         screen->w - splat_sprite->frame[0]->h - 1);
        splat_object[*splats].y = screen->h - splat_sprite->frame[0]->h - 1;
        splat_object[*splats].alive =
            10; // JA tweak here! frames of live of the splat
        *splats = *splats + 1;
    }

    f->alive = 0;

    *curlives = *curlives - 1;

    if (*curlives <= 0)
    {
        *curlives = 0;
    }

    if (settings.sys_sound)
    {
        PlaySound(sound[SPLAT_WAV]);
    }

    LOG("Enterint AddSplat()\n");
}

#define NUM_RTL_LANGUAGES 4
static int RTL()
{
    int   i;
    char* languages[NUM_RTL_LANGUAGES] = {"he_IL.utf8", "ar_AE.utf8",
                                          "ar_BH.utf8", "ar_DZ.utf8"}; // etc
    for (i = 0; i < NUM_RTL_LANGUAGES; i++)
    {
        if (strcmp(settings.theme_locale_name, languages[i]) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static void DrawFish(int which)
{
    int j           = 0;
    int red_letters = 0;
    int current_letter;
    /* 'x_inset' and 'y_inset' are where the glyph to be drawn relative */
    /* the fish_sprite graphic: */
    const int x_inset = 5;
    const int y_inset = 0;
    /* letter_x and letter_y are where the upper left corner of the glyph needs
     */
    /* to be located - (e.g. how SDL blitting understands locations) */
    int letter_x = 0;
    int letter_y = 0;

    LOG("Entering DrawFish()\n\n");

    SDL_Surface* letter_surface;

    /* Make sure needed pointers are valid - if not, return: */
    if (!fish_sprite || !fish_sprite->frame[0])
    {
        fprintf(stderr, "DrawFish() - returning, needed pointer invalid\n");
        return;
    }

    LOG("DrawFish() - drawing fishies:\n");

    /* Draw the fishies: */
    for (j = 0; j < fish_object[which].len; j++)
    {
        T4K_DrawSprite(fish_sprite,
                       fish_object[which].x + (fish_sprite->frame[0]->w * j),
                       fish_object[which].y);
    }

    LOG("DrawFish() - drawing letters:\n");

    /* Now we draw the letters on top of the fish: */
    /* we only draw the letter if tux cannot eat it yet */
    if (!fish_object[which].can_eat)
    {
        red_letters = -1;
        j           = 0;

        LOG("figure out how many letters are red:\n");
        while (j < tux_object.wordlen && red_letters == -1)
        {
            int k;
            for (k = 0; k < tux_object.wordlen - j; k++)
            {
                if (fish_object[which].word[k] != tux_object.word[j + k])
                {
                    k = 100000;
                }
            }

            if (k < 100000)
            {
                red_letters = tux_object.wordlen - j;
            }
            else
            {
                j++;
            }
        }

        LOG("Now draw each letter:\n");

        int length = wcslen(fish_object[which].word);
        for (j = 0; j < length; j++)
        {
            current_letter = (int)fish_object[which].word[j];

            if (j < red_letters)
            {
                letter_surface = GetRedGlyph(current_letter);
            }
            else
            {
                letter_surface = GetWhiteGlyph(current_letter);
            }

            /* Set "letter_x" and "letter_y to where we want the letter drawn:
             */
            letter_x =
                fish_object[which].x + (j * fish_sprite->frame[0]->w) + x_inset;

            if (RTL())
            {
                letter_x = fish_object[which].x +
                           ((length - 1 - j) * fish_sprite->frame[0]->w) +
                           x_inset;
            }

            letter_y = fish_object[which].y + y_inset;

            if (letter_surface != NULL)
            {
                T4K_DrawObject(letter_surface, letter_x, letter_y);
            }

            Target_DrawHintBelow((wchar_t)current_letter, letter_x, letter_y,
                                 letter_surface ? letter_surface->w : 24,
                                 letter_surface ? letter_surface->h : 28, 20,
                                 screen);
        }
    }
    LOG("Leaving DrawFish()\n");
}

/****************************
MoveFishies : Display and
move the fishies according
to their settings
*****************************/
static void MoveFishies(int* fishies, int* splats, int* lifes, int* frame)
{
    int i, j;

    LOG("\nEntering MoveFishies()\n");

    for (i = 0; i < *fishies; i++)
    {
        if (fish_object[i].alive)
        {
            /* Erase one extra slot past the word — Indic conjuncts (and some
             * accented Latin) can render wider than fish_sprite.w, spilling
             * into the slot to the right; without this last-slot erase, the
             * trailing pixels of the last char leave a streak as the fish
             * moves down. */
            for (j = 0; j <= fish_object[i].len; j++)
            {
                EraseSprite(fish_sprite,
                            fish_object[i].x + (fish_sprite->frame[0]->w * j),
                            fish_object[i].y);
            }
            /* In braille mode the chord glyph sits just below the
             * letter (and below the fish-sprite-sized erase above), so
             * trails persist without an extra erase strip. The
             * background has the keyboard guide baked in, so this just
             * restores the keyboard pixels under any stale braille. */
            if (settings.input_mode == INPUT_BRAILLE)
            {
                SDL_Rect below = {
                    fish_object[i].x,
                    fish_object[i].y + fish_sprite->frame[0]->h,
                    fish_sprite->frame[0]->w * (fish_object[i].len + 1), 32};
                SDL_BlitSurface(CurrentBkgd(), &below, screen, &below);
            }

            fish_object[i].y += fish_object[i].dy;

            if (fish_object[i].y >=
                (screen->h) - fish_sprite->frame[fish_sprite->cur]->h - 1)
            {
                AddSplat(splats, &fish_object[i], lifes, frame);
            }
        }
    }

    for (i = 0; i < *fishies; i++)
    {
        if (fish_object[i].alive && fish_object[i].can_eat)
        {
            DrawFish(i);
        }
    }

    for (i = 0; i < *fishies; i++)
    {
        if (fish_object[i].alive && !fish_object[i].can_eat)
        {
            DrawFish(i);
        }
    }

    for (i = 0; i < *splats; i++)
    {
        if (splat_object[i].alive)
        {
            splat_object[i].alive--;
            if (splat_object[i].alive > 1)
            {
                T4K_DrawSprite(splat_sprite, splat_object[i].x,
                               splat_object[i].y);
            }
        }
        else
        {
            EraseSprite(splat_sprite, splat_object[i].x, splat_object[i].y);
        }
    }

    LOG("Leaving MoveFishies()\n\n");
}

/* UpdateTux : anytime a key is pressed, we need check to
 * see if a fish can be eaten.  The fish that could hit
 * the bottom of the screen first should be choosen if
 * two fishies match what is typed
 */
void UpdateTux(wchar_t letter_pressed, int fishies, int frame)
{
    int i;
    int time_it_splats = 0;
    int which          = -1;

    /* --- move our word array to make room if needed --- */

    if (tux_object.wordlen == MAX_WORD_SIZE)
    {
        for (i = 0; i < MAX_WORD_SIZE; i++)
        {
            tux_object.word[i] = tux_object.word[i + 1];
        }
        tux_object.wordlen--;
    }

    /* --- add letter pressed to word array --- */

    tux_object.word[tux_object.wordlen] = letter_pressed;
    tux_object.wordlen++;
    tux_object.word[tux_object.wordlen] = 0;

    /* choose the word that matchs what has been typed  */
    /* and will hit the ground first                    */
    for (i = 0; i < fishies; i++)
    {
        if ((fish_object[i].alive && !fish_object[i].can_eat) &&
            check_word(i) &&
            (time_it_splats > fish_object[i].splat_time || !time_it_splats))
        {
            time_it_splats = fish_object[i].splat_time;
            which          = i;
        }
    }

    if (which != -1)
    {
        fish_object[which].can_eat = 1;
        tux_object.wordlen         = 0;
        tux_object.word[0]         = 0;
    }
    set_braille_letter_pos(fishies);
}

/*************************
CheckCollision: Check
for collisions between Tux
and Fishies. If collided,
perform appropriate action
***************************/
static void CheckCollision(int fishies, int* fish_left, int frame)
{
    int i, j;

    LOG("\nEntering CheckCollision()\n");

    for (i = 0; i < fishies; i++)
    {
        if ((fish_object[i].y >= tux_object.y - fish_sprite->frame[0]->h) &&
            (fish_object[i].x +
                 (fish_object[i].w - fish_sprite->frame[0]->w) / 2 >=
             tux_object.x) &&
            (fish_object[i].x +
                 (fish_object[i].w + fish_sprite->frame[0]->w) / 2 <=
             tux_object.x + tux_max_width))
        {

            if (fish_object[i].can_eat)
            {
                LOG("**EATING A FISHY** - in CheckCollision()\n");

                fish_object[i].alive   = 0;
                fish_object[i].can_eat = 0;

                for (j = 0; j < fish_object[i].len; j++)
                {
                    EraseSprite(
                        fish_sprite,
                        (fish_object[i].x + (j * fish_sprite->frame[0]->w)),
                        fish_object[i].y);
                }

                *fish_left = *fish_left - 1;

                tux_object.state = TUX_GULPING;
                REWIND(tux_object.spr[TUX_GULPING][tux_object.facing]);
                tux_object.dx   = 0;
                tux_object.endx = tux_object.x;

                if (settings.sys_sound)
                {
                    PlaySound(sound[BITE_WAV]);
                }
            }
            else if (tux_object.state == TUX_STANDING)
            {
                LOG("***EXCUSE ME!** - in CheckCollision()\n");

                /* Play "excuse me" once per fish — without the per-fish
                 * latch, every frame Tux stays under the same fish would
                 * retrigger it. */
                if (settings.sys_sound && !fish_object[i].excused)
                {
                    PlaySound(sound[EXCUSEME_WAV]);
                    fish_object[i].excused = 1;
                }
            }
        }
    }
    LOG("Leaving CheckCollision()\n");
}

static void next_tux_frame(void)
{
    if (tux_object.state != TUX_GULPING)
    {
        NEXT_FRAME(tux_object.spr[tux_object.state][tux_object.facing]);
    }
    else
    {
        NEXT_FRAME(tux_object.spr[TUX_GULPING][tux_object.facing]);
        if (tux_object.spr[TUX_GULPING][tux_object.facing]->cur == 0)
        {
            tux_object.state = TUX_STANDING;
        }
    }
}

/***********************************
MoveTux : Update Tux's location & then blit him!
************************************/
static void MoveTux(int frame, int fishies)
{
    int i;
    int which = -1, time_to_splat = 0;

    LOG("Entering MoveTux()\n");

    EraseSprite(tux_object.spr[tux_object.state][tux_object.facing],
                tux_object.x, tux_object.y);

    if (tux_object.state != TUX_GULPING)
    {
        for (i = 0; i < fishies; i++)
        {
            if (fish_object[i].can_eat &&
                (!time_to_splat || fish_object[i].splat_time < time_to_splat))
            {
                time_to_splat = fish_object[i].splat_time;
                which         = i;
            }
        }
        if (which != -1)
        {
            int endx = int_restrict(
                0,
                fish_object[which].x + (fish_object[which].w / 2) -
                    (tux_object.spr[TUX_GULPING][RIGHT]->frame[0]->w / 2),
                screen->w - tux_max_width - 1);
            if (endx != tux_object.endx)
            {
                tux_object.endx = endx;

                if (tux_object.endx >= tux_object.x)
                {
                    tux_object.facing = RIGHT;
                }
                else
                {
                    tux_object.facing = LEFT;
                }

                /* see how fast we need to go to get there... */
                if (time_to_splat - frame >
                    (fabs(tux_object.endx - tux_object.x) / WALKING_SPEED))
                {
                    tux_object.dx    = WALKING_SPEED;
                    tux_object.state = TUX_WALKING;

                    if (settings.sys_sound &&
                        T4K_IsPlayingSound(sound[RUN_WAV]))
                    {
                        T4K_StopSound(sound[RUN_WAV]);
                    }
                }
                else
                {
                    if (time_to_splat > frame)
                    {
                        tux_object.dx = float_restrict(
                            MIN_RUNNING_SPEED,
                            fabs(tux_object.endx - tux_object.x) /
                                (time_to_splat - frame),
                            MAX_RUNNING_SPEED);
                    }
                    else
                    {
                        tux_object.dx = MAX_RUNNING_SPEED;
                        if (settings.sys_sound &&
                            !T4K_IsPlayingSound(sound[RUN_WAV]))
                        {
                            if (fabs(tux_object.endx - tux_object.x) > 50)
                            {
                                PlaySound(sound[RUN_WAV]);
                            }
                        }
                    }

                    tux_object.state = TUX_RUNNING;
                }
            }
        }
    }

    /* --- move tux (if moving) --- */

    tux_object.x = float_restrict(
        0,
        tux_object.x +
            (tux_object.facing == RIGHT ? 1.0 : -1.0) * tux_object.dx,
        (screen->w - tux_max_width));

    /* if done with certain frames, then reset to standing */

    if (tux_object.state == TUX_WALKING || tux_object.state == TUX_RUNNING)
    {
        if ((tux_object.facing == RIGHT && tux_object.x >= tux_object.endx) ||
            (tux_object.facing == LEFT && tux_object.x <= tux_object.endx))
        {

            LOG("-In MoveTux(): returning tux to standing\n");
            tux_object.state = TUX_STANDING;
            tux_object.dx    = 0;
            tux_object.x     = tux_object.endx;
        }
    }
    //	if ((frame % 8) == 0) next_tux_frame();
    /* Changed from 8 to 3 because throttling frame rate down to 15 */
    if ((frame % 3) == 0)
    {
        next_tux_frame();
    }

    LOG("Leaving MoveTux()\n");
}

static void draw_bar(int curlevel, int diflevel, int curlives, int oldlives,
                     int fish_left, int oldfish_left)
{
    LOG("Entering draw_bar()\n");

    /* --- draw difficulty --- */
    T4K_DrawObject(level[diflevel], 1, 1);

    LOG("about to draw level()\n");

    /* --- draw level --- */
    T4K_DrawObject(curlev, 1 + GRAPHIC_SPACE + level[diflevel]->w, 1);
    DrawNumbers(curlevel + 1,
                1 + 2 * GRAPHIC_SPACE + level[diflevel]->w + curlev->w, 1, 0);

    LOG("about to draw lives()\n");

    /* --- draw lives --- */
    T4K_DrawObject(lives,
                   (screen->w) -
                       (1 + lives->w + fish->w +
                        ((MAX_FISHIES_DIGITS + 1) * 2 * number_max_w) +
                        GRAPHIC_SPACE),
                   1);

    if (oldlives != curlives)
    {
        EraseNumbers(oldlives,
                     (screen->w) -
                         (1 + fish->w +
                          ((MAX_FISHIES_DIGITS + 1) * 2 * number_max_w) +
                          GRAPHIC_SPACE),
                     1, 0);

        oldlives = curlives;
    }

    DrawNumbers(curlives,
                (screen->w) - (1 + fish->w +
                               ((MAX_FISHIES_DIGITS + 1) * 2 * number_max_w) +
                               GRAPHIC_SPACE),
                1, 0);

    LOG("about to draw fish left()\n");

    /* --- draw fish left --- */ /* Drawing text label "Fish" */
    T4K_DrawObject(
        fish, (screen->w) - (1 + fish->w + (MAX_FISHIES_DIGITS * number_max_w)),
        1);

    if (oldfish_left != fish_left)
    {
        LOG("about to erase numbers\n");

        EraseNumbers(oldfish_left,
                     (screen->w) - (1 + (MAX_FISHIES_DIGITS * number_max_w)), 1,
                     MAX_FISHIES_DIGITS);
        oldfish_left = fish_left;
    }

    LOG("about to draw numbers\n");

    DrawNumbers(fish_left,
                (screen->w) - (1 + (MAX_FISHIES_DIGITS * number[4]->w)), 1,
                MAX_FISHIES_DIGITS);

    LOG("Leaving draw_bar()\n");
}

/**********************************************************************
 * This function will announce the bottum most word's in the screen
 * when one starts typing, the remaining letters will be announced
 * till the word end's
 * *******************************************************************/
static int tts_announcer(void* struct_address)
{
    struct tts_announcer_cascade_data_struct struct_with_data_address =
        *((struct tts_announcer_cascade_data_struct*)(struct_address));
    int     fishies, i, j, iter;
    wchar_t buffer[3000];
    int     fish_object_positions[10];
    int     alive, temp;
    int     pitch_and_rate;
    int     which, correct_position;
    tts_announcer_switch = 1;
    int max;

    while (1)
    {
        // Converting and taking the value of fishies from void address
        // structure
        fishies = *struct_with_data_address.address_of_fishies;

        if (tts_announcer_switch == 0)
        {
            goto end;
        }
        else if (tts_announcer_switch == 2)
        {
            T4K_Tts_say(DEFAULT_VALUE, DEFAULT_VALUE, INTERRUPT,
                        "fish_left %d!", fish_left);
            SDL_Delay(20);
            T4K_Tts_wait();
            tts_announcer_switch = 1;
        }
        else if (tts_announcer_switch == 3)
        {
            T4K_Tts_say(DEFAULT_VALUE, DEFAULT_VALUE, INTERRUPT,
                        "%d lives remaining!", curlives);
            SDL_Delay(20);
            T4K_Tts_wait();
            tts_announcer_switch = 1;
        }

        // Wait to finish saying the previus word
        T4K_Tts_wait();

        // Checking the typed
        if (tux_object.wordlen == 0)
        {
            // Adding the iter of each fish wich are alive and not can_eat
            for (i = 0, j = 0; i < fishies; i++)
            {
                if (!fish_object[i].can_eat && fish_object[i].alive)
                {
                    fish_object_positions[j] = i;
                    j++;
                }
            }
            alive = j - 1;

            // Ordering the fish_object_positions with respect to splat time
            if (alive != 0)
            {
                for (i = alive; i > 0; i--)
                {
                    for (j = 0; j < i; j++)
                    {
                        // Comaparing fish_object[i].splat_time;
                        if (fish_object[fish_object_positions[j]].splat_time >
                            fish_object[fish_object_positions[j + 1]]
                                .splat_time)
                        {
                            temp = fish_object_positions[j];
                            fish_object_positions[j] =
                                fish_object_positions[j + 1];
                            fish_object_positions[j + 1] = temp;
                        }
                    }
                }
            }

            // We have to announce only last three otherwise it will make
            // confusion.
            if (alive >= 3)
            {
                max = 2;
            }
            else
            {
                max = alive;
            }

            // Using this corrected order to say each words and letters
            for (i = 0; i <= max; i++)
            {
                if (tts_announcer_switch == 0)
                {
                    goto end;
                }

                // Adding the word
                wcscpy(buffer, fish_object[fish_object_positions[i]].word);
                iter = wcslen(fish_object[fish_object_positions[i]].word);
                buffer[iter] = L'.';
                iter++;
                buffer[iter] = L' ';
                iter++;

                // Appending letters if word is not alphabet
                if (1 < wcslen(fish_object[fish_object_positions[i]].word))
                {
                    for (j = 0;
                         j < wcslen(fish_object[fish_object_positions[i]].word);
                         j++)
                    {
                        buffer[iter] =
                            fish_object[fish_object_positions[i]].word[j];
                        iter++;
                        buffer[iter] = L'.';
                        iter++;
                        buffer[iter] = L' ';
                        iter++;
                    }
                }
                // If not ended with '\0' it will say grabage values also
                buffer[iter] = L'\0';

                // Setting the pitch and rate with respect to y axis
                pitch_and_rate =
                    ((fish_object[fish_object_positions[i]].y * 100) /
                     (screen->h - fish_sprite->frame[0]->h));

                if (pitch_and_rate < 30)
                {
                    pitch_and_rate = 30;
                }

                if (pitch_and_rate > 60)
                {
                    pitch_and_rate = 60;
                }

                T4K_Tts_say(pitch_and_rate, pitch_and_rate, INTERRUPT, "%S",
                            buffer);

                SDL_WaitThread(tts_thread, NULL);
                SDL_Delay(100);
            }
        }
        else
        {
            // Detecting the corrent typing fish
            which = -1;
            for (i = 0; i < fishies; i++)
            {
                // Comaparing fish_object[i].splat_time;
                if (!fish_object[i].can_eat && fish_object[i].alive)
                {
                    j = wcsncmp(fish_object[i].word, tux_object.word,
                                tux_object.wordlen);
                    if (j == 0)
                    {
                        which = i;
                    }
                }
            }

            if (which != -1)
            {
                // Adding the word
                wcscpy(buffer, fish_object[which].word);
                iter         = wcslen(fish_object[which].word);
                buffer[iter] = L'.';
                iter++;
                buffer[iter] = L' ';
                iter++;

                // Adding the remaining letters to be announced.
                // This is as per my PAPA's suggestion (sathyan)
                // Eg : "BLUE. B. L. U. E"

                // Detecting the correct_position
                correct_position = 0;
                for (j = 0; j < tux_object.wordlen; j++)
                {
                    if (tux_object.word[j] == fish_object[which].word[j])
                    {
                        correct_position += 1;
                    }
                    else
                    {
                        tux_object.wordlen = 0;
                        tux_object.word[0] = L'\0';
                        break;
                    }
                }
                // Appending each letters from correct_position if word is not
                // alphabet
                if (1 < wcslen(fish_object[which].word))
                {
                    for (j = correct_position;
                         j < wcslen(fish_object[which].word); j++)
                    {
                        // Skipping if the letter is in orange color. if not it
                        // will be appended
                        buffer[iter] = fish_object[which].word[j];
                        iter++;
                        buffer[iter] = L'.';
                        iter++;
                        buffer[iter] = L' ';
                        iter++;
                    }
                }
                // If not ended with '\0' it will say grabage values also
                buffer[iter] = L'\0';

                // Setting the pitch and rate with respect to y axis
                pitch_and_rate = ((fish_object[which].y * 100) /
                                  (screen->h - fish_sprite->frame[0]->h));

                if (pitch_and_rate < 30)
                {
                    pitch_and_rate = 30;
                }

                if (pitch_and_rate > 60)
                {
                    pitch_and_rate = 60;
                }

                T4K_Tts_say(pitch_and_rate, pitch_and_rate, INTERRUPT, "%S",
                            buffer);
                SDL_WaitThread(tts_thread, NULL);
                SDL_Delay(100);
                fprintf(stderr, "\nBraille_Letter_Pos = %d",
                        braille_letter_pos);
            }
            else
            {
                tux_object.wordlen = 0;
                tux_object.word[0] = L'\0';
            }
        }
    }
end:
    return 0;
}

/********** Stop annoncing thread safely *********/
static void stop_tts_announcer()
{
    tts_announcer_switch = 0;
}

/*****************************************************************
 * Set the Braille letter position
 * For some specific language's which have same braille code for
 * alphabets and signs at begining, middle and end position.
 ****************************************************************/
static void set_braille_letter_pos(int fishies)
{
    int which, i;

    if (tux_object.wordlen == 0)
    {
        braille_letter_pos = 0;
    }
    else
    {
        // Detecting the corrent typing fish
        which = -1;
        for (i = 0; i < fishies; i++)
        {
            // Comaparing fish_object[i].splat_time;
            if (!fish_object[i].can_eat && fish_object[i].alive)
            {
                if (0 == wcsncmp(fish_object[i].word, tux_object.word,
                                 tux_object.wordlen))
                {
                    which = i;
                }
            }
        }
        if (which != -1)
        {
            // Braille letter Position should be 2 if next letter is end
            if (tux_object.wordlen == fish_object[which].len - 1)
            {
                braille_letter_pos = 2;
            }
            else
            {
                braille_letter_pos = 1;
            }
        }
        else
        {
            braille_letter_pos = 0;
        }
    }
}
