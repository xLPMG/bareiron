#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM
  #include "lwip/sockets.h"
  #include "lwip/netdb.h"
#else
  #include <arpa/inet.h>
  #include <unistd.h>
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

ssize_t writeByte (int client_fd, uint8_t byte) {
  return send(client_fd, &byte, 1, 0);
}
ssize_t writeUint16 (int client_fd, uint16_t num) {
  uint16_t be = htons(num);
  return send(client_fd, &be, sizeof(be), 0);
}
ssize_t writeUint32 (int client_fd, uint32_t num) {
  uint32_t be = htonl(num);
  return send(client_fd, &be, sizeof(be), 0);
}
ssize_t writeUint64 (int client_fd, uint64_t num) {
  uint64_t be = htonll(num);
  return send(client_fd, &be, sizeof(be), 0);
}
ssize_t writeFloat (int client_fd, float num) {
  uint32_t bits;
  memcpy(&bits, &num, sizeof(bits));
  bits = htonl(bits);
  return send(client_fd, &bits, sizeof(bits), 0);
}
ssize_t writeDouble (int client_fd, double num) {
  uint64_t bits;
  memcpy(&bits, &num, sizeof(bits));
  bits = htonll(bits);
  return send(client_fd, &bits, sizeof(bits), 0);
}

uint8_t readByte (int client_fd) {
  recv_count = recv(client_fd, recv_buffer, 1, MSG_WAITALL);
  return recv_buffer[0];
}
uint16_t readUint16 (int client_fd) {
  recv_count = recv(client_fd, recv_buffer, 2, MSG_WAITALL);
  return ((uint16_t)recv_buffer[0] << 8) | recv_buffer[1];
}
uint32_t readUint32 (int client_fd) {
  recv_count = recv(client_fd, recv_buffer, 4, MSG_WAITALL);
  return ((uint32_t)recv_buffer[0] << 24) |
         ((uint32_t)recv_buffer[1] << 16) |
         ((uint32_t)recv_buffer[2] << 8) |
         ((uint32_t)recv_buffer[3]);
}
uint64_t readUint64 (int client_fd) {
  recv_count = recv(client_fd, recv_buffer, 8, MSG_WAITALL);
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
  recv_count = recv(client_fd, recv_buffer, 8, MSG_WAITALL);
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
  recv_count = recv(client_fd, recv_buffer, length, MSG_WAITALL);
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

int client_states[MAX_PLAYERS * 2];

void setClientState (int client_fd, int new_state) {
  for (int i = 0; i < MAX_PLAYERS * 2; i += 2) {
    if (client_states[i] != client_fd && client_states[i] != 0) continue;
    client_states[i] = client_fd;
    client_states[i + 1] = new_state;
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

int reservePlayerData (int client_fd, char *uuid) {

  for (int i = 0; i < MAX_PLAYERS; i ++) {
    if (memcmp(player_data[i].uuid, uuid, 16) == 0) {
      player_data[i].client_fd = client_fd;
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
      player_data[i].y = -32767;
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
      player_data[i].client_fd = 0;
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

uint8_t getBlockChange (short x, short y, short z) {
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

void makeBlockChange (short x, short y, short z, uint8_t block) {

  // Transmit block update to all managed clients
  for (int i = 0; i < MAX_PLAYERS; i ++) {
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
uint16_t getMiningResult (int client_fd, uint8_t block) {

  PlayerData *player;
  if (getPlayerData(client_fd, &player)) return 0;
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
    sc_setContainerSlot(client_fd, 0, serverSlotToClientSlot(0, player->hotbar), 0, 0);
  }

  switch (block) {

    case B_oak_leaves:
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

    default: break;
  }

  return B_to_I[block];

}
