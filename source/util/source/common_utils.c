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

#include "common_utils.h"
#include <signal.h>
#include <unistd.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <freebsd-helper.h>
#include <libgen.h>
#include <ps5/klog.h>
#include "pt.h"
#include <orion/platform.h>
#include <orion/proc_query.h>
#include <orion/plugin.h>

typedef struct app_info {
  uint32_t app_id;
  uint64_t unknown1;
  uint32_t app_type;
  char     title_id[10];
  char     unknown2[0x3c];
} app_info_t;

int launchApp(const char *titleId);
int sceSystemServiceGetAppId(const char *title_id);
void free(void *ptr);
void *malloc(size_t size);
int elfldr_set_procname(pid_t pid, const char* name);

int sceKernelGetProcessName(int pid, char *name);
int sceKernelGetAppInfo(int pid, app_info_t *title);
atomic_bool not_connected = false;

#define SCE_NET_CTL_ERROR_NOT_CONNECTED 0x80412108
#define SCE_NET_CTL_ERROR_NOT_AVAIL 0x80412109

/* OrionHEN: no local spawn — plugins launch via external elfldr :9021. */

 static int
     sys_ptrace(int request, pid_t pid, caddr_t addr, int data) {
     pid_t mypid = getpid();
     uint64_t authid;
     int ret;

     if (!(authid = kernel_get_ucred_authid(mypid))) {
         return -1;
     }
     if (kernel_set_ucred_authid(mypid, 0x4800000000010003l)) {
         return -1;
     }

     ret = (int)syscall(SYS_ptrace, request, pid, addr, data);

     if (kernel_set_ucred_authid(mypid, authid)) {
         return -1;
     }

     return ret;
 }


 int pt_detach_proc(pid_t pid, int sig) {
     if (sys_ptrace(PT_DETACH, pid, 0, sig) == -1) {
         return -1;
     }

     return 0;
 }

 int pt_attach_proc(pid_t pid) {
     if (sys_ptrace(PT_ATTACH, pid, 0, 0) == -1) {
         return -1;
     }

     if (waitpid(pid, 0, 0) == -1) {
         return -1;
     }

     return 0;
 }

int get_ip_address(char *ip_address)
{
	unsigned int ret = 0;
	SceNetCtlInfo info;

	ret = sceNetCtlGetInfo(14, &info);
	if (ret < 0){
		if(ret == SCE_NET_CTL_ERROR_NOT_CONNECTED || ret == SCE_NET_CTL_ERROR_NOT_AVAIL){
			not_connected = true;
		}
		goto error;
	}

	memcpy(ip_address, info.ip_address, sizeof(info.ip_address));

	return ret;

error:
	memcpy(ip_address, "IP NOT FOUND", sizeof(info.ip_address));
	return -1;
}





bool copyFile(const char *source, const char *destination)
{

    FILE *src = fopen(source, "rb");
    if (src == NULL)
    {
        orion_notify(false, "copyFile failed for %s", source);
        OrionHEN_log("copyFile failed for %s", source);
        return false;
    }

    FILE *dest = fopen(destination, "wb");
    if (dest == NULL)
    {
        orion_notify(false, "copyFile failed for %s", destination);
        OrionHEN_log("copyFile failed for %s", destination);
        fclose(src);
        return false;
    }

    char buffer[1024];
    size_t bytes = 0;

    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0)
    {
        fwrite(buffer, 1, bytes, dest);
    }

    fclose(src);
    fclose(dest);

    return true;
}



void make_hb_elf(const char *tid, const void *start, const unsigned int size) {
  char path[1024];
  snprintf(path, sizeof(path), "/system_ex/app/%s/homebrew.elf", tid);
  FILE *fp = fopen(path, "wb+");
  if (fp == NULL) {
    perror("open failed");
    return;
  }
  fwrite(start, 1, size, fp);
  fclose(fp);
}


static bool mkdir_if_necessary(const char *path) {
  if (mkdir(path, 0777) == -1) {
    const int err = errno;
    if (err != EEXIST) {
      perror("mkdir failed");
      return false;
    }
  }
  return true;
}

bool make_plugin_app(const char *tid, const void *start,
                     const unsigned int size)
{
  // REDIS->NPXS40028
  char sys_app[255];
  static const char *json = "{\n"
                            "    \"applicationCategoryType\": 33554432,\n"
                            "    \"localizedParameters\": {\n"
                            "        \"defaultLanguage\": \"en-US\",\n"
                            "        \"en-US\": {\n"
                            "            \"titleName\": \"OrionHEN Plugin\"\n"
                            "        }\n"
                            "    },\n"
                            "    \"titleId\": \"%s"
                            "\"\n"
                            "}\n";
  snprintf(sys_app, sizeof(sys_app), "/system_ex/app/%s", tid);
  if (mkdir(sys_app, 0777) == -1)
  {
    const int err = errno;
    if (err != EEXIST)
    {
      perror("make_plugin_app mkdir /system_ex/app/");
      return false;
    }
    make_hb_elf(tid, start, size);
    return true;
  }
  make_hb_elf(tid, start, size);
  (void)memset(sys_app, 0, sizeof(sys_app));
  snprintf(sys_app, sizeof(sys_app), "/system_ex/app/%s/eboot.bin", tid);
  if (!copyFile("/system_ex/app/NPXS40028/eboot.bin", sys_app))
  {
    puts("failed to copy redis eboot.bin");
    return false;
  }
  (void)memset(sys_app, 0, sizeof(sys_app));
  snprintf(sys_app, sizeof(sys_app), "/system_ex/app/%s/sce_sys", tid);
  if (!mkdir_if_necessary(sys_app))
  {
    return false;
  }
  (void)memset(sys_app, 0, sizeof(sys_app));
  snprintf(sys_app, sizeof(sys_app), "/system_ex/app/%s/sce_sys/param.json",
           tid);
  FILE *fp = fopen(sys_app, "w+");
  if (fp == NULL)
  {
    perror("open failed");
    return false;
  }
  (void)memset(sys_app, 0, sizeof(sys_app));
  snprintf(sys_app, sizeof(sys_app), json, tid);
  fwrite(sys_app, 1, __builtin_strlen(sys_app), fp);
  fclose(fp);

  return true;
}

static void util_prepare_plugin_app(const char *title_id, const uint8_t *elf,
                                    size_t elf_sz, void *user) {
  (void)user;
  make_plugin_app(title_id, elf, elf_sz);
}

bool load_plugin(const char *path) {
  orion_plugin_load_opts opts = {
      .prepare_package = util_prepare_plugin_app,
      .prepare_user = NULL,
      .auto_delete_eorr37000 = 0,
      .always_succeed_after_launch = 0,
  };
  return orion_plugin_load(path, /*filename=*/NULL, &opts);
}

int launchApp(const char *titleId)
{
	int id = 0;

	uint32_t res = sceUserServiceGetForegroundUser(&id);
	if (res != 0)
	{
		printf("sceUserServiceGetForegroundUser failed: 0x%x\n", res);
		return res;
	}
	OrionHEN_log("[LA] user id %u", id);

    LncAppParam param;
	param.sz = sizeof(LncAppParam);
	param.user_id = id;
	param.app_opt = 0;
	param.crash_report = 0;
	param.check_flag = Flag_None;


	puts("calling sceLncUtilLaunchApp");
	int err = sceLncUtilLaunchApp(titleId, NULL, &param);
	OrionHEN_log("sceLncUtilLaunchApp returned 0x%x", (uint32_t)err);
	if (err >= 0)
	{
		return err;
	}
	switch ((uint32_t)err)
	{
	case SCE_LNC_UTIL_ERROR_ALREADY_RUNNING:
		OrionHEN_log("app %s is already running", titleId);
		break;
	case SCE_LNC_ERROR_APP_NOT_FOUND:
		OrionHEN_log("app %s not found", titleId);
		orion_notify(true, "app %s not found", titleId);
		break;
	default:
		OrionHEN_log("[LA] unknown error 0x%x", (uint32_t)err);
		// orion_notify(true, "unknown error 0x%llx", (uint32_t)err);
		break;
	}
	return err;
}
