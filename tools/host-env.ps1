# Dot-source in a build shell. All variables affect only that process.
param([string]$ClangRoot = 'C:/Program Files/Windhawk/Compiler')
$repoRoot = Split-Path $PSScriptRoot -Parent
$toolRoot = Join-Path $repoRoot 'work/toolchain'
$msvcRoot = Join-Path $toolRoot 'msvc/Contents/VC/Tools/MSVC/14.44.35207'
$sdkRoot = Join-Path $toolRoot 'winsdk/c'
$env:INCLUDE = "$msvcRoot/include;$sdkRoot/Include/10.0.26100.0/ucrt;$sdkRoot/Include/10.0.26100.0/shared;$sdkRoot/Include/10.0.26100.0/um;$sdkRoot/Include/10.0.26100.0/winrt".Replace('/', '\')
$env:LIB = "$msvcRoot/lib/x64;$sdkRoot/ucrt/x64;$sdkRoot/um/x64".Replace('/', '\')
$env:PATH = "$toolRoot/bin;$msvcRoot/bin/Hostx64/x64;$sdkRoot/bin/10.0.26100.0/x64;$ClangRoot/bin;$env:PATH".Replace('/', '\')
