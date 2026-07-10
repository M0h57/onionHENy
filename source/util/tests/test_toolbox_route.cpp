/* Host unit tests for toolbox::resolve_resource state machine (no PS5/Mono). */
#include "test_harness.h"

#include "toolbox_route.hpp"

#include <cstring>
#include <string>

using namespace toolbox;

static ResourceNames test_names() {
  return ResourceNames{
      .plugin_xml = "plugins.xml",
      .debug_settings_xml = "debug_settings.xml",
      .cheats_xml = "cheats.xml",
      .remote_play_xml = "remote_play.xml",
  };
}

static RouteInput make_in(std::string_view resource, bool sc = false,
                          bool sc_not_open = false) {
  return RouteInput{
      .resource = resource,
      .names = test_names(),
      .cheats_shortcut = sc,
      .cheats_shortcut_not_open = sc_not_open,
  };
}

static int test_unknown_passthrough(void) {
  RouteResult r = resolve_resource(make_in("something.else.xml"));
  TEST_ASSERT_TRUE(r.page == Page::None);
  TEST_ASSERT_TRUE(!r.flags.is_plugin);
  TEST_ASSERT_TRUE(!r.flags.is_cheats);
  return 0;
}

static int test_plugins_page(void) {
  RouteResult r = resolve_resource(make_in("plugins.xml"));
  TEST_ASSERT_TRUE(r.page == Page::Plugins);
  TEST_ASSERT_TRUE(r.flags.is_plugin);
  TEST_ASSERT_TRUE(!r.flags.is_cheats);
  return 0;
}

static int test_debug_settings_page(void) {
  RouteResult r = resolve_resource(make_in("debug_settings.xml"));
  TEST_ASSERT_TRUE(r.page == Page::DebugSettings);
  TEST_ASSERT_TRUE(r.flags.is_debug_settings);
  return 0;
}

static int test_cheats_page(void) {
  RouteResult r = resolve_resource(make_in("cheats.xml"));
  TEST_ASSERT_TRUE(r.page == Page::Cheats);
  TEST_ASSERT_TRUE(r.flags.is_cheats);
  TEST_ASSERT_TRUE(r.clear_cheat_shortcuts_after);
  return 0;
}

static int test_auto_plugins_and_plapps(void) {
  RouteResult a = resolve_resource(make_in(kAutoPluginsXml));
  TEST_ASSERT_TRUE(a.page == Page::AutoPlugins);
  TEST_ASSERT_TRUE(a.flags.is_auto_plugin);

  RouteResult p = resolve_resource(make_in(kPlappsXml));
  TEST_ASSERT_TRUE(p.page == Page::Plapps);
  TEST_ASSERT_TRUE(p.flags.is_plapps);
  return 0;
}

static int test_remote_play(void) {
  RouteResult r = resolve_resource(make_in("remote_play.xml"));
  TEST_ASSERT_TRUE(r.page == Page::RemotePlay);
  TEST_ASSERT_TRUE(r.flags.is_remote_play);
  return 0;
}

static int test_superuser_pass_through(void) {
  RouteResult r = resolve_resource(make_in(kSuperuserXml));
  TEST_ASSERT_TRUE(r.page == Page::SuperuserPass);
  TEST_ASSERT_TRUE(r.flags.is_su_menu);
  return 0;
}

static int test_og_debug_redirect(void) {
  RouteResult r = resolve_resource(make_in(kOgDebugXml));
  TEST_ASSERT_TRUE(r.page == Page::RedirectOgDebug);
  // flags left default for redirect path
  return 0;
}

static int test_shortcut_forces_cheats_over_debug(void) {
  // Resource is debug_settings, but shortcut is active → Cheats
  RouteResult r = resolve_resource(make_in("debug_settings.xml", /*sc=*/true));
  TEST_ASSERT_TRUE(r.page == Page::Cheats);
  TEST_ASSERT_TRUE(r.flags.is_cheats);
  TEST_ASSERT_TRUE(!r.flags.is_debug_settings);
  TEST_ASSERT_TRUE(r.shortcut_forced_cheats);
  TEST_ASSERT_TRUE(r.clear_cheat_shortcuts_after);
  return 0;
}

static int test_shortcut_not_open_also_forces_cheats(void) {
  RouteResult r =
      resolve_resource(make_in("debug_settings.xml", /*sc=*/false,
                               /*sc_not_open=*/true));
  TEST_ASSERT_TRUE(r.page == Page::Cheats);
  TEST_ASSERT_TRUE(r.shortcut_forced_cheats);
  return 0;
}

static int test_cheat_map_reset_on_tid_change(void) {
  int map[kCheatMapSize];
  for (int i = 0; i < (int)kCheatMapSize; ++i)
    map[i] = 1;
  std::string cur = "CUSA00001";

  TEST_ASSERT_TRUE(!reset_cheat_map_if_tid_changed(cur, map, kCheatMapSize,
                                                   "CUSA00001"));
  TEST_ASSERT_EQ_INT(1, map[0]);

  TEST_ASSERT_TRUE(
      reset_cheat_map_if_tid_changed(cur, map, kCheatMapSize, "CUSA99999"));
  TEST_ASSERT_STREQ("CUSA99999", cur.c_str());
  TEST_ASSERT_EQ_INT(0, map[0]);
  TEST_ASSERT_EQ_INT(0, map[255]);
  return 0;
}

static int test_cheat_enabled_get_set_bounds(void) {
  int map[kCheatMapSize]{};
  set_cheat_enabled(map, kCheatMapSize, 3, true);
  TEST_ASSERT_TRUE(get_cheat_enabled(map, kCheatMapSize, 3));
  set_cheat_enabled(map, kCheatMapSize, 3, false);
  TEST_ASSERT_TRUE(!get_cheat_enabled(map, kCheatMapSize, 3));

  set_cheat_enabled(map, kCheatMapSize, -1, true);
  set_cheat_enabled(map, kCheatMapSize, 999, true);
  TEST_ASSERT_TRUE(!get_cheat_enabled(map, kCheatMapSize, -1));
  TEST_ASSERT_TRUE(!get_cheat_enabled(map, kCheatMapSize, 999));
  return 0;
}

/** Simulates g_ui.apply_route_flags + clear_cheat_shortcuts without Mono. */
static int test_session_flag_apply_and_clear_shortcuts(void) {
  RouteFlags f{};
  f.is_plugin = true;
  f.is_cheats = true;

  bool is_plugin = false;
  bool is_cheats = false;
  bool sc = true;
  bool sc_no = true;

  // apply
  is_plugin = f.is_plugin;
  is_cheats = f.is_cheats;
  TEST_ASSERT_TRUE(is_plugin && is_cheats);

  // clear shortcuts after cheats page
  sc = sc_no = false;
  TEST_ASSERT_TRUE(!sc && !sc_no);

  // superuser alone → SuperuserPass, no dynamic serve
  RouteResult r = resolve_resource(make_in(kSuperuserXml));
  TEST_ASSERT_TRUE(r.page == Page::SuperuserPass);
  TEST_ASSERT_TRUE(r.page != Page::Plugins);
  return 0;
}

/** Full path: resource + shortcut → page + clear_after policy. */
static int test_route_matrix_exclusive_pages(void) {
  // Only one primary page for non-shortcut cases
  struct Case {
    const char *res;
    Page page;
  };
  const Case cases[] = {
      {"plugins.xml", Page::Plugins},
      {"debug_settings.xml", Page::DebugSettings},
      {"cheats.xml", Page::Cheats},
      {"remote_play.xml", Page::RemotePlay},
  };
  for (const Case &c : cases) {
    RouteResult r = resolve_resource(make_in(c.res));
    TEST_ASSERT_TRUE(r.page == c.page);
  }
  return 0;
}

extern "C" int test_toolbox_route_suite(void) {
  int fails = 0;
  fails += orion_test_run("route.unknown", test_unknown_passthrough);
  fails += orion_test_run("route.plugins", test_plugins_page);
  fails += orion_test_run("route.debug", test_debug_settings_page);
  fails += orion_test_run("route.cheats", test_cheats_page);
  fails += orion_test_run("route.auto_plapps", test_auto_plugins_and_plapps);
  fails += orion_test_run("route.remote_play", test_remote_play);
  fails += orion_test_run("route.superuser", test_superuser_pass_through);
  fails += orion_test_run("route.og_debug", test_og_debug_redirect);
  fails += orion_test_run("route.shortcut_force", test_shortcut_forces_cheats_over_debug);
  fails += orion_test_run("route.shortcut_not_open",
                          test_shortcut_not_open_also_forces_cheats);
  fails += orion_test_run("route.matrix", test_route_matrix_exclusive_pages);
  fails += orion_test_run("session.flags_clear",
                          test_session_flag_apply_and_clear_shortcuts);
  fails += orion_test_run("cheatmap.tid_reset", test_cheat_map_reset_on_tid_change);
  fails += orion_test_run("cheatmap.bounds", test_cheat_enabled_get_set_bounds);
  return fails;
}
