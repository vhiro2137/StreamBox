$ErrorActionPreference = 'Stop'

function Convert-ToMsysPath([string]$Path) {
    $normalized = $Path -replace '\\', '/'
    if ($normalized -match '^([A-Za-z]):(.*)$') {
        return '/' + $Matches[1].ToLowerInvariant() + $Matches[2]
    }
    return $normalized
}

$workspace = Split-Path -Parent $PSScriptRoot
$bash = Join-Path $workspace '.tooling\msys64\usr\bin\bash.exe'
$script = Convert-ToMsysPath (Join-Path $PSScriptRoot 'build_ffmpeg_mingw.sh')

if (-not (Test-Path -LiteralPath $bash)) {
    throw "MSYS2 bash not found: $bash"
}

$ffmpegSource = if ($env:STREAMBOX_FFMPEG_SOURCE) {
    $env:STREAMBOX_FFMPEG_SOURCE
} else {
    'D:\ffmpeg-8.1.2\ffmpeg-8.1.2'
}

$ffmpegSourceMsys = Convert-ToMsysPath $ffmpegSource

& $bash -lc "STREAMBOX_FFMPEG_SOURCE='$ffmpegSourceMsys' '$script'"
if ($LASTEXITCODE -ne 0) {
    throw "FFmpeg build failed with exit code $LASTEXITCODE"
}
