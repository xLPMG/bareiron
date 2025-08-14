#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "globals.h"
#include "tools.h"
#include "registries.h"
#include "worldgen.h"

uint32_t getHash (const void *data, size_t len) {
  const uint8_t *bytes = data;
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < len; i ++) {
    hash ^= bytes[i];
    hash *= 16777619u;
  }
  return hash;
}

int sapling (short x, short y, short z) {

  uint8_t buf[10];
  memcpy(buf, &x, 2);
  memcpy(buf + 2, &y, 2);
  memcpy(buf + 4, &z, 2);
  memcpy(buf + 6, &world_seed, 4);

  if (getHash(buf, sizeof(buf)) % 20 == 0) return B_oak_sapling;
  return B_air;

}

uint32_t getChunkHash (short x, short z) {

  uint8_t buf[8];
  memcpy(buf, &x, 2);
  memcpy(buf + 2, &z, 2);
  memcpy(buf + 4, &world_seed, 4);

  return getHash(buf, sizeof(buf));

}

int getCornerHeight (uint32_t hash) {

  int height = 60 + hash % 8 + (hash >> 8) % 5;
  if (height < 64) height -= (hash >> 16) % 5;
  return height;

}

int interpolate (int a, int b, int c, int d, int x, int z) {
  int top    = a * (chunk_size - x) + b * x;
  int bottom = c * (chunk_size - x) + d * x;
  return (top * (chunk_size - z) + bottom * z) / (chunk_size * chunk_size);
}

int getHeightAt (int rx, int rz, int _x, int _z, uint32_t chunk_hash) {

  if (rx == 0 && rz == 0) {
    int height = getCornerHeight(chunk_hash);
    if (height > 67) return height - 1;
  }
  return interpolate(
    getCornerHeight(chunk_hash),
    getCornerHeight(getChunkHash(_x + 1, _z)),
    getCornerHeight(getChunkHash(_x, _z + 1)),
    getCornerHeight(getChunkHash(_x + 1, _z + 1)),
    rx, rz
  );

}

uint8_t getTerrainAt (int x, int y, int z) {

  if (y > 80) return B_air;

  int _x = x / chunk_size;
  int _z = z / chunk_size;
  int rx = x % chunk_size;
  int rz = z % chunk_size;

  if (rx < 0) { rx += chunk_size; _x -= 1; }
  if (rz < 0) { rz += chunk_size; _z -= 1; }

  uint32_t chunk_hash = getChunkHash(_x, _z);
  int height = getHeightAt(rx, rz, _x, _z, chunk_hash);

  if (y >= 64 && y >= height) {

    uint8_t tree_position = chunk_hash % (chunk_size * chunk_size);

    short tree_x = tree_position % chunk_size;
    if (tree_x < 3 || tree_x > chunk_size - 3) goto skip_tree;
    tree_x += _x * chunk_size;

    short tree_z = tree_position / chunk_size;
    if (tree_z < 3 || tree_z > chunk_size - 3) goto skip_tree;
    tree_z += _z * chunk_size;

    uint8_t tree_y = getHeightAt(
      tree_x < 0 ? tree_x % chunk_size + chunk_size : tree_x % chunk_size,
      tree_z < 0 ? tree_z % chunk_size + chunk_size : tree_z % chunk_size,
      _x, _z, chunk_hash
    ) + 1;
    if (tree_y < 64) goto skip_tree;

    if (x == tree_x && z == tree_z) {
      if (y == tree_y - 1) return B_dirt;
      if (y >= tree_y && y < tree_y + 6) return B_oak_log;
    }

    uint8_t dx = x > tree_x ? x - tree_x : tree_x - x;
    uint8_t dz = z > tree_z ? z - tree_z : tree_z - z;

    if (dx < 3 && dz < 3 && y > tree_y + 2 && y < tree_y + 5) {
      if (y == tree_y + 4 && dx == 2 && dz == 2 && (chunk_hash >> (x + z + y)) & 1) return B_air;
      return B_oak_leaves;
    }
    if (dx < 2 && dz < 2 && y >= tree_y + 5 && y <= tree_y + 6) {
      if (y == tree_y + 6 && dx == 1 && dz == 1 && (chunk_hash >> (x + z + y)) & 1) return B_air;
      return B_oak_leaves;
    }

    if (y == height) return B_grass_block;
    return B_air;

  }

skip_tree:
  if (y >= 63 && y == height) return B_grass_block;
  if (y < height - 3) return B_stone;
  if (y <= height) return B_dirt;
  if (y < 64) return B_water;

  return B_air;

}

uint8_t getBlockAt (int x, int y, int z) {

  uint8_t block_change = getBlockChange(x, y, z);
  if (block_change != 0xFF) return block_change;

  return getTerrainAt(x, y, z);

}

uint8_t chunk_section[4096];

void buildChunkSection (int cx, int cy, int cz) {

  // Generate 4096 blocks in one buffer to reduce overhead
  for (int j = 0; j < 4096; j += 8) {
    // These values don't change in the lower array,
    // since all of the operations are on multiples of 8
    int y = j / 256 + cy;
    int z = j / 16 % 16 + cz;
    // The client expects "big-endian longs", which in our
    // case means reversing the order in which we store/send
    // each 8 block sequence.
    for (int offset = 7; offset >= 0; offset--) {
      int k = j + offset;
      int x = k % 16 + cx;
      chunk_section[j + 7 - offset] = getTerrainAt(x, y, z);
    }
  }

  // Apply block changes on top of terrain
  // This does mean that we're generating some terrain only to replace it,
  // but it's better to apply changes in one run rather than in individual
  // runs per block, as this is more expensive than terrain generation.
  for (int i = 0; i < block_changes_count; i ++) {
    if (block_changes[i].block == 0xFF) continue;
    if ( // Check if block is within this chunk section
      block_changes[i].x >= cx && block_changes[i].x < cx + 16 &&
      block_changes[i].y >= cy && block_changes[i].y < cy + 16 &&
      block_changes[i].z >= cz && block_changes[i].z < cz + 16
    ) {
      int dx = block_changes[i].x - cx;
      int dy = block_changes[i].y - cy;
      int dz = block_changes[i].z - cz;
      // Same 8-block sequence reversal as before, this time 10x dirtier
      // because we're working with specific indexes.
      unsigned address = (unsigned)(dx + (dz << 4) + (dy << 8));
      unsigned index = (address & ~7u) | (7u - (address & 7u));
      chunk_section[index] = block_changes[i].block;
    }
  }

}
