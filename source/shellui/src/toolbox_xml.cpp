/* Copyright (C) 2025 OnionHEN / LightningMods
 *
 * Extracted from MonoUtils.cpp for module locality.
 * Dynamic settings pages are built via ps5ui::Page (fluent XML builder).
 */

#include "HookedFuncs.hpp"
#include "RemotePlay.h"
#include "defs.h"
#include "external_symbols.hpp"
#include "ipc.hpp"
#include "ps5_settings_ui.hpp"
#include "toolbox_i18n.hpp"
#include "toolbox_values.hpp"
#include "../../extern/cJSON/onion_cjson.hpp"

#define PIN_CODE_SIZE 30
#define ACCOUNT_ID_BASE64_SIZE 16

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <vector>
#include <string>

#include "shellui_state.hpp"
#include "toolbox_helpers.hpp"

int usbpath();
void escapeXML(std::string& input);
bool Get_Running_App_TID(std::string& title_id, int& BigAppid);

void escapeXML(std::string& input) {
  input = ps5ui::escape(input);
}

namespace {

/** Payload .elf only (OnionHEN no longer supports .plugin packages). */
template <typename G>
void append_payload_entry(G& page, const std::string& directory, const char* filename,
                          bool list_page, int& next_id) {
  if (!toolbox::is_payload_elf_name(filename))
    return;

  const std::string path = directory + "/" + filename;
  char elf_key[64] = {};
  if (!toolbox::elf_key_from_name(filename, elf_key, sizeof(elf_key))) {
    shellui_log("Skipping invalid payload name: %s", filename);
    return;
  }

  /* Confirm file is readable (ELF magic checked at launch). */
  const int fd = open(path.c_str(), O_RDONLY, 0);
  if (fd < 0) {
    shellui_log("Failed to open payload: %s", path.c_str());
    return;
  }
  close(fd);

  shellui_log("Found payload: %s key=%s", path.c_str(), elf_key);

  const std::string shown_path = toolbox::display_path_for_ui(path);
  const std::string id_prefix = list_page ? "id_payload_" : "id_auto_payload_";
  const std::string id = id_prefix + std::to_string(next_id++);

  std::string second;
  if (list_page) {
    second = std::string(toolbox_i18n::tr("payload.start_stop")) + filename +
             toolbox_i18n::tr("payload.path") + shown_path + ") (" + elf_key +
             ")";
  } else {
    second = std::string(toolbox_i18n::tr("payload.autostart_enable")) +
             filename + toolbox_i18n::tr("payload.autostart_suffix") +
             shown_path + ")";
  }

  page.toggle(id, filename, /*on=*/false, second);

  PayloadEntry entry;
  entry.shellui_path = path;
  entry.tid = elf_key;
  entry.path = shown_path;
  entry.name = filename;
  entry.version = "";
  entry.id = id;
  if (list_page)
    g_ui.payloads_list.push_back(entry);
  else
    g_ui.auto_payloads_list.push_back(entry);
}

template <typename G>
void append_homebrew_game(G& page, const std::string& game_dir, const char* dir_name,
                          int random_num) {
  const std::string elf_path = game_dir + "/eboot.elf";
  if (access(elf_path.c_str(), F_OK) != 0)
    return;

#if SHELL_DEBUG == 1
  shellui_log("Found Game: %s", game_dir.c_str());
#endif

  std::string title_id, title, ver;
  const std::string shown_path = toolbox::display_path_for_ui(game_dir);
  const std::string icon_path = game_dir + "/sce_sys/icon0.png";

  GameEntry game;
  game.tid = title_id;
  game.title = title;
  game.version = ver;
  game.path = shown_path;
  game.dir_name = dir_name;
  game.icon_path = icon_path;
  game.id = "id_onionhen_pl_loader_" + title_id + "_" + std::to_string(random_num);
  g_ui.games_list.push_back(game);

  page.button(game.id, "(" + title_id + ") " + title,
              shown_path + toolbox_i18n::tr("plapps.version") + ver,
              std::nullopt, icon_path);
}

std::string read_file_to_string(const char* path) {
  struct stat st {};
  if (stat(path, &st) == -1 || st.st_size <= 0) {
    shellui_log("Unable to stat file %s", path);
    return {};
  }

  const int fd = open(path, O_RDONLY);
  if (fd < 0) {
    shellui_log("Error reading %s file!", path);
    return {};
  }

  std::string buf(static_cast<size_t>(st.st_size), '\0');
  const ssize_t n = read(fd, buf.data(), buf.size());
  close(fd);

  if (n < 0 || static_cast<size_t>(n) != buf.size()) {
    shellui_log("read failed for %s", path);
    return {};
  }
  return buf;
}

std::string join_authors(cJSON* root) {
  std::unordered_set<std::string> seen;
  std::string joined;
  bool first = true;
  cJSON* authors = onion_cjson::item(root, "authors");
  if (!cJSON_IsArray(authors))
    return joined;

  cJSON* author = nullptr;
  cJSON_ArrayForEach(author, authors) {
    const char* value = onion_cjson::string_value(author);
    if (!value || !seen.insert(value).second)
      continue;
    if (!first)
      joined += ", ";
    joined += value;
    first = false;
  }
  return joined;
}

template <typename G>
void append_cheat_entries(G& page, cJSON* root, const std::string& tid,
                          const std::string& game_name, bool can_toggle) {
  cJSON* cheats = onion_cjson::item(root, "cheats");
  if (!cJSON_IsArray(cheats))
    return;

  cJSON* entry = nullptr;
  cJSON_ArrayForEach(entry, cheats) {
    const std::string name = onion_cjson::string_item(entry, "name", "");
    std::string desc =
        onion_cjson::string_item(entry, "description", "");
    if (desc.empty())
      desc = toolbox_i18n::tr("cheats.on_off");
    const int id = onion_cjson::int_item(entry, "id");
    const bool enabled = onion_cjson::bool_item(entry, "enabled");
    const std::string id_attr = "id_cheat_" + tid + "_" + std::to_string(id);

    if (can_toggle) {
      page.toggle(id_attr, name, enabled, std::nullopt, desc, "tex_game_icon");
    } else {
      page.button(id_attr, name,
                  std::string(toolbox_i18n::tr("cheats.enable_for")) +
                      game_name + toolbox_i18n::tr("cheats.enable_mid") + name,
                  desc, "tex_game_icon");
    }
    g_ui.set_cheat_enabled(id, enabled);
  }
}

} // namespace

void generate_remote_play_xml(std::string& xml_buffer) {
  char pin_code[PIN_CODE_SIZE] = {0};
  char AccountID[ACCOUNT_ID_BASE64_SIZE] = {0};
  uint64_t dec_account_id = 0;
  bzero(AccountID, ACCOUNT_ID_BASE64_SIZE);

  shellui_log("Starting remote play");
  static bool remote_play_initialized = false;
  if (!remote_play_initialized) {
    InitRemotePlay();
    remote_play_initialized = true;
  }

  toolbox_i18n::apply_ui_lang(g_settings.ui_lang);
  ps5ui::Page page("remote_play_pin_display", toolbox_i18n::tr("rp.title"));
  page.root_style(ps5ui::Style::Center);

  if (IsNotActivated()) {
    GetEncodedAccountID(AccountID, dec_account_id);
    page.label("id_pin_2", toolbox_i18n::tr("rp.need_reboot"),
               ps5ui::Style::Center);
    xml_buffer = page.build();
    return;
  }

  shellui_log("Get encoded account id");
  GetEncodedAccountID(AccountID, dec_account_id);
  shellui_log("Get encoded account id ==> %s", AccountID);

  g_ui.remote_play_info =
      std::string(toolbox_i18n::tr("rp.account_id")) + AccountID;
  {
    std::stringstream ss;
    ss << std::hex << std::uppercase << dec_account_id;
    g_ui.remote_play_info +=
        std::string("\n") + toolbox_i18n::tr("rp.account_id_decoded") + ss.str();
  }

  const uint32_t pinCode = GeneratePINCode();
  shellui_log("Pin code => %d", pinCode);
  sprintf(pin_code, "%s%04d %04d    ", toolbox_i18n::tr("rp.pin"),
          pinCode / 10000, pinCode % 10000);
  g_ui.remote_play_info += "\n" + std::string(pin_code);
  shellui_log("Pin code str => %s", pin_code);

  page.label("id_pin", pin_code, ps5ui::Style::Center)
      .label("base64_account_id",
             std::string(toolbox_i18n::tr("rp.account_id")) + AccountID,
             ps5ui::Style::Center);

  if (usbpath() != -1)
    page.button("id_save_rp_info", toolbox_i18n::tr("rp.save_usb"), std::nullopt,
                std::nullopt, std::nullopt, ps5ui::Style::Center);

  xml_buffer = page.build();
}

void generate_payload_xml(std::string& xml_buffer, bool list_page) {
  static const std::vector<std::string> kPayloadDirs = {
      "/user/data/OnionHEN/payloads",
      "/data/OnionHEN/payloads",
      "/usb0/OnionHEN/payloads",
      "/usb1/OnionHEN/payloads",
      "/usb2/OnionHEN/payloads",
      "/usb3/OnionHEN/payloads",
  };

  toolbox_i18n::apply_ui_lang(g_settings.ui_lang);
  const char* root_id = list_page ? "id_payload" : "id_auto_payloads";
  const char* root_title =
      list_page ? toolbox_i18n::tr("payload.title")
                : toolbox_i18n::tr("payload.auto_title");
  ps5ui::Page page(root_id, root_title);

  int toggle_switch_id = 1;
  for (const auto& directory : kPayloadDirs) {
    DIR* dir = opendir(directory.c_str());
    if (!dir) {
      shellui_log("Failed to open directory: %s", directory.c_str());
      continue;
    }
    while (struct dirent* entry = readdir(dir))
      append_payload_entry(page, directory, entry->d_name, list_page,
                           toggle_switch_id);
    closedir(dir);
  }

  if (list_page) {
    page.link("id_auto_payloads", toolbox_i18n::tr("payload.auto.link"),
              "auto_payloads.xml", toolbox_i18n::tr("payload.auto.sub"));
  }

  xml_buffer = page.build();
}

void generate_cheats_xml(std::string& new_xml, std::string& not_open_tid,
                         bool running_as_debug_settings, bool show_while_not_open) {
  toolbox_i18n::apply_ui_lang(g_settings.ui_lang);
  const std::string list_id =
      running_as_debug_settings ? "id_debug_settings" : "id_cheat_title";

  int appid = -1;
  g_ui.is_game_open = Get_Running_App_TID(g_ui.running_tid, appid);
  g_ui.is_current_game_open =
      g_ui.is_game_open &&
      g_ui.running_tid == (show_while_not_open ? not_open_tid : g_ui.running_tid);

  if (!g_ui.is_game_open && !show_while_not_open) {
    ps5ui::Page page(list_id, toolbox_i18n::tr("cheats.none"));
    page.label("id_cheat_no_game", toolbox_i18n::tr("cheats.none.hint"),
               ps5ui::Style::Center);
    new_xml = page.build();
    return;
  }

  g_ui.running_tid = show_while_not_open ? not_open_tid : g_ui.running_tid;
  IPC_Client& client = IPC_Client::getInstance(true);

  std::string game_ver;
  if (!client.GameVerFromTid(g_ui.running_tid, game_ver))
    game_ver = toolbox_i18n::tr("cheats.ver_unknown");

  ps5ui::Page page(list_id, std::string(toolbox_i18n::tr("cheats.title_prefix")) +
                                g_ui.running_tid + " - " + game_ver);

  if (!g_ui.is_game_open && show_while_not_open) {
    page.label("id_cheat_disclaimer",
               g_ui.running_tid + toolbox_i18n::tr("cheats.not_running"),
               ps5ui::Style::Center);
  }

  std::string cheat_path;
  if (!client.GetGameCheats(g_ui.running_tid, game_ver, cheat_path)) {
    page.label("id_cheat_missing", toolbox_i18n::tr("cheats.missing"),
               ps5ui::Style::Center);
    new_xml = page.build();
    return;
  }

  const std::string json_string = read_file_to_string(cheat_path.c_str());
  if (json_string.empty()) {
    new_xml = page.build();
    return;
  }
  unlink(cheat_path.c_str());

  onion_cjson::Root res_json(json_string);
  if (!res_json) {
    shellui_log("Failed to parse json from cheat response!");
    new_xml = page.build();
    return;
  }

  const std::string game_name =
      onion_cjson::string_item(res_json.get(), "name", "");
  page.label("id_cheat_title", "★ " + game_name + " ★", ps5ui::Style::Center);

  const std::string authors = join_authors(res_json.get());
  page.label("credits",
             std::string(toolbox_i18n::tr("cheats.authors")) + authors,
             ps5ui::Style::Center);

  append_cheat_entries(page, res_json.get(), g_ui.running_tid, game_name,
                       g_ui.is_game_open && g_ui.is_current_game_open);
  new_xml = page.build();
}

void generate_plapps_xml(std::string& new_xml) {
  static const std::vector<std::string> kHomebrewDirs = {
      "/user/data/homebrew/games",
      "/usb0/homebrew",
      "/usb1/homebrew/games",
      "/usb2/homebrew/games",
      "/usb3/homebrew/games",
      "/mnt/ext1/homebrew/games",
      "/mnt/ext2/homebrew/games",
      "/mnt/ext0/homebrew/games",
  };

  toolbox_i18n::apply_ui_lang(g_settings.ui_lang);
  ps5ui::Page page("id_plapps", toolbox_i18n::tr("plapps.title"));

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(1000, 9999);

  for (const auto& directory : kHomebrewDirs) {
    DIR* dir = opendir(directory.c_str());
    if (!dir) {
#if SHELL_DEBUG == 1
      shellui_log("Failed to open directory: %s", directory.c_str());
#endif
      continue;
    }

    while (struct dirent* entry = readdir(dir)) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;

      const std::string game_dir = directory + "/" + entry->d_name;
      struct stat st {};
      if (stat(game_dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
#if SHELL_DEBUG == 1
        shellui_log("Skipping non-directory: %s", game_dir.c_str());
#endif
        continue;
      }

      append_homebrew_game(page, game_dir, entry->d_name, dist(gen));
    }
    closedir(dir);
  }

  new_xml = page.build();
}

namespace {

constexpr const char* kIconPkg =
    "/user/data/OnionHEN/assets/icon_xml_package.png";
constexpr const char* kIconPlugins =
    "/user/data/OnionHEN/assets/icon_xml_plugins.png";
constexpr const char* kIconGame = "/user/data/OnionHEN/assets/icon_xml_game.png";
constexpr const char* kIconSettings =
    "/user/data/OnionHEN/assets/icon_xml_settings.png";
constexpr const char* kIconShortcuts =
    "/user/data/OnionHEN/assets/icon_xml_shortcuts.png";
constexpr const char* kIconDebug =
    "/user/data/OnionHEN/assets/icon_xml_debug.png";
constexpr const char* kIconAbout =
    "/user/data/OnionHEN/assets/icon_xml_about.png";

bool toolbox_on(const char* id) {
  return resolve_toolbox_control_value(id) == "1";
}

std::string toolbox_val(const char* id, const char* fallback = "0") {
  std::string v = resolve_toolbox_control_value(id);
  return v.empty() ? fallback : v;
}

void append_toolbox_pkg_group(ps5ui::Group& g) {
  g.link("id_game_package_installer", toolbox_i18n::tr("pkg.installer"),
         "PkgInstaller/data/pkginstaller.xml")
      .link("id_game_add_content_manager", toolbox_i18n::tr("pkg.add_content"),
            "Addcontent/data/addcontent.xml");
}

void append_toolbox_payloads_group(ps5ui::Group& g) {
  g.link("id_payloads", toolbox_i18n::tr("payloads.link"), "payloads.xml")
      .group(
          "id_kstuff_opts", toolbox_i18n::tr("kstuff.group"),
          [](ps5ui::Group& k) {
            k.toggle("id_kstuff_autoload", toolbox_i18n::tr("kstuff.autoload"),
                     toolbox_on("id_kstuff_autoload"))
                .button("id_delete_kstuff", toolbox_i18n::tr("kstuff.delete"),
                        std::nullopt, toolbox_i18n::tr("kstuff.delete.desc"));
          },
          toolbox_i18n::tr("kstuff.group.sub"), std::nullopt,
          "id_kstuff_autoload");
}

void append_toolbox_game_group(ps5ui::Group& g) {
  g.link("id_cheats", toolbox_i18n::tr("cheats.link"), "cheats.xml")
      .link("remote_play", toolbox_i18n::tr("remote_play.link"),
            "remote_play.xml")
      .toggle("id_custom_game_opts", toolbox_i18n::tr("game_opts.toggle"),
              toolbox_on("id_custom_game_opts"),
              toolbox_i18n::tr("game_opts.toggle.sub"))
      .group(
          "id_overlay_opts", toolbox_i18n::tr("overlay.group"),
          [](ps5ui::Group& o) {
            o.list("id_overlay_change_pos", toolbox_i18n::tr("overlay.pos"),
                   [](ps5ui::ListBuilder& L) {
                     L.item("id_overlay_pos_1",
                            toolbox_i18n::tr("overlay.pos.top"), "0")
                         .item("id_overlay_pos_3",
                               toolbox_i18n::tr("overlay.pos.bottom"), "2");
                   },
                   toolbox_i18n::tr("overlay.pos.sub"),
                   toolbox_val("id_overlay_change_pos"))
                .toggle("id_overlay_fps", toolbox_i18n::tr("overlay.fps"),
                        toolbox_on("id_overlay_fps"), std::nullopt,
                        toolbox_i18n::tr("overlay.fps.desc"))
                .toggle("id_overlay_gpu", toolbox_i18n::tr("overlay.gpu"),
                        toolbox_on("id_overlay_gpu"), std::nullopt,
                        toolbox_i18n::tr("overlay.gpu.desc"))
                .toggle("id_overlay_cpu", toolbox_i18n::tr("overlay.cpu"),
                        toolbox_on("id_overlay_cpu"), std::nullopt,
                        toolbox_i18n::tr("overlay.cpu.desc"))
                .toggle("id_all_cpu_usage", toolbox_i18n::tr("overlay.cpu_all"),
                        toolbox_on("id_all_cpu_usage"), std::nullopt,
                        toolbox_i18n::tr("overlay.cpu_all.desc"))
                .toggle("id_overlay_ram", toolbox_i18n::tr("overlay.ram"),
                        toolbox_on("id_overlay_ram"), std::nullopt,
                        toolbox_i18n::tr("overlay.ram.desc"))
                .toggle("id_overlay_ip", toolbox_i18n::tr("overlay.ip"),
                        toolbox_on("id_overlay_ip"), std::nullopt,
                        toolbox_i18n::tr("overlay.ip.desc"));
          },
          toolbox_i18n::tr("overlay.group.sub"), std::nullopt,
          "id_overlay_change_pos");
}

void append_toolbox_system_group(ps5ui::Group& g) {
  g.list("id_ui_lang", toolbox_i18n::tr("lang.list"),
         [](ps5ui::ListBuilder& L) {
           L.item("id_ui_lang_zh", toolbox_i18n::tr("lang.zh"), "0")
               .item("id_ui_lang_en", toolbox_i18n::tr("lang.en"), "1");
         },
         toolbox_i18n::tr("lang.list.sub"), toolbox_val("id_ui_lang", "0"))
      .toggle("id_disp_titleids", toolbox_i18n::tr("disp_tids"),
              toolbox_on("id_disp_titleids"), toolbox_i18n::tr("disp_tids.sub"))
      .group(
          "id_group_fan", toolbox_i18n::tr("fan.group"),
          [](ps5ui::Group& f) {
            f.toggle("id_enable_fan_speed", toolbox_i18n::tr("fan.enable"),
                     toolbox_on("id_enable_fan_speed"))
                .text_field("id_fan_speed", toolbox_i18n::tr("fan.threshold"),
                            toolbox_i18n::tr("fan.threshold.sub"), "number", "2",
                            "2", std::nullopt, std::nullopt, std::nullopt,
                            toolbox_val("id_fan_speed", ""));
          },
          std::nullopt, std::nullopt, "id_enable_fan_speed")
      .group(
          "id_rest_mode", toolbox_i18n::tr("rest.group"),
          [](ps5ui::Group& r) {
            r.text_field("id_rest_1", toolbox_i18n::tr("rest.delay"),
                         toolbox_i18n::tr("rest.delay.sub"), "number", "1",
                         "255", std::nullopt, std::nullopt, std::nullopt,
                         toolbox_val("id_rest_1", ""))
                .toggle("id_rest_2", toolbox_i18n::tr("rest.kill_util"),
                        toolbox_on("id_rest_2"),
                        toolbox_i18n::tr("rest.kill_util.sub"))
                .toggle("id_rest_3", toolbox_i18n::tr("rest.kill_game"),
                        toolbox_on("id_rest_3"),
                        toolbox_i18n::tr("rest.kill_game.sub"));
          },
          toolbox_i18n::tr("rest.group.sub"), std::nullopt, "id_rest_1")
      .link("id_external_hdd", toolbox_i18n::tr("hdd.external"),
            "DebugSettings/data/debug_settings_external_hdd.xml")
      .link("id_licenseactivation", toolbox_i18n::tr("license.bd"),
            "DebugSettings/data/debug_settings_licenseactivation.xml");
}

void append_toolbox_shortcuts_group(ps5ui::Group& g) {
  g.list("id_cheats_shortcut", toolbox_i18n::tr("sc.cheats"),
         [](ps5ui::ListBuilder& L) {
           L.item("id_cheats_shortcut_0", toolbox_i18n::tr("sc.off"), "0")
               .item("id_cheats_shortcut_1", toolbox_i18n::tr("sc.r3_l3"), "1")
               .item("id_cheats_shortcut_2", toolbox_i18n::tr("sc.l2_tri"), "2")
               .item("id_cheats_shortcut_3",
                     toolbox_i18n::tr("sc.long_options"), "3")
               .item("id_cheats_shortcut_4", toolbox_i18n::tr("sc.long_share"),
                     "4")
               .item("id_cheats_shortcut_5", toolbox_i18n::tr("sc.share"), "5");
         },
         toolbox_i18n::tr("sc.cheats.sub"), toolbox_val("id_cheats_shortcut"))
      .list("id_toolbox_shortcut", toolbox_i18n::tr("sc.toolbox"),
            [](ps5ui::ListBuilder& L) {
              L.item("id_toolbox_shortcut_0", toolbox_i18n::tr("sc.off"), "0")
                  .item("id_toolbox_shortcut_1", toolbox_i18n::tr("sc.l2_r3"),
                        "1")
                  .item("id_toolbox_shortcut_2",
                        toolbox_i18n::tr("sc.long_share"), "2")
                  .item("id_toolbox_shortcut_3", toolbox_i18n::tr("sc.share"),
                        "3");
            },
            toolbox_i18n::tr("sc.toolbox.sub"),
            toolbox_val("id_toolbox_shortcut"));
}

void append_toolbox_debug_group(ps5ui::Group& g) {
  g.toggle("id_debug_jb", toolbox_i18n::tr("debug.jb"),
           toolbox_on("id_debug_jb"), toolbox_i18n::tr("debug.jb.sub"))
      .toggle("id_debug_legacy_cmd", toolbox_i18n::tr("debug.legacy_cmd"),
              toolbox_on("id_debug_legacy_cmd"),
              toolbox_i18n::tr("debug.legacy_cmd.sub"))
      .text_field("id_np_env", toolbox_i18n::tr("debug.np_env"), std::nullopt,
                  "basic_latin", "1", "16", "/NP/env",
                  toolbox_i18n::tr("debug.np_env.confirm"),
                  toolbox_i18n::tr("debug.np_env.confirm_phrase"));
}

void append_toolbox_about_group(ps5ui::Group& g) {
  g.group(
       "id_donation_methods", toolbox_i18n::tr("about.donate"),
       [](ps5ui::Group& d) {
         d.label("id_method_info", toolbox_i18n::tr("about.donate.methods"),
                 ps5ui::Style::Center)
             .label("id_method_1",
                    "- GitHub Sponsors  | https://github.com/sponsors/LightningMods",
                    ps5ui::Style::Center)
             .label("id_method_2",
                    "- GoldHEN / SiSTR0  | https://ko-fi.com/sistro",
                    ps5ui::Style::Center);
       },
       toolbox_i18n::tr("about.donate.sub"))
      .group(
          "id_onionhen_credits", toolbox_i18n::tr("about.credits"),
          [](ps5ui::Group& c) {
            const std::string ver =
                std::string("★ OnionHEN ") + OnionHEN_VERSION;
            c.label("id_about_version", ver, ps5ui::Style::Center)
                .label("id_about_lineage", toolbox_i18n::tr("about.lineage"),
                       ps5ui::Style::Center)
                .label("id_about_lineage_1",
                       toolbox_i18n::tr("about.lineage.etahen"),
                       ps5ui::Style::Center)
                .label("id_about_lineage_2",
                       toolbox_i18n::tr("about.lineage.goldhen"),
                       ps5ui::Style::Center)
                .label("id_about_testers", toolbox_i18n::tr("about.testers"),
                       ps5ui::Style::Center)
                .label("id_about_testers_intro",
                       toolbox_i18n::tr("about.testers.intro"),
                       ps5ui::Style::Center)
                /* Names from kylin-core-web non-OEM KcContributorModal avatars. */
                .label("id_about_tester_1",
                       "B站谢锡榆 · Misy · pj123rock · 风吹屁屁凉 · 风清扬",
                       ps5ui::Style::Center)
                .label("id_about_tester_2",
                       "狂爱龙卷风 · 萌面超人 · 石之心 · 西安萼片 · 萧河",
                       ps5ui::Style::Center)
                .label("id_about_tester_3",
                       "心どいの小霖 · 夜太美 · 影TGlobal-sky",
                       ps5ui::Style::Center)
                .label("id_about_more", toolbox_i18n::tr("about.more"),
                       ps5ui::Style::Center);
          },
          toolbox_i18n::tr("about.credits.sub"))
      .group(
          "id_inc_project", toolbox_i18n::tr("about.projects"),
          [](ps5ui::Group& p) {
            p.label("id_project_info", toolbox_i18n::tr("about.projects.info"),
                    ps5ui::Style::Center)
                .label("id_project_1",
                       "etaHEN — https://github.com/LightningMods/etaHEN",
                       ps5ui::Style::Center)
                .label("id_project_2",
                       "PS5 Payload Dev SDK — "
                       "https://github.com/ps5-payload-dev/sdk",
                       ps5ui::Style::Center)
                .label("id_project_3",
                       "PS5 Payload Dev elfldr — "
                       "https://github.com/ps5-payload-dev/elfldr",
                       ps5ui::Style::Center)
                .label("id_project_4",
                       "libhijacker (astrelsky) — "
                       "https://github.com/astrelsky/libhijacker",
                       ps5ui::Style::Center)
                .label("id_project_5",
                       "NineS (buzzer-re) — "
                       "https://github.com/buzzer-re/NineS",
                       ps5ui::Style::Center)
                .label("id_project_6",
                       "kstuff (sleirsgoevy / EchoStretch)",
                       ps5ui::Style::Center)
                .label("id_project_7",
                       "cJSON — https://github.com/DaveGamble/cJSON",
                       ps5ui::Style::Center)
                .label("id_project_8",
                       "7-Zip LZMA SDK — https://www.7-zip.org/sdk.html",
                       ps5ui::Style::Center)
                .label("id_project_9",
                       "miniz / Keystone (cheats engine)",
                       ps5ui::Style::Center);
          },
          toolbox_i18n::tr("about.projects.sub"));
}

} // namespace

void generate_toolbox_xml(std::string& new_xml) {
  toolbox_i18n::apply_ui_lang(g_settings.ui_lang);
  ps5ui::Page page("id_debug_settings", toolbox_i18n::tr("root.title"));
  page.root_focus("id_group_pkg");

  page.group(
          "id_group_pkg", toolbox_i18n::tr("group.pkg"),
          [](ps5ui::Group& g) { append_toolbox_pkg_group(g); },
          toolbox_i18n::tr("group.pkg.sub"), kIconPkg,
          "id_game_package_installer")
      .group(
          "id_group_payloads", toolbox_i18n::tr("group.payloads"),
          [](ps5ui::Group& g) { append_toolbox_payloads_group(g); },
          toolbox_i18n::tr("group.payloads.sub"), kIconPlugins, "id_payloads")
      .group(
          "id_group_game", toolbox_i18n::tr("group.game"),
          [](ps5ui::Group& g) { append_toolbox_game_group(g); },
          toolbox_i18n::tr("group.game.sub"), kIconGame, "id_cheats")
      .group(
          "id_group_system", toolbox_i18n::tr("group.system"),
          [](ps5ui::Group& g) { append_toolbox_system_group(g); },
          toolbox_i18n::tr("group.system.sub"), kIconSettings, "id_ui_lang")
      .group(
          "id_utils", toolbox_i18n::tr("group.shortcuts"),
          [](ps5ui::Group& g) { append_toolbox_shortcuts_group(g); },
          toolbox_i18n::tr("group.shortcuts.sub"), kIconShortcuts,
          "id_cheats_shortcut")
      .group(
          "id_group_debug", toolbox_i18n::tr("group.debug"),
          [](ps5ui::Group& g) { append_toolbox_debug_group(g); },
          toolbox_i18n::tr("group.debug.sub"), kIconDebug, "id_debug_jb")
      .group(
          "id_onionhen_credit_options", toolbox_i18n::tr("group.about"),
          [](ps5ui::Group& g) { append_toolbox_about_group(g); },
          toolbox_i18n::tr("group.about.sub"), kIconAbout, std::nullopt,
          ps5ui::Style::Center);

  new_xml = page.build();
}
