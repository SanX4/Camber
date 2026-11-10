[CmdletBinding()]
param([string]$Gradle='gradle',[string]$JavaHome=$env:JAVA_HOME,[string]$AndroidHome=$env:ANDROID_HOME,
    [ValidateSet('Debug','Internal')][string]$Configuration='Internal',[switch]$Bundle,[switch]$Rebuild)
$ErrorActionPreference='Stop'
. "$PSScriptRoot/build-context.ps1"
if($JavaHome){$env:JAVA_HOME=$JavaHome};if($AndroidHome){$env:ANDROID_HOME=$AndroidHome}
if(!$env:JAVA_HOME -or !$env:ANDROID_HOME){throw 'Set JAVA_HOME and ANDROID_HOME.'}
$dependencies=Get-Content "$PSScriptRoot/dependencies.json" -Raw -Encoding UTF8 | ConvertFrom-Json
$model=Join-Path $CamberProjectRoot 'mobile/android/app/src/main/assets/gesture_recognizer.task'
if((Get-FileHash -LiteralPath $model -Algorithm SHA256).Hash -ne $dependencies.gestureModel.sha256){throw 'Gesture model checksum mismatch.'}
$buildTools=Join-Path $env:ANDROID_HOME 'build-tools/36.0.0'
$variant=$Configuration.ToLowerInvariant()
$output=Join-Path $CamberOutput 'android'
$reports=Join-Path $CamberOutput 'reports'
New-Item -ItemType Directory -Force $output,$reports | Out-Null
Push-Location (Join-Path $CamberProjectRoot 'mobile/android')
try {
    $arguments=@(':app:testDebugUnitTest',":app:test${Configuration}UnitTest",":app:assemble$Configuration",'--console=plain','--dependency-verification','strict')
    if($Rebuild){$arguments+='--rerun-tasks'}
    & $Gradle @arguments
    if($LASTEXITCODE -ne 0){throw 'Android build or tests failed.'}
    $apkRoot=Join-Path $PWD "app/build/outputs/apk/$variant"
    $metadata=Get-Content (Join-Path $apkRoot 'output-metadata.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach($element in $metadata.elements){
        $abi=($element.filters | Where-Object filterType -eq 'ABI').value
        if(!$abi){$abi='universal'}
        $apk=Join-Path $output "Camber-$CamberVersion-$abi-$variant.apk"
        Copy-Item -LiteralPath (Join-Path $apkRoot $element.outputFile) -Destination $apk -Force
        & "$buildTools/apksigner.bat" verify --verbose --print-certs $apk | Set-Content -LiteralPath (Join-Path $reports "android-$abi-signature.txt") -Encoding UTF8
        if($LASTEXITCODE -ne 0){throw "APK signature failed: $abi"}
        & "$buildTools/zipalign.exe" -c -P 16 -v 4 $apk | Set-Content -LiteralPath (Join-Path $reports "android-$abi-zip-alignment.txt") -Encoding UTF8
        if($LASTEXITCODE -ne 0){throw "APK ZIP alignment failed: $abi"}
        if($abi -in @('arm64-v8a','x86_64','universal')){
            python "$PSScriptRoot/beta-readiness.py" --apk $apk --check-apk-only | Set-Content -LiteralPath (Join-Path $reports "android-$abi-16k.json") -Encoding UTF8
            if($LASTEXITCODE -ne 0){throw "APK ELF alignment failed: $abi"}
        }
    }
    if($Bundle){
        & $Gradle :app:bundleRelease -PcamberBundle=true --console=plain --dependency-verification strict
        if($LASTEXITCODE -ne 0){throw 'Android app bundle build failed.'}
        Copy-Item -LiteralPath 'app/build-bundle/outputs/bundle/release/app-release.aab' -Destination (Join-Path $output "Camber-$CamberVersion-store-candidate.aab") -Force
    }
    foreach($mapping in @("app/build/outputs/mapping/$variant/mapping.txt",'app/build-bundle/outputs/mapping/release/mapping.txt')){
        if(Test-Path -LiteralPath $mapping){
            $label=if($mapping -like '*build-bundle*'){'store'}else{$variant}
            Copy-Item -LiteralPath $mapping -Destination (Join-Path $reports "android-$label-mapping.txt") -Force
        }
    }
}finally{Pop-Location}
python "$PSScriptRoot/package-manifest.py"
if($LASTEXITCODE -ne 0){throw 'Package indexing failed.'}
Write-Host "Android development packages: $output"
Write-Host 'Internal APKs are minified and non-debuggable, signed with the existing development key. No installation or upload was performed.'
