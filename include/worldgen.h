#ifndef H_WORLDGEN
#define H_WORLDGEN

#include <stdint.h>

// For best performance, CHUNK_SIZE should be a power of 2
#define CHUNK_SIZE 8
// Terrain low point - should start a bit below sea level for rivers/lakes
#define TERRAIN_BASE_HEIGHT 60
// Center point of cave generation
#define CAVE_BASE_DEPTH 24

typedef struct {
  short x;
  short z;
  uint32_t hash;
} ChunkAnchor;

uint32_t getChunkHash (short x, short z);
int getHeightAt (int rx, int rz, int _x, int _z, uint32_t chunk_hash);
uint8_t getTerrainAt (int x, int y, int z, ChunkAnchor anchor);
uint8_t getBlockAt (int x, int y, int z);

extern uint8_t chunk_section[4096];
void buildChunkSection (int x, int y, int z);

#endif
