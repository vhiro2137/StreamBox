$ErrorActionPreference = 'Stop'

$workspace = Split-Path -Parent $PSScriptRoot
$build = Join-Path $workspace 'build'
$dist = Join-Path $workspace 'dist\StreamBox'
$cmake = 'D:\Qt\Tools\CMake_64\bin\cmake.exe'
$windeployqt = 'D:\Qt\6.11.0\mingw_64\bin\windeployqt.exe'
$ffmpegBin = Join-Path $workspace 'third_party\ffmpeg-sdk\bin'

& $cmake --build $build --parallel
if ($LASTEXITCODE -ne 0) { throw 'StreamBox build failed' }

New-Item -ItemType Directory -Force -Path $dist | Out-Null
Copy-Item -Force -LiteralPath (Join-Path $build 'StreamBox.exe') -Destination $dist
Copy-Item -Force -Path (Join-Path $ffmpegBin '*.dll') -Destination $dist

& $windeployqt --release --no-translations --compiler-runtime (Join-Path $dist 'StreamBox.exe')
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed' }

Copy-Item -Force -LiteralPath 'D:\ffmpeg-8.1.2\ffmpeg-8.1.2\LICENSE.md' -Destination (Join-Path $dist 'FFMPEG_LICENSE.md') -ErrorAction SilentlyContinue
Write-Host "Deployment ready: $dist"
