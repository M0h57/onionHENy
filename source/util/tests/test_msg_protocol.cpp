/* Host tests for shared IPC protocol surface (msg.hpp + reply body). */
#include "test_harness.h"

#include <msg.hpp>
#include <orion/ipc_server.hpp>

#include <cstring>
#include <string>
#include <type_traits>

static int test_ipc_paths_and_magic(void) {
  TEST_ASSERT_STREQ("/system_tmp/OrionHEN_crit_service", CRIT_IPC_SOC);
  TEST_ASSERT_STREQ("/system_tmp/OrionHEN_util_service", UTIL_IPC_SOC);
  TEST_ASSERT_EQ_INT(0x1000, DAEMON_BUFF_MAX);

  IPCMessage msg{};
  TEST_ASSERT_EQ_INT(0xDEADBABE, msg.magic);
  TEST_ASSERT_EQ_INT(0, msg.error);
  TEST_ASSERT_EQ_U64(sizeof(msg.msg), (unsigned long long)DAEMON_BUFF_MAX);
  return 0;
}

static int test_crit_command_base_and_order(void) {
  TEST_ASSERT_EQ_U64(0x9000000u, static_cast<unsigned>(BREW_TEST_CONNECTION));
  TEST_ASSERT_EQ_U64(0x9000002u, static_cast<unsigned>(BREW_RETURN_VALUE));
  /* Sequential after RETURN_VALUE */
  TEST_ASSERT_EQ_U64(static_cast<unsigned>(BREW_RETURN_VALUE) + 1u,
                     static_cast<unsigned>(BREW_REMOUNT_FOLDER));
  TEST_ASSERT_EQ_U64(static_cast<unsigned>(BREW_ENABLE_TOOLBOX),
                     static_cast<unsigned>(BREW_CHMOD_DIR) - 1u);
  return 0;
}

static int test_util_command_base(void) {
  TEST_ASSERT_EQ_U64(0x8000000u, static_cast<unsigned>(BREW_UTIL_TEST_CONNECTION));
  TEST_ASSERT_EQ_U64(0x8000002u, static_cast<unsigned>(BREW_UTIL_RETURN_VALUE));
  TEST_ASSERT_TRUE(static_cast<unsigned>(BREW_UTIL_UNUSED_DPI) >
                   static_cast<unsigned>(BREW_UTIL_RETURN_VALUE));
  return 0;
}

static int test_special_commands_stable(void) {
  /* Compatibility ordinals — do not renumber casually. */
  TEST_ASSERT_EQ_U64(0xE1F1D8u, static_cast<unsigned>(BREW_UTIL_LAUNCH_ELFLDR));
  TEST_ASSERT_EQ_U64(0xC0FFEEu, static_cast<unsigned>(BREW_RELOAD_SETTINGS));
  TEST_ASSERT_EQ_U64(0xDEAD0001u, static_cast<unsigned>(BREW_KILL_DAEMON));
  TEST_ASSERT_EQ_U64(0xDEADCAFEu, static_cast<unsigned>(BREW_FORCE_KILL_PID));
  return 0;
}

static int test_ipc_ret_distinct(void) {
  TEST_ASSERT_EQ_INT(0, static_cast<int>(IPC_Ret::NO_ERROR));
  TEST_ASSERT_EQ_INT(-1, static_cast<int>(IPC_Ret::INVALID));
  TEST_ASSERT_EQ_INT(-2, static_cast<int>(IPC_Ret::OPERATION_FAILED));
  TEST_ASSERT_TRUE(static_cast<int>(IPC_Ret::INVALID) !=
                   static_cast<int>(IPC_Ret::OPERATION_FAILED));
  return 0;
}

static int test_message_pod_layout(void) {
  IPCMessage a{};
  a.cmd = BREW_ENABLE_TOOLBOX;
  a.error = 1;
  std::snprintf(a.msg, sizeof(a.msg), "%s", R"({"titleId":"ETAH00002"})");
  TEST_ASSERT_TRUE(std::strstr(a.msg, "ETAH00002") != nullptr);
  TEST_ASSERT_EQ_U64(static_cast<unsigned>(BREW_ENABLE_TOOLBOX),
                     static_cast<unsigned>(a.cmd));
  return 0;
}

static int test_ipc_format_reply_body(void) {
  using orion::ipc_format_reply_body;
  std::string ok = ipc_format_reply_body(false, "Nothing");
  std::string err = ipc_format_reply_body(true, "fail");
  std::string tid = ipc_format_reply_body(false, "CUSA12345");
  TEST_ASSERT_STREQ("{\"res\":0, \"var\":\"Nothing\"}", ok.c_str());
  TEST_ASSERT_STREQ("{\"res\":-1, \"var\":\"fail\"}", err.c_str());
  TEST_ASSERT_STREQ("{\"res\":0, \"var\":\"CUSA12345\"}", tid.c_str());
  return 0;
}

extern "C" int test_msg_protocol_suite(void) {
  int failures = 0;
  failures += orion_test_run("ipc_paths_magic", test_ipc_paths_and_magic);
  failures += orion_test_run("crit_cmd_base_order", test_crit_command_base_and_order);
  failures += orion_test_run("util_cmd_base", test_util_command_base);
  failures += orion_test_run("special_cmds_stable", test_special_commands_stable);
  failures += orion_test_run("ipc_ret_distinct", test_ipc_ret_distinct);
  failures += orion_test_run("message_pod_layout", test_message_pod_layout);
  failures += orion_test_run("ipc_format_reply_body", test_ipc_format_reply_body);
  return failures;
}
