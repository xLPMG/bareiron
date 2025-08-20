#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "globals.h"

#ifdef ESP_PLATFORM
  #include "esp_task_wdt.h"
  #include "esp_timer.h"

  // Time between vTaskDelay calls in microseconds
  #define TASK_YIELD_INTERVAL 1000 * 1000
  // How many ticks to delay for on each yield
  #define TASK_YIELD_TICKS 1

  int64_t last_yield = 0;
  void task_yield () {
    int64_t time_now = esp_timer_get_time();
    if (time_now - last_yield < TASK_YIELD_INTERVAL) return;
    vTaskDelay(TASK_YIELD_TICKS);
    last_yield = time_now;
  }
#endif

ssize_t recv_count;
uint8_t recv_buffer[256] = {0};

uint32_t world_seed = 0xA103DE6B;
uint32_t rng_seed = 0xE2B9419;

uint16_t client_count;

BlockChange block_changes[20000];
int block_changes_count = 0;

PlayerData player_data[MAX_PLAYERS];
