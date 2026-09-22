#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <SDL2/SDL.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>

extern SDL_Window *sdl_window;
extern SDL_GLContext sdl_gl_context;
extern Display *sdl_display;
extern Window sdl_x11_window;
extern int sdl_initialized;
extern int creating_sdl;

/* Real on-screen framebuffer size. */
extern int window_width;
extern int window_height;

/* Fixed size the game renders at, before scaling to the window. */
extern int render_width;
extern int render_height;

/* Non-zero when the render rectangle is being scaled/letterboxed. */
extern int graphics_scaling_active;

int graphics_init_window(int width, int height);
void graphics_show_window(void);
void graphics_shutdown(void);
Display *graphics_get_display(void);
Window graphics_get_window(void);
int graphics_is_sdl_window(Window window);

/* Rebuild the scale/letterbox mapping after window_width/height,
 * render_width/height or the config change. */
void graphics_recompute_letterbox(void);

/* Re-read the drawable size from SDL, then recompute the letterbox. */
void graphics_sync_window_size(void);

/* Toggle borderless fullscreen at runtime (bound to Alt+Enter). */
void graphics_toggle_fullscreen(void);

/* Map a point from render space to real framebuffer space. */
void graphics_map_point(int x, int y, int *out_x, int *out_y);

/* The scaled render rectangle inside the real framebuffer. */
void graphics_content_rect(int *x, int *y, int *w, int *h);

/*
 * XFree86-VidMode gamma-ramp stubs. Referenced from preload.c for
 * Championship Tuning Race, which pokes the gamma ramp directly.
 */
int _XF86VidModeGetGammaRamp(void *display, int screen, int size, unsigned short *red, unsigned short *green, unsigned short *blue);
int _XF86VidModeSetGammaRamp(
    void *display,
    int screen,
    int size,
    unsigned short *red,
    unsigned short *green,
    unsigned short *blue);

/*
 * The one bit of GL we touch: the game's glViewport / glClear / glClearColor
 * are interposed to scale the fixed-size render up to the real window and
 * letterbox it. Everything else (including all the NVIDIA-only extensions
 * the game links against) is left to the real driver - run it on NVIDIA.
 */
void glViewport(int x, int y, int width, int height);
void glClear(unsigned int mask);
void glClearColor(float r, float g, float b, float a);

/*
 * The game's classic GLX calls are turned into thin wrappers over the
 * context SDL created for the window (see graphics.c) - the direct GLX
 * path is broken under GLVND / PRIME render offload.
 */
XVisualInfo *glXChooseVisual(Display *dpy, int screen, int *attribList);
GLXContext glXCreateContext(Display *dpy, XVisualInfo *vis, GLXContext share, Bool direct);
Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx);
void glXSwapBuffers(Display *dpy, GLXDrawable drawable);

/* Startup-critical: the games abort if glXAllocateMemoryNV returns NULL,
 * which the legacy entry point does on Mesa and on NVIDIA PRIME offload.
 * Hand back heap memory. glVertexArrayRangeNV is a no-op hint. */
void *glXAllocateMemoryNV(int size, float readfreq, float writefreq, float priority);
void glXFreeMemoryNV(void *pointer);
void glVertexArrayRangeNV(int length, const void *pointer);

#endif