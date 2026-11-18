param(
    [ValidateRange(1, 32)][int]$Jobs = 2,
    [string]$MesaSdk = '',
    [switch]$RecompileGuest,
    [switch]$GpuTimestamps,
    [switch]$StencilOnesSplit
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path "$PSScriptRoot/..").Path.Replace('\', '/')
$toolRoot = "$repoRoot/work/toolchain"
$cmake = "$toolRoot/python/cmake/data/bin/cmake.exe"
$build = "$repoRoot/build/switch-game"
if (-not $MesaSdk) {
    $MesaSdk = "$toolRoot/mesa-source/mesa-switch-1a8c1a66d6fd8d65f10107c4627ffc3606ba5631/mesa-unified-install/opt/devkitpro/portlibs/switch"
}
if (-not (Test-Path -LiteralPath "$MesaSdk/lib/libvulkan.a")) {
    throw 'Build the pinned Horizon Mesa SDK first; see docs/porting-casebook.md'
}
$previousPath = $env:PATH
$previousDevkit = $env:DEVKITPRO
Push-Location $repoRoot
try {
    $env:DEVKITPRO = "$toolRoot/switch-root/opt/devkitpro"
    $env:PATH = "$toolRoot/msys64/mingw64/bin;$toolRoot/msys64/usr/bin;$previousPath"
    & python "$repoRoot/tools/test-switch-clear-trace.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch bounded clear trace regression failed' }
    & python "$repoRoot/tools/test-switch-depth-clear.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch guarded depth clear regression failed' }
    & python "$repoRoot/tools/test-switch-stencil-zero.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch witnessed zero-stencil reuse regression failed' }
    & python "$repoRoot/tools/test-switch-gpu-profile.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch CPU profile regression failed' }
    & python "$repoRoot/tools/test-switch-native-queue-profile.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch native queue observation regression failed' }
    & python "$repoRoot/tools/test-switch-native-submit-capacity.py"
    if ($LASTEXITCODE -ne 0) { throw 'Horizon native completion-tail capacity regression failed' }
    & python "$repoRoot/tools/test-switch-dump-pitch.py"
    if ($LASTEXITCODE -ne 0) { throw 'EDRAM dump source-pitch identity regression failed' }
    & python "$repoRoot/tools/test-switch-transfer-pitch.py"
    if ($LASTEXITCODE -ne 0) { throw 'Ownership transfer source-pitch identity regression failed' }
    & python "$repoRoot/tools/test-switch-transfer-same-origin.py"
    if ($LASTEXITCODE -ne 0) { throw 'Bounded same-origin stencil address regression failed' }
    & python "$repoRoot/tools/test-switch-stencil-ones.py"
    if ($LASTEXITCODE -ne 0) { throw 'Stencil split eligibility and scope regression failed' }
    & python "$repoRoot/tools/test-switch-transfer-profile.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch transfer accounting regression failed' }
    & python "$repoRoot/tools/test-switch-draw-profile.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch emitted draw accounting regression failed' }
    & python "$repoRoot/tools/test-switch-no-effect-points.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch guarded no-effect point regression failed' }
    & python "$repoRoot/tools/test-switch-fence-samples.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch sparse fence-operation recorder regression failed' }
    & python "$repoRoot/tools/test-switch-direct-color-rules.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch direct-color resolve rules regression failed' }
    & python "$repoRoot/tools/test-switch-stencil-copy-rules.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch guarded stencil-copy rules regression failed' }
    & python "$repoRoot/tools/test-switch-fsi-specialization.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch FSI persistent shader-key regression failed' }
    & python "$repoRoot/tools/test-switch-gpu-timestamps.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch GPU timestamp lifecycle regression failed' }
    & python "$repoRoot/tools/test-switch-memexport-format.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch memexport format specialization regression failed' }
    & python "$repoRoot/tools/test-switch-dynamic-constants.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch constants binding regression failed; keep the rejected dynamic-constants experiment unapplied' }
    & python "$repoRoot/tools/test-switch-resolution.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch title resolution regression failed' }
    & python "$repoRoot/tools/test-switch-memory-commit.py"
    if ($LASTEXITCODE -ne 0) { throw 'Horizon memory regression failed; apply port/rexglue-horizon-batched-memory-commit.patch' }
    & python "$repoRoot/tools/test-switch-xma-chain.py"
    if ($LASTEXITCODE -ne 0) { throw 'Horizon XMA source regression check failed; apply the pinned patch' }
    $manifest = "$repoRoot/work/analysis/switch-game-guest-objects.json"
    if ($RecompileGuest -or -not (Test-Path -LiteralPath $manifest)) { $manifest = '' }
    $timestampMode = if ($GpuTimestamps) { 'ON' } else { 'OFF' }
    $stencilOnesMode = if ($StencilOnesSplit) { 'ON' } else { 'OFF' }
    & $cmake -S "$repoRoot/switch" -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release `
        "-DCMAKE_MAKE_PROGRAM=$toolRoot/python/bin/ninja.exe" `
        "-DCMAKE_TOOLCHAIN_FILE=$repoRoot/third_party/nfsmw-nx/tools/switch/cmake/switch-devkitA64.cmake" `
        "-DDEVKITPRO=$env:DEVKITPRO" "-DREXGLUE_SWITCH_NVK_SDK=$MesaSdk" `
        "-DLB2_ENABLE_GPU_TIMESTAMPS=$timestampMode" `
        "-DLB2_ENABLE_STENCIL_ONES_SPLIT=$stencilOnesMode" `
        "-DLB2_ENABLE_FSI_SPECIALIZATION=OFF" `
        "-DLB2_PREBUILT_GUEST_MANIFEST=$manifest" `
        "-DPYTHON_EXECUTABLE=$toolRoot/msys64/mingw64/bin/python3.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Switch configuration failed' }
    & $cmake --build $build --target legobatman2 -j $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Switch game build failed' }
    & python "$repoRoot/tools/package-switch-game.py"
    if ($LASTEXITCODE -ne 0) { throw 'Switch game packaging failed' }
} finally {
    Pop-Location
    $env:PATH = $previousPath
    $env:DEVKITPRO = $previousDevkit
}
