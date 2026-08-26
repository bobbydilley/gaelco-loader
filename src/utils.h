#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

void detourFunction(size_t address, void *function);
void log(const char *format, ...);
uint32_t getGameCRC32(void);

#endif
