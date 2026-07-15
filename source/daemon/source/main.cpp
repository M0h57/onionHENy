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

// Include files
#include <cstdint>
#include <orion/ucred.h>
#include <orion/proc_query.h>
#include <orion/platform.h>
#include "daemon_ops.hpp"
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>

// System includes
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <sys/_pthreadtypes.h>
#include <sys/signal.h>
#include <netinet/in.h>
#include <ps5/klog.h>

// Project includes
#include "../../include/backtrace.hpp"
#include "globalconf.hpp"
#include "launcher.hpp"
#include "ipc.hpp"
#include <orion/ready.h>

#define MSG_NOSIGNAL 0x20000 /* do not generate SIGPIPE on EOF. */
pthread_t cheat_thr = nullptr;

#define PAD_BUTTON_OPTIONS	0x00000008

// Structure definitions
typedef struct {
    int32_t type;             // 0x00
    int32_t req_id;           // 0x04
    int32_t priority;         // 0x08
    int32_t msg_id;           // 0x0C
    int32_t target_id;        // 0x10
    int32_t user_id;          // 0x14
    int32_t unk1;             // 0x18
    int32_t unk2;             // 0x1C
    int32_t app_id;           // 0x20
    int32_t error_num;        // 0x24
    int32_t unk3;             // 0x28
    char use_icon_image_uri;  // 0x2C
    char message[1024];       // 0x2D
    char uri[1024];           // 0x42D
    char unkstr[1024];        // 0x82D
  } OrbisNotificationRequest; // Size = 0xC30

typedef struct app_info {
  uint32_t app_id;
  uint64_t unknown1;
  uint32_t app_type;
  char title_id[10];
  char unknown2[0x3c];
} app_info_t;

// External C declarations
extern "C" {
    int sceKernelSendNotificationRequest(int32_t device,
        OrbisNotificationRequest *req,
        size_t size, int32_t blocking);
    int sceSystemServiceNavigateToGoHome(void);
    int sceUserServiceGetUserName(const int userId, char *userName, const size_t size);
    uint64_t sceKernelGetProcessTime();
    int sceSystemServiceGetAppId(const char *title_id);
    int sceSystemServiceGetAppIdOfRunningBigApp(void);
    int scePadSetProcessPrivilege(int priv);

    int sceUserServiceGetForegroundUser(int *userId);
    int sceLncUtilLaunchApp(const char *tid, const char *argv[], LncAppParam *param);
    uint32_t _sceApplicationGetAppId(int pid, uint32_t *appId);
    uint32_t sceLncUtilKillApp(uint32_t appId);
    int sceSysmoduleLoadModuleInternal(int id);
    int sceNetCtlInit();
    int sceUserServiceInitialize(const int *);
    int sceKernelLoadStartModule(const char *name, size_t argc, const void *argv, 
                                uint32_t flags, void *unknown, int *result);
    int sceKernelDlsym(uint32_t lib, const char *name, void **fun);
    int scePadClose(int handle);
    int sceSystemStateMgrEnterStandby(void);
    int sceKernelMprotect(void *addr, size_t len, int prot);
    ssize_t _read(int, void *, size_t);
    int sceKernelGetProcessName(int pid, char *name);
    int sceKernelGetAppInfo(int pid, app_info_t *info);
    void free(void *);
    int sceShellCoreUtilRequestEjectDevice(const char *path);

    // PayloadAPI definitions
    #include <ps5/payload.h>
    
    int sceNotificationSend(int userId, bool isLogged, const char* payload);

}

// Global variables
uint64_t p_syscall = 0;
char _end[1] = {};
int fd = -1;
static constexpr auto DEFAULT_PRIORITY = 256;
uintptr_t kernel_base = 0;

// Function declarations
int launchApp(const char *titleId);
bool enable_toolbox();
void sig_handler(int signo);
int elfldr_raise_privileges(pid_t pid);
extern void makenewapp();
// Play_time_thread / fifo_and_dumper_thread / get_ip_address / IPC_loop: daemon_ops.hpp
extern bool is_handler_enabled;

namespace {

/** Welcome toast shown once at daemon startup. */
constexpr const char kWelcomeToastJson[] =
    "{\n"
    "  \"rawData\": {\n"
    "    \"viewTemplateType\": \"InteractiveToastTemplateB\",\n"
    "    \"channelType\": \"Downloads\",\n"
    "    \"useCaseId\": \"IDC\",\n"
    "    \"toastOverwriteType\": \"No\",\n"
    "    \"isImmediate\": true,\n"
    "    \"priority\": 100,\n"
    "    \"viewData\": {\n"
    "      \"icon\": {\n"
    "        \"type\": \"Url\",\n"
    "        \"parameters\": {\n"
    "          \"url\": \"/user/data/OrionHEN/orionhen.png\"\n"
    "        }\n"
    "      },\n"
    "      \"message\": {\n"
    "        \"body\": \"OrionHEN\"\n"
    "      },\n"
    "      \"subMessage\": {\n"
    "        \"body\": \"Welcome to OrionHEN\"\n"
    "      },\n"
    "      \"actions\": [\n"
    "        {\n"
    "          \"actionName\": \"Go to the OrionHEN Toolbox\",\n"
    "          \"actionType\": \"DeepLink\",\n"
    "          \"defaultFocus\": true,\n"
    "          \"parameters\": {\n"
    "            \"actionUrl\": \"pssettings:play?function=debug_settings_old\"\n"
    "          }\n"
    "        }\n"
    "      ]\n"
    "    },\n"
    "    \"platformViews\": {\n"
    "      \"previewDisabled\": {\n"
    "        \"viewData\": {\n"
    "          \"icon\": {\n"
    "            \"type\": \"Predefined\",\n"
    "            \"parameters\": {\n"
    "              \"icon\": \"download\"\n"
    "            }\n"
    "          },\n"
    "          \"message\": {\n"
    "            \"body\": \"OrionHEN Running\"\n"
    "          }\n"
    "        }\n"
    "      }\n"
    "    }\n"
    "  },\n"
    "  \"createdDateTime\": \"2025-12-14T03:14:51.473Z\",\n"
    "  \"localNotificationId\": \"588193127\"\n"
    "}";

void install_crash_handlers() {
  struct sigaction action {};
  action.sa_handler = sig_handler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  for (int i = 0; i < 12; i++)
    sigaction(i, &action, nullptr);
}

void start_worker_threads(pthread_t* fifo_thr, pthread_t* pt_thr, pthread_t* msg_thr) {
  pthread_create(fifo_thr, nullptr, fifo_and_dumper_thread, nullptr);
  pthread_create(pt_thr, nullptr, Play_time_thread, nullptr);
  pthread_create(msg_thr, nullptr, IPC_loop, nullptr);
  pthread_t ctrl_thr = nullptr;
  pthread_create(&ctrl_thr, nullptr, control_tcp_loop, nullptr);
  pthread_detach(ctrl_thr);
}

/** Keep IPC_loop alive: rejoin + restart on exit. */
[[noreturn]] void ipc_supervisor_loop(pthread_t* msg_thr) {
  while (true) {
    pthread_join(*msg_thr, nullptr);
    pthread_create(msg_thr, nullptr, IPC_loop, nullptr);
    sleep(1);
  }
}

} // namespace

int launchApp(const char *titleId) {
    int id = 0;

    uint32_t res = sceUserServiceGetForegroundUser(&id);
    if (res != 0) {
        printf("sceUserServiceGetForegroundUser failed: 0x%x", res);
        return res;
    }
    OrionHEN_log("[LA] user id %u", id);

    // the thread will clean this up
    Flag flag = Flag_None;
    LncAppParam param{sizeof(LncAppParam), id, 0, 0, flag};

    puts("calling sceLncUtilLaunchApp");
    int err = sceLncUtilLaunchApp(titleId, nullptr, &param);
    OrionHEN_log("sceLncUtilLaunchApp returned 0x%x", (uint32_t)err);
    if (err >= 0) {
        return err;
    }
    
    switch ((uint32_t)err) {
    case SCE_LNC_UTIL_ERROR_ALREADY_RUNNING:
        OrionHEN_log("app %s is already running", titleId);
        break;
    case SCE_LNC_ERROR_APP_NOT_FOUND:
        OrionHEN_log("app %s not found", titleId);
        orion_notify(true, "app %s not found", titleId);
        break;
    default:
        OrionHEN_log("[LA] unknown error 0x%x", (uint32_t)err);
        // orion_notify(true, "unknown error 0x%llx", (uint32_t)err);
        break;
    }
    return err;
}

void sig_handler(int signo) {
    if(!is_handler_enabled){
        OrionHEN_log("Signal handler is disabled, ignoring signal %d", signo);
        return;
    }
    orion_notify(true,
          "OrionHEN has crashed ...\n\nPlease send /data/OrionHEN/OrionHEN_crash.log "
          "to the PKG-Zone discord: https://discord.gg/BduZHudWGj");
    OrionHEN_log("main OrionHEN has crashed ...");
    //printBacktraceForCrash();
    exit(1);
}

bool is_800 = false;

int main() {
  /* Raw 9021 uploads default to "payload.elf"; publish our stable name. */
  (void)syscall(SYS_thr_set_name, -1, "daemon.elf");

  orion_log_configure("OrionHEN", "/data/OrionHEN/OrionHEN.log");
  /* Real linked kernel export (not a dlsym function-pointer variable). */
  orion_notify_set_send(reinterpret_cast<orion_notify_send_fn>(
      sceKernelSendNotificationRequest));

  char buz[255];
  pthread_t fifo_thr = nullptr;
  pthread_t pt_thr = nullptr;
  pthread_t msg_thr = nullptr;

  sceNetCtlInit();
  sceUserServiceInitialize(&DEFAULT_PRIORITY);
  puts("daemon entered");

  OrbisKernelSwVersion sys_ver;
  sceKernelGetProsperoSystemSwVersion(&sys_ver);
  const int fw_ver = (sys_ver.version >> 16);

  install_crash_handlers();

  unlink("/data/OrionHEN/OrionHEN.log");
  unlink("/data/OrionHEN/OrionHEN_crash.log");

  payload_args_t* args = payload_get_args();
  kernel_base = args->kdata_base_addr;

  OrionHEN_log("=========== starting OrionHEN (0x%X) ... ===========", fw_ver);
  (void)sceKernelMprotect(&buz[0], 100, 0x7); // probe mprotect / kstuff state
  const bool toolbox_only = (fw_ver >= 0x10000);
  is_800 = (fw_ver >= 0x800);

  LoadSettings();

  /* liborion_proc big-app / name lookups used by get_game_pid / inject paths. */
  orion_proc_set_sce_hooks(
      [](int pid, char *name) -> int {
        return sceKernelGetProcessName(pid, name);
      },
      [](pid_t pid, void *info) -> int {
        return sceKernelGetAppInfo(pid, static_cast<app_info_t *>(info));
      },
      []() -> int { return sceSystemServiceGetAppIdOfRunningBigApp(); });

  get_ip_address(&buz[0]);
  start_worker_threads(&fifo_thr, &pt_thr, &msg_thr);
  orion_ready_signal(ORION_READY_DAEMON);

  OrionHEN_log("is toolbox only: %s | ver: %x", toolbox_only ? "Yes" : "No",
               sys_ver.version);

  /* Always inject toolbox into ShellUI; do not auto-open any settings page. */
  cmd_enable_toolbox();

  sceNotificationSend(0xFE, true, kWelcomeToastJson);
  OrionHEN_log("StartUp thread created!! - welcome to OrionHEN");

  const orion::Settings boot_cfg = g_settings.snapshot();
  if (boot_cfg.auto_eject_disc)
    sceShellCoreUtilRequestEjectDevice("/dev/cd0");

  ipc_supervisor_loop(&msg_thr);
  // unreachable
  return 0;
}
