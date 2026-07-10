/* Copyright (C) 2025 OrionHEN / LightningMods — OnPress misc (kstuff, RP, credits) */
#include "onpress.hpp"
#include <fstream>
#include <pthread.h>
#include <unistd.h>

void *kstuff_download_thread(void *args);
void *download_cheats_thr(void *);
extern std::string remote_play_info;

static OnPressResult id_download_kstuff(OnPressContext &ctx) {
  (void)ctx;
  pthread_t thr;
  pthread_create(&thr, nullptr, kstuff_download_thread, nullptr);
  pthread_detach(thr);
  return OnPressResult::Handled;
}

static OnPressResult id_kstuff_autoload(OnPressContext &ctx) {
  if (atol(ctx.value.c_str())) {
    unlink("/user/data/OrionHEN/no_kstuff");
    notify("Kstuff will be loaded on next boot");
  } else {
    touch_file("/user/data/OrionHEN/no_kstuff");
    notify("Kstuff will NOT be loaded on next boot");
  }
  return OnPressResult::Handled;
}

static OnPressResult id_delete_kstuff(OnPressContext &ctx) {
  (void)ctx;
  unlink("/user/data/OrionHEN/kstuff.elf");
  notify("The external kstuff download has been deleted");
  return OnPressResult::Handled;
}

static OnPressResult id_change_custom_pkg_path(OnPressContext &ctx) {
  custom_pkg_path.path = ctx.value;
  return OnPressResult::Handled;
}

static OnPressResult id_save_rp_info(OnPressContext &ctx) {
  (void)ctx;
  if (usbpath() == -1) {
    notify("Failed to save Remote Play info, USB not found");
    return OnPressResult::EarlyReturn;
  }
  std::string usb_rp_path =
      "/usb" + std::to_string(usbpath()) + "/remote_play_info.txt";
  shellui_log("Saving Remote Play info to %s", usb_rp_path.c_str());
  std::ofstream rp_file(usb_rp_path);
  if (!rp_file.is_open()) {
    notify("Failed to open Remote Play info file");
    return OnPressResult::EarlyReturn;
  }
  rp_file << remote_play_info;
  rp_file.close();
  notify("Remote Play info saved to /mnt%s", usb_rp_path.c_str());
  return OnPressResult::Handled;
}

static OnPressResult id_lm_test(OnPressContext &ctx) {
  (void)ctx;
  shellui_log("LM's Test Button Pressed");
  return OnPressResult::Handled;
}

static OnPressResult id_orionhen_credits(OnPressContext &ctx) {
  (void)ctx;
  return OnPressResult::EarlyReturn;
}

static OnPressResult id_dl_cheats(OnPressContext &ctx) {
  (void)ctx;
  pthread_t thr;
  pthread_create(&thr, nullptr, download_cheats_thr, nullptr);
  pthread_detach(thr);
  // Original returned oOnPress early without SaveSettings — preserve.
  ctx.dirty = false;
  return OnPressResult::EarlyReturn;
}

static OnPressResult id_sistro_ps5debug(OnPressContext &ctx) {
  (void)ctx;
  notify("PS5Debug is not bundled in OrionHEN");
  return OnPressResult::Handled;
}

static const OnPressExactEntry kExact[] = {
    {"id_download_kstuff", id_download_kstuff},
    {"id_kstuff_autoload", id_kstuff_autoload},
    {"id_delete_kstuff", id_delete_kstuff},
    {"id_change_custom_pkg_path", id_change_custom_pkg_path},
    {"id_save_rp_info", id_save_rp_info},
    {"id_lm_test", id_lm_test},
    {"id_orionhen_credits", id_orionhen_credits},
    {"id_dl_cheats", id_dl_cheats},
    {"id_sistro_ps5debug", id_sistro_ps5debug},
};

const OnPressExactEntry *onpress_misc_exact(size_t *count) {
  *count = sizeof(kExact) / sizeof(kExact[0]);
  return kExact;
}
