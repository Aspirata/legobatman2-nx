"""Check private key layout and transfer source-pitch cache identity."""
from pathlib import Path
import os,re,subprocess
root=Path(__file__).resolve().parents[1]
sdk=root/'third_party/nfsmw-nx/sdk/include/rex/graphics'
key=(sdk/'vulkan/render_target_cache.h').read_text().split('union TransferShaderKey {',1)[1].split('TransferShaderKey()',1)[0]
fields=re.findall(r'(\w+)\s+:\s+([^;]+);',key)
assert fields==[
 ('dest_msaa_samples','xenos::kMsaaSamplesBits'),('dest_color_rt_index','xenos::kColorRenderTargetIndexBits'),
 ('dest_resource_format','xenos::kRenderTargetFormatBits'),('source_msaa_samples','xenos::kMsaaSamplesBits'),
 ('host_depth_source_msaa_samples','xenos::kMsaaSamplesBits'),('source_resource_format','xenos::kRenderTargetFormatBits'),('mode','4')], 'Pinned SDK transfer-key fields changed'
xenos=(sdk/'xenos.h').read_text()
for name,value in [('kMsaaSamplesBits',2),('kColorRenderTargetIndexBits',2),('kColorRenderTargetFormatBits',4),('kDepthRenderTargetFormatBits',1)]:
 assert re.search(r'constexpr uint32_t '+name+r'\s*=\s*'+str(value)+r'\s*;',xenos), 'Pinned key field width changed'
stage=root/'work/analysis/transfer-pitch-test';stage.mkdir(parents=True,exist_ok=True)
compiler=root/'work/toolchain/msys64/mingw64/bin/g++.exe';exe=stage/'test.exe'
env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ['PATH'])
subprocess.run([str(compiler),'-std=c++20','-O2','-I',str(root/'src/platform/switch'),str(root/'tests/switch_transfer_pitch.cpp'),'-o',str(exe)],env=env,check=True)
subprocess.run([str(exe)],env=env,check=True)
