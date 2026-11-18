#include "lb2_dump_pitch_shader.h"
#include "lb2_dump_pitch.h"
#include <SPIRV/SpvBuilder.h>
Lb2DumpTileCoordinates Lb2BuildDumpTileCoordinates(spv::Builder& builder,uint32_t numerator,
                                                  uint32_t dynamic_pitch,
                                                  uint32_t specialized_pitch) {
  const auto type_uint=builder.makeUintType(32);
  const auto pitch=specialized_pitch && specialized_pitch<=lb2::dump_pitch::kMaxPitch
      ? builder.makeUintConstant(specialized_pitch) : dynamic_pitch;
  // Preserve the original integer operations and their order. Constant pitch
  // enables NIR's exact constant-divisor lowering before generic idiv lowering.
  const auto y=builder.createBinOp(spv::OpUDiv,type_uint,numerator,pitch);
  const auto x=builder.createBinOp(spv::OpUMod,type_uint,numerator,pitch);
  return {x,y};
}
