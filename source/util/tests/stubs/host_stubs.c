/*
 * Host stubs for util cheat parser tests (no PS5 SDK).
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void OrionHEN_log(const char *fmt, ...) {
  va_list args;
  if (getenv("ORION_TEST_VERBOSE") == NULL) {
    return;
  }
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fputc('\n', stderr);
}

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
