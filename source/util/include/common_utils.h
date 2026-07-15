/* Copyright (C) 2025 OnionHEN / LightningMods

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

#pragma once
#include <stdio.h>
#include <ps5/payload.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <ini.h>
#include <poll.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include "faulthandler.h"
#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stddef.h>
#include <sys/types.h>
#include <ifaddrs.h>

/* Rest-mode loop sleep (was in tcp.h with the removed klog/ftp helpers) */
#define SLEEP_PERIOD 1000000

/*================== Networking =================*/
typedef struct SceNetEtherAddr
{
	uint8_t data[6];
} SceNetEtherAddr;

typedef union SceNetCtlInfo
{
	uint32_t device;
	SceNetEtherAddr ether_addr;
	uint32_t mtu;
	uint32_t link;
	SceNetEtherAddr bssid;
	char ssid[33];
	uint32_t wifi_security;
	int32_t rssi_dbm;
	uint8_t rssi_percentage;
	uint8_t channel;
	uint32_t ip_config;
	char dhcp_hostname[256];
	char pppoe_auth_name[128];
	char ip_address[16];
	char netmask[16];
	char default_route[16];
	char primary_dns[16];
	char secondary_dns[16];
	uint32_t http_proxy_config;
	char http_proxy_server[256];
	uint16_t http_proxy_port;
} SceNetCtlInfo;

int32_t sceNetCtlGetInfo(int32_t s, SceNetCtlInfo *b);
void sceNetCtlTerm(void);
/*================== Networking =================*/

typedef struct notify_request
{
	char useless1[45];
	char message[3075];
} notify_request_t;

/*================ SETTINGS ==============*/
/* Persisted keys: onion::SettingsStore. Concurrent CMD flags stay atomic.
 * Always C++ linkage even when this header is included under extern "C". */
#ifdef __cplusplus
extern "C++" {
#include <onion/settings.hpp>
extern onion::SettingsStore g_settings;
/** Reload twin config into g_settings; missing file applies defaults (true). */
bool LoadSettings();
} /* extern "C++" */
#endif
extern atomic_bool g_legacy_cmd_server;
extern atomic_bool g_legacy_cmd_server_exit;
/*================ SETTINGS ==============*/
/* Payload load helpers (shared onion_payload). */
#include <onion/payload.h>

/* load_payload: see extern "C" block below */

extern atomic_bool g_running;
/*============ Back up JB server ==============*/
enum Commands
{
	INVALID_CMD = -1,
	ACTIVE_CMD = 0,
	LAUNCH_CMD,
	PROCLIST_CMD,
	KILL_CMD,
	KILL_APP_CMD,
	JAILBREAK_CMD,
	REMOUNT_FOLDER_CMD,
	ONIONHEN_VER_CMD,
	PATCH_LNC_DEBUG_CMD,
	TEST_CMD,
	SYMLINK_CMD,
};

typedef struct
{
	int32_t type;			 // 0x00
	int32_t req_id;			 // 0x04
	int32_t priority;		 // 0x08
	int32_t msg_id;			 // 0x0C
	int32_t target_id;		 // 0x10
	int32_t user_id;		 // 0x14
	int32_t unk1;			 // 0x18
	int32_t unk2;			 // 0x1C
	int32_t app_id;			 // 0x20
	int32_t error_num;		 // 0x24
	int32_t unk3;			 // 0x28
	char use_icon_image_uri; // 0x2C
	char message[1024];		 // 0x2D
	char uri[1024];			 // 0x42D
	char unkstr[1024];		 // 0x82D
} OrbisNotificationRequest;	 // Size = 0xC30


#include <onion/lnc.h>

bool copyFile(const char *source, const char *destination);

int32_t sceKernelSendNotificationRequest(int32_t device, OrbisNotificationRequest *req, size_t size, int32_t blocking);

bool IniliatizeHTTP(void);
/* download/extract/shutdown: see extern "C" block below */

/*============ Back up JB server ==============*/
/* Always C linkage (impl in common_utils.c) regardless of include context. */
#ifdef __cplusplus
extern "C" {
#endif
int get_ip_address(char *ip_address);
#include <onion/platform.h>
#include <onion/proc_query.h>
int sceNetCtlInit(void);
int sceUserServiceInitialize(void *ptr);
bool load_payload(const char *path);
bool download_file(const char *url, const char *dst);
bool check_for_new_commit(int repo);
bool extract_zip(const char *zip_path, const char *extract_dir);
#ifdef __cplusplus
}
#endif
