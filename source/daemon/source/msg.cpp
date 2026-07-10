/* Copyright (C) 2025 OrionHEN / LightningMods */

#include "daemon_ops.hpp"
#include <orion/platform.h>
#include <orion/ipc_server.hpp>
#include <msg.hpp>
#include <string>

bool is_handler_enabled = true;

static void handleIPC_adapt(orion::IpcClientArgs *client, std::string &msg,
                            DaemonCommands cmd) {
  handleIPC(client, msg, cmd);
}

static void ipc_server_log_line(const char *line) {
  OrionHEN_log("%s", line);
}

static orion::IpcServerOptions g_crit_ipc_opts = {
    CRIT_IPC_SOC,
    handleIPC_adapt,
    BREW_RETURN_VALUE,
    true,
    "crit",
};

void *IPC_loop(void *args) {
  (void)args;
  orion::ipc_server_set_log(ipc_server_log_line);
  return orion::ipc_server_loop(&g_crit_ipc_opts);
}
