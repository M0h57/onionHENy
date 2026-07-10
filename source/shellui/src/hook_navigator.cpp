/* Copyright (C) 2025 OrionHEN / LightningMods — P0 split. */


#include "HookedFuncs.hpp"
#include "ipc.hpp"
#include "external_symbols.hpp"
#include <string>

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

