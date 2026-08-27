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
        detourFunction(0x081a814c, (void *)_XF86VidModeGetGammaRamp);
        detourFunction(0x081a804c, (void *)_XF86VidModeSetGammaRamp);
        break;
    case RING_RIDERS:

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