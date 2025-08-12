#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "globals.h"
#include "varnum.h"

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

  uint64_t tmp;
  for (int i = 0; i < MAX_PLAYERS; i ++) {
    memcpy(&tmp, player_data + i * player_data_size, 8);
    if (memcmp(&tmp, uuid, 8) == 0) {
      memcpy(player_data + i * player_data_size + 16, &client_fd, 4);
      return 0;
    }
    if (tmp == 0) {
      memcpy(player_data + i * player_data_size, uuid, 16);
      memcpy(player_data + i * player_data_size + 16, &client_fd, 4);
      player_data[i * player_data_size + 20] = 8;
      player_data[i * player_data_size + 22] = 80;
      player_data[i * player_data_size + 24] = 8;
      return 0;
    }
  }

  return 1;

}

void clearPlayerFD (int client_fd) {

  int tmp;
  for (int i = 0; i < MAX_PLAYERS; i ++) {
    memcpy(&tmp, player_data + i * player_data_size + 16, 4);
    if (tmp == client_fd) {
      tmp = 0;
      memcpy(player_data + i * player_data_size + 16, &tmp, 4);
      break;
    }
  }

}

int savePlayerPosition (int client_fd, short x, short y, short z) {

  int tmp;
  for (int i = 0; i < MAX_PLAYERS; i ++) {
    if (memcmp(&client_fd, player_data + i * player_data_size + 16, 4) == 0) {

      memcpy(player_data + i * player_data_size + 20, &x, 2);
      memcpy(player_data + i * player_data_size + 22, &y, 2);
      memcpy(player_data + i * player_data_size + 24, &z, 2);

      return 0;
    }
  }

  return 1;

}

int savePlayerPositionAndRotation (int client_fd, short x, short y, short z, int8_t yaw, int8_t pitch) {

  int tmp;
  for (int i = 0; i < MAX_PLAYERS; i ++) {
    if (memcmp(&client_fd, player_data + i * player_data_size + 16, 4) == 0) {

      memcpy(player_data + i * player_data_size + 20, &x, 2);
      memcpy(player_data + i * player_data_size + 22, &y, 2);
      memcpy(player_data + i * player_data_size + 24, &z, 2);

      memcpy(player_data + i * player_data_size + 26, &yaw, 1);
      memcpy(player_data + i * player_data_size + 27, &pitch, 1);

      return 0;
    }
  }

  return 1;

}

int restorePlayerPosition (int client_fd, short *x, short *y, short *z, int8_t *yaw, int8_t *pitch) {

  int tmp;
  for (int i = 0; i < MAX_PLAYERS; i ++) {
    if (memcmp(&client_fd, player_data + i * player_data_size + 16, 4) == 0) {

      memcpy(x, player_data + i * player_data_size + 20, 2);
      memcpy(y, player_data + i * player_data_size + 22, 2);
      memcpy(z, player_data + i * player_data_size + 24, 2);

      if (yaw != NULL) memcpy(yaw, player_data + i * player_data_size + 26, 1);
      if (pitch != NULL) memcpy(pitch, player_data + i * player_data_size + 27, 1);

      return 0;
    }
  }

  return 1;

}

uint8_t *getPlayerInventory (int client_fd) {

  int tmp;
  for (int i = 0; i < MAX_PLAYERS; i ++) {
    if (memcmp(&client_fd, player_data + i * player_data_size + 16, 4) == 0) {
      return player_data + i * player_data_size + 29;
    }
  }

  return NULL;

}

uint8_t serverSlotToClientSlot (uint8_t slot) {
  if (slot >= 0 && slot <= 9) return slot + 36;
  if (slot == 40) return 45;
  if (slot >= 36 && slot <= 39) return 3 - (slot - 36) + 5;
}

uint8_t clientSlotToServerSlot (uint8_t slot) {
  if (slot >= 36 && slot <= 44) return slot - 36;
  if (slot == 45) return 40;
  if (slot >= 5 && slot <= 8) return 4 - (slot - 5) + 36;
  return 255;
}

uint8_t getBlockChange (short x, short y, short z) {
  short tmp;
  for (int i = 0; i < block_changes_count * 7; i += 7) {
    if (block_changes[i + 6] == 0xFF) continue;
    memcpy(&tmp, block_changes + i, 2);
    if (x != tmp) continue;
    memcpy(&tmp, block_changes + i + 2, 2);
    if (y != tmp) continue;
    memcpy(&tmp, block_changes + i + 4, 2);
    if (z != tmp) continue;
    return block_changes[i + 6];
  }
  return 0xFF;
}

void makeBlockChange (short x, short y, short z, uint8_t block) {

  short tmp;
  for (int i = 0; i < block_changes_count * 7; i += 7) {
    memcpy(&tmp, block_changes + i, 2);
    if (x != tmp) continue;
    memcpy(&tmp, block_changes + i + 2, 2);
    if (y != tmp) continue;
    memcpy(&tmp, block_changes + i + 4, 2);
    if (z != tmp) continue;
    block_changes[i + 6] = block;
    return;
  }

  int end = block_changes_count * 7;
  memcpy(block_changes + end, &x, 2);
  memcpy(block_changes + end + 2, &y, 2);
  memcpy(block_changes + end + 4, &z, 2);
  block_changes[end + 6] = block;
  block_changes_count ++;

}
