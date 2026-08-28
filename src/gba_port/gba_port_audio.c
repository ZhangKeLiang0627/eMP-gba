/**
 * @file gba_port_audio.c
 * ALSA audio backend for eMP-gba (T113-S3).
 *
 * Adapted from lv_gba_emu (MIT, _VIFEXTech). The original already supports
 * ALSA when LV_USE_SDL is 0: it opens PCM "default", runs a drain thread and
 * feeds the vba-next audio batch through snd_pcm_writei. We only fixed the
 * include path for this project's source layout.
 */

/*********************
 *      INCLUDES
 *********************/

#include "gba_emu.h"
#include "port.h"

#if LV_USE_SDL
#include <SDL2/SDL.h>
#else
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#endif

/*********************
 *      DEFINES
 *********************/

#define PCM_DEVICE "default"

/* The board codec (SUNXI-CODEC) does NOT support the GBA core's native
 * 32768Hz. asound.conf pins the default playback graph (softvol+dmix) at
 * 48kHz, so we resample 32768 -> 48000 in gba_audio_output_cb() and open
 * the PCM at 48kHz. Without this, the plug layer's resampler makes the DAC
 * drain ~46% faster than the core produces audio -> constant underruns. */
#define AUDIO_OUT_RATE 48000

/* ASRC tuning: keep the FIFO near this level (frames) and allow the
 * resampler ratio to drift up to +-5% to match the consumer clock. */
#define ASRC_TARGET_FRAMES 2048
#define ASRC_MAX_ADJ       ((int32_t)((32000LL << 16) / 48000) * 5 / 100) /* ~5% of base step */

/* Analog output distortion limit (25% FS). */
#define AUDIO_LIMIT        8192

#define AUDIO_FIFO_LEN 16384

#if LV_USE_SDL
#define AUDIO_LOCK() SDL_LockAudio()
#define AUDIO_UNLOCK() SDL_UnlockAudio()
#else
#define AUDIO_LOCK()
#define AUDIO_UNLOCK()
#endif

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    int16_t* buffer;
    volatile int head;  /* written by the LVGL thread (producer) only */
    volatile int tail;  /* written by the audio thread (consumer) only */
    int size;
} audio_fifo_t;

typedef struct {
    int sample_rate;
    int volume;          /* 0-100, applied at resample time */
    uint64_t resample_acc; /* 16.16 fixed: global output->input position.
                              MUST be 64-bit: 48000 out/s * 44739 step/s
                              overflows uint32 in ~2s and the wrapped phase
                              made g_idx - in_consumed underflow -> OOB read
                              -> SIGSEGV in gba_audio_output_cb */
    uint64_t in_consumed;  /* global count of input frames already consumed */
    int32_t asrc_avg;      /* EMA of (fifo_frames - target), drives rate match */
    audio_fifo_t fifo;
    int16_t buffer[AUDIO_FIFO_LEN];
#if LV_USE_SDL == 0
    snd_pcm_t* pcm_handle;
    pthread_t thread_id;
    volatile bool running;
#endif
} audio_ctx_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static size_t gba_audio_output_cb(void* user_data, const int16_t* data, size_t frames);
static int audio_init(audio_ctx_t* ctx);
static void audio_deinit(audio_ctx_t* ctx);
static void audio_fifo_init(audio_fifo_t* fifo, int16_t* buffer, int size);

/**********************
 *  STATIC VARIABLES
 **********************/

static audio_ctx_t g_audio_ctx;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

int gba_audio_init(lv_obj_t* gba_emu)
{
    int ret;
    audio_fifo_init(&g_audio_ctx.fifo, g_audio_ctx.buffer, AUDIO_FIFO_LEN);

    int sample_rate = lv_gba_emu_get_audio_sample_rate(gba_emu);
    LV_ASSERT(sample_rate > 0);
    g_audio_ctx.sample_rate = sample_rate;
    g_audio_ctx.volume = 100;
    g_audio_ctx.resample_acc = 0;
    g_audio_ctx.in_consumed = 0;
    g_audio_ctx.asrc_avg = 0;

    ret = audio_init(&g_audio_ctx);
    if (ret < 0) {
        return ret;
    }

    lv_gba_emu_set_audio_output_cb(gba_emu, gba_audio_output_cb, &g_audio_ctx);

    return 0;
}

void gba_audio_deinit(lv_obj_t* gba_emu)
{
    LV_UNUSED(gba_emu);
    audio_deinit(&g_audio_ctx);
}

void gba_audio_set_volume(int volume)
{
    if (volume < 0) {
        volume = 0;
    }
    if (volume > 100) {
        volume = 100;
    }
    g_audio_ctx.volume = volume;
    LV_LOG_USER("audio volume = %d", volume);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void audio_fifo_init(audio_fifo_t* fifo, int16_t* buffer, int size)
{
    memset(fifo, 0, sizeof(audio_fifo_t));
    fifo->buffer = buffer;
    fifo->size = size;
}

static bool audio_fifo_write(audio_fifo_t* fifo, int16_t data)
{
    int i = (fifo->head + 1) % fifo->size;

    if (i == fifo->tail) {
        return false;
    }

    fifo->buffer[fifo->head] = data;
    /* Publish the sample before the consumer can see the new head.
     * Cortex-A7 is weakly-ordered; without this fence (and volatile) the
     * audio thread may read a stale head and never drain the FIFO. */
    __sync_synchronize();
    fifo->head = i;
    return true;
}

static int16_t audio_fifo_read(audio_fifo_t* fifo)
{
    if (fifo->head == fifo->tail) {
        return 0;
    }

    int16_t data = fifo->buffer[fifo->tail];
    /* Consume the sample before releasing the slot to the producer. */
    __sync_synchronize();
    fifo->tail = (fifo->tail + 1) % fifo->size;
    return data;
}

static int audio_fifo_avaliable(audio_fifo_t* fifo)
{
    return (fifo->size + fifo->head - fifo->tail) % fifo->size;
}

#if LV_USE_SDL

static void sdl_audio_callback(void* user_data, uint8_t* stream, int len)
{
    audio_ctx_t* ctx = user_data;
    audio_fifo_t* fifo = &ctx->fifo;
    uint16_t* wr_ptr = (uint16_t*)stream;

    int samples = len / 2;
    int avaliable = audio_fifo_avaliable(fifo);

    if (avaliable < samples) {
        LV_LOG_INFO("audio under run: %d < %d", avaliable, samples);
    }

    for (int i = 0; i < samples; i++) {
        *wr_ptr++ = audio_fifo_read(fifo);
    }
}

static int audio_init(audio_ctx_t* ctx)
{
    SDL_AudioSpec audio_spec;
    memset(&audio_spec, 0, sizeof(audio_spec));
    audio_spec.freq = ctx->sample_rate;
    audio_spec.format = AUDIO_S16SYS;
    audio_spec.channels = 2;
    audio_spec.samples = 1024;
    audio_spec.callback = sdl_audio_callback;
    audio_spec.userdata = ctx;

    int ret = SDL_OpenAudio(&audio_spec, NULL);
    if (ret != 0) {
        LV_LOG_ERROR("SDL_OpenAudio failed: %d", ret);
        return ret;
    }
    SDL_PauseAudio(0);

    return ret;
}

static void audio_deinit(audio_ctx_t* ctx)
{
    SDL_CloseAudio();
}

#else

static void* audio_thread(void* arg)
{
    audio_ctx_t* ctx = arg;
    int16_t buffer[AUDIO_FIFO_LEN];
    audio_fifo_t* fifo = &ctx->fifo;
    /* Silence filler: when the producer hiccups (a heavy GBA frame stalls the
     * audio callback) the FIFO may run dry; feeding silence keeps the PCM
     * buffer from draining, avoiding snd_pcm_recover underrun loops. */
    static const int16_t silence[512 * 2] = { 0 };
    bool primed = false;

    while (ctx->running) {
        int avaliable = audio_fifo_avaliable(fifo);
        avaliable &= ~1; /* keep an even sample count: FIFO holds L/R pairs */

        if (avaliable > 0) {
            /* Prime: wait until we have a chunk before starting playback so
             * the dmix buffer starts with some headroom against jitter. */
            if (!primed && avaliable < 1024) {
                usleep(1000);
                continue;
            }
            primed = true;

            for (int i = 0; i < avaliable; i++) {
                buffer[i] = audio_fifo_read(fifo);
            }

            int channels = 2;
            int bytes_per_sample = 2; /* 16 bit */
            int buffer_size = avaliable * sizeof(int16_t);
            int frames = buffer_size / (channels * bytes_per_sample);
            snd_pcm_sframes_t frames_written;
            frames_written = snd_pcm_writei(ctx->pcm_handle, buffer, frames);
            if (frames_written < 0) {
                LV_LOG_ERROR("frames = %d, Write error: %s", frames, snd_strerror(frames_written));
                snd_pcm_recover(ctx->pcm_handle, frames_written, 0);
            } else if (frames_written != frames) {
                LV_LOG_WARN("Short write, expected %d frames but wrote %ld", avaliable, frames_written);
            }
        }
        else if (primed) {
            /* Producer gap: the core feeds audio in ~16.7ms batches and
             * scheduler jitter can leave the FIFO momentarily empty between
             * them. Filling silence immediately on every empty poll chopped
             * the stream into tiny gaps (audible as garbled/harsh music).
             * Wait a few ms first; only fill silence when the gap is real. */
            usleep(3000);
            int again = audio_fifo_avaliable(fifo) & ~1;
            if (again <= 0) {
                snd_pcm_sframes_t wr = snd_pcm_writei(ctx->pcm_handle, silence, 512);
                if (wr < 0) {
                    snd_pcm_recover(ctx->pcm_handle, wr, 0);
                }
            }
        }

        usleep(100);
    }
    return NULL;
}

/* Set a mixer playback volume element to a given value if found. */
static void mixer_set_volume(snd_mixer_t* mixer, const char* name, long value)
{
    snd_mixer_selem_id_t* sid = NULL;
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_name(sid, name);
    snd_mixer_elem_t* elem = snd_mixer_find_selem(mixer, sid);
    if (elem) {
        long min, max;
        snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        if (value > max) value = max;
        if (value < min) value = min;
        int rc = snd_mixer_selem_set_playback_volume_all(elem, value);
        if (rc != 0) {
            LV_LOG_WARN("set %s=%ld failed: %s", name, value, snd_strerror(rc));
        }
    }
    else {
        LV_LOG_WARN("%s control not found", name);
    }
}

/* The T113 codec boots with the output path muted and the headphone/DAC
 * gains low ('Headphone volume' defaults to 4/7 = -18dB, 'DAC volume' to
 * 160/255). The GBA core's audio is also dynamically quiet, so on top of
 * unmuting we push the codec gains to 0dB max for a clearly audible level. */
static void audio_setup_codec(void)
{
    snd_mixer_t* mixer = NULL;
    int ret = snd_mixer_open(&mixer, 0);
    if (ret < 0) {
        LV_LOG_WARN("snd_mixer_open failed: %s", snd_strerror(ret));
        return;
    }

    ret = snd_mixer_attach(mixer, "default");
    if (ret < 0) {
        LV_LOG_WARN("snd_mixer_attach failed: %s", snd_strerror(ret));
        snd_mixer_close(mixer);
        return;
    }

    snd_mixer_selem_register(mixer, NULL, NULL);
    snd_mixer_load(mixer);

    /* Unmute the output path. */
    {
        snd_mixer_selem_id_t* sid = NULL;
        snd_mixer_selem_id_alloca(&sid);
        snd_mixer_selem_id_set_name(sid, "Headphone Switch");
        snd_mixer_elem_t* elem = snd_mixer_find_selem(mixer, sid);
        if (elem) {
            snd_mixer_selem_set_playback_switch_all(elem, 1);
            LV_LOG_USER("codec Headphone Switch -> on (unmuted)");
        }
        else {
            LV_LOG_WARN("Headphone Switch control not found");
        }
    }

    /* Push the gains to leave headroom: full 0dB (255 / 7) made bass-heavy
     * transients clip in the analog output stage -> "garbled mic" noise on
     * drums/bass while quiet highs stayed clean. Digital-domain gain in
     * gba_audio_output_cb compensates the loudness. Softvol Master stays as
     * the user-controlled volume knob via EMP_GBA_VOLUME. */
    mixer_set_volume(mixer, "Headphone volume", 5);
    mixer_set_volume(mixer, "DAC volume", 220);

    snd_mixer_close(mixer);
}

static int audio_init(audio_ctx_t* ctx)
{
    int ret;
    const char* device = getenv("LV_GBA_AUDIO_DEVICE");
    if (!device) {
        device = PCM_DEVICE;
    }

    /* Initialize ALSA PCM handle */
    ret = snd_pcm_open(&ctx->pcm_handle, device, SND_PCM_STREAM_PLAYBACK, 0);
    if (ret < 0) {
        LV_LOG_ERROR("Failed to open PCM device: %s, error: %s", device, snd_strerror(ret));
        return ret;
    }

    LV_LOG_USER("pcm_handle = %p, in_rate = %d, out_rate = %d",
                ctx->pcm_handle, ctx->sample_rate, AUDIO_OUT_RATE);
    int channels = 2;

    /* Set hardware parameters; audio is resampled to 48kHz before it lands
     * in the FIFO (see gba_audio_output_cb), so open the PCM at 48kHz. */
    ret = snd_pcm_set_params(ctx->pcm_handle,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        channels,
        AUDIO_OUT_RATE,
        1,
        300000);
    if (ret < 0) {
        LV_LOG_ERROR("Unable to set PCM parameters: %s", snd_strerror(ret));
        snd_pcm_close(ctx->pcm_handle);
        ctx->pcm_handle = NULL;
        return ret;
    }

    audio_setup_codec();

    ctx->running = true;
    ret = pthread_create(&ctx->thread_id, NULL, audio_thread, ctx);
    LV_ASSERT_MSG(ret == 0, "pthread_create failed");
    return ret;
}

static void audio_deinit(audio_ctx_t* ctx)
{
    if (ctx->running) {
        ctx->running = false;
        pthread_join(ctx->thread_id, NULL);
    }

    if (ctx->pcm_handle) {
        snd_pcm_close(ctx->pcm_handle);
        ctx->pcm_handle = NULL;
    }
}

#endif

static size_t gba_audio_output_cb(void* user_data, const int16_t* data, size_t frames)
{
    audio_ctx_t* ctx = user_data;
    audio_fifo_t* fifo = &ctx->fifo;
    size_t written_frames = 0;
    const uint64_t in_start = ctx->in_consumed;

    /* ASRC (adaptive rate matching): the GBA audio clock follows the emulated
     * frame rate, which dips below 60 on heavy ROMs (Pokemon Emerald ~57fps).
     * Feeding a fixed 48k rate then drains the FIFO periodically -> silence
     * filler -> garbled/harsh music. We nudge the resampler ratio from the
     * FIFO level (negative feedback, clamped to +-5%) so production follows
     * the consumer clock smoothly; the music gets slightly slower instead of
     * chopped.
     * NOTE the sign: FIFO low (production behind) -> step smaller -> more
     * output frames -> refill. */
    const int64_t base_step = ((int64_t)ctx->sample_rate << 16) / AUDIO_OUT_RATE;
    int32_t fifo_frames = (int32_t)(audio_fifo_avaliable(fifo) >> 1);
    ctx->asrc_avg = ((int64_t)ctx->asrc_avg * 31 + (int64_t)(fifo_frames - ASRC_TARGET_FRAMES)) / 32;
    int32_t adj = (int32_t)(((int64_t)ctx->asrc_avg * 3));
    if (adj > ASRC_MAX_ADJ) adj = ASRC_MAX_ADJ;
    if (adj < -ASRC_MAX_ADJ) adj = -ASRC_MAX_ADJ;
    const uint64_t step = (uint64_t)(base_step + adj);
    const uint64_t in_end = ctx->in_consumed + (uint64_t)frames;

    while (1) {
        uint64_t g_idx = ctx->resample_acc >> 16;
        if (g_idx >= in_end) {
            break;
        }
        uint32_t frac = (uint32_t)(ctx->resample_acc & 0xFFFF);
        uint64_t local = g_idx - ctx->in_consumed;
        if (local >= frames) {
            break; /* defensive; must not happen */
        }
        if (local + 1 >= frames) {
            /* Batch boundary: interpolating between the last sample of this
             * batch and the first of the next would need the next batch's
             * data; holding l0 instead creates a slope kink every batch
             * (~60 clicks/s, audible as hiss/snow). Skip the boundary point,
             * the tiny time loss is absorbed by ASRC. */
            break;
        }

        int16_t l0 = data[local * 2];
        int16_t r0 = data[local * 2 + 1];
        int16_t l1 = (local + 1 < frames) ? data[(local + 1) * 2] : l0;
        int16_t r1 = (local + 1 < frames) ? data[(local + 1) * 2 + 1] : r0;

        /* Linear interpolation. NOTE: use 64-bit math for the interpolation
         * term - (l1-l0) can be +/-65535 and frac up to 65535, whose product
         * overflows int32 and produced corrupted samples on busy waveforms
         * (music sounded garbled/distorted while quiet beeps stayed clean). */
        int32_t dl = (int32_t)l1 - (int32_t)l0;
        int32_t dr = (int32_t)r1 - (int32_t)r0;
        int16_t lo = (int16_t)((int32_t)l0 + (int16_t)(((int64_t)dl * (int32_t)frac) >> 16));
        int16_t ro = (int16_t)((int32_t)r0 + (int16_t)(((int64_t)dr * (int32_t)frac) >> 16));

        /* Digital-domain gain (x1.6) to compensate the codec headroom left
         * in audio_setup_codec (DAC 220, Headphone 5/7 instead of full 0dB):
         * loudness stays adequate while bass transients no longer clip in
         * the analog output stage. GBA peaks are ~35% FS, x1.6 -> ~56%, safe. */
        int32_t lo32 = ((int32_t)lo * 8) / 5;
        int32_t ro32 = ((int32_t)ro * 8) / 5;

        if (ctx->volume < 100) {
            lo32 = lo32 * ctx->volume / 100;
            ro32 = ro32 * ctx->volume / 100;
        }

        /* Soft limiter: the board's analog output chain (codec -> amp ->
         * headphone) audibly distorts above ~25-30% FS. Verified on board:
         * EMP_GBA_VOLUME=5 (peak ~8% FS) sounds clean, high levels sound
         * harsh/hissy. Clamp the peak so ANY volume setting stays in the
         * clean zone; 100% volume then yields ~3x the loudness of the
         * VOLUME=5 experiment while remaining distortion-free. */
        if (lo32 > AUDIO_LIMIT) lo32 = AUDIO_LIMIT;
        else if (lo32 < -AUDIO_LIMIT) lo32 = -AUDIO_LIMIT;
        if (ro32 > AUDIO_LIMIT) ro32 = AUDIO_LIMIT;
        else if (ro32 < -AUDIO_LIMIT) ro32 = -AUDIO_LIMIT;
        lo = (int16_t)lo32;
        ro = (int16_t)ro32;

        AUDIO_LOCK();
        bool ok = audio_fifo_write(fifo, lo) && audio_fifo_write(fifo, ro);
        AUDIO_UNLOCK();
        if (!ok) {
            break;
        }
        written_frames++;
        ctx->resample_acc += step;
    }

    /* Advance the global input cursor only by what this callback consumed.
     * Return the consumed input frames (<= frames), per the libretro
     * audio_sample_batch contract. */
    uint64_t consumed = ctx->resample_acc >> 16;
    if (consumed < in_start) {
        consumed = in_start;
    }
    if (consumed > in_end) {
        consumed = in_end;
    }
    ctx->in_consumed = consumed;
    return consumed - in_start;
}
