/* Copyright (C) 2025 OnionHEN / LightningMods
 *
 * Encode a 64-bit PSN account id as standard base64 (8 little-endian bytes).
 * @p output must hold at least 13 bytes (12 chars + NUL; padding may apply).
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline void onion_account_id_base64_encode(uint64_t input, char *output) {
  static const char base64_table[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  unsigned char bytes[8];
  int i = 0, j = 0;

  if (!output)
    return;

  for (int k = 0; k < 8; k++)
    bytes[k] = (unsigned char)((input >> (k * 8)) & 0xFF);

  while (i < 8) {
    uint32_t octet_a = i < 8 ? bytes[i++] : 0;
    uint32_t octet_b = i < 8 ? bytes[i++] : 0;
    uint32_t octet_c = i < 8 ? bytes[i++] : 0;
    uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

    output[j++] = base64_table[(triple >> 18) & 0x3F];
    output[j++] = base64_table[(triple >> 12) & 0x3F];
    output[j++] = base64_table[(triple >> 6) & 0x3F];
    output[j++] = base64_table[triple & 0x3F];
  }

  for (int k = 0; k < (3 - 8 % 3) % 3; k++)
    output[j - k - 1] = '=';

  output[j] = '\0';
}

#ifdef __cplusplus
}
#endif
