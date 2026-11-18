"""Execute the SDK descriptor-binding loop for every combination of dirty sets."""
from pathlib import Path
import os
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
experimental = '--experimental' in sys.argv
stage = root / 'work/analysis/switch-dynamic-constants'
stage.mkdir(parents=True, exist_ok=True)
sdk = root / 'third_party/nfsmw-nx/sdk'
source = (sdk / 'src/graphics/vulkan/command_processor.cpp').read_text()
start = source.index('  uint32_t descriptor_sets_remaining =', source.index('// Bind the new descriptor sets.'))
end = source.index('  return true;', start)
binding = source[start:end]
type_start = source.find('constexpr VkDescriptorType kGuestConstantsDescriptorType =')
type_declaration = (source[type_start:source.index('#endif', type_start) + len('#endif')]
                    if type_start >= 0 else
                    'constexpr VkDescriptorType kGuestConstantsDescriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;')
assignments = []
for field in ('constants_binding.descriptorType', 'constants_descriptor_count.type', 'write_constants.descriptorType'):
    assignments.append(re.search(re.escape(field) + r'\s*=\s*[^;]+;', source)[0])
constructor = source[source.index('transient_descriptor_allocator_uniform_buffer_('):
                     source.index('transient_descriptor_allocator_storage_buffer_(')]
pool_arguments = constructor.split('->vulkan_device(),', 1)[1].strip()
assert pool_arguments.endswith('),')
pool_arguments = pool_arguments[:-2]
helper = root / 'src/platform/switch/lb2_vulkan_constants.h'
include = f'#include "{helper.as_posix()}"' if helper.exists() else '''
namespace lb2 { constexpr VkDescriptorType kConstantDescriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
auto UniformPoolSizes(VkDescriptorPoolSize s) {return std::array{s};} }
'''
test = r'''
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <vulkan/vulkan.h>
''' + include + '\n' + type_declaration + r'''
struct SpirvShaderTranslator {enum {kDescriptorSetConstants=1,kConstantBufferCount=5};};
namespace rex {
bool bit_scan_forward(uint32_t bits,uint32_t* index) {if(!bits)return false;*index=std::countr_zero(bits);return true;}
unsigned tzcnt(uint32_t bits) {return std::countr_zero(bits);}
}
struct Layout { VkPipelineLayout GetPipelineLayout(){return {};}};
struct Call {unsigned first,count,dynamics;std::vector<uint32_t> offsets;};
struct Recorder {
 std::vector<Call> calls;
 void CmdVkBindDescriptorSets(VkPipelineBindPoint,VkPipelineLayout,unsigned first,unsigned count,
                             const VkDescriptorSet*,unsigned dynamics,const uint32_t* offsets) {
  Call c{first,count,dynamics,{}};
  if(dynamics)c.offsets.assign(offsets,offsets+dynamics);
  calls.push_back(c);
 }
};
struct Allocator {
 std::vector<VkDescriptorPoolSize> sizes;
 Allocator(int,const VkDescriptorPoolSize* p,unsigned n,unsigned) : sizes(p,p+n) {}
};
bool bind(unsigned dirty,Recorder& deferred_command_buffer_) {
 uint32_t descriptor_sets_needed=15;
 uint32_t current_graphics_descriptor_sets_bound_up_to_date_=15^dirty;
 VkDescriptorSet current_graphics_descriptor_sets_[4]{};
 Layout layout;auto* current_guest_graphics_pipeline_layout_=&layout;
''' + binding + r'''
 return true;
}
int main() {
 unsigned errors=0;
 auto check=[&](bool ok,const char* text){if(!ok){std::printf("FAIL: %s\n",text);errors++;}};
 constexpr bool expect_dynamic=REX_PLATFORM_SWITCH && LB2_EXPECT_DYNAMIC;
 constexpr VkDescriptorType expected_type=expect_dynamic?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
 VkDescriptorSetLayoutBinding constants_binding{};
 VkDescriptorPoolSize constants_descriptor_count{};
 VkWriteDescriptorSet write_constants{};
''' + '\n'.join(assignments) + r'''
 check(constants_binding.descriptorType==expected_type &&
       constants_descriptor_count.type==constants_binding.descriptorType &&
       write_constants.descriptorType==constants_binding.descriptorType,
       "Actual layout, descriptor allocation and write types must match the direct path");
 const VkDescriptorPoolSize kDescriptorPoolSizeUniformBuffer{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,5120};
 constexpr unsigned kLinkedTypeDescriptorPoolSetCount=1024;
 Allocator allocator(0,
''' + pool_arguments + r''');
 const auto& pools=allocator.sizes;
 bool ordinary=false,dynamic=false;
 for(auto p:pools){ordinary|=p.type==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER&&p.descriptorCount==5120;
                   dynamic|=p.type==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC&&p.descriptorCount==5120;}
 check(ordinary && (dynamic==expect_dynamic),"Pool must retain static compute UBO capacity and match the selected guest UBO path");
 for(unsigned dirty=0;dirty<16;dirty++) {
  Recorder recorder;bind(dirty,recorder);unsigned seen=0;
  for(const auto& call:recorder.calls) {
   unsigned mask=((1u<<call.count)-1)<<call.first;seen|=mask;
   const bool constants=(mask&2)!=0;
   check(call.dynamics==((expect_dynamic&&constants)?5u:0u),"Dynamic offsets must match the selected guest UBO path and bound set range");
   for(auto offset:call.offsets)check(offset==0,"Dynamic offset must preserve the original descriptor byte address");
   // Simulate five unrelated aligned buffer regions and verify address/range
   // semantics stay identical, including a nonzero original base offset.
   if(constants && call.offsets.size()==5)for(unsigned i=0;i<5;i++) {
    uint64_t original=0x10000000ull+i*0x20000+0x100*i;
    check(original+call.offsets[i]==original,"Bound address changed");
   }
  }
  check(seen==dirty,"Unchanged descriptor sets must not be rebound");
 }
 if(!errors)std::printf("PASS: platform Switch=%d, actual descriptor types/pools, all16 dirty masks and unchanged addresses\n",REX_PLATFORM_SWITCH);
 return errors?1:0;
}
'''
(stage / 'test.cpp').write_text(test)
compiler = root / 'work/toolchain/msys64/mingw64/bin/g++.exe'
env = dict(os.environ, PATH=str(compiler.parent) + os.pathsep + os.environ['PATH'])
exe = stage / 'test.exe'
for switch in (1, 0):
    subprocess.run([str(compiler), '-std=c++20', '-O2', f'-DREX_PLATFORM_SWITCH={switch}',
                    f'-DLB2_EXPECT_DYNAMIC={int(experimental)}', '-I',
                    str(sdk / 'thirdparty/vulkan-headers/include'), str(stage / 'test.cpp'), '-o', str(exe)], env=env, check=True)
    subprocess.run([str(exe)], env=env, check=True)
