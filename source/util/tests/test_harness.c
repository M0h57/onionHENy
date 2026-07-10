#include "test_harness.h"

#include <stdarg.h>
#include <stdio.h>

int orion_test_fail(const char *file, int line, const char *fmt, ...) {
  va_list args;

  fprintf(stderr, "  %s:%d: ", file, line);
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fputc('\n', stderr);
  return 1;
}

int orion_test_run(const char *name, orion_test_fn_t fn) {
  const int rc = fn();
  fprintf(stderr, "[%s] %s\n", rc == 0 ? "PASS" : "FAIL", name);
  return rc;
}
