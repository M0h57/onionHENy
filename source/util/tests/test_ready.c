/* Host tests for onion_ready protocol + runtime flags. */
#include "test_harness.h"
#include <onion/ready.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int test_ready_signal_wait_clear(void) {
  const char *name = "host_test_marker";
  onion_ready_clear(name);
  TEST_ASSERT_TRUE(!onion_ready_is_set(name));
  TEST_ASSERT_TRUE(onion_ready_signal(name));
  TEST_ASSERT_TRUE(onion_ready_is_set(name));
  TEST_ASSERT_TRUE(onion_ready_wait(name, 100, 50));
  TEST_ASSERT_TRUE(onion_ready_clear(name));
  TEST_ASSERT_TRUE(!onion_ready_is_set(name));
  return 0;
}

static int test_ready_reject_slash(void) {
  TEST_ASSERT_TRUE(!onion_ready_signal("../evil"));
  TEST_ASSERT_TRUE(!onion_ready_is_set("../evil"));
  TEST_ASSERT_TRUE(!onion_ready_signal("a/b"));
  TEST_ASSERT_TRUE(!onion_ready_signal("dot.name"));
  TEST_ASSERT_TRUE(!onion_ready_signal(""));
  TEST_ASSERT_TRUE(!onion_ready_signal(NULL));
  return 0;
}

static int test_ready_timeout(void) {
  onion_ready_clear("never_set_xyz");
  TEST_ASSERT_TRUE(!onion_ready_wait("never_set_xyz", 100, 50));
  return 0;
}

static int test_ready_path_builder(void) {
  char buf[128];
  TEST_ASSERT_TRUE(onion_ready_path(ONION_FLAG_FPS_OVERLAY, buf, sizeof(buf)));
  TEST_ASSERT_TRUE(strstr(buf, "fps_overlay") != NULL);
  TEST_ASSERT_TRUE(strstr(buf, "onion_ready") != NULL);
  /* buffer too small */
  TEST_ASSERT_TRUE(!onion_ready_path(ONION_FLAG_FPS_OVERLAY, buf, 8));
  TEST_ASSERT_TRUE(!onion_ready_path(ONION_FLAG_FPS_OVERLAY, NULL, 64));
  return 0;
}

static int test_runtime_flag_fps_overlay(void) {
  onion_ready_clear(ONION_FLAG_FPS_OVERLAY);
  TEST_ASSERT_TRUE(!onion_ready_is_set(ONION_FLAG_FPS_OVERLAY));
  TEST_ASSERT_TRUE(onion_ready_signal(ONION_FLAG_FPS_OVERLAY));
  TEST_ASSERT_TRUE(onion_ready_is_set(ONION_FLAG_FPS_OVERLAY));
  TEST_ASSERT_TRUE(onion_ready_clear(ONION_FLAG_FPS_OVERLAY));
  TEST_ASSERT_TRUE(!onion_ready_is_set(ONION_FLAG_FPS_OVERLAY));
  return 0;
}

static int test_runtime_flag_util_booted(void) {
  onion_ready_clear(ONION_FLAG_UTIL_BOOTED);
  TEST_ASSERT_TRUE(onion_ready_signal(ONION_FLAG_UTIL_BOOTED));
  TEST_ASSERT_TRUE(onion_ready_is_set(ONION_FLAG_UTIL_BOOTED));
  onion_ready_clear(ONION_FLAG_UTIL_BOOTED);
  return 0;
}

static int test_toolbox_legacy_alias(void) {
  /* Clear both modern and legacy paths via clear() */
  onion_ready_clear(ONION_READY_TOOLBOX);
  TEST_ASSERT_TRUE(!onion_ready_is_set(ONION_READY_TOOLBOX));
  TEST_ASSERT_TRUE(onion_ready_signal(ONION_READY_TOOLBOX));
  TEST_ASSERT_TRUE(onion_ready_is_set(ONION_READY_TOOLBOX));
  /* legacy file should also exist on host */
  TEST_ASSERT_TRUE(access("/tmp/toolbox_online", F_OK) == 0);
  onion_ready_clear(ONION_READY_TOOLBOX);
  TEST_ASSERT_TRUE(!onion_ready_is_set(ONION_READY_TOOLBOX));
  return 0;
}

int test_ready_suite(void) {
  int failures = 0;
  failures += onion_test_run("ready_signal_wait_clear", test_ready_signal_wait_clear);
  failures += onion_test_run("ready_reject_slash", test_ready_reject_slash);
  failures += onion_test_run("ready_timeout", test_ready_timeout);
  failures += onion_test_run("ready_path_builder", test_ready_path_builder);
  failures += onion_test_run("flag_fps_overlay", test_runtime_flag_fps_overlay);
  failures += onion_test_run("flag_util_booted", test_runtime_flag_util_booted);
  failures += onion_test_run("toolbox_legacy_alias", test_toolbox_legacy_alias);
  return failures;
}
