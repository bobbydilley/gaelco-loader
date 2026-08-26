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
        detourFunction(0x080e6c14, (void *)controls_read_test_button);
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