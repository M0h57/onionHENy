/* Copyright (C) 2025 OnionHEN / LightningMods
 *
 * Process-local settings store + dual-path mtime reload gate.
 */

#include "daemon_ops.hpp"
#include "globalconf.hpp"
#include <onion/platform.h>
#include <onion/notify_i18n.h>
#include <onion/settings.hpp>

namespace {

struct ConfigState {
  /** Max mtime of twin paths at last successful store update. */
  time_t last_modified = 0;
  bool ever_loaded = false;
};

ConfigState config_state;

} // namespace

onion::SettingsStore g_settings;

bool LoadSettings() {
  const time_t newest = onion::settings_config_newest_mtime();

  // Skip disk I/O when neither twin is newer than the last applied snapshot.
  if (config_state.ever_loaded && !onion::settings_config_is_newer_than(
                                      config_state.last_modified)) {
    return true;
  }

  if (newest == 0) {
    OnionHEN_log("[Daemon] Config file not found. Creating default schema...");
    if (onion::settings_ensure_default()) {
      onion_notify(true, "OnionHEN config created! @ /data/OnionHEN/config.ini");
    }
  }

  OnionHEN_log("[Daemon] Loading Settings from shared schema...");
  onion::Settings s{};
  const bool from_file = onion::settings_load(&s);
  if (!from_file && newest != 0) {
    onion_notify(true, "Failed to Read the Settings file");
    return false;
  }

  if (from_file) {
    OnionHEN_log("[Daemon] Reading Settings from %s",
                 onion::settings_last_loaded_path());
  } else {
    OnionHEN_log("[Daemon] Using default settings (no config file)");
  }
  OnionHEN_log("fan_threshold: %d", s.fan_threshold);
  OnionHEN_log("enable_fan_speed: %d", s.enable_fan_speed ? 1 : 0);

  g_settings.store(s);
  onion_notify_apply_ui_language_cached(s.ui_lang);
  config_state.last_modified = onion::settings_config_newest_mtime();
  config_state.ever_loaded = true;
  return true;
}

void SettingsNoteDiskWritten() {
  config_state.last_modified = onion::settings_config_newest_mtime();
  config_state.ever_loaded = true;
}
