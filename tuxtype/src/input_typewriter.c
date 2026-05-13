/* Typewriter input mode: SDL_EVENT_TEXT_INPUT → composed glyph. Hint
 * shows the next-key green highlight + the finger that types it. */

#include "input.h"
#include "input_internal.h"

#include "funcs.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct
{
    Input base;
} Typewriter;

static InputToken tw_consume(Input* im, const SDL_Event* ev)
{
    (void)im;
    InputToken out = {0};
    if (ev->type == SDL_EVENT_TEXT_INPUT)
    {
        wchar_t   typed = 0;
        mbstate_t mbs   = {0};
        if (mbrtowc(&typed, ev->text.text, strlen(ev->text.text), &mbs) > 0)
        {
            out.ready = 1;
            out.ch    = typed;
        }
    }
    return out;
}

static void tw_draw_hint(Input* im, wchar_t target_ch, SDL_Surface* dst)
{
    int key = GetIndex(target_ch);
    if (Input_HandActive(im))
    {
        Hand_Display_DrawForKey(im->hand, GetFinger(key), GetShift(key), dst);
    }
    if (Input_KbdActive(im))
    {
        Kbd_Display_BlitGreenKey(im->kbd, key, dst);
        Kbd_Display_DrawShiftForKey(im->kbd, key, dst);
    }
}

static const InputOps tw_ops = {
    .destroy   = NULL,
    .reset     = NULL,
    .consume   = tw_consume,
    .tick      = NULL,
    .draw_hint = tw_draw_hint,
};

Input* Input_NewTypewriter(HandDisplay* hd, KbdDisplay* kbd)
{
    Typewriter* tw = calloc(1, sizeof(*tw));
    if (!tw)
    {
        return NULL;
    }
    tw->base.ops          = &tw_ops;
    tw->base.hand         = hd;
    tw->base.kbd          = kbd;
    tw->base.hand_visible = 1;
    tw->base.kbd_visible  = 1;
    return &tw->base;
}
