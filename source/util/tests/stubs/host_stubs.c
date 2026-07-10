/*
 * Host stubs for util unit tests (no PS5 SDK).
 *
 * Production OrionHEN_log / platform helpers are linked from
 * liborion_platform when ORION_HOST_TEST builds include them.
 * This file only supplies symbols that are PS5-runtime-only.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

/* --- liborion_plugin / elfldr_remote (device-only launch path) --- */

bool elfldr_remote_available(void) { return false; }

bool elfldr_remote_send_bytes(const uint8_t *elf, size_t size) {
  (void)elf;
  (void)size;
  return false;
}

bool elfldr_remote_send_file_uri(const char *abs_path) {
  (void)abs_path;
  return false;
}

bool elfldr_remote_write_and_launch(const char *abs_path, const uint8_t *elf,
                                    size_t size) {
  (void)abs_path;
  (void)elf;
  (void)size;
  return false;
}

pid_t find_pid(const char *name) {
  (void)name;
  return -1;
}

pid_t orion_find_pid(const char *name) {
  (void)name;
  return -1;
}

pid_t orion_find_pid_substr(const char *substr) {
  (void)substr;
  return -1;
}

int sceKernelGetProcessName(int pid, char *name) {
  (void)pid;
  if (name)
    name[0] = '\0';
  return -1;
}

/* PS5 klog sink — silence unless verbose. */
void klog_printf(const char *fmt, ...) {
  va_list args;
  if (getenv("ORION_TEST_VERBOSE") == NULL) {
    return;
  }
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fputc('\n', stderr);
}

/* Notification hardware — no-op on host. */
int32_t sceKernelSendNotificationRequest(int32_t device, void *req, size_t size,
                                         int32_t blocking) {
  (void)device;
  (void)req;
  (void)size;
  (void)blocking;
  return 0;
}

/* Fallback logger if a TU is compiled without liborion_platform log.c.
 * When log.c is linked, that definition wins if this is weak — but most
 * linkers take the first definition. Prefer always linking log.c and not
 * defining OrionHEN_log here.
 */

int util_file_read_alloc(const char *path, char **buf_out, size_t *size_out,
                         size_t max_size) {
  FILE *fp = NULL;
  long file_size = 0;
  char *buf = NULL;
  size_t read_size = 0;

  if (path == NULL || buf_out == NULL) {
    return -1;
  }
  *buf_out = NULL;
  if (size_out != NULL) {
    *size_out = 0;
  }
  if (max_size == 0) {
    max_size = 1024u * 1024u;
  }

  fp = fopen(path, "rb");
  if (fp == NULL) {
    return -1;
  }
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return -1;
  }
  file_size = ftell(fp);
  if (file_size <= 0 ||
      (max_size != (size_t)-1 && (size_t)file_size > max_size)) {
    fclose(fp);
    return -1;
  }
  if (fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    return -1;
  }

  buf = (char *)malloc((size_t)file_size + 1);
  if (buf == NULL) {
    fclose(fp);
    return -1;
  }
  read_size = fread(buf, 1, (size_t)file_size, fp);
  fclose(fp);
  if (read_size != (size_t)file_size) {
    free(buf);
    return -1;
  }
  buf[file_size] = '\0';
  *buf_out = buf;
  if (size_out != NULL) {
    *size_out = (size_t)file_size;
  }
  return 0;
}
