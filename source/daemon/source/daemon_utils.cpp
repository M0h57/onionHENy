/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Shared daemon helpers (file/net/app query) — extracted from commands.cpp.
 */

#include "daemon_ops.hpp"
#include "launcher.hpp"

#include <orion/platform.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

extern "C" {
  int sceNetCtlGetInfo(int32_t s, void *b);
  int sceUserServiceGetLoginUserIdList(void *list);
  int sceUserServiceGetUserName(const int userId, char *userName, const size_t size);
  int sceSystemServiceGetAppIdOfRunningBigApp();
  int sceSystemServiceGetAppTitleId(int app_id, char *title_id);
}

namespace {

typedef struct SceNetEtherAddr {
  uint8_t data[6];
} SceNetEtherAddr;

typedef union SceNetCtlInfo {
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

struct UserServiceLoginUserIdList {
  int user_id[4];
};

} // namespace

bool GetFileContents(const char *path, char **buffer) {
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) {
    OrionHEN_log("failed to open %s", path);
    return false;
  }

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  if (size == 0) {
    fclose(fp);
    OrionHEN_log("size is 0");
    return false;
  }

  *buffer = (char *)malloc(size + 1);
  if (*buffer == NULL) {
    OrionHEN_log("failed to allocate memory (OOM)");
    fclose(fp);
    return false;
  }

  if (fread(*buffer, size, 1, fp) != 1) {
    fclose(fp);
    free(*buffer);
    return false;
  }

  fclose(fp);
  (*buffer)[size] = '\0';
  return true;
}

int get_ip_address(char *ip_address) {
  SceNetCtlInfo info{};
  int ret = sceNetCtlGetInfo(14, &info);
  if (ret < 0) {
    memcpy(ip_address, "IP NOT FOUND", sizeof("IP NOT FOUND"));
    return -1;
  }
  memcpy(ip_address, info.ip_address, sizeof(info.ip_address));
  return ret;
}

bool Get_Running_App_TID(std::string &title_id, int &BigAppid) {
  char tid[255];
  BigAppid = sceSystemServiceGetAppIdOfRunningBigApp();
  if (BigAppid < 0)
    return false;

  (void)memset(tid, 0, sizeof tid);
  if (sceSystemServiceGetAppTitleId(BigAppid, &tid[0]) != 0)
    return false;

  title_id = std::string(tid);
  return true;
}

bool isUserLoggedIn() {
  bool isLoggedIn = false;
  UserServiceLoginUserIdList userIdList{};
  (void)memset(&userIdList, 0, sizeof(userIdList));

  if (sceUserServiceGetLoginUserIdList(&userIdList) < 0)
    return false;

  for (int i = 0; i < 4; i++) {
    char username[500] = {0};
    int userid = userIdList.user_id[i];
    if (userid == -1)
      continue;
    int ret = sceUserServiceGetUserName(userid, &username[0], sizeof(username));
    OrionHEN_log("sceUserServiceGetUserName returned %d", ret);
    if (ret == 0) {
      isLoggedIn = true;
      break;
    }
  }

  sleep(5);
  return isLoggedIn;
}

bool Open_Utility_Elf(const char *path, uint8_t **buffer) {
  if (!path || !buffer) {
    OrionHEN_log("Invalid arguments: path or buffer is null.");
    return false;
  }

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    OrionHEN_log("Failed to open file: %s (error: %s)", path, strerror(errno));
    return false;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    OrionHEN_log("Failed to get file stats for %s (error: %s)", path, strerror(errno));
    close(fd);
    return false;
  }

  if (st.st_size == 0) {
    OrionHEN_log("File %s is empty.", path);
    close(fd);
    return false;
  }

  uint8_t *buf = (uint8_t *)malloc((size_t)st.st_size);
  if (!buf) {
    OrionHEN_log("Failed to allocate memory for file %s (size: %ld bytes).", path,
                 st.st_size);
    close(fd);
    return false;
  }

  ssize_t bytes_read = read(fd, buf, (size_t)st.st_size);
  if (bytes_read != st.st_size) {
    OrionHEN_log("Failed to read the entire file %s (read: %ld bytes, expected: %ld bytes).",
                 path, bytes_read, st.st_size);
    free(buf);
    close(fd);
    return false;
  }

  close(fd);
  *buffer = buf;
  return true;
}
