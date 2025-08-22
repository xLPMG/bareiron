#ifndef H_PROCEDURES
#define H_PROCEDURES

#include <stdlib.h>
#include <unistd.h>

#include "globals.h"

extern int client_states[MAX_PLAYERS * 2];

void setClientState (int client_fd, int new_state);
int getClientState (int client_fd);
int getClientIndex (int client_fd);

void resetPlayerData (PlayerData *player);
int reservePlayerData (int client_fd, uint8_t *uuid, char* name);
int getPlayerData (int client_fd, PlayerData **output);
void clearPlayerFD (int client_fd);
int givePlayerItem (PlayerData *player, uint16_t item, uint8_t count);
void spawnPlayer (PlayerData *player);

uint8_t serverSlotToClientSlot (int window_id, uint8_t slot);
uint8_t clientSlotToServerSlot (int window_id, uint8_t slot);

uint8_t getBlockChange (short x, uint8_t y, short z);
void makeBlockChange (short x, uint8_t y, short z, uint8_t block);

uint8_t isInstantlyMined (PlayerData *player, uint8_t block);
uint8_t isColumnBlock (uint8_t block);
uint16_t getMiningResult (uint16_t held_item, uint8_t block);
void bumpToolDurability (PlayerData *player);
void handlePlayerAction (PlayerData *player, int action, short x, short y, short z);

void spawnMob (uint8_t type, short x, uint8_t y, short z, uint8_t health);
void hurtEntity (int entity_id, int attacker_id, uint8_t damage_type, uint8_t damage);
void handleServerTick (int64_t time_since_last_tick);

#endif
