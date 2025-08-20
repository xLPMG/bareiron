#ifndef H_WORLDGEN
#define H_WORLDGEN

#include <stdint.h>

// For best performance, CHUNK_SIZE should be a power of 2
#define CHUNK_SIZE 8
// Terrain low point - should start a bit below sea level for rivers/lakes
#define TERRAIN_BASE_HEIGHT 60
// Center point of cave generation
#define CAVE_BASE_DEPTH 24
// Size of every major biome in multiples of CHUNK_SIZE
// For best performance, should also be a power of 2
#define BIOME_SIZE 64
#define BIOME_RADIUS (BIOME_SIZE / 2)

typedef struct {
  short x;
  short z;
  uint32_t hash;
  uint8_t biome;
} ChunkAnchor;

uint32_t getChunkHash (short x, short z);
uint8_t getChunkBiome (short x, short z);
int getHeightAt (int rx, int rz, int _x, int _z, uint32_t chunk_hash, uint8_t biome);
uint8_t getTerrainAt (int x, int y, int z, ChunkAnchor anchor);
uint8_t getBlockAt (int x, int y, int z);

extern uint8_t chunk_section[4096];
uint8_t buildChunkSection (int cx, int cy, int cz);

#endif
