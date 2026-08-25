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

/*
 * Function pointers to the real libc functions.
 */
static int (*real_open)(const char *, int, ...) = NULL;
static int (*real_open64)(const char *, int, ...) = NULL;
static int (*real_openat)(int, const char *, int, ...) = NULL;
static int (*real_openat64)(int, const char *, int, ...) = NULL;


/*
 * Resolve all of the real functions.
 */
static void init_real_functions(void)
{
    if (!real_open)
        real_open = dlsym(RTLD_NEXT, "open");

    if (!real_open64)
        real_open64 = dlsym(RTLD_NEXT, "open64");

    if (!real_openat)
        real_openat = dlsym(RTLD_NEXT, "openat");

    if (!real_openat64)
        real_openat64 = dlsym(RTLD_NEXT, "openat64");
}


/*
 * Rewrite:
 *
 *   /home/joc2001/joc2001/salidas/foo
 *
 * to:
 *
 *   <exe directory>/../../../salidas/foo
 *
 * Returns pathname unchanged if it isn't one of the files
 * we want to redirect.
 */
static const char *redirect_path(const char *pathname)
{
    static char result[PATH_MAX];

    const char *prefix =
        "/home/joc2001/joc2001/salidas/";

    if (strncmp(pathname, prefix, strlen(prefix)) != 0)
        return pathname;

    char exe_path[PATH_MAX];

    ssize_t len = readlink(
        "/proc/self/exe",
        exe_path,
        sizeof(exe_path) - 1
    );

    if (len < 0)
        return pathname;

    exe_path[len] = '\0';


    char *slash = strrchr(exe_path, '/');

    if (!slash)
        return pathname;

    *slash = '\0';

    const char *filename = pathname + strlen(prefix);

    snprintf(
        result,
        sizeof(result),
        "%s/../../../salidas/%s",
        exe_path,
        filename
    );

    return result;
}


/*
 * open()
 */
int open(const char *pathname, int flags, ...)
{
    init_real_functions();

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
    init_real_functions();

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
    init_real_functions();

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
    init_real_functions();

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