/* Copyright (C) 2025 OrionHEN / LightningMods — P0 split. */


#include "HookedFuncs.hpp"
#include "ipc.hpp"
#include "external_symbols.hpp"
#include <orion/platform.h>
#include <string>
#include <atomic>

extern MonoClass* MemoryStream_IO;
extern MonoObject* MemoryStream_Instance;
extern bool is_plugin, is_su_menu, is_custom_pkg, is_debug_settings, is_cheats;
extern bool is_auto_plugin, is_remote_play, is_plapps;
extern bool cheats_shortcut_activated, cheats_shortcut_activated_not_open;
extern std::string current_menu_tid;
extern std::string plugin_xml, remote_play_xml, debug_settings_xml, cheats_xml;
extern std::string dec_xml_str, UI3_dec, legacy_dec;
void generate_plugin_xml(std::string& xml_buffer, bool plugins_xml);
void generate_remote_play_xml(std::string& xml_buffer);
void generate_plapps_xml(std::string& new_xml);
void generate_custom_pkg_xml(std::string& xml_buffer);
void generate_cheats_xml(std::string &new_xml, std::string& not_open_tid, bool running_as_debug_settings, bool show_while_not_open);

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

