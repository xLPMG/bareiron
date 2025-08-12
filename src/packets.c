#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <unistd.h>

#include "globals.h"
#include "tools.h"
#include "varnum.h"
#include "registries.h"
#include "worldgen.h"

// C->S Handshake
int cs_handshake (int client_fd) {
  printf("Received Handshake:\n");

  printf("  Protocol version: %d\n", readVarInt(client_fd));
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
// Leaves player name and UUID at indexes 0 and 17 of recv_buffer, respectively
int cs_loginStart (int client_fd) {
  printf("Received Login Start:\n");

  readString(client_fd);
  if (recv_count == -1) return 1;
  printf("  Player name: %s\n", recv_buffer);
  recv_count = recv(client_fd, recv_buffer + 17, 16, MSG_WAITALL);
  if (recv_count == -1) return 1;
  printf("  Player UUID: ");
  for (int i = 17; i < 33; i ++) printf("%x", recv_buffer[i]);
  printf("\n\n");

  return 0;
}

// S->C Login Success
int sc_loginSuccess (int client_fd, char *name, char *uuid) {
  printf("Sending Login Success...\n\n");

  int name_length = strlen(name);
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
  if (tmp) printf("  Chat colors: on\n", tmp);
  else printf("  Chat colors: off\n", tmp);
  tmp = readByte(client_fd);
  if (recv_count == -1) return 1;
  printf("  Skin parts: %d\n", tmp);
  tmp = readVarInt(client_fd);
  if (recv_count == -1) return 1;
  if (tmp) printf("  Main hand: right\n", tmp);
  else printf("  Main hand: left\n", tmp);
  tmp = readByte(client_fd);
  if (recv_count == -1) return 1;
  if (tmp) printf("  Text filtering: on\n", tmp);
  else printf("  Text filtering: off\n", tmp);
  tmp = readByte(client_fd);
  if (recv_count == -1) return 1;
  if (tmp) printf("  Allow listing: on\n", tmp);
  else printf("  Allow listing: off\n", tmp);
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
  if (strcmp(recv_buffer, "minecraft:brand") == 0) {
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
  writeVarInt(client_fd, 2);
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
int sc_setDefaultSpawnPosition (int client_fd, long x, long y, long z) {

  writeVarInt(client_fd, sizeVarInt(0x5A) + 12);
  writeVarInt(client_fd, 0x5A);

  writeUint64(client_fd, ((x & 0x3FFFFFF) << 38) | ((z & 0x3FFFFFF) << 12) | (y & 0xFFF));
  writeFloat(client_fd, 0);

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

  int palette_size = 0;
  for (int i = 0; i < 256; i ++) palette_size += sizeVarInt(block_palette[i]);

  const int chunk_data_size = (4101 + sizeVarInt(256) + palette_size) * 24;

  writeVarInt(client_fd, 17 + sizeVarInt(chunk_data_size) + chunk_data_size);
  writeByte(client_fd, 0x27);

  writeUint32(client_fd, _x);
  writeUint32(client_fd, _z);

  writeVarInt(client_fd, 0); // omit heightmaps

  writeVarInt(client_fd, chunk_data_size);

  int x = _x * 16, z = _z * 16, y;

  // send chunk sections
  for (int i = 0; i < 24; i ++) {
    y = i * 16 - 64;
    writeUint16(client_fd, 4096); // block count
    writeByte(client_fd, 8); // bits per entry
    writeVarInt(client_fd, 256);
    for (int j = 0; j < 256; j ++) writeVarInt(client_fd, block_palette[j]);
    for (int j = 0; j < 4096; j += 8) {
      for (int k = j + 7; k >= j; k --) {
        writeByte(client_fd, getBlockAt(
          k % 16 + x,
          k / 256 + y,
          k / 16 % 16 + z
        ));
      }
    }
    // biome data
    writeByte(client_fd, 0); // bits per entry
    writeByte(client_fd, 21); // palette (forest)
  }

  writeVarInt(client_fd, 0); // omit block entities

  // light data
  writeVarInt(client_fd, 0);
  writeVarInt(client_fd, 0);
  writeVarInt(client_fd, 0);
  writeVarInt(client_fd, 0);

  writeVarInt(client_fd, 0);
  writeVarInt(client_fd, 0);

  return 0;

}

// S->C Clientbound Keep Alive (play)
int sc_keepAlive (int client_fd) {

  writeVarInt(client_fd, 9);
  writeByte(client_fd, 0x26);

  writeUint64(client_fd, 0);

}

// S->C Set Container Slot
int sc_setContainerSlot (int client_fd, int container, uint16_t slot, uint8_t count, uint16_t item) {

  writeVarInt(client_fd,
    1 +
    sizeVarInt(container) +
    1 + 2 +
    sizeVarInt(count) +
    (count > 0 ? sizeVarInt(item) + 2 : 0)
  );
  writeByte(client_fd, 0x14);

  writeVarInt(client_fd, container);
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

// C->S Player Action
int cs_playerAction (int client_fd) {

  uint8_t action = readByte(client_fd);

  int64_t pos = readInt64(client_fd);
  int x = pos >> 38;
  int y = pos << 52 >> 52;
  int z = pos << 26 >> 38;

  readByte(client_fd); // ignore face

  int sequence = readVarInt(client_fd);

  if ((action == 0 && GAMEMODE == 1)) {
    // block was mined in creative
    makeBlockChange(x, y, z, 0);
  } else if (action == 2) {
    // block was mined in survival

    uint8_t block = getBlockAt(x, y, z);
    uint16_t item, tmp;

    if (block == B_oak_leaves) {
      if (sequence % 200 < 2) item = I_apple;
      else if (sequence % 50 < 2) item = I_stick;
      else if (sequence % 40 < 2) item = I_oak_sapling;
      else item = 0;
    } else item = B_to_I[block];

    uint8_t *inventory = getPlayerInventory(client_fd);

    makeBlockChange(x, y, z, 0);

    int slot_pair = -1;
    for (int i = 0; i < 36 * 3; i += 3) {
      memcpy(&tmp, inventory + i, 2);
      if (tmp == item && inventory[i+2] < 64) {
        slot_pair = i;
        break;
      }
    }

    if (slot_pair == -1) {
      for (int i = 0; i < 36 * 3; i += 3) {
        if ((inventory[i] == 0 && inventory[i + 1] == 0) || inventory[i+2] == 0) {
          slot_pair = i;
          break;
        }
      }
    }

    if (item && slot_pair != -1) {
      uint8_t slot = serverSlotToClientSlot(slot_pair / 3);
      memcpy(inventory + slot_pair, &item, 2);
      sc_setContainerSlot(client_fd, 0, slot, ++inventory[slot_pair + 2], item);
    }

  }

  return 0;

}

// C->S Use Item On
int cs_useItemOn (int client_fd) {

  uint8_t hand = readByte(client_fd);

  uint64_t pos = readUint64(client_fd);
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

  // first, get pointer to player inventory
  uint8_t *inventory = getPlayerInventory(client_fd);
  // then, get pointer to selected hotbar slot
  // the hotbar position is in address (inventory - 1)
  uint8_t *slot = inventory + (*(inventory - 1)) * 3;
  // the inventory is split into id-amount pairs, get the amount address
  uint8_t *amount = slot + 2;
  // convert the item id to a block id
  uint8_t block = I_to_B[*(uint16_t *)slot];

  // if the selected item doesn't correspond to a block, exit
  if (block == 0) return 0;
  // if the selected slot doesn't hold any items, exit
  if (*amount == 0) return 0;
  // decrease item amount in selected slot
  *amount = *amount - 1;
  // clear item id in slot if amount is zero
  if (*amount == 0) *slot = 0;

  switch (face) {
    case 0: makeBlockChange(x, y - 1, z, block); break;
    case 1: makeBlockChange(x, y + 1, z, block); break;
    case 2: makeBlockChange(x, y, z - 1, block); break;
    case 3: makeBlockChange(x, y, z + 1, block); break;
    case 4: makeBlockChange(x - 1, y, z, block); break;
    case 5: makeBlockChange(x + 1, y, z, block); break;
    default: break;
  }

  return 0;

}

int cs_clickContainer (int client_fd) {

  int window_id = readVarInt(client_fd);

  readVarInt(client_fd); // ignore state id
  readUint16(client_fd); // ignore clicked slot number
  readByte(client_fd);   // ignore button
  readVarInt(client_fd); // ignore operation mode

  int changes_count = readVarInt(client_fd);

  uint8_t *inventory = getPlayerInventory(client_fd);
  uint8_t slot, count;
  uint16_t item;
  int tmp;

  for (int i = 0; i < changes_count; i ++) {

    slot = clientSlotToServerSlot(readUint16(client_fd));

    if (!readByte(client_fd)) { // no item?
      if (slot != 255) {
        inventory[slot * 3] = 0;
        inventory[slot * 3 + 1] = 0;
        inventory[slot * 3 + 2] = 0;
      }
      continue;
    }

    item = readVarInt(client_fd);
    count = (uint8_t)readVarInt(client_fd);

    // ignore components
    tmp = readVarInt(client_fd);
    recv(client_fd, recv_buffer, tmp, MSG_WAITALL);
    tmp = readVarInt(client_fd);
    recv(client_fd, recv_buffer, tmp, MSG_WAITALL);

    if (item <= 255 && count > 0) {
      memcpy(inventory + slot * 3, &item, 2);
      inventory[slot * 3 + 2] = count;
    }

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

// C->S Set Held Item (serverbound)
int cs_setHeldItem (int client_fd) {

  uint8_t *hotbar = getPlayerInventory(client_fd) - 1;
  *hotbar = (uint8_t)readUint16(client_fd);

  return 0;
}

// S->C Set Held Item (clientbound)
int sc_setHeldItem (int client_fd, uint8_t slot) {

  writeVarInt(client_fd, sizeVarInt(0x62) + 1);
  writeVarInt(client_fd, 0x62);

  writeByte(client_fd, slot);

  return 0;
}

// S->C Registry Data (Multiple packets)
int sc_registries (int client_fd) {

  printf("Sending Registries\n\n");
  send(client_fd, registries_bin, sizeof(registries_bin), 0);

  char wolf_sound_variant[] = { 0x07, 0x1c, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x77, 0x6f, 0x6c, 0x66, 0x5f, 0x73, 0x6f, 0x75, 0x6e, 0x64, 0x5f, 0x76, 0x61, 0x72, 0x69, 0x61, 0x6e, 0x74, 0x07, 0x0f, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x61, 0x6e, 0x67, 0x72, 0x79, 0x00, 0x0d, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x62, 0x69, 0x67, 0x00, 0x11, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x63, 0x6c, 0x61, 0x73, 0x73, 0x69, 0x63, 0x00, 0x0e, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x63, 0x75, 0x74, 0x65, 0x00, 0x10, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x67, 0x72, 0x75, 0x6d, 0x70, 0x79, 0x00, 0x10, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x70, 0x75, 0x67, 0x6c, 0x69, 0x6e, 0x00, 0x0d, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x73, 0x61, 0x64, 0x00 };
  char pig_variant[] = { 0x07, 0x15, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x70, 0x69, 0x67, 0x5f, 0x76, 0x61, 0x72, 0x69, 0x61, 0x6e, 0x74, 0x03, 0x0e, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x63, 0x6f, 0x6c, 0x64, 0x00, 0x13, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x74, 0x65, 0x6d, 0x70, 0x65, 0x72, 0x61, 0x74, 0x65, 0x00, 0x0e, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x77, 0x61, 0x72, 0x6d, 0x00 };
  char frog_variant[] = { 0x07, 0x16, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x66, 0x72, 0x6f, 0x67, 0x5f, 0x76, 0x61, 0x72, 0x69, 0x61, 0x6e, 0x74, 0x03, 0x0e, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x63, 0x6f, 0x6c, 0x64, 0x00, 0x13, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x74, 0x65, 0x6d, 0x70, 0x65, 0x72, 0x61, 0x74, 0x65, 0x00, 0x0e, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x77, 0x61, 0x72, 0x6d, 0x00 };
  char cat_variant[] = { 0x07, 0x15, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x63, 0x61, 0x74, 0x5f, 0x76, 0x61, 0x72, 0x69, 0x61, 0x6e, 0x74, 0x0b, 0x13, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x61, 0x6c, 0x6c, 0x5f, 0x62, 0x6c, 0x61, 0x63, 0x6b, 0x00, 0x0f, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x62, 0x6c, 0x61, 0x63, 0x6b, 0x00, 0x1b, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x62, 0x72, 0x69, 0x74, 0x69, 0x73, 0x68, 0x5f, 0x73, 0x68, 0x6f, 0x72, 0x74, 0x68, 0x61, 0x69, 0x72, 0x00, 0x10, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x63, 0x61, 0x6c, 0x69, 0x63, 0x6f, 0x00, 0x10, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x6a, 0x65, 0x6c, 0x6c, 0x69, 0x65, 0x00, 0x11, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x70, 0x65, 0x72, 0x73, 0x69, 0x61, 0x6e, 0x00, 0x11, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x72, 0x61, 0x67, 0x64, 0x6f, 0x6c, 0x6c, 0x00, 0x0d, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x72, 0x65, 0x64, 0x00, 0x11, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x73, 0x69, 0x61, 0x6d, 0x65, 0x73, 0x65, 0x00, 0x0f, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x74, 0x61, 0x62, 0x62, 0x79, 0x00, 0x0f, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x77, 0x68, 0x69, 0x74, 0x65, 0x00 };
  char cow_variant[] = { 0x07, 0x15, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x63, 0x6f, 0x77, 0x5f, 0x76, 0x61, 0x72, 0x69, 0x61, 0x6e, 0x74, 0x03, 0x0e, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x63, 0x6f, 0x6c, 0x64, 0x00, 0x13, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x74, 0x65, 0x6d, 0x70, 0x65, 0x72, 0x61, 0x74, 0x65, 0x00, 0x0e, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x77, 0x61, 0x72, 0x6d, 0x00 };
  char chicken_variant[] = { 0x07, 0x19, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x63, 0x68, 0x69, 0x63, 0x6b, 0x65, 0x6e, 0x5f, 0x76, 0x61, 0x72, 0x69, 0x61, 0x6e, 0x74, 0x03, 0x0e, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x63, 0x6f, 0x6c, 0x64, 0x00, 0x13, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x74, 0x65, 0x6d, 0x70, 0x65, 0x72, 0x61, 0x74, 0x65, 0x00, 0x0e, 0x6d, 0x69, 0x6e, 0x65, 0x63, 0x72, 0x61, 0x66, 0x74, 0x3a, 0x77, 0x61, 0x72, 0x6d, 0x00 };

  writeVarInt(client_fd, sizeof(wolf_sound_variant));
  send(client_fd, &wolf_sound_variant, sizeof(wolf_sound_variant), 0);
  writeVarInt(client_fd, sizeof(pig_variant));
  send(client_fd, &pig_variant, sizeof(pig_variant), 0);
  writeVarInt(client_fd, sizeof(frog_variant));
  send(client_fd, &frog_variant, sizeof(frog_variant), 0);
  writeVarInt(client_fd, sizeof(cat_variant));
  send(client_fd, &cat_variant, sizeof(cat_variant), 0);
  writeVarInt(client_fd, sizeof(cow_variant));
  send(client_fd, &cow_variant, sizeof(cow_variant), 0);
  writeVarInt(client_fd, sizeof(chicken_variant));
  send(client_fd, &chicken_variant, sizeof(chicken_variant), 0);

  return 0;

}
