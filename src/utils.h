#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

// CRCs for the games so it's easy to know what to load
#define TOKYO_COP 0x7a518e3b
#define CHAMPIONSHIP_TUNING_RACE 0x6f1e5179
#define RING_RIDERS 0x51ea930f

char *getGameName();
void detourFunction(size_t address, void *function);
void debug(const char *format, ...);
uint32_t getGameCRC32(void);

#endif
