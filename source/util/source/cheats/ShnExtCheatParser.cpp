#include "cheats/ICheatParser.hpp"

#include "cheats/cheat_engine_internal.h"

namespace orion::cheats {

/**
 * Adapter: ShnExt crypto + deflate + optional keystone stay in C
 * (cheat_engine_parser_shnext.c). C++ only owns the Strategy interface.
 */
class ShnExtCheatParser final : public ICheatParser {
public:
  const char *name() const override { return "ShnExt"; }

  int parse(const uint8_t *data, size_t size, orion_cheat_file_t &out) override {
    if (data == nullptr || size == 0) {
      return -1;
    }
    return orion_cheat_parse_shnext_buffer(reinterpret_cast<const char *>(data),
                                           size, &out);
  }
};

std::unique_ptr<ICheatParser> makeShnExtCheatParser() {
  return std::make_unique<ShnExtCheatParser>();
}

} // namespace orion::cheats
