# Compatibility experiment only. This does not build or boot the full game.
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path "$PSScriptRoot/..").Path.Replace('\', '/')
$armRoot = "$repoRoot/work/toolchain/switch-root/opt/devkitpro"
$sdk = "$repoRoot/third_party/nfsmw-nx/sdk"
$deps = "$repoRoot/work/toolchain/rexglue-0.10.0/win-amd64/include"
$build = "$repoRoot/build/switch-guest-probe"
$stage = "$repoRoot/work/analysis/switch-compat/stage"
$compiler = "$armRoot/devkitA64/bin/aarch64-none-elf-g++.exe"
Push-Location $repoRoot
try {
    New-Item -ItemType Directory -Force $build | Out-Null
    & python tools/stage-switch-guest-probe.py
    if ($LASTEXITCODE -ne 0) { throw 'Guest staging failed' }
    $flags = @('-std=c++23', '-O2', '-march=armv8-a+crc+crypto', '-mtune=cortex-a57',
        '-mtp=soft', '-fPIE', '-ftls-model=local-exec', '-ffunction-sections', '-fdata-sections',
        '-ffp-contract=off', '-fno-strict-aliasing', '-D__SWITCH__', '-DNX', '-D_GNU_SOURCE',
        '-DSPDLOG_NO_TZ_OFFSET', '-DSPDLOG_FMT_EXTERNAL', '-Werror=attributes',
        "-I$sdk/switch_compat", "-I$sdk/include", "-I$deps", "-I$armRoot/libnx/include", "-I$stage",
        '-include', "$repoRoot/src/platform/switch/guest_compiler_compat.h")
    & $compiler @flags -c "$stage/guest.cpp" -o "$build/guest.o"
    if ($LASTEXITCODE -ne 0) { throw 'Guest ARM64 compile failed' }
    & $compiler @flags -c tests/switch_guest_probe/main.cpp -o "$build/main.o"
    if ($LASTEXITCODE -ne 0) { throw 'Probe ARM64 compile failed' }
    $specs = (Get-Content "$armRoot/libnx/switch.specs" -Raw).Replace(
        '%:getenv(DEVKITPRO /libnx/switch.ld)', "$armRoot/libnx/switch.ld")
    [IO.File]::WriteAllText("$build/switch.specs", $specs)
    & $compiler -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE `
        -ffp-contract=off -fno-strict-aliasing "-specs=$build/switch.specs" '-Wl,--gc-sections' `
        "$build/guest.o" "$build/main.o" "-L$armRoot/libnx/lib" -lnx -o "$build/lb2-guest-probe.elf"
    if ($LASTEXITCODE -ne 0) { throw 'Probe link failed' }
    & "$armRoot/tools/bin/nacptool.exe" --create 'LB2 ARM64 guest probe' 'Aspirata' '0.1' "$build/probe.nacp"
    if ($LASTEXITCODE -ne 0) { throw 'NACP creation failed' }
    & "$armRoot/tools/bin/elf2nro.exe" "$build/lb2-guest-probe.elf" "$build/lb2-guest-probe.nro" `
        "--nacp=$build/probe.nacp" "--icon=$armRoot/libnx/default_icon.jpg"
    if ($LASTEXITCODE -ne 0) { throw 'NRO packaging failed' }
    Get-FileHash "$build/lb2-guest-probe.nro"
} finally { Pop-Location }
