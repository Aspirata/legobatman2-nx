#include "xma_packet_fix.h"
#include <windows.h>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
  // Exercise the real runtime entry rather than a second copy of its helper.
  if (!LoadLibraryW(L"rexruntime.dll") || !lb2::install_xma_packet_fix() ||
      !lb2::install_xma_packet_fix()) return 1;
  using Next = uint32_t (*)(void*, uint8_t*, uint32_t, uint32_t);
  auto next = reinterpret_cast<Next>(GetProcAddress(GetModuleHandleW(L"rexruntime.dll"),
    "?GetNextPacketReadOffset@XmaContext@audio@rex@@AEAAIPEAEII@Z"));
  if (!next) return 2;
  std::vector<uint8_t> packets(5 * 2048);
  // Continuation-only mono packet skips a neighboring stereo packet.
  packets[0]=3; packets[1]=255; packets[2]=248; packets[3]=1;
  packets[2048]=252; packets[2048+2]=1;
  packets[4096]=52; packets[4096+2]=1;
  if (next(nullptr, packets.data(), 0, 5) != 32800 ||
      next(nullptr, packets.data(), 2, 5) != 32800 ||
      next(nullptr, packets.data(), 0, 2) != 32 ||
      next(nullptr, packets.data(), 5, 5) != 32) return 3;
  // Follow a second continuation, not just the first packet's skip count.
  packets[4096]=3; packets[4097]=255; packets[4098]=248; packets[4099]=1;
  packets[8192]=52; packets[8194]=1;
  if (next(nullptr, packets.data(), 0, 5) != 65568 ||
      next(nullptr, packets.data(), 0, 4) != 32) return 4;
  std::cout << "XMA: interleaved continuation chains, immediate target, bounds, repeat setup passed\n";
}
