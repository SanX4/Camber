[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$manifest = Get-Content (Join-Path $PSScriptRoot 'dependencies.json') -Raw | ConvertFrom-Json
$deps = Join-Path $projectRoot '.deps'
New-Item -ItemType Directory -Force $deps | Out-Null
$archive = Join-Path $deps 'libwebrtc.zip'
if (!(Test-Path $archive) -or (Get-FileHash $archive -Algorithm SHA256).Hash -ne $manifest.libwebrtcWindows.sha256) {
    & curl.exe --fail --location --retry 2 --max-time 180 $manifest.libwebrtcWindows.url --output $archive
    if ($LASTEXITCODE -ne 0) { throw 'Could not download the pinned WebRTC package.' }
}
if ((Get-FileHash $archive -Algorithm SHA256).Hash -ne $manifest.libwebrtcWindows.sha256) { throw 'WebRTC package checksum mismatch.' }
Expand-Archive -LiteralPath $archive -DestinationPath (Join-Path $deps 'libwebrtc') -Force
$redist = Join-Path $deps 'vc_redist.x64.exe'
if (!(Test-Path $redist)) {
    & curl.exe --fail --location --retry 2 --max-time 180 https://aka.ms/vs/17/release/vc_redist.x64.exe --output $redist
    if ($LASTEXITCODE -ne 0) { throw 'Could not download the Microsoft C++ runtime.' }
}
$signature = Get-AuthenticodeSignature -LiteralPath $redist
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') { throw 'C++ runtime signature verification failed.' }
Write-Host 'Verified WebRTC dependency. Native builds require Visual Studio 2022 C++ tools and Windows 11 SDK.'
