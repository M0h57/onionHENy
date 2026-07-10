/* Copyright (C) 2025 OrionHEN / LightningMods — P0 split. */


#include "HookedFuncs.hpp"
#include "ipc.hpp"
#include <atomic>
#include <string>

#include "shellui_state.hpp"

void* load_plugin_thread(void* args) {
    Plugins *plugin = (Plugins*)args;

    notify("Loading Plugin %s ...", plugin->path.c_str());
    IPC_Client& util_ipc = IPC_Client::getInstance(true);
    if (util_ipc.LaunchPlugin(plugin->path, plugin->tid) != IPC_Ret::NO_ERROR) {
        notify("Failed to launch plugin %s (%s)", plugin->path.c_str(), plugin->tid.c_str());
    }

    delete plugin;
    pthread_exit(nullptr);
    return nullptr;
}
void* download_cheats_thr(void*){
    if(g_ui.cheat_action_in_progress){
        notify("Cheat action already in progress, please wait for it to complete...");
        pthread_exit(nullptr);
        return nullptr;
    }
    g_ui.cheat_action_in_progress = true;
    notify("Preparing to download the %s cheats repo...", g_settings.selected_cheats_repo == CHEATS_REPO_ORIONHEN ? "OrionHEN PS5" : "GoldHEN PS4");
    IPC_Client& util_ipc = IPC_Client::getInstance(true);
    // daemon shows notification when done
    util_ipc.Cheats_Action(DOWNLOAD_CHEATS, g_settings.selected_cheats_repo);
    
    g_ui.cheat_action_in_progress = false;
    pthread_exit(nullptr);
    return nullptr;
}

void* kstuff_download_thread(void* args) {
    if (g_ui.download_kstuff_thread_in_progress) {
        notify("Download action already in progress, please wait for it to complete...");
        pthread_exit(nullptr);
        return nullptr;
    }
    g_ui.download_kstuff_thread_in_progress = true;
    IPC_Client& util_ipc = IPC_Client::getInstance(true);
    shellui_log("Ret: 0x%X", util_ipc.DownloadKstuff());
    g_ui.download_kstuff_thread_in_progress = false;
    pthread_exit(nullptr);
    return nullptr;
}

/* OnPress_Hook → hook_onpress.cpp */

