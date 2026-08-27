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
volatile int virtual_service = 0;
volatile int virtual_estop = 0;
volatile int virtual_steer_left = 0;
volatile int virtual_steer_right = 0;
volatile int virtual_accel = 0;
volatile int virtual_brake = 0;
volatile int input_thread_running = 0;

/* Neutral/idle byte for the wheel pot (port 0x44) and full-deflection step. */
#define WHEEL_CENTER 0x80
#define WHEEL_DEFLECT 0x60

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
 * MainLoop and every menu read from, plus two analog pot channels used
 * directly (not through Flancos/Nivel) for the wheel and pedals.
 *
 * Findings from static analysis of the port-decode logic:
 *
 *   Port 0x40 (digital, active-low unless noted):
 *     bit5 (0x20) -> Flancos(2,0x2000)  START / menu confirm
 *     bit6 (0x40) -> Nivel(2,0x4000), ACTIVE-HIGH. This is the motion
 *                    platform "safety circuit OK" line: MainLoop's Ramas
 *                    state machine only shows the flashing "pull the
 *                    emergency stop" reminder (pintaRotuloBotonSeguridad)
 *                    while this bit reads 0. We default it HIGH (armed),
 *                    so the game should already sail past that screen;
 *                    virtual_estop lets you flip it low to test that path.
 *
 *   Port 0x41 (digital, active-low):
 *     bit2 (0x04) -> Flancos(0,0x10), used as a SERVICE/select button in
 *                    some test sub-screens (e.g. SoundTest).
 *
 *   Port 0x42 (keyboard-scancode-style byte, debounced by the game itself
 *   over a few consecutive polls): 0x10 = COIN1, 0x14 = TEST. Neither has
 *   a dedicated switch bit - this looks like a built-in manufacturer
 *   debug/service input path layered on top of the raw switches.
 *
 *   Ports 0x44/0x45/0x46 (raw 0-255 analog, fed straight into the POTE
 *   calibration struct, not through Flancos/Nivel):
 *     0x44 -> wheel  (INVERTED: game stores 0xFF - raw; center ~0x80)
 *     0x45 -> accelerator pedal (raw, uninverted)
 *     0x46 -> brake pedal (raw, uninverted)
 *   ControlesMenu (the main test-menu cursor handler) reads the wheel's
 *   calibrated output directly: steering hard left/right moves the cursor
 *   up/down the options list, and flooring the accelerator confirms a
 *   selection (in addition to START). So there is no separate switch for
 *   test-menu navigation - it reuses the wheel and pedal already wired
 *   for driving, matching how the real cabinet's control panel is laid
 *   out (no joystick).
 *
 * Left/right steering polarity and the exact idle level for the pedals
 * are our best guess from the byte layout, not something we could verify
 * without running the game - flip WHEEL_DEFLECT's sign (or swap the
 * left/right branches below) if steering comes out backwards.
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
        if (virtual_estop)
        {
            v &= (unsigned char)~0x40; /* bit6 = safety circuit, active-HIGH: clear to simulate E-STOP pressed */
        }
        break;

    case 0x41:
        v = 0xFF;
        if (virtual_service)
        {
            v &= (unsigned char)~0x04; /* bit2 = SERVICE, active-low */
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

    case 0x44:
        v = WHEEL_CENTER;
        if (virtual_steer_left)
        {
            v = WHEEL_CENTER - WHEEL_DEFLECT;
        }
        else if (virtual_steer_right)
        {
            v = WHEEL_CENTER + WHEEL_DEFLECT;
        }
        break;

    case 0x45:
        v = virtual_accel ? 0xFF : 0x00;
        break;

    case 0x46:
        v = virtual_brake ? 0xFF : 0x00;
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

    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_9 &&
        !event->key.repeat)
    {
        virtual_service = 1;
        fprintf(stderr, "[controls] 9 -> SERVICE held\n");
    }

    if (event->type == SDL_KEYUP &&
        event->key.keysym.sym == SDLK_9)
    {
        virtual_service = 0;
        fprintf(stderr, "[controls] 9 -> SERVICE released\n");
    }

    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_e &&
        !event->key.repeat)
    {
        virtual_estop ^= 1;
        fprintf(stderr, "[controls] E -> motion E-STOP %s\n",
                virtual_estop ? "PRESSED (platform disabled)" : "released (platform armed)");
    }

    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_LEFT)
    {
        virtual_steer_left = 1;
    }

    if (event->type == SDL_KEYUP &&
        event->key.keysym.sym == SDLK_LEFT)
    {
        virtual_steer_left = 0;
    }

    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_RIGHT)
    {
        virtual_steer_right = 1;
    }

    if (event->type == SDL_KEYUP &&
        event->key.keysym.sym == SDLK_RIGHT)
    {
        virtual_steer_right = 0;
    }

    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_UP)
    {
        virtual_accel = 1;
    }

    if (event->type == SDL_KEYUP &&
        event->key.keysym.sym == SDLK_UP)
    {
        virtual_accel = 0;
    }

    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_DOWN)
    {
        virtual_brake = 1;
    }

    if (event->type == SDL_KEYUP &&
        event->key.keysym.sym == SDLK_DOWN)
    {
        virtual_brake = 0;
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
    virtual_service = 0;
    virtual_estop = 0;
    virtual_steer_left = 0;
    virtual_steer_right = 0;
    virtual_accel = 0;
    virtual_brake = 0;
}
