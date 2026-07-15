/* Copyright (C) 2025 OnionHEN / LightningMods
 * Extracted from HookFunctions.cpp — hook_boot
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

extern bool (*boot_orig)(MonoString* uri, int opt, MonoString* titleIdForBootAction);
extern bool (*boot_orig_2)(MonoString* uri, int opt);
#include "shellui_state.hpp"
#include <onion/platform.h>

std::string Mono_to_String(MonoString *str);

// 11.6 RN Settings (NPXS40008) deeplink map (from application.ps.bundle):
//   function=debug_settings     → DebugSettingsScreen + "/debugSettings"  (RN intermediate)
//   function=debug_settings_old → path "debugSettingsOld" → DebugSettingsOldScreen
//                                   (Legacy UI3 host; GetManifestResourceStream returns toolbox XML)
// Opening _old only avoids: Settings → RN DebugSettings → legacy XML stack pollution.
static constexpr const char kToolboxUri[] =
    "pssettings:play?mode=settings&function=debug_settings_old";
static constexpr const char kToolboxUriSimple[] =
    "pssettings:play?function=debug_settings_old";

/** Rewrite function=debug_settings → function=debug_settings_old (idempotent for _old). */
std::string rewrite_debug_settings_to_old(const std::string &uri) {
  static constexpr const char kNeedle[] = "function=debug_settings";
  static constexpr const char kOld[] = "function=debug_settings_old";
  std::string out = uri;
  size_t pos = 0;
  while ((pos = out.find(kNeedle, pos)) != std::string::npos) {
    const size_t end = pos + sizeof(kNeedle) - 1;
    // Already _old, or an unexpected longer suffix — leave alone.
    if (end < out.size() && out[end] != '&') {
      pos = end;
      continue;
    }
    out.replace(pos, sizeof(kNeedle) - 1, kOld);
    pos += sizeof(kOld) - 1;
  }
  return out;
}

bool handle_uri_boot_common(MonoString* uri, int opt, MonoString* titleIdForBootAction) {
    std::string uri_string = Mono_to_String(uri);
    std::string titleId = titleIdForBootAction ? Mono_to_String(titleIdForBootAction) : "";
    
#if SHELL_DEBUG==1
    shellui_log("Boot: %s (%s), OPT %i", 
                uri_string.c_str(), 
                !titleId.empty() ? titleId.c_str() : "NULL", 
                opt);
#endif
  
    if(uri_string == "OnionHEN?Cheats") {
#if SHELL_DEBUG==1
      shellui_log("cheats_shortcut URI detected");
#endif
      g_ui.cheats_shortcut_activated = true;
      return true; // Signal to redirect
    }
    else if(uri_string == "OnionHEN?Cheats_not_open") {
#if SHELL_DEBUG==1
      shellui_log("cheats_shortcut (not open) URI detected");
#endif
      g_ui.cheats_shortcut_activated_not_open = true;
      return true;
    }
    else if (uri_string == "OnionHEN?Dump") {
#if SHELL_DEBUG==1
        shellui_log("Dump URI detected");
#endif
        notify("Game dumper payload is not bundled in OnionHEN");
        return true; // Signal to redirect
    }
    else if (uri_string == "OnionHEN?DL_UPDATE") {
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
        if(uri_string == "OnionHEN?Dump") {
          return boot_orig(mono_string_new(Root_Domain, "pshomeui:navigateToHome?bootCondition=psButton"),  opt, titleIdForBootAction);
        }
      // Toolbox / cheats shortcuts → direct legacy DebugSettingsOldScreen
      return boot_orig(mono_string_new(Root_Domain, kToolboxUri), opt, titleIdForBootAction);
    }

    // Appdb ★Debug Settings and any other debug_settings deeplink: never land on RN screen.
    const std::string original_uri = Mono_to_String(uri);
    const std::string rewritten = rewrite_debug_settings_to_old(original_uri);
    if (rewritten != original_uri) {
#if SHELL_DEBUG == 1
      shellui_log("Boot: rewrite debug_settings → old: %s", rewritten.c_str());
#endif
      return boot_orig(mono_string_new(Root_Domain, rewritten.c_str()), opt,
                       titleIdForBootAction);
    }

    return boot_orig(uri, opt, titleIdForBootAction);
  }
  
  bool uri_boot_hook_2(MonoString* uri, int opt) {
    const std::string original_uri = Mono_to_String(uri);
  #if SHELL_DEBUG==1
    shellui_log("uri_boot_hook_2: %s, opt: %i", original_uri.c_str(), opt);
  #endif
    if(handle_uri_boot_common(uri, opt, nullptr)) {
      // Redirect to debug settings (no titleId parameter for older fw)
      if(original_uri == "OnionHEN?Dump") {
        return boot_orig_2(mono_string_new(Root_Domain, "pshomeui:navigateToHome?bootCondition=psButton"),  opt);
      }

      return boot_orig_2(mono_string_new(Root_Domain, kToolboxUriSimple), opt);
    }

    const std::string rewritten = rewrite_debug_settings_to_old(original_uri);
    if (rewritten != original_uri) {
#if SHELL_DEBUG == 1
      shellui_log("Boot2: rewrite debug_settings → old: %s", rewritten.c_str());
#endif
      return boot_orig_2(mono_string_new(Root_Domain, rewritten.c_str()), opt);
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
    if (g_settings.cheats_shortcut_opt != CHEATS_SC_OFF) {
      bool cheats_buttons_held = false;
  
      switch (g_settings.cheats_shortcut_opt) {
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
        GoToURI("OnionHEN?Cheats");
        result.Buttons = None; // Clear the Select button to prevent triggering other actions
        cheas_sc_activated = false; // Reset the flag
      }
    }
  
    // Toolbox Shortcut
    if (g_settings.toolbox_shortcut_opt != TOOLBOX_SC_OFF) {
      bool toolbox_buttons_held = false;
  
      switch (g_settings.toolbox_shortcut_opt) {
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
        GoToURI(kToolboxUri);
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

