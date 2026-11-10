[CmdletBinding()]
param([string]$Gradle='gradle')
$ErrorActionPreference='Stop'
foreach($name in @('CAMBER_ANDROID_KEYSTORE','CAMBER_ANDROID_STORE_PASSWORD','CAMBER_ANDROID_KEY_ALIAS','CAMBER_ANDROID_KEY_PASSWORD')) {
    if(![Environment]::GetEnvironmentVariable($name)){throw "Missing release signing setting: $name. Debug signing is never used for beta."}
}
if(!$env:JAVA_HOME){throw 'Set JAVA_HOME to the supported JDK before building a signed bundle.'}
Push-Location (Join-Path $PSScriptRoot '../mobile/android')
try {
    & $Gradle :app:testDebugUnitTest :app:testReleaseUnitTest :app:bundleRelease -PcamberBundle=true --console=plain --dependency-verification strict
    if($LASTEXITCODE -ne 0){throw 'Signed Android release build failed.'}
    $verify = & "$env:JAVA_HOME/bin/jarsigner.exe" '-J-Duser.language=en' -verify app/build-bundle/outputs/bundle/release/app-release.aab 2>&1 | Out-String
    if($LASTEXITCODE -ne 0 -or $verify -notmatch 'jar verified\.') {throw 'Android bundle signature verification failed.'}
    Write-Host 'Signed bundle verified. Physical tests and owner approval are still required before upload.'
} finally {Pop-Location}
