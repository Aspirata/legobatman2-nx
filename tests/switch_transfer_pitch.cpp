#include "lb2_transfer_pitch.h"
#include <cstdio>
#include <cstdlib>
#include <unordered_set>
#include <vector>
int main() {
  unsigned checks=0;
  auto check=[&](bool value,const char* message) {
    ++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
  };
  std::vector<uint32_t> bases{0,0xfffff,0x40102,0x20200,0x20002,0x40002,0x50202};
  for(unsigned bit=0;bit<20;++bit) bases.push_back(1u<<bit);
  std::unordered_set<uint32_t> identities;
  for(uint32_t base:bases) {
    for(uint32_t pitch=1;pitch<=1023;++pitch) {
      auto key=lb2::transfer_pitch::MakeKey(base,pitch,true);
      check(lb2::transfer_pitch::BaseKey(key)==base,"Source pitch must preserve transfer mode, formats, sample counts and RT slot");
      check(lb2::transfer_pitch::Pitch(key)==pitch,"Transfer shader identity must retain the exact source divisor");
      check(identities.insert(key).second,"Distinct transfer-key/source-pitch pairs cannot alias");
      check(lb2::transfer_pitch::MakeKey(base,pitch,false)==base,"Disabled trial preserves original identity and shader");
    }
    for(uint32_t pitch:{0u,1024u,0xffffffffu})
      check(lb2::transfer_pitch::MakeKey(base,pitch,true)==base,"Unsupported source pitches keep dynamic addressing");
  }
  for(uint32_t key:{0x100000u,0x80000000u,0xffffffffu})
    check(lb2::transfer_pitch::MakeKey(key,11,true)==key,"Unknown existing high key bits must remain untouched");
  check(lb2::transfer_pitch::BaseKey(lb2::transfer_pitch::MakeKey(0x40102,12,true))==0x40102,
        "Cache fallback must restore the exact original transfer mode");
  std::printf("PASS: %u transfer source-pitch identity and fallback checks\n",checks);
}
