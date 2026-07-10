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

#include <orion/notify.h>
#include <string>
#include <pthread.h>
#include "error_translator.hpp"
#include "../../extern/cJSON/orion_cjson.hpp"
extern "C" {
#include "common_utils.h"
#include <dirent.h>
#include <signal.h>
}

pthread_t pkg_installer_thread;
enum AppInstErrorCodes {
  SCE_APP_INSTALLER_ERROR_UNKNOWN = -2136801279,
  SCE_APP_INSTALLER_ERROR_NOSPACE,
  SCE_APP_INSTALLER_ERROR_PARAM,
  SCE_APP_INSTALLER_ERROR_APP_NOT_FOUND,
  SCE_APP_INSTALLER_ERROR_DISC_NOT_INSERTED,
  SCE_APP_INSTALLER_ERROR_PKG_INVALID_DRM_TYPE,
  SCE_APP_INSTALLER_ERROR_OUT_OF_MEMORY,
  SCE_APP_INSTALLER_ERROR_APP_BROKEN,
  SCE_APP_INSTALLER_ERROR_PKG_INVALID_CONTENT_TYPE,
  SCE_APP_INSTALLER_ERROR_USED_APP_NOT_FOUND,
  SCE_APP_INSTALLER_ERROR_ADDCONT_BROKEN,
  SCE_APP_INSTALLER_ERROR_APP_IS_RUNNING,
  SCE_APP_INSTALLER_ERROR_SYSTEM_VERSION,
  SCE_APP_INSTALLER_ERROR_NOT_INSTALL,
  SCE_APP_INSTALLER_ERROR_CONTENT_ID_DISAGREE,
  SCE_APP_INSTALLER_ERROR_NOSPACE_KERNEL,
  SCE_APP_INSTALLER_ERROR_APP_VER,
  SCE_APP_INSTALLER_ERROR_DB_DISABLE,
  SCE_APP_INSTALLER_ERROR_CANCELED,
  SCE_APP_INSTALLER_ERROR_ENTRYDIGEST,
  SCE_APP_INSTALLER_ERROR_BUSY,
  SCE_APP_INSTALLER_ERROR_DLAPP_ALREADY_INSTALLED,
  SCE_APP_INSTALLER_ERROR_NEED_ADDCONT_INSTALL,
  SCE_APP_INSTALLER_ERROR_APP_MOUNTED_BY_HOST_TOOL,
  SCE_APP_INSTALLER_ERROR_INVALID_PATCH_PKG,
  SCE_APP_INSTALLER_ERROR_NEED_ADDCONT_INSTALL_NO_CHANGE_TYPE = -2136801248,
  SCE_APP_INSTALLER_ERROR_ADDCONT_IS_INSTALLING,
  SCE_APP_INSTALLER_ERROR_ADDCONT_ALREADY_INSTALLED,
  SCE_APP_INSTALLER_ERROR_CANNOT_READ_DISC,
  SCE_APP_INSTALLER_ERROR_DATA_DISC_NOT_INSTALLED,
  SCE_APP_INSTALLER_ERROR_NOT_TRANSFER_DISC_VERSION,
  SCE_APP_INSTALLER_ERROR_NO_SLOT_SPACE,
  SCE_APP_INSTALLER_ERROR_NO_SLOT_INFORMATION,
  SCE_APP_INSTALLER_ERROR_INSTALL_MAIN_MISSING,
  SCE_APP_INSTALLER_ERROR_INSTALL_TIME_VALID_IN_FUTURE,
  SCE_APP_INSTALLER_ERROR_SYSTEM_FILE_DISAGREE,
  SCE_APP_INSTALLER_ERROR_INSTALL_BLANK_SLOT,
  SCE_APP_INSTALLER_ERROR_INSTALL_LINK_SLOT,
  SCE_APP_INSTALLER_ERROR_INSTALL_PKG_NOT_COMPLETED,
  SCE_APP_INSTALLER_ERROR_NOSPACE_IN_EXTERNAL_HDD,
  SCE_APP_INSTALLER_ERROR_NOSPACE_KERNEL_IN_EXTERNAL_HDD,
  SCE_APP_INSTALLER_ERROR_COMPILATION_DISC_INSERTED,
  SCE_APP_INSTALLER_ERROR_COMPILATION_DISC_INSERTED_NOT_VISIBLE_DISC_ICON,
  SCE_APP_INSTALLER_ERROR_ACCESS_FAILED_IN_EXTERNAL_HDD,
  SCE_APP_INSTALLER_ERROR_MOVE_FAILED_SOME_APPLICATIONS,
  SCE_APP_INSTALLER_ERROR_DUPLICATION,
  SCE_APP_INSTALLER_ERROR_INVALID_STATE,
  SCE_APP_INSTALLER_ERROR_NOSPACE_DISC,
  SCE_APP_INSTALLER_ERROR_NOSPACE_DISC_IN_EXTERNAL_HDD,
  SCE_APP_INST_UTIL_ERROR_NOT_INITIALIZED = -2136797184,
  SCE_APP_INST_UTIL_ERROR_OUT_OF_MEMORY
};

typedef struct {
  int32_t error_code;
  int32_t version;
  char description[512];
  char type[9];
} SceAppInstallErrorInfo;

typedef struct {
  char status[16];
  char src_type[8];
  uint32_t remain_time;
  uint64_t downloaded_size;
  uint64_t initial_chunk_size;
  uint64_t total_size;
  uint32_t promote_progress;
  SceAppInstallErrorInfo error_info;
  int32_t local_copy_percent;
  bool is_copy_only;
} SceAppInstallStatusInstalled;

void OrionHEN_log(const char *fmt, ...);
extern "C" {
int sceAppInstUtilInstallByPackage(MetaInfo *arg1,
                                   SceAppInstallPkgInfo *pkg_info,
                                   PlayGoInfo *arg2);
int sceAppInstUtilInitialize(void);
int sceAppInstUtilGetInstallStatus(const char *content_id,
                                   SceAppInstallStatusInstalled *status);
int sceAppInstUtilGetContentIdFromPkg(const char *pkg_path, char *content_id,
                                      bool *is_app);
}
#define UNUSED(x) (void)x
void call_func();
int server_fd, new_socket = -1;
struct sockaddr_in address;
int addrlen = 0;
atomic_bool is_running = false;
// make a new thread for installl pkgs
void *runDirectPKGInstaller(void *args) {
  UNUSED(args);
  const char *url = NULL;
  char json_str[0x255]; // Adjust the size based on your actual JSON content
  bool first_run = true;
  is_running = true;

  PlayGoInfo arg3;
  SceAppInstallPkgInfo pkg_info;
  (void)memset(&arg3, 0, sizeof(arg3));

  for (size_t i = 0; i < NUM_LANGUAGES; i++) {
    strncpy(arg3.languages[i], "", sizeof(arg3.languages[i]) - 1);
  }

  for (size_t i = 0; i < NUM_IDS; i++) {
    strncpy(arg3.playgo_scenario_ids[i], "", sizeof(playgo_scenario_id_t) - 1);
    strncpy(*arg3.content_ids, "", sizeof(content_id_t) - 1);
  }

  while (is_running) {
    // Endlessly wait for a URL
    if (!first_run)
      orion_notify(true, "DPI: Waiting for Requests...");

    first_run = false;

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                             (socklen_t *)&addrlen)) < 0) {
      if (errno == 0xA3) {
        break;
      }
      orion_notify(true, "DPI: Failed to accept socket address %s", strerror(errno));
      continue; // If accept fails, try again
    }
    char buffer[1024] = {0};
    long valread = read(new_socket, buffer, 1024);
    if (valread > 0) {
      orion_cjson::Root my_json(buffer);
      if (!my_json) {
        OrionHEN_log("Error parsing JSON");
        orion_notify(true, "Error parsing JSON");
        continue;
      }

      if ((url = orion_cjson::string_item(my_json.get(), "url")) == NULL) {
        orion_notify(true, "DPI: URL not found in JSON");
        continue;
      }

      OrionHEN_log("DPI: URL Received: %s", url);

      const char* content_name = orion_cjson::string_item(my_json.get(), "content_name");
      if (content_name) OrionHEN_log("DPI: content_name: %s", content_name);

      const char* content_id = orion_cjson::string_item(my_json.get(), "content_id");
      if (content_id) OrionHEN_log("DPI: content_id: %s", content_id);
      
      const char* playgo_scenario_id = orion_cjson::string_item(my_json.get(), "playgo_scenario_id");
      if (playgo_scenario_id) OrionHEN_log("DPI: playgo_scenario_id: %s", playgo_scenario_id);
      
      const char* ex_uri = orion_cjson::string_item(my_json.get(), "ex_uri");
      if (ex_uri) OrionHEN_log("DPI: ex_uri: %s", ex_uri);
      
      const char* icon_url = orion_cjson::string_item(my_json.get(), "icon_url");
      if (icon_url) OrionHEN_log("DPI: icon_url: %s", icon_url);

      MetaInfo arg1 = {.uri = url,
                       .ex_uri = ex_uri ? ex_uri : "",
                       .playgo_scenario_id = playgo_scenario_id ? playgo_scenario_id : "",
                       .content_id = content_id ? content_id : "",
                       .content_name = content_name ? content_name : "OrionHEN DPI",
                       .icon_url = icon_url ? icon_url : ""};

      int num = sceAppInstUtilInstallByPackage(&arg1, &pkg_info, &arg3);
      if (num == 0) {
        orion_notify(true, "DPI: Download and Install console Task initiated");
      } else {
        orion_notify(true, "DPI: Install failed with error code %d", num);
      }

      snprintf(json_str, sizeof(json_str), "{\"res\":\"%d\"}", num);
      OrionHEN_log("DPI: Sending response: %s", json_str);
      send(new_socket, json_str, strlen(json_str), MSG_NOSIGNAL);
      #if 0
      SceAppInstallStatusInstalled status;
      float prog = 0;
      while (strcmp(status.status, "playable") != 0) {
        sceAppInstUtilGetInstallStatus(pkg_info.content_id, &status);
        if (status.total_size != 0) {
          prog = ((float)status.downloaded_size / status.total_size) *
                 100.0f; // Cast to float and multiply by 100 for percentage
        }

        OrionHEN_log("DPI: content_id %s, Status: %s | error: %d | progress %.2f%% (%llu/%llu)",
                   pkg_info.content_id,status.status, status.error_info.error_code, prog,
                   status.downloaded_size, status.total_size);
      }
      #endif
    } else {
      orion_notify(true, "DPI: No data received, or connection closed by client.");
    }

    close(new_socket); // Close the connection and wait for the next one
  }

  close(server_fd);
  is_running = false;
  pthread_exit(NULL);

  return NULL;
}


void shutdownDirectPKGInstaller(void) {
  if (!is_running) {
    OrionHEN_log("DPI: DirectPKGInstaller is not running");
    return;
  }

  is_running = false;

  // Wake accept() so the installer thread can exit.
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock != -1) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9090);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    close(sock);
  }
  pthread_join(pkg_installer_thread, NULL);
}

bool startDirectPKGInstaller(void) {
  if (is_running) {
    OrionHEN_log("DPI: DirectPKGInstaller is already running");
    return true;
  }

  int rv = sceAppInstUtilInitialize();
  if (rv != 0) {
    orion_notify(true, "DPI: Failed to initialize libSceAppInstUtil.sprx");
    return false;
  }

  int opt = 1;
  addrlen = sizeof(address);
  const int PORT = 9090;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    orion_notify(true, "DPI: Failed to create socket file descriptor %s",
                 strerror(errno));
    return false;
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    orion_notify(true, "DPI: Failed to set socket options %s", strerror(errno));
    return false;
  }
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    orion_notify(true, "DPI: Failed to bind socket to port %s", strerror(errno));
    return false;
  }
  if (listen(server_fd, 3) < 0) {
    orion_notify(true, "DPI: Failed to listen on socket %s", strerror(errno));
    return false;
  }

  if (pthread_create(&pkg_installer_thread, NULL, runDirectPKGInstaller,
                     NULL) != 0) {
    orion_notify(true, "Failed to create runDirectPKGInstaller thread");
    return false;
  }

  return true;
}
