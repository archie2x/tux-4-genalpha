/*
   t4k_audio.c — Audio playback via SDL3_mixer.

   SDL3_mixer is a complete API rewrite (MIX_Mixer/MIX_Audio/MIX_Track).
   Tuxtype/tuxmath still use the SDL2-era T4K_* names (PlaySound, MusicPlay,
   etc.) and the Mix_Chunk / Mix_Music pointer types — those are aliased to
   MIX_Audio* in t4k_common.h so call sites compile unchanged.

   Internally we keep a single mixer, one track for music, and a small
   round-robin pool for short SFX so multiple sounds can overlap.

   Original copyright 2003, 2006, 2009, 2010 Sam Hart, Jesse Andrews,
   David Bruce, Brendan Luchen. GPL v3 or later. */

#include "t4k_common.h"
#include "t4k_globals.h"
#include <stdlib.h>

#define SFX_TRACK_POOL 8

static MIX_Mixer*  g_mixer       = NULL;
static MIX_Track*  g_music_track = NULL;
static MIX_Track*  g_sfx_tracks[SFX_TRACK_POOL] = {0};
/* Parallel array: which Mix_Chunk is on each track, for per-sound
 * IsPlaying / Stop lookups. SDL3_mixer can't tell us this directly. */
static Mix_Chunk*  g_sfx_audio[SFX_TRACK_POOL] = {0};
static int         g_next_sfx    = 0;
static bool        g_audio_on    = true;
static int         g_music_loops = 0;

/* Internal: lazy-init the mixer. Called from any path that wants to make
 * sound. Returns true on success. */
static bool t4k_audio_ensure(void)
{
    if (g_mixer) return true;

    if (!MIX_Init()) {
        fprintf(stderr, "T4K audio: MIX_Init failed: %s\n", SDL_GetError());
        return false;
    }
    /* MIX_CreateMixer alone produces an "offline" mixer not connected to any
     * audio device — fine for rendering to a buffer, useless for playback.
     * MIX_CreateMixerDevice(DEFAULT_PLAYBACK, NULL) opens the default output
     * with the default format. */
    g_mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!g_mixer) {
        fprintf(stderr, "T4K audio: MIX_CreateMixer failed: %s\n", SDL_GetError());
        MIX_Quit();
        return false;
    }
    g_music_track = MIX_CreateTrack(g_mixer);
    for (int i = 0; i < SFX_TRACK_POOL; i++)
        g_sfx_tracks[i] = MIX_CreateTrack(g_mixer);
    return true;
}

/* Exposed to t4k_loaders.c so MIX_LoadAudio can use the same mixer. */
MIX_Mixer* t4k_audio_get_mixer(void)
{
    if (!t4k_audio_ensure()) return NULL;
    return g_mixer;
}

void t4k_audio_shutdown(void)
{
    if (!g_mixer) return;
    if (g_music_track) { MIX_DestroyTrack(g_music_track); g_music_track = NULL; }
    for (int i = 0; i < SFX_TRACK_POOL; i++) {
        if (g_sfx_tracks[i]) {
            MIX_DestroyTrack(g_sfx_tracks[i]);
            g_sfx_tracks[i] = NULL;
        }
    }
    MIX_DestroyMixer(g_mixer);
    g_mixer = NULL;
    MIX_Quit();
}

/* Pick a free SFX track (one not currently playing); fall back to round-robin
 * if all are busy. */
static MIX_Track* t4k_pick_sfx_track(void)
{
    for (int i = 0; i < SFX_TRACK_POOL; i++) {
        MIX_Track* t = g_sfx_tracks[i];
        if (t && !MIX_TrackPlaying(t)) return t;
    }
    MIX_Track* t = g_sfx_tracks[g_next_sfx];
    g_next_sfx = (g_next_sfx + 1) % SFX_TRACK_POOL;
    if (t) MIX_StopTrack(t, 0);
    return t;
}

/* Run a track with the given loop count. SDL3_mixer takes loops as a
 * property: 0 = play once, -1 = forever, >0 = total plays - 1. */
static bool t4k_play_with_loops(MIX_Track* track, int loops)
{
    if (!track) return false;
    SDL_PropertiesID props = 0;
    if (loops != 0) {
        props = SDL_CreateProperties();
        SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, (Sint64)loops);
    }
    bool ok = MIX_PlayTrack(track, props);
    if (props) SDL_DestroyProperties(props);
    return ok;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void T4K_PlaySound(Mix_Chunk* sound)
{
    T4K_PlaySoundLoop(sound, 0);
}

void T4K_PlaySoundLoop(Mix_Chunk* sound, int loops)
{
    if (!sound || !g_audio_on) return;
    if (!t4k_audio_ensure()) return;
    MIX_Track* track = t4k_pick_sfx_track();
    if (!track) return;
    /* Record which chunk is on this track so IsPlayingSound / StopSound
     * can find it. Index match is guaranteed: g_sfx_tracks[i] / g_sfx_audio[i]. */
    for (int i = 0; i < SFX_TRACK_POOL; i++) {
        if (g_sfx_tracks[i] == track) { g_sfx_audio[i] = sound; break; }
    }
    MIX_SetTrackAudio(track, sound);
    t4k_play_with_loops(track, loops);
}

bool T4K_IsPlayingSound(Mix_Chunk* sound)
{
    if (!sound || !g_mixer) return false;
    for (int i = 0; i < SFX_TRACK_POOL; i++) {
        if (g_sfx_audio[i] == sound &&
            g_sfx_tracks[i] && MIX_TrackPlaying(g_sfx_tracks[i]))
            return true;
    }
    return false;
}

void T4K_StopSound(Mix_Chunk* sound)
{
    if (!sound || !g_mixer) return;
    for (int i = 0; i < SFX_TRACK_POOL; i++) {
        if (g_sfx_audio[i] == sound && g_sfx_tracks[i]) {
            MIX_StopTrack(g_sfx_tracks[i], 0);
            g_sfx_audio[i] = NULL;
        }
    }
}

void T4K_AudioHaltChannel(int channel)
{
    (void)channel;  /* SDL3_mixer doesn't expose old channel ids; stop all SFX. */
    if (!g_mixer) return;
    for (int i = 0; i < SFX_TRACK_POOL; i++)
        if (g_sfx_tracks[i]) MIX_StopTrack(g_sfx_tracks[i], 0);
}

void T4K_AudioMusicLoad(char* music_path, int loops)
{
    if (!music_path || !g_audio_on) return;
    if (!t4k_audio_ensure()) return;
    Mix_Music* m = T4K_LoadMusic(music_path);
    if (m) T4K_AudioMusicPlay(m, loops);
}

void T4K_AudioMusicUnload(void)
{
    if (!g_music_track) return;
    MIX_StopTrack(g_music_track, 0);
    /* We don't own the audio — the caller (or T4K_AudioMusicLoad) loaded
     * it via T4K_LoadMusic and is responsible for freeing. Leaking a
     * music object is preferable to a double-free here. */
}

bool T4K_IsPlayingMusic(void)
{
    return g_music_track && MIX_TrackPlaying(g_music_track);
}

void T4K_AudioMusicPlay(Mix_Music* music, int loops)
{
    if (!music || !g_audio_on) return;
    if (!t4k_audio_ensure()) return;
    MIX_StopTrack(g_music_track, 0);
    MIX_SetTrackAudio(g_music_track, music);
    g_music_loops = loops;
    t4k_play_with_loops(g_music_track, loops);
}

void T4K_AudioEnable(bool enabled)
{
    if (g_audio_on == enabled) return;
    g_audio_on = enabled;
    if (g_mixer) MIX_SetMixerGain(g_mixer, enabled ? 1.0f : 0.0f);
}

/* Volume APIs. Tuxtype's UI uses 0..128 (the SDL2_mixer convention).
 * SDL3_mixer uses 0.0..1.0 gain on tracks. */
static float vol_to_gain(int vol)
{
    if (vol <= 0) return 0.0f;
    if (vol >= 128) return 1.0f;
    return (float)vol / 128.0f;
}

void T4K_SetMusicVolume(int volume)
{
    if (!t4k_audio_ensure() || !g_music_track) return;
    MIX_SetTrackGain(g_music_track, vol_to_gain(volume));
}

void T4K_SetSfxVolume(int volume)
{
    if (!t4k_audio_ensure()) return;
    float g = vol_to_gain(volume);
    for (int i = 0; i < SFX_TRACK_POOL; i++)
        if (g_sfx_tracks[i]) MIX_SetTrackGain(g_sfx_tracks[i], g);
}

void T4K_AudioToggle(void)
{
    T4K_AudioEnable(!g_audio_on);
}
