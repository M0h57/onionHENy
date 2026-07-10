/* Copyright (C) 2025 OrionHEN / LightningMods — P0 split. */

#include "HookedFuncs.hpp"
#include "ipc.hpp"
#include "external_symbols.hpp"
#include "shellui_state.hpp"
#include "toolbox_route.hpp"
#include <orion/platform.h>
#include <string>

extern MonoClass *MemoryStream_IO;
extern MonoObject *MemoryStream_Instance;
extern std::string plugin_xml, remote_play_xml, debug_settings_xml, cheats_xml;
extern std::string dec_xml_str, UI3_dec, legacy_dec;
void generate_plugin_xml(std::string &xml_buffer, bool plugins_xml);
void generate_remote_play_xml(std::string &xml_buffer);
void generate_plapps_xml(std::string &new_xml);
void generate_cheats_xml(std::string &new_xml, std::string &not_open_tid,
                         bool running_as_debug_settings,
                         bool show_while_not_open);

uint64_t GetManifestResourceStream_Hook(uint64_t inst, MonoString *FileName) {
  std::string new_xml_string;
  std::string resourceName = Mono_to_String(FileName);

#if SHELL_DEBUG == 1
  shellui_log("GetManifestResourceStream_Hook: %s", resourceName.c_str());
#endif

  const bool shortcut = g_ui.any_cheat_shortcut();
  const bool shortcut_not_open = g_ui.cheats_shortcut_activated_not_open;

  toolbox::RouteResult route = toolbox::resolve_resource({
      .resource = resourceName,
      .names =
          {
              .plugin_xml = plugin_xml,
              .debug_settings_xml = debug_settings_xml,
              .cheats_xml = cheats_xml,
              .remote_play_xml = remote_play_xml,
          },
      .cheats_shortcut = g_ui.cheats_shortcut_activated,
      .cheats_shortcut_not_open = g_ui.cheats_shortcut_activated_not_open,
  });

  g_ui.apply_route_flags(route.flags);

  if (route.page == toolbox::Page::RedirectOgDebug) {
    return GetManifestResourceStream_Original(
        inst, mono_string_new(Root_Domain, debug_settings_xml.c_str()));
  }

  if (route.page == toolbox::Page::None ||
      route.page == toolbox::Page::SuperuserPass) {
    return GetManifestResourceStream_Original(inst, FileName);
  }

  if (!MemoryStream_IO) {
    MonoAssembly *Assembly = mono_domain_assembly_open(
        Root_Domain, "/system_ex/common_ex/lib/mscorlib.dll");
    MonoImage *mscorelib_image = mono_assembly_get_image(Assembly);
    if (!mscorelib_image) {
      shellui_log("Failed to get mscorelib image");
      return GetManifestResourceStream_Original(inst, FileName);
    }

    MemoryStream_IO =
        mono_class_from_name(mscorelib_image, "System.IO", "MemoryStream");
    if (!MemoryStream_IO) {
      shellui_log("Failed to open class MemoryStream");
      return GetManifestResourceStream_Original(inst, FileName);
    }
  }

  switch (route.page) {
  case toolbox::Page::DebugSettings:
    LoadSettings();
    new_xml_string = dec_xml_str;
    break;
  case toolbox::Page::Plugins:
    g_ui.plugins_list.clear();
    generate_plugin_xml(new_xml_string, true);
    break;
  case toolbox::Page::Cheats:
    generate_cheats_xml(new_xml_string, g_ui.current_menu_tid, shortcut,
                        shortcut_not_open);
    if (route.clear_cheat_shortcuts_after)
      g_ui.clear_cheat_shortcuts();
    break;
  case toolbox::Page::AutoPlugins:
    g_ui.auto_list.clear();
    generate_plugin_xml(new_xml_string, false);
    break;
  case toolbox::Page::RemotePlay:
    generate_remote_play_xml(new_xml_string);
    break;
  case toolbox::Page::Plapps:
    g_ui.payloads_apps_list.clear();
    generate_plapps_xml(new_xml_string);
    break;
  default:
    return GetManifestResourceStream_Original(inst, FileName);
  }

  MemoryStream_Instance = New_Mono_XML_From_String(new_xml_string);
  if (!MemoryStream_Instance) {
    return GetManifestResourceStream_Original(inst, FileName);
  }

  return (uint64_t)MemoryStream_Instance;
}
