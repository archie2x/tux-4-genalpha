/*
   funcs.h:

   Single function header for (almost) all source files.   
   Copyright 2000, 2007, 2008, 2009, 2010.
   Authors: Sam Hart, Jesse Andrews, David Bruce.
   
   Project email: <tux4kids-tuxtype-dev@lists.alioth.debian.org>
   Project website: http://tux4kids.alioth.debian.org

   funcs.h is part of Tux Typing, a.k.a "tuxtype".

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



/* NOTE - there is no reason to declare functions using "extern", as all */
/* non-local functions are visible throughout the program.               */ 

#include <t4k_common.h>
/* t4k_common.h redefines DEBUGCODE/DOUT/LOG with a different (mask-taking)
 * signature than tuxtype's globals.h uses. Restore tuxtype's versions: */
#undef DEBUGCODE
#undef DOUT
#undef LOG
#define LOG(str) if (settings.debug_on) fprintf(stderr, str);
#define DEBUGCODE if (settings.debug_on)
#define DOUT(x) if (settings.debug_on) fprintf(stderr, "%s = %d\n", #x, x);

/* TTS functions tuxtype calls but t4k_common.h doesn't declare publicly. */
void T4K_Tts_wait(void);
void T4K_Tts_cancel(void);
void T4K_Tts_stop(void);

#include "SDL_extras.h"

/* (still in alphabet.c:) */
int CheckNeededGlyphs(void);
void ClearWordList(void);
void FreeLetters(void);
int GenerateWordList(const char* wordFn);
void GenCharListFromString(const char* UTF8_str);
void ResetCharList(void);
wchar_t GetLetter(void);
wchar_t* GetWord(void);
SDL_Surface* GetWhiteGlyph(wchar_t t);
SDL_Surface* GetRedGlyph(wchar_t t);
int LoadKeyboard(void);
int GetFinger(int i);
int RenderLetters(int font_size);
int GetIndex(wchar_t uni_char);
void GetKeyShift(int index, char *buf);
int GetShift(int i);
void GetKeyPos(int index, char *buf);
void GetWrongKeyPos(int index, char *buf);
//int map_keys(wchar_t *wide_str,keymap key);
void GenerateKeyboard(SDL_Surface* keyboard);
void updatekeylist(int key,char ch);
void savekeyboard(void);
wchar_t GetLastKey(void);


/* In audio.c:   */
void PlaySound(Mix_Chunk* snd);
void PlaySoundLoop(Mix_Chunk* snd, int loops);
void audioHaltChannel(int channel);
void MusicLoad(const char* musicFilename, int repeatQty);
void MusicUnload(void);
void MusicPlay(Mix_Music* musicData, int repeatQty);


/* In laser.c:        */
int PlayLaserGame(int diff_level);


/* In loaders.c: */
int CheckFile(const char* file);
sprite* FlipSprite(sprite* in, int X, int Y);
void FreeSprite(sprite* gfx);
SDL_Surface* LoadImage(const char* datafile, int mode);
int LoadBothBkgds(const char* datafile);
SDL_Surface* CurrentBkgd(void);
void FreeBothBkgds(void);
void LoadLang(void);
Mix_Music* LoadMusic(const char* datafile);
Mix_Chunk* LoadSound(const char* datafile);
sprite* LoadSprite(const char* name, int MODE);

/* In options.c: */
void Opts_Initialize(void);

/* In pause.c: */
int  Pause(int in_game);


/* In playgame.c: */
int PlayCascade(int level);


/* In practice.c: */
int Phrases(wchar_t* practice_phrase);


/* In scripting.c: */
int XMLLesson(void);
void ProjectInfo(void);
void InstructCascade(void);
void InstructLaser(void);


/* In setup.c: */
void GraphicsInit(void);
void LibInit(Uint32 lib_flags);
void LoadSettings(void);
void SaveSettings(void);
int SetupPaths(const char* theme_dir);
void Cleanup(void);

/* Runtime path resolvers — relocatable: prefer paths relative to the
 * executable (<exe_dir>/../share/tuxtype etc.), fall back to the compile-time
 * DATA_PREFIX/VAR_PREFIX/CONF_PREFIX baked in via -D. */
const char* tt_data_prefix(void);
const char* tt_var_prefix(void);
const char* tt_conf_prefix(void);
const char* tt_locale_dir(void);

/* In theme.c: */
void ChooseTheme(void);


/* In titlescreen.c: */
//void SwitchScreenMode(void);
void TitleScreen(void);

