// Runs a small set of original translated LB2 functions, not the full game.
#include <switch.h>
#include "legobatman2_pch.h"
#include <array>
#include <cstdio>

REX_EXTERN(sub_82556980);
REX_EXTERN(sub_825CB588);

int main() {
  consoleInit(nullptr);
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  PadState pad;
  padInitializeDefault(&pad);
  alignas(64) std::array<uint8_t, 4096> ram{};
  auto* base = ram.data();
  PPCContext ctx{};
  ctx.lr=0x12345678; ctx.r14.u64=0x9988776655443322ull;
  ctx.r3.u64=0x100;
  sub_82556980(ctx,base);
  const bool null_ok=ctx.r3.u32==0;
  REX_STORE_U32(0x118,0x200);
  REX_STORE_U32(0x20C,0xCAFEBABE);
  ctx.r3.u64=0x100;
  sub_82556980(ctx,base);
  const bool memory_ok=ctx.r3.u32==0xCAFEBABE;
  sub_825CB588(ctx,base);
  const bool constant_ok=ctx.r3.u32==0x820E23C4;
  const bool registers_ok=ctx.lr==0x12345678 && ctx.r14.u64==0x9988776655443322ull;
  const bool ok=null_ok && memory_ok && constant_ok && registers_ok;
  std::printf("LB2 ARM64 guest code probe\n\nThis is NOT the full game.\n\n");
  std::printf("Original guest functions:\nNull branch: %s\nBig-endian read: %s\nGuest constant: %s\nLR/r14 preserved: %s\n\n",
    null_ok?"PASS":"FAIL",memory_ok?"PASS":"FAIL",constant_ok?"PASS":"FAIL",registers_ok?"PASS":"FAIL");
  std::printf("RESULT: %s\n\nPress + to exit.\n",ok?"PASS":"FAIL");
  while(appletMainLoop()) {
    padUpdate(&pad);
    if(padGetButtonsDown(&pad)&HidNpadButton_Plus) break;
    consoleUpdate(nullptr);
  }
  consoleExit(nullptr);
  return ok?0:1;
}
