[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PublishDirectory,
    [switch]$Launch,
    [switch]$ImplementationOnly,
    [ValidateRange(1, 30)][int]$StartupTimeoutSeconds = 8
)
$ErrorActionPreference = 'Stop'
$publishPath = (Resolve-Path -LiteralPath $PublishDirectory).Path
$rootPath=$publishPath
if(Test-Path -LiteralPath (Join-Path $publishPath 'app')){$publishPath=Join-Path $publishPath 'app'}
foreach ($name in @('Camber.Desktop.exe', 'Camber.Desktop.dll', 'Camber.Desktop.pri', 'Camber.Desktop.runtimeconfig.json', 'Microsoft.ui.xaml.dll')) {
    $file = Join-Path $publishPath $name
    if (!(Test-Path -LiteralPath $file -PathType Leaf) -or (Get-Item -LiteralPath $file).Length -eq 0) {
        throw "The Windows package is missing a required startup file: $name"
    }
}
Write-Host 'PASS: required Windows startup files are packaged.'
if(!$ImplementationOnly){
    foreach($name in @('Camber.Media.exe','Camber.Camera.dll','Camber.UpgradeCheck.exe','libwebrtc.dll')){
        if(!(Test-Path -LiteralPath (Join-Path $publishPath $name))){throw "Missing native application file: $name"}
    }
    $launcher=Join-Path $rootPath 'Camber.Desktop.exe'
    if(!(Test-Path -LiteralPath $launcher)){throw 'The root launcher is missing.'}
    & $launcher --verify-layout
    if($LASTEXITCODE -ne 0){throw 'The launcher cannot locate its application.'}
    Write-Host 'PASS: root launcher and native package layout.'
}
if ($Launch) {
    $process = Start-Process -FilePath (Join-Path $publishPath 'Camber.Desktop.exe') -WorkingDirectory $publishPath -WindowStyle Hidden -PassThru
    try {
        if ($process.WaitForExit($StartupTimeoutSeconds * 1000)) {
            throw "Published application exited during startup (code $($process.ExitCode))."
        }
        $process.Refresh()
        if ($process.MainWindowTitle -ne 'Camber Camera') { throw 'Published application did not create the Camber Camera window.' }
        Write-Host 'PASS: published application opens its native window without a startup crash.'
    }
    finally {
        # This is the precise process created above, never another user's app instance.
        if (!$process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue }
        $process.Dispose()
    }
}
