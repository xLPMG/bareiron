#ifndef SERIALIZE_H
#define SERIALIZE_H

#include "globals.h"

#if defined(SYNC_WORLD_TO_DISK) && !defined(ESP_PLATFORM)
  int initSerializer ();
  void writeBlockChangesToDisk (int from, int to);
  void writeChestChangesToDisk (uint8_t *storage_ptr, uint8_t slot);
  void writePlayerDataToDisk ();
#else
  // Define no-op placeholders for when disk syncing isn't enabled
  #define writeBlockChangesToDisk(a, b)
  #define writeChestChangesToDisk(a, b)
  #define writePlayerDataToDisk()
  #define initSerializer() 0
#endif

#endif
