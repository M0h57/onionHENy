/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Jailbreak FIFO watcher + util watchdog — extracted from commands.cpp.
 */

#include "daemon_ops.hpp"
#include "hijacker.hpp"
#include "globalconf.hpp"

#include <hijacker/hijacker.hpp>
#include <orion/proc_query.h>
#include <orion/platform.h>
#include <orion/ready.h>
#include <elfldr_remote.h>
#include "../../extern/cJSON/orion_cjson.hpp"

#include <atomic>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_set>

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern orion::Settings g_settings;
extern bool is_800;

namespace {

pthread_mutex_t jb_lock = PTHREAD_MUTEX_INITIALIZER;

bool is_whitelisted_app(const std::string &tid) {
  static const std::unordered_set<std::string> whitelist = {
      "NPXS39041",
      "PKGI13337",
      "PKGI12345",
      "TOOL00001",
  };
  if (whitelist.find(tid) != whitelist.end())
    return true;
  if (tid.find("LAPY") != std::string::npos)
    return true;
  return false;
}

} // namespace

void *fifo_and_dumper_thread(void *args) noexcept {
  (void)args;
  char *json_str = nullptr;
  std::string tid, sandbox_dir_base;
  int retries = 0;
  bool fifo_found = false;

  constexpr int kMaxRetries = 5;

  while (true) {
    std::string sandbox_dir;

    // Restart util if it crashes or exits
    if (find_pid("util.elf") < 0 && find_pid("OrionHEN Utility") < 0 &&
        retries < kMaxRetries) {
      if (retries == 0)
        orion_notify(true, "OrionHEN Utility is not running, restarting via 9021...");

      if (++retries >= kMaxRetries) {
        orion_notify(true,
                     "OrionHEN Utility services failed to restart — check elfldr "
                     ":9021 and /data/OrionHEN/daemons/util.elf");
        continue;
      }

      bool ok = elfldr_remote_send_file_uri("/data/OrionHEN/daemons/util.elf");
      if (ok) {
        sleep(2);
        OrionHEN_log("  Launched util via 9021!");
        orion_notify(true, "OrionHEN Utility services successfully restarted");
        retries = 0;
      } else {
        OrionHEN_log("failed to launch util via 9021 (need elfldr + util.elf), retry: %d",
                     retries);
      }
    }

    pthread_mutex_lock(&jb_lock);

    if (g_settings.enable_fan_speed)
      set_fan_threshold(g_settings.fan_threshold);

    int bappid = 0;
    if (!Get_Running_App_TID(tid, bappid)) {
      pthread_mutex_unlock(&jb_lock);
      continue;
    }

    if (orion_ready_is_set(ORION_FLAG_FPS_OVERLAY) &&
        (tid.rfind("CUSA") != std::string::npos ||
         tid.rfind("SCUS") != std::string::npos)) {
      if (is_800)
        cmd_enable_fps_new(bappid);
      else
        cmd_enable_fps(bappid);
    }

    if (!is_whitelisted_app(tid)) {
      pthread_mutex_unlock(&jb_lock);
      continue;
    }

    sandbox_dir_base = "/mnt/sandbox/" + tid + "_";
    fifo_found = false;

    for (int i = 0; i <= 50; ++i) {
      std::ostringstream oss;
      oss << std::setw(3) << std::setfill('0') << i;
      sandbox_dir = sandbox_dir_base + oss.str() + "/download0/orionhen_jailbreak";
      if (if_exists(sandbox_dir.c_str())) {
        fifo_found = true;
        break;
      }
    }

    if (!fifo_found) {
      pthread_mutex_unlock(&jb_lock);
      continue;
    }

    if (!GetFileContents(sandbox_dir.c_str(), &json_str)) {
      OrionHEN_log("Failed to get command from %s", sandbox_dir.c_str());
      pthread_mutex_unlock(&jb_lock);
      continue;
    }

    OrionHEN_log("\nfound. %s for %s", json_str, tid.c_str());
    orion_cjson::Root my_json(json_str);
    if (!my_json) {
      puts("Error parsing JSON");
      OrionHEN_log("Error parsing JSON");
      pthread_mutex_unlock(&jb_lock);
      continue;
    }

    const char *PID = orion_cjson::string_item(my_json.get(), "PID");
    if (!PID) {
      OrionHEN_log("PID is null");
      orion_notify(true, "Jailbreak failed, PID is null");
      pthread_mutex_unlock(&jb_lock);
      continue;
    }

    int reserved_value = atoi(PID);
    OrionHEN_log("reserved_value: %d", reserved_value);

    int hijack_retries = 0;
    UniquePtr<Hijacker> spawned = nullptr;
    do {
      spawned = Hijacker::getHijacker(reserved_value);
      if (!spawned) {
        if (++hijack_retries > 30 || isProcessAlive(reserved_value)) {
          orion_notify(true, "Jailbreak failed, PID is invaild");
          OrionHEN_log("Jailbreak failed, PID is invaild");
          break;
        }
      }
      OrionHEN_log("is null for PID %d", reserved_value);
    } while (spawned == nullptr);

    if (spawned) {
      OrionHEN_log("RIGHT Jailbreak command received: jailbreaking...");
      if (g_settings.debug_app_jb_msg)
        orion_notify(true, "App (PID %i) has been granted a jailbreak", reserved_value);

      spawned->jailbreak(true);
      spawned.release();
      unlink(sandbox_dir.c_str());
    }

    free(json_str);
    json_str = nullptr;
    pthread_mutex_unlock(&jb_lock);
  }

  return nullptr;
}
