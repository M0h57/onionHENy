/* Host tests for registry entity-id formula. */
#include "test_harness.h"

#include <orion/reg_entity.h>

#define FALLBACK 0x7940500
#define BASE 0x7800500

static int test_slot1(void) {
  TEST_ASSERT_EQ_INT(BASE, orion_reg_entity_number(1, BASE, FALLBACK));
  return 0;
}

static int test_slot_n(void) {
  /* slot 2 → base + 65536 */
  TEST_ASSERT_EQ_INT(BASE + 65536, orion_reg_entity_number(2, BASE, FALLBACK));
  TEST_ASSERT_EQ_INT(BASE + 15 * 65536,
                     orion_reg_entity_number(16, BASE, FALLBACK));
  return 0;
}

static int test_out_of_range(void) {
  TEST_ASSERT_EQ_INT(FALLBACK, orion_reg_entity_number(0, BASE, FALLBACK));
  TEST_ASSERT_EQ_INT(FALLBACK, orion_reg_entity_number(17, BASE, FALLBACK));
  TEST_ASSERT_EQ_INT(FALLBACK, orion_reg_entity_number(-1, BASE, FALLBACK));
  return 0;
}

int test_reg_entity_suite(void) {
  int failures = 0;
  failures += orion_test_run("reg_entity.slot1", test_slot1);
  failures += orion_test_run("reg_entity.slot_n", test_slot_n);
  failures += orion_test_run("reg_entity.oor", test_out_of_range);
  return failures;
}
