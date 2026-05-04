/*
   audio.c:

   Code for audio-related functions

   Copyright 2000, 2003, 2010.
   Authors: Jesse Andrews, David Bruce.
   
   Project email: <tux4kids-tuxtype-dev@lists.alioth.debian.org>
   Project website: http://tux4kids.alioth.debian.org

   audio.c is part of Tux Typing, a.k.a "tuxtype".

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

extern Mix_Music* sounds[];

static Mix_Music* defaultMusic = NULL; // holds music for audioMusicLoad/unload



// play sound once
void PlaySound(Mix_Chunk* snd)
{
  PlaySoundLoop(snd, 0);
}

void playsound(int snd)
{
#ifndef NOSOUND
	T4K_PlaySound(sounds[snd]);
#endif
}


/* SDL3 port note: SDL3_mixer is a complete API rewrite (MIX_Mixer/MIX_Audio/
 * MIX_Track) and we haven't done that port yet (task #13). All audio is
 * stubbed; these functions are now no-ops. The public API shape is preserved
 * so callers don't need to change. */

void PlaySoundLoop(Mix_Chunk* snd, int loops)
{
  (void)snd; (void)loops;
}

void audioHaltChannel(int channel)
{
  (void)channel;
}

void MusicLoad(const char* musicFilename, int loops)
{
  (void)musicFilename; (void)loops;
}

void MusicUnload(void)
{
  defaultMusic = NULL;
}

void MusicPlay(Mix_Music* musicData, int loops)
{
  (void)musicData; (void)loops;
}
