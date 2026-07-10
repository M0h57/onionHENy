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
#include "cheats/CheatService.hpp"
#include <orion/settings.hpp>
#include <orion/platform.h>
#include <orion/ucred.h>
#include <orion/proc_query.h>
#include <orion/ready.h>
extern "C" {
#include "freebsd-helper.h"
}
#include <cstdint>
#include <hijacker/hijacker.hpp>
#include <sys/_pthreadtypes.h>
#include <unistd.h>

typedef struct app_info {
    uint32_t app_id;
    uint64_t unknown1;
    uint32_t app_type;
    char     title_id[10];
    char     unknown2[0x3c];
} app_info_t;

pthread_t cmd_server = 0;

extern "C" {

    #include "faulthandler.h"
  #include "common_utils.h"
  #include <ps5/payload.h>

  int sceKernelGetAppInfo(pid_t pid, app_info_t * info);
  int sceKernelGetProcessName(int pid, char * out);
  int _sceApplicationGetAppId(int pid, uint32_t * appId);

  // set_proc_authid / get_proc_by_pid: liborion_proc
}

extern bool is_handler_enabled;

orion::SettingsStore g_settings;
atomic_bool g_legacy_cmd_server = false;
atomic_bool g_legacy_cmd_server_exit = false;
void start_ip_thread(void);
void* runCommandNControlServer(void*);
void patch_checker(void);
void* IPC_loop(void* args);
bool shellui_patch(void);

extern atomic_bool no_network_rest_mode_action;

jmp_buf g_catch_buf;
uintptr_t kernel_base = 0;
void* __stack_chk_guard = (void*)0xdeadbeef;

static void cleanup(void) {
    orion_notify(true, "OrionHEN utilities daemon has crashed...\n\nAttemping to recover...");
    exit(1);
}

void __stack_chk_fail(void) {
    puts("Stack smashing detected.");
}

bool LoadSettings() {
    orion::Settings s{};
    if (!orion::settings_load(&s)) {
        OrionHEN_log("config.ini missing; using defaults (path primary=%s)",
                     orion::kConfigPathPrimary);
    } else {
        OrionHEN_log("Loaded settings from %s", orion::settings_last_loaded_path());
    }

    g_settings.store(s);
    g_legacy_cmd_server = s.legacy_cmd_server;
    /* Missing file is not an error — defaults were applied. */
    return true;
}

int main(void) {
    pthread_t ipc_server = 0;
    char tmp_buf[200];
    
    sceNetCtlInit();
    sceUserServiceInitialize(NULL);
    orion_log_configure("OrionHEN utils", "/data/OrionHEN/OrionHEN_util_daemon.log");
    OrionHEN_log("util daemon entered");

    if (setjmp(g_catch_buf) == 0)
        OrionHEN_log("jump has been set");
    else
        orion_notify(true, "The Fatal error has been successfully resolved\n\nyou have nothing to worry about");

    OrionHEN_log("Registering signal handler...");
    fault_handler_init(cleanup);
    OrionHEN_log("   Success!");

    payload_args_t* args = payload_get_args();
    kernel_base = args->kdata_base_addr;
    set_proc_authid(getpid(), DEBUG_AUTHID);

	g_legacy_cmd_server_exit = false;

    unlink("/data/OrionHEN/OrionHEN_util_daemon.log");
    unlink("/data/OrionHEN/OrionHEN_util_crash.log");

    OrionHEN_log("=========== starting OrionHEN Utilities... ===========");

    LoadSettings();

    start_ip_thread();
    pthread_create(&ipc_server, NULL, IPC_loop, NULL);
    /* IPC thread is up — publish ready for bootstrapper/daemon consumers */
    orion_ready_signal(ORION_READY_UTIL);

    if (!IniliatizeHTTP()) {
        OrionHEN_log("Failed to initialize HTTP lib");
        orion_notify(true, "Failed to initialize the HTTP lib, downloading cheats will not work");
    }

    if (g_settings.snapshot().toolbox_auto_start &&
        orion_ready_is_set(ORION_FLAG_UTIL_BOOTED)) {
        OrionHEN_log("util already booted once — activating toolbox path");
        patch_checker();
    }
    /* Mark that util completed cold start (typed flag; replaces util_first_boot file). */
    orion_ready_signal(ORION_FLAG_UTIL_BOOTED);

    for (;;) {
        // for rest mode we wait til we can restart everything
        if (g_settings.snapshot().toolbox_auto_start &&
            get_ip_address(&tmp_buf[0]) < 0) {
            sleep(1);

            bool fail1 = get_ip_address(&tmp_buf[0]) < 0;
            if (!fail1)
                continue;

            sleep(2);

            bool fail2 = get_ip_address(&tmp_buf[0]) < 0;
            if (!fail2)
                continue;

            if (no_network_rest_mode_action) {
                patch_checker();
            }
            continue;
        }
        no_network_rest_mode_action = false;

        pthread_create(&cmd_server, NULL, runCommandNControlServer, NULL);
        OrionHEN_log("loading settings...");
        LoadSettings();
        OrionHEN_log("done loading settings...");

        OrionHEN_log("Initializing cheat engine...");
        orion::cheats::CheatService::instance().ensureDir();

        pthread_join(cmd_server, NULL);

        usleep(SLEEP_PERIOD);
    }
    
    return 0;
}
