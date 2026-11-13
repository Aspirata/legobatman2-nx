$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path "$PSScriptRoot/..").Path.Replace('\', '/')
$env:DEVKITPRO = "$repoRoot/work/toolchain/switch-root/opt/devkitpro"
$env:DEVKITA64 = "$env:DEVKITPRO/devkitA64"
$build = "$repoRoot/build/switch-probe"
New-Item -ItemType Directory -Force $build | Out-Null
$specs = (Get-Content "$env:DEVKITPRO/libnx/switch.specs" -Raw).Replace('%:getenv(DEVKITPRO /libnx/switch.ld)', "$env:DEVKITPRO/libnx/switch.ld")
[IO.File]::WriteAllText("$build/switch.specs", $specs)
& "$env:DEVKITA64/bin/aarch64-none-elf-g++.exe" `
    -std=c++17 -O0 -g -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE `
    -ffunction-sections -fdata-sections -ffp-contract=off -fno-strict-aliasing `
    "-specs=$build/switch.specs" "-I$env:DEVKITPRO/libnx/include" `
    "$repoRoot/tests/switch_probe/main.cpp" "-L$env:DEVKITPRO/libnx/lib" `
    -ldeko3d -lnx -o "$build/lb2-toolchain-probe.elf"
if ($LASTEXITCODE -ne 0) { throw 'Switch probe compile/link failed' }
& "$env:DEVKITPRO/tools/bin/uam.exe" -s vert -o "$build/probe.dksh" "$repoRoot/tests/switch_probe/probe.vert"
if ($LASTEXITCODE -ne 0) { throw 'uam shader compile failed' }
& "$env:DEVKITPRO/tools/bin/nacptool.exe" --create 'LB2 toolchain probe' 'Aspirata' '0.1' "$build/probe.nacp"
if ($LASTEXITCODE -ne 0) { throw 'NACP creation failed' }
& "$env:DEVKITPRO/tools/bin/elf2nro.exe" "$build/lb2-toolchain-probe.elf" "$build/lb2-toolchain-probe.nro" "--nacp=$build/probe.nacp" "--icon=$env:DEVKITPRO/libnx/default_icon.jpg"
if ($LASTEXITCODE -ne 0) { throw 'NRO packaging failed' }
Get-FileHash "$build/lb2-toolchain-probe.nro" -Algorithm SHA256
