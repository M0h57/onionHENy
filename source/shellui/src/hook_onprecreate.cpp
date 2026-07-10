/* Copyright (C) 2025 OrionHEN / LightningMods
 * Extracted from HookFunctions.cpp — hook_onprecreate
 */
#include "HookedFuncs.hpp"
#include "RemotePlay.h"
#include "Detour.h"
#include "ipc.hpp"
#include <msg.hpp>
#include <pthread.h>
#include <sys/stat.h>
#include <fstream>
#include <unistd.h>
#include <vector>
#include <atomic>
#include <cstring>
#include <algorithm>

extern int (*oOnPreCreate)(MonoObject* Instance, MonoObject* element);
extern MonoMethod* set_value_method;
extern int cheatEnabledMap[];
extern std::string currentCheatTID;

extern bool is_plugin, is_su_menu, is_custom_pkg, is_debug_settings, is_cheats, is_auto_plugin, is_remote_play, is_plapps;
std::string GetPropertyValue(MonoObject* element, const char* propertyName);
bool if_exists(const char* path);

int OnPreCreate_Hook(MonoObject* Instance, MonoObject* element) {
    bool& DPI = g_settings.DPI;
    bool& DPI_v2 = g_settings.DPI_v2;
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
		s_MonoText = mono_string_new(Root_Domain, g_settings.overlay_gpu ? "1" : "0");
    }
    else if (id == "id_overlay_fps") {
		s_MonoText = mono_string_new(Root_Domain, g_settings.overlay_fps ? "1" : "0");
    }
	else if (id == "id_overlay_ip") {
        s_MonoText = mono_string_new(Root_Domain, g_settings.overlay_ip ? "1" : "0");
	}
    else if (id == "id_all_cpu_usage") {
		s_MonoText = mono_string_new(Root_Domain, g_all_cpu_usage ? "1" : "0");
    }
	else if (id == "id_overlay_cpu") {
        s_MonoText = mono_string_new(Root_Domain, g_settings.overlay_cpu ? "1" : "0");
	}
    else if (id == "id_overlay_ram") {
		s_MonoText = mono_string_new(Root_Domain, g_settings.overlay_ram ? "1" : "0");
    }
    else if (id == "id_kstuff_autoload") {
		s_MonoText = mono_string_new(Root_Domain, !if_exists("/user/data/OrionHEN/no_kstuff") ? "1" : "0");
    }
    else if (id == "id_disp_titleids"){
        s_MonoText = mono_string_new(Root_Domain, g_settings.display_tids ? "1" : "0");
    }
    else if (id == "id_enable_fan_speed"){
        s_MonoText = mono_string_new(Root_Domain, g_settings.enable_fan_speed ? "1" : "0");
    }
    else if (id == "id_dpi_service") {
        s_MonoText = mono_string_new(Root_Domain, DPI ?  "1" : "0");
    }
    else if (id == "id_DPI_v2_service") {
        s_MonoText = mono_string_new(Root_Domain, DPI_v2 ?  "1" : "0");
    }
    else if (id == "id_selected_cheats_repo") {
        s_MonoText = mono_string_new(Root_Domain, g_settings.selected_cheats_repo ? "1" : "0");
    }
    else if (id == "id_start_opt") {
        s_MonoText = mono_string_new(Root_Domain, std::to_string(g_settings.start_option).c_str());
    }
    else if (id == "id_rest_1") {
         s_MonoText = mono_string_new(Root_Domain, std::to_string(g_settings.rest_mode_delay_seconds).c_str());
    }
    else if (id == "id_fan_speed") {
        s_MonoText = mono_string_new(Root_Domain, std::to_string(g_settings.fan_threshold).c_str());
    }
    else if (id == "id_rest_2") {
        s_MonoText = mono_string_new(Root_Domain, g_settings.util_rest_kill ? "1" : "0");
    }
    else if (id == "id_rest_3") {
        s_MonoText = mono_string_new(Root_Domain, g_settings.game_rest_kill ? "1" : "0");
    }
    else if (id == "id_rest_4") {
        s_MonoText = mono_string_new(Root_Domain, g_settings.disable_toolbox_auto_start_for_rest_mode ? "1" : "0");
    }
    else if (id.rfind("id_cheat_") != std::string::npos) {
        if(is_current_game_open){
           ParseCheatID(id.c_str(), tid, &cheat_id);
           bool enabled = cheatEnabledMap[cheat_id];
           s_MonoText = mono_string_new(Root_Domain, enabled ? "1" : "0");
        }
    }
    else if (id.rfind("id_toolbox_shortcut") != std::string::npos){
        s_MonoText = mono_string_new(Root_Domain, std::to_string(g_settings.toolbox_shortcut_opt).c_str());
    }
    else if (id == "id_cheats_shortcut") {
        s_MonoText = mono_string_new(Root_Domain, std::to_string(g_settings.cheats_shortcut_opt).c_str());
    }
    else if (id == "id_toolbox_auto_start") {
        s_MonoText = mono_string_new(Root_Domain, g_settings.toolbox_auto_start ? "1" : "0");
    }
    else if (id == "id_debug_jb"){
       s_MonoText = mono_string_new(Root_Domain, g_settings.debug_app_jb_msg ? "1" : "0");
    }
    else if (id == "id_debug_legacy_cmd") {
        s_MonoText = mono_string_new(Root_Domain, g_settings.legacy_cmd_server ? "1" : "0");
    }
    else if (id == "id_custom_game_opts"){
       s_MonoText = mono_string_new(Root_Domain, g_settings.orionhen_game_opts ? "1" : "0");
    }
    else if (id == "id_auto_eject") {
        s_MonoText = mono_string_new(Root_Domain, g_settings.auto_eject_disc ? "1" : "0");
    }
    else if (id == "id_overlay_change_pos") {
        s_MonoText = mono_string_new(Root_Domain, std::to_string(g_settings.overlay_pos).c_str());
	}

    if(s_MonoText)
       mono_runtime_invoke(set_value_method, element, (void**)&s_MonoText, NULL);

    return oOnPreCreate(Instance, element);
}

