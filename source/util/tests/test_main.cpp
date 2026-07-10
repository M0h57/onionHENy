#include <cstdio>

extern "C" int test_cheat_utils_suite(void);
extern "C" int test_cheat_parsers_suite(void);

int main() {
  int failures = 0;

  failures += test_cheat_utils_suite();
  failures += test_cheat_parsers_suite();

  if (failures == 0) {
    std::fprintf(stderr, "All util cheat host tests passed.\n");
    return 0;
  }

  std::fprintf(stderr, "%d util cheat host tests failed.\n", failures);
  return 1;
}
