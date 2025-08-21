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
  #include "esp_timer.h"
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

void handlePacket (int client_fd, int length, int packet_id) {

  int state = getClientState(client_fd);

  switch (packet_id) {

    case 0x00:
      if (state == STATE_NONE) {
        if (cs_handshake(client_fd)) break;
        return;
      } else if (state == STATE_LOGIN) {
        uint8_t uuid[16];
        char name[16];
        if (cs_loginStart(client_fd, uuid, name)) break;
        if (reservePlayerData(client_fd, uuid, name)) break;
        if (sc_loginSuccess(client_fd, uuid, name)) break;
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

        // Enter client into "play" state
        setClientState(client_fd, STATE_PLAY);
        sc_loginPlay(client_fd);

        PlayerData *player;
        if (getPlayerData(client_fd, &player)) break;

        // Send full client spawn sequence
        spawnPlayer(player);

        // Prepare join message for broadcast
        uint8_t player_name_len = strlen(player->name);
        char join_message[16 + player_name_len];
        strcpy(join_message, player->name);
        strcpy(join_message + player_name_len, " joined the game");

        // Register all existing players and spawn their entities, and broadcast
        // information about the joining player to all existing players.
        for (int i = 0; i < MAX_PLAYERS; i ++) {
          if (player_data[i].client_fd == -1) continue;
          sc_playerInfoUpdateAddPlayer(client_fd, player_data[i]);
          sc_systemChat(player_data[i].client_fd, join_message, 16 + player_name_len);
          if (player_data[i].client_fd == client_fd) continue;
          sc_playerInfoUpdateAddPlayer(player_data[i].client_fd, *player);
          sc_spawnEntityPlayer(client_fd, player_data[i]);
          sc_spawnEntityPlayer(player_data[i].client_fd, *player);
        }

        // Send information about all other entities (mobs)
        // For more info on the arguments, see the spawnMob function
        for (int i = 0; i < MAX_MOBS; i ++) {
          if (mob_data[i].type == 0) continue;
          sc_spawnEntity(
            client_fd, 65536 + i, recv_buffer,
            mob_data[i].type, mob_data[i].x, mob_data[i].y, mob_data[i].z,
            0, 0
          );
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

    case 0x08:
      if (state == STATE_PLAY) {
        cs_chat(client_fd);
        return;
      }
      break;

    case 0x0B:
      if (state == STATE_PLAY) {
        if (cs_clientStatus(client_fd)) break;
        return;
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
    case 0x1F:
      if (state == STATE_PLAY) {

        double x, y, z;
        float yaw, pitch;
        uint8_t on_ground;

        // Read player position (and rotation)
        if (packet_id == 0x1D) cs_setPlayerPosition(client_fd, &x, &y, &z, &on_ground);
        else if (packet_id == 0x1F) cs_setPlayerRotation (client_fd, &yaw, &pitch, &on_ground);
        else cs_setPlayerPositionAndRotation(client_fd, &x, &y, &z, &yaw, &pitch, &on_ground);

        PlayerData *player;
        if (getPlayerData(client_fd, &player)) break;

        // Update rotation in player data (if applicable)
        if (packet_id != 0x1D) {
          player->yaw = ((short)(yaw + 540) % 360 - 180) * 127 / 180;
          player->pitch = pitch / 90.0f * 127.0f;
        }

        // Handle fall damage
        if (on_ground) {
          int8_t damage = player->grounded_y - player->y - 3;
          if (damage > 0 && getBlockAt(player->x, player->y, player->z) != B_water) {
            if (damage >= player->health) player->health = 0;
            else player->health -= damage;
            sc_damageEvent(client_fd, client_fd, D_fall);
            sc_setHealth(client_fd, player->health, player->hunger);
          }
          player->grounded_y = player->y;
        }

        // Broadcast player position to other players
        #ifdef SCALE_MOVEMENT_UPDATES_TO_PLAYER_COUNT
          // If applicable, broadcast only every client_count-th movement update
          uint8_t should_broadcast = false;
          if (player->packets_since_update++ == client_count) {
            should_broadcast = true;
            player->packets_since_update = 0;
          }
        #else
          #define should_broadcast (client_count > 0)
        #endif
        if (should_broadcast) {
          // If the packet had no rotation data, calculate it from player data
          if (packet_id == 0x1D) {
            yaw = player->yaw * 180 / 127;
            pitch = player->pitch * 90 / 127;
          }
          // Send current position data to all connected players
          for (int i = 0; i < MAX_PLAYERS; i ++) {
            if (player_data[i].client_fd == -1) continue;
            if (player_data[i].client_fd == client_fd) continue;
            if (packet_id == 0x1F) {
              sc_updateEntityRotation(player_data[i].client_fd, client_fd, player->yaw, player->pitch);
            } else {
              sc_teleportEntity(player_data[i].client_fd, client_fd, x, y, z, yaw, pitch);
            }
            sc_setHeadRotation(player_data[i].client_fd, client_fd, player->yaw);
          }
        }

        // Don't continue if all we got was rotation data
        if (packet_id == 0x1F) return;

        // Cast the values to short to get integer position
        short cx = x, cy = y, cz = z;
        // Determine the player's chunk coordinates
        short _x = (cx < 0 ? cx - 16 : cx) / 16, _z = (cz < 0 ? cz - 16 : cz) / 16;
        // Calculate distance between previous and current chunk coordinates
        short dx = _x - (player->x < 0 ? player->x - 16 : player->x) / 16;
        short dz = _z - (player->z < 0 ? player->z - 16 : player->z) / 16;

        // Update position in player data
        player->x = cx;
        player->y = cy;
        player->z = cz;

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
        for (int i = 0; i < VISITED_HISTORY - 1; i ++) {
          player->visited_x[i] = player->visited_x[i + 1];
          player->visited_z[i] = player->visited_z[i + 1];
        }
        player->visited_x[VISITED_HISTORY - 1] = _x;
        player->visited_z[VISITED_HISTORY - 1] = _z;

        printf("sending new chunks (%d, %d)\n", _x, _z);
        sc_setCenterChunk(client_fd, _x, _z);

        int count = 0;
        clock_t start, end;
        start = clock();

        uint32_t r = fast_rand();
        if ((r & 3) == 0) {
          short mob_x = (_x + dx * VIEW_DISTANCE) * 16 + ((r >> 4) & 15);
          short mob_z = (_z + dz * VIEW_DISTANCE) * 16 + ((r >> 8) & 15);
          uint8_t mob_y = getHeightAt(mob_x, mob_z) + 1;
          if (getBlockAt(mob_x, mob_y, mob_z) == B_air) {
            spawnMob(95, mob_x, mob_y, mob_z);
          }
        }

        while (dx != 0) {
          sc_chunkDataAndUpdateLight(client_fd, _x + dx * VIEW_DISTANCE, _z);
          count ++;
          for (int i = 1; i <= VIEW_DISTANCE; i ++) {
            sc_chunkDataAndUpdateLight(client_fd, _x + dx * VIEW_DISTANCE, _z - i);
            sc_chunkDataAndUpdateLight(client_fd, _x + dx * VIEW_DISTANCE, _z + i);
            count += 2;
          }
          dx += dx > 0 ? -1 : 1;
        }
        while (dz != 0) {
          sc_chunkDataAndUpdateLight(client_fd, _x, _z + dz * VIEW_DISTANCE);
          count ++;
          for (int i = 1; i <= VIEW_DISTANCE; i ++) {
            sc_chunkDataAndUpdateLight(client_fd, _x - i, _z + dz * VIEW_DISTANCE);
            sc_chunkDataAndUpdateLight(client_fd, _x + i, _z + dz * VIEW_DISTANCE);
            count += 2;
          }
          dz += dz > 0 ? -1 : 1;
        }

        end = clock();
        double total_ms = (double)(end - start) / CLOCKS_PER_SEC * 1000;
        printf("generated %d chunks in %.0f ms (%.2f ms per chunk)\n", count, total_ms, total_ms / (double)count);

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
  if (*client_fd == -1) return;
  client_count --;
  setClientState(*client_fd, STATE_NONE);
  clearPlayerFD(*client_fd);
  close(*client_fd);
  *client_fd = -1;
  printf("Disconnected client %d, cause: %d, errno: %d\n\n", *client_fd, cause, errno);
}

int main () {

  // Hash the seeds to ensure they're random enough
  world_seed = splitmix64(world_seed);
  printf("World seed: ");
  for (int i = 3; i >= 0; i --) printf("%X", (unsigned int)((world_seed >> (8 * i)) & 255));

  rng_seed = splitmix64(rng_seed);
  printf("\nRNG seed: ");
  for (int i = 3; i >= 0; i --) printf("%X", (unsigned int)((rng_seed >> (8 * i)) & 255));
  printf("\n\n");

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
    player_data[i].client_fd = -1;
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

  // Track time of last server tick
  int64_t last_tick_time = get_program_time();

  /**
   * Cycles through all connected clients, handling one packet at a time
   * from each player. With every iteration, attempts to accept a new
   * client connection.
   */
  while (true) {
    // Check if it's time to yield to the idle task
    task_yield();

    // Attempt to accept a new connection
    for (int i = 0; i < MAX_PLAYERS; i ++) {
      if (clients[i] != -1) continue;
      clients[i] = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
      // If the accept was successful, make the client non-blocking too
      if (clients[i] != -1) {
        printf("New client, fd: %d\n", clients[i]);
        int flags = fcntl(clients[i], F_GETFL, 0);
        fcntl(clients[i], F_SETFL, flags | O_NONBLOCK);
        client_count ++;
      }
      break;
    }

    // Look for valid connected clients
    client_index ++;
    if (client_index == MAX_PLAYERS) client_index = 0;
    if (clients[client_index] == -1) continue;

    // Handle periodic events (server ticks)
    int64_t time_since_last_tick = get_program_time() - last_tick_time;
    if (time_since_last_tick > TIME_BETWEEN_TICKS) {
      handleServerTick(time_since_last_tick);
      last_tick_time = get_program_time();
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
  esp_timer_early_init();
  wifi_init();
}

#endif
