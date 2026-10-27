#ifndef GF_AUDIO_H
#define GF_AUDIO_H

#include <stdbool.h>

/*
 * Software mixer on a background thread, output through ALSA ("default").
 * Sounds are decoded to 44.1 kHz stereo 16-bit at load time, so mixing is
 * cheap. Supports 8/16-bit PCM WAV, mono or stereo, any sample rate.
 * Memory cost is ~176 KB per second of audio -- use short clips on a Pi.
 */
typedef struct gf_sound gf_sound;

gf_sound *gf_sound_load(const char *rel);
void      gf_sound_free(gf_sound *s);

/* Returns a voice handle (> 0) or -1 if audio is off / all voices busy. */
int  gf_sound_play(const gf_sound *s, float volume, bool loop);
void gf_voice_stop(int voice);
void gf_voice_set_volume(int voice, float volume);
void gf_audio_set_master(float volume);

/* Called by the core loop. Failure is non-fatal: the game runs silent. */
bool gf_audio_init(void);
void gf_audio_shutdown(void);

#endif
