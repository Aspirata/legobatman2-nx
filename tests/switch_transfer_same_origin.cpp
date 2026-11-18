#include "lb2_transfer_same_origin.h"
#include "lb2_transfer_pitch.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
int main() {
  unsigned checks=0;
  auto check=[&](bool ok,const char* msg){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",msg);std::exit(1);}};
  using namespace lb2::transfer_same_origin;
  check(Eligible(true,0xb40102,484,484,11,11,1,1),"Observed matching2x-to4x stencil transfer must use direct coordinates");
  check(!Eligible(false,0xb40102,484,484,11,11,1,1),"Disabled path retains generic addressing");
  for(unsigned key:{0xb40002u,0xb40103u,0xb50102u,0xb40112u,0x40b40102u,0x80b40102u})
    check(!Eligible(true,key,484,484,11,11,1,1),"Unknown sample/format/mode keys retain original shader");
  check(!Eligible(true,0xb40102,484,485,11,11,1,1),"Different base must retain wrap-aware addressing");
  check(!Eligible(true,0xb40102,484,484,11,12,1,1),"Different pitch must retain division");
  for(unsigned scale:{0u,2u,3u}) {
    check(!Eligible(true,0xb40102,484,484,11,11,scale,1),"Other X scale must fall back");
    check(!Eligible(true,0xb40102,484,484,11,11,1,scale),"Other Y scale must fall back");
  }
  check(!Eligible(true,0xb40102,2048,2048,11,11,1,1),"Invalid base must fall back");
  for(unsigned pitch:{0u,1024u,0xffffffffu})
    check(!Eligible(true,0xb40102,0,0,pitch,pitch,1,1),"Unsupported pitch must fall back");
  for(unsigned pitch=1;pitch<=1023;++pitch) {
    unsigned dw=pitch*40,sw=pitch*80,rows=2048/pitch,full_height=rows*8;
    check(Rectangle(0,0,dw,full_height,pitch,dw,16384,sw,16384),"Full nonwrapping rows must specialize");
    unsigned tail=2048%pitch;
    if(tail) check(Rectangle(0,full_height,tail*40,8,pitch,dw,16384,sw,16384),"Final partial row before wrap is valid");
    check(!Rectangle(0,full_height,dw,8,pitch,dw,16384,sw,16384),"First row reaching tile2048 must fall back");
    for(unsigned x:{0u,19u,20u,39u,dw-1}) {
      if(x>=dw) continue;
      check(Rectangle(x,0,1,1,pitch,dw,8,sw,8),"Column-swap boundary pixel must specialize");
    }
    check(!Rectangle(0,0,0,1,pitch,dw,8,sw,8),"Empty rectangles must fall back");
    check(!Rectangle(dw,0,1,1,pitch,dw,8,sw,8),"Destination X overflow must fall back");
    check(!Rectangle(0,8,1,1,pitch,dw,8,sw,8),"Destination Y overflow must fall back");
    check(!Rectangle(0,0,dw,8,pitch,dw,8,sw-1,8),"Incomplete source width must fall back");
    check(!Rectangle(0,0,dw,8,pitch,dw,8,sw,7),"Incomplete source height must fall back");
    check(!Rectangle(0,0,1,1,pitch,dw-1,8,sw,8),"Cropped image extent must not bypass column-swap bounds");
    check(!Rectangle(0xffffffff,0,2,1,pitch,dw,8,sw,8),"Wrapped X extent must fall back");
    check(!Rectangle(0,0xffffffff,1,2,pitch,dw,8,sw,8),"Wrapped Y extent must fall back");
    const unsigned generic=lb2::transfer_pitch::MakeKey(0x40102,pitch,true);
    const unsigned specialized=generic|kModeBit;
    check((specialized&kModeBit)!=0 && specialized!=generic,"Direct and generic pipelines must have distinct identity");
    check(lb2::transfer_pitch::RemovePitch(specialized)==(0x40102|kModeBit),"Cache fallback must preserve the addressing mode");
    check(lb2::transfer_pitch::Pitch(specialized)==pitch,"Mode bit must not corrupt source pitch");
    check(MakeKey(generic,true)==(0x40102|kModeBit),"Pitch-independent direct shader must have one canonical identity");
    check(MakeKey(generic,false)==generic,"Ineligible requests retain exact original key");
  }
  std::printf("PASS: %u same-origin bounds, eligibility and cache-mode checks\n",checks);
}
