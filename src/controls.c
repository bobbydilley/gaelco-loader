#include <SDL2/SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "controls.h"
#include "graphics.h"

volatile int virtual_test_button = 0;
volatile int virtual_coin1 = 0;
volatile int virtual_start1 = 0;
volatile int input_thread_running = 0;

static pthread_t input_thread;

/*
 * readTest__14G3Dintf_mandos is dead code at runtime (only reachable from a
 * test-mode I/O diagnostics sub-screen), so this hook is kept for that
 * screen's sake but is NOT what makes the TEST switch work in-game. The
 * real path is controls_read_port() below, hooked over inport().
 */
int controls_read_test_button(void *self)
{
    (void)self;

    static int previous = 0;
    int current = virtual_test_button != 0;
    int pressed = current && !previous;

    previous = current;
    return pressed;
}

/*
 * Replaces iolinux::inport(port, &value), the single syscall wrapper the
 * game uses to read its JAMMA I/O board (custom syscalls 0xe6/0xe7 that
 * don't exist on modern Linux, so the real calls always fail).
 *
 * UpdateControllers() polls this every frame for ports 0x40-0x47 and feeds
 * the decoded bits into the game's Flancos()/Nivel() input state, which
 * MainLoop and every menu read from. Confirmed via static analysis:
 *   - MainLoop:    Flancos(2, 0x800)  gates entering TEST mode
 *   - ControlCoins: Flancos(2, 0x200) adds a credit (COIN1)
 *   - many menus:  Flancos(2, 0x2000) is confirm/START
 *
 * TEST and COIN1 turn out to come from a keyboard-scancode-style byte on
 * port 0x42 (0x10 = coin, 0x14 = test, debounced over a few frames by the
 * game itself), not a dedicated switch bit - this looks like a built-in
 * manufacturer debug/service input path. START is a real hardware bit:
 * port 0x40 bit 5, active-low.
 */
int controls_read_port(void *self, int port, char *value)
{
    (void)self;

    unsigned char v;

    switch (port)
    {
    case 0x40:
        v = 0xFF;
        if (virtual_start1)
        {
            v &= (unsigned char)~0x20; /* bit5 = START, active-low */
        }
        break;

    case 0x42:
        if (virtual_test_button)
        {
            v = 0x14; /* keyboard-scancode-style TEST */
        }
        else if (virtual_coin1)
        {
            v = 0x10; /* keyboard-scancode-style COIN1 */
        }
        else
        {
            v = 0x00;
        }
        break;

    default:
        v = 0xFF;
        break;
    }

    if (value)
    {
        *value = (char)v;
    }

    return 0;
}

static void *input_thread_main(void *unused)
{
    (void)unused;

    fprintf(stderr, "[controls] input monitor started\n");

    while (input_thread_running)
    {
        if (!sdl_window)
        {
            usleep(10000);
            continue;
        }

        SDL_PumpEvents();

        SDL_Event event;

        while (SDL_PeepEvents(
                   &event,
                   1,
                   SDL_GETEVENT,
                   SDL_FIRSTEVENT,
                   SDL_LASTEVENT) > 0)
        {
            controls_handle_event(&event);
        }

        usleep(5000);
    }

    fprintf(stderr, "[controls] input monitor stopped\n");
    return NULL;
}

void controls_handle_event(const SDL_Event *event)
{
    if (!event)
    {
        return;
    }

    if (event->type == SDL_QUIT)
    {
        fprintf(stderr, "[controls] SDL_QUIT received\n");
        input_thread_running = 0;
        exit(0);
    }

    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_t &&
        !event->key.repeat)
    {
        virtual_test_button = 1;
        fprintf(stderr, "[controls] T -> TEST button held\n");
    }

    if (event->type == SDL_KEYUP &&
        event->key.keysym.sym == SDLK_t)
    {
        virtual_test_button = 0;
        fprintf(stderr, "[controls] T -> TEST button released\n");
    }

    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_5 &&
        !event->key.repeat)
    {
        virtual_coin1 = 1;
        fprintf(stderr, "[controls] 5 -> COIN1 held\n");
    }

    if (event->type == SDL_KEYUP &&
        event->key.keysym.sym == SDLK_5)
    {
        virtual_coin1 = 0;
        fprintf(stderr, "[controls] 5 -> COIN1 released\n");
    }

    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_1 &&
        !event->key.repeat)
    {
        virtual_start1 = 1;
        fprintf(stderr, "[controls] 1 -> START held\n");
    }

    if (event->type == SDL_KEYUP &&
        event->key.keysym.sym == SDLK_1)
    {
        virtual_start1 = 0;
        fprintf(stderr, "[controls] 1 -> START released\n");
    }
}

void controls_start_input_thread(void)
{
    if (input_thread_running)
    {
        return;
    }

    if (!sdl_window)
    {
        return;
    }

    input_thread_running = 1;

    if (pthread_create(&input_thread, NULL, input_thread_main, NULL) != 0)
    {
        input_thread_running = 0;
        fprintf(stderr, "[controls] failed to create input thread\n");
        return;
    }

    pthread_detach(input_thread);
}

void controls_stop_input_thread(void)
{
    input_thread_running = 0;
}

void controls_reset_input_state(void)
{
    virtual_test_button = 0;
    virtual_coin1 = 0;
    virtual_start1 = 0;
}
