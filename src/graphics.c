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
 * GL_NV_register_combiners is a fixed-function multitexture/lighting
 * pipeline that predates shaders - NVIDIA-only, never adopted by Mesa
 * (GL_REGISTER_COMBINERS_NV isn't even a token Mesa recognizes, so the
 * game's glEnable() for it is silently rejected there). Reimplementing
 * its exact per-stage math in general is out of scope, so
 * glCombinerInputNV/OutputNV/glFinalCombinerInputNV are deliberately
 * inert below. But GL_CONSTANT_COLOR0_NV/GL_CONSTANT_COLOR1_NV turned out
 * to matter: disassembly shows the game recomputing and re-uploading them
 * per-object from real material/lighting data (not just a one-time
 * startup default), and every combiner program we decoded multiplies the
 * final texture*vertex-color result by one of these registers before
 * writing the fragment. Dropping that multiply entirely is our best
 * explanation for lost coloring (e.g. tinted text rendering white).
 *
 * We approximate it with a small trick standard OpenGL 1.x + ARB_multitexture
 * (which Mesa fully supports) can do natively: keep a 1x1 texture on a
 * spare texture unit the game itself never touches, update its color
 * every time the game sets GL_CONSTANT_COLOR{0,1}_NV, and leave that unit
 * enabled in the default GL_MODULATE environment. Because texture units
 * multiply together by default, the game's own unit(s) 0/(1) still do
 * texture*vertex-color exactly as before, and our extra unit multiplies
 * the constant color on top - reproducing the combiner's overall effect
 * without needing to emulate its internal stage graph. This is a best
 * effort we can't verify without running the game.
 */
#define GAELCO_CONST_COLOR0_UNIT 0x84C3 /* GL_TEXTURE3_ARB - unused by the game */
#define GAELCO_CONST_COLOR1_UNIT 0x84C4 /* GL_TEXTURE4_ARB - unused by the game */

typedef void (*glActiveTextureARB_t)(unsigned int texture);
typedef void (*glBindTexture_t)(unsigned int target, unsigned int texture);
typedef void (*glGenTextures_t)(int n, unsigned int *textures);
typedef void (*glTexParameteri_t)(unsigned int target, unsigned int pname, int param);
typedef void (*glTexImage2D_t)(unsigned int target, int level, int internalformat, int width, int height, int border, unsigned int format, unsigned int type, const void *pixels);
typedef void (*glTexEnvi_t)(unsigned int target, unsigned int pname, int param);
typedef void (*glEnable_t)(unsigned int cap);
typedef void (*glGetIntegerv_t)(unsigned int pname, int *params);

static glActiveTextureARB_t real_glActiveTextureARB = NULL;
static glBindTexture_t real_glBindTexture = NULL;
static glGenTextures_t real_glGenTextures = NULL;
static glTexParameteri_t real_glTexParameteri = NULL;
static glTexImage2D_t real_glTexImage2D = NULL;
static glTexEnvi_t real_glTexEnvi = NULL;
static glEnable_t real_glEnable = NULL;
static glGetIntegerv_t real_glGetIntegerv = NULL;

static unsigned int const_color_tex[2] = {0, 0};
static unsigned char const_color_last[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}};
static int const_color_has_last[2] = {0, 0};

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
    real_glEnable = (glEnable_t)dlsym(RTLD_NEXT, "glEnable");
    real_glGetIntegerv = (glGetIntegerv_t)dlsym(RTLD_NEXT, "glGetIntegerv");
}

static void apply_constant_color(int slot, unsigned int texture_unit, const float *params)
{
    if (!real_glActiveTextureARB || !real_glBindTexture || !real_glGenTextures ||
        !real_glTexParameteri || !real_glTexImage2D || !real_glTexEnvi ||
        !real_glEnable || !real_glGetIntegerv)
    {
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

    if (const_color_has_last[slot] && memcmp(rgba, const_color_last[slot], sizeof(rgba)) == 0)
    {
        return;
    }

    memcpy(const_color_last[slot], rgba, sizeof(rgba));
    const_color_has_last[slot] = 1;

    int previous_active = 0;
    real_glGetIntegerv(0x84E0 /* GL_ACTIVE_TEXTURE_ARB */, &previous_active);

    real_glActiveTextureARB(texture_unit);

    if (const_color_tex[slot] == 0)
    {
        real_glGenTextures(1, &const_color_tex[slot]);
        real_glBindTexture(0x0DE1 /* GL_TEXTURE_2D */, const_color_tex[slot]);
        real_glTexParameteri(0x0DE1, 0x2801 /* GL_TEXTURE_MIN_FILTER */, 0x2600 /* GL_NEAREST */);
        real_glTexParameteri(0x0DE1, 0x2800 /* GL_TEXTURE_MAG_FILTER */, 0x2600 /* GL_NEAREST */);
        real_glTexEnvi(0x2300 /* GL_TEXTURE_ENV */, 0x2200 /* GL_TEXTURE_ENV_MODE */, 0x2100 /* GL_MODULATE */);
        real_glEnable(0x0DE1);
    }
    else
    {
        real_glBindTexture(0x0DE1, const_color_tex[slot]);
    }

    real_glTexImage2D(0x0DE1, 0, 0x1908 /* GL_RGBA */, 1, 1, 0, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, rgba);

    real_glActiveTextureARB((unsigned int)previous_active);
}

void glCombinerParameterfvNV(unsigned int pname, const float *params)
{
    if (!params)
    {
        return;
    }

    resolve_real_gl();

    if (pname == 0x852A /* GL_CONSTANT_COLOR0_NV */)
    {
        apply_constant_color(0, GAELCO_CONST_COLOR0_UNIT, params);
    }
    else if (pname == 0x852B /* GL_CONSTANT_COLOR1_NV */)
    {
        apply_constant_color(1, GAELCO_CONST_COLOR1_UNIT, params);
    }
}

void glCombinerParameteriNV(unsigned int pname, int param)
{
    (void)pname;
    (void)param;
}

void glCombinerInputNV(unsigned int stage, unsigned int portion, unsigned int variable, unsigned int input, unsigned int mapping, unsigned int componentUsage)
{
    (void)stage;
    (void)portion;
    (void)variable;
    (void)input;
    (void)mapping;
    (void)componentUsage;
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
    (void)input;
    (void)mapping;
    (void)componentUsage;
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
    if (!fences)
    {
        return;
    }

    for (int i = 0; i < n; i++)
    {
        fences[i] = (unsigned int)(i + 1);
    }
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
    (void)length;
    (void)pointer;
}

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

/*
 * GL_EXT_vertex_weighting: GPU-side vertex blending for skinned meshes.
 * Dropped for the same reason as the combiners above - the characters in
 * testing still animate correctly, which means this path either isn't
 * exercised on this game's models or the skinning is otherwise done
 * CPU-side already.
 */
void glVertexWeightPointerEXT(int size, unsigned int type, int stride, const void *pointer)
{
    (void)size;
    (void)type;
    (void)stride;
    (void)pointer;
}