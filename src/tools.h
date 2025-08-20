#ifndef H_TOOLS
#define H_TOOLS

#include <stdlib.h>
#include <unistd.h>

#include "globals.h"

ssize_t recv_all (int client_fd, void *buf, size_t n, uint8_t require_first);
ssize_t send_all (int fd, const void *buf, size_t len);

ssize_t writeByte (int client_fd, uint8_t byte);
ssize_t writeUint16 (int client_fd, uint16_t num);
ssize_t writeUint32 (int client_fd, uint32_t num);
ssize_t writeUint64 (int client_fd, uint64_t num);
ssize_t writeFloat (int client_fd, float num);
ssize_t writeDouble (int client_fd, double num);

uint8_t readByte (int client_fd);
uint16_t readUint16 (int client_fd);
uint32_t readUint32 (int client_fd);
uint64_t readUint64 (int client_fd);
int64_t readInt64 (int client_fd);
float readFloat (int client_fd);
double readDouble (int client_fd);

void readString (int client_fd);

uint32_t fast_rand ();
uint64_t splitmix64 (uint64_t state);

extern int client_states[MAX_PLAYERS * 2];

void setClientState (int client_fd, int new_state);
int getClientState (int client_fd);
int getClientIndex (int client_fd);

int reservePlayerData (int client_fd, uint8_t *uuid, char* name);
int getPlayerData (int client_fd, PlayerData **output);
void clearPlayerFD (int client_fd);
int givePlayerItem (PlayerData *player, uint16_t item, uint8_t count);

uint8_t serverSlotToClientSlot (int window_id, uint8_t slot);
uint8_t clientSlotToServerSlot (int window_id, uint8_t slot);

uint8_t getBlockChange (short x, uint8_t y, short z);
void makeBlockChange (short x, uint8_t y, short z, uint8_t block);

uint16_t getMiningResult (int client_fd, uint8_t block);

#endif
