"""Compile the actual Horizon XMA packet traversal with synthetic buffers."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
sdk = root / 'third_party/nfsmw-nx/sdk'
source = (sdk / 'src/audio/xma_context.cpp').read_text()
start = source.find('static uint32_t FindNextPacketReadOffset(')
if start < 0:
    start = source.index('uint32_t XmaContext::GetNextPacketReadOffset(')
end = source.index('memory::RingBuffer XmaContext::PrepareOutputRingBuffer(', start)
swap_start = source.index('void XmaContext::SwapInputBuffer(')
swap_end = source.index('void XmaContext::UpdateLoopStatus(', swap_start)
stage = root / 'work/analysis/switch-xma-chain'
stage.mkdir(parents=True, exist_ok=True)
harness = r'''
#include <array>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <rex/audio/xma/helpers.h>
using namespace rex::audio;
struct XMA_CONTEXT_DATA {
  unsigned current_buffer=0, input_buffer_0_valid=1, input_buffer_1_valid=1;
  uint32_t input_buffer_read_offset=32, error_status=0;
  std::array<uint32_t,2> counts{4,4};
  bool IsCurrentInputBufferValid() const { return current_buffer ? input_buffer_1_valid : input_buffer_0_valid; }
  uint32_t GetCurrentInputBufferPacketCount() const { return counts[current_buffer]; }
  uint32_t GetCurrentInputBufferAddress() const { return 1; }
};
struct XmaContext {
  static constexpr uint32_t kBytesPerPacket=2048, kBitsPerPacket=16384, kBitsPerPacketHeader=32;
  XMA_CONTEXT_DATA* data;
  std::array<std::vector<uint8_t>,2> buffers{std::vector<uint8_t>(8*2048),std::vector<uint8_t>(8*2048)};
  uint8_t* GetCurrentInputBuffer(XMA_CONTEXT_DATA* state) { return buffers[state->current_buffer].data(); }
  uint32_t GetNextPacketReadOffset(uint8_t*,uint32_t,uint32_t);
  void AdvanceInputPacket(XMA_CONTEXT_DATA*,uint32_t);
  static void SwapInputBuffer(XMA_CONTEXT_DATA*);
  void continuation(unsigned buffer,unsigned packet,uint8_t skip) {
    auto* p=buffers[buffer].data()+packet*2048;
    p[0]=3; p[1]=255; p[2]=248; p[3]=skip;
  }
};
''' + source[swap_start:swap_end] + source[start:end] + r'''
int main() {
  int failures=0;
  {
    XMA_CONTEXT_DATA data; XmaContext ctx{&data};
    ctx.continuation(0,3,1); ctx.AdvanceInputPacket(&data,3);
    bool ok=data.current_buffer==1 && data.input_buffer_read_offset==16384+32;
    std::printf("Cross-buffer stream skip: %s (buffer %u, offset %u)\n",ok?"PASS":"FAIL",data.current_buffer,data.input_buffer_read_offset);
    failures+=!ok;
  }
  {
    XMA_CONTEXT_DATA data; XmaContext ctx{&data};
    ctx.continuation(0,0,255); ctx.AdvanceInputPacket(&data,0);
    bool ok=data.current_buffer==0 && data.input_buffer_read_offset==16384+32;
    std::printf("Full-packet skip sentinel: %s\n",ok?"PASS":"FAIL"); failures+=!ok;
  }
  {
    XMA_CONTEXT_DATA data; data.counts[0]=5; XmaContext ctx{&data};
    ctx.continuation(0,0,1); ctx.continuation(0,2,1); ctx.AdvanceInputPacket(&data,0);
    bool ok=data.current_buffer==0 && data.input_buffer_read_offset==65536+32;
    std::printf("Interleaved continuation chain: %s\n",ok?"PASS":"FAIL"); failures+=!ok;
  }
  return failures ? 1 : 0;
}
'''
(stage / 'test.cpp').write_text(harness)
compiler = root / 'work/toolchain/msys64/mingw64/bin/g++.exe'
exe = stage / 'test.exe'
env = dict(os.environ, PATH=str(compiler.parent) + os.pathsep + os.environ['PATH'])
subprocess.run([str(compiler), '-std=c++20', '-O2', '-I'+str(sdk/'include'),
                str(stage/'test.cpp'), '-o', str(exe)], env=env, check=True)
raise SystemExit(subprocess.run([str(exe)], env=env).returncode)
