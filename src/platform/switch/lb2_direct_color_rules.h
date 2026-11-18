#pragma once
#include "lb2_direct_color_shader.h"
namespace lb2::direct_color {
struct Request {
  bool host_path=false,scale_one=false,single_sample=false,rgba8_exact=false;
  bool full_owner=false,plain_2d=false,sample_zero=false,no_bias=false;
  uint32_t owner_base=0,owner_pitch=0,owner_height=0,resolve_base=0,resolve_pitch=0;
  uint32_t offset_x=0,offset_y=0,width=0,height=0;
  uint32_t dst_x=0,dst_y=0,dst_pitch=0,dst_base=0,endian=0,swap=0;
  uint64_t dst_range=0;
};
inline uint32_t TiledOffset(uint32_t x,uint32_t y,uint32_t pitch) {
  pitch=(pitch+31)&~31u;
  const uint32_t macro=((x>>5)+(y>>5)*(pitch>>5))<<9;
  const uint32_t micro=((x&7)+((y&14)<<2))<<2;
  const uint32_t off=macro+((micro&~15u)<<1)+(micro&15)+((y&1)<<4);
  return ((off&~511u)<<3)+((y&16)<<7)+((off&448)<<2)+
      (((((y&8)>>2)+(x>>3))&3)<<6)+(off&63);
}
inline bool MakePlan(const Request& r,Lb2DirectColorPush& p) {
  p={};
  if(!r.host_path||!r.scale_one||!r.single_sample||!r.rgba8_exact||!r.full_owner||
      !r.plain_2d||!r.sample_zero||!r.no_bias)return false;
  if(!r.owner_pitch||r.owner_pitch>255||r.resolve_pitch!=r.owner_pitch||
      r.owner_base>=2048||r.resolve_base<r.owner_base||r.resolve_base>=2048)return false;
  if(!r.width||!r.height||r.width>8192||r.height>8192||((r.width|r.height)&7))return false;
  const uint64_t tile_offset=r.resolve_base-r.owner_base;
  const uint64_t sx=(tile_offset%r.owner_pitch)*80+r.offset_x;
  const uint64_t sy=(tile_offset/r.owner_pitch)*16+r.offset_y;
  if(sx+r.width>uint64_t(r.owner_pitch)*80||sy+r.height>r.owner_height)return false;
  const uint64_t last_tile=r.owner_base+((sy+r.height-1)/16)*r.owner_pitch+(sx+r.width-1)/80;
  if(last_tile>=2048)return false;
  if(!r.dst_pitch||r.dst_pitch>16384||r.dst_x>127||r.dst_y>127||
      uint64_t(r.dst_x)+r.width>r.dst_pitch||(r.dst_base&3)||r.endian>3||r.swap>1)return false;
  const uint32_t last_x=(r.dst_x+r.width-1)&~31u,last_y=(r.dst_y+r.height-1)&~31u;
  const uint64_t upper=uint64_t(TiledOffset(last_x,last_y,r.dst_pitch))+4096;
  if(r.dst_range>512ull*1024*1024||uint64_t(r.dst_base)+upper>r.dst_range)return false;
  p={uint32_t(sx),uint32_t(sy),r.width,r.height,r.dst_x,r.dst_y,r.dst_pitch,r.dst_base,r.endian,r.swap};
  return true;
}
}
