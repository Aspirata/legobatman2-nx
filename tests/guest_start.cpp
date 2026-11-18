#include "legobatman2_pch.h"
#include <windows.h>
#include <iostream>

REX_EXTERN(__imp__sub_823A1A60);
REX_EXTERN(__imp__sub_823A1AA0);
REX_EXTERN(__imp__sub_823A1AD0);
REX_EXTERN(__imp__sub_823A1A80);
static bool called = false;
static void target(PPCContext& ctx, uint8_t*) {
  called = ctx.r3.u32 == 0x100 && ctx.r4.u32 == 0xCAFEBABE;
  ctx.r3.u64 = 42;
}
int main() {
  // Reserve guest address space; commit only the object/vtable and one dispatch slot.
  auto* memory = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0x84000000ull, MEM_RESERVE, PAGE_READWRITE));
  if (!memory) return 1;
  auto* base = memory;
  if (!VirtualAlloc(memory, 4096, MEM_COMMIT, PAGE_READWRITE) ||
      !VirtualAlloc(memory + REX_IMAGE_BASE + REX_IMAGE_SIZE, 4096, MEM_COMMIT, PAGE_READWRITE)) return 2;
  REX_STORE_U32(0x100, 0x200);
  REX_STORE_U32(0x258, REX_CODE_BASE);
  REX_LOOKUP_FUNC(memory, REX_CODE_BASE) = target;
  PPCContext ctx{};
  ctx.r3.u64 = 0x100; ctx.r4.u64 = 0xCAFEBABE;
  ctx.lr = 0x12345678; ctx.r14.u64 = 0x1122334455667788ull;
  __imp__sub_823A1A60(ctx, memory);
  bool ok = called && ctx.r3.u32 == 42 && ctx.lr == 0x12345678 &&
                  ctx.r14.u64 == 0x1122334455667788ull && ctx.ctr.u32 == REX_CODE_BASE;
  // Save-selection route dispatches through a different slot of the same interface.
  REX_STORE_U32(0x258, 0);  // Catch a mistaken reuse of the Start slot.
  REX_STORE_U32(0x240, REX_CODE_BASE);
  ctx.r3.u64 = 0x100;
  called = false;
  __imp__sub_823A1AA0(ctx, memory);
  ok = ok && called && ctx.r3.u32 == 42 && ctx.r4.u32 == 0xCAFEBABE &&
       ctx.lr == 0x12345678 && ctx.r14.u64 == 0x1122334455667788ull;
  // Overwrite confirmation uses +0x54, not either of the previous slots.
  REX_STORE_U32(0x240, 0);
  REX_STORE_U32(0x254, REX_CODE_BASE);
  ctx.r3.u64 = 0x100;
  called = false;
  __imp__sub_823A1AD0(ctx, memory);
  ok = ok && called && ctx.r3.u32 == 42 && ctx.r4.u32 == 0xCAFEBABE &&
       ctx.lr == 0x12345678 && ctx.r14.u64 == 0x1122334455667788ull &&
       ctx.ctr.u32 == REX_CODE_BASE;
  // Normal menu navigation reaches a separate +0x3C virtual callback.
  REX_STORE_U32(0x254, 0);
  REX_STORE_U32(0x23C, REX_CODE_BASE);
  ctx.r3.u64 = 0x100;
  called = false;
  __imp__sub_823A1A80(ctx, memory);
  ok = ok && called && ctx.r3.u32 == 42 && ctx.r4.u32 == 0xCAFEBABE &&
       ctx.lr == 0x12345678 && ctx.r14.u64 == 0x1122334455667788ull &&
       ctx.ctr.u32 == REX_CODE_BASE;
  VirtualFree(memory, 0, MEM_RELEASE);
  if (!ok) return 3;
  std::cout << "Start/save/overwrite/menu callbacks: vtable +0x58/+0x40/+0x54/+0x3C, arguments, return, LR/r14 passed\n";
}
