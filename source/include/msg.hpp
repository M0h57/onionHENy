#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string>
#include <sys/param.h>

#define INVAIL -1
#define CRIT_IPC_SOC "/system_tmp/OrionHEN_crit_service"
#define UTIL_IPC_SOC "/system_tmp/OrionHEN_util_service"
#define DAEMON_BUFF_MAX 0x1000


enum class IPC_Ret : int {
  INVALID = -1,
  NO_ERROR = 0,
  OPERATION_FAILED = -1,
  DEFAULT_RET = -1337
};

enum DaemonCommands : int {
  BREW_TEST_CONNECTION = 0x9000000,
  BREW_RETURN_VALUE = 0x9000002,
  BREW_REMOUNT_FOLDER,
  BREW_STAT_CMD,
  BREW_CALC_DIR_SIZE,
  BREW_COPY_FILE,
  BREW_COPY_DIR,
  BREW_DELETE_DIR,
  BREW_UNUSED_1,// not used anymore but kept fopr backwards compatibility for now
  BREW_TEST_SB_FILE,
  BREW_DAEMON_PID,
  BREW_UNUSED_STORE_INSTALLER,
  BREW_DECRYPT_DIR,
  BREW_LAST_RET,
  BREW_TESTKIT_CHECK,
  BREW_ENABLE_TOOLBOX,
  BREW_CHMOD_DIR,
  // BREW_LAUNCH_DUMPER removed (no embedded ps5-app-dumper)
  BREW_ADJUST_FAN_SPEED,

  BREW_UTIL_TEST_CONNECTION = 0x8000000,
  BREW_UTIL_RETURN_VALUE = 0x8000002,
  BREW_UTIL_DAEMON_PID,
  BREW_UTIL_TOGGLE_FTP,
  BREW_UTIL_TOGGLE_KLOG,
  BREW_UTIL_TOGGLE_DPI,
  BREW_UTIL_LAUNCH_PLUGIN,
  BREW_UTIL_SHELLUI_ON_STANDBY,
  BREW_UTIL_GET_GAME_VER,
  BREW_UTIL_GET_GAME_CHEAT,
  BREW_UTIL_TOGGLE_CHEAT,
  BREW_UTIL_DOWNLOAD_CHEATS,
  BREW_UTIL_RELOAD_CHEATS,
  BREW_UTIL_DOWNLOAD_KSTUFF,
  BREW_UTIL_TOGGLE_LEGACY_CMD_SERVER,
  BREW_UTIL_UNUSED_2,
  BREW_UTIL_UNUSED_3,

  // Legacy: 9021 elfldr auto-launch removed (command kept for IPC compatibility)
  BREW_UTIL_LAUNCH_ELFLDR = 0xE1F1D8,
  BREW_RELOAD_SETTINGS = 0xC0FFEE,

  // BREW_TOGGLE_PS5DEBUG removed (ps5debug no longer embedded)

  //KILL MAIN DAEMOM
  BREW_KILL_DAEMON = 0xDEAD0001,
  BREW_FORCE_KILL_PID = 0xDEADCAFE,
};

struct IPCMessage {
  int magic = 0xDEADBABE;
  enum DaemonCommands cmd;
  int error = 0;
  char msg[DAEMON_BUFF_MAX];
};
