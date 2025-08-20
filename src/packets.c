#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM
  #include "lwip/sockets.h"
  #include "lwip/netdb.h"
  #include "esp_task_wdt.h"
#else
  #include <arpa/inet.h>
  #include <unistd.h>
#endif

#include "globals.h"
#include "tools.h"
#include "varnum.h"
#include "registries.h"
#include "worldgen.h"
#include "crafting.h"

// C->S Handshake
int cs_handshake (int client_fd) {
  printf("Received Handshake:\n");

  printf("  Protocol version: %d\n", (int)readVarInt(client_fd));
  readString(client_fd);
  if (recv_count == -1) return 1;
  printf("  Server address: %s\n", recv_buffer);
  printf("  Server port: %u\n", readUint16(client_fd));
  int intent = readVarInt(client_fd);
  if (intent == VARNUM_ERROR) return 1;
  printf("  Intent: %d\n\n", intent);
  setClientState(client_fd, intent);

  return 0;
}

// C->S Login Start
int cs_loginStart (int client_fd, uint8_t *uuid, char *name) {
  printf("Received Login Start:\n");

  readString(client_fd);
  if (recv_count == -1) return 1;
  memcpy(name, recv_buffer, strlen((char *)recv_buffer) + 1);
  printf("  Player name: %s\n", name);
  recv_count = recv_all(client_fd, recv_buffer, 16, false);
  if (recv_count == -1) return 1;
  memcpy(uuid, recv_buffer, 16);
  printf("  Player UUID: ");
  for (int i = 0; i < 16; i ++) printf("%x", uuid[i]);
  printf("\n\n");

  return 0;
}

// S->C Login Success
int sc_loginSuccess (int client_fd, uint8_t *uuid, char *name) {
  printf("Sending Login Success...\n\n");

  uint8_t name_length = strlen(name);
  writeVarInt(client_fd, 1 + 16 + sizeVarInt(name_length) + name_length + 1);
  writeVarInt(client_fd, 0x02);
  send(client_fd, uuid, 16, 0);
  writeVarInt(client_fd, name_length);
  send(client_fd, name, name_length, 0);
  writeVarInt(client_fd, 0);

  return 0;
}

int cs_clientInformation (int client_fd) {
  int tmp;
  printf("Received Client Information:\n");
  readString(client_fd);
  if (recv_count == -1) return 1;
  printf("  Locale: %s\n", recv_buffer);
  tmp = readByte(client_fd);
  if (recv_count == -1) return 1;
  printf("  View distance: %d\n", tmp);
  tmp = readVarInt(client_fd);
  if (recv_count == -1) return 1;
  printf("  Chat mode: %d\n", tmp);
  tmp = readByte(client_fd);
  if (recv_count == -1) return 1;
  if (tmp) printf("  Chat colors: on\n");
  else printf("  Chat colors: off\n");
  tmp = readByte(client_fd);
  if (recv_count == -1) return 1;
  printf("  Skin parts: %d\n", tmp);
  tmp = readVarInt(client_fd);
  if (recv_count == -1) return 1;
  if (tmp) printf("  Main hand: right\n");
  else printf("  Main hand: left\n");
  tmp = readByte(client_fd);
  if (recv_count == -1) return 1;
  if (tmp) printf("  Text filtering: on\n");
  else printf("  Text filtering: off\n");
  tmp = readByte(client_fd);
  if (recv_count == -1) return 1;
  if (tmp) printf("  Allow listing: on\n");
  else printf("  Allow listing: off\n");
  tmp = readVarInt(client_fd);
  if (recv_count == -1) return 1;
  printf("  Particles: %d\n\n", tmp);
  return 0;
}

// S->C Clientbound Known Packs
int sc_knownPacks (int client_fd) {
  printf("Sending Server's Known Packs\n\n");
  char known_packs[] = {
    0x0e, 0x01, 0x09, 0x6d, 0x69, 0x6e,
    0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x04, 0x63,
    0x6f, 0x72, 0x65, 0x06, 0x31, 0x2e, 0x32, 0x31,
    0x2e, 0x38
  };
  writeVarInt(client_fd, 24);
  send(client_fd, &known_packs, 24, 0);
  return 0;
}

// C->S Serverbound Plugin Message
int cs_pluginMessage (int client_fd) {
  printf("Received Plugin Message:\n");
  readString(client_fd);
  if (recv_count == -1) return 1;
  printf("  Channel: \"%s\"\n", recv_buffer);
  if (strcmp((char *)recv_buffer, "minecraft:brand") == 0) {
    readString(client_fd);
    if (recv_count == -1) return 1;
    printf("  Brand: \"%s\"\n", recv_buffer);
  }
  printf("\n");
  return 0;
}

// S->C Finish Configuration
int sc_finishConfiguration (int client_fd) {
  writeVarInt(client_fd, 1);
  writeVarInt(client_fd, 0x03);
  return 0;
}

// S->C Login (play)
int sc_loginPlay (int client_fd) {

  writeVarInt(client_fd, 69 + sizeVarInt(MAX_PLAYERS));
  writeByte(client_fd, 0x2B);
  // entity id
  uint32_t entity_id = getClientIndex(client_fd);
  send(client_fd, &entity_id, 4, 0);
  // hardcore
  writeByte(client_fd, false);
  // dimensions
  writeVarInt(client_fd, 1);
  writeVarInt(client_fd, 19);
  const char *dimension = "minecraft:overworld";
  send(client_fd, dimension, 19, 0);
  // maxplayers
  writeVarInt(client_fd, MAX_PLAYERS);
  // view distance
  writeVarInt(client_fd, VIEW_DISTANCE);
  // sim distance
  writeVarInt(client_fd, 2);
  // reduced debug info
  writeByte(client_fd, 0);
  // respawn screen
  writeByte(client_fd, true);
  // limited crafting
  writeByte(client_fd, false);
  // dimension id
  writeVarInt(client_fd, 0);
  // dimension name
  writeVarInt(client_fd, 19);
  send(client_fd, dimension, 19, 0);
  // hashed seed
  writeUint64(client_fd, 0x0123456789ABCDEF);
  // gamemode
  writeByte(client_fd, GAMEMODE);
  // previous gamemode
  writeByte(client_fd, 0xFF);
  // is debug
  writeByte(client_fd, 0);
  // is flat
  writeByte(client_fd, 0);
  // has death location
  writeByte(client_fd, 0);
  // portal cooldown
  writeVarInt(client_fd, 0);
  // sea level
  writeVarInt(client_fd, 63);
  // secure chat
  writeByte(client_fd, 0);

  return 0;

}

// S->C Synchronize Player Position
int sc_synchronizePlayerPosition (int client_fd, double x, double y, double z, float yaw, float pitch) {

  writeVarInt(client_fd, 61 + sizeVarInt(-1));
  writeByte(client_fd, 0x41);

  // Teleport ID
  writeVarInt(client_fd, -1);

  // Position
  writeDouble(client_fd, x);
  writeDouble(client_fd, y);
  writeDouble(client_fd, z);

  // Velocity
  writeDouble(client_fd, 0);
  writeDouble(client_fd, 0);
  writeDouble(client_fd, 0);

  // Angles (Yaw/Pitch)
  writeFloat(client_fd, yaw);
  writeFloat(client_fd, pitch);

  // Flags
  writeUint32(client_fd, 0);

  return 0;

}

// S->C Set Default Spawn Position
int sc_setDefaultSpawnPosition (int client_fd, int64_t x, int64_t y, int64_t z) {

  writeVarInt(client_fd, sizeVarInt(0x5A) + 12);
  writeVarInt(client_fd, 0x5A);

  writeUint64(client_fd, ((x & 0x3FFFFFF) << 38) | ((z & 0x3FFFFFF) << 12) | (y & 0xFFF));
  writeFloat(client_fd, 0);

  return 0;
}

// S->C Player Abilities (clientbound)
int sc_playerAbilities (int client_fd, uint8_t flags) {

  writeVarInt(client_fd, 10);
  writeByte(client_fd, 0x39);

  writeByte(client_fd, flags);
  writeFloat(client_fd, 0.05f);
  writeFloat(client_fd, 0.1f);

  return 0;
}

// S->C Update Time
int sc_updateTime (int client_fd, uint64_t ticks) {

  writeVarInt(client_fd, sizeVarInt(0x6A) + 17);
  writeVarInt(client_fd, 0x6A);

  writeUint64(client_fd, ticks);
  writeUint64(client_fd, ticks);
  writeByte(client_fd, true);

  return 0;
}

// S->C Game Event 13 (Start waiting for level chunks)
int sc_startWaitingForChunks (int client_fd) {
  writeVarInt(client_fd, 6);
  writeByte(client_fd, 0x22);
  writeByte(client_fd, 13);
  writeUint32(client_fd, 0);
  return 0;
}

// S->C Set Center Chunk
int sc_setCenterChunk (int client_fd, int x, int y) {
  writeVarInt(client_fd, 1 + sizeVarInt(x) + sizeVarInt(y));
  writeByte(client_fd, 0x57);
  writeVarInt(client_fd, x);
  writeVarInt(client_fd, y);
  return 0;
}

// S->C Chunk Data and Update Light
int sc_chunkDataAndUpdateLight (int client_fd, int _x, int _z) {

  const int chunk_data_size = (4101 + sizeVarInt(256) + sizeof(network_block_palette)) * 20 + 6 * 12;
  const int light_data_size = 14 + (sizeVarInt(2048) + 2048) * 26;

  writeVarInt(client_fd, 11 + sizeVarInt(chunk_data_size) + chunk_data_size + light_data_size);
  writeByte(client_fd, 0x27);

  writeUint32(client_fd, _x);
  writeUint32(client_fd, _z);

  writeVarInt(client_fd, 0); // omit heightmaps

  writeVarInt(client_fd, chunk_data_size);

  int x = _x * 16, z = _z * 16, y;

  // send 4 chunk sections (up to Y=0) with no blocks
  for (int i = 0; i < 4; i ++) {
    writeUint16(client_fd, 4096); // block count
    writeByte(client_fd, 0); // block bits
    writeVarInt(client_fd, 85); // block palette (bedrock)
    writeByte(client_fd, 0); // biome bits
    writeByte(client_fd, 0); // biome palette
  }
  // reset watchdog and yield
  wdt_reset();

  // send chunk sections
  for (int i = 0; i < 20; i ++) {
    y = i * 16;
    writeUint16(client_fd, 4096); // block count
    writeByte(client_fd, 8); // bits per entry
    writeVarInt(client_fd, 256); // block palette length
    // block palette as varint buffer
    send(client_fd, network_block_palette, sizeof(network_block_palette), 0);
    // chunk section buffer
    buildChunkSection(x, y, z);
    send(client_fd, chunk_section, 4096, 0);
    // biome data
    writeByte(client_fd, 0); // bits per entry
    writeByte(client_fd, W_plains); // biome palette
    // reset watchdog and yield
    wdt_reset();
  }

  // send 8 chunk sections (up to Y=192) with no blocks
  for (int i = 0; i < 8; i ++) {
    writeUint16(client_fd, 4096); // block count
    writeByte(client_fd, 0); // block bits
    writeVarInt(client_fd, 0); // block palette (air)
    writeByte(client_fd, 0); // biome bits
    writeByte(client_fd, 0); // biome palette
  }
  // reset watchdog and yield
  wdt_reset();

  writeVarInt(client_fd, 0); // omit block entities

  // light data
  writeVarInt(client_fd, 1);
  writeUint64(client_fd, 0b11111111111111111111111111);
  writeVarInt(client_fd, 0);
  writeVarInt(client_fd, 0);
  writeVarInt(client_fd, 0);

  // sky light array
  writeVarInt(client_fd, 26);
  for (int i = 0; i < 2048; i ++) chunk_section[i] = 0xFF;
  for (int i = 2048; i < 4096; i ++) chunk_section[i] = 0;
  for (int i = 0; i < 8; i ++) {
    writeVarInt(client_fd, 2048);
    send(client_fd, chunk_section + 2048, 2048, 0);
  }
  for (int i = 0; i < 18; i ++) {
    writeVarInt(client_fd, 2048);
    send(client_fd, chunk_section, 2048, 0);
  }
  // don't send block light
  writeVarInt(client_fd, 0);

  return 0;

}

// S->C Clientbound Keep Alive (play)
int sc_keepAlive (int client_fd) {

  writeVarInt(client_fd, 9);
  writeByte(client_fd, 0x26);

  writeUint64(client_fd, 0);

  return 0;
}

// S->C Set Container Slot
int sc_setContainerSlot (int client_fd, int window_id, uint16_t slot, uint8_t count, uint16_t item) {

  writeVarInt(client_fd,
    1 +
    sizeVarInt(window_id) +
    1 + 2 +
    sizeVarInt(count) +
    (count > 0 ? sizeVarInt(item) + 2 : 0)
  );
  writeByte(client_fd, 0x14);

  writeVarInt(client_fd, window_id);
  writeVarInt(client_fd, 0);
  writeUint16(client_fd, slot);

  writeVarInt(client_fd, count);
  if (count > 0) {
    writeVarInt(client_fd, item);
    writeVarInt(client_fd, 0);
    writeVarInt(client_fd, 0);
  }

  return 0;

}

// S->C Block Update
int sc_blockUpdate (int client_fd, int64_t x, int64_t y, int64_t z, uint8_t block) {
  writeVarInt(client_fd, 9 + sizeVarInt(block_palette[block]));
  writeByte(client_fd, 0x08);
  writeUint64(client_fd, ((x & 0x3FFFFFF) << 38) | ((z & 0x3FFFFFF) << 12) | (y & 0xFFF));
  writeVarInt(client_fd, block_palette[block]);
}

// S->C Acknowledge Block Change
int sc_acknowledgeBlockChange (int client_fd, int sequence) {
  writeVarInt(client_fd, 1 + sizeVarInt(sequence));
  writeByte(client_fd, 0x04);
  writeVarInt(client_fd, sequence);
  return 0;
}

// C->S Player Action
int cs_playerAction (int client_fd) {

  uint8_t action = readByte(client_fd);

  int64_t pos = readInt64(client_fd);
  int x = pos >> 38;
  int y = pos << 52 >> 52;
  int z = pos << 26 >> 38;

  readByte(client_fd); // ignore face

  int sequence = readVarInt(client_fd);
  sc_acknowledgeBlockChange(client_fd, sequence);

  if ((action == 0 && GAMEMODE == 1)) {
    // block was mined in creative
    makeBlockChange(x, y, z, 0);
  } else if (action == 2) {
    // block was mined in survival

    uint8_t block = getBlockAt(x, y, z);
    uint16_t item = getMiningResult(client_fd, block);

    makeBlockChange(x, y, z, 0);

    if (item) {
      PlayerData *player;
      if (getPlayerData(client_fd, &player)) return 1;
      givePlayerItem(player, item, 1);
    }

  }

  return 0;

}

// S->C Open Screen
int sc_openScreen (int client_fd, uint8_t window, const char *title, uint16_t length) {

  writeVarInt(client_fd, 1 + 2 * sizeVarInt(window) + 1 + 2 + length);
  writeByte(client_fd, 0x34);

  writeVarInt(client_fd, window);
  writeVarInt(client_fd, window);

  writeByte(client_fd, 8); // string nbt tag
  writeUint16(client_fd, length); // string length
  send(client_fd, title, length, 0);

  return 0;
}

// C->S Use Item On
int cs_useItemOn (int client_fd) {

  uint8_t hand = readByte(client_fd);

  int64_t pos = readInt64(client_fd);
  int x = pos >> 38;
  int y = pos << 52 >> 52;
  int z = pos << 26 >> 38;

  uint8_t face = readByte(client_fd);

  // ignore cursor position
  readUint32(client_fd);
  readUint32(client_fd);
  readUint32(client_fd);

  // ignore "inside block" and "world border hit"
  readByte(client_fd);
  readByte(client_fd);

  int sequence = readVarInt(client_fd);
  sc_acknowledgeBlockChange(client_fd, sequence);

  uint8_t target = getBlockAt(x, y, z);
  if (target == B_crafting_table) {
    sc_openScreen(client_fd, 12, "Crafting", 8);
    return 0;
  } else if (target == B_furnace) {
    sc_openScreen(client_fd, 14, "Furnace", 7);
    return 0;
  }

  switch (face) {
    case 0: y -= 1; break;
    case 1: y += 1; break;
    case 2: z -= 1; break;
    case 3: z += 1; break;
    case 4: x -= 1; break;
    case 5: x += 1; break;
    default: break;
  }

  PlayerData *player;
  if (getPlayerData(client_fd, &player)) return 1;

  // check if the player is in the way
  if (x == player->x && (y == player->y || y == player->y + 1) && z == player->z) return 0;

  uint16_t *item = &player->inventory_items[player->hotbar];
  uint8_t *count = &player->inventory_count[player->hotbar];
  uint8_t block = I_to_B(*item);

  // if the selected item doesn't correspond to a block, exit
  if (block == 0) return 0;
  // if the selected slot doesn't hold any items, exit
  if (*count == 0) return 0;
  // decrease item amount in selected slot
  *count -= 1;
  // clear item id in slot if amount is zero
  if (*count == 0) *item = 0;

  makeBlockChange(x, y, z, block);
  sc_setContainerSlot(client_fd, 0, serverSlotToClientSlot(0, player->hotbar), *count, *item);

  return 0;

}

// C->S Click Container
int cs_clickContainer (int client_fd) {

  int window_id = readVarInt(client_fd);

  readVarInt(client_fd); // ignore state id
  readUint16(client_fd); // ignore clicked slot number
  readByte(client_fd);   // ignore button
  readVarInt(client_fd); // ignore operation mode

  int changes_count = readVarInt(client_fd);

  PlayerData *player;
  if (getPlayerData(client_fd, &player)) return 1;

  uint8_t slot, count, craft = false;
  uint16_t item;
  int tmp;

  for (int i = 0; i < changes_count; i ++) {

    slot = clientSlotToServerSlot(window_id, readUint16(client_fd));
    // slots outside of the inventory overflow into the crafting buffer
    if (slot > 40) craft = true;

    if (!readByte(client_fd)) { // no item?
      if (slot != 255) {
        player->inventory_items[slot] = 0;
        player->inventory_count[slot] = 0;
      }
      continue;
    }

    item = readVarInt(client_fd);
    count = (uint8_t)readVarInt(client_fd);

    // ignore components
    tmp = readVarInt(client_fd);
    recv_all(client_fd, recv_buffer, tmp, false);
    tmp = readVarInt(client_fd);
    recv_all(client_fd, recv_buffer, tmp, false);

    if (count > 0) {
      player->inventory_items[slot] = item;
      player->inventory_count[slot] = count;
    }

  }

  // window 0 is player inventory, window 12 is crafting table
  if (craft && (window_id == 0 || window_id == 12)) {
    getCraftingOutput(player, &count, &item);
    sc_setContainerSlot(client_fd, window_id, 0, count, item);
  } else if (window_id == 14) { // furnace
    getSmeltingOutput(player);
    for (int i = 0; i < 3; i ++) {
      sc_setContainerSlot(client_fd, window_id, i, player->craft_count[i], player->craft_items[i]);
    }
  }

  // read but ignore carried item slot (for now)
  if (readByte(client_fd)) {
    readVarInt(client_fd);
    readVarInt(client_fd);
    tmp = readVarInt(client_fd);
    recv_all(client_fd, recv_buffer, tmp, false);
    tmp = readVarInt(client_fd);
    recv_all(client_fd, recv_buffer, tmp, false);
  }

  return 0;

}

// C->S Set Player Position And Rotation
int cs_setPlayerPositionAndRotation (int client_fd, double *x, double *y, double *z, float *yaw, float *pitch) {

  *x = readDouble(client_fd);
  *y = readDouble(client_fd);
  *z = readDouble(client_fd);

  *yaw = readFloat(client_fd);
  *pitch = readFloat(client_fd);

  // ignore flags
  readByte(client_fd);

  return 0;

}

// C->S Set Player Position
int cs_setPlayerPosition (int client_fd, double *x, double *y, double *z) {

  *x = readDouble(client_fd);
  *y = readDouble(client_fd);
  *z = readDouble(client_fd);

  // ignore flags
  readByte(client_fd);

  return 0;

}

// C->S Set Player Rotation
int cs_setPlayerRotation (int client_fd, float *yaw, float *pitch) {

  *yaw = readFloat(client_fd);
  *pitch = readFloat(client_fd);

  // ignore flags
  readByte(client_fd);

  return 0;

}

// C->S Set Held Item (serverbound)
int cs_setHeldItem (int client_fd) {

  PlayerData *player;
  if (getPlayerData(client_fd, &player)) return 1;

  player->hotbar = (uint8_t)readUint16(client_fd);

  return 0;
}

// S->C Set Held Item (clientbound)
int sc_setHeldItem (int client_fd, uint8_t slot) {

  writeVarInt(client_fd, sizeVarInt(0x62) + 1);
  writeVarInt(client_fd, 0x62);

  writeByte(client_fd, slot);

  return 0;
}

// C->S Close Container (serverbound)
int cs_closeContainer (int client_fd) {

  uint8_t window_id = readVarInt(client_fd);

  PlayerData *player;
  if (getPlayerData(client_fd, &player)) return 1;

  // return all items in crafting slots to the player
  for (uint8_t i = 0; i < 9; i ++) {
    givePlayerItem(player, player->craft_items[i], player->craft_count[i]);
    player->craft_items[i] = 0;
    player->craft_count[i] = 0;
    uint8_t client_slot = serverSlotToClientSlot(window_id, 41 + i);
    if (client_slot != 255) sc_setContainerSlot(player->client_fd, 0, client_slot, 0, 0);
  }

  return 0;
}

// S->C Player Info Update, "Add Player" action
int sc_playerInfoUpdateAddPlayer (int client_fd, PlayerData player) {

  writeVarInt(client_fd, 21 + strlen(player.name)); // Packet length
  writeByte(client_fd, 0x3F); // Packet ID

  writeByte(client_fd, 0x01); // EnumSet: Add Player
  writeByte(client_fd, 1); // Player count (1 per packet)

  // Player UUID
  send(client_fd, player.uuid, 16, 0);
  // Player name
  writeByte(client_fd, strlen(player.name));
  send(client_fd, player.name, strlen(player.name), 0);
  // Properties (don't send any)
  writeByte(client_fd, 0);

  return 0;
}

// S->C Spawn Entity
int sc_spawnEntity (
  int client_fd,
  int id, uint8_t *uuid, int type,
  double x, double y, double z,
  uint8_t yaw, uint8_t pitch
) {

  writeVarInt(client_fd, 51 + sizeVarInt(id) + sizeVarInt(type));
  writeByte(client_fd, 0x01);

  writeVarInt(client_fd, id); // Entity ID
  send(client_fd, uuid, 16, 0); // Entity UUID
  writeVarInt(client_fd, type); // Entity type

  // Position
  writeDouble(client_fd, x);
  writeDouble(client_fd, y);
  writeDouble(client_fd, z);

  // Angles
  writeByte(client_fd, yaw);
  writeByte(client_fd, pitch);
  writeByte(client_fd, yaw);

  // Data - mostly unused
  writeByte(client_fd, 0);

  // Velocity
  writeUint16(client_fd, 0);
  writeUint16(client_fd, 0);
  writeUint16(client_fd, 0);

  return 0;
}

// S->C Spawn Entity (from PlayerData)
int sc_spawnEntityPlayer (int client_fd, PlayerData player) {
  return sc_spawnEntity(
    client_fd,
    player.client_fd, player.uuid, 149,
    player.x > 0 ? (double)player.x + 0.5 : (double)player.x - 0.5,
    player.y,
    player.z > 0 ? (double)player.z + 0.5 : (float)player.z - 0.5,
    player.yaw, player.pitch
  );
}

// S->C Teleport Entity
int sc_teleportEntity (
  int client_fd, int id,
  double x, double y, double z,
  float yaw, float pitch
) {

  // Packet length and ID
  writeVarInt(client_fd, 58 + sizeVarInt(id));
  writeByte(client_fd, 0x1F);

  // Entity ID
  writeVarInt(client_fd, id);
  // Position
  writeDouble(client_fd, x);
  writeDouble(client_fd, y);
  writeDouble(client_fd, z);
  // Velocity
  writeUint64(client_fd, 0);
  writeUint64(client_fd, 0);
  writeUint64(client_fd, 0);
  // Angles
  writeFloat(client_fd, yaw);
  writeFloat(client_fd, pitch);
  // On ground flag
  writeByte(client_fd, 1);

  return 0;
}

int sc_setHeadRotation (int client_fd, int id, uint8_t yaw) {

  // Packet length and ID
  writeByte(client_fd, 2 + sizeVarInt(id));
  writeByte(client_fd, 0x4C);
  // Entity ID
  writeVarInt(client_fd, id);
  // Head yaw
  writeByte(client_fd, yaw);

  return 0;
}

int sc_updateEntityRotation (int client_fd, int id, uint8_t yaw, uint8_t pitch) {

  // Packet length and ID
  writeByte(client_fd, 4 + sizeVarInt(id));
  writeByte(client_fd, 0x31);
  // Entity ID
  writeVarInt(client_fd, id);
  // Angles
  writeByte(client_fd, yaw);
  writeByte(client_fd, pitch);
  // "On ground" flag
  writeByte(client_fd, 1);

  return 0;
}

// S->C Registry Data (multiple packets) and Update Tags (configuration, multiple packets)
int sc_registries (int client_fd) {

  printf("Sending Registries\n\n");
  send(client_fd, registries_bin, sizeof(registries_bin), 0);

  printf("Sending Tags\n\n");
  send(client_fd, tags_bin, sizeof(tags_bin), 0);

  return 0;

}
