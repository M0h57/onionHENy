/* Host tests for onion_notify_format (pure string path). */
#include "test_harness.h"

#include <onion/notify.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void format_msg(char *out, size_t out_sz, int wm, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  onion_notify_format(out, out_sz, wm, fmt, ap);
  va_end(ap);
}

static int test_notify_format_prefix(void) {
  char out[128];
  format_msg(out, sizeof(out), 1, "hello %s", "world");
  TEST_ASSERT_STREQ("[OnionHEN] hello world", out);
  return 0;
}

static int test_notify_format_truncates(void) {
  char out[16];
  format_msg(out, sizeof(out), 0, "0123456789ABCDEFGHIJ");
  /* must be NUL-terminated and start with prefix */
  TEST_ASSERT_TRUE(out[sizeof(out) - 1] == '\0' || strlen(out) < sizeof(out));
  TEST_ASSERT_TRUE(strncmp(out, "[OnionHEN]", 10) == 0);
  return 0;
}

static int test_notify_send_noop(void) {
  /* hits sceKernelSendNotificationRequest stub — must not crash */
  onion_notify(1, "host test notify %d", 7);
  return 0;
}

int test_platform_notify_suite(void) {
  int failures = 0;
  failures += onion_test_run("notify_format_prefix", test_notify_format_prefix);
  failures += onion_test_run("notify_format_truncates", test_notify_format_truncates);
  failures += onion_test_run("notify_send_noop", test_notify_send_noop);
  return failures;
}
