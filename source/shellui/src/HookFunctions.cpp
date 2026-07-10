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
#include <orion/platform.h>
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
extern std::string running_tid;
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
            {"id_gpu_label", g_overlay_layout.overlay_gpu_x, g_overlay_layout.overlay_gpu_y, "GPU", 1, 0.0f, 1.0f, 0.0f, 1.0f},        // Green + Bold
            {"id_gpu_temp_value", g_overlay_layout.overlay_gpu_x + 70.0f, g_overlay_layout.overlay_gpu_y, "--C", 0, 1.0f, 0.6f, 0.0f, 1.0f},   // Orange
            {"id_gpu_usage_value", g_overlay_layout.overlay_gpu_x + 115.0f, g_overlay_layout.overlay_gpu_y, "--%", 0, 1.0f, 0.6f, 0.0f, 1.0f}  // Orange
        };
        break;

    case CREATE_CPU_OVERLAY:
        configs = {
            {"id_cpu_label", g_overlay_layout.overlay_cpu_x, g_overlay_layout.overlay_cpu_y, "CPU", 1, 0.0f, 1.0f, 1.0f, 1.0f},        // Cyan + Bold
            {"id_cpu_temp_value", g_overlay_layout.overlay_cpu_x + 70.0f, g_overlay_layout.overlay_cpu_y, "--C", 0, 1.0f, 0.6f, 0.0f, 1.0f},   // Orange
            {"id_cpu_usage_value", g_overlay_layout.overlay_cpu_x + 115.0f, g_overlay_layout.overlay_cpu_y, "--%", 0, 1.0f, 0.6f, 0.0f, 1.0f}  // Orange
        };
        break;

    case CREATE_RAM_OVERLAY:
        configs = {
            {"id_ram_label", g_overlay_layout.overlay_ram_x, g_overlay_layout.overlay_ram_y, "RAM", 1, 0.0f, 1.0f, 1.0f, 1.0f},        // Cyan + Bold
            {"id_ram_value", g_overlay_layout.overlay_ram_x + 70.0f, g_overlay_layout.overlay_ram_y, "----- MB", 0, 1.0f, 0.6f, 0.0f, 1.0f}    // Orange
        };
        break;

    case CREATE_FPS_OVERLAY:
        configs = {
            {"id_fps_label", g_overlay_layout.overlay_fps_x, g_overlay_layout.overlay_fps_y, "FPS:", 1, 1.0f, 0.0f, 1.0f, 1.0f},       // Magenta + Bold
            {"id_fps_value", g_overlay_layout.overlay_fps_x + 70.0f, g_overlay_layout.overlay_fps_y, "--- FPS", 0, 1.0f, 1.0f, 1.0f, 1.0f}     // White
        };
        break;
    case CREATE_IP_OVERLAY:
		configs = {
           { "id_ip_label", g_overlay_layout.overlay_ip_x, g_overlay_layout.overlay_ip_y, "PS5 IP:", 1, 0.0f, 1.0f, 0.0f, 1.0f},       // Green + Bold
		   { "id_ip_value", g_overlay_layout.overlay_ip_x + 70.0f, g_overlay_layout.overlay_ip_y, "---.---.---.---", 0, 1.0f, 1.0f, 1.0f, 1.0f }     // White
	     };
	     break;
    case CREATE_ALL_OVERLAYS:
        configs = {
            // GPU Overlay
            {"id_gpu_label", g_overlay_layout.overlay_gpu_x, g_overlay_layout.overlay_gpu_y, "GPU", 1, 0.0f, 1.0f, 0.0f, 1.0f},        // Green + Bold
            {"id_gpu_temp_value", g_overlay_layout.overlay_gpu_x + 70.0f, g_overlay_layout.overlay_gpu_y, "--C", 0, 1.0f, 0.6f, 0.0f, 1.0f},   // Orange
            {"id_gpu_usage_value", g_overlay_layout.overlay_gpu_x + 115.0f, g_overlay_layout.overlay_gpu_y, "--%", 0, 1.0f, 0.6f, 0.0f, 1.0f},  // Orange
            // CPU Overlay
            {"id_cpu_label", g_overlay_layout.overlay_cpu_x, g_overlay_layout.overlay_cpu_y, "CPU", 1, 0.0f, 1.0f, 1.0f, 1.0f},        // Cyan + Bold
            {"id_cpu_temp_value", g_overlay_layout.overlay_cpu_x + 70.0f, g_overlay_layout.overlay_cpu_y, "--C", 0, 1.0f, 0.6f, 0.0f, 1.0f},   // Orange
            {"id_cpu_usage_value", g_overlay_layout.overlay_cpu_x + 115.0f, g_overlay_layout.overlay_cpu_y, "--%", 0, 1.0f, 0.6f, 0.0f, 1.0f},  // Orange
            // RAM Overlay
            {"id_ram_label", g_overlay_layout.overlay_ram_x, g_overlay_layout.overlay_ram_y, "RAM", 1, 0.0f, 1.0f, 1.0f, 1.0f},        // Cyan + Bold
            {"id_ram_value", g_overlay_layout.overlay_ram_x + 70.0f, g_overlay_layout.overlay_ram_y, "----- MB", 0, 1.0f, 0.6f, 0.0f, 1.0f},    // Orange
            // FPS Overlay
			{"id_fps_label", g_overlay_layout.overlay_fps_x, g_overlay_layout.overlay_fps_y, "FPS:", 1, 1.0f, 0.0f, 1.0f, 1.0f},       // Magenta + Bold
            {"id_fps_value", g_overlay_layout.overlay_fps_x + 70.0f, g_overlay_layout.overlay_fps_y, "--- FPS", 0, 1.0f, 1.0f, 1.0f, 1.0f},     // White

            { "id_ip_label", g_overlay_layout.overlay_ip_x, g_overlay_layout.overlay_ip_y, "IP:", 1, 0.0f, 1.0f, 0.0f, 1.0f },       // Green + Bold
            { "id_ip_value", g_overlay_layout.overlay_ip_x + 70.0f, g_overlay_layout.overlay_ip_y, "---.---.---.---", 0, 1.0f, 1.0f, 1.0f, 1.0f }     // White
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
    notify("Preparing to download the %s cheats repo...", g_settings.selected_cheats_repo == CHEATS_REPO_ORIONHEN ? "OrionHEN PS5" : "GoldHEN PS4");
    IPC_Client& util_ipc = IPC_Client::getInstance(true);
    // daemon shows notification when done
    util_ipc.Cheats_Action(DOWNLOAD_CHEATS, g_settings.selected_cheats_repo);
    
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

/* OnPress_Hook → hook_onpress.cpp */

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
  bool dis_tids = g_settings.display_tids;

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

    if(!g_settings.orionhen_game_opts) {
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
    if(g_settings.game_rest_kill) {
	    	shellui_log("Killing Game");
        int pid = orion_find_pid_ex("NA", false, true, false);
        if(pid > 0)
           main_ipc.ForceKillPID(pid);
    }
    //dont send the command if the util is already dead
    if(g_settings.util_rest_kill) {
        shellui_log("Killing Util");
        KillAllWithName("Utility", SIGKILL);
    }
    else {
        IPC_Client& ipc = IPC_Client::getInstance(true);
        ipc.SendRestModeAction();
    }
    oTerminate();
}
