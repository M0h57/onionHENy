/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Extracted from MonoUtils.cpp for module locality.
 * Dynamic settings pages are built via ps5ui::Page (fluent XML builder).
 */

#include "HookedFuncs.hpp"
#include "RemotePlay.h"
#include "external_symbols.hpp"
#include "ipc.hpp"
#include "ps5_settings_ui.hpp"
#include "../../extern/cJSON/orion_cjson.hpp"

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

int usbpath();
void escapeXML(std::string& input);
bool is_valid_plugin(CustomPluginHeader& header);
bool Get_Running_App_TID(std::string& title_id, int& BigAppid);

bool is_valid_plugin(CustomPluginHeader& header) {
  if (strncmp(header.prefix, "OrionHEN_PLUGIN", 14) != 0) {
    shellui_log("Plugin header prefix does not match");
    return false;
  }

  for (int i = 0; i < 4; ++i) {
    if (header.titleID[i] < 'A' || header.titleID[i] > 'Z') {
      shellui_log("Invalid plugin file: titleID must contain 4 uppercase letters as the start");
      return false;
    }
  }
  for (int i = 4; i < 9; ++i) {
    if (header.titleID[i] < '0' || header.titleID[i] > '9') {
      shellui_log("Invalid plugin file: titleID must contain 5 numbers as the end");
      return false;
    }
  }

  if (header.titleID[9] != '\0') {
    shellui_log("Invalid plugin file: titleID must be nullptr-terminated");
    return false;
  }

  for (int i = 0; i < 3; ++i) {
    if (header.plugin_version[i] == '.')
      continue;
    if (header.plugin_version[i] < '0' || header.plugin_version[i] > '9') {
      shellui_log("Invalid plugin file: version must be in the following format xx.xx");
      return false;
    }
  }

  return true;
}

void escapeXML(std::string& input) {
  input = ps5ui::escape(input);
}

namespace {

/** UI-facing path: strip /user prefix, map /usb* → /mnt/usb*. */
std::string display_path_for_ui(const std::string& path) {
  if (path.rfind("/user", 0) == 0)
    return path.substr(5);
  if (path.rfind("/usb", 0) == 0)
    return "/mnt" + path;
  return path;
}

bool read_plugin_header(const std::string& path, CustomPluginHeader& header) {
  const int fd = open(path.c_str(), O_RDONLY, 0);
  if (fd < 0) {
    shellui_log("Failed to open Plugin file");
    return false;
  }
  const ssize_t n = read(fd, &header, sizeof(header));
  close(fd);
  if (n != static_cast<ssize_t>(sizeof(header))) {
    shellui_log("Failed to read Plugin file, %s", path.c_str());
    return false;
  }
  return true;
}

bool is_plugin_or_elf_name(const char* name) {
  const bool is_elf = strstr(name, ".elf") != nullptr;
  const bool is_plugin = strstr(name, ".plugin") != nullptr;
  const bool is_auto = strstr(name, ".auto_start") != nullptr;
  return (is_plugin || is_elf) && !is_auto;
}

template <typename G>
void append_plugin_entry(G& page, const std::string& directory, const char* filename,
                         bool plugins_xml, int& next_id) {
  if (!is_plugin_or_elf_name(filename))
    return;

  const bool is_elf = strstr(filename, ".elf") != nullptr;
  const std::string path = directory + "/" + filename;
  shellui_log("Found Plugin: %s", path.c_str());

  CustomPluginHeader header{};
  if (!read_plugin_header(path, header))
    return;

  if (!is_elf && !is_valid_plugin(header)) {
    shellui_log("Invalid plugin file.");
    return;
  }
  if (is_elf) {
    strncpy(header.prefix, "<elf>", 5);
    strncpy(header.plugin_version, "", 4);
  }
  shellui_log("Valid plugin file.");

  const std::string shown_path = display_path_for_ui(path);
  const std::string version_str =
      is_elf ? "" : ("(v" + std::string(header.plugin_version) + ")");
  const std::string id_prefix = plugins_xml ? "id_plugin_" : "id_auto_plugin_";
  const std::string id = id_prefix + std::to_string(next_id++);
  const std::string title = std::string(filename) + " " + version_str;

  std::string second;
  if (plugins_xml) {
    const char* tid = is_elf ? filename : header.titleID;
    second = "启动/停止 " + std::string(filename) + " (路径: " + shown_path + ") (" +
             tid + ")";
  } else {
    second = "启用/禁用 " + std::string(filename) + " 的自动启动  (" + shown_path + ")";
  }

  page.toggle(id, title, /*on=*/false, second);

  Plugins entry;
  entry.shellui_path = path;
  entry.tid = is_elf ? filename : header.titleID;
  entry.path = shown_path;
  entry.name = filename;
  entry.version = header.plugin_version;
  entry.id = id;
  if (plugins_xml)
    plugins_list.push_back(entry);
  else
    auto_list.push_back(entry);
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
  const std::string shown_path = display_path_for_ui(game_dir);
  const std::string icon_path = game_dir + "/sce_sys/icon0.png";

  GameEntry game;
  game.tid = title_id;
  game.title = title;
  game.version = ver;
  game.path = shown_path;
  game.dir_name = dir_name;
  game.icon_path = icon_path;
  game.id = "id_orionhen_pl_loader_" + title_id + "_" + std::to_string(random_num);
  games_list.push_back(game);

  page.button(game.id, "(" + title_id + ") " + title,
              shown_path + " | 版本: " + ver, std::nullopt, icon_path);
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

template <typename G>
void append_download_cheats_block(G& page) {
  page.list("id_selected_cheats_repo", "金手指仓库来源", [](ps5ui::ListBuilder& L) {
        L.item("id_selected_cheats_repo_1", "OrionHEN PS5 金手指仓库", "0")
            .item("id_selected_cheats_repo_2", "GoldHEN PS4 金手指仓库", "1");
      })
      .button("id_dl_cheats", "下载/更新金手指",
              "从所选 GitHub 仓库下载到 /data/OrionHEN/cheats/（TITLEID_VERSION.ext）");
}

std::string join_authors(cJSON* root) {
  std::unordered_set<std::string> seen;
  std::string joined;
  bool first = true;
  cJSON* authors = orion_cjson::item(root, "authors");
  if (!cJSON_IsArray(authors))
    return joined;

  cJSON* author = nullptr;
  cJSON_ArrayForEach(author, authors) {
    const char* value = orion_cjson::string_value(author);
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
  cJSON* cheats = orion_cjson::item(root, "cheats");
  if (!cJSON_IsArray(cheats))
    return;

  cJSON* entry = nullptr;
  cJSON_ArrayForEach(entry, cheats) {
    const std::string name = orion_cjson::string_item(entry, "name", "");
    const std::string desc = orion_cjson::string_item(entry, "description", "开/关");
    const int id = orion_cjson::int_item(entry, "id");
    const bool enabled = orion_cjson::bool_item(entry, "enabled");
    const std::string id_attr = "id_cheat_" + tid + "_" + std::to_string(id);

    if (can_toggle) {
      page.toggle(id_attr, name, enabled, std::nullopt, desc, "tex_game_icon");
    } else {
      page.button(id_attr, name, "为 " + game_name + " 启用/禁用 " + name, desc,
                  "tex_game_icon");
    }
    cheatEnabledMap[id] = enabled;
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

  ps5ui::Page page("remote_play_pin_display", "远程游玩连接详情");
  page.root_style(ps5ui::Style::Center);

  if (IsNotActivated()) {
    GetEncodedAccountID(AccountID, dec_account_id);
    page.label("id_pin_2",
               "账号已由 OrionHEN 激活，请重启主机后再使用远程游玩！",
               ps5ui::Style::Center);
    xml_buffer = page.build();
    return;
  }

  shellui_log("Get encoded account id");
  GetEncodedAccountID(AccountID, dec_account_id);
  shellui_log("Get encoded account id ==> %s", AccountID);

  remote_play_info = "账号 ID: " + std::string(AccountID);
  {
    std::stringstream ss;
    ss << std::hex << std::uppercase << dec_account_id;
    remote_play_info += "\n解码后账号 ID: " + ss.str();
  }

  const uint32_t pinCode = GeneratePINCode();
  shellui_log("Pin code => %d", pinCode);
  sprintf(pin_code, "PIN 码  : %04d %04d    ", pinCode / 10000, pinCode % 10000);
  remote_play_info += "\n" + std::string(pin_code);
  shellui_log("Pin code str => %s", pin_code);

  page.label("id_pin", pin_code, ps5ui::Style::Center)
      .label("base64_account_id", std::string("账号 ID: ") + AccountID,
             ps5ui::Style::Center);

  if (usbpath() != -1)
    page.button("id_save_rp_info", "将远程游玩详情保存到 USB", std::nullopt,
                std::nullopt, std::nullopt, ps5ui::Style::Center);

  xml_buffer = page.build();
}

void generate_plugin_xml(std::string& xml_buffer, bool plugins_xml) {
  static const std::vector<std::string> kPluginDirs = {
      "/user/data/OrionHEN/plugins",
      "/usb0/OrionHEN/plugins",
      "/usb1/OrionHEN/plugins",
      "/usb2/OrionHEN/plugins",
      "/usb3/OrionHEN/plugins",
      "/user/data/OrionHEN/payloads",
      "/usb0/OrionHEN/payloads",
      "/usb1/OrionHEN/payloads",
      "/usb2/OrionHEN/payloads",
      "/usb3/OrionHEN/payloads",
  };

  const char* root_id = plugins_xml ? "id_plugin" : "id_auto_plugins";
  const char* root_title = plugins_xml ? "插件" : "★ 插件 - 启动菜单";
  ps5ui::Page page(root_id, root_title);

  int toggle_switch_id = 1;
  for (const auto& directory : kPluginDirs) {
    DIR* dir = opendir(directory.c_str());
    if (!dir) {
      shellui_log("Failed to open directory: %s", directory.c_str());
      continue;
    }
    while (struct dirent* entry = readdir(dir))
      append_plugin_entry(page, directory, entry->d_name, plugins_xml,
                          toggle_switch_id);
    closedir(dir);
  }

  if (plugins_xml) {
    page.link("id_auto_plugins", "★ 插件 - 启动菜单", "auto_plugins.xml",
              "配置在加载 OrionHEN 时自动启动的插件");
  }

  xml_buffer = page.build();
}

void generate_cheats_xml(std::string& new_xml, std::string& not_open_tid,
                         bool running_as_debug_settings, bool show_while_not_open) {
  const std::string list_id =
      running_as_debug_settings ? "id_debug_settings" : "id_cheat_title";

  int appid = -1;
  is_game_open = Get_Running_App_TID(running_tid, appid);
  is_current_game_open =
      is_game_open &&
      running_tid == (show_while_not_open ? not_open_tid : running_tid);

  if (!is_game_open && !show_while_not_open) {
    ps5ui::Page page(list_id, "OrionHEN 金手指 - 当前没有打开的游戏");
    append_download_cheats_block(page);
    new_xml = page.build();
    return;
  }

  running_tid = show_while_not_open ? not_open_tid : running_tid;
  IPC_Client& client = IPC_Client::getInstance(true);

  std::string game_ver;
  if (!client.GameVerFromTid(running_tid, game_ver))
    game_ver = "无法检测补丁版本";

  ps5ui::Page page(list_id,
                   "OrionHEN 金手指 - " + running_tid + " - " + game_ver);

  if (!is_game_open && show_while_not_open) {
    page.label("id_cheat_disclaimer",
               running_tid + " 当前未运行，除非打开游戏否则无法激活任何金手指",
               ps5ui::Style::Center);
  }

  std::string cheat_path;
  if (!client.GetGameCheats(running_tid, game_ver, cheat_path)) {
    append_download_cheats_block(page);
    new_xml = page.build();
    return;
  }

  const std::string json_string = read_file_to_string(cheat_path.c_str());
  if (json_string.empty()) {
    new_xml = page.build();
    return;
  }
  unlink(cheat_path.c_str());

  orion_cjson::Root res_json(json_string);
  if (!res_json) {
    shellui_log("Failed to parse json from cheat response!");
    new_xml = page.build();
    return;
  }

  const std::string game_name =
      orion_cjson::string_item(res_json.get(), "name", "");
  page.label("id_cheat_title", "★ " + game_name + " ★", ps5ui::Style::Center);

  const std::string authors = join_authors(res_json.get());
  page.label("credits", "金手指作者: " + authors, ps5ui::Style::Center);

  append_cheat_entries(page, res_json.get(), running_tid, game_name,
                       is_game_open && is_current_game_open);
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

  ps5ui::Page page("id_plapps", "OrionHEN Payload 自制软件 - 应用程序");

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
