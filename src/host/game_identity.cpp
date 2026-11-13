#include "game_identity.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace lb2 {
namespace {
void check(NTSTATUS status) {
  if (status < 0) throw std::runtime_error("Windows SHA-256 operation failed");
}
struct Hash {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  std::vector<UCHAR> object;
  ~Hash() {
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
  }
};
}
std::string sha256_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("Cannot open selected XEX");
  Hash state;
  check(BCryptOpenAlgorithmProvider(&state.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0));
  DWORD bytes = 0, object_size = 0;
  check(BCryptGetProperty(state.algorithm, BCRYPT_OBJECT_LENGTH,
      reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &bytes, 0));
  state.object.resize(object_size);
  check(BCryptCreateHash(state.algorithm, &state.hash, state.object.data(), object_size, nullptr, 0, 0));
  std::array<char, 65536> buffer;
  while (input) {
    input.read(buffer.data(), buffer.size());
    if (input.gcount()) check(BCryptHashData(state.hash,
        reinterpret_cast<PUCHAR>(buffer.data()), static_cast<ULONG>(input.gcount()), 0));
  }
  if (input.bad() || !input.eof()) throw std::runtime_error("Read error in selected XEX");
  std::array<UCHAR, 32> result;
  check(BCryptFinishHash(state.hash, result.data(), result.size(), 0));
  const char digits[] = "0123456789abcdef";
  std::string hex;
  for (UCHAR byte : result) { hex += digits[byte >> 4]; hex += digits[byte & 15]; }
  return hex;
}
bool verify_game(const std::filesystem::path& path, const std::string& expected) {
  try { return sha256_file(path) == expected; }
  catch (const std::exception&) { return false; }
}
}
