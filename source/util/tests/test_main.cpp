#include <cstdio>

extern "C" int test_cheat_utils_suite(void);
extern "C" int test_cheat_parsers_suite(void);
extern "C" int test_settings_suite(void);
extern "C" int test_ready_suite(void);
extern "C" int test_platform_fs_suite(void);
extern "C" int test_platform_log_suite(void);
extern "C" int test_platform_notify_suite(void);
extern "C" int test_msg_protocol_suite(void);

int main() {
  int failures = 0;

  failures += test_cheat_utils_suite();
  failures += test_cheat_parsers_suite();
  failures += test_settings_suite();
  failures += test_ready_suite();
  failures += test_platform_fs_suite();
  failures += test_platform_log_suite();
  failures += test_platform_notify_suite();
  failures += test_msg_protocol_suite();

  if (failures == 0) {
    std::fprintf(stderr, "All util host tests passed.\n");
    return 0;
  }

  std::fprintf(stderr, "%d util host tests failed.\n", failures);
  return 1;
}
