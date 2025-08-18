#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

#ifdef ESP_PLATFORM
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "nvs_flash.h"
  #include "esp_wifi.h"
  #include "esp_event.h"
  #include "esp_task_wdt.h"
  #include "lwip/sockets.h"
  #include "lwip/netdb.h"
#else
  #include <arpa/inet.h>
  #include <unistd.h>
#endif

#include "globals.h"
#include "tools.h"
#include "varnum.h"
#include "packets.h"
#include "worldgen.h"
#include "registries.h"

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
        if (reservePlayerData(client_fd, (char *)(recv_buffer + 17))) break;
        if (sc_loginSuccess(client_fd, (char *)recv_buffer, (char *)(recv_buffer + 17))) break;
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

        float spawn_x = 8.5f, spawn_y = 80.0f, spawn_z = 8.5f;
        float spawn_yaw = 0.0f, spawn_pitch = 0.0f;

        if (player->y == -32767) { // is this a new player?
          int _x = 8 / CHUNK_SIZE;
          int _z = 8 / CHUNK_SIZE;
          int rx = 8 % CHUNK_SIZE;
          int rz = 8 % CHUNK_SIZE;
          spawn_y = getHeightAt(rx, rz, _x, _z, getChunkHash(_x, _z)) + 1;
        } else {
          spawn_x = player->x > 0 ? (float)player->x + 0.5 : (float)player->x - 0.5;
          spawn_y = player->y;
          spawn_z = player->z > 0 ? (float)player->z + 0.5 : (float)player->z - 0.5;
          spawn_yaw = player->yaw * 180 / 127;
          spawn_pitch = player->pitch * 90 / 127;
        }

        sc_synchronizePlayerPosition(client_fd, spawn_x, spawn_y, spawn_z, spawn_yaw, spawn_pitch);

        for (uint8_t i = 0; i < 41; i ++) {
          sc_setContainerSlot(client_fd, 0, serverSlotToClientSlot(0, i), player->inventory_count[i], player->inventory_items[i]);
        }
        sc_setHeldItem(client_fd, player->hotbar);

        sc_playerAbilities(client_fd, 0x01 + 0x04); // invulnerability + flight
        sc_updateTime(client_fd, world_time);

        short _x = player->x / 16, _z = player->z / 16;
        if (player->x % 16 < 0) _x -= 1;
        if (player->z % 16 < 0) _z -= 1;

        sc_setDefaultSpawnPosition(client_fd, 8, 80, 8);
        sc_startWaitingForChunks(client_fd);
        sc_setCenterChunk(client_fd, _x, _z);

        // Send spawn chunk first
        sc_chunkDataAndUpdateLight(client_fd, _x, _z);
        for (int i = -2; i <= 2; i ++) {
          for (int j = -2; j <= 2; j ++) {
            if (i == 0 && j == 0) continue;
            sc_chunkDataAndUpdateLight(client_fd, _x + i, _z + j);
          }
        }
        // Re-synchronize player position after all chunks have been sent
        sc_synchronizePlayerPosition(client_fd, spawn_x, spawn_y, spawn_z, spawn_yaw, spawn_pitch);

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

    case 0x12:
      if (state == STATE_PLAY) {
        cs_closeContainer(client_fd);
        return;
      }
      break;

    case 0x1D:
    case 0x1E:
      if (state == STATE_PLAY) {

        double x, y, z;
        float yaw, pitch;

        // Read player position (and rotation)
        if (packet_id == 0x1D) cs_setPlayerPosition(client_fd, &x, &y, &z);
        else cs_setPlayerPositionAndRotation(client_fd, &x, &y, &z, &yaw, &pitch);
        // Cast the values to short to get integer position
        short cx = x, cy = y, cz = z;

        PlayerData *player;
        if (getPlayerData(client_fd, &player)) break;

        // Determine the player's chunk coordinates
        short _x = (cx < 0 ? cx - 16 : cx) / 16, _z = (cz < 0 ? cz - 16 : cz) / 16;
        // Calculate distance between previous and current chunk coordinates
        short dx = _x - (player->x < 0 ? player->x - 16 : player->x) / 16;
        short dz = _z - (player->z < 0 ? player->z - 16 : player->z) / 16;

        // Update position (and rotation) in player data
        player->x = cx;
        player->y = cy;
        player->z = cz;
        if (packet_id == 0x1E) {
          player->yaw = ((short)(yaw + 540) % 360 - 180) * 127 / 180;
          player->pitch = pitch / 90.0f * 127.0f;
        }

        // Exit early if no chunk borders were crossed
        if (dx == 0 && dz == 0) return;

        // Check if the player has recently been in this chunk
        int found = false;
        for (int i = 0; i < VISITED_HISTORY; i ++) {
          if (player->visited_x[i] == _x && player->visited_z[i] == _z) {
            found = true;
            break;
          }
        }
        if (found) return;

        // Update player's recently visited chunks
        for (int i = 0; i < VISITED_HISTORY; i ++) {
          player->visited_x[i] = player->visited_x[i + 1];
          player->visited_z[i] = player->visited_z[i + 1];
        }
        player->visited_x[VISITED_HISTORY - 1] = _x;
        player->visited_z[VISITED_HISTORY - 1] = _z;

        printf("sending new chunks (%d, %d)\n", _x, _z);
        sc_setCenterChunk(client_fd, _x, _z);

        clock_t start, end;
        start = clock();

        if (dx != 0 && dz != 0) {
          sc_chunkDataAndUpdateLight(client_fd, _x + dx * 2, _z - dx);
          sc_chunkDataAndUpdateLight(client_fd, _x + dx * 2, _z);
          sc_chunkDataAndUpdateLight(client_fd, _x + dx * 2, _z + dz);
          sc_chunkDataAndUpdateLight(client_fd, _x + dx * 2, _z + dz * 2);
          sc_chunkDataAndUpdateLight(client_fd, _x + dx, _z + dz * 2);
          sc_chunkDataAndUpdateLight(client_fd, _x, _z + dz * 2);
          sc_chunkDataAndUpdateLight(client_fd, _x - dz, _z + dz * 2);
        } else {
          sc_chunkDataAndUpdateLight(client_fd, _x + dx * 2, _z + dz * 2);
          sc_chunkDataAndUpdateLight(client_fd, _x + dx * 2 + dz, _z + dz * 2 + dx);
          sc_chunkDataAndUpdateLight(client_fd, _x + dx * 2 - dz, _z + dz * 2 - dx);
          sc_chunkDataAndUpdateLight(client_fd, _x + dx * 2 + dz * 2, _z + dz * 2 + dx * 2);
          sc_chunkDataAndUpdateLight(client_fd, _x + dx * 2 - dz * 2, _z + dz * 2 - dx * 2);
        }

        end = clock();
        double total_ms = (double)(end - start) / CLOCKS_PER_SEC * 1000;
        printf("generated 5 chunks in %.0f ms (%.2f ms per chunk)\n", total_ms, total_ms / 5.0f);

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

void disconnectClient (int *client_fd, int cause) {
  setClientState(*client_fd, STATE_NONE);
  clearPlayerFD(*client_fd);
  close(*client_fd);
  *client_fd = -1;
  printf("Disconnected client %d, cause: %d, errno: %d\n\n", *client_fd, cause, errno);
}

int main () {

  for (int i = 0; i < sizeof(block_changes) / sizeof(BlockChange); i ++) {
    block_changes[i].block = 0xFF;
  }

  int server_fd, opt = 1;
  struct sockaddr_in server_addr, client_addr;
  socklen_t addr_len = sizeof(client_addr);

  int clients[MAX_PLAYERS], client_index = 0;
  for (int i = 0; i < MAX_PLAYERS; i ++) {
    clients[i] = -1;
    client_states[i * 2] = -1;
  }

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

  // Set non-blocking socket flag
  int flags = fcntl(server_fd, F_GETFL, 0);
  fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

  // Track client keep-alives
  struct timespec time_now;
  struct timespec keepalive_last;
  clock_gettime(CLOCK_REALTIME, &time_now);
  clock_gettime(CLOCK_REALTIME, &keepalive_last);

  /**
   * Cycles through all connected clients, handling one packet at a time
   * from each player. With every iteration, attempts to accept a new
   * client connection.
   */
  while (true) {
    wdt_reset();

    // Attempt to accept a new connection
    for (int i = 0; i < MAX_PLAYERS; i ++) {
      if (clients[i] != -1) continue;
      clients[i] = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
      // If the accept was successful, make the client non-blocking too
      if (clients[i] != -1) {
        printf("New client, fd: %d\n", clients[i]);
        int flags = fcntl(clients[i], F_GETFL, 0);
        fcntl(clients[i], F_SETFL, flags | O_NONBLOCK);
      }
      break;
    }

    // Look for valid connected clients
    client_index ++;
    if (client_index == MAX_PLAYERS) client_index = 0;
    if (clients[client_index] == -1) continue;

    // Handle infrequent periodic events every 10 seconds
    clock_gettime(CLOCK_REALTIME, &time_now);
    time_t seconds_since_update = time_now.tv_sec - keepalive_last.tv_sec;
    if (seconds_since_update > 10) {
      // Send Keep Alive and Update Time packets to all in-game clients
      world_time += 20 * seconds_since_update;
      for (int i = 0; i < MAX_PLAYERS; i ++) {
        if (clients[i] == -1) continue;
        if (getClientState(clients[i]) != STATE_PLAY) continue;
        sc_keepAlive(clients[i]);
        sc_updateTime(clients[i], world_time);
      }
      // Reset keep-alive timer
      clock_gettime(CLOCK_REALTIME, &keepalive_last);
      /**
       * If the RNG seed ever hits 0, it'll never generate anything
       * else. This is because the fast_rand function uses a simple
       * XORshift. This isn't a common concern, so we only check for
       * this periodically. If it does become zero, we reset it to
       * the world seed as a good-enough fallback.
       */
      if (rng_seed == 0) rng_seed = world_seed;
    }

    // Handle this individual client
    int client_fd = clients[client_index];

    // Check if at least 2 bytes are available for reading
    ssize_t recv_count = recv(client_fd, &recv_buffer, 2, MSG_PEEK);
    if (recv_count < 2) {
      if (recv_count == 0 || (recv_count < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        disconnectClient(&clients[client_index], 1);
      }
      continue;
    }

    // Read packet length
    int length = readVarInt(client_fd);
    if (length == VARNUM_ERROR) {
      disconnectClient(&clients[client_index], 2);
      continue;
    }
    // Read packet ID
    int packet_id = readVarInt(client_fd);
    if (packet_id == VARNUM_ERROR) {
      disconnectClient(&clients[client_index], 3);
      continue;
    }
    // Handle packet data
    handlePacket(client_fd, length - sizeVarInt(packet_id), packet_id);
    if (recv_count == 0 || (recv_count == -1 && errno != EAGAIN && errno != EWOULDBLOCK)) {
      disconnectClient(&clients[client_index], 4);
      continue;
    }

  }

  close(server_fd);
  printf("Server closed.\n");

}

#ifdef ESP_PLATFORM

void bareiron_main (void *pvParameters) {
  esp_task_wdt_add(NULL);
  main();
  vTaskDelete(NULL);
}

static void wifi_event_handler (void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    printf("Got IP, starting server...\n\n");
    xTaskCreate(bareiron_main, "bareiron", 4096, NULL, 5, NULL);
  }
}

void wifi_init () {
  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

  wifi_config_t wifi_config = {
    .sta = {
      .ssid = WIFI_SSID,
      .password = WIFI_PASS,
      .threshold.authmode = WIFI_AUTH_WPA2_PSK
    }
  };

  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  esp_wifi_start();
}

void app_main () {
  wifi_init();
}

#endif
