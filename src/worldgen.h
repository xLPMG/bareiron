#ifndef H_WORLDGEN
#define H_WORLDGEN

#include <stdint.h>

#define chunk_size 8

uint32_t getChunkHash (short x, short z);
int getHeightAt (int rx, int rz, int _x, int _z, uint32_t chunk_hash);
uint8_t getBlockAt (int x, int y, int z);

#endif
