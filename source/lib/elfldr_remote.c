/* OrionHEN: external elfldr (9021) launch helpers */

#include "elfldr_remote.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0x20000
#endif

static int connect_9021(void) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(ELFLDR_REMOTE_PORT);
  addr.sin_addr.s_addr = htonl(0x7f000001);

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

bool elfldr_remote_available(void) {
  int fd = connect_9021();
  if (fd < 0)
    return false;
  close(fd);
  return true;
}

bool elfldr_remote_send_bytes(const uint8_t *elf, size_t size) {
  if (!elf || size < 4)
    return false;

  int fd = connect_9021();
  if (fd < 0)
    return false;

  size_t sent = 0;
  while (sent < size) {
    ssize_t n = send(fd, elf + sent, size - sent, MSG_NOSIGNAL);
    if (n <= 0) {
      close(fd);
      return false;
    }
    sent += (size_t)n;
  }
  close(fd);
  return true;
}

bool elfldr_remote_send_file_uri(const char *abs_path) {
  if (!abs_path || abs_path[0] != '/')
    return false;

  /* socksrv: magic starts with "file" (0x656C6966 LE) then path until \n */
  char line[768];
  int n = snprintf(line, sizeof(line), "file:%s\n", abs_path);
  if (n <= 0 || (size_t)n >= sizeof(line))
    return false;

  int fd = connect_9021();
  if (fd < 0)
    return false;

  size_t total = (size_t)n;
  size_t sent = 0;
  while (sent < total) {
    ssize_t w = send(fd, line + sent, total - sent, MSG_NOSIGNAL);
    if (w <= 0) {
      close(fd);
      return false;
    }
    sent += (size_t)w;
  }
  close(fd);
  return true;
}

static void mkdir_parent(const char *abs_path) {
  char tmp[512];
  snprintf(tmp, sizeof(tmp), "%s", abs_path);
  char *slash = strrchr(tmp, '/');
  if (!slash || slash == tmp)
    return;
  *slash = '\0';
  mkdir(tmp, 0777);
}

bool elfldr_remote_write_and_launch(const char *abs_path,
                                    const uint8_t *elf, size_t size) {
  if (!abs_path || !elf || size < 4)
    return false;

  mkdir_parent(abs_path);

  int out = open(abs_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if (out < 0)
    return false;

  size_t off = 0;
  while (off < size) {
    ssize_t w = write(out, elf + off, size - off);
    if (w <= 0) {
      close(out);
      return false;
    }
    off += (size_t)w;
  }
  close(out);

  return elfldr_remote_send_file_uri(abs_path);
}

pid_t elfldr_remote_wait_name(const char *name_substr, int timeout_ms) {
  /* Implemented in callers with their find_pid when needed; stub keeps API. */
  (void)name_substr;
  (void)timeout_ms;
  return -1;
}
