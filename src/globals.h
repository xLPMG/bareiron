#ifndef H_GLOBALS
#define H_GLOBALS

#include <stdint.h>

#define true 1
#define false 0

#define PORT 25565
#define MAX_PLAYERS 16
#define GAMEMODE 0

#define STATE_NONE 0
#define STATE_STATUS 1
#define STATE_LOGIN 2
#define STATE_TRANSFER 3
#define STATE_CONFIGURATION 4
#define STATE_PLAY 5

extern ssize_t recv_count;
extern uint8_t recv_buffer[256];

extern uint32_t world_seed;
extern uint8_t block_changes[50 * 1024];
extern int block_changes_count;

extern uint8_t player_data[(
  16 + // UUID
  4 +  // File descriptor
  2 +  // X position (short)
  2 +  // Y position (short)
  2 +  // Z position (short)
  2 +  // Angles (both, i8)
  1 +  // Hotbar slot
  82   // Inventory
) * MAX_PLAYERS];
extern int player_data_size;

#endif
