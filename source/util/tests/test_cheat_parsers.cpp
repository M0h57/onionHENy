#include <cstdint>
#include <cstring>
#include <string>

#include "cheats/ICheatParser.hpp"
#include "cheats/cheat_engine.h"
#include "test_harness.h"
#include "test_support.h"

using orion::cheats::CheatParserFactory;

namespace {

int load_path(const char *path, orion_cheat_file_t &file) {
  return CheatParserFactory::loadFile(path, file);
}

int load_buf(const char *format, const char *data, orion_cheat_file_t &file) {
  return CheatParserFactory::loadBuffer(
      format, reinterpret_cast<const uint8_t *>(data), std::strlen(data), file);
}

int test_parse_json_buffer_success() {
  const char *json =
      "{"
      "\"process\":\"eboot.bin\","
      "\"name\":\"Demo\","
      "\"mods\":[{"
      "\"name\":\"Infinite HP\","
      "\"description\":\"Lock HP\","
      "\"memory\":[{"
      "\"offset\":\"1000\","
      "\"on\":\"AABBCCDD\","
      "\"off\":\"11223344\","
      "\"section\":1"
      "}]"
      "}]"
      "}";
  static orion_cheat_file_t file;
  const uint8_t on_expected[] = {0xAA, 0xBB, 0xCC, 0xDD};
  const uint8_t off_expected[] = {0x11, 0x22, 0x33, 0x44};

  TEST_ASSERT_EQ_INT(0, load_buf("json", json, file));
  TEST_ASSERT_STREQ("eboot.bin", file.process);
  TEST_ASSERT_STREQ("Demo", file.name);
  TEST_ASSERT_EQ_INT(1, static_cast<int>(file.cheat_count));
  TEST_ASSERT_STREQ("Infinite HP", file.cheats[0].name);
  TEST_ASSERT_EQ_INT(1, static_cast<int>(file.cheats[0].patch_count));
  TEST_ASSERT_EQ_INT(1, file.cheats[0].patches[0].section);
  TEST_ASSERT_EQ_U64(0x1000, file.cheats[0].patches[0].offset);
  TEST_ASSERT_MEMEQ(on_expected, file.cheats[0].patches[0].on,
                    sizeof(on_expected));
  TEST_ASSERT_MEMEQ(off_expected, file.cheats[0].patches[0].off,
                    sizeof(off_expected));
  orion_cheat_file_clear(&file);
  return 0;
}

int test_parse_json_buffer_rejects_invalid_hex() {
  const char *json =
      "{"
      "\"process\":\"eboot.bin\","
      "\"name\":\"Demo\","
      "\"mods\":[{"
      "\"name\":\"Broken\","
      "\"memory\":[{"
      "\"offset\":\"10\","
      "\"on\":\"ABG\","
      "\"off\":\"00\""
      "}]"
      "}]"
      "}";
  static orion_cheat_file_t file;

  TEST_ASSERT_EQ_INT(-1, load_buf("json", json, file));
  orion_cheat_file_clear(&file);
  return 0;
}

int test_parse_json_ignores_out_of_range_section() {
  const char *json =
      "{"
      "\"process\":\"eboot.bin\","
      "\"name\":\"Demo\","
      "\"mods\":[{"
      "\"name\":\"Patch\","
      "\"memory\":[{"
      "\"offset\":\"10\","
      "\"on\":\"AA\","
      "\"off\":\"BB\","
      "\"section\":99"
      "}]"
      "}]"
      "}";
  static orion_cheat_file_t file;

  TEST_ASSERT_EQ_INT(0, load_buf("json", json, file));
  TEST_ASSERT_EQ_INT(0, file.cheats[0].patches[0].section);
  orion_cheat_file_clear(&file);
  return 0;
}

int test_parse_xml_buffer_success() {
  const char *xml =
      "<Trainer Process=\"eboot.bin\" Game=\"Demo\">"
      "<Cheat Text=\"Infinite Ammo\" Description=\"No reload\">"
      "<Cheatline><Offset>1234</Offset><Section>2</Section>"
      "<ValueOn>AA-BB</ValueOn><ValueOff>00-11</ValueOff></Cheatline>"
      "</Cheat>"
      "</Trainer>";
  static orion_cheat_file_t file;
  const uint8_t on_expected[] = {0xAA, 0xBB};
  const uint8_t off_expected[] = {0x00, 0x11};

  TEST_ASSERT_EQ_INT(0, load_buf("shn", xml, file));
  TEST_ASSERT_STREQ("eboot.bin", file.process);
  TEST_ASSERT_STREQ("Demo", file.name);
  TEST_ASSERT_EQ_INT(1, static_cast<int>(file.cheat_count));
  TEST_ASSERT_STREQ("Infinite Ammo", file.cheats[0].name);
  TEST_ASSERT_EQ_INT(1, static_cast<int>(file.cheats[0].patch_count));
  TEST_ASSERT_EQ_INT(2, file.cheats[0].patches[0].section);
  TEST_ASSERT_EQ_U64(0x1234, file.cheats[0].patches[0].offset);
  TEST_ASSERT_MEMEQ(on_expected, file.cheats[0].patches[0].on,
                    sizeof(on_expected));
  TEST_ASSERT_MEMEQ(off_expected, file.cheats[0].patches[0].off,
                    sizeof(off_expected));
  orion_cheat_file_clear(&file);
  return 0;
}

int test_parse_xml_ignores_out_of_range_section() {
  const char *xml =
      "<Trainer Process=\"eboot.bin\" Game=\"Demo\">"
      "<Cheat Text=\"Patch\">"
      "<Cheatline><Offset>10</Offset><Section>99</Section>"
      "<ValueOn>AA</ValueOn><ValueOff>BB</ValueOff></Cheatline>"
      "</Cheat>"
      "</Trainer>";
  static orion_cheat_file_t file;

  TEST_ASSERT_EQ_INT(0, load_buf("shn", xml, file));
  TEST_ASSERT_EQ_INT(0, file.cheats[0].patches[0].section);
  orion_cheat_file_clear(&file);
  return 0;
}

int test_load_file_mc4_success() {
  const char *xml =
      "<Trainer Process=\"eboot.bin\" Game=\"Demo\">"
      "<Cheat Text=\"Master Code\" Description=\"base\">"
      "<Cheatline><Offset>2000</Offset><ValueOn>AA</ValueOn>"
      "<ValueOff>BB</ValueOff></Cheatline>"
      "</Cheat>"
      "</Trainer>";
  char path[256];
  static orion_cheat_file_t file;

  TEST_ASSERT_EQ_INT(0, orion_test_write_temp_mc4_file(xml, path, sizeof(path)));
  TEST_ASSERT_EQ_INT(0, load_path(path, file));
  TEST_ASSERT_STREQ("eboot.bin", file.process);
  TEST_ASSERT_EQ_INT(1, static_cast<int>(file.cheat_count));
  TEST_ASSERT_STREQ("Master Code", file.cheats[0].name);
  orion_cheat_file_clear(&file);
  orion_test_remove_file(path);
  return 0;
}

int test_fixture_json_file_loads() {
  char path[512];
  static orion_cheat_file_t file;

  TEST_ASSERT_EQ_INT(
      0, orion_test_fixture_path("fixtures/cheats/PPSA26344_01.008.000.json",
                                 path, sizeof(path)));
  TEST_ASSERT_EQ_INT(0, load_path(path, file));
  /* extract_string does not unescape JSON; keep raw \u014d sequence */
  TEST_ASSERT_STREQ("Ghost of Y\\u014dtei", file.name);
  TEST_ASSERT_STREQ("eboot.bin", file.process);
  TEST_ASSERT_EQ_INT(3, static_cast<int>(file.cheat_count));
  TEST_ASSERT_STREQ("Infi Health", file.cheats[0].name);
  orion_cheat_file_clear(&file);
  return 0;
}

int test_fixture_shn_file_loads() {
  char path[512];
  static orion_cheat_file_t file;

  TEST_ASSERT_EQ_INT(
      0, orion_test_fixture_path("fixtures/cheats/PPSA21159_01.001.000.shn",
                                 path, sizeof(path)));
  TEST_ASSERT_EQ_INT(0, load_path(path, file));
  TEST_ASSERT_STREQ("Silent Hill f", file.name);
  TEST_ASSERT_STREQ("eboot.bin", file.process);
  TEST_ASSERT_EQ_INT(9, static_cast<int>(file.cheat_count));
  TEST_ASSERT_STREQ("Infi Health", file.cheats[0].name);
  orion_cheat_file_clear(&file);
  return 0;
}

int test_fixture_mc4_file_loads() {
  char path[512];
  static orion_cheat_file_t file;

  TEST_ASSERT_EQ_INT(
      0, orion_test_fixture_path("fixtures/cheats/PPSA08710_01.005.000.mc4",
                                 path, sizeof(path)));
  TEST_ASSERT_EQ_INT(0, load_path(path, file));
  TEST_ASSERT_STREQ("eboot.bin", file.process);
  TEST_ASSERT_TRUE(file.cheat_count > 0);
  TEST_ASSERT_TRUE(file.name[0] != '\0');
  TEST_ASSERT_TRUE(file.cheats[0].name[0] != '\0');
  orion_cheat_file_clear(&file);
  return 0;
}

int test_fixture_shnext_file_loads() {
  char path[512];
  static orion_cheat_file_t file;
  uint8_t nop3[3];
  uint8_t nop6[6];
  uint8_t nop8[8];

  std::memset(nop3, 0x90, 3);
  std::memset(nop6, 0x90, 6);
  std::memset(nop8, 0x90, 8);

  TEST_ASSERT_EQ_INT(
      0, orion_test_fixture_path(
             "fixtures/cheats/"
             "Assassins-Creed-Mirage_PPSA07230_01.012.000_Aigars_Uze.ShnExt",
             path, sizeof(path)));
  TEST_ASSERT_EQ_INT(0, load_path(path, file));
  TEST_ASSERT_STREQ("Assassin's Creed Mirage", file.name);
  TEST_ASSERT_STREQ("eboot.bin", file.process);
  TEST_ASSERT_EQ_INT(4, static_cast<int>(file.cheat_count));

  TEST_ASSERT_STREQ("Enemys Dont See You", file.cheats[0].name);
  TEST_ASSERT_STREQ("by Anonyme", file.cheats[0].description);
  TEST_ASSERT_STREQ("eboot.bin", file.cheats[0].module_name);
  TEST_ASSERT_EQ_INT(1, static_cast<int>(file.cheats[0].patch_count));
  TEST_ASSERT_EQ_U64(109230218, file.cheats[0].patches[0].offset);
  TEST_ASSERT_TRUE(!file.cheats[0].patches[0].is_asm);
  TEST_ASSERT_EQ_INT(3, static_cast<int>(file.cheats[0].patches[0].on_len));
  TEST_ASSERT_MEMEQ(nop3, file.cheats[0].patches[0].on, 3);
  TEST_ASSERT_TRUE(file.cheats[0].patches[0].off_len > 0);

  TEST_ASSERT_STREQ("Infinite Air Under Water", file.cheats[1].name);
  TEST_ASSERT_EQ_U64(88497722, file.cheats[1].patches[0].offset);
  TEST_ASSERT_TRUE(!file.cheats[1].patches[0].is_asm);
  TEST_ASSERT_EQ_INT(8, static_cast<int>(file.cheats[1].patches[0].on_len));
  TEST_ASSERT_MEMEQ(nop8, file.cheats[1].patches[0].on, 8);

  TEST_ASSERT_STREQ("Infinite Health", file.cheats[2].name);
  TEST_ASSERT_EQ_U64(31563972, file.cheats[2].patches[0].offset);
  TEST_ASSERT_EQ_INT(6, static_cast<int>(file.cheats[2].patches[0].on_len));
  TEST_ASSERT_MEMEQ(nop6, file.cheats[2].patches[0].on, 6);

  TEST_ASSERT_STREQ("Infinite Stamina", file.cheats[3].name);
  TEST_ASSERT_EQ_U64(39201365, file.cheats[3].patches[0].offset);
  TEST_ASSERT_TRUE(file.cheats[3].patches[0].on_len > 0);
  TEST_ASSERT_TRUE(file.cheats[3].patches[0].off_len > 0);

  orion_cheat_file_clear(&file);
  return 0;
}

int test_factory_extension_routing() {
  auto json = CheatParserFactory::createByFormat("json");
  auto shn = CheatParserFactory::createByFormat(".shn");
  auto mc4 = CheatParserFactory::createByFormat("MC4");
  auto shnext = CheatParserFactory::createByFormat("ShnExt");

  TEST_ASSERT_STREQ("json", json->name());
  TEST_ASSERT_STREQ("shn", shn->name());
  TEST_ASSERT_STREQ("mc4", mc4->name());
  TEST_ASSERT_STREQ("ShnExt", shnext->name());
  return 0;
}

} // namespace

extern "C" int test_cheat_parsers_suite(void) {
  int failures = 0;

  failures += orion_test_run("cheat json parse success",
                             test_parse_json_buffer_success);
  failures += orion_test_run("cheat json rejects invalid hex",
                             test_parse_json_buffer_rejects_invalid_hex);
  failures += orion_test_run("cheat json ignores out-of-range section",
                             test_parse_json_ignores_out_of_range_section);
  failures +=
      orion_test_run("cheat xml parse success", test_parse_xml_buffer_success);
  failures += orion_test_run("cheat xml ignores out-of-range section",
                             test_parse_xml_ignores_out_of_range_section);
  failures +=
      orion_test_run("cheat mc4 load success", test_load_file_mc4_success);
  failures +=
      orion_test_run("fixture json file loads", test_fixture_json_file_loads);
  failures +=
      orion_test_run("fixture shn file loads", test_fixture_shn_file_loads);
  failures +=
      orion_test_run("fixture mc4 file loads", test_fixture_mc4_file_loads);
  failures += orion_test_run("fixture shnext file loads",
                             test_fixture_shnext_file_loads);
  failures += orion_test_run("parser factory extension routing",
                             test_factory_extension_routing);
  return failures;
}
