/* Host tests for liborion_platform OrionHEN_log / configure. */
#include "test_harness.h"

#include <orion/log.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int test_log_file_sink(void) {
  char tmpl[] = "/tmp/orion-log-XXXXXX";
  int fd = mkstemp(tmpl);
  TEST_ASSERT_TRUE(fd >= 0);
  close(fd);
  unlink(tmpl); /* log open will create */

  orion_log_configure("HostTest", tmpl);
  OrionHEN_log("hello %d", 42);

  FILE *f = fopen(tmpl, "r");
  TEST_ASSERT_TRUE(f != NULL);
  char buf[256];
  size_t n = fread(buf, 1, sizeof(buf) - 1, f);
  fclose(f);
  buf[n] = '\0';
  TEST_ASSERT_TRUE(strstr(buf, "hello 42") != NULL);
  TEST_ASSERT_TRUE(strchr(buf, '\n') != NULL);

  unlink(tmpl);
  /* disable file sink for later suites */
  orion_log_configure("OrionHEN", NULL);
  return 0;
}

static int test_log_configure_tag_only(void) {
  orion_log_configure("TagOnly", NULL);
  /* no file sink — should not crash */
  OrionHEN_log("silent-ish");
  orion_log_configure("OrionHEN", NULL);
  return 0;
}

int test_platform_log_suite(void) {
  int failures = 0;
  failures += orion_test_run("log_file_sink", test_log_file_sink);
  failures += orion_test_run("log_configure_tag_only", test_log_configure_tag_only);
  return failures;
}
