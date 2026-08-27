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
            fprintf(stderr, "[graphics][GL] has GL_EXT_texture_compression_s3tc=%s\n",
                (extensions && strstr((const char *)extensions, "GL_EXT_texture_compression_s3tc")) ? "yes" : "no");
        }
    }

    log_gl_errors("glXMakeCurrent");

    return result;
}

/*
 * Diagnostic for the blurry-text report, now that viewport/window size is
 * ruled out: log the game's own texture filtering choices and whether its
 * compressed-texture uploads (it imports glCompressedTexImage2DARB, so it
 * does use S3TC/DXT-style compression for at least some textures) succeed
 * on this driver. If Mesa doesn't like the compressed format the game is
 * uploading, or filtering isn't what the game asked for, that would
 * explain soft-looking text while other geometry looks fine.
 */
typedef void (*glCompressedTexImage2DARB_t)(unsigned int target, int level, unsigned int internalformat, int width, int height, int border, int imageSize, const void *data);
static glCompressedTexImage2DARB_t real_glCompressedTexImage2DARB = NULL;
static int texparam_logged = 0;
static int compressedtex_logged = 0;

void glTexParameteri(unsigned int target, unsigned int pname, int param)
{
    typedef void (*glTexParameteri_diag_t)(unsigned int, unsigned int, int);
    static glTexParameteri_diag_t real = NULL;

    if (!real)
    {
        real = (glTexParameteri_diag_t)dlsym(RTLD_NEXT, "glTexParameteri");
    }

    if ((pname == 0x2801 /* GL_TEXTURE_MIN_FILTER */ || pname == 0x2800 /* GL_TEXTURE_MAG_FILTER */) &&
        texparam_logged < 40)
    {
        texparam_logged++;
        fprintf(stderr, "[graphics][texture] glTexParameteri target=0x%04x pname=%s param=0x%04x\n",
            target, pname == 0x2801 ? "MIN_FILTER" : "MAG_FILTER", (unsigned int)param);
    }

    if (real)
    {
        real(target, pname, param);
    }

    log_gl_errors("glTexParameteri");
}

void glCompressedTexImage2DARB(unsigned int target, int level, unsigned int internalformat, int width, int height, int border, int imageSize, const void *data)
{
    if (!real_glCompressedTexImage2DARB)
    {
        real_glCompressedTexImage2DARB = (glCompressedTexImage2DARB_t)dlsym(RTLD_NEXT, "glCompressedTexImage2DARB");
    }

    if (compressedtex_logged < 40)
    {
        compressedtex_logged++;
        fprintf(stderr, "[graphics][texture] glCompressedTexImage2DARB target=0x%04x level=%d format=0x%04x %dx%d imageSize=%d\n",
            target, level, internalformat, width, height, imageSize);
    }

    if (real_glCompressedTexImage2DARB)
    {
        real_glCompressedTexImage2DARB(target, level, internalformat, width, height, border, imageSize, data);
    }

    log_gl_errors("glCompressedTexImage2DARB");
}

/*
 * Uncompressed texture uploads - likely path for font/UI textures, which
 * the compressed-texture log ruled out (that path's filtering/format/size
 * all look correct and unremarkable). Logging these should show whether
 * text specifically goes through a much smaller/lower-res texture that's
 * then scaled up (blurring it), or uses different filtering than the 3D
 * geometry.
 */
typedef void (*glTexImage2D_diag_t)(unsigned int target, int level, int internalformat, int width, int height, int border, unsigned int format, unsigned int type, const void *pixels);
static glTexImage2D_diag_t real_glTexImage2D_diag = NULL;
static int teximage_logged = 0;

void glTexImage2D(unsigned int target, int level, int internalformat, int width, int height, int border, unsigned int format, unsigned int type, const void *pixels)
{
    if (!real_glTexImage2D_diag)
    {
        real_glTexImage2D_diag = (glTexImage2D_diag_t)dlsym(RTLD_NEXT, "glTexImage2D");
    }

    if (teximage_logged < 60)
    {
        teximage_logged++;
        fprintf(stderr, "[graphics][texture] glTexImage2D target=0x%04x level=%d internalformat=0x%04x %dx%d format=0x%04x type=0x%04x\n",
            target, level, internalformat, width, height, format, type);
    }

    if (real_glTexImage2D_diag)
    {
        real_glTexImage2D_diag(target, level, internalformat, width, height, border, format, type, pixels);
    }

    log_gl_errors("glTexImage2D");
}

/*
 * GL_NV_register_combiners is a fixed-function multitexture/lighting
 * pipeline that predates shaders - NVIDIA-only, never adopted by Mesa.
 * Reimplementing its exact per-stage math is out of scope, so these are
 * plain no-ops. We tried two different approximations of
 * GL_CONSTANT_COLOR0/1_NV using an extra multiply texture unit - first
 * unconditionally (made everything darker), then scoped to only the
 * combiner programs that actually reference it (introduced a blotchy red
 * pattern on character faces) - both reverted. Character skin tone
 * renders correctly without any of this, confirming the base
 * texture*vertex-color modulation Mesa already does by default is enough
 * for 3D geometry - it's specifically 2D UI (e.g. "INSERT COIN") that
 * loses its tint, which suggests that geometry has no vertex color array
 * of its own and relies entirely on GL_CONSTANT_COLOR0_NV for color.
 *
 * Third attempt, much narrower: just forward GL_CONSTANT_COLOR0_NV into a
 * plain glColor4f() call - no texture units, no multitexture state at
 * all. Anything using a per-vertex color array (which the correctly-
 * rendering 3D characters clearly do) ignores glColor4f entirely, so this
 * shouldn't be able to reintroduce the texture-unit regressions; anything
 * that doesn't (2D UI, apparently) picks it up as intended.
 */
typedef void (*glColor4f_t)(float r, float g, float b, float a);
static glColor4f_t real_glColor4f = NULL;

void glCombinerParameterfvNV(unsigned int pname, const float *params)
{
    if (pname == 0x852A /* GL_CONSTANT_COLOR0_NV */ && params)
    {
        if (!real_glColor4f)
        {
            real_glColor4f = (glColor4f_t)dlsym(RTLD_NEXT, "glColor4f");
        }

        if (real_glColor4f)
        {
            real_glColor4f(params[0], params[1], params[2], params[3]);
        }
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
typedef void (*glGetIntegerv_t)(unsigned int pname, int *params);
typedef void (*glDisable_t)(unsigned int cap);

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
static glGetIntegerv_t real_glGetIntegerv = NULL;
static glDisable_t real_glDisable = NULL;

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
    real_glGetIntegerv = (glGetIntegerv_t)dlsym(RTLD_NEXT, "glGetIntegerv");
    real_glDisable = (glDisable_t)dlsym(RTLD_NEXT, "glDisable");
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

/*
 * glEnable/glDisable: forward everything except the two NV/EXT tokens
 * Mesa doesn't recognize, which we track ourselves instead.
 *
 * The persistent, every-single-draw-call GL_INVALID_ENUM we're chasing
 * means there's very likely a THIRD such cap we haven't identified and
 * filtered yet - logged here per distinct value (only once each) so it
 * shows up directly instead of needing another guess.
 */
#define GAELCO_MAX_SEEN_CAPS 64
static unsigned int seen_caps[GAELCO_MAX_SEEN_CAPS];
static int seen_caps_count = 0;

static void log_cap_if_new(const char *fn, unsigned int cap)
{
    for (int i = 0; i < seen_caps_count; i++)
    {
        if (seen_caps[i] == cap)
        {
            return;
        }
    }

    if (seen_caps_count < GAELCO_MAX_SEEN_CAPS)
    {
        seen_caps[seen_caps_count++] = cap;
    }

    fprintf(stderr, "[graphics][capstate] %s(0x%04x) - first time seeing this cap\n", fn, cap);
}

void glEnable(unsigned int cap)
{
    resolve_weighting_gl();
    log_cap_if_new("glEnable", cap);

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
        log_gl_errors("glEnable(unfiltered cap, forwarded to real GL)");
    }
}

void glDisable(unsigned int cap)
{
    resolve_weighting_gl();
    log_cap_if_new("glDisable", cap);

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
        log_gl_errors("glDisable(unfiltered cap, forwarded to real GL)");
    }
}

/*
 * Diagnostic for the blurry-text report: if the game's glViewport size
 * doesn't match the SDL window's actual drawable pixel size, the GPU
 * scales the rendered image to fit, which blurs sharp edges like font
 * glyphs far more visibly than it blurs 3D geometry/textures - a classic
 * cause of "text looks soft but everything else looks okay-ish".
 */
typedef void (*glViewport_t)(int x, int y, int width, int height);
static glViewport_t real_glViewport = NULL;
static int viewport_logged = 0;

void glViewport(int x, int y, int width, int height)
{
    if (!real_glViewport)
    {
        real_glViewport = (glViewport_t)dlsym(RTLD_NEXT, "glViewport");
    }

    if (!viewport_logged)
    {
        viewport_logged = 1;

        int drawable_w = 0;
        int drawable_h = 0;

        if (sdl_window)
        {
            SDL_GL_GetDrawableSize(sdl_window, &drawable_w, &drawable_h);
        }

        fprintf(stderr, "[graphics][viewport] glViewport(%d,%d,%d,%d) vs SDL window %dx%d, GL drawable %dx%d\n",
            x, y, width, height, window_width, window_height, drawable_w, drawable_h);
    }

    if (real_glViewport)
    {
        real_glViewport(x, y, width, height);
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

/*
 * GPU path: do the weight*M0 + (1-weight)*M1 blend in a vertex shader
 * instead of a per-vertex CPU loop. Blending the two 4x4 matrices first
 * and then transforming is mathematically identical to blending the two
 * already-transformed points (matrix multiplication distributes over
 * addition/scalar multiplication), so this is the same math as the CPU
 * path, just executed per-vertex on the GPU in parallel instead of in a
 * C loop - which also means the original glDrawElements call (indices,
 * vertex pointer, everything) is used completely unmodified, no scratch
 * buffer or draw-call substitution needed.
 *
 * Trade-off: a custom vertex shader replaces Mesa's fixed-function vertex
 * processing entirely, including per-vertex lighting, and doing that
 * properly (replicating glLight/glMaterial's equations) is a further
 * chunk of work: this version passes the vertex color straight through
 * unlit, which is simpler/lower-risk but will look flatter-shaded than
 * before on these specific meshes. If that turns out to matter visually,
 * lighting can be added as a follow-up.
 *
 * The mesh's own normal/color/texcoord arrays are left completely alone -
 * only position is affected - and 4.6 Mesa Compatibility Profile (per
 * the driver info logged earlier) supports GLSL 1.20 natively, so no new
 * library dependency is needed. If shader setup fails for any reason, we
 * fall back to the CPU path below rather than drawing incorrectly.
 */
#define GAELCO_WEIGHT_ATTRIB_LOCATION 7

typedef unsigned int (*glCreateShader_t)(unsigned int type);
typedef void (*glShaderSource_t)(unsigned int shader, int count, const char *const *string, const int *length);
typedef void (*glCompileShader_t)(unsigned int shader);
typedef void (*glGetShaderiv_t)(unsigned int shader, unsigned int pname, int *params);
typedef void (*glGetShaderInfoLog_t)(unsigned int shader, int bufSize, int *length, char *infoLog);
typedef unsigned int (*glCreateProgram_t)(void);
typedef void (*glAttachShader_t)(unsigned int program, unsigned int shader);
typedef void (*glBindAttribLocation_t)(unsigned int program, unsigned int index, const char *name);
typedef void (*glLinkProgram_t)(unsigned int program);
typedef void (*glGetProgramiv_t)(unsigned int program, unsigned int pname, int *params);
typedef void (*glGetProgramInfoLog_t)(unsigned int program, int bufSize, int *length, char *infoLog);
typedef void (*glUseProgram_t)(unsigned int program);
typedef int (*glGetUniformLocation_t)(unsigned int program, const char *name);
typedef void (*glUniformMatrix4fv_t)(int location, int count, unsigned char transpose, const float *value);
typedef void (*glVertexAttribPointer_t)(unsigned int index, int size, unsigned int type, unsigned char normalized, int stride, const void *pointer);
typedef void (*glEnableVertexAttribArray_t)(unsigned int index);
typedef void (*glDisableVertexAttribArray_t)(unsigned int index);
typedef void (*glDeleteShader_t)(unsigned int shader);

static glCreateShader_t real_glCreateShader = NULL;
static glShaderSource_t real_glShaderSource = NULL;
static glCompileShader_t real_glCompileShader = NULL;
static glGetShaderiv_t real_glGetShaderiv = NULL;
static glGetShaderInfoLog_t real_glGetShaderInfoLog = NULL;
static glCreateProgram_t real_glCreateProgram = NULL;
static glAttachShader_t real_glAttachShader = NULL;
static glBindAttribLocation_t real_glBindAttribLocation = NULL;
static glLinkProgram_t real_glLinkProgram = NULL;
static glGetProgramiv_t real_glGetProgramiv = NULL;
static glGetProgramInfoLog_t real_glGetProgramInfoLog = NULL;
static glUseProgram_t real_glUseProgram = NULL;
static glGetUniformLocation_t real_glGetUniformLocation = NULL;
static glUniformMatrix4fv_t real_glUniformMatrix4fv = NULL;
static glVertexAttribPointer_t real_glVertexAttribPointer = NULL;
static glEnableVertexAttribArray_t real_glEnableVertexAttribArray = NULL;
static glDisableVertexAttribArray_t real_glDisableVertexAttribArray = NULL;
static glDeleteShader_t real_glDeleteShader = NULL;

static unsigned int weight_shader_program = 0;
static int weight_uniform_modelview1 = -1;
static int weight_shader_attempted = 0;

static const char *weight_vertex_shader_src =
    "#version 120\n"
    "attribute float weight;\n"
    "uniform mat4 modelview1;\n"
    "void main()\n"
    "{\n"
    "    mat4 blended = weight * gl_ModelViewMatrix + (1.0 - weight) * modelview1;\n"
    "    gl_Position = gl_ProjectionMatrix * (blended * gl_Vertex);\n"
    "    gl_FrontColor = gl_Color;\n"
    "    gl_TexCoord[0] = gl_MultiTexCoord0;\n"
    "}\n";

static void resolve_shader_gl(void)
{
    if (real_glCreateShader)
    {
        return;
    }

    real_glCreateShader = (glCreateShader_t)dlsym(RTLD_NEXT, "glCreateShader");
    real_glShaderSource = (glShaderSource_t)dlsym(RTLD_NEXT, "glShaderSource");
    real_glCompileShader = (glCompileShader_t)dlsym(RTLD_NEXT, "glCompileShader");
    real_glGetShaderiv = (glGetShaderiv_t)dlsym(RTLD_NEXT, "glGetShaderiv");
    real_glGetShaderInfoLog = (glGetShaderInfoLog_t)dlsym(RTLD_NEXT, "glGetShaderInfoLog");
    real_glCreateProgram = (glCreateProgram_t)dlsym(RTLD_NEXT, "glCreateProgram");
    real_glAttachShader = (glAttachShader_t)dlsym(RTLD_NEXT, "glAttachShader");
    real_glBindAttribLocation = (glBindAttribLocation_t)dlsym(RTLD_NEXT, "glBindAttribLocation");
    real_glLinkProgram = (glLinkProgram_t)dlsym(RTLD_NEXT, "glLinkProgram");
    real_glGetProgramiv = (glGetProgramiv_t)dlsym(RTLD_NEXT, "glGetProgramiv");
    real_glGetProgramInfoLog = (glGetProgramInfoLog_t)dlsym(RTLD_NEXT, "glGetProgramInfoLog");
    real_glUseProgram = (glUseProgram_t)dlsym(RTLD_NEXT, "glUseProgram");
    real_glGetUniformLocation = (glGetUniformLocation_t)dlsym(RTLD_NEXT, "glGetUniformLocation");
    real_glUniformMatrix4fv = (glUniformMatrix4fv_t)dlsym(RTLD_NEXT, "glUniformMatrix4fv");
    real_glVertexAttribPointer = (glVertexAttribPointer_t)dlsym(RTLD_NEXT, "glVertexAttribPointer");
    real_glEnableVertexAttribArray = (glEnableVertexAttribArray_t)dlsym(RTLD_NEXT, "glEnableVertexAttribArray");
    real_glDisableVertexAttribArray = (glDisableVertexAttribArray_t)dlsym(RTLD_NEXT, "glDisableVertexAttribArray");
    real_glDeleteShader = (glDeleteShader_t)dlsym(RTLD_NEXT, "glDeleteShader");
}

static void ensure_weight_shader(void)
{
    if (weight_shader_attempted)
    {
        return;
    }

    weight_shader_attempted = 1;
    resolve_shader_gl();

    if (!real_glCreateShader || !real_glShaderSource || !real_glCompileShader ||
        !real_glGetShaderiv || !real_glGetShaderInfoLog || !real_glCreateProgram ||
        !real_glAttachShader || !real_glBindAttribLocation || !real_glLinkProgram ||
        !real_glGetProgramiv || !real_glGetProgramInfoLog || !real_glGetUniformLocation ||
        !real_glDeleteShader)
    {
        fprintf(stderr, "[graphics][weight] GPU shader path unavailable (missing GL 2.0 entry points), staying on CPU path\n");
        return;
    }

    unsigned int shader = real_glCreateShader(0x8B31 /* GL_VERTEX_SHADER */);
    real_glShaderSource(shader, 1, &weight_vertex_shader_src, NULL);
    real_glCompileShader(shader);

    int compiled = 0;
    real_glGetShaderiv(shader, 0x8B81 /* GL_COMPILE_STATUS */, &compiled);

    if (!compiled)
    {
        char log[1024];
        int len = 0;
        real_glGetShaderInfoLog(shader, sizeof(log), &len, log);
        fprintf(stderr, "[graphics][weight] vertex shader compile failed, staying on CPU path: %.*s\n", len, log);
        real_glDeleteShader(shader);
        return;
    }

    unsigned int program = real_glCreateProgram();
    real_glAttachShader(program, shader);
    real_glBindAttribLocation(program, GAELCO_WEIGHT_ATTRIB_LOCATION, "weight");
    real_glLinkProgram(program);

    int linked = 0;
    real_glGetProgramiv(program, 0x8B82 /* GL_LINK_STATUS */, &linked);

    if (!linked)
    {
        char log[1024];
        int len = 0;
        real_glGetProgramInfoLog(program, sizeof(log), &len, log);
        fprintf(stderr, "[graphics][weight] shader program link failed, staying on CPU path: %.*s\n", len, log);
        real_glDeleteShader(shader);
        return;
    }

    weight_uniform_modelview1 = real_glGetUniformLocation(program, "modelview1");
    weight_shader_program = program;

    fprintf(stderr, "[graphics][weight] GPU vertex-weighting shader ready (program=%u)\n", program);
}

void glDrawElements(unsigned int mode, int count, unsigned int type, const void *indices)
{
    /* glUseProgram has no enum parameter and can never itself raise
     * GL_INVALID_ENUM, so if that's where the error is landing it must be
     * latched from something earlier in the frame. Check right at entry,
     * before touching anything, to see whether it's already queued. */
    log_gl_errors("glDrawElements: entry (before any of our code runs)");

    resolve_weighting_gl();

    if (!real_glDrawElements)
    {
        return;
    }

    int weighted_draw = vertex_weighting_enabled && wp_pointer && vp_pointer &&
        vp_size == 3 && vp_type == 0x1406 /* GL_FLOAT */ && wp_type == 0x1406 /* GL_FLOAT */ &&
        count > 0 && indices;

    if (!weighted_draw)
    {
        real_glDrawElements(mode, count, type, indices);
        return;
    }

    ensure_weight_shader();

    if (weight_shader_program && real_glUseProgram && real_glUniformMatrix4fv &&
        real_glVertexAttribPointer && real_glEnableVertexAttribArray && real_glDisableVertexAttribArray)
    {
        real_glUseProgram(weight_shader_program);
        log_gl_errors("weight shader: glUseProgram(program)");

        if (weight_uniform_modelview1 >= 0)
        {
            real_glUniformMatrix4fv(weight_uniform_modelview1, 1, 0, modelview1_stack[modelview1_sp].m);
            log_gl_errors("weight shader: glUniformMatrix4fv");
        }

        real_glEnableVertexAttribArray(GAELCO_WEIGHT_ATTRIB_LOCATION);
        log_gl_errors("weight shader: glEnableVertexAttribArray");

        real_glVertexAttribPointer(GAELCO_WEIGHT_ATTRIB_LOCATION, 1, 0x1406 /* GL_FLOAT */, 0, wp_stride, wp_pointer);
        log_gl_errors("weight shader: glVertexAttribPointer");

        real_glDrawElements(mode, count, type, indices);
        log_gl_errors("weight shader: glDrawElements itself");

        real_glDisableVertexAttribArray(GAELCO_WEIGHT_ATTRIB_LOCATION);
        log_gl_errors("weight shader: glDisableVertexAttribArray");

        real_glUseProgram(0);
        log_gl_errors("weight shader: glUseProgram(0)");

        return;
    }

    /* GPU path unavailable - fall back to the CPU blend. */
    int can_blend = count <= GAELCO_MAX_BLEND_VERTS && real_glVertexPointer &&
        real_glGetFloatv && real_glGetIntegerv && real_glMatrixMode && real_glLoadIdentity &&
        real_glPushMatrix && real_glPopMatrix && real_glDrawArrays;

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