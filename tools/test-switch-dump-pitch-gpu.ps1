$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/..").Path
$tool="$root/work/toolchain"
$build="$root/build/dump-pitch-gpu"
$stage="$root/work/analysis/dump-pitch-gpu"
New-Item -ItemType Directory -Path $stage -Force | Out-Null
. "$root/tools/host-env.ps1"
& "$tool/python/cmake/data/bin/cmake.exe" -S "$root/tests/switch_dump_pitch_gpu" -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_COMPILER=$tool/clang-cl/clang-cl.exe" -DCMAKE_CXX_COMPILER_TARGET=x86_64-pc-windows-msvc -DCMAKE_EXE_LINKER_FLAGS=/machine:x64 "-DCMAKE_MAKE_PROGRAM=$tool/python/bin/ninja.exe" *> "$stage/configure.log"
if($LASTEXITCODE -ne 0){throw "Configuration failed: $stage/configure.log"}
& "$tool/python/cmake/data/bin/cmake.exe" --build $build -j 3 *> "$stage/build.log"
if($LASTEXITCODE -ne 0){throw "Build failed: $stage/build.log"}
foreach($name in 'msvcp140.dll','msvcp140_atomic_wait.dll','vcruntime140.dll','vcruntime140_1.dll') { Copy-Item -LiteralPath "$root/build/pc-play/$name" -Destination $build }
& "$build/dump-pitch-gpu.exe" $stage *> "$stage/results.txt"
$testExit=$LASTEXITCODE
Get-Content "$stage/results.txt"
if($testExit -ne 0){throw "GPU validation failed: $stage/results.txt"}
$validationLog="$stage/spirv-validation.log"
Set-Content $validationLog ''
for($i=0;$i -lt 33;$i++) {
 & "$tool/msys64/mingw64/bin/spirv-val.exe" --target-env vulkan1.2 "$stage/kernel_$i.spv" *>> $validationLog
 if($LASTEXITCODE -ne 0){throw "SPIR-V validation failed: kernel_$i.spv"}
}
'PASS 33 SPIR-V modules validated for Vulkan 1.2' | Tee-Object -FilePath $validationLog -Append
