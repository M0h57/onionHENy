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
#include <elfldr_remote.h>
#include <orion/platform.h>
#include <orion/proc_query.h>

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


uint8_t *get_elf_header_address(unsigned char *file_buffer)
{
	// The ELF header should start right after the custom plugin header
	return file_buffer + sizeof(CustomPluginHeader);
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

bool is_valid_plugin(const unsigned char *file_buffer)
{
  // Check if the prefix matches
  if (strncmp((const char *)file_buffer, "OrionHEN_PLUGIN", 14) != 0)
  {
    puts("Plugin header prefix does not match");
    return false;
  }

  // Validate the title ID format (4 uppercase letters followed by 4 numbers)
  const CustomPluginHeader *header = (const CustomPluginHeader *)file_buffer;
  for (int i = 0; i < 4; ++i)
  {
    if (header->titleID[i] < 'A' || header->titleID[i] > 'Z')
    {
      puts("Invalid plugin file: titleID must contain 4 uppercase letters as the start");
      return false;
    }
  }
  for (int i = 4; i < 9; ++i)
  {
    if (header->titleID[i] < '0' || header->titleID[i] > '9')
    {
      puts("Invalid plugin file: titleID must contain 5 numbers as the end");
      return false;
    }
  }

  // Ensure the title ID is null-terminated
  if (header->titleID[9] != '\0')
  {
    puts("Invalid plugin file: titleID must be null-terminated");
    return false;
  }

  for (int i = 0; i < 3; ++i)
  {
    if (header->plugin_version[i] == '.')
    {
      continue;
    }
    else if (header->plugin_version[i] < '0' || header->plugin_version[i] > '9')
    {
      puts("Invalid plugin file: version must be in the following format xx.xx");
      return false;
    }
  }

  return true;
}



bool is_elf_file(const void *buffer, size_t size)
{
  if (size < 4)
    return false;

  const unsigned char elf_magic[] = {0x7F, 'E', 'L', 'F'};
  return memcmp(buffer, elf_magic, 4) == 0;
}

static void plugin_pid_path(char *out, size_t out_sz, const char *title_id)
{
  snprintf(out, out_sz, "/system_tmp/%s.PID", title_id);
}

static pid_t read_plugin_pid(const char *pid_path)
{
  int f = open(pid_path, O_RDONLY);
  if (f < 0)
    return -1;

  char t[32];
  int r = read(f, t, sizeof(t) - 1);
  close(f);
  if (r <= 0)
    return -1;

  t[r] = '\0';
  return (pid_t)atoi(t);
}

static void write_plugin_pid(const char *pid_path, pid_t pid)
{
  int f = open(pid_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (f < 0)
    return;

  if (pid >= 0) {
    char t[32];
    int len = snprintf(t, sizeof(t), "%d", pid);
    write(f, t, len);
  } else {
    unlink(pid_path);
  }
  close(f);
}

static void stop_running_plugin(const char *pid_path, const char *title_id)
{
  pid_t pid = read_plugin_pid(pid_path);
  if (pid > 0) {
    char name[32];
    if (sceKernelGetProcessName(pid, name) < 0) {
      OrionHEN_log("Stale plugin PID file detected for %s, removing", title_id);
      unlink(pid_path);
      pid = -1;
    }
  }

  if (pid > 0) {
    OrionHEN_log("killing pid %d (plugin: %s)", pid, title_id);
    kill(pid, SIGKILL);
    unlink(pid_path);
  }
}

/** Returns pid (>=0) on success, -1 on failure. Unknown live pid → 1. */
static pid_t launch_plugin_via_9021(const char *title_id, const uint8_t *elf,
                                    size_t elf_sz, const char *log_label)
{
  char epath[256];
  snprintf(epath, sizeof(epath), "/data/OrionHEN/plugins/%s.elf", title_id);
  OrionHEN_log("loading %s via 9021 %s", log_label, title_id);

  if (!elfldr_remote_write_and_launch(epath, elf, elf_sz)) {
    OrionHEN_log("  Failed 9021 launch");
    return -1;
  }

  sleep(2);
  char nbuf[64];
  snprintf(nbuf, sizeof(nbuf), "%s.elf", title_id);
  pid_t pid = find_pid(nbuf);
  if (pid < 0)
    pid = find_pid(title_id);
  if (pid < 0)
    pid = 1;
  OrionHEN_log("  Launched via 9021!");
  return pid;
}

static uint8_t *read_file_all(const char *path, size_t *out_size)
{
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    OrionHEN_log("Failed to open file, %s (error %s)", path, strerror(errno));
    return NULL;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    OrionHEN_log("Failed to get file stats");
    close(fd);
    return NULL;
  }

  uint8_t *buf = (uint8_t *)malloc(st.st_size);
  if (!buf) {
    OrionHEN_log("Failed to allocate memory for Plugin file");
    close(fd);
    return NULL;
  }

  if (read(fd, buf, st.st_size) != st.st_size) {
    OrionHEN_log("Failed to read Plugin file");
    free(buf);
    close(fd);
    return NULL;
  }
  close(fd);
  *out_size = (size_t)st.st_size;
  return buf;
}

bool load_plugin(const char *path)
{
  size_t size = 0;
  uint8_t *buf = read_file_all(path, &size);
  if (!buf)
    return false;

  const CustomPluginHeader *header = (const CustomPluginHeader *)buf;
  const char *filename = basename((char *)path);
  char pid_path[256];
  plugin_pid_path(pid_path, sizeof(pid_path), header->titleID);

  /* ---- raw .elf payload ---- */
  if (strstr(filename, ".elf") != NULL) {
    OrionHEN_log("ELF detected: %s", filename);
    if (!is_elf_file(buf, size)) {
      OrionHEN_log("Invalid ELF file.");
      orion_notify(true, "Invalid ELF file: %s", filename);
      free(buf);
      return false;
    }

    stop_running_plugin(pid_path, header->titleID);
    pid_t pid = launch_plugin_via_9021(header->titleID, buf, size, filename);
    free(buf);
    write_plugin_pid(pid_path, pid);
    return (pid >= 0);
  }

  /* ---- .plugin package ---- */
  if (!is_valid_plugin(buf)) {
    OrionHEN_log("Invalid plugin file.");
    free(buf);
    return false;
  }

  OrionHEN_log("============== Plugin info ===============");
  OrionHEN_log("Plugin Prefix: %s", header->prefix);
  OrionHEN_log("Plugin TitleID: %s", header->titleID);
  OrionHEN_log("Plugin Version: %s", header->plugin_version);
  OrionHEN_log("=========================================");

  stop_running_plugin(pid_path, header->titleID);

  uint8_t *elf = get_elf_header_address(buf);
  size_t elf_sz = size - sizeof(CustomPluginHeader);
  make_plugin_app(header->titleID, elf, elf_sz);

  pid_t pid = launch_plugin_via_9021(header->titleID, elf, elf_sz, path);
  write_plugin_pid(pid_path, pid);
  free(buf);
  return (pid >= 0);
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
