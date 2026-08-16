# Testing StreamBox

## Automated tests

```powershell
& 'D:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build --output-on-failure
```

The test suite currently covers:

- FFmpeg SDK link and runtime loading.
- Retry backoff policy.
- Buffer percentage clamping.
- Seekability rules for VOD and live inputs.
- Audio-master video display, delay and drop decisions.
- Playlist mode boundaries and previous/next behavior.
- Generated-WAV open, decode, 2.0× tempo, pause/resume, seek and stop.
- In-process HTTP playback, failed connection retry and cancellation.
- One hundred consecutive media switches.

## Runtime regression

The application supports deterministic smoke runs:

```powershell
$env:STREAMBOX_SCREENSHOT="$PWD\runtime-smoke.png"
& '.\build\StreamBox.exe' --speed 2.0 --open 'sample.wav'

# 精确 Seek 验收（毫秒）
& '.\build\StreamBox.exe' --open 'sample.mp4' --seek 20000

# 全屏控制栏 3 秒自动隐藏验收
& '.\build\StreamBox.exe' --open 'sample.mp4' --fullscreen `
  --screenshot-delay 4500 --screenshot "$PWD\fullscreen-smoke.png"
```

The media smoke probe requires decoded output from every audio/video stream type declared by the opened media; an audio packet alone cannot make a video probe pass.

For a real-device pause/resume check:

```powershell
$env:Path='D:\Qt\6.11.0\mingw_64\bin;D:\Qt_project\codex_project\third_party\ffmpeg-sdk\bin;' + $env:Path
& '.\build\player_runtime_probe.exe' 'D:\media\sample.mp4'
```

The probe requires a video frame, pauses after 2 seconds, verifies for 700 ms that both position and displayed-frame count remain unchanged, then resumes and requires new position and video-frame output.

Validated scenarios are recorded in `TECH_STACK_AUDIT.md`. Long-duration tests require representative local media and a controllable HTTP server; they are release gates rather than unit tests.

## Format matrix

```powershell
& '.\scripts\format_matrix.ps1' -Media @('sample.wav', 'sample.mp4', 'sample.webm')
```

The probe succeeds only after decoded output is produced. Results are maintained in `ACCEPTANCE_TEST_REPORT.md`.

## Two-hour stability gate

```powershell
& '.\scripts\deploy_windows.ps1'
& '.\scripts\soak_test.ps1' -Media 'D:\media\long-video.mp4' -DurationMinutes 120
```

Memory, thread and handle samples are written to `dist\soak-test.csv`.

## Physical audio endpoint

```powershell
$env:Path='D:\Qt\6.11.0\mingw_64\bin;' + $env:Path
& '.\build\audio_device_test.exe'
```

This plays an audible three-second intermittent 1 kHz tone through Qt's current default output and verifies at least 2.5 seconds of `QAudioSink` hardware time with no sink error. Human confirmation is still required to prove that the selected physical endpoint is connected and audible.

The decoder integration test also verifies that filtered PCM remains frame-aligned, that generated identical stereo channels remain identical, and that 2.0× output has the expected byte count. These checks detect planar/packed mistakes and truncated audio that a non-empty-output assertion would miss.
