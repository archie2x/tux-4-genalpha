/* Path resolution helpers — single source of truth for where the binary
 * lives, where its read-only data is, where to write per-user state.
 *
 * Call T4K_PathInit(app_name) once at startup before any T4K_Path*()
 * accessor. Org name (e.g. "tux4-genalpha") comes from the T4K_ORG
 * compile-def. */

#pragma once

#include <limits.h>

#ifdef PATH_MAX
#define T4K_PATH_MAX PATH_MAX
#else
#define T4K_PATH_MAX 4096
#endif

/* Cache app_name for later T4K_Path*() lookups. */
void T4K_PathInit(const char* app_name);

/* Per-user writable dir for saves / custom content. Wraps SDL_GetPrefPath,
 * which creates the dir if missing:
 *   macOS:   ~/Library/Application Support/<T4K_ORG>/<app>/
 *   Linux:   $XDG_DATA_HOME/<T4K_ORG>/<app>/
 *   Windows: %APPDATA%/<T4K_ORG>/<app>/ */
const char* T4K_PathPref(void);

/* Read-only data dir, resolved relative to the binary: <exe>/../share/<app>.
 * Works for cmake --install --prefix anywhere, /usr, /usr/local, .app
 * bundles (Contents/Resources/../share/<app>). */
const char* T4K_PathData(void);

/* Locale (gettext) dir, resolved relative to the binary:
 * <exe>/../share/locale. */
const char* T4K_PathLocale(void);
