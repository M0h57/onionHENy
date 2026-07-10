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

#include <orion/platform.h>
#include <orion/ipc_client.hpp>
#include <orion/ready.h>
#include <orion/proc_query.h>
#include <orion/settings.hpp>
#include <orion/hijack_retry.h>
#include "common_utils.h"
#include <string>
#include <vector>
#include <unistd.h>
#include <atomic>
#include <pthread.h>
#include <sys/sysctl.h>

#include "dbg/dbg.hpp"
#include "elf/elf.hpp"
#include "hijacker/hijacker.hpp"
#include "ipc.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ps5/kernel.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>


// External C declarations
extern "C" {
    #include "common_utils.h"
    #include "global.h"
    #include "pt.h"

    int sceUserServiceGetUserName(const int userId, char *userName, const size_t size);
    int _sceApplicationGetAppId(int pid, int32_t *appId);
    int sceSystemServiceGetAppTitleId(uint32_t appId, char *titleId);
    int sceLncUtilGetAppStatusList(AppStatus *outStatusList, uint32_t numEntries, uint32_t *outEntries);
    int sceKernelGetProcessName(int pid, char *out);
    int sceKernelIsGenuineDevKit();

    extern uint8_t shellui_elf_start[];
    extern const unsigned int shellui_elf_size;

    // Atomic state variables
    atomic_bool rest_mode_action = false;
    atomic_bool no_network_rest_mode_action = false;
    atomic_bool no_network_patched = false;
    atomic_bool real_rest_mode_detected = false;
}

// Types and Constants
enum write_flag : uint32_t {
    no_flag = 0,
    isOffsetConfigureOutput = 1 << 1,
    isOffsetVideoModeSupported = 1 << 2,
};

struct Command {
    unsigned int magic = 0;
    Commands cmd = INVALID_CMD;
    int PID = -1;
    int ret = 0;
    char msg1[0x500];
    char msg2[0x500];
};

// Global variables
char ip_address[40];
int numb_of_tries = 0;
int retries = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t jb_lock = PTHREAD_MUTEX_INITIALIZER;
extern atomic_bool not_connected;

// Function forward declarations
void check_addr_change(void);
// get_ip_address: common_utils.h (always C linkage)

// util_toolbox.cpp
bool enable_toolbox();
bool isUserLoggedIn();
void patch_checker(bool rest_resume);

// Command server functions
static void replyError(int sock) {
    Command cmd;
    cmd.ret = -1;
    send(sock, reinterpret_cast<void *>(&cmd), sizeof(cmd), MSG_NOSIGNAL);
}

static void replyOk(int sock) {
    const Command cmd{};
    send(sock, &cmd, sizeof(cmd), MSG_NOSIGNAL);
}

void cmd_server(int sock, Command &cmd) {
    pthread_mutex_lock(&jb_lock);
    UniquePtr<Hijacker> spawned = nullptr;
    OrionHEN_log("command: %u", cmd.cmd);
    
    if (cmd.cmd == 0) {
        numb_of_tries++;
    }

    if (numb_of_tries > 40) {
        numb_of_tries = 0;
    }

    switch (cmd.cmd) {
    case JAILBREAK_CMD:
        if (cmd.magic != 0xDEADBEEF) {
            orion_notify(true, "Jailbreak failed, magic is invaild");
            replyError(sock);
            break;
        }
        if (cmd.PID == -1 || !isProcessAlive(cmd.PID)) {
            orion_notify(true, "Jailbreak failed, PID is invaild");
            replyError(sock);
            break;
        }
        
        OrionHEN_log("WRONG Jailbreak command received: jailbreaking...");
        {
            do {
                spawned = Hijacker::getHijacker(cmd.PID);
                if (spawned == nullptr) {
                    retries++;
                    OrionHEN_log("is null for PID %d (attempt %d)", cmd.PID,
                                 retries);
                    if (orion_hijack_retry_should_stop(isProcessAlive(cmd.PID),
                                                       retries, 30)) {
                        orion_notify(true, "Jailbreak failed, PID is invaild");
                        OrionHEN_log("Jailbreak failed, PID is invaild");
                        break;
                    }
                }
            } while (spawned == nullptr);

            retries = 0;

            if (!spawned) {
                replyError(sock);
                break;
            }

            orion_notify(true, "[Legacy] App has been granted a jailbreak\n\nAn update for "
                      "this PKG is available");
            spawned->jailbreak(true);
            OrionHEN_log("jailbroke app %s", cmd.msg1);
        }
        replyOk(sock);
        break;
        
    case INVALID_CMD:
        puts("invalid command");
        replyError(sock);
        break;
        
    default:
        puts("default command");
        orion_notify(true, "Update the PKG you are using before continuing\nGot Command %i",
              cmd.cmd);
        replyError(sock);
        break;
    }
    
    pthread_mutex_unlock(&jb_lock);
}

void *runCommandNControlServer(void *) {
    int client = -1;
    int s = -1;
    int readSize = 0;
    Command cmd;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
        orion_notify(true, "Failed to create socket %s", strerror(errno));
        return nullptr;
    }

    struct sockaddr_in sockaddr;
    bzero(&sockaddr, sizeof(sockaddr));

    int optval = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(9028);
    sockaddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (const struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
        orion_notify(true, "Failed to bind to port 9028 %s", strerror(errno));
        return nullptr;
    }

    if (listen(s, 5) < 0) {
        orion_notify(true, "Failed to listen on port 9028 %s", strerror(errno));
        return nullptr;
    }

    if(g_legacy_cmd_server)
	   OrionHEN_log("[Daemon LEGACY IPC] Server started on port 9028");

    // Accept clients
    while (!g_legacy_cmd_server_exit) {
        client = accept(s, 0, 0);
        if (errno == 0xA3) {
            pthread_mutex_lock(&jb_lock);
            rest_mode_action = true;
            pthread_mutex_unlock(&jb_lock);
            break;
        }
        if (client > 0 && g_legacy_cmd_server) {
            OrionHEN_log("[Daemon IPC] Client connected");
            while ((readSize = recv(client, reinterpret_cast<void *>(&cmd),
                                  sizeof(cmd), MSG_NOSIGNAL)) > 0) {
                if (cmd.magic == 0xDEADBEEF ) {
                    cmd_server(client, cmd);
                } else {
                    OrionHEN_log("[Daemon IPC] Invalid magic number");
                }
            }
        }
    }

    if (client >= 0)
        close(client), client = -1;

    if (s >= 0)
        close(s), s = -1;

    OrionHEN_log("[Daemon IPC] Server stopped");

    if (g_legacy_cmd_server_exit) {
        g_legacy_cmd_server_exit = false;
		return runCommandNControlServer(nullptr);
    }
    return nullptr;
}

// Network monitoring and restart functionality
void check_addr_change(void) {
    pthread_mutex_lock(&jb_lock);
    char func_ip_address[40];

    if (get_ip_address(&func_ip_address[0]) < 0) {
        pthread_mutex_unlock(&jb_lock);
        return;
    }

    if (get_ip_address(&ip_address[0]) < 0) {
        pthread_mutex_unlock(&jb_lock);
        return;
    }

    bool ip_changed = strcmp(&ip_address[0], &func_ip_address[0]) != 0;
    if (ip_changed || rest_mode_action) {
        if (ip_changed || !real_rest_mode_detected) {
            orion_notify(true, "IP Address changed to %s, restarting server(s)",
                  func_ip_address);
        } else if (rest_mode_action && !no_network_patched && !not_connected &&
                  real_rest_mode_detected) {
            LoadSettings();
            const uint64_t delay =
                g_settings.snapshot().rest_mode_delay_seconds;
            OrionHEN_log("sleeping for %llu secs",
                         static_cast<unsigned long long>(delay));
            sleep(static_cast<unsigned int>(delay));
            orion_notify(true, "Coming out of Rest Mode detected, restarting server(s)");
            OrionHEN_log("waiting for logged in user");
            
            while (!isUserLoggedIn()) {
                sleep(2);
            }
            
            OrionHEN_log("user is logged in");
            OrionHEN_log("Coming out rest mode, activating patches");
            

            const orion::Settings cfg = g_settings.snapshot();
            if (cfg.toolbox_auto_start &&
                !cfg.disable_toolbox_auto_start_for_rest_mode &&
                !enable_toolbox()) {
                orion_notify(true, "Failed to inject toolbox");
            }
        }
        
        real_rest_mode_detected = not_connected = no_network_patched = rest_mode_action = false;
    }
    
    pthread_mutex_unlock(&jb_lock);
}

void *ip_thread(void *arg) {
    (void)arg;
    do {
        sleep(1);
    } while (get_ip_address(&ip_address[0]) < 0);

    while (true) {
        check_addr_change();
        sleep(2);
    }
}

void start_ip_thread(void) {
    pthread_t ip_thread_thr;
    pthread_create(&ip_thread_thr, NULL, ip_thread, NULL);
    pthread_detach(ip_thread_thr);
}

// System recovery and patch checker
