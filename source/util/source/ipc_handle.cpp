/* Copyright (C) 2025 OrionHEN / LightningMods
 * Util daemon IPC command dispatch.
 * Transport (listen/accept/thread) stays in msg.cpp.
 */
#include <orion/platform.h>
#include "ipc.hpp"
#include <msg.hpp>
#include <orion/settings.hpp>
#include "common_utils.h"
#include <signal.h>
#include <stdint.h>
#include <unistd.h>
extern "C" {
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
}
#include "../../extern/cJSON/orion_cjson.hpp"
#include "cheats/CheatService.hpp"
#include "cheats/runtime.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <fstream>
#include <memory>
#include <sfo.hpp>
#include <sstream>
#include <atomic>
#include <string>
#include <vector>

extern pthread_t cmd_server;
void* runCommandNControlServer(void*);
extern atomic_bool no_network_rest_mode_action, real_rest_mode_detected;
extern bool is_handler_enabled;
// g_settings from common_utils.h; concurrent CMD flags:
extern atomic_bool g_legacy_cmd_server;
extern atomic_bool g_legacy_cmd_server_exit;

void reply(int sender_socket, bool error, std::string out_var = "Nothing");
extern "C" {
bool load_payload(const char *path);
int launchApp(const char *titleId);
bool download_file(const char *url, const char *dst);
bool extract_zip(const char *zip_path, const char *out_dir);
bool check_for_new_commit(int repo);
extern char ip_address[];
}
std::string GetPS5Version(const std::string &jsonpath);
std::vector<uint8_t> readFile(std::string filename);

void handleIPC(clientArgs *client, std::string &inputStr,
               DaemonCommands command) {

  int sender_app = client->socket;

  std::string path_buf, path_buf2, json_path;

  char temp[0x255];
  std::string out_var = "Nothing"; // default send var

  OrionHEN_log("Received IPC command 0x%X", command);
  // OrionHEN_log("Received IPC data: %s", inputStr.c_str());

  orion_cjson::Root my_json(inputStr);
  if (!my_json) {
    OrionHEN_log("Error parsing JSON");
    orion_notify(true, "Error parsing JSON");
    reply(sender_app, true);
    return;
  }

  switch (command) {
  case BREW_UTIL_TEST_CONNECTION: {
    reply(sender_app, false, out_var);
    break;
  }
  case BREW_UTIL_SHELLUI_ON_STANDBY: {
    OrionHEN_log("ShellUI on standby");
    real_rest_mode_detected = no_network_rest_mode_action = true;
    reply(sender_app, false);
    break;
  }
  case BREW_UTIL_UNUSED_FTP:
  case BREW_UTIL_UNUSED_KLOG:
  case BREW_UTIL_UNUSED_DPI:
    /* FTP (1337), Klog (9081), and DirectPKGInstaller removed; ordinals kept for IPC compat. */
    OrionHEN_log("Removed-service toggle: unsupported (cmd=%u)", static_cast<unsigned>(command));
    reply(sender_app, true);
    break;
  case BREW_UTIL_DAEMON_PID: {
    snprintf(temp, sizeof(temp), "%d", getpid());
    reply(sender_app, false, temp);
    break;
  }
  case BREW_UTIL_GET_GAME_VER: {
    auto tid = std::string(orion_cjson::string_item(my_json.get(), "tid", ""));
    if (tid.empty()) {
      orion_notify(true, "Failed to get tid");
      reply(sender_app, true);
      break;
    }

    std::string tmp, game_version;
    bool is_PS5 = tid.rfind("PPSA", 0) == 0; // Check if tid starts with "PPSA"
    if (is_PS5) {
      // Attempt to load JSON files for PS5 games
      tmp = "/system_data/priv/appmeta/" + tid + "/param.json";
      if (!if_exists(tmp.c_str())) {
        OrionHEN_log("%s: json %s does not exist", tid.c_str(), tmp.c_str());
        tmp = "/system_data/priv/appmeta/external/" + tid + "/param.json";

        if (!if_exists(tmp.c_str())) {
          OrionHEN_log("%s: json %s does not exist", tid.c_str(), tmp.c_str());
          tmp = "/system_ex/app/" + tid + "/sce_sys/param.json";
          if (!if_exists(tmp.c_str())) {
            OrionHEN_log("%s: json %s does not exist", tid.c_str(), tmp.c_str());
            orion_notify(true, "Failed to get game version");
            reply(sender_app, true);
            break;
          }
        }
      }

      game_version = GetPS5Version(tmp);
      if (game_version.empty()) {
        orion_notify(true, "Failed to get game version");
        OrionHEN_log("Failed to get game version for PS5 Game");
        reply(sender_app, true);
        break;
      }
    } else {
      // Attempt to load SFO files for PS4 games
      tmp = "/system_data/priv/appmeta/" + tid + "/param.sfo";
      if (!if_exists(tmp.c_str())) {
        OrionHEN_log("%s: sfo %s does not exist", tid.c_str(), tmp.c_str());
        tmp = "/system_data/priv/appmeta/external/" + tid + "/param.sfo";
        if (!if_exists(tmp.c_str())) {
          OrionHEN_log("%s: sfo %s does not exist", tid.c_str(), tmp.c_str());
          orion_notify(true, "Failed to get game version");
          reply(sender_app, true);
          break;
        }
      }

      std::vector<uint8_t> sfo_data = readFile(tmp);
      if (sfo_data.empty()) {
        orion_notify(true, "Failed to read SFO file");
        reply(sender_app, true);
        break;
      }

      SfoReader sfo(sfo_data);
      // VERSION key holds the original version, it doesn't change if updated
      try {
          std::string version_str = sfo.GetValueFor<std::string>("VERSION");
          std::string app_ver_str = sfo.GetValueFor<std::string>("APP_VER");

          float version_val = std::stof(version_str);
          float app_ver_val = std::stof(app_ver_str);

          game_version = (version_val > app_ver_val) ? version_str : app_ver_str;
      }
      catch (const std::exception& e) {
          // Fallback to APP_VER if there's an issue
          game_version = sfo.GetValueFor<std::string>("APP_VER");
      }
    }

    OrionHEN_log("Version: %s", game_version.c_str());
    reply(sender_app, false, game_version);

    break;
  }
  case BREW_UTIL_LAUNCH_PAYLOAD: {
    std::string payload_path =
        std::string(orion_cjson::string_item(my_json.get(), "payload_path", ""));
    std::string title_id =
        std::string(orion_cjson::string_item(my_json.get(), "title_id", ""));
    OrionHEN_log("Launching payload %s (key: %s)", payload_path.c_str(),
                 title_id.c_str());
    if (!load_payload(payload_path.c_str())) {
      orion_notify(true, "Failed to load payload\nPath: %s\nKey: %s",
                   payload_path.c_str(), title_id.c_str());
      reply(sender_app, true);
      break;
    }
    orion_notify(true, "Payload launched\nPath: %s\nKey: %s",
                 payload_path.c_str(), title_id.c_str());
    reply(sender_app, false);
    break;
  }

  case BREW_UTIL_GET_GAME_CHEAT: {
    std::string title_id =
        std::string(orion_cjson::string_item(my_json.get(), "tid", ""));
    std::string version =
        std::string(orion_cjson::string_item(my_json.get(), "version", ""));
    int pid = orion_cjson::int_item(my_json.get(), "pid");
    int appid = orion_cjson::int_item(my_json.get(), "appid");
    std::string shm_path = "/user/data/OrionHEN/" + title_id + "_cheats";

    auto &cheats = orion::cheats::CheatService::instance();
    cheats.ensureDir();
    if (cheats.exportList(title_id, version, pid, appid, shm_path) == 0) {
      reply(sender_app, false, shm_path);
    } else {
      orion_notify(true, "No cheats available for %s version %s!", title_id.c_str(),
             version.c_str());
      reply(sender_app, true);
    }
    break;
  }

  case BREW_UTIL_TOGGLE_CHEAT: {
    std::string title_id =
        std::string(orion_cjson::string_item(my_json.get(), "tid", ""));
    std::string version =
        std::string(orion_cjson::string_item(my_json.get(), "version", ""));
    int pid = orion_cjson::int_item(my_json.get(), "pid");
    int appid = orion_cjson::int_item(my_json.get(), "appid");
    int cheat_id = orion_cjson::int_item(my_json.get(), "cheat_id");
    std::string status;

    OrionHEN_log("Received toggle command for cheat %d on %s PID %d",
                 cheat_id, title_id.c_str(), pid);

    auto &cheats = orion::cheats::CheatService::instance();
    if (cheats.toggle(pid, appid, title_id, version, cheat_id, status) == 0) {
      OrionHEN_log("Cheat toggle ok: %s", status.c_str());
      reply(sender_app, false, status);
    } else {
      OrionHEN_log("Cheat toggle failed: %s", status.c_str());
      reply(sender_app, true, status);
    }
    break;
  }
  case BREW_UTIL_LAUNCH_ELFLDR:
    /* 9021 elfldr service removed from OrionHEN — not bundled. */
    OrionHEN_log("BREW_UTIL_LAUNCH_ELFLDR: unsupported (no 9021 service)");
    reply(sender_app, true);
    break;
  case BREW_UTIL_DOWNLOAD_CHEATS: {
    int repo = orion_cjson::int_item(my_json.get(), "repo");
    const char *staging = "/data/OrionHEN/cheats_staging";

    if(!check_for_new_commit(repo)){
      OrionHEN_log("Failed to check for new commit or is up to date");
      reply(sender_app, false);
      break;
    }
    orion_notify(true, "Downloading the latest %s Cheats repo....", repo ? "GoldHEN PS4" : "OrionHEN PS5");
    if (!download_file(repo ? "https://api.github.com/repos/GoldHEN/GoldHEN_Cheat_Repository/zipball" : "https://api.github.com/repos/OrionHEN/PS5_Cheats/zipball",
                       "/data/OrionHEN/cheats.zip")) {
      OrionHEN_log("Failed to download cheats");
      reply(sender_app, true);
      break;
    }
    mkdir(ORION_DATA_ROOT, 0777);
    mkdir(staging, 0777);
    OrionHEN_log("Extracting Zip to staging folder");
    if (!extract_zip("/data/OrionHEN/cheats.zip", staging)) {
      OrionHEN_log("Failed to extract zip");
      reply(sender_app, true);
      break;
    }

    unlink("/data/OrionHEN/cheats.zip");
    auto &cheats = orion::cheats::CheatService::instance();
    cheats.ensureDir();
    if (cheats.flattenInstallTree(staging) < 0) {
      orion_notify(true, "Downloaded repo but no flat cheat files were installed");
      reply(sender_app, true);
      break;
    }
    orion_notify(true, "Successfully installed cheats to %s (flat TITLE_VERSION.ext)",
           ORION_CHEATS_DIR);
    reply(sender_app, false);
    break;
  }
  case BREW_UTIL_DOWNLOAD_KSTUFF: {
      orion_notify(true, "Attempting to Download kstuff ...");
      if (!download_file("https://github.com/EchoStretch/kstuff/releases/latest/download/kstuff.elf",
          "/data/OrionHEN/kstuff.elf")) {
		  unlink("/data/OrionHEN/kstuff.elf");
          OrionHEN_log("Failed to download kstuff");
          reply(sender_app, true);
          break;
      }

      orion_notify(true, "Successfully downloaded latest kstuff");
      reply(sender_app, false);
      break;
  }
  case BREW_UTIL_UNUSED_RELOAD_CHEATS:
    /* Old full-tree index rebuild removed; load uses file signature hot-reload. */
    OrionHEN_log("RELOAD_CHEATS: unsupported (hot-reload only)");
    reply(sender_app, true);
    break;
  case BREW_UTIL_TOGGLE_LEGACY_CMD_SERVER: {
    bool turn_on = orion_cjson::bool_item(my_json.get(), "toggle");
    OrionHEN_log("Legacy Command Server toggle: %d", turn_on);
    if (turn_on) {
      orion_notify(true, "Legacy Command Server Enabled");
      g_legacy_cmd_server = true;
      g_legacy_cmd_server_exit = true;
    } else {
	  // dont exit server because its used to detect rest mode too 
      // just stop handling commands
      g_legacy_cmd_server = false;
      orion_notify(true, "Legacy Command Server Disabled");
    }
    reply(sender_app, false);
	break;
  }
  case BREW_KILL_DAEMON:{
    is_handler_enabled = false;
    exit(1337);
    kill(getpid(), SIGKILL);
    reply(sender_app, false);
    break;
  }
  case BREW_RELOAD_SETTINGS: {
    LoadSettings();
    //orion_notify(true, "Reloaded Settings");
    reply(sender_app, false);
    break;
  }
  default:
    orion_notify(true, "Unknown command 0x%X", command);
    reply(sender_app, true);
    break;
  }
}

