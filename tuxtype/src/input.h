/* Input-mode abstraction.
 *
 * Each typing input method (keyboard, braille, semaphore, morse) is an
 * Input. The game loop consumes SDL events through Input_Consume(),
 * gets back decoded runes, and calls Input_DrawHint() to show the user
 * what to do next for the current target rune.
 *
 * Control events (ESC, F-keys, arrows) are NOT this module's concern —
 * caller filters those first and passes only typing-related events. */

#pragma once
#include "globals.h"
#include "hand_display.h"
#include "keyboard_display.h"

typedef struct Input_s Input;

typedef struct InputToken
{
    int     ready; /* 1 if a rune has been decoded this event/tick */
    wchar_t ch;    /* the decoded rune */
} InputToken;

/* Lifecycle. The hand + keyboard widgets are borrowed (caller owns them);
 * pass NULL for either if the mode shouldn't draw to it (e.g. Comet Zap
 * doesn't show hands). The mode's draw_hint uses whichever it has. */
Input* Input_NewTypewriter(HandDisplay* hd, KbdDisplay* kbd);
Input* Input_NewBraille(HandDisplay* hd, KbdDisplay* kbd);
/* Future:
 *   Input* Input_NewSemaphore(HandDisplay*, KbdDisplay*);
 *   Input* Input_NewMorse(KbdDisplay*); */
void Input_Free(Input* im);
void Input_Reset(Input* im);

/* Per-event: consume one SDL typing event. Returns ready=1 with the
 * decoded rune when a complete input unit lands. */
InputToken Input_Consume(Input* im, const SDL_Event* ev);

/* Per-frame (or per-poll-cycle): timer-driven decoder hook. Currently a
 * no-op for everything except morse — but morse's letter boundary is
 * detected by inter-press gap timeout, not by an SDL event, so the
 * caller must give it a chance to fire. */
InputToken Input_Tick(Input* im, Uint32 now_ms);

/* Caller sets which position in the current word the cursor sits at
 * before each Consume(). Braille's chord decoder uses this to pick
 * between value_begin / value_middle / value_end. Other modes ignore. */
void Input_SetWordPosition(Input* im, int pos);

/* Toggle which widgets DrawHint should overlay. Pass NULL to suppress
 * a widget without changing it permanently. Useful for show_keyboard
 * toggles or other UI gates. */
void Input_SetHandVisible(Input* im, int visible);
void Input_SetKbdVisible(Input* im, int visible);

/* Hint rendering: draws whatever visual makes sense for `target_ch`
 * (next-key highlight + finger hint for typewriter; chord highlight +
 * finger stack for braille; flag positions for semaphore; etc.). */
void Input_DrawHint(Input* im, wchar_t target_ch, SDL_Surface* dst);
