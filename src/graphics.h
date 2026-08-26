#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <SDL2/SDL.h>
#include <X11/Xlib.h>

extern SDL_Window *sdl_window;
extern Display *sdl_display;
extern Window sdl_x11_window;
extern int sdl_initialized;
extern int creating_sdl;
extern int window_width;
extern int window_height;

int graphics_init_window(int width, int height);
void graphics_show_window(void);
void graphics_shutdown(void);
Display *graphics_get_display(void);
Window graphics_get_window(void);
int graphics_is_sdl_window(Window window);

int _XF86VidModeGetGammaRamp(void *display, int screen, int size, unsigned short *red, unsigned short *green, unsigned short *blue);
int _XF86VidModeSetGammaRamp(
    void *display,
    int screen,
    int size,
    unsigned short *red,
    unsigned short *green,
    unsigned short *blue);

#endif