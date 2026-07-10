/* Copyright (C) 2025 OrionHEN / LightningMods */

#include "daemon_ops.hpp"
#include <orion/platform.h>
#include <orion/proc_query.h>
#include <orion/ipc_server.hpp>
#include <msg.hpp>
#include <atomic>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/ioctl.h>

extern "C" {
int nmount(struct iovec *iov, unsigned int niov, int flags);
int sceKernelGetProcessName(int pid, char *name);
int sceSystemServiceGetAppIdOfRunningBigApp();
int _sceApplicationGetAppId(int pid, int *appid);
int sceKernelTerminateProcess(int pid, int *ret);
}

// set_proc_authid from liborion_proc — freestanding kernel types, include carefully
extern "C" uintptr_t set_proc_authid(pid_t pid, uintptr_t new_authid);

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
bool remount(const char *dev, const char *path, int mnt_flag) {
  NonStupidIovec iov[]{
      "fstype"_iov, "nullfs"_iov, "fspath"_iov, {path},
      "target"_iov, {dev},        "rw"_iov,     {nullptr, 0},
  };
  constexpr size_t iovlen = sizeof(iov) / sizeof(iov[0]);
  return nmount(reinterpret_cast<struct iovec *>(iov), iovlen, mnt_flag) == 0;
}

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


namespace {
std::atomic<int> g_last_ipc_error{0};
} // namespace

int daemon_last_ipc_error() {
  return g_last_ipc_error.load(std::memory_order_relaxed);
}

void reply(int sender_socket, bool error, std::string out_var) {
  g_last_ipc_error.store(error ? -1 : 0, std::memory_order_relaxed);
  orion::ipc_reply(sender_socket, BREW_RETURN_VALUE, error, out_var);
}

int get_shellui_pid() {
  /* sysctl allproc via liborion_proc — no 0..9999 PID scan. */
  return static_cast<int>(orion_find_pid("SceShellUI"));
}


int get_game_pid() {
  /*
   * Running BigApp process: orion_find_pid_ex with for_bigapp requires SCE
   * hooks registered in daemon main (process name + app info + bigapp id).
   * Returns process pid (not appid).
   */
  pid_t pid = orion_find_pid_ex(/*name=*/"", /*needle=*/false,
                                /*for_bigapp=*/true, /*need_eboot=*/false);
  if (pid > 0) {
    return static_cast<int>(pid);
  }

  /* Fallback: scan via application API when hooks unavailable. */
  char proc_name[255] = {0};
  int app_pid = -1;
  int appid = sceSystemServiceGetAppIdOfRunningBigApp();
  if (appid < 0) {
    return -1;
  }

  for (int j = 1; j <= 9999; j++) {
    int bappid = 0;
    if (_sceApplicationGetAppId(j, &bappid) < 0)
      continue;
    if (appid != bappid)
      continue;
    app_pid = j;
    if (sceKernelGetProcessName(app_pid, proc_name) < 0) {
      OrionHEN_log("sceKernelGetProcessName failed for (%d)", app_pid);
    }
    break;
  }
  return app_pid;
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

[[noreturn]] void cmd_shutdown_orion_stack(void) {
  OrionHEN_log("cmd_shutdown_orion_stack: stopping util → ShellUI → self");

  /* util.elf (ki_comm) — also try common display names. */
  int util_pid = static_cast<int>(orion_find_pid("util.elf"));
  if (util_pid < 0)
    util_pid = static_cast<int>(orion_find_pid_substr("util.elf"));
  if (util_pid > 0 && util_pid != getpid()) {
    OrionHEN_log("shutdown: ForceKill util pid=%d", util_pid);
    ForceKillProc(util_pid);
  } else {
    OrionHEN_log("shutdown: util.elf not found");
  }

  int shellui_pid = get_shellui_pid();
  if (shellui_pid > 0 && shellui_pid != getpid()) {
    OrionHEN_log("shutdown: ForceKill SceShellUI pid=%d", shellui_pid);
    ForceKillProc(shellui_pid);
  } else {
    OrionHEN_log("shutdown: SceShellUI not found");
  }

  is_handler_enabled = false;
  orion_notify(true, "OrionHEN stack shutdown (util + ShellUI + daemon)");
  usleep(200 * 1000);
  exit(0);
}

bool set_fan_threshold(int THRESHOLDTEMP) {

   if(THRESHOLDTEMP > 100){
     THRESHOLDTEMP = 100;
   }

   int fd = open("/dev/icc_fan", O_RDONLY, 0);
   if (fd <= 0) {
     orion_notify(true, "Unable to Open Fan Settings!");
     return false;
   }

    char data[10] = {0x00, 0x00, 0x00, 0x00, 0x00, static_cast<char>(THRESHOLDTEMP), 0x00, 0x00, 0x00, 0x00};
    if(ioctl(fd, 0xC01C8F07, data) < 0) {
        orion_notify(true, "Unable to Set Fan Speed!");
        close(fd);
        return false;
    }
    close(fd);
    //OrionHEN_log("Fan speed set to %d%% THRESHOLDTEMP", THRESHOLDTEMP);
    return true;
}

