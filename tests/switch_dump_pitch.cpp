#include "lb2_dump_pitch.h"
#include <cstdio>
#include <cstdlib>
#include <unordered_set>
int main() {
  unsigned checks=0;
  auto check=[&](bool value,const char* message) {
    ++checks; if(!value) {std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
  };
  std::unordered_set<uint32_t> identities;
  for(uint32_t base=0;base<128;++base) {
    for(uint32_t pitch=1;pitch<=1023;++pitch) {
      auto key=lb2::dump_pitch::MakeKey(base,pitch,true);
      check(lb2::dump_pitch::BaseKey(key)==base,"Specialization preserves every original format/sample/depth bit");
      check(lb2::dump_pitch::Pitch(key)==pitch,"Pipeline identity retains exact source divisor");
      check(identities.insert(key).second,"Distinct base/pitch pairs cannot alias a cached pipeline");
      check(lb2::dump_pitch::MakeKey(base,pitch,false)==base,"Disabled specialization preserves the original cache identity");
    }
    for(uint32_t pitch:{0u,1024u,0xffffffffu}) {
      check(lb2::dump_pitch::MakeKey(base,pitch,true)==base,"Unsupported source pitch retains the generic shader");
    }
  }
  for(uint32_t key:{128u,0x10000u,0xffffffffu}) {
    check(lb2::dump_pitch::MakeKey(key,11,true)==key,"Unknown pre-existing high key bits must not be overwritten");
  }
  check(lb2::dump_pitch::Pitch(64)==0,"Original depth key selects dynamic pitch");
  check(lb2::dump_pitch::Pitch(1)==0,"Original color key selects dynamic pitch");
  std::printf("PASS: %u dump source-pitch cache identity and fallback checks\n",checks);
}
