#include "game_identity.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::filesystem::path path = argv[1];
  { std::ofstream file(path, std::ios::binary); file << "abc"; }
  const std::string expected = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
  if (lb2::sha256_file(path) != expected) return 3;
  if (!lb2::verify_game(path, expected)) return 4;
  if (lb2::verify_game(path, std::string(64, '0'))) return 5;
  if (lb2::verify_game(path.string()+".missing", expected)) return 6;
  std::ifstream file(path); std::string data; file >> data;
  if (data != "abc") return 7;
  std::cout << "SHA-256 known vector, mismatch and missing input: passed\n";
  return 0;
}
