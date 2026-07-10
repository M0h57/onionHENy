#include "cheats/ICheatParser.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>

#include "cheats/cheat_engine_internal.h"

extern "C" void OrionHEN_log(const char *fmt, ...);

namespace orion::cheats {

/* Factories implemented in format translation units */
std::unique_ptr<ICheatParser> makeJsonCheatParser();
std::unique_ptr<ICheatParser> makeXmlCheatParser();
std::unique_ptr<ICheatParser> makeMc4CheatParser();
std::unique_ptr<ICheatParser> makeShnExtCheatParser();

namespace {

std::string normalizeFormat(std::string format) {
  while (!format.empty() && format.front() == '.') {
    format.erase(format.begin());
  }
  for (char &c : format) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return format;
}

std::string extensionOf(const std::string &path) {
  const auto pos = path.find_last_of('.');
  if (pos == std::string::npos || pos + 1 >= path.size()) {
    return {};
  }
  return path.substr(pos + 1);
}

} // namespace

std::unique_ptr<ICheatParser>
CheatParserFactory::createByFormat(const std::string &format) {
  const std::string f = normalizeFormat(format);
  if (f == "shn") {
    return makeXmlCheatParser();
  }
  if (f == "mc4") {
    return makeMc4CheatParser();
  }
  if (f == "shnext") {
    return makeShnExtCheatParser();
  }
  /* json or unknown → JSON (GoldHEN-style default) */
  return makeJsonCheatParser();
}

std::unique_ptr<ICheatParser>
CheatParserFactory::createByPath(const std::string &path) {
  return createByFormat(extensionOf(path));
}

int CheatParserFactory::loadBuffer(const std::string &format,
                                   const uint8_t *data, size_t size,
                                   orion_cheat_file_t &out) {
  if (data == nullptr || size == 0) {
    return -1;
  }
  auto parser = createByFormat(format);
  OrionHEN_log("[engine] parse format=%s size=%zu", parser->name(), size);
  return parser->parse(data, size, out);
}

int CheatParserFactory::loadFile(const std::string &path,
                                 orion_cheat_file_t &out) {
  long size = 0;
  char *buf = nullptr;

  orion_cheat_file_clear(&out);
  if (path.empty()) {
    return -1;
  }

  OrionHEN_log("[engine] load path=%s", path.c_str());
  buf = orion_cheat_load_file_buffer(path.c_str(), &size);
  if (buf == nullptr || size <= 0) {
    OrionHEN_log("[engine] load failed path=%s", path.c_str());
    free(buf);
    return -1;
  }

  auto parser = createByPath(path);
  const int rc = parser->parse(reinterpret_cast<const uint8_t *>(buf),
                               static_cast<size_t>(size), out);

  orion_cheat_secure_zero(buf, static_cast<size_t>(size));
  free(buf);
  OrionHEN_log("[engine] loaded format=%s cheats=%zu rc=%d", parser->name(),
               out.cheat_count, rc);
  return rc;
}

} // namespace orion::cheats
