param(
    [Parameter(Mandatory = $true)]
    [string[]]$Media
)

$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$probe = Join-Path $workspace 'build\media_smoke.exe'
$qtBin = 'D:\Qt\6.11.0\mingw_64\bin'
$ffmpegBin = Join-Path $workspace 'third_party\ffmpeg-sdk\bin'
$env:Path = "$qtBin;$ffmpegBin;$env:Path"

if (-not (Test-Path -LiteralPath $probe)) { throw "Media probe not built: $probe" }
$results = foreach ($item in $Media) {
    & $probe $item
    [pscustomobject]@{ Media = $item; ExitCode = $LASTEXITCODE; Passed = $LASTEXITCODE -eq 0 }
}
$results | Format-Table -AutoSize
if ($results.Passed -contains $false) { throw 'One or more media probes failed' }
