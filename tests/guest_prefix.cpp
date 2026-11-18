// Synthetic guest memory validates both paths through a split PPC method.
#include "legobatman2_pch.h"
#include <array>
#include <cstdint>
#include <iostream>

REX_EXTERN(__imp__sub_82556980);
REX_EXTERN(__imp__sub_827580D8);
static uint32_t tail_target = 0;
static uint32_t tail_argument = 0;
// Test-only native hooks prove the preserved tail/alias route. Never linked into the game app.
REX_HOOK_RAW(sub_825F8C68) { tail_target = 1; tail_argument = ctx.r3.u32; }
REX_HOOK_RAW(sub_825F8D48) { tail_target = 2; tail_argument = ctx.r3.u32; }

int main() {
    alignas(64) std::array<uint8_t, 4096> memory{};
    PPCContext ctx{};
    ctx.lr = 0x12345678;
    ctx.r14.u64 = 0x9988776655443322ull;
    ctx.r3.u64 = 0x100;
    __imp__sub_82556980(ctx, memory.data());
    if (ctx.r3.u32 != 0 || ctx.lr != 0x12345678 || ctx.r14.u64 != 0x9988776655443322ull) return 1;
    // Object +0x18 points to a guest record; record +0x0C is the return value.
    memory[0x119] = 0; memory[0x11A] = 2; memory[0x11B] = 0;
    memory[0x20C] = 0xCA; memory[0x20D] = 0xFE; memory[0x20E] = 0xBA; memory[0x20F] = 0xBE;
    ctx.r3.u64 = 0x100;
    __imp__sub_82556980(ctx, memory.data());
    if (ctx.r3.u32 != 0xCAFEBABE || ctx.lr != 0x12345678 || ctx.r14.u64 != 0x9988776655443322ull) return 2;
    // A second split method updates bit15 and tails through an existing 4-byte block.
    memory[0x35A] = 0x12; memory[0x35B] = 0x34;
    memory[0x35E] = 5;  // BE32 +0x5C points to guest address0x500.
    ctx.r3.u64 = 0x300; ctx.r4.u64 = 0;
    __imp__sub_827580D8(ctx, memory.data());
    if (tail_target || memory[0x35A] != 0x12 || memory[0x35B] != 0x34) return 3;
    ctx.r3.u64 = 0x300; ctx.r4.u64 = 1;
    __imp__sub_827580D8(ctx, memory.data());
    if (tail_target != 1 || tail_argument != 0x500 || memory[0x35A] != 0x92 || memory[0x35B] != 0x34 || !ctx.xer.ca) return 4;
    ctx.r3.u64 = 0x300; ctx.r4.u64 = 0;
    __imp__sub_827580D8(ctx, memory.data());
    if (tail_target != 2 || tail_argument != 0x500 || memory[0x35A] != 0x12 || memory[0x35B] != 0x34 || ctx.xer.ca) return 5;
    if (ctx.lr != 0x12345678 || ctx.r14.u64 != 0x9988776655443322ull) return 6;
    std::cout << "Split methods: guest branches, tail hooks, flag bits, carry, LR/r14 passed\n";
    return 0;
}
