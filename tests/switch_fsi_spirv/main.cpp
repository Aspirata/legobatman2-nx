#include <rex/graphics/pipeline/shader/spirv.h>
#include <rex/string/buffer.h>
#include "lb2_fsi_specialization.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <format>
#include <map>

int main(int argc,char**argv) {
 if(argc!=3)return 1;
 using namespace rex::graphics;
 SpirvShaderTranslator::Modification fields;
 fields.pixel.interpolator_mask=0xffff;fields.pixel.interpolators_centroid=0xffff;
 fields.pixel.dynamic_addressable_register_count=255;fields.pixel.param_gen_enable=1;
 fields.pixel.param_gen_interpolator=15;fields.pixel.param_gen_point=1;
 fields.pixel.depth_stencil_mode=SpirvShaderTranslator::Modification::DepthStencilMode(7);
 if(fields.value&lb2::fsi::kPayloadMask)return 2;
 static_assert(unsigned(xenos::BlendFactor::kOne)==1&&unsigned(xenos::BlendFactor::kSrcAlpha)==6&&
               unsigned(xenos::BlendFactor::kOneMinusSrcAlpha)==7);
 struct Guest {uint64_t hash;std::vector<uint32_t> code;};std::map<unsigned,Guest> chosen;
 std::ifstream in(argv[1],std::ios::binary);uint64_t header;in.read((char*)&header,8);
 for(;;){uint64_t hash;uint32_t size;if(!in.read((char*)&hash,8)||!in.read((char*)&size,4))break;
  unsigned n=size&0x7fffffff;if(n>100000)return 3;std::vector<uint32_t> code(n);
  if(!in.read((char*)code.data(),n*4))break;if(!(size>>31))continue;
  SpirvShader s(xenos::ShaderType::kPixel,hash,code.data(),n);rex::string::StringBuffer scratch;s.AnalyzeUcode(scratch);
  unsigned targets=lb2::fsi::TargetCount(s.writes_color_targets());
  if(targets&&(!chosen.contains(targets)||hash==0x8D7122DBA3E31ADCull))chosen[targets]={hash,std::move(code)};
 }
 std::filesystem::create_directories(argv[2]);
 for(auto&[targets,g]:chosen){
  SpirvShader shader(xenos::ShaderType::kPixel,g.hash,g.code.data(),g.code.size());
  rex::string::StringBuffer scratch;shader.AnalyzeUcode(scratch);auto mask=shader.writes_color_targets();
  SpirvShaderTranslator::Features features(true);features.spirv_version=spv::Spv_1_5;
  features.denorm_flush_to_zero_float32=false;features.signed_zero_inf_nan_preserve_float32=false;
  features.rounding_mode_rte_float32=false;
  SpirvShaderTranslator translator(features,true,true,true);
  SpirvShaderTranslator::Modification mod(translator.GetDefaultPixelShaderModification(0));
  mod.pixel.interpolator_mask=0xffff;
  for(int scenario=-1;scenario<8;++scenario){
   std::array<uint32_t,4> formats{},blends{};formats.fill(std::array<unsigned,4>{0,1,5,7}[unsigned(std::max(scenario,0))/2]);
   blends.fill(lb2::fsi::kBlends[scenario>=0&&(scenario&1)?2:1]);
   uint64_t key=scenario<0?mod.value:lb2::fsi::Specialize(mod.value,true,mask,formats,blends);
   auto* t=shader.GetOrCreateTranslation(key);
   if(!translator.TranslateAnalyzedShader(*t)||!t->is_valid())return 4;
   const auto&bytes=t->translated_binary();auto name=std::format("{:016X}_{:04X}.spv",g.hash,key>>49);
   std::ofstream out(std::filesystem::path(argv[2])/name,std::ios::binary);out.write((char*)bytes.data(),bytes.size());
   std::cout<<name<<","<<targets<<","<<scenario<<","<<bytes.size()<<"\n";
   SpirvShader reload(xenos::ShaderType::kPixel,g.hash,g.code.data(),g.code.size());reload.AnalyzeUcode(scratch);
   auto* rt=reload.GetOrCreateTranslation(key);SpirvShaderTranslator fresh(features,true,true,true);
   if(!fresh.TranslateAnalyzedShader(*rt)||rt->translated_binary()!=bytes)return 5;
  }
 }
 return chosen.contains(1)&&chosen.size()>=2?0:6;
}
std::string& FLAGS_dump_shaders_storage_(){static std::string empty;return empty;}
