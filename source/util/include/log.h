#pragma once

#include "common_utils.h"


// NOLINTBEGIN(*)
#define __MACRO_STRINGIFY__(x) #x
#define __FILE_LINE_STRING__(x, y) x":"__MACRO_STRINGIFY__(y)
#define LOG_PERROR(msg) OrionHEN_log("[OrionHEN UTIL] " __FILE_LINE_STRING__(__FILE_NAME__, __LINE__) ": " msg)
#define LOG_PRINTLN(msg) OrionHEN_log("[OrionHEN UTIL] " __FILE_LINE_STRING__(__FILE_NAME__, __LINE__) ": " msg)
#define LOG_PRINTF(msg, ...) OrionHEN_log("[OrionHEN UTIL] " __FILE_LINE_STRING__(__FILE_NAME__, __LINE__) ": " msg, __VA_ARGS__)
// NOLINTEND(*)
