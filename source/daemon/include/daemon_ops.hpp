/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Daemon domain ops — settings, FS, inject. msg.cpp only owns IPC_loop.
 */
#pragma once

#include <string>
#include <sys/types.h>
#include <msg.hpp>
#include <orion/ipc_server.hpp>

// clientArgs alias
using clientArgs = orion::IpcClientArgs;

extern bool is_handler_enabled;

/**
 * Refresh g_settings from twin config paths when either is newer.
 * Returns true if store is usable (including defaults / skip-if-current).
 * Missing config is not an error.
 */
bool LoadSettings();

/** After settings_save from this process, refresh mtime gate so we don't thrash. */
void SettingsNoteDiskWritten();

void reply(int sender_socket, bool error, std::string out_var = "Nothing");
/** Last reply error for BREW_LAST_RET (0 = success, -1 = error). */
int daemon_last_ipc_error();
bool remount(const char *dev, const char *path, int mnt_flag);
int change_permissions_recursive(const char *path);
bool test_sb_file(const char *filename);
int get_shellui_pid();
int get_game_pid();
void ForceKillProc(int pid);
bool set_fan_threshold(int temp);

bool cmd_enable_toolbox();
bool cmd_enable_fps(int appid);
bool cmd_enable_fps_new(int appid);

void *IPC_loop(void *args);
void handleIPC(clientArgs *client, std::string &inputStr, DaemonCommands command);

/* ---- shared helpers (daemon_utils.cpp) ---- */
bool GetFileContents(const char *path, char **buffer);
int get_ip_address(char *ip_address);
bool Get_Running_App_TID(std::string &title_id, int &BigAppid);
bool isUserLoggedIn();
bool Open_Utility_Elf(const char *path, uint8_t **buffer);

/* ---- background threads ---- */
void *Play_time_thread(void *args) noexcept;       // daemon_playtime.cpp
void *fifo_and_dumper_thread(void *args) noexcept; // daemon_jailbreak.cpp
