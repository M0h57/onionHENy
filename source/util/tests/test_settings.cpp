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
  in.fan_threshold = 55;
  in.overlay_fps = true;
  in.legacy_cmd_server = true;
  in.cheats_shortcut_opt = 2;
  in.selected_cheats_repo = 1;
  in.rest_mode_delay_seconds = 7;
  in.start_option = 3;

  TEST_ASSERT_TRUE(orion::settings_save_file(path.c_str(), in));

  orion::Settings out{};
  TEST_ASSERT_TRUE(orion::settings_load_file(path.c_str(), &out));

  TEST_ASSERT_EQ_INT(55, out.fan_threshold);
  TEST_ASSERT_TRUE(out.overlay_fps == true);
  TEST_ASSERT_TRUE(out.legacy_cmd_server == true);
  TEST_ASSERT_EQ_INT(2, out.cheats_shortcut_opt);
  TEST_ASSERT_EQ_INT(1, out.selected_cheats_repo);
  TEST_ASSERT_EQ_U64(7, out.rest_mode_delay_seconds);
  TEST_ASSERT_EQ_INT(3, out.start_option);
  TEST_ASSERT_EQ_INT(orion::kSettingsSchemaVersion, out.schema_version);

  unlink(path.c_str());
  return 0;
}

static int test_legacy_key_debug_legacy_cmd_server(void) {
  std::string path = temp_ini_path();
  TEST_ASSERT_TRUE(!path.empty());

  const char *body =
      "[Settings]\n"
      "debug_legacy_cmd_server=1\n";
  FILE *f = fopen(path.c_str(), "w");
  TEST_ASSERT_TRUE(f != nullptr);
  fputs(body, f);
  fclose(f);

  orion::Settings out{};
  TEST_ASSERT_TRUE(orion::settings_load_file(path.c_str(), &out));
  TEST_ASSERT_TRUE(out.legacy_cmd_server == true);

  unlink(path.c_str());
  return 0;
}

static int test_missing_file_defaults(void) {
  orion::Settings out{};
  out.fan_threshold = 1;
  TEST_ASSERT_TRUE(
      !orion::settings_load_file("/tmp/orion-settings-does-not-exist-xyz.ini",
                                 &out));
  /* load_file resets to defaults even on failure */
  TEST_ASSERT_EQ_INT(77, out.fan_threshold);
  return 0;
}

static int test_full_schema_roundtrip(void) {
  std::string path = temp_ini_path();
  TEST_ASSERT_TRUE(!path.empty());

  orion::Settings in{};
  in.toolbox_auto_start = false;
  in.disable_toolbox_auto_start_for_rest_mode = true;
  in.util_rest_kill = true;
  in.game_rest_kill = true;
  in.start_option = 2;
  in.rest_mode_delay_seconds = 42;
  in.libhijacker_cheats = true;
  in.debug_app_jb_msg = true;
  in.legacy_cmd_server = true;
  in.selected_cheats_repo = 1;
  in.auto_eject_disc = true;
  in.display_tids = true;
  in.orionhen_game_opts = false;
  in.enable_fan_speed = true;
  in.fan_threshold = 90;
  in.overlay_ram = false;
  in.overlay_cpu = false;
  in.overlay_gpu = false;
  in.overlay_fps = true;
  in.overlay_ip = true;
  in.overlay_pos = 3;
  in.cheats_shortcut_opt = 4;
  in.toolbox_shortcut_opt = 2;

  TEST_ASSERT_TRUE(orion::settings_save_file(path.c_str(), in));
  orion::Settings out{};
  TEST_ASSERT_TRUE(orion::settings_load_file(path.c_str(), &out));

  TEST_ASSERT_TRUE(out.toolbox_auto_start == in.toolbox_auto_start);
  TEST_ASSERT_TRUE(out.disable_toolbox_auto_start_for_rest_mode ==
                   in.disable_toolbox_auto_start_for_rest_mode);
  TEST_ASSERT_TRUE(out.util_rest_kill == in.util_rest_kill);
  TEST_ASSERT_TRUE(out.game_rest_kill == in.game_rest_kill);
  TEST_ASSERT_EQ_INT(in.start_option, out.start_option);
  TEST_ASSERT_EQ_U64(in.rest_mode_delay_seconds, out.rest_mode_delay_seconds);
  TEST_ASSERT_TRUE(out.libhijacker_cheats == in.libhijacker_cheats);
  TEST_ASSERT_TRUE(out.debug_app_jb_msg == in.debug_app_jb_msg);
  TEST_ASSERT_TRUE(out.legacy_cmd_server == in.legacy_cmd_server);
  TEST_ASSERT_EQ_INT(in.selected_cheats_repo, out.selected_cheats_repo);
  TEST_ASSERT_TRUE(out.auto_eject_disc == in.auto_eject_disc);
  TEST_ASSERT_TRUE(out.display_tids == in.display_tids);
  TEST_ASSERT_TRUE(out.orionhen_game_opts == in.orionhen_game_opts);
  TEST_ASSERT_TRUE(out.enable_fan_speed == in.enable_fan_speed);
  TEST_ASSERT_EQ_INT(in.fan_threshold, out.fan_threshold);
  TEST_ASSERT_TRUE(out.overlay_ram == in.overlay_ram);
  TEST_ASSERT_TRUE(out.overlay_cpu == in.overlay_cpu);
  TEST_ASSERT_TRUE(out.overlay_gpu == in.overlay_gpu);
  TEST_ASSERT_TRUE(out.overlay_fps == in.overlay_fps);
  TEST_ASSERT_TRUE(out.overlay_ip == in.overlay_ip);
  TEST_ASSERT_EQ_INT(in.overlay_pos, out.overlay_pos);
  TEST_ASSERT_EQ_INT(in.cheats_shortcut_opt, out.cheats_shortcut_opt);
  TEST_ASSERT_EQ_INT(in.toolbox_shortcut_opt, out.toolbox_shortcut_opt);

  unlink(path.c_str());
  return 0;
}

static int test_partial_ini_keeps_defaults(void) {
  std::string path = temp_ini_path();
  TEST_ASSERT_TRUE(!path.empty());
  FILE *f = fopen(path.c_str(), "w");
  TEST_ASSERT_TRUE(f != nullptr);
  fputs("[Settings]\ntoolbox_auto_start=0\n", f);
  fclose(f);

  orion::Settings out{};
  TEST_ASSERT_TRUE(orion::settings_load_file(path.c_str(), &out));
  /* specified key applied; unspecified keys stay at defaults */
  TEST_ASSERT_TRUE(out.toolbox_auto_start == false);
  TEST_ASSERT_EQ_INT(77, out.fan_threshold);

  unlink(path.c_str());
  return 0;
}

static int test_serialize_contains_overlay_keys(void) {
  orion::Settings s{};
  s.overlay_fps = true;
  s.overlay_ip = true;
  s.overlay_pos = 2;
  std::string text = orion::settings_serialize(s);
  TEST_ASSERT_TRUE(text.find("overlay_fps=1") != std::string::npos);
  TEST_ASSERT_TRUE(text.find("overlay_ip=1") != std::string::npos);
  TEST_ASSERT_TRUE(text.find("Overlay_pos=2") != std::string::npos);
  return 0;
}

static int test_empty_file_loads_defaults(void) {
  std::string path = temp_ini_path();
  TEST_ASSERT_TRUE(!path.empty());
  FILE *f = fopen(path.c_str(), "w");
  TEST_ASSERT_TRUE(f != nullptr);
  fclose(f);

  orion::Settings out{};
  /* empty file: load may succeed or fall back — either way defaults applied */
  (void)orion::settings_load_file(path.c_str(), &out);
  TEST_ASSERT_EQ_INT(77, out.fan_threshold);
  unlink(path.c_str());
  return 0;
}

extern "C" int test_settings_suite(void) {
  int failures = 0;
  failures += orion_test_run("settings_defaults_serialize", test_defaults_and_serialize_keys);
  failures += orion_test_run("settings_roundtrip_file", test_roundtrip_file);
  failures += orion_test_run("settings_legacy_debug_cmd_key",
                             test_legacy_key_debug_legacy_cmd_server);
  failures += orion_test_run("settings_missing_file_defaults", test_missing_file_defaults);
  failures += orion_test_run("settings_full_schema_roundtrip", test_full_schema_roundtrip);
  failures += orion_test_run("settings_partial_ini_defaults", test_partial_ini_keeps_defaults);
  failures += orion_test_run("settings_serialize_overlay_keys", test_serialize_contains_overlay_keys);
  failures += orion_test_run("settings_empty_file_defaults", test_empty_file_loads_defaults);
  return failures;
}
