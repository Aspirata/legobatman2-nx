#include "xma_packet_fix.h"
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstring>

namespace lb2 {
namespace {
// SDK 0.10.0 increments the packet index when a continuation packet has no
// new frames. In interleaved XMA this selects another channel's stream.
// Source-level fix: port/rexglue-xma-packet-traversal.patch.
uint32_t next_packet(void*, uint8_t* buffer, uint32_t index, uint32_t count) {
  while (index < count) {
    const uint8_t* packet = buffer + index * 2048;
    const uint32_t first = 32 + ((packet[0] & 3) << 13) + (packet[1] << 5) + (packet[2] >> 3);
    if (first <= 16352) return index * 16384 + first;
    index += uint32_t(packet[3]) + 1;
  }
  return 32;
}
}

bool install_xma_packet_fix() {
  const auto module = GetModuleHandleW(L"rexruntime.dll");
  if (!module) return false;
  auto* method = reinterpret_cast<uint8_t*>(GetProcAddress(module,
      "?GetNextPacketReadOffset@XmaContext@audio@rex@@AEAAIPEAEII@Z"));
  if (!method) return false;
  // Fail closed for an unsupported binary. Only the pinned SDK method may
  // be substituted, before its worker thread exists; DLL files stay intact.
  constexpr uint8_t expected[] = {0x56,0x57,0x45,0x39,0xc1,0x45,0x0f,0x46,
                                 0xc8,0x45,0x89,0xc2,0x41,0xc1,0xe0,0x0e};
  std::array<uint8_t, 16> jump{0xff,0x25,0,0,0,0};
  const auto destination = reinterpret_cast<uintptr_t>(&next_packet);
  std::memcpy(jump.data()+6, &destination, sizeof(destination));
  jump[14]=jump[15]=0x90;
  if (!std::memcmp(method, jump.data(), jump.size())) return true;
  if (std::memcmp(method, expected, sizeof(expected))) return false;
  DWORD old_protect;
  if (!VirtualProtect(method, jump.size(), PAGE_EXECUTE_READWRITE, &old_protect)) return false;
  std::memcpy(method, jump.data(), jump.size());
  const bool flushed = FlushInstructionCache(GetCurrentProcess(), method, jump.size()) != FALSE;
  DWORD ignored;
  const bool restored = VirtualProtect(method, jump.size(), old_protect, &ignored) != FALSE;
  return flushed && restored;
}
}
