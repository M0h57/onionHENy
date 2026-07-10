/* Copyright (C) 2025 OrionHEN / LightningMods */

#include <orion/notify.h>
#include <orion/log.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  int32_t type;
  int32_t req_id;
  int32_t priority;
  int32_t msg_id;
  int32_t target_id;
  int32_t user_id;
  int32_t unk1;
  int32_t unk2;
  int32_t app_id;
  int32_t error_num;
  int32_t unk3;
  char use_icon_image_uri;
  char message[1024];
  char uri[1024];
  char unkstr[1024];
} OrbisNotificationRequest;

int32_t sceKernelSendNotificationRequest(int32_t device,
                                         OrbisNotificationRequest *req,
                                         size_t size, int32_t blocking);

void orion_notify_format(char *out, size_t out_sz, int show_watermark,
                         const char *fmt, va_list ap) {
  char buff[3075];
  vsnprintf(buff, sizeof(buff), fmt, ap);
  (void)show_watermark;
  snprintf(out, out_sz, "[OrionHEN] %s", buff);
}

void orion_notify_v(int show_watermark, const char *fmt, va_list ap) {
  OrbisNotificationRequest req;
  memset(&req, 0, sizeof(req));
  orion_notify_format(req.message, sizeof(req.message), show_watermark, fmt, ap);

  req.type = 0;
  req.unk3 = 0;
  req.use_icon_image_uri = 1;
  req.target_id = -1;
  strncpy(req.uri, "cxml://psnotification/tex_icon_system", sizeof(req.uri) - 1);

  OrionHEN_log("Notify: %s", req.message);
  sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

void orion_notify(int show_watermark, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  orion_notify_v(show_watermark, fmt, args);
  va_end(args);
}
