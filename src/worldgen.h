#ifndef H_WORLDGEN
#define H_WORLDGEN

#include <stdint.h>

uint8_t getBlockAt (int x, int y, int z);
void writeChunkSection (int client_fd, int _x, int _z, int i);

#endif
