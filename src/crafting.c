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

  uint16_t first_item = player->craft_items[first];
  uint8_t first_col = first % 3, first_row = first / 3;

  switch (filled) {

    case 0:
      *item = 0;
      *count = 0;
      return;

    case 1:
      switch (first_item) {
        case I_oak_log:
          *item = I_oak_planks;
          *count = 4;
          return;
        case I_oak_planks:
          *item = I_oak_button;
          *count = 1;
          return;

        default: break;
      }
      break;

    case 2:
      switch (first_item) {
        case I_oak_planks:
          if (first_col != 2 && player->craft_items[first + 1] == I_oak_planks) {
            *item = I_oak_pressure_plate;
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
      break;

    case 3:
      switch (first_item) {
        case I_oak_planks:
        case I_cobblestone:
        case I_iron_ingot:
        case I_gold_ingot:
        case I_diamond:
        case I_netherite_ingot:
          if (
            first_row == 0 &&
            player->craft_items[first + 3] == I_stick &&
            player->craft_items[first + 6] == I_stick
          ) {
            if (first_item == I_oak_planks) *item = I_wooden_shovel;
            else if (first_item == I_cobblestone) *item = I_stone_shovel;
            else if (first_item == I_iron_ingot) *item = I_iron_shovel;
            else if (first_item == I_gold_ingot) *item = I_golden_shovel;
            else if (first_item == I_diamond) *item = I_diamond_shovel;
            else if (first_item == I_netherite_ingot) *item = I_netherite_shovel;
            *count = 1;
            return;
          }
          if (
            first_row == 0 &&
            player->craft_items[first + 3] == first_item &&
            player->craft_items[first + 6] == I_stick
          ) {
            if (first_item == I_oak_planks) *item = I_wooden_sword;
            else if (first_item == I_cobblestone) *item = I_stone_sword;
            else if (first_item == I_iron_ingot) *item = I_iron_sword;
            else if (first_item == I_gold_ingot) *item = I_golden_sword;
            else if (first_item == I_diamond) *item = I_diamond_sword;
            else if (first_item == I_netherite_ingot) *item = I_netherite_sword;
            *count = 1;
            return;
          }
          break;

        default: break;
      }
      break;

    case 4:
      switch (first_item) {
        case I_oak_planks:
          if (
            first_col != 2 && first_row != 2 &&
            player->craft_items[first + 1] == I_oak_planks &&
            player->craft_items[first + 3] == I_oak_planks &&
            player->craft_items[first + 4] == I_oak_planks
          ) {
            *item = I_crafting_table;
            *count = 1;
            return;
          }
          break;

        default: break;
      }
      break;

    case 5:
      switch (first_item) {
        case I_oak_planks:
        case I_cobblestone:
        case I_iron_ingot:
        case I_gold_ingot:
        case I_diamond:
        case I_netherite_ingot:
          if (
            first == 0 &&
            player->craft_items[first + 1] == first_item &&
            player->craft_items[first + 2] == first_item &&
            player->craft_items[first + 4] == I_stick &&
            player->craft_items[first + 7] == I_stick
          ) {
            if (first_item == I_oak_planks) *item = I_wooden_pickaxe;
            else if (first_item == I_cobblestone) *item = I_stone_pickaxe;
            else if (first_item == I_iron_ingot) *item = I_iron_pickaxe;
            else if (first_item == I_gold_ingot) *item = I_golden_pickaxe;
            else if (first_item == I_diamond) *item = I_diamond_pickaxe;
            else if (first_item == I_netherite_ingot) *item = I_netherite_pickaxe;
            *count = 1;
            return;
          }
          if (
            first < 2 &&
            player->craft_items[first + 1] == first_item &&
            ((
              player->craft_items[first + 3] == first_item &&
              player->craft_items[first + 4] == I_stick &&
              player->craft_items[first + 7] == I_stick
            ) || (
              player->craft_items[first + 4] == first_item &&
              player->craft_items[first + 3] == I_stick &&
              player->craft_items[first + 6] == I_stick
            ))
          ) {
            if (first_item == I_oak_planks) *item = I_wooden_axe;
            else if (first_item == I_cobblestone) *item = I_stone_axe;
            else if (first_item == I_iron_ingot) *item = I_iron_axe;
            else if (first_item == I_gold_ingot) *item = I_golden_axe;
            else if (first_item == I_diamond) *item = I_diamond_axe;
            else if (first_item == I_netherite_ingot) *item = I_netherite_axe;
            *count = 1;
            return;
          }
          break;

        default: break;
      }
      break;

    default: break;

  }

  *count = 0;
  *item = 0;

}
