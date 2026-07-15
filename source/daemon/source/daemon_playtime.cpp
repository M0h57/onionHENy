/* Copyright (C) 2025 OnionHEN / LightningMods
 *
 * Playtime tracking thread — extracted from commands.cpp.
 */

#include "daemon_ops.hpp"

#include <onion/platform.h>
#include <onion/playtime.h>

#include <unistd.h>

void *Play_time_thread(void *args) noexcept {
  (void)args;
  const char *filename = "/data/OnionHEN/playtime.bin";
  std::string tid;
  uint64_t duration = 0;
  int appid = 0;

  while (true) {
    if (!Get_Running_App_TID(tid, appid))
      continue;

    OnionHEN_log("getting duration for %s", tid.c_str());
    if (!onion_playtime_get_duration(filename, tid.c_str(), &duration))
      continue;

    OnionHEN_log("got duration for %s: %llu", tid.c_str(),
                 (unsigned long long)duration);
    duration++;
    if (!onion_playtime_modify_duration(filename, tid.c_str(), duration)) {
      OnionHEN_log("Failed to modify record duration for %s", tid.c_str());
      continue;
    }

    OnionHEN_log("Record duration for %s changed to %llu", tid.c_str(),
                 (unsigned long long)duration);
    sleep(59);
  }

  return nullptr;
}
