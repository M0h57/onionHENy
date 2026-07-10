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

#include "HookedFuncs.hpp"
#include "RemotePlay.h"
#include "Detour.h"
#include "ipc.hpp"
#include <climits>
#include <msg.hpp>
#include <pthread.h>
#include <sys/_pthreadtypes.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <fstream>
#include <unistd.h>
#include <util.hpp>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
extern "C"{
#include <ps5/kernel.h>
}

extern bool is_6xx, is_3xx;
/* ================================= ORIG HOOKED MONO FUNCS ============================================= */
int (*oOnPress)(MonoObject* Instance, MonoObject* element, MonoObject* e) = nullptr;
int (*oOnPreCreate)(MonoObject* Instance, MonoObject* element) = nullptr;
MonoString* (*CxmlUri)(MonoObject* obj, MonoString* uri) = nullptr;
uint64_t(*GetManifestResourceStream_Original)(uint64_t inst, MonoString* FileName) = nullptr;
uint64_t(*GetManifestResourceInternal_Orig)(MonoObject* instance, MonoString* name, int* size, MonoObject& module) = nullptr;
void (*DebugSettings_GetModel_Orig)(MonoObject* instance, MonoObject* param, MonoObject* promise) = nullptr;
void (*ReactNavigatorManager_UpdateNavigationState_Orig)(MonoObject* instance, MonoObject* state) = nullptr;
void (*UpdateImposeStatusFlag_Orig)(MonoObject* Instance, MonoObject* a) = nullptr;
bool (*CheckRemotePlayRestriction_Orig)(MonoObject* instance) = nullptr;
void (*oTerminate)(void) = nullptr;
GamePadData (*GetData)(int deviceIndex) = nullptr;

bool (*boot_orig)(MonoString* uri, int opt, MonoString* titleIdForBootAction) = nullptr;
void (*OnShareButton_orig)(MonoObject* data) = nullptr;
bool (*boot_orig_2)(MonoString* uri, int opt) = nullptr;

void (*CaptureScreen_orig_old)(MonoObject * inst, int userId, long deviceId, int capType, MonoObject* capacityInfo) = nullptr;
void (*CaptureScreen_orig_new)(MonoObject * inst, int userId, long deviceId, int capType, MonoString* format, MonoObject* capacityInfo) = nullptr;
void(*CallDecrypt_orig)(unsigned char* bundleData, int bundleOffset, int bundleSize, int* payloadOffset, int* realPayloadSize) = nullptr;

void (*createJson)(MonoObject*, MonoObject* array, MonoString* id, MonoString* label, MonoString* actionUrl, MonoString* actionId, MonoString* messageId, MonoObject* subMenu, bool enable) = nullptr;

int (*__sys_regmgr_call)(long, long, int*, int*, long) = nullptr;

MonoString *(*oGetString)(MonoObject *Instance, MonoString *str) = nullptr;
int (*LaunchApp_orig)(MonoString* titleId, uint64_t* args, int argsSize, LaunchAppParam *param) = nullptr;

// Store original function pointer
DecryptRnpsBundle_t DecryptRnpsBundle = NULL;



/* ================================= HOOKED GLOBAL VARS ============================================= */
MonoClass* MemoryStream_IO = nullptr;

std::atomic_bool cheat_action_in_progress(false);
std::atomic_bool download_kstuff_thread_in_progress(false);

static std::string current_menu_tid;
int usbpath();
#define MAX_CHEATS 256

bool is_plugin = false;
bool is_su_menu = false;
bool is_custom_pkg = false;
bool is_debug_settings = false;
bool is_cheats = false;
bool is_auto_plugin = false;
bool is_remote_play = false;
bool is_plapps = false;
bool cheats_shortcut_activated = false;
bool cheats_shortcut_activated_not_open = false;

extern int cheatEnabledMap[MAX_CHEATS]; // holds the current activated/deactivated cheats, used for onPreCreateHook
std::string currentCheatTID; // holds current title ID being cheated, this is used to reset the map above

void RemoveGameWidget(RemoveWidget widget) {

    // Helper lambda to remove widgets by name
    auto removeWidgets = [](const std::vector<const char*>& widgetNames) {
        MonoClass* widgetClass = mono_class_from_name(pui_img, "Sce.PlayStation.PUI.UI2", "Widget");
        MonoObject* rootWidget = Get_Property<MonoObject*>(pui_img, "Sce.PlayStation.PUI.UI2", "Scene", Game, "RootWidget");
        for (const char* name : widgetNames) {
            MonoObject* child = Invoke<MonoObject*>(pui_img, widgetClass, rootWidget, "FindWidgetByName", mono_string_new(Root_Domain, name));
            if (child) {
                Invoke<void>(pui_img, widgetClass, child, "RemoveFromParent");
            }
        }
    };

    switch (widget) {
    case REMOVE_GPU_OVERLAY:
        removeWidgets({ "id_gpu_temp_value", "id_gpu_usage_value", "id_gpu_label" });
        break;
    case REMOVE_CPU_OVERLAY:
        removeWidgets({ "id_cpu_label", "id_cpu_temp_value", "id_cpu_usage_value" });
        break;
    case REMOVE_RAM_OVERLAY:
        removeWidgets({ "id_ram_label", "id_ram_value" });
        break;
    case REMOVE_FPS_OVERLAY:
        removeWidgets({ "id_fps_label", "id_fps_value" });
        break;
    case REMOVE_IP_OVERLAY:
		removeWidgets({ "id_ip_label", "id_ip_value" });
		break;
    case REMOVE_ALL_OVERLAYS:
        removeWidgets({ "id_gpu_temp_value", "id_gpu_usage_value", "id_gpu_label",
                        "id_cpu_label", "id_cpu_temp_value", "id_cpu_usage_value",
                        "id_ram_label", "id_ram_value",
                        "id_fps_label", "id_fps_value", 
                        "id_ip_label", "id_ip_value" });
		break;
    }
}

void CreateGameWidget(CreateWidget widget) {
    MonoObject* font = CreateUIFont(22, 0, 0);
    MonoObject* rootWidget = Get_Property<MonoObject*>(pui_img, "Sce.PlayStation.PUI.UI2", "Scene", Game, "RootWidget");

    std::vector<WidgetConfig> configs;

    switch (widget) {
    case CREATE_GPU_OVERLAY:
        configs = {
            {"id_gpu_label", global_conf.overlay_gpu_x, global_conf.overlay_gpu_y, "GPU", 1, 0.0f, 1.0f, 0.0f, 1.0f},        // Green + Bold
            {"id_gpu_temp_value", global_conf.overlay_gpu_x + 70.0f, global_conf.overlay_gpu_y, "--C", 0, 1.0f, 0.6f, 0.0f, 1.0f},   // Orange
            {"id_gpu_usage_value", global_conf.overlay_gpu_x + 115.0f, global_conf.overlay_gpu_y, "--%", 0, 1.0f, 0.6f, 0.0f, 1.0f}  // Orange
        };
        break;

    case CREATE_CPU_OVERLAY:
        configs = {
            {"id_cpu_label", global_conf.overlay_cpu_x, global_conf.overlay_cpu_y, "CPU", 1, 0.0f, 1.0f, 1.0f, 1.0f},        // Cyan + Bold
            {"id_cpu_temp_value", global_conf.overlay_cpu_x + 70.0f, global_conf.overlay_cpu_y, "--C", 0, 1.0f, 0.6f, 0.0f, 1.0f},   // Orange
            {"id_cpu_usage_value", global_conf.overlay_cpu_x + 115.0f, global_conf.overlay_cpu_y, "--%", 0, 1.0f, 0.6f, 0.0f, 1.0f}  // Orange
        };
        break;

    case CREATE_RAM_OVERLAY:
        configs = {
            {"id_ram_label", global_conf.overlay_ram_x, global_conf.overlay_ram_y, "RAM", 1, 0.0f, 1.0f, 1.0f, 1.0f},        // Cyan + Bold
            {"id_ram_value", global_conf.overlay_ram_x + 70.0f, global_conf.overlay_ram_y, "----- MB", 0, 1.0f, 0.6f, 0.0f, 1.0f}    // Orange
        };
        break;

    case CREATE_FPS_OVERLAY:
        configs = {
            {"id_fps_label", global_conf.overlay_fps_x, global_conf.overlay_fps_y, "FPS:", 1, 1.0f, 0.0f, 1.0f, 1.0f},       // Magenta + Bold
            {"id_fps_value", global_conf.overlay_fps_x + 70.0f, global_conf.overlay_fps_y, "--- FPS", 0, 1.0f, 1.0f, 1.0f, 1.0f}     // White
        };
        break;
    case CREATE_IP_OVERLAY:
		configs = {
           { "id_ip_label", global_conf.overlay_ip_x, global_conf.overlay_ip_y, "PS5 IP:", 1, 0.0f, 1.0f, 0.0f, 1.0f},       // Green + Bold
		   { "id_ip_value", global_conf.overlay_ip_x + 70.0f, global_conf.overlay_ip_y, "---.---.---.---", 0, 1.0f, 1.0f, 1.0f, 1.0f }     // White
	     };
	     break;
    case CREATE_ALL_OVERLAYS:
        configs = {
            // GPU Overlay
            {"id_gpu_label", global_conf.overlay_gpu_x, global_conf.overlay_gpu_y, "GPU", 1, 0.0f, 1.0f, 0.0f, 1.0f},        // Green + Bold
            {"id_gpu_temp_value", global_conf.overlay_gpu_x + 70.0f, global_conf.overlay_gpu_y, "--C", 0, 1.0f, 0.6f, 0.0f, 1.0f},   // Orange
            {"id_gpu_usage_value", global_conf.overlay_gpu_x + 115.0f, global_conf.overlay_gpu_y, "--%", 0, 1.0f, 0.6f, 0.0f, 1.0f},  // Orange
            // CPU Overlay
            {"id_cpu_label", global_conf.overlay_cpu_x, global_conf.overlay_cpu_y, "CPU", 1, 0.0f, 1.0f, 1.0f, 1.0f},        // Cyan + Bold
            {"id_cpu_temp_value", global_conf.overlay_cpu_x + 70.0f, global_conf.overlay_cpu_y, "--C", 0, 1.0f, 0.6f, 0.0f, 1.0f},   // Orange
            {"id_cpu_usage_value", global_conf.overlay_cpu_x + 115.0f, global_conf.overlay_cpu_y, "--%", 0, 1.0f, 0.6f, 0.0f, 1.0f},  // Orange
            // RAM Overlay
            {"id_ram_label", global_conf.overlay_ram_x, global_conf.overlay_ram_y, "RAM", 1, 0.0f, 1.0f, 1.0f, 1.0f},        // Cyan + Bold
            {"id_ram_value", global_conf.overlay_ram_x + 70.0f, global_conf.overlay_ram_y, "----- MB", 0, 1.0f, 0.6f, 0.0f, 1.0f},    // Orange
            // FPS Overlay
			{"id_fps_label", global_conf.overlay_fps_x, global_conf.overlay_fps_y, "FPS:", 1, 1.0f, 0.0f, 1.0f, 1.0f},       // Magenta + Bold
            {"id_fps_value", global_conf.overlay_fps_x + 70.0f, global_conf.overlay_fps_y, "--- FPS", 0, 1.0f, 1.0f, 1.0f, 1.0f},     // White

            { "id_ip_label", global_conf.overlay_ip_x, global_conf.overlay_ip_y, "IP:", 1, 0.0f, 1.0f, 0.0f, 1.0f },       // Green + Bold
            { "id_ip_value", global_conf.overlay_ip_x + 70.0f, global_conf.overlay_ip_y, "---.---.---.---", 0, 1.0f, 1.0f, 1.0f, 1.0f }     // White
		};
        break;
}



    // Create and append all widgets
    for (const auto& config : configs) {
        MonoObject* label = CreateLabel(config.id, config.x, config.y, config.text, font,
            config.bold, 0, config.r, config.g, config.b, config.a);
        Widget_Append_Child(rootWidget, label);
    }
}

extern "C"{
int sceShellCoreUtilIsUsbMassStorageMounted(int num);
int sceNetCtlGetInfo(int number,  SceNetCtlInfo *info);
int sceNetSend(int sockfd, const void *buf, size_t len, int flags);
}

MonoString *GetString_Hook(MonoObject *Instance, MonoString *str) {
    if (!str || !Instance) {
      shellui_log("GetString_Hook: Invalid Parameters");
      return nullptr;
    }
    std::string resourceName = Mono_to_String(str);
    shellui_log("Resource Name: %s", resourceName.c_str());
    if (resourceName == "msg_options") {
      return mono_string_new(Root_Domain, "PKG 安装器选项");
    } else if (resourceName == "msg_installing") {
      return mono_string_new(Root_Domain,
                             "OrionHEN 正在安装所选 PKG");
    } else if (resourceName == "msg_yes") {
      return mono_string_new(Root_Domain, "是");
    } else if (resourceName == "msg_no") {
      return mono_string_new(Root_Domain, "否");
    } else if (resourceName == "msg_sort") {
      return mono_string_new(Root_Domain, "OrionHEN PKG 排序");
    } else if (resourceName == "msg_sort_name_az") {
      return mono_string_new(Root_Domain, "名称（A-Z）");
    } else if (resourceName == "msg_sort_name_za") {
      return mono_string_new(Root_Domain, "名称（Z-A）");
    } else if (resourceName == "msg_updated") {
      return mono_string_new(Root_Domain, "已更新");
    } else if (resourceName == "msg_wait") {
      return mono_string_new(Root_Domain, "请稍候...");
    }
    else if (resourceName == "msg_ok"){
      return mono_string_new(Root_Domain, "确定");
    }
    else if (resourceName == "msg_cancel_vb"){
        return mono_string_new(Root_Domain, "取消");
    }
    //else if (resourceName == "msg_deselect_all") {
   //   return mono_string_new(Root_Domain, "取消全选"); // IDK WHY BUT ONLY 1 CAN BE ACTIVE OR SHELLUI CRASHES
  //  }
    else if (resourceName == "msg_select_all") {
      return mono_string_new(Root_Domain, "全选");
    }
    
    return oGetString(Instance, str);
  }
  

bool if_exists(const char* path) {
	struct stat buffer;
	return (stat(path, &buffer) == 0);
}

int get_ip_address(char* ip_address)
{
    unsigned int ret = 0;
    SceNetCtlInfo info;

    ret = sceNetCtlGetInfo(14, &info);
    if (ret < 0) {
        goto error;
    }

    memcpy(ip_address, info.ip_address, sizeof(info.ip_address));

    return ret;

error:
    memcpy(ip_address, "IP NOT FOUND", sizeof(info.ip_address));
    return -1;
}

void patch_bundle_strings(unsigned char* buffer, int* size_ptr, int actual_size) {
  if (!buffer || !size_ptr) {
      return;
  }
  
  // Replace "Debug Settings" with "OrionHEN 工具箱"
  // Note: replacement must not exceed original length when patching in-place buffers.
  // "OrionHEN Toolbox" is same length as original English branding used previously.
  int count = replace_all(buffer, size_ptr, actual_size, "Debug Settings", "OrionHEN 工具箱");
#if SHELL_DEBUG == 1
  if (count > 0) {
      shellui_log("patch_bundle_strings: Replaced %d occurrences of 'Debug Settings' with 'OrionHEN 工具箱'", count);
  } else {
      shellui_log("patch_bundle_strings: No occurrences of 'Debug Settings' found");
  }
#else
  (void)count;
#endif
  
  // Replace "icon_setting" with "orionhen_sicon"
  replace_all(buffer, size_ptr, actual_size, "icon_setting", "orionhen_sicon");
}

int ioctl_hook(int fd, unsigned long request, void *argp) {
  const int IOCTL_SYSCALL = 0x36;
  const unsigned long  DECRYPT_RNPS_BUNDLE = 0xC0105203; // RNPS request code for ioctl

  int ret = __syscall(IOCTL_SYSCALL, fd, request, argp);
  if (ret == 0 && request == DECRYPT_RNPS_BUNDLE) {
      ioctl_C0105203_args *args = (ioctl_C0105203_args *)argp;
#if SHELL_DEBUG == 1
      shellui_log("ioctl_hook called with fd: %d, request: 0x%X, argp: %p", fd, request, argp);
#endif
      patch_bundle_strings((unsigned char*)args->buffer, &args->size, args->size);
  }
  return ret;
}

void CallDecrypt(unsigned char* bundleData, int bundleOffset, int bundleSize, int* payloadOffset, int* realPayloadSize) {
#if SHELL_DEBUG == 1
  shellui_log("CallDecrypt: bundleData: %p, bundleOffset: %d, bundleSize: %d, payloadOffset: %p, realPayloadSize: %p", 
      bundleData, bundleOffset, bundleSize, payloadOffset, realPayloadSize);
#endif
  
  if (!bundleData || !payloadOffset || !realPayloadSize) {
#if SHELL_DEBUG == 1
      shellui_log("CallDecrypt: Invalid Parameters");
#endif
      return;
  }
  
  CallDecrypt_orig(bundleData, bundleOffset, bundleSize, payloadOffset, realPayloadSize);
  patch_bundle_strings(bundleData, realPayloadSize, *realPayloadSize);
}




void ParseCheatID(const char* id, char* tid, int* cheat_id)
{
    sscanf(id, "id_cheat_%[^_]_%d", tid, cheat_id);
}

//
// Scene has changed, stop Remote Play thread if is running
//
void UpdateImposeStatusFlag_hook(MonoObject* scene, MonoObject* frontActiveScene)
{
    if(!frontActiveScene || !scene) {
        shellui_log("Scene or frontActiveScene is null, returning...");
        return;
    }
    if (!is_remote_play && IsRunningConfirmRegistLoop)
    {
        StopConfirmRegistLoop();
    }

    if (is_remote_play)
    {
        //
        // If the scene is switching, means that we exiting from the current state, a state machine would be good here
        // otherwise we would need to reverse the SceneBase     
        //
        is_remote_play = false; 
    }

    UpdateImposeStatusFlag_Orig(scene, frontActiveScene);
}

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
extern std::string remote_play_info;
void* load_ps5debug_thr(void*){ return nullptr; }
void* download_cheats_thr(void*){
    if(cheat_action_in_progress){
        notify("Cheat action already in progress, please wait for it to complete...");
        pthread_exit(nullptr);
        return nullptr;
    }
    cheat_action_in_progress = true;
    notify("Preparing to download the %s cheats repo...", global_conf.selected_cheats_repo == CHEATS_REPO_ORIONHEN ? "OrionHEN PS5" : "GoldHEN PS4");
    IPC_Client& util_ipc = IPC_Client::getInstance(true);
    // daemon shows notification when done
    util_ipc.Cheats_Action(DOWNLOAD_CHEATS, global_conf.selected_cheats_repo);
    
    cheat_action_in_progress = false;
    pthread_exit(nullptr);
    return nullptr;
}

void* reload_cheats_thr(void*){
    if(cheat_action_in_progress){
        notify("Cheat action already in progress, please wait for it to complete...");
        pthread_exit(nullptr);
        return nullptr;
    }
    cheat_action_in_progress = true;
    IPC_Client& util_ipc = IPC_Client::getInstance(true);
    if (util_ipc.Cheats_Action(RELOAD_CHEATS, 0)) 
       notify("The Cheats have been Cache and cheats list has been successfully reloaded");

    cheat_action_in_progress = false;
    pthread_exit(nullptr);
    return nullptr;
}

void* kstuff_download_thread(void* args) {
    if (download_kstuff_thread_in_progress) {
        notify("Download action already in progress, please wait for it to complete...");
        pthread_exit(nullptr);
        return nullptr;
    }
    download_kstuff_thread_in_progress = true;
    IPC_Client& util_ipc = IPC_Client::getInstance(true);
    shellui_log("Ret: 0x%X", util_ipc.DownloadKstuff());
    download_kstuff_thread_in_progress = false;
    pthread_exit(nullptr);
    return nullptr;
}

int OnPress_Hook(MonoObject* Instance, MonoObject* element, MonoObject* e)
{
    bool& DPI = global_conf.DPI;
    bool& Data_SB = global_conf.allow_data_sandbox;
    int& StartOption = global_conf.start_option;
    bool& util_rest_kill = global_conf.util_rest_kill;
    bool& game_rest_kill = global_conf.game_rest_kill;
    uint64_t& delay_secs = global_conf.rest_delay_seconds;
    bool& DPI_v2 = global_conf.DPI_v2;
    bool& dis_tids = global_conf.display_tids;
    cheats_repo_source& selected_cheats_repo = global_conf.selected_cheats_repo;

    // Define the array of IDs to exclude (you can put this at the top of your function or as a static/global)
    const std::vector<std::string> excludedIds = {
        "id_dl_cheats",
        "id_reload_cheats",
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
		if (atoi(value.c_str()) == global_conf.overlay_gpu) {
			return oOnPress(Instance, element, e);
		}
        if (!atoi(value.c_str())) {
            RemoveGameWidget(REMOVE_GPU_OVERLAY);
        }
        else {
			CreateGameWidget(CREATE_GPU_OVERLAY);
        }

        global_conf.overlay_gpu = !global_conf.overlay_gpu;
    }
    else if (id == "id_overlay_cpu") {
		if (atoi(value.c_str()) == global_conf.overlay_cpu) {
			return oOnPress(Instance, element, e);
		}
        if (!atoi(value.c_str())) {
            if (!global_conf.all_cpu_usage) {
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

        global_conf.overlay_cpu = !global_conf.overlay_cpu;
    }
    else if (id == "id_overlay_ram") {
		if (atoi(value.c_str()) == global_conf.overlay_ram) {
			return oOnPress(Instance, element, e);
		}
        if (!atoi(value.c_str())) {
			RemoveGameWidget(REMOVE_RAM_OVERLAY);
        }
        else {
			CreateGameWidget(CREATE_RAM_OVERLAY);   
        }

        global_conf.overlay_ram = !global_conf.overlay_ram;
    }
    else if (id == "id_overlay_fps") {
		if (atoi(value.c_str()) == global_conf.overlay_fps) {
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

        global_conf.overlay_fps = !global_conf.overlay_fps;
    }
    else if (id == "id_overlay_ip") {
		if (atoi(value.c_str()) == global_conf.overlay_ip) {
			return oOnPress(Instance, element, e);
		}
        if (!atoi(value.c_str())) {
            RemoveGameWidget(REMOVE_IP_OVERLAY);
        }
        else {
            CreateGameWidget(CREATE_IP_OVERLAY);
        }

        global_conf.overlay_ip = !global_conf.overlay_ip;
	}
    else if (id == "id_all_cpu_usage") {
        if (global_conf.all_cpu_usage == atoi(value.c_str())) {
            return oOnPress(Instance, element, e);
		}
        if(!global_conf.overlay_cpu){
            notify("To change CPU overlay mode, please enable the CPU overlay first");
            return oOnPress(Instance, element, e);
		}
        global_conf.all_cpu_usage = !global_conf.all_cpu_usage;
    }
    else if (id == "id_overlay_change_pos") {

        if((overlay_positions)atoi(value.c_str()) == global_conf.overlay_pos){
            return oOnPress(Instance, element, e);
		}

        global_conf.overlay_pos = (overlay_positions)atoi(value.c_str());

        if (global_conf.overlay_pos == OVERLAY_POS_TOP_LEFT) {
            global_conf.overlay_fps_x = 10.0f;
            global_conf.overlay_fps_y = 10.0f;

            global_conf.overlay_gpu_x = 10.0f;
            global_conf.overlay_gpu_y = 35.0f;

            global_conf.overlay_cpu_x = 10.0f;
            global_conf.overlay_cpu_y = 60.0f;

            global_conf.overlay_ram_x = 10.0f;
            global_conf.overlay_ram_y = 85.0f;

            global_conf.overlay_ip_x = 10.0f;
            global_conf.overlay_ip_y = 110.0f;
        }
        else if (global_conf.overlay_pos == OVERLAY_POS_BOTTOM_LEFT) {
            global_conf.overlay_ram_x = 10.0f;
            global_conf.overlay_ram_y = 970.0f;
            global_conf.overlay_cpu_x = 10.0f;
            global_conf.overlay_cpu_y = 990.0f;
            global_conf.overlay_gpu_x = 10.0f;
            global_conf.overlay_gpu_y = 1010.0f;
            global_conf.overlay_fps_x = 10.0f;
            global_conf.overlay_fps_y = 1030.0f;
            global_conf.overlay_ip_x = 10.0f;
            global_conf.overlay_ip_y = 1050.0f;
        }
        else if (global_conf.overlay_pos == OVERLAY_POS_TOP_RIGHT) {
            global_conf.overlay_fps_x = 1720.0f;
            global_conf.overlay_fps_y = 10.0f;
            global_conf.overlay_gpu_x = 1720.0f;
            global_conf.overlay_gpu_y = 35.0f;
            global_conf.overlay_cpu_x = 1720.0f;
            global_conf.overlay_cpu_y = 60.0f;
            global_conf.overlay_ram_x = 1720.0f;
            global_conf.overlay_ram_y = 85.0f;
            global_conf.overlay_ip_x = 1670.0f;;
            global_conf.overlay_ip_y = 110.0f;
        }
        else if (global_conf.overlay_pos == OVERLAY_POS_BOTTOM_RIGHT) {
            global_conf.overlay_ram_x = 1720.0f;
            global_conf.overlay_ram_y = 970.0f;
            global_conf.overlay_cpu_x = 1720.0f;
            global_conf.overlay_cpu_y = 990.0f;
            global_conf.overlay_gpu_x = 1720.0f;
            global_conf.overlay_gpu_y = 1010.0f;
            global_conf.overlay_fps_x = 1720.0f;
            global_conf.overlay_fps_y = 1030.0f;
            global_conf.overlay_ip_x = 1670.0f;
            global_conf.overlay_ip_y = 1050.0f;
        }
       
        if (global_conf.overlay_cpu) {
            RemoveGameWidget(REMOVE_CPU_OVERLAY);
            CreateGameWidget(CREATE_CPU_OVERLAY);
		}
        if (global_conf.overlay_ram) {
            RemoveGameWidget(REMOVE_RAM_OVERLAY);
			CreateGameWidget(CREATE_RAM_OVERLAY);
        }
		if (global_conf.overlay_gpu) {
			RemoveGameWidget(REMOVE_GPU_OVERLAY);
			CreateGameWidget(CREATE_GPU_OVERLAY);
        }
        if (global_conf.overlay_fps) {
            RemoveGameWidget(REMOVE_FPS_OVERLAY);
            CreateGameWidget(CREATE_FPS_OVERLAY);
        }
        if (global_conf.overlay_ip) {
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
        global_conf.auto_eject_disc = atol(value.c_str());
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
        if (atol(value.c_str()) == global_conf.enable_fan_speed) {
            shellui_log("Fan speed control already %s", global_conf.enable_fan_speed ? "Enabled" : "Disabled");
            return oOnPress(Instance, element, e);
        }
        global_conf.enable_fan_speed = !global_conf.enable_fan_speed;
        IPC_Client::getInstance(false).Set_Fan_Threshold(global_conf.fan_threshold, global_conf.enable_fan_speed);

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
    else if (id == "id_reload_cheats") {
        pthread_t thr;
        pthread_create(&thr, nullptr, reload_cheats_thr, nullptr);
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
        if (atoi(value.c_str()) == global_conf.debug_app_jb_msg) {
            shellui_log("Debug JB already %s", global_conf.debug_app_jb_msg ? "Enabled" : "Disabled");
            return oOnPress(Instance, element, e);
        }
        global_conf.debug_app_jb_msg = !global_conf.debug_app_jb_msg;
        reload_main_settings = true;
    }
    else if (id == "id_debug_legacy_cmd") {
        if (atoi(value.c_str()) == global_conf.debug_legacy_cmd_server) {
            shellui_log("Debug cmd already %s", global_conf.debug_legacy_cmd_server ? "Enabled" : "Disabled");
            return oOnPress(Instance, element, e);
        }
        global_conf.debug_legacy_cmd_server = !global_conf.debug_legacy_cmd_server;

        if (IPC_Client::getInstance(true).ToggleSetting(BREW_UTIL_TOGGLE_LEGACY_CMD_SERVER, global_conf.debug_legacy_cmd_server) != IPC_Ret::NO_ERROR) {
            notify(global_conf.debug_legacy_cmd_server ? "cmd Failed to Start ..." : "CMD Server Failed to Stop ...");
            global_conf.debug_legacy_cmd_server = !global_conf.debug_legacy_cmd_server;
        }//
    }
    else if (id == "id_custom_game_opts") {
        if (atoi(value.c_str()) == global_conf.OrionHEN_game_opts) {
            shellui_log("OrionHEN Game Options already %s", global_conf.OrionHEN_game_opts ? "Enabled" : "Disabled");
            return oOnPress(Instance, element, e);
        }
        global_conf.OrionHEN_game_opts = !global_conf.OrionHEN_game_opts;
        shellui_log("OrionHEN Game Options: %s", global_conf.OrionHEN_game_opts ? "Enabled" : "Disabled");
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
        if (atoi(value.c_str()) == global_conf.toolbox_auto_start) {
            shellui_log("toolbox Access already %s", global_conf.toolbox_auto_start ? "Enabled" : "Disabled");
            return oOnPress(Instance, element, e);
        }
        global_conf.toolbox_auto_start = !global_conf.toolbox_auto_start;

    }
    else if (id == "id_sistro_ps5debug") {
        notify("PS5Debug is not bundled in OrionHEN");
    }
    else if (id == "id_rest_1") {
        delay_secs = atol(value.c_str());
    }
    else if (id == "id_fan_speed") {
        int &fan_speed = global_conf.fan_threshold;
        fan_speed = atoi(value.c_str());
        if(!global_conf.enable_fan_speed){
            notify("Manual Fan speed threshold is not enabled");
            return oOnPress(Instance, element, e);
        }
        shellui_log("Setting fan speed to %d%%", fan_speed);
        IPC_Client::getInstance(false).Set_Fan_Threshold(fan_speed, global_conf.enable_fan_speed);
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
      bool &disable_for_rest_mode = global_conf.disable_toolbox_auto_start_for_rest_mode ;
      if (atoi(value.c_str()) == disable_for_rest_mode) {
          shellui_log("game_rest_kill already %s", disable_for_rest_mode ? "Enabled" : "Disabled");
          return oOnPress(Instance, element, e);
      }
      disable_for_rest_mode = !disable_for_rest_mode; //global_conf.disable_toolbox_auto_start_for_rest_mode 
    }
    else if (id == "id_cheats_shortcut") {
      if (atoi(value.c_str()) == global_conf.cheats_shortcut_opt) {
          shellui_log("Cheats_shortcut already %i", global_conf.cheats_shortcut_opt);
          return oOnPress(Instance, element, e);
      }
      Cheats_Shortcut opt = (Cheats_Shortcut)atoi(value.c_str());
  
      if(opt == CHEATS_SINGLE_SHARE ){
         if(global_conf.toolbox_shortcut_opt == TOOLBOX_SINGLE_SHARE){
              shellui_log("Toolbox and Cheats shortcuts cannot be the same, current selection will NOT be saved");
              notify("Toolbox and Cheats shortcuts cannot be the same, current selection will NOT be saved");
              return oOnPress(Instance, element, e);
          }
      }
      else if(opt == CHEATS_LONG_SHARE ){
         if(global_conf.toolbox_shortcut_opt == TOOLBOX_LONG_SHARE){
              shellui_log("Toolbox and Cheats long shortcuts cannot be the same, current selection will NOT be saved");
              notify("Toolbox and Cheats long shortcuts cannot be the same, current selection will NOT be saved");
              return oOnPress(Instance, element, e);
          }
      }
      global_conf.cheats_shortcut_opt = opt;
  }
  else if (id == "id_toolbox_shortcut" ){
      if (atoi(value.c_str()) == global_conf.toolbox_shortcut_opt) {
          shellui_log("toolbox_shortcut_opt already %i", global_conf.toolbox_shortcut_opt);
          return oOnPress(Instance, element, e);
      }
      Toolbox_Shortcut opt = (Toolbox_Shortcut)atoi(value.c_str());
  
      if(opt == TOOLBOX_SINGLE_SHARE ){
         if(global_conf.cheats_shortcut_opt == CHEATS_SINGLE_SHARE){
              shellui_log("Cheats and Toolbox shortcuts cannot be the same, current selection will NOT be saved");
              notify("Cheats and Toolbox shortcuts cannot be the same, current selection will NOT be saved");
              return oOnPress(Instance, element, e);
          }
      }
      else if(opt == TOOLBOX_LONG_SHARE ){
         if(global_conf.cheats_shortcut_opt == CHEATS_LONG_SHARE){
              shellui_log("Cheats and Toolbox long shortcuts cannot be the same, current selection will NOT be saved");
              notify("Cheats and Toolbox long shortcuts cannot be the same, current selection will NOT be saved");
              return oOnPress(Instance, element, e);
          }
      }
      global_conf.toolbox_shortcut_opt = opt;
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

extern std::string running_tid;
MonoString * CxmlUri_Hook(MonoObject * Instance, MonoString * uri) {

  if (!Instance || !uri) {
    #if SHELL_DEBUG==1 
    shellui_log("CxmlUri_Hook: args are null");
    #endif
    return CxmlUri(Instance, uri);
  }
  std::string uri_string = Mono_to_String(uri);
  #if SHELL_DEBUG==1 
  shellui_log("uri_string: %s", uri_string.c_str());
  #endif
  ///shellui_log("CxmlUri_Hook: %s", uri_string.c_str());
  if (uri_string.rfind("tex_game_icon") != std::string::npos) {
    //shellui_log("CxmlUri_Hook: Returning store icon");
    std::string icon = "/user/appmeta/" + running_tid + "/icon0.png";
    if(!if_exists(icon.c_str())){
        icon = "/user/appmeta/external/" + running_tid + "/icon0.png";

        if(!if_exists(icon.c_str())){ // pirated PS5 Games
           std::string game_src = "/system_ex/app/" + running_tid + "/sce_sys/icon0.png"; // shellui cant access this path
           icon = "/user/appmeta/" + running_tid;
           mkdir(icon.c_str(), 0777);
           icon = "/user/appmeta/" + running_tid + "/icon0.png";
           IPC_Client::getInstance(false).CopyFile(game_src, icon);
        }
    }
   // shellui_log("CxmlUri_Hook: %s", icon.c_str());
    return mono_string_new(Root_Domain, icon.c_str());
  }
  else if (uri_string.rfind("//usb") != std::string::npos || uri_string.rfind("//data") != std::string::npos || uri_string.rfind("//user//data") != std::string::npos){
    //replace // with//
    std::string new_uri = uri_string;
    size_t pos = 0;
    while (( pos = new_uri.find("//", pos)) != std::string::npos) {
        new_uri.replace(pos, 2, "/");
    }
    #if SHELL_DEBUG==1 
    shellui_log("CxmlUri_Hook: %s", new_uri.c_str());
    #endif
    return mono_string_new(Root_Domain, new_uri.c_str());
  }
  return CxmlUri(Instance, uri);
}
MonoObject* MemoryStream_Instance = nullptr;
static bool debug_settings_nav_redirecting = false;

static std::string MonoObjectToString(MonoObject* obj) {
    if (!obj || !mono_object_get_class) {
        return "";
    }

    MonoClass* klass = mono_object_get_class(obj);
    if (!klass) {
        return "";
    }

    MonoString* text = Invoke<MonoString*>(nullptr, klass, obj, "ToString");
    if (!text) {
        return "";
    }

    return Mono_to_String(text);
}

void ReactNavigatorManager_UpdateNavigationState_Hook(MonoObject* instance, MonoObject* state) {
    std::string state_text = MonoObjectToString(state);

    if (state_text.find("DebugSettingsOldScreen") != std::string::npos ||
        state_text.find("ps5:settings:debug settings old") != std::string::npos) {
        debug_settings_nav_redirecting = false;
    }

    if (state_text.find("DebugSettingsScreen") != std::string::npos &&
        state_text.find("DebugSettingsOldScreen") == std::string::npos &&
        state_text.find("ps5:settings:debug settings old") == std::string::npos) {
        if (!debug_settings_nav_redirecting) {
            shellui_log("[DBG-NAV] DebugSettingsScreen route blocked before RN scene load; opening debug_settings_old");
            debug_settings_nav_redirecting = true;
            GoToURI("pssettings:play?function=debug_settings_old");
        } else {
            shellui_log("[DBG-NAV] DebugSettingsScreen route blocked before RN scene load; redirect already pending");
        }
        return;
    }

    if (ReactNavigatorManager_UpdateNavigationState_Orig) {
        ReactNavigatorManager_UpdateNavigationState_Orig(instance, state);
    }
}

void DebugSettings_GetModel_Hook(MonoObject* instance, MonoObject* param, MonoObject* promise) {
    std::string param_text;
    std::string page_id;

    if (param && mono_object_get_class) {
        MonoClass* param_class = mono_object_get_class(param);
        if (param_class) {
            MonoObject* page_token = Invoke<MonoObject*>(nullptr,
                                                        param_class,
                                                        param,
                                                        "GetValue",
                                                        mono_string_new(Root_Domain, "pageId"));
            if (page_token) {
                MonoClass* token_class = mono_object_get_class(page_token);
                if (token_class) {
                    MonoString* page_string = Invoke<MonoString*>(nullptr, token_class, page_token, "ToString");
                    if (page_string) {
                        page_id = Mono_to_String(page_string);
                    }
                }
            }

            MonoString* param_string = Invoke<MonoString*>(nullptr, param_class, param, "ToString");
            if (param_string) {
                param_text = Mono_to_String(param_string);
            }
        }
    }

    if (!page_id.empty()) {
        shellui_log("[DBG-GETMODEL] pageId=%s", page_id.c_str());
    } else {
        shellui_log("[DBG-GETMODEL] pageId=<empty>");
    }

    if (!param_text.empty()) {
        shellui_log("[DBG-GETMODEL] param=%s", param_text.c_str());
    } else {
        shellui_log("[DBG-GETMODEL] param=<empty>");
    }

    if (page_id == "id_debug_settings" || param_text.find("id_debug_settings") != std::string::npos) {
        shellui_log("[DBG-GETMODEL] id_debug_settings reached RN model; navigation-state redirect did not catch this path");
    }

    if (DebugSettings_GetModel_Orig) {
        DebugSettings_GetModel_Orig(instance, param, promise);
    }
}

uint64_t GetManifestResourceStream_Hook(uint64_t inst, MonoString* FileName) {
    
    std::string new_xml_string;
    std::string resourceName = Mono_to_String(FileName);

#if SHELL_DEBUG==1 
    shellui_log("GetManifestResourceStream_Hook: %s", resourceName.c_str());
#endif

    is_plugin = (resourceName == plugin_xml);
    is_debug_settings = (resourceName == debug_settings_xml);
    is_cheats = (resourceName == cheats_xml);
    is_auto_plugin = (resourceName == "Sce.Vsh.ShellUI.Legacy.src.Sce.Vsh.ShellUI.Settings.Plugins.auto_plugins.xml");
    is_plapps = (resourceName == "Sce.Vsh.ShellUI.Legacy.src.Sce.Vsh.ShellUI.Settings.Plugins.plapps.xml");
	is_custom_pkg = (resourceName == "Sce.Vsh.ShellUI.Legacy.src.Sce.Vsh.ShellUI.Settings.Plugins.custompkginstaller.xml");
	is_su_menu = (resourceName == "Sce.Vsh.ShellUI.Legacy.src.Sce.Vsh.ShellUI.Settings.Plugins.superuser.xml");
    
    is_remote_play = (resourceName == remote_play_xml);


    if(cheats_shortcut_activated || cheats_shortcut_activated_not_open){
        is_debug_settings = false;
        is_cheats = true;
    }

    // TEstKIt OG Debug Settings
    if((resourceName == "Sce.Vsh.ShellUI.Legacy.src.Sce.Vsh.ShellUI.Settings.Plugins.og_debug.xml")){
       // shellui_log("Sce.Vsh.ShellUI.Legacy.src.Sce.Vsh.ShellUI.Settings.Plugins.og_debug.xml 111111111");
        return GetManifestResourceStream_Original(inst, mono_string_new(Root_Domain, debug_settings_xml.c_str()));
    }

    if (!is_plugin && !is_debug_settings && !is_cheats && !is_auto_plugin && !is_remote_play && !is_plapps && !is_su_menu && !is_custom_pkg) {
        return GetManifestResourceStream_Original(inst, FileName);
    }


    // Don't try to open the class again if it's already open
    if (!MemoryStream_IO) {
        MonoAssembly* Assembly = mono_domain_assembly_open(Root_Domain, "/system_ex/common_ex/lib/mscorlib.dll");
        MonoImage* mscorelib_image = mono_assembly_get_image(Assembly);
        if (!mscorelib_image) {
            shellui_log("Failed to get mscorelib image");
            return GetManifestResourceStream_Original(inst, FileName);
        }

        MemoryStream_IO = mono_class_from_name(mscorelib_image, "System.IO", "MemoryStream");
        if (!MemoryStream_IO) {
            shellui_log("Failed to open class MemoryStream");
            return GetManifestResourceStream_Original(inst, FileName);
        }
    }

    if (is_debug_settings) {
        LoadSettings();
        new_xml_string = dec_xml_str;
    }
    else if (is_plugin) {
       // shellui_log("Plugins clicked");
        if (!plugins_list.empty()) {
            plugins_list.clear();
            //shellui_log("Plugins found");
        }
        generate_plugin_xml(new_xml_string, true);
       // shellui_log("Plugins XML: %s", new_xml_string.c_str());
    }
    else if (is_custom_pkg) {

        if (!custom_pkg_list.empty()) {
            custom_pkg_list.clear();
            //shellui_log("Custom Pkg Installers found");
        }
        generate_custom_pkg_xml(new_xml_string);
       // shellui_log("Custom Pkg Installers XML: %s", new_xml_string.c_str());
	}
    else if (is_su_menu) {
#if 0
        if (!su_list.empty()) {
            su_list.clear();
            //shellui_log("Superuser apps found");
        }
        generate_su_xml(new_xml_string);
        // shellui_log("Superuser apps XML: %s", new_xml_string.c_str());
#endif
    }
    else if (is_cheats) {
        generate_cheats_xml(new_xml_string, current_menu_tid, (cheats_shortcut_activated || cheats_shortcut_activated_not_open), cheats_shortcut_activated_not_open);
        cheats_shortcut_activated_not_open = cheats_shortcut_activated = false;
    }
	else if (is_auto_plugin) {
        if (!auto_list.empty()) {
            auto_list.clear();
           // shellui_log("Plugins found");
        }
		generate_plugin_xml(new_xml_string, false);
	} 
  else if (is_remote_play) {
        //shellui_log("Generate remote play XML\n");
        generate_remote_play_xml(new_xml_string);   
  }
	else if (is_plapps) {
        //shellui_log("Generate payloads XML\n");
        if (!payloads_apps_list.empty()) {
             payloads_apps_list.clear();
            //shellui_log("Payloads found");
        }
       generate_plapps_xml(new_xml_string);
  }

    MemoryStream_Instance = New_Mono_XML_From_String(new_xml_string);
    if (!MemoryStream_Instance) {
        return GetManifestResourceStream_Original(inst, FileName);
    }

    return (uint64_t)MemoryStream_Instance;
}

extern "C" int sceKernelGetPs4SystemSwVersion(OrbisKernelSwVersion *);

MonoMethod* set_value_method = nullptr;
int OnPreCreate_Hook(MonoObject* Instance, MonoObject* element) {
    bool& DPI = global_conf.DPI;
    bool& DPI_v2 = global_conf.DPI_v2;
    MonoString* s_MonoText = nullptr;

    char tid[32] = { 0 };
    int cheat_id = 0;

    if (!Instance || !element)
    {
#if SHELL_DEBUG==1
        shellui_log("[LM HOOK] OnPreCreate_Hook: args are null");
#endif
        return oOnPreCreate(Instance, element);
    }

    std::string id = GetPropertyValue(element, "Id");
   // shellui_log("[LM HOOK] OnPreCreate_Hook: Id: %s", id.c_str());

    if (!set_value_method) {
        MonoAssembly* Legacy_assembly = mono_domain_assembly_open(Root_Domain, legacy_dec.c_str());
        if (!Legacy_assembly) {
            shellui_log("Failed to open assembly.");
            return -1;
        }

        // Get the image
        MonoImage* leg_img = mono_assembly_get_image(Legacy_assembly);
        if (!leg_img) {
            shellui_log("Failed to get image.");
            return -1;
        }

        MonoClass* klass = mono_class_from_name(leg_img, UI3_dec.c_str(), "SettingElement");
        if (!klass) {
            sceKernelDebugOutText(0, "Failed to find class\n");
            return -1;
        }

        MonoProperty* s_Property = mono_class_get_property_from_name(klass, "Value");
        if (s_Property == NULL) {
            shellui_log("Failed to find property");
            return -1;
        }

        set_value_method = mono_property_get_set_method(s_Property);
        if (set_value_method == NULL) {
            shellui_log("Failed to find set method");
            return -1;
        }
    }


    if (!plugins_list.empty()) {
        for (auto plugin : plugins_list) {
            if (plugin.id == id) {
                s_MonoText = mono_string_new(Root_Domain, (sceSystemServiceGetAppId(plugin.tid.c_str()) > 0) ? "1" : "0");
            }
        }
    }

    if (!auto_list.empty()) {
        for (auto plugin : auto_list) {
            if (plugin.id == id) {
                std::string auto_path = plugin.shellui_path + ".auto_start";
                s_MonoText = mono_string_new(Root_Domain, if_exists(auto_path.c_str()) ? "1" : "0");
            }
        }
    }
  
    if (id == "id_lm_test") {
        s_MonoText = mono_string_new(Root_Domain, "0");
    }
    else if (id == "id_overlay_gpu") {
		s_MonoText = mono_string_new(Root_Domain, global_conf.overlay_gpu ? "1" : "0");
    }
    else if (id == "id_overlay_fps") {
		s_MonoText = mono_string_new(Root_Domain, global_conf.overlay_fps ? "1" : "0");
    }
	else if (id == "id_overlay_ip") {
        s_MonoText = mono_string_new(Root_Domain, global_conf.overlay_ip ? "1" : "0");
	}
    else if (id == "id_all_cpu_usage") {
		s_MonoText = mono_string_new(Root_Domain, global_conf.all_cpu_usage ? "1" : "0");
    }
	else if (id == "id_overlay_cpu") {
        s_MonoText = mono_string_new(Root_Domain, global_conf.overlay_cpu ? "1" : "0");
	}
    else if (id == "id_overlay_ram") {
		s_MonoText = mono_string_new(Root_Domain, global_conf.overlay_ram ? "1" : "0");
    }
    else if (id == "id_kstuff_autoload") {
		s_MonoText = mono_string_new(Root_Domain, !if_exists("/user/data/OrionHEN/no_kstuff") ? "1" : "0");
    }
    else if (id == "id_disp_titleids"){
        s_MonoText = mono_string_new(Root_Domain, global_conf.display_tids ? "1" : "0");
    }
    else if (id == "id_enable_fan_speed"){
        s_MonoText = mono_string_new(Root_Domain, global_conf.enable_fan_speed ? "1" : "0");
    }
    else if (id == "id_dpi_service") {
        s_MonoText = mono_string_new(Root_Domain, DPI ?  "1" : "0");
    }
    else if (id == "id_DPI_v2_service") {
        s_MonoText = mono_string_new(Root_Domain, DPI_v2 ?  "1" : "0");
    }
    else if (id == "id_selected_cheats_repo") {
        s_MonoText = mono_string_new(Root_Domain, global_conf.selected_cheats_repo ? "1" : "0");
    }
    else if (id == "id_start_opt") {
        s_MonoText = mono_string_new(Root_Domain, std::to_string(global_conf.start_option).c_str());
    }
    else if (id == "id_data_sb") {
        s_MonoText = mono_string_new(Root_Domain, global_conf.allow_data_sandbox ? "1" : "0");
	  }
    else if (id == "id_sistro_ps5debug") {
		    s_MonoText = mono_string_new(Root_Domain, "0");
	  }
    else if (id == "id_rest_1") {
         s_MonoText = mono_string_new(Root_Domain, std::to_string(global_conf.rest_delay_seconds).c_str());
    }
    else if (id == "id_fan_speed") {
        s_MonoText = mono_string_new(Root_Domain, std::to_string(global_conf.fan_threshold).c_str());
    }
    else if (id == "id_rest_2") {
        s_MonoText = mono_string_new(Root_Domain, global_conf.util_rest_kill ? "1" : "0");
    }
    else if (id == "id_rest_3") {
        s_MonoText = mono_string_new(Root_Domain, global_conf.game_rest_kill ? "1" : "0");
    }
    else if (id == "id_rest_4") {
        s_MonoText = mono_string_new(Root_Domain, global_conf.disable_toolbox_auto_start_for_rest_mode ? "1" : "0");
    }
    else if (id.rfind("id_cheat_") != std::string::npos) {
        if(is_current_game_open){
           ParseCheatID(id.c_str(), tid, &cheat_id);
           bool enabled = cheatEnabledMap[cheat_id];
           s_MonoText = mono_string_new(Root_Domain, enabled ? "1" : "0");
        }
    }
    else if (id.rfind("id_toolbox_shortcut") != std::string::npos){
        s_MonoText = mono_string_new(Root_Domain, std::to_string(global_conf.toolbox_shortcut_opt).c_str());
    }
    else if (id == "id_cheats_shortcut") {
        s_MonoText = mono_string_new(Root_Domain, std::to_string(global_conf.cheats_shortcut_opt).c_str());
    }
    else if (id == "id_toolbox_auto_start") {
        s_MonoText = mono_string_new(Root_Domain, global_conf.toolbox_auto_start ? "1" : "0");
    }
    else if (id == "id_debug_jb"){
       s_MonoText = mono_string_new(Root_Domain, global_conf.debug_app_jb_msg ? "1" : "0");
    }
    else if (id == "id_debug_legacy_cmd") {
        s_MonoText = mono_string_new(Root_Domain, global_conf.debug_legacy_cmd_server ? "1" : "0");
    }
    else if (id == "id_custom_game_opts"){
       s_MonoText = mono_string_new(Root_Domain, global_conf.OrionHEN_game_opts ? "1" : "0");
    }
    else if (id == "id_auto_eject") {
        s_MonoText = mono_string_new(Root_Domain, global_conf.auto_eject_disc ? "1" : "0");
    }
    else if (id == "id_overlay_change_pos") {
        s_MonoText = mono_string_new(Root_Domain, std::to_string(global_conf.overlay_pos).c_str());
	}

    if(s_MonoText)
       mono_runtime_invoke(set_value_method, element, (void**)&s_MonoText, NULL);

    return oOnPreCreate(Instance, element);
}

void CheckRunningOnMainThread() {
	//notify("Main thread check called!");
}
void Patch_Main_thread_Check(MonoImage * image_core) {

    uint64_t real_addr = Get_Address_of_Method(image_core, "Sce.PlayStation.Core.Runtime", "Diagnostics", "CheckRunningOnMainThread", 0);
    if (!real_addr) {
#if SHELL_DEBUG==1
        shellui_log("Failed to get method address");
#endif
        return;
    }
#if SHELL_DEBUG==1
    shellui_log("changing permissions on (%p).", real_addr);
#endif
    
	DetourFunction(real_addr, (void*)&CheckRunningOnMainThread);
#if SHELL_DEBUG==1
    shellui_log("Main thread check patched\n");
#endif

}
// Common logic function
bool handle_uri_boot_common(MonoString* uri, int opt, MonoString* titleIdForBootAction) {
    std::string uri_string = Mono_to_String(uri);
    std::string titleId = titleIdForBootAction ? Mono_to_String(titleIdForBootAction) : "";
    
#if SHELL_DEBUG==1
    shellui_log("Boot: %s (%s), OPT %i", 
                uri_string.c_str(), 
                !titleId.empty() ? titleId.c_str() : "NULL", 
                opt);
#endif
  
    if(uri_string == "OrionHEN?Cheats") {
#if SHELL_DEBUG==1
      shellui_log("cheats_shortcut URI detected");
#endif
      cheats_shortcut_activated = true;
      return true; // Signal to redirect
    }
    else if(uri_string == "OrionHEN?Cheats_not_open") {
#if SHELL_DEBUG==1
      shellui_log("cheats_shortcut (not open) URI detected");
#endif
      cheats_shortcut_activated_not_open = true;
      return true;
    }
    else if (uri_string == "OrionHEN?Dump") {
#if SHELL_DEBUG==1
        shellui_log("Dump URI detected");
#endif
        notify("Game dumper payload is not bundled in OrionHEN");
        return true; // Signal to redirect
    }
    else if (uri_string == "OrionHEN?DL_UPDATE") {
#if SHELL_DEBUG==1
        shellui_log("DL_UPDATE URI detected");
#endif
        
        return true; // Signal to redirect
    }

    return false; // No redirect needed
  }
  
  bool uri_boot_hook(MonoString* uri, int opt, MonoString* titleIdForBootAction) {
    if(handle_uri_boot_common(uri, opt, titleIdForBootAction)) {
        std::string uri_string = Mono_to_String(uri);
        if(uri_string == "OrionHEN?Dump") {
          return boot_orig(mono_string_new(Root_Domain, "pshomeui:navigateToHome?bootCondition=psButton"),  opt, titleIdForBootAction);
        }
      // Redirect to debug settings
      return boot_orig(mono_string_new(Root_Domain, "pssettings:play?mode=settings&function=debug_settings"), opt, titleIdForBootAction);
    }
    
    return boot_orig(uri, opt, titleIdForBootAction);
  }
  
  bool uri_boot_hook_2(MonoString* uri, int opt) {
  #if SHELL_DEBUG==1
    shellui_log("uri_boot_hook_2: %s, opt: %i", Mono_to_String(uri).c_str(), opt);
  #endif
    if(handle_uri_boot_common(uri, opt, nullptr)) {
      // Redirect to debug settings (no titleId parameter for older fw)
      std::string uri_string = Mono_to_String(uri);
      if(uri_string == "OrionHEN?Dump") {
        return boot_orig_2(mono_string_new(Root_Domain, "pshomeui:navigateToHome?bootCondition=psButton"),  opt);
      }

      return boot_orig_2(mono_string_new(Root_Domain, "pssettings:play?function=debug_settings"),  opt);
    }
    
    return boot_orig_2(uri, opt);
  }

  GamePadData GetData_hook(int deviceIndex) {
    GamePadData result;
    bool cheas_sc_activated = false;
    bool toolbox_sc_activated = false;
  
    const std::chrono::milliseconds LONG_PRESS_DURATION(1000); // 1 second
  
    // Static variables for Cheats shortcut hold detection
    static bool cheats_pressed = false;
    static std::chrono::steady_clock::time_point cheats_press_start;
    static bool cheats_long_press_triggered = false;
  
    // Static variables for Toolbox shortcut hold detection
    static bool toolbox_pressed = false;
    static std::chrono::steady_clock::time_point toolbox_press_start;
    static bool toolbox_long_press_triggered = false;

  
    result = GetData(deviceIndex);

    // Cheats Shortcut
    if (global_conf.cheats_shortcut_opt != CHEATS_SC_OFF) {
      bool cheats_buttons_held = false;
  
      switch (global_conf.cheats_shortcut_opt) {
      case R3_L3:
        cheats_buttons_held = (result.Buttons & R3) && (result.Buttons & L3);
        break;
      case L2_TRIANGLE:
        cheats_buttons_held = (result.Buttons & L2) && (result.Buttons & Triangle);
        break;
      case LONG_OPTIONS:
        cheats_buttons_held = (result.Buttons & Option);
        break;
      default:
        break;
      }
  
      if (cheats_buttons_held) {
        if (!cheats_pressed) {
          cheats_pressed = true;
          cheats_press_start = std::chrono::steady_clock::now();
          cheats_long_press_triggered = false;
          #if SHELL_DEBUG == 1
          shellui_log("Cheats buttons pressed - starting timer");
          #endif
        } else {
          auto current_time = std::chrono::steady_clock::now();
          auto hold_duration = std::chrono::duration_cast < std::chrono::milliseconds > (
            current_time - cheats_press_start
          );
  
          // Log every 500ms to track progress
          static auto last_log_time = std::chrono::steady_clock::now();
          if (std::chrono::duration_cast < std::chrono::milliseconds > (
              current_time - last_log_time) >= std::chrono::milliseconds(500)) {
              #if SHELL_DEBUG == 1
              shellui_log("Cheats buttons held for %lld ms (need %lld ms)",
              hold_duration.count(),
              LONG_PRESS_DURATION.count());
              #endif
            last_log_time = current_time;
          }
  
          if (hold_duration >= LONG_PRESS_DURATION && !cheats_long_press_triggered) {
            #if SHELL_DEBUG == 1
            shellui_log("Cheats long press threshold reached! Duration: %lld ms",
              hold_duration.count());
            #endif
            cheas_sc_activated = true;
            cheats_long_press_triggered = true;
          }
        }
      } else {
        if (cheats_pressed) {
          #if SHELL_DEBUG == 1
          auto current_time = std::chrono::steady_clock::now();
          auto hold_duration = std::chrono::duration_cast < std::chrono::milliseconds > (
            current_time - cheats_press_start
          );
          shellui_log("Cheats buttons released after %lld ms (needed %lld ms)",
            hold_duration.count(),
            LONG_PRESS_DURATION.count());
          #endif
        }
        cheats_pressed = false;
        cheats_long_press_triggered = false;
      }
  
      if (cheas_sc_activated) {
#if SHELL_DEBUG == 1
        shellui_log("Cheats Shortcut Activated");
#endif
        GoToURI("OrionHEN?Cheats");
        result.Buttons = None; // Clear the Select button to prevent triggering other actions
        cheas_sc_activated = false; // Reset the flag
      }
    }
  
    // Toolbox Shortcut
    if (global_conf.toolbox_shortcut_opt != TOOLBOX_SC_OFF) {
      bool toolbox_buttons_held = false;
  
      switch (global_conf.toolbox_shortcut_opt) {
      case L2_R3:
        toolbox_buttons_held = (result.Buttons & L2) && (result.Buttons & R3);
        break;
      default:
        break;
      }
  
      if (toolbox_buttons_held) {
        if (!toolbox_pressed) {
          toolbox_pressed = true;
          toolbox_press_start = std::chrono::steady_clock::now();
          toolbox_long_press_triggered = false;
        } else {
          auto current_time = std::chrono::steady_clock::now();
          auto hold_duration = std::chrono::duration_cast < std::chrono::milliseconds > (
            current_time - toolbox_press_start
          );
  
          if (hold_duration >= LONG_PRESS_DURATION && !toolbox_long_press_triggered) {
            toolbox_sc_activated = true;
            toolbox_long_press_triggered = true;
          }
        }
      } else {
        toolbox_pressed = false;
        toolbox_long_press_triggered = false;
      }
  
      if (toolbox_sc_activated) {
#if SHELL_DEBUG == 1
        shellui_log("Toolbox Shortcut Activated");
#endif
        GoToURI("pssettings:play?mode=settings&function=debug_settings");
        result.Buttons = None; // Clear the Select button to prevent triggering other actions
      }
    }
  
#if SHELL_DEBUG==1
    if (result.Buttons & Option) {
      shellui_log("Option button pressed");
    }
#endif
  
    return result;
  }

bool CaptureScreen(){
  if(global_conf.cheats_shortcut_opt == CHEATS_LONG_SHARE){
    //shellui_log("CaptureScreen: Long Share Shortcut activated");
    GoToURI("OrionHEN?Cheats");
    return true;
  }
  else if (global_conf.toolbox_shortcut_opt == TOOLBOX_LONG_SHARE){
    //shellui_log("CaptureScreen: Long Share Shortcut activated");
    GoToURI("pssettings:play?mode=settings&function=debug_settings");
    return true;
  }

  return false;
}
void CaptureScreen_old(MonoObject *inst, int userId, long deviceId, int capType, MonoObject* capInfo){
#if SHELL_DEBUG == 1
  shellui_log("CaptureScreen: userId: %d, deviceId: %ld, capType: %d", userId, deviceId, capType);
#endif

  if(CaptureScreen()){
#if SHELL_DEBUG == 1
    shellui_log("CaptureScreen: Shortcut activated, redirecting");
#endif
    return;
  }
  CaptureScreen_orig_old(inst, userId, deviceId, capType, capInfo);

}

void CaptureScreen_new(MonoObject * inst, int userId, long deviceId, int capType, MonoString* format, MonoObject* capInfo) {
#if SHELL_DEBUG == 1
  shellui_log("CaptureScreen_new: userId: %d, deviceId: %ld, capType: %d", userId, deviceId, capType);
#endif
  if(CaptureScreen()){
#if SHELL_DEBUG == 1
    shellui_log("CaptureScreen_new: Shortcut activated, redirecting");
#endif
    return;
  }
  CaptureScreen_orig_new(inst, userId, deviceId, capType, format, capInfo);
}

void OnShareButton(MonoObject * data) {
#if SHELL_DEBUG == 1
  shellui_log("OnShareButton: data: %p", data);
#endif

  if( global_conf.cheats_shortcut_opt == CHEATS_SINGLE_SHARE) {
    // shellui_log("Share Shortcut: Redirecting to Cheats");
    GoToURI("OrionHEN?Cheats");
    return;
  }
  else if (global_conf.toolbox_shortcut_opt == TOOLBOX_SINGLE_SHARE) {
    // shellui_log("Share Shortcut: Redirecting to Toolbox");
    GoToURI("pssettings:play?mode=settings&function=debug_settings");
    return;
  }

  OnShareButton_orig(data);
}

void save_appid(int value, const char* filename) {
    std::ofstream file(filename);
    file << value;
}
bool app_launched = false;
int LaunchApp(MonoString* titleId, uint64_t* args, int argsSize, LaunchAppParam *param){
#if 1
   if(!if_exists("/system_tmp/patch_plugin")) {
      #if SHELL_DEBUG == 1
      shellui_log("patch plugin not running .. returning with orig");
      #endif
	  unsigned int ret = LaunchApp_orig(titleId, args, argsSize, param);
      if (ret < 0) {
         #if SHELL_DEBUG == 1
         notify("LaunchApp failed with error code: %d", ret);
         #endif
         return ret;
      }

      app_launched = true;
      return ret;

   }
#endif
#if SHELL_DEBUG == 1
  shellui_log("LaunchApp called with titleId: %s, argsSize: %d, param->size: %d", mono_string_to_utf8(titleId), argsSize, param->size);
#endif
  notify("Launching app: %s checking for patches ...", mono_string_to_utf8(titleId));

  unsigned int ret = LaunchApp_orig(titleId, args, argsSize, param);
  if (ret < 0) {
    #if SHELL_DEBUG == 1
    notify("LaunchApp failed with error code: %d", ret);
    #endif
    return ret;
  }

  app_launched = true;

 #if SHELL_DEBUG == 1
  notify("LaunchApp returned: %d", ret);
  #endif

  save_appid(ret, "/system_tmp/app_launched");
  return ret;

}

int sceRegMgrGetInt_hook(long regid, int* out_val){
  bool dis_tids = global_conf.display_tids;

  if(dis_tids && regid == SCE_REGMGR_ENT_KEY_DEVENV_TOOL_SHELLUI_disp_titleid){
    if (out_val) {
       *out_val = 1;
    }
#if SHELL_DEBUG==1
    shellui_log("RegMGR lookup called for SHELLUI_disp_titleid, spoofing out_var to 1");
#endif
    return 0;
  }

  #define visualize_fps_range 2013460994
  #define visualize_fps_en 2013460993
  #define visualize_fps_pos 2013460995
  #define visualize_fps_port 2013460996
  static int (*crash)() = nullptr;

  if(regid == visualize_fps_range) {
      crash();
    shellui_log("visualize_fps_range regid %lx", regid);
    if (out_val) {
      *out_val = 2;
    }
    return 0;
  }
  else if(regid == visualize_fps_en) {

	  crash();
    shellui_log("visualize_fps_en regid %lx", regid); 
    if (out_val) {
      *out_val = 3;
    }
    return 0;
  }
  else if(regid == visualize_fps_pos) {
      crash();
    shellui_log("visualize_fps_pos regid %lx", regid);
    if (out_val) {
      *out_val = 1;
    }
    return 0;
  }
  else if(regid == visualize_fps_port) {
      crash();
    shellui_log("visualize_fps_port regid %lx", regid);
    if (out_val) {
      *out_val = 0;
    }

    return 0;
  }

  //shellui_log("sceRegMgrGetInt_hook: regid %lx", regid);

  int ret = 0;
  if(__sys_regmgr_call(2, regid, &ret, out_val, SCE_REGMGR_INT_SIZE)){
#if SHELL_DEBUG==1
    shellui_log("sceRegMgrGetInt_hook: Failed to get regid 0x%lx, ret %d", regid, ret);
#endif
    ret = SCE_REGMGR_ERROR_PRM_REGID;
  }

  return ret;
}
static std::string extractTIDFromURI(const std::string& url) {
    const std::string prefix = "titleId=";
    size_t pos = url.find(prefix);
    
    if (pos != std::string::npos) {
        pos += prefix.length();
        size_t end = url.find('&', pos);
        if (end == std::string::npos) {
            return url.substr(pos);
        } else {
            return url.substr(pos, end - pos);
        }
    }
    return std::string(); // Not found
}

void createJson_hook(MonoObject* inst, MonoObject* array, MonoString* id, MonoString* label, MonoString* actionUrl, MonoString* actionId, MonoString* messageId, MonoObject* subMenu, bool enable) {

    std::string id_str = Mono_to_String(id);

#if SHELL_DEBUG==1
    shellui_log("createJson_hook: %lx id: %s, label: %s, actionUrl: %s, actionId: %s, messageId: %s", 
               inst, id_str.c_str(), 
               Mono_to_String(label).c_str(), 
               Mono_to_String(actionUrl).c_str(), 
               Mono_to_String(actionId).c_str(), 
               Mono_to_String(messageId).c_str());
#endif

    if(!global_conf.OrionHEN_game_opts) {
        createJson(inst, array, id, label, actionUrl, actionId, messageId, subMenu, enable);
        return;
    }

    // Only extract and update titleId if one is found in the current URL
    std::string extracted_tid = extractTIDFromURI(Mono_to_String(actionUrl));
    if (!extracted_tid.empty() && extracted_tid != current_menu_tid) {
        current_menu_tid = extracted_tid;
#if SHELL_DEBUG==1
        //notify("Current menu titleId: %s", current_menu_tid.c_str());
        shellui_log("Updated menu titleId: %s", current_menu_tid.c_str());
#endif
    }
#if 1
    if(id_str == "MENU_ID_SAVE_DATA_MANAGEMENT_PS4_MANUAL" || id_str == "MENU_ID_SAVE_DATA_MANAGEMENT_PS5_MANUAL" || (id_str == "MENU_ID_UPDATE_HISTORY" && 0)){
       createJson(inst, array, mono_string_new(Root_Domain, "MENU_ID_CUST_UPDATES"), mono_string_new(Root_Domain, "★ (测试版) 转储游戏/应用"), mono_string_new(Root_Domain, "OrionHEN?Dump"), actionId, nullptr, subMenu, enable);
       return;
    }
#endif
    if(id_str == "MENU_ID_CHECK_PATCH"){  
      //createJson_hook: 8815fec90 id: MENU_ID_CHECK_PATCH, label: , actionUrl: pspatchcheck:check-for-update?titleid=CUSA01127, actionId: , messageId: msgid_check_update
        createJson(inst, array, mono_string_new(Root_Domain, "MENU_ID_CHEATS"), mono_string_new(Root_Domain, "★ OrionHEN 金手指"), mono_string_new(Root_Domain, "OrionHEN?Cheats_not_open"), actionId, nullptr, subMenu, enable);
        return;
    }

    if(id_str == "MENU_ID_INTELLECTUAL_PROPERTY_NOTICES"){
        std::string uri = "psappinst:pat-uninstall?titleid=" + current_menu_tid;
        createJson(inst, array, mono_string_new(Root_Domain, "MENU_ID_REMOVE_UPDATE"), mono_string_new(Root_Domain, "★ 删除"), mono_string_new(Root_Domain, uri.c_str()), actionId, nullptr, subMenu, enable);
        return;
    }

    createJson(inst, array, id, label, actionUrl, actionId, messageId, subMenu, enable);
}

void Terminate() {
    shellui_log("******************************\nShellUI is exiting\n*****************************");
    shellui_log("Sending Action");
    IPC_Client& main_ipc = IPC_Client::getInstance(false);
    if(global_conf.game_rest_kill) {
	    	shellui_log("Killing Game");
        int pid = find_pid("NA", false, true);
        if(pid > 0)
           main_ipc.ForceKillPID(pid);
    }
    //dont send the command if the util is already dead
    if(global_conf.util_rest_kill) {
        shellui_log("Killing Util");
        KillAllWithName("Utility", SIGKILL);
    }
    else {
        IPC_Client& ipc = IPC_Client::getInstance(true);
        ipc.SendRestModeAction();
    }
    oTerminate();
}
