#include <SDL2/SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "controls.h"
#include "graphics.h"

volatile int virtual_test_button = 0;
volatile int input_thread_running = 0;

static pthread_t input_thread;

int controls_read_test_button(void *self)
{
    (void)self;

    static int previous = 0;
    int current = virtual_test_button != 0;
    int pressed = current && !previous;

    previous = current;
    fprintf(stderr, "[controls] TEST button %d\n", virtual_test_button);
    return pressed;
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
}
