/* Copyright (C) 2025 OrionHEN / LightningMods
 * ShellUI OnPress_Hook — toolbox/settings/cheats press dispatch.
 * Other hooks remain in HookFunctions.cpp.
 */
#include "HookedFuncs.hpp"
#include "RemotePlay.h"
#include "Detour.h"
#include "ipc.hpp"
#include <climits>
#include <msg.hpp>
#include <pthread.h>
#include <sys/stat.h>
#include <fstream>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <cstring>

extern int (*oOnPress)(MonoObject* Instance, MonoObject* element, MonoObject* e);

extern bool is_current_game_open;
extern std::atomic_bool cheat_action_in_progress;
extern std::atomic_bool download_kstuff_thread_in_progress;
extern int cheatEnabledMap[];
extern std::string currentCheatTID;

#define MAX_CHEATS 256

void ParseCheatID(const char* id, char* tid, int* cheat_id);
void RemoveGameWidget(RemoveWidget widget);
void CreateGameWidget(CreateWidget widget);
bool if_exists(const char* path);
int usbpath();
void notify(const char *text, ...);
bool SaveSettings();
std::string GetPropertyValue(MonoObject* element, const char* propertyName);
void *load_plugin_thread(void *args);
void *download_cheats_thr(void *);
void *kstuff_download_thread(void *args);
extern bool is_6xx, is_3xx;
extern std::string remote_play_info;
extern std::string running_tid;

int OnPress_Hook(MonoObject* Instance, MonoObject* element, MonoObject* e)
{
    bool& DPI = g_settings.DPI;
    bool& Data_SB = g_settings.allow_data_in_sandbox;
    int& StartOption = g_settings.start_option;
    bool& util_rest_kill = g_settings.util_rest_kill;
    bool& game_rest_kill = g_settings.game_rest_kill;
    uint64_t& delay_secs = g_settings.rest_mode_delay_seconds;
    bool& DPI_v2 = g_settings.DPI_v2;
    bool& dis_tids = g_settings.display_tids;
    int& selected_cheats_repo = g_settings.selected_cheats_repo;

    // Define the array of IDs to exclude (you can put this at the top of your function or as a static/global)
    const std::vector<std::string> excludedIds = {
        "id_dl_cheats",
        "id_save_rp_info",
        "id_download_kstuff",
        "id_delete_kstuff"
    };


    // shellui_log("OnPress_Hook: %p, %p, %p", Instance, element, e);
    if (!Instance || !element)
    {
#if SHELL_DEBUG==1
        shellui_log("[LM HOOK] OnPress_Hook: args are null");
#endif
        return oOnPress(Instance, element, e);
    }

    std::string id = GetPropertyValue(element, "Id");
    std::string value = GetPropertyValue(element, "Value");
    std::string title = GetPropertyValue(element, "Title");

    bool is_cust_pkg = (id.rfind("id_pkg_") != std::string::npos);
    bool is_orionhen_pl = (id.rfind("id_orionhen_pl_loader_") != std::string::npos);

    if (id.rfind("id_cheat_") != std::string::npos && !is_current_game_open) {
        notify("The Game is not running, to activate cheats launch the game first");
#if SHELL_DEBUG==1
        shellui_log("Failed to activate %s, game is not running", id.c_str());
#endif
        return oOnPress(Instance, element, e);
    }

    // Check if id is in the excluded list
    bool isExcludedId = std::find(excludedIds.begin(), excludedIds.end(), id) != excludedIds.end();
    if (value.empty() && !isExcludedId && !is_cust_pkg && !is_orionhen_pl) {
#if SHELL_DEBUG==1
        shellui_log("[LM HOOK] OnPress_Hook: Id: %s has no value set", id.c_str());
#endif
        return oOnPress(Instance, element, e);
    }


    bool reload_main_settings = false;
    bool reload_util_settings = false;

#if SHELL_DEBUG==1
    shellui_log("[LM HOOK] OnPress_Hook: Id: %s, Value: %s", id.c_str(), value.c_str());
#endif
    if (id == "id_download_kstuff") {
        pthread_t thr;
        pthread_create(&thr, nullptr, kstuff_download_thread, nullptr);
        pthread_detach(thr);
    }
    else if (id == "id_overlay_gpu") {
		if (atoi(value.c_str()) == g_settings.overlay_gpu) {
			return oOnPress(Instance, element, e);
		}
        if (!atoi(value.c_str())) {
            RemoveGameWidget(REMOVE_GPU_OVERLAY);
        }
        else {
			CreateGameWidget(CREATE_GPU_OVERLAY);
        }

        g_settings.overlay_gpu = !g_settings.overlay_gpu;
    }
    else if (id == "id_overlay_cpu") {
		if (atoi(value.c_str()) == g_settings.overlay_cpu) {
			return oOnPress(Instance, element, e);
		}
        if (!atoi(value.c_str())) {
            if (!g_all_cpu_usage) {
				RemoveGameWidget(REMOVE_CPU_OVERLAY);
            }
            else {
				notify("To disable CPU overlay, please disable the All CPU usage option first");
				return oOnPress(Instance, element, e);
            }
        }
        else {
			CreateGameWidget(CREATE_CPU_OVERLAY);
            
        }

        g_settings.overlay_cpu = !g_settings.overlay_cpu;
    }
    else if (id == "id_overlay_ram") {
		if (atoi(value.c_str()) == g_settings.overlay_ram) {
			return oOnPress(Instance, element, e);
		}
        if (!atoi(value.c_str())) {
			RemoveGameWidget(REMOVE_RAM_OVERLAY);
        }
        else {
			CreateGameWidget(CREATE_RAM_OVERLAY);   
        }

        g_settings.overlay_ram = !g_settings.overlay_ram;
    }
    else if (id == "id_overlay_fps") {
		if (atoi(value.c_str()) == g_settings.overlay_fps) {
			return oOnPress(Instance, element, e);
		}
        if (!atoi(value.c_str())) {
			RemoveGameWidget(REMOVE_FPS_OVERLAY);
            unlink("/system_tmp/fps_enabled");
            
        }
        else {
			CreateGameWidget(CREATE_FPS_OVERLAY);
            touch_file("/system_tmp/fps_enabled");
        }

        g_settings.overlay_fps = !g_settings.overlay_fps;
    }
    else if (id == "id_overlay_ip") {
		if (atoi(value.c_str()) == g_settings.overlay_ip) {
			return oOnPress(Instance, element, e);
		}
        if (!atoi(value.c_str())) {
            RemoveGameWidget(REMOVE_IP_OVERLAY);
        }
        else {
            CreateGameWidget(CREATE_IP_OVERLAY);
        }

        g_settings.overlay_ip = !g_settings.overlay_ip;
	}
    else if (id == "id_all_cpu_usage") {
        if (g_all_cpu_usage == atoi(value.c_str())) {
            return oOnPress(Instance, element, e);
		}
        if(!g_settings.overlay_cpu){
            notify("To change CPU overlay mode, please enable the CPU overlay first");
            return oOnPress(Instance, element, e);
		}
        g_all_cpu_usage = !g_all_cpu_usage;
    }
    else if (id == "id_overlay_change_pos") {

        if((overlay_positions)atoi(value.c_str()) == g_settings.overlay_pos){
            return oOnPress(Instance, element, e);
		}

        g_settings.overlay_pos = (overlay_positions)atoi(value.c_str());

        if (g_settings.overlay_pos == OVERLAY_POS_TOP_LEFT) {
            g_overlay_layout.overlay_fps_x = 10.0f;
            g_overlay_layout.overlay_fps_y = 10.0f;

            g_overlay_layout.overlay_gpu_x = 10.0f;
            g_overlay_layout.overlay_gpu_y = 35.0f;

            g_overlay_layout.overlay_cpu_x = 10.0f;
            g_overlay_layout.overlay_cpu_y = 60.0f;

            g_overlay_layout.overlay_ram_x = 10.0f;
            g_overlay_layout.overlay_ram_y = 85.0f;

            g_overlay_layout.overlay_ip_x = 10.0f;
            g_overlay_layout.overlay_ip_y = 110.0f;
        }
        else if (g_settings.overlay_pos == OVERLAY_POS_BOTTOM_LEFT) {
            g_overlay_layout.overlay_ram_x = 10.0f;
            g_overlay_layout.overlay_ram_y = 970.0f;
            g_overlay_layout.overlay_cpu_x = 10.0f;
            g_overlay_layout.overlay_cpu_y = 990.0f;
            g_overlay_layout.overlay_gpu_x = 10.0f;
            g_overlay_layout.overlay_gpu_y = 1010.0f;
            g_overlay_layout.overlay_fps_x = 10.0f;
            g_overlay_layout.overlay_fps_y = 1030.0f;
            g_overlay_layout.overlay_ip_x = 10.0f;
            g_overlay_layout.overlay_ip_y = 1050.0f;
        }
        else if (g_settings.overlay_pos == OVERLAY_POS_TOP_RIGHT) {
            g_overlay_layout.overlay_fps_x = 1720.0f;
            g_overlay_layout.overlay_fps_y = 10.0f;
            g_overlay_layout.overlay_gpu_x = 1720.0f;
            g_overlay_layout.overlay_gpu_y = 35.0f;
            g_overlay_layout.overlay_cpu_x = 1720.0f;
            g_overlay_layout.overlay_cpu_y = 60.0f;
            g_overlay_layout.overlay_ram_x = 1720.0f;
            g_overlay_layout.overlay_ram_y = 85.0f;
            g_overlay_layout.overlay_ip_x = 1670.0f;;
            g_overlay_layout.overlay_ip_y = 110.0f;
        }
        else if (g_settings.overlay_pos == OVERLAY_POS_BOTTOM_RIGHT) {
            g_overlay_layout.overlay_ram_x = 1720.0f;
            g_overlay_layout.overlay_ram_y = 970.0f;
            g_overlay_layout.overlay_cpu_x = 1720.0f;
            g_overlay_layout.overlay_cpu_y = 990.0f;
            g_overlay_layout.overlay_gpu_x = 1720.0f;
            g_overlay_layout.overlay_gpu_y = 1010.0f;
            g_overlay_layout.overlay_fps_x = 1720.0f;
            g_overlay_layout.overlay_fps_y = 1030.0f;
            g_overlay_layout.overlay_ip_x = 1670.0f;
            g_overlay_layout.overlay_ip_y = 1050.0f;
        }
       
        if (g_settings.overlay_cpu) {
            RemoveGameWidget(REMOVE_CPU_OVERLAY);
            CreateGameWidget(CREATE_CPU_OVERLAY);
		}
        if (g_settings.overlay_ram) {
            RemoveGameWidget(REMOVE_RAM_OVERLAY);
			CreateGameWidget(CREATE_RAM_OVERLAY);
        }
		if (g_settings.overlay_gpu) {
			RemoveGameWidget(REMOVE_GPU_OVERLAY);
			CreateGameWidget(CREATE_GPU_OVERLAY);
        }
        if (g_settings.overlay_fps) {
            RemoveGameWidget(REMOVE_FPS_OVERLAY);
            CreateGameWidget(CREATE_FPS_OVERLAY);
        }
        if (g_settings.overlay_ip) {
            RemoveGameWidget(REMOVE_IP_OVERLAY);
            CreateGameWidget(CREATE_IP_OVERLAY);
		}
    }
    else if (id == "id_kstuff_autoload") {
       // if(atoi(value.c_str()) == if_exists("/user/data/OrionHEN/no_kstuff")) {
		//	return oOnPress(Instance, element, e);
		//}
        if(atol(value.c_str())){
			unlink("/user/data/OrionHEN/no_kstuff");
            notify("Kstuff will be loaded on next boot");
        }
        else{
            touch_file("/user/data/OrionHEN/no_kstuff");
            notify("Kstuff will NOT be loaded on next boot");
		}
    }
    else if (id == "id_delete_kstuff") {
       unlink("/user/data/OrionHEN/kstuff.elf");
	   notify("The external kstuff download has been deleted");
    }
    else if (id == "id_change_custom_pkg_path") {
		custom_pkg_path.path = value;
	}
    else if (id == "id_auto_eject") {
        g_settings.auto_eject_disc = atol(value.c_str());
    }
      else if (id.rfind("id_plugin") != std::string::npos)
    {
        if (!plugins_list.empty())
        {
            for (auto plugin : plugins_list)
            {
                if (plugin.id == id)
                {
                    int pid = -1;
                    if(plugin.tid.rfind(".elf") != std::string::npos && (pid = sceSystemServiceGetAppId(plugin.tid.c_str())) > 0){
                        IPC_Client::getInstance(false).ForceKillPID(pid);
                        notify("killed payload %s", plugin.tid.c_str());
                        break;
                    }
                    char pbuf[256];
                    snprintf(pbuf, sizeof(pbuf), "/system_tmp/%s.PID", plugin.tid.c_str());

                    int f = open(pbuf, O_RDONLY);
                    if (f >= 0)
                    {
                        char t[32];
                        int r = read(f, t, sizeof(t) - 1);
                        close(f);
                        if (r > 0)
                        {
                            t[r] = 0;
                            pid = atoi(t);
                        }
                    }

                    if (pid > 0)
                    {
                        char name[32];
                        if (sceKernelGetProcessName(pid, name) < 0)
                        {
                            shellui_log("Stale plugin PID file detected for %s, removing", plugin.tid.c_str());
                            unlink(pbuf);
                            pid = -1;
                        }
                    }

                    if (pid > 0 && atol(value.c_str()) == 0)
                    {
                        shellui_log("killing pid: 0x%X", pid);
                        IPC_Client::getInstance(false).ForceKillPID(pid);

                        if (plugin.tid == "XMLS00001")
                            unlink("/system_tmp/patch_plugin");

                        unlink(pbuf);

                        notify("%s killed", plugin.tid.c_str());
                        break;
                    }
                    else if (pid <= 0 && atol(value.c_str()) == 1)
                    {
                        pthread_t thr;
                        shellui_log("Plugin %s not running", plugin.tid.c_str());
                        auto plugin_info = new Plugins(plugin);
                        pthread_create(&thr, nullptr, load_plugin_thread, (void *)plugin_info);
                    }
                }
            }
        }
    }
    else if (is_cust_pkg) {
        if (custom_pkg_list.empty()) {
            return oOnPress(Instance, element, e);
        }
        for (auto selected_pkgs : custom_pkg_list) {
            if (selected_pkgs.id == id) {
#if SHELL_DEBUG==1
                shellui_log("[Clicked %s] %s path: %s", selected_pkgs.id.c_str(), selected_pkgs.name.c_str(), selected_pkgs.shellui_path.c_str());
#endif
                std::string dl_url;
                if (is_6xx)
                    dl_url = "http://127.0.0.1:12800" + selected_pkgs.path;
                else
                    dl_url = (selected_pkgs.path.rfind("/data") != std::string::npos) ? selected_pkgs.shellui_path : selected_pkgs.path;

                playgo_info_t playgoinfo = {};
                pkg_info_t pkginfo = {};
                pkg_metadata_t metainfo;
                metainfo.playgo_scenario_id = "";
                metainfo.content_name = "";
                metainfo.content_id = "";
                metainfo.icon_url = "";
                metainfo.ex_uri = "";
                metainfo.uri = dl_url.c_str();
                

                // msgok(MSG_DIALOG::NORMAL, "trying InstallByPackage");
				shellui_log("Installing package from: %s", metainfo.uri);
                int num = sceAppInstUtilInstallByPackage(&metainfo, &pkginfo, &playgoinfo);
                if (num != 0) {
					notify("Failed to install %s\nError: 0x%X\nis DPIv2 enabled???", selected_pkgs.name.c_str(), num);
                }
                else
                {
                    notify("%s installation started successfully", selected_pkgs.name.c_str());
                }
            }
        }
    }
    else if (id.rfind("id_auto_plugin") != std::string::npos) {
		if (!auto_list.empty()) {
			for (auto plugin : auto_list) {
				if (plugin.id == id) {
                    std::string auto_path = plugin.shellui_path + ".auto_start";
                    shellui_log("Auto start path: %s", auto_path.c_str());
                    if (if_exists(auto_path.c_str()) && !atol(value.c_str())) {
					            	unlink(auto_path.c_str());
					           }
                    else if(atol(value.c_str())){
						int fd = open(auto_path.c_str(), O_CREAT | O_RDWR, 0777);
						if (fd < 0) {
							notify("Failed to create auto start file");
						}
						else {
							close(fd);
						}
					}
				}
			}
		}
	}
    else if (id.rfind("id_cheat_") != std::string::npos) {
        if(!is_current_game_open){
            notify("The Game is not running, to activate cheats launch the game first");
            shellui_log("Failed to activate %s, game is not running", id.c_str());
            return oOnPress(Instance, element, e);
        }
        char tid[32];
        int cheat_id;
        std::string cheat_name;
        ParseCheatID(id.c_str(), tid, &cheat_id);
        shellui_log("Getting PID for %s", id.c_str());
        int pid = find_pid(tid, false, true, true);
        if(pid < 0) {
            notify("[ERROR] Failed to activate %s\nfailed to find game pid", cheat_name.c_str());   
            shellui_log("Failed to get pid for %s", tid);
            return oOnPress(Instance, element, e);
        }
        
        shellui_log("Got proc for %s, tid %s, pid %i", id.c_str(), tid, pid);
        
        if (IPC_Client::getInstance(true).ToggleGameCheat(pid, tid, cheat_id, cheat_name))
        {
            if (currentCheatTID != tid)
            {
                currentCheatTID = tid;
                bzero(cheatEnabledMap, MAX_CHEATS);
            }

            bool enabled = value == "1";
            cheatEnabledMap[cheat_id] = enabled;
            notify("★ %s [%s] ★", cheat_name.c_str(), enabled ? "ON" : "OFF");
        }
        else{
            notify("[ERROR] Failed to activate %s", cheat_name.c_str());   
        }
    }
    else if (id.rfind("id_orionhen_pl_loader_") != std::string::npos) {
        if (games_list.empty()) {
           return oOnPress(Instance, element, e);
        }
        for (const auto& game : games_list) {
            if (game.id == id) {
                // Payload homebrew entry selected (id_orionhen_pl_loader_*)
                break;
            }
        }
    }
    else if (id == "id_save_rp_info"){
      if(usbpath() == -1){
        notify("Failed to save Remote Play info, USB not found");
        return oOnPress(Instance, element, e);
      }

      std::string usb_rp_path = "/usb" + std::to_string(usbpath()) + "/remote_play_info.txt";
      shellui_log("Saving Remote Play info to %s", usb_rp_path.c_str());
      std::ofstream rp_file(usb_rp_path);
      if (!rp_file.is_open()) {
          notify("Failed to open Remote Play info file");
          return oOnPress(Instance, element, e);
      }
      rp_file << remote_play_info;
      rp_file.close();
      notify("Remote Play info saved to /mnt%s", usb_rp_path.c_str());

    }
    else if (id == "id_disp_titleids"){
        if (atol(value.c_str()) == dis_tids) {
            shellui_log("Display TIDs already %s", dis_tids ? "Enabled" : "Disabled");
            return oOnPress(Instance, element, e);
        }
        dis_tids = !dis_tids;
        ReloadRNPSApp("NPXS40002");
    }
    else if (id == "id_enable_fan_speed") {
        if (atol(value.c_str()) == g_settings.enable_fan_speed) {
            shellui_log("Fan speed control already %s", g_settings.enable_fan_speed ? "Enabled" : "Disabled");
            return oOnPress(Instance, element, e);
        }
        g_settings.enable_fan_speed = !g_settings.enable_fan_speed;
        IPC_Client::getInstance(false).Set_Fan_Threshold(g_settings.fan_threshold, g_settings.enable_fan_speed);

    }
    else if (id == "id_lm_test")
    {
        shellui_log("LM's Test Button Pressed");
        //call_show_alert(element, "msg_error_remoteplay_use_feature");
        //SendShelluiNotify();
        // notify("LM's Test Button Pressed (123)");
    }
    else if (id == "id_orionhen_credits") {
        // notify("Home Menu Button Pressed");
        return oOnPress(Instance, element, e);
    }//
    else if (id == "id_dl_cheats") {
        pthread_t thr;
        pthread_create(&thr, nullptr, download_cheats_thr, nullptr);
        pthread_detach(thr);
        return oOnPress(Instance, element, e);
    }//
    else if (id == "id_dpi_service") {
        if (atoi(value.c_str()) == DPI) {
            shellui_log("DPI already %s", DPI ? "Enabled" : "Disabled");
            return oOnPress(Instance, element, e);
        }
        DPI = !DPI;
        if (!IPC_Client::getInstance(true).ToggleDPI(DPI, false)) {
            notify(DPI ? "DPI Server Failed to Start ..." : "DPI Server Failed to Stop ...");
            DPI = !DPI;
        }
    }
    else if (id == "id_DPI_v2_service") {
        if (atoi(value.c_str()) == DPI_v2) {
            shellui_log("DPI_v2 already %s", DPI_v2 ? "Enabled" : "Disabled");
            return oOnPress(Instance, element, e);
        }
        DPI_v2 = !DPI_v2;
        if (!IPC_Client::getInstance(true).ToggleDPI(DPI_v2, true)) {
            notify(DPI_v2 ? "DPI_v2 Server Failed to Start ..." : "DPI_v2 Server Failed to Stop ...");
            DPI_v2 = !DPI_v2;
        }
    }
    else if (id == "id_debug_jb") {
        if (atoi(value.c_str()) == g_settings.debug_app_jb_msg) {
            shellui_log("Debug JB already %s", g_settings.debug_app_jb_msg ? "Enabled" : "Disabled");
            return oOnPress(Instance, element, e);
        }
        g_settings.debug_app_jb_msg = !g_settings.debug_app_jb_msg;
        reload_main_settings = true;
    }
    else if (id == "id_debug_legacy_cmd") {
        if (atoi(value.c_str()) == g_settings.legacy_cmd_server) {
            shellui_log("Debug cmd already %s", g_settings.legacy_cmd_server ? "Enabled" : "Disabled");
            return oOnPress(Instance, element, e);
        }
        g_settings.legacy_cmd_server = !g_settings.legacy_cmd_server;

        if (IPC_Client::getInstance(true).ToggleSetting(BREW_UTIL_TOGGLE_LEGACY_CMD_SERVER, g_settings.legacy_cmd_server) != IPC_Ret::NO_ERROR) {
            notify(g_settings.legacy_cmd_server ? "cmd Failed to Start ..." : "CMD Server Failed to Stop ...");
            g_settings.legacy_cmd_server = !g_settings.legacy_cmd_server;
        }//
    }
    else if (id == "id_custom_game_opts") {
        if (atoi(value.c_str()) == g_settings.orionhen_game_opts) {
            shellui_log("OrionHEN Game Options already %s", g_settings.orionhen_game_opts ? "Enabled" : "Disabled");
            return oOnPress(Instance, element, e);
        }
        g_settings.orionhen_game_opts = !g_settings.orionhen_game_opts;
        shellui_log("OrionHEN Game Options: %s", g_settings.orionhen_game_opts ? "Enabled" : "Disabled");
    }
    else if (id == "id_start_opt") {
        StartOption = atoi(value.c_str());
        shellui_log("Start option: %d", StartOption);
    }
    else if (id == "id_selected_cheats_repo") {
        selected_cheats_repo = static_cast<cheats_repo_source>(atoi(value.c_str()));
        shellui_log("Selected cheats repo: %s", selected_cheats_repo == CHEATS_REPO_ORIONHEN ? "OrionHEN PS5" : "GoldHEN PS4");
    }
    else if (id == "id_data_sb") {
        if (atoi(value.c_str()) == Data_SB) {
            shellui_log("Data Sandbox already %s", Data_SB ? "Enabled" : "Disabled");
            return oOnPress(Instance, element, e);
        }
        Data_SB = !Data_SB;
    }
    else if(id == "id_toolbox_auto_start"){
        if (atoi(value.c_str()) == g_settings.toolbox_auto_start) {
            shellui_log("toolbox Access already %s", g_settings.toolbox_auto_start ? "Enabled" : "Disabled");
            return oOnPress(Instance, element, e);
        }
        g_settings.toolbox_auto_start = !g_settings.toolbox_auto_start;

    }
    else if (id == "id_sistro_ps5debug") {
        notify("PS5Debug is not bundled in OrionHEN");
    }
    else if (id == "id_rest_1") {
        delay_secs = atol(value.c_str());
    }
    else if (id == "id_fan_speed") {
        int &fan_speed = g_settings.fan_threshold;
        fan_speed = atoi(value.c_str());
        if(!g_settings.enable_fan_speed){
            notify("Manual Fan speed threshold is not enabled");
            return oOnPress(Instance, element, e);
        }
        shellui_log("Setting fan speed to %d%%", fan_speed);
        IPC_Client::getInstance(false).Set_Fan_Threshold(fan_speed, g_settings.enable_fan_speed);
    }
    else if (id == "id_rest_2") {
        if (atoi(value.c_str()) == util_rest_kill) {
            shellui_log("util_rest_kill already %s", util_rest_kill ? "Enabled" : "Disabled");
            return oOnPress(Instance, element, e);
        }
        util_rest_kill = !util_rest_kill;
    }
    else if (id == "id_rest_3") {
        if (atoi(value.c_str()) == game_rest_kill) {
            shellui_log("game_rest_kill already %s", game_rest_kill ? "Enabled" : "Disabled");
            return oOnPress(Instance, element, e);
        }
        game_rest_kill = !game_rest_kill; 
    }
    else if (id == "id_rest_4") {
      bool &disable_for_rest_mode = g_settings.disable_toolbox_auto_start_for_rest_mode ;
      if (atoi(value.c_str()) == disable_for_rest_mode) {
          shellui_log("game_rest_kill already %s", disable_for_rest_mode ? "Enabled" : "Disabled");
          return oOnPress(Instance, element, e);
      }
      disable_for_rest_mode = !disable_for_rest_mode; //g_settings.disable_toolbox_auto_start_for_rest_mode 
    }
    else if (id == "id_cheats_shortcut") {
      if (atoi(value.c_str()) == g_settings.cheats_shortcut_opt) {
          shellui_log("Cheats_shortcut already %i", g_settings.cheats_shortcut_opt);
          return oOnPress(Instance, element, e);
      }
      Cheats_Shortcut opt = (Cheats_Shortcut)atoi(value.c_str());
  
      if(opt == CHEATS_SINGLE_SHARE ){
         if(g_settings.toolbox_shortcut_opt == TOOLBOX_SINGLE_SHARE){
              shellui_log("Toolbox and Cheats shortcuts cannot be the same, current selection will NOT be saved");
              notify("Toolbox and Cheats shortcuts cannot be the same, current selection will NOT be saved");
              return oOnPress(Instance, element, e);
          }
      }
      else if(opt == CHEATS_LONG_SHARE ){
         if(g_settings.toolbox_shortcut_opt == TOOLBOX_LONG_SHARE){
              shellui_log("Toolbox and Cheats long shortcuts cannot be the same, current selection will NOT be saved");
              notify("Toolbox and Cheats long shortcuts cannot be the same, current selection will NOT be saved");
              return oOnPress(Instance, element, e);
          }
      }
      g_settings.cheats_shortcut_opt = opt;
  }
  else if (id == "id_toolbox_shortcut" ){
      if (atoi(value.c_str()) == g_settings.toolbox_shortcut_opt) {
          shellui_log("toolbox_shortcut_opt already %i", g_settings.toolbox_shortcut_opt);
          return oOnPress(Instance, element, e);
      }
      Toolbox_Shortcut opt = (Toolbox_Shortcut)atoi(value.c_str());
  
      if(opt == TOOLBOX_SINGLE_SHARE ){
         if(g_settings.cheats_shortcut_opt == CHEATS_SINGLE_SHARE){
              shellui_log("Cheats and Toolbox shortcuts cannot be the same, current selection will NOT be saved");
              notify("Cheats and Toolbox shortcuts cannot be the same, current selection will NOT be saved");
              return oOnPress(Instance, element, e);
          }
      }
      else if(opt == TOOLBOX_LONG_SHARE ){
         if(g_settings.cheats_shortcut_opt == CHEATS_LONG_SHARE){
              shellui_log("Cheats and Toolbox long shortcuts cannot be the same, current selection will NOT be saved");
              notify("Cheats and Toolbox long shortcuts cannot be the same, current selection will NOT be saved");
              return oOnPress(Instance, element, e);
          }
      }
      g_settings.toolbox_shortcut_opt = opt;
  }
    else {
        shellui_log("Not a toolbox item!");
    }

    SaveSettings();
    if(reload_main_settings){
       IPC_Client::getInstance(false).Reload_Daemon_Settings();
    }
    if(reload_util_settings){
       IPC_Client::getInstance(true).Reload_Daemon_Settings();
    }
   // shellui_log("[LM HOOK] OnPress_Hook: Id: %s, Value: %s", id.c_str(), value.c_str());

    return oOnPress(Instance, element, e);

}

