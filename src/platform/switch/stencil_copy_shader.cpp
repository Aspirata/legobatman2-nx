#include "lb2_stencil_copy_shader.h"
#include <SPIRV/SpvBuilder.h>

std::vector<uint32_t> Lb2BuildStencilCopyShader() {
  spv::Builder b(spv::Spv_1_3,0x00080001,nullptr);
  b.addCapability(spv::CapabilityShader);
  b.setMemoryModel(spv::AddressingModelLogical,spv::MemoryModelGLSL450);
  const auto u=b.makeUintType(32),i=b.makeIntType(32),v2=b.makeVectorType(u,2);
  const auto i2=b.makeVectorType(i,2),v3=b.makeVectorType(u,3),v4=b.makeVectorType(u,4);
  const auto boolean=b.makeBoolType(),zero=b.makeUintConstant(0),one=b.makeUintConstant(1);
  auto image=b.createVariable(spv::NoPrecision,spv::StorageClassUniformConstant,
      b.makeImageType(u,spv::Dim2D,false,false,true,1,spv::ImageFormatUnknown),"source_stencil");
  b.addDecoration(image,spv::DecorationDescriptorSet,0);
  b.addDecoration(image,spv::DecorationBinding,1);
  auto array=b.makeRuntimeArray(u);b.addDecoration(array,spv::DecorationArrayStride,4);
  std::vector<spv::Id> members{array};auto output_type=b.makeStructType(members,"PackedStencil");
  b.addDecoration(output_type,spv::DecorationBlock);
  b.addMemberDecoration(output_type,0,spv::DecorationOffset,0);
  b.addMemberDecoration(output_type,0,spv::DecorationNonReadable);
  auto output=b.createVariable(spv::NoPrecision,spv::StorageClassStorageBuffer,output_type,"output_words");
  b.addDecoration(output,spv::DecorationDescriptorSet,1);b.addDecoration(output,spv::DecorationBinding,0);
  members.assign(5,u);auto push_type=b.makeStructType(members,"Region");
  b.addDecoration(push_type,spv::DecorationBlock);
  for(unsigned n=0;n<5;++n)b.addMemberDecoration(push_type,n,spv::DecorationOffset,int(n*4));
  auto push=b.createVariable(spv::NoPrecision,spv::StorageClassPushConstant,push_type,"region");
  auto global=b.createVariable(spv::NoPrecision,spv::StorageClassInput,v3,"gl_GlobalInvocationID");
  b.addDecoration(global,spv::DecorationBuiltIn,spv::BuiltInGlobalInvocationId);
  auto main=b.makeEntryPoint("main");
  auto word=b.createVariable(spv::NoPrecision,spv::StorageClassFunction,u,"packed");
  auto index=b.createCompositeExtract(b.createLoad(global,spv::NoPrecision),u,0);
  auto load_push=[&](unsigned n){std::vector<spv::Id> a{b.makeIntConstant(int(n))};
    return b.createLoad(b.createAccessChain(spv::StorageClassPushConstant,push,a),spv::NoPrecision);};
  auto x0=load_push(0),y0=load_push(1),width=load_push(2),height=load_push(3),row_words=load_push(4);
  auto count=b.createBinOp(spv::OpIMul,u,row_words,height);
  spv::Builder::If in_bounds(b.createBinOp(spv::OpULessThan,boolean,index,count),spv::SelectionControlMaskNone,b);
  auto row=b.createBinOp(spv::OpUDiv,u,index,row_words);
  auto x=b.createBinOp(spv::OpIMul,u,b.createBinOp(spv::OpUMod,u,index,row_words),b.makeUintConstant(4));
  auto dy=b.createBinOp(spv::OpIAdd,u,y0,row);
  b.createStore(zero,word);
  for(unsigned n=0;n<4;++n){
    auto local_x=b.createBinOp(spv::OpIAdd,u,x,b.makeUintConstant(n));
    spv::Builder::If valid_pixel(b.createBinOp(spv::OpULessThan,boolean,local_x,width),spv::SelectionControlMaskNone,b);
    auto dx=b.createBinOp(spv::OpIAdd,u,x0,local_x);
    std::vector<spv::Id> xy{b.createBinOp(spv::OpShiftRightLogical,u,dx,one),
                            b.createBinOp(spv::OpShiftRightLogical,u,dy,one)};
    auto coord=b.createUnaryOp(spv::OpBitcast,i2,b.createCompositeConstruct(v2,xy));
    auto sample=b.createBinOp(spv::OpBitwiseOr,u,b.createBinOp(spv::OpBitwiseAnd,u,dx,one),
        b.createBinOp(spv::OpShiftLeftLogical,u,b.createBinOp(spv::OpBitwiseAnd,u,dy,one),one));
    std::vector<spv::Id> fetch{b.createLoad(image,spv::NoPrecision),coord,spv::ImageOperandsSampleMask,sample};
    auto stencil=b.createCompositeExtract(b.createOp(spv::OpImageFetch,v4,fetch),u,0);
    auto byte=b.createBinOp(spv::OpBitwiseAnd,u,stencil,b.makeUintConstant(255));
    auto shifted=b.createBinOp(spv::OpShiftLeftLogical,u,byte,b.makeUintConstant(n*8));
    b.createStore(b.createBinOp(spv::OpBitwiseOr,u,b.createLoad(word,spv::NoPrecision),shifted),word);
    valid_pixel.makeEndIf();
  }
  std::vector<spv::Id> address{zero,index};
  b.createStore(b.createLoad(word,spv::NoPrecision),
                b.createAccessChain(spv::StorageClassStorageBuffer,output,address));
  in_bounds.makeEndIf();
  b.makeReturn(false);b.leaveFunction();
  auto entry=b.addEntryPoint(spv::ExecutionModelGLCompute,main,"main");entry->addIdOperand(global);
  b.addExecutionMode(main,spv::ExecutionModeLocalSize,64,1,1);
  std::vector<uint32_t> code;b.dump(code);return code;
}
