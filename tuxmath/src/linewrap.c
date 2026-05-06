/* linewrap.c — thin wrappers around t4k_common's linewrap functions.
 * Original implementation lived under src/linebreak/, which has since
 * moved into libt4k_common; these wrappers preserve the legacy
 * `linewrap`/`linewrap_list` names used throughout tuxmath. */

#include <string.h>
#include "linewrap.h"
#include "globals.h"

char wrapped_lines[MAX_LINES][MAX_LINEWIDTH];

int linewrap(const char* input, char str_list[MAX_LINES][MAX_LINEWIDTH],
             int width, int max_lines, int max_width)
{
    return T4K_LineWrap(input, str_list, width, max_lines, max_width);
}

void linewrap_list(const char input[MAX_LINES][MAX_LINEWIDTH],
                   char str_list[MAX_LINES][MAX_LINEWIDTH], int width,
                   int max_lines, int max_width)
{
    T4K_LineWrapList(input, str_list, width, max_lines, max_width);
}
