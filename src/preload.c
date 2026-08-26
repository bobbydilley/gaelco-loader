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

// CRCs for the games so it's easy to know what to load
#define TOKYO_COP 0x100
#define CHAMPIONSHIP_TUNING_RACE 0x6f1e5179
#define RING_RIDERS 0x300

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

    switch (getGameCRC32())
    {
    case TOKYO_COP:
        fprintf(stderr, "[preload] Detected Tokyo Cop\n");
        break;
    case CHAMPIONSHIP_TUNING_RACE:
        fprintf(stderr, "[preload] Detected Championship Tuning Race\n");
        break;
    case RING_RIDERS:
        fprintf(stderr, "[preload] Detected Ring Riders\n");
        break;
    default:
        break;
    }


    initDetours();

}


__attribute__((destructor)) static void destroyPreload(void)
{
    log(
        "[preload] shutting down\n");

    controls_stop_input_thread();
    controls_reset_input_state();
    graphics_shutdown();
}