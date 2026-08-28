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
volatile int virtual_view = 0;
volatile int virtual_siren = 0;
volatile int virtual_volume_up = 0;
volatile int virtual_volume_down = 0;
/*
 * Starts PRESSED (motion platform disabled), matching the real cabinet:
 * the physical E-STOP is pushed in by default and the operator has to
 * pull it out before the platform arms. Toggle with E.
 */
volatile int virtual_estop = 1;
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
 * Findings below are from static analysis of the port-decode logic,
 * corrected against live testing on the in-game "CONTROLS TEST" screen
 * (test_io) - the static guesses for View/Siren/Break Pedal/Gas Pedal
 * were wrong (source order doesn't match on-screen order the way we
 * assumed) and have been fixed up from what was actually observed:
 *
 *   Port 0x40 (digital, active-low unless noted):
 *     bit4 (0x10) -> Break Pedal, a floor-position microswitch. This is
 *                    DIGITAL only - the pedal has no analog readout on
 *                    the CONTROLS TEST screen, unlike the accelerator.
 *                    Wired to virtual_brake (same key as the analog
 *                    brake on port 0x46).
 *     bit5 (0x20) -> Flancos(2,0x2000)  START / menu confirm. ClaxonCar
 *                    (the horn sound) also fires off Nivel(2,0x2000) -
 *                    same physical switch as START.
 *     bit6 (0x40) -> Nivel(2,0x4000), ACTIVE-HIGH. Security/E-STOP - the
 *                    motion platform "safety circuit OK" line. MainLoop's
 *                    Ramas state machine only shows the flashing "pull
 *                    the emergency stop" reminder while this bit reads 0.
 *                    virtual_estop starts at 1 (button pressed in /
 *                    platform disabled) so the game boots into that state
 *                    like the real cabinet; press E to arm the platform.
 *
 *   Port 0x41 (digital, active-low) - operator service panel, read
 *   directly (not through the analog wheel/pedal path):
 *     bit0 (0x01) -> Flancos(0,0x40) = View. Confirmed live: this is also
 *                    what testGetUserEntry() (test_main_menu's cursor
 *                    input) reads as one direction of test-menu nav, so
 *                    View doubles as a menu button - matches a wheel
 *                    cabinet with no dedicated joystick.
 *     bit1 (0x02) -> Flancos(0,0x20) = Siren. Also read by
 *                    testGetUserEntry() as the other nav direction.
 *     bit2 (0x04) -> Flancos(0,0x10), used as a select/activate button in
 *                    some test sub-screens (e.g. SoundTest) - not one of
 *                    the ten CONTROLS TEST items.
 *     bit3 (0x08) -> Nivel(3,0x1000)  Coin Chute 1 (raw coin switch pulse)
 *     bit4 (0x10) -> Nivel(4,0x1000)  Coin Chute 2
 *
 *   Port 0x42 (keyboard-scancode-style byte, debounced by the game itself
 *   over a few consecutive polls): 0x10 = Service (also adds a credit via
 *   ControlCoins - standard JAMMA "service credit" behaviour), 0x14 =
 *   Test, 0x18 = Volume up, 0x1c = Volume down. None of these have a
 *   dedicated switch bit - this looks like a built-in manufacturer
 *   debug/service input path layered on top of the raw switches.
 *
 *   Ports 0x44/0x45/0x46 (raw 0-255 analog, fed straight into the POTE
 *   calibration struct, not through Flancos/Nivel):
 *     0x44 -> wheel  (INVERTED: game stores 0xFF - raw; center ~0x80)
 *     0x45 -> accelerator pedal (raw, uninverted) - Gas Pedal's readout
 *             on the CONTROLS TEST screen, confirmed showing 255 when held
 *     0x46 -> brake pedal (raw, uninverted) - no on-screen readout, but
 *             presumably used for actual braking force during gameplay
 *
 * Service (port 0x42 scancode 0x10) still doesn't light up in testing
 * despite going through the exact same keyboard-scancode mechanism as
 * Test (scancode 0x14) and Volume (0x18/0x1c), which do work - still
 * unexplained.
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
            v &= (unsigned char)~0x20; /* bit5 = START (also the horn/siren sound), active-low */
        }
        if (virtual_brake)
        {
            v &= (unsigned char)~0x10; /* bit4 = Break Pedal floor microswitch, active-low */
        }
        if (virtual_estop)
        {
            v &= (unsigned char)~0x40; /* bit6 = safety circuit, active-HIGH: clear to simulate E-STOP pressed */
        }
        break;

    case 0x41:
        v = 0xFF;
        if (virtual_view)
        {
            v &= (unsigned char)~0x01; /* bit0 = View, active-low */
        }
        if (virtual_siren)
        {
            v &= (unsigned char)~0x02; /* bit1 = Siren, active-low */
        }
        if (virtual_service)
        {
            v &= (unsigned char)~0x04; /* bit2 = select/activate (test sub-screens), active-low */
        }
        break;

    case 0x42:
        if (virtual_test_button)
        {
            v = 0x14; /* keyboard-scancode-style TEST */
        }
        else if (virtual_coin1)
        {
            v = 0x10; /* keyboard-scancode-style SERVICE (also adds a credit) */
        }
        else if (virtual_volume_up)
        {
            v = 0x18; /* keyboard-scancode-style VOLUME UP */
        }
        else if (virtual_volume_down)
        {
            v = 0x1c; /* keyboard-scancode-style VOLUME DOWN */
        }
        else
        {
            v = 0x00;
        }
        break;

    case 0x44:
        /* Port 0x44 is stored inverted (0xFF - raw) by the game, so we
         * invert our steering sense here too - this was backwards before. */
        v = WHEEL_CENTER;
        if (virtual_steer_left)
        {
            v = WHEEL_CENTER + WHEEL_DEFLECT;
        }
        else if (virtual_steer_right)
        {
            v = WHEEL_CENTER - WHEEL_DEFLECT;
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

    if (event->type == SDL_WINDOWEVENT &&
        (event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
         event->window.event == SDL_WINDOWEVENT_RESIZED))
    {
        graphics_sync_window_size();
    }

    /* Alt+Enter: toggle borderless fullscreen. */
    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_RETURN &&
        (event->key.keysym.mod & KMOD_ALT) &&
        !event->key.repeat)
    {
        fprintf(stderr, "[controls] Alt+Enter -> toggle fullscreen\n");
        graphics_toggle_fullscreen();
        return;
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
        event->key.keysym.sym == SDLK_EQUALS &&
        !event->key.repeat)
    {
        virtual_volume_up = 1;
        fprintf(stderr, "[controls] = -> VOLUME UP held\n");
    }

    if (event->type == SDL_KEYUP &&
        event->key.keysym.sym == SDLK_EQUALS)
    {
        virtual_volume_up = 0;
        fprintf(stderr, "[controls] = -> VOLUME UP released\n");
    }

    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_MINUS &&
        !event->key.repeat)
    {
        virtual_volume_down = 1;
        fprintf(stderr, "[controls] - -> VOLUME DOWN held\n");
    }

    if (event->type == SDL_KEYUP &&
        event->key.keysym.sym == SDLK_MINUS)
    {
        virtual_volume_down = 0;
        fprintf(stderr, "[controls] - -> VOLUME DOWN released\n");
    }

    /* View and Siren double as test-menu up/down navigation - no separate nav switch exists. */
    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_v &&
        !event->key.repeat)
    {
        virtual_view = 1;
        fprintf(stderr, "[controls] V -> VIEW held\n");
    }

    if (event->type == SDL_KEYUP &&
        event->key.keysym.sym == SDLK_v)
    {
        virtual_view = 0;
        fprintf(stderr, "[controls] V -> VIEW released\n");
    }

    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_s &&
        !event->key.repeat)
    {
        virtual_siren = 1;
        fprintf(stderr, "[controls] S -> SIREN held\n");
    }

    if (event->type == SDL_KEYUP &&
        event->key.keysym.sym == SDLK_s)
    {
        virtual_siren = 0;
        fprintf(stderr, "[controls] S -> SIREN released\n");
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
    virtual_view = 0;
    virtual_siren = 0;
    virtual_volume_up = 0;
    virtual_volume_down = 0;
    virtual_estop = 1; /* pressed in / platform disabled, matching startup default */
    virtual_steer_left = 0;
    virtual_steer_right = 0;
    virtual_accel = 0;
    virtual_brake = 0;
}
