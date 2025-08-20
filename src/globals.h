#ifndef H_GLOBALS
#define H_GLOBALS

#include <stdint.h>
#include <unistd.h>

#ifdef ESP_PLATFORM
  #define WIFI_SSID "your-ssid"
  #define WIFI_PASS "your-password"
  #define wdt_reset();    \
    esp_task_wdt_reset(); \
    vTaskDelay(1);
#else
  #define wdt_reset();
#endif

#define true 1
#define false 0

#define PORT 25565
#define MAX_PLAYERS 16
#define GAMEMODE 0
#define VIEW_DISTANCE 2
// How many visited chunks to "remember"
// The server will not re-send chunks that the player has recently been in
#define VISITED_HISTORY 4
// If defined, scales the frequency at which player movement updates are
// broadcast based on the amount of players, reducing overhead for higher
// player counts. For very many players, makes movement look jittery.
#define SCALE_MOVEMENT_UPDATES_TO_PLAYER_COUNT

#define STATE_NONE 0
#define STATE_STATUS 1
#define STATE_LOGIN 2
#define STATE_TRANSFER 3
#define STATE_CONFIGURATION 4
#define STATE_PLAY 5

extern ssize_t recv_count;
extern uint8_t recv_buffer[256];

extern uint32_t world_seed;
extern uint32_t rng_seed;

extern uint16_t client_count;

#pragma pack(push, 1)

typedef struct {
  short x;
  uint8_t y;
  short z;
  uint8_t block;
} BlockChange;

typedef struct {
  uint8_t uuid[16];
  char name[16];
  int client_fd;
  short x;
  short y;
  short z;
  short visited_x[VISITED_HISTORY];
  short visited_z[VISITED_HISTORY];
  #ifdef SCALE_MOVEMENT_UPDATES_TO_PLAYER_COUNT
    uint16_t packets_since_update;
  #endif
  int8_t yaw;
  int8_t pitch;
  uint8_t hotbar;
  uint16_t inventory_items[41];
  uint16_t craft_items[9];
  uint8_t inventory_count[41];
  uint8_t craft_count[9];
} PlayerData;

#pragma pack(pop)

extern BlockChange block_changes[20000];
extern int block_changes_count;

extern PlayerData player_data[MAX_PLAYERS];

#endif
