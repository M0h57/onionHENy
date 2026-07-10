/* Host unit tests for liborion_plugin pure helpers (no elfldr/9021). */
#include "test_harness.h"
#include "test_support.h"

#include <orion/plugin.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void fill_header(OrionPluginHeader *h, const char *tid,
                        const char *ver) {
  memset(h, 0, sizeof(*h));
  memcpy(h->prefix, "OrionHEN_PLUGIN", 15); /* 14 chars + NUL in field */
  snprintf(h->titleID, sizeof(h->titleID), "%s", tid);
  snprintf(h->plugin_version, sizeof(h->plugin_version), "%s", ver);
}

static int test_is_elf(void) {
  const unsigned char elf[8] = {0x7F, 'E', 'L', 'F', 2, 1, 1, 0};
  const unsigned char junk[4] = {0, 0, 0, 0};

  TEST_ASSERT_TRUE(orion_plugin_is_elf(elf, sizeof(elf)));
  TEST_ASSERT_TRUE(!orion_plugin_is_elf(junk, sizeof(junk)));
  TEST_ASSERT_TRUE(!orion_plugin_is_elf(elf, 3));
  TEST_ASSERT_TRUE(!orion_plugin_is_elf(NULL, 16));
  return 0;
}

static int test_is_valid_ok(void) {
  OrionPluginHeader h;
  fill_header(&h, "CUSA12345", "01.00");
  TEST_ASSERT_TRUE(orion_plugin_is_valid(&h));

  fill_header(&h, "PPSA00001", "99.99");
  TEST_ASSERT_TRUE(orion_plugin_is_valid(&h));
  return 0;
}

static int test_is_valid_bad_prefix(void) {
  OrionPluginHeader h;
  fill_header(&h, "CUSA12345", "01.00");
  memcpy(h.prefix, "NotAPlugin!!!!", 15);
  TEST_ASSERT_TRUE(!orion_plugin_is_valid(&h));
  TEST_ASSERT_TRUE(!orion_plugin_is_valid(NULL));
  return 0;
}

static int test_is_valid_bad_title(void) {
  OrionPluginHeader h;

  fill_header(&h, "cusa12345", "01.00"); /* lowercase */
  TEST_ASSERT_TRUE(!orion_plugin_is_valid(&h));

  fill_header(&h, "CUSA12ABC", "01.00"); /* letters in digit part */
  TEST_ASSERT_TRUE(!orion_plugin_is_valid(&h));

  fill_header(&h, "CU1A12345", "01.00"); /* digit in letter part */
  TEST_ASSERT_TRUE(!orion_plugin_is_valid(&h));

  fill_header(&h, "CUSA1234", "01.00"); /* too short — last digit missing,
                                           titleID[8] may be 0 from snprintf */
  /* "CUSA1234" is 8 chars + NUL at [8]; [8] is NUL not digit → fail */
  TEST_ASSERT_TRUE(!orion_plugin_is_valid(&h));
  return 0;
}

static int test_package_elf_offset(void) {
  unsigned char buf[sizeof(OrionPluginHeader) + 8];
  OrionPluginHeader *h = (OrionPluginHeader *)buf;

  fill_header(h, "CUSA12345", "01.00");
  buf[sizeof(OrionPluginHeader) + 0] = 0x7F;
  buf[sizeof(OrionPluginHeader) + 1] = 'E';

  const uint8_t *elf = orion_plugin_package_elf(buf);
  TEST_ASSERT_TRUE(elf == buf + sizeof(OrionPluginHeader));
  TEST_ASSERT_EQ_INT(0x7F, elf[0]);
  return 0;
}

static int test_pid_path(void) {
  char path[128];
  orion_plugin_pid_path(path, sizeof(path), "CUSA12345");
  TEST_ASSERT_STREQ("/system_tmp/CUSA12345.PID", path);
  return 0;
}

static int test_pid_file_roundtrip(void) {
  char path[256];
  TEST_ASSERT_EQ_INT(0, orion_test_write_temp_file(".PID", "", 0, path,
                                                   sizeof(path)));

  orion_plugin_write_pid_file(path, 4242);
  TEST_ASSERT_EQ_INT(4242, (int)orion_plugin_read_pid_file(path));

  orion_plugin_write_pid_file(path, -1); /* unlink when pid < 0 */
  TEST_ASSERT_EQ_INT(-1, (int)orion_plugin_read_pid_file(path));

  /* missing file */
  TEST_ASSERT_EQ_INT(-1, (int)orion_plugin_read_pid_file(
                             "/tmp/orion-plugin-pid-missing-xyz.PID"));
  orion_test_remove_file(path);
  return 0;
}

static int test_read_file(void) {
  char path[256];
  const char payload[] = "hello-plugin";
  size_t sz = 0;
  uint8_t *buf = NULL;

  TEST_ASSERT_EQ_INT(0, orion_test_write_temp_file(".bin", payload,
                                                   sizeof(payload) - 1, path,
                                                   sizeof(path)));
  buf = orion_plugin_read_file(path, &sz);
  TEST_ASSERT_TRUE(buf != NULL);
  TEST_ASSERT_EQ_U64(sizeof(payload) - 1, sz);
  TEST_ASSERT_MEMEQ(payload, buf, sizeof(payload) - 1);
  free(buf);

  TEST_ASSERT_TRUE(orion_plugin_read_file(NULL, &sz) == NULL);
  TEST_ASSERT_TRUE(orion_plugin_read_file(path, NULL) == NULL);
  TEST_ASSERT_TRUE(orion_plugin_read_file("/tmp/orion-missing-plugin-xyz",
                                          &sz) == NULL);

  /* empty file rejected */
  TEST_ASSERT_EQ_INT(0, orion_test_write_temp_file(".bin", "", 0, path,
                                                   sizeof(path)));
  TEST_ASSERT_TRUE(orion_plugin_read_file(path, &sz) == NULL);
  orion_test_remove_file(path);
  return 0;
}

int test_plugin_suite(void) {
  int failures = 0;
  failures += orion_test_run("plugin.is_elf", test_is_elf);
  failures += orion_test_run("plugin.is_valid_ok", test_is_valid_ok);
  failures += orion_test_run("plugin.is_valid_bad_prefix",
                             test_is_valid_bad_prefix);
  failures += orion_test_run("plugin.is_valid_bad_title",
                             test_is_valid_bad_title);
  failures += orion_test_run("plugin.package_elf_offset",
                             test_package_elf_offset);
  failures += orion_test_run("plugin.pid_path", test_pid_path);
  failures += orion_test_run("plugin.pid_file_roundtrip",
                             test_pid_file_roundtrip);
  failures += orion_test_run("plugin.read_file", test_read_file);
  return failures;
}
