#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef ESP_PLATFORM
  #include "lwip/sockets.h"
  #include "lwip/netdb.h"
  #include "esp_timer.h"
#else
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <time.h>
  #ifndef CLOCK_MONOTONIC
    #define CLOCK_MONOTONIC 1
  #endif
#endif

#include "globals.h"
#include "registries.h"
#include "varnum.h"
#include "packets.h"
#include "worldgen.h"
#include "tools.h"

static uint64_t htonll (uint64_t value) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return ((uint64_t)htonl((uint32_t)(value >> 32))) |
         ((uint64_t)htonl((uint32_t)(value & 0xFFFFFFFF)) << 32);
#else
  return value;
#endif
}

ssize_t recv_all (int client_fd, void *buf, size_t n, uint8_t require_first) {
  char *p = buf;
  size_t total = 0;

  // First-byte check if requested
  if (require_first) {
    ssize_t r = recv(client_fd, p, 1, MSG_PEEK);
    if (r <= 0) {
      if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return 0; // no first byte available yet
      }
      return -1; // error or connection closed
    }
  }

  // Busy-wait until we get exactly n bytes
  while (total < n) {
    ssize_t r = recv(client_fd, p + total, n - total, 0);
    if (r < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        task_yield();
        continue;
      } else {
        return -1; // real error
      }
    } else if (r == 0) {
      // connection closed before full read
      return total;
    }
    total += r;
  }

  return total; // got exactly n bytes
}

ssize_t send_all (int client_fd, const void *buf, size_t len) {
  const uint8_t *p = (const uint8_t *)buf;
  size_t sent = 0;

  while (sent < len) {
    ssize_t n = send(client_fd, p + sent, len - sent, MSG_NOSIGNAL);
    if (n > 0) {
      sent += (size_t)n;
      continue;
    }
    if (n == 0) {
      errno = ECONNRESET;
      return -1;
    }
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
      task_yield();
      continue;
    }
    return -1;
  }

  return 0;
}

ssize_t writeByte (int client_fd, uint8_t byte) {
  return send_all(client_fd, &byte, 1);
}
ssize_t writeUint16 (int client_fd, uint16_t num) {
  uint16_t be = htons(num);
  return send_all(client_fd, &be, sizeof(be));
}
ssize_t writeUint32 (int client_fd, uint32_t num) {
  uint32_t be = htonl(num);
  return send_all(client_fd, &be, sizeof(be));
}
ssize_t writeUint64 (int client_fd, uint64_t num) {
  uint64_t be = htonll(num);
  return send_all(client_fd, &be, sizeof(be));
}
ssize_t writeFloat (int client_fd, float num) {
  uint32_t bits;
  memcpy(&bits, &num, sizeof(bits));
  bits = htonl(bits);
  return send_all(client_fd, &bits, sizeof(bits));
}
ssize_t writeDouble (int client_fd, double num) {
  uint64_t bits;
  memcpy(&bits, &num, sizeof(bits));
  bits = htonll(bits);
  return send_all(client_fd, &bits, sizeof(bits));
}

uint8_t readByte (int client_fd) {
  recv_count = recv_all(client_fd, recv_buffer, 1, false);
  return recv_buffer[0];
}
uint16_t readUint16 (int client_fd) {
  recv_count = recv_all(client_fd, recv_buffer, 2, false);
  return ((uint16_t)recv_buffer[0] << 8) | recv_buffer[1];
}
uint32_t readUint32 (int client_fd) {
  recv_count = recv_all(client_fd, recv_buffer, 4, false);
  return ((uint32_t)recv_buffer[0] << 24) |
         ((uint32_t)recv_buffer[1] << 16) |
         ((uint32_t)recv_buffer[2] << 8) |
         ((uint32_t)recv_buffer[3]);
}
uint64_t readUint64 (int client_fd) {
  recv_count = recv_all(client_fd, recv_buffer, 8, false);
  return ((uint64_t)recv_buffer[0] << 56) |
         ((uint64_t)recv_buffer[1] << 48) |
         ((uint64_t)recv_buffer[2] << 40) |
         ((uint64_t)recv_buffer[3] << 32) |
         ((uint64_t)recv_buffer[4] << 24) |
         ((uint64_t)recv_buffer[5] << 16) |
         ((uint64_t)recv_buffer[6] << 8) |
         ((uint64_t)recv_buffer[7]);
}
int64_t readInt64 (int client_fd) {
  recv_count = recv_all(client_fd, recv_buffer, 8, false);
  return ((int64_t)recv_buffer[0] << 56) |
         ((int64_t)recv_buffer[1] << 48) |
         ((int64_t)recv_buffer[2] << 40) |
         ((int64_t)recv_buffer[3] << 32) |
         ((int64_t)recv_buffer[4] << 24) |
         ((int64_t)recv_buffer[5] << 16) |
         ((int64_t)recv_buffer[6] << 8) |
         ((int64_t)recv_buffer[7]);
}
float readFloat (int client_fd) {
  uint32_t bytes = readUint32(client_fd);
  float output;
  memcpy(&output, &bytes, sizeof(output));
  return output;
}
double readDouble (int client_fd) {
  uint64_t bytes = readUint64(client_fd);
  double output;
  memcpy(&output, &bytes, sizeof(output));
  return output;
}

void readString (int client_fd) {
  uint32_t length = readVarInt(client_fd);
  recv_count = recv_all(client_fd, recv_buffer, length, false);
  recv_buffer[recv_count] = '\0';
}

uint32_t fast_rand () {
  rng_seed ^= rng_seed << 13;
  rng_seed ^= rng_seed >> 17;
  rng_seed ^= rng_seed << 5;
  return rng_seed;
}

uint64_t splitmix64 (uint64_t state) {
  uint64_t z = state + 0x9e3779b97f4a7c15;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
  z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
  return z ^ (z >> 31);
}

#ifndef ESP_PLATFORM
int64_t get_program_time () {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
}
#endif

int client_states[MAX_PLAYERS * 2];

void setClientState (int client_fd, int new_state) {
  // Look for a client state with a matching file descriptor
  for (int i = 0; i < MAX_PLAYERS * 2; i += 2) {
    if (client_states[i] != client_fd) continue;
    client_states[i + 1] = new_state;
    return;
  }
  // If the above failed, look for an unused client state slot
  for (int i = 0; i < MAX_PLAYERS * 2; i += 2) {
    if (client_states[i] != -1) continue;
    client_states[i] = client_fd;
    client_states[i + 1] = new_state;
    return;
  }
}

int getClientState (int client_fd) {
  for (int i = 0; i < MAX_PLAYERS * 2; i += 2) {
    if (client_states[i] != client_fd) continue;
    return client_states[i + 1];
  }
  return STATE_NONE;
}

int getClientIndex (int client_fd) {
  for (int i = 0; i < MAX_PLAYERS * 2; i += 2) {
    if (client_states[i] != client_fd) continue;
    return i;
  }
  return -1;
}

void resetPlayerData (PlayerData *player) {
  player->health = 20;
  player->hunger = 20;
  player->y = -32767;
  player->grounded_y = 0;
  for (int i = 0; i < 41; i ++) {
    player->inventory_items[i] = 0;
    player->inventory_count[i] = 0;
  }
  for (int i = 0; i < 9; i ++) {
    player->craft_items[i] = 0;
    player->craft_count[i] = 0;
  }
}

int reservePlayerData (int client_fd, uint8_t *uuid, char *name) {

  for (int i = 0; i < MAX_PLAYERS; i ++) {
    if (memcmp(player_data[i].uuid, uuid, 16) == 0) {
      player_data[i].client_fd = client_fd;
      memcpy(player_data[i].name, name, 16);
      return 0;
    }
    uint8_t empty = true;
    for (uint8_t j = 0; j < 16; j ++) {
      if (player_data[i].uuid[j] != 0) {
        empty = false;
        break;
      }
    }
    if (empty) {
      player_data[i].client_fd = client_fd;
      memcpy(player_data[i].uuid, uuid, 16);
      memcpy(player_data[i].name, name, 16);
      resetPlayerData(&player_data[i]);
      return 0;
    }
  }

  return 1;

}

int getPlayerData (int client_fd, PlayerData **output) {
  for (int i = 0; i < MAX_PLAYERS; i ++) {
    if (player_data[i].client_fd == client_fd) {
      *output = &player_data[i];
      return 0;
    }
  }
  return 1;
}

void clearPlayerFD (int client_fd) {
  for (int i = 0; i < MAX_PLAYERS; i ++) {
    if (player_data[i].client_fd == client_fd) {
      player_data[i].client_fd = -1;
      return;
    }
  }
}

uint8_t serverSlotToClientSlot (int window_id, uint8_t slot) {

  if (window_id == 0) { // player inventory

    if (slot < 9) return slot + 36;
    if (slot >= 9 && slot <= 35) return slot;
    if (slot == 40) return 45;
    if (slot >= 36 && slot <= 39) return 3 - (slot - 36) + 5;
    if (slot >= 41 && slot <= 44) return slot - 40;

  } else if (window_id == 12) { // crafting table

    if (slot >= 41 && slot <= 49) return slot - 40;
    return serverSlotToClientSlot(0, slot - 1);

  } else if (window_id == 14) { // furnace

    if (slot >= 41 && slot <= 43) return slot - 41;
    return serverSlotToClientSlot(0, slot + 6);

  }

  return 255;
}

uint8_t clientSlotToServerSlot (int window_id, uint8_t slot) {

  if (window_id == 0) { // player inventory

    if (slot >= 36 && slot <= 44) return slot - 36;
    if (slot >= 9 && slot <= 35) return slot;
    if (slot == 45) return 40;
    if (slot >= 5 && slot <= 8) return 4 - (slot - 5) + 36;

    // map inventory crafting slots to player data crafting grid (semi-hack)
    // this abuses the fact that the buffers are adjacent in player data
    if (slot == 1) return 41;
    if (slot == 2) return 42;
    if (slot == 3) return 44;
    if (slot == 4) return 45;

  } else if (window_id == 12) { // crafting table

    // same crafting offset overflow hack as above
    if (slot >= 1 && slot <= 9) return 40 + slot;
    // the rest of the slots are identical, just shifted by one
    if (slot >= 10 && slot <= 45) return clientSlotToServerSlot(0, slot - 1);

  } else if (window_id == 14) { // furnace

    // move furnace items to the player's crafting grid
    // this lets us put them back in the inventory once the window closes
    if (slot <= 2) return 41 + slot;
    // the rest of the slots are identical, just shifted by 6
    if (slot >= 3 && slot <= 38) return clientSlotToServerSlot(0, slot + 6);

  }

  return 255;
}

int givePlayerItem (PlayerData *player, uint16_t item, uint8_t count) {

  uint8_t slot = 255;
  for (int i = 0; i < 41; i ++) {
    if (player->inventory_items[i] == item && player->inventory_count[i] <= 64 - count) {
      slot = i;
      break;
    }
  }

  if (slot == 255) {
    for (int i = 0; i < 41; i ++) {
      if (player->inventory_count[i] == 0) {
        slot = i;
        break;
      }
    }
  }

  if (slot == 255) return 1;

  player->inventory_items[slot] = item;
  player->inventory_count[slot] += count;
  sc_setContainerSlot(player->client_fd, 0, serverSlotToClientSlot(0, slot), player->inventory_count[slot], item);

  return 0;

}

// Sends the full sequence for spawning the player to the client
void spawnPlayer (PlayerData *player) {

  // Player spawn coordinates, initialized to placeholders
  float spawn_x = 8.5f, spawn_y = 80.0f, spawn_z = 8.5f;
  float spawn_yaw = 0.0f, spawn_pitch = 0.0f;

  if (player->y == -32767) { // Is this a new player?
    // Determine spawning Y coordinate based on terrain height
    spawn_y = getHeightAt(8, 8) + 1;
  } else { // Not a new player
    // Calculate spawn position from player data
    spawn_x = player->x > 0 ? (float)player->x + 0.5 : (float)player->x - 0.5;
    spawn_y = player->y;
    spawn_z = player->z > 0 ? (float)player->z + 0.5 : (float)player->z - 0.5;
    spawn_yaw = player->yaw * 180 / 127;
    spawn_pitch = player->pitch * 90 / 127;
  }

  // Teleport player to spawn coordinates (first pass)
  sc_synchronizePlayerPosition(player->client_fd, spawn_x, spawn_y, spawn_z, spawn_yaw, spawn_pitch);

  task_yield(); // Check task timer between packets

  // Sync client inventory and hotbar
  for (uint8_t i = 0; i < 41; i ++) {
    sc_setContainerSlot(player->client_fd, 0, serverSlotToClientSlot(0, i), player->inventory_count[i], player->inventory_items[i]);
  }
  sc_setHeldItem(player->client_fd, player->hotbar);
  // Sync client health and hunger
  sc_setHealth(player->client_fd, player->health, player->hunger);
  // Sync client clock time
  sc_updateTime(player->client_fd, world_time);

  // Give the player flight (for testing)
  sc_playerAbilities(player->client_fd, 0x04);

  // Calculate player's chunk coordinates
  short _x = div_floor(player->x, 16), _z = div_floor(player->z, 16);

  // Indicate that we're about to send chunk data
  sc_setDefaultSpawnPosition(player->client_fd, 8, 80, 8);
  sc_startWaitingForChunks(player->client_fd);
  sc_setCenterChunk(player->client_fd, _x, _z);

  task_yield(); // Check task timer between packets

  // Send spawn chunk first
  sc_chunkDataAndUpdateLight(player->client_fd, _x, _z);
  for (int i = -VIEW_DISTANCE; i <= VIEW_DISTANCE; i ++) {
    for (int j = -VIEW_DISTANCE; j <= VIEW_DISTANCE; j ++) {
      if (i == 0 && j == 0) continue;
      sc_chunkDataAndUpdateLight(player->client_fd, _x + i, _z + j);
    }
  }
  // Re-teleport player after all chunks have been sent
  sc_synchronizePlayerPosition(player->client_fd, spawn_x, spawn_y, spawn_z, spawn_yaw, spawn_pitch);

  task_yield(); // Check task timer between packets

}

uint8_t getBlockChange (short x, uint8_t y, short z) {
  for (int i = 0; i < block_changes_count; i ++) {
    if (block_changes[i].block == 0xFF) continue;
    if (
      block_changes[i].x == x &&
      block_changes[i].y == y &&
      block_changes[i].z == z
    ) return block_changes[i].block;
  }
  return 0xFF;
}

void makeBlockChange (short x, uint8_t y, short z, uint8_t block) {

  // Transmit block update to all in-game clients
  for (int i = 0; i < MAX_PLAYERS; i ++) {
    if (player_data[i].client_fd == -1) continue;
    if (getClientState(player_data[i].client_fd) != STATE_PLAY) continue;
    sc_blockUpdate(player_data[i].client_fd, x, y, z, block);
  }

  // Calculate terrain at these coordinates and compare it to the input block.
  // Since block changes get overlayed on top of terrain, we don't want to
  // store blocks that don't differ from the base terrain.
  ChunkAnchor anchor = {
    x / CHUNK_SIZE,
    z / CHUNK_SIZE
  };
  if (x % CHUNK_SIZE < 0) anchor.x --;
  if (z % CHUNK_SIZE < 0) anchor.z --;
  anchor.hash = getChunkHash(anchor.x, anchor.z);
  anchor.biome = getChunkBiome(anchor.x, anchor.z);

  uint8_t is_base_block = block == getTerrainAt(x, y, z, anchor);

  // Look for existing block change entries and replace them
  // 0xFF indicates a missing/restored entry
  for (int i = 0; i < block_changes_count; i ++) {
    if (block_changes[i].block == 0xFF && !is_base_block) {
      block_changes[i].x = x;
      block_changes[i].y = y;
      block_changes[i].z = z;
      block_changes[i].block = block;
      return;
    }
    if (
      block_changes[i].x == x &&
      block_changes[i].y == y &&
      block_changes[i].z == z
    ) {
      if (is_base_block) block_changes[i].block = 0xFF;
      else block_changes[i].block = block;
      return;
    }
  }

  // Don't create a new entry if it contains the base terrain block
  if (is_base_block) return;

  block_changes[block_changes_count].x = x;
  block_changes[block_changes_count].y = y;
  block_changes[block_changes_count].z = z;
  block_changes[block_changes_count].block = block;
  block_changes_count ++;

}

// Returns the result of mining a block, taking into account the block type and tools
// Probability numbers obtained with this formula: N = floor(P * 32 ^ 2)
uint16_t getMiningResult (uint16_t held_item, uint8_t block) {

  switch (block) {

    case B_oak_leaves:
      uint32_t r = fast_rand();
      if (r < 21474836) return I_apple; // 0.5%
      if (r < 85899345) return I_stick; // 2%
      if (r < 214748364) return I_oak_sapling; // 5%
      return 0;
      break;

    case B_stone:
    case B_cobblestone:
    case B_coal_ore:
    case B_iron_ore:
      // Check if player is holding (any) pickaxe
      if (
        held_item != I_wooden_pickaxe &&
        held_item != I_stone_pickaxe &&
        held_item != I_iron_pickaxe &&
        held_item != I_golden_pickaxe &&
        held_item != I_diamond_pickaxe &&
        held_item != I_netherite_pickaxe
      ) return 0;
      break;

    case B_gold_ore:
    case B_redstone_ore:
    case B_diamond_ore:
      // Check if player is holding an iron (or better) pickaxe
      if (
        held_item != I_iron_pickaxe &&
        held_item != I_golden_pickaxe &&
        held_item != I_diamond_pickaxe &&
        held_item != I_netherite_pickaxe
      ) return 0;
      break;

    case B_snow:
      // Check if player is holding (any) shovel
      if (
        held_item != I_wooden_shovel &&
        held_item != I_stone_shovel &&
        held_item != I_iron_shovel &&
        held_item != I_golden_shovel &&
        held_item != I_diamond_shovel &&
        held_item != I_netherite_shovel
      ) return 0;
      break;

    default: break;
  }

  return B_to_I[block];

}

// Rolls a random number to determine whether the player's tool should break
void bumpToolDurability (PlayerData *player) {

  uint16_t held_item = player->inventory_items[player->hotbar];

  // In order to avoid storing durability data, items break randomly with
  // the probability weighted based on vanilla durability.
  uint32_t r = fast_rand();
  if (
    ((held_item == I_wooden_pickaxe || held_item == I_wooden_axe || held_item == I_wooden_shovel) && r < 72796055) ||
    ((held_item == I_stone_pickaxe || held_item == I_stone_axe || held_item == I_stone_shovel) && r < 32786009) ||
    ((held_item == I_iron_pickaxe || held_item == I_iron_axe || held_item == I_iron_shovel) && r < 17179869) ||
    ((held_item == I_golden_pickaxe || held_item == I_golden_axe || held_item == I_golden_shovel) && r < 134217728) ||
    ((held_item == I_diamond_pickaxe || held_item == I_diamond_axe || held_item == I_diamond_shovel) && r < 2751420) ||
    ((held_item == I_netherite_pickaxe || held_item == I_netherite_axe || held_item == I_netherite_shovel) && r < 2114705)
  ) {
    player->inventory_items[player->hotbar] = 0;
    player->inventory_count[player->hotbar] = 0;
    sc_setContainerSlot(player->client_fd, 0, serverSlotToClientSlot(0, player->hotbar), 0, 0);
  }

}

// Checks whether the given block would be mined instantly with the held tool
uint8_t isInstantlyMined (PlayerData *player, uint8_t block) {

  uint16_t held_item = player->inventory_items[player->hotbar];

  if (block == B_snow) return (
    held_item == I_stone_shovel ||
    held_item == I_iron_shovel ||
    held_item == I_diamond_shovel ||
    held_item == I_netherite_shovel ||
    held_item == I_golden_shovel
  );

  return (
    block == B_dead_bush ||
    block == B_short_grass ||
    block == B_torch
  );

}

// Checks whether the given block has to have something beneath it
uint8_t isColumnBlock (uint8_t block) {

  return (
    block == B_snow ||
    block == B_moss_carpet ||
    block == B_cactus ||
    block == B_short_grass ||
    block == B_dead_bush ||
    block == B_sand ||
    block == B_torch
  );

}

void handlePlayerAction (PlayerData *player, int action, short x, short y, short z) {

  // In creative, only the "start mining" action is sent
  // No additional verification is performed, the block is simply removed
  if (action == 0 && GAMEMODE == 1) {
    makeBlockChange(x, y, z, 0);
    return;
  }

  // Ignore actions not pertaining to mining blocks
  if (action != 0 && action != 2) return;

  uint8_t block = getBlockAt(x, y, z);

  // If this is a "start mining" packet, the block must be instamine
  if (action == 0 && !isInstantlyMined(player, block)) return;

  makeBlockChange(x, y, z, 0);

  uint16_t held_item = player->inventory_items[player->hotbar];
  uint16_t item = getMiningResult(held_item, block);
  if (item) givePlayerItem(player, item, 1);
  bumpToolDurability(player);

  // Check if any blocks above this should break
  uint8_t y_offset = 1;
  uint8_t block_above = getBlockAt(x, y + y_offset, z);

  // Iterate upward over all blocks in the column
  while (isColumnBlock(block_above)) {
    // Destroy the next block
    makeBlockChange(x, y + y_offset, z, 0);
    // Check for item drops *without a tool*
    uint16_t item = getMiningResult(0, block_above);
    if (item) givePlayerItem(player, item, 1);
    // Select the next block in the column
    y_offset ++;
    block_above = getBlockAt(x, y + y_offset, z);
  }

}

void spawnMob (uint8_t type, short x, uint8_t y, short z) {

  for (int i = 0; i < MAX_MOBS; i ++) {
    // Look for type 0 (unallocated)
    if (mob_data[i].type != 0) continue;

    // Assign it the input parameters
    mob_data[i].type = type;
    mob_data[i].x = x;
    mob_data[i].y = y;
    mob_data[i].z = z;

    // Broadcast entity creation to all players
    for (int j = 0; j < MAX_PLAYERS; j ++) {
      if (player_data[j].client_fd == -1) continue;
      sc_spawnEntity(
        player_data[j].client_fd,
        65536 + i, // Try to avoid conflict with client file descriptors
        recv_buffer, // The UUID doesn't matter, feed it garbage
        type, x, y, z,
        // Face opposite of the player, as if looking at them when spawning
        (player_data[j].yaw + 127) & 255, 0
      );
    }

    break;
  }

}

// Simulates events scheduled for regular intervals
// Takes the time since the last tick in microseconds as the only arguemnt
void handleServerTick (int64_t time_since_last_tick) {

  // Send Keep Alive and Update Time packets to all in-game clients
  world_time += 20 * time_since_last_tick / 1000000;
  for (int i = 0; i < MAX_PLAYERS; i ++) {
    if (player_data[i].client_fd == -1) continue;
    sc_keepAlive(player_data[i].client_fd);
    sc_updateTime(player_data[i].client_fd, world_time);
  }

  /**
   * If the RNG seed ever hits 0, it'll never generate anything
   * else. This is because the fast_rand function uses a simple
   * XORshift. This isn't a common concern, so we only check for
   * this periodically. If it does become zero, we reset it to
   * the world seed as a good-enough fallback.
   */
  if (rng_seed == 0) rng_seed = world_seed;

  // Tick mob behavior
  for (int i = 0; i < MAX_MOBS; i ++) {
    if (mob_data[i].type == 0) continue;

    uint32_t r = fast_rand();

    // Skip 50% of ticks randomly
    if (r & 1) continue;

    // Move by one block on the X or Z axis
    // Yaw is set to face in the direction of motion
    short new_x = mob_data[i].x, new_z = mob_data[i].z;
    uint8_t yaw;
    if ((r >> 2) & 1) {
      if ((r >> 1) & 1) { new_x += 1; yaw = 192; }
      else { new_x -= 1; yaw = 64; }
    } else {
      if ((r >> 1) & 1) { new_z += 1; yaw = 0; }
      else { new_z -= 1; yaw = 128; }
    }
    // Vary the yaw angle to look just a little less robotic
    yaw += ((r >> 6) & 15) - 8;

    // Check if the block we're moving into is passable:
    //   if yes, and the block below is solid, keep the same Y level;
    //   if yes, but the block below isn't solid, drop down one block;
    //   if not, go up by up to one block;
    //   if going up isn't possible, skip this iteration.
    uint8_t new_y = mob_data[i].y;
    uint8_t block = getBlockAt(new_x, new_y, new_z);
    if (block != B_air) {
      if (getBlockAt(new_x, new_y + 1, new_z) == B_air) new_y += 1;
      else continue;
    } else if (getBlockAt(new_x, new_y - 1, new_z) == B_air) new_y -= 1;

    // Store new mob position
    mob_data[i].x = new_x;
    mob_data[i].y = new_y;
    mob_data[i].z = new_z;

    // Broadcast relevant entity movement packets
    for (int j = 0; j < MAX_PLAYERS; j ++) {
      if (player_data[j].client_fd == -1) continue;
      int entity_id = 65536 + i;
      sc_teleportEntity (
        player_data[j].client_fd, entity_id,
        (double)new_x + 0.5, new_y, (double)new_z + 0.5,
        yaw * 360 / 256, 0
      );
      sc_setHeadRotation(player_data[j].client_fd, entity_id, yaw);
    }

  }

}
