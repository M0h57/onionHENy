/* Copyright (C) 2025 OrionHEN / LightningMods — P0 split. */


#include <orion/platform.h>
#include <orion/ready.h>
#include <orion/ipc_client.hpp>
#include <orion/settings.hpp>
#include <orion/toolbox_timing.h>
#include "common_utils.h"
#include <atomic>
#include <unistd.h>
#include <cstring>

extern std::atomic_bool no_network_rest_mode_action;
extern std::atomic_bool no_network_patched;
extern std::atomic_bool real_rest_mode_detected;

extern "C" {
int sceUserServiceGetLoginUserIdList(void *list);
int sceUserServiceGetUserName(const int userId, char *userName, const size_t size);
}
struct UserServiceLoginUserIdList { int user_id[4]; };

bool enable_toolbox() {
    // Single client path into crit daemon (replaces hand-rolled Unix socket).
    // Wait briefly for crit socket / ready — bootstrap order is util then daemon.
    for (int wait = 0; wait <= 20; ++wait) {
      if (orion_ready_is_set(ORION_READY_DAEMON) ||
          if_exists("/system_tmp/OrionHEN_crit_service")) {
        break;
      }
      if (wait == 20) {
        orion_notify(true, "Failed to load the OrionHEN toolbox");
        return false;
      }
      sleep(1);
    }
    return IPC_Client::getInstance(/*util=*/false).EnableToolbox();
}


bool isUserLoggedIn() {
    bool isLoggedIn = false;
    UserServiceLoginUserIdList userIdList;
    (void)memset(&userIdList, 0, sizeof(UserServiceLoginUserIdList));
    
    if (sceUserServiceGetLoginUserIdList(&userIdList) < 0) {
        return false;
    }

    for (int i = 0; i < 4; i++) {
        char username[500] = {0};
        int userid = userIdList.user_id[i];
        if (userid != -1) {
            int ret = sceUserServiceGetUserName(userid, &username[0], sizeof(username));
            OrionHEN_log("sceUserServiceGetUserName returned %d", ret);
            if (ret == 0) {
                isLoggedIn = true;
                break;
            }
        }
    }
    
    sleep(5);
    return isLoggedIn;
}
/**
 * Re-request toolbox inject via crit daemon.
 * @param rest_resume  true only for real rest-mode recovery paths.
 *                     false for util restart / re-HEN reinject (no rest copy).
 */
void patch_checker(bool rest_resume) {
    if (!isUserLoggedIn()) {
        OrionHEN_log("User is not logged in yet, skipping toolbox reinject...");
        return;
    }

    LoadSettings();
    const orion::Settings cfg = g_settings.snapshot();
    if (rest_resume && cfg.disable_toolbox_auto_start_for_rest_mode) {
        OrionHEN_log("Toolbox auto start for rest mode is disabled");
        return;
    }

    if (rest_resume &&
        orion_toolbox_should_apply_rest_delay(true, cfg.rest_mode_delay_seconds)) {
        OrionHEN_log("rest resume delay %llu secs",
                     static_cast<unsigned long long>(cfg.rest_mode_delay_seconds));
        sleep(static_cast<unsigned int>(cfg.rest_mode_delay_seconds));
        orion_notify(true,
                     "Coming out of Rest Mode — re-activating the OrionHEN toolbox...");
    } else {
        OrionHEN_log("toolbox reinject (not rest resume)");
    }

    if (!enable_toolbox()) {
        orion_notify(true, "Failed to inject toolbox");
    }

    if (rest_resume) {
        no_network_rest_mode_action = false;
        no_network_patched = true;
        real_rest_mode_detected = false;
    }
}


