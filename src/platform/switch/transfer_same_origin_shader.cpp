#include "lb2_transfer_same_origin_shader.h"
#include <SPIRV/SpvBuilder.h>
Lb2TransferSameOriginCoordinates Lb2BuildTransferSameOriginCoordinates(
    spv::Builder& b,uint32_t x,uint32_t y,uint32_t sample) {
  const auto u=b.makeUintType(32),boolean=b.makeBoolType();
  const auto local_x=b.createBinOp(spv::OpUMod,u,x,b.makeUintConstant(40));
  const auto offset=b.createTriOp(spv::OpSelect,u,
      b.createBinOp(spv::OpULessThan,boolean,local_x,b.makeUintConstant(20)),
      b.makeUintConstant(40),b.makeUintConstant(uint32_t(-40)));
  const auto sample_x=b.createBinOp(spv::OpIAdd,u,
      b.createBinOp(spv::OpIMul,u,x,b.makeUintConstant(2)),
      b.createBinOp(spv::OpBitwiseAnd,u,sample,b.makeUintConstant(1)));
  // Uint wrap implements subtraction in the second half; the final valid
  // image coordinate is nonnegative. Y and source sample remap stay exact.
  return {b.createBinOp(spv::OpIAdd,u,sample_x,offset),y};
}
