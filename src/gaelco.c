#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <program> [args...]\n", argv[0]);
        return 1;
    }

    char exe_path[PATH_MAX];
    char preload_path[PATH_MAX];

    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);

    if (len < 0)
    {
        perror("readlink /proc/self/exe");
        return 1;
    }

    exe_path[len] = '\0';

    char *slash = strrchr(exe_path, '/');

    if (!slash)
    {
        fprintf(stderr, "Unable to determine executable directory\n");
        return 1;
    }

    *slash = '\0';

    snprintf(preload_path, sizeof(preload_path), "%s/gaelco-preload.so", exe_path);

    const char *old_ld_library_path = getenv("LD_LIBRARY_PATH");

    char ld_library_path[4096];

    if (old_ld_library_path && old_ld_library_path[0] != '\0')
    {
        snprintf(ld_library_path, sizeof(ld_library_path), "%s:.", old_ld_library_path);
    }
    else
    {
        snprintf(ld_library_path, sizeof(ld_library_path), ".");
    }

    if (setenv("LD_LIBRARY_PATH", ld_library_path, 1) != 0)
    {
        perror("setenv LD_LIBRARY_PATH");
        return 1;
    }

    if (setenv("LD_PRELOAD", preload_path, 1) != 0)
    {
        perror("setenv LD_PRELOAD");
        return 1;
    }

    execvp(argv[1], &argv[1]);

    perror("execvp");
    return 1;
}