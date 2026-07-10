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
  unsigned int size;
  uint32_t userId;
} SceShellUIUtilLaunchByUriParam;

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

// External C declarations
extern "C" {
    int sceKernelSendNotificationRequest(int32_t device,
        OrbisNotificationRequest *req,
        size_t size, int32_t blocking);
    int sceSystemServiceNavigateToGoHome(void);
    int sceUserServiceGetUserName(const int userId, char *userName, const size_t size);
    uint64_t sceKernelGetProcessTime();
    int sceSystemServiceGetAppId(const char *title_id);
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
    //int sceShellUIUtilInitialize(void);
    int scePadClose(int handle);
    //int sceShellUIUtilLaunchByUri(const char *uri, SceShellUIUtilLaunchByUriParam *Param);
    int sceSystemStateMgrEnterStandby(void);
    int sceKernelMprotect(void *addr, size_t len, int prot);
    ssize_t _read(int, void *, size_t);
    int sceKernelGetProcessName(int pid, char *name);
    void free(void *);
    int sceShellCoreUtilRequestEjectDevice(const char *path);

    // PayloadAPI definitions
    #include <ps5/payload.h>
    
    int sceNotificationSend(int userId, bool isLogged, const char* payload);

}

// Global variables
uint64_t p_syscall = 0;
char _end[1] = {};
orion::Settings g_settings;
int fd = -1;
static constexpr auto DEFAULT_PRIORITY = 256;
uintptr_t kernel_base = 0;

// Function declarations
int launchApp(const char *titleId);
int get_ip_address(char *ip_address);
int ItemzLaunchByUri(const char *uri);
bool enable_toolbox();
void sig_handler(int signo);
void *fifo_and_dumper_thread(void *args);
void *Play_time_thread(void *args) noexcept;
void patch_checker();
int elfldr_raise_privileges(pid_t pid);
extern void makenewapp();
extern void *IPC_loop(void *);
extern bool is_handler_enabled;

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

int (*sceShellUIUtilInitialize)(void) = nullptr;
int (*sceShellUIUtilLaunchByUri)(const char* uri, SceShellUIUtilLaunchByUriParam* Param) = nullptr;
#define KERNEL_DLSYM(handle, sym) \
    (*(void**)&sym=(void*)kernel_dynlib_dlsym(-1, handle, #sym))
int ItemzLaunchByUri(const char* uri) {
    int libcmi = -1;

    if (!uri)
        return -1;

    if ((libcmi = sceKernelLoadStartModule("/system_ex/common_ex/lib/libSceShellUIUtil.sprx", 0, 0, 0, 0, 0)) < 0 || libcmi < 0)
        return -1;

    KERNEL_DLSYM(libcmi, sceShellUIUtilInitialize);
    KERNEL_DLSYM(libcmi, sceShellUIUtilLaunchByUri);
    if (!sceShellUIUtilInitialize || !sceShellUIUtilLaunchByUri) {
        OrionHEN_log("failed to load libSceShellUIUtil.sprx");
        return -1;
    }
    //
    SceShellUIUtilLaunchByUriParam Param;
    Param.size = sizeof(SceShellUIUtilLaunchByUriParam);
    sceShellUIUtilInitialize();
    sceUserServiceGetForegroundUser((int*)&Param.userId); // DONT CARE

    return sceShellUIUtilLaunchByUri(uri, &Param);
}

bool is_800 = false;
int main() {
  orion_log_configure("OrionHEN", "/data/OrionHEN/OrionHEN.log");
    char buz[255];
    pthread_t fifo_thr = nullptr;
    pthread_t pt_thr = nullptr;
    pthread_t msg_thr = nullptr;
    
    sceNetCtlInit();
    sceUserServiceInitialize(&DEFAULT_PRIORITY);
    puts("daemon entered");
    
    OrbisKernelSwVersion sys_ver;
    sceKernelGetProsperoSystemSwVersion(&sys_ver);
    int fw_ver = (sys_ver.version >> 16);

    // Set up signal handlers
    struct sigaction new_SIG_action;
    new_SIG_action.sa_handler = sig_handler;
    sigemptyset(&new_SIG_action.sa_mask);
    new_SIG_action.sa_flags = 0;

    for (int i = 0; i < 12; i++)
        sigaction(i, &new_SIG_action, NULL);

    unlink("/data/OrionHEN/OrionHEN.log");
    unlink("/data/OrionHEN/OrionHEN_crash.log");

    payload_args_t *args = payload_get_args();
    kernel_base = args->kdata_base_addr;

    OrionHEN_log("=========== starting OrionHEN (0x%X) ... ===========", fw_ver);
    (void)sceKernelMprotect(&buz[0], 100, 0x7); // probe mprotect / kstuff state
    bool toolbox_only = (fw_ver >= 0x10000);
    is_800 = (fw_ver >= 0x800);


    LoadSettings();

    // Start threads
    get_ip_address(&buz[0]);
    pthread_create(&fifo_thr, nullptr, fifo_and_dumper_thread, nullptr);
    pthread_create(&pt_thr, nullptr, Play_time_thread, nullptr);
    pthread_create(&msg_thr, nullptr, IPC_loop, nullptr);
    orion_ready_signal(ORION_READY_DAEMON);

    OrionHEN_log("is toolbox only: %s | ver: %x", toolbox_only ? "Yes" : "No", sys_ver.version);
    // Initialize toolbox if needed
    if (g_settings.toolbox_auto_start) {
        cmd_enable_toolbox();
    }
    else if (!g_settings.toolbox_auto_start) {
        orion_notify(true, "the OrionHEN Toolbox auto start is disabled in the config.ini\n\n"
                    "Re-enable toolbox_auto_start in /data/OrionHEN/config.ini or open Debug Settings");
    }

     const char json_payload[] =
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
     "            \"actionUrl\": \"pssettings:play?function=debug_settings\"\n"
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
	sceNotificationSend(0xFE, true, &json_payload[0]);


    OrionHEN_log("StartUp thread created!! - welcome to OrionHEN");

    // Launch the appropriate app based on configuration
    const char *URI = nullptr;
    switch (g_settings.start_option) {
    case HOME_MENU: {
        URI = "pshomeui:navigateToHome?bootCondition=psButton";
        break;
    }
    case TOOLBOX: {
        if (g_settings.toolbox_auto_start)
            URI = "pssettings:play?mode=settings&function=debug_settings";
        else
            URI = "pshomeui:navigateToHome?bootCondition=psButton";
        break;
    }
    case SETTINGS: {
        URI = "pssettings:play?mode=settings";
        break;
    }
    default:
        OrionHEN_log("unknown opt %d", g_settings.start_option);
        break;
    }

    if (URI)
        OrionHEN_log("ret %d", ItemzLaunchByUri(URI));

    if(g_settings.auto_eject_disc){
        sceShellCoreUtilRequestEjectDevice("/dev/cd0");
    }

    // Main loop to keep the process running
    while (true) {
        pthread_join(msg_thr, NULL);
        pthread_create(&msg_thr, nullptr, IPC_loop, nullptr);
        sleep(1);
    }

    puts("main thread ended");
    return 0;
}
