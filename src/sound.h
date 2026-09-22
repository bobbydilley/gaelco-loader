#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

/*
 * Software replacement for the game's sound engine.
 *
 * The original G3Dintf_sound class methods (SW_init_sound, sound_setup_channel,
 * sound_start_channel, sound_freq, sound_panning, sound_volume, sound_keyoff,
 * sound_stop, sound_reset, sound_master_volume, sound_master_balance,
 * get_channel_status, sound_flush_commbuf, commbufferReady) all bottom out in
 * add_comm(), which queues commands for the game's custom "iolinux" Linux
 * kernel module (syscalls 0xe2-0xe9) to execute - hardware that doesn't exist
 * once you're not running on the original PCB/kernel. We detour the class
 * methods themselves instead of trying to reimplement that ring-buffer
 * protocol: at that level the arguments are already plain, decoded
 * ints/floats, matching (and cross-checked against) TeknoParrot's
 * OpenSndGaelco, an existing open-source reimplementation of this exact
 * sound engine for the Windows PC releases of these games. The sample data
 * format (roms.new, SAMPLE_HEADER, IMA-ADPCM decode) is identical across
 * platforms since it comes from the game's own data files, not the driver.
 *
 * This file owns:
 *   - loading and decoding roms.new (16-bit PCM and IMA-ADPCM samples) into
 *     flat S16 mono buffers, resampled once at load time to a common rate
 *   - a small SDL2 audio-callback mixer for up to SOUND_CHANNELS voices,
 *     each with independent volume, stereo pan, loop and playback-rate
 *     (real-time pitch) control
 *   - the detour targets themselves, matching each class method's cdecl
 *     signature with `self` (the fixed G3Dintf_sound instance) as an
 *     ignored first argument
 */

#define SOUND_CHANNELS 12
#define SOUND_MAX_SAMPLES 512

void sound_init(void);
void sound_shutdown(void);

int SW_init_sound_hook(void *self, const char *path);
void sound_master_volume_hook(void *self, float volume);
void sound_master_balance_hook(void *self, float left, float right, float subwoofer, float rear);
int get_channel_status_hook(void *self);
void sound_flush_commbuf_hook(void *self);
void sound_start_hook(void *self, int channel, int sample, int flags, float p0, float p1, float p2, float p3);
void sound_setup_channel_hook(void *self, int channel, int sample, int flags);
void sound_start_channel_hook(void *self, int channel);
void sound_freq_hook(void *self, int channel, float phaseInc);
void sound_panning_hook(void *self, int channel, float left, float right, float subwoofer, float rear);
void sound_volrate_hook(void *self, int channel, float volume, float rate);
void sound_volume_hook(void *self, int channel, float volume);
void sound_keyoff_hook(void *self, float rate, int channelMask);
void sound_stop_hook(void *self, int channelMask);
void sound_reset_hook(void *self);
void sound_mute_hook(void *self, int mute); /* Ring Riders only */
int commbufferReady_hook(void *self);

/*
 * Championship Tuning Race - same sound engine and roms.new format, but its
 * Sound:: class methods are compiled WITHOUT an implicit `this` pointer
 * (verified from disassembly: the first stack slot is the first real
 * argument, not a self pointer). These thin wrappers drop the missing
 * `self` and forward to the shared hooks above.
 */
int champ_SW_init_sound_hook(const char *path);
void champ_sound_master_volume_hook(float volume);
void champ_sound_master_balance_hook(float left, float right, float subwoofer, float rear);
int champ_get_channel_status_hook(void);
void champ_sound_start_hook(int channel, int sample, int flags, float p0, float p1, float p2, float p3);
void champ_sound_setup_channel_hook(int channel, int sample, int flags);
void champ_sound_start_channel_hook(int channel);
void champ_sound_freq_hook(int channel, float phaseInc);
void champ_sound_panning_hook(int channel, float left, float right, float subwoofer, float rear);
void champ_sound_volume_hook(int channel, float volume);
void champ_sound_volrate_hook(int channel, float volume, float rate);
void champ_sound_keyoff_hook(float rate, int channelMask);
void champ_sound_stop_hook(int channelMask);
void champ_sound_reset_hook(void);
void champ_sound_flush_commbuf_hook(void);
void champ_sound_mute_hook(int mute);
int champ_commbufferReady_hook(void);

#endif
