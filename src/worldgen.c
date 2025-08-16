#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "globals.h"
#include "tools.h"
#include "registries.h"
#include "worldgen.h"

uint32_t getChunkHash (short x, short z) {

  uint8_t buf[8];
  memcpy(buf, &x, 2);
  memcpy(buf + 2, &z, 2);
  memcpy(buf + 4, &world_seed, 4);

  return splitmix64(*((uint64_t *)buf));

}

int getCornerHeight (uint32_t hash) {

  // Use parts of the hash as random values for the height variation.
  // We stack multiple different numbers to stabilize the distribution
  // while allowing for occasional variances.
  int height = terrain_base_height + (
    (hash & 3) +
    (hash >> 4 & 3) +
    (hash >> 8 & 3) +
    (hash >> 12 & 3)
  );

  // If height dips below sea level, push it down further
  // This selectively makes bodies of water larger and deeper
  if (height < 64) height -= (hash >> 24) & 7;

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

typedef struct {
  short x;
  short z;
  uint32_t hash;
} ChunkAnchor;

uint8_t getTerrainAt (int x, int y, int z, ChunkAnchor anchor) {

  if (y > 80) return B_air;

  int rx = x % chunk_size;
  int rz = z % chunk_size;
  if (rx < 0) rx += chunk_size;
  if (rz < 0) rz += chunk_size;

  int height = getHeightAt(rx, rz, anchor.x, anchor.z, anchor.hash);

  if (y >= 64 && y >= height) {

    uint8_t tree_position = anchor.hash % (chunk_size * chunk_size);

    short tree_x = tree_position % chunk_size;
    if (tree_x < 3 || tree_x > chunk_size - 3) goto skip_tree;
    short tree_z = tree_position / chunk_size;
    if (tree_z < 3 || tree_z > chunk_size - 3) goto skip_tree;

    uint8_t tree_short = (anchor.hash >> (tree_x + tree_z)) & 1;

    tree_x += anchor.x * chunk_size;
    tree_z += anchor.z * chunk_size;

    uint8_t tree_y = getHeightAt(
      tree_x < 0 ? tree_x % chunk_size + chunk_size : tree_x % chunk_size,
      tree_z < 0 ? tree_z % chunk_size + chunk_size : tree_z % chunk_size,
      anchor.x, anchor.z, anchor.hash
    ) + 1;
    if (tree_y < 64) goto skip_tree;

    if (x == tree_x && z == tree_z) {
      if (y == tree_y - 1) return B_dirt;
      if (y >= tree_y && y < tree_y - tree_short + 6) return B_oak_log;
    }

    uint8_t dx = x > tree_x ? x - tree_x : tree_x - x;
    uint8_t dz = z > tree_z ? z - tree_z : tree_z - z;

    if (dx < 3 && dz < 3 && y > tree_y - tree_short + 2 && y < tree_y - tree_short + 5) {
      if (y == tree_y - tree_short + 4 && dx == 2 && dz == 2) return B_air;
      return B_oak_leaves;
    }
    if (dx < 2 && dz < 2 && y >= tree_y - tree_short + 5 && y <= tree_y - tree_short + 6) {
      if (y == tree_y - tree_short + 6 && dx == 1 && dz == 1) return B_air;
      return B_oak_leaves;
    }

    if (y == height) return B_grass_block;
    return B_air;

  }

skip_tree:
  // For surface-level terrain, generate grass blocks
  if (y == height && height >= 63) return B_grass_block;
  // Starting at 4 blocks below terrain level, generate minerals and caves
  if (y <= height - 4) {
    // Caves use the same shape as surface terrain, just mirrored
    int8_t gap = height - terrain_base_height;
    if (y < cave_base_depth + gap && y > cave_base_depth - gap) return B_air;

    // The chunk-relative X and Z coordinates are used in a bit shift on the hash
    // The sum of these is then used to get the Y coordinate of the ore in this column
    // This way, each column is guaranteed to have exactly one ore candidate
    uint8_t ore_x_component = (anchor.hash >> rx) & 31;
    uint8_t ore_z_component = (anchor.hash >> (rz + 16)) & 31;
    uint8_t ore_y = ore_x_component + ore_z_component;

    if (y == ore_y) {
      // Since the ore Y coordinate is effectely a random number in range [0;64],
      // we use it in another bit shift to get a pseudo-random number for the column
      uint8_t ore_probability = (anchor.hash >> ore_y) & 127;
      // Ore placement is determined by Y level and "probability"
      if (y < 15 && ore_probability < 15) return B_diamond_ore;
      if (y < 30) {
        if (ore_probability < 5) return B_gold_ore;
        if (ore_probability < 20) return B_redstone_ore;
      }
      if (y < 54 && ore_probability < 50) return B_iron_ore;
      if (ore_probability < 60) return B_coal_ore;
    }

    // For everything else, fall back to stone
    return B_stone;
  }
  // Under water and in the space between stone and grass, generate dirt
  if (y <= height) return B_dirt;
  // If all else failed, but we're below sea level, generate water
  if (y < 64) return B_water;

  // For everything else, fall back to air
  return B_air;

}

uint8_t getBlockAt (int x, int y, int z) {

  uint8_t block_change = getBlockChange(x, y, z);
  if (block_change != 0xFF) return block_change;

  ChunkAnchor anchor = {
    x / chunk_size,
    z / chunk_size
  };
  if (x % chunk_size < 0) anchor.x --;
  if (z % chunk_size < 0) anchor.z --;
  anchor.hash = getChunkHash(anchor.x, anchor.z);

  return getTerrainAt(x, y, z, anchor);

}

uint8_t chunk_section[4096];
ChunkAnchor chunk_anchors[256 / (chunk_size * chunk_size)];

void buildChunkSection (int cx, int cy, int cz) {

  // Precompute the hashes and anchors for each minichunk
  int anchor_index = 0;
  for (int i = cz; i < cz + 16; i += chunk_size) {
    for (int j = cx; j < cx + 16; j += chunk_size) {

      ChunkAnchor *anchor = chunk_anchors + anchor_index;

      anchor->x = j / chunk_size;
      anchor->z = i / chunk_size;
      anchor->hash = getChunkHash(anchor->x, anchor->z);

      anchor_index ++;
    }
  }

  // Generate 4096 blocks in one buffer to reduce overhead
  for (int j = 0; j < 4096; j += 8) {
    // These values don't change in the lower array,
    // since all of the operations are on multiples of 8
    int y = j / 256 + cy;
    int z = j / 16 % 16 + cz;
    // The client expects "big-endian longs", which in our
    // case means reversing the order in which we store/send
    // each 8 block sequence.
    anchor_index = (j % 16) / chunk_size + (j / 16 % 16) / chunk_size * 2;
    for (int offset = 7; offset >= 0; offset--) {
      int k = j + offset;
      int x = k % 16 + cx;
      chunk_section[j + 7 - offset] = getTerrainAt(x, y, z, chunk_anchors[anchor_index]);
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
