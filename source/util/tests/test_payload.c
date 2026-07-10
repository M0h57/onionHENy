/* Host unit tests for liborion_payload payload helpers (no elfldr/9021). */
#include "test_harness.h"
#include "test_support.h"

#include <orion/payload.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int test_is_elf(void) {
  const unsigned char elf[8] = {0x7F, 'E', 'L', 'F', 2, 1, 1, 0};
  const unsigned char junk[4] = {0, 0, 0, 0};

  TEST_ASSERT_TRUE(orion_payload_is_elf(elf, sizeof(elf)));
  TEST_ASSERT_TRUE(!orion_payload_is_elf(junk, sizeof(junk)));
  TEST_ASSERT_TRUE(!orion_payload_is_elf(elf, 3));
  TEST_ASSERT_TRUE(!orion_payload_is_elf(NULL, 16));
  return 0;
}

static int test_pid_path(void) {
  char path[128];
  orion_payload_pid_path(path, sizeof(path), "mytool");
  TEST_ASSERT_STREQ("/system_tmp/mytool.PID", path);
  return 0;
}

static int test_elf_key_from_name(void) {
  char key[64];
  TEST_ASSERT_TRUE(orion_payload_elf_key_from_name("foo.elf", key, sizeof(key)));
  TEST_ASSERT_STREQ("foo", key);
  TEST_ASSERT_TRUE(
      orion_payload_elf_key_from_name("/data/OrionHEN/payloads/bar.elf", key,
                                     sizeof(key)));
  TEST_ASSERT_STREQ("bar", key);
  TEST_ASSERT_TRUE(orion_payload_elf_key_from_name("noext", key, sizeof(key)));
  TEST_ASSERT_STREQ("noext", key);
  TEST_ASSERT_TRUE(!orion_payload_elf_key_from_name(".elf", key, sizeof(key)));
  TEST_ASSERT_TRUE(!orion_payload_elf_key_from_name(NULL, key, sizeof(key)));
  TEST_ASSERT_TRUE(!orion_payload_elf_key_from_name("", key, sizeof(key)));
  return 0;
}

static int test_pid_file_roundtrip(void) {
  char path[256];
  TEST_ASSERT_EQ_INT(0, orion_test_write_temp_file(".PID", "", 0, path,
                                                   sizeof(path)));

  orion_payload_write_pid_file(path, 4242);
  TEST_ASSERT_EQ_INT(4242, (int)orion_payload_read_pid_file(path));

  orion_payload_write_pid_file(path, -1);
  TEST_ASSERT_EQ_INT(-1, (int)orion_payload_read_pid_file(path));

  TEST_ASSERT_EQ_INT(-1, (int)orion_payload_read_pid_file(
                             "/tmp/orion-payload-pid-missing-xyz.PID"));
  orion_test_remove_file(path);
  return 0;
}

static int test_read_file(void) {
  char path[256];
  const char payload[] = "hello-payload";
  size_t sz = 0;
  uint8_t *buf = NULL;

  TEST_ASSERT_EQ_INT(0, orion_test_write_temp_file(".bin", payload,
                                                   sizeof(payload) - 1, path,
                                                   sizeof(path)));
  buf = orion_payload_read_file(path, &sz);
  TEST_ASSERT_TRUE(buf != NULL);
  TEST_ASSERT_EQ_U64(sizeof(payload) - 1, sz);
  TEST_ASSERT_MEMEQ(payload, buf, sizeof(payload) - 1);
  free(buf);

  TEST_ASSERT_TRUE(orion_payload_read_file(NULL, &sz) == NULL);
  TEST_ASSERT_TRUE(orion_payload_read_file(path, NULL) == NULL);
  TEST_ASSERT_TRUE(orion_payload_read_file("/tmp/orion-missing-payload-xyz",
                                          &sz) == NULL);

  TEST_ASSERT_EQ_INT(0, orion_test_write_temp_file(".bin", "", 0, path,
                                                   sizeof(path)));
  TEST_ASSERT_TRUE(orion_payload_read_file(path, &sz) == NULL);
  orion_test_remove_file(path);
  return 0;
}

int test_payload_suite(void) {
  int failures = 0;
  failures += orion_test_run("payload.is_elf", test_is_elf);
  failures += orion_test_run("payload.pid_path", test_pid_path);
  failures += orion_test_run("payload.elf_key_from_name", test_elf_key_from_name);
  failures += orion_test_run("payload.pid_file_roundtrip",
                             test_pid_file_roundtrip);
  failures += orion_test_run("payload.read_file", test_read_file);
  return failures;
}
