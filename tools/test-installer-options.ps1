[CmdletBinding()]
param([string]$ReportDirectory=(Join-Path $PSScriptRoot '../artifacts/installer-options-tests'))
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$reportPath=[IO.Path]::GetFullPath($ReportDirectory)
$artifactPath=[IO.Path]::GetFullPath((Join-Path $projectRoot 'artifacts'))+[IO.Path]::DirectorySeparatorChar
if(!$reportPath.StartsWith($artifactPath,[StringComparison]::OrdinalIgnoreCase)){throw 'Test output must stay inside project artifacts.'}
New-Item -ItemType Directory -Force $reportPath | Out-Null
$compiler=Join-Path $projectRoot '.tools/InnoSetup/ISCC.exe'
& $compiler '/Q' "/O$reportPath" (Join-Path $projectRoot 'installer/tests/options-fixture.iss')
if($LASTEXITCODE -ne 0){throw 'Fixture compilation failed.'}
$fixture=Join-Path $reportPath 'Camber-Installer-Options-Fixture.exe'
$results=[Collections.Generic.List[string]]::new()
$shell=New-Object -ComObject WScript.Shell
function RunFixture([string]$target,[string]$tasks,[string]$label) {
    $arguments=@('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/DIR=`"$target`"","/TASKS=`"$tasks`"","/LOG=`"$(Join-Path $reportPath "$label.log")`"")
    $process=Start-Process -FilePath $fixture -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(30000)){Stop-Process -Id $process.Id;throw 'Fixture timed out.'}
    if($process.ExitCode -ne 0){throw "Fixture failed: $label ($($process.ExitCode))"}
    $process.Dispose()
    foreach($task in @('desktopicon','startup')) {
        $folder=if($task -eq 'startup'){'test-startup'}else{'test-desktop'}
        $link=Join-Path $target "$folder/Camber Camera.lnk"
        $selected=$task -in $tasks.Split(',')
        if((Test-Path -LiteralPath $link) -ne $selected){throw "Incorrect $task selection: $label"}
        if($selected) {
            $shortcut=$shell.CreateShortcut($link)
            if($shortcut.TargetPath -ne (Join-Path $target 'Camber.Desktop.exe')){throw 'Shortcut target lost the custom installation folder.'}
            if($shortcut.WorkingDirectory -ne $target){throw 'Shortcut working directory is incorrect.'}
            $expected=if($task -eq 'startup'){'--startup'}else{''}
            if($shortcut.Arguments -ne $expected){throw 'Shortcut has incorrect startup arguments.'}
        }
    }
    $results.Add("PASS $label - selected links, custom folder, target, working directory and startup arguments")
}
function UninstallFixture([string]$target,[string]$label) {
    $uninstaller=Join-Path $target 'unins000.exe'
    $process=Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LOG=`"$(Join-Path $reportPath "$label-uninstall.log")`"") -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(30000)){Stop-Process -Id $process.Id;throw 'Fixture uninstall timed out.'}
    if($process.ExitCode -ne 0){throw 'Fixture uninstall failed.'}
    $process.Dispose()
    foreach($relative in @('Camber.Desktop.exe','test-desktop/Camber Camera.lnk','test-startup/Camber Camera.lnk')) {
        if(Test-Path -LiteralPath (Join-Path $target $relative)){throw "Fixture left a managed file: $relative"}
    }
    $results.Add("PASS $label - uninstall removes only fixture executable and selected links")
}
try {
    foreach($case in @(@('none',''),@('desktop','desktopicon'),@('startup','startup'),@('both','desktopicon,startup'))) {
        $target=Join-Path $reportPath "Custom location with spaces/$($case[0])"
        RunFixture $target $case[1] "fresh-$($case[0])"
        UninstallFixture $target $case[0]
    }
    $target=Join-Path $reportPath 'Custom location with spaces/upgrade'
    RunFixture $target 'desktopicon,startup' 'upgrade-before'
    RunFixture $target 'desktopicon' 'upgrade-disable-startup'
    RunFixture $target '' 'upgrade-disable-both'
    UninstallFixture $target 'upgrade'
    $results | Set-Content -LiteralPath (Join-Path $reportPath 'results.txt') -Encoding UTF8
    $results | ForEach-Object {Write-Host $_}
    Write-Host 'Fixture only: actual elevation, common shell folders, camera registration and firewall lifecycle remain separate tests.'
} finally {[Runtime.InteropServices.Marshal]::ReleaseComObject($shell) | Out-Null}
