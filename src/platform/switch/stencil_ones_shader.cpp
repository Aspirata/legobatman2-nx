#include "lb2_stencil_ones_shader.h"
#include <SPIRV/SpvBuilder.h>
uint32_t Lb2BuildStencilKill(spv::Builder& b,uint32_t packed,uint32_t mask,
                            lb2::stencil_ones::ShaderMode mode) {
  const auto u=b.makeUintType(32),boolean=b.makeBoolType();
  if(mode==lb2::stencil_ones::ShaderMode::kBulk) {
    const auto byte=b.createBinOp(spv::OpBitwiseAnd,u,packed,b.makeUintConstant(255));
    return b.createBinOp(spv::OpINotEqual,boolean,byte,b.makeUintConstant(255));
  }
  const auto empty=b.createBinOp(spv::OpIEqual,boolean,
      b.createBinOp(spv::OpBitwiseAnd,u,packed,mask),b.makeUintConstant(0));
  if(mode==lb2::stencil_ones::ShaderMode::kResidual) {
    const auto byte=b.createBinOp(spv::OpBitwiseAnd,u,packed,b.makeUintConstant(255));
    const auto full=b.createBinOp(spv::OpIEqual,boolean,byte,b.makeUintConstant(255));
    return b.createBinOp(spv::OpLogicalOr,boolean,full,empty);
  }
  return empty;
}
