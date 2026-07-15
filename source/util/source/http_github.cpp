/* Copyright (C) 2025 OnionHEN / LightningMods */

#include "http_github.h"

#include "../../extern/cJSON/cJSON.hpp"

#include <cstring>

extern "C" void OnionHEN_log(const char *fmt, ...);

extern "C" bool onion_http_extract_commit_sha(const char *json_data,
                                              char *sha_buffer,
                                              size_t buffer_size) {
  if (!json_data || !sha_buffer || buffer_size == 0)
    return false;

  cJSON *root = cJSON_Parse(json_data);
  if (!root) {
    OnionHEN_log("Could not parse GitHub commit JSON");
    return false;
  }

  cJSON *commit = cJSON_IsArray(root) ? cJSON_GetArrayItem(root, 0) : root;
  if (!commit) {
    OnionHEN_log("Could not find commit object in JSON response");
    cJSON_Delete(root);
    return false;
  }

  cJSON *sha = cJSON_GetObjectItemCaseSensitive(commit, "sha");
  if (!cJSON_IsString(sha) || !sha->valuestring) {
    OnionHEN_log("Could not find 'sha' field in JSON response");
    cJSON_Delete(root);
    return false;
  }

  size_t sha_length = std::strlen(sha->valuestring);
  if (sha_length >= buffer_size) {
    OnionHEN_log("SHA too long for buffer");
    cJSON_Delete(root);
    return false;
  }

  std::memcpy(sha_buffer, sha->valuestring, sha_length);
  sha_buffer[sha_length] = '\0';
  cJSON_Delete(root);

  OnionHEN_log("Extracted commit SHA: %s", sha_buffer);
  return true;
}
