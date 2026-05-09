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

/* Thin delegation to t4k_common's SDL3_mixer-backed audio. */

void PlaySoundLoop(Mix_Chunk* snd, int loops)
{
    if (!settings.sys_sound)
    {
        return;
    }
    T4K_PlaySoundLoop(snd, loops);
}

void audioHaltChannel(int channel)
{
    T4K_AudioHaltChannel(channel);
}

void MusicLoad(const char* musicFilename, int loops)
{
    if (!settings.sys_sound || !musicFilename)
    {
        return;
    }
    Mix_Music* m = LoadMusic(musicFilename);
    if (m)
    {
        MusicUnload();
        defaultMusic = m;
        T4K_AudioMusicPlay(m, loops);
    }
}

void MusicUnload(void)
{
    T4K_AudioMusicUnload();
    defaultMusic = NULL;
}

void MusicPlay(Mix_Music* musicData, int loops)
{
    if (!settings.sys_sound || !musicData)
    {
        return;
    }
    MusicUnload();
    T4K_AudioMusicPlay(musicData, loops);
}
