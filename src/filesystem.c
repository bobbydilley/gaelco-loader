#define _GNU_SOURCE

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "utils.h"

static const char *redirect_path(const char *pathname)
{
    static char result[PATH_MAX];

    // printf("redirect_path: %s\n", pathname);

    const char *prefix =
        "/home/joc2001/joc2001/salidas/";

    const char *prefix2 =
        "/home/joc2001/joc2004/salidas/";

    const char *prefix3 =
        "/home/joc2001/joc2003/salidas/";

    if (strncmp(pathname, prefix, strlen(prefix)) != 0 && strncmp(pathname, prefix2, strlen(prefix2)) != 0  && strncmp(pathname, prefix3, strlen(prefix3)) != 0)
        return pathname;

    char exe_path[PATH_MAX];

    ssize_t len = readlink(
        "/proc/self/exe",
        exe_path,
        sizeof(exe_path) - 1);

    if (len < 0)
        return pathname;

    exe_path[len] = '\0';

    char *slash = strrchr(exe_path, '/');

    if (!slash)
        return pathname;

    *slash = '\0';

    const char *filename = pathname + strlen(prefix);

    switch (getGameCRC32())
    {
    case TOKYO_COP:
        snprintf(
            result,
            sizeof(result),
            "%s/../../../salidas/%s",
            exe_path,
            filename);
        break;
    case CHAMPIONSHIP_TUNING_RACE:
        snprintf(
            result,
            sizeof(result),
            "%s/%s",
            exe_path,
            filename);
        break;
    case RING_RIDERS:
        snprintf(
            result,
            sizeof(result),
            "%s/../../../salidas/%s",
            exe_path,
            filename);
        break;
    default:
        return pathname;
    }

    return result;
}

/*
 * open()
 */
int open(const char *pathname, int flags, ...)
{
    static int (*real_open)(const char *, int, ...) = NULL;

    if (!real_open)
        real_open = dlsym(RTLD_NEXT, "open");

    const char *new_path = redirect_path(pathname);

    if (flags & O_CREAT)
    {
        va_list args;
        va_start(args, flags);

        mode_t mode = va_arg(args, mode_t);

        va_end(args);

        return real_open(new_path, flags, mode);
    }

    return real_open(new_path, flags);
}

/*
 * open64()
 */
int open64(const char *pathname, int flags, ...)
{
    static int (*real_open64)(const char *, int, ...) = NULL;

    if (!real_open64)
        real_open64 = dlsym(RTLD_NEXT, "open64");

    const char *new_path = redirect_path(pathname);

    if (flags & O_CREAT)
    {
        va_list args;
        va_start(args, flags);

        mode_t mode = va_arg(args, mode_t);

        va_end(args);

        return real_open64(new_path, flags, mode);
    }

    return real_open64(new_path, flags);
}

/*
 * openat()
 */
int openat(int dirfd, const char *pathname, int flags, ...)
{
    static int (*real_openat)(int, const char *, int, ...) = NULL;

    if (!real_openat)
        real_openat = dlsym(RTLD_NEXT, "openat");

    const char *new_path = redirect_path(pathname);

    if (flags & O_CREAT)
    {
        va_list args;
        va_start(args, flags);

        mode_t mode = va_arg(args, mode_t);

        va_end(args);

        return real_openat(dirfd, new_path, flags, mode);
    }

    return real_openat(dirfd, new_path, flags);
}

/*
 * openat64()
 */
int openat64(int dirfd, const char *pathname, int flags, ...)
{
    static int (*real_openat64)(int, const char *, int, ...) = NULL;

    if (!real_openat64)
        real_openat64 = dlsym(RTLD_NEXT, "openat64");

    const char *new_path = redirect_path(pathname);

    if (flags & O_CREAT)
    {
        va_list args;
        va_start(args, flags);

        mode_t mode = va_arg(args, mode_t);

        va_end(args);

        return real_openat64(dirfd, new_path, flags, mode);
    }

    return real_openat64(dirfd, new_path, flags);
}

FILE *fopen(const char *pathname, const char *mode)
{
    static FILE *(*real_fopen)(const char *, const char *) = NULL;

    if (!real_fopen)
        real_fopen = dlsym(RTLD_NEXT, "fopen");

    const char *new_path = redirect_path(pathname);

    return real_fopen(new_path, mode);
}

FILE *fopen64(const char *pathname, const char *mode)
{
    static FILE *(*real_fopen64)(const char *, const char *) = NULL;

    if (!real_fopen64)
        real_fopen64 = dlsym(RTLD_NEXT, "fopen64");

    const char *new_path = redirect_path(pathname);

    return real_fopen64(new_path, mode);
}
