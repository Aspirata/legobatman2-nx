// Native contract test for the static ELF input wrappers used by Switch.
#include <rex/ppc/context.h>
#include <rex/hook.h>
#include <array>
#include <atomic>
#include <chrono>
#include <thread>

namespace {
std::atomic<unsigned> active{0}, peak{0}, seen{0};
std::atomic<bool> valid{true};
void original(PPCContext& ctx, uint8_t* base, unsigned id) {
  auto count=++active;
  auto previous=peak.load();
  while(previous<count && !peak.compare_exchange_weak(previous,count)) {}
  if(ctx.r3.u32!=0x1234 || ctx.r4.u32!=0x5678 || ctx.r5.u32!=0x9ABC || *base!=0x42)
    valid=false;
  std::this_thread::sleep_for(std::chrono::microseconds(100));
  seen.fetch_or(1u<<id);
  ctx.r3.u64=0x9000+id;
  --active;
}
}
#define INPUT_CASE(name,id) \
  REX_EXTERN(__wrap___imp__XamInput##name); \
  REX_HOOK_RAW(__real___imp__XamInput##name) { original(ctx,base,id); }
INPUT_CASE(GetState,0)
INPUT_CASE(GetCapabilities,1)
INPUT_CASE(GetCapabilitiesEx,2)
INPUT_CASE(SetState,3)
INPUT_CASE(GetKeystroke,4)
INPUT_CASE(GetKeystrokeEx,5)

int main() {
  std::array<PPCFunc*,6> functions={__wrap___imp__XamInputGetState,
    __wrap___imp__XamInputGetCapabilities,__wrap___imp__XamInputGetCapabilitiesEx,
    __wrap___imp__XamInputSetState,__wrap___imp__XamInputGetKeystroke,
    __wrap___imp__XamInputGetKeystrokeEx};
  std::array<std::thread,6> threads;
  for(unsigned i=0;i<threads.size();++i) threads[i]=std::thread([&,i] {
    uint8_t memory=0x42;
    for(unsigned n=0;n<16;++n) {
      PPCContext ctx{};ctx.r3.u64=0x1234;ctx.r4.u64=0x5678;ctx.r5.u64=0x9ABC;
      ctx.lr=0xABCD;ctx.r14.u64=0x1122334455667788ull;
      functions[i](ctx,&memory);
      if(ctx.r3.u32!=0x9000+i || ctx.lr!=0xABCD || ctx.r14.u64!=0x1122334455667788ull)
        valid=false;
    }
  });
  for(auto& thread:threads) thread.join();
  return valid && peak==1 && seen==63 ? 0 : 1;
}
