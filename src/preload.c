#define _GNU_SOURCE

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <GL/gl.h>
#include <GL/glx.h>

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>


/*
 * ============================================================================
 * errno compatibility
 * ============================================================================
 */

#ifdef errno
#undef errno
#endif

int errno = 0;

extern int _errno __attribute__((alias("errno")));


/*
 * ============================================================================
 * NVIDIA extension constants
 * ============================================================================
 *
 * Some modern Mesa headers don't expose the old NVIDIA extension constants.
 * Define the ones we use if they aren't already present.
 * ============================================================================
 */


/*
 * GL_NV_fence
 */

#ifndef GL_ALL_COMPLETED_NV
#define GL_ALL_COMPLETED_NV             0x84F2
#endif

#ifndef GL_FENCE_STATUS_NV
#define GL_FENCE_STATUS_NV              0x84F3
#endif

#ifndef GL_FENCE_CONDITION_NV
#define GL_FENCE_CONDITION_NV           0x84F4
#endif


/*
 * GL_NV_register_combiners
 */

#ifndef GL_REGISTER_COMBINERS_NV
#define GL_REGISTER_COMBINERS_NV        0x8522
#endif

#ifndef GL_VARIABLE_A_NV
#define GL_VARIABLE_A_NV                0x8523
#endif

#ifndef GL_VARIABLE_B_NV
#define GL_VARIABLE_B_NV                0x8524
#endif

#ifndef GL_VARIABLE_C_NV
#define GL_VARIABLE_C_NV                0x8525
#endif

#ifndef GL_VARIABLE_D_NV
#define GL_VARIABLE_D_NV                0x8526
#endif

#ifndef GL_VARIABLE_E_NV
#define GL_VARIABLE_E_NV                0x8527
#endif

#ifndef GL_VARIABLE_F_NV
#define GL_VARIABLE_F_NV                0x8528
#endif

#ifndef GL_VARIABLE_G_NV
#define GL_VARIABLE_G_NV                0x8529
#endif

#ifndef GL_CONSTANT_COLOR0_NV
#define GL_CONSTANT_COLOR0_NV           0x852A
#endif

#ifndef GL_CONSTANT_COLOR1_NV
#define GL_CONSTANT_COLOR1_NV           0x852B
#endif

#ifndef GL_PRIMARY_COLOR_NV
#define GL_PRIMARY_COLOR_NV             0x852C
#endif

#ifndef GL_SECONDARY_COLOR_NV
#define GL_SECONDARY_COLOR_NV           0x852D
#endif

#ifndef GL_SPARE0_NV
#define GL_SPARE0_NV                    0x852E
#endif

#ifndef GL_SPARE1_NV
#define GL_SPARE1_NV                    0x852F
#endif

#ifndef GL_DISCARD_NV
#define GL_DISCARD_NV                   0x8530
#endif

#ifndef GL_E_TIMES_F_NV
#define GL_E_TIMES_F_NV                 0x8531
#endif

#ifndef GL_SPARE0_PLUS_SECONDARY_COLOR_NV
#define GL_SPARE0_PLUS_SECONDARY_COLOR_NV 0x8532
#endif

#ifndef GL_UNSIGNED_IDENTITY_NV
#define GL_UNSIGNED_IDENTITY_NV         0x8536
#endif

#ifndef GL_UNSIGNED_INVERT_NV
#define GL_UNSIGNED_INVERT_NV           0x8537
#endif

#ifndef GL_EXPAND_NORMAL_NV
#define GL_EXPAND_NORMAL_NV             0x8538
#endif

#ifndef GL_EXPAND_NEGATE_NV
#define GL_EXPAND_NEGATE_NV             0x8539
#endif

#ifndef GL_HALF_BIAS_NORMAL_NV
#define GL_HALF_BIAS_NORMAL_NV          0x853A
#endif

#ifndef GL_HALF_BIAS_NEGATE_NV
#define GL_HALF_BIAS_NEGATE_NV          0x853B
#endif

#ifndef GL_SIGNED_IDENTITY_NV
#define GL_SIGNED_IDENTITY_NV           0x853C
#endif

#ifndef GL_SIGNED_NEGATE_NV
#define GL_SIGNED_NEGATE_NV             0x853D
#endif

#ifndef GL_SCALE_BY_TWO_NV
#define GL_SCALE_BY_TWO_NV              0x853E
#endif

#ifndef GL_SCALE_BY_FOUR_NV
#define GL_SCALE_BY_FOUR_NV             0x853F
#endif

#ifndef GL_SCALE_BY_ONE_HALF_NV
#define GL_SCALE_BY_ONE_HALF_NV         0x8540
#endif

#ifndef GL_BIAS_BY_NEGATIVE_ONE_HALF_NV
#define GL_BIAS_BY_NEGATIVE_ONE_HALF_NV 0x8541
#endif

#ifndef GL_COMBINER_INPUT_NV
#define GL_COMBINER_INPUT_NV             0x8542
#endif

#ifndef GL_COMBINER_MAPPING_NV
#define GL_COMBINER_MAPPING_NV           0x8543
#endif

#ifndef GL_COMBINER_COMPONENT_USAGE_NV
#define GL_COMBINER_COMPONENT_USAGE_NV   0x8544
#endif

#ifndef GL_COMBINER_AB_DOT_PRODUCT_NV
#define GL_COMBINER_AB_DOT_PRODUCT_NV    0x8545
#endif

#ifndef GL_COMBINER_CD_DOT_PRODUCT_NV
#define GL_COMBINER_CD_DOT_PRODUCT_NV    0x8546
#endif

#ifndef GL_COMBINER_MUX_SUM_NV
#define GL_COMBINER_MUX_SUM_NV           0x8547
#endif

#ifndef GL_COMBINER_SCALE_NV
#define GL_COMBINER_SCALE_NV             0x8548
#endif

#ifndef GL_COMBINER_BIAS_NV
#define GL_COMBINER_BIAS_NV              0x8549
#endif

#ifndef GL_COMBINER_AB_OUTPUT_NV
#define GL_COMBINER_AB_OUTPUT_NV         0x854A
#endif

#ifndef GL_COMBINER_CD_OUTPUT_NV
#define GL_COMBINER_CD_OUTPUT_NV         0x854B
#endif

#ifndef GL_COMBINER_SUM_OUTPUT_NV
#define GL_COMBINER_SUM_OUTPUT_NV        0x854C
#endif

#ifndef GL_MAX_GENERAL_COMBINERS_NV
#define GL_MAX_GENERAL_COMBINERS_NV      0x854D
#endif

#ifndef GL_NUM_GENERAL_COMBINERS_NV
#define GL_NUM_GENERAL_COMBINERS_NV      0x854E
#endif

#ifndef GL_COLOR_SUM_CLAMP_NV
#define GL_COLOR_SUM_CLAMP_NV            0x854F
#endif

#ifndef GL_COMBINER0_NV
#define GL_COMBINER0_NV                  0x8550
#endif

#ifndef GL_COMBINER1_NV
#define GL_COMBINER1_NV                  0x8551
#endif

#ifndef GL_COMBINER2_NV
#define GL_COMBINER2_NV                  0x8552
#endif

#ifndef GL_COMBINER3_NV
#define GL_COMBINER3_NV                  0x8553
#endif

#ifndef GL_COMBINER4_NV
#define GL_COMBINER4_NV                  0x8554
#endif

#ifndef GL_COMBINER5_NV
#define GL_COMBINER5_NV                  0x8555
#endif

#ifndef GL_COMBINER6_NV
#define GL_COMBINER6_NV                  0x8556
#endif

#ifndef GL_COMBINER7_NV
#define GL_COMBINER7_NV                  0x8557
#endif


/*
 * ============================================================================
 * SDL / X11 state
 * ============================================================================
 */

static SDL_Window *sdl_window = NULL;

static Display *sdl_display = NULL;
static Window sdl_x11_window = None;

static int sdl_initialized = 0;

static int creating_sdl = 0;

static int window_width = 640;
static int window_height = 480;


/*
 * ============================================================================
 * Real Xlib functions
 * ============================================================================
 */

typedef Display *(*real_XOpenDisplay_t)(
    const char *
);

typedef Window (*real_XCreateWindow_t)(
    Display *,
    Window,
    int,
    int,
    unsigned int,
    unsigned int,
    unsigned int,
    int,
    unsigned int,
    Visual *,
    unsigned long,
    XSetWindowAttributes *
);

typedef int (*real_XMapWindow_t)(
    Display *,
    Window
);

static real_XOpenDisplay_t real_XOpenDisplay_func = NULL;
static real_XCreateWindow_t real_XCreateWindow_func = NULL;
static real_XMapWindow_t real_XMapWindow_func = NULL;


/*
 * ============================================================================
 * Resolve real Xlib functions
 * ============================================================================
 */

static void resolve_xlib_functions(void)
{
    if (!real_XOpenDisplay_func) {

        real_XOpenDisplay_func =
            (real_XOpenDisplay_t)dlsym(
                RTLD_NEXT,
                "XOpenDisplay"
            );
    }

    if (!real_XCreateWindow_func) {

        real_XCreateWindow_func =
            (real_XCreateWindow_t)dlsym(
                RTLD_NEXT,
                "XCreateWindow"
            );
    }

    if (!real_XMapWindow_func) {

        real_XMapWindow_func =
            (real_XMapWindow_t)dlsym(
                RTLD_NEXT,
                "XMapWindow"
            );
    }
}


/*
 * ============================================================================
 * Create SDL window
 * ============================================================================
 */

static int init_sdl_window(
    int width,
    int height)
{
    SDL_SysWMinfo wm;


    if (sdl_window != NULL) {
        return 1;
    }


    if (creating_sdl) {
        return 0;
    }


    creating_sdl = 1;


    if (width > 0) {
        window_width = width;
    }

    if (height > 0) {
        window_height = height;
    }


    fprintf(
        stderr,
        "[preload] creating SDL window %dx%d\n",
        window_width,
        window_height
    );


    SDL_setenv(
        "SDL_VIDEODRIVER",
        "x11",
        1
    );


    if (!sdl_initialized) {

        if (SDL_Init(SDL_INIT_VIDEO) != 0) {

            fprintf(
                stderr,
                "[preload] SDL_Init failed: %s\n",
                SDL_GetError()
            );

            creating_sdl = 0;

            return 0;
        }

        sdl_initialized = 1;
    }


    sdl_window = SDL_CreateWindow(
        "Galeco Tokyo Cop",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        window_width,
        window_height,
        SDL_WINDOW_SHOWN |
        SDL_WINDOW_OPENGL
    );


    if (!sdl_window) {

        fprintf(
            stderr,
            "[preload] SDL_CreateWindow failed: %s\n",
            SDL_GetError()
        );

        creating_sdl = 0;

        return 0;
    }


    memset(
        &wm,
        0,
        sizeof(wm)
    );

    SDL_VERSION(&wm.version);


    if (!SDL_GetWindowWMInfo(
            sdl_window,
            &wm)) {

        fprintf(
            stderr,
            "[preload] SDL_GetWindowWMInfo failed: %s\n",
            SDL_GetError()
        );

        SDL_DestroyWindow(
            sdl_window
        );

        sdl_window = NULL;

        creating_sdl = 0;

        return 0;
    }


    if (wm.subsystem != SDL_SYSWM_X11) {

        fprintf(
            stderr,
            "[preload] SDL window is not using X11 "
            "(subsystem=%d)\n",
            wm.subsystem
        );

        SDL_DestroyWindow(
            sdl_window
        );

        sdl_window = NULL;

        creating_sdl = 0;

        return 0;
    }


    sdl_display =
        wm.info.x11.display;

    sdl_x11_window =
        wm.info.x11.window;


    fprintf(
        stderr,
        "[preload] SDL X11 display = %p\n",
        (void *)sdl_display
    );

    fprintf(
        stderr,
        "[preload] SDL X11 window  = 0x%lx\n",
        (unsigned long)sdl_x11_window
    );


    creating_sdl = 0;

    return 1;
}


/*
 * ============================================================================
 * XOpenDisplay
 * ============================================================================
 */

Display *XOpenDisplay(
    const char *display_name)
{
    resolve_xlib_functions();


    if (creating_sdl) {

        if (real_XOpenDisplay_func) {

            return real_XOpenDisplay_func(
                display_name
            );
        }

        fprintf(
            stderr,
            "[preload] real XOpenDisplay unavailable\n"
        );

        return NULL;
    }


    if (!sdl_window) {

        if (!init_sdl_window(
                window_width,
                window_height)) {

            fprintf(
                stderr,
                "[preload] unable to initialize SDL window\n"
            );

            return NULL;
        }
    }


    fprintf(
        stderr,
        "[preload] XOpenDisplay(\"%s\") -> SDL Display %p\n",
        display_name ?
            display_name :
            "(null)",
        (void *)sdl_display
    );


    return sdl_display;
}


/*
 * ============================================================================
 * XCreateWindow
 * ============================================================================
 */

Window XCreateWindow(
    Display *display,
    Window parent,
    int x,
    int y,
    unsigned int width,
    unsigned int height,
    unsigned int border_width,
    int depth,
    unsigned int class,
    Visual *visual,
    unsigned long valueMask,
    XSetWindowAttributes *attributes)
{
    resolve_xlib_functions();


    if (creating_sdl) {

        if (real_XCreateWindow_func) {

            return real_XCreateWindow_func(
                display,
                parent,
                x,
                y,
                width,
                height,
                border_width,
                depth,
                class,
                visual,
                valueMask,
                attributes
            );
        }

        fprintf(
            stderr,
            "[preload] real XCreateWindow unavailable\n"
        );

        return None;
    }


    if (!sdl_window) {

        if (width > 0) {
            window_width = (int)width;
        }

        if (height > 0) {
            window_height = (int)height;
        }


        if (!init_sdl_window(
                window_width,
                window_height)) {

            fprintf(
                stderr,
                "[preload] XCreateWindow: "
                "SDL window creation failed\n"
            );

            return None;
        }
    }


    fprintf(
        stderr,
        "[preload] XCreateWindow intercepted:\n"
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
        (unsigned long)sdl_x11_window
    );


    if (width > 0 &&
        height > 0) {

        if ((int)width != window_width ||
            (int)height != window_height) {

            window_width = (int)width;
            window_height = (int)height;

            SDL_SetWindowSize(
                sdl_window,
                window_width,
                window_height
            );
        }
    }


    return sdl_x11_window;
}


/*
 * ============================================================================
 * XMapWindow
 * ============================================================================
 */

int XMapWindow(
    Display *display,
    Window window)
{
    resolve_xlib_functions();


    if (sdl_window &&
        window == sdl_x11_window) {

        fprintf(
            stderr,
            "[preload] XMapWindow(0x%lx) "
            "-> SDL_ShowWindow()\n",
            (unsigned long)window
        );

        SDL_ShowWindow(
            sdl_window
        );

        return 0;
    }


    /*
     * Important:
     *
     * The previous version returned 0 for every X window. That can break
     * unrelated X11 windows created by Mesa/GLX or the game.
     *
     * Pass unrelated windows to real Xlib.
     */
    if (real_XMapWindow_func) {

        return real_XMapWindow_func(
            display,
            window
        );
    }


    return 0;
}


/*
 * ============================================================================
 * XIfEvent diagnostics
 * ============================================================================
 */

typedef Bool (*XIfEventPredicate)(
    Display *,
    XEvent *,
    XPointer
);

typedef int (*real_XIfEvent_t)(
    Display *,
    XEvent *,
    XIfEventPredicate,
    XPointer
);

typedef int (*real_XPending_t)(
    Display *
);

typedef int (*real_XPeekEvent_t)(
    Display *,
    XEvent *
);

static real_XIfEvent_t real_XIfEvent_func = NULL;
static real_XPending_t real_XPending_func = NULL;
static real_XPeekEvent_t real_XPeekEvent_func = NULL;


static __thread XIfEventPredicate current_predicate = NULL;
static __thread XPointer current_predicate_arg = NULL;


/*
 * ============================================================================
 * Resolve event functions
 * ============================================================================
 */

static void resolve_event_functions(void)
{
    if (!real_XIfEvent_func) {

        real_XIfEvent_func =
            (real_XIfEvent_t)dlsym(
                RTLD_NEXT,
                "XIfEvent"
            );
    }


    if (!real_XPending_func) {

        real_XPending_func =
            (real_XPending_t)dlsym(
                RTLD_NEXT,
                "XPending"
            );
    }


    if (!real_XPeekEvent_func) {

        real_XPeekEvent_func =
            (real_XPeekEvent_t)dlsym(
                RTLD_NEXT,
                "XPeekEvent"
            );
    }
}


/*
 * ============================================================================
 * X event names
 * ============================================================================
 */

static const char *x_event_name(
    int type)
{
    switch (type) {

        case KeyPress:
            return "KeyPress";

        case KeyRelease:
            return "KeyRelease";

        case ButtonPress:
            return "ButtonPress";

        case ButtonRelease:
            return "ButtonRelease";

        case MotionNotify:
            return "MotionNotify";

        case EnterNotify:
            return "EnterNotify";

        case LeaveNotify:
            return "LeaveNotify";

        case FocusIn:
            return "FocusIn";

        case FocusOut:
            return "FocusOut";

        case Expose:
            return "Expose";

        case GraphicsExpose:
            return "GraphicsExpose";

        case NoExpose:
            return "NoExpose";

        case VisibilityNotify:
            return "VisibilityNotify";

        case CreateNotify:
            return "CreateNotify";

        case DestroyNotify:
            return "DestroyNotify";

        case UnmapNotify:
            return "UnmapNotify";

        case MapNotify:
            return "MapNotify";

        case MapRequest:
            return "MapRequest";

        case ReparentNotify:
            return "ReparentNotify";

        case ConfigureNotify:
            return "ConfigureNotify";

        case ConfigureRequest:
            return "ConfigureRequest";

        case GravityNotify:
            return "GravityNotify";

        case ResizeRequest:
            return "ResizeRequest";

        case CirculateNotify:
            return "CirculateNotify";

        case CirculateRequest:
            return "CirculateRequest";

        case PropertyNotify:
            return "PropertyNotify";

        case SelectionClear:
            return "SelectionClear";

        case SelectionRequest:
            return "SelectionRequest";

        case SelectionNotify:
            return "SelectionNotify";

        case ColormapNotify:
            return "ColormapNotify";

        case ClientMessage:
            return "ClientMessage";

        case MappingNotify:
            return "MappingNotify";

        default:
            return "Unknown";
    }
}


/*
 * ============================================================================
 * X event logger
 * ============================================================================
 */

static void log_xevent(
    const char *prefix,
    XEvent *event,
    Bool predicate_result)
{
    if (!event) {
        return;
    }


    fprintf(
        stderr,
        "[preload] %s: type=%d (%s) predicate=%s",
        prefix,
        event->type,
        x_event_name(event->type),
        predicate_result ? "MATCH" : "no"
    );


    switch (event->type) {

        case MapNotify:

            fprintf(
                stderr,
                " window=0x%lx event=0x%lx",
                (unsigned long)event->xmap.window,
                (unsigned long)event->xmap.event
            );

            break;


        case UnmapNotify:

            fprintf(
                stderr,
                " window=0x%lx event=0x%lx",
                (unsigned long)event->xunmap.window,
                (unsigned long)event->xunmap.event
            );

            break;


        case ConfigureNotify:

            fprintf(
                stderr,
                " window=0x%lx event=0x%lx "
                "x=%d y=%d width=%d height=%d",
                (unsigned long)event->xconfigure.window,
                (unsigned long)event->xconfigure.event,
                event->xconfigure.x,
                event->xconfigure.y,
                event->xconfigure.width,
                event->xconfigure.height
            );

            break;


        case Expose:

            fprintf(
                stderr,
                " window=0x%lx x=%d y=%d width=%d height=%d count=%d",
                (unsigned long)event->xexpose.window,
                event->xexpose.x,
                event->xexpose.y,
                event->xexpose.width,
                event->xexpose.height,
                event->xexpose.count
            );

            break;


        case PropertyNotify:

            fprintf(
                stderr,
                " window=0x%lx atom=0x%lx state=%d",
                (unsigned long)event->xproperty.window,
                (unsigned long)event->xproperty.atom,
                event->xproperty.state
            );

            break;


        case ClientMessage:

            fprintf(
                stderr,
                " window=0x%lx message_type=0x%lx",
                (unsigned long)event->xclient.window,
                (unsigned long)event->xclient.message_type
            );

            break;


        case FocusIn:
        case FocusOut:

            fprintf(
                stderr,
                " window=0x%lx",
                (unsigned long)event->xfocus.window
            );

            break;


        case KeyPress:
        case KeyRelease:

            fprintf(
                stderr,
                " window=0x%lx keycode=%u",
                (unsigned long)event->xkey.window,
                event->xkey.keycode
            );

            break;


        case ButtonPress:
        case ButtonRelease:

            fprintf(
                stderr,
                " window=0x%lx button=%u",
                (unsigned long)event->xbutton.window,
                event->xbutton.button
            );

            break;


        default:
            break;
    }


    fprintf(
        stderr,
        "\n"
    );
}


/*
 * ============================================================================
 * XIfEvent predicate proxy
 * ============================================================================
 */

static Bool proxy_x_if_event_predicate(
    Display *display,
    XEvent *event,
    XPointer arg)
{
    XIfEventPredicate predicate =
        current_predicate;

    XPointer predicate_arg =
        current_predicate_arg;


    if (!predicate) {
        return False;
    }


    Bool result =
        predicate(
            display,
            event,
            predicate_arg
        );


    log_xevent(
        "XIfEvent examined",
        event,
        result
    );


    return result;
}


/*
 * ============================================================================
 * XIfEvent
 * ============================================================================
 */

int XIfEvent(
    Display *display,
    XEvent *event_return,
    XIfEventPredicate predicate,
    XPointer arg)
{
    resolve_event_functions();


    fprintf(
        stderr,
        "[preload] XIfEvent("
        "display=%p, "
        "event_return=%p, "
        "predicate=%p, "
        "arg=%p)\n",
        (void *)display,
        (void *)event_return,
        (void *)predicate,
        (void *)arg
    );


    /*
     * Original game's WaitForMapNotify() predicate.
     */
    if ((uintptr_t)predicate == 0x080cd388 &&
        sdl_window &&
        sdl_x11_window != 0 &&
        (Window)(uintptr_t)arg == sdl_x11_window) {

        fprintf(
            stderr,
            "[preload] XIfEvent: "
            "detected WaitForMapNotify()\n"
        );


        fprintf(
            stderr,
            "[preload] XIfEvent: "
            "synthesizing MapNotify for window 0x%lx\n",
            (unsigned long)sdl_x11_window
        );


        if (event_return) {

            memset(
                event_return,
                0,
                sizeof(XEvent)
            );


            event_return->type =
                MapNotify;


            event_return->xmap.display =
                display;

            event_return->xmap.event =
                sdl_x11_window;

            event_return->xmap.window =
                sdl_x11_window;

            event_return->xmap.override_redirect =
                False;
        }


        fprintf(
            stderr,
            "[preload] XIfEvent: "
            "returning synthetic MapNotify\n"
        );


        return 1;
    }


    if (!real_XIfEvent_func) {

        fprintf(
            stderr,
            "[preload] XIfEvent: "
            "real XIfEvent unavailable\n"
        );

        return 0;
    }


    if (real_XPending_func) {

        int pending =
            real_XPending_func(
                display
            );


        fprintf(
            stderr,
            "[preload] XIfEvent: "
            "%d event(s) currently pending\n",
            pending
        );


        if (pending > 0 &&
            real_XPeekEvent_func) {

            XEvent peeked;

            memset(
                &peeked,
                0,
                sizeof(peeked)
            );


            if (real_XPeekEvent_func(
                    display,
                    &peeked)) {

                log_xevent(
                    "XIfEvent next event",
                    &peeked,
                    False
                );
            }
        }
    }


    current_predicate =
        predicate;

    current_predicate_arg =
        arg;


    fprintf(
        stderr,
        "[preload] XIfEvent: "
        "entering real XIfEvent with predicate proxy\n"
    );


    int result =
        real_XIfEvent_func(
            display,
            event_return,
            proxy_x_if_event_predicate,
            arg
        );


    current_predicate =
        NULL;

    current_predicate_arg =
        NULL;


    if (event_return) {

        log_xevent(
            "XIfEvent result",
            event_return,
            True
        );
    }


    fprintf(
        stderr,
        "[preload] XIfEvent: returned %d\n",
        result
    );


    return result;
}


/*
 * ============================================================================
 * GL_NV_vertex_array_range compatibility
 * ============================================================================
 *
 * NVIDIA's vertex-array-range extension is primarily a memory-management/
 * performance facility. We don't need an NVIDIA-specific memory pool on
 * modern Mesa.
 * ============================================================================
 */

static const void *nv_vertex_array_pointer = NULL;
static GLsizei nv_vertex_array_length = 0;


void *glXAllocateMemoryNV(
    GLsizei size,
    GLfloat readFrequency,
    GLfloat writeFrequency,
    GLfloat priority)
{
    (void)readFrequency;
    (void)writeFrequency;
    (void)priority;


    if (size <= 0) {
        return NULL;
    }


    void *ptr = NULL;


    if (posix_memalign(
            &ptr,
            64,
            (size_t)size) != 0) {

        fprintf(
            stderr,
            "[preload] glXAllocateMemoryNV: "
            "posix_memalign failed\n"
        );

        return NULL;
    }


    memset(
        ptr,
        0,
        (size_t)size
    );


    fprintf(
        stderr,
        "[preload] glXAllocateMemoryNV(%d) -> %p\n",
        size,
        ptr
    );


    return ptr;
}


void glXFreeMemoryNV(
    void *pointer)
{
    fprintf(
        stderr,
        "[preload] glXFreeMemoryNV(%p)\n",
        pointer
    );


    free(pointer);
}


void glVertexArrayRangeNV(
    GLsizei length,
    const GLvoid *pointer)
{
    nv_vertex_array_length =
        length;

    nv_vertex_array_pointer =
        pointer;


    fprintf(
        stderr,
        "[preload] glVertexArrayRangeNV("
        "length=%d, pointer=%p)\n",
        length,
        pointer
    );


    /*
     * Nothing else is required.
     *
     * The game can continue using the ordinary CPU address.
     */
}


void glFlushVertexArrayRangeNV(
    void)
{
    __sync_synchronize();


    fprintf(
        stderr,
        "[preload] glFlushVertexArrayRangeNV()\n"
    );
}


/*
 * ============================================================================
 * GL_NV_fence compatibility
 * ============================================================================
 */

#define MAX_COMPAT_FENCES 4096

typedef struct
{
    int allocated;
    int signaled;
} CompatFence;


static CompatFence compat_fences[
    MAX_COMPAT_FENCES
];


static pthread_mutex_t compat_fence_mutex =
    PTHREAD_MUTEX_INITIALIZER;


static int compat_fence_valid(
    GLuint id)
{
    return
        id != 0 &&
        id < MAX_COMPAT_FENCES &&
        compat_fences[id].allocated;
}


void glGenFencesNV(
    GLsizei n,
    GLuint *fences)
{
    if (n <= 0 ||
        !fences) {

        return;
    }


    pthread_mutex_lock(
        &compat_fence_mutex
    );


    for (GLsizei i = 0;
         i < n;
         i++) {

        GLuint id = 0;


        for (GLuint j = 1;
             j < MAX_COMPAT_FENCES;
             j++) {

            if (!compat_fences[j].allocated) {

                compat_fences[j].allocated =
                    1;

                compat_fences[j].signaled =
                    0;

                id = j;

                break;
            }
        }


        fences[i] =
            id;
    }


    pthread_mutex_unlock(
        &compat_fence_mutex
    );


    fprintf(
        stderr,
        "[preload] glGenFencesNV(%d)\n",
        n
    );
}


void glDeleteFencesNV(
    GLsizei n,
    const GLuint *fences)
{
    if (n <= 0 ||
        !fences) {

        return;
    }


    pthread_mutex_lock(
        &compat_fence_mutex
    );


    for (GLsizei i = 0;
         i < n;
         i++) {

        GLuint id =
            fences[i];


        if (id < MAX_COMPAT_FENCES) {

            compat_fences[id].allocated =
                0;

            compat_fences[id].signaled =
                0;
        }
    }


    pthread_mutex_unlock(
        &compat_fence_mutex
    );
}


GLboolean glIsFenceNV(
    GLuint fence)
{
    GLboolean result;


    pthread_mutex_lock(
        &compat_fence_mutex
    );


    result =
        compat_fence_valid(fence)
            ? GL_TRUE
            : GL_FALSE;


    pthread_mutex_unlock(
        &compat_fence_mutex
    );


    return result;
}


void glSetFenceNV(
    GLuint fence,
    GLenum condition)
{
    (void)condition;


    pthread_mutex_lock(
        &compat_fence_mutex
    );


    if (compat_fence_valid(fence)) {

        compat_fences[fence].signaled =
            0;


        /*
         * Make the fence conservative.
         */
        glFlush();


        compat_fences[fence].signaled =
            1;
    }


    pthread_mutex_unlock(
        &compat_fence_mutex
    );
}


GLboolean glTestFenceNV(
    GLuint fence)
{
    GLboolean result =
        GL_FALSE;


    pthread_mutex_lock(
        &compat_fence_mutex
    );


    if (compat_fence_valid(fence)) {

        result =
            compat_fences[fence].signaled
                ? GL_TRUE
                : GL_FALSE;
    }


    pthread_mutex_unlock(
        &compat_fence_mutex
    );


    return result;
}


void glFinishFenceNV(
    GLuint fence)
{
    pthread_mutex_lock(
        &compat_fence_mutex
    );


    if (compat_fence_valid(fence)) {

        glFinish();

        compat_fences[fence].signaled =
            1;
    }


    pthread_mutex_unlock(
        &compat_fence_mutex
    );
}


void glGetFenceivNV(
    GLuint fence,
    GLenum pname,
    GLint *params)
{
    if (!params) {
        return;
    }


    *params = 0;


    pthread_mutex_lock(
        &compat_fence_mutex
    );


    if (compat_fence_valid(fence)) {

        switch (pname) {

            case GL_FENCE_STATUS_NV:

                *params =
                    compat_fences[fence].signaled
                        ? GL_TRUE
                        : GL_FALSE;

                break;


            case GL_FENCE_CONDITION_NV:

                *params =
                    GL_ALL_COMPLETED_NV;

                break;


            default:

                *params =
                    0;

                break;
        }
    }


    pthread_mutex_unlock(
        &compat_fence_mutex
    );
}


/*
 * ============================================================================
 * GL_NV_register_combiners compatibility state
 * ============================================================================
 *
 * IMPORTANT:
 *
 * This records the NVIDIA register-combiner state.
 *
 * It does NOT simply discard the calls.
 *
 * The next stage is to translate this state into GLSL. The Khronos extension
 * specification defines the combiner as an actual fragment operation, so
 * making these functions no-ops would result in incorrect colours even though
 * the game would continue running.
 * ============================================================================
 */

#define NV_MAX_GENERAL_COMBINERS 8


typedef struct
{
    GLenum input;
    GLenum mapping;
    GLenum componentUsage;

} NVCombinerInputState;


typedef struct
{
    NVCombinerInputState input[4];

    GLenum abOutput;
    GLenum cdOutput;
    GLenum sumOutput;

    GLenum scale;
    GLenum bias;

    GLboolean abDotProduct;
    GLboolean cdDotProduct;
    GLboolean muxSum;

} NVCombinerPortionState;


typedef struct
{
    NVCombinerPortionState rgb;
    NVCombinerPortionState alpha;

} NVCombinerStageState;


typedef struct
{
    NVCombinerInputState input[7];

} NVFinalCombinerState;


typedef struct
{
    GLboolean enabled;

    GLint numGeneralCombiners;

    GLboolean colorSumClamp;

    GLfloat constantColor0[4];
    GLfloat constantColor1[4];

    NVCombinerStageState stage[
        NV_MAX_GENERAL_COMBINERS
    ];

    NVFinalCombinerState final;

} NVRegisterCombinerState;


static NVRegisterCombinerState nv_rc;


static pthread_mutex_t nv_rc_mutex =
    PTHREAD_MUTEX_INITIALIZER;


/*
 * Convert:
 *
 *     GL_COMBINER0_NV ... GL_COMBINER7_NV
 *
 * to an array index.
 */
static int nv_rc_stage_index(
    GLenum stage)
{
    if (stage < GL_COMBINER0_NV ||
        stage > GL_COMBINER7_NV) {

        return -1;
    }


    int index =
        (int)(stage - GL_COMBINER0_NV);


    if (index < 0 ||
        index >= NV_MAX_GENERAL_COMBINERS) {

        return -1;
    }


    return index;
}


/*
 * Convert A/B/C/D variable to array index.
 */
static int nv_rc_variable_index(
    GLenum variable)
{
    switch (variable) {

        case GL_VARIABLE_A_NV:
            return 0;

        case GL_VARIABLE_B_NV:
            return 1;

        case GL_VARIABLE_C_NV:
            return 2;

        case GL_VARIABLE_D_NV:
            return 3;

        default:
            return -1;
    }
}


/*
 * Convert final A/B/C/D/E/F/G variable.
 */
static int nv_rc_final_variable_index(
    GLenum variable)
{
    switch (variable) {

        case GL_VARIABLE_A_NV:
            return 0;

        case GL_VARIABLE_B_NV:
            return 1;

        case GL_VARIABLE_C_NV:
            return 2;

        case GL_VARIABLE_D_NV:
            return 3;

        case GL_VARIABLE_E_NV:
            return 4;

        case GL_VARIABLE_F_NV:
            return 5;

        case GL_VARIABLE_G_NV:
            return 6;

        default:
            return -1;
    }
}


/*
 * ============================================================================
 * glCombinerParameterfvNV
 * ============================================================================
 */

void glCombinerParameterfvNV(
    GLenum pname,
    const GLfloat *params)
{
    if (!params) {
        return;
    }


    pthread_mutex_lock(
        &nv_rc_mutex
    );


    switch (pname) {

        case GL_CONSTANT_COLOR0_NV:

            memcpy(
                nv_rc.constantColor0,
                params,
                sizeof(GLfloat) * 4
            );

            break;


        case GL_CONSTANT_COLOR1_NV:

            memcpy(
                nv_rc.constantColor1,
                params,
                sizeof(GLfloat) * 4
            );

            break;


        default:

            fprintf(
                stderr,
                "[NVRC] glCombinerParameterfvNV "
                "unhandled pname=0x%x\n",
                pname
            );

            break;
    }


    pthread_mutex_unlock(
        &nv_rc_mutex
    );


    fprintf(
        stderr,
        "[NVRC] glCombinerParameterfvNV("
        "pname=0x%x, "
        "values=%f,%f,%f,%f)\n",
        pname,
        params[0],
        params[1],
        params[2],
        params[3]
    );
}


/*
 * ============================================================================
 * glCombinerParameteriNV
 * ============================================================================
 */

void glCombinerParameteriNV(
    GLenum pname,
    GLint param)
{
    pthread_mutex_lock(
        &nv_rc_mutex
    );


    switch (pname) {

        case GL_NUM_GENERAL_COMBINERS_NV:

            if (param < 0) {
                param = 0;
            }

            if (param > NV_MAX_GENERAL_COMBINERS) {
                param = NV_MAX_GENERAL_COMBINERS;
            }

            nv_rc.numGeneralCombiners =
                param;

            break;


        case GL_COLOR_SUM_CLAMP_NV:

            nv_rc.colorSumClamp =
                param ? GL_TRUE : GL_FALSE;

            break;


        default:

            fprintf(
                stderr,
                "[NVRC] glCombinerParameteriNV "
                "unhandled pname=0x%x "
                "param=%d\n",
                pname,
                param
            );

            break;
    }


    pthread_mutex_unlock(
        &nv_rc_mutex
    );


    fprintf(
        stderr,
        "[NVRC] glCombinerParameteriNV("
        "pname=0x%x, "
        "param=%d)\n",
        pname,
        param
    );
}


/*
 * ============================================================================
 * glCombinerParameterfNV
 * ============================================================================
 */

void glCombinerParameterfNV(
    GLenum pname,
    GLfloat param)
{
    fprintf(
        stderr,
        "[NVRC] glCombinerParameterfNV("
        "pname=0x%x, "
        "param=%f)\n",
        pname,
        param
    );


    if (pname == GL_CONSTANT_COLOR0_NV ||
        pname == GL_CONSTANT_COLOR1_NV) {

        GLfloat values[4] = {
            param,
            param,
            param,
            param
        };


        glCombinerParameterfvNV(
            pname,
            values
        );

        return;
    }


    if (pname == GL_NUM_GENERAL_COMBINERS_NV ||
        pname == GL_COLOR_SUM_CLAMP_NV) {

        glCombinerParameteriNV(
            pname,
            (GLint)param
        );
    }
}


/*
 * ============================================================================
 * glCombinerInputNV
 * ============================================================================
 */

void glCombinerInputNV(
    GLenum stage,
    GLenum portion,
    GLenum variable,
    GLenum input,
    GLenum mapping,
    GLenum componentUsage)
{
    int stage_index =
        nv_rc_stage_index(stage);


    int variable_index =
        nv_rc_variable_index(variable);


    fprintf(
        stderr,
        "[NVRC] glCombinerInputNV("
        "stage=0x%x, "
        "portion=0x%x, "
        "variable=0x%x, "
        "input=0x%x, "
        "mapping=0x%x, "
        "usage=0x%x)\n",
        stage,
        portion,
        variable,
        input,
        mapping,
        componentUsage
    );


    if (stage_index < 0 ||
        variable_index < 0) {

        fprintf(
            stderr,
            "[NVRC] WARNING: invalid "
            "stage/variable\n"
        );

        return;
    }


    pthread_mutex_lock(
        &nv_rc_mutex
    );


    NVCombinerPortionState *state;


    if (portion == GL_RGB) {

        state =
            &nv_rc.stage[stage_index].rgb;

    } else if (portion == GL_ALPHA) {

        state =
            &nv_rc.stage[stage_index].alpha;

    } else {

        pthread_mutex_unlock(
            &nv_rc_mutex
        );

        fprintf(
            stderr,
            "[NVRC] WARNING: unknown "
            "portion=0x%x\n",
            portion
        );

        return;
    }


    state->input[
        variable_index
    ].input =
        input;


    state->input[
        variable_index
    ].mapping =
        mapping;


    state->input[
        variable_index
    ].componentUsage =
        componentUsage;


    pthread_mutex_unlock(
        &nv_rc_mutex
    );
}


/*
 * ============================================================================
 * glCombinerOutputNV
 * ============================================================================
 */

void glCombinerOutputNV(
    GLenum stage,
    GLenum portion,
    GLenum abOutput,
    GLenum cdOutput,
    GLenum sumOutput,
    GLenum scale,
    GLenum bias,
    GLboolean abDotProduct,
    GLboolean cdDotProduct,
    GLboolean muxSum)
{
    int stage_index =
        nv_rc_stage_index(stage);


    fprintf(
        stderr,
        "[NVRC] glCombinerOutputNV("
        "stage=0x%x, "
        "portion=0x%x, "
        "AB=0x%x, "
        "CD=0x%x, "
        "SUM=0x%x, "
        "scale=0x%x, "
        "bias=0x%x, "
        "ABdot=%d, "
        "CDdot=%d, "
        "mux=%d)\n",
        stage,
        portion,
        abOutput,
        cdOutput,
        sumOutput,
        scale,
        bias,
        abDotProduct,
        cdDotProduct,
        muxSum
    );


    if (stage_index < 0) {

        fprintf(
            stderr,
            "[NVRC] WARNING: invalid stage\n"
        );

        return;
    }


    pthread_mutex_lock(
        &nv_rc_mutex
    );


    NVCombinerPortionState *state;


    if (portion == GL_RGB) {

        state =
            &nv_rc.stage[stage_index].rgb;

    } else if (portion == GL_ALPHA) {

        state =
            &nv_rc.stage[stage_index].alpha;

    } else {

        pthread_mutex_unlock(
            &nv_rc_mutex
        );

        fprintf(
            stderr,
            "[NVRC] WARNING: unknown "
            "portion=0x%x\n",
            portion
        );

        return;
    }


    state->abOutput =
        abOutput;

    state->cdOutput =
        cdOutput;

    state->sumOutput =
        sumOutput;

    state->scale =
        scale;

    state->bias =
        bias;

    state->abDotProduct =
        abDotProduct;

    state->cdDotProduct =
        cdDotProduct;

    state->muxSum =
        muxSum;


    pthread_mutex_unlock(
        &nv_rc_mutex
    );
}


/*
 * ============================================================================
 * glFinalCombinerInputNV
 * ============================================================================
 */

void glFinalCombinerInputNV(
    GLenum variable,
    GLenum input,
    GLenum mapping,
    GLenum componentUsage)
{
    int variable_index =
        nv_rc_final_variable_index(
            variable
        );


    fprintf(
        stderr,
        "[NVRC] glFinalCombinerInputNV("
        "variable=0x%x, "
        "input=0x%x, "
        "mapping=0x%x, "
        "usage=0x%x)\n",
        variable,
        input,
        mapping,
        componentUsage
    );


    if (variable_index < 0) {

        fprintf(
            stderr,
            "[NVRC] WARNING: invalid final "
            "combiner variable\n"
        );

        return;
    }


    pthread_mutex_lock(
        &nv_rc_mutex
    );


    nv_rc.final.input[
        variable_index
    ].input =
        input;


    nv_rc.final.input[
        variable_index
    ].mapping =
        mapping;


    nv_rc.final.input[
        variable_index
    ].componentUsage =
        componentUsage;


    pthread_mutex_unlock(
        &nv_rc_mutex
    );
}


/*
 * ============================================================================
 * Optional helpers for GL extension queries
 * ============================================================================
 *
 * These aren't part of the five unresolved symbols you listed, but they are
 * useful when the game asks whether register combiners are present.
 *
 * We deliberately DON'T globally claim that Mesa implements the extension.
 * The actual rendering emulation still has to be completed.
 * ============================================================================
 */


/*
 * ============================================================================
 * Diagnostic dump
 * ============================================================================
 */

static void nv_rc_dump_state(void)
{
    pthread_mutex_lock(
        &nv_rc_mutex
    );


    fprintf(
        stderr,
        "\n"
        "[NVRC] ==================================================\n"
        "[NVRC] REGISTER COMBINER STATE\n"
        "[NVRC] enabled              = %d\n"
        "[NVRC] numGeneralCombiners = %d\n"
        "[NVRC] colorSumClamp       = %d\n"
        "[NVRC] constant0           = "
        "%f %f %f %f\n",
        nv_rc.enabled,
        nv_rc.numGeneralCombiners,
        nv_rc.colorSumClamp,
        nv_rc.constantColor0[0],
        nv_rc.constantColor0[1],
        nv_rc.constantColor0[2],
        nv_rc.constantColor0[3]
    );


    fprintf(
        stderr,
        "[NVRC] constant1           = "
        "%f %f %f %f\n",
        nv_rc.constantColor1[0],
        nv_rc.constantColor1[1],
        nv_rc.constantColor1[2],
        nv_rc.constantColor1[3]
    );


    for (int i = 0;
         i < nv_rc.numGeneralCombiners &&
         i < NV_MAX_GENERAL_COMBINERS;
         i++) {

        NVCombinerStageState *stage =
            &nv_rc.stage[i];


        fprintf(
            stderr,
            "[NVRC] COMBINER%d\n",
            i
        );


        fprintf(
            stderr,
            "[NVRC]   RGB: "
            "AB=0x%x CD=0x%x SUM=0x%x "
            "scale=0x%x bias=0x%x "
            "ABdot=%d CDdot=%d mux=%d\n",
            stage->rgb.abOutput,
            stage->rgb.cdOutput,
            stage->rgb.sumOutput,
            stage->rgb.scale,
            stage->rgb.bias,
            stage->rgb.abDotProduct,
            stage->rgb.cdDotProduct,
            stage->rgb.muxSum
        );


        fprintf(
            stderr,
            "[NVRC]   ALPHA: "
            "AB=0x%x CD=0x%x SUM=0x%x "
            "scale=0x%x bias=0x%x "
            "ABdot=%d CDdot=%d mux=%d\n",
            stage->alpha.abOutput,
            stage->alpha.cdOutput,
            stage->alpha.sumOutput,
            stage->alpha.scale,
            stage->alpha.bias,
            stage->alpha.abDotProduct,
            stage->alpha.cdDotProduct,
            stage->alpha.muxSum
        );
    }


    pthread_mutex_unlock(
        &nv_rc_mutex
    );


    fprintf(
        stderr,
        "[NVRC] ==================================================\n\n"
    );
}


/*
 * ============================================================================
 * Optional initialization
 * ============================================================================
 */

__attribute__((constructor))
static void preload_init(void)
{
    memset(
        &nv_rc,
        0,
        sizeof(nv_rc)
    );


    /*
     * NV_register_combiners defaults.
     *
     * The specification's default number of active combiners is one.
     */
    nv_rc.numGeneralCombiners =
        1;


    nv_rc.colorSumClamp =
        GL_TRUE;


    fprintf(
        stderr,
        "[preload] NVIDIA compatibility loader initialized\n"
    );
}


/*
 * ============================================================================
 * Shutdown
 * ============================================================================
 */

__attribute__((destructor))
static void shutdown_sdl(void)
{
    fprintf(
        stderr,
        "[preload] shutting down\n"
    );


    nv_rc_dump_state();


    if (sdl_window) {

        SDL_DestroyWindow(
            sdl_window
        );

        sdl_window = NULL;
    }


    if (sdl_initialized) {

        SDL_Quit();

        sdl_initialized = 0;
    }
}