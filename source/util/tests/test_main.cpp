#include <cstdio>

extern "C" int test_cheat_utils_suite(void);
extern "C" int test_cheat_parsers_suite(void);
extern "C" int test_settings_suite(void);
extern "C" int test_ready_suite(void);

int main() {
  int failures = 0;

  failures += test_cheat_utils_suite();
  failures += test_cheat_parsers_suite();
  failures += test_settings_suite();
  failures += test_ready_suite();

  if (failures == 0) {
    std::fprintf(stderr, "All util host tests passed.\n");
    return 0;
  }

  std::fprintf(stderr, "%d util host tests failed.\n", failures);
  return 1;
}
