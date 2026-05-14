/**************************************************************************
practice.c
-  description: practice module
-------------------
begin                : Friday Jan 25, 2003
copyright            : (C) 2003 by Jesse Andrews
email                : jdandr2@uky.edu

Revised extensively: 2007 and 2008
David Bruce <davidstuartbruce@gmail.com>
Revised extensively: 2008
Sreyas Kurumanghat <k.sreyas@gmail.com>
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "globals.h"
#include "funcs.h"
#include "SDL_extras.h"
#include "hand_display.h"
#include "input.h"
#include "keyboard_display.h"
#include "braille.h"
#include <t4k/visible_bell.h>
#include <ctype.h>
#include <wctype.h>

#define MAX_PHRASES 256
#define MAX_PHRASE_LENGTH 256
#define MAX_WRAP_LINES 10
#define TEXT_HEIGHT 28
#define SPRITE_FRAME_TIME 200
/* "Local globals" for practice.c */
static int fontsize    = 0;
static int medfontsize = 0;
static int bigfontsize = 0;

/* Surfaces for things we want to pre-render: */
static HandDisplay  practice_hands;
static KbdDisplay   practice_keyboard;
static Input*       practice_input      = NULL;
static sprite*      tux_stand           = NULL;
static sprite*      tux_win             = NULL;
static SDL_Surface* time_label_srfc     = NULL;
static SDL_Surface* chars_label_srfc    = NULL;
static SDL_Surface* cpm_label_srfc      = NULL;
static SDL_Surface* wpm_label_srfc      = NULL;
static SDL_Surface* errors_label_srfc   = NULL;
static SDL_Surface* accuracy_label_srfc = NULL;

static wchar_t    phrases[MAX_PHRASES][MAX_PHRASE_LENGTH];
static Mix_Chunk* cheer  = NULL;
static Mix_Chunk* snd_ok = NULL;

/* Visual feedback for wrong keys — full-screen white flash that fades.
 * Replaces the abrasive buzz.wav that used to fire on every miss. */
static T4K_VisibleBell s_wrong_bell;

static int phrase_draw_width = 0; /* How wide before text needs wrapping */
static int num_phrases       = 0;

/* Locations for blitting  */

/* Three main areas within window: */
static SDL_Rect left_pane;
static SDL_Rect top_pane;
static SDL_Rect bottom_pane;

/* Locations within left pane: */
static SDL_Rect tux_loc;
static SDL_Rect time_label;
static SDL_Rect time_rect;
static SDL_Rect chars_typed_label;
static SDL_Rect chars_typed_rect;
static SDL_Rect cpm_label;
static SDL_Rect cpm_rect;
static SDL_Rect wpm_label;
static SDL_Rect wpm_rect;
static SDL_Rect errors_label;
static SDL_Rect errors_rect;
static SDL_Rect accuracy_label;
static SDL_Rect accuracy_rect;

/* Locations within top pane: */
static SDL_Rect phr_text_rect;
static SDL_Rect user_text_rect;

/* Locations within bottom pane: */
static SDL_Rect hand_loc;
static SDL_Rect nextletter_rect; /* Latin preview, always shown */
static SDL_Rect
    altglyph_rect; /* input-mode encoding glyph, non-keyboard modes */
// static SDL_Rect letter_loc;
static SDL_Rect keyboard_loc;

/*local function prototypes: */
static int  load_phrases(const char* phrase_file);
static int  find_next_wrap(const wchar_t* wstr, int font_size, int width);
static void recalc_positions(void);
static void calc_font_sizes(void);
static int  create_labels(void);
static void display_next_letter(const wchar_t* str, Uint16 index);
static int  practice_load_media(void);
static void practice_unload_media(void);

/* Fixed-width cell-row rendering. Lets the prompt's Latin row line up
 * cell-by-cell with the echo's mode-specific glyphs by sourcing the
 * per-char slot width from Input_CellsForChar. Each returns the cell
 * count consumed from wstr[0..len). */
static int draw_cell_row_prompt(const wchar_t* wstr, int len, SDL_Rect rect);
static int draw_cell_row_echo(const wchar_t* wstr, int len, SDL_Rect rect);
static int cell_width_for_row(int font_size);
// static void show(char t);
static void print_load_results(void);
wchar_t* get_next_word_letters(int cur_phrase, int cursor, int till_next_space);
wchar_t* get_next_word(int cur_phrase, int cursor);

// Braille Variable
int braille_letter_pos;

/************************************************************************/
/*                                                                      */
/*         "Public" functions (callable throughout program)             */
/*                                                                      */
/************************************************************************/

/*  --------  This is the main function for the 'Practice' activity. ------ */
int Phrases(wchar_t* pphrase)
{
    Uint32       last_keypress_ms = 0, a = 0, tuxtime = 0;
    int          quit                        = 0;
    int          i                           = 0;
    int          cursor                      = 0;
    int          scroll_offset_ch            = 0;
    int          total                       = 0;
    int          phrase_just_changed         = 1;
    int          once_only                   = 0;
    int          correct_chars               = 0;
    int          wrong_chars                 = 0;
    float        accuracy                    = 0;
    int          cur_phrase                  = 0;
    int    keytimes[MAX_PHRASE_LENGTH] = {0};
    char         time_str[20]                = "";
    char         chars_typed_str[20]         = "";
    char         cpm_str[20]                 = "";
    char         wpm_str[20]                 = "";
    char         errors_str[20]              = "";
    char         accuracy_str[20]            = "";
    SDL_Surface* tmpsurf                     = NULL;

    /* Per-game word-position tracking lives in the file-scope
     * braille_letter_pos; the input decoder pulls it via
     * Input_SetWordPosition() before each Consume(). */
    braille_letter_pos = 0;
    Input_Free(practice_input);
    practice_input =
        (settings.input_mode == INPUT_BRAILLE)
            ? Input_NewBraille(&practice_hands, &practice_keyboard)
            : Input_NewTypewriter(&practice_hands, &practice_keyboard);
    Input_SetKbdVisible(practice_input, settings.show_keyboard);

    /* Load all needed graphics, strings, sounds.... */
    if (!practice_load_media())
    {
        fprintf(stderr,
                "Phrases() - practice_load_media() failed, returning.\n\n");
        return 0;
    }

    /* Enable text input so dead-key composition (e.g. macOS Option+U U → ü)
     * reaches us as SDL_EVENT_TEXT_INPUT. SDL3 never delivers composed glyphs
     * via SDL_EVENT_KEY_DOWN — that only carries raw keycodes. */
    SDL_StartTextInput(tt_window);

    /* If we got a phrase string arg, use it, otherwise we */
    /* load practice phrases from  the default file:       */
    if (pphrase != NULL && wcslen(pphrase) > 0)
    {
        wcsncpy(phrases[0], pphrase, MAX_PHRASE_LENGTH);
        num_phrases = 1;
        once_only   = 1;
    }
    else
    {
        num_phrases = load_phrases("phrases.txt");
    }
    /* Set up positions for blitting: */
    recalc_positions();

    last_keypress_ms = tuxtime = SDL_GetTicks();
    /* Begin main event loop for "Practice" activity. Every frame
     * blits the full background and redraws every element from
     * scratch. Hint flashing keys off elapsed time since the last
     * keypress. */
    do
    {
        if (T4K_QuitConfirmed())
        {
            quit = 1;
            break;
        }

        /* --- Phrase reset --- */
        if (phrase_just_changed)
        {
            for (i = 0; i < MAX_PHRASE_LENGTH; i++)
            {
                keytimes[i] = 0;
            }
            time_str[0]         = '\0';
            chars_typed_str[0]  = '\0';
            cpm_str[0]          = '\0';
            wpm_str[0]          = '\0';
            errors_str[0]       = '\0';
            accuracy_str[0]     = '\0';
            total               = 0;
            cursor              = 0;
            scroll_offset_ch    = 0;
            correct_chars       = 0;
            wrong_chars         = 0;
            last_keypress_ms    = SDL_GetTicks();
            phrase_just_changed = 0;
            Input_Reset(practice_input);

            if (pphrase == NULL)
            {
                T4K_Tts_say(DEFAULT_VALUE, DEFAULT_VALUE, INTERRUPT, "%S %S %S",
                            phrases[cur_phrase],
                            get_next_word(cur_phrase, cursor),
                            get_next_word_letters(cur_phrase, cursor, 1));
            }
            else
            {
                T4K_Tts_say(DEFAULT_VALUE, DEFAULT_VALUE, INTERRUPT, "%S",
                            get_next_word_letters(cur_phrase, cursor, 1));
            }
        }

        /* --- Scroll window. Window starts at scroll_offset_ch. Once
         * cursor's visual offset exceeds midpoint, slide
         * scroll_offset_ch forward to pin cursor to middle column.
         * visible_chars = chars fitting from scroll_offset_ch —
         * cell-counted in braille, pixel-measured in keyboard. */
        int cell_w        = cell_width_for_row(medfontsize);
        int visible_chars = 0;
        if (settings.input_mode == INPUT_BRAILLE)
        {
            int phr_cells_cap  = phr_text_rect.w / cell_w;
            int midpoint_cells = phr_cells_cap / 2;
            /* Cursor cell offset within window. */
            int cursor_cells = 0;
            for (int j = scroll_offset_ch; j < cursor; j++)
            {
                cursor_cells +=
                    Input_CellsForChar(practice_input, phrases[cur_phrase][j]);
            }
            /* Slide forward until cursor pulls back to midpoint. */
            while (cursor_cells > midpoint_cells && scroll_offset_ch < cursor)
            {
                cursor_cells -= Input_CellsForChar(
                    practice_input, phrases[cur_phrase][scroll_offset_ch]);
                scroll_offset_ch++;
            }
            /* Chars fitting from scroll_offset_ch. */
            int cells_used = 0;
            int j          = 0;
            for (; phrases[cur_phrase][scroll_offset_ch + j] &&
                   j < MAX_PHRASE_LENGTH;
                 j++)
            {
                int n = Input_CellsForChar(
                    practice_input, phrases[cur_phrase][scroll_offset_ch + j]);
                if (cells_used + n > phr_cells_cap)
                {
                    break;
                }
                cells_used += n;
            }
            visible_chars = j;
        }
        else
        {
            visible_chars =
                find_next_wrap(&phrases[cur_phrase][scroll_offset_ch],
                               medfontsize, phrase_draw_width) +
                1;
            int midpoint_chars = visible_chars / 2;
            while ((cursor - scroll_offset_ch) > midpoint_chars &&
                   scroll_offset_ch < cursor)
            {
                scroll_offset_ch++;
                visible_chars =
                    find_next_wrap(&phrases[cur_phrase][scroll_offset_ch],
                                   medfontsize, phrase_draw_width) +
                    1;
                midpoint_chars = visible_chars / 2;
            }
        }

        /* --- Full redraw: background + every element --- */
        SDL_BlitSurface(CurrentBkgd(), NULL, screen, NULL);
        SDL_BlitSurface(tux_stand->frame[tux_stand->cur], NULL, screen,
                        &tux_loc);
        SDL_BlitSurface(time_label_srfc, NULL, screen, &time_label);
        SDL_BlitSurface(chars_label_srfc, NULL, screen, &chars_typed_label);
        SDL_BlitSurface(cpm_label_srfc, NULL, screen, &cpm_label);
        SDL_BlitSurface(wpm_label_srfc, NULL, screen, &wpm_label);
        SDL_BlitSurface(errors_label_srfc, NULL, screen, &errors_label);
        SDL_BlitSurface(accuracy_label_srfc, NULL, screen, &accuracy_label);

        /* Prompt + echo + caret. Both start at scroll_offset_ch, run
         * for visible_chars; echo clipped at cursor. */
        int typed_w = 0;
        if (settings.input_mode == INPUT_BRAILLE)
        {
            draw_cell_row_prompt(&phrases[cur_phrase][scroll_offset_ch],
                                 visible_chars, phr_text_rect);
            int echo_chars = cursor - scroll_offset_ch;
            if (echo_chars > visible_chars)
            {
                echo_chars = visible_chars;
            }
            int typed_cells =
                draw_cell_row_echo(&phrases[cur_phrase][scroll_offset_ch],
                                   echo_chars, user_text_rect);
            int pending_cells = Input_DrawPendingEcho(
                practice_input, user_text_rect.x + typed_cells * cell_w,
                user_text_rect.y, cell_w, user_text_rect.h, screen);
            typed_w = (typed_cells + pending_cells) * cell_w;
        }
        else
        {
            tmpsurf = BlackOutline_w(&phrases[cur_phrase][scroll_offset_ch],
                                     medfontsize, &white, visible_chars);
            if (tmpsurf)
            {
                SDL_Rect src = {0, 0,
                                (tmpsurf->w > phr_text_rect.w) ? phr_text_rect.w
                                                               : tmpsurf->w,
                                tmpsurf->h};
                SDL_BlitSurface(tmpsurf, &src, screen, &phr_text_rect);
                SDL_DestroySurface(tmpsurf);
                tmpsurf = NULL;
            }
            int echo_chars = cursor - scroll_offset_ch;
            if (echo_chars > visible_chars)
            {
                echo_chars = visible_chars;
            }
            tmpsurf = BlackOutline_w(&phrases[cur_phrase][scroll_offset_ch],
                                     medfontsize, &white, echo_chars);
            if (tmpsurf)
            {
                SDL_Rect src = {0, 0,
                                (tmpsurf->w > user_text_rect.w)
                                    ? user_text_rect.w
                                    : tmpsurf->w,
                                tmpsurf->h};
                SDL_BlitSurface(tmpsurf, &src, screen, &user_text_rect);
                typed_w = src.w;
                SDL_DestroySurface(tmpsurf);
                tmpsurf = NULL;
            }
        }
        {
            SDL_Rect cur = {user_text_rect.x + typed_w,
                            user_text_rect.y + user_text_rect.h - 4,
                            medfontsize / 2, 2};
            if (cur.x + cur.w > user_text_rect.x + user_text_rect.w)
            {
                cur.w = user_text_rect.x + user_text_rect.w - cur.x;
            }
            if (cur.w > 0)
            {
                SDL_Rect shadow = {cur.x + 1, cur.y + 1, cur.w, cur.h};
                const SDL_PixelFormatDetails* pf =
                    SDL_GetPixelFormatDetails(screen->format);
                SDL_FillSurfaceRect(screen, &shadow,
                                    SDL_MapRGB(pf, NULL, 0, 0, 0));
                SDL_FillSurfaceRect(screen, &cur,
                                    SDL_MapRGB(pf, NULL, 255, 255, 255));
            }
        }

        /* Stat values */
        tmpsurf = T4K_BlackOutline(time_str, fontsize, &white);
        if (tmpsurf)
        {
            SDL_BlitSurface(tmpsurf, NULL, screen, &time_rect);
            SDL_DestroySurface(tmpsurf);
            tmpsurf = NULL;
        }
        tmpsurf = T4K_BlackOutline(chars_typed_str, fontsize, &white);
        if (tmpsurf)
        {
            SDL_BlitSurface(tmpsurf, NULL, screen, &chars_typed_rect);
            SDL_DestroySurface(tmpsurf);
            tmpsurf = NULL;
        }
        tmpsurf = T4K_BlackOutline(cpm_str, fontsize, &white);
        if (tmpsurf)
        {
            SDL_BlitSurface(tmpsurf, NULL, screen, &cpm_rect);
            SDL_DestroySurface(tmpsurf);
            tmpsurf = NULL;
        }
        tmpsurf = T4K_BlackOutline(wpm_str, fontsize, &white);
        if (tmpsurf)
        {
            SDL_BlitSurface(tmpsurf, NULL, screen, &wpm_rect);
            SDL_DestroySurface(tmpsurf);
            tmpsurf = NULL;
        }
        tmpsurf = T4K_BlackOutline(errors_str, fontsize, &white);
        if (tmpsurf)
        {
            SDL_BlitSurface(tmpsurf, NULL, screen, &errors_rect);
            SDL_DestroySurface(tmpsurf);
            tmpsurf = NULL;
        }
        tmpsurf = T4K_BlackOutline(accuracy_str, fontsize, &white);
        if (tmpsurf)
        {
            SDL_BlitSurface(tmpsurf, NULL, screen, &accuracy_rect);
            SDL_DestroySurface(tmpsurf);
            tmpsurf = NULL;
        }

        /* Hand + keyboard base */
        Hand_Display_DrawBase(&practice_hands, screen);
        if (settings.show_keyboard)
        {
            Kbd_Display_DrawBase(&practice_keyboard, screen);
        }

        /* Hint overlay — time-based: solid hint after 500 ms idle,
         * flashing (~5 Hz) after 750 ms. */
        {
            Uint32 since_press = SDL_GetTicks() - last_keypress_ms;
            int    show_hint   = 0;
            if (since_press > 500)
            {
                if (since_press < 750)
                {
                    show_hint = 1;
                }
                else
                {
                    show_hint = ((since_press - 750) / 100) % 2 == 0;
                }
            }
            if (show_hint)
            {
                wchar_t target     = phrases[cur_phrase][cursor];
                int     pos        = Braille_PositionForChar(target);
                braille_letter_pos = (pos >= 0) ? pos : 0;
                Input_SetWordPosition(practice_input, braille_letter_pos);
                Input_DrawHint(practice_input, target, screen);
            }
        }

        /* Tux animation tick */
        if ((SDL_GetTicks() - tuxtime) > SPRITE_FRAME_TIME)
        {
            tuxtime = SDL_GetTicks();
            NEXT_FRAME(tux_stand);
        }

        /* Next-letter preview (Latin + encoding) */
        display_next_letter(phrases[cur_phrase], cursor);

        /* Visible bell overlay (wrong-key flash) */
        T4K_VisibleBell_Render(&s_wrong_bell, screen);

        /* --- Event handling --- */
        while (SDL_PollEvent(&event))
        {
            int handled_control = 0;
            if (event.type == SDL_EVENT_KEY_DOWN)
            {
                switch (event.key.key)
                {
                case SDLK_ESCAPE:
                    if (Pause() == 1)
                    {
                        quit = 1;
                    }
                    handled_control = 1;
                    break;
                case SDLK_F10:
                    T4K_SwitchScreenMode();
                    recalc_positions();
                    create_labels();
                    handled_control = 1;
                    break;
                case SDLK_DOWN:
                    if (cur_phrase < num_phrases - 1)
                    {
                        cur_phrase++;
                        phrase_just_changed = 1;
                    }
                    handled_control = 1;
                    break;
                case SDLK_UP:
                    if (cur_phrase > 0)
                    {
                        cur_phrase--;
                        phrase_just_changed = 1;
                    }
                    handled_control = 1;
                    break;
                default:
                    break;
                }
            }

            Input_SetWordPosition(practice_input, braille_letter_pos);
            InputToken typed = Input_Consume(practice_input, &event);
            if (!typed.ready || handled_control)
            {
                continue;
            }

            wchar_t typed_ch = typed.ch;
            int     key      = GetIndex(typed_ch);

            if (key != -1)
            {
                updatekeylist(key, typed_ch);
            }

            a                = SDL_GetTicks();
            keytimes[cursor] = a - last_keypress_ms;
            last_keypress_ms = a;
            total += keytimes[cursor];
            sprintf(time_str, "%.2f %s", (float)total / 1000, N_("sec"));
            sprintf(chars_typed_str, "%d", correct_chars);
            sprintf(cpm_str, "%.1f",
                    (float)correct_chars / ((float)total / 60000));
            sprintf(wpm_str, "%.1f",
                    (float)((float)correct_chars / 5) / ((float)total / 60000));
            sprintf(errors_str, "%d", wrong_chars);
            if (correct_chars + wrong_chars == 0)
            {
                accuracy = 1;
            }
            else
            {
                accuracy = (float)correct_chars /
                           ((float)(correct_chars + wrong_chars));
            }
            sprintf(accuracy_str, "%.1f%%", accuracy * 100);

            if (phrases[cur_phrase][cursor] == typed_ch)
            {
                cursor++;
                correct_chars++;

                if (phrases[cur_phrase][cursor - 1] == L' ')
                {
                    if (pphrase == NULL)
                    {
                        T4K_Tts_say(
                            DEFAULT_VALUE, DEFAULT_VALUE, INTERRUPT, "%S %S",
                            get_next_word(cur_phrase, cursor),
                            get_next_word_letters(cur_phrase, cursor, 1));
                    }
                    else
                    {
                        T4K_Tts_say(
                            DEFAULT_VALUE, DEFAULT_VALUE, INTERRUPT, "%S",
                            get_next_word_letters(cur_phrase, cursor, 1));
                    }
                }
                else
                {
                    PlaySound(snd_ok);
                }
                if (phrases[cur_phrase][cursor - 1] != L' ')
                {
                    T4K_Tts_say(DEFAULT_VALUE, DEFAULT_VALUE, INTERRUPT, "%S",
                                get_next_word_letters(cur_phrase, cursor, 1));
                }

                /* Phrase complete → celebrate. */
                if (cursor == wcslen(phrases[cur_phrase]))
                {
                    T4K_Tts_say(
                        DEFAULT_VALUE, DEFAULT_VALUE, INTERRUPT,
                        gettext(
                            "Wow. you completed sentence with %s characters in \
				      %s time, your speed is %s word per minut and \
				      percentage of accuracy is %s"),
                        chars_typed_str, time_str, wpm_str, accuracy_str);

                    Uint32       cel_start    = SDL_GetTicks();
                    const Uint32 CELEBRATE_MS = 1500;
                    int          done         = 0;

                    PlaySound(cheer);

                    /* Drain queued input so the keystroke that
                     * completed the phrase doesn't immediately dismiss
                     * the celebration. */
                    while (SDL_PollEvent(&event))
                    { /* discard */
                    }

                    SDL_BlitSurface(CurrentBkgd(), NULL, screen, NULL);
                    SDL_BlitSurface(time_label_srfc, NULL, screen, &time_label);
                    SDL_BlitSurface(chars_label_srfc, NULL, screen,
                                    &chars_typed_label);
                    SDL_BlitSurface(cpm_label_srfc, NULL, screen, &cpm_label);
                    SDL_BlitSurface(wpm_label_srfc, NULL, screen, &wpm_label);
                    SDL_BlitSurface(errors_label_srfc, NULL, screen,
                                    &errors_label);
                    SDL_BlitSurface(accuracy_label_srfc, NULL, screen,
                                    &accuracy_label);

                    SDL_Surface* banner = T4K_BlackOutline(
                        gettext("Done!"), bigfontsize, &yellow);
                    if (banner)
                    {
                        SDL_Rect br;
                        br.x = (screen->w - banner->w) / 2;
                        br.y = (screen->h - banner->h) / 2 - 40;
                        br.w = banner->w;
                        br.h = banner->h;
                        SDL_BlitSurface(banner, NULL, screen, &br);
                        SDL_DestroySurface(banner);
                    }

                    while (!done)
                    {
                        if (T4K_QuitConfirmed())
                        {
                            quit = 1;
                            done = 1;
                            break;
                        }
                        while (SDL_PollEvent(&event))
                        {
                            if ((event.type == SDL_EVENT_KEY_DOWN) ||
                                (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN))
                            {
                                done = 1;
                            }
                        }
                        if (SDL_GetTicks() - cel_start >= CELEBRATE_MS)
                        {
                            done = 1;
                        }

                        SDL_BlitSurface(CurrentBkgd(), &tux_loc, screen,
                                        &tux_loc);
                        if (tux_win && tux_win->frame[tux_win->cur])
                        {
                            SDL_BlitSurface(tux_win->frame[tux_win->cur], NULL,
                                            screen, &tux_loc);
                        }
                        T4K_UpdateRect(screen, NULL);
                        NEXT_FRAME(tux_win);
                        SDL_Delay(80);
                    }

                    if (once_only)
                    {
                        quit = 1;
                    }
                    else
                    {
                        if (cur_phrase < num_phrases)
                        {
                            cur_phrase++;
                        }
                        else
                        {
                            cur_phrase = 0;
                        }
                    }
                    phrase_just_changed = 1;
                }
            }
            else
            {
                /* Wrong key */
                if (-1 != key && settings.show_keyboard)
                {
                    Kbd_Display_DrawRedKey(&practice_keyboard, key, screen);
                }
                if (event.key.key != SDLK_RSHIFT &&
                    event.key.key != SDLK_LSHIFT)
                {
                    {
                        wrong_chars++;
                        T4K_VisibleBell_Trigger(&s_wrong_bell);

                        if (settings.input_mode != INPUT_BRAILLE)
                        {
                            T4K_Tts_say(
                                DEFAULT_VALUE, DEFAULT_VALUE, INTERRUPT,
                                "Type %S",
                                get_next_word_letters(cur_phrase, cursor, 1));
                        }
                        else
                        {
                            wchar_t dots[6];
                            int     n = Braille_DotsForChar(
                                phrases[cur_phrase][cursor], dots);
                            if (n > 0)
                            {
                                static const wchar_t dot_to_num[128] = {
                                    [L'f'] = L'1', [L'd'] = L'2',
                                    [L's'] = L'3', [L'j'] = L'4',
                                    [L'k'] = L'5', [L'l'] = L'6'};
                                wchar_t tts_temp[64];
                                int     len = 0;
                                for (int j2 = 0; j2 < n; j2++)
                                {
                                    tts_temp[len++] =
                                        dot_to_num[dots[j2] & 0x7f];
                                    tts_temp[len++] = L' ';
                                    tts_temp[len++] = L',';
                                }
                                tts_temp[len] = L'\0';
                                T4K_Tts_say(DEFAULT_VALUE, DEFAULT_VALUE,
                                            INTERRUPT, "Type %S with dot %S",
                                            get_next_word_letters(cur_phrase,
                                                                  cursor, 0),
                                            tts_temp);
                            }
                        }
                    }
                }
            }
        } /* end SDL_PollEvent */

        T4K_UpdateRect(screen, NULL);
        SDL_Delay(30);

    } while (!quit); /* ------- End of main event loop ------------- */

    SDL_StopTextInput(tt_window);

    savekeyboard();

    practice_unload_media();
    Input_Free(practice_input);
    practice_input = NULL;

    return quit;
}

/************************************************************************/
/*                                                                      */
/*       "Private" functions (local to practice.c)                      */
/*                                                                      */
/************************************************************************/

/* Select appropriate font sizes based on the screen we're working with: */
static void calc_font_sizes(void)
{
    fontsize    = (screen->h) / 28;
    medfontsize = fontsize * 1.5;
    bigfontsize = fontsize * 4;
    if (bigfontsize > 80)
    {
        bigfontsize = 80;
    }
}

static int practice_load_media(void)
{
    int load_failed = 0;
    int labels_ok   = 0;

    DEBUGCODE
    {
        printf("Entering practice_load_media\n");
    }

    /* load needed SDL_Surfaces: */
    LoadBothBkgds("main_bkg.png");
    Hand_Display_Init(&practice_hands);
    if (!Hand_Display_Load(&practice_hands))
    {
        load_failed = 1;
    }

    /* load tux sprites: */
    tux_win   = LoadSprite("tux/win", IMG_ALPHA);
    tux_stand = LoadSprite("tux/stand", IMG_ALPHA);
    /* load needed sounds: */
    cheer  = LoadSound("cheer.wav");
    snd_ok = LoadSound("tock.wav");

    /* Wrong-key visual flash; defaults: 200ms duration, peak alpha 110. */
    T4K_VisibleBell_Init(&s_wrong_bell, 0, 0);

    /* load needed fonts: */
    calc_font_sizes();
    RenderLetters(fontsize);
    Kbd_Display_Load(&practice_keyboard, 255, 0);

    /* create labels: */
    labels_ok = create_labels();

    /* Get out if anything failed to load (except sounds): */
    if (load_failed || !practice_hands.base || !CurrentBkgd() || !tux_win ||
        !tux_stand || !practice_keyboard.base || !labels_ok)
    {
        fprintf(stderr,
                "practice_load_media() - failed to load needed media \n");
        print_load_results();
        return 0;
    }

    LOG("DONE - Loading practice media\n");
    DEBUGCODE
    {
        printf("Leaving practice_load_media\n");
    }
    return 1;
}

static void print_load_results(void)
{
    LOG("\npractice - print_load_results:\n");
    /* Get out if anything failed to load: */
    if (!CurrentBkgd())
    {
        LOG("CurrentBkgd() did not load\n");
    }
    if (!practice_hands.base)
    {
        LOG("hand display did not load\n");
    }
    if (!tux_win)
    {
        LOG("tux_win did not load\n");
    }
    if (!tux_stand)
    {
        LOG("tux_stand did not load\n");
    }
    if (!practice_keyboard.base)
    {
        LOG("keyboard did not load\n");
    }
    if (!time_label_srfc)
    {
        LOG("time_label_srfc did not load\n");
    }
    if (!chars_label_srfc)
    {
        LOG("chars_label_srfc did not load\n");
    }
    if (!cpm_label_srfc)
    {
        LOG("cpm_label_srfc did not load\n");
    }
    if (!wpm_label_srfc)
    {
        LOG("wpm_label_srfc did not load\n");
    }
    if (!time_label_srfc)
    {
        LOG("time_label_srfc did not load\n");
    }
    if (!errors_label_srfc)
    {
        LOG("errors_label_srfc did not load\n");
    }
    if (!accuracy_label_srfc)
    {
        LOG("accuracy_label_srfc did not load\n");
    }

    LOG("End print_load_results()\n\n");
}

static void recalc_positions(void)
{
    int text_height;
    calc_font_sizes();
    text_height = fontsize * 1.5;

    DEBUGCODE
    {
        fprintf(stderr, "Entering recalc_positions(), screen is %d x %d\n",
                screen->w, screen->h);
    }

    if (!practice_keyboard.base || !tux_win || !tux_win->frame[0] ||
        !practice_hands.base)
    {
        fprintf(stderr,
                "recalc_positions() - needed ptr invalid - returning\n");
    }

    /* Screen is divided into three areas or 'panes' :

        +-----------------------------------+
        |        |                          |
        | left   |           top            |
        |        |                          |
        |        *--------------------------+
        |        |                          |
        |        |                          |
        |        |          bottom          |
        |        |                          |
        |        |                          |
        +-----------------------------------+
    */

    left_pane.x = 0;
    left_pane.y = 0;
    left_pane.w = screen->w * 0.25;
    left_pane.h = screen->h;

    top_pane.x = screen->w * 0.25;
    top_pane.y = 0;
    top_pane.w = screen->w * 0.75;
    top_pane.h = screen->h * 0.4;

    bottom_pane.x = screen->w * 0.25;
    bottom_pane.y = screen->h * 0.4;
    bottom_pane.w = screen->w * 0.75;
    bottom_pane.h = screen->h * 0.6;

    /* Set up all the locations within the left pane: */
    tux_loc.x = left_pane.x + 5;
    tux_loc.y = left_pane.y + 5;
    tux_loc.w = tux_stand->frame[0]->w;
    tux_loc.h = tux_stand->frame[0]->h;

    time_label.x = left_pane.x + 5;
    time_label.y = tux_loc.y + tux_loc.h;
    time_label.w = left_pane.w - 5;
    time_label.h = text_height;

    time_rect.x = left_pane.x + 5;
    time_rect.y = time_label.y + time_label.h;
    time_rect.w = left_pane.w - 5;
    time_rect.h = text_height;

    chars_typed_label.x = left_pane.x + 5;
    chars_typed_label.y = time_rect.y + time_rect.h;
    chars_typed_label.w = left_pane.w - 5;
    chars_typed_label.h = text_height;

    chars_typed_rect.x = left_pane.x + 5;
    chars_typed_rect.y = chars_typed_label.y + chars_typed_label.h;
    chars_typed_rect.w = left_pane.w - 5;
    chars_typed_rect.h = text_height;

    cpm_label.x = left_pane.x + 5;
    cpm_label.y = chars_typed_rect.y + chars_typed_rect.h;
    cpm_label.w = left_pane.w - 5;
    cpm_label.h = text_height;

    cpm_rect.x = left_pane.x + 5;
    cpm_rect.y = cpm_label.y + cpm_label.h;
    cpm_rect.w = left_pane.w - 5;
    cpm_rect.h = text_height;

    wpm_label.x = left_pane.x + 5;
    wpm_label.y = cpm_rect.y + cpm_rect.h;
    wpm_label.w = left_pane.w - 5;
    wpm_label.h = text_height;

    wpm_rect.x = left_pane.x + 5;
    wpm_rect.y = wpm_label.y + wpm_label.h;
    wpm_rect.w = left_pane.w - 5;
    wpm_rect.h = text_height;

    errors_label.x = left_pane.x + 5;
    errors_label.y = wpm_rect.y + wpm_rect.h;
    errors_label.w = left_pane.w - 5;
    errors_label.h = text_height;

    errors_rect.x = left_pane.x + 5;
    errors_rect.y = errors_label.y + errors_label.h;
    errors_rect.w = left_pane.w - 5;
    errors_rect.h = text_height;

    accuracy_label.x = left_pane.x + 5;
    accuracy_label.y = errors_rect.y + errors_rect.h;
    accuracy_label.w = left_pane.w - 5;
    accuracy_label.h = text_height;

    accuracy_rect.x = left_pane.x + 5;
    accuracy_rect.y = accuracy_label.y + accuracy_label.h;
    accuracy_rect.w = left_pane.w - 5;
    accuracy_rect.h = text_height;

    /* Set up all the locations within the top pane: */
    phr_text_rect.x = top_pane.x + 5;
    phr_text_rect.y = top_pane.y + top_pane.h * 0.3;
    phr_text_rect.w = top_pane.w - 5;
    phr_text_rect.h = medfontsize;

    /* we can't just use phr_text_rect.w to calc wrap */
    /* because SDL_BlitSurface() clobbers it: */
    phrase_draw_width = phr_text_rect.w;

    user_text_rect.x = top_pane.x + 5;
    user_text_rect.y = top_pane.y + top_pane.h * 0.6;
    user_text_rect.w = top_pane.w - 5;
    user_text_rect.h = medfontsize * 1.5;

    /* Set up all the locations within the bottom pane: */
    keyboard_loc.x =
        bottom_pane.x + bottom_pane.w / 4 - practice_keyboard.base->w / 4;
    keyboard_loc.y = bottom_pane.y + 5;
    keyboard_loc.w = practice_keyboard.base->w;
    keyboard_loc.h = practice_keyboard.base->h;
    Kbd_Display_SetPosition(&practice_keyboard, keyboard_loc.x, keyboard_loc.y);

    hand_loc.x = keyboard_loc.x;
    hand_loc.y = keyboard_loc.y + keyboard_loc.h + 20;
    hand_loc.w = practice_hands.base->w;
    hand_loc.h = practice_hands.base->h;
    Hand_Display_SetPosition(&practice_hands, hand_loc.x, hand_loc.y);

    nextletter_rect.w = bigfontsize * 1.5;
    nextletter_rect.h = bigfontsize * 1.5;
    nextletter_rect.x = keyboard_loc.x + keyboard_loc.w - nextletter_rect.w;
    nextletter_rect.y = keyboard_loc.y + keyboard_loc.h;

    /* Alt-glyph preview sits just left of the Latin one, same size +
     * a small gap. Always reserved (so the input-mode toggle doesn't
     * shift layout); only painted in non-keyboard modes. */
    altglyph_rect.w = nextletter_rect.w;
    altglyph_rect.h = nextletter_rect.h;
    altglyph_rect.x = nextletter_rect.x - altglyph_rect.w - 8;
    altglyph_rect.y = nextletter_rect.y;
}

static void practice_unload_media(void)
{
    FreeBothBkgds();
    FreeLetters();

    if (time_label_srfc)
    {
        SDL_DestroySurface(time_label_srfc);
    }
    time_label_srfc = NULL;

    if (chars_label_srfc)
    {
        SDL_DestroySurface(chars_label_srfc);
    }
    chars_label_srfc = NULL;

    if (cpm_label_srfc)
    {
        SDL_DestroySurface(cpm_label_srfc);
    }
    cpm_label_srfc = NULL;

    if (wpm_label_srfc)
    {
        SDL_DestroySurface(wpm_label_srfc);
    }
    wpm_label_srfc = NULL;

    if (errors_label_srfc)
    {
        SDL_DestroySurface(errors_label_srfc);
    }
    errors_label_srfc = NULL;

    if (accuracy_label_srfc)
    {
        SDL_DestroySurface(accuracy_label_srfc);
    }
    accuracy_label_srfc = NULL;

    Hand_Display_Free(&practice_hands);
    Kbd_Display_Free(&practice_keyboard);

    if (tux_stand)
    {
        FreeSprite(tux_stand);
        tux_stand = NULL;
    }

    if (tux_win)
    {
        FreeSprite(tux_win);
        tux_win = NULL;
    }

    if (cheer)
    {
        ((void)0);
    }
    cheer = NULL;

    T4K_VisibleBell_Free(&s_wrong_bell);
}

static int cell_width_for_row(int font_size)
{
    /* Slot needs to fit the widest mode glyph at this font size. Braille
     * cells (rendered via DejaVu fallback) are roughly the font height
     * wide; pad a couple of pixels so adjacent cells don't kiss. */
    return font_size + 4;
}

static int draw_cell_row_prompt(const wchar_t* wstr, int len, SDL_Rect rect)
{
    int cell_w   = cell_width_for_row(medfontsize);
    int cell_idx = 0;
    for (int i = 0; i < len && wstr[i]; i++)
    {
        int n_cells = Input_CellsForChar(practice_input, wstr[i]);
        /* Latin prompt sits right-justified in the slot's last cell. */
        int slot_last_x = rect.x + (cell_idx + n_cells - 1) * cell_w;
        if (slot_last_x + cell_w > rect.x + rect.w)
        {
            break;
        }
        if (wstr[i] != L' ')
        {
            wchar_t      ltr[2] = {wstr[i], 0};
            SDL_Surface* s      = BlackOutline_w(ltr, medfontsize, &white, 1);
            if (s)
            {
                SDL_Rect dr = {slot_last_x + (cell_w - s->w) / 2,
                               rect.y + (rect.h - s->h) / 2, s->w, s->h};
                SDL_BlitSurface(s, NULL, screen, &dr);
                SDL_DestroySurface(s);
            }
        }
        cell_idx += n_cells;
    }
    return cell_idx;
}

static int draw_cell_row_echo(const wchar_t* wstr, int len, SDL_Rect rect)
{
    int cell_w   = cell_width_for_row(medfontsize);
    int cell_idx = 0;
    for (int i = 0; i < len && wstr[i]; i++)
    {
        int n_cells = Input_CellsForChar(practice_input, wstr[i]);
        for (int c = 0; c < n_cells; c++)
        {
            int x = rect.x + (cell_idx + c) * cell_w;
            if (x + cell_w > rect.x + rect.w)
            {
                return cell_idx + c;
            }
            Input_DrawEchoCell(practice_input, wstr[i], c, x, rect.y, cell_w,
                               rect.h, screen);
        }
        cell_idx += n_cells;
    }
    return cell_idx;
}

/* Looks for phrases.txt in theme, then in default if not found, */
/* loads it into phrases[][] array.  Returns number of phrases   */
/* successfully loaded.                                          */
static int load_phrases(const char* phrase_file)
{
    int   found       = 0;
    int   num_phrases = 0;
    char  buf[MAX_PHRASE_LENGTH];
    FILE* fp;
    char  fn[FNLEN];

    /* If using theme, look there first: */
    if (!settings.use_english)
    {
        sprintf(fn, "%s/%s", settings.theme_data_path, phrase_file);
        if (T4K_CheckFile(fn))
        {
            found = 1;
        }
    }

    /* Now checking English: */
    if (!found)
    {
        sprintf(fn, "%s/%s", settings.default_data_path, phrase_file);
        if (T4K_CheckFile(fn))
        {
            found = 1;
        }
    }

    if (!found)
    {
        fprintf(stderr,
                "Could not find phrases file '%s' - cannot do Practice\n",
                phrase_file);
        return 0;
    }

    DEBUGCODE
    {
        printf("load_phrases(): phrases file is '%s'\n", fn);
    }

    /* We know it will open OK because we already ran T4K_CheckFile() on it */
    fp = fopen(fn, "r");

    /* So now copy each line into phrases array: */
    /* NOTE we need to convert to wchar_t so just fscanf won't work! */
    while (!feof(fp) && num_phrases <= MAX_PHRASES)
    {
        /* Similar check to above but compiler complains unless we */
        /* inspect return value of fscanf():                       */
        if (EOF != fscanf(fp, "%[^\n]\n", buf))
        {
            T4K_ConvertFromUTF8(phrases[num_phrases], buf, MAX_PHRASE_LENGTH);
            DEBUGCODE
            {
                printf("phrase %d:\t%S\n", num_phrases, phrases[num_phrases]);
            }
            num_phrases++;
        }
    }

    if (num_phrases > MAX_PHRASES)
    {
        fprintf(stderr, "File contains more than MAX_PHRASES - stopping\n");
        num_phrases = MAX_PHRASES;
    }

    fclose(fp);

    DOUT(num_phrases);

    return num_phrases;
}

/* Returns index (relative to wstr) of last char in longest whole-word
 * prefix that fits in `width` pixels at font_size. Scroll window in
 * keyboard mode uses this to size visible range from scroll_offset_ch. */
static int find_next_wrap(const wchar_t* wstr, int font_size, int width)
{
    wchar_t      buf[MAX_PHRASE_LENGTH];
    char         UTF8buf[MAX_PHRASE_LENGTH];
    SDL_Surface* s             = NULL;
    int          word_end      = -1;
    int          prev_word_end = -1;

    int i          = 0;
    int phr_length = 0;
    int test_w     = 0; /* The width in pixels of the SDL-rendered string */
    /* FIXME get rid of this once overhaul done: */

    LOG("Entering find__next_wrap\n");

    /* Make sure args OK: */
    if (!wstr)
    {
        fprintf(stderr, "find_next_wrap() - error - invalid string argument\n");
        return -1;
    }

    DOUT(width);
    DEBUGCODE
    {
        fprintf(stderr, "wstr = %S\n", wstr);
    }

    phr_length = wcslen(wstr);

    DOUT(phr_length);
    /* Using 'MAX_PHRASE_LENGTH - 1' will make sure our copied string is   */
    /* null-terminated, even if it didn't fit.                             */

    if (phr_length > (MAX_PHRASE_LENGTH - 1))
    {
        fprintf(
            stderr,
            "find_next_wrap() - error - phrase exceeds MAX_PHRASE_LENGTH\n");
        return -1;
    }

    /* The function will eventually return from within the loop */
    while (1)
    {
        /* Find next either next space or end of string to check width */
        for (; i < phr_length && wstr[i] != ' '; i++)
            ;

        DOUT(i);

        /* If exited because space found, back up one so we are at last char in
         * word: */
        if (wstr[i] == ' ')
        {
            word_end = i - 1;
        }
        else
        {
            word_end = i;
        }

        /* See if we have exceeded the width */
        /* Copy string into buf and null terminate after point to be checked: */
        wcsncpy(buf, wstr, MAX_PHRASE_LENGTH);
        buf[word_end + 1] = '\0';
        DEBUGCODE
        {
            fprintf(stderr, "buf = %S\n", buf);
        }

        /* Need to convert to UTF8 because couldn't get UNICODE version to work:
         */
        T4K_ConvertToUTF8(buf, UTF8buf, MAX_PHRASE_LENGTH);
        /*  Now check width of string: */
        s = T4K_SimpleText(UTF8buf, font_size, &white);
        if (!s)
        {
            /* An error occurred: */
            return -1;
        }

        test_w = s->w;
        SDL_DestroySurface(s);
        s = NULL;

        DOUT(test_w);
        DOUT(width);
        /* If we've gone past the width, the previous space was the wrap point,
         */
        /* whether or not we are at the end of the string: */
        if (test_w > width)
        {
            DEBUGCODE
            {
                fprintf(stderr, "width exceeded, returning end of previous "
                                "word as wrap point\n");
                fprintf(stderr, "prev_word_end is %d\n", prev_word_end);
                fprintf(stderr, "leaving find_next_wrap()\n");
            }
            return prev_word_end;
        }
        else
        {
            if (i >= phr_length)
            {
                DEBUGCODE
                {
                    fprintf(stderr, "width not exceeded, returning because end "
                                    "of string reached\n");
                    fprintf(stderr, "word_end is %d\n", word_end);
                }
                /* We reached the end of the phrase without exceeding the width,
                 */
                /* so just return our current position: */
                return word_end;
            }
            else
            {
                prev_word_end = word_end;
                i++;
            }
        }
    }
}

/* FIXME this isn't very safe because index could be out of allocated string, */
/* and there a very good way to test for this within this function.           */
/* Draw the next letter the user is expected to type — Latin in the
 * always-shown slot, plus the input mode's encoding glyph in the alt
 * slot (braille cell, etc.) when relevant. */
static void display_next_letter(const wchar_t* str, Uint16 index)
{
    if (!str || (index >= MAX_PHRASE_LENGTH))
    {
        return;
    }

    /* Latin preview is always shown. */
    if (str[index] != L' ')
    {
        int font_size = nextletter_rect.h - 12;
        if (font_size < 10)
        {
            font_size = 10;
        }
        wchar_t      ltr[2] = {str[index], 0};
        SDL_Surface* s      = BlackOutline_w(ltr, font_size, &white, 1);
        if (s)
        {
            SDL_Rect dr = {nextletter_rect.x + (nextletter_rect.w - s->w) / 2,
                           nextletter_rect.y + (nextletter_rect.h - s->h) / 2,
                           s->w, s->h};
            SDL_BlitSurface(s, NULL, screen, &dr);
            SDL_DestroySurface(s);
        }
    }

    /* Encoding glyph preview in the dedicated alt slot, only when the
     * input mode has its own visual representation. */
    if (settings.input_mode != INPUT_KEYBOARD)
    {
        Input_DrawNextChar(practice_input, str[index], altglyph_rect, screen);
    }
}

static int create_labels(void)
{
    if (time_label_srfc)
    {
        SDL_DestroySurface(time_label_srfc);
    }
    time_label_srfc = T4K_BlackOutline(_("Time"), fontsize, &yellow);

    if (chars_label_srfc)
    {
        SDL_DestroySurface(chars_label_srfc);
    }
    chars_label_srfc = T4K_BlackOutline(_("Chars"), fontsize, &yellow);

    if (cpm_label_srfc)
    {
        SDL_DestroySurface(cpm_label_srfc);
    }
    cpm_label_srfc = T4K_BlackOutline(_("CPM"), fontsize, &yellow);

    if (wpm_label_srfc)
    {
        SDL_DestroySurface(wpm_label_srfc);
    }
    wpm_label_srfc = T4K_BlackOutline(_("WPM"), fontsize, &yellow);

    if (errors_label_srfc)
    {
        SDL_DestroySurface(errors_label_srfc);
    }
    errors_label_srfc = T4K_BlackOutline(_("Errors"), fontsize, &yellow);

    if (accuracy_label_srfc)
    {
        SDL_DestroySurface(accuracy_label_srfc);
    }
    accuracy_label_srfc = T4K_BlackOutline(_("Accuracy"), fontsize, &yellow);

    if (time_label_srfc && chars_label_srfc && cpm_label_srfc &&
        wpm_label_srfc && errors_label_srfc && accuracy_label_srfc)
    {
        return 1;
    }
    return 0;
}

/****************************************************************************
 * get the remaining letter
 * if till_next_space  is 1 then get lettesrs till a space reached
 * otherwise return only next charecter
 * *************************************************************************/
wchar_t* get_next_word_letters(int cur_phrase, int cursor, int till_next_space)
{
    int      iter, i, len;
    wchar_t* temp;

    temp    = (wchar_t*)malloc(1000);
    len     = wcslen(phrases[cur_phrase]);
    temp[0] = L'\0';
    for (iter = 0, i = cursor; i <= len; i++)
    {
        // Break if a space found
        if (phrases[cur_phrase][i] == L' ')
        {
            break;
        }

        if (phrases[cur_phrase][i] == L',')
        {
            wcscat(temp, L"comma");
        }
        else if (phrases[cur_phrase][i] == L'.')
        {
            wcscat(temp, L"full stop");
        }
        else if (phrases[cur_phrase][i] == L'\'')
        {
            wcscat(temp, L"apostophe");
        }
        else if (phrases[cur_phrase][i] == L';')
        {
            wcscat(temp, L"semicolon");
        }
        else if (phrases[cur_phrase][i] == L':')
        {
            wcscat(temp, L"colon");
        }
        else if (phrases[cur_phrase][i] == L'?')
        {
            wcscat(temp, L"Qustion mark");
        }
        else if (phrases[cur_phrase][i] == L'-')
        {
            wcscat(temp, L"Hyphen");
        }
        else
        {
            iter         = wcslen(temp);
            temp[iter++] = L' ';
            if (iswupper(phrases[cur_phrase][i]))
            {
                temp[iter] = L'\0';
                wcscat(temp, L"Capitol ");
                iter += 8;
            }
            temp[iter++] = phrases[cur_phrase][i];
            temp[iter++] = L' ';
            temp[iter]   = L'\0';
        }

        if (till_next_space == 0)
        {
            break;
        }
    }
    // Add space if any
    if (phrases[cur_phrase][i] == L' ')
    {
        wcscat(temp, L" Space");
    }

    return temp;
}

/*********************************************************
 * Get the next word
 ********************************************************/
wchar_t* get_next_word(int cur_phrase, int cursor)
{
    int      iter, i, len;
    wchar_t* temp;
    temp    = (wchar_t*)malloc(1000);
    temp[0] = L'\0';

    len = wcslen(phrases[cur_phrase]);
    for (iter = 0, i = cursor; i < len; i++)
    {
        // Break if a space found
        if (phrases[cur_phrase][i] == L' ')
        {
            break;
        }
        temp[iter] = phrases[cur_phrase][i];
        iter++;
    }
    temp[iter] = L'\0';
    return temp;
}
