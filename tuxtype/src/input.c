/* Input dispatch + shared lifecycle. Each concrete mode lives in
 * its own *_input.c file and registers via the InputModeOps vtable. */

#include "input.h"
#include "input_internal.h"

#include "funcs.h"
#include <stdlib.h>

void Input_Free(Input* im)
{
    if (!im)
    {
        return;
    }
    if (im->ops->destroy)
    {
        im->ops->destroy(im);
    }
    free(im);
}

void Input_Reset(Input* im)
{
    if (im && im->ops->reset)
    {
        im->ops->reset(im);
    }
}

InputToken Input_Consume(Input* im, const SDL_Event* ev)
{
    InputToken empty = {0};
    if (im && ev && im->ops->consume)
    {
        return im->ops->consume(im, ev);
    }
    return empty;
}

InputToken Input_Tick(Input* im, Uint32 now_ms)
{
    InputToken empty = {0};
    if (im && im->ops->tick)
    {
        return im->ops->tick(im, now_ms);
    }
    return empty;
}

void Input_SetWordPosition(Input* im, int pos)
{
    if (im)
    {
        im->word_pos = pos;
    }
}

void Input_SetHandVisible(Input* im, int visible)
{
    if (im)
    {
        im->hand_visible = visible;
    }
}

void Input_SetKbdVisible(Input* im, int visible)
{
    if (im)
    {
        im->kbd_visible = visible;
    }
}

void Input_DrawHint(Input* im, wchar_t target_ch, SDL_Surface* dst)
{
    if (im && dst && im->ops->draw_hint)
    {
        im->ops->draw_hint(im, target_ch, dst);
    }
}

void Input_DrawNextChar(Input* im, wchar_t target_ch, SDL_Rect rect,
                        SDL_Surface* dst)
{
    if (im && dst && im->ops->draw_next_char)
    {
        im->ops->draw_next_char(im, target_ch, rect, dst);
    }
}

int Input_CellsForChar(Input* im, wchar_t target_ch)
{
    if (im && im->ops->cells_for_char)
    {
        return im->ops->cells_for_char(im, target_ch);
    }
    return 1; /* default: one cell per char */
}

void Input_DrawEchoCell(Input* im, wchar_t target_ch, int cell_idx, int x,
                        int y, int cell_w, int cell_h, SDL_Surface* dst)
{
    if (im && dst && im->ops->draw_echo_cell)
    {
        im->ops->draw_echo_cell(im, target_ch, cell_idx, x, y, cell_w, cell_h,
                                dst);
    }
}

int Input_DrawPendingEcho(Input* im, int x, int y, int cell_w, int cell_h,
                          SDL_Surface* dst)
{
    if (im && dst && im->ops->draw_pending_echo)
    {
        return im->ops->draw_pending_echo(im, x, y, cell_w, cell_h, dst);
    }
    return 0;
}
