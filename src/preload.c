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
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <X11/Xlib.h>
#include <X11/extensions/xf86vmode.h>

#include <sys/mman.h>

#include "filesystem.h"

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

// CRCs for the games so it's easy to know what to load
#define TOKYO_COP 0x100
#define CHAMPIONSHIP_TUNING_RACE 0x6f1e5179
#define RING_RIDERS 0x300

/* Set to 1 to enable debug output, 0 to disable it */
int debug_enabled = 1;

int debug_fprintf(FILE *stream, const char *format, ...)
{
    if (!debug_enabled)
        return 0;

    va_list args;
    va_start(args, format);

    int result = vfprintf(stream, format, args);

    va_end(args);

    return result;
}

static int hooked_XF86VidModeSetGammaRamp(
    void *display,
    int screen,
    int size,
    unsigned short *red,
    unsigned short *green,
    unsigned short *blue)
{
    return 1;
}

static int hooked_XF86VidModeGetGammaRamp(
    void *display,
    int screen,
    int size,
    unsigned short *red,
    unsigned short *green,
    unsigned short *blue)
{
    fprintf(stderr,
            "[preload] XF86VidModeGetGammaRamp intercepted: "
            "screen=%d size=%d\n",
            screen, size);

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

Bool XF86VidModeSetGammaRamp(
    Display *display,
    int screen,
    int size,
    unsigned short *red,
    unsigned short *green,
    unsigned short *blue)
{
    return True;
}

static uint32_t crc32_table[256];
static int crc32_initialised = 0;
static int game_crc32 = 0;

static void crc32_init(void)
{
    if (crc32_initialised)
        return;

    for (uint32_t i = 0; i < 256; i++)
    {
        uint32_t crc = i;

        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }

        crc32_table[i] = crc;
    }

    crc32_initialised = 1;
}

static uint32_t crc32_file(const char *filename)
{
    crc32_init();

    int fd = open(filename, O_RDONLY);

    if (fd < 0)
        return 0;

    uint8_t buffer[65536];
    uint32_t crc = 0xFFFFFFFF;

    ssize_t n;

    while ((n = read(fd, buffer, sizeof(buffer))) > 0)
    {
        for (ssize_t i = 0; i < n; i++)
        {
            crc = crc32_table[(crc ^ buffer[i]) & 0xFF] ^
                  (crc >> 8);
        }
    }

    close(fd);

    if (n < 0)
        return 0;

    return crc ^ 0xFFFFFFFF;
}

static uint32_t crc32_self(void)
{
    char path[64];

    snprintf(path, sizeof(path),
             "/proc/%d/exe",
             (int)getpid());

    return crc32_file(path);
}

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
 * Virtual test button
 * ============================================================================
 *
 * 0 = released
 * 1 = held
 *
 * Pressing T sets this to 1.
 * Releasing T sets this back to 0.
 *
 * replacement_readTest() converts this held state into a single
 * "new press" event, matching the original game's readTest() behaviour.
 */

static volatile int virtual_test_button = 0;

static volatile int input_thread_running = 0;
static pthread_t input_thread;

/*
 * ============================================================================
 * Game test-button detour
 * ============================================================================
 */

void detourFunction(size_t address, void *function)
{
    int pagesize = sysconf(_SC_PAGE_SIZE);

    void *toModify = (void *)(address - (address % pagesize));

    int prot = mprotect(toModify, pagesize, PROT_EXEC | PROT_WRITE);
    if (prot != 0)
    {
        printf("Error: Cannot detour memory region to change variable (%d)\n", prot);
        return;
    }

    uint32_t jumpAddress = (function - (void *)address) - 5;

    // Build the assembly to make the function jump
    char cave[5] = {0xE9, 0x00, 0x00, 0x00, 0x00};
    cave[4] = (jumpAddress >> (8 * 3)) & 0xFF;
    cave[3] = (jumpAddress >> (8 * 2)) & 0xFF;
    cave[2] = (jumpAddress >> (8 * 1)) & 0xFF;
    cave[1] = (jumpAddress) & 0xFF;

    memcpy((void *)address, cave, 5);
}

/*
 * ============================================================================
 * Replacement for readTest()
 * ============================================================================
 *
 * Original:
 *
 *     int readTest__14G3Dintf_mandos(G3Dintf_mandos *this)
 *
 * The original function returns 1 only when the test button transitions
 * from released to pressed.
 */

static int replacement_readTest(
    void *self)
{
    (void)self;

    static int previous = 0;

    int current =
        virtual_test_button != 0;

    int pressed =
        current && !previous;

    previous =
        current;

    fprintf(
        stderr,
        "[preload] TEST button %d\n", virtual_test_button);

    return pressed;
}

/*
 * ============================================================================
 * Install game controller detour
 * ============================================================================
 */

static void install_controller_detours(void)
{
   
    switch (game_crc32)
    {
    case TOKYO_COP:
        detourFunction(0x080e6c14, (void *)replacement_readTest);
        break;
    case CHAMPIONSHIP_TUNING_RACE:
        detourFunction(0x081a814c, (void *)hooked_XF86VidModeGetGammaRamp);
        detourFunction(0x081a804c, (void *)hooked_XF86VidModeSetGammaRamp);
        break;
    case RING_RIDERS:

        break;
    default:
        break;
    }
}

/*
 * ============================================================================
 * Real Xlib functions
 * ============================================================================
 */

typedef Display *(*real_XOpenDisplay_t)(
    const char *);

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
    XSetWindowAttributes *);

typedef int (*real_XMapWindow_t)(
    Display *,
    Window);

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
    if (!real_XOpenDisplay_func)
    {
        real_XOpenDisplay_func =
            (real_XOpenDisplay_t)dlsym(
                RTLD_NEXT,
                "XOpenDisplay");
    }

    if (!real_XCreateWindow_func)
    {
        real_XCreateWindow_func =
            (real_XCreateWindow_t)dlsym(
                RTLD_NEXT,
                "XCreateWindow");
    }

    if (!real_XMapWindow_func)
    {
        real_XMapWindow_func =
            (real_XMapWindow_t)dlsym(
                RTLD_NEXT,
                "XMapWindow");
    }
}

/*
 * ============================================================================
 * Create SDL window
 * ============================================================================
 */
static void start_input_thread(void);

static int init_sdl_window(
    int width,
    int height)
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

    debug_fprintf(
        stderr,
        "[preload] creating SDL window %dx%d\n",
        window_width,
        window_height);

    SDL_setenv(
        "SDL_VIDEODRIVER",
        "x11",
        1);

    if (!sdl_initialized)
    {

        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {

            debug_fprintf(
                stderr,
                "[preload] SDL_Init failed: %s\n",
                SDL_GetError());

            creating_sdl = 0;

            return 0;
        }

        sdl_initialized = 1;
    }

    sdl_window = SDL_CreateWindow(
        "Gaelco Loader",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        window_width,
        window_height,
        SDL_WINDOW_SHOWN |
            SDL_WINDOW_OPENGL);

    if (!sdl_window)
    {

        debug_fprintf(
            stderr,
            "[preload] SDL_CreateWindow failed: %s\n",
            SDL_GetError());

        creating_sdl = 0;

        return 0;
    }

    memset(
        &wm,
        0,
        sizeof(wm));

    SDL_VERSION(&wm.version);

    if (!SDL_GetWindowWMInfo(
            sdl_window,
            &wm))
    {

        debug_fprintf(
            stderr,
            "[preload] SDL_GetWindowWMInfo failed: %s\n",
            SDL_GetError());

        SDL_DestroyWindow(
            sdl_window);

        sdl_window = NULL;

        creating_sdl = 0;

        return 0;
    }

    if (wm.subsystem != SDL_SYSWM_X11)
    {

        debug_fprintf(
            stderr,
            "[preload] SDL window is not using X11 "
            "(subsystem=%d)\n",
            wm.subsystem);

        SDL_DestroyWindow(
            sdl_window);

        sdl_window = NULL;

        creating_sdl = 0;

        return 0;
    }

    sdl_display =
        wm.info.x11.display;

    sdl_x11_window =
        wm.info.x11.window;

    debug_fprintf(
        stderr,
        "[preload] SDL X11 display = %p\n",
        (void *)sdl_display);

    debug_fprintf(
        stderr,
        "[preload] SDL X11 window  = 0x%lx\n",
        (unsigned long)sdl_x11_window);

    creating_sdl = 0;

    start_input_thread();

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

    if (creating_sdl)
    {

        if (real_XOpenDisplay_func)
        {
            return real_XOpenDisplay_func(
                display_name);
        }

        debug_fprintf(
            stderr,
            "[preload] real XOpenDisplay unavailable\n");

        return NULL;
    }

    if (!sdl_window)
    {

        if (!init_sdl_window(
                window_width,
                window_height))
        {

            debug_fprintf(
                stderr,
                "[preload] unable to initialize SDL window\n");

            return NULL;
        }
    }

    debug_fprintf(
        stderr,
        "[preload] XOpenDisplay(\"%s\") -> SDL Display %p\n",
        display_name ? display_name : "(null)",
        (void *)sdl_display);

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

    if (creating_sdl)
    {

        if (real_XCreateWindow_func)
        {
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
                attributes);
        }

        debug_fprintf(
            stderr,
            "[preload] real XCreateWindow unavailable\n");

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

        if (!init_sdl_window(
                window_width,
                window_height))
        {

            debug_fprintf(
                stderr,
                "[preload] XCreateWindow: "
                "SDL window creation failed\n");

            return None;
        }
    }

    debug_fprintf(
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
        (unsigned long)sdl_x11_window);

    if (width > 0 &&
        height > 0)
    {

        if ((int)width != window_width ||
            (int)height != window_height)
        {

            window_width =
                (int)width;

            window_height =
                (int)height;

            SDL_SetWindowSize(
                sdl_window,
                window_width,
                window_height);
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
        window == sdl_x11_window)
    {

        debug_fprintf(
            stderr,
            "[preload] XMapWindow(0x%lx) "
            "-> SDL_ShowWindow()\n",
            (unsigned long)window);

        SDL_ShowWindow(
            sdl_window);

        return 0;
    }

    if (real_XMapWindow_func)
    {

        return real_XMapWindow_func(
            display,
            window);
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
    XPointer);

typedef int (*real_XIfEvent_t)(
    Display *,
    XEvent *,
    XIfEventPredicate,
    XPointer);

typedef int (*real_XPending_t)(
    Display *);

typedef int (*real_XPeekEvent_t)(
    Display *,
    XEvent *);

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
    if (!real_XIfEvent_func)
    {
        real_XIfEvent_func =
            (real_XIfEvent_t)dlsym(
                RTLD_NEXT,
                "XIfEvent");
    }

    if (!real_XPending_func)
    {
        real_XPending_func =
            (real_XPending_t)dlsym(
                RTLD_NEXT,
                "XPending");
    }

    if (!real_XPeekEvent_func)
    {
        real_XPeekEvent_func =
            (real_XPeekEvent_t)dlsym(
                RTLD_NEXT,
                "XPeekEvent");
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
    switch (type)
    {

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
    if (!event)
    {
        return;
    }

    debug_fprintf(
        stderr,
        "[preload] %s: type=%d (%s) predicate=%s",
        prefix,
        event->type,
        x_event_name(event->type),
        predicate_result ? "MATCH" : "no");

    switch (event->type)
    {

    case MapNotify:

        debug_fprintf(
            stderr,
            " window=0x%lx event=0x%lx",
            (unsigned long)event->xmap.window,
            (unsigned long)event->xmap.event);

        break;

    case UnmapNotify:

        debug_fprintf(
            stderr,
            " window=0x%lx event=0x%lx",
            (unsigned long)event->xunmap.window,
            (unsigned long)event->xunmap.event);

        break;

    case ConfigureNotify:

        debug_fprintf(
            stderr,
            " window=0x%lx event=0x%lx "
            "x=%d y=%d width=%d height=%d",
            (unsigned long)event->xconfigure.window,
            (unsigned long)event->xconfigure.event,
            event->xconfigure.x,
            event->xconfigure.y,
            event->xconfigure.width,
            event->xconfigure.height);

        break;

    case Expose:

        debug_fprintf(
            stderr,
            " window=0x%lx x=%d y=%d width=%d height=%d count=%d",
            (unsigned long)event->xexpose.window,
            event->xexpose.x,
            event->xexpose.y,
            event->xexpose.width,
            event->xexpose.height,
            event->xexpose.count);

        break;

    case PropertyNotify:

        debug_fprintf(
            stderr,
            " window=0x%lx atom=0x%lx state=%d",
            (unsigned long)event->xproperty.window,
            (unsigned long)event->xproperty.atom,
            event->xproperty.state);

        break;

    case ClientMessage:

        debug_fprintf(
            stderr,
            " window=0x%lx message_type=0x%lx",
            (unsigned long)event->xclient.window,
            (unsigned long)event->xclient.message_type);

        break;

    case FocusIn:
    case FocusOut:

        debug_fprintf(
            stderr,
            " window=0x%lx",
            (unsigned long)event->xfocus.window);

        break;

    case KeyPress:
    case KeyRelease:

        debug_fprintf(
            stderr,
            " window=0x%lx keycode=%u",
            (unsigned long)event->xkey.window,
            event->xkey.keycode);

        break;

    case ButtonPress:
    case ButtonRelease:

        debug_fprintf(
            stderr,
            " window=0x%lx button=%u",
            (unsigned long)event->xbutton.window,
            event->xbutton.button);

        break;

    default:
        break;
    }

    debug_fprintf(
        stderr,
        "\n");
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

    if (!predicate)
    {
        return False;
    }

    Bool result =
        predicate(
            display,
            event,
            predicate_arg);

    log_xevent(
        "XIfEvent examined",
        event,
        result);

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

    debug_fprintf(
        stderr,
        "[preload] XIfEvent("
        "display=%p, "
        "event_return=%p, "
        "predicate=%p, "
        "arg=%p)\n",
        (void *)display,
        (void *)event_return,
        (void *)predicate,
        (void *)arg);

    if ((uintptr_t)predicate == 0x080cd388 &&
        sdl_window &&
        sdl_x11_window != 0 &&
        (Window)(uintptr_t)arg == sdl_x11_window)
    {

        debug_fprintf(
            stderr,
            "[preload] XIfEvent: "
            "detected WaitForMapNotify()\n");

        debug_fprintf(
            stderr,
            "[preload] XIfEvent: "
            "synthesizing MapNotify for window 0x%lx\n",
            (unsigned long)sdl_x11_window);

        if (event_return)
        {

            memset(
                event_return,
                0,
                sizeof(XEvent));

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

        debug_fprintf(
            stderr,
            "[preload] XIfEvent: "
            "returning synthetic MapNotify\n");

        return 1;
    }

    if (!real_XIfEvent_func)
    {

        debug_fprintf(
            stderr,
            "[preload] XIfEvent: "
            "real XIfEvent unavailable\n");

        return 0;
    }

    if (real_XPending_func)
    {

        int pending =
            real_XPending_func(
                display);

        debug_fprintf(
            stderr,
            "[preload] XIfEvent: "
            "%d event(s) currently pending\n",
            pending);

        if (pending > 0 &&
            real_XPeekEvent_func)
        {

            XEvent peeked;

            memset(
                &peeked,
                0,
                sizeof(peeked));

            if (real_XPeekEvent_func(
                    display,
                    &peeked))
            {

                log_xevent(
                    "XIfEvent next event",
                    &peeked,
                    False);
            }
        }
    }

    current_predicate =
        predicate;

    current_predicate_arg =
        arg;

    debug_fprintf(
        stderr,
        "[preload] XIfEvent: "
        "entering real XIfEvent with predicate proxy\n");

    int result =
        real_XIfEvent_func(
            display,
            event_return,
            proxy_x_if_event_predicate,
            arg);

    current_predicate =
        NULL;

    current_predicate_arg =
        NULL;

    if (event_return)
    {

        log_xevent(
            "XIfEvent result",
            event_return,
            True);
    }

    debug_fprintf(
        stderr,
        "[preload] XIfEvent: returned %d\n",
        result);

    return result;
}

/*
 * ============================================================================
 * SDL input monitor
 * ============================================================================
 *
 * T key:
 *
 *     KEYDOWN -> virtual_test_button = 1
 *     KEYUP   -> virtual_test_button = 0
 *
 * SDL_QUIT deliberately does NOT call exit().
 * The game is left running.
 */
static void *input_thread_main(void *unused)
{
    (void)unused;

    fprintf(
        stderr,
        "[preload] input monitor started\n");

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

            if (event.type == SDL_QUIT)
            {

                debug_fprintf(
                    stderr,
                    "[preload] SDL_QUIT received\n");

                input_thread_running = 0;

                /*
                 * The user clicked the window close button.
                 * Terminate the process just as the previous
                 * implementation intended.
                 */
                exit(0);
            }

            if (event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_t &&
                !event.key.repeat)
            {

                virtual_test_button = 1;

                fprintf(
                    stderr,
                    "[preload] T -> TEST button held\n");
            }

            if (event.type == SDL_KEYUP &&
                event.key.keysym.sym == SDLK_t)
            {

                virtual_test_button = 0;

                fprintf(
                    stderr,
                    "[preload] T -> TEST button released\n");
            }
        }

        usleep(5000);
    }

    debug_fprintf(
        stderr,
        "[preload] input monitor stopped\n");

    return NULL;
}

/*
 * ============================================================================
 * Start input monitor
 * ============================================================================
 */

static void start_input_thread(void)
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

    if (pthread_create(
            &input_thread,
            NULL,
            input_thread_main,
            NULL) != 0)
    {

        input_thread_running = 0;

        debug_fprintf(
            stderr,
            "[preload] failed to create input thread\n");

        return;
    }

    pthread_detach(
        input_thread);
}

static void maybe_start_input_thread(void)
{
    if (sdl_window &&
        sdl_initialized)
    {

        start_input_thread();
    }
}

/*
 * ============================================================================
 * Initialization
 * ============================================================================
 */

__attribute__((constructor)) static void preload_init(void)
{

    printf("Gaelco Loader Installed\n");

    game_crc32 = crc32_self();

    fprintf(stderr,
            "[preload] PID=%d CRC32=%08x\n",
            (int)getpid(),
            game_crc32);

    switch (game_crc32)
    {
    case TOKYO_COP:
        fprintf(stderr, "[preload] Detected Tokyo Cop\n");
        break;
    case CHAMPIONSHIP_TUNING_RACE:
        fprintf(stderr, "[preload] Detected Championship Tuning Race\n");
        break;
    case RING_RIDERS:
        fprintf(stderr, "[preload] Detected Ring Riders\n");
        break;
    default:
        break;
    }

    /*
     * Install the game's test-button detour immediately.
     */

    install_controller_detours();

    /*
     * The input thread is started once the SDL window exists.
     */
}

/*
 * ============================================================================
 * Hook input-thread startup into window creation
 * ============================================================================
 *
 * We need to start the keyboard monitor after SDL_CreateWindow() has
 * successfully completed.
 *
 * This wrapper is called from the X11 interception paths below.
 */

static void ensure_input_thread(void)
{
    if (sdl_window &&
        sdl_initialized &&
        !input_thread_running)
    {

        start_input_thread();
    }
}

/*
 * ============================================================================
 * Shutdown
 * ============================================================================
 */

__attribute__((destructor)) static void shutdown_sdl(void)
{
    debug_fprintf(
        stderr,
        "[preload] shutting down\n");

    input_thread_running = 0;

    virtual_test_button = 0;

    if (sdl_window)
    {

        SDL_DestroyWindow(
            sdl_window);

        sdl_window = NULL;
    }

    if (sdl_initialized)
    {

        SDL_Quit();

        sdl_initialized = 0;
    }
}