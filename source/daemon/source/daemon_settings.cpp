/* Copyright (C) 2025 OrionHEN / LightningMods */

#include "daemon_ops.hpp"
#include "globalconf.hpp"
#include <orion/platform.h>
#include <orion/settings.hpp>
#include <sys/stat.h>



#include <sys/stat.h>

struct ConfigState {
  time_t last_modified = 0;
};

ConfigState config_state;

void LoadSettings() {
  struct stat file_stat {};
  const char *paths[] = {orion::kConfigPathPrimary, orion::kConfigPathShellui};

  // Prefer primary; fall back to shellui path for mtime / create.
  const char *config_path = nullptr;
  for (const char *p : paths) {
    if (stat(p, &file_stat) == 0) {
      config_path = p;
      break;
    }
  }

  if (!config_path) {
    OrionHEN_log("[Daemon] Config file not found. Creating default schema...");
    if (orion::settings_ensure_default()) {
      orion_notify(true, "OrionHEN config created! @ /data/OrionHEN/config.ini");
      config_state.last_modified = 0;
    }
    // Apply defaults even if create failed.
    orion::Settings s{};
    orion::settings_load(&s);
    g_settings = s;
    return;
  }

  // Only reload if file has been modified since last load
  if (file_stat.st_mtime <= config_state.last_modified) {
    return;
  }

  OrionHEN_log("[Daemon] Loading Settings from shared schema...");
  orion::Settings s{};
  if (!orion::settings_load(&s)) {
    orion_notify(true, "Failed to Read the Settings file");
    return;
  }

  OrionHEN_log("[Daemon] Reading Settings from %s",
               orion::settings_last_loaded_path());
  OrionHEN_log("fan_threshold: %d", s.fan_threshold);
  OrionHEN_log("enable_fan_speed: %d", s.enable_fan_speed ? 1 : 0);

  g_settings = s;

  config_state.last_modified = file_stat.st_mtime;
}
