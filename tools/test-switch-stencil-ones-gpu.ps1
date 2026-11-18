$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/..").Path
$tool="$root/work/toolchain"
$build="$root/build/stencil-ones-gpu"
$stage="$root/work/analysis/stencil-ones-gpu"
New-Item -ItemType Directory -Path $stage -Force | Out-Null
. "$root/tools/host-env.ps1"
& "$tool/python/cmake/data/bin/cmake.exe" -S "$root/tests/switch_stencil_ones_gpu" -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_COMPILER=$tool/clang-cl/clang-cl.exe" -DCMAKE_CXX_COMPILER_TARGET=x86_64-pc-windows-msvc -DCMAKE_EXE_LINKER_FLAGS=/machine:x64 "-DCMAKE_MAKE_PROGRAM=$tool/python/bin/ninja.exe" *> "$stage/configure.log"
if($LASTEXITCODE -ne 0){throw "GPU harness configuration failed: $stage/configure.log"}
& "$tool/python/cmake/data/bin/cmake.exe" --build $build -j 3 *> "$stage/build.log"
if($LASTEXITCODE -ne 0){throw "GPU harness build failed: $stage/build.log"}
foreach($name in 'msvcp140.dll','msvcp140_atomic_wait.dll','vcruntime140.dll','vcruntime140_1.dll') {
 Copy-Item -LiteralPath "$root/build/pc-play/$name" -Destination $build
}
& "$build/stencil-ones-gpu.exe" "$stage/kernel.spv" "$stage" *> "$stage/results.txt"
$testExit=$LASTEXITCODE
Get-Content "$stage/results.txt"
if($testExit -ne 0){throw "GPU stencil copy validation failed (exit $testExit): $stage/results.txt"}
& "$tool/msys64/mingw64/bin/spirv-val.exe" --target-env vulkan1.2 "$stage/kernel.spv" *> "$stage/spirv-validation.log"
if($LASTEXITCODE -ne 0){Get-Content "$stage/spirv-validation.log"; throw "Production kernel SPIR-V invalid: $stage/spirv-validation.log"}
$validationResult='PASS: production kernel.spv validated by spirv-val --target-env vulkan1.2'
$validationResult | Add-Content "$stage/spirv-validation.log"
$validationResult | Add-Content "$stage/results.txt"
Write-Output $validationResult

for($i=0;$i -lt 3;$i++) {
 & "$tool/msys64/mingw64/bin/spirv-val.exe" --target-env vulkan1.2 "$stage/fragment_$i.spv" *>> "$stage/spirv-validation.log"
 if($LASTEXITCODE -ne 0){throw "Fragment SPIR-V invalid: $i"}
}
'PASS: all three production helper fragment shaders validated' | Tee-Object -FilePath "$stage/spirv-validation.log" -Append

