/* Unified path resolution — see include/t4k/path.h. T4K_ORG is set as a
 * compile-def from CMakeLists.txt and is the org dir SDL_GetPrefPath
 * roots per-user state under. */

#include <t4k/path.h>
#include <t4k/common.h> /* T4K_RelocatablePath */
#include <SDL3/SDL.h>
#include <string.h>

#ifndef T4K_ORG
#error "T4K_ORG must be defined at compile time"
#endif

static const char* s_app_name = NULL;

void T4K_PathInit(const char* app_name) { s_app_name = app_name; }

const char* T4K_PathPref(void)
{
    static char cached[T4K_PATH_MAX];
    if ('\0' == cached[0] && s_app_name)
    {
        char* p = SDL_GetPrefPath(T4K_ORG, s_app_name);
        if (p)
        {
            strncpy(cached, p, T4K_PATH_MAX - 1);
            SDL_free(p);
        }
    }
    return cached;
}

const char* T4K_PathData(void)
{
    static char cached[T4K_PATH_MAX];
    if ('\0' == cached[0] && s_app_name)
    {
        char rel[T4K_PATH_MAX];
        snprintf(rel, T4K_PATH_MAX, "../share/%s", s_app_name);
        const char* p = T4K_RelocatablePath(rel);
        if (p)
        {
            strncpy(cached, p, T4K_PATH_MAX - 1);
        }
    }
    return cached;
}

const char* T4K_PathLocale(void)
{
    static char cached[T4K_PATH_MAX];
    if ('\0' == cached[0])
    {
        const char* p = T4K_RelocatablePath("../share/locale");
        if (p)
        {
            strncpy(cached, p, T4K_PATH_MAX - 1);
        }
    }
    return cached;
}
