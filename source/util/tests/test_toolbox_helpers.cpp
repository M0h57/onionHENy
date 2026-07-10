/* Host tests for toolbox_helpers (display path + plugin name filter). */
#include "test_harness.h"

#include "toolbox_helpers.hpp"

#include <string>

using namespace toolbox;

static int test_display_strip_user(void) {
  std::string a = display_path_for_ui("/user/data/OrionHEN/plugins/x.plugin");
  std::string b = display_path_for_ui("/user");
  TEST_ASSERT_STREQ("/data/OrionHEN/plugins/x.plugin", a.c_str());
  TEST_ASSERT_STREQ("", b.c_str());
  return 0;
}

static int test_display_map_usb(void) {
  std::string a = display_path_for_ui("/usb0/orionhen/plugins/a.elf");
  std::string b = display_path_for_ui("/usb3/x");
  TEST_ASSERT_STREQ("/mnt/usb0/orionhen/plugins/a.elf", a.c_str());
  TEST_ASSERT_STREQ("/mnt/usb3/x", b.c_str());
  return 0;
}

static int test_display_passthrough(void) {
  std::string a = display_path_for_ui("/data/OrionHEN/x");
  std::string b = display_path_for_ui("relative/path");
  std::string c = display_path_for_ui("");
  std::string d = display_path_for_ui("/userdata/x");
  TEST_ASSERT_STREQ("/data/OrionHEN/x", a.c_str());
  TEST_ASSERT_STREQ("relative/path", b.c_str());
  TEST_ASSERT_STREQ("", c.c_str());
  /* /userdata is not /user prefix (rfind at 0 needs exact prefix) */
  TEST_ASSERT_STREQ("/userdata/x", d.c_str());
  return 0;
}

static int test_plugin_name_accept(void) {
  TEST_ASSERT_TRUE(is_plugin_or_elf_name("foo.plugin"));
  TEST_ASSERT_TRUE(is_plugin_or_elf_name("bar.elf"));
  TEST_ASSERT_TRUE(is_plugin_or_elf_name("CUSA12345.elf"));
  TEST_ASSERT_TRUE(is_plugin_or_elf_name("mixed.plugin.backup")); /* contains .plugin */
  return 0;
}

static int test_plugin_name_reject(void) {
  TEST_ASSERT_TRUE(!is_plugin_or_elf_name(nullptr));
  TEST_ASSERT_TRUE(!is_plugin_or_elf_name(""));
  TEST_ASSERT_TRUE(!is_plugin_or_elf_name("readme.txt"));
  TEST_ASSERT_TRUE(!is_plugin_or_elf_name("foo.plugin.auto_start"));
  TEST_ASSERT_TRUE(!is_plugin_or_elf_name("bar.elf.auto_start"));
  TEST_ASSERT_TRUE(!is_plugin_or_elf_name("only.auto_start"));
  return 0;
}

extern "C" int test_toolbox_helpers_suite(void) {
  int fails = 0;
  fails += orion_test_run("toolbox.display_strip_user", test_display_strip_user);
  fails += orion_test_run("toolbox.display_map_usb", test_display_map_usb);
  fails += orion_test_run("toolbox.display_passthrough", test_display_passthrough);
  fails += orion_test_run("toolbox.plugin_name_accept", test_plugin_name_accept);
  fails += orion_test_run("toolbox.plugin_name_reject", test_plugin_name_reject);
  return fails;
}
