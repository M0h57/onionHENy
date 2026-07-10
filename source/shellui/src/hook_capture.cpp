/* Copyright (C) 2025 OrionHEN / LightningMods
 * Extracted from HookFunctions.cpp — hook_capture
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

extern void (*CaptureScreen_orig_old)(MonoObject * inst, int userId, long deviceId, int capType, MonoObject* capacityInfo);
extern void (*CaptureScreen_orig_new)(MonoObject * inst, int userId, long deviceId, int capType, MonoString* format, MonoObject* capacityInfo);
extern void (*OnShareButton_orig)(MonoObject* data);

bool CaptureScreen();

bool CaptureScreen(){
  if(g_settings.cheats_shortcut_opt == CHEATS_LONG_SHARE){
    //shellui_log("CaptureScreen: Long Share Shortcut activated");
    GoToURI("OrionHEN?Cheats");
    return true;
  }
  else if (g_settings.toolbox_shortcut_opt == TOOLBOX_LONG_SHARE){
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

  if( g_settings.cheats_shortcut_opt == CHEATS_SINGLE_SHARE) {
    // shellui_log("Share Shortcut: Redirecting to Cheats");
    GoToURI("OrionHEN?Cheats");
    return;
  }
  else if (g_settings.toolbox_shortcut_opt == TOOLBOX_SINGLE_SHARE) {
    // shellui_log("Share Shortcut: Redirecting to Toolbox");
    GoToURI("pssettings:play?mode=settings&function=debug_settings");
    return;
  }

  OnShareButton_orig(data);
}

