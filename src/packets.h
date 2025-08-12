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

int sc_loginSuccess (int client_fd, char *name, char *uuid);
int sc_knownPacks (int client_fd);
int sc_finishConfiguration (int client_fd);
int sc_loginPlay (int client_fd);
int sc_synchronizePlayerPosition (int client_fd, double x, double y, double z, float yaw, float pitch);
int sc_setDefaultSpawnPosition (int client_fd, long x, long y, long z);
int sc_startWaitingForChunks (int client_fd);
int sc_setCenterChunk (int client_fd, int x, int y);
int sc_chunkDataAndUpdateLight (int client_fd, int _x, int _z);
int sc_keepAlive (int client_fd);
int sc_setContainerSlot (int client_fd, int container, uint16_t slot, uint8_t count, uint8_t item);
int sc_registries(int client_fd);

#endif
