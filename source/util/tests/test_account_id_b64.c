/* Host tests for PSN account-id base64 encoder. */
#include "test_harness.h"

#include <orion/account_id_b64.h>

#include <string.h>

static int test_zero(void) {
  char out[16];
  orion_account_id_base64_encode(0, out);
  /* 8 zero bytes → AAAAAAAAAAA= (standard) */
  TEST_ASSERT_STREQ("AAAAAAAAAAA=", out);
  return 0;
}

static int test_known_le(void) {
  char out[16];
  /* LE bytes 08 07 06 05 04 03 02 01 → CAcGBQQDAgE= */
  orion_account_id_base64_encode(0x0102030405060708ULL, out);
  TEST_ASSERT_STREQ("CAcGBQQDAgE=", out);
  return 0;
}

static int test_null_safe(void) {
  orion_account_id_base64_encode(1, NULL); /* no crash */
  return 0;
}

static int test_ascii_stable(void) {
  char out[16];
  orion_account_id_base64_encode(1ULL, out);
  /* byte0=1 rest 0 → AQAAAAAAAAA= */
  TEST_ASSERT_STREQ("AQAAAAAAAAA=", out);
  return 0;
}

int test_account_id_b64_suite(void) {
  int failures = 0;
  failures += orion_test_run("account_id_b64.zero", test_zero);
  failures += orion_test_run("account_id_b64.known_le", test_known_le);
  failures += orion_test_run("account_id_b64.null_safe", test_null_safe);
  failures += orion_test_run("account_id_b64.one", test_ascii_stable);
  return failures;
}
