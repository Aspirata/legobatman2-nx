#include <cstdio>
#include <cstdlib>
#include "../src/platform/switch/lb2_direct_color_rules.h"
int main(){
 unsigned checks=0;auto check=[&](bool v,const char* why){++checks;if(!v){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}};
 using namespace lb2::direct_color;
 Request r;r.host_path=r.scale_one=r.single_sample=r.rgba8_exact=r.full_owner=r.plain_2d=r.sample_zero=r.no_bias=true;
 r.owner_pitch=r.resolve_pitch=11;r.owner_height=2992;r.width=848;r.height=480;
 r.dst_pitch=864;r.dst_range=4*1024*1024;
 Lb2DirectColorPush p{};
 check(MakePlan(r,p),"Fully owned1xRGBA8 scene resolve must qualify");
 check(p.src_x==0&&p.src_y==0&&p.width==848&&p.height==480,"Prepared source rectangle must preserve resolve dimensions");
 for(auto member:{&Request::host_path,&Request::scale_one,&Request::single_sample,&Request::rgba8_exact,&Request::full_owner,&Request::plain_2d,&Request::sample_zero,&Request::no_bias}){
  auto bad=r;bad.*member=false;check(!MakePlan(bad,p),"Each semantic guard must fail closed");
 }
 auto shifted=r;shifted.owner_base=20;shifted.resolve_base=32;shifted.offset_x=8;shifted.offset_y=8;shifted.width=64;shifted.height=32;
 check(MakePlan(shifted,p)&&p.src_x==88&&p.src_y==24,"Adjusted source base and small packed offsets combine exactly once");
 auto bad=r;bad.resolve_pitch=10;check(!MakePlan(bad,p),"Mismatched pitches cannot use direct coordinates");
 bad=r;bad.owner_base=1;check(!MakePlan(bad,p),"Source before owner base cannot silently wrap");
 bad=r;bad.owner_pitch=bad.resolve_pitch=0;check(!MakePlan(bad,p),"Zero source pitch is invalid");
 bad=r;bad.owner_height=479;check(!MakePlan(bad,p),"Source height bounds are mandatory");
 bad=r;bad.offset_x=40;check(!MakePlan(bad,p),"Resolve row must not extend across source pitch");
 bad=r;bad.width=847;check(!MakePlan(bad,p),"Non8-aligned original resolve falls back");
 bad=r;bad.height=0;check(!MakePlan(bad,p),"Empty resolve falls back");
 bad=r;bad.dst_range=4;check(!MakePlan(bad,p),"Destination tiled footprint must fit descriptor range");
 bad=r;bad.dst_base=2;check(!MakePlan(bad,p),"Unaligned destination buffer base falls back");
 bad=r;bad.endian=4;check(!MakePlan(bad,p),"Cross-word64/128 endian modes are unsupported");
 bad=r;bad.swap=2;check(!MakePlan(bad,p),"Unknown channel swap mode falls back");
 bad=r;bad.owner_base=bad.resolve_base=2040;check(!MakePlan(bad,p),"Global EDRAM tile wrap must fail closed");
 for(unsigned pitch:{1u,31u,32u,33u,64u,848u,864u,880u})for(unsigned y=0;y<96;++y)for(unsigned x=0;x<96;++x)
  check(TiledOffset(x,y,pitch)==uint32_t(SdkTiledOffset(int32_t(x),int32_t(y),pitch,2)),"Exact SDK tiled oracle must match across pitches and tile boundaries");
 std::printf("PASS: %u direct-color eligibility, bounds and exact SDK tiled-address checks\n",checks);
}
