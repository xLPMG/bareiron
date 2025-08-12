#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "globals.h"

ssize_t recv_count;
uint8_t recv_buffer[256] = {0};

uint32_t world_seed = 0xA103DE6C;
uint8_t block_changes[50 * 1024];
int block_changes_count = 0;

uint8_t player_data[(
  16 + // UUID
  4 +  // File descriptor
  2 +  // X position (short)
  2 +  // Y position (short)
  2 +  // Z position (short)
  2 +  // Angles (both, i8)
  1 +  // Hotbar slot
  82   // Inventory
) * MAX_PLAYERS];
int player_data_size = 16 + 4 + 2 + 2 + 2 + 2 + 1 + 82;
