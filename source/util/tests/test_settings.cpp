/* Host unit tests for liborion_settings schema (no PS5 SDK). */
#include "test_harness.h"

#include <orion/settings.hpp>

#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

static std::string temp_ini_path() {
  char tmpl[] = "/tmp/orion-settings-XXXXXX.ini";
  /* mkstemps needs suffix length 4 for ".ini" */
  int fd = mkstemps(tmpl, 4);
  if (fd < 0) {
    return {};
  }
  close(fd);
  unlink(tmpl); /* write fresh via settings_save_file */
  return std::string(tmpl);
}

static int test_defaults_and_serialize_keys(void) {
  orion::Settings s{};
  std::string text = orion::settings_serialize(s);

  TEST_ASSERT_TRUE(text.find("[Settings]") != std::string::npos);
  TEST_ASSERT_TRUE(text.find("schema_version=1") != std::string::npos);
  TEST_ASSERT_TRUE(text.find("DPI=1") != std::string::npos);
  TEST_ASSERT_TRUE(text.find("DPI_v2=0") != std::string::npos);
  TEST_ASSERT_TRUE(text.find("toolbox_auto_start=1") != std::string::npos);
  TEST_ASSERT_TRUE(text.find("fan_threshold=77") != std::string::npos);
  TEST_ASSERT_TRUE(text.find("legacy_cmd_server=0") != std::string::npos);
  TEST_ASSERT_TRUE(text.find("Overlay_pos=0") != std::string::npos);
  return 0;
}

static int test_roundtrip_file(void) {
  std::string path = temp_ini_path();
  TEST_ASSERT_TRUE(!path.empty());

  orion::Settings in{};
  in.DPI = false;
  in.DPI_v2 = true;
  in.fan_threshold = 55;
  in.overlay_fps = true;
  in.legacy_cmd_server = true;
  in.cheats_shortcut_opt = 2;
  in.selected_cheats_repo = 1;
  in.rest_mode_delay_seconds = 7;
  in.start_option = 3;
  in.allow_data_in_sandbox = true;

  TEST_ASSERT_TRUE(orion::settings_save_file(path.c_str(), in));

  orion::Settings out{};
  TEST_ASSERT_TRUE(orion::settings_load_file(path.c_str(), &out));

  TEST_ASSERT_TRUE(out.DPI == false);
  TEST_ASSERT_TRUE(out.DPI_v2 == true);
  TEST_ASSERT_EQ_INT(55, out.fan_threshold);
  TEST_ASSERT_TRUE(out.overlay_fps == true);
  TEST_ASSERT_TRUE(out.legacy_cmd_server == true);
  TEST_ASSERT_EQ_INT(2, out.cheats_shortcut_opt);
  TEST_ASSERT_EQ_INT(1, out.selected_cheats_repo);
  TEST_ASSERT_EQ_U64(7, out.rest_mode_delay_seconds);
  TEST_ASSERT_EQ_INT(3, out.start_option);
  TEST_ASSERT_TRUE(out.allow_data_in_sandbox == true);
  TEST_ASSERT_EQ_INT(orion::kSettingsSchemaVersion, out.schema_version);

  unlink(path.c_str());
  return 0;
}

static int test_legacy_key_debug_legacy_cmd_server(void) {
  std::string path = temp_ini_path();
  TEST_ASSERT_TRUE(!path.empty());

  const char *body =
      "[Settings]\n"
      "DPI=0\n"
      "debug_legacy_cmd_server=1\n";
  FILE *f = fopen(path.c_str(), "w");
  TEST_ASSERT_TRUE(f != nullptr);
  fputs(body, f);
  fclose(f);

  orion::Settings out{};
  TEST_ASSERT_TRUE(orion::settings_load_file(path.c_str(), &out));
  TEST_ASSERT_TRUE(out.legacy_cmd_server == true);
  TEST_ASSERT_TRUE(out.DPI == false);

  unlink(path.c_str());
  return 0;
}

static int test_missing_file_defaults(void) {
  orion::Settings out{};
  out.DPI = false;
  out.fan_threshold = 1;
  TEST_ASSERT_TRUE(
      !orion::settings_load_file("/tmp/orion-settings-does-not-exist-xyz.ini",
                                 &out));
  /* load_file resets to defaults even on failure */
  TEST_ASSERT_TRUE(out.DPI == true);
  TEST_ASSERT_EQ_INT(77, out.fan_threshold);
  return 0;
}

extern "C" int test_settings_suite(void) {
  int failures = 0;
  failures += orion_test_run("settings_defaults_serialize", test_defaults_and_serialize_keys);
  failures += orion_test_run("settings_roundtrip_file", test_roundtrip_file);
  failures += orion_test_run("settings_legacy_debug_cmd_key",
                             test_legacy_key_debug_legacy_cmd_server);
  failures += orion_test_run("settings_missing_file_defaults", test_missing_file_defaults);
  return failures;
}
