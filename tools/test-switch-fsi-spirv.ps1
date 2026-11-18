param([Parameter(Mandatory=$true)][string]$ShaderCache)
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/..").Path
$cache=(Resolve-Path -LiteralPath $ShaderCache).Path
$tool="$root/work/toolchain"
$build="$root/build/fsi-spirv-test"
$stage="$root/work/analysis/switch-fsi-spirv-test"
New-Item -ItemType Directory -Path $stage -Force|Out-Null
. "$root/tools/host-env.ps1"
& "$tool/python/cmake/data/bin/cmake.exe" -S "$root/tests/switch_fsi_spirv" -B $build -G Ninja `
 -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_COMPILER=$tool/clang-cl/clang-cl.exe" `
 -DCMAKE_CXX_COMPILER_TARGET=x86_64-pc-windows-msvc -DCMAKE_EXE_LINKER_FLAGS=/machine:x64 `
 "-DCMAKE_MAKE_PROGRAM=$tool/python/bin/ninja.exe" `
 "-Drexglue_DIR=$tool/rexglue-0.10.0/win-amd64/lib/cmake/rexglue" *> "$stage/configure.log"
if($LASTEXITCODE -ne 0){throw "FSI test configuration failed: $stage/configure.log"}
& "$tool/python/cmake/data/bin/cmake.exe" --build $build -j 3 *> "$stage/build.log"
if($LASTEXITCODE -ne 0){throw "FSI test build failed: $stage/build.log"}
Copy-Item -LiteralPath "$tool/rexglue-0.10.0/win-amd64/bin/rexruntime.dll" -Destination $build
foreach($name in 'msvcp140.dll','msvcp140_atomic_wait.dll','vcruntime140.dll','vcruntime140_1.dll') {
 Copy-Item -LiteralPath "$root/build/pc-play/$name" -Destination $build
}
& "$build/fsi-spirv.exe" $cache "$stage/output" *> "$stage/translate.csv"
if($LASTEXITCODE -ne 0){throw "FSI translation/reload failed: $stage/translate.csv"}
$env:PATH="$tool/msys64/mingw64/bin;$env:PATH"
$rows=@()
foreach($row in Import-Csv "$stage/translate.csv" -Header file,targets,scenario,raw) {
 $inputPath=Join-Path "$stage/output" $row.file
 & spirv-val --target-env vulkan1.2 --scalar-block-layout $inputPath
 if($LASTEXITCODE -ne 0){throw "Invalid FSI module: $($row.file)"}
 $optimized="$inputPath.optimized"
 & spirv-opt --target-env=vulkan1.2 --scalar-block-layout -O $inputPath -o $optimized
 if($LASTEXITCODE -ne 0){throw 'FSI optimization failed'}
 & spirv-val --target-env vulkan1.2 --scalar-block-layout $optimized
 if($LASTEXITCODE -ne 0){throw 'Invalid optimized FSI module'}
 $rows+=[PSCustomObject]@{file=$row.file;targets=[int]$row.targets;scenario=[int]$row.scenario;bytes=(Get-Item -LiteralPath $optimized).Length}
}
$rows|ConvertTo-Json|Set-Content "$stage/results.json"
$rows|Where-Object scenario -LE 0|Format-Table
foreach($base in $rows|Where-Object scenario -eq -1) {
 $specialized=$rows|Where-Object {$_.targets -eq $base.targets -and $_.scenario -eq 0}
 if($specialized.bytes -ge $base.bytes*0.7){throw "FSI specialization failed to remove generic format/blend code for $($base.targets) targets"}
}
Write-Output 'PASS: actual FSI translation, pixel-key isolation, cache reload, validation and code reduction'
