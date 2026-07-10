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
#include "../../extern/cJSON/orion_cjson.hpp"
#include "globalconf.hpp"
#include <atomic>
#include <msg.hpp>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <strings.h>
#include <sys/_pthreadtypes.h>
#include <sys/_stdint.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <ps5/kernel.h>
#include <sys/user.h>
#include <vector>
#include "../../include/ini.h"

#include "dbg/dbg.hpp"
#include "elf/elf.hpp"
#include "hijacker/hijacker.hpp"


typedef struct app_info {
  uint32_t app_id;
  uint64_t unknown1;
  uint32_t app_type;
  char     title_id[10];
  char     unknown2[0x3c];
} app_info_t;


bool if_exists(const char *path);

extern "C" {
#include <sys/mount.h>

int32_t sceKernelPrepareToSuspendProcess(pid_t pid);
int32_t sceKernelSuspendProcess(pid_t pid);
int32_t sceKernelPrepareToResumeProcess(pid_t pid);
int32_t sceKernelResumeProcess(pid_t pid);
int32_t sceUserServiceInitialize(int32_t *priority);
int32_t sceUserServiceGetForegroundUser(int32_t *new_id);
int32_t scePadSetProcessPrivilege(int32_t num);
int sceKernelMprotect(void *addr, size_t len, int prot);
int sceSystemServiceLoadExec(const char *path, const char *argv[]);
int sceSystemServiceGetAppIdOfRunningBigApp();

extern uint8_t shellui_elf_start[];
extern const unsigned int shellui_elf_size;

extern uint8_t fps_elf_start[];
extern const unsigned int fps_elf_size;

bool Inject_Toolbox(int pid, uint8_t *elf);
int sceKernelGetAppInfo(int pid, app_info_t *title);
int sceKernelGetProcessName(int pid, char *name);
int _sceApplicationGetAppId(int pid, int* appid);

}


bool is_handler_enabled = true;
using namespace std;
extern pthread_t cheat_thr;
extern struct daemon_settings global_conf;
extern atomic_bool shortcut_activated;

int launchApp(const char *titleId);
int ItemzLaunchByUri(const char *uri);

void OrionHEN_log(const char *fmt, ...);

extern "C" int unmount(const char *path, int flags);
bool copyRecursive(const char *source, const char *destination);
bool rmtree(const char *path);
void calculateSize(uint64_t size, char *result);

extern "C" void sceLncUtilGetAppTitleId(uint32_t appId, char *titleId);
bool GetFileContents(const char *path, char **buffer);
uint64_t calculateTotalSize(const char *path);
bool copyFile(const char *source, const char *destination, bool for_dumper);

void notify(bool show_watermark, const char *text, ...);
bool isProcessAlive(int pid) noexcept;

int DaemonSocket = 0;


struct NonStupidIovec {
  const void *iov_base;
  size_t iov_length;

  constexpr NonStupidIovec(const char *str)
      : iov_base(str), iov_length(__builtin_strlen(str) + 1) {}
  constexpr NonStupidIovec(const char *str, size_t length)
      : iov_base(str), iov_length(length) {}
};

constexpr NonStupidIovec operator""_iov(const char *str, unsigned long len) {
  return {str, len + 1};
}
static bool remount(const char *dev, const char *path, int mnt_flag) {
  NonStupidIovec iov[]{
      "fstype"_iov, "nullfs"_iov, "fspath"_iov, {path},
      "target"_iov, {dev},        "rw"_iov,     {nullptr, 0},
  };
  constexpr size_t iovlen = sizeof(iov) / sizeof(iov[0]);
  return nmount(reinterpret_cast<struct iovec *>(iov), iovlen, mnt_flag) == 0;
}


#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

int change_permissions_recursive(const char* path) {
    struct stat statbuf;
    struct dirent* entry;
    DIR* dir;
    int result = 0;

    if (!path || strlen(path) == 0) {
        OrionHEN_log( "Invalid path provided");
        return -1;
    }

    if (lstat(path, &statbuf) != 0) {
        OrionHEN_log( "Failed to stat '%s': %s", path, strerror(errno));
        return -1;
    }

    if (S_ISLNK(statbuf.st_mode)) {
        OrionHEN_log("Skipping symbolic link: %s", path);
        return 0;
    }

    // Skip special files (devices, pipes, sockets, etc.)
    if (!S_ISREG(statbuf.st_mode) && !S_ISDIR(statbuf.st_mode)) {
        OrionHEN_log("Skipping special file: %s", path);
        return 0;
    }

    if (!S_ISDIR(statbuf.st_mode)) {
        if (chmod(path, 0777) != 0) {
            OrionHEN_log( "Failed to chmod '%s': %s", path, strerror(errno));
            return -1;
        }
        return 0;
    }

    dir = opendir(path);
    if (!dir) {
        OrionHEN_log( "Failed to open directory '%s': %s", path, strerror(errno));
        return -1;
    }

    errno = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        size_t path_len = strlen(path);
        size_t name_len = strlen(entry->d_name);

        if (path_len + name_len + 2 > PATH_MAX) {
            OrionHEN_log( "Path too long: %s/%s", path, entry->d_name);
            result = -1;
            continue;
        }

        char newpath[PATH_MAX];
        int ret = snprintf(newpath, sizeof(newpath), "%s/%s", path, entry->d_name);
        if (ret >= sizeof(newpath)) {
            OrionHEN_log( "Path truncated: %s/%s", path, entry->d_name);
            result = -1;
            continue;
        }

        if (change_permissions_recursive(newpath) != 0) {
            result = -1;
        }
    }

    if (errno != 0) {
        OrionHEN_log( "Error reading directory '%s': %s", path, strerror(errno));
        result = -1;
    }

    closedir(dir);

    if (chmod(path, 0777) != 0) {
        OrionHEN_log( "Failed to chmod directory '%s': %s", path, strerror(errno));
        return -1;
    }

    return result;
}


#include <sys/stat.h>

struct ConfigState {
  time_t last_modified = 0;
};

ConfigState config_state;

void LoadSettings() {
  struct stat file_stat;
  const char* config_path = "/data/OrionHEN/config.ini";
  
  // Check if file exists and get its modification time
  if (stat(config_path, &file_stat) != 0) {
    // File doesn't exist, create default config
    OrionHEN_log("[Daemon] Config file not found. Creating default...");
    std::string ini_file(
      "[Settings]\nFTP=1\nAllow_data_in_sandbox=0\nDPI=0\ntoolbox_auto_start=1\nDPI_v2=0\nKlog=0\nAPP_JB_Debug_Msg=0\nauto_eject_disc=0\n");
    int fd = open(config_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd >= 0) {
      write(fd, ini_file.c_str(), ini_file.length());
      close(fd);
      notify(true, "OrionHEN config created! @ /data/OrionHEN/config.ini");
      config_state.last_modified = 0;
    }
    return;
  }
  
  // Only reload if file has been modified since last load
  if (file_stat.st_mtime <= config_state.last_modified) {
    return; // File hasn't changed, skip reload
  }
  
  // File has changed, proceed with loading
  OrionHEN_log("[Daemon] Loading Settings...");
  
  IniParser parser;
  if (ini_parser_load(&parser, config_path)) {
    OrionHEN_log("[Daemon] Reading Settings...");
    
    const char * libhijacker_cheats_str =
      ini_parser_get(&parser, "Settings.libhijacker_cheats", "0");
    const char * start_option =
      ini_parser_get(&parser, "Settings.StartOption", "0");
    const char * DPI_v2 = ini_parser_get(&parser, "Settings.DPI_v2", "0");
    const char * auto_eject_disc =
      ini_parser_get(&parser, "Settings.auto_eject_disc", "0");
    const char* fan_threshold =
      ini_parser_get(&parser, "Settings.fan_threshold", "77");
    const char* enable_fan_speed =
      ini_parser_get(&parser, "Settings.enable_fan_speed", "0");
    const char* overlay_fps =
      ini_parser_get(&parser, "Settings.overlay_fps", "0");

    OrionHEN_log("fan_threshold: %s", fan_threshold);
    OrionHEN_log("enable_fan_speed: %s", enable_fan_speed);
    
    global_conf.fan_threshold = fan_threshold ? atoi(fan_threshold) : 77;
    global_conf.enable_fan_speed = enable_fan_speed ? atoi(enable_fan_speed) : 0;
    global_conf.overlay_fps = overlay_fps ? atoi(overlay_fps) : 0;
    global_conf.libhijacker_cheats =
      libhijacker_cheats_str ? atoi(libhijacker_cheats_str) : 0;
    global_conf.start_opt =
      start_option ? (StartOpts) atoi(start_option) : NONE;
    global_conf.DPIv2 = DPI_v2 ? atoi(DPI_v2) : 0;
    global_conf.toolbox_auto_start =
      atoi(ini_parser_get(&parser, "Settings.toolbox_auto_start", "1"));

    global_conf.seconds =
      atol(ini_parser_get(&parser, "Settings.Rest_Mode_Delay_Seconds", "0"));
    global_conf.debug_app_jb_msg =
      atoi(ini_parser_get(&parser, "Settings.APP_JB_Debug_Msg", "0"));
    global_conf.auto_eject_disc = auto_eject_disc ? atoi(auto_eject_disc) : 0;

    if (if_exists("/mnt/usb0/toolbox_auto_start"))
      global_conf.toolbox_auto_start = false;
    
    // Update last modified time after successful load
    config_state.last_modified = file_stat.st_mtime;
  } else {
    notify(true, "Failed to Read the Settings file");
  }
}

static pid_t find_pid(const char *name) {
  int mib[4] = {1, 14, 8, 0};
  pid_t pid = -1;
  size_t buf_size;
  uint8_t *buf;

  if (sysctl(mib, 4, 0, &buf_size, 0, 0)) {
      perror("sysctl");
      return -1;
  }

  if (!(buf = (uint8_t *)malloc(buf_size))) {
      perror("malloc");
      return -1;
  }

  if (sysctl(mib, 4, buf, &buf_size, 0, 0)) {
      perror("sysctl");
      free(buf);
      return -1;
  }

  for (uint8_t *ptr = buf; ptr < (buf + buf_size);) {
      int ki_structsize = *(int *)ptr;
      pid_t ki_pid = *(pid_t *)&ptr[72];
      char *ki_tdname = (char *)&ptr[447];

      ptr += ki_structsize;
      if (strcmp(ki_tdname, name) == 0) {
          printf("[MATCH] ki_pid: %d, ki_tdname: %s\n", ki_pid, ki_tdname);
          pid = ki_pid;
          break;
      }
  }

  free(buf);
  return pid;
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

  //OrionHEN_log("Socket has name %s", server.sun_path);

  r = listen(s, 100);
  if (r < 0) {
    OrionHEN_log("[Daemon] listen failed! %s", strerror(errno));
    return INVAIL;
  }

  return s;
}

int networkAccept(int socket) {
  //touch_file("/system_tmp/IPC_init");
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

#include <fcntl.h>
// pop -Winfinite-recursion error for this func for clang
#define MB(x) ((size_t)(x) << 20)

#define READ_SIZE 0x1024

bool test_sb_file(const char *filename) {
  if (!filename) {
    OrionHEN_log("test_sb_file: filename is null");
    return false;
  }

  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    OrionHEN_log("test_sb_file: Failed to open %s", filename);
    return false;
  }

  // Determine the size of the file
  struct stat fileInfo;
  if (fstat(fd, &fileInfo) < 0) {
    OrionHEN_log("test_sb_file: Failed to get file size for %s", filename);
    close(fd);
    return false;
  }

  off_t fileSize = fileInfo.st_size;
  char buffer[READ_SIZE];

  // Read start
  if (read(fd, buffer, READ_SIZE) < 0) {
    OrionHEN_log("test_sb_file: Failed to read start of %s", filename);
    close(fd);
    return false;
  }

  // Calculate middle, ensuring we don't try to seek beyond the file size
  off_t middlePosition =
      fileSize / 2 > READ_SIZE ? fileSize / 2 - READ_SIZE / 2 : 0;
  if (lseek(fd, middlePosition, SEEK_SET) < 0 ||
      read(fd, buffer, READ_SIZE) < 0) {
    OrionHEN_log("test_sb_file: Failed to read middle of %s", filename);
    close(fd);
    return false;
  }

  // Read end
  off_t endPosition = fileSize > READ_SIZE ? fileSize - READ_SIZE : 0;
  if (lseek(fd, endPosition, SEEK_SET) < 0 || read(fd, buffer, READ_SIZE) < 0) {
    OrionHEN_log("test_sb_file: Failed to read end of %s", filename);
    close(fd);
    return false;
  }

  close(fd);
  OrionHEN_log("test_sb_file: Successfully sampled %s", filename);
  return true;
}
extern "C" int sceSystemServiceKillApp(uint32_t appid, int opt, int method,
                                       int reason);
extern "C" int sceSystemServiceGetAppId(const char *tid);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winfinite-recursion"
bool if_exists(const char *path);




void reply(int sender_socket, bool error, std::string out_var = "Nothing") {

  std::string inputStr = "{\"res\":" + std::to_string(error ? -1 : 0) +
                         ", \"var\":\"" + out_var + "\"}";

  IPCMessage outputMessage;
  outputMessage.cmd = BREW_RETURN_VALUE;
  outputMessage.error = error ? -1 : 0;
  OrionHEN_log("error: %d", outputMessage.error);
  bzero(outputMessage.msg, sizeof(outputMessage.msg));
  if (!inputStr.empty()) {
    strncpy(outputMessage.msg, inputStr.c_str(), sizeof(outputMessage.msg) - 1);
    // Null-terminate the destination array
    outputMessage.msg[sizeof(outputMessage.msg) - 1] = '\0';
  }

  networkSendData(sender_socket, reinterpret_cast<void *>(&outputMessage),
                  sizeof(outputMessage));
}

int get_shellui_pid() {
  int pid = -1;
  size_t NumbOfProcs = 9999;

  for (int j = 0; j <= NumbOfProcs; j++) {
      char tmp_buf[500];
      memset(tmp_buf, 0, sizeof(tmp_buf));
      sceKernelGetProcessName(j, tmp_buf);
      if (strcmp("SceShellUI", tmp_buf) == 0) {
          pid = j;
          break;
      }
  }

  return pid == -1 ? find_pid( "SceShellUI") : pid;
}


int get_game_pid() {
   
    char proc_name[255] = { 0 };
	int app_pid = -1;
    int appid = sceSystemServiceGetAppIdOfRunningBigApp();

    for (size_t j = 0; j <= 9999; j++) {
        int bappid = 0;
        if (_sceApplicationGetAppId(j, &bappid) < 0)
            continue;

        if (appid == bappid) {
            app_pid = j; // APP PID NOT TO BE CONFUSED WITH APPID
            if (sceKernelGetProcessName(app_pid, &proc_name[0]) < 0) {
                OrionHEN_log("sceKernelGetProcessName failed for (%d)", app_pid);
                continue;
            }
            // cheat_log("Found %s (%d)", proc_name, app_pid);

            break;
        }
    }
    return app_pid;
}
extern "C" {
  struct proc* get_proc_by_pid(pid_t pid);
  uintptr_t set_proc_authid(pid_t pid, uintptr_t new_authid)
{
    struct proc* proc = get_proc_by_pid(getpid());

    if (proc)
    {
        //
        // Read from kernel
        //
        uintptr_t authid = 0;
        kernel_copyout((uintptr_t) proc->p_ucred + 0x58, &authid, sizeof(uintptr_t));
        kernel_copyin(&new_authid, (uintptr_t) proc->p_ucred + 0x58, sizeof(uintptr_t));

        free(proc);

        return authid;
    }

    return 0;
}
  int sceKernelTerminateProcess(int pid, int *ret);
}

void ForceKillProc(int pid) {
  if (pid < 0) {
    OrionHEN_log("Invalid PID: %d", pid);
    return;
  }
  
  #define DECID_AUTH_ID 0x4800000000000022 // required for killing with sceKernelTerminateProcess / sys_proc_term  syscall
  uintptr_t authid = set_proc_authid(getpid(), DECID_AUTH_ID );

  int ret = 0;
  if (sceKernelTerminateProcess(pid, &ret) != 0) {
    OrionHEN_log("Failed to terminate process with PID: %d, error: %d", pid, ret);
  } else {
    OrionHEN_log("Successfully terminated process with PID: %d", pid);
  }

  set_proc_authid(getpid(), authid); // Restore original authid
}

extern "C" {
    int32_t sceKernelPrepareToSuspendProcess(pid_t pid);
    int32_t sceKernelSuspendProcess(pid_t pid);
    int32_t sceKernelPrepareToResumeProcess(pid_t pid);
    int32_t sceKernelResumeProcess(pid_t pid);
    int32_t sceUserServiceInitialize(int32_t* priority);
    int32_t sceUserServiceGetForegroundUser(int32_t* new_id);
    int32_t sceSysmoduleLoadModuleInternal(uint32_t moduleId);
    int32_t sceSysmoduleUnloadModuleInternal(uint32_t moduleId);
    int32_t sceVideoOutOpen();
    int32_t sceVideoOutConfigureOutput();
    int32_t sceVideoOutIsOutputSupported();

    int sceKernelLoadStartModule(const char* name, size_t argc, const void* argv, uint32_t flags, void* option, int* res);
}
static void SuspendApp(pid_t pid)
{
    sceKernelPrepareToSuspendProcess(pid);
    sceKernelSuspendProcess(pid);
}

static void ResumeApp(pid_t pid)
{
    // we need to sleep the thread after suspension
    // because this will cause a kernel panic when user quits the process after sometime
    // the kernel will not be very happy with us.
    usleep(1000);
    sceKernelPrepareToResumeProcess(pid);
    sceKernelResumeProcess(pid);
}
struct GameStuff {
    uintptr_t scePadReadState;
    uintptr_t debugout;
    uintptr_t sceKernelLoadStartModule;
    uintptr_t sceKernelDlsym;
    uintptr_t sceKernelSendNotificationRequest;
    uintptr_t anything;
    uint64_t ASLR_Base = 0;
    char prx_path[256];
    int loaded = 0;

    GameStuff(Hijacker& hijacker) noexcept
        : debugout(hijacker.getLibKernelAddress(nid::sceKernelDebugOutText)),
        sceKernelLoadStartModule(hijacker.getLibKernelAddress(nid::sceKernelLoadStartModule)),
        sceKernelDlsym(hijacker.getLibKernelAddress(nid::sceKernelDlsym)),
        sceKernelSendNotificationRequest(hijacker.getLibKernelAddress(nid::sceKernelSendNotificationRequest)) {
    }
};

struct GameBuilder {

    static constexpr size_t SHELLCODE_SIZE = 218;
    static constexpr size_t EXTRA_STUFF_ADDR_OFFSET = 2;

    uint8_t shellcode[SHELLCODE_SIZE];

    void setExtraStuffAddr(uintptr_t addr) noexcept {
        *reinterpret_cast<uintptr_t*>(shellcode + EXTRA_STUFF_ADDR_OFFSET) = addr;
    }
};

static constexpr GameBuilder BUILDER_TEMPLATE{
    0x48, 0xba, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // MOV scePadReadState, RDX // 10


    // Additional shellcode0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x54, 0x53, 0x48, 0x83, 0xec, 0x60, 0x4c, 0x8b, 0x62, 0x20,0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x54, 0x53, 0x48, 0x83, 0xec, 0x30, 0x4c, 0x8b, 0x62, 0x20,
    0x55, 0x41, 0x57, 0x41, 0x56, 0x53, 0x48, 0x83, 0xec, 0x48, 0x48, 0xb8, 0x73, 0x68, 0x65, 0x6c,
    0x6c, 0x6d, 0x61, 0x69, 0x48, 0xb9, 0x6e, 0x20, 0x69, 0x73, 0x20, 0x6e, 0x75, 0x6c, 0x48, 0xc7,
    0x44, 0x24, 0x08, 0x00, 0x00, 0x00, 0x00, 0x49, 0x89, 0xd6, 0x48, 0x89, 0xf3, 0x89, 0xfd, 0x48,
    0x89, 0x44, 0x24, 0x30, 0x48, 0x89, 0x4c, 0x24, 0x38, 0x48, 0xc7, 0x44, 0x24, 0x40, 0x6c, 0x00,
    0x00, 0x00, 0x48, 0x89, 0x44, 0x24, 0x10, 0x48, 0xc7, 0x44, 0x24, 0x18, 0x6e, 0x00, 0x00, 0x00,
    0x48, 0xc7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0xff, 0x12, 0x41, 0x89, 0xc7, 0x85, 0xed,
    0x7e, 0x60, 0x45, 0x85, 0xff, 0x75, 0x5b, 0x80, 0x7b, 0x4c, 0x00, 0x74, 0x55, 0x41, 0x83, 0xbe,
    0x38, 0x01, 0x00, 0x00, 0x00, 0x75, 0x4b, 0x49, 0x8d, 0x7e, 0x38, 0x31, 0xf6, 0x31, 0xd2, 0x31,
    0xc9, 0x45, 0x31, 0xc0, 0x45, 0x31, 0xc9, 0x41, 0xff, 0x56, 0x10, 0x48, 0x8d, 0x74, 0x24, 0x10,
    0x48, 0x8d, 0x54, 0x24, 0x08, 0x89, 0xc7, 0x41, 0xff, 0x56, 0x18, 0x48, 0x8b, 0x44, 0x24, 0x08,
    0x48, 0x85, 0xc0, 0x74, 0x07, 0x4c, 0x89, 0xf7, 0xff, 0xd0, 0xeb, 0x0b, 0x48, 0x8d, 0x74, 0x24,
    0x30, 0x31, 0xff, 0x41, 0xff, 0x56, 0x08, 0x41, 0xc7, 0x86, 0x38, 0x01, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x44, 0x89, 0xf8, 0x48, 0x83, 0xc4, 0x48, 0x5b, 0x41, 0x5e, 0x41, 0x5f, 0x5d, 0xc3,
};

bool HookGame(UniquePtr<Hijacker>& hijacker, uint64_t alsr_b) {
    OrionHEN_log("Patching Game Now");

    GameBuilder builder = BUILDER_TEMPLATE;
    GameStuff stuff{ *hijacker };

    UniquePtr<SharedLib> lib = hijacker->getLib("libScePad.sprx");
    if (lib.get() == nullptr) {
        OrionHEN_log("libScePad.sprx not found!");
        return false;
    }
    OrionHEN_log("libScePad.sprx addr: 0x%llx", lib->imagebase());
    stuff.scePadReadState = hijacker->getFunctionAddress(lib.get(), nid::scePadReadState);

    //libSceGnmDriver
    UniquePtr<SharedLib> gnmlib = hijacker->getLib("libSceGnmDriverForNeoMode.sprx");
    if (gnmlib.get() == nullptr) {
        OrionHEN_log("libSceGnmDriverForNeoMode.sprx not found!");
        gnmlib = hijacker->getLib("libSceGnmDriver.sprx");
        if (gnmlib.get() == nullptr) {
            OrionHEN_log("libSceGnmDriver.sprx not found!");
			return false;
		}   
    }
    OrionHEN_log("libSceGnmDriver.sprx addr: 0x%llx", gnmlib->imagebase());
    stuff.anything = hijacker->getFunctionAddress(gnmlib.get(), nid::sceGnmSubmitAndFlipCommandBuffersForWorkload);

    OrionHEN_log("scePadReadState addr: 0x%llx", stuff.scePadReadState);
    if (stuff.scePadReadState == 0) {
        OrionHEN_log("failed to locate scePadReadState");
        return false;
    }

    stuff.ASLR_Base = alsr_b;
    strcpy(stuff.prx_path, "/data/OrionHEN/fps.prx");

    auto code = hijacker->getTextAllocator().allocate(GameBuilder::SHELLCODE_SIZE);
    OrionHEN_log("shellcode addr: 0x%llx", code);
    auto stuffAddr = hijacker->getDataAllocator().allocate(sizeof(GameStuff));
    // static constexpr Nid printfNid{"hcuQgD53UxM"};
    // static constexpr Nid amd64_set_fsbaseNid{"3SVaehJvYFk"};
    auto meta = hijacker->getEboot()->getMetaData();
    const auto& plttab = meta->getPltTable();
    auto index = meta->getSymbolTable().getSymbolIndex(nid::scePadReadState);
    for (const auto& plt : plttab) {
        if (ELF64_R_SYM(plt.r_info) == index) {
            builder.setExtraStuffAddr(stuffAddr);
            hijacker->write(code, builder.shellcode);
            hijacker->write(stuffAddr, stuff);

            uintptr_t hook_adr = hijacker->getEboot()->imagebase() + plt.r_offset;

            // write the hook
            hijacker->write<uintptr_t>(hook_adr, code);
            OrionHEN_log("hook addr: 0x%llx", hook_adr);
            hijacker.release();

            return true;
        }
    }
    return false;
}

int done_appid;
extern "C" int sceKernelGetCurrentFanDuty(int *unk, int *duty);
bool set_fan_threshold(int THRESHOLDTEMP) {

   if(THRESHOLDTEMP > 100){
     THRESHOLDTEMP = 100;
   }

   int fd = open("/dev/icc_fan", O_RDONLY, 0);
   if (fd <= 0) {
     notify(true, "Unable to Open Fan Settings!");
     return false;
   }

    char data[10] = {0x00, 0x00, 0x00, 0x00, 0x00, static_cast<char>(THRESHOLDTEMP), 0x00, 0x00, 0x00, 0x00};
    if(ioctl(fd, 0xC01C8F07, data) < 0) {
        notify(true, "Unable to Set Fan Speed!");
        close(fd);
        return false;
    }
    close(fd);
    //OrionHEN_log("Fan speed set to %d%% THRESHOLDTEMP", THRESHOLDTEMP);
    return true;
}



bool cmd_enable_fps_new(int appid) {
 
    if(done_appid == appid){
       // OrionHEN_log("FPS already enabled for %x", appid);
        return true;
  	}
    
    OrionHEN_log("Enabling fps for appid %d", appid);

    sleep(5);

    SuspendApp(appid);

    int pid = get_game_pid();
    if (pid < 0) {
        notify(true, "Failed to get game pid");
        return false;
    }

    if (!Inject_Toolbox(pid, fps_elf_start)) {
        ForceKillProc(pid);
        notify(true, "Failed to inject fps");
        return false;
    }

    sleep(1);
    ResumeApp(pid);

    done_appid = appid;

    return true;
}


bool cmd_enable_fps(int appid) {
   
    if(done_appid == appid){
       // OrionHEN_log("FPS already enabled for %x", appid);
        return true;
	   }

    SuspendApp(appid);

    int bappid = 0, pid = 0;
    for (size_t j = 0; j <= 9999; j++) {
        if (_sceApplicationGetAppId(j, &bappid) < 0)
            continue;

        if (appid == bappid) {
            pid = j;
            OrionHEN_log("Game is running, appid 0x%X, pid %i", appid, pid);
            //printf_notification("Game is running, appid 0x%X, pid %i", appid, pid);
            break;
        }
    }

    UniquePtr<Hijacker> executable = Hijacker::getHijacker(pid);
    uintptr_t text_base = 0;
    uint64_t text_size = 0;
    if (executable)
    {
        text_base = executable->getEboot()->getTextSection()->start();
        text_size = executable->getEboot()->getTextSection()->sectionLength();
    }
    else
    {
        OrionHEN_log("Failed to get hijacker for (%d)", pid);
       // printf_notification("Failed to get hijacker for (%d), try re-running the plugin", pid);
        return false;
    }
    if (text_base == 0 || text_size == 0)
    {
        OrionHEN_log("text_base == 0 || text_size == 0");
        //printf_notification("text_base == 0 || text_size == 0 (%d), try re-running the plugin", pid);
        return false;
    }

    while (!HookGame(executable, text_base)) {
        //OrionHEN_log("Failed to patch the game");
        sleep(1);
    }

    sleep(1);
    ResumeApp(pid);

    done_appid = appid;
    return true;
}

bool cmd_enable_toolbox(){
    int wait = 0;
    char buz[100] = {0};

    /*
     * If kstuff is present, wait until mprotect works (patches applied) before
     * we ptrace ShellUI. Injecting while kstuff is still patching ShellUI
     * causes "waiting for toolbox" forever / ShellUI crash.
     * (Race seen when daemon+kstuff launched close together via 9021.)
     * Note: we do NOT pause/resume kstuff — plugins may own that; only wait for
     * mprotect readiness.
     */
    if (find_pid("kstuff.elf") > 0 || find_pid("kstuff") > 0) {
      OrionHEN_log("kstuff present — waiting for mprotect before toolbox inject");
      for (int i = 0; i < 20; i++) {
        if (sceKernelMprotect(&buz[0], 100, 0x7) == 0)
          break;
        sleep(1);
      }
      sleep(2);
    }

    OrionHEN_log("Activating toolbox...");
    if (if_exists("/system_tmp/util_first_boot")) {
      LoadSettings();
      OrionHEN_log("sleeping for %llu", global_conf.seconds);
      sleep(global_conf.seconds);
    }

    /* ShellUI needs a moment after kstuff trophy patches */
    sleep(2);

    int pid = get_shellui_pid();
    if (pid < 0) {
      notify(true, "Failed to get shellui pid");
      return false;
    }
    OrionHEN_log("Injecting toolbox into SceShellUI pid=%d", pid);

    if (!Inject_Toolbox(pid, shellui_elf_start)) {
      /* Do NOT ForceKill ShellUI — that loops home menu / coredumps */
      notify(true, "Failed to inject toolbox");
      return false;
    }

    while (!if_exists("/system_tmp/toolbox_online")) {
      OrionHEN_log("waiting for toolbox to start");
      sleep(1);
      if (++wait >= 45) {
        /* Keep ShellUI alive; user can retry from Debug Settings */
        notify(true, "Failed to load the OrionHEN toolbox (timeout, ShellUI left running)");
        return false;
      }
    }
    unlink("/system_tmp/toolbox_online");
    OrionHEN_log("Toolbox online");

    return true;
}
void handleIPC(struct clientArgs *client, std::string &inputStr,
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
    notify(true, "Error parsing JSON");
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
  case BREW_DECRYPT_DIR: {
    // Generic decrypt IPC (no Itemzflow DUMP00000 hand-off)
    reply(sender_app, false);
    std::string dest_path =
        std::string(orion_cjson::string_item(my_json.get(), "dest_path", ""));
    std::string sandbox_dir =
        std::string(orion_cjson::string_item(my_json.get(), "src_path", ""));
    OrionHEN_log("Decrypt to %s", dest_path.c_str());
    mkdir(dest_path.c_str(), 0777);
    notify(false, "Attempting to decrypt %s -> %s", sandbox_dir.c_str(),
           dest_path.c_str());
    last_ipc_error = !decrypt_dir(sandbox_dir, dest_path);
    break;
  }
  case BREW_TESTKIT_CHECK: {
    reply(sender_app, !if_exists("/system/priv/lib/libSceDeci5Ttyp.sprx"));
    break;
  }
  case BREW_REMOUNT_FOLDER:
    path_buf = std::string(orion_cjson::string_item(my_json.get(), "mount_dest", ""));
    path_buf2 = std::string(orion_cjson::string_item(my_json.get(), "mount_src", ""));
    json_path = path_buf + "/sce_sys/param.json";
    OrionHEN_log("change dir selected, %s", path_buf2.c_str());

    if(path_buf.rfind("/user") == std::string::npos && path_buf.length() <= strlen("/system_ex/app/")) {
      notify(true, "Invalid path of size %d", path_buf.length());
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
          notify(true, "Failed to unmount | error %s",
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
        notify(true, "remount error: %s\nPath: %s", strerror(errno),
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
    notify(true, "This command is not used anymore");
    reply(sender_app, true);
    break;
  }
  case BREW_ADJUST_FAN_SPEED: {
    int speed = orion_cjson::int_item(my_json.get(), "speed");
    int enabled = orion_cjson::int_item(my_json.get(), "enabled");
    OrionHEN_log("Adjusting Fan Speed to: %d", speed);
    if (speed < 0 || speed > 100) {
      notify(true, "Invalid fan speed: %d. Must be between 0 and 100.", speed);
      reply(sender_app, true);
      break;
    }

    global_conf.enable_fan_speed = enabled;

    if (!enabled) {
      notify(true, "Fan speed adjustment is disabled.");
      set_fan_threshold(77);
      reply(sender_app, false);
      break;
    }

    if (set_fan_threshold(speed)) {
      notify(true, "Fan threshold adjusted to %i°C.", speed);
      global_conf.fan_threshold = speed;
      reply(sender_app, false);
    } else {
      notify(true, "Failed to adjust fan speed.");
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
    notify(true, "Reloaded Settings");
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
      std::string message = ipcMessage.msg; // Retrieve the string message
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
  int serverSocket = networkListen(CRIT_IPC_SOC);
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
    cli_new++;
  }

  // Cleanup
  networkCloseConnection(serverSocket);
  return nullptr;
}
