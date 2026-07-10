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

void generate_remote_play_xml(std::string &xml_buffer)
{
  // int pair_stat = -1, pair_err = -1, err = -1;
  char pin_code[PIN_CODE_SIZE] = {0};
  char AccountID[ACCOUNT_ID_BASE64_SIZE] = {0};
  uint64_t dec_account_id = 0;
  uint32_t pinCode = 0;
  bzero(AccountID, ACCOUNT_ID_BASE64_SIZE);
  std::stringstream ss;

  xml_buffer = R"(<?xml version="1.0" encoding="UTF-8" ?>
    <system_settings version="1.0" plugin="debug_settings_plugin">
    <setting_list id="remote_play_pin_display" title="远程游玩连接详情" style="center">)";

  shellui_log("Starting remote play");
  static bool remote_play_initialized = false;
  if (!remote_play_initialized)
  {
    InitRemotePlay();
    remote_play_initialized = true;
  }

  if (IsNotActivated())
  {
    //
    // Implicit activate it
    //
    GetEncodedAccountID(AccountID, dec_account_id);
    xml_buffer += R"(<label id="id_pin_2" title="账号已由 OrionHEN 激活，请重启主机后再使用远程游玩！" style="center"/>)";
    goto close;
  }

  shellui_log("Get encoded account id");
  GetEncodedAccountID(AccountID, dec_account_id);
  shellui_log("Get encoded account id ==> %s", AccountID);
  remote_play_info = "账号 ID: " + std::string(AccountID);
  ss << std::hex << std::uppercase << dec_account_id;
  remote_play_info += "\n解码后账号 ID: " + ss.str();

  pinCode = GeneratePINCode();
  shellui_log("Pin code => %d", pinCode);

  sprintf(pin_code, "PIN 码  : %04d %04d    ", pinCode / 10000, pinCode % 10000);
  remote_play_info += "\n" + std::string(pin_code);
  xml_buffer += R"(<label id="id_pin" title=")" + std::string(pin_code) + R"(" style="center"/>)";
  shellui_log("Pin code str => %s", pin_code);

  xml_buffer += R"(<label id="base64_account_id" title="账号 ID: )";
  xml_buffer += std::string(AccountID) + R"(" style="center"/>)";

  if(usbpath() != -1)
      xml_buffer += R"(<button id="id_save_rp_info" title="将远程游玩详情保存到 USB" style="center"/>)";

close:
  xml_buffer += R"(</setting_list></system_settings>)";

  // shellui_log("%s\n", xml_buffer.c_str());
}

void generate_plugin_xml(std::string &xml_buffer, bool plugins_xml)
{
  struct dirent *entry;
  int toggle_switch_id = 1;

  std::vector<std::string> directories = {
      "/user/data/OrionHEN/plugins",
      "/usb0/OrionHEN/plugins",
      "/usb1/OrionHEN/plugins",
      "/usb2/OrionHEN/plugins",
      "/usb3/OrionHEN/plugins",

      "/user/data/OrionHEN/payloads",
      "/usb0/OrionHEN/payloads",
      "/usb1/OrionHEN/payloads",
      "/usb2/OrionHEN/payloads",
      "/usb3/OrionHEN/payloads"
    };

  xml_buffer = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
               "<system_settings version=\"1.0\" plugin=\"debug_settings_plugin\">\n"
               "\n";

  if (plugins_xml)
    xml_buffer += "<setting_list id=\"id_plugin\" title=\"插件\">\n";
  else
    xml_buffer += "<setting_list id=\"id_auto_plugins\" title=\"★ 插件 - 启动菜单\">\n";

  for (const auto &directory : directories)
  {
    DIR *dir = opendir(directory.c_str());
    // Open the directory
    if (!dir)
    {
      shellui_log("Failed to open directory: %s", directory.c_str());
      continue;
    }
    // Iterate over each file in the directory
    while ((entry = readdir(dir)) != nullptr)
    {
      bool is_elf = strstr(entry->d_name, ".elf") != NULL;
      if ((strstr(entry->d_name, ".plugin") || is_elf) && strstr(entry->d_name, ".auto_start") == NULL)
      {
        Plugins new_list;
        // Store the ID in the plugin_ids array
        CustomPluginHeader header = {};
        std::string toggle_switch;
        std::string id;
        std::string path = directory + "/" + entry->d_name;

        shellui_log("Found Plugin: %s", path.c_str());

        int fd = open(path.c_str(), O_RDONLY, 0);
        if (fd < 0)
        {
          shellui_log("Failed to open Plugin file");
          continue;
        }

        if (read(fd, (void *)&header, sizeof(CustomPluginHeader)) != sizeof(CustomPluginHeader))
        {
          shellui_log("Failed to read Plugin file, %s", path.c_str());
          close(fd);
          continue;
        }

        close(fd);

        if (!is_elf && !is_valid_plugin(header))
        {
          shellui_log("Invalid plugin file.");
          continue;
        }
        else if(is_elf){
          strncpy(header.prefix, "<elf>", 5);
          strncpy(header.plugin_version, "", 4);
        }
        shellui_log("Valid plugin file.");

        std::string shown_path = path; // Initialize with the original path
        //path before any edits for shellui
        new_list.shellui_path = path;

        const std::string prefix = "/user";
        if (path.find(prefix) == 0) { // Check if the path starts with "/user"
           shown_path = path.substr(prefix.length()); // Remove "/user"
        }

        shown_path = (path.substr(0, 4) == "/usb") ? "/mnt" + path : shown_path;

	      std::string version_str = !is_elf ? "(v" + std::string(header.plugin_version) + ")" : "";

        id = plugins_xml ? "id_plugin_" + std::to_string(toggle_switch_id++) : "id_auto_plugin_" + std::to_string(toggle_switch_id++);
        if (plugins_xml)
          toggle_switch = "<toggle_switch id=\"" + id + "\" title=\"" + entry->d_name + " " + version_str + "\" second_title=\"启动/停止 " + entry->d_name + " (路径: " + shown_path + ") (" + (is_elf ? entry->d_name : header.titleID) + ")\" value=\"0\"/>\n";
        else
          toggle_switch = "<toggle_switch id=\"" + id + "\" title=\"" + entry->d_name + " " + version_str + "\" second_title=\"启用/禁用 " + entry->d_name + " 的自动启动  (" + shown_path + ")\" value=\"0\"/>\n";

        xml_buffer += toggle_switch;
        new_list.tid = (is_elf ? entry->d_name : header.titleID);
        new_list.path = shown_path;
        new_list.name = entry->d_name;
        new_list.version = header.plugin_version;
        new_list.id = id;
        plugins_xml ? plugins_list.push_back(new_list) : auto_list.push_back(new_list);
      }
    }
    closedir(dir);
  }

  if (plugins_xml)
  {
    xml_buffer += "<link id=\"id_auto_plugins\" title=\"★ 插件 - 启动菜单\" file=\"auto_plugins.xml\" second_title=\"配置在加载 OrionHEN 时自动启动的插件\"/>\n";
    xml_buffer += "</setting_list>\n</setting_list>\n</system_settings> ";
  }
  else
  {
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

void generate_cheats_xml(std::string &new_xml, std::string& not_open_tid, bool running_as_debug_settings, bool show_while_not_open)
{
  int appid = -1;
  std::string list_id = running_as_debug_settings ? "id_debug_settings" : "id_cheat_title";

  // buttons for if nothing is found
  /* Download only; no RELOAD/index rebuild — util hot-reloads on file signature. */
  std::string dl_cheats = R"(<list id="id_selected_cheats_repo" title="金手指仓库来源" >
                   <list_item id="id_selected_cheats_repo_1" title="OrionHEN PS5 金手指仓库" value="0"/>
                   <list_item id="id_selected_cheats_repo_2" title="GoldHEN PS4 金手指仓库" value="1"/>
                 </list>
                <button id="id_dl_cheats" title="下载/更新金手指" second_title="从所选 GitHub 仓库下载到 /data/OrionHEN/cheats/（TITLEID_VERSION.ext）"/>)";
  //

  new_xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
      "<system_settings version=\"1.0\" plugin=\"debug_settings_plugin\">\n"
      "\n";

  is_game_open = Get_Running_App_TID(running_tid, appid);
  is_current_game_open = (is_game_open && running_tid == (show_while_not_open ? not_open_tid : running_tid));

  if (!is_game_open && !show_while_not_open)
  {
    new_xml += "<setting_list id=\"" + list_id + "\" title=\"OrionHEN 金手指 - 当前"
               "没有打开的游戏\">\n";
    new_xml += dl_cheats;
  }
  else
  {
    std::string game_ver;
    std::string cheat_info_json;
    IPC_Client &client = IPC_Client::getInstance(true);

    running_tid = show_while_not_open ? not_open_tid : running_tid;

    if (!client.GameVerFromTid(running_tid, game_ver))
    {
      game_ver = "无法检测补丁版本";
    } 

    new_xml += "<setting_list id=\"" + list_id + "\" title=\"OrionHEN 金手指 - ";
    new_xml += running_tid + " - " + game_ver + "\">\n";

    if(!is_game_open && show_while_not_open)
      new_xml += R"(<label id="id_cheat_disclaimer" title=")" + running_tid + R"( 当前未运行，除非打开游戏否则无法激活任何金手指")" + R"( style="center"/>)";

    if (client.GetGameCheats(running_tid, game_ver, cheat_info_json))
    {
      struct stat st;

      if (stat(cheat_info_json.c_str(), &st) == -1)
      {
        shellui_log("Unable to stat file %s", cheat_info_json.c_str());
        goto close;
      }

      int fd = open(cheat_info_json.c_str(), O_RDONLY);

      if (fd == -1)
      {
        shellui_log("Error reading %s file!", cheat_info_json.c_str());
        goto close;
      }

      char* json_data = (char*) malloc(st.st_size);
      // Write the buffer to the file
      if (read(fd, json_data, st.st_size) == -1) 
      {
        perror("read failed");
        close(fd);
        free(json_data);
        goto close;
      }

      // Close the file descriptor
      close(fd);
      unlink(cheat_info_json.c_str());
      std::string json_string(json_data, st.st_size);
      orion_cjson::Root res_json(json_string);
      if (!res_json)
      {
        shellui_log("Failed to parse json from cheat response!");
        free(json_data);
        goto close;
      }

      std::string game_name =
          orion_cjson::string_item(res_json.get(), "name", "");
      escapeXML(game_name);

      new_xml += R"(<label id="id_cheat_title" title="★ )" + game_name + R"( ★" style="center"/>)";

      // Cheat creator credits
      new_xml += R"(<label id="credits" style="center" title="金手指作者: )";

      std::unordered_map<std::string, bool> knownAuthors;
      bool first_author = true;
      cJSON *authors = orion_cjson::item(res_json.get(), "authors");
      if (cJSON_IsArray(authors))
      {
          cJSON *author = nullptr;
          cJSON_ArrayForEach(author, authors)
          {
              const char *author_value = orion_cjson::string_value(author);
              if (!author_value)
                continue;

              std::string author_name = author_value;
              
              if (knownAuthors.find(author_name) != knownAuthors.end())
              {
                //
                // repeated
                //
                continue;
              }
              knownAuthors[author_name] = true;
              escapeXML(author_name);
              if (!first_author)
              {
                  new_xml += ", ";
              }
              new_xml += author_name;
              first_author = false;
          }
      }
      new_xml += R"(" />)";

      // Build toggle switch XML entry
      cJSON *cheats = orion_cjson::item(res_json.get(), "cheats");
      if (cJSON_IsArray(cheats))
      {
          cJSON *cheat_entry = nullptr;
          cJSON_ArrayForEach(cheat_entry, cheats)
          {
              std::string cheat_name =
                  orion_cjson::string_item(cheat_entry, "name", "");
              std::string description =
                  orion_cjson::string_item(cheat_entry, "description", "开/关");
              escapeXML(cheat_name);
              escapeXML(description);

              int cheat_id = orion_cjson::int_item(cheat_entry, "id");
              bool enabled = orion_cjson::bool_item(cheat_entry, "enabled");
              std::string enabled_value = enabled ? "1" : "0";
              std::string toggle_switch;
              if(is_game_open && is_current_game_open)
                 toggle_switch = R"(<toggle_switch id="id_cheat_)" + running_tid + "_" + std::to_string(cheat_id) + R"(" icon="tex_game_icon" title=")" + cheat_name + R"(" description=")" + description + R"(" value=")" + enabled_value + R"("/>)";
              else
                  toggle_switch = R"(<button id="id_cheat_)" + running_tid + "_" + std::to_string(cheat_id) + R"(" icon="tex_game_icon" title=")" + cheat_name + R"(" description=")" + description + R"(" second_title="为 )" + game_name + R"( 启用/禁用 )" + cheat_name + R"(" />)";

              new_xml += toggle_switch;

              cheatEnabledMap[cheat_id] = enabled;
          }
      }

      // Cleanup
      free(json_data);
    }
    else{
      new_xml += dl_cheats;
    }
  }
close:
  new_xml += "</setting_list>\n</system_settings>";

//  shellui_log("Cheat UI XML => \n%s\n", new_xml.c_str());
}

void generate_plapps_xml(std::string& new_xml) {

  struct dirent *entry;

  std::vector<std::string> directories = {
    "/user/data/homebrew/games",
    "/usb0/homebrew",
    "/usb1/homebrew/games",
    "/usb2/homebrew/games",
    "/usb3/homebrew/games",
    "/mnt/ext1/homebrew/games",
    "/mnt/ext2/homebrew/games",
    "/mnt/ext0/homebrew/games",
  };

    new_xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
      "<system_settings version=\"1.0\" plugin=\"debug_settings_plugin\">\n"
      "\n";

    new_xml += "<setting_list id=\"id_plapps\" title=\"OrionHEN Payload 自制软件 - 应用程序\">\n";

  // Initialize random number generator
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(1000, 9999);

  for (const auto &directory : directories)
  {
    DIR *dir = opendir(directory.c_str());
    // Open the directory
    if (!dir)
    {
      #if SHELL_DEBUG==1 
      shellui_log("Failed to open directory: %s", directory.c_str());
      #endif
      continue;
    }
    
    // Iterate over each entry in the games directory
    while ((entry = readdir(dir)) != nullptr)
    {
      // Skip . and .. directories
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
        
      std::string game_dir = directory + "/" + entry->d_name;
      
      // Check if this is a directory by trying to open it
      struct stat st;
      if (stat(game_dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        #if SHELL_DEBUG==1 
        shellui_log("Skipping non-directory: %s", game_dir.c_str());
        #endif
        continue;
      }
        
      std::string elf_path = game_dir + "/eboot.elf";
      std::string icon_path = game_dir + "/sce_sys/icon0.png";
      
      // Check if param.json exists
      if (access(elf_path.c_str(), F_OK) != 0) {
        #if SHELL_DEBUG==1 
        shellui_log("No param.json found in: %s", game_dir.c_str());
        #endif
        continue;
      }
      #if SHELL_DEBUG==1 
      shellui_log("Found Game: %s", game_dir.c_str());
      #endif
      
      // Parse the JSON to get title_id, content_id, title, and version
      std::string title_id, title, ver;
            #if 0
      if (!getContentInfofromJson(param_path, title_id, title, ver)) {
        #if SHELL_DEBUG==1 
        shellui_log("Failed to parse param.json in: %s", game_dir.c_str());
        #endif
        continue;
      }
      #endif
      
      std::string shown_path = game_dir; // Initialize with the original path
      
      const std::string prefix = "/user";
      if (shown_path.find(prefix) == 0) { // Check if the path starts with "/user"
         shown_path = shown_path.substr(prefix.length()); // Remove "/user"
      }
      
      shown_path = (game_dir.substr(0, 4) == "/usb") ? "/mnt" + game_dir : shown_path;
      // Generate a random number for the ID
      int random_num = dist(gen);
      
      // Escape the icon path for XML
      escapeXML(icon_path);
      
      // Create and populate a GameEntry
      GameEntry game;
      game.tid = title_id;
      game.title = title;
      escapeXML(game.title);
      game.version = ver;
      game.path = shown_path;
      game.dir_name = entry->d_name;
      escapeXML(game.dir_name);
      game.icon_path = icon_path;
      game.id = "id_orionhen_pl_loader_" + title_id + "_" + std::to_string(random_num);
      
      // Add to the games list
      games_list.push_back(game);
      
      // Format the button XML
      std::string button = "<button id=\"" + game.id + "\" title=\"(" + title_id + ") " + title + 
      "\" icon=\"" + icon_path + "\" second_title=\"" + shown_path + " | 版本: " + ver + "\"/>\n";
      
      new_xml += button;
    }
    //shellui_log("cloaing dir %s", directory.c_str());
    closedir(dir);
  }

    new_xml += "</setting_list>\n</system_settings>";
}

