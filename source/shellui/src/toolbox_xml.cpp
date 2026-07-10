/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Extracted from MonoUtils.cpp for module locality.
 */

#include "HookedFuncs.hpp"
#include "ipc.hpp"
#include "RemotePlay.h"
#include "external_symbols.hpp"
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
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <vector>
#include <string>

int usbpath();
void escapeXML(std::string& input);
bool is_valid_plugin(CustomPluginHeader &header);
bool Get_Running_App_TID(std::string &title_id, int &BigAppid);
extern int cheatEnabledMap[256];
extern std::string running_tid;
extern bool is_game_open;
extern bool is_current_game_open;
extern std::string remote_play_info;
extern std::vector<GameEntry> games_list;
extern std::vector<Plugins> plugins_list, auto_list;

bool is_valid_plugin(CustomPluginHeader &header)
{
  // Check if the prefix matches
  if (strncmp(header.prefix, "OrionHEN_PLUGIN", 14) != 0)
  {
    shellui_log("Plugin header prefix does not match");
    return false;
  }

  for (int i = 0; i < 4; ++i)
  {
    if (header.titleID[i] < 'A' || header.titleID[i] > 'Z')
    {
      shellui_log("Invalid plugin file: titleID must contain 4 uppercase letters as the start");
      return false;
    }
  }
  for (int i = 4; i < 9; ++i)
  {
    if (header.titleID[i] < '0' || header.titleID[i] > '9')
    {
      shellui_log("Invalid plugin file: titleID must contain 5 numbers as the end");
      return false;
    }
  }

  // Ensure the title ID is nullptr-terminated
  if (header.titleID[9] != '\0')
  {
    shellui_log("Invalid plugin file: titleID must be nullptr-terminated");
    return false;
  }

  for (int i = 0; i < 3; ++i)
  {
    if (header.plugin_version[i] == '.')
    {
      continue;
    }
    else if (header.plugin_version[i] < '0' || header.plugin_version[i] > '9')
    {
      shellui_log("Invalid plugin file: version must be in the following format xx.xx");
      return false;
    }
  }

  return true;
}

namespace {

/** UI-facing path: strip /user prefix, map /usb* → /mnt/usb*. */
std::string display_path_for_ui(const std::string& path) {
  if (path.rfind("/user", 0) == 0)
    return path.substr(5); // drop "/user"
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

void append_plugin_entry(std::string& xml, const std::string& directory,
                         const char* filename, bool plugins_xml, int& next_id) {
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

  std::string toggle;
  if (plugins_xml) {
    const char* tid = is_elf ? filename : header.titleID;
    toggle = "<toggle_switch id=\"" + id + "\" title=\"" + filename + " " +
             version_str + "\" second_title=\"启动/停止 " + filename +
             " (路径: " + shown_path + ") (" + tid + ")\" value=\"0\"/>\n";
  } else {
    toggle = "<toggle_switch id=\"" + id + "\" title=\"" + filename + " " +
             version_str + "\" second_title=\"启用/禁用 " + filename +
             " 的自动启动  (" + shown_path + ")\" value=\"0\"/>\n";
  }
  xml += toggle;

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

void append_homebrew_game(std::string& xml, const std::string& game_dir,
                          const char* dir_name, int random_num) {
  const std::string elf_path = game_dir + "/eboot.elf";
  if (access(elf_path.c_str(), F_OK) != 0)
    return;

#if SHELL_DEBUG == 1
  shellui_log("Found Game: %s", game_dir.c_str());
#endif

  std::string title_id, title, ver;
  const std::string shown_path = display_path_for_ui(game_dir);

  std::string icon_path = game_dir + "/sce_sys/icon0.png";
  escapeXML(icon_path);

  GameEntry game;
  game.tid = title_id;
  game.title = title;
  escapeXML(game.title);
  game.version = ver;
  game.path = shown_path;
  game.dir_name = dir_name;
  escapeXML(game.dir_name);
  game.icon_path = icon_path;
  game.id = "id_orionhen_pl_loader_" + title_id + "_" + std::to_string(random_num);
  games_list.push_back(game);

  xml += "<button id=\"" + game.id + "\" title=\"(" + title_id + ") " + title +
         "\" icon=\"" + icon_path + "\" second_title=\"" + shown_path +
         " | 版本: " + ver + "\"/>\n";
}

} // namespace

void generate_remote_play_xml(std::string& xml_buffer) {
  char pin_code[PIN_CODE_SIZE] = {0};
  char AccountID[ACCOUNT_ID_BASE64_SIZE] = {0};
  uint64_t dec_account_id = 0;

  bzero(AccountID, ACCOUNT_ID_BASE64_SIZE);

  xml_buffer = R"(<?xml version="1.0" encoding="UTF-8" ?>
    <system_settings version="1.0" plugin="debug_settings_plugin">
    <setting_list id="remote_play_pin_display" title="远程游玩连接详情" style="center">)";

  shellui_log("Starting remote play");
  static bool remote_play_initialized = false;
  if (!remote_play_initialized) {
    InitRemotePlay();
    remote_play_initialized = true;
  }

  if (IsNotActivated()) {
    GetEncodedAccountID(AccountID, dec_account_id);
    xml_buffer += R"(<label id="id_pin_2" title="账号已由 OrionHEN 激活，请重启主机后再使用远程游玩！" style="center"/>)";
    xml_buffer += R"(</setting_list></system_settings>)";
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
  xml_buffer +=
      R"(<label id="id_pin" title=")" + std::string(pin_code) + R"(" style="center"/>)";
  shellui_log("Pin code str => %s", pin_code);

  xml_buffer += R"(<label id="base64_account_id" title="账号 ID: )";
  xml_buffer += std::string(AccountID) + R"(" style="center"/>)";

  if (usbpath() != -1)
    xml_buffer +=
        R"(<button id="id_save_rp_info" title="将远程游玩详情保存到 USB" style="center"/>)";

  xml_buffer += R"(</setting_list></system_settings>)";
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

  xml_buffer = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
               "<system_settings version=\"1.0\" plugin=\"debug_settings_plugin\">\n"
               "\n";

  if (plugins_xml)
    xml_buffer += "<setting_list id=\"id_plugin\" title=\"插件\">\n";
  else
    xml_buffer += "<setting_list id=\"id_auto_plugins\" title=\"★ 插件 - 启动菜单\">\n";

  int toggle_switch_id = 1;
  for (const auto& directory : kPluginDirs) {
    DIR* dir = opendir(directory.c_str());
    if (!dir) {
      shellui_log("Failed to open directory: %s", directory.c_str());
      continue;
    }

    while (struct dirent* entry = readdir(dir))
      append_plugin_entry(xml_buffer, directory, entry->d_name, plugins_xml,
                          toggle_switch_id);

    closedir(dir);
  }

  if (plugins_xml) {
    xml_buffer +=
        "<link id=\"id_auto_plugins\" title=\"★ 插件 - 启动菜单\" "
        "file=\"auto_plugins.xml\" "
        "second_title=\"配置在加载 OrionHEN 时自动启动的插件\"/>\n";
    xml_buffer += "</setting_list>\n</setting_list>\n</system_settings> ";
  } else {
    xml_buffer += "</setting_list>\n</system_settings> ";
  }
}

void escapeXML(std::string& input) 
{
    std::unordered_map<std::string, std::string> escapeSequences = 
    {
        {"&", "&amp;"},
        {"<", "&lt;"},
        {">", "&gt;"},
        {"\"", "&quot;"},
        {"/", "//"}
    };
    
    for (const auto& pair : escapeSequences) 
    {
        size_t pos = 0;
        while ((pos = input.find(pair.first, pos)) != std::string::npos) 
        {
            input.replace(pos, pair.first.length(), pair.second);
            pos += pair.second.length(); // Move past the replaced part
        }
    }
}

namespace {

/** Download / repo selector block when no cheats are available. */
constexpr const char* kDownloadCheatsXml =
    R"(<list id="id_selected_cheats_repo" title="金手指仓库来源" >
         <list_item id="id_selected_cheats_repo_1" title="OrionHEN PS5 金手指仓库" value="0"/>
         <list_item id="id_selected_cheats_repo_2" title="GoldHEN PS4 金手指仓库" value="1"/>
       </list>
       <button id="id_dl_cheats" title="下载/更新金手指"
               second_title="从所选 GitHub 仓库下载到 /data/OrionHEN/cheats/（TITLEID_VERSION.ext）"/>)";

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

void append_authors_xml(std::string& xml, cJSON* root) {
  xml += R"(<label id="credits" style="center" title="金手指作者: )";

  std::unordered_set<std::string> seen;
  bool first = true;
  cJSON* authors = orion_cjson::item(root, "authors");
  if (cJSON_IsArray(authors)) {
    cJSON* author = nullptr;
    cJSON_ArrayForEach(author, authors) {
      const char* value = orion_cjson::string_value(author);
      if (!value || !seen.insert(value).second)
        continue;

      std::string name = value;
      escapeXML(name);
      if (!first)
        xml += ", ";
      xml += name;
      first = false;
    }
  }
  xml += R"(" />)";
}

void append_cheat_entries_xml(std::string& xml, cJSON* root,
                              const std::string& tid,
                              const std::string& game_name, bool can_toggle) {
  cJSON* cheats = orion_cjson::item(root, "cheats");
  if (!cJSON_IsArray(cheats))
    return;

  cJSON* entry = nullptr;
  cJSON_ArrayForEach(entry, cheats) {
    std::string name = orion_cjson::string_item(entry, "name", "");
    std::string desc = orion_cjson::string_item(entry, "description", "开/关");
    escapeXML(name);
    escapeXML(desc);

    const int id = orion_cjson::int_item(entry, "id");
    const bool enabled = orion_cjson::bool_item(entry, "enabled");
    const std::string id_attr = "id_cheat_" + tid + "_" + std::to_string(id);

    if (can_toggle) {
      xml += R"(<toggle_switch id=")" + id_attr +
             R"(" icon="tex_game_icon" title=")" + name +
             R"(" description=")" + desc +
             R"(" value=")" + (enabled ? "1" : "0") + R"("/>)";
    } else {
      xml += R"(<button id=")" + id_attr +
             R"(" icon="tex_game_icon" title=")" + name +
             R"(" description=")" + desc +
             R"(" second_title="为 )" + game_name + R"( 启用/禁用 )" + name +
             R"(" />)";
    }
    cheatEnabledMap[id] = enabled;
  }
}

void finish_cheats_xml(std::string& xml) {
  xml += "</setting_list>\n</system_settings>";
}

} // namespace

void generate_cheats_xml(std::string& new_xml, std::string& not_open_tid,
                         bool running_as_debug_settings, bool show_while_not_open) {
  const std::string list_id =
      running_as_debug_settings ? "id_debug_settings" : "id_cheat_title";

  new_xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
      "<system_settings version=\"1.0\" plugin=\"debug_settings_plugin\">\n"
      "\n";

  int appid = -1;
  is_game_open = Get_Running_App_TID(running_tid, appid);
  is_current_game_open =
      is_game_open &&
      running_tid == (show_while_not_open ? not_open_tid : running_tid);

  // No game running and not browsing offline cheats → download UI only.
  if (!is_game_open && !show_while_not_open) {
    new_xml += "<setting_list id=\"" + list_id +
               "\" title=\"OrionHEN 金手指 - 当前没有打开的游戏\">\n";
    new_xml += kDownloadCheatsXml;
    finish_cheats_xml(new_xml);
    return;
  }

  running_tid = show_while_not_open ? not_open_tid : running_tid;
  IPC_Client& client = IPC_Client::getInstance(true);

  std::string game_ver;
  if (!client.GameVerFromTid(running_tid, game_ver))
    game_ver = "无法检测补丁版本";

  new_xml += "<setting_list id=\"" + list_id + "\" title=\"OrionHEN 金手指 - " +
             running_tid + " - " + game_ver + "\">\n";

  if (!is_game_open && show_while_not_open) {
    new_xml += R"(<label id="id_cheat_disclaimer" title=")" + running_tid +
               R"( 当前未运行，除非打开游戏否则无法激活任何金手指" style="center"/>)";
  }

  std::string cheat_path;
  if (!client.GetGameCheats(running_tid, game_ver, cheat_path)) {
    new_xml += kDownloadCheatsXml;
    finish_cheats_xml(new_xml);
    return;
  }

  const std::string json_string = read_file_to_string(cheat_path.c_str());
  if (json_string.empty()) {
    finish_cheats_xml(new_xml);
    return;
  }
  unlink(cheat_path.c_str());

  orion_cjson::Root res_json(json_string);
  if (!res_json) {
    shellui_log("Failed to parse json from cheat response!");
    finish_cheats_xml(new_xml);
    return;
  }

  std::string game_name = orion_cjson::string_item(res_json.get(), "name", "");
  escapeXML(game_name);
  new_xml += R"(<label id="id_cheat_title" title="★ )" + game_name +
             R"( ★" style="center"/>)";

  append_authors_xml(new_xml, res_json.get());
  append_cheat_entries_xml(new_xml, res_json.get(), running_tid, game_name,
                           is_game_open && is_current_game_open);
  finish_cheats_xml(new_xml);
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

  new_xml = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
            "<system_settings version=\"1.0\" plugin=\"debug_settings_plugin\">\n"
            "\n"
            "<setting_list id=\"id_plapps\" title=\"OrionHEN Payload 自制软件 - 应用程序\">\n";

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

      append_homebrew_game(new_xml, game_dir, entry->d_name, dist(gen));
    }
    closedir(dir);
  }

  new_xml += "</setting_list>\n</system_settings>";
}

