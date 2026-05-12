/*
   setup.c:

   Initialization of SDL and other libraries.

   Copyright 2000, 2003, 2007, 2008, 2009, 2010.
   Authors: Sam Hart, Jesse Andrews, David Bruce.
   Project email: <tux4kids-tuxtype-dev@lists.alioth.debian.org>
   Project website: http://tux4kids.alioth.debian.org

   setup.c is part of Tux Typing, a.k.a "tuxtype".

Tux Typing is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

Tux Typing is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "globals.h"
#include "funcs.h"
#include "SDL_extras.h"

int fs_res_x = 0;
int fs_res_y = 0;

/* The SDL_Window we created (also handed off to t4k_common). Declared
 * extern in globals.h so the rest of tuxtype (titlescreen.c, etc.) can
 * pass it to SDL_SetWindowMouseGrab / SDL_WarpMouseInWindow / etc. */
SDL_Window* tt_window = NULL;

/* Local function prototypes: */
static void seticon(void);
static int  load_settings_fp(FILE* fp);
static int  load_settings_filename(const char* fn);

/***************************
    GraphicsInit: Initializes the graphic system (SDL3 port).
****************************/
void GraphicsInit(void)
{
    DEBUGCODE
    {
        fprintf(stderr, "Entering GraphicsInit()\n");
    }

    /* SDL3: get desktop resolution for fullscreen mode. */
    const SDL_DisplayMode* dm =
        SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    if (dm)
    {
        fs_res_x = dm->w;
        fs_res_y = dm->h;
    }
    else
    {
        fs_res_x = RES_X;
        fs_res_y = RES_Y;
    }

    /* RESIZABLE makes macOS show the green native-fullscreen traffic-light
     * button (and lets the user drag-resize). Our backing surface stays at a
     * fixed 640x480 logical resolution; t4k_present() rescales each frame, so
     * any window size adapts automatically. */
    Uint32 win_flags = SDL_WINDOW_RESIZABLE;
    int    win_w = RES_X, win_h = RES_Y;
    if (settings.fullscreen == 1)
    {
        win_flags |= SDL_WINDOW_FULLSCREEN;
        win_w = fs_res_x;
        win_h = fs_res_y;
    }

    tt_window = SDL_CreateWindow("Tux Typing", win_w, win_h, win_flags);
    if (!tt_window && settings.fullscreen)
    {
        fprintf(stderr, "\nWarning: I could not open fullscreen mode: %s\n",
                SDL_GetError());
        settings.fullscreen = 0;
        tt_window           = SDL_CreateWindow("Tux Typing", RES_X, RES_Y, 0);
    }

    if (!tt_window)
    {
        fprintf(stderr, "\nError: I could not open the display: %s\n",
                SDL_GetError());
        exit(2);
    }

    /* Tell t4k_common about the window. It allocates a fixed-resolution
     * backing surface (640x480) that all rendering targets — t4k_common
     * scale-blits it onto the actual window surface in T4K_UpdateRect.
     * Use T4K_GetScreen() so tuxtype draws to the same backing. */
    T4K_RegisterWindow(tt_window);
    screen = T4K_GetScreen();

    /* seticon() commented out — SDL3 expects an SDL_Surface* via
     * SDL_SetWindowIcon; do that in a follow-up. */

    InitBlitQueue();

    DEBUGCODE
    {
        fprintf(stderr, "-SDL VidMode successfully set to %ix%i\n",
                screen ? screen->w : 0, screen ? screen->h : 0);
    }

    LOG("GraphicsInit():END\n");
}

/****************************
    LibInit : Init the SDL
    library
*****************************/
/* NOTE lib_flags is *always* SDL_INIT_VIDEO|SDL_INIT_AUDIO - maybe we */
/* should just simplify all this:                                      */
void LibInit(Uint32 lib_flags)
{
    LOG("LibInit():\n-About to init SDL Library\n");

    /* SDL3: SDL_Init returns bool (true=success) instead of int<0. */
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(2);
    }
    /* Audio: SDL3_mixer init lazily on first use via t4k_common's T4K_PlaySound
     * etc. The settings.sys_sound flag remains as the per-user enable. */

    // atexit(SDL_Quit); // fire and forget...

    LOG("-SDL Library init'd successfully\n");

    DEBUGCODE
    {
        fprintf(stderr, "settings.sys_sound = %d\n", settings.sys_sound);
    }

    /* Audio init stubbed for SDL3 port (task #13). settings.sys_sound is
     * forced to 0 above so this block never runs. */

    LOG("-about to init SDL text library (SDL_ttf or SDL_Pango\n");

    if (!Setup_SDL_Text())
    {
        fprintf(stderr, "Couldn't initialize desired SDL text library\n");
        exit(2);
    }
    //	atexit(TTF_Quit);

    LOG("LibInit():END\n");
}

/* Load the settings from a file... make sure to update SaveSettings if you
 * change what can be saved/loaded
 */
void LoadSettings(void)
{
    char fn[FNLEN];
    // 	char setting[FNLEN];
    // 	char value[FNLEN];
    //	FILE *settingsFile;

    /* set the settings directory/file */

#ifdef WIN32
    // MDTTEMP: commented out the next line and added the line after
    // snprintf(fn, FNLEN - 1, "userdata/settings.txt");
    snprintf(fn, FNLEN - 1, "%s/TuxType/settings.txt", getenv("APPDATA"));
    LOG("WIN32 defined\n");
#else
    const char* xdg_dir;

    xdg_dir = getenv("XDG_CONFIG_HOME");
    if (xdg_dir != NULL)
    {
        snprintf(fn, FNLEN - 1, (const char*)"%s/tuxtype/settings.txt",
                 xdg_dir);
    }
    else
    {
        snprintf(fn, FNLEN - 1, (const char*)"%s/.config/tuxtype/settings.txt",
                 getenv("HOME"));
    }
    LOG("WIN32 not defined\n");
#endif

    DEBUGCODE
    {
        printf("LoadSettings: settings file is '%s'\n", fn);
    }

    LOG("LoadSettings: trying to open settings file\n");

    load_settings_filename(fn);
}

/* Load the settings if given the complete pathname to the settings file.
   Returns 1 if able to call load_settings_fp() successfully on named file.
 */
static int load_settings_filename(const char* fn)
{
    FILE* fp = fopen(fn, "r");

    if (!fp)
    {
        fprintf(
            stderr,
            "load_settings_filename(): no theme-specific settings found: %s\n",
            fn);
        return -1;
    }

    if (!load_settings_fp(fp))
    {
        fprintf(stderr, "No settings in settings file.\n");
        fclose(fp); /* still need to close fp */
        return 0;
    }

    /* Success! */
    fclose(fp);
    return 1;
}

/* Load the settings if given an open FILE* pointer to the settings file.
   Returns 1 if at least one setting value found, 0 otherwise. It does not close
   the FILE*.
 */
static int load_settings_fp(FILE* fp)
{
    char setting[FNLEN]; /* these don't really need to be 'FNLEN' long */
    char value[FNLEN];
    int  setting_found = 0;

    if (!fp)
    {
        fprintf(stderr, "load_settings_fp() - FILE* parameter NULL\n");
        return 0;
    }

    /* we load all the settings here */
    while (!feof(fp))
    {
        if (EOF == fscanf(fp, "%[^=]=%[^\n]\n", setting, value))
        {
            break;
        }

        DEBUGCODE
        {
            fprintf(stderr, "%s = %s", setting, value);
        }
        // For now we are not reading or saving the language selection:
        // MDTTEMP: uncommented the following 8 lines and joined the else to the
        // if on line 259
        if (0 == strncmp(setting, "lang", sizeof("lang")))
        {
            DEBUGCODE
            {
                fprintf(stderr, "LoadSettings: Setting language to %s\n",
                        value);
            }
            strncpy(settings.lang, value, FNLEN - 1);
            setting_found = 1;
            SetupPaths(value); /* Does this really belong here? */
        }
        else if (0 == strncmp(setting, "o_lives", sizeof("o_lives")))
        {
            DEBUGCODE
            {
                fprintf(stderr, "LoadSettings: Setting lives to %s\n", value);
            }
            settings.o_lives = atoi(value);
            setting_found    = 1;
        }
        else if (0 == strncmp(setting, "mus_volume", sizeof("mus_volume")))
        {
            DEBUGCODE
            {
                fprintf(stderr, "LoadSettings: Setting music volume to %s\n",
                        value);
            }
            settings.mus_volume = atoi(value);
            setting_found       = 1;
        }
        else if (0 == strncmp(setting, "sfx_volume", sizeof("sfx_volume")))
        {
            DEBUGCODE
            {
                fprintf(stderr, "LoadSettings: Setting effects volume to %s\n",
                        value);
            }
            settings.sfx_volume = atoi(value);
            setting_found       = 1;
        }
        else if (0 == strncmp(setting, "menu_music", sizeof("menu_music")))
        {
            DEBUGCODE
            {
                fprintf(stderr, "LoadSettings: Setting menu music to %s\n",
                        value);
            }
            settings.menu_music = atoi(value);
            setting_found       = 1;
        }
        else if (0 == strncmp(setting, "fullscreen", sizeof("fullscreen")))
        {
            settings.fullscreen = atoi(value);
            setting_found       = 1;
        }
        else if (0 ==
                 strncmp(setting, "theme_font_name", sizeof("theme_font_name")))
        {
            DEBUGCODE
            {
                fprintf(stderr,
                        "load_settings_fp(): Setting theme font to %s\n",
                        value);
            }
            strncpy(settings.theme_font_name, value, FNLEN - 1);
            setting_found = 1;
        }
        else if (0 == strncmp(setting, "theme_locale_name",
                              sizeof("theme_locale_name")))
        {
            DEBUGCODE
            {
                fprintf(stderr,
                        "load_settings_fp(): Setting theme locale to %s\n",
                        value);
            }
            strncpy(settings.theme_locale_name, value, FNLEN - 1);
            setting_found = 1;
        }
        else if (0 == strncmp(setting, "tts_volume", sizeof("tts_volume")))
        {
            DEBUGCODE
            {
                fprintf(stderr, "LoadSettings: Setting tts volume to %s\n",
                        value);
            }
            settings.tts_volume = atoi(value);
            setting_found       = 1;
        }
        else
        {
            DEBUGCODE
            {
                fprintf(stderr, "load_settings_fp(): unrecognized string: %s\n",
                        value);
            }
        }
    }
    if (setting_found)
    {
        return 1;
    }
    else
    {
        fprintf(
            stderr,
            "load_settings_fp() - no settings in file - empty or corrupt?\n");
        return 0;
    }
}

/* Save the settings from a file... make sure to update LoadSettings if you
 * change what can be saved/loaded
 */
void SaveSettings(void)
{
    char  fn[FNLEN];
    FILE* settingsFile;

    /* set the settings directory/file */

    // #ifdef WIN32
    //     //MDTTEMP: Commented out the next line and added the next 2 lines
    //     after
    //	//_mkdir( "userdata" );  // just in case try to create save location
    //	snprintf( fn, FNLEN-1, (const char*)"%s/TuxType", getenv("APPDATA") );
    //	_mkdir( fn );  // just in case try to create save location
    //     // MDTTEMP: Commented out the next line and added the line after
    //	//snprintf( fn, FNLEN-1, "userdata/settings.txt" );
    //	snprintf( fn, FNLEN-1, "%s/TuxType/settings.txt", getenv("APPDATA"));
    // #else
    //	snprintf( fn, FNLEN-1, (const char*)"%s/.tuxtype", getenv("HOME") );
    //	mkdir( fn, 0755 ); // just in case try to create save location
    //	snprintf( fn, FNLEN-1, (const char*)"%s/.tuxtype/settings.txt",
    // getenv("HOME") ); #endif

    // Use the settings.user_settings_path member
    snprintf(fn, FNLEN - 1, (const char*)"%s/settings.txt",
             settings.user_settings_path);

    DEBUGCODE
    {
        printf("SaveSettings: settings file is '%s'\n", fn);
    }

    LOG("SaveSettings: trying to open settings file\n");

    settingsFile = fopen(fn, "w");

    if (settingsFile == NULL)
    {
        printf("SaveSettings: Settings file cannot be created!\n");
        return;
    }

    /* Save all the settings here! */
    // NOTE for now, don't save theme because things get screwed up if the
    // language
    //  doesn't match the LANG environmental variable - DSB
    //  MDTTEMP: uncommented the following 2 lines
    if (strncmp(settings.theme_name, "", FNLEN) != 0)
    {
        fprintf(settingsFile, "lang=%s\n", settings.theme_name);
    }
    if (settings.o_lives > 9)
    {
        fprintf(settingsFile, "o_lives=%d\n", settings.o_lives);
    }

    fprintf(settingsFile, "mus_volume=%d\n", settings.mus_volume);
    fprintf(settingsFile, "sfx_volume=%d\n", settings.sfx_volume);
    fprintf(settingsFile, "menu_music=%d\n", settings.menu_music);
    fprintf(settingsFile, "fullscreen=%d\n", settings.fullscreen);
    fprintf(settingsFile, "tts_volume=%d\n", settings.tts_volume);

    // 	if ((tt_window ? (SDL_GetWindowFlags(tt_window) & SDL_WINDOW_FULLSCREEN)
    // : 0)){ 		fprintf( settingsFile, "fullscreen=%s\n", "1"); 	} else {
    // fprintf( settingsFile, "fullscreen=%s\n", "0");
    // 	}
    fclose(settingsFile);
}

/* NOTE Now default path is _always_ DATA_PREFIX - hackish workarounds removed.
 */
/* Check for default (English) and theme data paths and update settings struct.
 */
/* Returns 0 if default data path not found, 1 if successfully located. */
/* If theme not found, still returns 1 but settings changed to use English. */
/* TODO should have this function set up the user and global settings paths. */
/* TODO settings should be re-loaded when theme changes. */

int SetupPaths(const char* theme_dir)
{
    DEBUGCODE
    {
        fprintf(stderr, "Entering SetupPaths()\n");
    }

    settings.use_english =
        1;          // default is to use English if we cannot find theme
    char fn[FNLEN]; // used later when setting settings.user_settings_path

    const char* data_prefix = T4K_PathData();
    if (T4K_CheckFile(data_prefix))
    {
        strncpy(settings.default_data_path, data_prefix, FNLEN - 1);
        DEBUGCODE
        {
            fprintf(stderr,
                    "path '%s' found, copy to settings.default_data_path\n",
                    data_prefix);
        }
    }
    else
    {
        fprintf(stderr, "Error - data prefix = '%s' not found!\n", data_prefix);
        return 0;
    }

    /* Now look for theme directory: */
    if (theme_dir != NULL)
    {
        char full_theme_path[FNLEN];
        char theme_settings_path[FNLEN];

        sprintf(full_theme_path, "%s/themes/%s", settings.default_data_path,
                theme_dir);

        DEBUGCODE
        {
            fprintf(stderr, "SetupPaths(): checking for '%s' as theme path\n",
                    full_theme_path);
        }

        if (T4K_CheckFile(full_theme_path)) /* Theme found - set it up! */
        {
            settings.use_english = 0;
            strncpy(settings.theme_data_path, full_theme_path, FNLEN - 1);
            DEBUGCODE
            {
                fprintf(stderr, "settings.theme_data_path is: %s\n",
                        settings.theme_data_path);
            }

            strncpy(settings.theme_name, theme_dir, FNLEN - 1);
            /* (Need to do this in case we are changing from a theme with */
            /* a special font to a theme that uses the default, but lacks */
            /* an explicit statement to use the default(                  */
            strncpy(settings.theme_font_name, DEFAULT_FONT_NAME, FNLEN);

            /* Load fontname or any other theme-specific settings: */
            sprintf(theme_settings_path, "%s/settings.txt", full_theme_path);

            DEBUGCODE
            {
                fprintf(stderr, "theme_settings_path is: %s\n",
                        theme_settings_path);
            }

            load_settings_filename(theme_settings_path);
        }
        else /* Theme not found! */
        {
            settings.use_english =
                1; // default is to use English if we cannot find theme
            strcpy(settings.theme_name, "");
            strncpy(settings.theme_font_name, DEFAULT_FONT_NAME, FNLEN);
            strncpy(settings.theme_locale_name, DEFAULT_LOCALE, FNLEN);
            fprintf(stderr, "SetupPaths(): could not find '%s'\n",
                    full_theme_path);
        }
    }
    else /* No theme name passed as arg so just use English: */
    {
        settings.use_english =
            1; // default is to use English if we cannot find theme
        strcpy(settings.theme_name, "");
        strncpy(settings.theme_font_name, DEFAULT_FONT_NAME, FNLEN);
        strncpy(settings.theme_locale_name, DEFAULT_LOCALE, FNLEN);
    }

    /* Modifiable user data (custom word lists, high scores). */
    const char* var_prefix = T4K_PathPref();
    if (T4K_CheckFile(var_prefix))
    {
        strncpy(settings.var_data_path, var_prefix, FNLEN - 1);
        DEBUGCODE
        {
            fprintf(stderr, "path '%s' found, copy to settings.var_data_path\n",
                    var_prefix);
        }
    }
    else
    {
        fprintf(stderr, "Error - var prefix = '%s' not found!\n", var_prefix);
        return 0;
    }

    /* Program-wide settings. Collapses to the same per-user pref dir as
     * var_prefix — no system-wide /etc location under SDL's path model. */
    const char* conf_prefix = T4K_PathPref();
    if (T4K_CheckFile(conf_prefix))
    {
        strncpy(settings.global_settings_path, conf_prefix, FNLEN - 1);
        DEBUGCODE
        {
            fprintf(stderr,
                    "path '%s' found, copy to settings.global_settings_path\n",
                    conf_prefix);
        }
    }
    else
    {
        fprintf(stderr, "Error - conf prefix = '%s' not found!\n", conf_prefix);
        return 0;
    }

    /* Editor wordlists + saved settings in the same per-user pref dir. */
    strncpy(fn, T4K_PathPref(), FNLEN - 1);
    fn[FNLEN - 1] = '\0';

    if (T4K_CheckFile(fn))
    {
        strncpy(settings.user_settings_path, fn, FNLEN - 1);
        DEBUGCODE
        {
            fprintf(stderr,
                    "path '%s' found, copying to settings.user_settings_path\n",
                    fn);
        }
    }
    else
    {
        fprintf(stderr,
                "Error - User Settings Path = '%s' not found, I will attempt "
                "to create it.\n",
                fn);
        SDL_CreateDirectory(fn);
        if (T4K_CheckFile(fn))
        {
            strncpy(settings.user_settings_path, fn, FNLEN - 1);
            DEBUGCODE
            {
                fprintf(stderr,
                        "path '%s' successfully created, copy to "
                        "settings.user_settings_path\n",
                        fn);
            }
        }
        else
        {
            // NOTE this shouldn't happen, right? We should always be able to
            // make a directory in our own home folder - DSB
            fprintf(stderr,
                    "Error - Could not create: '%s', falling back to the "
                    "global settings.\n",
                    fn);
#ifdef WIN32
            strncpy(settings.user_settings_path, "userdata", FNLEN - 1);
#else
            // FIXME: in this fallback case if we are forced to use global
            // settings, and /etc/tuxtype contains those settings we can't write
            // to it, so use /tmp - but then we have nothing to read from, copy
            // the global settings to temp first?  Or just forget about saving
            // the settings when we exit? - MDT
            strncpy(settings.user_settings_path, "/tmp", FNLEN - 1);
#endif
        }
    }

    DEBUGCODE
    {
        fprintf(stderr, "Leaving SetupPaths():\n");
        fprintf(stderr, "default_data_path: '%s'\n",
                settings.default_data_path);
        fprintf(stderr, "theme_data_path: '%s'\n\n", settings.theme_data_path);
        fprintf(stderr, "var_data_path: '%s'\n\n", settings.var_data_path);
        fprintf(stderr, "user_settings_path: '%s'\n\n",
                settings.user_settings_path);
        fprintf(stderr, "global_settings_path: '%s'\n\n",
                settings.global_settings_path);
    }

    return 1;
}

/* Set the application's icon: */

__attribute__((unused)) static void seticon(void)
{
    SDL_Surface* icon;
    int          colorkey;
    char         icon_path[FNLEN];

    snprintf(icon_path, FNLEN, "%s/images/icons/icon.png", T4K_PathData());
    icon = IMG_Load(icon_path);
    if (icon == NULL)
    {
        fprintf(stderr,
                "\nWarning: could not load icon image: %s\n"
                "SDL error: %s\n\n",
                icon_path, SDL_GetError());
        return;
    }

    /* Set up key for transparency: */
    colorkey =
        SDL_MapRGB(SDL_GetPixelFormatDetails(icon->format), NULL, 255, 0, 255);
    SDL_SetSurfaceColorKey(icon, true, colorkey);

    /* SDL3: SDL_SetWindowIcon takes the window pointer. */
    if (tt_window)
    {
        SDL_SetWindowIcon(tt_window, icon);
    }

    SDL_DestroySurface(icon);
}

void Cleanup(void)
{
    SDL_DestroySurface(screen);
    screen = NULL;
    Cleanup_SDL_Text();
    SDL_Quit();
}
