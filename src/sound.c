#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sound.h"
#include "utils.h"

#pragma pack(push, 1)
typedef struct
{
    uint32_t offset;
    uint32_t size;
    uint32_t loopStart;
    uint32_t loopEnd;
    uint16_t phaseInc;
    uint16_t Vnorm;
    uint16_t rkon;
    uint16_t rkoff;
    uint16_t ramping;
    uint16_t type;
    uint32_t padding;
} sample_header_t;
#pragma pack(pop)

#define SAMPLE_TYPE_16BIT 2
#define SAMPLE_TYPE_ADPCM 4

#define MIX_SAMPLE_RATE 48000

typedef struct
{
    int16_t *data;
    uint32_t count; /* samples, not bytes */
} decoded_sample_t;

typedef struct
{
    int active;
    int loop;
    int sample;
    double position;
    double freq_ratio;
    float volume;
    float pan_left;
    float pan_right;
} channel_t;

static uint8_t *rom_data = NULL;
static uint32_t rom_size = 0;
static uint32_t sample_count = 0;

static decoded_sample_t samples[SOUND_MAX_SAMPLES];
static channel_t channels[SOUND_CHANNELS];

static float master_volume = 1.0f;
static float master_left = 1.0f;
static float master_right = 1.0f;
static int muted = 0;

static SDL_AudioDeviceID audio_device = 0;
static int sound_ready = 0;

/* First table lookup for IMA-ADPCM quantizer */
static const int8_t index_adjust[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

/* Second table lookup for IMA-ADPCM quantizer */
static const int16_t step_size[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

static void decode_adpcm_sample(uint32_t sample)
{
    const sample_header_t *header = (const sample_header_t *)rom_data + sample;

    if (header->phaseInc == 0)
    {
        return;
    }

    uint32_t phaseEndLO = header->size << 12;
    uint32_t phaseEndHI = header->size >> 20;
    uint64_t decodedSamples = (uint64_t)header->size * 0x1000 / header->phaseInc;

    if (decodedSamples == 0)
    {
        return;
    }

    int16_t *out = malloc(decodedSamples * sizeof(int16_t));

    if (!out)
    {
        return;
    }

    uint32_t phaseLO = 0;
    uint32_t phaseHI = 0;
    int16_t data0 = 0;
    int16_t data1 = 0;
    uint8_t aindex = 0;
    int next = 0;
    uint32_t cursor = 0;
    int skipFirst = 1;
    int decoding = 1;

    while (decoding)
    {
        uint32_t ph0 = phaseLO;
        uint32_t ph1;
        int32_t data;

        if (!next)
        {
            uint32_t idx = (ph0 >> 12) + (phaseHI << 20);
            const uint8_t *pp = rom_data + header->offset;
            int32_t raw = pp[idx >> 1];

            if (!(idx & 1))
            {
                raw >>= 4;
            }

            int32_t delta = raw & 0x0f;
            int32_t index = aindex;
            int32_t step = step_size[index];
            int32_t sign = delta & 8;

            delta &= 7;
            index += index_adjust[delta];

            if (index < 0)
            {
                index = 0;
            }
            if (index > 88)
            {
                index = 88;
            }

            int32_t vout = data0;
            int32_t vpdiff = step >> 3;

            if (delta & 4)
            {
                vpdiff += step;
            }
            if (delta & 2)
            {
                vpdiff += step >> 1;
            }
            if (delta & 1)
            {
                vpdiff += step >> 2;
            }

            if (sign)
            {
                vout -= vpdiff;
            }
            else
            {
                vout += vpdiff;
            }

            if (vout > 32767)
            {
                vout = 32767;
            }
            else if (vout < -32768)
            {
                vout = -32768;
            }

            aindex = (uint8_t)index;
            data1 = (int16_t)vout;
        }

        int32_t d0 = (int32_t)(ph0 & 0xFFF);
        data = data0 + (((data1 - data0) * d0) >> 12);
        data = (data * 0x1000) >> 12;

        ph1 = ph0 + header->phaseInc;
        if ((0xFFFFFFFFu - ph0) < header->phaseInc)
        {
            phaseHI++;
        }
        phaseLO = ph1;

        if (!((ph0 ^ ph1) & 0x1000))
        {
            next = 1;
        }
        else
        {
            next = 0;
            data0 = data1;
        }

        if ((ph1 >= phaseEndLO) && (phaseHI >= phaseEndHI))
        {
            decoding = 0;
        }

        if (data < -0x7FFF)
        {
            data = -0x7FFF;
        }
        else if (data > 0x7FFF)
        {
            data = 0x7FFF;
        }

        if (!skipFirst && cursor < decodedSamples)
        {
            out[cursor] = (int16_t)data;
            cursor++;
        }

        skipFirst = 0;
    }

    samples[sample].data = out;
    samples[sample].count = cursor;
}

static void decode_16bit_sample(uint32_t sample)
{
    const sample_header_t *header = (const sample_header_t *)rom_data + sample;

    if (header->phaseInc == 0)
    {
        return;
    }

    uint32_t phaseEnd = header->size << 12;
    uint32_t decodedSamples = phaseEnd / header->phaseInc;

    if (decodedSamples == 0)
    {
        return;
    }

    int16_t *out = malloc(decodedSamples * sizeof(int16_t));

    if (!out)
    {
        return;
    }

    uint32_t phase = 0;
    uint32_t cursor = 0;
    int skipFirst = 1;
    int decoding = 1;

    while (decoding)
    {
        uint32_t idx = (phase >> 12) << 1;
        int16_t d0;
        int16_t d1;
        int32_t data;

        memcpy(&d0, rom_data + header->offset + idx, sizeof(d0));

        if ((idx + 2) < header->size * 2)
        {
            memcpy(&d1, rom_data + header->offset + idx + 2, sizeof(d1));
        }
        else
        {
            d1 = d0;
        }

        data = d0 + (((d1 - d0) * (int32_t)(phase & 0xFFF)) >> 12);
        data = (data * 0x1000) >> 12;
        phase += header->phaseInc;

        if (phase >= phaseEnd)
        {
            decoding = 0;
        }

        if (data < -0x7FFF)
        {
            data = -0x7FFF;
        }
        else if (data > 0x7FFF)
        {
            data = 0x7FFF;
        }

        if (!skipFirst && cursor < decodedSamples)
        {
            out[cursor] = (int16_t)data;
            cursor++;
        }

        skipFirst = 0;
    }

    samples[sample].data = out;
    samples[sample].count = cursor;
}

static void mix_audio(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;

    int16_t *out = (int16_t *)stream;
    int frames = len / (int)sizeof(int16_t) / 2;

    memset(stream, 0, (size_t)len);

    if (muted)
    {
        return;
    }

    for (int c = 0; c < SOUND_CHANNELS; c++)
    {
        channel_t *ch = &channels[c];

        if (!ch->active)
        {
            continue;
        }

        if (ch->sample < 0 || ch->sample >= (int)SOUND_MAX_SAMPLES || !samples[ch->sample].data)
        {
            ch->active = 0;
            continue;
        }

        const decoded_sample_t *s = &samples[ch->sample];
        float vl = master_volume * master_left * ch->volume * ch->pan_left;
        float vr = master_volume * master_right * ch->volume * ch->pan_right;

        for (int i = 0; i < frames; i++)
        {
            if (ch->position >= (double)s->count)
            {
                if (ch->loop && s->count > 0)
                {
                    ch->position -= (double)s->count;
                }
                else
                {
                    ch->active = 0;
                    break;
                }
            }

            uint32_t idx = (uint32_t)ch->position;
            int16_t sample_value = s->data[idx];

            int32_t left = out[i * 2 + 0] + (int32_t)(sample_value * vl);
            int32_t right = out[i * 2 + 1] + (int32_t)(sample_value * vr);

            if (left > 32767)
            {
                left = 32767;
            }
            else if (left < -32768)
            {
                left = -32768;
            }

            if (right > 32767)
            {
                right = 32767;
            }
            else if (right < -32768)
            {
                right = -32768;
            }

            out[i * 2 + 0] = (int16_t)left;
            out[i * 2 + 1] = (int16_t)right;

            ch->position += ch->freq_ratio;
        }
    }
}

static int try_load_rom(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (!f)
    {
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0)
    {
        fclose(f);
        return 0;
    }

    uint8_t *buffer = malloc((size_t)size);

    if (!buffer)
    {
        fclose(f);
        return 0;
    }

    size_t got = fread(buffer, 1, (size_t)size, f);
    fclose(f);

    if (got != (size_t)size)
    {
        free(buffer);
        return 0;
    }

    rom_data = buffer;
    rom_size = (uint32_t)size;

    debug("[sound] loaded roms.new from %s (%u bytes)\n", path, rom_size);

    return 1;
}

int SW_init_sound_hook(void *self, const char *path)
{
    (void)self;

    if (sound_ready)
    {
        return 1;
    }

    if (!path || !try_load_rom(path))
    {
        char alt[4096];

        snprintf(alt, sizeof(alt), "%s", "roms.new");

        if (!try_load_rom(alt))
        {
            snprintf(alt, sizeof(alt), "%s", "salidas/roms.new");

            if (!try_load_rom(alt))
            {
                const char *env = getenv("GAELCO_ROMS_NEW");

                if (!env || !try_load_rom(env))
                {
                    fprintf(stderr, "[sound] failed to load roms.new (tried \"%s\", \"roms.new\", \"salidas/roms.new\", $GAELCO_ROMS_NEW)\n",
                            path ? path : "(null)");
                    return 0;
                }
            }
        }
    }

    if (rom_size < sizeof(uint32_t))
    {
        fprintf(stderr, "[sound] roms.new too small\n");
        return 0;
    }

    uint32_t header_bytes;
    memcpy(&header_bytes, rom_data, sizeof(header_bytes));
    sample_count = header_bytes / (uint32_t)sizeof(sample_header_t);

    if (sample_count > SOUND_MAX_SAMPLES)
    {
        fprintf(stderr, "[sound] roms.new has %u samples, clamping to %d\n", sample_count, SOUND_MAX_SAMPLES);
        sample_count = SOUND_MAX_SAMPLES;
    }

    debug("[sound] sample_count=%u\n", sample_count);

    for (uint32_t i = 0; i < sample_count; i++)
    {
        const sample_header_t *header = (const sample_header_t *)rom_data + i;

        if (header->size == 0)
        {
            continue;
        }

        if ((uint64_t)header->offset + header->size > rom_size)
        {
            continue;
        }

        if (header->type == SAMPLE_TYPE_16BIT)
        {
            decode_16bit_sample(i);
        }
        else if (header->type == SAMPLE_TYPE_ADPCM)
        {
            decode_adpcm_sample(i);
        }
    }

    SDL_AudioSpec want;
    SDL_AudioSpec have;

    memset(&want, 0, sizeof(want));
    want.freq = MIX_SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = mix_audio;

    audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);

    if (audio_device == 0)
    {
        fprintf(stderr, "[sound] SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return 0;
    }

    SDL_PauseAudioDevice(audio_device, 0);

    sound_ready = 1;

    return 1;
}

void sound_master_volume_hook(void *self, float volume)
{
    (void)self;

    if (!sound_ready)
    {
        return;
    }

    SDL_LockAudioDevice(audio_device);
    master_volume = volume;
    SDL_UnlockAudioDevice(audio_device);
}

void sound_master_balance_hook(void *self, float left, float right, float subwoofer, float rear)
{
    (void)self;
    (void)subwoofer;
    (void)rear;

    if (!sound_ready)
    {
        return;
    }

    SDL_LockAudioDevice(audio_device);
    master_left = left;
    master_right = right;
    SDL_UnlockAudioDevice(audio_device);
}

int get_channel_status_hook(void *self)
{
    (void)self;

    if (!sound_ready)
    {
        return 0;
    }

    int status = 0;

    SDL_LockAudioDevice(audio_device);

    for (int i = 0; i < SOUND_CHANNELS; i++)
    {
        if (channels[i].active)
        {
            status |= (1 << i);
        }
    }

    SDL_UnlockAudioDevice(audio_device);

    return status;
}

void sound_flush_commbuf_hook(void *self)
{
    (void)self;
}

void sound_start_hook(void *self, int channel, int sample, int flags, float p0, float p1, float p2, float p3)
{
    /* Unused by the game (matches the reference implementation). */
    (void)self;
    (void)channel;
    (void)sample;
    (void)flags;
    (void)p0;
    (void)p1;
    (void)p2;
    (void)p3;
}

void sound_setup_channel_hook(void *self, int channel, int sample, int flags)
{
    (void)self;

    if (!sound_ready || channel < 0 || channel >= SOUND_CHANNELS)
    {
        return;
    }

    if (sample < 0 || sample >= (int)sample_count || !samples[sample].data)
    {
        return;
    }

    SDL_LockAudioDevice(audio_device);

    channels[channel].active = 0;
    channels[channel].sample = sample;
    channels[channel].position = 0.0;
    channels[channel].freq_ratio = 1.0;
    channels[channel].volume = 1.0f;
    channels[channel].pan_left = 1.0f;
    channels[channel].pan_right = 1.0f;
    channels[channel].loop = flags > 0;

    SDL_UnlockAudioDevice(audio_device);
}

void sound_start_channel_hook(void *self, int channel)
{
    (void)self;

    if (!sound_ready || channel < 0 || channel >= SOUND_CHANNELS)
    {
        return;
    }

    SDL_LockAudioDevice(audio_device);
    channels[channel].active = 1;
    SDL_UnlockAudioDevice(audio_device);
}

void sound_freq_hook(void *self, int channel, float phaseInc)
{
    (void)self;

    if (!sound_ready || channel < 0 || channel >= SOUND_CHANNELS)
    {
        return;
    }

    double ratio = phaseInc;

    if (ratio < 0.1)
    {
        ratio = 0.1;
    }
    else if (ratio > 4.0)
    {
        ratio = 4.0;
    }

    SDL_LockAudioDevice(audio_device);
    channels[channel].freq_ratio = ratio;
    SDL_UnlockAudioDevice(audio_device);
}

void sound_panning_hook(void *self, int channel, float left, float right, float subwoofer, float rear)
{
    (void)self;
    (void)subwoofer;
    (void)rear;

    if (!sound_ready || channel < 0 || channel >= SOUND_CHANNELS)
    {
        return;
    }

    SDL_LockAudioDevice(audio_device);
    channels[channel].pan_left = left;
    channels[channel].pan_right = right;
    SDL_UnlockAudioDevice(audio_device);
}

void sound_volrate_hook(void *self, int channel, float volume, float rate)
{
    (void)rate;

    sound_volume_hook(self, channel, volume);
}

void sound_volume_hook(void *self, int channel, float volume)
{
    (void)self;

    if (!sound_ready || channel < 0 || channel >= SOUND_CHANNELS)
    {
        return;
    }

    SDL_LockAudioDevice(audio_device);
    channels[channel].volume = volume;
    SDL_UnlockAudioDevice(audio_device);
}

void sound_keyoff_hook(void *self, float rate, int channelMask)
{
    /* No-op, matching the reference implementation (no volume ramping). */
    (void)self;
    (void)rate;
    (void)channelMask;
}

void sound_stop_hook(void *self, int channelMask)
{
    (void)self;

    if (!sound_ready)
    {
        return;
    }

    SDL_LockAudioDevice(audio_device);

    for (int i = 0; i < SOUND_CHANNELS; i++)
    {
        if ((channelMask >> i) & 1)
        {
            channels[i].active = 0;
        }
    }

    SDL_UnlockAudioDevice(audio_device);
}

void sound_reset_hook(void *self)
{
    sound_stop_hook(self, (1 << SOUND_CHANNELS) - 1);
}

/*
 * Ring Riders only: G3Dintf_sound::sound_mute(int) - on the cabinet this
 * flipped the amplifier mute relay. Just gate the mixer.
 */
void sound_mute_hook(void *self, int mute)
{
    (void)self;

    if (!sound_ready)
    {
        return;
    }

    SDL_LockAudioDevice(audio_device);
    muted = mute != 0;
    SDL_UnlockAudioDevice(audio_device);
}

int commbufferReady_hook(void *self)
{
    (void)self;

    return sound_ready;
}

/* ===================== Championship Tuning Race wrappers =====================
 *
 * CTR's Sound:: methods have no implicit `this`, so they cannot be detoured
 * straight onto the *_hook functions above (those expect `self` in the first
 * stack slot and would misread every argument). Forward with self = NULL.
 */
int champ_SW_init_sound_hook(const char *path)
{
    return SW_init_sound_hook(NULL, path);
}

void champ_sound_master_volume_hook(float volume)
{
    sound_master_volume_hook(NULL, volume);
}

void champ_sound_master_balance_hook(float left, float right, float subwoofer, float rear)
{
    sound_master_balance_hook(NULL, left, right, subwoofer, rear);
}

int champ_get_channel_status_hook(void)
{
    return get_channel_status_hook(NULL);
}

void champ_sound_start_hook(int channel, int sample, int flags, float p0, float p1, float p2, float p3)
{
    sound_start_hook(NULL, channel, sample, flags, p0, p1, p2, p3);
}

void champ_sound_setup_channel_hook(int channel, int sample, int flags)
{
    sound_setup_channel_hook(NULL, channel, sample, flags);
}

void champ_sound_start_channel_hook(int channel)
{
    sound_start_channel_hook(NULL, channel);
}

void champ_sound_freq_hook(int channel, float phaseInc)
{
    sound_freq_hook(NULL, channel, phaseInc);
}

void champ_sound_panning_hook(int channel, float left, float right, float subwoofer, float rear)
{
    sound_panning_hook(NULL, channel, left, right, subwoofer, rear);
}

void champ_sound_volume_hook(int channel, float volume)
{
    sound_volume_hook(NULL, channel, volume);
}

void champ_sound_volrate_hook(int channel, float volume, float rate)
{
    sound_volrate_hook(NULL, channel, volume, rate);
}

void champ_sound_keyoff_hook(float rate, int channelMask)
{
    sound_keyoff_hook(NULL, rate, channelMask);
}

void champ_sound_stop_hook(int channelMask)
{
    sound_stop_hook(NULL, channelMask);
}

void champ_sound_reset_hook(void)
{
    sound_reset_hook(NULL);
}

void champ_sound_flush_commbuf_hook(void)
{
    sound_flush_commbuf_hook(NULL);
}

void champ_sound_mute_hook(int mute)
{
    sound_mute_hook(NULL, mute);
}

int champ_commbufferReady_hook(void)
{
    return commbufferReady_hook(NULL);
}

void sound_init(void)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    {
        fprintf(stderr, "[sound] SDL_InitSubSystem(AUDIO) failed: %s\n", SDL_GetError());
    }
}

void sound_shutdown(void)
{
    if (audio_device)
    {
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }

    for (int i = 0; i < SOUND_MAX_SAMPLES; i++)
    {
        free(samples[i].data);
        samples[i].data = NULL;
    }

    free(rom_data);
    rom_data = NULL;

    sound_ready = 0;
}
