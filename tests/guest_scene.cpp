// Exercise the observed end-of-cutscene callback and its existing tail blocks.
#include "legobatman2_pch.h"
#include <windows.h>
#include <iostream>

REX_EXTERN(__imp__sub_8226B460);

int main() {
  auto* base = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0x84000000ull, MEM_RESERVE, PAGE_READWRITE));
  if (!base) return 1;
  if (!VirtualAlloc(base, 0x4000, MEM_COMMIT, PAGE_READWRITE) ||
      !VirtualAlloc(base + 0x82C69000, 4096, MEM_COMMIT, PAGE_READWRITE)) return 2;
  REX_STORE_U32(0x2008, 0x2800);  // r4=0x1000 -> nested record.
  REX_STORE_U32(0x282C, 0x3000);
  struct Case { uint32_t object, flags_f0, flags_d8, flags_e8, selector, global, initial, expected; };
  const Case cases[] = {
    {0, 0, 0, 0, 0, 0, 0x12345671, 0x1234567B},
    {0x1000, 0, 0, 0, 0, 0, 0x12345671, 0x1234567B},
    {0x1000, 0x00100000, 0, 0, 0, 0, 0xFFFFFFF5, 0},
    {0x1000, 0, 0x02000000, 0, 11, 0, 0xFFFFFFFF, 0xB},
    {0x1000, 0x00100000, 0, 0x80000000, 11, 0, 0xFFFFFFFF, 0xA},
    {0, 0, 0, 0, 11, 0, 0x12345671, 0x1234567B},
    {0, 0, 0, 0, 11, 1, 0x12345670, 0x1234567B},
    {0x1000, 0, 0x02000000, 0, 9, 0, 0xFFFFFFFF, 9},
    {0, 0, 0, 0, 9, 1, 0x12345670, 0x1234567B},
    {0x1000, 0, 0x02000000, 0, 12, 0, 0xFFFFFFFF, 12},
    {0, 0, 0, 0, 12, 1, 0x12345670, 0x1234567E},
  };
  unsigned index=0;
  for (const auto& c : cases) {
    REX_STORE_U32(0x30F0, c.flags_f0);
    REX_STORE_U32(0x30D8, c.flags_d8);
    REX_STORE_U32(0x30E8, c.flags_e8);
    REX_STORE_U32(0x82C693E8, c.global);
    REX_STORE_U32(0x100, c.initial);
    PPCContext ctx{};
    ctx.r3.u64=0x777; ctx.r4.u64=c.object; ctx.r5.u64=0x100; ctx.r6.u64=c.selector;
    ctx.lr=0x12345678; ctx.r14.u64=0x1122334455667788ull;
    __imp__sub_8226B460(ctx,base);
    if (REX_LOAD_U32(0x100) != c.expected || ctx.r3.u32 != 0x777 ||
        ctx.lr != 0x12345678 || ctx.r14.u64 != 0x1122334455667788ull) {
      std::cerr << "Scene callback case " << index << " failed: " << std::hex << REX_LOAD_U32(0x100) << '\n';
      return 3;
    }
    ++index;
  }
  VirtualFree(base,0,MEM_RELEASE);
  std::cout << "Scene callback: null/nested flags, selectors 9/11/12, shared tails, LR/r14 passed\n";
}
