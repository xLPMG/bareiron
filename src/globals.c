#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "globals.h"

ssize_t recv_count;
uint8_t recv_buffer[256] = {0};

uint32_t world_seed = 0xA103DE6C;
uint32_t rng_seed = 0xE2B9419;

uint16_t client_count;

BlockChange block_changes[20000];
int block_changes_count = 0;

PlayerData player_data[MAX_PLAYERS];
