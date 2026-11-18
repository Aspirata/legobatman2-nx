#pragma once
#include <array>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>

namespace lb2::clear_trace {
enum class Kind : uint32_t {
  kOperation=1, kRegisters, kShader, kConstants, kVertexIndex, kVertexExport,
  kExtent, kHeight, kOwnership, kPreviousOwner, kTransfer, kGuestDraw, kResolve,
  kShaderCode, kDump, kTransferRectangle, kTextureBindings, kResolveInfo
};
inline size_t PackTextureBindings(uint32_t mask,const uint32_t* fetch,
                                 std::array<uint32_t,225>& output) {
  size_t size=0; output[size++]=mask;
  for (unsigned slot=0;slot<32;++slot) {
    if (!(mask & (uint32_t(1)<<slot))) continue;
    output[size++]=slot;
    for (unsigned i=0;i<6;++i) output[size++]=fetch[slot*6+i];
  }
  return size;
}
// Used only by the GPU command thread. Records are a contiguous prefix on overflow.
class Recorder {
 public:
  explicit Recorder(size_t capacity_words=1024*1024) : capacity_(capacity_words) {}
  bool BeginOperation(uint64_t frame,bool enabled) {
    if (!enabled || (frame!=180 && frame!=280) || frame==closed_) return false;
    if (!active_) {
      words_.clear(); words_.reserve(capacity_);
      frame_=frame; operations_=records_=dropped_=0; active_=true;
    }
    if (frame_!=frame) return false;
    ++operations_; Record(Kind::kOperation,{}); return true;
  }
  void Record(Kind kind,std::span<const uint32_t> payload) {
    if (!active_) return;
    if (dropped_ || words_.size()+3+payload.size()>capacity_) { ++dropped_; return; }
    words_.push_back(uint32_t(kind)); words_.push_back(uint32_t(payload.size()));
    words_.push_back(operations_);
    words_.insert(words_.end(),payload.begin(),payload.end()); ++records_;
  }
  void Record(Kind kind,std::initializer_list<uint32_t> payload) {
    Record(kind,std::span<const uint32_t>(payload.begin(),payload.size()));
  }
  bool CloseFrame(uint64_t frame) {
    if (!active_ || frame_!=frame) return false;
    active_=false; closed_=frame; return true;
  }
  std::array<uint32_t,8> header() const {
    return {0x5443324c,1,uint32_t(frame_),uint32_t(frame_>>32),operations_,records_,dropped_,uint32_t(words_.size())};
  }
  std::span<const uint32_t> words() const { return words_; }
  bool active() const { return active_; }
 private:
  std::vector<uint32_t> words_;
  size_t capacity_;
  uint64_t frame_=0,closed_=0;
  uint32_t operations_=0,records_=0,dropped_=0;
  bool active_=false;
};
}

extern "C" bool Lb2ClearTraceActive();
extern "C" bool Lb2ClearTraceVertices();
extern "C" void Lb2ClearTraceBegin(uint64_t frame,bool enabled,const uint32_t* regs);
extern "C" void Lb2ClearTraceFinish(uint64_t frame);
extern "C" void Lb2ClearTraceRecord(unsigned kind,const uint32_t* words,uint32_t count);
extern "C" void Lb2ClearTraceShader(unsigned stage,uint64_t hash,const uint32_t* code,uint32_t count,const uint32_t* regs);
extern "C" void Lb2ClearTraceTextureBindings(uint32_t mask,const uint32_t* regs);
inline void Lb2ClearTraceWords(lb2::clear_trace::Kind kind,std::initializer_list<uint32_t> words) {
  if (Lb2ClearTraceActive()) Lb2ClearTraceRecord(unsigned(kind),words.begin(),uint32_t(words.size()));
}
