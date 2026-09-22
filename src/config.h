#ifndef CONFIG_H
#define CONFIG_H

/*
 * Runtime display configuration, read once from "gaelco.ini".
 *
 * The Gaelco games always render internally at a fixed resolution
 * (640x480 for Tokyo Cop). This lets the operator pick the real
 * window / fullscreen resolution; graphics.c scales that fixed render
 * up to the chosen output size, letterboxing to keep the 4:3 aspect
 * unless keep_aspect is turned off.
 *
 * gaelco.ini is looked for next to the game executable first, then in
 * the current working directory. Example:
 *
 *     [display]
 *     fullscreen = 1
 *     width      = 1280
 *     height     = 960
 *     keep_aspect = 1
 *     integer_scale = 0
 */

typedef struct
{
    int fullscreen;    /* 0 = windowed, 1 = borderless fullscreen desktop */
    int width;         /* output window width  (windowed only)            */
    int height;        /* output window height (windowed only)            */
    int render_width;  /* internal resolution the game renders at         */
    int render_height;
    int keep_aspect;   /* 1 = letterbox to preserve aspect, 0 = stretch   */
    int integer_scale; /* 1 = snap the letterbox scale to whole integers  */
} gaelco_config_t;

/* Parse gaelco.ini. Safe to call repeatedly; only the first call works. */
void config_load(void);

/* Never NULL after the first config_load() (or even before - returns
 * the built-in defaults). */
const gaelco_config_t *config_get(void);

#endif
