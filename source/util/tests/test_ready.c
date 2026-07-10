/* Host tests for orion_ready protocol */
#include "test_harness.h"
#include <orion/ready.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int test_ready_signal_wait_clear(void) {
  const char *name = "host_test_marker";
  orion_ready_clear(name);
  TEST_ASSERT_TRUE(!orion_ready_is_set(name));
  TEST_ASSERT_TRUE(orion_ready_signal(name));
  TEST_ASSERT_TRUE(orion_ready_is_set(name));
  TEST_ASSERT_TRUE(orion_ready_wait(name, 100, 50));
  TEST_ASSERT_TRUE(orion_ready_clear(name));
  TEST_ASSERT_TRUE(!orion_ready_is_set(name));
  return 0;
}

static int test_ready_reject_slash(void) {
  TEST_ASSERT_TRUE(!orion_ready_signal("../evil"));
  TEST_ASSERT_TRUE(!orion_ready_is_set("../evil"));
  return 0;
}

static int test_ready_timeout(void) {
  orion_ready_clear("never_set_xyz");
  TEST_ASSERT_TRUE(!orion_ready_wait("never_set_xyz", 100, 50));
  return 0;
}

int test_ready_suite(void) {
  int failures = 0;
  failures += orion_test_run("ready_signal_wait_clear", test_ready_signal_wait_clear);
  failures += orion_test_run("ready_reject_slash", test_ready_reject_slash);
  failures += orion_test_run("ready_timeout", test_ready_timeout);
  return failures;
}
