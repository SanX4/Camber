$CamberProjectRoot = Split-Path $PSScriptRoot -Parent
[xml]$CamberVersionDefinition = Get-Content -LiteralPath (Join-Path $CamberProjectRoot 'Directory.Build.props') -Raw -Encoding UTF8
$CamberVersion = [string]$CamberVersionDefinition.Project.PropertyGroup.Version
$CamberAndroidCode = [int]$CamberVersionDefinition.Project.PropertyGroup.AndroidVersionCode
if ($CamberVersion -notmatch '^\d+\.\d+\.\d+[-.a-zA-Z0-9]*$') { throw 'Invalid build version.' }
$CamberOutput = Join-Path $CamberProjectRoot "artifacts/$CamberVersion"
