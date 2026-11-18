param([Parameter(Mandatory = $true)][string]$ShaderCache)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/..").Path
$cache = (Resolve-Path -LiteralPath $ShaderCache).Path
$build = "$root/build/memexport-spirv-test"
$stage = "$root/work/analysis/switch-memexport-spirv-test"
$tool = "$root/work/toolchain"
New-Item -ItemType Directory -Force -Path $stage | Out-Null
. "$root/tools/host-env.ps1"
& "$tool/python/cmake/data/bin/cmake.exe" -S "$root/tests/switch_memexport_spirv" -B $build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_COMPILER=$tool/clang-cl/clang-cl.exe" `
    -DCMAKE_CXX_COMPILER_TARGET=x86_64-pc-windows-msvc -DCMAKE_EXE_LINKER_FLAGS=/machine:x64 `
    "-DCMAKE_MAKE_PROGRAM=$tool/python/bin/ninja.exe" `
    "-Drexglue_DIR=$tool/rexglue-0.10.0/win-amd64/lib/cmake/rexglue" *> "$stage/configure.log"
if ($LASTEXITCODE -ne 0) { throw "Test configuration failed: $stage/configure.log" }
& "$tool/python/cmake/data/bin/cmake.exe" --build $build -j 3 *> "$stage/build.log"
if ($LASTEXITCODE -ne 0) { throw "Test build failed: $stage/build.log" }
Copy-Item -LiteralPath "$tool/rexglue-0.10.0/win-amd64/bin/rexruntime.dll" -Destination $build
foreach ($name in 'msvcp140.dll', 'msvcp140_atomic_wait.dll', 'vcruntime140.dll', 'vcruntime140_1.dll') {
    if (Test-Path -LiteralPath "$root/build/pc-play/$name") {
        Copy-Item -LiteralPath "$root/build/pc-play/$name" -Destination $build
    }
}
& "$build/memexport-spirv.exe" $cache "$stage/output" *> "$stage/translate.log"
if ($LASTEXITCODE -ne 0) { throw "Translation/cache reload failed: $stage/translate.log" }
$env:PATH = "$tool/msys64/mingw64/bin;$env:PATH"
$results = @()
foreach ($file in Get-ChildItem "$stage/output" -Filter '*.spv') {
    & spirv-val --target-env vulkan1.2 --scalar-block-layout $file.FullName
    if ($LASTEXITCODE -ne 0) { throw "Invalid SPIR-V: $($file.Name)" }
    $optimized = Join-Path $file.DirectoryName ($file.BaseName + '.optimized')
    & spirv-opt --target-env=vulkan1.2 --scalar-block-layout -O $file.FullName -o $optimized
    if ($LASTEXITCODE -ne 0) { throw "Optimization failed: $($file.Name)" }
    & spirv-val --target-env vulkan1.2 --scalar-block-layout $optimized
    if ($LASTEXITCODE -ne 0) { throw "Invalid optimized SPIR-V: $($file.Name)" }
    $results += [PSCustomObject]@{ variant = $file.BaseName; bytes = (Get-Item -LiteralPath $optimized).Length }
}
$generic = ($results | Where-Object variant -eq '0000000000000000').bytes
if ($results.Count -ne 5 -or -not $generic) { throw 'Missing test variants' }
foreach ($result in $results | Where-Object variant -ne '0000000000000000') {
    if ($result.bytes -ge $generic / 4) { throw 'Generic format code survived specialization' }
}
$results | ConvertTo-Json | Set-Content "$stage/results.json"
$results | Format-Table
Write-Output 'PASS: SDK translation, vertex-key isolation, cache reload, SPIR-V validation and dead-format elimination'
