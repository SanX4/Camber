[CmdletBinding()]
param([switch]$Native,[switch]$LocalCompiler,[switch]$Installer,[switch]$ReleasePackage)
$ErrorActionPreference='Stop'
. "$PSScriptRoot/build-context.ps1"
Set-Location $CamberProjectRoot
$dotnet=Join-Path $CamberProjectRoot '.tools/dotnet/dotnet.exe'
if(!(Test-Path -LiteralPath $dotnet)){$dotnet=(Get-Command dotnet -ErrorAction Stop).Source}
$env:DOTNET_CLI_TELEMETRY_OPTOUT='1'
$env:DOTNET_SKIP_FIRST_TIME_EXPERIENCE='1'
function RunChecked([scriptblock]$Action){& $Action;if($LASTEXITCODE -ne 0){throw 'Build or verification failed.'}}
$package=Join-Path $CamberOutput 'windows'
$implementation=Join-Path $package 'app'
New-Item -ItemType Directory -Force $implementation,(Join-Path $CamberOutput 'reports') | Out-Null
RunChecked {& $dotnet run --project tests/Camber.Core.Tests -c Release}
RunChecked {& $dotnet publish src/Camber.Host -c Release -r win-x64 --self-contained true -o $implementation -v minimal}
RunChecked {& $dotnet publish src/Camber.Desktop -c Release -r win-x64 --self-contained true -p:Platform=x64 -o $implementation -v minimal}
if($Native){
    if($LocalCompiler){& "$PSScriptRoot/build-native-local.ps1";$nativeOutput=Join-Path $CamberProjectRoot 'native/build-msvc-local'}
    else {
        & "$PSScriptRoot/bootstrap.ps1"
        RunChecked {cmake -S native -B native/build-msvc -G 'Visual Studio 17 2022' -A x64}
        RunChecked {cmake --build native/build-msvc --config Release}
        RunChecked {ctest --test-dir native/build-msvc -C Release --output-on-failure}
        $nativeOutput=Join-Path $CamberProjectRoot 'native/build-msvc/Release'
    }
    foreach($name in @('Camber.Media.exe','Camber.Camera.dll','libwebrtc.dll','Camber.UpgradeCheck.exe')){
        Copy-Item -LiteralPath (Join-Path $nativeOutput $name) -Destination $implementation -Force
    }
    Copy-Item -LiteralPath (Join-Path $nativeOutput 'Camber.Launcher.exe') -Destination (Join-Path $package 'Camber.Desktop.exe') -Force
}
New-Item -ItemType Directory -Force (Join-Path $package 'licenses') | Out-Null
Copy-Item third_party/licenses/* (Join-Path $package 'licenses') -Recurse -Force
New-Item -ItemType Directory -Force (Join-Path $package 'docs') | Out-Null
Copy-Item docs/* (Join-Path $package 'docs') -Recurse -Force
Copy-Item README.md,docs/PRIVACY.md -Destination $package -Force
$userGuide=(Get-Content docs/USER-GUIDE.md -Raw -Encoding UTF8).Replace('](CONNECTING.md)','](docs/CONNECTING.md)')
Set-Content -LiteralPath (Join-Path $package 'USER-GUIDE.md') -Value $userGuide -Encoding UTF8 -NoNewline
Copy-Item third_party/NOTICE.md -Destination (Join-Path $package 'licenses/NOTICE.md') -Force
& "$PSScriptRoot/test-windows-package.ps1" -PublishDirectory $package -ImplementationOnly:(!$Native)
if($ReleasePackage){
    if(!$Native){throw 'A release package requires the native camera targets.'}
    if(!$env:CAMBER_SIGNING_CERT_SHA1){throw 'Configure the Windows signing certificate. Internal builds remain unsigned.'}
    foreach($binary in Get-ChildItem -LiteralPath $package -File -Recurse | Where-Object {$_.Name -like 'Camber.*.exe' -or $_.Name -like 'Camber.*.dll'}){
        RunChecked {signtool sign /sha1 $env:CAMBER_SIGNING_CERT_SHA1 /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 $binary.FullName}
        RunChecked {signtool verify /pa /all $binary.FullName}
    }
}
if($Installer -or $ReleasePackage){
    if(!$Native){throw 'Setup requires the complete native package.'}
    $iscc=Join-Path $CamberProjectRoot '.tools/InnoSetup/ISCC.exe'
    if(!(Test-Path -LiteralPath $iscc)){$iscc=(Get-Command ISCC -ErrorAction Stop).Source}
    New-Item -ItemType Directory -Force (Join-Path $CamberOutput 'windows-installer') | Out-Null
    RunChecked {& $iscc "/DAppVersion=$CamberVersion" "/DPublishDirectory=$package" "/O$(Join-Path $CamberOutput 'windows-installer')" installer/Camber.iss}
    $setup=Join-Path $CamberOutput "windows-installer/Camber-Setup-$CamberVersion.exe"
    if($ReleasePackage){
        RunChecked {signtool sign /sha1 $env:CAMBER_SIGNING_CERT_SHA1 /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 $setup}
        RunChecked {signtool verify /pa /all $setup}
        RunChecked {python "$PSScriptRoot/beta-readiness.py" --evidence "$CamberProjectRoot/artifacts/beta/evidence.json"}
    }
}
python "$PSScriptRoot/package-manifest.py"
if($LASTEXITCODE -ne 0){throw 'Package indexing failed.'}
Write-Host "Windows development package: $package"
Write-Host 'No installation or deployment was performed. Security and physical-device release gates still apply.'
