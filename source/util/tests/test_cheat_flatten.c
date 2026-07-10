/* Host unit tests for cheat flat-name helpers (no PS5 FS required). */
#include "test_harness.h"

#include "cheats/runtime.h"

#include <string.h>

static int test_match_ext_known(void) {
  char ext[16];

  TEST_ASSERT_EQ_INT(1, orion_cheat_match_ext("CUSA12345_01.00.json", ext,
                                              sizeof(ext)));
  TEST_ASSERT_STREQ("json", ext);

  TEST_ASSERT_EQ_INT(1, orion_cheat_match_ext("PPSA00001_01.001.000.SHN", ext,
                                              sizeof(ext)));
  TEST_ASSERT_STREQ("shn", ext);

  TEST_ASSERT_EQ_INT(1, orion_cheat_match_ext("game.mc4", ext, sizeof(ext)));
  TEST_ASSERT_STREQ("mc4", ext);

  TEST_ASSERT_EQ_INT(1, orion_cheat_match_ext("x.ShnExt", ext, sizeof(ext)));
  TEST_ASSERT_STREQ("ShnExt", ext);

  TEST_ASSERT_EQ_INT(1, orion_cheat_match_ext("x.shnext", ext, sizeof(ext)));
  TEST_ASSERT_STREQ("ShnExt", ext);
  return 0;
}

static int test_match_ext_reject(void) {
  char ext[16] = "keep";

  TEST_ASSERT_EQ_INT(0, orion_cheat_match_ext("readme.txt", ext, sizeof(ext)));
  TEST_ASSERT_EQ_INT(0, orion_cheat_match_ext("json", ext, sizeof(ext)));
  TEST_ASSERT_EQ_INT(0, orion_cheat_match_ext(NULL, ext, sizeof(ext)));
  TEST_ASSERT_EQ_INT(0, orion_cheat_match_ext("", ext, sizeof(ext)));
  return 0;
}

static int test_flat_simple(void) {
  char out[128];

  TEST_ASSERT_EQ_INT(0, orion_cheat_build_flat_name("CUSA05786_01.04.json", out,
                                                   sizeof(out)));
  TEST_ASSERT_STREQ("CUSA05786_01.04.json", out);

  TEST_ASSERT_EQ_INT(
      0, orion_cheat_build_flat_name("PPSA01340_01.004.000.shn", out,
                                    sizeof(out)));
  TEST_ASSERT_STREQ("PPSA01340_01.004.000.shn", out);
  return 0;
}

static int test_flat_strips_process_suffix(void) {
  char out[128];

  /* GoldHEN style: TITLE_VER_process.json → drop process segment */
  TEST_ASSERT_EQ_INT(
      0, orion_cheat_build_flat_name("CUSA05786_01.04_eboot.bin.json", out,
                                    sizeof(out)));
  TEST_ASSERT_STREQ("CUSA05786_01.04.json", out);

  /* Game name before first '_' with non-version text → no digit version */
  TEST_ASSERT_EQ_INT(
      -1, orion_cheat_build_flat_name(
              "Assassins-Creed-Mirage_PPSA07230_01.012.000.ShnExt", out,
              sizeof(out)));
  return 0;
}

static int test_flat_shnext_with_tid_prefix(void) {
  char out[128];

  TEST_ASSERT_EQ_INT(
      0, orion_cheat_build_flat_name("PPSA07230_01.012.000_Aigars_Uze.ShnExt",
                                    out, sizeof(out)));
  TEST_ASSERT_STREQ("PPSA07230_01.012.000.ShnExt", out);
  return 0;
}

static int test_flat_uppercase_title(void) {
  char out[128];

  TEST_ASSERT_EQ_INT(
      0, orion_cheat_build_flat_name("cusa12345_1.00.json", out, sizeof(out)));
  TEST_ASSERT_STREQ("CUSA12345_1.00.json", out);
  return 0;
}

static int test_flat_dash_separator(void) {
  char out[128];

  TEST_ASSERT_EQ_INT(
      0, orion_cheat_build_flat_name("CUSA12345-01.00.json", out, sizeof(out)));
  TEST_ASSERT_STREQ("CUSA12345_01.00.json", out);
  return 0;
}

static int test_flat_reject(void) {
  char out[128];

  TEST_ASSERT_EQ_INT(-1, orion_cheat_build_flat_name("readme.txt", out,
                                                    sizeof(out)));
  TEST_ASSERT_EQ_INT(-1, orion_cheat_build_flat_name("nounderscore.json", out,
                                                    sizeof(out)));
  TEST_ASSERT_EQ_INT(-1, orion_cheat_build_flat_name("ab_01.json", out,
                                                    sizeof(out))); /* tid < 4 */
  TEST_ASSERT_EQ_INT(-1, orion_cheat_build_flat_name("CUSA12345_.json", out,
                                                    sizeof(out))); /* no ver */
  TEST_ASSERT_EQ_INT(-1, orion_cheat_build_flat_name(NULL, out, sizeof(out)));
  TEST_ASSERT_EQ_INT(-1, orion_cheat_build_flat_name("CUSA12345_1.0.json", NULL,
                                                    0));
  return 0;
}

static int test_normalize_version(void) {
  char out[32];

  orion_cheat_normalize_version("01.004.000", out, sizeof(out));
  TEST_ASSERT_STREQ("01.004.000", out);

  orion_cheat_normalize_version("1.0 (beta)", out, sizeof(out));
  TEST_ASSERT_STREQ("1.0__beta_", out);

  orion_cheat_normalize_version("a/b\\c", out, sizeof(out));
  TEST_ASSERT_STREQ("a_b_c", out);

  orion_cheat_normalize_version(NULL, out, sizeof(out));
  TEST_ASSERT_STREQ("", out);

  orion_cheat_normalize_version("x", out, 1); /* only room for NUL */
  TEST_ASSERT_STREQ("", out);

  orion_cheat_normalize_version("ab", out, 2); /* one char + NUL */
  TEST_ASSERT_STREQ("a", out);
  return 0;
}

int test_cheat_flatten_suite(void) {
  int failures = 0;
  failures += orion_test_run("flatten.match_ext_known", test_match_ext_known);
  failures += orion_test_run("flatten.match_ext_reject", test_match_ext_reject);
  failures += orion_test_run("flatten.flat_simple", test_flat_simple);
  failures += orion_test_run("flatten.flat_strips_process",
                             test_flat_strips_process_suffix);
  failures +=
      orion_test_run("flatten.flat_shnext_tid", test_flat_shnext_with_tid_prefix);
  failures +=
      orion_test_run("flatten.flat_uppercase", test_flat_uppercase_title);
  failures += orion_test_run("flatten.flat_dash", test_flat_dash_separator);
  failures += orion_test_run("flatten.flat_reject", test_flat_reject);
  failures +=
      orion_test_run("flatten.normalize_version", test_normalize_version);
  return failures;
}
