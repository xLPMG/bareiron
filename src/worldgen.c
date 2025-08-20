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

uint8_t getChunkBiome (short x, short z) {

  // Center biomes on 0;0
  x += BIOME_RADIUS;
  z += BIOME_RADIUS;

  // Calculate "biome coordinates" (one step above chunk coordinates)
  // The pattern repeats every 4 biomes, so the coordinate range is [0;3]
  uint8_t _x = mod_abs(x / BIOME_SIZE, 16) & 3;
  uint8_t _z = mod_abs(z / BIOME_SIZE, 16) & 3;
  // To prevent obvious mirroring, invert values on negative axes
  if (x < 0) _x = 3 - _x;
  if (z < 0) _z = 3 - _z;

  // Calculate distance from biome center
  int8_t dx = BIOME_RADIUS - mod_abs(x, BIOME_SIZE);
  int8_t dz = BIOME_RADIUS - mod_abs(z, BIOME_SIZE);
  // Each biome is a circular island, with beaches in-between
  // Determine whether the given chunk is within the island
  if (dx * dx + dz * dz > BIOME_RADIUS * BIOME_RADIUS) return W_beach;

  // Finally, the biome itself is plucked from the world seed.
  // The 32-bit seed is treated as a 4x4 biome matrix, with each biome
  // taking up 2 bytes. This is why there are only 4 biomes, excluding
  // beaches. Using the world seed as a repeating pattern avoids
  // having to generate and layer yet another hash.
  return (world_seed >> (_x + _z * 4)) & 3;

}

int getCornerHeight (uint32_t hash, uint8_t biome) {

  // When calculating the height, parts of the hash are used as random values.
  // Often, multiple values are stacked to stabilize the distribution while
  // allowing for occasionally larger variances.
  int height = TERRAIN_BASE_HEIGHT;

  switch (biome) {

    case W_mangrove_swamp: {
      height += (
        (hash % 3) +
        ((hash >> 4) % 3) +
        ((hash >> 8) % 3) +
        ((hash >> 12) % 3)
      );
      // If height dips below sea level, push it down further
      // This ends up creating many large ponds of water
      if (height < 64) height -= (hash >> 24) & 3;
      break;
    }

    case W_plains: {
      height += (
        (hash & 3) +
        (hash >> 4 & 3) +
        (hash >> 8 & 3) +
        (hash >> 12 & 3)
      );
      break;
    }

    case W_desert: {
      height += 4 + (
        (hash & 3) +
        (hash >> 4 & 3)
      );
      break;
    }

    case W_beach: {
      // Start slightly below sea level to ensure it's all water
      height = 62 - (
        (hash & 3) +
        (hash >> 4 & 3) +
        (hash >> 8 & 3)
      );
      break;
    }

    case W_snowy_plains: {
      // Use fewer components with larger ranges to create hills
      height += (
        (hash & 7) +
        (hash >> 4 & 7)
      );
      break;
    }

    default: break;
  }

  return height;

}

int interpolate (int a, int b, int c, int d, int x, int z) {
  int top    = a * (CHUNK_SIZE - x) + b * x;
  int bottom = c * (CHUNK_SIZE - x) + d * x;
  return (top * (CHUNK_SIZE - z) + bottom * z) / (CHUNK_SIZE * CHUNK_SIZE);
}

int getHeightAt (int rx, int rz, int _x, int _z, uint32_t chunk_hash, uint8_t biome) {

  if (rx == 0 && rz == 0) {
    int height = getCornerHeight(chunk_hash, biome);
    if (height > 67) return height - 1;
  }
  return interpolate(
    getCornerHeight(chunk_hash, biome),
    getCornerHeight(getChunkHash(_x + 1, _z), getChunkBiome(_x + 1, _z)),
    getCornerHeight(getChunkHash(_x, _z + 1), getChunkBiome(_x, _z + 1)),
    getCornerHeight(getChunkHash(_x + 1, _z + 1), getChunkBiome(_x + 1, _z + 1)),
    rx, rz
  );

}

uint8_t getTerrainAt (int x, int y, int z, ChunkAnchor anchor) {

  if (y > 80) return B_air;

  int rx = x % CHUNK_SIZE;
  int rz = z % CHUNK_SIZE;
  if (rx < 0) rx += CHUNK_SIZE;
  if (rz < 0) rz += CHUNK_SIZE;

  int height = getHeightAt(rx, rz, anchor.x, anchor.z, anchor.hash, anchor.biome);

  if (y < 64 || y < height) goto skip_feature;

  uint8_t feature_position = anchor.hash % (CHUNK_SIZE * CHUNK_SIZE);

  short feature_x = feature_position % CHUNK_SIZE;
  short feature_z = feature_position / CHUNK_SIZE;

  // The following check does two things:
  //  firstly, it ensures that trees don't cross chunk boundaries;
  //  secondly, it reduces overall feature count. This is favorable
  //  everywhere except for swamps, which are otherwise very boring.
  if (anchor.biome != W_mangrove_swamp) {
    if (feature_x < 3 || feature_x > CHUNK_SIZE - 3) goto skip_feature;
    if (feature_z < 3 || feature_z > CHUNK_SIZE - 3) goto skip_feature;
  }

  uint8_t feature_variant = (anchor.hash >> (feature_x + feature_z)) & 1;

  feature_x += anchor.x * CHUNK_SIZE;
  feature_z += anchor.z * CHUNK_SIZE;

  switch (anchor.biome) {
    case W_plains: { // Generate trees in the plains biome

      uint8_t feature_y = getHeightAt(
        feature_x < 0 ? feature_x % CHUNK_SIZE + CHUNK_SIZE : feature_x % CHUNK_SIZE,
        feature_z < 0 ? feature_z % CHUNK_SIZE + CHUNK_SIZE : feature_z % CHUNK_SIZE,
        anchor.x, anchor.z, anchor.hash, anchor.biome
      ) + 1;
      if (feature_y < 64) break;

      if (x == feature_x && z == feature_z) {
        if (y == feature_y - 1) return B_dirt;
        if (y >= feature_y && y < feature_y - feature_variant + 6) return B_oak_log;
      }

      uint8_t dx = x > feature_x ? x - feature_x : feature_x - x;
      uint8_t dz = z > feature_z ? z - feature_z : feature_z - z;

      if (dx < 3 && dz < 3 && y > feature_y - feature_variant + 2 && y < feature_y - feature_variant + 5) {
        if (y == feature_y - feature_variant + 4 && dx == 2 && dz == 2) break;
        return B_oak_leaves;
      }
      if (dx < 2 && dz < 2 && y >= feature_y - feature_variant + 5 && y <= feature_y - feature_variant + 6) {
        if (y == feature_y - feature_variant + 6 && dx == 1 && dz == 1) break;
        return B_oak_leaves;
      }

      if (y == height) return B_grass_block;
      return B_air;
    }

    case W_desert: { // Generate dead bushes and cacti in deserts

      if (x != feature_x || z != feature_z) break;

      if (feature_variant == 0) {
        if (y == height + 1) return B_dead_bush;
      } else if (y > height) {
        // The size of the cactus is determined based on whether the terrain
        // height is even or odd at the target location
        if (height & 1 && y <= height + 3) return B_cactus;
        if (y <= height + 2) return B_cactus;
      }

      break;

    }

    case W_mangrove_swamp: { // Generate lilypads and moss carpets in swamps

      if (x == feature_x && z == feature_z && y == 64 && height < 63) {
        return B_lily_pad;
      }

      if (y == height + 1) {
        uint8_t dx = x > feature_x ? x - feature_x : feature_x - x;
        uint8_t dz = z > feature_z ? z - feature_z : feature_z - z;
        if (dx + dz < 4) return B_moss_carpet;
      }

      break;
    }

    case W_snowy_plains: { // Generate grass stubs in snowy plains

      if (x == feature_x && z == feature_z && y == height + 1 && height >= 64) {
        return B_short_grass;
      }

      break;
    }

    default: break;
  }

skip_feature:
  // Handle surface-level terrain (the very topmost blocks)
  if (height >= 63) {
    if (y == height) {
      if (anchor.biome == W_mangrove_swamp) return B_mud;
      if (anchor.biome == W_snowy_plains) return B_snowy_grass_block;
      if (anchor.biome == W_desert) return B_sand;
      if (anchor.biome == W_beach) return B_sand;
      return B_grass_block;
    }
    if (anchor.biome == W_snowy_plains && y == height + 1) {
      return B_snow;
    }
  }
  // Starting at 4 blocks below terrain level, generate minerals and caves
  if (y <= height - 4) {
    // Caves use the same shape as surface terrain, just mirrored
    int8_t gap = height - TERRAIN_BASE_HEIGHT;
    if (y < CAVE_BASE_DEPTH + gap && y > CAVE_BASE_DEPTH - gap) return B_air;

    // The chunk-relative X and Z coordinates are used in a bit shift on the hash
    // The sum of these is then used to get the Y coordinate of the ore in this column
    // This way, each column is guaranteed to have exactly one ore candidate
    uint8_t ore_x_component = (anchor.hash >> rx) & 31;
    uint8_t ore_z_component = (anchor.hash >> (rz + 16)) & 31;
    uint8_t ore_y = ore_x_component + ore_z_component;

    if (y == ore_y) {
      // Since the ore Y coordinate is effectely a random number in range [0;64],
      // we use it in another bit shift to get a pseudo-random number for the column
      uint8_t ore_probability = (anchor.hash >> ore_y) & 255;
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
  // Handle the space between stone and grass
  if (y <= height) {
    if (anchor.biome == W_desert) return B_sandstone;
    if (anchor.biome == W_mangrove_swamp) return B_mud;
    if (anchor.biome == W_beach && height > 64) return B_sandstone;
    return B_dirt;
  }
  // If all else failed, but we're below sea level, generate water (or ice)
  if (y == 63 && anchor.biome == W_snowy_plains) return B_ice;
  if (y < 64) return B_water;

  // For everything else, fall back to air
  return B_air;

}

uint8_t getBlockAt (int x, int y, int z) {

  uint8_t block_change = getBlockChange(x, y, z);
  if (block_change != 0xFF) return block_change;

  ChunkAnchor anchor = {
    x / CHUNK_SIZE,
    z / CHUNK_SIZE
  };
  if (x % CHUNK_SIZE < 0) anchor.x --;
  if (z % CHUNK_SIZE < 0) anchor.z --;
  anchor.hash = getChunkHash(anchor.x, anchor.z);
  anchor.biome = getChunkBiome(anchor.x, anchor.z);

  return getTerrainAt(x, y, z, anchor);

}

uint8_t chunk_section[4096];
ChunkAnchor chunk_anchors[256 / (CHUNK_SIZE * CHUNK_SIZE)];

void buildChunkSection (int cx, int cy, int cz) {

  // Precompute the hashes and anchors for each minichunk
  int anchor_index = 0;
  for (int i = cz; i < cz + 16; i += CHUNK_SIZE) {
    for (int j = cx; j < cx + 16; j += CHUNK_SIZE) {

      ChunkAnchor *anchor = chunk_anchors + anchor_index;

      anchor->x = j / CHUNK_SIZE;
      anchor->z = i / CHUNK_SIZE;
      anchor->hash = getChunkHash(anchor->x, anchor->z);
      anchor->biome = getChunkBiome(anchor->x, anchor->z);

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
    anchor_index = (j % 16) / CHUNK_SIZE + (j / 16 % 16) / CHUNK_SIZE * 2;
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
