/* OnionHEN: elfldr socket launch helpers */

#include "elfldr_remote.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0x20000
#endif

static int connect_port(uint16_t port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(0x7f000001);

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

static bool send_all(int fd, const void *buf, size_t size) {
  const uint8_t *p = (const uint8_t *)buf;
  size_t sent = 0;
  while (sent < size) {
    ssize_t n = send(fd, p + sent, size - sent, MSG_NOSIGNAL);
    if (n <= 0)
      return false;
    sent += (size_t)n;
  }
  return true;
}

bool elfldr_remote_available_on(uint16_t port) {
  int fd = connect_port(port);
  if (fd < 0)
    return false;
  close(fd);
  return true;
}

bool elfldr_remote_onion_available(void) {
  int fd = connect_port(ONION_ELFLDR_PORT);
  if (fd < 0)
    return false;

  struct timeval timeout;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  const char ping[] = ONION_ELFLDR_PING "\n";
  if (!send_all(fd, ping, sizeof(ping) - 1)) {
    close(fd);
    return false;
  }

  char buf[64];
  const ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
  close(fd);
  if (n <= 0)
    return false;
  buf[n] = '\0';

  return strncmp(buf, ONION_ELFLDR_PONG, strlen(ONION_ELFLDR_PONG)) == 0;
}

bool elfldr_remote_available(void) {
  return elfldr_remote_available_on(ELFLDR_REMOTE_PORT);
}

bool elfldr_remote_send_bytes_to(uint16_t port, const uint8_t *elf,
                                 size_t size) {
  if (!elf || size < 4)
    return false;

  int fd = connect_port(port);
  if (fd < 0)
    return false;

  if (!send_all(fd, elf, size)) {
    close(fd);
    return false;
  }
  close(fd);
  return true;
}

bool elfldr_remote_send_bytes(const uint8_t *elf, size_t size) {
  return elfldr_remote_send_bytes_to(ELFLDR_REMOTE_PORT, elf, size);
}

static int send_file_uri(uint16_t port, const char *abs_path) {
  if (!abs_path || abs_path[0] != '/')
    return -1;

  /* socksrv: magic starts with "file" (0x656C6966 LE) then path until \n */
  char line[768];
  int n = snprintf(line, sizeof(line), "file:%s\n", abs_path);
  if (n <= 0 || (size_t)n >= sizeof(line))
    return -1;

  int fd = connect_port(port);
  if (fd < 0)
    return -1;

  if (!send_all(fd, line, (size_t)n)) {
    close(fd);
    return -1;
  }
  return fd;
}

static pid_t read_pid_response(int fd) {
  char buf[64];
  struct timeval timeout;
  timeout.tv_sec = 5;
  timeout.tv_usec = 0;
  (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  const ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
  if (n <= 0)
    return 0;
  buf[n] = '\0';

  if (strncmp(buf, "OK ", 3) != 0)
    return -1;
  const long pid = strtol(buf + 3, NULL, 10);
  return pid > 1 ? (pid_t)pid : 0;
}

bool elfldr_remote_send_file_uri_to(uint16_t port, const char *abs_path) {
  int fd = send_file_uri(port, abs_path);
  if (fd < 0)
    return false;
  close(fd);
  return true;
}

bool elfldr_remote_send_file_uri(const char *abs_path) {
  return elfldr_remote_send_file_uri_to(ELFLDR_REMOTE_PORT, abs_path);
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

bool elfldr_remote_write_and_launch_to(uint16_t port, const char *abs_path,
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

  return elfldr_remote_send_file_uri_to(port, abs_path);
}

pid_t elfldr_remote_write_and_launch_get_pid(uint16_t port,
                                             const char *abs_path,
                                             const uint8_t *elf, size_t size) {
  if (!abs_path || !elf || size < 4)
    return -1;

  mkdir_parent(abs_path);

  int out = open(abs_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if (out < 0)
    return -1;

  size_t off = 0;
  while (off < size) {
    ssize_t w = write(out, elf + off, size - off);
    if (w <= 0) {
      close(out);
      return -1;
    }
    off += (size_t)w;
  }
  close(out);

  int fd = send_file_uri(port, abs_path);
  if (fd < 0)
    return -1;

  const pid_t pid = read_pid_response(fd);
  close(fd);
  return pid;
}

bool elfldr_remote_write_and_launch(const char *abs_path,
                                    const uint8_t *elf, size_t size) {
  return elfldr_remote_write_and_launch_to(ELFLDR_REMOTE_PORT, abs_path, elf,
                                          size);
}

pid_t elfldr_remote_wait_name(const char *name_substr, int timeout_ms) {
  /* Implemented in callers with their find_pid when needed; stub keeps API. */
  (void)name_substr;
  (void)timeout_ms;
  return -1;
}
