#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>

#include "controls.h"
#include "graphics.h"
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

    initDetours();
}


__attribute__((destructor)) static void destroyPreload(void)
{
    debug(
        "[preload] shutting down\n");

    controls_stop_input_thread();
    controls_reset_input_state();
    graphics_shutdown();
}