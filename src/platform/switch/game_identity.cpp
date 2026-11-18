#include "game_identity.h"
#include "crypto/sha256.h"
#include <array>
#include <fstream>

namespace lb2 {
std::string sha256_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return {};
  sha256::SHA256 digest;
  std::array<char,65536> buffer;
  while(stream) {
    stream.read(buffer.data(),buffer.size());
    if(stream.gcount()) digest.add(buffer.data(),static_cast<size_t>(stream.gcount()));
  }
  return stream.eof() ? digest.getHash() : std::string{};
}
bool verify_game(const std::filesystem::path& path, const std::string& expected) {
  return sha256_file(path)==expected;
}
}
