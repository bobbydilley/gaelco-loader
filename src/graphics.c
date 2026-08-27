#define _GNU_SOURCE

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <dlfcn.h>
#include <math.h>
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
 * Reimplementing its exact per-stage math in general is out of scope, so
 * glCombinerInputNV/OutputNV/glFinalCombinerInputNV mostly just track
 * state (see below) rather than doing anything themselves.
 *
 * The logged session confirmed GL_CONSTANT_COLOR0/1_NV carry real,
 * frequently-changing per-object tint data (we saw it set to pure red,
 * various grays, a fading green, etc - not just a neutral default). Our
 * first attempt at approximating this with an always-on extra texture
 * unit made rendering worse (uniformly darker), because it applied to
 * every draw regardless of whether that draw's own combiner program
 * actually referenced the constant color at all.
 *
 * This version fixes that: glCombinerInputNV/glFinalCombinerInputNV are
 * called to define this game's programs anew before basically every draw
 * (see GL_NUM_GENERAL_COMBINERS_NV, pname 0x854E, as the reset point), so
 * we track - per program - whether GL_CONSTANT_COLOR0/1_NV was actually
 * referenced as an input, and only enable our approximation texture unit
 * when the program being set up right now uses it. Otherwise we make
 * sure it's off, so unrelated draws are unaffected.
 */
#define GAELCO_CONST_COLOR0_UNIT 0x84C3 /* GL_TEXTURE3_ARB - unused by the game */
#define GAELCO_CONST_COLOR1_UNIT 0x84C4 /* GL_TEXTURE4_ARB - unused by the game */
#define GAELCO_GL_CONSTANT_COLOR0_NV 0x852A
#define GAELCO_GL_CONSTANT_COLOR1_NV 0x852B
#define GAELCO_GL_NUM_GENERAL_COMBINERS_NV 0x854E

typedef void (*glActiveTextureARB_t)(unsigned int texture);
typedef void (*glBindTexture_t)(unsigned int target, unsigned int texture);
typedef void (*glGenTextures_t)(int n, unsigned int *textures);
typedef void (*glTexParameteri_t)(unsigned int target, unsigned int pname, int param);
typedef void (*glTexImage2D_t)(unsigned int target, int level, int internalformat, int width, int height, int border, unsigned int format, unsigned int type, const void *pixels);
typedef void (*glTexEnvi_t)(unsigned int target, unsigned int pname, int param);
typedef void (*glDisable_t)(unsigned int cap);
typedef void (*glGetIntegerv_t)(unsigned int pname, int *params);

static glActiveTextureARB_t real_glActiveTextureARB = NULL;
static glBindTexture_t real_glBindTexture = NULL;
static glGenTextures_t real_glGenTextures = NULL;
static glTexParameteri_t real_glTexParameteri = NULL;
static glTexImage2D_t real_glTexImage2D = NULL;
static glTexEnvi_t real_glTexEnvi = NULL;
static glDisable_t real_glDisable = NULL;
static glGetIntegerv_t real_glGetIntegerv = NULL;

static unsigned int const_color_tex[2] = {0, 0};
static unsigned char const_color_last[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}};
static int const_color_has_last[2] = {0, 0};
static int const_color_enabled[2] = {0, 0};

/* Whether the combiner program currently being defined references
 * CONSTANT_COLOR0/1_NV as an input - reset each time the game starts a
 * new program (GL_NUM_GENERAL_COMBINERS_NV), set by glCombinerInputNV/
 * glFinalCombinerInputNV below. */
static int program_uses_const[2] = {0, 0};

/* Defined further down alongside the vertex-weighting emulation, which
 * also needs to override glEnable/glDisable globally - declared here so
 * resolve_real_gl() below can resolve/use it too. */
extern void (*gaelco_real_glEnable)(unsigned int cap);

static void resolve_real_gl(void)
{
    if (real_glActiveTextureARB)
    {
        return;
    }

    real_glActiveTextureARB = (glActiveTextureARB_t)dlsym(RTLD_NEXT, "glActiveTextureARB");
    real_glBindTexture = (glBindTexture_t)dlsym(RTLD_NEXT, "glBindTexture");
    real_glGenTextures = (glGenTextures_t)dlsym(RTLD_NEXT, "glGenTextures");
    real_glTexParameteri = (glTexParameteri_t)dlsym(RTLD_NEXT, "glTexParameteri");
    real_glTexImage2D = (glTexImage2D_t)dlsym(RTLD_NEXT, "glTexImage2D");
    real_glTexEnvi = (glTexEnvi_t)dlsym(RTLD_NEXT, "glTexEnvi");
    real_glDisable = (glDisable_t)dlsym(RTLD_NEXT, "glDisable");
    real_glGetIntegerv = (glGetIntegerv_t)dlsym(RTLD_NEXT, "glGetIntegerv");

    if (!gaelco_real_glEnable)
    {
        gaelco_real_glEnable = (void (*)(unsigned int))dlsym(RTLD_NEXT, "glEnable");
    }
}

static void apply_constant_color(int slot, unsigned int texture_unit, const float *params, int in_use)
{
    resolve_real_gl();

    if (!real_glActiveTextureARB || !real_glBindTexture || !real_glGenTextures ||
        !real_glTexParameteri || !real_glTexImage2D || !real_glTexEnvi ||
        !real_glDisable || !real_glGetIntegerv || !gaelco_real_glEnable)
    {
        return;
    }

    int previous_active = 0;

    if (!in_use)
    {
        if (const_color_enabled[slot])
        {
            real_glGetIntegerv(0x84E0 /* GL_ACTIVE_TEXTURE_ARB */, &previous_active);
            real_glActiveTextureARB(texture_unit);
            real_glDisable(0x0DE1 /* GL_TEXTURE_2D */);
            real_glActiveTextureARB((unsigned int)previous_active);
            const_color_enabled[slot] = 0;
        }

        return;
    }

    unsigned char rgba[4];

    for (int i = 0; i < 4; i++)
    {
        float v = params[i];

        if (v < 0.0f)
        {
            v = 0.0f;
        }
        else if (v > 1.0f)
        {
            v = 1.0f;
        }

        rgba[i] = (unsigned char)(v * 255.0f + 0.5f);
    }

    int color_changed = !const_color_has_last[slot] || memcmp(rgba, const_color_last[slot], sizeof(rgba)) != 0;

    if (!color_changed && const_color_enabled[slot])
    {
        return;
    }

    memcpy(const_color_last[slot], rgba, sizeof(rgba));
    const_color_has_last[slot] = 1;

    real_glGetIntegerv(0x84E0 /* GL_ACTIVE_TEXTURE_ARB */, &previous_active);
    real_glActiveTextureARB(texture_unit);

    if (const_color_tex[slot] == 0)
    {
        real_glGenTextures(1, &const_color_tex[slot]);
        real_glBindTexture(0x0DE1 /* GL_TEXTURE_2D */, const_color_tex[slot]);
        real_glTexParameteri(0x0DE1, 0x2801 /* GL_TEXTURE_MIN_FILTER */, 0x2600 /* GL_NEAREST */);
        real_glTexParameteri(0x0DE1, 0x2800 /* GL_TEXTURE_MAG_FILTER */, 0x2600 /* GL_NEAREST */);
        real_glTexEnvi(0x2300 /* GL_TEXTURE_ENV */, 0x2200 /* GL_TEXTURE_ENV_MODE */, 0x2100 /* GL_MODULATE */);
    }
    else
    {
        real_glBindTexture(0x0DE1, const_color_tex[slot]);
    }

    if (color_changed)
    {
        real_glTexImage2D(0x0DE1, 0, 0x1908 /* GL_RGBA */, 1, 1, 0, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, rgba);
    }

    if (!const_color_enabled[slot])
    {
        gaelco_real_glEnable(0x0DE1);
        const_color_enabled[slot] = 1;
    }

    real_glActiveTextureARB((unsigned int)previous_active);
}

void glCombinerParameterfvNV(unsigned int pname, const float *params)
{
    if (!params)
    {
        return;
    }

    if (pname == GAELCO_GL_CONSTANT_COLOR0_NV)
    {
        apply_constant_color(0, GAELCO_CONST_COLOR0_UNIT, params, program_uses_const[0]);
    }
    else if (pname == GAELCO_GL_CONSTANT_COLOR1_NV)
    {
        apply_constant_color(1, GAELCO_CONST_COLOR1_UNIT, params, program_uses_const[1]);
    }
}

void glCombinerParameteriNV(unsigned int pname, int param)
{
    if (pname == GAELCO_GL_NUM_GENERAL_COMBINERS_NV)
    {
        program_uses_const[0] = 0;
        program_uses_const[1] = 0;
    }

    (void)param;
}

void glCombinerInputNV(unsigned int stage, unsigned int portion, unsigned int variable, unsigned int input, unsigned int mapping, unsigned int componentUsage)
{
    (void)stage;
    (void)portion;
    (void)variable;
    (void)mapping;
    (void)componentUsage;

    if (input == GAELCO_GL_CONSTANT_COLOR0_NV)
    {
        program_uses_const[0] = 1;
    }
    else if (input == GAELCO_GL_CONSTANT_COLOR1_NV)
    {
        program_uses_const[1] = 1;
    }
}

void glCombinerOutputNV(unsigned int stage, unsigned int portion, unsigned int abOutput, unsigned int cdOutput, unsigned int sumOutput, unsigned int scale, unsigned int bias, unsigned char abDotProduct, unsigned char cdDotProduct, unsigned char muxSum)
{
    (void)stage;
    (void)portion;
    (void)abOutput;
    (void)cdOutput;
    (void)sumOutput;
    (void)scale;
    (void)bias;
    (void)abDotProduct;
    (void)cdDotProduct;
    (void)muxSum;
}

void glFinalCombinerInputNV(unsigned int variable, unsigned int input, unsigned int mapping, unsigned int componentUsage)
{
    (void)variable;
    (void)mapping;
    (void)componentUsage;

    if (input == GAELCO_GL_CONSTANT_COLOR0_NV)
    {
        program_uses_const[0] = 1;
    }
    else if (input == GAELCO_GL_CONSTANT_COLOR1_NV)
    {
        program_uses_const[1] = 1;
    }
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
 * GL_EXT_vertex_weighting: two-bone GPU vertex skinning. The logged
 * session showed 1321 calls to glVertexWeightPointerEXT (our earlier
 * static analysis only found one call *site*, which was misleading - it's
 * inside a per-character setup routine) with real per-vertex weight data,
 * and GL_EXT_vertex_weighting isn't in Mesa's extension string at all.
 * That's a solid match for "missing limbs": per the spec, each weighted
 * vertex's eye-space position is
 *
 *   pos = weight * (MODELVIEW0 * objpos) + (1 - weight) * (MODELVIEW1 * objpos)
 *
 * blending between the standard GL_MODELVIEW matrix and a second,
 * separate GL_MODELVIEW1_EXT stack (typically one bone's transform vs.
 * another's, for the joints where two body parts meet). Mesa has no
 * GL_MODELVIEW1_EXT matrix mode at all, so any glMatrixMode/glLoadMatrix*
 * call the game makes for it either errors and does nothing, or (worse)
 * silently lands on whatever real matrix mode Mesa was last actually in.
 *
 * We emulate this in three parts, all in this file:
 *   1. Shadow GL_MODELVIEW1_EXT as our own CPU-side matrix stack instead
 *      of forwarding it to a mode Mesa doesn't have (glMatrixMode,
 *      glLoadIdentity/glLoadMatrixf/glLoadMatrixd/glMultMatrixf,
 *      glTranslatef, glRotatef, glPushMatrix, glPopMatrix below).
 *   2. Track the position array (glVertexPointer) and weight array
 *      (here) the game has bound.
 *   3. On glDrawElements, if weighting is enabled and both arrays and
 *      both matrices are in a usable state, blend each referenced
 *      vertex's position on the CPU into a scratch buffer, temporarily
 *      swap in that buffer with an identity GL_MODELVIEW, and draw that
 *      instead - otherwise fall through to the normal draw untouched, so
 *      a case we don't handle degrades to today's behavior rather than
 *      breaking something new.
 *
 * This is a best-effort port of a fixed-function GPU feature to the CPU
 * without being able to test it - if it's wrong it should mean skinned
 * geometry looks off in some other way, not a crash, given the fallback
 * path above.
 */
#define GAELCO_GL_MODELVIEW1_EXT 0x850A
#define GAELCO_GL_VERTEX_WEIGHTING_EXT 0x8509
#define GAELCO_GL_MODELVIEW_MATRIX 0x0BA6
#define GAELCO_MAX_BLEND_VERTS 65536

typedef struct
{
    float m[16]; /* column-major, standard GL layout */
} gaelco_mat4_t;

static void mat4_identity(float *m)
{
    for (int i = 0; i < 16; i++)
    {
        m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
}

static void mat4_mul(float *out, const float *a, const float *b)
{
    float r[16];

    for (int col = 0; col < 4; col++)
    {
        for (int row = 0; row < 4; row++)
        {
            float sum = 0.0f;

            for (int k = 0; k < 4; k++)
            {
                sum += a[k * 4 + row] * b[col * 4 + k];
            }

            r[col * 4 + row] = sum;
        }
    }

    memcpy(out, r, sizeof(r));
}

static void mat4_transform_point(float *out3, const float *m, const float *p3)
{
    for (int row = 0; row < 3; row++)
    {
        out3[row] = m[0 * 4 + row] * p3[0] + m[1 * 4 + row] * p3[1] + m[2 * 4 + row] * p3[2] + m[3 * 4 + row];
    }
}

static void mat4_translate(float *m, float x, float y, float z)
{
    float t[16];

    mat4_identity(t);
    t[12] = x;
    t[13] = y;
    t[14] = z;
    mat4_mul(m, m, t);
}

static void mat4_rotate(float *m, float angle_deg, float x, float y, float z)
{
    float len = sqrtf(x * x + y * y + z * z);

    if (len < 1e-6f)
    {
        return;
    }

    x /= len;
    y /= len;
    z /= len;

    float rad = angle_deg * (float)M_PI / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);
    float ic = 1.0f - c;
    float r[16];

    mat4_identity(r);
    r[0] = x * x * ic + c;
    r[1] = y * x * ic + z * s;
    r[2] = x * z * ic - y * s;
    r[4] = x * y * ic - z * s;
    r[5] = y * y * ic + c;
    r[6] = y * z * ic + x * s;
    r[8] = x * z * ic + y * s;
    r[9] = y * z * ic - x * s;
    r[10] = z * z * ic + c;

    mat4_mul(m, m, r);
}

#define GAELCO_MV1_STACK_DEPTH 32

static gaelco_mat4_t modelview1_stack[GAELCO_MV1_STACK_DEPTH];
static int modelview1_sp = 0;
static int modelview1_initialized = 0;
static unsigned int current_matrix_mode = 0x1700 /* GL_MODELVIEW */;
static int vertex_weighting_enabled = 0;

static void ensure_modelview1_initialized(void)
{
    if (!modelview1_initialized)
    {
        mat4_identity(modelview1_stack[0].m);
        modelview1_sp = 0;
        modelview1_initialized = 1;
    }
}

typedef void (*glMatrixMode_t)(unsigned int mode);
typedef void (*glLoadIdentity_t)(void);
typedef void (*glLoadMatrixf_t)(const float *m);
typedef void (*glLoadMatrixd_t)(const double *m);
typedef void (*glMultMatrixf_t)(const float *m);
typedef void (*glTranslatef_t)(float x, float y, float z);
typedef void (*glRotatef_t)(float angle, float x, float y, float z);
typedef void (*glPushMatrix_t)(void);
typedef void (*glPopMatrix_t)(void);
typedef void (*glVertexPointer_t)(int size, unsigned int type, int stride, const void *pointer);
typedef void (*glDrawElements_t)(unsigned int mode, int count, unsigned int type, const void *indices);
typedef void (*glDrawArrays_t)(unsigned int mode, int first, int count);
typedef void (*glGetFloatv_t)(unsigned int pname, float *params);

static glMatrixMode_t real_glMatrixMode = NULL;
static glLoadIdentity_t real_glLoadIdentity = NULL;
static glLoadMatrixf_t real_glLoadMatrixf = NULL;
static glLoadMatrixd_t real_glLoadMatrixd = NULL;
static glMultMatrixf_t real_glMultMatrixf = NULL;
static glTranslatef_t real_glTranslatef = NULL;
static glRotatef_t real_glRotatef = NULL;
static glPushMatrix_t real_glPushMatrix = NULL;
static glPopMatrix_t real_glPopMatrix = NULL;
static glVertexPointer_t real_glVertexPointer = NULL;
static glDrawElements_t real_glDrawElements = NULL;
static glDrawArrays_t real_glDrawArrays = NULL;
static glGetFloatv_t real_glGetFloatv = NULL;

/* Shared with the combiner code above. */
void (*gaelco_real_glEnable)(unsigned int cap) = NULL;

static void resolve_weighting_gl(void)
{
    if (real_glMatrixMode)
    {
        return;
    }

    real_glMatrixMode = (glMatrixMode_t)dlsym(RTLD_NEXT, "glMatrixMode");
    real_glLoadIdentity = (glLoadIdentity_t)dlsym(RTLD_NEXT, "glLoadIdentity");
    real_glLoadMatrixf = (glLoadMatrixf_t)dlsym(RTLD_NEXT, "glLoadMatrixf");
    real_glLoadMatrixd = (glLoadMatrixd_t)dlsym(RTLD_NEXT, "glLoadMatrixd");
    real_glMultMatrixf = (glMultMatrixf_t)dlsym(RTLD_NEXT, "glMultMatrixf");
    real_glTranslatef = (glTranslatef_t)dlsym(RTLD_NEXT, "glTranslatef");
    real_glRotatef = (glRotatef_t)dlsym(RTLD_NEXT, "glRotatef");
    real_glPushMatrix = (glPushMatrix_t)dlsym(RTLD_NEXT, "glPushMatrix");
    real_glPopMatrix = (glPopMatrix_t)dlsym(RTLD_NEXT, "glPopMatrix");
    real_glVertexPointer = (glVertexPointer_t)dlsym(RTLD_NEXT, "glVertexPointer");
    real_glDrawElements = (glDrawElements_t)dlsym(RTLD_NEXT, "glDrawElements");
    real_glDrawArrays = (glDrawArrays_t)dlsym(RTLD_NEXT, "glDrawArrays");
    real_glGetFloatv = (glGetFloatv_t)dlsym(RTLD_NEXT, "glGetFloatv");
    gaelco_real_glEnable = (void (*)(unsigned int))dlsym(RTLD_NEXT, "glEnable");
}

void glMatrixMode(unsigned int mode)
{
    resolve_weighting_gl();
    ensure_modelview1_initialized();

    current_matrix_mode = mode;

    if (mode == GAELCO_GL_MODELVIEW1_EXT)
    {
        /* Don't forward - Mesa doesn't have this mode, and forwarding
         * would either error or silently move a different real stack. */
        return;
    }

    if (real_glMatrixMode)
    {
        real_glMatrixMode(mode);
    }
}

void glLoadIdentity(void)
{
    resolve_weighting_gl();
    ensure_modelview1_initialized();

    if (current_matrix_mode == GAELCO_GL_MODELVIEW1_EXT)
    {
        mat4_identity(modelview1_stack[modelview1_sp].m);
        return;
    }

    if (real_glLoadIdentity)
    {
        real_glLoadIdentity();
    }
}

void glLoadMatrixf(const float *m)
{
    resolve_weighting_gl();
    ensure_modelview1_initialized();

    if (current_matrix_mode == GAELCO_GL_MODELVIEW1_EXT)
    {
        if (m)
        {
            memcpy(modelview1_stack[modelview1_sp].m, m, sizeof(float) * 16);
        }

        return;
    }

    if (real_glLoadMatrixf)
    {
        real_glLoadMatrixf(m);
    }
}

void glLoadMatrixd(const double *m)
{
    resolve_weighting_gl();
    ensure_modelview1_initialized();

    if (current_matrix_mode == GAELCO_GL_MODELVIEW1_EXT)
    {
        if (m)
        {
            for (int i = 0; i < 16; i++)
            {
                modelview1_stack[modelview1_sp].m[i] = (float)m[i];
            }
        }

        return;
    }

    if (real_glLoadMatrixd)
    {
        real_glLoadMatrixd(m);
    }
}

void glMultMatrixf(const float *m)
{
    resolve_weighting_gl();
    ensure_modelview1_initialized();

    if (current_matrix_mode == GAELCO_GL_MODELVIEW1_EXT)
    {
        if (m)
        {
            mat4_mul(modelview1_stack[modelview1_sp].m, modelview1_stack[modelview1_sp].m, m);
        }

        return;
    }

    if (real_glMultMatrixf)
    {
        real_glMultMatrixf(m);
    }
}

void glTranslatef(float x, float y, float z)
{
    resolve_weighting_gl();
    ensure_modelview1_initialized();

    if (current_matrix_mode == GAELCO_GL_MODELVIEW1_EXT)
    {
        mat4_translate(modelview1_stack[modelview1_sp].m, x, y, z);
        return;
    }

    if (real_glTranslatef)
    {
        real_glTranslatef(x, y, z);
    }
}

void glRotatef(float angle, float x, float y, float z)
{
    resolve_weighting_gl();
    ensure_modelview1_initialized();

    if (current_matrix_mode == GAELCO_GL_MODELVIEW1_EXT)
    {
        mat4_rotate(modelview1_stack[modelview1_sp].m, angle, x, y, z);
        return;
    }

    if (real_glRotatef)
    {
        real_glRotatef(angle, x, y, z);
    }
}

void glPushMatrix(void)
{
    resolve_weighting_gl();
    ensure_modelview1_initialized();

    if (current_matrix_mode == GAELCO_GL_MODELVIEW1_EXT)
    {
        if (modelview1_sp + 1 < GAELCO_MV1_STACK_DEPTH)
        {
            modelview1_stack[modelview1_sp + 1] = modelview1_stack[modelview1_sp];
            modelview1_sp++;
        }

        return;
    }

    if (real_glPushMatrix)
    {
        real_glPushMatrix();
    }
}

void glPopMatrix(void)
{
    resolve_weighting_gl();
    ensure_modelview1_initialized();

    if (current_matrix_mode == GAELCO_GL_MODELVIEW1_EXT)
    {
        if (modelview1_sp > 0)
        {
            modelview1_sp--;
        }

        return;
    }

    if (real_glPopMatrix)
    {
        real_glPopMatrix();
    }
}

/* glEnable/glDisable: forward everything except the two NV/EXT tokens
 * Mesa doesn't recognize, which we track ourselves instead. */
void glEnable(unsigned int cap)
{
    resolve_weighting_gl();

    if (cap == GAELCO_GL_VERTEX_WEIGHTING_EXT)
    {
        vertex_weighting_enabled = 1;
        return;
    }

    if (cap == 0x8522 /* GL_REGISTER_COMBINERS_NV */)
    {
        return;
    }

    if (gaelco_real_glEnable)
    {
        gaelco_real_glEnable(cap);
    }
}

void glDisable(unsigned int cap)
{
    resolve_weighting_gl();
    resolve_real_gl(); /* also resolves real_glDisable, shared with the combiner code above */

    if (cap == GAELCO_GL_VERTEX_WEIGHTING_EXT)
    {
        vertex_weighting_enabled = 0;
        return;
    }

    if (cap == 0x8522 /* GL_REGISTER_COMBINERS_NV */)
    {
        return;
    }

    if (real_glDisable)
    {
        real_glDisable(cap);
    }
}

static int vp_size = 0;
static unsigned int vp_type = 0;
static int vp_stride = 0;
static const void *vp_pointer = NULL;

void glVertexPointer(int size, unsigned int type, int stride, const void *pointer)
{
    resolve_weighting_gl();

    vp_size = size;
    vp_type = type;
    vp_stride = stride;
    vp_pointer = pointer;

    if (real_glVertexPointer)
    {
        real_glVertexPointer(size, type, stride, pointer);
    }
}

static int wp_stride = 0;
static unsigned int wp_type = 0;
static const void *wp_pointer = NULL;

void glVertexWeightPointerEXT(int size, unsigned int type, int stride, const void *pointer)
{
    (void)size; /* always 1 per the EXT_vertex_weighting spec */

    wp_type = type;
    wp_stride = stride;
    wp_pointer = pointer;
}

static unsigned int read_index(const void *indices, unsigned int type, int i)
{
    if (type == 0x1401 /* GL_UNSIGNED_BYTE */)
    {
        return ((const unsigned char *)indices)[i];
    }

    if (type == 0x1403 /* GL_UNSIGNED_SHORT */)
    {
        return ((const unsigned short *)indices)[i];
    }

    return ((const unsigned int *)indices)[i];
}

static float blend_scratch[GAELCO_MAX_BLEND_VERTS * 3];

void glDrawElements(unsigned int mode, int count, unsigned int type, const void *indices)
{
    resolve_weighting_gl();
    resolve_real_gl(); /* also resolves real_glGetIntegerv, shared with the combiner code above */

    if (!real_glDrawElements)
    {
        return;
    }

    int can_blend = vertex_weighting_enabled && wp_pointer && vp_pointer &&
        vp_size == 3 && vp_type == 0x1406 /* GL_FLOAT */ && wp_type == 0x1406 /* GL_FLOAT */ &&
        count > 0 && count <= GAELCO_MAX_BLEND_VERTS && real_glVertexPointer &&
        real_glGetFloatv && real_glGetIntegerv && real_glMatrixMode && real_glLoadIdentity &&
        real_glPushMatrix && real_glPopMatrix && real_glDrawArrays && indices;

    if (!can_blend)
    {
        real_glDrawElements(mode, count, type, indices);
        return;
    }

    float m0[16];
    real_glGetFloatv(GAELCO_GL_MODELVIEW_MATRIX, m0);

    const float *m1 = modelview1_stack[modelview1_sp].m;

    for (int i = 0; i < count; i++)
    {
        unsigned int idx = read_index(indices, type, i);

        const unsigned char *pos_base = (const unsigned char *)vp_pointer + (size_t)idx * (size_t)vp_stride;
        const unsigned char *w_base = (const unsigned char *)wp_pointer + (size_t)idx * (size_t)wp_stride;

        float objpos[3];
        memcpy(objpos, pos_base, sizeof(objpos));

        float weight;
        memcpy(&weight, w_base, sizeof(weight));

        if (weight < 0.0f)
        {
            weight = 0.0f;
        }
        else if (weight > 1.0f)
        {
            weight = 1.0f;
        }

        float p0[3];
        float p1[3];

        mat4_transform_point(p0, m0, objpos);
        mat4_transform_point(p1, m1, objpos);

        blend_scratch[i * 3 + 0] = weight * p0[0] + (1.0f - weight) * p1[0];
        blend_scratch[i * 3 + 1] = weight * p0[1] + (1.0f - weight) * p1[1];
        blend_scratch[i * 3 + 2] = weight * p0[2] + (1.0f - weight) * p1[2];
    }

    int previous_matrix_mode = 0x1700;
    real_glGetIntegerv(0x0BA0 /* GL_MATRIX_MODE */, &previous_matrix_mode);

    real_glMatrixMode(0x1700 /* GL_MODELVIEW */);
    real_glPushMatrix();
    real_glLoadIdentity();

    real_glVertexPointer(3, 0x1406 /* GL_FLOAT */, 0, blend_scratch);
    real_glDrawArrays(mode, 0, count);
    real_glVertexPointer(vp_size, vp_type, vp_stride, vp_pointer);

    real_glPopMatrix();
    real_glMatrixMode((unsigned int)previous_matrix_mode);
}