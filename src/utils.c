#include "utils.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int debugEnabled = 1;

static uint32_t crc32Table[256];
static uint32_t gameCRC32 = 0;

char *getGameName()
{
    switch (getGameCRC32())
    {
    case TOKYO_COP:
        return "Tokyo Cop";
    case CHAMPIONSHIP_TUNING_RACE:
        return "Championship Tuning Race";
    case RING_RIDERS:
        return "Ring Riders";
    default:
        return "Unknown Game";
    }

    return "Unknown Game";
}

uint32_t getGameCRC32(void)
{
    if (gameCRC32 != 0)
        return gameCRC32;

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

        crc32Table[i] = crc;
    }

    char path[64];
    snprintf(path, sizeof(path),
             "/proc/%d/exe",
             (int)getpid());

    int fd = open(path, O_RDONLY);

    if (fd < 0)
        return 0;

    uint8_t buffer[65536];
    uint32_t crc = 0xFFFFFFFF;

    ssize_t n;

    while ((n = read(fd, buffer, sizeof(buffer))) > 0)
    {
        for (ssize_t i = 0; i < n; i++)
        {
            crc = crc32Table[(crc ^ buffer[i]) & 0xFF] ^
                  (crc >> 8);
        }
    }

    close(fd);

    if (n < 0)
        return 0;

    gameCRC32 = crc ^ 0xFFFFFFFF;

    return gameCRC32;
}

void debug(const char *format, ...)
{
    if (!debugEnabled)
        return;

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

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

    uintptr_t function_addr = (uintptr_t)function;
    intptr_t jumpAddress = (intptr_t)(function_addr - address) - 5;

    char cave[5] = {0xE9, 0x00, 0x00, 0x00, 0x00};
    cave[4] = (char)((jumpAddress >> (8 * 3)) & 0xFF);
    cave[3] = (char)((jumpAddress >> (8 * 2)) & 0xFF);
    cave[2] = (char)((jumpAddress >> (8 * 1)) & 0xFF);
    cave[1] = (char)(jumpAddress & 0xFF);

    memcpy((void *)address, cave, sizeof(cave));
}
