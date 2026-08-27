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

/*
 * The game links directly against several NVIDIA-only GL/GLX extensions
 * (GL_NV_register_combiners, GL_NV_fence, GL_NV_vertex_array_range,
 * GLX_NV_vertex_array_range, GL_EXT_vertex_weighting) that Mesa/Intel
 * drivers don't export. These are plain dynamic symbols the game calls
 * directly (not resolved via glXGetProcAddress), so on Mesa the process
 * either fails to start or crashes the first time one is actually called.
 * Exporting them from this LD_PRELOAD library satisfies the game's linker
 * needs on any driver. See graphics.c for what each one actually does.
 */
void glCombinerParameterfvNV(unsigned int pname, const float *params);
void glCombinerParameteriNV(unsigned int pname, int param);
void glCombinerInputNV(unsigned int stage, unsigned int portion, unsigned int variable, unsigned int input, unsigned int mapping, unsigned int componentUsage);
void glCombinerOutputNV(unsigned int stage, unsigned int portion, unsigned int abOutput, unsigned int cdOutput, unsigned int sumOutput, unsigned int scale, unsigned int bias, unsigned char abDotProduct, unsigned char cdDotProduct, unsigned char muxSum);
void glFinalCombinerInputNV(unsigned int variable, unsigned int input, unsigned int mapping, unsigned int componentUsage);

void glGenFencesNV(int n, unsigned int *fences);

void glVertexArrayRangeNV(int length, const void *pointer);
void *glXAllocateMemoryNV(int size, float readfreq, float writefreq, float priority);
void glXFreeMemoryNV(void *pointer);

void glVertexWeightPointerEXT(int size, unsigned int type, int stride, const void *pointer);

#endif