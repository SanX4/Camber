[CmdletBinding()]
param([string]$CompilerRoot = (Join-Path $PSScriptRoot '../.tools/compiler/msvc'))
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$CompilerRoot = (Resolve-Path $CompilerRoot).Path
$msvcVersion = (Get-ChildItem "$CompilerRoot/VC/Tools/MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$sdkVersion = (Get-ChildItem "$CompilerRoot/Windows Kits/10/Include" -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$vc = "$CompilerRoot/VC/Tools/MSVC/$msvcVersion"
$sdk = "$CompilerRoot/Windows Kits/10"
$env:PATH = "$vc/bin/Hostx64/x64;$sdk/bin/$sdkVersion/x64;" + $env:PATH
$env:INCLUDE = "$vc/include;$sdk/Include/$sdkVersion/ucrt;$sdk/Include/$sdkVersion/shared;$sdk/Include/$sdkVersion/um;$sdk/Include/$sdkVersion/winrt;$sdk/Include/$sdkVersion/cppwinrt"
$env:LIB = "$vc/lib/x64;$sdk/Lib/$sdkVersion/ucrt/x64;$sdk/Lib/$sdkVersion/um/x64"
$env:VSCMD_ARG_TGT_ARCH = 'x64'
Set-Location $projectRoot
cmake -S native -B native/build-msvc-local -G Ninja '-DCMAKE_CXX_COMPILER=cl.exe' '-DCMAKE_RC_COMPILER=rc.exe' '-DCMAKE_MT=mt.exe' '-DCMAKE_BUILD_TYPE=Release'
if ($LASTEXITCODE -ne 0) { throw 'Native configuration failed.' }
cmake --build native/build-msvc-local
if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
ctest --test-dir native/build-msvc-local --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Native checks failed.' }
