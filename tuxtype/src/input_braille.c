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

/* Convert a chord (dot keys f/d/s/j/k/l) to a Unicode Braille Patterns
 * codepoint (U+2800 base + bit per dot). */
static wchar_t dots_to_braille_codepoint(const wchar_t* dots, int n)
{
    wchar_t cp = 0x2800;
    for (int i = 0; i < n; i++)
    {
        switch (dots[i])
        {
        case L'f':
            cp |= 0x01;
            break; /* dot 1 */
        case L'd':
            cp |= 0x02;
            break; /* dot 2 */
        case L's':
            cp |= 0x04;
            break; /* dot 3 */
        case L'j':
            cp |= 0x08;
            break; /* dot 4 */
        case L'k':
            cp |= 0x10;
            break; /* dot 5 */
        case L'l':
            cp |= 0x20;
            break; /* dot 6 */
        default:
            break;
        }
    }
    return cp;
}

/* Compute the 2x3 cell footprint within the font's advance box at the
 * given font_size by scanning U+283F (all 6 dots). All braille glyphs
 * at that font_size share this footprint — only which dots are lit
 * varies — so we cache and reuse. */
static int      s_cell_font_size = 0;
static SDL_Rect s_cell_bbox      = {0, 0, 0, 0};

static void ensure_cell_bbox(int font_size)
{
    if (font_size == s_cell_font_size && s_cell_bbox.w > 0)
    {
        return;
    }
    s_cell_font_size     = font_size;
    wchar_t      full[2] = {0x283F, 0};
    SDL_Surface* ref     = BlackOutline_w(full, font_size, &white, 1);
    if (!ref)
    {
        s_cell_bbox = (SDL_Rect){0, 0, 0, 0};
        return;
    }
    SDL_Rect bbox = {0, 0, ref->w, ref->h};
    if (SDL_LockSurface(ref))
    {
        const SDL_PixelFormatDetails* pf =
            SDL_GetPixelFormatDetails(ref->format);
        int min_x = ref->w, max_x = -1, min_y = ref->h, max_y = -1;
        for (int y = 0; y < ref->h; y++)
        {
            const Uint8* row = (const Uint8*)ref->pixels + y * ref->pitch;
            for (int x = 0; x < ref->w; x++)
            {
                Uint32 px = 0;
                memcpy(&px, row + x * pf->bytes_per_pixel, pf->bytes_per_pixel);
                Uint8 r, g, b, a;
                SDL_GetRGBA(px, pf, NULL, &r, &g, &b, &a);
                if (a > 32)
                {
                    if (x < min_x)
                    {
                        min_x = x;
                    }
                    if (x > max_x)
                    {
                        max_x = x;
                    }
                    if (y < min_y)
                    {
                        min_y = y;
                    }
                    if (y > max_y)
                    {
                        max_y = y;
                    }
                }
            }
        }
        SDL_UnlockSurface(ref);
        if (max_x >= min_x && max_y >= min_y)
        {
            bbox.x = min_x;
            bbox.y = min_y;
            bbox.w = max_x - min_x + 1;
            bbox.h = max_y - min_y + 1;
        }
    }
    s_cell_bbox = bbox;
    SDL_DestroySurface(ref);
}

static void br_draw_next_char(Input* im, wchar_t target_ch, SDL_Rect rect,
                              SDL_Surface* dst)
{
    Braille* b = (Braille*)im;
    if (target_ch == L' ')
    {
        return; /* space — leave the preview blank */
    }

    /* Caps prefix pending? Show dot-6-alone cell first (U+2820); on the
     * next call (after the prefix has been consumed) we'll show the
     * letter cell. */
    wchar_t cp;
    if (iswupper(target_ch) && !b->caps_pending)
    {
        cp = 0x2820;
    }
    else
    {
        wchar_t dots[6];
        int     n = Braille_DotsForChar(target_ch, dots);
        if (n <= 0)
        {
            return;
        }
        cp = dots_to_braille_codepoint(dots, n);
    }

    /* Faded translucent "card" under the glyph: concentric inset rects
     * with rising alpha approximate a radial halo without an art asset.
     * Gives the off-dots visual context so a single ⠁ doesn't look like
     * a stray speck floating in air. */
    for (int i = 0; i < 6; i++)
    {
        SDL_Rect rr = {rect.x + i * 2, rect.y + i * 2, rect.w - i * 4,
                       rect.h - i * 4};
        if (rr.w <= 0 || rr.h <= 0)
        {
            break;
        }
        T4K_DrawButtonOn(dst, &rr, rr.h / 6, 255, 255, 255, 20);
    }

    int font_size = rect.h - 12;
    if (font_size < 10)
    {
        font_size = 10;
    }
    ensure_cell_bbox(font_size);
    wchar_t      ltr[2] = {cp, 0};
    SDL_Surface* s      = BlackOutline_w(ltr, font_size, &white, 1);
    if (!s)
    {
        return;
    }
    SDL_Rect cell =
        (s_cell_bbox.w > 0) ? s_cell_bbox : (SDL_Rect){0, 0, s->w, s->h};
    SDL_Rect dr = {rect.x + (rect.w - cell.w) / 2,
                   rect.y + (rect.h - cell.h) / 2, cell.w, cell.h};
    SDL_BlitSurface(s, &cell, dst, &dr);
    SDL_DestroySurface(s);
}

static const InputOps br_ops = {
    .destroy        = NULL,
    .reset          = br_reset,
    .consume        = br_consume,
    .tick           = NULL,
    .draw_hint      = br_draw_hint,
    .draw_next_char = br_draw_next_char,
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
