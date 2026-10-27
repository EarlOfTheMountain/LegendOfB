#include "gf_audio.h"
#include "gf_assets.h"
#include "gf_log.h"

#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RATE        44100
#define PERIOD      1024        /* frames per mixer pass (~23 ms) */
#define NUM_VOICES  16
#define LATENCY_US  80000       /* raise if you hear crackles under load */

struct gf_sound {
    int16_t *data;              /* interleaved stereo, RATE Hz */
    size_t   frames;
};

typedef struct {
    const gf_sound *snd;
    size_t   pos;
    int      vol;               /* 0..256 */
    bool     loop;
    bool     active;
    uint32_t gen;
} voice;

static struct {
    snd_pcm_t      *pcm;
    pthread_t       th;
    pthread_mutex_t mu;
    voice           v[NUM_VOICES];
    volatile int    run;
    int             master;     /* 0..256 */
    uint32_t        gen;
    bool            up;
    bool            mu_init;
} A;

static int to_fixed(float v)
{
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return (int)(v * 256.0f + 0.5f);
}

static void *mix_main(void *arg)
{
    static int32_t acc[PERIOD * 2];
    static int16_t out[PERIOD * 2];
    (void)arg;

    while (A.run) {
        memset(acc, 0, sizeof acc);

        pthread_mutex_lock(&A.mu);
        for (int i = 0; i < NUM_VOICES; i++) {
            voice *v = &A.v[i];
            if (!v->active) continue;
            const int vol = (v->vol * A.master) >> 8;
            int o = 0;
            while (o < PERIOD && v->active) {
                size_t left = v->snd->frames - v->pos;
                int n = PERIOD - o;
                if ((size_t)n > left) n = (int)left;
                const int16_t *s = v->snd->data + v->pos * 2;
                int32_t *a = acc + o * 2;
                for (int k = 0; k < n * 2; k++) a[k] += (s[k] * vol) >> 8;
                o += n;
                v->pos += (size_t)n;
                if (v->pos >= v->snd->frames) {
                    if (v->loop) v->pos = 0;
                    else v->active = false;
                }
            }
        }
        pthread_mutex_unlock(&A.mu);

        for (int k = 0; k < PERIOD * 2; k++) {
            int32_t s = acc[k];
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            out[k] = (int16_t)s;
        }

        snd_pcm_sframes_t r = snd_pcm_writei(A.pcm, out, PERIOD);
        if (r < 0) r = snd_pcm_recover(A.pcm, (int)r, 1);
        if (r < 0) usleep(10000);
    }
    return NULL;
}

bool gf_audio_init(void)
{
    memset(&A, 0, sizeof A);
    A.master = 256;
    pthread_mutex_init(&A.mu, NULL);
    A.mu_init = true;

    int err = snd_pcm_open(&A.pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        GF_LOGW("audio disabled: snd_pcm_open: %s", snd_strerror(err));
        A.pcm = NULL;
        return false;
    }
    err = snd_pcm_set_params(A.pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                             2, RATE, 1, LATENCY_US);
    if (err < 0) {
        GF_LOGW("audio disabled: snd_pcm_set_params: %s", snd_strerror(err));
        snd_pcm_close(A.pcm);
        A.pcm = NULL;
        return false;
    }

    A.run = 1;
    if (pthread_create(&A.th, NULL, mix_main, NULL) != 0) {
        GF_LOGW("audio disabled: cannot start mixer thread");
        snd_pcm_close(A.pcm);
        A.pcm = NULL;
        A.run = 0;
        return false;
    }
    A.up = true;
    return true;
}

void gf_audio_shutdown(void)
{
    if (A.up) {
        A.run = 0;
        pthread_join(A.th, NULL);
        snd_pcm_drop(A.pcm);
        snd_pcm_close(A.pcm);
        A.pcm = NULL;
        A.up = false;
    }
    if (A.mu_init) {
        pthread_mutex_destroy(&A.mu);
        A.mu_init = false;
    }
}

/* ---- WAV loading ------------------------------------------------------ */

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static float sample_at(const uint8_t *d, int bits, int ch, size_t frame, int c)
{
    const uint8_t *p = d + (frame * (size_t)ch + (size_t)c) * (size_t)(bits / 8);
    if (bits == 16) return (float)(int16_t)rd16(p);
    return (float)(((int)p[0] - 128) << 8);
}

gf_sound *gf_sound_load(const char *rel)
{
    size_t n = 0;
    uint8_t *d = gf_asset_read(rel, &n);
    if (!d) return NULL;

    if (n < 12 || memcmp(d, "RIFF", 4) != 0 || memcmp(d + 8, "WAVE", 4) != 0) {
        GF_LOGE("'%s' is not a WAV file", rel);
        free(d);
        return NULL;
    }

    unsigned fmt = 0, ch = 0, bits = 0;
    uint32_t rate = 0;
    const uint8_t *data = NULL;
    size_t dlen = 0;

    size_t p = 12;
    while (p + 8 <= n) {
        size_t size = rd32(d + p + 4);
        const uint8_t *c = d + p + 8;
        if (size > n - p - 8) size = n - p - 8;     /* tolerate bogus sizes */
        if (memcmp(d + p, "fmt ", 4) == 0 && size >= 16) {
            fmt = rd16(c);
            ch = rd16(c + 2);
            rate = rd32(c + 4);
            bits = rd16(c + 14);
        } else if (memcmp(d + p, "data", 4) == 0) {
            data = c;
            dlen = size;
        }
        p += 8 + size + (size & 1);
    }

    if ((fmt != 1 && fmt != 0xFFFE) || (ch != 1 && ch != 2) ||
        (bits != 8 && bits != 16) || rate == 0 || !data) {
        GF_LOGE("'%s': unsupported WAV (need 8/16-bit PCM, mono/stereo)", rel);
        free(d);
        return NULL;
    }

    size_t in_frames = dlen / (ch * (bits / 8));
    size_t out_frames = (size_t)(((uint64_t)in_frames * RATE) / rate);
    if (out_frames == 0) {
        GF_LOGE("'%s' contains no audio", rel);
        free(d);
        return NULL;
    }

    gf_sound *s = malloc(sizeof *s);
    int16_t *out = malloc(out_frames * 2 * sizeof(int16_t));
    if (!s || !out) { free(s); free(out); free(d); return NULL; }

    for (size_t i = 0; i < out_frames; i++) {
        size_t i0;
        float f;
        if (rate == RATE) { i0 = i; f = 0.0f; }
        else {
            double pos = (double)i * (double)rate / (double)RATE;
            i0 = (size_t)pos;
            f = (float)(pos - (double)i0);
        }
        size_t i1 = (i0 + 1 < in_frames) ? i0 + 1 : i0;
        for (int c = 0; c < 2; c++) {
            int src_c = ch > 1 ? c : 0;
            float a = sample_at(data, (int)bits, (int)ch, i0, src_c);
            float b = (f > 0.0f) ? sample_at(data, (int)bits, (int)ch, i1, src_c) : a;
            out[i * 2 + (size_t)c] = (int16_t)(a + (b - a) * f);
        }
    }

    free(d);
    s->data = out;
    s->frames = out_frames;
    return s;
}

void gf_sound_free(gf_sound *s)
{
    if (!s) return;
    if (A.mu_init) {
        pthread_mutex_lock(&A.mu);
        for (int i = 0; i < NUM_VOICES; i++)
            if (A.v[i].snd == s) { A.v[i].active = false; A.v[i].snd = NULL; }
        pthread_mutex_unlock(&A.mu);
    }
    free(s->data);
    free(s);
}

/* ---- playback control ------------------------------------------------- */

int gf_sound_play(const gf_sound *s, float volume, bool loop)
{
    int handle = -1;
    if (!A.up || !s) return -1;

    pthread_mutex_lock(&A.mu);
    for (int i = 0; i < NUM_VOICES; i++) {
        voice *v = &A.v[i];
        if (v->active) continue;
        if (++A.gen == 0 || A.gen > 0x7fffffu) A.gen = 1;
        v->snd = s;
        v->pos = 0;
        v->vol = to_fixed(volume);
        v->loop = loop;
        v->gen = A.gen;
        v->active = true;
        handle = (int)((A.gen << 8) | (uint32_t)i);
        break;
    }
    pthread_mutex_unlock(&A.mu);
    return handle;
}

static voice *find_voice(int handle)
{
    if (handle <= 0) return NULL;
    int idx = handle & 0xff;
    uint32_t gen = (uint32_t)handle >> 8;
    if (idx >= NUM_VOICES) return NULL;
    voice *v = &A.v[idx];
    return (v->active && v->gen == gen) ? v : NULL;
}

void gf_voice_stop(int handle)
{
    if (!A.up) return;
    pthread_mutex_lock(&A.mu);
    voice *v = find_voice(handle);
    if (v) v->active = false;
    pthread_mutex_unlock(&A.mu);
}

void gf_voice_set_volume(int handle, float volume)
{
    if (!A.up) return;
    pthread_mutex_lock(&A.mu);
    voice *v = find_voice(handle);
    if (v) v->vol = to_fixed(volume);
    pthread_mutex_unlock(&A.mu);
}

void gf_audio_set_master(float volume)
{
    A.master = to_fixed(volume);
}
