#ifndef H_PACKETS
#define H_PACKETS

int cs_handshake (int client_fd);
int cs_loginStart (int client_fd);
int cs_clientInformation (int client_fd);
int cs_pluginMessage (int client_fd);
int cs_playerAction (int client_fd);
int cs_useItemOn (int client_fd);
int cs_setPlayerPositionAndRotation (int client_fd, double *x, double *y, double *z, float *yaw, float *pitch);
int cs_setPlayerPosition (int client_fd, double *x, double *y, double *z);
int cs_setHeldItem (int client_fd);
int cs_clickContainer (int client_fd);
int cs_closeContainer (int client_fd);

int sc_loginSuccess (int client_fd, char *name, char *uuid);
int sc_knownPacks (int client_fd);
int sc_finishConfiguration (int client_fd);
int sc_loginPlay (int client_fd);
int sc_synchronizePlayerPosition (int client_fd, double x, double y, double z, float yaw, float pitch);
int sc_setDefaultSpawnPosition (int client_fd, int64_t x, int64_t y, int64_t z);
int sc_startWaitingForChunks (int client_fd);
int sc_playerAbilities (int client_fd, uint8_t flags);
int sc_updateTime (int client_fd, uint64_t ticks);
int sc_setCenterChunk (int client_fd, int x, int y);
int sc_chunkDataAndUpdateLight (int client_fd, int _x, int _z);
int sc_keepAlive (int client_fd);
int sc_setContainerSlot (int client_fd, int window_id, uint16_t slot, uint8_t count, uint16_t item);
int sc_setHeldItem (int client_fd, uint8_t slot);
int sc_blockUpdate (int client_fd, int64_t x, int64_t y, int64_t z, uint8_t block);
int sc_openScreen (int client_fd, uint8_t window, const char *title, uint16_t length);
int sc_registries(int client_fd);

#endif
