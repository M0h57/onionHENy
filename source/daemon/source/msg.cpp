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
#include <orion/platform.h>
#include <orion/proc_query.h>
// orion/ucred.h (and orion/proc.h) pull freestanding kernel proc types that
// clash with SDK <sys/proc.h> — only include where set_proc_authid is used.
#include "../../extern/cJSON/orion_cjson.hpp"
#include "globalconf.hpp"
#include <orion/settings.hpp>
#include <orion/ready.h>
#include <orion/ipc_server.hpp>
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
int _sceApplicationGetAppId(int pid, int *appid);
int nmount(struct iovec *iov, unsigned int niov, int flags);
}

bool is_handler_enabled = true;
using namespace std;
extern pthread_t cheat_thr;
extern orion::Settings g_settings;
extern atomic_bool shortcut_activated;

int launchApp(const char *titleId);
int ItemzLaunchByUri(const char *uri);

extern "C" int unmount(const char *path, int flags);
bool copyRecursive(const char *source, const char *destination);
void calculateSize(uint64_t size, char *result);

extern "C" void sceLncUtilGetAppTitleId(uint32_t appId, char *titleId);
bool GetFileContents(const char *path, char **buffer);
uint64_t calculateTotalSize(const char *path);
bool copyFile(const char *source, const char *destination, bool for_dumper);


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
bool remount(const char *dev, const char *path, int mnt_flag) {
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
  struct stat file_stat {};
  const char *paths[] = {orion::kConfigPathPrimary, orion::kConfigPathShellui};

  // Prefer primary; fall back to shellui path for mtime / create.
  const char *config_path = nullptr;
  for (const char *p : paths) {
    if (stat(p, &file_stat) == 0) {
      config_path = p;
      break;
    }
  }

  if (!config_path) {
    OrionHEN_log("[Daemon] Config file not found. Creating default schema...");
    if (orion::settings_ensure_default()) {
      orion_notify(true, "OrionHEN config created! @ /data/OrionHEN/config.ini");
      config_state.last_modified = 0;
    }
    // Apply defaults even if create failed.
    orion::Settings s{};
    orion::settings_load(&s);
    g_settings = s;
    return;
  }

  // Only reload if file has been modified since last load
  if (file_stat.st_mtime <= config_state.last_modified) {
    return;
  }

  OrionHEN_log("[Daemon] Loading Settings from shared schema...");
  orion::Settings s{};
  if (!orion::settings_load(&s)) {
    orion_notify(true, "Failed to Read the Settings file");
    return;
  }

  OrionHEN_log("[Daemon] Reading Settings from %s",
               orion::settings_last_loaded_path());
  OrionHEN_log("fan_threshold: %d", s.fan_threshold);
  OrionHEN_log("enable_fan_speed: %d", s.enable_fan_speed ? 1 : 0);

  g_settings = s;

  config_state.last_modified = file_stat.st_mtime;
}



int networkListen(const char *soc_path) {
  return orion::ipc_network_listen(soc_path);
}

int networkAccept(int socket) {
  return orion::ipc_network_accept(socket);
}

int networkReceiveData(int socket, void *buffer, int32_t size) {
  return orion::ipc_network_recv(socket, buffer, size);
}

int networkSendData(int socket, void *buffer, int32_t size) {
  return orion::ipc_network_send(socket, buffer, size);
}

int networkSendDebugData(void *buffer, int32_t size) {
  return networkSendData(DaemonSocket, buffer, size);
}

int networkCloseConnection(int socket) {
  return orion::ipc_network_close(socket);
}

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




void reply(int sender_socket, bool error, std::string out_var = "Nothing") {
  orion::ipc_reply(sender_socket, BREW_RETURN_VALUE, error, out_var);
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
  int sceKernelTerminateProcess(int pid, int *ret);
  uintptr_t set_proc_authid(pid_t pid, uintptr_t new_authid);
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
        orion_notify(true, "Failed to get game pid");
        return false;
    }

    if (!Inject_Toolbox(pid, fps_elf_start)) {
        ForceKillProc(pid);
        orion_notify(true, "Failed to inject fps");
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
      OrionHEN_log("sleeping for %llu", g_settings.rest_mode_delay_seconds);
      sleep(g_settings.rest_mode_delay_seconds);
    }

    /* Prefer kstuff ready marker when present; fall back to short settle. */
    if (!orion_ready_wait(ORION_READY_KSTUFF, /*timeout_ms=*/5000, /*poll_ms=*/200)) {
      sleep(1);
    }

    int pid = get_shellui_pid();
    if (pid < 0) {
      orion_notify(true, "Failed to get shellui pid");
      return false;
    }
    OrionHEN_log("Injecting toolbox into SceShellUI pid=%d", pid);

    if (!Inject_Toolbox(pid, shellui_elf_start)) {
      /* Do NOT ForceKill ShellUI — that loops home menu / coredumps */
      orion_notify(true, "Failed to inject toolbox");
      return false;
    }

    /* Ready protocol: shellui signals ORION_READY_TOOLBOX after inject hooks run */
    if (!orion_ready_wait(ORION_READY_TOOLBOX, /*timeout_ms=*/45 * 1000,
                          /*poll_ms=*/250)) {
      orion_notify(true, "Failed to load the OrionHEN toolbox (timeout, ShellUI left running)");
      return false;
    }
    orion_ready_clear(ORION_READY_TOOLBOX);
    OrionHEN_log("Toolbox online (ready protocol)");

    return true;
}
/* handleIPC → ipc_handle.cpp */
void handleIPC(clientArgs *client, std::string &inputStr,
               DaemonCommands command);

static void handleIPC_adapt(orion::IpcClientArgs *client, std::string &msg,
                            DaemonCommands cmd) {
  handleIPC(client, msg, cmd);
}

static void ipc_server_log_line(const char *line) {
  OrionHEN_log("%s", line);
}

static orion::IpcServerOptions g_crit_ipc_opts = {
    CRIT_IPC_SOC,
    handleIPC_adapt,
    BREW_RETURN_VALUE,
    true,
    "crit",
};

void *IPC_loop(void *args) {
  (void)args;
  orion::ipc_server_set_log(ipc_server_log_line);
  return orion::ipc_server_loop(&g_crit_ipc_opts);
}
