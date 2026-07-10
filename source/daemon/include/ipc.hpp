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

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string>
#include <msg.hpp>

struct clientArgs {
    std::string ip;
    int socket;
    int cl_nmb;

};

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#ifdef __cplusplus
#define restrict // Define restrict as empty for C++
#endif

extern "C"
{
#define ENTRYPOINT_OFFSET 0x70

#define PROCESS_LAUNCHED 1

#define LOOB_BUILDER_SIZE 21
#define LOOP_BUILDER_TARGET_OFFSET 3
#define USLEEP_NID "QcteRwbsnV0"
#include <ps5/kernel.h>

}

/*==================== DPI =========================*/
#define PLAYGOSCENARIOID_SIZE 3
#define CONTENTID_SIZE 0x30
#define LANGUAGE_SIZE 8

typedef char playgo_scenario_id_t[PLAYGOSCENARIOID_SIZE];
typedef char language_t[LANGUAGE_SIZE];
typedef char content_id_t[CONTENTID_SIZE];

typedef struct
{
	content_id_t content_id;
	int content_type;
	int content_platform;
} SceAppInstallPkgInfo;

typedef struct
{
	const char *uri;
	const char *ex_uri;
	const char *playgo_scenario_id;
	const char *content_id;
	const char *content_name;
	const char *icon_url;
} MetaInfo;

#define NUM_LANGUAGES 30
#define NUM_IDS 64

typedef struct {
    language_t languages[NUM_LANGUAGES];
    playgo_scenario_id_t playgo_scenario_ids[NUM_IDS];
    content_id_t content_ids[NUM_IDS];
    long unknown[810];
} PlayGoInfo;

typedef struct {
  uint64_t pad0;
  char version_str[0x1C];
  uint32_t version;
  uint64_t pad1;
} OrbisKernelSwVersion;
extern "C" int sceKernelGetProsperoSystemSwVersion(OrbisKernelSwVersion *sw);

/*==================== DPI =========================*/

extern "C"  int sceAppInstUtilInstallByPackage(MetaInfo *arg1, SceAppInstallPkgInfo *pkg_info, PlayGoInfo *arg2);
extern "C"  int sceAppInstUtilInitialize(void);

void startMessageReceiver();
bool notifyHandlers(const uint32_t prefix, const pid_t pid, const bool isHomebrew) noexcept;
bool hasPrefixHandler(const uint32_t prefix) noexcept;
void* messageThread(void*);
bool GetFileContents(const char *path, char **buffer);
bool touch_file(const char *destfile);
void *IPC_loop(void *args);