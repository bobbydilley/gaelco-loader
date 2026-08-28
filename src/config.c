#define _GNU_SOURCE

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "config.h"
#include "utils.h"

static gaelco_config_t cfg = {
    .fullscreen = 0,
    .width = 640,
    .height = 480,
    .render_width = 640,
    .render_height = 480,
    .keep_aspect = 1,
    .integer_scale = 0,
};

static int cfg_loaded = 0;

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s))
    {
        s++;
    }

    char *end = s + strlen(s);

    while (end > s && isspace((unsigned char)end[-1]))
    {
        *--end = '\0';
    }

    return s;
}

static int parse_bool(const char *v, int fallback)
{
    if (!strcasecmp(v, "1") || !strcasecmp(v, "true") ||
        !strcasecmp(v, "yes") || !strcasecmp(v, "on"))
    {
        return 1;
    }

    if (!strcasecmp(v, "0") || !strcasecmp(v, "false") ||
        !strcasecmp(v, "no") || !strcasecmp(v, "off"))
    {
        return 0;
    }

    return fallback;
}

static void apply_kv(const char *key, const char *val)
{
    if (!strcasecmp(key, "fullscreen"))
    {
        cfg.fullscreen = parse_bool(val, cfg.fullscreen);
    }
    else if (!strcasecmp(key, "width"))
    {
        cfg.width = atoi(val);
    }
    else if (!strcasecmp(key, "height"))
    {
        cfg.height = atoi(val);
    }
    else if (!strcasecmp(key, "render_width") || !strcasecmp(key, "renderwidth"))
    {
        cfg.render_width = atoi(val);
    }
    else if (!strcasecmp(key, "render_height") || !strcasecmp(key, "renderheight"))
    {
        cfg.render_height = atoi(val);
    }
    else if (!strcasecmp(key, "keep_aspect") || !strcasecmp(key, "aspect") ||
             !strcasecmp(key, "keepaspect"))
    {
        cfg.keep_aspect = parse_bool(val, cfg.keep_aspect);
    }
    else if (!strcasecmp(key, "integer_scale") || !strcasecmp(key, "integerscale"))
    {
        cfg.integer_scale = parse_bool(val, cfg.integer_scale);
    }
    else
    {
        debug("[config] ignoring unknown key '%s'\n", key);
    }
}

static int parse_file(const char *path)
{
    FILE *f = fopen(path, "r");

    if (!f)
    {
        return 0;
    }

    char line[512];

    while (fgets(line, sizeof(line), f))
    {
        char *p = trim(line);

        if (*p == '\0' || *p == '#' || *p == ';' || *p == '[')
        {
            continue;
        }

        char *eq = strchr(p, '=');

        if (!eq)
        {
            continue;
        }

        *eq = '\0';

        char *key = trim(p);
        char *val = trim(eq + 1);

        if (*key)
        {
            apply_kv(key, val);
        }
    }

    fclose(f);
    debug("[config] loaded %s\n", path);
    return 1;
}

static void try_exe_dir(char *out, size_t out_size)
{
    char exe[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);

    if (n <= 0)
    {
        out[0] = '\0';
        return;
    }

    exe[n] = '\0';

    char *slash = strrchr(exe, '/');

    if (!slash)
    {
        out[0] = '\0';
        return;
    }

    *slash = '\0';
    snprintf(out, out_size, "%s/gaelco.ini", exe);
}

void config_load(void)
{
    if (cfg_loaded)
    {
        return;
    }

    cfg_loaded = 1;

    const char *env = getenv("GAELCO_INI");
    int found = 0;

    if (env && *env)
    {
        found = parse_file(env);
    }

    if (!found)
    {
        char path[PATH_MAX];
        try_exe_dir(path, sizeof(path));

        if (path[0])
        {
            found = parse_file(path);
        }
    }

    if (!found)
    {
        found = parse_file("gaelco.ini");
    }

    if (!found)
    {
        debug("[config] no gaelco.ini found, using defaults\n");
    }

    if (cfg.width < 64)
    {
        cfg.width = 64;
    }

    if (cfg.height < 64)
    {
        cfg.height = 64;
    }

    if (cfg.render_width < 64)
    {
        cfg.render_width = 640;
    }

    if (cfg.render_height < 64)
    {
        cfg.render_height = 480;
    }

    fprintf(stderr,
        "[config] fullscreen=%d output=%dx%d render=%dx%d keep_aspect=%d integer_scale=%d\n",
        cfg.fullscreen, cfg.width, cfg.height,
        cfg.render_width, cfg.render_height,
        cfg.keep_aspect, cfg.integer_scale);
}

const gaelco_config_t *config_get(void)
{
    return &cfg;
}
