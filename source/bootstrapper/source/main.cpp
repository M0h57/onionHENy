/* Copyright (C) 2025 OrionHEN / LightningMods

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */


/******************************************************************************
 * Standard and System Header Includes
 ******************************************************************************/
 #include <csignal>
 #include <dirent.h>
 #include <errno.h>
 #include <fcntl.h>
 #include <netinet/in.h>
 #include <pthread.h>
 #include <setjmp.h>
 #include <stdarg.h>
 #include <stdbool.h>
 #include <stdint.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <sys/_iovec.h>
 #include <sys/mount.h>
 #include <sys/signal.h>
 #include <sys/socket.h>
 #include <sys/stat.h>
 #include <sys/sysctl.h>
 #include <sys/types.h>
 #include <sys/un.h>
 #include <sys/wait.h>
 #include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <orion/ready.h>
#include <orion/platform.h>
#include <orion/notify.h>
#include <orion/platform.h>
#include <orion/proc_query.h>
#include <orion/plugin.h>
#include <errno.h>
 
 /******************************************************************************
  * Custom Header Includes
  ******************************************************************************/
 #include <util.hpp>
 #include <freebsd-helper.h>
 #include <elfldr_remote.h>
 
 extern "C" {
 #include "elfldr.h"
 #include "faulthandler.h"
 #include "hbldr.h"
 #include "pt.h"
 #include <ps5/klog.h>
 #include <ps5/kernel.h>

 int sceKernelMprotect(void* addr, size_t len, int prot);

 extern uint8_t kstuff_start[];
 extern const unsigned int kstuff_size;

 extern uint8_t fps_prx_start[];
 extern const unsigned int fps_prx_size;

int sceNotificationSend(int userId, bool isLogged, const char* payload);
 }

 
const char json_payload[] =
     "{\n"
     "  \"rawData\": {\n"
     "    \"viewTemplateType\": \"InteractiveToastTemplateB\",\n"
     "    \"channelType\": \"Downloads\",\n"
     "    \"useCaseId\": \"IDC\",\n"
     "    \"toastOverwriteType\": \"No\",\n"
     "    \"isImmediate\": true,\n"
     "    \"priority\": 100,\n"
     "    \"viewData\": {\n"
     "      \"icon\": {\n"
     "        \"type\": \"Url\",\n"
     "        \"parameters\": {\n"
     "          \"url\": \"/user/data/OrionHEN/orionhen.png\"\n"
     "        }\n"
     "      },\n"
     "      \"message\": {\n"
     "        \"body\": \"OrionHEN is starting...\"\n"
     "      },\n"
     "      \"subMessage\": {\n"
     "        \"body\": \"Please Wait For The Welcome Message\"\n"
     "      },\n"
     "      \"actions\": [\n"
     "        {\n"
     "          \"actionName\": \"Go to Debug Settings\",\n"
     "          \"actionType\": \"DeepLink\",\n"
     "          \"defaultFocus\": true,\n"
     "          \"parameters\": {\n"
     "            \"actionUrl\": \"pssettings:play?function=debug_settings_old\"\n"
     "          }\n"
     "        }\n"
     "      ]\n"
     "    },\n"
     "    \"platformViews\": {\n"
     "      \"previewDisabled\": {\n"
     "        \"viewData\": {\n"
     "          \"icon\": {\n"
     "            \"type\": \"Predefined\",\n"
     "            \"parameters\": {\n"
     "              \"icon\": \"download\"\n"
     "            }\n"
     "          },\n"
     "          \"message\": {\n"
     "            \"body\": \"OrionHEN is starting...\"\n"
     "          }\n"
     "        }\n"
     "      }\n"
     "    }\n"
     "  },\n"
     "  \"createdDateTime\": \"2025-12-14T03:14:51.473Z\",\n"
     "  \"localNotificationId\": \"588193127\"\n"
     "}";
 
 /******************************************************************************
  * Macros and Constants
  ******************************************************************************/
 #define QAFLAGS_SIZE 16
 #define USER_SERVICE_ID 0x80000011
 #define SYSTEM_SERVICE_ID 0x80000010
 #define LNC_UTIL_ERROR_ALREADY_RUNNING 0x8094000c
 #define LNC_ERROR_APP_NOT_FOUND 0x80940031
 #define ENTRYPOINT_OFFSET 0x70
 
 #define PROCESS_LAUNCHED 1
 
 #define LOOB_BUILDER_SIZE 21
 #define LOOP_BUILDER_TARGET_OFFSET 3
 
 #define USLEEP_NID "QcteRwbsnV0"
 
 #define LOOKUP_SYMBOL(resolver, sym) \
   resolver_lookup_symbol(resolver, sym, strlen(sym))
   
 #define SET_FUNCTION_ADDRESS(resolver, function) \
   *(void **)&(function) = \
       (void *)LOOKUP_SYMBOL(resolver, #function) /* NOLINT */
 
 #define BUILD_IOVEC(str) \
   { .iov_base = (str), .iov_length = __builtin_strlen(str) + 1 }
 
 /******************************************************************************
  * Type Definitions and Structures
  ******************************************************************************/
 typedef struct {
   int32_t type;             // 0x00
   int32_t req_id;           // 0x04
   int32_t priority;         // 0x08
   int32_t msg_id;           // 0x0C
   int32_t target_id;        // 0x10
   int32_t user_id;          // 0x14
   int32_t unk1;             // 0x18
   int32_t unk2;             // 0x1C
   int32_t app_id;           // 0x20
   int32_t error_num;        // 0x24
   int32_t unk3;             // 0x28
   char use_icon_image_uri;  // 0x2C
   char message[1024];       // 0x2D
   char uri[1024];           // 0x42D
   char unkstr[1024];        // 0x82D
 } OrbisNotificationRequest; // Size = 0xC30
 
 typedef enum {
   Flag_None = 0,
   SkipLaunchCheck = 1,
   SkipResumeCheck = 1,
   SkipSystemUpdateCheck = 2,
   RebootPatchInstall = 4,
   VRMode = 8,
   NonVRMode = 16,
   Pft = 32UL,
   RaIsConfirmed = 64UL,
   ShellUICheck = 128UL
 } Flag;
 
 typedef struct {
   uint32_t sz;
   int user_id;
   uint32_t app_opt;
   uint64_t crash_report;
   Flag check_flag;
 } LncAppParam;
 
 typedef struct {
   const void *iov_base;
   size_t iov_length;
 } iovec_t;
 
 typedef struct FileDescriptors {
   int fd = 1;
 } FileDescriptor;
 
 typedef struct {
   uint64_t pad0;
   char version_str[0x1C];
   uint32_t version;
   uint64_t pad1;
 } OrbisKernelSwVersion;
 
 typedef struct app_info {
   uint32_t app_id;
   uint64_t unknown1;
   uint32_t app_type;
   char     title_id[10];
   char     unknown2[0x3c];
 } app_info_t;
 
 /******************************************************************************
  * External Declarations
  ******************************************************************************/
 extern "C" {
     int sceKernelSendNotificationRequest(int32_t device,
                                          OrbisNotificationRequest *req,
                                          size_t size, int32_t blocking);
     int sceUserServiceGetForegroundUser(uint32_t *userId);
     int sceLncUtilLaunchApp(const char *tid, const char *argv[],
                             LncAppParam *param);
     uint32_t sceLncUtilKillApp(uint32_t appId);
     int sceSystemServiceGetAppId(const char *titleId);
     int sceUserServiceInitialize(void *param);
     int sceKernelGetProsperoSystemSwVersion(OrbisKernelSwVersion *sw);
     int unmount(const char *path, int flags);
     int sceKernelGetAppInfo(int pid, app_info_t *title);
     int sceKernelGetProcessName(int pid, char *name);
     int sceKernelGetOpenPsIdForSystem(void *psid);
     int sceKernelIsGenuineDevKit();

     bool devkit_byepervisor(void);
     void notify(const char *text, ...) {
     va_list args;
     va_start(args, text);
     orion_notify_v(/*show_watermark=*/0, text, args);
     va_end(args);
 }

    
 }
 
 extern int _write(int fd, const void *, size_t); // NOLINT
 extern ssize_t _read(int, void *, size_t);       // NOLINT
 
 extern const unsigned int daemon_size;
 extern uint8_t daemon_start[];
 extern uint8_t util_start[];
 extern const unsigned int util_size;
 extern uint8_t sicon_start[];
 extern const unsigned int sicon_size;

 extern uint8_t icon_xml_package_start[];
 extern const unsigned int icon_xml_package_size;
 extern uint8_t icon_xml_plugins_start[];
 extern const unsigned int icon_xml_plugins_size;
 extern uint8_t icon_xml_game_start[];
 extern const unsigned int icon_xml_game_size;
 extern uint8_t icon_xml_network_start[];
 extern const unsigned int icon_xml_network_size;
 extern uint8_t icon_xml_settings_start[];
 extern const unsigned int icon_xml_settings_size;
 extern uint8_t icon_xml_shortcuts_start[];
 extern const unsigned int icon_xml_shortcuts_size;
 extern uint8_t icon_xml_debug_start[];
 extern const unsigned int icon_xml_debug_size;
 extern uint8_t icon_xml_about_start[];
 extern const unsigned int icon_xml_about_size;
 
 /******************************************************************************
  * Global Variables
  ******************************************************************************/
 int plugin_count = 0;
 char buff[255];
 char **loaded_filenames = NULL;
 jmp_buf g_catch_buf;
 FileDescriptor sock;
 
 // Constants
 /* Must NOT use 9021 — that is external elfldr. */
 static const int LOGGER_PORT = 9088;
 static const int STDOUT = 1;
 static const int STDERR = 2;
 
 /******************************************************************************
  * Function Prototypes
  ******************************************************************************/
 void write_embedded_assets();
 void notify(const char *text, ...);
static void cleanup(void);
 FileDescriptor FileDescriptor_init(int fd);
 int initStdout();
 void release(FileDescriptor *fd);
 void patch_app_db(void);
 static bool remount(const char *dev, const char *path);
 
 /******************************************************************************
  * Function Implementations
  ******************************************************************************/
 extern uint8_t shellui_prx_start[];
 extern const unsigned int shellui_prx_size;

  static void write_blob_file(const char *path, const void *data, size_t size) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
      perror("open failed");
      return;
    }
    if (write(fd, data, size) == -1) {
      perror("write failed");
    }
    close(fd);
  }

  void write_embedded_assets() {
    mkdir("/data/OrionHEN/", 0777);
    mkdir("/data/OrionHEN/assets/", 0777);
#if 0
    int fd = open("/system_ex/common_ex/lib/shell.prx", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        perror("open failed");
        return;
    }
    if (write(fd, &shellui_prx_start, shellui_prx_size) == -1) {
        perror("write failed");
        return;
    }
    close(fd);
#endif
#if 0
   /// if (!if_exists("/data/OrionHEN/fps.prx")) {
        int fd = open("/data/OrionHEN/fps.prx", O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (fd == -1) {
            perror("open failed");
            return;
        }
        if (write(fd, &fps_prx_start, fps_prx_size) == -1) {
            perror("write failed");
        }
        close(fd);
  //  }
#endif

    if (!if_exists("/data/OrionHEN/orionhen.png")) {
      write_blob_file("/data/OrionHEN/orionhen.png", &sicon_start, sicon_size);
    }

    // Toolbox category icons (always overwrite so asset updates take effect)
    write_blob_file("/data/OrionHEN/assets/icon_xml_package.png", &icon_xml_package_start, icon_xml_package_size);
    write_blob_file("/data/OrionHEN/assets/icon_xml_plugins.png", &icon_xml_plugins_start, icon_xml_plugins_size);
    write_blob_file("/data/OrionHEN/assets/icon_xml_game.png", &icon_xml_game_start, icon_xml_game_size);
    write_blob_file("/data/OrionHEN/assets/icon_xml_network.png", &icon_xml_network_start, icon_xml_network_size);
    write_blob_file("/data/OrionHEN/assets/icon_xml_settings.png", &icon_xml_settings_start, icon_xml_settings_size);
    write_blob_file("/data/OrionHEN/assets/icon_xml_shortcuts.png", &icon_xml_shortcuts_start, icon_xml_shortcuts_size);
    write_blob_file("/data/OrionHEN/assets/icon_xml_debug.png", &icon_xml_debug_start, icon_xml_debug_size);
    write_blob_file("/data/OrionHEN/assets/icon_xml_about.png", &icon_xml_about_start, icon_xml_about_size);
 
    if (!if_exists("/system_ex/rnps/apps/NPXS40008/assets/src/modules/categoriesList/assets/texture/orionhen_sicon.png")) {
      write_blob_file("/system_ex/rnps/apps/NPXS40008/assets/src/modules/categoriesList/assets/texture/orionhen_sicon.png",
                      &sicon_start, sicon_size);
    }
 
    if (!if_exists("/mnt/rnps/apps/NPXS40008/assets/src/modules/categoriesList/assets/texture/orionhen_sicon.png")) {
      write_blob_file("/mnt/rnps/apps/NPXS40008/assets/src/modules/categoriesList/assets/texture/orionhen_sicon.png",
                      &sicon_start, sicon_size);
    }
}

  bool is_elf_header(uint8_t* data)
  {
      uint8_t header[] = { 0x7f, 'E', 'L', 'F' };

      return !memcmp(data, header, 4);
  }


  uint8_t* get_kstuff_address(bool& require_cleanup) {
      const char* path = "/data/OrionHEN/kstuff.elf";
      long offset = 0;
      off_t size;
      uint8_t* address;
      int fd;

      if (!if_exists(path)) {
          goto embedded_kstuff;
      }

      fd = open(path, O_RDONLY);
      if (fd <= 0) {
          goto embedded_kstuff;
      }

      size = lseek(fd, 0, SEEK_END);
      address = (uint8_t*)malloc(size);

      if (!address) {
          goto close_fd;
      }

      lseek(fd, 0, SEEK_SET);

      while (offset != size) {
          int n = read(fd, address + offset, size - offset);

          if (n <= 0)
          {
              goto free_mem;
          }

          offset += n;
      }

      if (!is_elf_header(address)) {
          notify( "Kstuff '%s' doesn't have ELF header.", path);
          goto free_mem;
      }

      require_cleanup = true;
      notify("Loading kstuff from: %s", path);
      return address;

  free_mem:
      free(address);
  close_fd:
      close(fd);
  embedded_kstuff:
      require_cleanup = false;
      return kstuff_start;
  }
 
 static bool remount(const char *dev, const char *path) {
   iovec_t iov[] = {BUILD_IOVEC("fstype"),    BUILD_IOVEC("exfatfs"),
                    BUILD_IOVEC("fspath"),    BUILD_IOVEC(path),
                    BUILD_IOVEC("from"),      BUILD_IOVEC(dev),
                    BUILD_IOVEC("large"),     BUILD_IOVEC("yes"),
                    BUILD_IOVEC("timezone"),  BUILD_IOVEC("static"),
                    BUILD_IOVEC("async"),     {NULL, 0},
                    BUILD_IOVEC("ignoreacl"), {NULL, 0}};
   return nmount((struct iovec *)iov, sizeof(iov) / sizeof(iov[0]),
                 MNT_UPDATE) == 0;
 }
 static void cleanup(void) { 
    if (sock.fd != -1) {
      close(sock.fd);
      sock.fd = -1;
    }
  
    // Notify user about cleanup
    notify("OrionHEN has been cleaned up.");
  
    // Exit the program
    exit(0);
 }
 
 // FileDescriptor methods implementations
 FileDescriptor FileDescriptor_init(int fd) {
   FileDescriptor newFd;
   newFd.fd = fd;
   return newFd;
 }
 
 void release(FileDescriptor *fd) { 
   fd->fd = -1; 
 }
 
 // Stdout initialization logic
 int initStdout() {
   // Check for logging file existence logic here
   // For simplicity, I'm assuming it always exists
   char error_msg[500] = {0};
 
   sock.fd = -1;
   sock = FileDescriptor_init(socket(AF_INET, SOCK_STREAM, 0));
   if (sock.fd == -1) {
     snprintf(error_msg, sizeof(error_msg), "Failed to create socket: %s",
              strerror(errno));
     notify(error_msg);
     return -1;
   }
 
   int value = 1;
   if (setsockopt(sock.fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
     snprintf(error_msg, sizeof(error_msg), "Failed to set socket options: %s",
              strerror(errno));
     notify(error_msg);
     return -1;
   }
 
   struct sockaddr_in server_addr;
   (void)memset(&server_addr, 0, sizeof(server_addr));
   server_addr.sin_family = AF_INET;
   server_addr.sin_port = htons(LOGGER_PORT);
   server_addr.sin_addr.s_addr = 0;
 
   if (bind(sock.fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
     snprintf(error_msg, sizeof(error_msg), "Failed to bind socket: %s",
              strerror(errno));
     notify(error_msg);
     return -1;
   }
 
   if (listen(sock.fd, 1) != 0) {
     snprintf(error_msg, sizeof(error_msg), "Failed to listen on socket: %s",
              strerror(errno));
     notify(error_msg);
     return -1;
   }
 
   struct sockaddr client_addr;
   socklen_t addr_len = sizeof(client_addr);
   int conn = accept(sock.fd, &client_addr, &addr_len);
   if (conn != -1) {
     dup2(conn, STDOUT);
     dup2(conn, STDERR);
     close(conn);
     return conn;
   }
 
   snprintf(error_msg, sizeof(error_msg), "Failed to accept connection: %s",
            strerror(errno));
   notify(error_msg);
   return -1;
 }
 
 // Function to check if the file buffer contains a valid custom plugin header
 bool load_plugin(const char *path, const char *filename)
{
  orion_plugin_load_opts opts = {};
  opts.auto_delete_eorr37000 = 1;
  opts.always_succeed_after_launch = 1;
  return orion_plugin_load(path, filename, &opts);
}

/*=================== LOAD PLUGINS =========================*/
char **find_plugin_files() {
  const char *base_dirs[] = {
    // Plugin directories
    "/mnt/usb0/orionhen/plugins", "/mnt/usb0/OrionHEN/plugins",
    "/mnt/usb1/orionhen/plugins", "/mnt/usb2/orionhen/plugins",
    "/mnt/usb3/orionhen/plugins", "/user/data/OrionHEN/plugins",
    "/user/data/orionhen/plugins",
    
    // Payload directories
    "/mnt/usb0/orionhen/payloads", "/mnt/usb0/OrionHEN/payloads",
    "/mnt/usb1/orionhen/payloads", "/mnt/usb2/orionhen/payloads",
    "/mnt/usb3/orionhen/payloads", "/user/data/OrionHEN/payloads",
    "/user/data/orionhen/payloads"
};

  int base_dirs_count = sizeof(base_dirs) / sizeof(base_dirs[0]);

  char **plugin_paths = NULL;
  char full_path[255];
  char auto_start_path[255];
  plugin_count = 0;
  loaded_filenames = (char **)malloc(255 * sizeof(char *));

  for (int i = 0; i < base_dirs_count; i++) {
    DIR *dir = opendir(base_dirs[i]);
    if (dir) {
      struct dirent *entry;
      while ((entry = readdir(dir)) != NULL) {
        (void)memset(full_path, 0, sizeof(full_path));
        if (entry->d_type == DT_REG) { // Regular file
          const char *ext = strrchr(entry->d_name, '.');
          if (ext && (strcmp(ext, ".plugin") == 0 || strcmp(ext, ".elf") == 0)) {
            bool skip = false;
            // Construct full path
            snprintf(full_path, sizeof(full_path), "%s/%s", base_dirs[i],
                     entry->d_name);
            snprintf(auto_start_path, sizeof(auto_start_path),
                     "%s/%s.auto_start", base_dirs[i], entry->d_name);

            if (!if_exists(auto_start_path)) {
              printf("skipping auto start for plugin: %s\n", full_path);
              continue;
            }

            for (int j = 0; j < plugin_count; j++) {
              if (strcmp(loaded_filenames[j], entry->d_name) == 0) {
                skip = true;
                // Only print the message for /data/OrionHEN/plugins/elfldr.plugin
                // as per specific requirement
                if ((strcmp(base_dirs[i], "/data/OrionHEN/plugins") == 0) || (strcmp(entry->d_name, "/data/OrionHEN/payloads") == 0)) {
                  printf("skipping duplicate plugin: %s | already loaded: %s\n",
                         full_path, loaded_filenames[j]);
                }
                break;
              }
            }
            if (skip)
              continue;

            // Add to array
            plugin_paths = (char **)realloc(plugin_paths, (plugin_count + 1) *
                                                              sizeof(char *));
            plugin_paths[plugin_count] = strdup(full_path);

            // Copy filename to loaded_filenames
            loaded_filenames[plugin_count] =
                strdup(entry->d_name); // Use strdup for simplicity
            plugin_count++;
          }
        }
      }
      closedir(dir);
    }
  }

  return plugin_paths;
}
void free_plugin_files(char **plugin_files) {
  // Free memory for loaded_filenames
  for (int i = 0; i < plugin_count; i++) {
    free(loaded_filenames[i]);
  }
  free(loaded_filenames);

  for (int i = 0; i < plugin_count; i++) {
    free((void *)plugin_files[i]);
  }
  free((void *)plugin_files);
}

int main(void) {
  signal(SIGCHLD, SIG_IGN);

  klog_puts("Jailbreaking the boostrapper ...");
  if (elfldr_raise_privileges(getpid())) {
    notify("Unable to raise privileges");
    return -1;
  }
  klog_printf("   Success!\n");

  if (if_exists("/data/I_want_logging_for_orionhen")) {
    klog_printf("Redirecting stdout and stderr to logger ...");
    if (initStdout() >= 0)
      klog_puts("   Success!");
    else
      klog_puts("   Failed!");
  }

  OrbisKernelSwVersion sys_ver;
  sceKernelGetProsperoSystemSwVersion(&sys_ver);

  // Byepervisor (1.xx–2.xx HV path) removed from OrionHEN.
  if (sys_ver.version < 0x3000000 && !sceKernelIsGenuineDevKit()) {
    klog_printf("FW %s is < 3.00 and Byepervisor is not bundled; continuing without HV path\n",
                sys_ver.version_str);
  }

  klog_puts("============== Spawner (Bootstrapper) Started =================");

  // Directory layout
  mkdir("/data/OrionHEN", 0777);
  mkdir("/data/OrionHEN/plugins", 0777);
  mkdir("/data/OrionHEN/payloads", 0777);
  mkdir("/data/OrionHEN/daemons", 0777);
  mkdir("/data/OrionHEN/assets", 0777);
  mkdir("/data/OrionHEN/games", 0777);

  klog_printf("Registering signal handler ...");
  fault_handler_init(cleanup);
  klog_printf("   Success!\n");

  klog_printf("Remounting system partitions ...");
  if (!remount("/dev/ssd0.system_ex", "/system_ex")) {
    perror("failed to mount /system_ex\nif you see this reboot");
    notify("failed to mount /system_ex\nif you see this reboot");
    return -1;
  }
  if (!remount("/dev/ssd0.system", "/system")) {
    perror("failed to mount /system_\nif you see this reboot");
    notify("failed to mount /system\nif you see this reboot");
    return -1;
  }
  klog_printf("   Success!\n");

  klog_printf("Writing embedded assets ...");
  write_embedded_assets();
  klog_printf("   Written!\n");

  klog_printf("Unmounting /update forcefully ...");
  unlink("/update/PS5UPDATE.PUP");
  unlink("/update/PS5UPDATE.PUP.net.temp");
  if ((int)unmount("/update", 0x80000LL) < 0)
    unmount("/update", 0);
  klog_puts("   Success!");

  /*
   * Launch policy: NO local spawn.
   * 1) Write util/daemon/kstuff to disk first.
   * 2) Ask external elfldr :9021 via file: URI (one at a time, wait between).
   */
  char buz[100] = {0};

  auto write_elf_file = [](const char *path, const uint8_t *elf,
                           size_t size) -> bool {
    mkdir("/data/OrionHEN", 0777);
    mkdir("/data/OrionHEN/daemons", 0777);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd < 0)
      return false;
    size_t off = 0;
    while (off < size) {
      ssize_t n = write(fd, elf + off, size - off);
      if (n <= 0) {
        close(fd);
        return false;
      }
      off += (size_t)n;
    }
    close(fd);
    return true;
  };

  auto kill_by_name = [](const char *a, const char *b) {
    int p = -1;
    while ((p = orion_find_pid_substr(a)) > 0 ||
           (p = orion_find_pid_substr(b)) > 0) {
      kill(p, SIGKILL);
      sleep(1);
    }
  };

  auto launch_path = [](const char *path, const char *label,
                        const char *wait_name) -> bool {
    klog_printf("9021 file: %s (%s)\n", path, label);
    if (!elfldr_remote_send_file_uri(path)) {
      klog_printf("  send FAILED %s\n", label);
      return false;
    }
    for (int i = 0; i < 30; i++) {
      if (wait_name && orion_find_pid_substr(wait_name) > 0) {
        klog_printf("  running: %s\n", wait_name);
        return true;
      }
      sleep(1);
    }
    klog_printf("  sent %s (process name not seen yet, continuing)\n", label);
    return true;
  };

  klog_puts("Writing daemon ELFs to /data/OrionHEN/daemons ...");
  if (!write_elf_file("/data/OrionHEN/daemons/util.elf", util_start, util_size)) {
    notify("failed to write util.elf");
    return -2;
  }
  if (!write_elf_file("/data/OrionHEN/daemons/daemon.elf", daemon_start, daemon_size)) {
    notify("failed to write daemon.elf");
    return -2;
  }
  if (!if_exists("/data/OrionHEN/kstuff.elf")) {
    (void)write_elf_file("/data/OrionHEN/daemons/kstuff.elf", kstuff_start,
                         (size_t)kstuff_size);
  }
  klog_puts("   Daemon ELFs written");

  if (!elfldr_remote_available()) {
    klog_puts("FATAL: no elfldr on 127.0.0.1:9021");
    notify("Start elfldr on 9021 first, then re-run. ELFs are on disk under /data/OrionHEN/daemons/");
    return -2;
  }
  klog_puts("elfldr :9021 OK - launching via file URI (serialized)");
  sleep(3); /* settle after remount/unmount */

  /*
   * Order: util → kstuff → daemon
   * (daemon injects toolbox; kstuff must patch ShellUI first)
   */
  klog_puts("Starting util via 9021 ...");
  kill_by_name("util.elf", "OrionHEN Utility");
  orion_ready_clear(ORION_READY_UTIL);
  orion_ready_clear(ORION_READY_KSTUFF);
  orion_ready_clear(ORION_READY_DAEMON);
  orion_ready_clear(ORION_READY_TOOLBOX);

  if (!launch_path("/data/OrionHEN/daemons/util.elf", "util", "util.elf")) {
    notify("failed to launch util via elfldr :9021");
    return -2;
  }
  if (!orion_ready_wait(ORION_READY_UTIL, /*timeout_ms=*/15000, /*poll_ms=*/200))
    klog_puts("util ready timeout — continuing (process may still be starting)");

  const bool skip_kstuff =
      if_exists("/mnt/usb0/no_kstuff") || if_exists("/data/OrionHEN/no_kstuff");
  if (skip_kstuff) {
    klog_puts("kstuff disabled via no_kstuff file");
    orion_ready_signal(ORION_READY_KSTUFF);
  } else if (sys_ver.version >= 0x3000000) {
    klog_puts("Loading kstuff via 9021 (before daemon/toolbox) ...");
    const char *kpath = if_exists("/data/OrionHEN/kstuff.elf")
                            ? "/data/OrionHEN/kstuff.elf"
                            : "/data/OrionHEN/daemons/kstuff.elf";
    if (launch_path(kpath, "kstuff", "kstuff.elf")) {
      int wait = 0;
      bool not_loaded = true;
      while ((not_loaded = (sceKernelMprotect(&buz[0], 100, 0x7) < 0))) {
        if (wait++ > 15) {
          notify("Failed to load kstuff, continuing without it");
          break;
        }
        sleep(1);
      }
      if (!not_loaded) {
        klog_puts("kstuff mprotect OK — signal ready");
        orion_ready_signal(ORION_READY_KSTUFF);
        sleep(1); /* brief settle for ShellUI trophy patches */
      }
    } else {
      notify("Failed to load kstuff via 9021, continuing");
    }
  } else {
    orion_ready_signal(ORION_READY_KSTUFF);
  }

  klog_puts("Starting daemon via 9021 (toolbox inject) ...");
  kill_by_name("daemon.elf", "OrionHEN Critical");
  orion_ready_clear(ORION_READY_DAEMON);
  if (!launch_path("/data/OrionHEN/daemons/daemon.elf", "daemon", "daemon.elf")) {
    notify("failed to launch daemon via elfldr :9021");
    return -2;
  }
  if (!orion_ready_wait(ORION_READY_DAEMON, /*timeout_ms=*/20000, /*poll_ms=*/200))
    klog_puts("daemon ready timeout — continuing");

  sceNotificationSend(0xFE, true, &json_payload[0]);

  // Autostart plugins (skip elfldr.plugin)
  char **plugin_paths = find_plugin_files();
  if (plugin_paths && plugin_count > 0) {
    int loaded_plugins = 0;
    for (int i = 0; i < plugin_count; i++) {
      if (strstr(plugin_paths[i], "elfldr") != nullptr)
        continue;
      klog_printf("Loading plugin: %s\n", plugin_paths[i]);
      if (!load_plugin(plugin_paths[i], loaded_filenames[i])) {
        snprintf(buff, sizeof(buff),
                 "[OrionHEN] Failed to load plugin!\nPath: %s",
                 plugin_paths[i]);
        notify(buff);
        klog_puts("FAILED!");
        continue;
      }
      klog_puts("Loaded!");
      loaded_plugins++;
    }
    klog_printf("Successfully loaded %d plugins\n", loaded_plugins);
    free_plugin_files(plugin_paths);
  }

  klog_puts("============== Spawner (Bootstrapper) Finished =================");
  return 0;
}
