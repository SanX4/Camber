[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$locator=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if(!(Test-Path -LiteralPath $locator)){throw 'Install Visual Studio C++ tools and the Windows SDK.'}
$installation=& $locator -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$installation){throw 'No supported Visual Studio C++ installation was found.'}
$developer=Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
$environment=& $env:ComSpec /d /s /c ('""'+$developer+'" -no_logo -arch=x64 -host_arch=x64 && set"')
if($LASTEXITCODE -ne 0){throw 'Visual Studio environment initialization failed.'}
foreach($line in $environment) {
    if($line -match '^(PATH|INCLUDE|LIB|LIBPATH|WindowsSdkDir|WindowsSDKVersion|VCToolsInstallDir|VSCMD_ARG_TGT_ARCH)=(.*)$') {
        [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')
    }
}
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
