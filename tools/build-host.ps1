param([int]$Jobs = 4)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/host-env.ps1"
$repoRoot = (Resolve-Path "$PSScriptRoot/..").Path.Replace('\', '/')
$toolRoot = "$repoRoot/work/toolchain"
$cmake = "$toolRoot/python/cmake/data/bin/cmake.exe"
$build = "$repoRoot/build/host"
Push-Location $repoRoot
try {
    & python tools/codegen.py
    if ($LASTEXITCODE -ne 0) { throw 'Codegen failed; see work/analysis/codegen.log' }
    & $cmake -S $repoRoot -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release `
        "-DCMAKE_MAKE_PROGRAM=$toolRoot/python/bin/ninja.exe" `
        "-DCMAKE_CXX_COMPILER=$toolRoot/clang-cl/clang-cl.exe" `
        -DCMAKE_CXX_COMPILER_TARGET=x86_64-pc-windows-msvc `
        "-DCMAKE_LINKER=$toolRoot/msvc/Contents/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/link.exe" `
        "-DCMAKE_PREFIX_PATH=$toolRoot/rexglue-0.10.0/win-amd64"
    if ($LASTEXITCODE -ne 0) { throw 'Host configuration failed' }
    & $cmake --build $build -j $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Host build failed' }
    & "$toolRoot/python/cmake/data/bin/ctest.exe" --test-dir $build --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Host tests failed' }
} finally { Pop-Location }
