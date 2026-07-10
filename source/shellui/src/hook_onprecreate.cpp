/* Copyright (C) 2025 OrionHEN / LightningMods
 * Extracted from HookFunctions.cpp — hook_onprecreate
 *
 * Value binding is table-driven: each known Id maps to a provider string.
 */
#include "HookedFuncs.hpp"
#include "RemotePlay.h"
#include "Detour.h"
#include "ipc.hpp"
#include <msg.hpp>
#include <pthread.h>
#include <sys/stat.h>
#include <fstream>
#include <unistd.h>
#include <vector>
#include <atomic>
#include <cstring>
#include <algorithm>
#include <string>

#include "shellui_state.hpp"

extern int (*oOnPreCreate)(MonoObject* Instance, MonoObject* element);
extern MonoMethod* set_value_method;

std::string GetPropertyValue(MonoObject* element, const char* propertyName);
bool if_exists(const char* path);
void ParseCheatID(const char* id, char* tid, int* cheat_id);

namespace {

std::string bool_str(bool v) { return v ? "1" : "0"; }
std::string int_str(int v) { return std::to_string(v); }

/** Exact-Id value providers (no capture; touch only globals / pure). */
struct ExactValueEntry {
  const char* id;
  std::string (*get)();
};

const ExactValueEntry kExactValues[] = {
    {"id_lm_test", +[]() -> std::string { return "0"; }},
    {"id_overlay_gpu",
     +[]() -> std::string { return bool_str(g_settings.overlay_gpu); }},
    {"id_overlay_fps",
     +[]() -> std::string { return bool_str(g_settings.overlay_fps); }},
    {"id_overlay_ip",
     +[]() -> std::string { return bool_str(g_settings.overlay_ip); }},
    {"id_all_cpu_usage", +[]() -> std::string { return bool_str(g_ui.all_cpu_usage); }},
    {"id_overlay_cpu",
     +[]() -> std::string { return bool_str(g_settings.overlay_cpu); }},
    {"id_overlay_ram",
     +[]() -> std::string { return bool_str(g_settings.overlay_ram); }},
    {"id_kstuff_autoload",
     +[]() -> std::string {
       return bool_str(!if_exists("/user/data/OrionHEN/no_kstuff"));
     }},
    {"id_disp_titleids",
     +[]() -> std::string { return bool_str(g_settings.display_tids); }},
    {"id_enable_fan_speed",
     +[]() -> std::string { return bool_str(g_settings.enable_fan_speed); }},
    // Historical: int used as bool (0 → "0", non-zero → "1")
    {"id_selected_cheats_repo",
     +[]() -> std::string {
       return bool_str(g_settings.selected_cheats_repo != 0);
     }},
    {"id_start_opt",
     +[]() -> std::string { return int_str(g_settings.start_option); }},
    {"id_rest_1",
     +[]() -> std::string {
       return int_str(static_cast<int>(g_settings.rest_mode_delay_seconds));
     }},
    {"id_fan_speed",
     +[]() -> std::string { return int_str(g_settings.fan_threshold); }},
    {"id_rest_2",
     +[]() -> std::string { return bool_str(g_settings.util_rest_kill); }},
    {"id_rest_3",
     +[]() -> std::string { return bool_str(g_settings.game_rest_kill); }},
    {"id_rest_4",
     +[]() -> std::string {
       return bool_str(g_settings.disable_toolbox_auto_start_for_rest_mode);
     }},
    {"id_cheats_shortcut",
     +[]() -> std::string { return int_str(g_settings.cheats_shortcut_opt); }},
    {"id_toolbox_auto_start",
     +[]() -> std::string { return bool_str(g_settings.toolbox_auto_start); }},
    {"id_debug_jb",
     +[]() -> std::string { return bool_str(g_settings.debug_app_jb_msg); }},
    {"id_debug_legacy_cmd",
     +[]() -> std::string { return bool_str(g_settings.legacy_cmd_server); }},
    {"id_custom_game_opts",
     +[]() -> std::string { return bool_str(g_settings.orionhen_game_opts); }},
    {"id_auto_eject",
     +[]() -> std::string { return bool_str(g_settings.auto_eject_disc); }},
    {"id_overlay_change_pos",
     +[]() -> std::string { return int_str(g_settings.overlay_pos); }},
};

bool ensure_set_value_method() {
  if (set_value_method)
    return true;

  MonoAssembly* Legacy_assembly =
      mono_domain_assembly_open(Root_Domain, legacy_dec.c_str());
  if (!Legacy_assembly) {
    shellui_log("Failed to open assembly.");
    return false;
  }

  MonoImage* leg_img = mono_assembly_get_image(Legacy_assembly);
  if (!leg_img) {
    shellui_log("Failed to get image.");
    return false;
  }

  MonoClass* klass =
      mono_class_from_name(leg_img, UI3_dec.c_str(), "SettingElement");
  if (!klass) {
    sceKernelDebugOutText(0, "Failed to find class\n");
    return false;
  }

  MonoProperty* s_Property = mono_class_get_property_from_name(klass, "Value");
  if (!s_Property) {
    shellui_log("Failed to find property");
    return false;
  }

  set_value_method = mono_property_get_set_method(s_Property);
  if (!set_value_method) {
    shellui_log("Failed to find set method");
    return false;
  }
  return true;
}

/** Lookup exact table; returns empty optional if not found. */
bool try_exact_value(const std::string& id, std::string& out) {
  for (const auto& e : kExactValues) {
    if (id == e.id) {
      out = e.get();
      return true;
    }
  }
  return false;
}

bool try_plugin_list_value(const std::string& id, std::string& out) {
  for (const auto& plugin : g_ui.plugins_list) {
    if (plugin.id != id)
      continue;
    out = bool_str(sceSystemServiceGetAppId(plugin.tid.c_str()) > 0);
    return true;
  }
  return false;
}

bool try_auto_plugin_value(const std::string& id, std::string& out) {
  for (const auto& plugin : g_ui.auto_list) {
    if (plugin.id != id)
      continue;
    const std::string auto_path = plugin.shellui_path + ".auto_start";
    out = bool_str(if_exists(auto_path.c_str()));
    return true;
  }
  return false;
}

bool try_cheat_value(const std::string& id, std::string& out) {
  // Historical match: substring "id_cheat_" anywhere in id.
  if (id.find("id_cheat_") == std::string::npos)
    return false;
  if (!g_ui.is_current_game_open)
    return false;

  char tid[32] = {};
  int cheat_id = 0;
  ParseCheatID(id.c_str(), tid, &cheat_id);
  out = bool_str(g_ui.get_cheat_enabled(cheat_id));
  return true;
}

bool try_toolbox_shortcut_value(const std::string& id, std::string& out) {
  if (id.find("id_toolbox_shortcut") == std::string::npos)
    return false;
  out = int_str(g_settings.toolbox_shortcut_opt);
  return true;
}

std::string resolve_element_value(const std::string& id) {
  std::string value;

  // Dynamic lists first (id assigned at generation time)
  if (try_plugin_list_value(id, value))
    return value;
  if (try_auto_plugin_value(id, value))
    return value;

  if (try_exact_value(id, value))
    return value;

  if (try_cheat_value(id, value))
    return value;
  if (try_toolbox_shortcut_value(id, value))
    return value;

  return {};
}

} // namespace

int OnPreCreate_Hook(MonoObject* Instance, MonoObject* element) {
  if (!Instance || !element) {
#if SHELL_DEBUG == 1
    shellui_log("[LM HOOK] OnPreCreate_Hook: args are null");
#endif
    return oOnPreCreate(Instance, element);
  }

  if (!ensure_set_value_method())
    return -1;

  const std::string id = GetPropertyValue(element, "Id");
  const std::string value = resolve_element_value(id);

  if (!value.empty()) {
    MonoString* text = mono_string_new(Root_Domain, value.c_str());
    mono_runtime_invoke(set_value_method, element, reinterpret_cast<void**>(&text),
                        nullptr);
  }

  return oOnPreCreate(Instance, element);
}
