/* Host tests for binary playtime store. */
#include "test_harness.h"
#include "test_support.h"

#include <orion/playtime.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int make_temp_path(char *path, size_t n) {
  snprintf(path, n, "/tmp/orion-playtime-XXXXXX");
  int fd = mkstemp(path);
  if (fd < 0)
    return -1;
  close(fd);
  unlink(path); /* store recreates */
  return 0;
}

static int test_write_and_get(void) {
  char path[256];
  uint64_t dur = 42;
  TEST_ASSERT_EQ_INT(0, make_temp_path(path, sizeof(path)));

  TEST_ASSERT_TRUE(
      orion_playtime_write_record(path, "CUSA12345", 42));
  dur = 0;
  TEST_ASSERT_TRUE(orion_playtime_get_duration(path, "CUSA12345", &dur));
  TEST_ASSERT_EQ_U64(42, dur);

  unlink(path);
  return 0;
}

static int test_modify(void) {
  char path[256];
  uint64_t dur = 1;
  TEST_ASSERT_EQ_INT(0, make_temp_path(path, sizeof(path)));

  TEST_ASSERT_TRUE(orion_playtime_write_record(path, "PPSA00001", 1));
  TEST_ASSERT_TRUE(orion_playtime_modify_duration(path, "PPSA00001", 99));
  TEST_ASSERT_TRUE(orion_playtime_get_duration(path, "PPSA00001", &dur));
  TEST_ASSERT_EQ_U64(99, dur);

  unlink(path);
  return 0;
}

static int test_multi_tid(void) {
  char path[256];
  uint64_t a = 0, b = 0;
  TEST_ASSERT_EQ_INT(0, make_temp_path(path, sizeof(path)));

  TEST_ASSERT_TRUE(orion_playtime_write_record(path, "CUSA11111", 10));
  TEST_ASSERT_TRUE(orion_playtime_write_record(path, "CUSA22222", 20));
  TEST_ASSERT_TRUE(orion_playtime_get_duration(path, "CUSA11111", &a));
  TEST_ASSERT_TRUE(orion_playtime_get_duration(path, "CUSA22222", &b));
  TEST_ASSERT_EQ_U64(10, a);
  TEST_ASSERT_EQ_U64(20, b);

  TEST_ASSERT_TRUE(orion_playtime_modify_duration(path, "CUSA22222", 200));
  b = 0;
  TEST_ASSERT_TRUE(orion_playtime_get_duration(path, "CUSA22222", &b));
  TEST_ASSERT_EQ_U64(200, b);
  a = 0;
  TEST_ASSERT_TRUE(orion_playtime_get_duration(path, "CUSA11111", &a));
  TEST_ASSERT_EQ_U64(10, a);

  unlink(path);
  return 0;
}

static int test_missing_tid_appends_zero(void) {
  char path[256];
  uint64_t dur = 5;
  TEST_ASSERT_EQ_INT(0, make_temp_path(path, sizeof(path)));

  TEST_ASSERT_TRUE(orion_playtime_write_record(path, "CUSA11111", 5));
  /* get unknown tid → write with 0 */
  TEST_ASSERT_TRUE(orion_playtime_get_duration(path, "CUSA99999", &dur));
  /* after create-with-0, re-get should yield 0 */
  dur = 123;
  TEST_ASSERT_TRUE(orion_playtime_get_duration(path, "CUSA99999", &dur));
  TEST_ASSERT_EQ_U64(0, dur);

  unlink(path);
  return 0;
}

static int test_tid_truncation(void) {
  char path[256];
  uint64_t dur = 0;
  TEST_ASSERT_EQ_INT(0, make_temp_path(path, sizeof(path)));

  /* longer than 10: stored truncated; lookup must use same prefix rules */
  TEST_ASSERT_TRUE(
      orion_playtime_write_record(path, "CUSA123456789", 7));
  /* first 10 chars: "CUSA123456" */
  TEST_ASSERT_TRUE(orion_playtime_get_duration(path, "CUSA123456", &dur));
  TEST_ASSERT_EQ_U64(7, dur);

  unlink(path);
  return 0;
}

static int test_null_args(void) {
  uint64_t d = 0;
  TEST_ASSERT_TRUE(!orion_playtime_write_record(NULL, "x", 1));
  TEST_ASSERT_TRUE(!orion_playtime_write_record("/tmp/x", NULL, 1));
  TEST_ASSERT_TRUE(!orion_playtime_get_duration(NULL, "x", &d));
  TEST_ASSERT_TRUE(!orion_playtime_get_duration("/tmp/x", "x", NULL));
  return 0;
}

int test_playtime_suite(void) {
  int failures = 0;
  failures += orion_test_run("playtime.write_get", test_write_and_get);
  failures += orion_test_run("playtime.modify", test_modify);
  failures += orion_test_run("playtime.multi_tid", test_multi_tid);
  failures +=
      orion_test_run("playtime.missing_tid", test_missing_tid_appends_zero);
  failures += orion_test_run("playtime.tid_trunc", test_tid_truncation);
  failures += orion_test_run("playtime.null_args", test_null_args);
  return failures;
}
