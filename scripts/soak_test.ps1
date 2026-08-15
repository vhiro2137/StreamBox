param(
    [Parameter(Mandatory = $true)]
    [string]$Media,
    [double]$DurationMinutes = 120,
    [int]$SampleIntervalSeconds = 10,
    [int]$MaximumGrowthMB = 256
)

$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $workspace 'dist\StreamBox\StreamBox.exe'
$log = Join-Path $workspace 'dist\soak-test.csv'

if (-not (Test-Path -LiteralPath $executable)) {
    throw "Deploy StreamBox before running the soak test: $executable"
}
if (-not (Test-Path -LiteralPath $Media)) {
    throw "Media file not found: $Media"
}

$durationMs = [Math]::Max(1000, [int64]($DurationMinutes * 60 * 1000))
$process = Start-Process -FilePath $executable `
    -ArgumentList @('--open', $Media, '--quit-after', $durationMs) `
    -WindowStyle Hidden -PassThru

$samples = [System.Collections.Generic.List[object]]::new()
try {
    while (-not $process.HasExited) {
        $process.Refresh()
        $samples.Add([pscustomobject]@{
            Timestamp = (Get-Date).ToString('o')
            WorkingSetMB = [Math]::Round($process.WorkingSet64 / 1MB, 2)
            Threads = $process.Threads.Count
            Handles = $process.HandleCount
        })
        $samples | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $log
        Start-Sleep -Seconds $SampleIntervalSeconds
    }
} finally {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id }
    $samples | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $log
}

if ($process.ExitCode -ne 0) { throw "StreamBox exited with code $($process.ExitCode)" }
if ($samples.Count -gt 1) {
    $growth = $samples[$samples.Count - 1].WorkingSetMB - $samples[0].WorkingSetMB
    if ($growth -gt $MaximumGrowthMB) {
        throw "Working set grew by $growth MB, exceeding $MaximumGrowthMB MB"
    }
}

Write-Host "Soak test passed. Samples: $($samples.Count). Log: $log"
