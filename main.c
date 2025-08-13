#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

#include <arpa/inet.h>
#include <unistd.h>

#include "src/globals.h"
#include "src/tools.h"
#include "src/varnum.h"
#include "src/packets.h"
#include "src/worldgen.h"

uint64_t world_time = 0;

void handlePacket (int client_fd, int length, int packet_id) {

  int state = getClientState(client_fd);

  switch (packet_id) {

    case 0x00:
      if (state == STATE_NONE) {
        if (cs_handshake(client_fd)) break;
        return;
      } else if (state == STATE_LOGIN) {
        if (cs_loginStart(client_fd)) break;
        if (reservePlayerData(client_fd, recv_buffer + 17)) break;
        if (sc_loginSuccess(client_fd, recv_buffer, recv_buffer + 17)) break;
        return;
      } else if (state == STATE_CONFIGURATION) {
        if (cs_clientInformation(client_fd)) break;
        if (sc_knownPacks(client_fd)) break;
        if (sc_registries(client_fd)) break;
        return;
      }
      break;

    case 0x02:
      if (state == STATE_CONFIGURATION) {
        if (cs_pluginMessage(client_fd)) break;
        return;
      }
      break;

    case 0x03:
      if (state == STATE_LOGIN) {
        printf("Client Acknowledged Login\n\n");
        setClientState(client_fd, STATE_CONFIGURATION);
        return;
      } else if (state == STATE_CONFIGURATION) {
        printf("Client Acknowledged Configuration\n\n");
        setClientState(client_fd, STATE_PLAY);

        sc_loginPlay(client_fd);

        PlayerData *player;
        if (getPlayerData(client_fd, &player)) break;

        if (player->y == -32767) { // is this a new player?

          int _x = 8 / chunk_size;
          int _z = 8 / chunk_size;
          int rx = 8 % chunk_size;
          int rz = 8 % chunk_size;

          uint32_t chunk_hash = getChunkHash(_x, _z);
          sc_synchronizePlayerPosition(client_fd, 8.5, getHeightAt(rx, rz, _x, _z, chunk_hash) + 1, 8.5, 0, 0);

        } else {
          sc_synchronizePlayerPosition(client_fd, player->x, player->y, player->z, player->yaw * 180 / 127, player->pitch * 90 / 127);
        }

        for (uint8_t i = 0; i < 41; i ++) {
          sc_setContainerSlot(client_fd, 0, serverSlotToClientSlot(i), player->inventory_count[i], player->inventory_items[i]);
        }
        sc_setHeldItem(client_fd, player->hotbar);

        sc_playerAbilities(client_fd, 0x01 + 0x04); // invulnerability + flight
        sc_updateTime(client_fd, world_time);

        short _x = player->x / 16, _z = player->z / 16;
        sc_setDefaultSpawnPosition(client_fd, 8, 80, 8);
        sc_startWaitingForChunks(client_fd);
        sc_setCenterChunk(client_fd, _x, _z);

        for (int i = -2; i <= 2; i ++) {
          for (int j = -2; j <= 2; j ++) {
            sc_chunkDataAndUpdateLight(client_fd, _x + i, _z + j);
          }
        }

        return;
      }
      break;

    case 0x07:
      if (state == STATE_CONFIGURATION) {
        printf("Received Client's Known Packs\n");
        printf("  Finishing configuration\n\n");
        sc_finishConfiguration(client_fd);
      }
      break;

    case 0x0C:
      if (state == STATE_PLAY) {
        // client tick
        return;
      }
      break;

    case 0x11:
      if (state == STATE_PLAY) {
        cs_clickContainer(client_fd);
        return;
      }
      break;

    case 0x1D:
    case 0x1E:
      if (state == STATE_PLAY) {

        double x, y, z;
        float yaw, pitch;

        if (packet_id == 0x1D) cs_setPlayerPosition(client_fd, &x, &y, &z);
        else cs_setPlayerPositionAndRotation(client_fd, &x, &y, &z, &yaw, &pitch);
        short cx = x + 0.5, cy = y, cz = z + 0.5;

        PlayerData *player;
        if (getPlayerData(client_fd, &player)) break;

        short _x = (cx < 0 ? cx - 16 : cx) / 16, _z = (cz < 0 ? cz - 16 : cz) / 16;
        short dx = _x - (player->x < 0 ? player->x - 16 : player->x) / 16, dz = _z - (player->z < 0 ? player->z - 16 : player->z) / 16;

        if (dx != 0 || dz != 0) {

          printf("sending new chunks (%d, %d)\n", _x, _z);
          sc_setCenterChunk(client_fd, _x, _z);

          clock_t start, end;
          int count = 0;
          start = clock();

          if (dx != 0 && dz != 0) {
            sc_chunkDataAndUpdateLight(client_fd, _x + dx * 2, _z - dx);
            sc_chunkDataAndUpdateLight(client_fd, _x + dx * 2, _z);
            sc_chunkDataAndUpdateLight(client_fd, _x + dx * 2, _z + dz);
            sc_chunkDataAndUpdateLight(client_fd, _x + dx * 2, _z + dz * 2);
            sc_chunkDataAndUpdateLight(client_fd, _x + dx, _z + dz * 2);
            sc_chunkDataAndUpdateLight(client_fd, _x, _z + dz * 2);
            sc_chunkDataAndUpdateLight(client_fd, _x - dz, _z + dz * 2);
            count += 5;
          } else {
            for (int i = -2; i <= 2; i ++) {
              count ++;
              sc_chunkDataAndUpdateLight(client_fd, _x + dx * 2 + i * dz, _z + dz * 2 + i * dx);
              printf("(%d, %d) ", _x + dx * 2 + i * dz, _z + dz * 2 + i * dx);
            }
            printf("\n");
          }

          end = clock();
          double total_ms = (double)(end - start) / CLOCKS_PER_SEC * 1000;
          printf("generated %d chunks in %.0f ms (%.2f ms per chunk)\n", count, total_ms, total_ms / count);

        }

        player->x = cx;
        player->y = cy;
        player->z = cz;
        if (packet_id == 0x1E) {
          player->yaw = ((short)(yaw + 540) % 360 - 180) * 127 / 180;
          player->pitch = pitch / 90.0f * 127.0f;
        }

        return;
      }
      break;

    case 0x34:
      if (state == STATE_PLAY) {
        cs_setHeldItem(client_fd);
        return;
      }
      break;

    case 0x28:
      if (state == STATE_PLAY) {
        cs_playerAction(client_fd);
        return;
      }
      break;

    case 0x3F:
      if (state == STATE_PLAY) {
        cs_useItemOn(client_fd);
        return;
      }
      break;

    default: break;

  }

  // if (packet_id < 16) printf("Unknown/bad packet: 0x0%X, length: %d, state: %d\n\n", packet_id, length, state);
  // else printf("Unknown/bad packet: 0x%X, length: %d, state: %d\n\n", packet_id, length, state);
  recv_count = recv(client_fd, recv_buffer, length, 0);

}

int main () {

  for (int i = 0; i < sizeof(block_changes) / sizeof(BlockChange); i ++) {
    block_changes[i].block = 0xFF;
  }

  int server_fd, client_fd, opt = 1;
  struct sockaddr_in server_addr, client_addr;
  socklen_t addr_len = sizeof(client_addr);
  struct sockaddr_in addr;

  // Create socket
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("socket options failed");
    exit(EXIT_FAILURE);
  }

  // Bind socket to IP/port
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("bind failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  // Listen for incoming connections
  if (listen(server_fd, 5) < 0) {
    perror("listen failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %d...\n", PORT);

  while (true) {

    // Accept a connection
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
      perror("accept failed");
      close(server_fd);
      exit(EXIT_FAILURE);
    }

    printf("Client connected.\n");

    struct timespec time_now;
    struct timespec keepalive_last;
    clock_gettime(CLOCK_REALTIME, &time_now);
    clock_gettime(CLOCK_REALTIME, &keepalive_last);

    while (true) {

      if (getClientState(client_fd) == STATE_PLAY) {
        clock_gettime(CLOCK_REALTIME, &time_now);
        if (time_now.tv_sec - keepalive_last.tv_sec > 10) {
          sc_keepAlive(client_fd);
          sc_updateTime(client_fd, world_time += 200);
          clock_gettime(CLOCK_REALTIME, &keepalive_last);
        }
      }

      int length = readVarInt(client_fd);
      if (length == VARNUM_ERROR) break;
      int packet_id = readVarInt(client_fd);
      if (packet_id == VARNUM_ERROR) break;
      handlePacket(client_fd, length - sizeVarInt(packet_id), packet_id);
      if (recv_count == -1) break;

    }

    setClientState(client_fd, STATE_NONE);
    clearPlayerFD(client_fd);

    close(client_fd);
    printf("Connection closed.\n");

  }

  close(server_fd);
  printf("Server closed.\n");

  return 0;

}
