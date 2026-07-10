/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Process-local settings store + dual-path mtime reload gate.
 */

#include "daemon_ops.hpp"
#include "globalconf.hpp"
#include <orion/platform.h>
#include <orion/settings.hpp>

namespace {

struct ConfigState {
  /** Max mtime of twin paths at last successful store update. */
  time_t last_modified = 0;
  bool ever_loaded = false;
};

ConfigState config_state;

} // namespace

orion::SettingsStore g_settings;

bool LoadSettings() {
  const time_t newest = orion::settings_config_newest_mtime();

  // Skip disk I/O when neither twin is newer than the last applied snapshot.
  if (config_state.ever_loaded && !orion::settings_config_is_newer_than(
                                      config_state.last_modified)) {
    return true;
  }

  if (newest == 0) {
    OrionHEN_log("[Daemon] Config file not found. Creating default schema...");
    if (orion::settings_ensure_default()) {
      orion_notify(true, "OrionHEN config created! @ /data/OrionHEN/config.ini");
    }
  }

  OrionHEN_log("[Daemon] Loading Settings from shared schema...");
  orion::Settings s{};
  const bool from_file = orion::settings_load(&s);
  if (!from_file && newest != 0) {
    orion_notify(true, "Failed to Read the Settings file");
    return false;
  }

  if (from_file) {
    OrionHEN_log("[Daemon] Reading Settings from %s",
                 orion::settings_last_loaded_path());
  } else {
    OrionHEN_log("[Daemon] Using default settings (no config file)");
  }
  OrionHEN_log("fan_threshold: %d", s.fan_threshold);
  OrionHEN_log("enable_fan_speed: %d", s.enable_fan_speed ? 1 : 0);

  g_settings.store(s);
  config_state.last_modified = orion::settings_config_newest_mtime();
  config_state.ever_loaded = true;
  return true;
}

void SettingsNoteDiskWritten() {
  config_state.last_modified = orion::settings_config_newest_mtime();
  config_state.ever_loaded = true;
}
