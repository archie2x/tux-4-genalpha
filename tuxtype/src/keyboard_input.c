/*
 * Shared typing-input decoder. Handles SDL_EVENT_TEXT_INPUT in normal mode and
 * KEY_DOWN-accumulate / KEY_UP-decode (chord + capital/number prefix) in
 * braille mode. State is file-static; only one game runs at a time so no
 * per-instance struct.
 */

#include "keyboard_input.h"

#include "braille.h"
#include "funcs.h"
#include <ctype.h>
#include <string.h>
#include <wchar.h>

static wchar_t kbd_pressed[1000];
static int     kbd_iter            = 0;
static int kbd_capital_pending = 0; /* set by dot-6 chord, applied to next */
static int     kbd_numbers_pending = 0; /* set by ';' chord, applied to next */

static void kbd_clear_chord(void)
{
    kbd_iter       = 0;
    kbd_pressed[0] = L'\0';
}

void Kbd_Input_Reset(void)
{
    kbd_clear_chord();
    kbd_capital_pending = 0;
    kbd_numbers_pending = 0;
}

int Kbd_Input_CapitalPending(void)
{
    return kbd_capital_pending;
}

void Kbd_Input_HandleEvent(const SDL_Event* event, int braille_letter_pos,
                           KbdTyped* out)
{
    out->ready     = 0;
    out->ch        = 0;
    out->key_index = -1;

    /* Braille mode accumulates raw KEY_DOWN keys into a chord buffer. */
    if (event->type == SDL_EVENT_KEY_DOWN && settings.braille)
    {
        if (kbd_iter < (int)(sizeof(kbd_pressed) / sizeof(kbd_pressed[0])) - 1)
        {
            kbd_pressed[kbd_iter++] = event->key.key;
            kbd_pressed[kbd_iter]   = L'\0';
        }
        return;
    }

    /* Normal mode: TEXT_INPUT delivers composed glyphs (handles shift, dead
     * keys, IME). Skipped in braille mode where dot keys aren't characters. */
    if (event->type == SDL_EVENT_TEXT_INPUT && !settings.braille)
    {
        wchar_t   typed = 0;
        mbstate_t mbs   = {0};
        if (mbrtowc(&typed, event->text.text, strlen(event->text.text), &mbs) >
            0)
        {
            out->ch        = typed;
            out->key_index = GetIndex(typed);
            out->ready     = 1;
        }
        return;
    }

    /* Braille mode: KEY_UP triggers chord decode. */
    if (event->type == SDL_EVENT_KEY_UP && settings.braille)
    {
        /* Single-key prefixes — set a one-shot flag for the next chord.
         * Dot 6 alone is the standard braille capital indicator (⠠). */
        if (wcscmp(kbd_pressed, L"l") == 0)
        {
            kbd_capital_pending = 1;
            kbd_clear_chord();
            return;
        }
        if (wcscmp(kbd_pressed, L";") == 0)
        {
            Braille_LoadLanguage("numerical.txt");
            kbd_numbers_pending = 1;
            kbd_clear_chord();
            return;
        }

        /* Space chord — produce a literal space. */
        if (wcscmp(kbd_pressed, L" ") == 0)
        {
            out->ch        = L' ';
            out->key_index = GetIndex(L' ');
            out->ready     = 1;
            kbd_clear_chord();
            return;
        }

        /* Letter chord — sort dot keys into canonical order, then look up. */
        if (kbd_iter > 0)
        {
            Braille_Reorder(kbd_pressed);
            for (int i = 0; i < 100; i++)
            {
                if (wcscmp(kbd_pressed, braille_key_value_map[i].key) != 0)
                {
                    continue;
                }
                wchar_t ch;
                if (braille_letter_pos == 0)
                {
                    ch = braille_key_value_map[i].value_begin[0];
                }
                else if (braille_letter_pos == 1)
                {
                    ch = braille_key_value_map[i].value_middle[0];
                }
                else
                {
                    ch = braille_key_value_map[i].value_end[0];
                }

                if (kbd_capital_pending)
                {
                    ch                  = toupper(ch);
                    kbd_capital_pending = 0;
                }
                if (kbd_numbers_pending)
                {
                    /* Reload the active language map to switch back from
                     * numerical to letter mode after one digit. */
                    char fn[FNLEN];
                    if (settings.use_english)
                    {
                        snprintf(fn, FNLEN, "english.txt");
                    }
                    else
                    {
                        snprintf(fn, FNLEN, "%s.txt", settings.theme_name);
                    }
                    Braille_LoadLanguage(fn);
                    kbd_numbers_pending = 0;
                }

                out->ch        = ch;
                out->key_index = GetIndex(ch);
                out->ready     = 1;
                break;
            }
        }
        kbd_clear_chord();
        return;
    }
}
