/*
   t4k_audio.c:

   Audio-related functions.

   STUBBED for the initial SDL3 port — see task #13. SDL3_mixer is a complete
   API rewrite (MIX_Mixer/MIX_Audio/MIX_Track) and deserves a focused port.
   These functions are no-ops so the rest of the library can compile and run
   silently. The public API shape (Mix_Chunk pointers, Mix_Music pointers) is
   preserved via opaque forward-declared types in t4k_common.h.

   Copyright 2003, 2006, 2009, 2010.
   Authors: Sam Hart, Jesse Andrews, David Bruce, Brendan Luchen
   GPL v3 or later. */

#include "t4k_common.h"
#include "t4k_globals.h"

static bool audio_enabled = true;

void T4K_PlaySound(Mix_Chunk* sound)
{
    (void)sound;
}

void T4K_PlaySoundLoop(Mix_Chunk* sound, int loops)
{
    (void)sound;
    (void)loops;
}

void T4K_AudioHaltChannel(int channel)
{
    (void)channel;
}

void T4K_AudioMusicLoad(char* music_path, int loops)
{
    (void)music_path;
    (void)loops;
}

void T4K_AudioMusicUnload(void)
{
}

bool T4K_IsPlayingMusic(void)
{
    return false;
}

void T4K_AudioMusicPlay(Mix_Music* musicData, int loops)
{
    (void)musicData;
    (void)loops;
}

void T4K_AudioEnable(bool enabled)
{
    audio_enabled = enabled;
}

void T4K_AudioToggle(void)
{
    audio_enabled = !audio_enabled;
}
