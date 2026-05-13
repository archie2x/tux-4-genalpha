/* Braille input mode: six-key chord on f-d-s-j-k-l + dot 6 (l alone)
 * as the capital prefix. Hint stacks per-dot finger highlights on the
 * hand silhouette + green-highlights the chord keys. */

#include "input.h"
#include "input_internal.h"
#include "braille.h"

#include "funcs.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

typedef struct
{
    Input   base;
    wchar_t chord_buf[16]; /* keys pressed since last decode */
    int     chord_len;
    int     caps_pending; /* set by dot-6 chord, applied to next */
} Braille;

static void br_clear_chord(Braille* b)
{
    b->chord_len    = 0;
    b->chord_buf[0] = L'\0';
}

static void br_reset(Input* im)
{
    Braille* b      = (Braille*)im;
    b->caps_pending = 0;
    br_clear_chord(b);
}

static InputToken br_consume(Input* im, const SDL_Event* ev)
{
    Braille*   b   = (Braille*)im;
    InputToken out = {0};

    /* Accumulate raw KEY_DOWN keys into a chord buffer. */
    if (ev->type == SDL_EVENT_KEY_DOWN)
    {
        if (b->chord_len <
            (int)(sizeof(b->chord_buf) / sizeof(b->chord_buf[0])) - 1)
        {
            b->chord_buf[b->chord_len++] = ev->key.key;
            b->chord_buf[b->chord_len]   = L'\0';
        }
        return out;
    }

    if (ev->type != SDL_EVENT_KEY_UP)
    {
        return out;
    }

    /* Dot 6 alone is the standard braille capital indicator (⠠). */
    if (wcscmp(b->chord_buf, L"l") == 0)
    {
        b->caps_pending = 1;
        br_clear_chord(b);
        return out;
    }

    /* Space chord — produce a literal space. */
    if (wcscmp(b->chord_buf, L" ") == 0)
    {
        out.ready = 1;
        out.ch    = L' ';
        br_clear_chord(b);
        return out;
    }

    if (b->chord_len > 0)
    {
        Braille_Reorder(b->chord_buf);
        for (int i = 0; i < 100; i++)
        {
            if (wcscmp(b->chord_buf, braille_key_value_map[i].key) != 0)
            {
                continue;
            }
            wchar_t ch;
            if (im->word_pos == 0)
            {
                ch = braille_key_value_map[i].value_begin[0];
            }
            else if (im->word_pos == 1)
            {
                ch = braille_key_value_map[i].value_middle[0];
            }
            else
            {
                ch = braille_key_value_map[i].value_end[0];
            }
            if (b->caps_pending)
            {
                ch              = toupper(ch);
                b->caps_pending = 0;
            }
            out.ready = 1;
            out.ch    = ch;
            break;
        }
    }
    br_clear_chord(b);
    return out;
}

/* Map a chord-key char ('f','d','s','j','k','l') to finger index 0..9
 * for hand highlighting. Returns -1 if not a chord key. */
static int chord_key_to_finger(wchar_t k)
{
    switch (k)
    {
    case L'f':
        return 3;
    case L'd':
        return 2;
    case L's':
        return 1;
    case L'j':
        return 6;
    case L'k':
        return 7;
    case L'l':
        return 8;
    default:
        return -1;
    }
}

static void br_draw_hint(Input* im, wchar_t target_ch, SDL_Surface* dst)
{
    Braille* b = (Braille*)im;
    if (target_ch == L' ')
    {
        if (im->hand)
        {
            Hand_Display_DrawBase(im->hand, dst);
        }
        if (im->kbd)
        {
            int key = GetIndex(L' ');
            if (key >= 0 && key < MAX_UNICODES)
            {
                Kbd_Display_BlitGreenKey(im->kbd, key, dst);
            }
        }
        return;
    }

    /* Caps prefix pending hint: dot 6 chord, then the letter chord. */
    wchar_t dots[6];
    int     n;
    if (iswupper(target_ch) && !b->caps_pending)
    {
        dots[0] = L'l';
        n       = 1;
    }
    else
    {
        n = Braille_DotsForChar(target_ch, dots);
    }
    if (n <= 0)
    {
        if (im->hand)
        {
            Hand_Display_DrawBase(im->hand, dst);
        }
        return;
    }

    if (Input_HandActive(im))
    {
        Hand_Display_DrawForChord(im->hand, dots, n, dst);
    }
    if (Input_KbdActive(im))
    {
        for (int i = 0; i < n; i++)
        {
            int key = GetIndex(dots[i]);
            if (key >= 0 && key < MAX_UNICODES)
            {
                Kbd_Display_BlitGreenKey(im->kbd, key, dst);
            }
        }
    }
    (void)chord_key_to_finger; /* reserved — finger from key for future use */
}

static const InputOps br_ops = {
    .destroy   = NULL,
    .reset     = br_reset,
    .consume   = br_consume,
    .tick      = NULL,
    .draw_hint = br_draw_hint,
};

Input* Input_NewBraille(HandDisplay* hd, KbdDisplay* kbd)
{
    Braille* b = calloc(1, sizeof(*b));
    if (!b)
    {
        return NULL;
    }
    b->base.ops          = &br_ops;
    b->base.hand         = hd;
    b->base.kbd          = kbd;
    b->base.hand_visible = 1;
    b->base.kbd_visible  = 1;
    return &b->base;
}
