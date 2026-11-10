[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$locator=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if(!(Test-Path -LiteralPath $locator)){throw 'Install Visual Studio C++ tools and the Windows SDK.'}
$installation=& $locator -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$installation){throw 'No supported Visual Studio C++ installation was found.'}
$vc=Get-ChildItem (Join-Path $installation 'VC/Tools/MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName
$sdk=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10'
$sdkVersion=Get-ChildItem (Join-Path $sdk 'Include') -Directory | Where-Object {Test-Path (Join-Path $_.FullName 'um/mfvirtualcamera.h')} | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty Name
if(!$vc -or !$sdkVersion){throw 'MSVC and a Windows 11 SDK are required.'}
# Import only the compiler/SDK paths, matching the portable compiler workflow;
# avoid batch environment initialization differences across runner images.
$env:PATH="$vc/bin/Hostx64/x64;$sdk/bin/$sdkVersion/x64;"+$env:PATH
$env:INCLUDE="$vc/include;$sdk/Include/$sdkVersion/ucrt;$sdk/Include/$sdkVersion/shared;$sdk/Include/$sdkVersion/um;$sdk/Include/$sdkVersion/winrt;$sdk/Include/$sdkVersion/cppwinrt"
$env:LIB="$vc/lib/x64;$sdk/Lib/$sdkVersion/ucrt/x64;$sdk/Lib/$sdkVersion/um/x64"
$env:VSCMD_ARG_TGT_ARCH='x64'
Push-Location $projectRoot
try {
    # Ninja supports the installed MSVC toolchain without hard-coding a Visual
    # Studio generator year (hosted images may move from VS 2022 to VS 2026).
    cmake -S native -B native/build-installed -G Ninja '-DCMAKE_CXX_COMPILER=cl.exe' '-DCMAKE_BUILD_TYPE=Release'
    if($LASTEXITCODE -ne 0){throw 'Native configuration failed.'}
    cmake --build native/build-installed
    if($LASTEXITCODE -ne 0){throw 'Native build failed.'}
    ctest --test-dir native/build-installed --output-on-failure
    if($LASTEXITCODE -ne 0){throw 'Native tests failed.'}
}finally{Pop-Location}
