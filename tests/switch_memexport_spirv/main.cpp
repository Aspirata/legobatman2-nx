#include <rex/graphics/pipeline/shader/spirv.h>
#include <rex/string/buffer.h>
#include "lb2_memexport_format.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <format>
#include <cstring>
int main(int argc, char** argv) {
 if(argc!=3)return 1;
 std::ifstream in(argv[1],std::ios::binary);uint64_t header;in.read((char*)&header,8);
 std::vector<uint32_t> code;
 for(;;){uint64_t hash;uint32_t size;if(!in.read((char*)&hash,8)||!in.read((char*)&size,4))return 2;
  uint32_t count=size&0x7fffffff;code.resize(count);if(!in.read((char*)code.data(),count*4))return 3;
  if(hash==lb2::memexport::kCopyShaderHash)break;
 }
 using namespace rex::graphics;
 SpirvShader shader(xenos::ShaderType::kVertex,lb2::memexport::kCopyShaderHash,code.data(),code.size());
 rex::string::StringBuffer scratch;shader.AnalyzeUcode(scratch);
 SpirvShaderTranslator::Features features(true);
 features.spirv_version=spv::Spv_1_5;
 features.fragment_shader_sample_interlock=false;
 features.denorm_flush_to_zero_float32=false;
 features.signed_zero_inf_nan_preserve_float32=false;
 features.rounding_mode_rte_float32=false;
 SpirvShaderTranslator translator(features,false,false,false);
 auto base=translator.GetDefaultVertexShaderModification(0);
 // Verify the pinned SDK fields don't overlap the added vertex-key bits.
 SpirvShaderTranslator::Modification all;
 all.vertex.interpolator_mask=0xffff;all.vertex.output_point_parameters=1;
 all.vertex.dynamic_addressable_register_count=255;all.vertex.user_clip_plane_count=7;
 all.vertex.user_clip_plane_cull=1;all.vertex.vertex_kill_and=1;all.vertex.point_ps_ucp_mode=3;
 all.vertex.host_vertex_shader_type=Shader::HostVertexShaderType(15);all.vertex.tessellation_mode=3;
 if(all.value & lb2::memexport::kKeyMask)return 4;
 std::filesystem::create_directories(argv[2]);
 for(uint32_t f : {0u,0x4b021a00u,0x4b031a00u,0x4b0a1a02u,0x4b002600u}) {
  uint64_t key=f?lb2::memexport::Specialize(base,lb2::memexport::kCopyShaderHash,84,true,f,0,0,0x40800000,0x41000000):base;
  auto* translation=shader.GetOrCreateTranslation(key);
  if(!translator.TranslateAnalyzedShader(*translation)||!translation->is_valid())return 5;
  const auto& bytes=translation->translated_binary();
  auto name=std::format("{:016X}.spv",key);
  std::ofstream out(std::filesystem::path(argv[2])/name,std::ios::binary);
  out.write((char*)bytes.data(),bytes.size());
  std::cout<<name<<","<<bytes.size()<<"\n";
  // Simulate stored-translation reload into a fresh shader/translator; no
  // guest registers are supplied, so the key must fully determine the binary.
  SpirvShader fresh(xenos::ShaderType::kVertex,lb2::memexport::kCopyShaderHash,code.data(),code.size());
  fresh.AnalyzeUcode(scratch);auto* reloaded=fresh.GetOrCreateTranslation(key);
  SpirvShaderTranslator fresh_translator(features,false,false,false);
  if(!fresh_translator.TranslateAnalyzedShader(*reloaded) || reloaded->translated_binary()!=bytes)return 6;
 }
}
std::string& FLAGS_dump_shaders_storage_(){static std::string empty;return empty;}
