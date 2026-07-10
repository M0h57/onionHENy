/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Playtime tracking thread — extracted from commands.cpp.
 */

#include "daemon_ops.hpp"

#include <orion/platform.h>
#include <orion/playtime.h>

#include <unistd.h>

void *Play_time_thread(void *args) noexcept {
  (void)args;
  const char *filename = "/data/OrionHEN/playtime.bin";
  std::string tid;
  uint64_t duration = 0;
  int appid = 0;

  while (true) {
    if (!Get_Running_App_TID(tid, appid))
      continue;

    OrionHEN_log("getting duration for %s", tid.c_str());
    if (!orion_playtime_get_duration(filename, tid.c_str(), &duration))
      continue;

    OrionHEN_log("got duration for %s: %llu", tid.c_str(),
                 (unsigned long long)duration);
    duration++;
    if (!orion_playtime_modify_duration(filename, tid.c_str(), duration)) {
      OrionHEN_log("Failed to modify record duration for %s", tid.c_str());
      continue;
    }

    OrionHEN_log("Record duration for %s changed to %llu", tid.c_str(),
                 (unsigned long long)duration);
    sleep(59);
  }

  return nullptr;
}
