/* Copyright (C) 2025 OrionHEN / LightningMods
 * Crit daemon IPC command dispatch.
 * Transport (listen/accept/thread) stays in msg.cpp.
 */
#include "ipc.hpp"
#include <orion/platform.h>
#include "../../extern/cJSON/orion_cjson.hpp"
#include "globalconf.hpp"
#include <msg.hpp>
#include <atomic>
#include <string>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <strings.h>

extern orion::Settings g_settings;
extern bool is_handler_enabled;

void reply(int sender_socket, bool error, std::string out_var = "Nothing");
void LoadSettings();
bool copyRecursive(const char *source, const char *destination);
bool copyFile(const char *source, const char *destination, bool for_dumper);
void calculateSize(uint64_t size, char *result);
uint64_t calculateTotalSize(const char *path);
int change_permissions_recursive(const char *path);
bool test_sb_file(const char *filename);
bool cmd_enable_toolbox();
void ForceKillProc(int pid);
bool remount(const char *dev, const char *path, int mnt_flag);
bool set_fan_threshold(int temp);
extern "C" int unmount(const char *path, int flags);

void handleIPC(clientArgs *client, std::string &inputStr,
               DaemonCommands command) {

  int sender_app = client->socket;

  struct stat buffer;
  std::string path_buf, path_buf2, json_path;
  const char *path = nullptr, *dest = nullptr;
  char size_buf[0x255];
  bool last_ipc_error = false;

  std::string out_var = "Nothing"; // default send var

  OrionHEN_log("Received IPC command 0x%X", command);

  orion_cjson::Root my_json(inputStr);
  if (!my_json) {
    OrionHEN_log("Error parsing JSON");
    orion_notify(true, "Error parsing JSON");
    reply(sender_app, true);
    return;
  }

  switch (command) {
  case BREW_TEST_CONNECTION: {
    reply(sender_app, false, out_var);
    break;
  }
  case BREW_ENABLE_TOOLBOX: {
    if(cmd_enable_toolbox()){
        reply(sender_app, false);
    } else {
        reply(sender_app, true);
    }
    break;
  }
  case BREW_LAST_RET: {
    reply(sender_app, last_ipc_error, last_ipc_error ? "1" : "0");
    break;
  }
  case BREW_UNUSED_DECRYPT_DIR:
    /* SELF directory decrypt removed from OrionHEN. */
    OrionHEN_log("BREW_DECRYPT_DIR: unsupported (removed)");
    reply(sender_app, true);
    break;
  case BREW_UNUSED_TESTKIT_CHECK:
    /* Prefer local probe in clients; keep ordinal for IPC compatibility. */
    OrionHEN_log("BREW_TESTKIT_CHECK: unsupported (removed)");
    reply(sender_app, true);
    break;
  case BREW_REMOUNT_FOLDER:
    path_buf = std::string(orion_cjson::string_item(my_json.get(), "mount_dest", ""));
    path_buf2 = std::string(orion_cjson::string_item(my_json.get(), "mount_src", ""));
    json_path = path_buf + "/sce_sys/param.json";
    OrionHEN_log("change dir selected, %s", path_buf2.c_str());

    if(path_buf.rfind("/user") == std::string::npos && path_buf.length() <= strlen("/system_ex/app/")) {
      orion_notify(true, "Invalid path of size %d", path_buf.length());
      reply(sender_app, true);
      break;
    }

    mkdir(path_buf.c_str(), 0777);

    if (if_exists(json_path.c_str())) {
      OrionHEN_log("param.json exists, trying to unmount");
      int retries = 0;
      do {
        if (retries == 0)
          OrionHEN_log("unmounting .....");
        else
          OrionHEN_log("retrying attempt unmounting %d | prev. error %s", retries, strerror(errno));

        if (retries >= 20) {
          orion_notify(true, "Failed to unmount | error %s",
                 strerror(errno));
          reply(sender_app, true);
          break;
        }
        retries++;

      } while (unmount(path_buf.c_str(), MNT_FORCE) < 0);
    }

    if (!remount(path_buf2.c_str(), path_buf.c_str(), MNT_FORCE)) {
      if (errno == EBADF || errno == EPERM ||
          errno == EIO) { // if anyone repots a game not mounting til the 2nd
                          // time look at this
        OrionHEN_log("trying to unmount");
        unmount(path_buf.c_str(), MNT_FORCE);
      }
      if (!remount(path_buf2.c_str(), path_buf.c_str(), MNT_UPDATE)) {
        orion_notify(true, "remount error: %s\nPath: %s", strerror(errno),
               path_buf2.c_str());
        OrionHEN_log("remount error: %s Path: %s", strerror(errno),
                   path_buf2.c_str());
        reply(sender_app, true);
        break;
      } 
    }

    reply(sender_app, false);
    break;
  case BREW_STAT_CMD: {
    path = orion_cjson::string_item(my_json.get(), "path");
    if (stat(path, &buffer) == 0) {
      snprintf(size_buf, sizeof(size_buf), "%ld", buffer.st_size);
      OrionHEN_log("%s exists | size %s", path, size_buf);
      reply(sender_app, false, size_buf);
    } else {
      OrionHEN_log("error for %s | %s", path, strerror(errno));
      reply(sender_app, true);
    }
    break;
  }
  case BREW_CALC_DIR_SIZE: {
    uint64_t size = calculateTotalSize(orion_cjson::string_item(my_json.get(), "path"));
    snprintf(size_buf, sizeof(size_buf), "%lu", size);
    OrionHEN_log("size %lu", size_buf);
    reply(sender_app, false, size_buf);
    break;
  }
  case BREW_COPY_FILE: {
    path = orion_cjson::string_item(my_json.get(), "path");
    dest = orion_cjson::string_item(my_json.get(), "dest");
    if (copyFile(path, dest, false)) {
      reply(sender_app, false);
    } else {
      OrionHEN_log("error for %s | %s", path, strerror(errno));
      reply(sender_app, true);
    }
    break;
  }
  case BREW_COPY_DIR: {
    path = orion_cjson::string_item(my_json.get(), "path");
    dest = orion_cjson::string_item(my_json.get(), "dest");
    snprintf(size_buf, sizeof(size_buf), "%lu", calculateTotalSize(path));
    if (copyRecursive(path, dest)) {
      reply(sender_app, false, size_buf);
    } else {
      OrionHEN_log("error for %s | %s", path, strerror(errno));
      reply(sender_app, true);
    }
    break;
  }
  case BREW_DELETE_DIR: {
    path = orion_cjson::string_item(my_json.get(), "path");
    if (rmtree(path)) {
      reply(sender_app, false);
    } else {
      reply(sender_app, true);
    }
    break;
  }
  case BREW_TEST_SB_FILE: {
    reply(sender_app, !test_sb_file(orion_cjson::string_item(my_json.get(), "path")));
    break;
  }
  case BREW_DAEMON_PID: {
    snprintf(size_buf, sizeof(size_buf), "%d", getpid());
    reply(sender_app, false, size_buf);
    break;
  }
  case BREW_UNUSED_1: {
    // This command is not used anymore but kept for backwards compatibility
    orion_notify(true, "This command is not used anymore");
    reply(sender_app, true);
    break;
  }
  case BREW_ADJUST_FAN_SPEED: {
    int speed = orion_cjson::int_item(my_json.get(), "speed");
    int enabled = orion_cjson::int_item(my_json.get(), "enabled");
    OrionHEN_log("Adjusting Fan Speed to: %d", speed);
    if (speed < 0 || speed > 100) {
      orion_notify(true, "Invalid fan speed: %d. Must be between 0 and 100.", speed);
      reply(sender_app, true);
      break;
    }

    g_settings.enable_fan_speed = enabled;

    if (!enabled) {
      orion_notify(true, "Fan speed adjustment is disabled.");
      set_fan_threshold(77);
      reply(sender_app, false);
      break;
    }

    if (set_fan_threshold(speed)) {
      orion_notify(true, "Fan threshold adjusted to %i°C.", speed);
      g_settings.fan_threshold = speed;
      reply(sender_app, false);
    } else {
      orion_notify(true, "Failed to adjust fan speed.");
      reply(sender_app, true);
    }
    break;
  }
  case BREW_KILL_DAEMON:{
    is_handler_enabled = false;
    exit(1337);
    kill(getpid(), SIGKILL);
    reply(sender_app, false);
    break;
  }
  case BREW_FORCE_KILL_PID: {
    int pid = orion_cjson::int_item(my_json.get(), "pid");
    if (pid < 0) {
      OrionHEN_log("Invalid PID: %d", pid);
      reply(sender_app, true);
      break;
    }

    ForceKillProc(pid);
    reply(sender_app, false);
    break;
  }
  case BREW_RELOAD_SETTINGS: {
    LoadSettings();
    orion_notify(true, "Reloaded Settings");
    reply(sender_app, false);
    break;
  }
  case BREW_CHMOD_DIR: {
	OrionHEN_log("BREW_CHMOD_DIR called");
    path = orion_cjson::string_item(my_json.get(), "path");
    if(!path) {
      OrionHEN_log("Invalid path for chmod");
      reply(sender_app, true);
      break;
	}
   // kernel_set_ucred_authid(getpid(), 0x4801000000000013L);
	  change_permissions_recursive(path);
	  reply(sender_app, false);
    break;
  }
  default:
    orion_notify(true, "Unknown command 0x%X", command);
    reply(sender_app, true);
    break;
  }
}

