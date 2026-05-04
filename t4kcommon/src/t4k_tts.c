/*
   t4k_tts.c — Text-To-Speech functions, STUBBED for the SDL3 port.

   Original implementation supported eSpeak and speech-dispatcher backends.
   In this port we don't link against either; all functions are no-ops so
   the rest of the library compiles on systems without TTS libraries.
   Restoring real TTS support is a separate follow-up.

   Copyright 2013, Nalin.x.Linux. GPL v3 or later. */

#include "t4k_common.h"
#include "t4k_globals.h"

#include <SDL3/SDL_thread.h>

SDL_Thread *tts_thread = NULL;
int text_to_speech_status = 0;
int text_to_speech_speaking = 0;

void T4K_Tts_cancel(void) {}
void T4K_Tts_wait(void) {}

int T4K_Tts_init(void)
{
    return 1; /* nonzero -> "could not initialize" in InitT4KCommon. Caller
                 just prints a warning, doesn't bail. */
}

int T4K_Tts_set_voice(char voice_name[])
{
    (void)voice_name;
    return 0;
}

void T4K_Tts_stop(void) {}
void T4K_Tts_set_volume(int volume) { (void)volume; }
void T4K_Tts_set_rate(int rate) { (void)rate; }
void T4K_Tts_set_pitch(int pitch) { (void)pitch; }

void T4K_Tts_say(int rate, int pitch, int mode, const char* text, ...)
{
    (void)rate;
    (void)pitch;
    (void)mode;
    (void)text;
}
