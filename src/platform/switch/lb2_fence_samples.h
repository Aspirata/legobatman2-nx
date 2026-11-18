#pragma once
#include <cstdint>
enum class Lb2FenceDrain { kComplete,kUnsafe,kFailed };
struct Lb2FenceKey {
  unsigned kind=0; //0 draw,1 copy.
  uint64_t vs=0,ps=0;
  uint32_t surface=0,control=0,mask=0,extra=0;
  bool operator==(const Lb2FenceKey&) const=default;
};
struct Lb2FenceWork { uint64_t guest_draws=0,vertices=0,transfer_draws=0,transfer_pixels=0; };
class Lb2FenceSample {
 public:
  using Drain=Lb2FenceDrain(*)(void*);
  Lb2FenceSample(const Lb2FenceKey&,bool enabled,Drain,void*);
  ~Lb2FenceSample();
  Lb2FenceSample(const Lb2FenceSample&)=delete;
  Lb2FenceSample& operator=(const Lb2FenceSample&)=delete;
  void Commit(){committed_=true;}
  bool active()const{return active_;}
 private:
  Lb2FenceKey key_{};Lb2FenceWork work_{};
  Drain drain_=nullptr;void* context_=nullptr;
  uint64_t start_=0,prior_=0;
  bool active_=false,committed_=false;
};
extern "C" void Lb2FenceSampleGuestDraw(uint32_t vertices);
extern "C" void Lb2FenceSampleTransfer(uint64_t pixels);
extern "C" void Lb2FenceSamplesReport();
