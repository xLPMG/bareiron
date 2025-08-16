#ifndef H_WORLDGEN
#define H_WORLDGEN

#include <stdint.h>

// For best performance, chunk_size should be a power of 2
#define chunk_size 8
// Terrain low point - should start a bit below sea level for rivers/lakes
#define terrain_base_height 60
// Center point of cave generation
#define cave_base_depth 24

uint32_t getChunkHash (short x, short z);
int getHeightAt (int rx, int rz, int _x, int _z, uint32_t chunk_hash);
uint8_t getBlockAt (int x, int y, int z);

extern uint8_t chunk_section[4096];
void buildChunkSection (int x, int y, int z);

#endif
