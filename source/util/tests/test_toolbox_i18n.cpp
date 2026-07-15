/* Host unit tests for shellui toolbox_i18n (zh-Hans / en). */
#include "test_harness.h"

#include "toolbox_i18n.hpp"

#include <cstring>
#include <string>

using namespace toolbox_i18n;

static int test_default_zh(void) {
  set_lang(Lang::ZhHans);
  TEST_ASSERT_TRUE(std::strcmp(tr("root.title"), "★OnionHEN 工具箱") == 0);
  TEST_ASSERT_TRUE(std::strcmp(tr("group.pkg"), "软件包安装") == 0);
  return 0;
}

static int test_en(void) {
  set_lang(Lang::En);
  TEST_ASSERT_TRUE(std::strcmp(tr("root.title"), "★OnionHEN Toolbox") == 0);
  TEST_ASSERT_TRUE(std::strcmp(tr("group.pkg"), "Package Install") == 0);
  TEST_ASSERT_TRUE(std::strcmp(tr("sc.off"), "Off (no shortcut)") == 0);
  return 0;
}

static int test_apply_ui_lang(void) {
  apply_ui_lang(1);
  TEST_ASSERT_TRUE(active_lang() == Lang::En);
  apply_ui_lang(0);
  TEST_ASSERT_TRUE(active_lang() == Lang::ZhHans);
  apply_ui_lang(99); /* invalid → zh */
  TEST_ASSERT_TRUE(active_lang() == Lang::ZhHans);
  return 0;
}

static int test_missing_key(void) {
  set_lang(Lang::En);
  TEST_ASSERT_STREQ("no.such.key", tr("no.such.key"));
  return 0;
}

extern "C" int test_toolbox_i18n_suite(void) {
  int fails = 0;
  fails += onion_test_run("i18n.default_zh", test_default_zh);
  fails += onion_test_run("i18n.en", test_en);
  fails += onion_test_run("i18n.apply_ui_lang", test_apply_ui_lang);
  fails += onion_test_run("i18n.missing_key", test_missing_key);
  return fails;
}
