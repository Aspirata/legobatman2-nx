#include "lb2_direct_color_shader.h"
#include <SPIRV/SpvBuilder.h>

std::vector<uint32_t> Lb2BuildDirectColorResolveShader() {
  spv::Builder b(spv::Spv_1_3,0x00080001,nullptr);
  b.addCapability(spv::CapabilityShader);
  b.setMemoryModel(spv::AddressingModelLogical,spv::MemoryModelGLSL450);
  const auto u=b.makeUintType(32),i=b.makeIntType(32),f=b.makeFloatType(32);
  const auto u2=b.makeVectorType(u,2),i2=b.makeVectorType(i,2);
  const auto u3=b.makeVectorType(u,3),f4=b.makeVectorType(f,4),boolean=b.makeBoolType();
  auto c=[&](uint32_t x){return b.makeUintConstant(x);};
  auto bin=[&](spv::Op op,spv::Id a,spv::Id d){return b.createBinOp(op,u,a,d);};
  auto add=[&](spv::Id a,spv::Id d){return bin(spv::OpIAdd,a,d);};
  auto mul=[&](spv::Id a,spv::Id d){return bin(spv::OpIMul,a,d);};
  auto land=[&](spv::Id a,uint32_t mask){return bin(spv::OpBitwiseAnd,a,c(mask));};
  auto shl=[&](spv::Id a,uint32_t bits){return bin(spv::OpShiftLeftLogical,a,c(bits));};
  auto shr=[&](spv::Id a,uint32_t bits){return bin(spv::OpShiftRightLogical,a,c(bits));};
  auto lor=[&](spv::Id a,spv::Id d){return bin(spv::OpBitwiseOr,a,d);};
  const auto source=b.createVariable(spv::NoPrecision,spv::StorageClassUniformConstant,
      b.makeImageType(f,spv::Dim2D,false,false,false,1,spv::ImageFormatUnknown),"source_rgba8");
  b.addDecoration(source,spv::DecorationDescriptorSet,0);b.addDecoration(source,spv::DecorationBinding,0);
  auto words=b.makeRuntimeArray(u);b.addDecoration(words,spv::DecorationArrayStride,4);
  std::vector<spv::Id> members{words};const auto out_type=b.makeStructType(members,"GuestPixels");
  b.addDecoration(out_type,spv::DecorationBlock);b.addMemberDecoration(out_type,0,spv::DecorationOffset,0);
  b.addMemberDecoration(out_type,0,spv::DecorationNonReadable);
  const auto output=b.createVariable(spv::NoPrecision,spv::StorageClassStorageBuffer,out_type,"guest_pixels");
  b.addDecoration(output,spv::DecorationDescriptorSet,1);b.addDecoration(output,spv::DecorationBinding,0);
  members.assign(10,u);const auto push_type=b.makeStructType(members,"ResolveRegion");
  b.addDecoration(push_type,spv::DecorationBlock);
  for(unsigned n=0;n<10;++n)b.addMemberDecoration(push_type,n,spv::DecorationOffset,n*4);
  const auto push=b.createVariable(spv::NoPrecision,spv::StorageClassPushConstant,push_type,"region");
  const auto global=b.createVariable(spv::NoPrecision,spv::StorageClassInput,u3,"global_id");
  b.addDecoration(global,spv::DecorationBuiltIn,spv::BuiltInGlobalInvocationId);
  const auto main=b.makeEntryPoint("main");
  auto load=[&](unsigned n){std::vector<spv::Id> index{b.makeIntConstant(n)};
    return b.createLoad(b.createAccessChain(spv::StorageClassPushConstant,push,index),spv::NoPrecision);};
  const auto id=b.createLoad(global,spv::NoPrecision);
  const auto x=b.createCompositeExtract(id,u,0),y=b.createCompositeExtract(id,u,1);
  const auto width=load(2),height=load(3);
  const auto in_x=b.createBinOp(spv::OpULessThan,boolean,x,width);
  const auto in_y=b.createBinOp(spv::OpULessThan,boolean,y,height);
  spv::Builder::If in_bounds(b.createBinOp(spv::OpLogicalAnd,boolean,in_x,in_y),spv::SelectionControlMaskNone,b);
  std::vector<spv::Id> xy{add(load(0),x),add(load(1),y)};
  const auto coord=b.createUnaryOp(spv::OpBitcast,i2,b.createCompositeConstruct(u2,xy));
  std::vector<spv::Id> fetch{b.createLoad(source,spv::NoPrecision),coord,spv::ImageOperandsLodMask,b.makeIntConstant(0)};
  const auto color=b.createOp(spv::OpImageFetch,f4,fetch);
  auto packed=c(0);
  for(unsigned n=0;n<4;++n){
    const auto component=b.createCompositeExtract(color,f,n);
    const auto scaled=b.createBinOp(spv::OpFMul,f,component,b.makeFloatConstant(255.0f));
    const auto rounded=b.createBinOp(spv::OpFAdd,f,scaled,b.makeFloatConstant(0.5f));
    packed=lor(packed,shl(b.createUnaryOp(spv::OpConvertFToU,u,rounded),n*8));
  }
  const auto swapped=lor(land(packed,0xFF00FF00),lor(shl(land(packed,255),16),land(shr(packed,16),255)));
  packed=b.createTriOp(spv::OpSelect,u,b.createBinOp(spv::OpINotEqual,boolean,load(9),c(0)),swapped,packed);
  const auto swap8=lor(shl(land(packed,0x00FF00FF),8),land(shr(packed,8),0x00FF00FF));
  const auto swap32=lor(shl(swap8,16),shr(swap8,16));
  const auto swap16=lor(shl(packed,16),shr(packed,16));
  const auto endian=load(8);
  packed=b.createTriOp(spv::OpSelect,u,b.createBinOp(spv::OpIEqual,boolean,endian,c(1)),swap8,packed);
  packed=b.createTriOp(spv::OpSelect,u,b.createBinOp(spv::OpIEqual,boolean,endian,c(2)),swap32,packed);
  packed=b.createTriOp(spv::OpSelect,u,b.createBinOp(spv::OpIEqual,boolean,endian,c(3)),swap16,packed);
  const auto dx=add(load(4),x),dy=add(load(5),y),pitch=load(6);
  // Exact SDK texture_util::GetTiledOffset2D with bytes_per_block_log2=2.
  const auto macro=shl(add(shr(dx,5),mul(shr(dy,5),shr(add(pitch,c(31)),5))),9);
  const auto micro=shl(add(land(dx,7),shl(land(dy,14),2)),2);
  const auto offset=add(add(macro,shl(land(micro,0xFFFFFFF0),1)),add(land(micro,15),shl(land(dy,1),4)));
  auto address=add(shl(land(offset,0xFFFFFE00),3),shl(land(dy,16),7));
  address=add(address,shl(land(offset,0x1C0),2));
  address=add(address,shl(land(add(shr(land(dy,8),2),shr(dx,3)),3),6));
  address=add(address,land(offset,63));address=add(address,load(7));
  std::vector<spv::Id> index{c(0),shr(address,2)};
  b.createStore(packed,b.createAccessChain(spv::StorageClassStorageBuffer,output,index));
  in_bounds.makeEndIf();b.makeReturn(false);b.leaveFunction();
  auto entry=b.addEntryPoint(spv::ExecutionModelGLCompute,main,"main");entry->addIdOperand(global);
  b.addExecutionMode(main,spv::ExecutionModeLocalSize,8,8,1);
  std::vector<uint32_t> code;b.dump(code);return code;
}
