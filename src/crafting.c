#include <stdlib.h>
#include <string.h>

#include "globals.h"
#include "registries.h"
#include "crafting.h"

void getCraftingOutput (PlayerData *player, uint8_t *count, uint16_t *item) {

  uint8_t i, filled = 0, first = 10;
  for (i = 0; i < 9; i ++) {
    if (player->craft_items[i]) {
      filled ++;
      if (first == 10) first = i;
    }
  }

  uint8_t first_col = first % 3, first_row = first / 3;

  switch (filled) {

    case 0:
      *item = 0;
      *count = 0;
      return;

    case 1:
      switch (player->craft_items[first]) {
        case I_oak_log:
          *item = I_oak_planks;
          *count = 4;
          return;
        case I_oak_planks:
          *item = 715; // oak_button
          *count = 1;
          return;

        default: break;
      }
      break;

    case 2:
      switch (player->craft_items[first]) {
        case I_oak_planks:
          if (first_col != 2 && player->craft_items[first + 1] == I_oak_planks) {
            *item = 731; // oak_pressure_plate
            *count = 1;
            return;
          } else if (first_row != 2 && player->craft_items[first + 3] == I_oak_planks) {
            *item = I_stick;
            *count = 4;
            return;
          }
          break;

        default: break;
      }

    case 4:
      switch (player->craft_items[first]) {
        case I_oak_planks:
          if (
            first_col != 2 && first_row != 2 &&
            player->craft_items[first + 1] == I_oak_planks &&
            player->craft_items[first + 3] == I_oak_planks &&
            player->craft_items[first + 4] == I_oak_planks
          ) {
            *item = 320; // crafting_table
            *count = 1;
            return;
          }
          break;

        default: break;
      }

    case 5:
      switch (player->craft_items[first]) {
        case I_oak_planks:
          if (
            first == 0 &&
            player->craft_items[first + 1] == I_oak_planks &&
            player->craft_items[first + 2] == I_oak_planks &&
            player->craft_items[first + 4] == I_stick &&
            player->craft_items[first + 7] == I_stick
          ) {
            *item = 877; // wooden_pickaxe
            *count = 1;
            return;
          }
          break;

        default: break;
      }

    default: break;

  }

  *count = 0;
  *item = 0;

}
