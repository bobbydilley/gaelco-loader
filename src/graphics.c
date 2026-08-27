#define _GNU_SOURCE

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "controls.h"
#include "graphics.h"
#include "utils.h"

SDL_Window *sdl_window = NULL;
Display *sdl_display = NULL;
Window sdl_x11_window = None;
int sdl_initialized = 0;
int creating_sdl = 0;
int window_width = 640;
int window_height = 480;

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

    if (width > 0)
    {
        window_width = width;
    }

    if (height > 0)
    {
        window_height = height;
    }

    debug("[graphics] creating SDL window %dx%d\n",
        window_width,
        window_height);

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

    sdl_window = SDL_CreateWindow(
        getGameName(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        window_width,
        window_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);

    if (!sdl_window)
    {
        debug("[graphics] SDL_CreateWindow failed: %s\n",
            SDL_GetError());
        creating_sdl = 0;
        return 0;
    }

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
        if (width > 0)
        {
            window_width = (int)width;
        }

        if (height > 0)
        {
            window_height = (int)height;
        }

        if (!graphics_init_window(window_width, window_height))
        {
            debug("[graphics] XCreateWindow: SDL window creation failed\n");
            return None;
        }

        controls_start_input_thread();
    }

    debug("[graphics] XCreateWindow intercepted:\n"
        "          requested parent = 0x%lx\n"
        "          requested x      = %d\n"
        "          requested y      = %d\n"
        "          requested size   = %ux%u\n"
        "          requested border = %u\n"
        "          requested depth  = %d\n"
        "          requested class  = %u\n"
        "          requested visual = %p\n"
        "          requested mask   = 0x%lx\n"
        "          SDL window       = 0x%lx\n",
        (unsigned long)parent,
        x,
        y,
        width,
        height,
        border_width,
        depth,
        class,
        (void *)visual,
        valueMask,
        (unsigned long)sdl_x11_window);

    if (width > 0 && height > 0 && ((int)width != window_width || (int)height != window_height))
    {
        window_width = (int)width;
        window_height = (int)height;
        SDL_SetWindowSize(sdl_window, window_width, window_height);
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

    debug("[graphics] XIfEvent(display=%p, event_return=%p, predicate=%p, arg=%p)\n",
        (void *)display,
        (void *)event_return,
        (void *)predicate,
        (void *)arg);

    if (((uintptr_t)predicate == 0x080cd388 || (uintptr_t)predicate == 0x80d7190) && graphics_is_sdl_window((Window)(uintptr_t)arg))
    {
        debug("[graphics] XIfEvent: detected WaitForMapNotify()\n");
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

        debug("[graphics] XIfEvent: returning synthetic MapNotify\n");
        return 1;
    }

    if (!real_XIfEvent_func)
    {
        debug("[graphics] XIfEvent: real XIfEvent unavailable\n");
        return 0;
    }

    {
        int pending = XPending(display);
        debug("[graphics] XIfEvent: %d event(s) currently pending\n", pending);

        if (pending > 0)
        {
            XEvent peeked;
            memset(&peeked, 0, sizeof(peeked));
            if (XPeekEvent(display, &peeked))
            {
            }
        }
    }

    current_predicate = predicate;
    current_predicate_arg = arg;

    debug("[graphics] XIfEvent: entering real XIfEvent with predicate proxy\n");

    int result = real_XIfEvent_func(display, event_return, proxy_x_if_event_predicate, arg);

    current_predicate = NULL;
    current_predicate_arg = NULL;

    debug("[graphics] XIfEvent: returned %d\n", result);
    return result;
}

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

/*
 * Diagnostics: lightweight GL error checking + one-time driver info dump,
 * so a real play session tells us what's actually happening instead of
 * more static-analysis guesses. Resolved lazily via dlsym like the X11
 * hooks above, since we don't link libGL directly.
 */
typedef unsigned int (*glGetError_t)(void);
typedef const unsigned char *(*glGetString_t)(unsigned int name);
typedef int (*real_glXMakeCurrent_t)(Display *, XID, void *);

static glGetError_t real_glGetError = NULL;
static glGetString_t real_glGetString = NULL;
static int driver_info_logged = 0;

static void resolve_diag_gl(void)
{
    if (!real_glGetError)
    {
        real_glGetError = (glGetError_t)dlsym(RTLD_NEXT, "glGetError");
    }

    if (!real_glGetString)
    {
        real_glGetString = (glGetString_t)dlsym(RTLD_NEXT, "glGetString");
    }
}

static void log_gl_errors(const char *where)
{
    resolve_diag_gl();

    if (!real_glGetError)
    {
        return;
    }

    unsigned int err;

    while ((err = real_glGetError()) != 0 /* GL_NO_ERROR */)
    {
        fprintf(stderr, "[graphics][GL ERROR] %s -> 0x%04x\n", where, err);
    }
}

/* Logs the first N calls of a given tag, then goes quiet - avoids flooding
 * stderr from functions called per-object/per-frame while still capturing
 * enough of the pattern to be useful. */
static int log_budget(int *counter, int max)
{
    if (*counter >= max)
    {
        return 0;
    }

    (*counter)++;
    return 1;
}

int glXMakeCurrent(Display *dpy, XID drawable, void *ctx)
{
    static real_glXMakeCurrent_t real_glXMakeCurrent = NULL;

    if (!real_glXMakeCurrent)
    {
        real_glXMakeCurrent = (real_glXMakeCurrent_t)dlsym(RTLD_NEXT, "glXMakeCurrent");
    }

    int result = real_glXMakeCurrent ? real_glXMakeCurrent(dpy, drawable, ctx) : 0;

    if (result && !driver_info_logged)
    {
        driver_info_logged = 1;
        resolve_diag_gl();

        if (real_glGetString)
        {
            const unsigned char *vendor = real_glGetString(0x1F00 /* GL_VENDOR */);
            const unsigned char *renderer = real_glGetString(0x1F01 /* GL_RENDERER */);
            const unsigned char *version = real_glGetString(0x1F02 /* GL_VERSION */);
            const unsigned char *extensions = real_glGetString(0x1F03 /* GL_EXTENSIONS */);

            fprintf(stderr, "[graphics][GL] vendor=%s\n", vendor ? (const char *)vendor : "(null)");
            fprintf(stderr, "[graphics][GL] renderer=%s\n", renderer ? (const char *)renderer : "(null)");
            fprintf(stderr, "[graphics][GL] version=%s\n", version ? (const char *)version : "(null)");
            fprintf(stderr, "[graphics][GL] has GL_NV_register_combiners=%s\n",
                (extensions && strstr((const char *)extensions, "GL_NV_register_combiners")) ? "yes" : "no");
            fprintf(stderr, "[graphics][GL] has GL_ARB_multitexture=%s\n",
                (extensions && strstr((const char *)extensions, "GL_ARB_multitexture")) ? "yes" : "no");
            fprintf(stderr, "[graphics][GL] has GL_EXT_vertex_weighting=%s\n",
                (extensions && strstr((const char *)extensions, "GL_EXT_vertex_weighting")) ? "yes" : "no");
        }
    }

    log_gl_errors("glXMakeCurrent");

    return result;
}

/*
 * GL_NV_register_combiners is a fixed-function multitexture/lighting
 * pipeline that predates shaders - NVIDIA-only, never adopted by Mesa.
 * Reimplementing its exact per-stage math is out of scope, so these are
 * inert: the state they'd configure is simply dropped. We tried
 * approximating GL_CONSTANT_COLOR0/1_NV with an extra multiply texture
 * unit, but that made rendering worse (much darker), so it's reverted -
 * these are back to plain no-ops while we gather real data on what's
 * actually going on via the logging above and below.
 */
void glCombinerParameterfvNV(unsigned int pname, const float *params)
{
    static int count = 0;

    if (log_budget(&count, 150))
    {
        fprintf(stderr, "[graphics][combiner] ParameterfvNV pname=0x%04x params=(%.3f,%.3f,%.3f,%.3f)\n",
            pname,
            params ? params[0] : 0.0f,
            params ? params[1] : 0.0f,
            params ? params[2] : 0.0f,
            params ? params[3] : 0.0f);
    }

    log_gl_errors("glCombinerParameterfvNV");
}

void glCombinerParameteriNV(unsigned int pname, int param)
{
    static int count = 0;

    if (log_budget(&count, 150))
    {
        fprintf(stderr, "[graphics][combiner] ParameteriNV pname=0x%04x param=0x%04x\n", pname, (unsigned int)param);
    }

    log_gl_errors("glCombinerParameteriNV");
}

void glCombinerInputNV(unsigned int stage, unsigned int portion, unsigned int variable, unsigned int input, unsigned int mapping, unsigned int componentUsage)
{
    static int count = 0;

    if (log_budget(&count, 300))
    {
        fprintf(stderr, "[graphics][combiner] InputNV stage=0x%04x portion=0x%04x variable=0x%04x input=0x%04x mapping=0x%04x usage=0x%04x\n",
            stage, portion, variable, input, mapping, componentUsage);
    }

    log_gl_errors("glCombinerInputNV");
}

void glCombinerOutputNV(unsigned int stage, unsigned int portion, unsigned int abOutput, unsigned int cdOutput, unsigned int sumOutput, unsigned int scale, unsigned int bias, unsigned char abDotProduct, unsigned char cdDotProduct, unsigned char muxSum)
{
    static int count = 0;

    if (log_budget(&count, 150))
    {
        fprintf(stderr, "[graphics][combiner] OutputNV stage=0x%04x portion=0x%04x ab=0x%04x cd=0x%04x sum=0x%04x scale=0x%04x bias=0x%04x abDot=%u cdDot=%u mux=%u\n",
            stage, portion, abOutput, cdOutput, sumOutput, scale, bias, abDotProduct, cdDotProduct, muxSum);
    }

    log_gl_errors("glCombinerOutputNV");
}

void glFinalCombinerInputNV(unsigned int variable, unsigned int input, unsigned int mapping, unsigned int componentUsage)
{
    static int count = 0;

    if (log_budget(&count, 150))
    {
        fprintf(stderr, "[graphics][combiner] FinalCombinerInputNV variable=0x%04x input=0x%04x mapping=0x%04x usage=0x%04x\n",
            variable, input, mapping, componentUsage);
    }

    log_gl_errors("glFinalCombinerInputNV");
}

/*
 * GL_NV_fence: the game only calls glGenFencesNV (not SetFenceNV/
 * TestFenceNV/FinishFenceNV/IsFenceNV, which aren't in its import table,
 * so it's presumably only using this to probe for the extension or as
 * dead code). Handing back distinct nonzero-looking handles is enough to
 * satisfy the call without crashing; nothing ever waits on them.
 */
void glGenFencesNV(int n, unsigned int *fences)
{
    fprintf(stderr, "[graphics][fence] glGenFencesNV n=%d\n", n);

    if (fences)
    {
        for (int i = 0; i < n; i++)
        {
            fences[i] = (unsigned int)(i + 1);
        }
    }

    log_gl_errors("glGenFencesNV");
}

/*
 * GL_NV_vertex_array_range / GLX_NV_vertex_array_range: an NVIDIA-only
 * fast path for streaming vertex data through pinned AGP/VRAM memory.
 * glVertexArrayRangeNV is purely a performance hint (it just marks a
 * range as eligible for the fast path) - safe to drop entirely, since the
 * game still submits geometry through the ordinary glVertexPointer/
 * glDrawElements calls either way. glXAllocateMemoryNV must return real,
 * usable memory though: the game uses it as its vertex/texture storage,
 * and a NULL return would look like an allocation failure. Regular heap
 * memory works fine here - we just don't get the AGP fast path, which
 * doesn't matter on modern hardware.
 */
void glVertexArrayRangeNV(int length, const void *pointer)
{
    fprintf(stderr, "[graphics][var] glVertexArrayRangeNV length=%d pointer=%p\n", length, pointer);
    log_gl_errors("glVertexArrayRangeNV");
}

void *glXAllocateMemoryNV(int size, float readfreq, float writefreq, float priority)
{
    fprintf(stderr, "[graphics][var] glXAllocateMemoryNV size=%d readfreq=%.3f writefreq=%.3f priority=%.3f\n",
        size, readfreq, writefreq, priority);

    if (size <= 0)
    {
        return NULL;
    }

    void *ptr = malloc((size_t)size);

    fprintf(stderr, "[graphics][var] glXAllocateMemoryNV -> %p\n", ptr);

    return ptr;
}

void glXFreeMemoryNV(void *pointer)
{
    fprintf(stderr, "[graphics][var] glXFreeMemoryNV pointer=%p\n", pointer);
    free(pointer);
}

/*
 * GL_EXT_vertex_weighting: GPU-side vertex blending for skinned meshes.
 * Only one call site exists in the whole binary, which makes it an
 * unlikely explanation for limbs missing on multiple characters - but
 * logging every call (there should only be a handful) costs nothing and
 * rules it in or out.
 */
void glVertexWeightPointerEXT(int size, unsigned int type, int stride, const void *pointer)
{
    fprintf(stderr, "[graphics][weight] glVertexWeightPointerEXT size=%d type=0x%04x stride=%d pointer=%p\n",
        size, type, stride, pointer);
    log_gl_errors("glVertexWeightPointerEXT");
}