/* Input internals — concrete modes include this for the vtable +
 * base struct. Not part of the public io_input.h API. */

#pragma once
#include "input.h"

typedef struct InputOps
{
    void (*destroy)(Input*);
    void (*reset)(Input*);
    InputToken (*consume)(Input*, const SDL_Event*);
    InputToken (*tick)(Input*, Uint32 now_ms);
    void (*draw_hint)(Input*, wchar_t target_ch, SDL_Surface* dst);
} InputOps;

struct Input_s
{
    const InputOps* ops;
    HandDisplay*    hand;
    KbdDisplay*     kbd;
    int             hand_visible; /* gates DrawHint output to hand widget */
    int             kbd_visible;  /* gates DrawHint output to kbd widget  */
    int             word_pos;     /* 0 = begin, 1 = middle, 2 = end */
    /* Concrete modes embed Input as the first field for downcasting. */
};

/* Mode helpers for "should I draw to this widget?". */
static inline int Input_HandActive(const Input* im)
{
    return im && im->hand && im->hand_visible;
}
static inline int Input_KbdActive(const Input* im)
{
    return im && im->kbd && im->kbd_visible;
}
