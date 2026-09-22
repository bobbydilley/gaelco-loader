#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>

#include "controls.h"
#include "graphics.h"
#include "sound.h"
#include "utils.h"

#ifdef errno
#undef errno
#endif

int errno = 0;

extern int _errno __attribute__((alias("errno")));


static void initDetours(void)
{
    switch (getGameCRC32())
    {
    case TOKYO_COP:
        /*
         * readTest is dead code at runtime (only reached from a test-mode
         * I/O diagnostics sub-screen) - kept detoured for that screen's
         * sake, but the fix that actually makes credits/start/test work
         * in-game is the inport() hook below. See controls_read_port().
         */
        detourFunction(0x080e6c14, (void *)controls_read_test_button);
        detourFunction(0x080e6e4c, (void *)controls_read_port);

        /*
         * The game's own G3Dintf_sound methods all bottom out in add_comm(),
         * a ring-buffer protocol for a custom Linux kernel module that
         * doesn't exist on modern Linux. Detour the class methods directly
         * instead - see sound.c/sound.h for why and how.
         */
        detourFunction(0x080e7420, (void *)SW_init_sound_hook);
        detourFunction(0x080e7554, (void *)sound_master_volume_hook);
        detourFunction(0x080e7570, (void *)sound_master_balance_hook);
        detourFunction(0x080e75ac, (void *)get_channel_status_hook);
        detourFunction(0x080e75c8, (void *)sound_flush_commbuf_hook);
        detourFunction(0x080e75e4, (void *)sound_start_hook);
        detourFunction(0x080e7630, (void *)sound_setup_channel_hook);
        detourFunction(0x080e7660, (void *)sound_start_channel_hook);
        detourFunction(0x080e767c, (void *)sound_freq_hook);
        detourFunction(0x080e76c4, (void *)sound_panning_hook);
        detourFunction(0x080e7854, (void *)sound_volrate_hook);
        detourFunction(0x080e78c8, (void *)sound_volume_hook);
        detourFunction(0x080e7914, (void *)sound_keyoff_hook);
        detourFunction(0x080e7960, (void *)sound_stop_hook);
        detourFunction(0x080e797c, (void *)sound_reset_hook);
        detourFunction(0x080e7984, (void *)commbufferReady_hook);
        break;
    case CHAMPIONSHIP_TUNING_RACE:
        /* The game's statically-linked libXxf86vm copies - neuter hardware
         * gamma so the loader owns the window's colour ramp. */
        detourFunction(0x081a814c, (void *)_XF86VidModeGetGammaRamp);
        detourFunction(0x081a804c, (void *)_XF86VidModeSetGammaRamp);

        /*
         * Same Gaelco-syscall I/O wrappers as Ring Riders:
         *   0x0818b92c  iogaelco_inport(int port, char *value)       (0xe6)
         *   0x0818b9b4  iogaelco_intf_command(int cmd, int *value)   (0xe9)
         */
        detourFunction(0x0818b92c, (void *)champrace_inport);
        detourFunction(0x0818b9b4, (void *)champrace_intf_command);

        /*
         * Sound engine. Same class methods and roms.new format as the other
         * two titles, but CTR's Sound:: methods are compiled with NO implicit
         * `this` argument (verified from disassembly), so they get the
         * champ_* wrappers rather than the shared *_hook entry points.
         */
        detourFunction(0x0818a59c, (void *)champ_SW_init_sound_hook);        /* SW_init_sound(char*)          */
        detourFunction(0x0818a1f0, (void *)champ_sound_master_volume_hook);  /* sound_master_volume(f)        */
        detourFunction(0x0818a206, (void *)champ_sound_master_balance_hook); /* sound_master_balance(f,f,f,f) */
        detourFunction(0x0818a236, (void *)champ_get_channel_status_hook);   /* get_channel_status()          */
        detourFunction(0x0818a250, (void *)champ_sound_start_hook);          /* sound_start(i,i,i,f,f,f,f)    */
        detourFunction(0x0818a292, (void *)champ_sound_setup_channel_hook);  /* sound_setup_channel(i,i,i)    */
        detourFunction(0x0818a2c0, (void *)champ_sound_start_channel_hook);  /* sound_start_channel(i)        */
        detourFunction(0x0818a2d8, (void *)champ_sound_freq_hook);           /* sound_freq(i,f)              */
        detourFunction(0x0818a320, (void *)champ_sound_panning_hook);        /* sound_panning(i,f,f,f,f)      */
        detourFunction(0x0818a4b0, (void *)champ_sound_volume_hook);         /* sound_volume(i,f)            */
        detourFunction(0x0818a4ce, (void *)champ_sound_volrate_hook);        /* sound_volrate(i,f,f)         */
        detourFunction(0x0818a51e, (void *)champ_sound_keyoff_hook);         /* sound_keyoff(f,i)           */
        detourFunction(0x0818a550, (void *)champ_sound_reset_hook);          /* sound_reset()               */
        detourFunction(0x0818a558, (void *)champ_sound_flush_commbuf_hook);  /* sound_flush_commbuf()       */
        detourFunction(0x0818a56e, (void *)champ_sound_mute_hook);           /* sound_mute(i)               */
        detourFunction(0x0818a584, (void *)champ_sound_stop_hook);           /* sound_stop(i)               */
        detourFunction(0x0818a6de, (void *)champ_commbufferReady_hook);      /* commbufferReady()           */
        break;
    case RING_RIDERS:
        /*
         * Ring Riders' I/O funnels through two Gaelco-syscall wrappers.
         * Detour both to the emulation in controls.c:
         *   0x08107358  iogaelco_inport(int port, char *value)      (syscall 0xe6)
         *   0x081074b8  iogaelco_intf_command(int cmd, int *value)  (syscall 0xe9)
         */
        detourFunction(0x08107358, (void *)ringriders_inport);
        detourFunction(0x081074b8, (void *)ringriders_intf_command);

        /*
         * Same G3Dintf_sound class as Tokyo Cop (newer build, Itanium-ABI
         * mangled names, same method signatures with `this` as the first
         * stack arg) - reuse the sound.c hooks. roms.new has the same
         * SAMPLE_HEADER + IMA-ADPCM / S16 layout.
         */
        detourFunction(0x0810804c, (void *)SW_init_sound_hook);          /* SW_init_sound(char*)                */
        detourFunction(0x08107c5c, (void *)sound_master_volume_hook);    /* sound_master_volume(float)          */
        detourFunction(0x08107c74, (void *)sound_master_balance_hook);   /* sound_master_balance(f,f,f,f)       */
        detourFunction(0x08107ca8, (void *)get_channel_status_hook);     /* get_channel_status()               */
        detourFunction(0x08107cc4, (void *)sound_start_hook);            /* sound_start(i,i,i,f,f,f,f)          */
        detourFunction(0x08107d0c, (void *)sound_setup_channel_hook);    /* sound_setup_channel(i,i,i)          */
        detourFunction(0x08107d3c, (void *)sound_start_channel_hook);    /* sound_start_channel(i)              */
        detourFunction(0x08107d58, (void *)sound_freq_hook);             /* sound_freq(i,f)                     */
        detourFunction(0x08107d88, (void *)sound_panning_hook);          /* sound_panning(i,f,f,f,f)            */
        detourFunction(0x08107f1c, (void *)sound_volume_hook);           /* sound_volume(i,f)                   */
        detourFunction(0x08107f3c, (void *)sound_volrate_hook);          /* sound_volrate(i,f,f)               */
        detourFunction(0x08107f98, (void *)sound_keyoff_hook);           /* sound_keyoff(f,i)                   */
        detourFunction(0x08107fd0, (void *)sound_reset_hook);            /* sound_reset()                      */
        detourFunction(0x08107fd8, (void *)sound_flush_commbuf_hook);    /* sound_flush_commbuf()             */
        detourFunction(0x08107ff4, (void *)sound_mute_hook);             /* sound_mute(i)                      */
        detourFunction(0x08108014, (void *)commbufferReady_hook);        /* commbufferReady()                 */
        detourFunction(0x08108030, (void *)sound_stop_hook);             /* sound_stop(i)                      */
        break;
    default:
        break;
    }
}

__attribute__((constructor)) static void initPreload(void)
{

    printf("Gaelco Loader Installed\n");

    fprintf(stderr,
            "[preload] PID=%d CRC32=%08x\n",
            (int)getpid(),
            getGameCRC32());

    sound_init();
    initDetours();
}


__attribute__((destructor)) static void destroyPreload(void)
{
    debug(
        "[preload] shutting down\n");

    controls_stop_input_thread();
    controls_reset_input_state();
    sound_shutdown();
    graphics_shutdown();
}