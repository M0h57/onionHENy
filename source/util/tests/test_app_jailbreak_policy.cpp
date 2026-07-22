#include "test_harness.h"

#include <onion/app_jailbreak_policy.hpp>

static int test_itemzflow_is_whitelisted(void) {
  TEST_ASSERT_TRUE(onion::app_jailbreak::is_whitelisted("ITEM00001"));
  TEST_ASSERT_STREQ("exact",
                    onion::app_jailbreak::whitelist_reason("ITEM00001"));
  return 0;
}

static int test_existing_policy_is_preserved(void) {
  TEST_ASSERT_TRUE(onion::app_jailbreak::is_whitelisted("NPXS39041"));
  TEST_ASSERT_TRUE(onion::app_jailbreak::is_whitelisted("PKGI13337"));
  TEST_ASSERT_TRUE(onion::app_jailbreak::is_whitelisted("PKGI12345"));
  TEST_ASSERT_TRUE(onion::app_jailbreak::is_whitelisted("TOOL00001"));
  TEST_ASSERT_TRUE(onion::app_jailbreak::is_whitelisted("LAPY12345"));
  TEST_ASSERT_TRUE(!onion::app_jailbreak::is_whitelisted("CUSA12345"));
  TEST_ASSERT_STREQ("LAPY*",
                    onion::app_jailbreak::whitelist_reason("LAPY12345"));
  TEST_ASSERT_STREQ("none",
                    onion::app_jailbreak::whitelist_reason("CUSA12345"));
  return 0;
}

extern "C" int test_app_jailbreak_policy_suite(void) {
  int failures = 0;
  failures += onion_test_run("app_jailbreak.itemzflow_whitelisted",
                             test_itemzflow_is_whitelisted);
  failures += onion_test_run("app_jailbreak.existing_policy",
                             test_existing_policy_is_preserved);
  return failures;
}
