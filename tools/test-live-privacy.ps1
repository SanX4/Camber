[CmdletBinding()]
param([Parameter(Mandatory)][string]$Device,[Parameter(Mandatory)][string]$ReportDirectory,
    [ValidateSet('pc-pause','phone-focus-loss','phone-process-termination')][string[]]$Cases=@('pc-pause','phone-focus-loss','phone-process-termination'))
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$adb=Join-Path $env:LOCALAPPDATA 'Android/Sdk/platform-tools/adb.exe'
$report=[IO.Path]::GetFullPath($ReportDirectory)
if(!$report.StartsWith([IO.Path]::GetFullPath((Join-Path $root 'artifacts'))+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Reports must stay in project artifacts.'}
New-Item -ItemType Directory -Force $report | Out-Null
function Adb([string[]]$Command){$result=& $adb -s $Device @Command;if($LASTEXITCODE -ne 0){throw 'Phone command failed.'};return ($result -join "`n")}
function RequireCamberFocus {
    $windows=Adb @('shell','dumpsys','window')
    if(([regex]::Match($windows,'(?m)^.*mCurrentFocus=.*$').Value) -notmatch 'camera.camber'){throw 'Camber is no longer foreground. No further phone input performed.'}
}
function TapCamber([string]$pattern) {
    RequireCamberFocus
    $dump=Adb @('shell','uiautomator','dump','/data/local/tmp/camber-privacy-check.xml')
    if($dump -match 'ERROR'){throw 'No fresh phone accessibility snapshot. Input stopped.'}
    [xml]$view=Adb @('shell','cat','/data/local/tmp/camber-privacy-check.xml')
    $nodes=@($view.SelectNodes('//node[@package="camera.camber"]')|Where-Object {$_.text -match $pattern})
    if($nodes.Count -ne 1){throw "Expected exactly one Camber control: $pattern"}
    $bounds=[regex]::Matches($nodes[0].bounds,'\d+')|ForEach-Object {[int]$_.Value}
    RequireCamberFocus
    $null=Adb @('shell','input','tap',([int](($bounds[0]+$bounds[2])/2)).ToString(),([int](($bounds[1]+$bounds[3])/2)).ToString())
}
function Control($message) {
    $sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $pipe=[IO.Pipes.NamedPipeClientStream]::new('.','Camber.Control.'+$sid,[IO.Pipes.PipeDirection]::InOut,[IO.Pipes.PipeOptions]::CurrentUserOnly)
    try {
        $pipe.Connect(2000);$writer=[IO.StreamWriter]::new($pipe,[Text.UTF8Encoding]::new($false),4096,$true)
        $writer.AutoFlush=$true;$reader=[IO.StreamReader]::new($pipe,[Text.Encoding]::UTF8,$false,4096,$true)
        $writer.WriteLine(($message|ConvertTo-Json -Depth 10 -Compress))
        $read=$reader.ReadLineAsync();if(!$read.Wait(3000)){throw 'PC control timed out.'}
        return ($read.Result|ConvertFrom-Json)
    }finally{$pipe.Dispose()}
}
function CameraActive {
    $dump=Adb @('shell','dumpsys','media.camera')
    $section=($dump -split 'Active Camera Clients:',2)[1]
    if($null -eq $section){throw 'Camera activity cannot be verified on this Android version.'}
    return (($section -split 'Allowed user',2)[0] -match 'camera.camber')
}
$results=[Collections.Generic.List[object]]::new()
try {
    foreach($case in $Cases) {
        RequireCamberFocus
        $status=Control @{type='status'}
        if(!$status.state.approved -or $status.state.privacyBlocked) {
            # A closed signaling connection requires reconnect, then a separate
            # phone Start. Never emulate approval through the PC control pipe.
            try {TapCamber '^Reconnect to PC$'}catch {if($_.Exception.Message -notmatch 'Expected exactly one'){throw}}
            TapCamber '^(Start sharing|Resume sharing)$'
        }
        $timer=[Diagnostics.Stopwatch]::StartNew()
        do {Start-Sleep -Milliseconds 100;$status=Control @{type='status'}}while((!$status.state.approved -or $status.state.privacyBlocked) -and $timer.ElapsedMilliseconds -lt 5000)
        if(!$status.state.approved -or $status.state.privacyBlocked){throw 'Fresh phone sharing did not authorize the session.'}
        $output=Join-Path $report "$case-output.txt"
        $observer=Start-Process -FilePath (Join-Path $root 'native/build-msvc-local/Camber.InstalledCameraTest.exe') -ArgumentList '--expect-live','210' -WindowStyle Hidden -PassThru -RedirectStandardOutput $output -RedirectStandardError (Join-Path $report "$case-errors.txt")
        try {
            $timer.Restart()
            do {Start-Sleep -Milliseconds 100;$observed=[string](Get-Content -LiteralPath $output -Raw)}while(![regex]::IsMatch($observed,'(?m)^LIVE_READY tick=\d+') -and $timer.ElapsedMilliseconds -lt 5000 -and !$observer.HasExited)
            $liveReady=[regex]::Match($observed,'(?m)^LIVE_READY tick=(\d+)')
            if(!$liveReady.Success){throw 'The Windows consumer did not receive two distinct live frames before the privacy action.'}
            if(!(CameraActive)){throw 'The phone camera was not active before the privacy action.'}
            $phoneProcess=(Adb @('shell','pidof','camera.camber')).Trim()
            $gestureLog=Adb @('logcat','-d','--pid',$phoneProcess,'-s','CamberGestureState:I','CamberGestures:W','AndroidRuntime:E','*:S')
            $gestureObserved=$gestureLog -match 'CamberGestureState'
            $gestureFailure=$gestureLog -match 'Gesture processing unavailable|Gesture runtime unavailable|FATAL EXCEPTION'
            $eventTick=[Environment]::TickCount64
            switch($case) {
                'pc-pause' {$null=Control @{type='pause'}}
                'phone-focus-loss' {RequireCamberFocus;$null=Adb @('shell','input','keyevent','KEYCODE_HOME')}
                'phone-process-termination' {RequireCamberFocus;$null=Adb @('shell','am','force-stop','camera.camber')}
            }
            $timer.Restart()
            do {$active=CameraActive;if(!$active){break};Start-Sleep -Milliseconds 50}while($timer.ElapsedMilliseconds -lt 5000)
            $releaseMs=[Environment]::TickCount64-$eventTick
            $status=Control @{type='status'}
            if(!$observer.WaitForExit(12000)){throw 'Camera consumer did not finish.'}
            $observed=Get-Content -LiteralPath $output -Raw
            $neutralTicks=[regex]::Matches($observed,'OUTPUT tick=(\d+) live=0')|ForEach-Object {[long]$_.Groups[1].Value}|Where-Object {$_ -ge $eventTick}
            $neutralTick=$neutralTicks|Select-Object -First 1
            $liveAfterNeutral=$false
            if($neutralTick){$liveAfterNeutral=@([regex]::Matches($observed,'OUTPUT tick=(\d+) live=1')|Where-Object {[long]$_.Groups[1].Value -gt $neutralTick}).Count -gt 0}
            $results.Add(@{check=$case;liveReadyTick=[long]$liveReady.Groups[1].Value;eventTick=$eventTick;neutralMs=if($neutralTick){$neutralTick-$eventTick}else{$null};cameraReleaseObservedMs=$releaseMs;cameraReleased=!$active;privacyBlocked=$status.state.privacyBlocked;resumedWithoutConsent=$liveAfterNeutral;consumerExit=$observer.ExitCode;gestureInferenceObserved=$gestureObserved;gestureFailureLogged=$gestureFailure})
            if($observer.ExitCode -ne 0 -or !$neutralTick -or $active -or !$status.state.privacyBlocked -or $liveAfterNeutral){throw "Privacy test did not pass: $case"}
        }finally{if(!$observer.HasExited){Stop-Process -Id $observer.Id};$observer.Dispose()}
        if($case -ne 'pc-pause') {
            $null=Adb @('shell','am','start','--activity-reorder-to-front','-n','camera.camber/.MainActivity')
            if(CameraActive){throw 'Returning to Camber silently restarted capture.'}
            if($case -eq 'phone-focus-loss') {
                RequireCamberFocus
                $fresh=Adb @('shell','uiautomator','dump','/data/local/tmp/camber-privacy-check.xml')
                if($fresh -match 'ERROR'){throw 'Unable to verify the out-of-focus label.'}
                [xml]$view=Adb @('shell','cat','/data/local/tmp/camber-privacy-check.xml')
                $outOfFocus=@($view.SelectNodes('//node[@package="camera.camber"]')|Where-Object {$_.text -match 'Out of focus'}).Count -gt 0
                $results[$results.Count-1]['outOfFocusLabel']=$outOfFocus
                if(!$outOfFocus){throw 'The phone did not retain its out-of-focus explanation.'}
            }
        }
    }
}finally {
    try {$null=Control @{type='stop'}}catch {Write-Warning 'Unable to send final PC Stop.'}
    $results|ConvertTo-Json -Depth 7|Set-Content -LiteralPath (Join-Path $report 'results.json') -Encoding UTF8
    try {$null=Adb @('shell','rm','-f','/data/local/tmp/camber-privacy-check.xml')}catch {Write-Warning 'Phone test connection unavailable for temporary accessibility-file cleanup.'}
}
$results|ConvertTo-Json -Depth 7
