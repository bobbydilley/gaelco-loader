#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <program>\n", argv[0]);
        return 1;
    }

    const char *old_ld_library_path = getenv("LD_LIBRARY_PATH");

    char ld_library_path[4096];

    if (old_ld_library_path && old_ld_library_path[0] != '\0') {
        snprintf(
            ld_library_path,
            sizeof(ld_library_path),
            "%s:.",
            old_ld_library_path
        );
    } else {
        snprintf(
            ld_library_path,
            sizeof(ld_library_path),
            "."
        );
    }

    if (setenv("LD_LIBRARY_PATH", ld_library_path, 1) != 0) {
        perror("setenv LD_LIBRARY_PATH");
        return 1;
    }

    if (setenv("LD_PRELOAD", "./gaelco-preload.so", 1) != 0) {
        perror("setenv LD_PRELOAD");
        return 1;
    }

    /*
     * Execute the requested program.
     *
     * argv[1] is passed as argv[0], so:
     *
     *     ./galeco ./game
     *
     * becomes effectively:
     *
     *     ./game
     */
    execl(argv[1], argv[1], (char *)NULL);

    perror("execl");
    return 1;
}