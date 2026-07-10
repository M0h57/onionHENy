/* Host unit tests for shellui ps5ui::Page fluent XML builder (no PS5 SDK). */
#include "test_harness.h"

#include "ps5_settings_ui.hpp"

#include <string>

using namespace ps5ui;

static int test_escape_special_chars(void) {
  const std::string a = escape("a&b");
  const std::string b = escape("<tag>");
  const std::string c = escape("\"q\"");
  const std::string d = escape("/user/data/OrionHEN");
  const std::string e = escape("金手指");
  TEST_ASSERT_STREQ("a&amp;b", a.c_str());
  TEST_ASSERT_STREQ("&lt;tag&gt;", b.c_str());
  TEST_ASSERT_STREQ("&quot;q&quot;", c.c_str());
  /* Legacy path convention: / → // */
  TEST_ASSERT_STREQ("//user//data//OrionHEN", d.c_str());
  /* CJK unchanged */
  TEST_ASSERT_STREQ("金手指", e.c_str());
  return 0;
}

static int test_minimal_page_shell(void) {
  const std::string xml = Page("id_root", "Root Title").build();

  TEST_ASSERT_TRUE(xml.find("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>") !=
                   std::string::npos);
  TEST_ASSERT_TRUE(xml.find("<system_settings version=\"1.0\" "
                            "plugin=\"debug_settings_plugin\">") !=
                   std::string::npos);
  TEST_ASSERT_TRUE(xml.find("<setting_list id=\"id_root\" title=\"Root Title\">") !=
                   std::string::npos);
  TEST_ASSERT_TRUE(xml.find("</setting_list>") != std::string::npos);
  TEST_ASSERT_TRUE(xml.find("</system_settings>") != std::string::npos);
  return 0;
}

static int test_toggle_button_label_attrs(void) {
  const std::string xml =
      Page("id_page", "Page")
          .label("id_lbl", "Hello", Style::Center)
          .toggle("id_sw", "Switch", true, "second", "desc", "tex_icon")
          .button("id_btn", "Btn", "sec", "d", "icn", Style::Center)
          .build();

  TEST_ASSERT_TRUE(xml.find("<label id=\"id_lbl\" title=\"Hello\" style=\"center\"/>") !=
                   std::string::npos);
  TEST_ASSERT_TRUE(xml.find("<toggle_switch id=\"id_sw\" title=\"Switch\" "
                            "second_title=\"second\" description=\"desc\" "
                            "icon=\"tex_icon\" value=\"1\"/>") != std::string::npos);
  TEST_ASSERT_TRUE(xml.find("value=\"0\"") == std::string::npos); // only the on=true toggle
  TEST_ASSERT_TRUE(xml.find("<button id=\"id_btn\" title=\"Btn\" second_title=\"sec\" "
                            "description=\"d\" icon=\"icn\" style=\"center\"/>") !=
                   std::string::npos);
  return 0;
}

static int test_toggle_off_value(void) {
  const std::string xml =
      Page("p", "P").toggle("id_off", "Off", false).build();
  TEST_ASSERT_TRUE(xml.find("id=\"id_off\"") != std::string::npos);
  TEST_ASSERT_TRUE(xml.find("value=\"0\"") != std::string::npos);
  return 0;
}

static int test_list_and_items(void) {
  const std::string xml =
      Page("id_cheats", "Cheats")
          .list("id_selected_cheats_repo", "Repo",
                [](ListBuilder& L) {
                  L.item("id_selected_cheats_repo_1", "OrionHEN PS5", "0")
                      .item("id_selected_cheats_repo_2", "GoldHEN PS4", "1");
                })
          .button("id_dl_cheats", "Download", "to /data/OrionHEN/cheats/")
          .build();

  TEST_ASSERT_TRUE(xml.find("<list id=\"id_selected_cheats_repo\" title=\"Repo\">") !=
                   std::string::npos);
  TEST_ASSERT_TRUE(
      xml.find("<list_item id=\"id_selected_cheats_repo_1\" title=\"OrionHEN PS5\" "
               "value=\"0\"/>") != std::string::npos);
  TEST_ASSERT_TRUE(
      xml.find("<list_item id=\"id_selected_cheats_repo_2\" title=\"GoldHEN PS4\" "
               "value=\"1\"/>") != std::string::npos);
  TEST_ASSERT_TRUE(xml.find("</list>") != std::string::npos);
  TEST_ASSERT_TRUE(xml.find("id=\"id_dl_cheats\"") != std::string::npos);
  /* path second_title must escape slashes */
  TEST_ASSERT_TRUE(xml.find("second_title=\"to //data//OrionHEN//cheats//\"") !=
                   std::string::npos);
  return 0;
}

static int test_nested_group(void) {
  const std::string xml =
      Page("id_root", "Root")
          .group(
              "id_sub", "Sub",
              [](Group& g) {
                g.toggle("id_a", "A", true).button("id_b", "B");
              },
              "sub second")
          .build();

  TEST_ASSERT_TRUE(
      xml.find("<setting_list id=\"id_sub\" title=\"Sub\" second_title=\"sub second\">") !=
      std::string::npos);
  TEST_ASSERT_TRUE(xml.find("id=\"id_a\"") != std::string::npos);
  TEST_ASSERT_TRUE(xml.find("id=\"id_b\"") != std::string::npos);
  /* Nested close: root + sub */
  size_t closes = 0;
  for (size_t pos = 0; (pos = xml.find("</setting_list>", pos)) != std::string::npos;
       pos += 15)
    ++closes;
  TEST_ASSERT_EQ_INT(2, static_cast<int>(closes));
  return 0;
}

static int test_link_and_root_style(void) {
  const std::string xml =
      Page("id_payload", "Payload")
          .root_style(Style::Center)
          .link("id_auto_payloads", "★ 启动菜单", "auto_payloads.xml", "cfg")
          .build();

  TEST_ASSERT_TRUE(xml.find("id=\"id_payload\"") != std::string::npos);
  TEST_ASSERT_TRUE(xml.find("style=\"center\"") != std::string::npos);
  TEST_ASSERT_TRUE(xml.find("<link id=\"id_auto_payloads\" title=\"★ 启动菜单\" "
                            "second_title=\"cfg\" file=\"auto_payloads.xml\"/>") !=
                       std::string::npos ||
                   xml.find("<link id=\"id_auto_payloads\" title=\"★ 启动菜单\" "
                            "file=\"auto_payloads.xml\" second_title=\"cfg\"/>") !=
                       std::string::npos);
  return 0;
}

static int test_attr_escaping_in_titles(void) {
  const std::string xml =
      Page("id", "T & T")
          .label("l1", "a < b > c \"d\"")
          .build();

  TEST_ASSERT_TRUE(xml.find("title=\"T &amp; T\"") != std::string::npos);
  TEST_ASSERT_TRUE(xml.find("title=\"a &lt; b &gt; c &quot;d&quot;\"") !=
                   std::string::npos);
  return 0;
}

static int test_remote_play_like_page(void) {
  const std::string xml =
      Page("remote_play_pin_display", "远程游玩连接详情")
          .root_style(Style::Center)
          .label("id_pin", "PIN 码  : 1234 5678    ", Style::Center)
          .label("base64_account_id", "账号 ID: ABCD", Style::Center)
          .button("id_save_rp_info", "将远程游玩详情保存到 USB", std::nullopt,
                  std::nullopt, std::nullopt, Style::Center)
          .build();

  TEST_ASSERT_TRUE(xml.find("id=\"remote_play_pin_display\"") != std::string::npos);
  TEST_ASSERT_TRUE(xml.find("id=\"id_pin\"") != std::string::npos);
  TEST_ASSERT_TRUE(xml.find("id=\"base64_account_id\"") != std::string::npos);
  TEST_ASSERT_TRUE(xml.find("id=\"id_save_rp_info\"") != std::string::npos);
  TEST_ASSERT_TRUE(xml.find("远程游玩") != std::string::npos);
  return 0;
}

static int test_fluent_returns_this(void) {
  Page page("p", "P");
  Page& chain = page.toggle("t1", "T1", false).button("b1", "B1").label("l1", "L1");
  TEST_ASSERT_TRUE(&chain == &page);
  const std::string xml = page.build();
  TEST_ASSERT_TRUE(xml.find("id=\"t1\"") != std::string::npos);
  TEST_ASSERT_TRUE(xml.find("id=\"b1\"") != std::string::npos);
  TEST_ASSERT_TRUE(xml.find("id=\"l1\"") != std::string::npos);
  return 0;
}

extern "C" int test_ps5_settings_ui_suite(void) {
  int fails = 0;
  fails += orion_test_run("ps5ui.escape", test_escape_special_chars);
  fails += orion_test_run("ps5ui.minimal_page", test_minimal_page_shell);
  fails += orion_test_run("ps5ui.widgets", test_toggle_button_label_attrs);
  fails += orion_test_run("ps5ui.toggle_off", test_toggle_off_value);
  fails += orion_test_run("ps5ui.list", test_list_and_items);
  fails += orion_test_run("ps5ui.nested_group", test_nested_group);
  fails += orion_test_run("ps5ui.link_style", test_link_and_root_style);
  fails += orion_test_run("ps5ui.attr_escape", test_attr_escaping_in_titles);
  fails += orion_test_run("ps5ui.remote_play_like", test_remote_play_like_page);
  fails += orion_test_run("ps5ui.fluent_chain", test_fluent_returns_this);
  return fails;
}
