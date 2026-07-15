#pragma once

#include <cstdint>
#include <string>

#include "cheats/cheat_engine.h"
#include "util_platform.h"

namespace onion::cheats {

struct FileSignature {
  std::string path;
  uint64_t size = 0;
  uint64_t inode = 0;
  int64_t mtime = 0;
  int64_t ctime = 0;

  bool operator==(const FileSignature &o) const {
    return path == o.path && size == o.size && inode == o.inode &&
           mtime == o.mtime && ctime == o.ctime;
  }
  bool operator!=(const FileSignature &o) const { return !(*this == o); }
};

/**
 * Resolves flat cheat paths and loads format-specific files
 * via CheatParserFactory (Strategy: json / shn / mc4 / ShnExt).
 */
class CheatRepository {
public:
  /** Resolve existing cheat file for title+version; empty if none. */
  static std::string resolvePath(const game_context_t &game);

  static bool fileExists(const std::string &path);
  static bool statSignature(const std::string &path, FileSignature &out);

  /** Load path into out (clears out first). 0 = ok. */
  static int loadFile(const std::string &path, onion_cheat_file_t &out);

  static void ensureCheatsDir();
  /** Flatten nested repo tree into ONION_CHEATS_DIR. */
  static int flattenInstallTree(const std::string &root);
};

} // namespace onion::cheats
