#include <cstdio>

extern "C" int test_cheat_utils_suite(void);
extern "C" int test_cheat_parsers_suite(void);
extern "C" int test_cheat_flatten_suite(void);
extern "C" int test_plugin_suite(void);
extern "C" int test_base64_suite(void);
extern "C" int test_aes_cbc_suite(void);
extern "C" int test_hde64_suite(void);
extern "C" int test_http_github_suite(void);
extern "C" int test_reg_entity_suite(void);
extern "C" int test_account_id_b64_suite(void);
extern "C" int test_playtime_suite(void);
extern "C" int test_settings_suite(void);
extern "C" int test_ready_suite(void);
extern "C" int test_platform_fs_suite(void);
extern "C" int test_platform_log_suite(void);
extern "C" int test_platform_notify_suite(void);
extern "C" int test_msg_protocol_suite(void);
extern "C" int test_ipc_harden_suite(void);
extern "C" int test_toolbox_timing_suite(void);
extern "C" int test_hijack_retry_suite(void);
extern "C" int test_ps5_settings_ui_suite(void);
extern "C" int test_toolbox_route_suite(void);
extern "C" int test_toolbox_helpers_suite(void);

int main() {
  int failures = 0;

  failures += test_cheat_utils_suite();
  failures += test_cheat_parsers_suite();
  failures += test_cheat_flatten_suite();
  failures += test_plugin_suite();
  failures += test_base64_suite();
  failures += test_aes_cbc_suite();
  failures += test_hde64_suite();
  failures += test_http_github_suite();
  failures += test_reg_entity_suite();
  failures += test_account_id_b64_suite();
  failures += test_playtime_suite();
  failures += test_settings_suite();
  failures += test_ready_suite();
  failures += test_platform_fs_suite();
  failures += test_platform_log_suite();
  failures += test_platform_notify_suite();
  failures += test_msg_protocol_suite();
  failures += test_ipc_harden_suite();
  failures += test_toolbox_timing_suite();
  failures += test_hijack_retry_suite();
  failures += test_ps5_settings_ui_suite();
  failures += test_toolbox_route_suite();
  failures += test_toolbox_helpers_suite();

  if (failures == 0) {
    std::fprintf(stderr, "All util host tests passed.\n");
    return 0;
  }

  std::fprintf(stderr, "%d util host tests failed.\n", failures);
  return 1;
}
