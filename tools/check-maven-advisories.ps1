[CmdletBinding()]
param([string]$Output=(Join-Path $PSScriptRoot '../artifacts/maven-advisories.json'))
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$packages=@()
Get-Content (Join-Path $root 'mobile/android/app/gradle.lockfile')|ForEach-Object {
    $line=$_ -split '=',2;$coordinate=$line[0] -split ':'
    if((-not $_.StartsWith('#')) -and $coordinate.Count -eq 3) {
        $configurations=if($line.Count -eq 2){$line[1] -split ','}else{@()}
        $packages+=@{name="$($coordinate[0]):$($coordinate[1])";version=$coordinate[2];appRuntime=@($configurations|Where-Object {$_ -match '^(debug|internal|release)RuntimeClasspath$'}).Count -gt 0}
    }
}
$queries=@($packages|ForEach-Object {@{package=@{ecosystem='Maven';name=$_.name};version=$_.version}})
$path=[IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force (Split-Path $path -Parent)|Out-Null
$queryCompleted=$false
try {
    $body=@{queries=$queries}|ConvertTo-Json -Depth 6 -Compress
    for($attempt=1;$attempt -le 3;$attempt++) {
        try {$response=Invoke-RestMethod -Uri 'https://api.osv.dev/v1/querybatch' -Method Post -ContentType 'application/json' -Body $body -HttpVersion 1.1 -TimeoutSec 10;break}
        catch {if($attempt -eq 3){throw};Start-Sleep -Seconds 1}
    }
    if($response.results.Count -ne $packages.Count){throw 'Advisory service returned incomplete results.'}
    $findings=@()
    for($i=0;$i -lt $packages.Count;$i++) {
        if($response.results[$i].vulns) {$findings+=@{package=$packages[$i].name;version=$packages[$i].version;appRuntime=$packages[$i].appRuntime;vulnerabilities=@($response.results[$i].vulns.id)}}
    }
    $runtime=@($findings|Where-Object appRuntime)
    $report=@{status='query-completed';source='https://api.osv.dev/v1/querybatch';packageVersions=$packages.Count;appRuntimeMatches=$runtime.Count;toolingMatches=@($findings|Where-Object {!$_.appRuntime}).Count;findings=$findings;limits='Version matches require reachability review. Native libraries and full license/provenance review are separate. No match is not a security guarantee.'}
    $report|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $path -Encoding UTF8
    $queryCompleted=$true
    Write-Host "Queried $($packages.Count) locked Maven versions: $($runtime.Count) app-runtime matches; $($report.toolingMatches) build/test-tool matches."
    if($runtime.Count){throw 'App-runtime advisory matches require remediation or documented reachability review.'}
}catch {
    if(!$queryCompleted){@{status='blocked';source='https://api.osv.dev/v1/querybatch';error=$_.Exception.Message}|ConvertTo-Json|Set-Content -LiteralPath $path -Encoding UTF8}
    throw
}
