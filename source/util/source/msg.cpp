
/* Copyright (C) 2025 OrionHEN / LightningMods

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */

#include "ipc.hpp"
#include <msg.hpp>
#include <signal.h>
#include <stdint.h>
#include <unistd.h>
extern "C" {
#include "common_utils.h"
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>

int sceKernelMprotect(void *addr, size_t len, int prot);


int sceSystemServiceLoadExec(const char *path, const char *arg);
extern bool is_handler_enabled;
}
#include "../../extern/cJSON/orion_cjson.hpp"
#include <CheatManager.hpp>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <sfo.hpp>
#include <sstream>

extern pthread_t cmd_server;
void* runCommandNControlServer(void*);
// pop -Winfinite-recursion error for this func for clang
#define MB(x) ((size_t)(x) << 20)
#define READ_SIZE 0x1024

extern atomic_bool no_network_rest_mode_action, real_rest_mode_detected;

extern int shellui_pid_for_comp;
extern uintptr_t code_addr;

extern char ip_address[];

int DaemonSocket = 0;

bool startDirectPKGInstaller(bool is_v2);
bool if_exists(const char *path);

extern "C" int launchApp(const char *titleId);

bool if_exists(const char *path);
void activate_shellui_patch();
bool LoadSettings();

bool rmtree(const char *path) {
  DIR *dir = opendir(path);
  if (dir == NULL) {
    OrionHEN_log("Error opening directory %s", path);
    return false;
  }

  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL) {
    // Skip "." and ".." entries
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char path_1[1000];
    snprintf(path_1, sizeof(path_1), "%s/%s", path, entry->d_name);

    if (entry->d_type == DT_DIR) {
      // Recursive call for subdirectories
      rmtree(path_1);
    } else {
      // Delete files
      if (unlink(path_1) != 0) {
        // perror("Error deleting file");
        OrionHEN_log("Error deleting file %s", path);
      }
    }
  }

  closedir(dir);

  // Delete the empty folder
  if (rmdir(path) != 0) {
    // perror("Error deleting folder");
    OrionHEN_log("Error deleting folder %s", path);
  }

  return true;
}

struct sockaddr_in networkAdress(uint16_t port) {
  struct sockaddr_in address;
  address.sin_len = sizeof(address);
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  memset(address.sin_zero, 0, sizeof(address.sin_zero));
  return address;
}

int networkListen(const char *soc_path) {
  struct sockaddr_un server;
  unlink(soc_path);
  OrionHEN_log("[Daemon] Deleted Socket...");
  int s = socket(AF_UNIX, SOCK_STREAM, 0);
  if (s < 0) {
    OrionHEN_log("[Daemon] Socket failed! %s", strerror(errno));
    return INVAIL;
  }

  memset(&server, 0, sizeof(server));
  server.sun_family = AF_UNIX;
  strcpy(server.sun_path, soc_path);

  int r = bind(s, (struct sockaddr *)&server, SUN_LEN(&server));
  if (r < 0) {
    OrionHEN_log("[Daemon] Bind failed! %s", strerror(errno));
    return INVAIL;
  }

 // OrionHEN_log("Socket has name %s", server.sun_path);

  r = listen(s, 100);
  if (r < 0) {
    OrionHEN_log("[Daemon] listen failed! %s", strerror(errno));
    return INVAIL;
  }

  return s;
}

int networkAccept(int socket) {
  return accept(socket, 0, 0);
}

int networkReceiveData(int socket, void *buffer, int32_t size) {
  int nu = recv(socket, buffer, size, 0);
  OrionHEN_log("got %i bytes", nu);
  return nu;
}

int networkSendData(int socket, void *buffer, int32_t size) {
  return send(socket, buffer, size, MSG_NOSIGNAL);
}

int networkSendDebugData(void *buffer, int32_t size) {
  return networkSendData(DaemonSocket, buffer, size);
}

int networkCloseConnection(int socket) { return close(socket); }

int networkCloseDebugConnection() {
  return networkCloseConnection(DaemonSocket);
}

void reply(int sender_socket, bool error, std::string out_var = "Nothing") {

  std::string inputStr = "{\"res\":" + std::to_string(error ? -1 : 0) +
                         ", \"var\":\"" + out_var + "\"}";

  IPCMessage outputMessage;
  outputMessage.cmd = BREW_UTIL_RETURN_VALUE;
  outputMessage.error = error ? -1 : 0;
  OrionHEN_log("error: %d", outputMessage.error);
  if (!inputStr.empty()) {
    strncpy(outputMessage.msg, inputStr.c_str(), sizeof(outputMessage.msg) - 1);
    // Null-terminate the destination array
    outputMessage.msg[sizeof(outputMessage.msg) - 1] = '\0';
  }

  networkSendData(sender_socket, reinterpret_cast<void *>(&outputMessage),
                  sizeof(outputMessage));
}

std::vector<uint8_t> readFile(std::string filename) {
  // open the file:
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    OrionHEN_log("Failed to open %s", filename.c_str());
    return std::vector<uint8_t>();
  }

  // Stop eating new lines in binary mode!!!
  file.unsetf(std::ios::skipws);

  // get its size:
  std::streampos fileSize;

  file.seekg(0, std::ios::end);
  fileSize = file.tellg();
  file.seekg(0, std::ios::beg);

  // reserve capacity
  std::vector<uint8_t> vec;

  vec.reserve(fileSize);

  // read the data:
  vec.insert(vec.begin(), std::istream_iterator<uint8_t>(file),
             std::istream_iterator<uint8_t>());

  return vec;
}

std::string GetPS5Version(const std::string &jsonpath) {
  try {
    std::ifstream input_file(jsonpath);
    if (!input_file.is_open()) {
      OrionHEN_log("Failed to open file for reading: %s", jsonpath.c_str());
      return "Error Opening Json";
    }

    std::stringstream buffer;
    buffer << input_file.rdbuf();
    input_file.close();

    orion_cjson::Root j(buffer.str());
    const char *content_version =
        j ? orion_cjson::string_item(j.get(), "contentVersion") : nullptr;
    if (content_version)
      return std::string(content_version);

  } catch (const std::exception &e) {
    // Handle exceptions here, you can log the error or perform other error
    // handling tasks
    OrionHEN_log("An exception occurred: %s", e.what());
    return "Error getting version";
  }

  return "Error getting version";
}

// Callback function to write received data
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  FILE *fp = (FILE *)userp;
  return fwrite(contents, size, nmemb, fp);
}

void handleIPC(struct clientArgs *client, std::string &inputStr,
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
    notify(true, "Error parsing JSON");
    reply(sender_app, true);
    return;
  }

  switch (command) {
  case BREW_UTIL_SHELLUI_ON_STANDBY: {
    OrionHEN_log("ShellUI on standby");
    real_rest_mode_detected = no_network_rest_mode_action = true;
    reply(sender_app, false);
    break;
  }
  case BREW_UTIL_UNUSED_FTP:
  case BREW_UTIL_UNUSED_KLOG:
    /* FTP (1337) and Klog (9081) servers removed from OrionHEN. */
    OrionHEN_log("FTP/Klog toggle: unsupported (services removed)");
    reply(sender_app, true);
    break;
  case BREW_UTIL_TOGGLE_DPI: {
    bool turn_on = orion_cjson::bool_item(my_json.get(), "toggle");
    bool is_v2 = orion_cjson::bool_item(my_json.get(), "is_v2");
    OrionHEN_log("DPI toggle: %d | is_v2 %s", turn_on, is_v2 ? "true" : "false");
    if (turn_on) {
      if (startDirectPKGInstaller(is_v2)) {
        notify(true,
               is_v2 ? "Direct PKG Installer V2 Server Started\nWebUI: "
                       "http://%s:12800 "
                     : "Direct PKG Installer Server Started\nIP: %s Port: 9090",
               ip_address);
        reply(sender_app, false);
      } else
        reply(sender_app, true);
    } else {
      shutdownDirectPKGInstaller(is_v2);
      notify(true, is_v2 ? "Direct PKG Installer V2 Server Stopped"
                         : "Direct PKG Installer Server Stopped");
      reply(sender_app, false);
    }
    break;
  }
  case BREW_UTIL_DAEMON_PID: {
    snprintf(temp, sizeof(temp), "%d", getpid());
    reply(sender_app, false, temp);
    break;
  }
  case BREW_UTIL_GET_GAME_VER: {
    auto tid = std::string(orion_cjson::string_item(my_json.get(), "tid", ""));
    if (tid.empty()) {
      notify(true, "Failed to get tid");
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
            notify(true, "Failed to get game version");
            reply(sender_app, true);
            break;
          }
        }
      }

      game_version = GetPS5Version(tmp);
      if (game_version.empty()) {
        notify(true, "Failed to get game version");
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
          notify(true, "Failed to get game version");
          reply(sender_app, true);
          break;
        }
      }

      std::vector<uint8_t> sfo_data = readFile(tmp);
      if (sfo_data.empty()) {
        notify(true, "Failed to read SFO file");
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
  case BREW_UTIL_LAUNCH_PLUGIN: {
    std::string plugin_path =
        std::string(orion_cjson::string_item(my_json.get(), "plugin_path", ""));
    std::string title_id =
        std::string(orion_cjson::string_item(my_json.get(), "title_id", ""));
    OrionHEN_log("Launching %s (TID: %s)", plugin_path.c_str(),
               title_id.c_str());
    if (!load_plugin(plugin_path.c_str())) {
      notify(true, "Failed to Load in\nPath: %s\nTID: %s",
             plugin_path.c_str(), title_id.c_str());
      reply(sender_app, true);
      break;
    }
    notify(true, "Plugin or ELF launched successfully\nPath: %s\nTID: %s",
           plugin_path.c_str(), title_id.c_str());
    reply(sender_app, false);
    break;
  }

  case BREW_UTIL_GET_GAME_CHEAT: {
    std::string title_id =
        std::string(orion_cjson::string_item(my_json.get(), "tid", ""));
    std::string version =
        std::string(orion_cjson::string_item(my_json.get(), "version", ""));
    GameCheat *cheat = CheatManager::GetGameCheat(title_id, version);

    if (cheat) {
      //
      // Build json response, we need escape the quotes because the IPC response
      // is also between quotes, which break the JSON response
      //
      cJSON *res_json = cJSON_CreateObject();
      cJSON *cheats = cJSON_AddArrayToObject(res_json, "cheats");
      cJSON *authors = cJSON_AddArrayToObject(res_json, "authors");
      cJSON_AddStringToObject(res_json, "name", cheat->name.c_str());

      for (size_t i = 0; i < cheat->cheats.size(); ++i) {
        cJSON *cheat_entry = cJSON_CreateObject();
        cJSON_AddStringToObject(cheat_entry, "name", cheat->cheats[i].name.c_str());
        cJSON_AddNumberToObject(cheat_entry, "id", static_cast<int>(i));
        cJSON_AddBoolToObject(cheat_entry, "enabled", cheat->cheats[i].enabled);
        cJSON_AddStringToObject(cheat_entry, "description",
                                cheat->cheats[i].description.c_str());
        cJSON_AddItemToArray(cheats, cheat_entry);
      }

      for (size_t i = 0; i < cheat->authors.size(); ++i) {
        cJSON_AddItemToArray(authors, cJSON_CreateString(cheat->authors[i].c_str()));
      }

      orion_cjson::Printed printed(res_json);
      std::string res = printed.str();
      cJSON_Delete(res_json);
      #if SHELL_DEBUG == 1
      OrionHEN_log("Response json => %s (%d bytes)", res.c_str(), res.size());
      #endif

      //
      // Create a shared file contained the parsed cheat
      //

      std::string shm_path = "/user/data/OrionHEN/" + title_id + "_cheats";
      unlink(shm_path.c_str());

      int fd = open(shm_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
      if (fd >= 0) {
        // Write the buffer to the file
        if (write(fd, res.c_str(), res.length()) == -1) {
          perror("write failed");
        }
        // Close the file descriptor
        close(fd);
      }

      reply(sender_app, false, shm_path);

    } else {
      notify(true, "No cheats available for %s version %s!", title_id.c_str(),
             version.c_str());
      reply(sender_app, true);
    }

    break;
  }

  case BREW_UTIL_TOGGLE_CHEAT: {
    std::string title_id =
        std::string(orion_cjson::string_item(my_json.get(), "tid", ""));
    int pid = orion_cjson::int_item(my_json.get(), "pid");
    int cheat_id = orion_cjson::int_item(my_json.get(), "cheat_id");
    std::string cheat_name;

    OrionHEN_log("Received toggle command for cheat %d on %s PID %d ID %d",
               cheat_id, title_id.c_str(), pid, cheat_id);

    if (CheatManager::ToggleCheat(pid, title_id, cheat_id, cheat_name)) {
      OrionHEN_log("Cheat successfully activated!");
      reply(sender_app, false, cheat_name);
    } else {
      reply(sender_app, true);
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

    if(!check_for_new_commit(repo)){
      OrionHEN_log("Failed to check for new commit or is up to date");
      reply(sender_app, false);
      break;
    }
    notify(true, "Downloading the latest %s Cheats repo....", repo ? "GoldHEN PS4" : "OrionHEN PS5");
    if (!download_file(repo ? "https://api.github.com/repos/GoldHEN/GoldHEN_Cheat_Repository/zipball" : "https://api.github.com/repos/OrionHEN/PS5_Cheats/zipball",
                       "/data/OrionHEN/cheats.zip")) {
      OrionHEN_log("Failed to download cheats");
      reply(sender_app, true);
      break;
    }
    OrionHEN_log("Extracting Zip to the cheats folder");
    if (!extract_zip("/data/OrionHEN/cheats.zip", "/data/OrionHEN/cheats")) {
      OrionHEN_log("Failed to extract zip");
      reply(sender_app, true);
      break;
    }

    unlink("/data/OrionHEN/cheats.zip");
    MakeInitialCheatCache(NULL);
    notify(true, "Successfully updated & refreshed the OrionHEN Cheats with the latest cheats repo");
    reply(sender_app, false);
    break;
  }
  case BREW_UTIL_DOWNLOAD_KSTUFF: {
      notify(true, "Attempting to Download kstuff ...");
      if (!download_file("https://github.com/EchoStretch/kstuff/releases/latest/download/kstuff.elf",
          "/data/OrionHEN/kstuff.elf")) {
		  unlink("/data/OrionHEN/kstuff.elf");
          OrionHEN_log("Failed to download kstuff");
          reply(sender_app, true);
          break;
      }

      notify(true, "Successfully downloaded latest kstuff");
      reply(sender_app, false);
      break;
  }
  case BREW_UTIL_RELOAD_CHEATS: {
    notify(true, "Reloading cheats cache");
    ReloadCheatsCache(NULL);
    reply(sender_app, false);
    break;
  }
  case BREW_UTIL_TOGGLE_LEGACY_CMD_SERVER: {
    bool turn_on = orion_cjson::bool_item(my_json.get(), "toggle");
    OrionHEN_log("Legacy Command Server toggle: %d", turn_on);
    if (turn_on) {
      notify(true, "Legacy Command Server Enabled");
      global_conf.legacy_cmd_server = true;
      global_conf.legacy_cmd_server_exit = true;
    } else {
	  // dont exit server because its used to detect rest mode too 
      // just stop handling commands
      global_conf.legacy_cmd_server = false;
      notify(true, "Legacy Command Server Disabled");
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
    //notify(true, "Reloaded Settings");
    reply(sender_app, false);
    break;
  }
  default:
    notify(true, "Unknown command 0x%X", command);
    reply(sender_app, true);
    break;
  }
}

void *ipc_client(void *args) {
  struct clientArgs *client = (struct clientArgs *)args;
  OrionHEN_log("[Daemon IPC] Thread created for Socket %i", client->socket);

  uint32_t readSize = 0;
  IPCMessage ipcMessage; // Create an IPCMessage struct to store received data

  while ((readSize = networkReceiveData(client->socket,
                                        reinterpret_cast<void *>(&ipcMessage),
                                        sizeof(ipcMessage))) > 0) {
    if (ipcMessage.magic == 0xDEADBABE) {
      // Handle IPCMessage
      std::string message = ipcMessage.msg; // Retrieve the std::string message
      handleIPC(client, message, ipcMessage.cmd);
    } else {
      OrionHEN_log("[Daemon IPC][client %i] Invalid magic number",
                 client->cl_nmb);
      ipcMessage.error = -1;
      networkSendData(client->socket, reinterpret_cast<void *>(&ipcMessage),
                      sizeof(ipcMessage));
    }
  }

  OrionHEN_log(
      "[Daemon IPC][client %i] IPC Connection disconnected, Shutting down ...",
      client->cl_nmb);

  networkCloseConnection(client->socket);
  delete client;
  pthread_exit(NULL);

  return NULL;
}

void *IPC_loop(void *args) {
  // Listen on port
  int serverSocket = networkListen(UTIL_IPC_SOC);
  if (serverSocket < 0) {
    OrionHEN_log("[Daemon IPC] networkListen error %s", strerror(errno));
    return nullptr;
  }

  // Keep accepting client connections
  int cli_new = 0;
  while (true) {
    // Accept a client connection
    int clientSocket = networkAccept(serverSocket);
    if (clientSocket < 0) {
      OrionHEN_log("[Daemon IPC] networkAccept error %s", strerror(errno));
      break; // Breaking out of the loop on error to cleanup
    }

    OrionHEN_log("[Daemon IPC] Connection Accepted");
    OrionHEN_log("[Daemon IPC] cl_nmb %i", cli_new);

    // Build data to send to thread
    auto clientParams = new clientArgs();
    clientParams->ip = "localhost";
    clientParams->socket = clientSocket;
    clientParams->cl_nmb = cli_new;

    OrionHEN_log("[Daemon IPC] clientParams->cl_nmb %i", clientParams->cl_nmb);
    pthread_t ipc_thread;
    pthread_create(&ipc_thread, NULL, ipc_client, clientParams);
    pthread_detach(ipc_thread); // Detach the thread to allow it to run independently
    cli_new++;
  }

  // Cleanup
  networkCloseConnection(serverSocket);
  return nullptr;
}
