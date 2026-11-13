#pragma once
#include <filesystem>
#include <string>
namespace lb2 {
std::string sha256_file(const std::filesystem::path& path);
bool verify_game(const std::filesystem::path& path, const std::string& expected);
}
