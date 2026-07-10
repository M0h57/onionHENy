#include "test_support.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mc4/aes.h"
#include "mc4/base64.h"

static const uint8_t MC4_AES256CBC_KEY[] = "304c6528f659c766110239a51cl5dd9c";
static const uint8_t MC4_AES256CBC_IV[] = "u@}kzW2u[u(8DWar";

int orion_test_write_temp_file(const char *suffix, const void *data, size_t len,
                               char *path_out, size_t path_out_size) {
  size_t suffix_len = 0;
  int fd = -1;

  if (path_out == NULL || path_out_size == 0 || suffix == NULL) {
    return -1;
  }
  suffix_len = strlen(suffix);
  if (path_out_size <= sizeof("/tmp/orion-test-XXXXXX") + suffix_len) {
    return -1;
  }

  snprintf(path_out, path_out_size, "/tmp/orion-test-XXXXXX%s", suffix);
  fd = mkstemps(path_out, (int)suffix_len);
  if (fd < 0) {
    return -1;
  }
  if (len > 0 && write(fd, data, len) != (ssize_t)len) {
    close(fd);
    unlink(path_out);
    return -1;
  }
  close(fd);
  return 0;
}

int orion_test_write_temp_text_file(const char *suffix, const char *text,
                                    char *path_out, size_t path_out_size) {
  if (text == NULL) {
    return -1;
  }
  return orion_test_write_temp_file(suffix, text, strlen(text), path_out,
                                    path_out_size);
}

int orion_test_write_temp_mc4_file(const char *xml, char *path_out,
                                   size_t path_out_size) {
  size_t plain_len = 0;
  size_t padded_len = 0;
  uint8_t *cipher = NULL;
  unsigned char *encoded = NULL;
  size_t encoded_len = 0;
  struct AES_ctx ctx;
  int rc = -1;

  if (xml == NULL) {
    return -1;
  }
  plain_len = strlen(xml);
  padded_len = ((plain_len + 15) / 16) * 16;
  cipher = (uint8_t *)calloc(padded_len, 1);
  if (cipher == NULL) {
    return -1;
  }
  memcpy(cipher, xml, plain_len);

  AES_init_ctx_iv(&ctx, MC4_AES256CBC_KEY, MC4_AES256CBC_IV);
  AES_CBC_encrypt_buffer(&ctx, cipher, padded_len);

  encoded = base64_encode(cipher, padded_len, &encoded_len);
  if (encoded == NULL) {
    free(cipher);
    return -1;
  }

  rc = orion_test_write_temp_file(".mc4", encoded, encoded_len, path_out,
                                  path_out_size);
  free(encoded);
  free(cipher);
  return rc;
}

void orion_test_remove_file(const char *path) {
  if (path != NULL && path[0] != '\0') {
    unlink(path);
  }
}

int orion_test_fixture_path(const char *rel, char *out, size_t out_size) {
  const char *root = getenv("ORION_TEST_ROOT");
  if (rel == NULL || out == NULL || out_size == 0) {
    return -1;
  }
  if (root != NULL && root[0] != '\0') {
    snprintf(out, out_size, "%s/%s", root, rel);
  } else {
    /* Default: run from source/util/tests */
    snprintf(out, out_size, "%s", rel);
  }
  return access(out, R_OK) == 0 ? 0 : -1;
}
