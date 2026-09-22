#define _GNU_SOURCE

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "controls.h"
#include "graphics.h"
#include "utils.h"

/*
 * This file does the bare minimum to get the game onto the screen:
 *
 *   1. Redirect the game's X11 window creation onto an SDL window, so we
 *      own the event loop and can drive it from the input thread.
 *   2. Let SDL create the GL context (it copes with GLVND / PRIME render
 *      offload); turn the game's classic glX{ChooseVisual,CreateContext,
 *      MakeCurrent,SwapBuffers} calls into thin wrappers over SDL's.
 *   3. Scale the game's fixed 640x480 render up to the window / fullscreen
 *      size from gaelco.ini, letterboxing to keep the 4:3 aspect.
 *   4. Stub the two XFree86-VidMode gamma-ramp calls.
 *
 * There is deliberately NO emulation of NVIDIA-only GL extensions
 * (register combiners, vertex weighting, vertex programs, fences,
 * GLX_NV_vertex_array_range, occlusion queries, ...). Run the game on an
 * NVIDIA driver that provides them natively.
 */

SDL_Window *sdl_window = NULL;
SDL_GLContext sdl_gl_context = NULL;
Display *sdl_display = NULL;
Window sdl_x11_window = None;
int sdl_initialized = 0;
int creating_sdl = 0;

/* Real on-screen framebuffer size (window client area or fullscreen). */
int window_width = 640;
int window_height = 480;

/* Fixed resolution the game itself renders at - everything it draws is
 * scaled from this rectangle up to window_width x window_height. */
int render_width = 640;
int render_height = 480;

/* Placement of the scaled render rectangle inside the real framebuffer:
 * a pixel the game draws at (x, y) lands at
 *     (letterbox_x + x * letterbox_scale_x, letterbox_y + y * letterbox_scale_y)
 * letterbox_{x,y} are non-zero only when keeping aspect leaves bars. */
static int letterbox_x = 0;
static int letterbox_y = 0;
static int letterbox_w = 640;
static int letterbox_h = 480;
static double letterbox_scale_x = 1.0;
static double letterbox_scale_y = 1.0;
int graphics_scaling_active = 0;

void graphics_recompute_letterbox(void)
{
    const gaelco_config_t *cfg = config_get();

    if (render_width < 1)
    {
        render_width = 1;
    }

    if (render_height < 1)
    {
        render_height = 1;
    }

    /* "Follow the game" mode: no explicit window size, not fullscreen. The
     * window always tracks the game's render size and nothing is scaled.
     * Forced here (rather than derived from an SDL size read-back) so a
     * late/!spurious SDL resize event can't flip scaling back on - CTR's
     * multi-pass renderer does not survive the viewport/scissor remap. */
    if (!cfg->fullscreen && cfg->width <= 0 && cfg->height <= 0)
    {
        window_width = render_width;
        window_height = render_height;
        letterbox_x = 0;
        letterbox_y = 0;
        letterbox_w = render_width;
        letterbox_h = render_height;
        letterbox_scale_x = 1.0;
        letterbox_scale_y = 1.0;
        graphics_scaling_active = 0;

        fprintf(stderr,
            "[graphics] scale: window %dx%d = render (follow-game, 1:1)\n",
            window_width, window_height);
        return;
    }

    if (cfg->keep_aspect)
    {
        double s = (double)window_width / render_width;
        double sy = (double)window_height / render_height;

        if (sy < s)
        {
            s = sy;
        }

        if (cfg->integer_scale && s >= 1.0)
        {
            s = (double)(int)s;
        }

        if (s <= 0.0)
        {
            s = 1.0;
        }

        letterbox_scale_x = s;
        letterbox_scale_y = s;
        letterbox_w = (int)(render_width * s + 0.5);
        letterbox_h = (int)(render_height * s + 0.5);
    }
    else
    {
        letterbox_scale_x = (double)window_width / render_width;
        letterbox_scale_y = (double)window_height / render_height;
        letterbox_w = window_width;
        letterbox_h = window_height;
    }

    letterbox_x = (window_width - letterbox_w) / 2;
    letterbox_y = (window_height - letterbox_h) / 2;

    if (letterbox_x < 0)
    {
        letterbox_x = 0;
    }

    if (letterbox_y < 0)
    {
        letterbox_y = 0;
    }

    graphics_scaling_active =
        letterbox_x != 0 || letterbox_y != 0 ||
        letterbox_w != render_width || letterbox_h != render_height;

    fprintf(stderr,
        "[graphics] scale: window %dx%d, render %dx%d -> rect %d,%d %dx%d (x%.3f%s)\n",
        window_width, window_height, render_width, render_height,
        letterbox_x, letterbox_y, letterbox_w, letterbox_h, letterbox_scale_x,
        graphics_scaling_active ? "" : ", 1:1");
}

void graphics_map_point(int x, int y, int *out_x, int *out_y)
{
    if (out_x)
    {
        *out_x = letterbox_x + (int)(x * letterbox_scale_x + 0.5);
    }

    if (out_y)
    {
        *out_y = letterbox_y + (int)(y * letterbox_scale_y + 0.5);
    }
}

void graphics_content_rect(int *x, int *y, int *w, int *h)
{
    if (x) *x = letterbox_x;
    if (y) *y = letterbox_y;
    if (w) *w = letterbox_w;
    if (h) *h = letterbox_h;
}

/* Re-read the real drawable size from SDL and rebuild the letterbox. */
void graphics_sync_window_size(void)
{
    if (!sdl_window)
    {
        return;
    }

    int w = 0;
    int h = 0;

    SDL_GL_GetDrawableSize(sdl_window, &w, &h);

    if (w <= 0 || h <= 0)
    {
        SDL_GetWindowSize(sdl_window, &w, &h);
    }

    if (w > 0 && h > 0)
    {
        window_width = w;
        window_height = h;
    }

    graphics_recompute_letterbox();
}

void graphics_toggle_fullscreen(void)
{
    if (!sdl_window)
    {
        return;
    }

    Uint32 flags = SDL_GetWindowFlags(sdl_window);
    int now_fs = (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;

    SDL_SetWindowFullscreen(sdl_window,
        now_fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_ShowCursor(now_fs ? SDL_ENABLE : SDL_DISABLE);

    if (now_fs)
    {
        const gaelco_config_t *cfg = config_get();
        SDL_SetWindowSize(sdl_window, cfg->width, cfg->height);
        SDL_SetWindowPosition(sdl_window,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    graphics_sync_window_size();
}

int graphics_init_window(int width, int height)
{
    SDL_SysWMinfo wm;

    if (sdl_window != NULL)
    {
        return 1;
    }

    if (creating_sdl)
    {
        return 0;
    }

    creating_sdl = 1;

    config_load();

    const gaelco_config_t *cfg = config_get();

    /* The game passes the size it wants to render at (640x480 for Tokyo
     * Cop). Keep that as the internal render size; the real window size
     * comes from gaelco.ini instead. */
    if (width > 0)
    {
        render_width = width;
    }
    else
    {
        render_width = cfg->render_width;
    }

    if (height > 0)
    {
        render_height = height;
    }
    else
    {
        render_height = cfg->render_height;
    }

    Uint32 window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL;

    if (cfg->fullscreen)
    {
        window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    /* width/height == 0 in the config means "match the game's render size",
     * which keeps the letterbox scaler disabled (1:1). Only an explicit
     * gaelco.ini override or fullscreen turns scaling on. */
    window_width = cfg->width > 0 ? cfg->width : render_width;
    window_height = cfg->height > 0 ? cfg->height : render_height;

    debug("[graphics] creating SDL window %dx%d%s (render %dx%d)\n",
        window_width, window_height,
        cfg->fullscreen ? " fullscreen" : "",
        render_width, render_height);

    SDL_setenv("SDL_VIDEODRIVER", "x11", 1);

    if (!sdl_initialized)
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            debug("[graphics] SDL_Init failed: %s\n",
                SDL_GetError());
            creating_sdl = 0;
            return 0;
        }

        sdl_initialized = 1;
    }

    /* Leave the profile unset (legacy / no-profile context) - the games
     * are fixed-function + NV extensions. Just ask for the buffer sizes
     * they request in their own glXChooseVisual attrib lists. */
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    sdl_window = SDL_CreateWindow(
        getGameName(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        window_width,
        window_height,
        window_flags);

    if (!sdl_window)
    {
        debug("[graphics] SDL_CreateWindow failed: %s\n",
            SDL_GetError());
        creating_sdl = 0;
        return 0;
    }

    if (cfg->fullscreen)
    {
        SDL_ShowCursor(SDL_DISABLE);
    }

    graphics_sync_window_size();

    memset(&wm, 0, sizeof(wm));
    SDL_VERSION(&wm.version);

    if (!SDL_GetWindowWMInfo(sdl_window, &wm))
    {
        debug("[graphics] SDL_GetWindowWMInfo failed: %s\n",
            SDL_GetError());
        SDL_DestroyWindow(sdl_window);
        sdl_window = NULL;
        creating_sdl = 0;
        return 0;
    }

    if (wm.subsystem != SDL_SYSWM_X11)
    {
        debug("[graphics] SDL window is not using X11 (subsystem=%d)\n",
            wm.subsystem);
        SDL_DestroyWindow(sdl_window);
        sdl_window = NULL;
        creating_sdl = 0;
        return 0;
    }

    sdl_display = wm.info.x11.display;
    sdl_x11_window = wm.info.x11.window;

    debug("[graphics] SDL X11 display = %p\n",
        (void *)sdl_display);
    debug("[graphics] SDL X11 window  = 0x%lx\n",
        (unsigned long)sdl_x11_window);

    /* Create the GL context now, while SDL still has all the context it
     * needs to pick the right vendor / FBConfig. The game's own
     * glXCreateContext / glXMakeCurrent become wrappers over this. */
    sdl_gl_context = SDL_GL_CreateContext(sdl_window);

    if (!sdl_gl_context)
    {
        fprintf(stderr, "[graphics] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
    }
    else
    {
        SDL_GL_MakeCurrent(sdl_window, sdl_gl_context);
        fprintf(stderr, "[graphics] SDL GL context ready\n");
    }

    creating_sdl = 0;
    return 1;
}

void graphics_show_window(void)
{
    if (sdl_window)
    {
        SDL_ShowWindow(sdl_window);
    }
}

void graphics_shutdown(void)
{
    if (sdl_gl_context)
    {
        SDL_GL_DeleteContext(sdl_gl_context);
        sdl_gl_context = NULL;
    }

    if (sdl_window)
    {
        SDL_DestroyWindow(sdl_window);
        sdl_window = NULL;
    }

    if (sdl_initialized)
    {
        SDL_Quit();
        sdl_initialized = 0;
    }
}

Display *graphics_get_display(void)
{
    return sdl_display;
}

Window graphics_get_window(void)
{
    return sdl_x11_window;
}

int graphics_is_sdl_window(Window window)
{
    return sdl_window != NULL && sdl_x11_window != 0 && window == sdl_x11_window;
}

/* ===================================================================== *
 *  X11 window-creation redirection                                       *
 * ===================================================================== */

typedef Display *(*real_XOpenDisplay_t)(const char *);
typedef Window (*real_XCreateWindow_t)(Display *, Window, int, int, unsigned int, unsigned int, unsigned int, int, unsigned int, Visual *, unsigned long, XSetWindowAttributes *);
typedef int (*real_XMapWindow_t)(Display *, Window);
typedef Bool (*XIfEventPredicate)(Display *, XEvent *, XPointer);
typedef int (*real_XIfEvent_t)(Display *, XEvent *, XIfEventPredicate, XPointer);

static real_XIfEvent_t real_XIfEvent_func = NULL;
static __thread XIfEventPredicate current_predicate = NULL;
static __thread XPointer current_predicate_arg = NULL;

static Bool proxy_x_if_event_predicate(Display *display, XEvent *event, XPointer arg)
{
    XIfEventPredicate predicate = current_predicate;
    XPointer predicate_arg = current_predicate_arg;

    if (!predicate)
    {
        return False;
    }

    return predicate(display, event, predicate_arg);
}

Display *XOpenDisplay(const char *display_name)
{
    static real_XOpenDisplay_t real_XOpenDisplay = NULL;

    if (!real_XOpenDisplay)
    {
        real_XOpenDisplay = (real_XOpenDisplay_t)dlsym(RTLD_NEXT, "XOpenDisplay");
    }

    if (creating_sdl)
    {
        if (real_XOpenDisplay)
        {
            return real_XOpenDisplay(display_name);
        }

        debug("[graphics] real XOpenDisplay unavailable\n");
        return NULL;
    }

    if (!sdl_window)
    {
        if (!graphics_init_window(window_width, window_height))
        {
            debug("[graphics] unable to initialize SDL window\n");
            return NULL;
        }

        controls_start_input_thread();
    }

    debug("[graphics] XOpenDisplay(\"%s\") -> SDL Display %p\n",
        display_name ? display_name : "(null)",
        (void *)sdl_display);

    return sdl_display;
}

Window XCreateWindow(Display *display, Window parent, int x, int y, unsigned int width, unsigned int height, unsigned int border_width, int depth, unsigned int class, Visual *visual, unsigned long valueMask, XSetWindowAttributes *attributes)
{
    static real_XCreateWindow_t real_XCreateWindow = NULL;

    if (!real_XCreateWindow)
    {
        real_XCreateWindow = (real_XCreateWindow_t)dlsym(RTLD_NEXT, "XCreateWindow");
    }

    if (creating_sdl)
    {
        if (real_XCreateWindow)
        {
            return real_XCreateWindow(display, parent, x, y, width, height, border_width, depth, class, visual, valueMask, attributes);
        }

        debug("[graphics] real XCreateWindow unavailable\n");
        return None;
    }

    if (!sdl_window)
    {
        /* Pass the game's requested size through as the *render* size;
         * graphics_init_window sizes the real window from gaelco.ini. */
        if (!graphics_init_window((int)width, (int)height))
        {
            debug("[graphics] XCreateWindow: SDL window creation failed\n");
            return None;
        }

        controls_start_input_thread();
    }

    debug("[graphics] XCreateWindow intercepted: requested %ux%u, SDL window 0x%lx\n",
        width, height, (unsigned long)sdl_x11_window);

    /* The game reveals its true render size here (it may differ from the
     * size the window was first created at - e.g. Championship Tuning Race
     * asks for 720x540). Adopt it as the render size. */
    if (width > 0 && height > 0 &&
        ((int)width != render_width || (int)height != render_height))
    {
        render_width = (int)width;
        render_height = (int)height;

        /* If the user did not pin a window size in gaelco.ini, keep the
         * real window matched to the render size so the letterbox scaler
         * stays disabled (1:1) - some engines (CTR) render multiple passes
         * / render-to-texture and don't survive the viewport+scissor
         * remap. An explicit override or fullscreen still scales. */
        const gaelco_config_t *cfg = config_get();

        if (cfg->width <= 0 && cfg->height <= 0 && !cfg->fullscreen && sdl_window)
        {
            /* Physically resize the drawable to the render size, then pump
             * events until SDL reports it (bounded wait) so the very first
             * frame already has a correctly-sized framebuffer. */
            SDL_SetWindowSize(sdl_window, render_width, render_height);

            for (int i = 0; i < 100; i++)
            {
                int dw = 0, dh = 0;
                SDL_PumpEvents();
                SDL_GL_GetDrawableSize(sdl_window, &dw, &dh);

                if (dw == render_width && dh == render_height)
                {
                    break;
                }

                SDL_Delay(2);
            }
        }

        graphics_recompute_letterbox();
    }

    return sdl_x11_window;
}

int XMapWindow(Display *display, Window window)
{
    static real_XMapWindow_t real_XMapWindow = NULL;

    if (!real_XMapWindow)
    {
        real_XMapWindow = (real_XMapWindow_t)dlsym(RTLD_NEXT, "XMapWindow");
    }

    if (graphics_is_sdl_window(window))
    {
        debug("[graphics] XMapWindow(0x%lx) -> SDL_ShowWindow()\n",
            (unsigned long)window);
        graphics_show_window();
        return 0;
    }

    if (real_XMapWindow)
    {
        return real_XMapWindow(display, window);
    }

    return 0;
}

int XIfEvent(Display *display, XEvent *event_return, XIfEventPredicate predicate, XPointer arg)
{
    if (!real_XIfEvent_func)
    {
        real_XIfEvent_func = (real_XIfEvent_t)dlsym(RTLD_NEXT, "XIfEvent");
    }

    /* The game blocks in XIfEvent waiting for a MapNotify on the window it
     * thinks it created. That window is really our SDL window, which SDL
     * has already mapped, so synthesise the MapNotify and return. */
    if (((uintptr_t)predicate == 0x080cd388 || (uintptr_t)predicate == 0x80d7190) && graphics_is_sdl_window((Window)(uintptr_t)arg))
    {
        debug("[graphics] XIfEvent: synthesizing MapNotify for window 0x%lx\n",
            (unsigned long)sdl_x11_window);

        if (event_return)
        {
            memset(event_return, 0, sizeof(XEvent));
            event_return->type = MapNotify;
            event_return->xmap.display = display;
            event_return->xmap.event = sdl_x11_window;
            event_return->xmap.window = sdl_x11_window;
            event_return->xmap.override_redirect = False;
        }

        return 1;
    }

    if (!real_XIfEvent_func)
    {
        debug("[graphics] XIfEvent: real XIfEvent unavailable\n");
        return 0;
    }

    current_predicate = predicate;
    current_predicate_arg = arg;

    int result = real_XIfEvent_func(display, event_return, proxy_x_if_event_predicate, arg);

    current_predicate = NULL;
    current_predicate_arg = NULL;

    return result;
}

/* ===================================================================== *
 *  GLX -> SDL GL context                                                 *
 *                                                                       *
 *  The game uses the classic GLX 1.2 API (glXChooseVisual +             *
 *  glXCreateContext + glXMakeCurrent + glXSwapBuffers). That path is    *
 *  broken under GLVND / PRIME render offload - glXChooseVisual returns  *
 *  NULL on the server's screen and glXCreateContext(NULL) is BadValue.  *
 *  SDL already made a working context in graphics_init_window(); route  *
 *  the game's calls straight to it.                                     *
 * ===================================================================== */

/* Caller XFree()s the result, matching glXChooseVisual semantics. Only
 * used so the game has a non-NULL XVisualInfo to hand around; the actual
 * context is SDL's regardless of what visual is in here. */
XVisualInfo *glXChooseVisual(Display *dpy, int screen, int *attribList)
{
    (void)attribList;

    XVisualInfo tmpl;
    memset(&tmpl, 0, sizeof(tmpl));
    tmpl.screen = screen;

    if (dpy && sdl_x11_window)
    {
        XWindowAttributes wa;

        if (XGetWindowAttributes(dpy, sdl_x11_window, &wa) && wa.visual)
        {
            tmpl.visualid = XVisualIDFromVisual(wa.visual);

            int n = 0;
            XVisualInfo *vi = XGetVisualInfo(dpy, VisualIDMask, &tmpl, &n);

            if (vi && n > 0)
            {
                debug("[graphics] glXChooseVisual -> SDL window visual 0x%lx\n",
                    (unsigned long)vi->visualid);
                return vi;
            }

            if (vi)
            {
                XFree(vi);
            }
        }
    }

    /* Fall back to the server's default visual - good enough, the game
     * only passes this pointer to glXCreateContext, which we ignore. */
    int n = 0;
    tmpl.visualid = 0;
    XVisualInfo *vi = XGetVisualInfo(dpy, 0, &tmpl, &n);

    if (vi && n > 0)
    {
        return vi;
    }

    if (vi)
    {
        XFree(vi);
    }

    return NULL;
}

GLXContext glXCreateContext(Display *dpy, XVisualInfo *vis, GLXContext share, Bool direct)
{
    (void)dpy;
    (void)vis;
    (void)share;
    (void)direct;

    if (sdl_gl_context)
    {
        debug("[graphics] glXCreateContext -> SDL GL context %p\n", sdl_gl_context);
        return (GLXContext)sdl_gl_context;
    }

    /* SDL context creation failed earlier - last-ditch: try the real one. */
    static GLXContext (*real_fn)(Display *, XVisualInfo *, GLXContext, Bool) = NULL;

    if (!real_fn)
    {
        real_fn = (GLXContext (*)(Display *, XVisualInfo *, GLXContext, Bool))dlsym(RTLD_NEXT, "glXCreateContext");
    }

    return real_fn ? real_fn(dpy, vis, share, direct) : NULL;
}

Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx)
{
    (void)dpy;
    (void)drawable;
    (void)ctx;

    if (sdl_gl_context && sdl_window)
    {
        return SDL_GL_MakeCurrent(sdl_window, sdl_gl_context) == 0 ? True : False;
    }

    static Bool (*real_fn)(Display *, GLXDrawable, GLXContext) = NULL;

    if (!real_fn)
    {
        real_fn = (Bool (*)(Display *, GLXDrawable, GLXContext))dlsym(RTLD_NEXT, "glXMakeCurrent");
    }

    return real_fn ? real_fn(dpy, drawable, ctx) : False;
}

void glXSwapBuffers(Display *dpy, GLXDrawable drawable)
{
    (void)dpy;
    (void)drawable;

    if (sdl_window)
    {
        SDL_GL_SwapWindow(sdl_window);
        return;
    }

    static void (*real_fn)(Display *, GLXDrawable) = NULL;

    if (!real_fn)
    {
        real_fn = (void (*)(Display *, GLXDrawable))dlsym(RTLD_NEXT, "glXSwapBuffers");
    }

    if (real_fn)
    {
        real_fn(dpy, drawable);
    }
}

/* ===================================================================== *
 *  glXAllocateMemoryNV - NOT feature emulation, just a working malloc.   *
 *                                                                       *
 *  The games call glXAllocateMemoryNV() once at startup to get a ~24 MB  *
 *  block they keep their vertex/index data in, and abort with "Couldn't  *
 *  allocate Video Memory" if it returns NULL. This legacy AGP-memory     *
 *  entry point returns NULL on Mesa and on NVIDIA under PRIME render     *
 *  offload, so hand back ordinary heap memory - the data is still        *
 *  submitted through plain glVertexPointer / glDrawElements.             *
 *  glVertexArrayRangeNV (a pure "this range is fast" hint) is a no-op.   *
 * ===================================================================== */

void *glXAllocateMemoryNV(int size, float readfreq, float writefreq, float priority)
{
    (void)readfreq;
    (void)writefreq;
    (void)priority;

    if (size <= 0)
    {
        return NULL;
    }

    return malloc((size_t)size);
}

void glXFreeMemoryNV(void *pointer)
{
    free(pointer);
}

void glVertexArrayRangeNV(int length, const void *pointer)
{
    (void)length;
    (void)pointer;
}

/* ===================================================================== *
 *  XFree86-VidMode gamma-ramp stubs                                      *
 *                                                                       *
 *  The games poke the display gamma ramp through these; on a windowed /  *
 *  compositor setup that either fails or affects the whole screen, so    *
 *  we just no-op set and return a linear ramp for get.                   *
 * ===================================================================== */

int _XF86VidModeSetGammaRamp(
    void *display,
    int screen,
    int size,
    unsigned short *red,
    unsigned short *green,
    unsigned short *blue)
{
    (void)display;
    (void)screen;
    (void)size;
    (void)red;
    (void)green;
    (void)blue;

    return 1;
}

int _XF86VidModeGetGammaRamp(
    void *display,
    int screen,
    int size,
    unsigned short *red,
    unsigned short *green,
    unsigned short *blue)
{
    (void)display;
    (void)screen;

    if (size > 0)
    {
        for (int i = 0; i < size; i++)
        {
            unsigned short value;

            if (size == 1)
                value = 65535;
            else
                value = (unsigned short)((i * 65535) / (size - 1));

            if (red)
                red[i] = value;

            if (green)
                green[i] = value;

            if (blue)
                blue[i] = value;
        }
    }

    return 1;
}

/* ===================================================================== *
 *  Render-to-window scaling                                              *
 *                                                                       *
 *  The game renders at a fixed render_width x render_height and sets its  *
 *  viewport in that space. We remap every glViewport into the scaled,    *
 *  aspect-preserved rectangle inside the real window and keep a matching  *
 *  glScissor enabled, so the game's own glClear/draws stay inside the    *
 *  image; glClear() paints the letterbox bars black each frame.          *
 *                                                                       *
 *  These are the only GL entry points we touch.                          *
 * ===================================================================== */

typedef void (*gl_clearcolor_t)(float, float, float, float);
typedef void (*gl_clear_t)(unsigned int);
typedef void (*gl_scissor_t)(int, int, int, int);
typedef void (*gl_cap_t)(unsigned int);
typedef void (*gl_viewport_t)(int, int, int, int);

static gl_clearcolor_t real_glClearColor = NULL;
static gl_clear_t real_glClear = NULL;
static gl_scissor_t real_glScissor = NULL;
static gl_cap_t real_glEnable_gl = NULL;
static gl_cap_t real_glDisable_gl = NULL;
static gl_viewport_t real_glViewport = NULL;

#define GAELCO_GL_SCISSOR_TEST      0x0C11
#define GAELCO_GL_COLOR_BUFFER_BIT  0x00004000
#define GAELCO_GL_DEPTH_BUFFER_BIT  0x00000100

static void resolve_scale_gl(void)
{
    if (real_glClear)
    {
        return;
    }

    real_glClear = (gl_clear_t)dlsym(RTLD_NEXT, "glClear");
    real_glScissor = (gl_scissor_t)dlsym(RTLD_NEXT, "glScissor");
    real_glEnable_gl = (gl_cap_t)dlsym(RTLD_NEXT, "glEnable");
    real_glDisable_gl = (gl_cap_t)dlsym(RTLD_NEXT, "glDisable");
    real_glViewport = (gl_viewport_t)dlsym(RTLD_NEXT, "glViewport");

    if (!real_glClearColor)
    {
        real_glClearColor = (gl_clearcolor_t)dlsym(RTLD_NEXT, "glClearColor");
    }
}

/* Last clear colour the game asked for, so we can restore it after
 * painting the letterbox bars black. */
static float game_clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

void glClearColor(float r, float g, float b, float a)
{
    resolve_scale_gl();

    game_clear_color[0] = r;
    game_clear_color[1] = g;
    game_clear_color[2] = b;
    game_clear_color[3] = a;

    if (real_glClearColor)
    {
        real_glClearColor(r, g, b, a);
    }
}

void glClear(unsigned int mask)
{
    resolve_scale_gl();

    if (graphics_scaling_active && real_glClear && real_glScissor &&
        real_glClearColor && real_glEnable_gl && real_glDisable_gl)
    {
        int cx, cy, cw, ch;
        graphics_content_rect(&cx, &cy, &cw, &ch);

        /* Wipe the whole framebuffer (bars included) black, ignoring the
         * game's clear colour, then restrict its own clear to the image. */
        real_glDisable_gl(GAELCO_GL_SCISSOR_TEST);
        real_glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        real_glClear(GAELCO_GL_COLOR_BUFFER_BIT | GAELCO_GL_DEPTH_BUFFER_BIT);
        real_glClearColor(game_clear_color[0], game_clear_color[1],
            game_clear_color[2], game_clear_color[3]);

        real_glScissor(cx, cy, cw, ch);
        real_glEnable_gl(GAELCO_GL_SCISSOR_TEST);
    }

    if (real_glClear)
    {
        real_glClear(mask);
    }
}

void glViewport(int x, int y, int width, int height)
{
    resolve_scale_gl();

    static int logged = 0;

    if (!logged)
    {
        logged = 1;
        fprintf(stderr,
            "[graphics] glViewport(%d,%d,%d,%d); window %dx%d, scaling=%d\n",
            x, y, width, height, window_width, window_height, graphics_scaling_active);
    }

    if (graphics_scaling_active && real_glViewport)
    {
        int rx, ry, rx2, ry2;
        graphics_map_point(x, y, &rx, &ry);
        graphics_map_point(x + width, y + height, &rx2, &ry2);
        real_glViewport(rx, ry, rx2 - rx, ry2 - ry);

        if (real_glScissor && real_glEnable_gl)
        {
            int cx, cy, cw, ch;
            graphics_content_rect(&cx, &cy, &cw, &ch);
            real_glScissor(cx, cy, cw, ch);
            real_glEnable_gl(GAELCO_GL_SCISSOR_TEST);
        }

        return;
    }

    if (real_glViewport)
    {
        real_glViewport(x, y, width, height);
    }
}
